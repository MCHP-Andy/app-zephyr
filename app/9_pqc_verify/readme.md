# 9_pqc_verify

## Overview
A Post-Quantum Cryptography (FIPS 204 / ML-DSA-87) signature verification example. It verifies with both the MEC Boot ROM API and wolfSSL so the results can be compared.

- **Boot ROM API** (`CONFIG_BOARD_MEC172XMODULAR_ASSY6930`)
  - `api_mchp_fips204_verify()`: verifies a 48-byte message (SHA-384 digest) in one call
  - `api_mchp_fips204_verify_begin/update/finish()`: streams a 128KB message in 4KB chunks
- **wolfSSL** (`CONFIG_WOLFSSL_MLDSA`)
  - `wc_MlDsaKey_VerifyCtx()`: verifies the test message in one call
  - Computes `mu` chunk by chunk with SHAKE256, then verifies the 128KB message with `wc_MlDsaKey_VerifyMu()`

The test public key, signature and message are embedded in `src/pqc_verify.c`.
`src/boot_rom_puf.c` (PUF / ECDSA example) is not currently included in the build.

## How to Build

### Environment Setup
Run from the project root:
```sh
source tools/script/setup.sh
```

### Build
MEC172x board (Boot ROM API + wolfSSL):
```sh
west build -p -b mec172xmodular_assy6930 app/9_pqc_verify
```

QEMU Cortex-M3 (wolfSSL only, the Boot ROM API is not available):
```sh
west build -p -b qemu_cortex_m3 app/9_pqc_verify/
```

## Run on QEMU
```sh
west build -t run
```

To exit QEMU, press `CTRL+a`, then `x`.

Sample log:
```
[QEMU] CPU: cortex-m3
qemu-system-arm: warning: nic stellaris_enet.0 has no peer


*** Booting Zephyr OS build v3.7.2 ***
[00:00:00.000,000] <inf> pqc_verify: --- Zephyr Boot rom test ---
[00:00:00.000,000] <inf> pqc_verify: ML-DSA-87 public key loaded
[00:00:00.000,000] <inf> pqc_verify: Running FIPS 204 Level 5 signature verification (heavy computation)...
[00:00:00.650,000] <inf> pqc_verify: [PASS] FIPS 204 (ML-DSA-87) signature verified, firmware is valid
[00:00:00.650,000] <inf> pqc_verify: === Verification done ===
[00:00:00.650,000] <inf> pqc_verify: ML-DSA-87 public key loaded
[00:00:02.280,000] <inf> pqc_verify: Running FIPS 204 Level 5 signature verification (128KB stream)...
[00:00:02.890,000] <inf> pqc_verify: [PASS] FIPS 204 (ML-DSA-87) 128KB signature verified
[00:00:02.890,000] <inf> pqc_verify: === Verification done ===
[00:00:02.890,000] <inf> main: Hello World! qemu_cortex_m3/ti_lm3s6965
MCHP:~$
```
