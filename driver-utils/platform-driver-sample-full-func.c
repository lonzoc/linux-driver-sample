/*
  * Platform Driver sample
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/completion.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/of_irq.h>
#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif


#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
#include <linux/input/mt.h>
#define INPUT_TYPE_B_PROTOCOL
#endif

#define GOOIDX_INPUT_PHYS		"samp/input0"
#define PINCTRL_STATE_ACTIVE    "pmx_ts_active"
#define PINCTRL_STATE_SUSPEND   "pmx_ts_suspend"

static void samp_ext_sysfs_release(struct kobject *kobj)
{
	ts_info("Kobject released!");
}

#define to_ext_module(kobj)	container_of(kobj,\
				struct samp_ext_module, kobj)
#define to_ext_attr(attr)	container_of(attr,\
				struct samp_ext_attribute, attr)

static ssize_t samp_ext_sysfs_show(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	struct samp_ext_module *module = to_ext_module(kobj);
	struct samp_ext_attribute *ext_attr = to_ext_attr(attr);

	if (ext_attr->show)
		return ext_attr->show(module, buf);

	return -EIO;
}

static ssize_t samp_ext_sysfs_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	struct samp_ext_module *module = to_ext_module(kobj);
	struct samp_ext_attribute *ext_attr = to_ext_attr(attr);

	if (ext_attr->store)
		return ext_attr->store(module, buf, count);

	return -EIO;
}

static const struct sysfs_ops samp_ext_ops = {
	.show = samp_ext_sysfs_show,
	.store = samp_ext_sysfs_store
};

static struct kobj_type samp_ext_ktype = {
	.release = samp_ext_sysfs_release,
	.sysfs_ops = &samp_ext_ops,
};

struct kobj_type *samp_get_default_ktype(void)
{
	return &samp_ext_ktype;
}
EXPORT_SYMBOL(samp_get_default_ktype);

struct kobject *samp_get_default_kobj(void)
{
	struct kobject *kobj = NULL;

	if (samp_modules.core_data &&
			samp_modules.core_data->pdev)
		kobj = &samp_modules.core_data->pdev->dev.kobj;
	return kobj;
}
EXPORT_SYMBOL(samp_get_default_kobj);

/* debug fs */
struct debugfs_buf {
	struct debugfs_blob_wrapper buf;
	int pos;
	struct dentry *dentry;
} samp_dbg;

void samp_msg_printf(const char *fmt, ...)
{
	va_list args;
	int r;

	if (samp_dbg.pos < samp_dbg.buf.size) {
		va_start(args, fmt);
		r = vscnprintf(samp_dbg.buf.data + samp_dbg.pos,
			 samp_dbg.buf.size - 1, fmt, args);
		samp_dbg.pos += r;
		va_end(args);
	}
}
EXPORT_SYMBOL(samp_msg_printf);

static int samp_debugfs_init(void)
{
	struct dentry *r_b;
	samp_dbg.buf.size = PAGE_SIZE;
	samp_dbg.pos = 0;
	samp_dbg.buf.data = kzalloc(samp_dbg.buf.size, GFP_KERNEL);
	if (samp_dbg.buf.data == NULL) {
		pr_err("Debugfs init failed\n");
		goto exit;
	}
	r_b = debugfs_create_blob("samp", 0644, NULL, &samp_dbg.buf);
	if (!r_b) {
		pr_err("Debugfs create failed\n");
		return -ENOENT;
	}
	samp_dbg.dentry = r_b;

exit:
	return 0;
}

static void samp_debugfs_exit(void)
{
	debugfs_remove(samp_dbg.dentry);
	samp_dbg.dentry = NULL;
	pr_info("Debugfs module exit\n");
}

/* sysfs attributes */
static ssize_t samp_driver_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "DriverVersion:%s\n",
			SAMP_DRIVER_VERSION);
}

static ssize_t samp_chip_info_show(struct device  *dev,
		struct device_attribute *attr, char *buf)
{
	struct samp_core *core_data =
		dev_get_drvdata(dev);
	struct samp_device *ts_dev = core_data->ts_dev;
	struct samp_version chip_ver;
	int r, cnt = 0;

	cnt += snprintf(buf, PAGE_SIZE,
			"TouchDeviceName:%s\n", ts_dev->name);
	if (ts_dev->hw_ops->read_version) {
		r = ts_dev->hw_ops->read_version(ts_dev, &chip_ver);
		if (!r && chip_ver.valid) {
			cnt += snprintf(&buf[cnt], PAGE_SIZE,
					"PID:%s\nVID:%04x\nSensorID:%02x\n",
					chip_ver.pid, chip_ver.vid,
					chip_ver.sensor_id);
		}
	}

	return cnt;
}

static ssize_t samp_config_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct samp_core *core_data =
		dev_get_drvdata(dev);
	struct samp_device *ts_dev = core_data->ts_dev;

	if (ts_dev->normal_cfg->initialized) {
		char ver[1];
		ts_dev->hw_ops->read(ts_dev, ts_dev->normal_cfg->reg_base,
				ver, 1);
		return snprintf(buf, PAGE_SIZE, "version:%02xh\n", ver[0]);
	}

	return -EINVAL;
}

static DEVICE_ATTR(driver_info, S_IRUGO, samp_driver_info_show, NULL);
static DEVICE_ATTR(chip_info, S_IRUGO, samp_chip_info_show, NULL);
static DEVICE_ATTR(config, S_IRUGO, samp_config_data_show, NULL);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_driver_info.attr,
	&dev_attr_chip_info.attr,
	&dev_attr_config.attr,
	NULL,
};

static const struct attribute_group sysfs_group = {
	.attrs = sysfs_attrs,
};

static int samp_sysfs_init(struct samp_core *core_data)
{
	return sysfs_create_group(&core_data->pdev->dev.kobj, &sysfs_group);
}

static void samp_sysfs_exit(struct samp_core *core_data)
{
	sysfs_remove_group(&core_data->pdev->dev.kobj, &sysfs_group);
}

/**
 * samp_threadirq_func - Bottom half of interrupt
 * This functions is excuted in thread context,
 * sleep in this function is permit.
 *
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static irqreturn_t samp_threadirq_func(int irq, void *data)
{
	struct samp_core *core_data = data;
	struct samp_device *ts_dev =  core_data->ts_dev;
	struct samp_ext_module *ext_module;
	struct samp_event *ts_event = &core_data->ts_event;
	int r;

	list_for_each_entry(ext_module, &samp_modules.head, list) {
		if (!ext_module->funcs->irq_event)
			continue;
		r = ext_module->funcs->irq_event(core_data, ext_module);
		if (r == EVT_CANCEL_IRQEVT)
			return IRQ_HANDLED;
	}

	r = ts_dev->hw_ops->event_handler(ts_dev, ts_event);
	if (likely(r >= 0)) {
		if (ts_event->event_type == EVENT_TOUCH) {
			samp_input_report(core_data->input_dev,
					&ts_event->event_data.touch_data);
		}
	}

	return IRQ_HANDLED;
}

/**
 * samp_init_irq - Requset interrput line from system
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int samp_irq_setup(struct samp_core *core_data)
{
	const struct samp_board_data *board_data =
		core_data->ts_dev->board_data;
	int r;

	if (!gpio_is_valid(board_data->irq_gpio)) {
		ts_err("Invalie GPIO:%u", board_data->irq_gpio);
		return -EINVAL;
	} else  {
		core_data->irq = gpio_to_irq(board_data->irq_gpio);
	}

	ts_info("IRQ:%u,flags:%d", core_data->irq, (int)board_data->irq_flags);
	r = devm_request_threaded_irq(&core_data->pdev->dev,
			core_data->irq, NULL,
			samp_threadirq_func,
			board_data->irq_flags | IRQF_ONESHOT,
			SAMP_CORE_DRIVER_NAME,
			core_data);
	if (r < 0)
		ts_err("Failed to requeset threaded irq:%d", r);
	else
		atomic_set(&core_data->irq_enabled, 1);

	return r;
}

/**
 * samp_irq_enable - Enable/Disable a irq
 * @core_data: pointer to touch core data
 * enable: enable or disable irq
 * return: 0 ok, <0 failed
 */
int samp_irq_enable(struct samp_core *core_data,
			bool enable)
{
	if (enable) {
		if (!atomic_cmpxchg(&core_data->irq_enabled, 0, 1)) {
			enable_irq(core_data->irq);
			ts_debug("Irq enabled");
		}
	} else {
		if (atomic_cmpxchg(&core_data->irq_enabled, 1, 0)) {
			disable_irq(core_data->irq);
			ts_debug("Irq disabled");
		}
	}

	return 0;
}
EXPORT_SYMBOL(samp_irq_enable);
/**
 * samp_power_init - Get regulator for touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int samp_power_init(struct samp_core *core_data)
{
	struct device *dev = NULL;
	struct samp_board_data *board_data;
	int r = 0;

	ts_info("Power init");
	if (!core_data->ts_dev->board_data) {
		ts_err("Invalid board data");
		return -ENODEV;
	}

	dev =  core_data->ts_dev->dev;

	board_data = core_data->ts_dev->board_data;
	if (board_data->avdd_name) {
		core_data->avdd = devm_regulator_get(dev,
			 board_data->avdd_name);
		if (IS_ERR_OR_NULL(core_data->avdd)) {
			r = PTR_ERR(core_data->avdd);
			ts_err("Failed to get regulator avdd:%d", r);
			core_data->avdd = NULL;
			return r;
		}
	} else {
		ts_info("Avdd name is NULL");
	}

	if (board_data->vbus_name) {
		core_data->vbus = devm_regulator_get(dev,
			 board_data->vbus_name);
		if (IS_ERR_OR_NULL(core_data->vbus)) {
			r = PTR_ERR(core_data->vbus);
			ts_err("Failed to get regulator vbus:%d", r);
			core_data->avdd = NULL;
			core_data->vbus = NULL;
			return r;
		}
	} else {
		ts_info("Vbus name is NULL");
	}

	return r;
}

/**
 * samp_power_on - Turn on power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int samp_power_on(struct samp_core *core_data)
{
	int r;

	ts_info("Device power on");
	if (core_data->power_on)
		return 0;

	if (core_data->avdd) {
		r = regulator_enable(core_data->avdd);
		if (r) {
			ts_err("Failed to enable analog power:%d", r);
			return r;
		}
	}

	if (core_data->vbus) {
		r = regulator_enable(core_data->vbus);
		if (r) {
			ts_err("Failed to enable bus power:%d", r);
			regulator_disable(core_data->avdd);
			return r;
		}
	}

	core_data->power_on = 1;
	return 0;
}

/**
 * samp_power_off - Turn off power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int samp_power_off(struct samp_core *core_data)
{
	int r;

	ts_info("Device power off");

	if (!core_data->power_on)
		return 0;

	if (core_data->avdd) {
		r = regulator_disable(core_data->avdd);
		if (r)
			ts_err("Failed to disable analog power:%d", r);
	}

	if (core_data->vbus) {
		r = regulator_disable(core_data->vbus);
		if (r)
			ts_err("Failed to disable bus power:%d", r);
	}

	core_data->power_on = 0;
	return 0;
}

#ifdef CONFIG_PINCTRL
/**
 * samp_pinctrl_init - Get pinctrl handler and pinctrl_state
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int samp_pinctrl_init(struct samp_core *core_data)
{
	int r = 0;

	/* get pinctrl handler from of node */
	core_data->pinctrl = devm_pinctrl_get(core_data->ts_dev->dev);
	if (IS_ERR_OR_NULL(core_data->pinctrl)) {
		ts_err("Failed to get pinctrl handler");
		return PTR_ERR(core_data->pinctrl);
	}

	core_data->pin_sta_active = pinctrl_lookup_state(core_data->pinctrl,
				PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(core_data->pin_sta_active)) {
		r = PTR_ERR(core_data->pin_sta_active);
		ts_err("Failed to get pinctrl state:%s, r:%d",
				PINCTRL_STATE_ACTIVE, r);
		goto exit_pinctrl_put;
	}

	core_data->pin_sta_suspend = pinctrl_lookup_state(core_data->pinctrl,
				PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(core_data->pin_sta_suspend)) {
		r = PTR_ERR(core_data->pin_sta_suspend);
		ts_err("Failed to get pinctrl state:%s, r:%d",
				PINCTRL_STATE_SUSPEND, r);
		goto exit_pinctrl_put;
	}

	return 0;
exit_pinctrl_put:
	devm_pinctrl_put(core_data->pinctrl);
	core_data->pinctrl = NULL;
	return r;
}
#endif

/**
 * samp_gpio_setup - Request gpio resources from GPIO subsysten
 *	reset_gpio and irq_gpio number are obtained from samp_device
 *  which created in hardware layer driver. e.g.samp_xx_i2c.c
 *	A samp_device should set those two fileds to right value
 *	before registed to touch core driver.
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int samp_gpio_setup(struct samp_core *core_data)
{
	struct samp_board_data *board_data =
				 core_data->ts_dev->board_data;
	int r = 0;

	ts_info("GPIO setup,reset-gpip:%d, irq-gpio:%d",
		board_data->reset_gpio, board_data->irq_gpio);

	if (gpio_is_valid(board_data->reset_gpio)) {
		r = devm_gpio_request(&core_data->pdev->dev,
				board_data->reset_gpio, "ts_reset_gpio");
		if (r < 0) {
			ts_err("Failed to request reset gpio[%d], r:%d",
				board_data->reset_gpio, r);
			return r;
		}

		r = gpio_direction_output(board_data->reset_gpio, 0);
		if (r < 0) {
			ts_err("Unable to set output of reset gpio");
			return r;
		}
	}

	if (gpio_is_valid(board_data->irq_gpio)) {
		r = devm_gpio_request(&core_data->pdev->dev,
				board_data->irq_gpio, "ts_irq_gpio");
		if (r < 0) {
			ts_err("Failed to request irq gpio[%d], r:%d",
				board_data->irq_gpio, r);
			return r;
		}

		r = gpio_direction_input(board_data->irq_gpio);
		if (r < 0) {
			ts_err("Unable to set dirction of irq gpio");
			return r;
		}
	}

	return 0;
}

/**
 * samp_input_dev_config - Requset and config a input device
 *  then register it to input sybsystem.
 *  NOTE that some hardware layer may provide a input device
 *  (ts_dev->input_dev not NULL).
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int samp_input_dev_config(struct samp_core *core_data)
{
	struct samp_device *ts_dev =
				core_data->ts_dev;
	struct input_dev *input_dev = NULL;
	int r, i;

	input_dev = devm_input_allocate_device(&core_data->pdev->dev);
	if (!input_dev) {
		ts_err("Failed to allocated input device");
		return -ENOMEM;
	}

	core_data->input_dev = input_dev;
	input_set_drvdata(input_dev, core_data);

	input_dev->name = SAMP_CORE_DRIVER_NAME;
	input_dev->phys = GOOIDX_INPUT_PHYS;
	input_dev->id.product = 0xDEAD;
	input_dev->id.vendor = 0xBEEF;
	input_dev->id.version = 10427;

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(BTN_TOOL_FINGER, input_dev->keybit);

#ifdef INPUT_PROP_DIRECT
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
#endif
	if (ts_dev->board_data->swap_axis)
		swap(ts_dev->board_data->panel_max_x,
			ts_dev->board_data->panel_max_y);

	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID,
			0, ts_dev->board_data->panel_max_id, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			0, ts_dev->board_data->panel_max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			0, ts_dev->board_data->panel_max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			0, ts_dev->board_data->panel_max_w, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR,
			0, ts_dev->board_data->panel_max_w, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE,
			0, ts_dev->board_data->panel_max_p, 0, 0);

#ifdef INPUT_TYPE_B_PROTOCOL
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 7, 0)
	input_mt_init_slots(input_dev,
			ts_dev->board_data->panel_max_id, INPUT_MT_DIRECT);
#else
	input_mt_init_slots(input_dev,
			ts_dev->board_data->panel_max_id);
#endif
#endif

	if (ts_dev->board_data->panel_max_key) {
		for (i = 0;
			i < ts_dev->board_data->panel_max_key; i++)
			input_set_capability(input_dev, EV_KEY,
					ts_dev->board_data->panel_key_map[i]);
	}

	input_set_capability(input_dev, EV_KEY, KEY_POWER);
	r = input_register_device(input_dev);
	if (r < 0) {
		ts_err("Unable to register input device");
		return r;
	}

	return 0;
}

/**
 * samp_suspend - Touchscreen suspend function
 * Called by PM/FB/EARLYSUSPEN module to put the device to  sleep
 */
static int samp_suspend(struct samp_core *core_data)
{
	struct samp_ext_module *ext_module;
	struct samp_device *ts_dev = core_data->ts_dev;
	int r;

	ts_debug("Suspend start");
	samp_irq_enable(core_data, false);

	/* let touch ic work in sleep mode */
	if (ts_dev && ts_dev->hw_ops->suspend)
		ts_dev->hw_ops->suspend(ts_dev);
	atomic_set(&core_data->suspended, 1);

#ifdef CONFIG_PINCTRL
	if (core_data->pinctrl) {
		r = pinctrl_select_state(core_data->pinctrl,
					core_data->pin_sta_suspend);
		if (r < 0)
			ts_err("Failed to select active pinstate, r:%d", r);
	}
#endif

out:
	core_data->ts_event.event_data.touch_data.touch_num = 0;
	samp_input_report(core_data->input_dev,
			&core_data->ts_event.event_data.touch_data);
	ts_debug("Suspend end");
	return 0;
}


/**
 * samp_resume - Touchscreen resume function
 * Called by PM/FB/EARLYSUSPEN module to wakeup device
 */
static int samp_resume(struct samp_core *core_data)
{
	struct samp_ext_module *ext_module;
	struct samp_device *ts_dev =
				core_data->ts_dev;
	int r;

	ts_debug("Resume start");
#ifdef CONFIG_PINCTRL
	if (core_data->pinctrl) {
		r = pinctrl_select_state(core_data->pinctrl,
					core_data->pin_sta_active);
		if (r < 0)
			ts_err("Failed to select active pinstate, r:%d", r);
	}
#endif

	atomic_set(&core_data->suspended, 0);
	/* reset device */
	if (ts_dev && ts_dev->hw_ops->resume)
		ts_dev->hw_ops->resume(ts_dev);

	samp_irq_enable(core_data, true);

out:
	ts_debug("Resume end");
	return 0;
}

#ifdef CONFIG_FB
/**
 * samp_fb_notifier_callback - Framebuffer notifier callback
 * Called by kernel during framebuffer blanck/unblank phrase
 */
static int samp_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct samp_core *core_data =
		container_of(self, struct samp_core, fb_notifier);
	struct fb_event *fb_event = data;

	if (fb_event && fb_event->data && core_data) {
		if (event == FB_EARLY_EVENT_BLANK) {
			/* before fb blank */
		} else if (event == FB_EVENT_BLANK) {
			int *blank = fb_event->data;
			if (*blank == FB_BLANK_UNBLANK)
				samp_resume(core_data);
			else if (*blank == FB_BLANK_POWERDOWN)
				samp_suspend(core_data);
		}
	}

	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
/**
 * samp_earlysuspend - Early suspend function
 * Called by kernel during system suspend phrase
 */
static void samp_earlysuspend(struct early_suspend *h)
{
	struct samp_core *core_data =
		container_of(h, struct samp_core,
			 early_suspend);

	samp_suspend(core_data);
}
/**
 * samp_lateresume - Late resume function
 * Called by kernel during system wakeup
 */
static void samp_lateresume(struct early_suspend *h)
{
	struct samp_core *core_data =
		container_of(h, struct samp_core,
			 early_suspend);

	samp_resume(core_data);
}
#endif

#ifdef CONFIG_PM
#if !defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND)
/**
 * samp_pm_suspend - PM suspend function
 * Called by kernel during system suspend phrase
 */
static int samp_pm_suspend(struct device *dev)
{
	struct samp_core *core_data =
		dev_get_drvdata(dev);

	return samp_suspend(core_data);
}
/**
 * samp_pm_resume - PM resume function
 * Called by kernel during system wakeup
 */
static int samp_pm_resume(struct device *dev)
{
	struct samp_core *core_data =
		dev_get_drvdata(dev);

	return samp_resume(core_data);
}
#endif
#endif

/**
 * samp_probe
 * Called by kernel when a Goodix touch platform driver is added.
 */
static int samp_probe(struct platform_device *pdev)
{
	struct samp_core *core_data = NULL;
	struct samp_device *ts_device;
	int r;

	ts_device = pdev->dev.platform_data;
	if (!ts_device || !ts_device->hw_ops) {
		ts_err("No touch device registered");
		return -ENODEV;
	}

	core_data = devm_kzalloc(&pdev->dev, sizeof(struct samp_core),
						GFP_KERNEL);
	if (!core_data) {
		ts_err("Failed to allocate memory for core data");
		return -ENOMEM;
	}

	core_data->pdev = pdev;
	core_data->ts_dev = ts_device;
	platform_set_drvdata(pdev, core_data);

	r = samp_power_init(core_data);
	if (r < 0)
		goto out;

#ifdef CONFIG_PINCTRL
	/* Pinctrl handle is optional. */
	r = samp_pinctrl_init(core_data);
	if (!r && core_data->pinctrl) {
		r = pinctrl_select_state(core_data->pinctrl,
					core_data->pin_sta_active);
		if (r < 0)
			ts_err("Failed to select active pinstate, r:%d", r);
	}
#endif

	r = samp_gpio_setup(core_data);
	if (r < 0)
		goto out;

	r = samp_input_dev_config(core_data);
	if (r < 0)
		goto out;

	r = samp_irq_setup(core_data);
	if (r < 0)
		goto out;

	samp_sysfs_init(core_data);
#ifdef CONFIG_FB
	core_data->fb_notifier.notifier_call = samp_fb_notifier_callback;
	if (fb_register_client(&core_data->fb_notifier))
		ts_err("Failed to register fb notifier client:%d", r);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	core_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	core_data->early_suspend.resume = samp_lateresume;
	core_data->early_suspend.suspend = samp_earlysuspend;
	register_early_suspend(&core_data->early_suspend);
#endif

	return 0;
/* we use resource managed api(devm_),
 * no need to free resource */
out:
	samp_modules.core_exit = true;
	ts_err("Core layer probe failed");
	return r;
}

static int samp_remove(struct platform_device *pdev)
{
	struct samp_core *core_data =
		platform_get_drvdata(pdev);

	samp_power_off(core_data);
	samp_debugfs_exit();
	samp_sysfs_exit(core_data);
	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops dev_pm_ops = {
#if !defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend = samp_pm_suspend,
	.resume = samp_pm_resume,
#endif
};
#endif

static struct platform_driver samp_driver = {
	.driver = {
		.name = SAMP_CORE_DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &dev_pm_ops,
#endif
	},
	.probe = samp_probe,
	.remove = samp_remove,
};

static int __init samp_core_init(void)
{
	samp_debugfs_init();
	return platform_driver_register(&samp_driver);
}


static void __exit samp_core_exit(void)
{
	platform_driver_unregister(&samp_driver);
	return;
}

module_init(samp_core_init);
module_exit(samp_core_exit);

MODULE_DESCRIPTION("Goodix Touchscreen Core Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
