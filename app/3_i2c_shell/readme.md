# 3_i2c_shell

## Overview
An I2C controller / target example with the I2C Shell and GPIO Shell enabled.

- `i2c_smb_1` (port 7, GPIO012/GPIO013, TP257) is configured as an **I2C target** at address `0x76`
  - Data written by the host is stored in a buffer and logged on STOP
  - Reads from the host return the buffer contents (default `0x00, 0x01, 0x02, ...`)
- `i2c_smb_2` (port 6, GPIO132/GPIO140, TP252) acts as the **I2C controller** and can issue I2C commands from the shell

Connect the two I2C buses together to self-test from the shell:
```sh
MCHP:~$ i2c scan <i2c_smb_2 device>
MCHP:~$ i2c write <i2c_smb_2 device> 0x76 0x11 0x22 0x33
MCHP:~$ i2c read <i2c_smb_2 device> 0x76 4
```

## How to Build

### Environment Setup
Run from the project root:
```sh
source tools/script/setup.sh
```

### Build
```sh
west build -p -b mec172xmodular_assy6930 app/3_i2c_shell
```
