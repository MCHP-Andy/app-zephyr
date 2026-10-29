# 1_blink_led

## Overview
A basic GPIO example. It toggles the on-board LED (devicetree alias `led0`, mapped to `led2`) every 1 second.

- `src/blink.c`: a dedicated thread (`K_THREAD_DEFINE`) that toggles the LED periodically
- `src/main.c`: prints `Hello World! <board>` to the console at boot

## How to Build

### Environment Setup
Run from the project root:
```sh
source tools/script/setup.sh
```

### Build
```sh
west build -p -b mec172xmodular_assy6930 app/1_blink_led
```
