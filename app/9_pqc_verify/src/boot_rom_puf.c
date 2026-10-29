

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "boot_rom.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(rom_puf, LOG_LEVEL_INF);

#define BK_ECC_MAX_CURVE_SIZE_N_WORDS 17
#define BK_ECC_KEY_CODE_HEADER_WORDS 12

#define BK_ECC_PRIVATE_KEY_CODE_SIZE_BYTES                                     \
    (4 * (BK_ECC_KEY_CODE_HEADER_WORDS + BK_ECC_MAX_CURVE_SIZE_N_WORDS))

// #include "iidbroadkey.h" // PUF / BroadKey 宣告與定義
#define mchp_pk_init ((mchp_pk_init_td)0x0001f0ad)
#define rom_puf_init ((rom_puf_init_td)0x0001f401)
#define bk_create_private_key ((bk_create_private_key_td)0x0001f425)
#define bk_ecdsa_sign ((bk_ecdsa_sign_td)0x0001f435)

typedef uint8_t iid_return_t;
#define IID_SUCCESS                  0x00


typedef enum bk_ecc_curve {
    /**
     * \brief Curve NIST-P192
     *
     * \details The elliptic-curve cryptosystem specified by the
     * NIST-P192/secp192r1 domain parameters:
     *
     * - BK curve ID: 0x00
     * - size in bytes of private keys: 24
     * - size in bytes of public keys: 49
     * - size in bytes of ECDSA signatures: 48
     * - size in bytes of ECDH shared secrets: 24
     * - size in bytes of ECIES header: 49
     * - maximal security strength for operations over this curve: 96-bit
     */
    BK_ECC_CURVE_NIST_P192 = 0x00,
    /**
     * \brief Curve NIST-P224
     *
     * \details The elliptic-curve cryptosystem specified by the
     * NIST-P224/secp224r1 domain parameters:
     *
     * - BK curve ID: 0x01
     * - size in bytes of private keys: 28
     * - size in bytes of public keys: 57
     * - size in bytes of ECDSA signatures: 56
     * - size in bytes of ECDH shared secrets: 28
     * - size in bytes of ECIES header: 57
     * - maximal security strength for operations over this curve: 112-bit
     */
    BK_ECC_CURVE_NIST_P224 = 0x01,
    /**
     * \brief Curve NIST-P256
     *
     * \details The elliptic-curve cryptosystem specified by the
     * NIST-P256/secp256r1 domain parameters:
     *
     * - BK curve ID: 0x02
     * - size in bytes of private keys: 32
     * - size in bytes of public keys: 65
     * - size in bytes of ECDSA signatures: 64
     * - size in bytes of ECDH shared secrets: 32
     * - size in bytes of ECIES header: 65
     * - maximal security strength for operations over this curve: 128-bit
     */
    BK_ECC_CURVE_NIST_P256 = 0x02,
    /**
     * \brief Curve NIST-P384
     *
     * \details The elliptic-curve cryptosystem specified by the
     * NIST-P384/secp384r1 domain parameters:
     *
     * - BK curve ID: 0x03
     * - size in bytes of private keys: 48
     * - size in bytes of public keys: 97
     * - size in bytes of ECDSA signatures: 96
     * - size in bytes of ECDH shared secrets: 48
     * - size in bytes of ECIES header: 97
     * - maximal security strength for operations over this curve: 192-bit
     */
    BK_ECC_CURVE_NIST_P384 = 0x03,
    /**
     * \brief Curve NIST-P521
     *
     * \details The elliptic-curve cryptosystem specified by the
     * NIST-P521/secp521r1 domain parameters:
     *
     * - BK curve ID: 0x04
     * - size in bytes of private keys: 66
     * - size in bytes of public keys: 133
     * - size in bytes of ECDSA signatures: 132
     * - size in bytes of ECDH shared secrets: 66
     * - size in bytes of ECIES header: 133
     * - maximal security strength for operations over this curve: 260-bit
     */
    BK_ECC_CURVE_NIST_P521 = 0x04,
} bk_ecc_curve_t;

struct mchp_regs
{
   char *base;
};

/** Description of the (hardware) accelerator*/
struct mchp_pk_accel {
    struct mchp_regs regs;
    char *cryptoram;
    int op_size;
    const struct mchp_pk_cmd_def *cmd;
    struct mchp_pk_cnx *cnx;
    const char *outputops[4];
    void *userctxt;
};
typedef struct mchp_pk_accel mchp_pk_accel;


/** Description for the type of PKE operation to be carried out.*/
struct mchp_pk_cmd_def {
    uint32_t cmdcode;
    uint32_t outslots;
    uint32_t inslots;
    uint32_t selected_ptrA;
};
/** Description of the (hardware) accelerator capabilities and features.*/
struct mchp_pk_capabilities {
    /** Maximum pending requests at any time. */
    int maxpending;
    /** Maximum operand size for prime field operands. 0 when not supported. */
    int max_gfp_opsz;
    /** Maximum operand size for elliptic curve operands. 0 when not supported.
     */
    int max_ecc_opsz;
    /** Maximum operand size for binary field operands. 0 when not supported. */
    int max_gfb_opsz;
};

// PKE Hardware accelerator status (mainly used for asynchronous apis)
struct mchp_pk_dreq {
    mchp_pk_accel *req;
    int status;
};
// Structure detailing the current PKE instance capabilities and accelerator
// instance
struct mchp_pk_cnx {
    struct mchp_pk_accel instances[1];
    struct mchp_pk_capabilities caps;
};
// Elliptic curve configuration and parameters.
struct mchp_pk_ecurve {
    uint32_t curveflags;
    int sz;
    const char *params;
    int offset;
    struct mchp_pk_cnx *cnx;
};

typedef enum bk_key_source_id {
    /**
     * \brief Key derived from PUF
     *
     * \details The key is derived in a direct line from the SRAM PUF's device
     * unique start-up data. Keys derived in this way can always be exactly
     * rederived by calling cryptographic functions with the same arguments in
     * the same cryptographic context, and on the same device.
     */
    BK_KEY_SOURCE_PUF_DERIVED = 0x00,
    /**
     * \brief Randomly generated key
     *
     * \details The key is randomly generated using BK's internal
     * cryptographically secure random number generator which is seeded by
     * entropy coming from the device noise. Keys generated in this way cannot
     * be rederived by cryptographic functions.
     */
    BK_KEY_SOURCE_RNG_DERIVED = 0x01,
    /**
     * \brief Key provided by user
     *
     * \details The key is an external value which is provided by the calling
     * application to the bk_create_private_key() function.
     */
    BK_KEY_SOURCE_USER_PROVIDED = 0x02,
} bk_key_source_id_t;

/** \brief Elliptic curve key (code)s marked with this purpose flag can be used for elliptic curve key agreement (ECDH)
 *  and en/decryption functions (ECIES, cryptogram).
 */
#define BK_ECC_KEY_PURPOSE_ECDH               ((bk_ecc_key_purpose_t)0x01)

/** \brief Elliptic curve key (code)s marked with this purpose flag can be used for elliptic curve signature (ECDSA)
 * generation and verification functions (ECDSA-sign, ECDSA-verify, CSR, self-signed certificate).
 */
#define BK_ECC_KEY_PURPOSE_ECDSA              ((bk_ecc_key_purpose_t)0x02)

/** \brief This is the combination of the ECDH and ECDSA flags. Elliptic curve key (code)s marked with this purpose flag
 * can be used for elliptic curve key agreement (ECDH) en/decryption functions (ECIES, cryptogram) as well as signature
 * (ECDSA) generation and verification functions (ECDSA-sign, ECDSA-verify, CSR, self-signed certificate)
 */
#define BK_ECC_KEY_PURPOSE_ECDH_AND_ECDSA     ((bk_ecc_key_purpose_t)(0x01 | 0x02))

typedef uint8_t bk_key_purpose_t;
typedef bk_key_purpose_t bk_ecc_key_purpose_t;
typedef bk_key_source_id_t bk_ecc_key_source_t;
typedef uint8_t bk_ecc_private_key_code_t[BK_ECC_PRIVATE_KEY_CODE_SIZE_BYTES];

// 宣告需要的 API 原型 (或引自 Boot ROM API 標頭檔)

typedef struct mchp_pk_cnx *(*mchp_pk_init_td)(uint32_t *cnxmem, size_t cnxmsz);
typedef uint8_t (*rom_puf_init_td)(uint8_t *pufSRAMBuff,
                                   const uint16_t pufSRAMBuffSize,
                                   uint8_t *pufActCodeBuff, bool genActCode);
typedef uint8_t (*bk_create_private_key_td)(
    const bk_ecc_curve_t curve, const bk_ecc_key_purpose_t purpose_flags,
    const uint8_t *const usage_context, const uint32_t usage_context_length,
    const bk_ecc_key_source_t key_source, const uint8_t *const private_key,
    bk_ecc_private_key_code_t *const private_key_code);
typedef uint8_t (*bk_ecdsa_sign_td)(
    const bk_ecc_private_key_code_t *const private_key_code,
    const bool deterministic_signature, const uint8_t *const message,
    const uint32_t message_length, const bool message_is_hash,
    uint8_t *const signature, uint16_t *const signature_length);

#define PK_CNX_MEM_SIZE 64

// 緩衝區宣告 (必須滿足 32-bit 對齊)
__attribute__((aligned(4))) static uint32_t pk_cnx_mem[PK_CNX_MEM_SIZE / 4];
__attribute__((aligned(4))) static uint8_t sram_puf_buf[1024];
__attribute__((aligned(4))) static uint8_t act_code_buf[1024];

static void ecdsa_puf_sign_example(void) {
    iid_return_t ret;

    // ------------------------------------------------------------------
    // 步驟 1: PKE Engine 與 Crypto SRAM 初始化
    // ------------------------------------------------------------------
    struct mchp_pk_cnx *pke_cnx = mchp_pk_init(pk_cnx_mem, PK_CNX_MEM_SIZE);
    if (pke_cnx == NULL) {
        // PKE 初始化失敗處理
        return;
    }

    // 依規範，呼叫 rom_puf_init 前先清空 0x00127180 全域區域 (128 bytes)
    memset((void *)0x00127180, 0, 128);

    // ------------------------------------------------------------------
    // 步驟 2: 初始化 PUF (BroadKey) 模組
    // ------------------------------------------------------------------
    // genActCode: true 代表重新生成 Activation Code；false 則使用已儲存的代碼
    ret = rom_puf_init(sram_puf_buf, sizeof(sram_puf_buf), act_code_buf, true);
    if (ret != IID_SUCCESS) {
        // PUF 初始化失敗處理
        LOG_ERR("rom_puf_init: %d",ret);
        return;
    }

    // ------------------------------------------------------------------
    // 步驟 3: 建立保護格式的私鑰碼 (Private Key Code)
    // ------------------------------------------------------------------
    // 假設使用 NIST P-256 曲線，且由使用者提供 32-byte 原始私鑰
    uint8_t raw_priv_key[32] = {0}; /* ... 32-byte raw private key data ... */
    bk_ecc_private_key_code_t priv_key_code;

    // ret = bk_create_private_key(
    //     BK_ECC_CURVE_NIST_P256,          // 橢圓曲線類型
    //     BK_ECC_KEY_PURPOSE_ECDH,        // BK_ECC_KEY_PURPOSE_SIGN,         // 密鑰用途標誌
    //     NULL, 0,                         // Usage context (可選)
    //     BK_KEY_SOURCE_USER_PROVIDED,    // BK_ECC_KEY_SOURCE_USER_PROVIDED, // 密鑰來源 (USER_PROVIDED /
    //                                      // DEVICE_UNIQUE / RANDOM)
    //     raw_priv_key,                    // 原始私鑰緩衝區
    //     &priv_key_code                   // 輸出的私鑰碼結構
    // );

    // if (ret != IID_SUCCESS) {
    //     // 私鑰碼生成失敗處理
    //     LOG_ERR("bk_create_private_key: %d",ret);
    //     return;
    // }

    // ------------------------------------------------------------------
    // 步驟 4: 執行 ECDSA 簽章 (bk_ecdsa_sign)
    // ------------------------------------------------------------------
    uint8_t message_hash[32] = {0}; /* ... 32-byte SHA-256 hash 值 ... */
    uint8_t signature;
    uint16_t sig_len = sizeof(signature);

    // ret = bk_ecdsa_sign(
    //     &priv_key_code, // 步驟 3 建立的私鑰碼
    //     false,        // deterministic_signature (false: 標準非確定性/隨機種子)
    //     message_hash, // 待簽章的訊息或 Hash 緩衝區
    //     sizeof(message_hash), // 訊息/Hash 長度
    //     true,      // message_is_hash (true: 輸入已是 Hash；false: 原始訊息)
    //     signature, // 輸出簽章緩衝區 (R + S)
    //     &sig_len   // 傳入緩衝區大小，完成後傳出實際簽章長度
    // );

    // if (ret == IID_SUCCESS) {
    //     // 簽章成功，signature 包含輸出的 ECDSA 簽章結果
    //     LOG_INF("ECDSA PASS");
    // }
}



#include <zephyr/init.h>

static int init_config(void) {
    int ret;

    LOG_INF("--- Zephyr PUF test ---");
    ecdsa_puf_sign_example();

    return 0;
}


SYS_INIT(init_config, APPLICATION, 0);
