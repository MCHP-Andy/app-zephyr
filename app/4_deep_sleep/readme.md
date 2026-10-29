# 4_deep_sleep

## Overview
A Zephyr Power Management (`CONFIG_PM`) deep sleep example.

- `src/deep_slp.c`: overrides `pm_state_set()` / `pm_state_exit_post_ops()`
  - `PM_STATE_SUSPEND_TO_IDLE`: light sleep
  - `PM_STATE_SUSPEND_TO_RAM`: heavy / deep sleep (48MHz PLL turned off)
  - Before sleep, turns off ADC, TACH, SMB and Timer through PCR `SLP_EN` or the peripheral registers, and restores them on wake
  - Routes `TST_CLK_OUT` (GPIO060) to a pin so you can check with an oscilloscope whether the clock stops
- `src/blink.c`: toggles the LED every 1 second; the system sleeps while idle
- The overlay disables unused peripherals such as TACH0 and SMB0~2 to reduce power

> Note: the MEC does not turn off the 48MHz PLL while a JTAG/SWD debugger stays connected.

## How to Build

### Environment Setup
Run from the project root:
```sh
source tools/script/setup.sh
```

### Build
```sh
west build -p -b mec172xmodular_assy6930 app/4_deep_sleep
```

## Patch
The SoC default implementations in zephyr-rtos must be made weak so this app can override them:

`zephyr-rtos/soc/microchip/mec/mec172x/power.c`
```c
__weak void pm_state_set(enum pm_state state, uint8_t substate_id)

__weak void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
```
