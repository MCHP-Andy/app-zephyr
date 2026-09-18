/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(em_rst, LOG_LEVEL_INF);

#include <stdbool.h>
#include <stdint.h>

/*******************************************************************************
 * * MEC175x Register Base Addresses and Offsets Definition
 * ******************************************************************************/
/* VCI (VBAT-Powered Control Interface) Register */
#define VCI_REGISTER_ADDR                                                      \
    (0x4000AE00U) /* EC Subsystem Registers - Embedded Reset */
#define EMBEDDED_RESET_ENABLE_ADDR (0x4000FCB0U)
#define EMBEDDED_RESET_TIMEOUT_VAL_ADDR (0x4000FCB4U)
#define EMBEDDED_RESET_STATUS_ADDR (0x4000FCB8U)
/*******************************************************************************
 * * Register Bit Fields & Masks Definition
 * ******************************************************************************/
/* VCI Register (0x4000_AE00) Bit definitions */
#define VCI_REG_VCI_FW_CNTRL_BIT (10U)
#define VCI_REG_FW_EXT_BIT (11U)
#define VCI_REG_VCI_FW_CNTRL_MASK (1U << VCI_REG_VCI_FW_CNTRL_BIT)
#define VCI_REG_FW_EXT_MASK                                                    \
    (1U << VCI_REG_FW_EXT_BIT) /* Embedded Reset Enable Register (0x4000_FCB0) \
                                  Bit definitions */
#define EMBEDDED_RESET_ENABLE_BIT (0U)
#define EMBEDDED_RESET_ENABLE_MASK                                             \
    (1U << EMBEDDED_RESET_ENABLE_BIT) /* Embedded Reset Timeout Value Register \
                                         (0x4000_FCB4) Bit definitions */
#define EMBEDDED_RESET_TIMEOUT_MASK                                            \
    (0x07U) /* Timeout Value Selection: 0=6s, 1=7s, 2=8s, 3=9s, 4=10s, 5=11s,  \
               6=12s, 7=14s/15s */
#define EMBEDDED_RESET_TIMEOUT_15S_VAL                                         \
    (7U) /* Embedded Reset Status Register (0x4000_FCB8) Bit definitions */
#define EMBEDDED_RESET_STATUS_BIT (0U)
#define EMBEDDED_RESET_STATUS_MASK (1U << EMBEDDED_RESET_STATUS_BIT)

/*******************************************************************************
 * * Macro for Register Direct Access
 * ******************************************************************************/
#define REG32(addr) (*(volatile uint32_t *)(addr))

/*******************************************************************************
 * * Function Implementation
 * ******************************************************************************/
/** 
 * @brief 控制 VCI_OUT 並重新配置 Embedded Reset 流程
 * @return bool: 若先前觸發過 Embedded Reset 回傳 true，否則回傳 false 
 */
static bool vci_out_and_embedded_reset_control(void) {
    bool reset_occurred =
        false; // -------------------------------------------------------------------------
    // 步驟 1: 關閉 Embedded Reset 功能 (Disable Inhibit)
    // -------------------------------------------------------------------------
    // 寫入 0 至 EMBEDDED_RESET_ENABLE，關閉引擎並解除硬體對 VCI_OUT 的抑制鎖定
    REG32(EMBEDDED_RESET_ENABLE_ADDR) &= ~EMBEDDED_RESET_ENABLE_MASK;
    LOG_INF("Step 1: embedded reset disabled, enable reg = 0x%08x",
            REG32(EMBEDDED_RESET_ENABLE_ADDR));
    // -------------------------------------------------------------------------

    // 步驟 2: 檢查並清除 Embedded Reset 狀態
    // -------------------------------------------------------------------------
    // 讀取 status 狀態 bit 0
    LOG_INF("Step 2: embedded reset status reg = 0x%08x",
            REG32(EMBEDDED_RESET_STATUS_ADDR));
    if ((REG32(EMBEDDED_RESET_STATUS_ADDR) & EMBEDDED_RESET_STATUS_MASK) !=
        0U) {
        reset_occurred = true; 
        // 寫入 1 清除 Embedded Reset Status Flag
        REG32(EMBEDDED_RESET_STATUS_ADDR) |= EMBEDDED_RESET_STATUS_MASK;
        LOG_INF("Step 2: previous embedded reset detected, status flag cleared "
                "(status reg = 0x%08x)",
                REG32(EMBEDDED_RESET_STATUS_ADDR));

        k_msleep(1000);

        // Reset VBAT
        REG32(0x40080188) |= BIT(0);
        // Reset EC
        REG32(0x40080118) |= BIT(8);
    } else {
        LOG_INF("Step 2: no previous embedded reset");
    }

    // -------------------------------------------------------------------------
    // 步驟 3 & 4: 啟用 VCI_OUT FW 控制，並將 VCI_OUT 拉 High
    // -------------------------------------------------------------------------
    // 同時將 Bit 10 (VCI_FW_CNTRL = 1) 與 Bit 11 (FW_EXT = 1) 設定為 1
    // FW_EXT = 1: 選擇由韌體控制 VCI_OUT 腳位
    // VCI_FW_CNTRL = 1: 將 VCI_OUT 輸出驅動為 High
    LOG_INF("Step 3: VCI reg before FW control = 0x%08x",
            REG32(VCI_REGISTER_ADDR));
    REG32(VCI_REGISTER_ADDR) |=
        (VCI_REG_VCI_FW_CNTRL_MASK | VCI_REG_FW_EXT_MASK);
    LOG_INF("Step 4: VCI_OUT driven high by FW, VCI reg = 0x%08x",
            REG32(VCI_REGISTER_ADDR));
    // -------------------------------------------------------------------------
    // 步驟 5: 啟用 Embedded Reset 並設置 15 秒 Timeout
    // -------------------------------------------------------------------------
    // 5a. 設置 15 秒 Timeout (設定 EMBEDDED_RESET_TIMEOUT_VALUE 為 7)
    uint32_t timeout_reg = REG32(EMBEDDED_RESET_TIMEOUT_VAL_ADDR);
    timeout_reg &= ~EMBEDDED_RESET_TIMEOUT_MASK;
    timeout_reg |=
        (EMBEDDED_RESET_TIMEOUT_15S_VAL & EMBEDDED_RESET_TIMEOUT_MASK);
    REG32(EMBEDDED_RESET_TIMEOUT_VAL_ADDR) = timeout_reg;
    LOG_INF("Step 5a: timeout set to 15s, timeout reg = 0x%08x",
            REG32(EMBEDDED_RESET_TIMEOUT_VAL_ADDR));

    // 5b. 重新啟用 Embedded Reset 引擎
    REG32(EMBEDDED_RESET_ENABLE_ADDR) |= EMBEDDED_RESET_ENABLE_MASK;
    LOG_INF("Step 5b: embedded reset re-enabled, enable reg = 0x%08x",
            REG32(EMBEDDED_RESET_ENABLE_ADDR));
    return reset_occurred;
}

static int init_config(void) {
    bool reset_occurred;

    LOG_INF("embedded reset init start");
    reset_occurred = vci_out_and_embedded_reset_control();
    LOG_INF("embedded reset init done, previous embedded reset: %s",
            reset_occurred ? "yes" : "no");

    return 0;
}

SYS_INIT(init_config, APPLICATION, 0);
