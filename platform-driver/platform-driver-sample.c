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
#include <linux/platform_device.h>

struct samp_device {
	platform_device *dev;
	int irq;
	void __iomem *reg_base;
	struct clk *core_clk, *bus_clk;
};

static irqreturn_t samp_irq_top_half(int irq, void *dev_id)
{
	/* atomic context, do something emergence,
	 * and then wakeup bottom half thread to handle
	 * the real task 
	 */
	return IRQ_WAKE_THREAD;
}
/* threaded irq function */
static irqreturn_t samp_irq_bottom_half(int irq, void *dev_id)
{
	/*
	 * thread context, do the real task
	 */
	return IRQ_HANDLED;
}
#ifdef CONFIG_PM
/**
 * samp_pm_suspend - PM suspend function
 * Called by kernel during system suspend phrase
 */
static int samp_pm_suspend(struct device *dev)
{
	struct samp_device *samp_dev =
		dev_get_drvdata(dev);

	return 0;
}
/**
 * samp_pm_resume - PM resume function
 * Called by kernel during system wakeup
 */
static int samp_pm_resume(struct device *dev)
{
	struct samp_device *samp_dev =
		dev_get_drvdata(dev);

	return 0;
}
#endif

	&dev_attr_set_xres.attr,
/**
 * samp_probe
 * Called by kernel when a Goodix touch platform driver is added.
 */
static int samp_probe(struct platform_device *pdev)
{
	struct samp_device *samp_dev = NULL;
	struct resource *res;
	int r;

	samp_dev = devm_kzalloc(&pdev->dev, sizeof(struct samp_device),
						GFP_KERNEL);
	if (!samp_dev)
		return -ENOMEM;

	samp_dev->pdev = pdev;
	platform_set_drvdata(pdev, samp_dev);

	/* if this device has platform resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		samp_dev->reg_base = devm_ioremap(&pdev->dev,
				res->start, resource_size(res));
		if (!samp_dev->reg_base)
			return -ENODEV;
	}
	
	/* if this device has interrupt line */
	samp_dev->irq = platform_get_irq(pdev);
	if (samp_dev->irq <= 0)
		return -ENODEV;

	/* request irq */
	r = devm_request_threaded_irq(&pdev->dev,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT | IRQF_DISABLED,
			samp_irq_top_half,
			samp_irq_bottom_half,
			"samp-dev-irq");
	if (r < 0)
		/* handle error */;


	/* if this device has clock */
	samp_dev->core_clk = devm_clk_get(&pdev->dev, "core");
	if (IS_ERR_OR_NULL(samp_dev->core_clk)) {
		return -ENOMEM;
	} else {
		if (clk_prepare_enable(sam_dev->core_clk))
			return -ENOMEM;
	}

	samp_dev->bus_clk = devm_clk_get(&pdev->dev, "bus");
	if (IS_ERR_OR_NULL(samp_dev->bus_clk)) {
		return -ENOMEM;
	} else {
		if (clk_prepare_enable(sam_dev->bus_clk))
			return -ENOMEM;
	}

	enable_irq(samp_dev->irq);
	return 0;
}

static int samp_remove(struct platform_device *pdev)
{
	struct samp_device *samp_dev =
		platform_get_drvdata(pdev);

	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops dev_pm_ops = {
	.suspend = samp_pm_suspend,
	.resume = samp_pm_resume,
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

static int __init samp_device_init(void)
{
	return platform_driver_register(&samp_driver);
}


static void __exit samp_device_exit(void)
{
	platform_driver_unregister(&samp_driver);
	return;
}

module_init(samp_device_init);
module_exit(samp_device_exit);

MODULE_DESCRIPTION("Platform device sample driver");
MODULE_AUTHOR("Yulong Cai");
MODULE_LICENSE("GPL v2");
