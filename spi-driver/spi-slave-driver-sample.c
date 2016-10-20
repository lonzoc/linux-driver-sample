/* 
 * SPI Slave Driver sample.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/delay.h>
 
#define SPI_DRIVER_NAME "spi-slave-samp"

#define SPI_MAX_TRANS_SIZE    (0x1 << 6)
#define MASK_8BIT 0xFF

/**
 * samp_spi_read - read device register through spi bus
 * @dev: pointer to device data
 * @addr: register address, bytes of register address is set in
 		  dev->reg_len
 * @data: read buffer
 * @len: bytes to read
 * return: 0 - read ok, -EBUSERR - spi transter error
 * 0xF0 - REG_H - REG_L - 0xF1 - data
*/
static int samp_spi_read(struct samp_device *dev, u32 addr,
		u8 *data, u32 len) 
{
	struct spi_device *spi = to_spi_device(dev->dev);
	struct spi_transfer xfers;
	struct spi_message spi_msg;
	u8 buffer[4 + SPI_MAX_TRANS_SIZE];
	u32 remain, trans_len, offset = 0;
	int r = 0;

	remain = len;
	while (remain > 0) {
		spi_message_init(&spi_msg);
		memset(&xfers, 0x00, sizeof(xfers));
		memset(&buffer[4], 0x00, SPI_MAX_TRANS_SIZE);

		/* set register address */
		buffer[0] = SAMP_SPI_WR;
		buffer[1] = ((addr + offset) >> 8) & MASK_8BIT;
		buffer[2] = (addr + offset) & MASK_8BIT;

		xfers.tx_buf = buffer;
		xfers.len = 3;
		xfers.cs_change = 1;
		spi_message_add_tail(&xfers, &spi_msg);
		r = spi_sync(spi, &spi_msg);
		if (r < 0) {
			pr_err("Spi transfer error:%d\n",r);
			return r;
		}
		
		spi_message_init(&spi_msg);
		memset(&xfers, 0x00, sizeof(xfers));
		buffer[3] = SAMP_SPI_RD;
		trans_len = min(remain, SPI_MAX_TRANS_SIZE);
		xfers.tx_buf = &buffer[3];
		xfers.rx_buf = buffer;
		xfers.len = 1 + trans_len;
		xfers.cs_change = 1;
		spi_message_add_tail(&xfers, &spi_msg);
		r = spi_sync(spi, &spi_msg);
		if (!r) {
			memcpy(data + offset, &buffer[1], trans_len);
			offset += trans_len;
			remain -= trans_len;
		} else {
			pr_err("Fialed to read: %04X len: %d, errno: %d\n",
				addr + offset, trans_len, r);
			break;
		}
	}

	//spi_message_free(spi_msg);
	return r;
}

/**
 * samp_spi_write - write data to reg through spi bus
 * @dev: pointer to device data
 * @addr: register address, bytes of register address is set in
 		  dev->reg_len
 * @data: write buffer
 * @len: bytes to write
 * return: 0 - write ok, -EBUSERR - spi transter error
 * 0xF0 - REG_H - REG_L - 0xF1 - data
*/
static int samp_spi_write(struct samp_ts_device *dev, u32 addr,
		u8 *data, u32 len)
{	
	struct spi_device *spi = to_spi_device(dev->dev);
	struct spi_transfer xfers = {0};
	struct spi_message spi_msg;
	u8 buffer[5 + SPI_MAX_TRANS_SIZE];
	u32 remain, trans_len, offset = 0;
	int r = 0;

/*
	spi_msg = spi_message_alloc(1, GFP_KERNEL);
	if (!spi_msg) {
		ts_err("Failed to alloc memory for spi_msg");
		return -ENOMEM;
	}
	xfers = (struct spi_transfer *)(spi_msg + 1);
*/

	/* message init */
	buffer[0] = SAMP_SPI_WR;
	remain = len;
	while (remain > 0) {
		spi_message_init(&spi_msg);
		memset(&xfers, 0x00, sizeof(xfers));

		trans_len = min(remain, SPI_MAX_TRANS_SIZE);
		memcpy(&buffer[5], data + offset, trans_len);
		xfers.tx_buf = buffer;
		xfers.len = 5 + trans_len;
		xfers.cs_change = 1;
		spi_message_add_tail(&xfers, &spi_msg);
		
		/* set register address */
		buffer[1] = ((addr + offset) >> 8) & MASK_8BIT;
		buffer[2] = (addr + offset) & MASK_8BIT;
		buffer[3] = (trans_len >> 8) & MASK_8BIT;
		buffer[4] = trans_len & MASK_8BIT;

		r = spi_sync(spi, &spi_msg);
		if (!r) {
			offset += trans_len;
			remain -= trans_len;
		} else {
			pr_err("Fialed to read: %04X len: %d, errno: %d\n",
				addr + offset, trans_len, r);
			break;
		}
	}

	//spi_message_free(spi_msg);
	return r;
}

/**
 * samp_spi_probe - driver probe spi slave device
 * 
 */
static int samp_spi_probe(struct spi_device *spi)
{
	struct samp_device *samp_spi_dev;
	int r = 0;

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	spi->max_speed_hz = 1000 * 1000;
	spi_setup(spi);

	samp_spi_dev = devm_kzalloc(&spi->dev,
		sizeof(struct platform_device), GFP_KERNEL);
	if (!samp_spi_dev) {
		return -ENOMEM;
	}

	smap_spi_dev->dev = &spi->dev;
	spi_set_drvdata(spi, samp_spi_dev);

	return r;
}

static int samp_spi_remove(struct spi_device *device)
{
	struct platform_device *pdev = spi_get_drvdata(device);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id spi_match_table[] = {
	{.compatible = "vendor,chipset",},
	{ },
};
#endif

static const struct spi_device_id spi_id_table[] = {
	{SPI_DRIVER_NAME, 0},
	{ },
};

static struct spi_driver samp_spi_driver = {
	.driver = {
		.name = SPI_DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = spi_match_table,
#endif
	},
	.probe = samp_spi_probe,
	.remove = googdix_boston_spi_remove,
	.id_table = spi_id_table,
};

static int __init samp_spi_init(void)
{
	spi_register_driver(&samp_spi_driver);
	return 0;
}

static void __exit samp_spi_exit(void)
{
	return;
}

module_init(samp_spi_init);
module_exit(samp_spi_exit);

MODULE_DESCRIPTION("SPI Slave Sample Driver");
MODULE_AUTHOR("Yulong Cai");
MODULE_LICENSE("GPL v2");
