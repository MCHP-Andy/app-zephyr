#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
// #include <zephyr/drivers/retained_mem.h>
// #include <zephyr/retention/retention.h>
#include <zephyr/logging/log.h>

#include <zephyr/arch/cpu.h> // 引入 ARM 核心底層 API


LOG_MODULE_REGISTER(fw_upgrade, LOG_LEVEL_INF);

#define PAYLOAD_BUF_SIZE 2048

// 確保結構體記憶體對齊與排列順序與 Python 一致
struct __packed shared_exchange_mem {
    uint8_t  payload_buffer[PAYLOAD_BUF_SIZE]; // 1. Payload Buffer (2KB)
    uint32_t total_bin_size;                  // 2. 總 Bin 大小
    uint32_t spi_start_addr;                 // 3. SPI 起始點
    uint32_t current_spi_addr;               // 4. 本次 SPI 寫入起點
    uint32_t chunk_size;                     // 5. 本次 chunk 大小
    volatile uint8_t busy_bit;               // 6. 1: Host寫完/MCU忙碌中, 0: MCU寫完/閒置
};

// static const struct device *retain_dev = DEVICE_DT_GET(DT_NODELABEL(swap_data0));

static struct shared_exchange_mem *shared_mem = (volatile struct shared_exchange_mem *)(0x126800); // swap_data0
static const struct device *flash_dev = DEVICE_DT_GET(DT_NODELABEL(int_flash));

static void service(void)
{
    int ret = 0;
    bool is_first_chunk = true;

    if (!device_is_ready(flash_dev)) {
        LOG_ERR("Flash device not ready");
        return;
    }

    LOG_INF("MCU IPC Exchange Service Started.");

    memset(shared_mem, 0, sizeof(struct shared_exchange_mem));

    while (1) {
        // 6. 等待 busy_bit 立成 1
        if (shared_mem->busy_bit == 0) {
            k_msleep(10); // 等待時 sleep 10ms
            __DSB();
            continue;
        }

        // 第一次收到 payload 時，清除整顆外部 flash
        if (is_first_chunk) {
            LOG_INF("First chunk received. Erasing whole external flash...");
            LOG_INF("Address: 0x%08X, Total Size: 0x%08X", shared_mem->spi_start_addr, shared_mem->total_bin_size);
            // 這裡假設清除大小。實務上可以從 flash_get_page_info 或直接填入晶片總容量
            // 為了展示安全，這裡使用全晶片擦除（注意：可能需要數秒到數十秒，視晶片而定）
            // 如果擦除時間過長，Host 端的 wait 5ms 可能需要調整，或在這裡餵 Watchdog
            ret = flash_erase(flash_dev, shared_mem->spi_start_addr, shared_mem->total_bin_size);
            if (ret != 0) {
                LOG_ERR("Flash erase failed at 0x%08X, len: 0x%08X", shared_mem->spi_start_addr, shared_mem->total_bin_size);
            }
            LOG_INF("image: 0x%02x, 0x%02x, 0x%02x, 0x%02x", 
                shared_mem->payload_buffer[0], 
                shared_mem->payload_buffer[1], 
                shared_mem->payload_buffer[2], 
                shared_mem->payload_buffer[3]);
            is_first_chunk = false;
            LOG_INF("Flash erase completed.");
        }

        // 寫入本次 chunk 資料到外部 SPI Flash
        if (shared_mem->chunk_size > 0 && shared_mem->chunk_size <= PAYLOAD_BUF_SIZE) {
            ret = flash_write(flash_dev, 
                                  shared_mem->current_spi_addr, 
                                  shared_mem->payload_buffer, 
                                  shared_mem->chunk_size);
            if (ret != 0) {
                LOG_ERR("Flash write failed at 0x%08X", shared_mem->current_spi_addr);
            }
        }


        LOG_INF("0x%08x / 0x%08x written to SPI Flash.", shared_mem->current_spi_addr - shared_mem->spi_start_addr, shared_mem->total_bin_size);
        // 寫入結束，將 busy_bit 清除為 0，通知 Host 可以發送下一包
        shared_mem->busy_bit = 0;
    }
}


K_THREAD_DEFINE(fw_upgrade_id, 4096, service, NULL, NULL, NULL, 10,
                0, 0);
