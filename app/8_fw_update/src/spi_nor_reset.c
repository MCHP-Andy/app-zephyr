#define DT_DRV_COMPAT microchip_xec_spi_nor_reset

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(spi_nor_reset, CONFIG_FLASH_LOG_LEVEL);

#define SPI_NOR_CMD_RESET_ENABLE 0x66
#define SPI_NOR_CMD_RESET_MEMORY 0x99
#define SPI_NOR_CMD_READ 0x03
#define SPI_NOR_CMD_PAGE_PROGRAM 0x02
#define SPI_NOR_CMD_READ_STATUS 0x05
#define SPI_NOR_CMD_WRITE_ENABLE 0x06
#define SPI_NOR_CMD_SECTOR_ERASE 0x20
#define SPI_NOR_STATUS_WIP BIT(0)
#define SPI_NOR_ADDRESS_BYTES 3u
#define SPI_NOR_PAGE_SIZE 256u
#define SPI_NOR_SECTOR_SIZE 4096u
#define SPI_NOR_READY_TIMEOUT_MS 10000u
#define SPI_NOR_READY_POLL_MS 1u

struct spi_nor_reset_config {
	struct spi_dt_spec spi;
	size_t flash_size;
#if defined(CONFIG_FLASH_PAGE_LAYOUT)
	struct flash_pages_layout pages_layout;
#endif
};

struct spi_nor_reset_data {
	struct k_mutex lock;
};

static const struct flash_parameters spi_nor_reset_parameters = {
	.write_block_size = 1,
	.erase_value = 0xff,
};

#if defined(CONFIG_FLASH_PAGE_LAYOUT)
static void spi_nor_reset_pages_layout(const struct device *dev,
				       const struct flash_pages_layout **layout,
				       size_t *layout_size)
{
	const struct spi_nor_reset_config *config = dev->config;

	*layout = &config->pages_layout;
	*layout_size = 1;
}
#endif

static bool spi_nor_reset_range_valid(const struct spi_nor_reset_config *config,
				     off_t offset, size_t len)
{
	return offset >= 0 && (uint64_t)offset <= config->flash_size &&
	       len <= config->flash_size - (size_t)offset;
}

static int spi_nor_reset_write_command(const struct device *dev,
				       const uint8_t *command, size_t command_len,
				       const void *data, size_t data_len)
{
	const struct spi_nor_reset_config *config = dev->config;
	struct spi_buf buffers[2] = {
		{
			.buf = (void *)command,
			.len = command_len,
		},
		{
			.buf = (void *)data,
			.len = data_len,
		},
	};
	struct spi_buf_set tx_buffers = {
		.buffers = buffers,
		.count = data_len ? ARRAY_SIZE(buffers) : 1,
	};

	return spi_write_dt(&config->spi, &tx_buffers);
}

static int spi_nor_reset_read_command(const struct device *dev,
				      const uint8_t *command, size_t command_len,
				      void *data, size_t data_len)
{
	const struct spi_nor_reset_config *config = dev->config;
	struct spi_buf tx_buffer = {
		.buf = (void *)command,
		.len = command_len,
	};
	struct spi_buf rx_buffer = {
		.buf = data,
		.len = data_len,
	};
	struct spi_buf_set tx_buffers = {
		.buffers = &tx_buffer,
		.count = 1,
	};
	struct spi_buf_set rx_buffers = {
		.buffers = &rx_buffer,
		.count = 1,
	};

	int ret = spi_transceive_dt(&config->spi, &tx_buffers, &rx_buffers);

	if (ret) {
		LOG_ERR("SPI command 0x%02x (%zu TX, %zu RX) failed: %d",
			command[0], command_len, data_len, ret);
	}

	return ret;
}

static void spi_nor_reset_make_address(uint8_t *command, uint8_t opcode, off_t offset)
{
	command[0] = opcode;
	command[1] = (uint8_t)(offset >> 16);
	command[2] = (uint8_t)(offset >> 8);
	command[3] = (uint8_t)offset;
}

static int spi_nor_reset_wait_ready(const struct device *dev)
{
	uint8_t command = SPI_NOR_CMD_READ_STATUS;
	uint8_t status;
	int64_t deadline = k_uptime_get() + SPI_NOR_READY_TIMEOUT_MS;
	int ret;

	do {
		ret = spi_nor_reset_read_command(dev, &command, sizeof(command),
						&status, sizeof(status));
		if (ret) {
			return ret;
		}
		if (!(status & SPI_NOR_STATUS_WIP)) {
			return 0;
		}
		k_msleep(SPI_NOR_READY_POLL_MS);
	} while (k_uptime_get() < deadline);

	return -ETIMEDOUT;
}

static int spi_nor_reset_write_enable(const struct device *dev)
{
	uint8_t command = SPI_NOR_CMD_WRITE_ENABLE;

	return spi_nor_reset_write_command(dev, &command, sizeof(command), NULL, 0);
}

static int spi_nor_reset_command(const struct device *dev)
{
	const struct spi_nor_reset_config *config = dev->config;
	uint8_t command = SPI_NOR_CMD_RESET_ENABLE;
	struct spi_buf tx_buffer = {
		.buf = &command,
		.len = sizeof(command),
	};
	struct spi_buf_set tx_buffers = {
		.buffers = &tx_buffer,
		.count = 1,
	};
	int ret;

	ret = spi_write_dt(&config->spi, &tx_buffers);
	if (ret) {
		LOG_ERR("Reset-enable command failed: %d", ret);
		return ret;
	}

	command = SPI_NOR_CMD_RESET_MEMORY;
	ret = spi_write_dt(&config->spi, &tx_buffers);
	if (ret) {
		LOG_ERR("Reset-memory command failed: %d", ret);
	}

	return ret;
}

static int spi_nor_reset_read(const struct device *dev, off_t offset,
			      void *data, size_t len)
{
	const struct spi_nor_reset_config *config = dev->config;
	uint8_t command[SPI_NOR_ADDRESS_BYTES + 1u];
	int ret;

	if (!spi_nor_reset_range_valid(config, offset, len) || (len && data == NULL)) {
		return -EINVAL;
	}
	if (!len) {
		return 0;
	}

	spi_nor_reset_make_address(command, SPI_NOR_CMD_READ, offset);
	k_mutex_lock(&((struct spi_nor_reset_data *)dev->data)->lock, K_FOREVER);
	ret = spi_nor_reset_wait_ready(dev);
	if (!ret) {
		ret = spi_nor_reset_read_command(dev, command, sizeof(command), data, len);
	}
	k_mutex_unlock(&((struct spi_nor_reset_data *)dev->data)->lock);
	if (ret) {
		LOG_ERR("Flash read at 0x%lx (%zu bytes) failed: %d",
			(long)offset, len, ret);
	}

	return ret;
}

static int spi_nor_reset_write(const struct device *dev, off_t offset,
			       const void *data, size_t len)
{
	const struct spi_nor_reset_config *config = dev->config;
	const uint8_t *source = data;
	uint8_t command[SPI_NOR_ADDRESS_BYTES + 1u];
	int ret = 0;

	if (!spi_nor_reset_range_valid(config, offset, len) || (len && data == NULL)) {
		return -EINVAL;
	}
	if (!len) {
		return 0;
	}

	k_mutex_lock(&((struct spi_nor_reset_data *)dev->data)->lock, K_FOREVER);
	while (len) {
		size_t chunk = MIN(len, SPI_NOR_PAGE_SIZE -
					    ((size_t)offset % SPI_NOR_PAGE_SIZE));

		ret = spi_nor_reset_wait_ready(dev);
		if (ret) {
			break;
		}
		ret = spi_nor_reset_write_enable(dev);
		if (ret) {
			break;
		}
		spi_nor_reset_make_address(command, SPI_NOR_CMD_PAGE_PROGRAM, offset);
		ret = spi_nor_reset_write_command(dev, command, sizeof(command),
						 source, chunk);
		if (ret) {
			break;
		}
		ret = spi_nor_reset_wait_ready(dev);
		if (ret) {
			break;
		}

		offset += chunk;
		source += chunk;
		len -= chunk;
	}
	k_mutex_unlock(&((struct spi_nor_reset_data *)dev->data)->lock);

	return ret;
}

static int spi_nor_reset_erase(const struct device *dev, off_t offset, size_t size)
{
	const struct spi_nor_reset_config *config = dev->config;
	uint8_t command[SPI_NOR_ADDRESS_BYTES + 1u];
	int ret = 0;

	if (!spi_nor_reset_range_valid(config, offset, size) ||
	    ((size_t)offset % SPI_NOR_SECTOR_SIZE) != 0u ||
	    (size % SPI_NOR_SECTOR_SIZE) != 0u) {
		return -EINVAL;
	}
	if (!size) {
		return 0;
	}

	k_mutex_lock(&((struct spi_nor_reset_data *)dev->data)->lock, K_FOREVER);
	while (size) {
		ret = spi_nor_reset_wait_ready(dev);
		if (ret) {
			break;
		}
		ret = spi_nor_reset_write_enable(dev);
		if (ret) {
			break;
		}
		spi_nor_reset_make_address(command, SPI_NOR_CMD_SECTOR_ERASE, offset);
		ret = spi_nor_reset_write_command(dev, command, sizeof(command), NULL, 0);
		if (ret) {
			break;
		}
		ret = spi_nor_reset_wait_ready(dev);
		if (ret) {
			break;
		}

		offset += SPI_NOR_SECTOR_SIZE;
		size -= SPI_NOR_SECTOR_SIZE;
	}
	k_mutex_unlock(&((struct spi_nor_reset_data *)dev->data)->lock);

	return ret;
}

static const struct flash_parameters *spi_nor_reset_get_parameters(const struct device *dev)
{
	ARG_UNUSED(dev);

	return &spi_nor_reset_parameters;
}

#if defined(CONFIG_FLASH_EX_OP_ENABLED)
static int spi_nor_reset_ex_op(const struct device *dev, uint16_t code,
			       const uintptr_t in, void *out)
{
	ARG_UNUSED(in);
	ARG_UNUSED(out);

	if (code != FLASH_EX_OP_RESET) {
		return -ENOTSUP;
	}

	k_mutex_lock(&((struct spi_nor_reset_data *)dev->data)->lock, K_FOREVER);
	int ret = spi_nor_reset_command(dev);
	k_mutex_unlock(&((struct spi_nor_reset_data *)dev->data)->lock);

	return ret;
}
#endif

static int spi_nor_reset_init(const struct device *dev)
{
	const struct spi_nor_reset_config *config = dev->config;

	if (!spi_is_ready_dt(&config->spi)) {
		LOG_ERR("SPI bus is not ready");
		return -ENODEV;
	}

	k_mutex_init(&((struct spi_nor_reset_data *)dev->data)->lock);
	return spi_nor_reset_command(dev);
}

static const struct flash_driver_api spi_nor_reset_api = {
	.read = spi_nor_reset_read,
	.write = spi_nor_reset_write,
	.erase = spi_nor_reset_erase,
	.get_parameters = spi_nor_reset_get_parameters,
#if defined(CONFIG_FLASH_PAGE_LAYOUT)
	.page_layout = spi_nor_reset_pages_layout,
#endif
#if defined(CONFIG_FLASH_EX_OP_ENABLED)
	.ex_op = spi_nor_reset_ex_op,
#endif
};

#define SPI_NOR_RESET_DEFINE(inst) \
	static struct spi_nor_reset_data spi_nor_reset_data_##inst; \
	static const struct spi_nor_reset_config spi_nor_reset_config_##inst = { \
		.spi = SPI_DT_SPEC_INST_GET(inst, \
			SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8), 0), \
		.flash_size = DT_INST_PROP(inst, size) / 8u, \
		IF_ENABLED(CONFIG_FLASH_PAGE_LAYOUT, ( \
			.pages_layout = { \
				.pages_count = DT_INST_PROP(inst, size) / 8u / \
					SPI_NOR_SECTOR_SIZE, \
				.pages_size = SPI_NOR_SECTOR_SIZE, \
			},)) \
	}; \
	DEVICE_DT_INST_DEFINE(inst, spi_nor_reset_init, NULL, \
			      &spi_nor_reset_data_##inst, \
			      &spi_nor_reset_config_##inst, POST_KERNEL, \
			      CONFIG_FLASH_INIT_PRIORITY, &spi_nor_reset_api)

DT_INST_FOREACH_STATUS_OKAY(SPI_NOR_RESET_DEFINE)