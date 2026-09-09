#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/retention/retention.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_retention, LOG_LEVEL_INF);

/* 1. 從 Devicetree 取得我們定義的 sys_info0 裝置實例 */
static const struct device *retention_dev = DEVICE_DT_GET(DT_NODELABEL(sys_info0));

/* 2. 定義您要儲存的自訂資料結構（注意：大小不可超過 DTS 規定的 0x100 空間） */
struct boot_state {
    uint32_t count;
    uint32_t mode;
};

#include <zephyr/init.h>

static int init_config(void)
{
    struct boot_state state;
    int ret;

    if (!device_is_ready(retention_dev)) {
        LOG_ERR("Retention device not ready");
        return -EINVAL;
    }

    /* 🟢 步驟 1：使用 retention_is_valid 檢查魔術字串(prefix)與校驗和(checksum) */
    if (retention_is_valid(retention_dev) == 1) {
        
        /* 🟢 步驟 2：資料有效，直接讀取 */
        ret = retention_read(retention_dev, 0, (uint8_t *)&state, sizeof(state));
        if (ret == 0) {
            LOG_INF("Success! Read retention data successfully! Count: %d, Mode: 0x%08x", state.count, state.mode);
            state.count++;
        }
    } else {
        /* 🔴 步驟 3：資料無效（如首次冷開機），進行初始化 */
        LOG_WRN("Retention data is invalid, initializing...");
        state.count = 1;
        state.mode = 0xAABBCCDD;
        
        /* 注意：寫入前不需要手動 clear，底下的 write 會自動覆蓋並重新計算校驗 */
    }

    /* 🟢 步驟 4：將最新狀態寫回保留區（驅動會自動更新內部的 prefix 與 checksum） */
    ret = retention_write(retention_dev, 0, (uint8_t *)&state, sizeof(state));
    if (ret < 0) {
        LOG_ERR("寫入失敗: %d", ret);
    }

    return 0;
}

SYS_INIT(init_config, APPLICATION, 0);
