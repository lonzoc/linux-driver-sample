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
	u8 addr_buf[2], *buf;
	int r = 0;
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

	/* 
	 * length of one message should not exceed the max
	 * length supported by the i2c master.
	 * You should check the source of i2c master to
	 * get the max supported length, or split the length
	 * to relatively small size for each meaasge and do
	 * i2c_transfer in the looper
	 */
	buf = kzalloc(len, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	/*we assume that device use big-endian format */
	msgs[0].buf[0] = (addr >> 8) & 0xFF;
	msgs[0].buf[1] = addr & 0xFF;
	msgs[1].buf = buf;
	msgs[1].len = len;

	/* 
	 * i2c transfer may fail, you can add a looper
	 * here to retry the transmission
	 */
	r = i2c_transfer(client->adapter, msgs, 2);
	if (likely(r == 2)) {
		memcpy(&data[0], msgs[1].buf, len);
		r = 0;
	} else {
		r = -EIO;
	}

	kfree(buf);
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

	/* 
	 * length of one message should not exceed the max
	 * length supported by the i2c master.
	 * You should check the source of i2c master to
	 * get the max supported length, or split the length
	 * to relatively small size for each meaasge and do
	 * i2c_transfer in the looper
	 */
	msg.buf = kmalloc(len, GFP_KERNEL);
	if (msg.buf == NULL)
		return -ENOMEM;

	msg.buf[0] = (unsigned char)((address >> 8) & 0xFF);
	msg.buf[1] = (unsigned char)(address & 0xFF);
	msg.len = trans_len + 2;
	memcpy(&msg.buf[2], &data[pos], trans_len);

	/* 
	 * i2c transfer may fail, you can add a looper
	 * here to retry the transmission
	 */
	r = i2c_transfer(client->adapter, &msg, 1);
	if (r != 1)
		r = -EIO;
	else
		r = 0; /* no error */

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
