#include <linux/clk.h>
#include <linux/io.h>
//#include <linux/of.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include "rockchip_pwm.h"



/*sys/module/rk_pwm_remotectl/parameters,
modify code_print to change the value*/

static int rk_remote_print_code;
module_param_named(code_print, rk_remote_print_code, int, 0644);
#define DBG_CODE(args...) \
	do { \
		if (1) { \
			pr_info(args); \
		} \
	} while (0)

static int rk_remote_pwm_dbg_level;
module_param_named(dbg_level, rk_remote_pwm_dbg_level, int, 0644);
#define DBG(args...) \
	do { \
		if (1) { \
			pr_info(args); \
		} \
	} while (0)


struct rkxx_remote_key_table {
	int scancode;
	int keycode;
};

struct rkxx_remotectl_button {
	int usercode;
	int nbuttons;
	struct rkxx_remote_key_table key_table[MAX_NUM_KEYS];
};

struct rkxx_remotectl_drvdata {
	void __iomem *base;
//	int state;
	int result;
	int count;
	int irq;
	int remote_pwm_id;
	int handle_cpu_id;
	int wakeup;
	int clk_rate;
	unsigned long period;
	unsigned long temp_period;
	int pwm_freq_nstime;
	struct tasklet_struct remote_tasklet;
	struct wake_lock remotectl_wake_lock;
};

static void rk_pwm_capturectl_do_something(unsigned long  data){
	struct rkxx_remotectl_drvdata *ddata;
	ddata = (struct rkxx_remotectl_drvdata *)data;
	printk("nstime = %lu\n", ddata->period);
}


static irqreturn_t rockchip_pwm_irq(int irq, void *dev_id)
{
	struct rkxx_remotectl_drvdata *ddata = dev_id;
	int val;
	int temp_hpr;
	int temp_lpr;
	int temp_period;
	unsigned int id = ddata->remote_pwm_id;
	DBG("rockchip_pwm_irq start\n");
	if (id > 3)
		return IRQ_NONE;
	val = readl_relaxed(ddata->base + PWM_REG_INTSTS(id));
	DBG("rockchip_pwm_irq PWM_CH_INT(id)=%lu, ppl=%lu, val=%d\n",PWM_CH_INT(id), PWM_CH_POL(id), val);
	if ((val & PWM_CH_INT(id)) == 0)
	//	return IRQ_NONE;
	DBG("rockchip_pwm_irq val\n");
	if ((val & PWM_CH_POL(id)) == 0) {
		temp_hpr = readl_relaxed(ddata->base + PWM_REG_HPR);
		DBG("hpr=%d\n", temp_hpr);
		temp_lpr = readl_relaxed(ddata->base + PWM_REG_LPR);
		DBG("lpr=%d\n", temp_lpr);
		temp_period = ddata->pwm_freq_nstime * temp_lpr / 1000;
		if (temp_period > RK_PWM_TIME_BIT0_MIN) {
			ddata->period = ddata->temp_period
			    + ddata->pwm_freq_nstime * temp_hpr / 1000;
			tasklet_hi_schedule(&ddata->remote_tasklet);
			ddata->temp_period = 0;
			//DBG("period+ =%ld\n", ddata->period);
		} else {
			ddata->temp_period += ddata->pwm_freq_nstime
			    * (temp_hpr + temp_lpr) / 1000;
		}
	}
	writel_relaxed(PWM_CH_INT(id), ddata->base + PWM_REG_INTSTS(id));
	
	//wake_lock_timeout(&ddata->remotectl_wake_lock, HZ);
	DBG("rockchip_pwm_irq end\n");
	return IRQ_HANDLED;
}



static int rk_pwm_remotectl_hw_init(struct rkxx_remotectl_drvdata *ddata)
{
	int val;

	val = readl_relaxed(ddata->base + PWM_REG_CTRL);
	val = (val & 0xFFFFFFFE) | PWM_DISABLE;
	writel_relaxed(val, ddata->base + PWM_REG_CTRL);
	val = readl_relaxed(ddata->base + PWM_REG_CTRL);
	val = (val & 0xFFFFFFF9) | PWM_MODE_CAPTURE;
	writel_relaxed(val, ddata->base + PWM_REG_CTRL);
	val = readl_relaxed(ddata->base + PWM_REG_CTRL);
	val = (val & 0xFF008DFF) | 0x0006000;
	writel_relaxed(val, ddata->base + PWM_REG_CTRL);
	switch (ddata->remote_pwm_id) {
	case 0: {
		val = readl_relaxed(ddata->base + PWM0_REG_INT_EN);
		val = (val & 0xFFFFFFFE) | PWM_CH0_INT_ENABLE;
		writel_relaxed(val, ddata->base + PWM0_REG_INT_EN);
	}
	break;
	case 1:	{
		val = readl_relaxed(ddata->base + PWM1_REG_INT_EN);
		val = (val & 0xFFFFFFFD) | PWM_CH1_INT_ENABLE;
		writel_relaxed(val, ddata->base + PWM1_REG_INT_EN);
	}
	break;
	case 2:	{
		val = readl_relaxed(ddata->base + PWM2_REG_INT_EN);
		val = (val & 0xFFFFFFFB) | PWM_CH2_INT_ENABLE;
		writel_relaxed(val, ddata->base + PWM2_REG_INT_EN);
	}
	break;
	case 3:	{
		val = readl_relaxed(ddata->base + PWM3_REG_INT_EN);
		val = (val & 0xFFFFFFF7) | PWM_CH3_INT_ENABLE;
		writel_relaxed(val, ddata->base + PWM3_REG_INT_EN);
	}
	break;
	default:
	break;
	}
	val = readl_relaxed(ddata->base + PWM_REG_CTRL);
	val = (val & 0xFFFFFFFE) | PWM_ENABLE;
	writel_relaxed(val, ddata->base + PWM_REG_CTRL);
	return 0;
}


static int rk_pwm_probe(struct platform_device *pdev)
{
	struct rkxx_remotectl_drvdata *ddata;
	struct device_node *np = pdev->dev.of_node;
	struct resource *r;

	struct clk *clk;
	struct cpumask cpumask;
	int irq;
	int ret;
	int cpu_id;
	int pwm_id;
	int pwm_freq;
	//unsigned long irq_flags;

	pr_err(".. rk pwm remotectl v1.1 init\n");
	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		dev_err(&pdev->dev, "no memory resources defined\n");
		return -ENODEV;
	}
	ddata = devm_kzalloc(&pdev->dev, sizeof(struct rkxx_remotectl_drvdata),
			     GFP_KERNEL);
	if (!ddata) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}
	//ddata->state = RMC_PRELOAD;
	ddata->temp_period = 0;
	ddata->base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(ddata->base))
		return PTR_ERR(ddata->base);
	clk = devm_clk_get(&pdev->dev, "pclk_pwm");
	if (IS_ERR(clk))
		return PTR_ERR(clk);
	platform_set_drvdata(pdev, ddata);
	
	
	wake_lock_init(&ddata->remotectl_wake_lock,
		       WAKE_LOCK_SUSPEND, "rk29_pwm_remote");
	ret = clk_prepare_enable(clk);
	if (ret)
		return ret;
	irq = platform_get_irq(pdev, 0);
	//irq_pin = of_get_named_gpio_flags(np, "irq-gpio", 0, (enum of_gpio_flags *)(&irq_flags));
	//irq = gpio_to_irq(irq_pin);
	if (irq < 0) {
		dev_err(&pdev->dev, "cannot find IRQ\n");
		return ret;
	}
	ddata->irq = irq;
	ddata->wakeup = 1;
	of_property_read_u32(np, "remote_pwm_id", &pwm_id);
	ddata->remote_pwm_id = pwm_id;
	DBG("remotectl: remote pwm id=0x%x\n", pwm_id);
	of_property_read_u32(np, "handle_cpu_id", &cpu_id);
	ddata->handle_cpu_id = cpu_id;
	DBG("remotectl: handle cpu id=0x%x\n", cpu_id);
	//rk_remotectl_parse_ir_keys(pdev);
	tasklet_init(&ddata->remote_tasklet, rk_pwm_capturectl_do_something,
		     (unsigned long)ddata);
	/*for (j = 0; j < num; j++) {
		DBG("remotectl probe j = 0x%x\n", j);
		for (i = 0; i < remotectl_button[j].nbuttons; i++) {
			unsigned int type = EV_KEY;

			input_set_capability(input, type, remotectl_button[j].
					     key_table[i].keycode);
		}
	}*/
	//ret = input_register_device(input);
	//if (ret)
	//	pr_err("remotectl: register input device err, ret: %d\n", ret);
	//input_set_capability(input, EV_KEY, KEY_WAKEUP);
	device_init_wakeup(&pdev->dev, 1);
	enable_irq_wake(irq);
	//setup_timer(&ddata->timer, rk_pwm_remotectl_timer,
	//	    (unsigned long)ddata);
	//mod_timer(&ddata->timer, jiffies + msecs_to_jiffies(1000));
	cpumask_clear(&cpumask);
	cpumask_set_cpu(cpu_id, &cpumask);
	irq_set_affinity(irq, &cpumask);
	ret = devm_request_irq(&pdev->dev, irq, rockchip_pwm_irq,
			       IRQF_NO_SUSPEND, "rk_pwm_irq", ddata);
	//ret = request_irq(irq, rockchip_pwm_irq, IRQF_TRIGGER_RISING, "rk_pwm_capture_irq", ddata);
	if (ret) {
		dev_err(&pdev->dev, "cannot claim IRQ %d\n", irq);
		return ret;
	}
	DBG("request_irq: irq=%d\n", irq);
	rk_pwm_remotectl_hw_init(ddata);
	pwm_freq = clk_get_rate(clk) / 64;
	ddata->pwm_freq_nstime = 1000000000 / pwm_freq;
	DBG("rk_pwm_probe success\n");
	return ret;
}

static int rk_pwm_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM
static int remotectl_suspend(struct device *dev)
{
	int cpu = 0;
	struct cpumask cpumask;
	struct platform_device *pdev = to_platform_device(dev);
	struct rkxx_remotectl_drvdata *ddata = platform_get_drvdata(pdev);

	cpumask_clear(&cpumask);
	cpumask_set_cpu(cpu, &cpumask);
	irq_set_affinity(ddata->irq, &cpumask);
	return 0;
}


static int remotectl_resume(struct device *dev)
{
	struct cpumask cpumask;
	struct platform_device *pdev = to_platform_device(dev);
	struct rkxx_remotectl_drvdata *ddata = platform_get_drvdata(pdev);

	cpumask_clear(&cpumask);
	cpumask_set_cpu(ddata->handle_cpu_id, &cpumask);
	irq_set_affinity(ddata->irq, &cpumask);
	return 0;
}

static const struct dev_pm_ops remotectl_pm_ops = {
	.suspend_late = remotectl_suspend,
	.resume_early = remotectl_resume,
};
#endif

static const struct of_device_id rk_pwm_of_match[] = {
	{ .compatible =  "rockchip,capturectl-pwm"},
	{ }
};

MODULE_DEVICE_TABLE(of, rk_pwm_of_match);

static struct platform_driver rk_pwm_driver = {
	.driver = {
		.name = "capturectl-pwm",
		.of_match_table = rk_pwm_of_match,
#ifdef CONFIG_PM
		.pm = &remotectl_pm_ops,
#endif
	},
	.probe = rk_pwm_probe,
	.remove = rk_pwm_remove,
};

module_platform_driver(rk_pwm_driver);

MODULE_LICENSE("GPL");
