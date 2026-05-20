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
