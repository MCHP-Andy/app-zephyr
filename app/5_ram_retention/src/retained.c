#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/retained_mem.h>
#include <zephyr/retention/retention.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_retained, LOG_LEVEL_INF);

#define RETENTION_MAGIC 0xABCDABCD

/* 1. 從 Devicetree 取得我們定義的 boot_info0 裝置實例 */
static const struct device *retain_dev = DEVICE_DT_GET(DT_NODELABEL(boot_info0));

/* 2. 定義您要儲存的自訂資料結構（注意：大小不可超過 DTS 規定的 0x100 空間） */
struct my_boot_data {
    uint32_t magic;
    uint32_t reset_count;
    uint8_t  boot_mode;
    char     magic_str[8];
};

#include <zephyr/init.h>

static int init_config(void)
{
    int ret;
    struct my_boot_data data;

    LOG_INF("--- Zephyr Retention system test ---");

    /* 檢查裝置是否已經就緒 */
    if (!device_is_ready(retain_dev)) {
        LOG_ERR("Retention device is not ready!");
        return -1;
    }

    /* 3. 讀取保留記憶體中的資料 */
    /* 參數：(裝置, 記憶體偏移量, 緩衝區指標, 讀取長度) */
    ret = retention_read(retain_dev, 0, (uint8_t *)&data, sizeof(data));

    if (ret == -EILSEQ || ret == -ENOMSG || data.magic != RETENTION_MAGIC) {
        /* 
         * 當系統第一次冷開機（Cold Boot），或者不正常斷電導致資料損毀時，
         * Prefix 或 Checksum 會驗證失敗，retention_read 會回傳 -EILSEQ 或 -ENOMSG。
         * 此時必須將區域初始化（或清空）。
         */
        LOG_WRN("Invalid retention data detected (possibly first boot), initializing...");

        /* 初始化結構體資料 */
        data.reset_count = 1;
        data.boot_mode = 0x01;
        data.magic = RETENTION_MAGIC;
        snprintk(data.magic_str, sizeof(data.magic_str), "APP");

        /* 第一次寫入前，強烈建議先呼叫 retention_clear 清空，這會重置校驗狀態 */
        ret = retention_clear(retain_dev);
        if (ret < 0) {
            LOG_ERR("Failed to clear Retention: %d", ret);
            return ret;
        }

        /* 寫入初始資料 */
        ret = retention_write(retain_dev, 0, (uint8_t *)&data, sizeof(data));
        if (ret < 0) {
            LOG_ERR("Failed to write Retention: %d", ret);
            return ret;
        }
        LOG_INF("Initialization written successfully!");

    } else if (ret < 0) {
        /* 其他硬體或驅動錯誤 */
        LOG_ERR("Failed to read Retention, error code: %d", ret);
    } else {
        /* 
         * 讀取成功！代表這是熱重啟（Warm Boot，如軟體重啟或看門狗重啟），
         * 資料成功被保留下來了。
         */
        LOG_INF("Successfully read retention data!");
        LOG_INF("Current reset count (Reset Count): %d", data.reset_count);
        LOG_INF("Boot mode (Boot Mode): 0x%02X", data.boot_mode);
        LOG_INF("Magic string: %s", data.magic_str);

        /* 遞增計數器，並寫回記憶體，供下次重啟使用 */
        data.reset_count++;
        ret = retention_write(retain_dev, 0, (uint8_t *)&data, sizeof(data));
        if (ret < 0) {
            LOG_ERR("Failed to update Retention data: %d", ret);
        }
    }

    return 0;
}

SYS_INIT(init_config, APPLICATION, 0);
