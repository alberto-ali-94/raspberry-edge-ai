# Raspberry Edge AI

A learning project building toward an Edge AI system on Raspberry Pi, combining embedded programming, sensor integration, and on-device inference.

The current implementation is a C terminal application for GPIO control. It reads temperatures entered manually, applies hysteresis around a user-defined threshold, and controls an LED through the Linux GPIO character device API v2.

## Project goals

The long-term goal is to develop two Edge AI applications on Raspberry Pi, sharing foundations in embedded programming, data acquisition, and local inference.

### 1. Intelligent environmental monitoring

Build a system that collects measurements from physical sensors and uses models developed with Python and Keras to detect anomalies, identify patterns, and drive hardware responses to relevant environmental conditions.

### 2. Home security with computer vision and facial recognition

Build a personal home security system that captures images from a camera and uses computer vision and facial recognition to distinguish known faces from unrecognized faces.

The planned control logic has two states:

- **DISARMED:** alerts for unrecognized faces are disabled.
- **ARMED:** detecting an unrecognized face should trigger an alert to the user.

This future application will combine camera acquisition, model inference, state transitions, and notification handling. Model development will use Python and Keras; recognition thresholds, false positives, missed detections, and repeated alerts will need evaluation before integration. The camera, model, and notification channel have not been selected yet.

Both applications are future goals. Camera acquisition, facial recognition, ARMED/DISARMED states, and security notifications are not implemented. The current temperature alarm/LED state described below does not represent this planned security state machine.

The project will grow through incremental, working milestones:

1. **Embedded foundations:** develop modular C/C++ code for GPIO control, reliable input handling, buttons, and hardware events.
2. **Sensor data collection:** acquire measurements, process samples, and record datasets for analysis.
3. **Model development:** use Python and Keras to train and evaluate models for sensor-based anomaly detection and camera-based facial recognition.
4. **Edge deployment:** adapt a suitable model for inference on the Raspberry Pi and evaluate its accuracy, latency, and resource usage.
5. **System integration:** connect data acquisition, inference, and hardware actions in a documented application.

These are development goals. Sensor acquisition and AI inference are not implemented yet. Model choice and deployment details for both applications will depend on the available data and the Raspberry Pi's hardware limits; accuracy, latency, and resource usage on the Raspberry Pi 3 B+ remain to be measured.

## Current functionality

- Accept an integer temperature threshold and successive integer temperatures from the keyboard.
- Control an LED on GPIO17 using hysteresis: turn on above `threshold + 2`, turn off at or below `threshold - 2`, and retain the previous state between those boundaries.
- Report whether each temperature is above the base threshold and whether the alarm is active.
- Request GPIO18 and GPIO19 as inputs with internal pull-ups, in preparation for button integration. The main loop does not read these inputs yet.
- Attempt to turn the LED off and release GPIO resources when an input or GPIO error reaches cleanup.

Temperatures are entered manually; the application does not read a temperature sensor. The current hardware milestone provides foundations for the broader Edge AI project.

## Requirements

- A Raspberry Pi running Linux with GPIO character device API v2 support and compatible GPIO headers. The development device is a Raspberry Pi 3 B+.
- A C11 compiler available as `gcc`, and Make.
- Permission to access `/dev/gpiochip0`.
- GPIO offsets 17, 18, and 19 available for the application's line request.
- An LED circuit with an appropriate current-limiting resistor connected to GPIO17, matching the project's existing wiring.

GPIO numbers here are line offsets, not physical header pin numbers. Verify the controller and wiring for your board before running the application.

The full application uses Linux GPIO interfaces and must be built on the Raspberry Pi or another suitable Linux environment. The temperature and terminal I/O modules can be compiled separately on macOS. Python and Keras belong to future milestones and are not required for the current C application.

## Project structure

| File | Responsibility |
|---|---|
| `main.c` | Coordinates GPIO configuration, threshold input, temperature readings, hysteresis, LED state, and cleanup. |
| `io.h` / `io.c` | Declare and implement `print_menu` and `read_integer`. |
| `temperatura.h` / `temperatura.c` | Declare and implement `is_temperature_high` and the hysteresis function `update_alarm`. |
| `gpio.h` / `gpio.c` | Declare and implement GPIO controller access, multi-line requests, line value operations, and descriptor cleanup. |
| `old/` | Earlier learning probes and experiments; excluded from the main application build. |
| `Makefile` | Defines dependencies, compilation of four object files, and executable linking. |
| `.gitignore` | Excludes object files, known local executables, and macOS metadata. |

## Build

From the project directory, run:

```sh
make
```

Make builds `main.o`, `io.o`, `temperatura.o`, and `gpio.o`, then links them into `control_center`. Compilation enables C11 and the `-Wall -Wextra -Wpedantic` warnings.

Subsequent builds rebuild files according to their modification times and declared dependencies. For example, editing `io.c` rebuilds `io.o` and relinks the executable. Editing `io.h` also rebuilds `main.o`.

To force a full rebuild:

```sh
make -B
```

## Run

```sh
./control_center
```

1. The application opens `/dev/gpiochip0` and requests GPIO17 as an output and GPIO18/GPIO19 as inputs with pull-ups.
2. Enter an integer temperature threshold.
3. Enter one integer temperature at a time, pressing Enter after each value.
4. The application updates the LED using hysteresis and prints the threshold comparison and alarm state.

There is currently no dedicated exit command: `-999` is treated as a temperature. Invalid input or end-of-input triggers the cleanup path. In a terminal, Ctrl+D at an empty input prompt can signal end-of-input; the application currently handles this as an input error and returns a nonzero status.

## Example

With a threshold of `30` and the alarm initially off, enter these temperatures in order:

| Input temperature | Threshold comparison | Alarm / LED |
|---|---|---|
| `31` | Above threshold | Off: the turn-on boundary has not been crossed |
| `33` | Above threshold | On: above `32` |
| `30` | At or below threshold | Stays on: within the hysteresis band |
| `28` | At or below threshold | Off: at or below `28` |

The threshold comparison and alarm state can differ because hysteresis takes the previous alarm state into account.

## Input handling and current limitations

- `read_integer` uses `scanf("%d", p)`. Input is blocking, and the main loop does not process button events while waiting for keyboard input.
- Entire input lines are not validated: `30abc` can initially be read as `30`, leaving the remaining characters for the next read. Input recovery and numeric range validation are not implemented yet.
- Calculations of `threshold + 2` and `threshold - 2` are not guarded against integer overflow. Use ordinary temperature values away from the limits of `int`.
- Invalid threshold input sets exit status `1`; invalid temperature input sets exit status `2`. GPIO and cleanup failures also produce nonzero exit statuses, and later cleanup failures can overwrite an earlier error code.
- GPIO18/GPIO19 input handling, button debounce, and a dedicated shutdown command remain to be integrated into the main application.
- The current source configuration and behavior are documented here; each hardware change still needs validation on the Raspberry Pi.

## Next milestone

Understand and integrate keyboard and GPIO event handling so that buttons can be processed while the application waits for temperature input. The intended controls are a manual LED toggle on GPIO18 and orderly shutdown on GPIO19. This milestone precedes sensor acquisition and the later AI pipeline.
