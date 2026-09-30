# APP Zephyr

## Zephyr toolchain
Windows Minimal: [link](https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/zephyr-sdk-0.16.8_windows-x86_64_minimal.7z)

arm-zephyr-eabi: [link](https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/toolchain_windows-x86_64_arm-zephyr-eabi.7z)


## SDK install
```
source tools/script/install.sh
``` 

## Setup
```
source tools/script/setup.sh
```

## Build
```
west build -p -b mec172xmodular_assy6930 app/hello_world/
```

## Flash
```

```


## Debug

Load image
```sh
#OpenOCD
openocd -f interface/cmsis-dap.cfg  -f ./tools/mec1753_ram.cfg 

telnet localhost 4444

> halt
# > soft_reset_halt

> load_image build/zephyr/zephyr.elf

> reg pc 0x000cc6cd
> resume
```


Read Boot ROM status
```sh
# JLINK
JLinkExe
J-Link> connect
J-Link> swd

# With valid image:
J-Link>mem64 0x001263F4 1
001263F4 = 0010000708400230

# Without valid image:
J-Link>mem64 0x001263F4 1
001263F4 = 0010000380000230

#OpenOCD
openocd -f interface/cmsis-dap.cfg  -f ./tools/mec1753_ram.cfg 

telnet localhost 4444

mdw 0x001263F4 2
0x001263f4: 80000230 00100003
```
