# Raspberry Pi Control Center

A C terminal application that reads integer temperatures from the keyboard, compares them with a user-defined threshold, and controls a red LED through the Linux GPIO character device API v2.

This is a learning project for embedded development on Raspberry Pi. Temperatures are entered manually; the application does not read a temperature sensor.

## Requirements

- A Raspberry Pi running Linux with GPIO character device API v2 support and compatible GPIO headers.
- A C11 compiler available as `gcc`, and Make.
- Permission to access `/dev/gpiochip0`.
- An LED circuit connected to GPIO17, matching the project's existing wiring.

The current application uses Linux GPIO interfaces and must be built on the Raspberry Pi or another suitable Linux environment. The temperature and terminal I/O modules can be compiled separately on macOS.

## Project structure

| File | Responsibility |
|---|---|
| `main.c` | Coordinates GPIO setup, threshold input, temperature readings, LED state, and cleanup. |
| `io.h` / `io.c` | Declare and implement `print_menu` and `read_integer`. |
| `temperatura.h` / `temperatura.c` | Declare and implement `is_temperature_high` and the hysteresis function `update_alarm`. |
| `gpio_probe.h` / `gpio_probe.c` | Declare and implement GPIO controller access, line requests, line value changes, and descriptor cleanup. |
| `Makefile` | Defines dependencies, compilation of four object files, and executable linking. |

`update_alarm` is implemented but is not yet used by `main.c`; the running application currently uses a single threshold.

## Build

From the project directory, run:

```sh
make
```

Make builds `main.o`, `io.o`, `temperatura.o`, and `gpio_probe.o`, then links them into `control_center`. Compilation enables C11 and the `-Wall -Wextra -Wpedantic` warnings.

Subsequent builds rebuild files according to their modification times and declared dependencies. For example, editing `io.c` rebuilds `io.o` and relinks the executable. Editing `io.h` also rebuilds `main.o`.

To force a full rebuild:

```sh
make -B
```

## Run

```sh
./control_center
```

1. The application opens `/dev/gpiochip0` and requests line offset 17 as an output.
2. Enter an integer temperature threshold.
3. Enter one integer temperature at a time, pressing Enter after each value.
4. A temperature strictly above the threshold turns the LED on. A temperature at or below the threshold turns it off.
5. Enter `-999` as a temperature to exit. Cleanup attempts to turn the LED off and close both GPIO descriptors.

`-999` is reserved for exit only during temperature input; it is accepted as an ordinary value when entering the threshold.

## Example

With a threshold of `30`:

| Input temperature | Result |
|---|---|
| `29` | Temperature within normal range; LED off |
| `30` | Temperature within normal range; LED off |
| `31` | High temperature; LED on |
| `-999` | Cleanup and shutdown |

## Input handling and current limitations

`read_integer` returns the result of `scanf("%d", p)`. The caller checks whether one integer was read.

If reading fails, for example because the input is `hello` or input ends, the application prints `Input error!` and enters cleanup. An invalid threshold sets exit status `1`; an invalid temperature sets exit status `2`. Normal exit returns `0` if cleanup succeeds. GPIO and cleanup failures produce nonzero exit statuses; later cleanup failures currently overwrite an earlier error code.

Input handling does not validate the entire line: `30abc` can initially be read as `30`, leaving the remaining characters for the next read. Recovery from invalid input is not implemented yet.

The GPIO request helper currently initializes only `offsets[0]`, so it is used with `num_lines` equal to `1`.

## Next steps

Integrate the existing hysteresis function into the main loop, improve input handling, and extend the control center with further hardware components as the learning project progresses.
