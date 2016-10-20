/*
 * I2C Client Driver sample
 *
 * Authors:  Yulong Cai <caiyulong@goodix.com>
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
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/i2c.h>

#define DT_COMPATIBLE	"vendor,chipset"
#define I2C_DRIVER_NAME "samp_i2c"
#define I2C_MAX_TRANSFER_SIZE	256
#define I2C_ADDR_LENGTH	2
#define I2C_RETRY_TIMES	3

struct samp_device {
	char *name;
	struct device *dev;
};

/**
 * samp_i2c_read - read device register through i2c bus
 * @dev: pointer to device data
 * @addr: register address
 * @data: read buffer
 * @len: bytes to read
 * return: 0 - read ok, < 0 - i2c transter error
*/
static int samp_i2c_read(struct samp_device *dev, u32 addr,
	u8 *data, u32 len)
{
	struct i2c_client *client = to_i2c_client(dev->dev);
	u32  trans_len = 0, pos = 0;
	u8 buf[64], addr_buf[2];
	int retry, r = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = !I2C_M_RD,
			.buf = &addr_buf[0],
			.len = I2C_ADDR_LENGTH,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
		}
	};

	if (likely(len < sizeof(buf))) {
		msgs[1].buf = &buf[0];
	} else {
		msgs[1].buf = kzalloc(I2C_MAX_TRANSFER_SIZE < len?
				I2C_MAX_TRANSFER_SIZE : len, GFP_KERNEL);
		if (msgs[1].buf == NULL) {
			ts_err("Malloc failed");
			return -ENOMEM;
		}
	}

	while (pos != len) {
		if (unlikely(len - pos > I2C_MAX_TRANSFER_SIZE))
			trans_len = I2C_MAX_TRANSFER_SIZE;
		else
			trans_len = len - pos;

		msgs[0].buf[0] = (addr >> 8) & 0xFF;
		msgs[0].buf[1] = addr & 0xFF;
		msgs[1].len = trans_len;

		for (retry = 0; retry < I2C_RETRY_TIMES; retry++) {
			if (likely(i2c_transfer(client->adapter, msgs, 2) == 2)) {
				memcpy(&data[pos], msgs[1].buf, trans_len);
				pos += trans_len;
				addr += trans_len;
				break;
			}
			pr_info("I2c read retry[%d]:0x%x\n", retry + 1, addr);
			msleep(20);
		}
		if (unlikely(retry == I2C_RETRY_TIMES)) {
			pr_err("I2c read failed,dev:%02x,reg:%04x,size:%u\n",
					client->addr, addr, len);
			r = -EIO;
			goto read_exit;
		}
	}

read_exit:
	if (unlikely(len > sizeof(buf)))
		kfree(msgs[1].buf);
	return r;
}

/**
 * samp_i2c_write - write device register through i2c bus
 * @dev: pointer to device data
 * @addr: register address
 * @data: write buffer
 * @len: bytes to write
 * return: 0 - write ok; < 0 - i2c transter error.
*/
static int samp_i2c_write(struct samp_device *dev, u32 addr,
		u8 *data, u32 len)
{
	struct i2c_client *client = to_i2c_client(dev->dev);
	u32 pos = 0, trans_len = 0;
	u8 buf[64];
	int retry, r = 0;
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = !I2C_M_RD,
	};

	if (likely(len + I2C_ADDR_LENGTH < sizeof(buf))) {
		msg.buf = &buf[0];
	} else {
		msg.buf = kmalloc(I2C_MAX_TRANSFER_SIZE < len +
				I2C_ADDR_LENGTH ? I2C_MAX_TRANSFER_SIZE :
				len + I2C_ADDR_LENGTH,
				GFP_KERNEL);
		if (msg.buf == NULL) {
			pr_err("Malloc failed\n");
			return -ENOMEM;
		}
	}

	while (pos != len) {
		if (unlikely(len - pos > I2C_MAX_TRANSFER_SIZE - I2C_ADDR_LENGTH))
			trans_len = I2C_MAX_TRANSFER_SIZE - I2C_ADDR_LENGTH;
		else
			trans_len = len - pos;

		msg.buf[0] = (unsigned char)((address >> 8) & 0xFF);
		msg.buf[1] = (unsigned char)(address & 0xFF);
		msg.len = trans_len + 2;
		memcpy(&msg.buf[2], &data[pos], trans_len);

		for (retry = 0; retry < I2C_RETRY_TIMES; retry++) {
			if (likely(i2c_transfer(client->adapter, &msg, 1) == 1)) {
				pos += trans_len;
				addr += trans_len;
				break;
			}
			pr_info("I2c write retry[%d]\n", retry + 1);
			msleep(20);
		}
		if (unlikely(retry == I2C_RETRY_TIMES)) {
			pr_err("I2c write failed,dev:%02x,reg:%04x,size:%u\n",
					client->addr, addr, len);
			r = -EIO;
			goto write_exit;
		}
	}

write_exit:
	if (likely(len + I2C_ADDR_LENGTH > sizeof(buf)))
		kfree(msg.buf);
	return r;
}

/**
 * samp_i2c_probe - driver probe device
 */
static int samp_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
{
	struct samp_device *samp_dev;
	u8 buf[1];
	int r;

	r = i2c_check_functionality(client->adapter,
			I2C_FUNC_I2C);
	if (!r)
		return -EIO;

	samp_dev = devm_kzalloc(&client->dev,
			sizeof(struct samp_device), GFP_KERNEL);
	if (!samp_dev)
		return -ENOMEM;

	samp_dev->name = "samp-dev";
	samp_dev->dev = &client->dev;
	i2c_set_clientdata(client, samp_dev);

	/* do i2c test to check whether slave device is 
	 * connnected */
	if (samp_i2c_read(samp_dev, 0xabcd, buf, sizeof(buf) < 0)) {
		pr_err("No i2c slave device found\n");
		return -ENODEV;
	}

	return 0;
}

static int samp_i2c_remove(struct i2c_client *client)
{
	struct samp_device *samp_dev;

	samp_dev = i2c_get_clientdata(client);
	/* because we use devm_kzalloc api,
	 * so there is no need to do kfree here */
	/*kfree(samp_dev);*/
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id i2c_match_table[] = {
	{.compatible = TS_DT_COMPATIBLE,},
	{ },
};
#endif

static const struct i2c_device_id i2c_id_table[] = {
	{I2C_DRIVER_NAME, 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, i2c_id_table);

static struct i2c_driver samp_i2c_driver = {
	.driver = {
		.name = I2C_DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = i2c_match_table,
#endif
	},
	.probe = samp_i2c_probe,
	.remove = samp_i2c_remove,
	.id_table = i2c_id_table,
};

static int __init samp_i2c_init(void)
{
	return i2c_add_driver(&samp_i2c_driver);
}

static void __exit samp_i2c_exit(void)
{
	i2c_del_driver(&samp_i2c_driver);
	return;
}

module_init(samp_i2c_init);
module_exit(samp_i2c_exit);

MODULE_DESCRIPTION("I2C Client Driver");
MODULE_AUTHOR("Yulong Cai");
MODULE_LICENSE("GPL v2");
