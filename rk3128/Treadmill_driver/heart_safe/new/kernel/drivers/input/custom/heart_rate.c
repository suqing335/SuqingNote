/* 
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>
#include <linux/miscdevice.h>
//#include <linux/sensor-dev.h>

/*************************************************************
* 有线心率和无线心率的上报方式，我采用温度传感器的上报方式
* 心率一般(220 - 年龄) 所以8位可以表示心率
* 最低8位为有线心率，8-15位为无线心率。
**************************************************************/
/**
 * temep: temperture {
 * 	compatible = "temperture";
 * 	heart-io = <&gpio1 GPIO_B4 GPIO_ACTIVE_LOW>;
 * 	heart-w-io = <&gpio1 GPIO_A7 GPIO_ACTIVE_LOW>;
 * };
 */

#define RPM_NUM 	12

#define SENSOR_ON		1
#define SENSOR_OFF		0
#define SENSOR_UNKNOW_DATA	-1

#define TEMPERATURE_IOCTL_MAGIC 't'
#define TEMPERATURE_IOCTL_GET_ENABLED 		_IOR(TEMPERATURE_IOCTL_MAGIC, 1, int *)
#define TEMPERATURE_IOCTL_ENABLE 		_IOW(TEMPERATURE_IOCTL_MAGIC, 2, int *)
#define TEMPERATURE_IOCTL_DISABLE       	_IOW(TEMPERATURE_IOCTL_MAGIC, 3, int *)
#define TEMPERATURE_IOCTL_SET_DELAY       	_IOW(TEMPERATURE_IOCTL_MAGIC, 4, int *)

struct rate_table {
	int   count;
	unsigned long jifvalue[RPM_NUM];
};

struct misc_heart {
	/**设备架构*/
	struct input_dev  *input;
	struct miscdevice miscdev;
	struct file_operations fops;
	struct device     *dev;
	/**功能实现架构*/
	struct timer_list timer;
	struct timer_list heart_timer;
	struct rate_table rpm_table;
	struct rate_table wrpm_table;
	unsigned int      ht_irq; //有线心率中断
	unsigned int      ht_w_irq;//无线心率中断
	unsigned int 	  heart_timer_enable;
	unsigned char 	  rate_count;
	unsigned char 	  orate_count;//old value
	unsigned char 	  wrate_count;
	unsigned char 	  owrate_count;
	unsigned char 	  status_cur;
};
static struct misc_heart *pheart = NULL;
static int heart_dev_open(struct inode *inode, struct file *file) {
	return 0;
}
static int heart_dev_release(struct inode *inode, struct file *file) {
	return 0;
}
static long heart_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	struct misc_heart *heart = pheart;
	unsigned int *argp = (unsigned int *)arg;

	switch(cmd)
	{
		case TEMPERATURE_IOCTL_GET_ENABLED:
			*argp = (unsigned int)heart->status_cur;
			break;
		case TEMPERATURE_IOCTL_ENABLE:		
			/**
			 *上层不打开这个服务的时候，这里的中断不会跑，这样不会浪费cpu资源，
			 * 在hardware层打开的时候有使能 "1" or "0"
			 */
			if(*(unsigned int *)argp) {
				if(heart->status_cur == SENSOR_OFF) {
					//enable_irq(heart->ht_irq);
					//enable_irq(heart->ht_w_irq);
					heart->status_cur = SENSOR_ON;
				}	
			} else {
				if(heart->status_cur == SENSOR_ON) {
					/*关闭中断后程序返回, 如果在中断处理程序中, 那么会继续将中断处理程序执行完*/
					//disable_irq_nosync(heart->ht_irq);
					//disable_irq_nosync(heart->ht_w_irq);
					heart->status_cur = SENSOR_OFF;
				}
			}
			printk("%s:TEMPERATURESENSOR_IOCTL_ENABLE %d\n", __func__, *(unsigned int *)argp);
			break;
		
		default:
			break;
	}	
	return 0;
}

static int heart_misc_device_register(struct misc_heart *heart) {
	int result;
	heart->fops.owner = THIS_MODULE;
	heart->fops.unlocked_ioctl = heart_dev_ioctl;
	heart->fops.open = heart_dev_open;
	heart->fops.release = heart_dev_release;

	heart->miscdev.minor = MISC_DYNAMIC_MINOR;
	heart->miscdev.name = "heartrate";
	heart->miscdev.fops = &heart->fops;
				
	heart->miscdev.parent = heart->dev;
	result = misc_register(&heart->miscdev);
	if (result < 0) {
		printk("fail to register misc device %s\n", heart->miscdev.name);
		return result;
	}
	printk("%s:miscdevice: %s\n",__func__,heart->miscdev.name);
	return result;
}

static int heart_rpm_report_value(struct input_dev *input, int data)
{
	input_report_abs(input, ABS_HEART, data);
	input_sync(input);
	printk("report data : %x\n", data);
	return 0;
}
#ifdef HERAT_TEST_TIMER
struct input_dev *input_t;
struct timer_list *timer;
static unsigned int temep = 0xffff;
static void gsl_timer_handle(unsigned long data)
{
	temep -= 50;
	if(temep <= 0xff00)
		temep = 0xffff;
	
	timer.expires = jiffies + 3 * HZ;
	add_timer(&timer);
	heart_rpm_report_value(input_t, temep);	
}
#endif
static irqreturn_t heart_irq_function(int irq, void *dev_id) {
	
	struct misc_heart *heart = (struct misc_heart *)dev_id;
	
	heart->rpm_table.jifvalue[heart->rpm_table.count++] = jiffies;
	if(heart->rpm_table.count >= RPM_NUM ) {
		heart->rpm_table.count = 0;
	}
	heart->rate_count++;
	/**
	 * 开机没有打开定时器，等一次有中断时再开启定时器，上报数据。
	 */
	if(!heart->heart_timer_enable) { 
		heart->heart_timer.expires = jiffies + 3 * HZ;//3s
		add_timer(&heart->heart_timer);
		heart->heart_timer_enable = 1;
	}
	return IRQ_HANDLED;
}
static irqreturn_t heart_wireless_irq_function(int irq, void *dev_id) {
	
	struct misc_heart *heart = (struct misc_heart *)dev_id;
	
	heart->wrpm_table.jifvalue[heart->wrpm_table.count++] = jiffies;
	if(heart->wrpm_table.count >= RPM_NUM) {
		heart->wrpm_table.count = 0;
	}
	heart->wrate_count++;

	if(!heart->heart_timer_enable) { 
		heart->heart_timer.expires = jiffies + 3 * HZ;//3s
		add_timer(&heart->heart_timer);
		heart->heart_timer_enable = 1;
	}
	return IRQ_HANDLED;
}
static void heart_timer_handle(unsigned long data) {
	/**定时去上报心跳值。*/
	int currents;
	unsigned int rpm = 0, wrpm = 0;
	unsigned int value;
	unsigned long temp;
	struct misc_heart *heart = (struct misc_heart *)data;
	struct rate_table *table = &heart->rpm_table;
	struct rate_table *wtable = &heart->wrpm_table;
	/**判断这3s内有没有pulse, 没有就上报0，然后定时器不再启动，等下一次有pulse时再启动*/
	if (heart->rate_count == heart->orate_count && heart->wrate_count == heart->owrate_count) {
		heart_rpm_report_value(heart->input, 0);
		memset(table->jifvalue, 0, sizeof(table->jifvalue));
		memset(wtable->jifvalue, 0, sizeof(wtable->jifvalue));
		heart->heart_timer_enable = 0;
		return;
	}
	heart->orate_count = heart->rate_count;
	heart->owrate_count = heart->wrate_count;
	/**heart**/
	/**
	 * 1.current当前指向的数组元素是最后一个心跳中断值，但这个设计有一个缺陷...
	 * 2.temp 等于0时，返回 HZ，必须保证value不为0
	 */
	currents = (table->count + RPM_NUM -1) % RPM_NUM;
	if (currents > 0) {
		temp = table->jifvalue[currents] - table->jifvalue[currents -1];//jiffies val
		if (temp > 0 && temp <= 6000) {
			value = jiffies_to_msecs(temp);//ms
			rpm = 60000/value;//心率
			rpm = rpm > 255 ? 220 : rpm;
			/**清除后一个数据,如果没有下一个pulse来时current不会变，会计算出rpm = 220。这是一个无用值*/
			table->jifvalue[currents -1] = 0;
		} else {
			rpm = 0;
		}
		//printk("--temp = %lu, value = %u, rpm = %u\n", temp, value, rpm);
	} else {/*currents = 0*/
		temp = table->jifvalue[currents] - table->jifvalue[RPM_NUM -1];//jiffies val
		if (temp > 0 && temp <= 6000) {
			value = jiffies_to_msecs(temp);//ms
			rpm = 60000/value;//心率
			rpm = rpm > 255 ? 220 : rpm;
			table->jifvalue[RPM_NUM -1] = 0;
		} else {
			rpm = 0;
		}
	}
	/**heart wireless**/
	currents = (wtable->count + RPM_NUM -1) % RPM_NUM;
	if (currents > 0) {
		temp = wtable->jifvalue[currents] - wtable->jifvalue[currents -1];//jiffies val
		if (temp > 0 && temp <= 6000) {
			value = jiffies_to_msecs(temp);//ms
			wrpm = 60000/value;//心率
			wrpm = wrpm > 255 ? 220 : wrpm;
			wtable->jifvalue[currents -1] = 0;
		} else {
			wrpm = 0;
		}
		//printk("--w--temp = %lu, value = %u, rpm = %u\n", temp, value, wrpm);
	} else {
		temp = wtable->jifvalue[currents] - wtable->jifvalue[RPM_NUM -1];//jiffies val
		if (temp > 0 && temp <= 6000) {
			value = jiffies_to_msecs(temp);//ms
			wrpm = 60000/value;//心率
			wrpm = wrpm > 255 ? 220 : wrpm;
			wtable->jifvalue[RPM_NUM -1] = 0;
		} else {
			wrpm = 0;
		}
		wrpm = 0;
	}
	//printk("rpm = %u, wrpm = %u\n", rpm, wrpm);
	heart_rpm_report_value(heart->input, ((wrpm&0xff)<<8)|(rpm&0xff));
	heart->heart_timer.expires = jiffies + 3 * HZ;//3s
	add_timer(&heart->heart_timer);
}

static int heart_irq_request(struct misc_heart *heart) {
	int ret;
	ret = request_irq(heart->ht_irq, heart_irq_function, IRQF_TRIGGER_RISING, "heart_rate", heart);
	if (ret < 0) {
		printk( "heart_irq_request: request ht_irq failed\n");
		goto err_ht_irq;
	}
	
	//disable_irq_nosync(heart->ht_irq);/**等待上层服务开启,使能*/
	
	ret = request_irq(heart->ht_w_irq, heart_wireless_irq_function, IRQF_TRIGGER_RISING, "heart_wireless_rate", heart);
	if (ret < 0) {
		printk( "heart_irq_request: request ht_w_irq failed\n");
		goto err_ht_w_irq;
	}
	
	//disable_irq_nosync(heart->ht_w_irq);/**等待上层服务开启,使能*/
	
	return ret;
err_ht_w_irq:
	free_irq(heart->ht_w_irq, heart);
err_ht_irq:
	free_irq(heart->ht_irq, heart);
	return ret;
}
static int heart_init_timer(struct misc_heart *heart) {
	/**heart_timer**/
	init_timer(&heart->heart_timer);
	/**heart->heart_timer.expires = jiffies + msecs_to_jiffies(3*1000);**/
	heart->heart_timer.expires = jiffies + 3 * HZ;
	heart->heart_timer.function = &heart_timer_handle;
	heart->heart_timer.data = (unsigned long)heart;
	return 0;
}
static int heart_parse_dt(struct device_node *node, struct misc_heart *heart) {
	int ret;
	unsigned int gpio;
	enum of_gpio_flags gpio_flags;
	/**heart**/
	gpio = of_get_named_gpio_flags(node, "heart-io", 0, &gpio_flags);
	ret = gpio_request(gpio, "heart_io");
	if (ret < 0) {
		printk("heart-io request failed\n");
		return -ENODEV;
	}
	gpio_direction_input(gpio);
	heart->ht_irq = gpio_to_irq(gpio);
	if (heart->ht_irq < 0) {
		ret = heart->ht_irq;
		printk("irq for gpio %d error(%d)\n", gpio, ret);
		gpio_free(gpio);
		return ret;
	}
	/**heart wirless**/
	gpio = of_get_named_gpio_flags(node, "heart-w-io", 0, &gpio_flags);
	ret = gpio_request(gpio, "heart-w-io");
	if (ret < 0) {
		printk("heart-w-io request failed\n");
		return -ENODEV;
	}
	gpio_direction_input(gpio);
	heart->ht_w_irq = gpio_to_irq(gpio);
	if (heart->ht_w_irq < 0) {
		ret = heart->ht_w_irq;
		printk("irq for gpio %d error(%d)\n", gpio, ret);
		gpio_free(gpio);
		return ret;
	}
	return 0;
}
static int heart_custom_probe(struct platform_device *pdev) {
	int ret;
	struct misc_heart *heart = NULL;
	struct input_dev  *input = NULL;
	struct device_node *np = pdev->dev.of_node;
	
	heart = kzalloc(sizeof(struct misc_heart), GFP_KERNEL);
	if (!heart) {
		printk("no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	
	ret = heart_parse_dt(np, heart);//dts
	if (ret < 0) {
		printk("failed to find platform dts\n");
		goto err_parse;
	}
	input = input_allocate_device();
	if (!input) {    
                printk(KERN_ERR" Not enough memory\n");    
                ret = -ENOMEM;    
                goto err_alloc_input;
        }
	
	input->name ="heartrate";/**这个名字必须和hardware temperature层对应，不然找不到数据*/
	set_bit(EV_ABS, input->evbit);		
	input_set_abs_params(input, ABS_HEART, 100, 65535, 0, 0);/**上报温度*/
	
	input->dev.parent = &pdev->dev;
	
	ret = input_register_device(input);
	if (ret) {
		dev_err(&pdev->dev,"Unable to register input device %s\n", input->name);
		goto err_input;
	}
	
	heart->dev = &pdev->dev;
	heart->input = input;
	heart->rpm_table.count = 0;
	heart->wrpm_table.count = 0;
	heart->heart_timer_enable = 0;
	heart->rate_count = 0;
	heart->orate_count = 0;
	heart->wrate_count = 0;
	heart->owrate_count = 0;
	heart->status_cur = SENSOR_OFF;
	
	heart->miscdev.parent = &pdev->dev;
	ret = heart_misc_device_register(heart);
	if (ret < 0) {
		goto err_miscdev;
	}
	
	heart_init_timer(heart);
	
	heart_irq_request(heart);
	
	platform_set_drvdata(pdev, heart);
	pheart = heart;
#ifdef  HERAT_TEST_TIMER
	input_t = input:
	init_timer(&timer);
	timer.expires = jiffies + 3 * HZ;
	timer.function = &gsl_timer_handle;
	add_timer(&timer);
#endif	
	return ret;
err_miscdev:
	input_unregister_device(input);
err_input:
	input_free_device(input);
err_alloc_input:
err_parse:
	kfree(heart);
err_alloc:	
	return ret;
}

static int heart_custom_remove(struct platform_device *pdev) {
	struct misc_heart *heart = platform_get_drvdata(pdev);
	free_irq(heart->ht_w_irq, heart);
	free_irq(heart->ht_irq, heart);
	del_timer(&heart->heart_timer);
	input_unregister_device(heart->input);
	input_free_device(heart->input);
	kfree(heart);
	return 0;
}

static struct of_device_id heart_custom_of_match[] = {
	{ .compatible = "heartrate"},
	{ }
};
MODULE_DEVICE_TABLE(of, heart_custom_of_match);

static struct platform_driver heart_custom_driver = {
	.driver		= {
		.name		= "heartrate",
		.owner		= THIS_MODULE,
		.pm		= NULL,
		.of_match_table	= of_match_ptr(heart_custom_of_match),
	},
	.probe		= heart_custom_probe,
	.remove		= heart_custom_remove,
};

module_platform_driver(heart_custom_driver);

MODULE_DESCRIPTION("heart rate Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:heart rate");