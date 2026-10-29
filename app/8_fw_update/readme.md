# 8_fw_update

## Overview
An example of writing a firmware image to SPI flash through a debugger. The host writes into the MCU's shared RAM with pyOCD, and a thread on the MCU writes the received data to flash.

The shared memory is at `0x126800` (`swap_data0`), with this layout (matching `fw_upgrade.py`):

| Offset  | Field              | Description |
|---------|--------------------|-------------|
| `0x000` | `payload_buffer`   | 2KB payload |
| `0x800` | `total_bin_size`   | Total image size |
| `0x804` | `spi_start_addr`   | SPI start address |
| `0x808` | `current_spi_addr` | Write address for this chunk |
| `0x80C` | `chunk_size`       | Size of this chunk |
| `0x810` | `busy_bit`         | 1: host has written / MCU busy, 0: MCU idle |

Flow:
1. The host writes the payload and parameters, then sets `busy_bit` to 1
2. On the first chunk, the MCU erases the `total_bin_size` range, then writes the chunk
3. When done, the MCU clears `busy_bit` to 0 and the host sends the next chunk

The Flash Shell is also enabled, so flash can be accessed directly with the `flash read/write/erase` commands.

## How to Build

### Environment Setup
Run from the project root:
```sh
source tools/script/setup.sh
```

### Build
```sh
west build -p -b mec172xmodular_assy6930 app/8_fw_update
```

## Usage
After flashing and running this app, run on the host (requires pyOCD):
```sh
python app/8_fw_update/fw_upgrade.py <bin_path> <spi_start_addr_hex>
```
