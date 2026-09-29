#define DT_DRV_COMPAT microchip_xec_qmspi_pio

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(spi_xec_pio, CONFIG_SPI_LOG_LEVEL);

#include <spi/spi_context.h>

#define QMSPI_MODE_OFS              0x00u
#define QMSPI_CTRL_OFS              0x04u
#define QMSPI_EXE_OFS               0x08u
#define QMSPI_STS_OFS               0x10u
#define QMSPI_IEN_OFS               0x18u
#define QMSPI_TX_FIFO_OFS           0x20u
#define QMSPI_RX_FIFO_OFS           0x24u
#define QMSPI_CSTM_OFS              0x28u
#define QMSPI_DESCR0_OFS            0x30u
#define QMSPI_MODE_ALT1_OFS         0xc0u
#define QMSPI_TAPS_OFS               0xd0u
#define QMSPI_TAPS_ADJ_OFS           0xd4u
#define QMSPI_TAPS_CTRL_OFS          0xd8u

#define QMSPI_MODE_ACTIVATE         BIT(0)
#define QMSPI_MODE_SOFT_RESET       BIT(1)
#define QMSPI_MODE_SIG_MASK         0x00000700u
#define QMSPI_MODE_CS_MASK          0x00003000u
#define QMSPI_MODE_FDIV_MASK        0xffff0000u
#define QMSPI_MODE_SIG_POS          8u
#define QMSPI_MODE_CS_POS           12u
#define QMSPI_MODE_FDIV_POS         16u

#define QMSPI_CTRL_IFM_MASK         0x03u
#define QMSPI_CTRL_IFM_SINGLE       0x00u
#define QMSPI_CTRL_TX_DATA          BIT(2)
#define QMSPI_CTRL_RX_ENABLE        BIT(6)
#define QMSPI_CTRL_UNITS_BITS       0x00000000u
#define QMSPI_CTRL_UNITS_BYTES      BIT(10)
#define QMSPI_CTRL_DESCR_LAST       BIT(16)
#define QMSPI_CTRL_DESCR_ENABLE     BIT(16)
#define QMSPI_CTRL_DESCR0           0x00000000u
#define QMSPI_CTRL_MAX_UNITS        0x7fffu
#define QMSPI_CTRL_UNITS_POS        17u

#define QMSPI_EXE_START             BIT(0)
#define QMSPI_EXE_STOP              BIT(1)
#define QMSPI_EXE_CLEAR_FIFOS       BIT(2)

#define QMSPI_STS_DONE              BIT(0)
#define QMSPI_STS_TX_ERROR          BIT(2)
#define QMSPI_STS_RX_ERROR          BIT(3)
#define QMSPI_STS_PROGRAM_ERROR     BIT(4)
#define QMSPI_STS_TX_FULL           BIT(8)
#define QMSPI_STS_RX_EMPTY          BIT(13)
#define QMSPI_STS_ACTIVE            BIT(16)
#define QMSPI_STS_CLEAR_MASK        0x0000cc7fu
#define QMSPI_STS_ERROR_MASK        (QMSPI_STS_TX_ERROR | QMSPI_STS_RX_ERROR | \
					 QMSPI_STS_PROGRAM_ERROR)

#define QMSPI_FIFO_DEPTH            8u
#define QMSPI_TRANSFER_TIMEOUT_MS   100u

struct spi_xec_config {
	uintptr_t base;
	uint32_t input_clock_hz;
	uint32_t cs_timing;
	uintptr_t pcr_sleep_reg;
	uint8_t mode3_signaling;
	uint8_t pcr_sleep_bit;
	uint8_t chip_select;
	const struct pinctrl_dev_config *pinctrl;
};

struct spi_xec_data {
	struct spi_context ctx;
};

static inline uint32_t qmspi_read(const struct spi_xec_config *cfg, uint32_t offset)
{
	return sys_read32(cfg->base + offset);
}

static inline void qmspi_write(const struct spi_xec_config *cfg, uint32_t offset,
			       uint32_t value)
{
	sys_write32(value, cfg->base + offset);
}

static int qmspi_reset(const struct spi_xec_config *cfg)
{
	uint32_t timing[5];
	uint32_t mode;

	mode = qmspi_read(cfg, QMSPI_MODE_OFS);
	timing[0] = qmspi_read(cfg, QMSPI_CSTM_OFS);
	timing[1] = qmspi_read(cfg, QMSPI_MODE_ALT1_OFS);
	timing[2] = qmspi_read(cfg, QMSPI_TAPS_OFS);
	timing[3] = qmspi_read(cfg, QMSPI_TAPS_ADJ_OFS);
	timing[4] = qmspi_read(cfg, QMSPI_TAPS_CTRL_OFS);

	qmspi_write(cfg, QMSPI_MODE_OFS, QMSPI_MODE_SOFT_RESET);
	for (uint32_t timeout = 0; timeout < QMSPI_TRANSFER_TIMEOUT_MS; timeout++) {
		if (!(qmspi_read(cfg, QMSPI_MODE_OFS) & QMSPI_MODE_SOFT_RESET)) {
			qmspi_write(cfg, QMSPI_MODE_OFS,
				    mode & ~(QMSPI_MODE_ACTIVATE | QMSPI_MODE_SOFT_RESET));
			qmspi_write(cfg, QMSPI_CSTM_OFS, timing[0]);
			qmspi_write(cfg, QMSPI_MODE_ALT1_OFS, timing[1]);
			qmspi_write(cfg, QMSPI_TAPS_OFS, timing[2]);
			qmspi_write(cfg, QMSPI_TAPS_ADJ_OFS, timing[3]);
			qmspi_write(cfg, QMSPI_TAPS_CTRL_OFS, timing[4]);
			qmspi_write(cfg, QMSPI_IEN_OFS, 0u);
			qmspi_write(cfg, QMSPI_CTRL_OFS, QMSPI_CTRL_IFM_SINGLE);
			qmspi_write(cfg, QMSPI_EXE_OFS, QMSPI_EXE_CLEAR_FIFOS);
			qmspi_write(cfg, QMSPI_STS_OFS, QMSPI_STS_CLEAR_MASK);
			return 0;
		}
		k_busy_wait(1);
	}

	return -ETIMEDOUT;
}

static int qmspi_wait_done(const struct spi_xec_config *cfg, bool receive,
			   uint8_t **rx, size_t remaining)
{
	int64_t deadline = k_uptime_get() + QMSPI_TRANSFER_TIMEOUT_MS;

	while (true) {
		uint32_t status = qmspi_read(cfg, QMSPI_STS_OFS);

		if (status & QMSPI_STS_ERROR_MASK) {
			return -EIO;
		}

		if (receive && remaining && !(status & QMSPI_STS_RX_EMPTY)) {
			uint8_t value = sys_read8(cfg->base + QMSPI_RX_FIFO_OFS);

			if (*rx) {
				*(*rx)++ = value;
			}
			remaining--;
		}

		if ((status & QMSPI_STS_DONE) && (!receive || remaining == 0u)) {
			return 0;
		}

		if (k_uptime_get() >= deadline) {
			return -ETIMEDOUT;
		}

		k_busy_wait(1);
	}
}

static int qmspi_transfer_chunk(const struct spi_xec_config *cfg,
				const uint8_t *tx, uint8_t *rx, size_t len,
				bool dummy_tx)
{
	uint32_t descriptor;
	size_t queued = 0;
	size_t received = 0;
	int64_t deadline = k_uptime_get() + QMSPI_TRANSFER_TIMEOUT_MS;
	bool receive = rx != NULL || (tx == NULL && !dummy_tx);

	if (receive) {
		descriptor = QMSPI_CTRL_IFM_SINGLE | QMSPI_CTRL_RX_ENABLE |
			     QMSPI_CTRL_UNITS_BYTES | QMSPI_CTRL_DESCR_LAST |
			     ((uint32_t)len << QMSPI_CTRL_UNITS_POS);
	} else if (dummy_tx) {
		descriptor = QMSPI_CTRL_IFM_SINGLE | QMSPI_CTRL_UNITS_BITS |
			     QMSPI_CTRL_DESCR_LAST |
			     ((uint32_t)(len * 8u) << QMSPI_CTRL_UNITS_POS);
	} else {
		descriptor = QMSPI_CTRL_IFM_SINGLE | QMSPI_CTRL_TX_DATA |
			     QMSPI_CTRL_UNITS_BYTES | QMSPI_CTRL_DESCR_LAST |
			     ((uint32_t)len << QMSPI_CTRL_UNITS_POS);
	}

	qmspi_write(cfg, QMSPI_DESCR0_OFS, descriptor);
	qmspi_write(cfg, QMSPI_CTRL_OFS,
		    QMSPI_CTRL_IFM_SINGLE | QMSPI_CTRL_DESCR_ENABLE |
		    QMSPI_CTRL_DESCR0);
	qmspi_write(cfg, QMSPI_STS_OFS, QMSPI_STS_CLEAR_MASK);

	if (!receive && !dummy_tx) {
		while (queued < len && queued < QMSPI_FIFO_DEPTH) {
			if (qmspi_read(cfg, QMSPI_STS_OFS) & QMSPI_STS_TX_FULL) {
				break;
			}
			sys_write8(tx[queued++], cfg->base + QMSPI_TX_FIFO_OFS);
		}
	}

	qmspi_write(cfg, QMSPI_EXE_OFS, QMSPI_EXE_START);

	if (!receive && !dummy_tx) {
		while (queued < len) {
			uint32_t status = qmspi_read(cfg, QMSPI_STS_OFS);

			if (status & QMSPI_STS_ERROR_MASK) {
				return -EIO;
			}
			if (!(status & QMSPI_STS_TX_FULL)) {
				sys_write8(tx[queued++], cfg->base + QMSPI_TX_FIFO_OFS);
			}
			if (k_uptime_get() >= deadline) {
				return -ETIMEDOUT;
			}
			k_busy_wait(1);
		}
	}

	if (receive) {
		while (received < len) {
			uint32_t status = qmspi_read(cfg, QMSPI_STS_OFS);

			if (status & QMSPI_STS_ERROR_MASK) {
				return -EIO;
			}
			if (!(status & QMSPI_STS_RX_EMPTY)) {
				uint8_t value = sys_read8(cfg->base + QMSPI_RX_FIFO_OFS);

				if (rx) {
					rx[received] = value;
				}
				received++;
			}
			if (k_uptime_get() >= deadline) {
				return -ETIMEDOUT;
			}
			k_busy_wait(1);
		}
	}

	return qmspi_wait_done(cfg, false, NULL, 0);
}

static int qmspi_transfer_buffer(const struct spi_xec_config *cfg,
				 const struct spi_buf *buf, bool is_tx)
{
	const uint8_t *tx = is_tx ? buf->buf : NULL;
	uint8_t *rx = is_tx ? NULL : buf->buf;
	size_t remaining = buf->len;
	bool dummy_tx = is_tx && buf->buf == NULL;

	while (remaining) {
		size_t max_chunk = dummy_tx ? QMSPI_CTRL_MAX_UNITS / 8u :
						 QMSPI_CTRL_MAX_UNITS;
		size_t chunk = MIN(remaining, max_chunk);
		int ret = qmspi_transfer_chunk(cfg, tx, rx, chunk, dummy_tx);

		if (ret) {
			return ret;
		}
		if (tx) {
			tx += chunk;
		}
		if (rx) {
			rx += chunk;
		}
		remaining -= chunk;
	}

	return 0;
}

static int qmspi_configure(const struct device *dev, const struct spi_config *spi_cfg)
{
	const struct spi_xec_config *cfg = dev->config;
	struct spi_xec_data *data = dev->data;
	uint32_t mode;
	uint32_t signaling;
	uint32_t divider;
	uint32_t operation;

	if (!spi_cfg) {
		return -EINVAL;
	}
	if (spi_context_configured(&data->ctx, spi_cfg)) {
		return 0;
	}
	operation = spi_cfg->operation;
	if (operation & (SPI_TRANSFER_LSB | SPI_OP_MODE_SLAVE | SPI_MODE_LOOP |
			 SPI_HALF_DUPLEX | SPI_CS_ACTIVE_HIGH | SPI_LOCK_ON)) {
		return -ENOTSUP;
	}
	if (SPI_WORD_SIZE_GET(operation) != 8) {
		return -ENOTSUP;
	}
#ifdef CONFIG_SPI_EXTENDED_MODES
	if ((operation & SPI_LINES_MASK) != SPI_LINES_SINGLE) {
		return -ENOTSUP;
	}
#endif

	mode = qmspi_read(cfg, QMSPI_MODE_OFS) & ~QMSPI_MODE_FDIV_MASK;
	if (spi_cfg->frequency == 0u) {
		divider = 0u;
	} else {
		divider = DIV_ROUND_UP(cfg->input_clock_hz, spi_cfg->frequency);
		if (divider > 0xffffu) {
			divider = 0u;
		} else if (divider == 0u) {
			divider = 1u;
		}
	}
	mode |= (divider << QMSPI_MODE_FDIV_POS) & QMSPI_MODE_FDIV_MASK;
	qmspi_write(cfg, QMSPI_MODE_OFS, mode);

	signaling = 0u;
	if (operation & SPI_MODE_CPHA) {
		signaling |= BIT(0);
	}
	if (operation & SPI_MODE_CPOL) {
		signaling |= BIT(1);
	}
	{
		uint8_t signaling_modes[4] = { 0u, 6u, 1u, cfg->mode3_signaling };

		mode = qmspi_read(cfg, QMSPI_MODE_OFS) & ~QMSPI_MODE_SIG_MASK;
		mode |= (uint32_t)signaling_modes[signaling] << QMSPI_MODE_SIG_POS;
		mode &= ~QMSPI_MODE_CS_MASK;
		mode |= (uint32_t)cfg->chip_select << QMSPI_MODE_CS_POS;
		qmspi_write(cfg, QMSPI_MODE_OFS, mode);
	}

	qmspi_write(cfg, QMSPI_CTRL_OFS, QMSPI_CTRL_IFM_SINGLE);
	qmspi_write(cfg, QMSPI_CSTM_OFS, cfg->cs_timing);
	data->ctx.config = spi_cfg;
	qmspi_write(cfg, QMSPI_MODE_OFS,
		    qmspi_read(cfg, QMSPI_MODE_OFS) | QMSPI_MODE_ACTIVATE);

	return 0;
}

static int qmspi_transceive(const struct device *dev, const struct spi_config *spi_cfg,
			    const struct spi_buf_set *tx_bufs,
			    const struct spi_buf_set *rx_bufs)
{
	const struct spi_xec_config *cfg = dev->config;
	struct spi_xec_data *data = dev->data;
	int ret;

	if (!tx_bufs && !rx_bufs) {
		return 0;
	}
	if (!spi_cfg) {
		return -EINVAL;
	}
	spi_context_lock(&data->ctx, false, NULL, NULL, spi_cfg);
	ret = qmspi_configure(dev, spi_cfg);
	if (ret) {
		LOG_ERR("QMSPI configure failed: %d", ret);
		goto out;
	}

	qmspi_write(cfg, QMSPI_EXE_OFS, QMSPI_EXE_CLEAR_FIFOS);
	spi_context_cs_control(&data->ctx, true);
	if (tx_bufs) {
		for (size_t i = 0; i < tx_bufs->count; i++) {
			ret = qmspi_transfer_buffer(cfg, &tx_bufs->buffers[i], true);
			if (ret) {
				LOG_ERR("QMSPI TX buffer %zu failed: %d (STS=%08x CTRL=%08x)",
					i, ret, qmspi_read(cfg, QMSPI_STS_OFS),
					qmspi_read(cfg, QMSPI_CTRL_OFS));
				goto stop;
			}
		}
	}
	if (rx_bufs) {
		for (size_t i = 0; i < rx_bufs->count; i++) {
			ret = qmspi_transfer_buffer(cfg, &rx_bufs->buffers[i], false);
			if (ret) {
				LOG_ERR("QMSPI RX buffer %zu failed: %d (STS=%08x CTRL=%08x)",
					i, ret, qmspi_read(cfg, QMSPI_STS_OFS),
					qmspi_read(cfg, QMSPI_CTRL_OFS));
				goto stop;
			}
		}
	}
	ret = 0;

stop:
	if (!(spi_cfg->operation & SPI_HOLD_ON_CS) || ret) {
		int64_t deadline = k_uptime_get() + QMSPI_TRANSFER_TIMEOUT_MS;

		qmspi_write(cfg, QMSPI_EXE_OFS, QMSPI_EXE_STOP);
		while (qmspi_read(cfg, QMSPI_STS_OFS) & QMSPI_STS_ACTIVE) {
			if (k_uptime_get() >= deadline) {
				if (!ret) {
					ret = -ETIMEDOUT;
				}
				break;
			}
			k_busy_wait(1);
		}
		spi_context_cs_control(&data->ctx, false);
	}
out:
	spi_context_release(&data->ctx, ret);
	return ret;
}

static int qmspi_release(const struct device *dev, const struct spi_config *spi_cfg)
{
	const struct spi_xec_config *cfg = dev->config;
	struct spi_xec_data *data = dev->data;
	int64_t deadline = k_uptime_get() + QMSPI_TRANSFER_TIMEOUT_MS;

	qmspi_write(cfg, QMSPI_EXE_OFS, QMSPI_EXE_STOP);
	while (qmspi_read(cfg, QMSPI_STS_OFS) & QMSPI_STS_ACTIVE) {
		if (k_uptime_get() >= deadline) {
			spi_context_unlock_unconditionally(&data->ctx);
			return -ETIMEDOUT;
		}
		k_busy_wait(1);
	}
	spi_context_unlock_unconditionally(&data->ctx);
	return 0;
}

static int qmspi_init(const struct device *dev)
{
	const struct spi_xec_config *cfg = dev->config;
	struct spi_xec_data *data = dev->data;
	uint32_t pcr_value;
	int ret;

	ret = pinctrl_apply_state(cfg->pinctrl, PINCTRL_STATE_DEFAULT);
	if (ret) {
		return ret;
	}
	pcr_value = sys_read32(cfg->pcr_sleep_reg);
	sys_write32(pcr_value & ~BIT(cfg->pcr_sleep_bit), cfg->pcr_sleep_reg);

	ret = qmspi_reset(cfg);
	if (ret) {
		return ret;
	}
	spi_context_unlock_unconditionally(&data->ctx);
	return 0;
}

static const struct spi_driver_api qmspi_driver_api = {
	.transceive = qmspi_transceive,
	.release = qmspi_release,
};

#define QMSPI_CS_TIMING(inst) \
	(DT_INST_PROP_OR(inst, dcsckon, 6) | \
	 (DT_INST_PROP_OR(inst, dckcsoff, 4) << 8) | \
	 (DT_INST_PROP_OR(inst, dldh, 6) << 16) | \
	 (DT_INST_PROP_OR(inst, dcsda, 6) << 24))

#define QMSPI_PIO_INIT(inst) \
	PINCTRL_DT_INST_DEFINE(inst); \
	static struct spi_xec_data spi_xec_data_##inst = { \
		SPI_CONTEXT_INIT_LOCK(spi_xec_data_##inst, ctx), \
		SPI_CONTEXT_INIT_SYNC(spi_xec_data_##inst, ctx), \
		SPI_CONTEXT_CS_GPIOS_INITIALIZE(DT_DRV_INST(inst), ctx) \
	}; \
	static const struct spi_xec_config spi_xec_config_##inst = { \
		.base = DT_INST_REG_ADDR(inst), \
		.input_clock_hz = DT_INST_PROP(inst, input_clock_frequency), \
		.cs_timing = QMSPI_CS_TIMING(inst), \
		.pcr_sleep_reg = DT_INST_PROP(inst, pcr_sleep_reg), \
		.mode3_signaling = DT_INST_PROP_OR(inst, mode3_signaling, 7), \
		.pcr_sleep_bit = DT_INST_PROP(inst, pcr_sleep_bit), \
		.chip_select = DT_INST_PROP_OR(inst, chip_select, 0), \
		.pinctrl = PINCTRL_DT_INST_DEV_CONFIG_GET(inst), \
	}; \
	DEVICE_DT_INST_DEFINE(inst, qmspi_init, NULL, &spi_xec_data_##inst, \
			      &spi_xec_config_##inst, POST_KERNEL, \
			      CONFIG_SPI_INIT_PRIORITY, &qmspi_driver_api)

DT_INST_FOREACH_STATUS_OKAY(QMSPI_PIO_INIT)