# 6_embedded_reset

## Overview
An MEC175x Embedded Reset and VCI_OUT control example (`src/embedded_reset.c`). At boot it does the following in order:

1. Disable Embedded Reset (releases the hardware inhibit on VCI_OUT)
2. Check the Embedded Reset status; if it fired before, clear the flag and reset VBAT and the EC
3. Enable firmware control of VCI_OUT (`FW_EXT = 1`)
4. Drive VCI_OUT high (`VCI_FW_CNTRL = 1`)
5. Set the timeout to 15 seconds and re-enable Embedded Reset

Each step prints the related register values to the console. An LED blink (`src/blink.c`) shows that the system is running.

## How to Build

### Environment Setup
Run from the project root:
```sh
source tools/script/setup.sh
```

### Build
```sh
west build -p -b mec172xmodular_assy6930 app/6_embedded_reset
```
