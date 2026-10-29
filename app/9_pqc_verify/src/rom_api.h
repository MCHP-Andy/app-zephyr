
#pragma once

#define SHAKE256_RATE 136

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define api_efuse_byte_write ((api_efuse_byte_write_td)0x0000f00d)
#define api_efuse_byte_read ((api_efuse_byte_read_td)0x0000f009)
#define api_rom_ver ((api_rom_ver_td)0x0000f001)

#define api_mchp_fips204_verify ((api_mchp_fips204_verify_td)0x0001F32D)
#define api_mchp_fips204_verify_begin                                          \
    ((api_mchp_fips204_verify_begin_td)0x0001F33D)
#define api_mchp_fips204_verify_update                                         \
    ((api_mchp_fips204_verify_update_td)0x0001F341)
#define api_mchp_fips204_verify_finish                                         \
    ((api_mchp_fips204_verify_finish_td)0x0001F345)

typedef struct {
    uint64_t fips202_ctx[25];             // for sha3 state
    uint8_t shake_ctx_buf[SHAKE256_RATE]; // holds non-absorbed data
    uint32_t len;
} mchp_fips204_ctx_t;

typedef uint8_t (*api_efuse_byte_write_td)(uint16_t byte_idx, uint8_t in_data);
typedef uint8_t (*api_efuse_byte_read_td)(uint16_t byte_idx, uint8_t *out_data);
typedef uint32_t (*api_rom_ver_td)(void);

typedef int (*api_mchp_fips204_verify_td)(const uint8_t *sig, uint32_t siglen,
                                          const uint8_t *m, uint32_t mlen,
                                          const uint8_t *ctx, uint16_t ctxlen,
                                          const uint8_t *pk, uint32_t pklen,
                                          uint8_t *random128, uint8_t level);
typedef int (*api_mchp_fips204_verify_begin_td)(
    const uint8_t *ctx, uint16_t ctxlen, const uint8_t *pk, uint32_t pklen,
    uint8_t *random128, mchp_fips204_ctx_t *mchp_fips204_ctx, uint8_t level);
typedef int (*api_mchp_fips204_verify_update_td)(
    const uint8_t *msg, uint32_t mlen, mchp_fips204_ctx_t *mchp_fips204_ctx);
typedef int (*api_mchp_fips204_verify_finish_td)(
    const uint8_t *sig, uint32_t siglen, const uint8_t *pk, uint32_t pklen,
    uint8_t *random128, mchp_fips204_ctx_t *mchp_fips204_ctx, uint8_t level);
