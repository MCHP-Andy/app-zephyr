
source .venv/Scripts/activate

export ZES_ENABLE_SYSMAN=1
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr

export WORKSPACE=$(pwd)
export ZEPHYR_SDK_INSTALL_DIR=${WORKSPACE}/../zephyr_main/zephyr-sdk-0.16.8
export MEC172X_SPI_GEN=${WORKSPACE}/tools/kf_spi_gen_revb/Kenai_Fjords_spi_gen.exe
export MEC172X_SPI_CFG=${WORKSPACE}/tools/kf_spi_gen_revb/spi_cfg.ini

echo ================ Zephyr ENV =======================
echo ZES_ENABLE_SYSMAN=$ZES_ENABLE_SYSMAN
echo ZEPHYR_TOOLCHAIN_VARIANT=$ZEPHYR_TOOLCHAIN_VARIANT
echo ZEPHYR_SDK_INSTALL_DIR=$ZEPHYR_SDK_INSTALL_DIR
echo ================= MCHP ENV ========================
echo MEC5_SPI_GEN=$MEC172X_SPI_GEN
