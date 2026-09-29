#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/retained_mem.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/retention/retention.h>

LOG_MODULE_REGISTER(app_retained, LOG_LEVEL_INF);

#include <stdlib.h>

#define OTP_WRITE_SHELL_SUCCESS (0x00)
#define OTP_WRITE_SHELL_SUCCESS_ALREADY_SAME (0xFF)

#define api_efuse_byte_write ((api_efuse_byte_write_td)0x0000f00d)
#define api_efuse_byte_read ((api_efuse_byte_read_td)0x0000f009)
#define api_rom_ver ((api_rom_ver_td)0x0000f001)

typedef uint8_t (*api_efuse_byte_write_td)(uint16_t byte_idx, uint8_t in_data);
typedef uint8_t (*api_efuse_byte_read_td)(uint16_t byte_idx, uint8_t *out_data);
typedef uint32_t (*api_rom_ver_td)(void);

/* 1. 從 Devicetree 取得我們定義的 boot_info0 裝置實例 */
static const struct device *retain_dev = DEVICE_DT_GET(DT_NODELABEL(otp_info0));

static uint8_t otp_data[2048];
static uint32_t rom_ver = 0;

#include <zephyr/init.h>

static int init_config(void) {
    int ret;

    LOG_INF("--- Zephyr Retention system test ---");

    /* 檢查裝置是否已經就緒 */
    if (!device_is_ready(retain_dev)) {
        LOG_ERR("Retention device is not ready!");
        return -1;
    }

    for (int i = 0; i < sizeof(otp_data); i++) {
        api_efuse_byte_read(i, &otp_data[i]);
    }

    rom_ver = api_rom_ver();
    LOG_INF("ROM Version: 0x%08X", rom_ver);

    ret = retention_write(retain_dev, 0, otp_data, sizeof(otp_data));
    if (ret < 0) {
        LOG_ERR("Failed to write Retention: %d", ret);
        return ret;
    }

    ret = retention_write(retain_dev, sizeof(otp_data), &rom_ver,
                          sizeof(rom_ver));
    if (ret < 0) {
        LOG_ERR("Failed to write Retention: %d", ret);
        return ret;
    }

    return 0;
}

SYS_INIT(init_config, APPLICATION, 0);
