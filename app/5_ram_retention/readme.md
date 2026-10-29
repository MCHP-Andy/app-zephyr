# 5_ram_retention

## Overview
A Zephyr Retained Memory / Retention example. It checks that data in SRAM survives a warm reset (software reboot, watchdog).

The overlay reserves a retained RAM region at `0x127000` (size `0x800`) with two retention areas:

| Node         | Offset  | Size    | Description |
|--------------|---------|---------|-------------|
| `boot_info0` | `0x000` | `0x100` | `src/retained.c`: checks validity with magic `0xABCDABCD` |
| `sys_info0`  | `0x100` | `0x100` | `src/retention.c`: checks validity with `retention_is_valid()` (prefix + checksum) |

At boot, invalid data (cold boot) is initialized; valid data is printed and the reset count is incremented.
Run `kernel reboot warm` in the shell to see the count go up.

## How to Build

### Environment Setup
Run from the project root:
```sh
source tools/script/setup.sh
```

### Build
```sh
west build -p -b mec172xmodular_assy6930 app/5_ram_retention
```
