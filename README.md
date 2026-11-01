# Raspberry Edge AI

Raspberry Edge AI is a learning project that combines C/C++ embedded control on Raspberry Pi with Python/Keras model development. Its long-term goal is environmental monitoring and local AI-assisted security. Sensor logging, integrated inference, camera recognition, and remote notifications are future developments.

## Current application: Hardware Control Center

The `control_center` application reads a physical **NTC thermistor through a PCF8591 ADC**, accepts an initial temperature threshold from the keyboard, and controls an LED. Two GPIO buttons provide manual LED control and shutdown.

The application uses two threads:

- The **main thread** handles keyboard input, GPIO events, sensor notifications, and all LED commands.
- The **acquisition worker** reads the ADC, converts the result to degrees Celsius, and publishes the latest temperature.

Python and Keras are not required to build or run this application. The notebooks in `Keras_Practice/` are separate learning experiments, not an inference pipeline connected to the Control Center.

## Hardware and interfaces

The development board is a Raspberry Pi 3 B+, using components from the Freenove FNK0020 kit. The ADC used by this implementation is specifically the **PCF8591**, not the ADS7830 variant covered by some kit tutorials.

| Component | Interface or GPIO | Role |
|---|---|---|
| NTC thermistor and resistor divider | PCF8591 AIN0 | Analog temperature input |
| PCF8591 ADC | `/dev/i2c-1`, 7-bit address `0x48` | Convert the divider voltage to an 8-bit sample |
| LED with current-limiting resistor | GPIO19 | Automatic threshold indication and manual toggle |
| Exit button | GPIO24 | Request orderly shutdown |
| Toggle button | GPIO23 | Invert the LED state |

The application opens `/dev/gpiochip0`. GPIO numbers identify line offsets on the selected controller, not physical header pin numbers. Verify the controller and physical wiring for the board in use before running the application.

Both buttons use internal pull-ups and connect their input to ground when pressed. The code requests falling-edge events and a **30 ms debounce period** for the two inputs. The LED line is configured as an output.

The existing [circuit SVG](docs/images/control-center-circuit.svg) shows an earlier GPIO17/GPIO18/GPIO19 arrangement and does **not** describe the current circuit. It must not be used as the current wiring reference.

### Thermistor acquisition

`thermistor.c` implements the PCF8591 acquisition and conversion:

1. Write control byte `0x00` to select AIN0 in single-ended mode.
2. Read two bytes and use the second byte; the first belongs to the previous conversion.
3. Calculate the voltage as `raw / 256.0 * 3.3`.
4. Convert the divider voltage to resistance, then temperature using the NTC beta equation.

The conversion assumes a divider arranged as `3.3 V -> fixed resistor -> AIN0 node -> NTC -> ground`, with a 10 kΩ fixed resistor, a 10 kΩ NTC at 25 °C, and beta 3950 K. These are conversion assumptions, not a calibration result.

A zero ADC sample is rejected by the conversion guard. A successful I2C transfer alone does not establish that the thermistor is connected correctly or that the measured temperature is accurate.

## Runtime behavior

### Startup and threshold input

The main thread opens the GPIO and I2C resources, creates a nonblocking notification pipe, initializes the mutex and condition variable, and starts the acquisition worker. The worker can acquire temperatures while the program waits for the initial threshold.

Enter one signed decimal integer threshold and press Enter. The input reader accepts at most **three characters before Enter**, including an optional sign: `25`, `+25`, and `-12` fit. Invalid input is rejected without replacing the current value; oversized lines are discarded through their newline before another line is accepted.

The initial threshold is currently read **once**. After it is accepted, the main thread stops monitoring the keyboard and continues monitoring the GPIO buttons and notification pipe. Changing the threshold during operation is not implemented yet.

### Acquisition and event handling

The worker publishes temperature and a sequence counter under a mutex, then writes a one-byte notification to the pipe. The main thread reads notifications and copies the latest shared result under the same mutex. Notifications are also consumed before the threshold is available, without applying automatic LED control.

The pipe carries notifications, not temperature records. Shared memory holds only the latest sample: multiple acquisitions can be combined into one observed update. This is not a logger that preserves every sample. Sequence comparison prevents an already processed sample from being applied again merely because another notification remains in the pipe.

After each acquisition and notification, the worker waits **two seconds** using `pthread_cond_timedwait`. Its deadline uses `CLOCK_MONOTONIC`. The pause can be interrupted by a stop request; an I2C operation already in progress is not interrupted by the condition variable. The interval between acquisitions includes processing time as well as the two-second pause, so this is not a fixed-rate or hard real-time sampler.

The main thread uses `poll()` with **no timeout**. Before threshold entry it monitors stdin, GPIO, and the notification pipe; afterward it monitors GPIO and the pipe. It does not shut down merely because no events arrive.

### Automatic and manual LED control

Once a threshold is available, each newly observed valid sample determines the automatic LED state:

- Temperature **strictly above** the threshold: LED on.
- Temperature **at or below** the threshold: LED off.

Although `update_alarm()` supports separate switching thresholds, the current call passes the same threshold for both, so the application has no hysteresis band.

GPIO23 immediately toggles the LED, including before the initial threshold is entered. After threshold entry, the next newly observed valid sample can replace the manual state according to the temperature comparison. There is no persistent manual override.

Automatic temperature and alarm messages are printed when the automatic decision actually changes the LED state, rather than for every sample. Manual toggles print their own alarm-state message. This output is therefore a record of state changes, not a continuous temperature stream.

For example, with threshold `25`:

| Event | Result |
|---|---|
| New sample: 26.2 °C | LED on |
| New sample: 27.0 °C | LED remains on; no repeated automatic state message |
| Press GPIO23 | LED toggles off immediately |
| Next new sample: 27.0 °C | LED turns on again |
| New sample: 25.0 °C | LED off |
| Press GPIO24 | Orderly shutdown requested |

### Shutdown and errors

GPIO24 sends execution to the shared cleanup path. The main thread sets the stop flag under the mutex, broadcasts on the condition variable, and joins the worker before releasing its resources. Cleanup destroys initialized synchronization objects, closes descriptors, and attempts to turn the LED off before releasing the GPIO lines.

Initialization flags allow cleanup after partially completed startup. A failure reported by the worker is stored under the mutex and propagated to the main thread. Unexpected worker synchronization failures terminate the process immediately with `_Exit`, bypassing ordinary cleanup. If the main thread cannot establish safe synchronization or confirm worker termination, it does not attempt to destroy resources that might still be in use.

Exit codes are numeric literals in the relevant branches of `main.c`, rather than a separate error-code framework. Inspect the status immediately after execution:

```sh
./control_center
echo $?
```

A successful button-requested shutdown returns `0`. Examples of nonzero codes are `11` for opening the I2C bus, `60` for worker ADC acquisition, and `61` for temperature conversion. Cleanup reports additional failures while preserving an already selected primary error. Some module interfaces expose only success/failure, so the application code identifies the failed operation without always preserving a precise underlying system error.

## Build and run

Requirements:

- Linux with GPIO character device API v2 and I2C userspace headers.
- A C11 compiler available as `gcc`, Make, POSIX threads, and the math library.
- Enabled I2C and permission to access `/dev/i2c-1` and `/dev/gpiochip0`.
- The PCF8591/NTC circuit and the LED/buttons connected for the current configuration.
- An interactive terminal for the current keyboard interface.

From the repository root on the Raspberry Pi:

```sh
make -B
./control_center
```

The Makefile compiles `main.c`, `io.c`, `temperature.c`, `gpio.c`, and `thermistor.c`. It uses C11, `-Wall -Wextra -Wpedantic`, and `-pthread`, and links the math library with `-lm`. `make -B` forces a full rebuild; subsequent ordinary builds can use `make`.

The full application uses Linux-specific GPIO/I2C interfaces and is not a native macOS program.

## Repository components

| Path | Purpose |
|---|---|
| `main.c` | Acquisition worker, shared state, event loop, LED control, and resource lifecycle |
| `thermistor.c` / `thermistor.h` | PCF8591 reads and NTC-to-Celsius conversion |
| `io.c` / `io.h` | Menu, stateful keyboard reader, integer parser, and display helpers |
| `temperature.c` / `temperature.h` | Temperature comparison and LED-state calculation |
| `gpio.c` / `gpio.h` | Linux GPIO controller, line requests, values, and descriptor cleanup |
| `Makefile` | Application compilation and linking |
| `docs/images/control-center-circuit.svg` | Historical circuit drawing using the previous pin assignment |
| `Keras_Practice/` | Standalone Python/Keras notebooks |

## Known limitations and validation

- **Worker failure while main is waiting:** ordinary worker errors update shared state but do not send a terminal notification. Because `poll()` has no timeout and the main thread keeps the pipe writer open, an error can remain unnoticed until another monitored event wakes the main thread. Updating shared memory alone does not wake `poll()`.
- **Terminal input display:** asynchronous messages can split the visible echo of a line being typed. The program does not redraw the unfinished line. Visual disruption does not by itself prove that input characters were lost.
- **Interactive input scope:** the reader has a three-character limit and does not reliably preserve multiple lines delivered in one read. Keyboard read errors and EOF share a return value; the keyboard reader does not yet retry interrupted reads. GPIO and notification-pipe reads, notification writes, and `poll()` do handle `EINTR`.
- **Event ordering:** GPIO handling precedes keyboard and sensor handling, but only one GPIO event is read per iteration. Exit has no guaranteed priority over another GPIO event already ahead of it in the queue. A newly observed sample processed after a toggle in the same iteration can restore automatic control immediately.
- **Initial threshold:** entering it does not independently reapply a sample already processed before threshold entry; automatic control waits for a new sample update.
- **Hardware accuracy:** thermistor acquisition has been reported working on the Raspberry Pi after correcting a zero-sample issue. Calibration against a reference temperature and disconnected-sensor recovery have not been established.
- **Verification scope:** local isolated checks exercised synchronization, selected error paths, notification reads, and cleanup. They do not replace a complete hardware acceptance test of the latest program.
- **Signal shutdown:** Ctrl+C does not use the button's cooperative cleanup path; no application signal handler is installed.

## Roadmap

1. Allow threshold changes during operation and preserve a readable input line while asynchronous events are displayed.
2. Ensure worker failures reliably wake the main event loop and consolidate event ordering and hardware verification.
3. Extend the environmental station with timestamped logging and additional kit sensors, such as light, humidity, and PIR motion inputs, after checking their hardware and interfaces.
4. Develop and evaluate Keras models, save reproducible model/preprocessing artifacts, and measure inference feasibility on the target device.
5. Connect acquisition, inference, hardware responses, and the future camera-based security application. Camera hardware, recognition policy, and notification channels remain to be selected and implemented.
