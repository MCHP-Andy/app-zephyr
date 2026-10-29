# 7_otp_ram

## Overview
At boot, reads OTP (eFuse) and EEPROM contents through the Boot ROM API and dumps them to retained RAM, so they can be inspected directly with a debugger.

The overlay reserves retained RAM at `0x123800` (size `0x4000`):

| Node           | Address    | Size     | Contents |
|----------------|------------|----------|----------|
| `otp_ver0`     | `0x123800` | `0x800`  | Boot ROM version (`api_rom_ver()`) |
| `otp_info0`    | `0x124000` | `0x800`  | OTP data (`api_efuse_byte_read()`) |
| `eeprom_info0` | `0x125000` | `0x2000` | EEPROM data (`eeprom_read()`) |

The EEPROM Shell is also enabled, so the EEPROM can be accessed with the `eeprom read/write` commands.

## How to Build

### Environment Setup
Run from the project root:
```sh
source tools/script/setup.sh
```

### Build
```sh
west build -p -b mec172xmodular_assy6930 app/7_otp_ram
```
