#include <dt-bindings/gpio/gpio.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/delay.h>


struct module_custom_data {
	struct device		*dev;
	struct miscdevice miscdev;
	struct file_operations fops;
	/*****************************/
	unsigned int 		pwrkey_gpio;
	unsigned int 		pwrkey_val;
	
	unsigned int 		vbuskey_gpio;
	unsigned int 		vbuskey_val;
	
	unsigned int 		vbaten_gpio;
	unsigned int 		vbaten_val;
	/*****led control******/
	unsigned int  		extled0_gpio;
	unsigned int  		extled0_val;
	
	unsigned int  		extled1_gpio;
	unsigned int  		extled1_val;
	
	unsigned int  		extled2_gpio;
	unsigned int  		extled2_val;
};
struct module_custom_data *pdata = NULL;
static ssize_t attr_ledctrl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!pdata)
		return 0;
	//extled0_val | extled1_val << 1 | extled2_val << 2
	return sprintf(buf, "%u\n", 0xf & (pdata->extled0_val | pdata->extled1_val << 1 | pdata->extled2_val << 2));
}

static ssize_t attr_ledctrl_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val;
	//int ret;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if (val >= 0) {
		pdata->extled0_val = (val & 0x1) ? 1: 0;
		pdata->extled1_val = (val & 0x2) ? 1: 0;
		pdata->extled2_val = (val & 0x4) ? 1: 0;
		
		gpio_set_value(pdata->extled0_gpio, pdata->extled0_val);
		gpio_set_value(pdata->extled1_gpio, pdata->extled1_val);
		gpio_set_value(pdata->extled2_gpio, pdata->extled2_val);
	}
	return size;
}
static DEVICE_ATTR(ledctrl, 0666, attr_ledctrl_show, attr_ledctrl_store);
/*
static int medule_dev_open(struct inode *inode, struct file *file) {
	return 0;
}
static int medule_dev_release(struct inode *inode, struct file *file) {
	return 0;
}
static long medule_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {	
	return 0;
}
*/
static int module_misc_device_register(struct module_custom_data *data) {
	int result;
	data->fops.owner = THIS_MODULE;
	/*data->fops.unlocked_ioctl = medule_dev_ioctl;
	data->fops.open = medule_dev_open;
	data->fops.release = medule_dev_release;*/

	data->miscdev.minor = MISC_DYNAMIC_MINOR;
	data->miscdev.name = "medule-led";
	data->miscdev.fops = &data->fops;
				
	data->miscdev.parent = data->dev;
	result = misc_register(&data->miscdev);
	if (result < 0) {
		printk("fail to register misc device %s\n", data->miscdev.name);
		return result;
	}
	printk("%s:miscdevice: %s\n",__func__,data->miscdev.name);
	return result;
}
static int module_parse_dt(struct device_node *np, struct module_custom_data *data) {
	int ret;
	unsigned int gpio;
	enum of_gpio_flags gpio_flags;
	/********************************************/
	gpio = of_get_named_gpio_flags(np, "vbuskey-gpio", 0, &gpio_flags);
	ret = gpio_request(gpio, "vbuskey-gpio");
	if (ret < 0) {
		printk("request I/O vbuskey-gpio : %d failed\n", gpio);
		return -ENODEV;
	}
	data->vbuskey_val = (gpio_flags == GPIO_ACTIVE_HIGH) ? 1:0;
	gpio_direction_output(gpio, data->vbuskey_val);
	data->vbuskey_gpio = gpio;
	/********************************************/
	gpio = of_get_named_gpio_flags(np, "vbaten-gpio", 0, &gpio_flags);
	ret = gpio_request(gpio, "vbaten-gpio");
	if (ret < 0) {
		printk("request I/O vbaten-gpio : %d failed\n", gpio);
		return -ENODEV;
	}
	data->vbaten_val = (gpio_flags == GPIO_ACTIVE_HIGH) ? 1:0;
	gpio_direction_output(gpio, data->vbaten_val);
	data->vbaten_gpio = gpio;
	/**4G module key gpio */
	gpio = of_get_named_gpio_flags(np, "pwrkey-gpio", 0, &gpio_flags);
	ret = gpio_request(gpio, "pwrkey-gpio");
	if (ret < 0) {
		printk("request I/O pwrkey-gpio : %d failed\n", gpio);
		return -ENODEV;
	}
	data->pwrkey_gpio = gpio;
	data->pwrkey_val = (gpio_flags == GPIO_ACTIVE_HIGH) ? 1:0;
	gpio_direction_output(gpio, !data->pwrkey_val);
	msleep(30);
	gpio_set_value(gpio, data->pwrkey_val);
	msleep(350);
	gpio_set_value(gpio, !data->pwrkey_val);

	/********led control************************/
	gpio = of_get_named_gpio_flags(np, "extled0-gpio", 0, &gpio_flags);
	ret = gpio_request(gpio, "extled0-gpio");
	if (ret < 0) {
		printk("request I/O extled0-gpio : %d failed\n", gpio);
		return -ENODEV;
	}
	data->extled0_val = (gpio_flags == GPIO_ACTIVE_HIGH) ? 1:0;
	gpio_direction_output(gpio, data->extled0_val);
	data->extled0_gpio = gpio;
	/*******************************************/
	gpio = of_get_named_gpio_flags(np, "extled1-gpio", 0, &gpio_flags);
	ret = gpio_request(gpio, "extled1-gpio");
	if (ret < 0) {
		printk("request I/O extled1-gpio : %d failed\n", gpio);
		return -ENODEV;
	}
	data->extled1_val = (gpio_flags == GPIO_ACTIVE_HIGH) ? 1:0;
	gpio_direction_output(gpio, data->extled1_val);
	data->extled1_gpio = gpio;
	/*******************************************/
	gpio = of_get_named_gpio_flags(np, "extled2-gpio", 0, &gpio_flags);
	ret = gpio_request(gpio, "extled2-gpio");
	if (ret < 0) {
		printk("request I/O extled2-gpio : %d failed\n", gpio);
		return -ENODEV;
	}
	data->extled2_val = (gpio_flags == GPIO_ACTIVE_HIGH) ? 1:0;
	gpio_direction_output(gpio, data->extled2_val);
	data->extled2_gpio = gpio;
	/*******************************************/
	printk("%s probe success\n", __func__);
	
	return ret;
}
static int module_custom_probe(struct platform_device *pdev)
{
	int ret;
	struct module_custom_data *module = NULL;
	struct device_node *np = pdev->dev.of_node;
	
	module = kzalloc(sizeof(struct module_custom_data), GFP_KERNEL);
	if (!module) {
		printk("no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	
	ret = module_parse_dt(np, module);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to find platform dts\n");
		goto err_parse;
	}
	module->dev = &pdev->dev;
	
	ret = module_misc_device_register(module);
	if (ret < 0) {
		printk("misc device register err:%d\n", ret);
		goto err_miscdev;
	}
	
	ret = device_create_file(module->miscdev.this_device, &dev_attr_ledctrl);
	if(ret) {
		printk("device_create_file err:%d\n", ret);
		goto dev_file_err;
	}
	
	platform_set_drvdata(pdev, module);
	pdata = module;
	printk("%s probe success ret : %d\n", __func__, ret);
	return ret;
dev_file_err:
	misc_deregister(&module->miscdev);	
err_miscdev:	
err_parse:
	kfree(module);
err_alloc:
	return ret;
}

static int module_custom_remove(struct platform_device *pdev) {	
	struct module_custom_data *module = platform_get_drvdata(pdev);
	gpio_set_value(module->vbaten_gpio, !module->vbaten_val);
	gpio_set_value(module->vbuskey_gpio, !module->vbuskey_val);
	gpio_set_value(module->pwrkey_gpio, !module->pwrkey_val);
	printk("%s : %d\n", __func__, __LINE__);
	return 0;
}
static void module_custom_shutdown(struct platform_device *pdev) {
	struct module_custom_data *module = platform_get_drvdata(pdev);
	gpio_set_value(module->vbaten_gpio, !module->vbaten_val);
	gpio_set_value(module->vbuskey_gpio, !module->vbuskey_val);
	gpio_set_value(module->pwrkey_gpio, !module->pwrkey_val);
	printk("%s : %d\n", __func__, __LINE__);
}

#ifdef CONFIG_PM
static int medule_suspend(struct device *device){
	//printk("%s : %d\n", __func__, __LINE__);
	return 0;
}

static int medule_resume(struct device *device) {
	//printk("%s : %d\n", __func__, __LINE__);
	return 0;
}
static const struct dev_pm_ops medule_pm_ops = {
	.suspend = medule_suspend,
	.resume = medule_resume,
};
#endif
static struct of_device_id module_custom_of_match[] = {
	{ .compatible = "module-EC25-E" },
	{ }
};
MODULE_DEVICE_TABLE(of, module_custom_of_match);

static struct platform_driver module_custom_driver = {
	.driver	= {
		.name		= "module-EC25-E",
		.owner		= THIS_MODULE,
#ifdef CONFIG_PM
		.pm 		= &medule_pm_ops,
#endif		
		.of_match_table	= of_match_ptr(module_custom_of_match),
	},
	.probe		= module_custom_probe,
	.remove		= module_custom_remove,
	.shutdown 	= module_custom_shutdown,
};

module_platform_driver(module_custom_driver);

MODULE_DESCRIPTION("module power for Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:module-EC25-E");