# 2_gpio_shell

## Overview
Enables the Zephyr Shell and GPIO Shell so GPIOs can be read and written from the UART console (prompt `MCHP:~$ `). Useful for quickly checking pins on the board.

Common commands:
```sh
MCHP:~$ gpio devices
MCHP:~$ gpio get <device> <pin>
MCHP:~$ gpio conf <device> <pin> o
MCHP:~$ gpio set <device> <pin> 1
```

## How to Build

### Environment Setup
Run from the project root:
```sh
source tools/script/setup.sh
```

### Build
```sh
west build -p -b mec172xmodular_assy6930 app/2_gpio_shell
```
