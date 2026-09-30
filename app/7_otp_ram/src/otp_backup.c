#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/eeprom.h>
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
static const struct device *otp_ver = DEVICE_DT_GET(DT_NODELABEL(otp_ver0));
static const struct device *otp_data = DEVICE_DT_GET(DT_NODELABEL(otp_info0));
static const struct device *eeprom_data = DEVICE_DT_GET(DT_NODELABEL(eeprom_info0));

static const struct device *eeprom = DEVICE_DT_GET(DT_NODELABEL(eeprom));

static uint8_t buf[DT_REG_SIZE(DT_NODELABEL(eeprom_info0))] = {0};
static uint32_t rom_ver = 0;

#include <zephyr/init.h>

static int init_config(void) {
    int ret;

    LOG_INF("--- Zephyr Retention system test ---");

    // Check if the retention memory devices are ready
    if (!device_is_ready(otp_ver)) {
        LOG_ERR("OTP version device is not ready!");
        return -1;
    }

    // Read the boot rom version from the ROM API and store it in retention memory
    rom_ver = api_rom_ver();
    LOG_INF("ROM Version: 0x%08X", rom_ver);
    ret = retention_write(otp_ver, 0, (uint8_t *)&rom_ver, sizeof(rom_ver));
    if (ret < 0) {
        LOG_ERR("Failed to write Retention: %d", ret);
        return ret;
    }

    // Check if the retention memory devices are ready
    if (!device_is_ready(otp_data)) {
        LOG_ERR("OTP buffer device is not ready!");
        return -1;
    }

    // Read the OTP data from efuse and store it in retention memory
    for (int i = 0; i < DT_REG_SIZE(DT_NODELABEL(otp_info0)); i++) {
        api_efuse_byte_read(i, &buf[i]);
    }
    ret =
        retention_write(otp_data, 0, buf, DT_REG_SIZE(DT_NODELABEL(otp_info0)));
    if (ret < 0) {
        LOG_ERR("Failed to write Retention: %d", ret);
        return ret;
    }

    memset(buf, 0xAA, sizeof(buf));

    // Check if the retention memory devices are ready
    if (!device_is_ready(eeprom_data)) {
        LOG_ERR("EEPROM buffer device is not ready!");
        return -1;
    }
    if (!device_is_ready(eeprom)) {
        LOG_ERR("EEPROM buffer device is not ready!");
        return -1;
    }

    // Read the EEPROM data and store it in retention memory
    ret = eeprom_read(eeprom, 0, buf, DT_REG_SIZE(DT_NODELABEL(eeprom_info0)));
    if (ret < 0) {
        LOG_ERR("Failed to read EEPROM: %d", ret);
    }
    ret = retention_write(eeprom_data, 0, buf,
                          DT_REG_SIZE(DT_NODELABEL(eeprom_info0)));
    if (ret < 0) {
        LOG_ERR("Failed to write Retention: %d", ret);
        return ret;
    }

    return 0;
}

SYS_INIT(init_config, APPLICATION, 0);
