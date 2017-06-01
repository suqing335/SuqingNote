/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>


/**
 * 实现安全锁中断的上报，和风扇pwm的控制，以及一些其他设备需要控制的都用在这个文件
 * 安全锁用输入子系统上报的方式上报到上层应用，pwm控制 /sys/下需要一个文件节点，
 * 正好可以用输入子系统注册的杂设备miscdev提供的dev创建设备文件。
 */

#define LIGHTSENSOR_IOCTL_MAGIC 'l'
#define LIGHTSENSOR_IOCTL_GET_ENABLED		_IOR(LIGHTSENSOR_IOCTL_MAGIC, 1, int *) 
#define LIGHTSENSOR_IOCTL_ENABLE		_IOW(LIGHTSENSOR_IOCTL_MAGIC, 2, int *) 
#define LIGHTSENSOR_IOCTL_DISABLE		_IOW(LIGHTSENSOR_IOCTL_MAGIC, 3, int *)

#define 	TNUM 		(5)

static unsigned int buzz_io;
struct misc_custom_data {
	struct device		*dev;
	struct input_dev 	*input;
	struct miscdevice 	miscdev;
	struct file_operations 	fops;
	struct timer_list 	timer;
	struct delayed_work 	safework;
	
	struct pwm_device	*pwm;
	unsigned int 		pwm_id;
	unsigned int 		duty_cycle;	//每个周期的高电平时间
	unsigned int		period;		//pwm周期时间ns
	unsigned char		pwm_enabled;
	/** safety lock 定义**/
	unsigned int 		safe_irq;
	unsigned int 		safe_gpio;
	unsigned int 		lock_val;
	unsigned int 		tcount;
	unsigned int 		reportval;
		
};
static struct misc_custom_data *pmisc = NULL; 
static void pwm_custom_power_on(struct misc_custom_data *misc)
{
	int ret;
	if (misc->pwm_enabled)
		return;
	ret = pwm_enable(misc->pwm);
	misc->pwm_enabled = 1;
	printk("custom fan power on ret = %d\n", ret);
}

static void pwm_custom_power_off(struct misc_custom_data *misc)
{
	int ret;
	if (!misc->pwm_enabled)
		return;

	ret = pwm_config(misc->pwm, 0, misc->period);
	pwm_disable(misc->pwm);
	misc->pwm_enabled = 0;
	printk("custom fan power off, ret = %d\n", ret);
}

static int pwm_custom_update_status(unsigned int duty_cycle)//us
{	
	int ret;
	struct misc_custom_data *misc = pmisc;
	printk("duty_cycle = %d\n", duty_cycle);
	if (duty_cycle > 0 && (duty_cycle <= misc->period)) {
		misc->duty_cycle = duty_cycle;
		ret = pwm_config(misc->pwm, misc->duty_cycle, misc->period);
		printk("fan config ret = %d\n",ret);
		pwm_custom_power_on(misc);
	} else {
		misc->duty_cycle = 0;
		pwm_custom_power_off(misc);
	}
	return 0;
}

static void buzzer_io_config(int enable) {
	/**buzz io --> low is enabled**/
	gpio_set_value(buzz_io, enable);
}

static ssize_t attr_pwmvalue_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!pmisc)
		return 0;
	
	return sprintf(buf, "%u\n", pmisc->duty_cycle/1000);
}

static ssize_t attr_pwmvalue_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val;
	//int ret;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if (val >= 0) {
		pwm_custom_update_status(val*1000);
	}
	return size;
}
static DEVICE_ATTR(pwmvalue, 0666, attr_pwmvalue_show, attr_pwmvalue_store);

static ssize_t attr_buzz_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", pmisc->duty_cycle/1000);
}

static ssize_t attr_buzz_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	/**buzz io --> high is enabled**/
	unsigned long val;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if (val >= 0) {
		buzzer_io_config(val);
	}
	return size;
}
static DEVICE_ATTR(buzz, 0666, attr_buzz_show, attr_buzz_store);

static int safe_value_report(struct input_dev *input, int index) {
	//input_report_abs(input, ABS_MISC, index);
	input_report_abs(input, ABS_SAFELOCK, index);
	input_sync(input);
	printk("safe_value_report %d\n", index);
	return 0;
}

static void safelock_timer_handle(unsigned long data) {
	struct misc_custom_data *misc = (struct misc_custom_data *)data;
	if (misc->lock_val) {
		--misc->tcount;
		safe_value_report(misc->input, --misc->reportval);
	} else {
		safe_value_report(misc->input, ++misc->reportval);
		if (misc->reportval > 1000) {
			misc->reportval = 2*TNUM + 1;
		}
	}
	if (misc->tcount) {
		misc->timer.expires = jiffies + 2*HZ;//2s
		add_timer(&misc->timer);
	}
}
static void safework_function(struct work_struct *work) {
	struct delayed_work *dwork = container_of(work, struct delayed_work, work);
	struct misc_custom_data *misc = container_of(dwork, struct misc_custom_data, safework);
	
	misc->lock_val = !gpio_get_value(misc->safe_gpio);
	misc->reportval = misc->lock_val ? 2*TNUM : (2*TNUM +1);
	/*把数据通过light-sensor通道报上去*/
	safe_value_report(misc->input, misc->reportval);
	if (misc->tcount) {
		misc->tcount = TNUM;
	} else {
		misc->timer.expires = jiffies + 2 * HZ;//2s
		add_timer(&misc->timer);
		misc->tcount = TNUM;
	}
}

static irqreturn_t safety_lock_intterupt(int irq, void *dev_id) {
	struct misc_custom_data *misc = (struct misc_custom_data *)dev_id;
	
	/**调度工作队列去扫描按键**/
	schedule_delayed_work(&misc->safework, msecs_to_jiffies(6));
	
	return IRQ_HANDLED;
}

static int safe_dev_open(struct inode *inode, struct file *file) {
	return 0;
}
static int safe_dev_release(struct inode *inode, struct file *file) {
	return 0;
}

static long safe_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	
	unsigned int *argp = (unsigned int *)arg;
	switch(cmd)
	{
		case LIGHTSENSOR_IOCTL_GET_ENABLED:
			*argp = 1;
			break;
		case LIGHTSENSOR_IOCTL_ENABLE:
			break;
		default:
			break;
	}
	return 0;
}

static int misc_device_register(struct misc_custom_data *misc) {
	int ret;
	misc->fops.owner = THIS_MODULE;
	misc->fops.unlocked_ioctl = safe_dev_ioctl;
	misc->fops.open = safe_dev_open;
	misc->fops.release = safe_dev_release;

	misc->miscdev.minor = MISC_DYNAMIC_MINOR;
	misc->miscdev.name = "safelock";
	misc->miscdev.fops = &misc->fops;
				
	misc->miscdev.parent = misc->dev;
	/**注册到杂设备类中*/
	ret = misc_register(&misc->miscdev);
	if (ret < 0) {
		printk("fail to register misc device %s\n", misc->miscdev.name);
		return ret;
	}
	printk("%s:miscdevice: %s\n",__func__, misc->miscdev.name);
	return ret;
}
static int misc_input_device_register(struct misc_custom_data *misc) {
	int ret;            
        struct input_dev *input = input_allocate_device();   //分配一个输入设备   
        if (!input) {    
                 printk(KERN_ERR" Not enough memory\n");    
                 ret = -ENOMEM;    
                 goto err_alloc_input;    
        }    
  
        input->name ="safelock";
        set_bit(EV_ABS, input->evbit);
	input_set_abs_params(input, ABS_SAFELOCK, 100, 65535, 0, 0);
	//input_set_abs_params(input, ABS_MISC, 0, 10, 0, 0);			
	//input_set_abs_params(input, ABS_TOOL_WIDTH ,  10, 4095, 0, 0);
	
	input->dev.parent = misc->dev;
        ret = input_register_device(input);  //注册输入设备  
        if (ret) {
                 printk(KERN_ERR" Failed to register device\n");    
                 goto err_register;    
        }
	misc->input = input;
	misc->miscdev.parent = misc->dev;
	ret = misc_device_register(misc);
	if (ret < 0) {
		printk(KERN_ERR" misc device register failed\n");
		goto err_misc;
	}
        return ret;	
err_misc:
	input_unregister_device(input);
err_register:
	input_free_device(input);   
err_alloc_input: 
        return ret;
}

static int misc_irq_request(struct misc_custom_data *misc) {
	int ret;
	ret = request_irq(misc->safe_irq, safety_lock_intterupt, IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, "safe_lock", misc);
	if (ret < 0) {
		printk( "safe_irq: request irq failed\n");
		return -ENODEV;
	}
	disable_irq_nosync(misc->safe_irq);
	printk( "safe_irq: request irq success\n");
	return ret;
}


static int misc_pwm_chip_init(struct misc_custom_data *misc) {
	int ret = 0;
	misc->pwm_enabled = 0;
	misc->period = 3200000;//3200*1000;//ns --> 周期
	misc->duty_cycle = 0;//1600*1000;//ns --> 高电平时间
	printk("--%s--%d--\n", __FUNCTION__, __LINE__);
	misc->pwm = devm_pwm_get(misc->dev, "fancontrol");
	if (IS_ERR(misc->pwm)) {
		dev_err(misc->dev, "unable to request PWM, trying legacy API\n");
		ret = PTR_ERR(misc->pwm);
		goto err_pwm;
	}
	printk("(%s : %d) hwpwm : %d  \n", __func__, __LINE__, misc->pwm->hwpwm);

err_pwm:	
	return ret;
}

static int misc_parse_dt(struct device_node *np, struct misc_custom_data *misc) {
	int ret;
	unsigned int gpio;
	enum of_gpio_flags gpio_flags;
	/***fan en**/
	gpio = of_get_named_gpio_flags(np, "fan-en", 0, &gpio_flags);
	ret = gpio_request(gpio, "fan-en");
	if (ret < 0) {
		printk("request I/O : %d failed\n", gpio);
		return -ENODEV;
	}
	gpio_direction_output(gpio, 1);
	
	/***buzz io**/
	buzz_io = of_get_named_gpio_flags(np, "buzz-io", 0, &gpio_flags);
	ret = gpio_request(buzz_io, "buzz-io");
	if (ret < 0) {
		printk("request I/O : %d failed\n", buzz_io);
		return -ENODEV;
	}
	if (gpio_flags == OF_GPIO_ACTIVE_LOW) {
		gpio_direction_output(buzz_io, 0);
	} else {
		gpio_direction_output(buzz_io, 1);
	}
	
	/**safe lock io */
	gpio = of_get_named_gpio_flags(np, "safe-io", 0, &gpio_flags);
	ret = gpio_request(gpio, "safe-io");
	if (ret < 0) {
		printk("request I/O : %d failed\n", gpio);
		return -ENODEV;
	}
	gpio_direction_input(gpio);
	misc->safe_gpio = gpio;
	
	misc->safe_irq = gpio_to_irq(gpio);
	if (misc->safe_irq < 0) {
		ret = misc->safe_irq;
		printk("irq for gpio %d error(%d)\n", gpio, ret);
		gpio_free(gpio);
		return ret;
	}
	
	return ret;
}

static int misc_custom_probe(struct platform_device *pdev)
{
	int ret;
	struct misc_custom_data *misc = NULL;
	struct device_node *np = pdev->dev.of_node;
	
	misc = kzalloc(sizeof(struct misc_custom_data), GFP_KERNEL);
	if (!misc) {
		printk("no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	
	ret = misc_parse_dt(np, misc);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to find platform dts\n");
		goto err_parse;
	}
	
	ret = misc_irq_request(misc);
	if (ret < 0) {
		goto err_misc_irq;
	}
	
	misc->dev = &pdev->dev;
	ret = misc_pwm_chip_init(misc);
	if (ret < 0) {
		goto err_pwm;
	}
	
	ret = misc_input_device_register(misc);
	if (ret < 0) {
		goto err_misc_input;
	}
	
	ret = device_create_file(misc->miscdev.this_device, &dev_attr_pwmvalue);
	if(ret) {
		printk("device_create_file err:%d\n", ret);
		goto dev_file_err;
	}
	ret = device_create_file(misc->miscdev.this_device, &dev_attr_buzz);
	if(ret) {
		printk("device_create_file 1 err:%d\n", ret);
		goto dev_file_err1;
	}
	
	INIT_DELAYED_WORK(&misc->safework, safework_function);
	
	misc->tcount = 0;
	/**timer**/
	init_timer(&misc->timer);
	//misc->timer.expires = jiffies + 2*HZ;
	misc->timer.function = &safelock_timer_handle;
	misc->timer.data = (unsigned long)misc;
	
	pmisc = misc;
	platform_set_drvdata(pdev, misc);
	enable_irq(misc->safe_irq);
	//pwm_custom_update_status(misc->duty_cycle);
	ret = gpio_get_value(misc->safe_gpio);
	if (ret) {
		misc->timer.expires = jiffies + 2 * HZ;
		add_timer(&misc->timer);
		misc->lock_val = !ret;
		misc->reportval = misc->lock_val ? 2*TNUM : (2*TNUM +1);
		misc->tcount = TNUM;
		ret = 0;
	}
	printk("pwm_custom_probe success!!!\n");
	return ret;
dev_file_err1:
	device_remove_file(misc->miscdev.this_device, &dev_attr_pwmvalue);
dev_file_err:
	misc_deregister(&misc->miscdev);
	input_unregister_device(misc->input);
	input_free_device(misc->input);
err_misc_input:
	pwm_free(misc->pwm);
err_pwm:
	free_irq(misc->safe_irq, misc);
err_misc_irq:
	gpio_free(misc->safe_gpio);
err_parse:
	kfree(misc);
err_alloc:
	return ret;
}

static int misc_custom_remove(struct platform_device *pdev)
{
	struct misc_custom_data *misc = platform_get_drvdata(pdev);
	pwm_custom_power_off(misc);
	
	return 0;
}

static struct of_device_id misc_custom_of_match[] = {
	{ .compatible = "misc-safelock_fan" },
	{ }
};
MODULE_DEVICE_TABLE(of, misc_custom_of_match);

static struct platform_driver misc_custom_driver = {
	.driver		= {
		.name		= "misc-safelock_fan",
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(misc_custom_of_match),
	},
	.probe		= misc_custom_probe,
	.remove		= misc_custom_remove,
};

module_platform_driver(misc_custom_driver);

MODULE_DESCRIPTION("MISC DEVICE Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:misc-safelock_fan");
