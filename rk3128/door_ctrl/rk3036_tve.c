/*
 * rk3036_tve.c
 *
 * Driver for rockchip rk3036 tv encoder control
 * Copyright (C) 2014
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 *
 */
#include <linux/clk.h>
#include <linux/display-sys.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <asm-generic/uaccess.h>
#include <linux/module.h>
#include <linux/rk_fb.h>
#include <linux/rockchip/grf.h>
#include <linux/rockchip/iomap.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/input.h> 
#include <linux/of_gpio.h>
#include <dt-bindings/gpio/gpio.h>
#include "../../hdmi/rockchip-hdmiv2/rockchip_hdmiv2.h"
#include "../../../arch/arm/mach-rockchip/efuse.h"
#include "rk3036_tve.h"
#include "../../../../../sound/soc/codecs/rk312x_codec.h"

extern unsigned int custom_capval;

static const struct fb_videomode rk3036_cvbs_mode[] = {
	/*name		refresh	xres	yres	pixclock	h_bp	h_fp	v_bp	v_fp	h_pw	v_pw			polariry				PorI		flag*/
/*	{"NTSC",        60,     720,    480,    27000000,       57,     19,     19,     0,      62,     3,      FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,   FB_VMODE_INTERLACED,    0},
	{"PAL",         50,     720,    576,    27000000,       69,     12,     19,     2,      63,     3,      FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,   FB_VMODE_INTERLACED,    0},
*/	{"NTSC",	60,	720,	480,	27000000,	43,	33,	19,	0,	62,	3,	FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,	FB_VMODE_INTERLACED,	0},
	{"PAL",		50,	720,	576,	27000000,	48,	33,	19,	2,	63,	3,	FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,	FB_VMODE_INTERLACED,	0},
	//{"NTSC",	60,	720,	480,	27000000,	43,	33,	19,	0,	62,	3,	FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,	FB_VMODE_INTERLACED,	0},
};

static struct rk3036_tve *rk3036_tve;
static unsigned int door_flags = 0;
static unsigned int audio_flags = 0;
static unsigned int opendoor_flag = 0;
static unsigned char set_gparr[6] = {1, 0, 1, 0, 1, 1};
static unsigned char open_gparr[6] = {1, 0, 1, 0, 1, 0};
static unsigned int set_data[6] = {100, 50, 50, 200, 100, 100};
static unsigned int open_data[6] = {100, 50, 50, 200, 100, 100};
static unsigned char temp_gparr[6] = {0};
static unsigned int temp_data[6] = {0};
static unsigned int opensetcnt = 0;
#define DOOR_NAME "door_name"
static int cvbsformat = -1;

#define tve_writel(offset, v)	writel_relaxed(v, rk3036_tve->regbase + offset)
#define tve_readl(offset)	readl_relaxed(rk3036_tve->regbase + offset)

#define tve_dac_writel(offset, v)   writel_relaxed(v, rk3036_tve->vdacbase + offset)
#define tve_dac_readl(offset)	readl_relaxed(rk3036_tve->vdacbase + offset)

#ifdef DEBUG
#define TVEDBG(format, ...) \
		dev_info(rk3036_tve->dev,\
		 "RK3036 TVE: " format "\n", ## __VA_ARGS__)
#else
#define TVEDBG(format, ...)
#endif

#define RK322X_VDAC_STANDARD 0x15

//static unsigned int irq_status = 0;
//static unsigned int met_irq_status = 0;
static unsigned int call_irq_status = 0;
static unsigned int call_irq1_status = 0;

static unsigned int call_falgs = 0;
static unsigned int delete_led = 0;
static void dac_enable(bool enable)
{
	u32 mask, val;
	u32 grfreg = 0;

	TVEDBG("%s enable %d\n", __func__, enable);

	if (enable) {
		mask = m_VBG_EN | m_DAC_EN | m_DAC_GAIN;
		if (rk3036_tve->soctype == SOC_RK312X) {
			val = m_VBG_EN | m_DAC_EN |
			      v_DAC_GAIN(rk3036_tve->daclevel);
			grfreg = RK312X_GRF_TVE_CON;
		} else if (rk3036_tve->soctype == SOC_RK3036) {
			val = m_VBG_EN | m_DAC_EN |
			      v_DAC_GAIN(rk3036_tve->daclevel);
			grfreg = RK3036_GRF_SOC_CON3;
		} else if (rk3036_tve->soctype == SOC_RK322X) {
			val = 0x70;
		}
	} else {
		mask = m_VBG_EN | m_DAC_EN;
		val = 0;
		if (rk3036_tve->soctype == SOC_RK312X)
			grfreg = RK312X_GRF_TVE_CON;
		else if (rk3036_tve->soctype == SOC_RK3036)
			grfreg = RK3036_GRF_SOC_CON3;
		else if (rk3036_tve->soctype == SOC_RK322X)
			val = v_CUR_REG(0x7) | m_DR_PWR_DOWN | m_BG_PWR_DOWN;
	}
	if (grfreg)
		grf_writel(grfreg, (mask << 16) | val);
	else if (rk3036_tve->vdacbase)
		tve_dac_writel(VDAC_VDAC1, val);
}

static void rk322x_dac_init(void)
{
	/*tve_dac_writel(VDAC_VDAC0, 0x0);*/
	tve_dac_writel(VDAC_VDAC1, v_CUR_REG(0x7) |
				   m_DR_PWR_DOWN | m_BG_PWR_DOWN);
	tve_dac_writel(VDAC_VDAC2, v_CUR_CTR(rk3036_tve->daclevel));
	tve_dac_writel(VDAC_VDAC3, v_CAB_EN(0));
}

static void tve_set_mode(int mode)
{
	TVEDBG("%s mode %d\n", __func__, mode);
	if (cvbsformat >= 0)
		return;
	if (rk3036_tve->soctype != SOC_RK322X) {
		tve_writel(TV_RESET, v_RESET(1));
		usleep_range(100, 110);
		tve_writel(TV_RESET, v_RESET(0));
	}
	if (rk3036_tve->inputformat == INPUT_FORMAT_RGB)
		tve_writel(TV_CTRL, v_CVBS_MODE(mode) | v_CLK_UPSTREAM_EN(2) |
			   v_TIMING_EN(2) | v_LUMA_FILTER_GAIN(0) |
			   v_LUMA_FILTER_UPSAMPLE(1) | v_CSC_PATH(0));
	else
		tve_writel(TV_CTRL, v_CVBS_MODE(mode) | v_CLK_UPSTREAM_EN(2) |
			   v_TIMING_EN(2) | v_LUMA_FILTER_GAIN(0) |
			   v_LUMA_FILTER_UPSAMPLE(1) | v_CSC_PATH(3));

	tve_writel(TV_LUMA_FILTER0, rk3036_tve->lumafilter0);
	tve_writel(TV_LUMA_FILTER1, rk3036_tve->lumafilter1);
	tve_writel(TV_LUMA_FILTER2, rk3036_tve->lumafilter2);

	if (mode == TVOUT_CVBS_NTSC) {
		tve_writel(TV_ROUTING, v_DAC_SENSE_EN(0) | v_Y_IRE_7_5(1) |
			v_Y_AGC_PULSE_ON(0) | v_Y_VIDEO_ON(1) |
			v_YPP_MODE(1) | v_Y_SYNC_ON(1) | v_PIC_MODE(mode));
		tve_writel(TV_BW_CTRL, v_CHROMA_BW(BP_FILTER_NTSC) |
			v_COLOR_DIFF_BW(COLOR_DIFF_FILTER_BW_1_3));
		tve_writel(TV_SATURATION, 0x0042543C);
		if (rk3036_tve->test_mode)
			tve_writel(TV_BRIGHTNESS_CONTRAST, 0x00008300);
		else
			tve_writel(TV_BRIGHTNESS_CONTRAST, 0x00007900);

		tve_writel(TV_FREQ_SC,	0x21F07BD7);
		tve_writel(TV_SYNC_TIMING, 0x00C07a81);
		tve_writel(TV_ADJ_TIMING, 0x96B40000 | 0x70);
		tve_writel(TV_ACT_ST,	0x001500D6);
		tve_writel(TV_ACT_TIMING, 0x069800FC | (1 << 12) | (1 << 28));

	} else if (mode == TVOUT_CVBS_PAL) {
		tve_writel(TV_ROUTING, v_DAC_SENSE_EN(0) | v_Y_IRE_7_5(0) |
			v_Y_AGC_PULSE_ON(0) | v_Y_VIDEO_ON(1) |
			v_YPP_MODE(1) | v_Y_SYNC_ON(1) | v_PIC_MODE(mode));
		tve_writel(TV_BW_CTRL, v_CHROMA_BW(BP_FILTER_PAL) |
			v_COLOR_DIFF_BW(COLOR_DIFF_FILTER_BW_1_3));

		tve_writel(TV_SATURATION, rk3036_tve->saturation);
		tve_writel(TV_BRIGHTNESS_CONTRAST, rk3036_tve->brightcontrast);

		tve_writel(TV_FREQ_SC,	0x2A098ACB);
		tve_writel(TV_SYNC_TIMING, 0x00C28381);
		tve_writel(TV_ADJ_TIMING, (0xc << 28) | 0x06c00800 | 0x80);
		tve_writel(TV_ACT_ST,	0x001500F6);
		tve_writel(TV_ACT_TIMING, 0x0694011D | (1 << 12) | (2 << 28));

		tve_writel(TV_ADJ_TIMING, rk3036_tve->adjtiming);
		tve_writel(TV_ACT_TIMING, 0x0694011D |
				(1 << 12) | (2 << 28));
	}
}

static int tve_switch_fb(const struct fb_videomode *modedb, int enable)
{
	struct rk_screen *screen = &rk3036_tve->screen;

	if (modedb == NULL)
		return -1;

	memset(screen, 0, sizeof(struct rk_screen));
	/* screen type & face */
	if (rk3036_tve->test_mode)
		screen->type = SCREEN_TVOUT_TEST;
	else
		screen->type = SCREEN_TVOUT;
	screen->face = OUT_P888;
	screen->color_mode = COLOR_YCBCR;
	screen->mode = *modedb;

	/* Pin polarity */
	if (FB_SYNC_HOR_HIGH_ACT & modedb->sync)
		screen->pin_hsync = 1;
	else
		screen->pin_hsync = 0;
	if (FB_SYNC_VERT_HIGH_ACT & modedb->sync)
		screen->pin_vsync = 1;
	else
		screen->pin_vsync = 0;

	screen->pin_den = 0;
	screen->pin_dclk = 0;
	screen->pixelrepeat = 1;

	/* Swap rule */
	screen->swap_rb = 0;
	screen->swap_rg = 0;
	screen->swap_gb = 0;
	screen->swap_delta = 0;
	screen->swap_dumy = 0;

	/* Operation function*/
	screen->init = NULL;
	screen->standby = NULL;
	rk_fb_switch_screen(screen, enable, 0);

	if (enable) {
		if (rk3036_tve->soctype == SOC_RK322X)
			ext_pll_set_27m_out();
		if (screen->mode.yres == 480)
			tve_set_mode(TVOUT_CVBS_NTSC);
		else
			tve_set_mode(TVOUT_CVBS_PAL);
	}
	return 0;
}

static int cvbs_set_enable(struct rk_display_device *device, int enable)
{
	TVEDBG("%s enable %d\n", __func__, enable);
	mutex_lock(&rk3036_tve->tve_lock);
	if (rk3036_tve->enable != enable) {
		rk3036_tve->enable = enable;
		if (rk3036_tve->suspend) {
			mutex_unlock(&rk3036_tve->tve_lock);
			return 0;
		}

		if (enable == 0) {
			dac_enable(false);
			cvbsformat = -1;
			tve_switch_fb(rk3036_tve->mode, 0);
		} else if (enable == 1) {
			tve_switch_fb(rk3036_tve->mode, 1);
			dac_enable(true);
		}
	}
	mutex_unlock(&rk3036_tve->tve_lock);
	return 0;
}

static int cvbs_get_enable(struct rk_display_device *device)
{
	TVEDBG("%s enable %d\n", __func__, rk3036_tve->enable);
	return rk3036_tve->enable;
}

static int cvbs_get_status(struct rk_display_device *device)
{
	return 1;
}

static int
cvbs_get_modelist(struct rk_display_device *device, struct list_head **modelist)
{
	*modelist = &(rk3036_tve->modelist);
	return 0;
}

static int
cvbs_set_mode(struct rk_display_device *device, struct fb_videomode *mode)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rk3036_cvbs_mode); i++) {
		if (fb_mode_is_equal(&rk3036_cvbs_mode[i], mode)) {
			if (rk3036_tve->mode != &rk3036_cvbs_mode[i]) {
				rk3036_tve->mode =
				(struct fb_videomode *)&rk3036_cvbs_mode[i];
				if (rk3036_tve->enable && !rk3036_tve->suspend) {
					dac_enable(false);
					msleep(200);
					tve_switch_fb(rk3036_tve->mode, 1);
					dac_enable(true);
				}
			}
			return 0;
		}
	}
	TVEDBG("%s\n", __func__);
	return -1;
}

static int
cvbs_get_mode(struct rk_display_device *device, struct fb_videomode *mode)
{
	*mode = *(rk3036_tve->mode);
	return 0;
}

static int
tve_fb_event_notify(struct notifier_block *self,
		    unsigned long action, void *data)
{
	struct fb_event *event = data;
	int blank_mode = *((int *)event->data);

	if (action == FB_EARLY_EVENT_BLANK) {
		switch (blank_mode) {
		case FB_BLANK_UNBLANK:
			break;
		default:
			TVEDBG("suspend tve\n");
			if (!rk3036_tve->suspend) {
				rk3036_tve->suspend = 1;
				if (rk3036_tve->enable) {
					tve_switch_fb(rk3036_tve->mode, 0);
					dac_enable(false);
					if (rk3036_tve->soctype == SOC_RK322X)
						clk_disable_unprepare(rk3036_tve->dac_clk);
				}
			}
			break;
		}
	} else if (action == FB_EVENT_BLANK) {
		switch (blank_mode) {
		case FB_BLANK_UNBLANK:
			TVEDBG("resume tve\n");
			mutex_lock(&rk3036_tve->tve_lock);
			if (rk3036_tve->suspend) {
				if (rk3036_tve->soctype == SOC_RK322X) {
					clk_prepare_enable(rk3036_tve->dac_clk);
					rk322x_dac_init();
				}
				rk3036_tve->suspend = 0;
				if (rk3036_tve->enable) {
					tve_switch_fb(rk3036_tve->mode, 1);
					dac_enable(true);
				}
			}
			mutex_unlock(&rk3036_tve->tve_lock);
			break;
		default:
			break;
		}
	}
	return NOTIFY_OK;
}

static struct notifier_block tve_fb_notifier = {
	.notifier_call = tve_fb_event_notify,
};

static struct rk_display_ops cvbs_display_ops = {
	.setenable = cvbs_set_enable,
	.getenable = cvbs_get_enable,
	.getstatus = cvbs_get_status,
	.getmodelist = cvbs_get_modelist,
	.setmode = cvbs_set_mode,
	.getmode = cvbs_get_mode,
};

static int
display_cvbs_probe(struct rk_display_device *device, void *devdata)
{
	device->owner = THIS_MODULE;
	strcpy(device->type, "TV");
	device->name = "cvbs";
	device->priority = DISPLAY_PRIORITY_TV;
	device->priv_data = devdata;
	device->ops = &cvbs_display_ops;
	return 1;
}

static struct rk_display_driver display_cvbs = {
	.probe = display_cvbs_probe,
};

#if defined(CONFIG_OF)
static const struct of_device_id rk3036_tve_dt_ids[] = {
	{.compatible = "rockchip,rk3036-tve",},
	{.compatible = "rockchip,rk312x-tve",},
	{.compatible = "rockchip,rk322x-tve",},
	{}
};
#endif

static int __init bootloader_tve_setup(char *str)
{
	static int ret;

	if (str) {
		pr_info("cvbs init tve.format is %s\n", str);
		ret = sscanf(str, "%d", &cvbsformat);
	}

	return 0;
}

early_param("tve.format", bootloader_tve_setup);
#ifdef HAVE_WORK_CTL
static enum hrtimer_restart hr_opendoor_callback(struct hrtimer *timer){
	/*****这里面是一个中断，不能用msleep睡太久，不然会挂********/
	struct rk3036_tve *rk3036_tve = container_of(timer, struct rk3036_tve, htimer);
	if (opensetcnt >= 6) {
		gpio_set_value(rk3036_tve->lock_gpio, 0);
		complete(&rk3036_tve->com);
		return HRTIMER_NORESTART;
	}
	gpio_set_value(rk3036_tve->lock_gpio, (unsigned int)temp_gparr[opensetcnt]);
	hrtimer_start(&rk3036_tve->htimer, ktime_set(0, temp_data[opensetcnt]*1000*1000), HRTIMER_MODE_REL);
	opensetcnt++;
	//printk("hr_opendoor_callback %d--\n", opensetcnt);
	return HRTIMER_NORESTART;
}


static void custom_opendoor_control(struct work_struct *work)
{
	struct rk3036_tve *rk3036_tve = container_of(work, struct rk3036_tve, work);
	opensetcnt = 1;
	gpio_set_value(rk3036_tve->lock_en, 1);
	msleep(200);
	if (opendoor_flag == 2) {
		//set
		memcpy(temp_data, set_data, sizeof(set_data));
		memcpy(temp_gparr, set_gparr, sizeof(set_gparr));
		
		gpio_set_value(rk3036_tve->lock_gpio, (unsigned int)temp_gparr[0]);
		hrtimer_start(&rk3036_tve->htimer, ktime_set(0, temp_data[0]*1000*1000), HRTIMER_MODE_REL);
		
	} else if (opendoor_flag == 1) {
		//open
		memcpy(temp_data, open_data, sizeof(open_data));
		memcpy(temp_gparr, open_gparr, sizeof(open_gparr));
		
		gpio_set_value(rk3036_tve->lock_gpio, (unsigned int)temp_gparr[0]);
		hrtimer_start(&rk3036_tve->htimer, ktime_set(0, temp_data[0]*1000*1000), HRTIMER_MODE_REL);
	}
	
	wait_for_completion(&rk3036_tve->com);///////
	msleep(4000);//
	gpio_set_value(rk3036_tve->lock_en, 0);
	msleep(100);
	opendoor_flag = 0;	
}
#endif
static irqreturn_t call_irq_func(int irq, void *dev_id)
{
	int i;
	int level;
	struct rk3036_tve *rk3036_tve = (struct rk3036_tve *)dev_id;
	if (!call_irq_status) {
		disable_irq_nosync(rk3036_tve->call_irq);
		call_irq_status = 1;
	} else {
		return IRQ_HANDLED;
	}

	//msleep(100);
	//printk("%s:get pin level again,pin=%d, level = %d\n",__FUNCTION__,call_irq, gpio_get_value(call_gpio));
	
	//disable_irq(call_irq);
	msleep(80);
	for(i = 0; i < 3; i++) {
		level = gpio_get_value(rk3036_tve->call_gpio);
		if(level < 0)
		{
			printk("%s:get pin level again,pin=%d,i=%d\n",__FUNCTION__,rk3036_tve->call_irq,i);
			msleep(1);
			continue;
		}
		else
		break;
	}

	if(level < 0)
	{
		printk("%s:get pin level  err!\n",__FUNCTION__);
		return IRQ_HANDLED;
	}
	//printk("%s:level = %d\n",__FUNCTION__, level);
	if (!level) {
		call_falgs = 1;
		wake_up_interruptible(&rk3036_tve->call_wait);
		irq_set_irq_type(rk3036_tve->call_irq, IRQF_TRIGGER_RISING);
		door_flags = 1;
		//wake_up_interruptible(&rk3036_tve->wait);
		input_report_key(rk3036_tve->input_dev, KEY_RIGHT, 1); //按键被按下 "1"
                input_sync(rk3036_tve->input_dev); //report到用户空间 
		msleep(50);
		input_report_key(rk3036_tve->input_dev, KEY_RIGHT, 0); 
                input_sync(rk3036_tve->input_dev); 
		
		/**************************************/
		if (!delete_led) {
			gpio_set_value(rk3036_tve->key_gpio, 1);
			msleep(10);	
		}
		if (audio_flags) {
			gpio_set_value(rk3036_tve->spk_ctl_gpio, 1);
			msleep(10);
		}
		/**************************************/
		printk("%s: open\n",__FUNCTION__);
	} else {
		irq_set_irq_type(rk3036_tve->call_irq, IRQF_TRIGGER_FALLING);
		door_flags = 0;
		//wake_up_interruptible(&rk3036_tve->wait);
		input_report_key(rk3036_tve->input_dev, KEY_LEFT, 1); //按键被按下 "1"
                input_sync(rk3036_tve->input_dev); //report到用户空间
		msleep(50);
		input_report_key(rk3036_tve->input_dev, KEY_LEFT, 0);
                input_sync(rk3036_tve->input_dev);
		
		/***************************************/
		if (!delete_led) {
			gpio_set_value(rk3036_tve->key_gpio, 0);
			msleep(10);
		}
		if (audio_flags) {
			gpio_set_value(rk3036_tve->spk_ctl_gpio, 0);
			msleep(10);
		}
		/***************************************/
		printk("%s: close\n", __FUNCTION__);	
	}
	//printk("call_irq_func----\n");
	rk3036_tve->timer_flags = 10;	

	call_irq_status = 0;
	enable_irq(rk3036_tve->call_irq);
	return IRQ_HANDLED;

}
#if 1
static void call_timer_handle(unsigned long data)
{
	struct rk3036_tve *rk3036_tve = (struct rk3036_tve *)data;
	rk3036_tve->timer_flags = !rk3036_tve->timer_flags;
	gpio_set_value(rk3036_tve->key_gpio, rk3036_tve->timer_flags);
	if (!rk3036_tve->timer_flags && rk3036_tve->timer_count) {
		rk3036_tve->timer_count--;
	} else if (!rk3036_tve->timer_flags && !rk3036_tve->timer_count) {
		rk3036_tve->timer_flags = 10;
		return ;
	}
	
	rk3036_tve->call_timer.expires = jiffies + HZ;
	add_timer(&rk3036_tve->call_timer);
	
}


static void custom_delay_function(struct work_struct *work) {
	int ret;
	struct rk3036_tve *rk3036_tve = container_of(work, struct rk3036_tve, call_work);
	//printk("%s start\n", __FUNCTION__);
	if (!gpio_get_value(rk3036_tve->met_gpio)) 
		goto custom_delay_end;
	
	ret = wait_event_interruptible_timeout(rk3036_tve->call_wait, call_falgs, msecs_to_jiffies(200));
	if (ret == 0) {
		/**timeout**/
		printk("timerout\n");
		input_report_key(rk3036_tve->input_dev, KEY_RIGHT, 1); //按键被按下 "1"
                input_sync(rk3036_tve->input_dev); //report到用户空间
		msleep(30);
		input_report_key(rk3036_tve->input_dev, KEY_RIGHT, 0);
                input_sync(rk3036_tve->input_dev);
		
		if (!delete_led && rk3036_tve->timer_flags == 10) {
			rk3036_tve->timer_count = 10;
			rk3036_tve->timer_flags = 1;
			gpio_set_value(rk3036_tve->key_gpio, rk3036_tve->timer_flags);
			rk3036_tve->call_timer.expires = jiffies + HZ;
			add_timer(&rk3036_tve->call_timer);
		} else {
			rk3036_tve->timer_count = 10;
		}
		
		
	} else if (ret == -ERESTARTSYS) {
		/***wake up**/
		printk("wake up\n");
	}
custom_delay_end:	
	call_falgs = 0;
	//printk("%s end\n", __FUNCTION__);
	call_irq1_status = 0;
	enable_irq(rk3036_tve->call_irq1);
	
}


static irqreturn_t call_irq_func1(int irq, void *dev_id) {
	struct rk3036_tve *rk3036_tve = (struct rk3036_tve *)dev_id;
	
	printk("call_irq_func1 start\n");
	if (!call_irq1_status) {
		disable_irq_nosync(rk3036_tve->call_irq1);
		call_irq1_status = 1;
	} else {
		return IRQ_HANDLED;
	}
	
	/**调度工作队列去检测**/
	schedule_work(&rk3036_tve->call_work);
	//enable_irq(rk3036_tve->call_irq1);
	printk("call_irq_func1 end\n");
	return IRQ_HANDLED;
}

#else

static irqreturn_t call_irq_func1(int irq, void *dev_id)
{
	int i;
	int level;
	struct rk3036_tve *rk3036_tve = (struct rk3036_tve *)dev_id;
	if (irq_status == 1) {
		return IRQ_HANDLED;
	}
	irq_status = 1;	
	//msleep(100);
	//printk("%s:get pin level again,pin=%d, level = %d\n",__FUNCTION__,call_irq, gpio_get_value(call_gpio));	
	//disable_irq(call_irq);
	disable_irq_nosync(rk3036_tve->call_irq1);
	msleep(100);
	for(i = 0; i < 3; i++) {
		level = gpio_get_value(rk3036_tve->call_gpio1);
		if(level < 0)
		{
			printk("%s:get pin level again,pin=%d,i=%d\n",__FUNCTION__,rk3036_tve->call_irq1,i);
			msleep(1);
			continue;
		}
		else
		break;
	}

	if(level < 0)
	{
		printk("%s:get pin level  err!\n",__FUNCTION__);
		return IRQ_HANDLED;
	}
	//printk("%s:level = %d\n",__FUNCTION__, level);
	if (!level) {
		irq_set_irq_type(rk3036_tve->call_irq1, IRQF_TRIGGER_RISING);
		door_flags = 1;
		wake_up_interruptible(&rk3036_tve->wait);
		input_report_key(rk3036_tve->input_dev, KEY_RIGHT, 1); //按键被按下 "1"
                input_sync(rk3036_tve->input_dev); //report到用户空间 
		msleep(50);
		input_report_key(rk3036_tve->input_dev, KEY_RIGHT, 0); 
                input_sync(rk3036_tve->input_dev); 
		
		/**************************************/
		//gpio_set_value(rk3036_tve->key_gpio, 1);
		//msleep(10);
		if (audio_flags) {
			gpio_set_value(rk3036_tve->spk_ctl_gpio, 1);
			msleep(10);
		}
		/**************************************/
		printk("%s: open\n",__FUNCTION__);
	} else {
		irq_set_irq_type(rk3036_tve->call_irq1, IRQF_TRIGGER_FALLING);
		door_flags = 0;
		wake_up_interruptible(&rk3036_tve->wait);
		input_report_key(rk3036_tve->input_dev, KEY_LEFT, 1); //按键被按下 "1"
                input_sync(rk3036_tve->input_dev); //report到用户空间
		msleep(50);
		input_report_key(rk3036_tve->input_dev, KEY_LEFT, 0);
                input_sync(rk3036_tve->input_dev);
		
		/***************************************/
		//gpio_set_value(rk3036_tve->key_gpio, 0);
		//msleep(10);
		if (audio_flags) {
			gpio_set_value(rk3036_tve->spk_ctl_gpio, 0);
			msleep(10);
		}
		/***************************************/
		printk("%s: close\n", __FUNCTION__);	
	}
	//printk("call_irq_func----\n");
		
	enable_irq(rk3036_tve->call_irq1);
	irq_status = 0;
	return IRQ_HANDLED;

}
#endif
#if 0
static irqreturn_t call_irq_func2(int irq, void *dev_id)
{
	int i;
	int level;
	struct rk3036_tve *rk3036_tve = (struct rk3036_tve *)dev_id;
	if (met_irq_status == 1) {
		return IRQ_HANDLED;
	}
	met_irq_status = 1;	
	//msleep(100);
	//printk("%s:get pin level again,pin=%d, level = %d\n",__FUNCTION__,call_irq, gpio_get_value(call_gpio));	
	//disable_irq(call_irq);
	//msleep(100);
	for(i = 0; i < 3; i++) {
		level = gpio_get_value(rk3036_tve->met_gpio);
		if(level < 0)
		{
			printk("%s:get pin level again,pin=%d,i=%d\n",__FUNCTION__,rk3036_tve->call_irq1,i);
			msleep(1);
			continue;
		}
		else
		break;
	}

	if(level < 0)
	{
		printk("%s:get pin level  err!\n",__FUNCTION__);
		return IRQ_HANDLED;
	}
	//printk("%s:level = %d\n",__FUNCTION__, level);
	if (!level) {
		irq_set_irq_type(rk3036_tve->met_irq, IRQF_TRIGGER_RISING);
		input_report_key(rk3036_tve->input_dev, KEY_UP, 1); //按键被按下 "1"
                input_sync(rk3036_tve->input_dev); //report到用户空间 
		msleep(30);
		input_report_key(rk3036_tve->input_dev, KEY_UP, 0); 
                input_sync(rk3036_tve->input_dev); 
		
		/**************************************/
		printk("%s: open\n",__FUNCTION__);
	} else {
		irq_set_irq_type(rk3036_tve->met_irq, IRQF_TRIGGER_FALLING);
		input_report_key(rk3036_tve->input_dev, KEY_DOWN, 1); //按键被按下 "1"
                input_sync(rk3036_tve->input_dev); //report到用户空间
		msleep(30);
		input_report_key(rk3036_tve->input_dev, KEY_DOWN, 0);
                input_sync(rk3036_tve->input_dev);
		
		/***************************************/
		printk("%s: close\n", __FUNCTION__);	
	}
	//printk("call_irq_func----\n");
		
	//enable_irq(call_irq);
	met_irq_status = 0;
	return IRQ_HANDLED;

}
#endif
static int custom_rk3036_tve_parse_dt(struct device_node *np, struct rk3036_tve *rk3036_tve) {
	int ret;
	unsigned int gpio_led;
	enum of_gpio_flags gpio_flags;
	
	/***********************open lock gpio ************************/
	rk3036_tve->lock_gpio = of_get_named_gpio_flags(np, "lock_ctl_io", 0, &gpio_flags);
	ret = gpio_request(rk3036_tve->lock_gpio, "lock_ctl_io");
	if (ret < 0) {
		printk("lock_gpio request failed\n");
		return -ENODEV;
	}
	gpio_direction_output(rk3036_tve->lock_gpio, 0);
	
	rk3036_tve->lock_en = of_get_named_gpio_flags(np, "lock_en_io", 0, &gpio_flags);
	ret = gpio_request(rk3036_tve->lock_en, "lock_en_io");
	if (ret < 0) {
		printk("lock_en request failed\n");
		return -ENODEV;
	}
	gpio_direction_output(rk3036_tve->lock_en, 0);
	
	/**********************led gpio********************************/
	gpio_led = of_get_named_gpio_flags(np, "led_ctl_io", 0, &gpio_flags);
	ret = gpio_request(gpio_led, "led_ctl_io");
	if (ret < 0) {
		printk("spk_ctl_gpio request failed\n");
		return -ENODEV;
	}
	gpio_direction_output(gpio_led, 0);
	
	rk3036_tve->spk_ctl_gpio = of_get_named_gpio_flags(np, "spk_ctl_io", 0, &gpio_flags);
	ret = gpio_request(rk3036_tve->spk_ctl_gpio, "spk_ctl_gpio");
	if (ret < 0) {
		printk("spk_ctl_gpio request failed\n");
		return -ENODEV;
	}
	gpio_direction_output(rk3036_tve->spk_ctl_gpio, 0);
	printk("spk_ctl_gpio request success and output\n");
	
	rk3036_tve->key_gpio = of_get_named_gpio_flags(np, "key-gpio", 0, &gpio_flags);
	ret = gpio_request(rk3036_tve->key_gpio, "key_gpio");
	if (ret < 0) {
		printk("key-gpio request failed\n");
		return -ENODEV;
	}
	gpio_direction_output(rk3036_tve->key_gpio, 0);
	printk("key-gpio request success and output LOW\n");
	
	rk3036_tve->apk_ctl_gpio = of_get_named_gpio_flags(np, "apk_ctl_io", 0, &gpio_flags);
	ret = gpio_request(rk3036_tve->apk_ctl_gpio, "apk_ctl_gpio");
	if (ret < 0) {
		printk("apk_ctl_gpio request failed\n");
		return -ENODEV;
	}
	gpio_direction_output(rk3036_tve->apk_ctl_gpio, 0);
	printk("apk_ctl_gpio request success and output LOW\n");
	
	rk3036_tve->call_gpio = of_get_named_gpio_flags(np, "call-gpio", 0, &gpio_flags);
	
	if (rk3036_tve->call_gpio > 0) {
		ret = gpio_request(rk3036_tve->call_gpio, "call_gpio");
		if (ret < 0) {
			printk("call_gpio: failed to request GPIO %d,"
				" error %d\n", rk3036_tve->call_gpio, ret);
		} else {
			ret = gpio_direction_input(rk3036_tve->call_gpio);
			if (ret < 0) {
				printk("call_gpio: failed to configure input"
					" direction for GPIO %d, error %d\n", rk3036_tve->call_gpio, ret);
				gpio_free(rk3036_tve->call_gpio);
			}
		}		
	}
	
	rk3036_tve->call_gpio1 = of_get_named_gpio_flags(np, "call-gpio1", 0, &gpio_flags);
	
	if (rk3036_tve->call_gpio1 > 0) {
		ret = gpio_request(rk3036_tve->call_gpio1, "call_gpio1");
		if (ret < 0) {
			printk("call_gpio1: failed to request GPIO %d,"
				" error %d\n", rk3036_tve->call_gpio1, ret);
		} else {
			ret = gpio_direction_input(rk3036_tve->call_gpio1);
			if (ret < 0) {
				printk("call_gpio1: failed to configure input"
					" direction for GPIO %d, error %d\n", rk3036_tve->call_gpio1, ret);
				gpio_free(rk3036_tve->call_gpio1);
			}
		}		
	}
	
	rk3036_tve->met_gpio = of_get_named_gpio_flags(np, "met-gpio", 0, &gpio_flags);
	
	if (rk3036_tve->met_gpio > 0) {
		ret = gpio_request(rk3036_tve->met_gpio, "met-gpio");
		if (ret < 0) {
			printk("call_gpio1: failed to request GPIO %d,"
				" error %d\n", rk3036_tve->met_gpio, ret);
		} else {
			ret = gpio_direction_input(rk3036_tve->met_gpio);
			if (ret < 0) {
				printk("call_gpio1: failed to configure input"
					" direction for GPIO %d, error %d\n", rk3036_tve->met_gpio, ret);
				gpio_free(rk3036_tve->met_gpio);
			}
		}		
	}
	
	return 0;
}
static int rk3036_tve_parse_dt(struct device_node *np,
			       struct rk3036_tve *rk3036_tve)
{
	int ret;
	u32 val;
	int getdac;

	if (rk3036_tve->soctype == SOC_RK312X) {
		ret = of_property_read_u32(np, "test_mode", &val);
		if (ret < 0)
			return -1;
		else
			rk3036_tve->test_mode = val;
	}

	ret = of_property_read_u32(np, "saturation", &val);
	if ((val == 0) || (ret < 0))
		return -1;
	else
		rk3036_tve->saturation = val;

	ret = of_property_read_u32(np, "brightcontrast", &val);
	if ((val == 0) || (ret < 0))
		return -1;
	else
		rk3036_tve->brightcontrast = val;

	ret = of_property_read_u32(np, "adjtiming", &val);
	if ((val == 0) || (ret < 0))
		return -1;
	else
		rk3036_tve->adjtiming = val;

	ret = of_property_read_u32(np, "lumafilter0", &val);
	if ((val == 0) || (ret < 0))
		return -1;
	else
		rk3036_tve->lumafilter0 = val;

	ret = of_property_read_u32(np, "lumafilter1", &val);
	if ((val == 0) || (ret < 0))
		return -1;
	else
		rk3036_tve->lumafilter1 = val;

	ret = of_property_read_u32(np, "lumafilter2", &val);
	if ((val == 0) || (ret < 0))
		return -1;
	else
		rk3036_tve->lumafilter2 = val;

	ret = of_property_read_u32(np, "daclevel", &val);
	if ((val == 0) || (ret < 0)) {
		return -1;
	} else {
		rk3036_tve->daclevel = val;
		if (rk3036_tve->soctype == SOC_RK322X) {
			getdac = rockchip_get_cvbs_adjust();
			if (getdac > 0) {
				rk3036_tve->daclevel =
				getdac + 5 + val - RK322X_VDAC_STANDARD;
				if (rk3036_tve->daclevel > 0x3f ||
				    rk3036_tve->daclevel < 0) {
					pr_err("rk322x daclevel error!\n");
					rk3036_tve->daclevel = val;
				}
			}
		}
	}
	custom_rk3036_tve_parse_dt(np, rk3036_tve);
	
	return 0;
}
/*******************************************************************/
static long fm_ops_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	return 0;
}
static ssize_t fm_ops_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
	int i;
	int ret;
	int level;
	int buffer[10];
	struct rk3036_tve *rk3036_tve = filp->private_data;
	
	ret = copy_from_user(buffer, buf, len);
	if (ret) {
		printk("fm_ops_read copy_to_user failed\n");
		return len - ret;
	} else if (buffer[0] == 0x1000){
		audio_flags = 1;
		for(i = 0; i < 3; i++) {
			level = gpio_get_value(rk3036_tve->call_gpio);
			if(level < 0)
			{
				printk("%s:get pin level again,pin=%d,i=%d\n",__FUNCTION__, rk3036_tve->call_gpio, i);
				msleep(1);
				continue;
			}
			else
			break;
		}

		if (level == 0) {
			gpio_set_value(rk3036_tve->spk_ctl_gpio, 1);
			msleep(200);
		}
	}
	return len - ret;	

	return 0;
}
static ssize_t fm_ops_read(struct file *filp, char *buf, size_t len, loff_t *off) {
/*	int ret;
	struct rk3036_tve *rk3036_tve = filp->private_data;
	if (door_flags == 2) {
		wait_event_interruptible(rk3036_tve->wait, door_flags != 2);
		door_flags = 2;
	}
	ret = copy_to_user(buf, &door_flags, sizeof(door_flags));
	if (ret) {
		printk("fm_ops_read copy_to_user failed\n");
		return sizeof(door_flags) - ret;
	}
	return sizeof(door_flags) - ret;
*/
	return 0;
}

static int fm_ops_open(struct inode *inode, struct file *filp) {
	struct fm_platform *plat = container_of(inode->i_cdev, struct fm_platform, cdev);
	struct rk3036_tve *rk3036_tve = container_of(plat, struct rk3036_tve, fm_pltf);
	filp->private_data = (void *)rk3036_tve;
	return 0;
}
static int fm_ops_release(struct inode *inode, struct file *filp) {
	return 0;
}

static struct file_operations fm_ops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = fm_ops_ioctl,
	.write = fm_ops_write,
	.read = fm_ops_read,
	.open = fm_ops_open,
	.release = fm_ops_release,
};
#if 0
static int fm_pltf_cdev_destroy(struct rk3036_tve *ddata)
{
	/*字符设备的释放和销毁*/
	device_remove_file(&ddata->fm_pltf.dev, &dev_attr_enable);
	device_destroy(ddata->fm_pltf.cls, ddata->fm_pltf.dev_t);
	class_destroy(ddata->fm_pltf.cls);
	cdev_del(&ddata->fm_pltf.cdev);
	unregister_chrdev_region(ddata->fm_pltf.dev_t, 1);	
	//kfree(ddata->fm_pltf);
	return 0;
}
#endif
static ssize_t attr_gpio_enable_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int ret;
	ret = gpio_get_value(rk3036_tve->apk_ctl_gpio);
	printk("gpio_get_value apk_ctl_gpio :%d\n", ret);
	return sprintf(buf, "%d\n", ret);
}

static ssize_t attr_gpio_enable_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	unsigned long val;
	//int ret;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	
	if (val == 1 || val == 0) {
		gpio_set_value(rk3036_tve->apk_ctl_gpio, (unsigned int)val);
		printk("gpio_set_value apk_ctl_gpio :%d\n", (unsigned int)val);
	}
	return size;
}

static DEVICE_ATTR(enable, 0666, attr_gpio_enable_show, attr_gpio_enable_store);
#if 1
static ssize_t attr_gpio_spkctl_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int ret;
	ret = gpio_get_value(rk3036_tve->spk_ctl_gpio);
	printk("gpio_get_value apk_ctl_gpio :%d\n", ret);
	return sprintf(buf, "%d\n", ret);
}

static ssize_t attr_gpio_spkctl_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	unsigned long val;
	//int ret;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	
	if (val == 1 || val == 0) {
		gpio_set_value(rk3036_tve->spk_ctl_gpio, (unsigned int)val);
		msleep(300);
		printk("gpio_set_value apk_ctl_gpio :%d\n", (unsigned int)val);
	} 
	return size;
}

static DEVICE_ATTR(spkctl, 0666, attr_gpio_spkctl_show, attr_gpio_spkctl_store);
#endif
static ssize_t attr_opendoor_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", opendoor_flag);
}

static ssize_t attr_opendoor_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	unsigned long val;
	//int ret;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	
	if (val == 1 || val == 2) {
		if (!opendoor_flag) {
			opendoor_flag = (unsigned int)val;
			schedule_work(&rk3036_tve->work);
			//complete(&rk3036_tve->com);
		}
	}
	return size;
}

static DEVICE_ATTR(opendoor, 0666, attr_opendoor_show, attr_opendoor_store);

#if 1
static ssize_t attr_gpio_capval_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", custom_capval);
}

static ssize_t attr_gpio_capval_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	unsigned long val;
	//int ret;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	
	if (val >= 0 && val <= 31 ) {
		custom_capval = (unsigned int)val;
		rk312x_custom_capval(custom_capval);
	}
	return size;
}

static DEVICE_ATTR(capval, 0666, attr_gpio_capval_show, attr_gpio_capval_store);
#endif
#if 1
static ssize_t attr_gpio_ledctl_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", gpio_get_value(rk3036_tve->key_gpio));
}

static ssize_t attr_gpio_ledctl_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	unsigned long val;
	//int ret;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	
	if (val >= 0) {
		delete_led = 1;
		gpio_set_value(rk3036_tve->key_gpio, !!val);
	}
	return size;
}

static DEVICE_ATTR(ledctl, 0666, attr_gpio_ledctl_show, attr_gpio_ledctl_store);
#endif
#if 1
static ssize_t attr_gpio_kodet_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", !gpio_get_value(rk3036_tve->met_gpio));
}

static ssize_t attr_gpio_kodet_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(kodet, 0666, attr_gpio_kodet_show, attr_gpio_kodet_store);
#endif
static int custom_platfrom_hwinit(struct platform_device *pdev) {
	int ret;
	struct fm_platform *plat;	
	struct rk3036_tve *ddata = platform_get_drvdata(pdev);
	plat = &ddata->fm_pltf;
	ret = alloc_chrdev_region(&plat->dev_t, 0, 1, DOOR_NAME);
	if (ret) {
		printk("alloc dev_t failed\n");
		goto chrdev_err;
	}
	
	DBG("alloc %s:%d:%d\n", DOOR_NAME, MAJOR(plat->dev_t), MINOR(plat->dev_t));
	cdev_init(&plat->cdev, &fm_ops);
	plat->cdev.owner = THIS_MODULE;
	plat->cdev.ops = &fm_ops;
	
	ret = cdev_add(&plat->cdev, plat->dev_t, 1);
	if (ret) {
		printk("add dev_t failed\n");
		goto cdev_err;
	}
	
	plat->cls = class_create(THIS_MODULE, DOOR_NAME);
	if (IS_ERR(plat->cls)) {
		ret = PTR_ERR(plat->cls);
		printk("class_create err:%d\n", ret);
		goto class_err;
	}

	plat->dev = device_create(plat->cls, NULL, plat->dev_t, NULL, DOOR_NAME);
	if (IS_ERR(plat->dev)) {
                ret = PTR_ERR(plat->dev);
		printk("device_create err:%d\n", ret);
                goto device_err;
        }
	ret = device_create_file(plat->dev, &dev_attr_enable);
	if(ret) {
		printk("device_create_file err:%d\n", ret);
		goto dev_file_err;
	}
	#if 1
	ret = device_create_file(plat->dev, &dev_attr_spkctl);
	if(ret) {
		printk("device_create_file err:%d\n", ret);
		goto dev_file_err1;
	}
	#endif
	#if 1
	ret = device_create_file(plat->dev, &dev_attr_capval);
	if(ret) {
		printk("device_create_file err:%d\n", ret);
		goto dev_file_err2;
	}
	
	ret = device_create_file(plat->dev, &dev_attr_ledctl);
	if(ret) {
		printk("device_create_file err:%d\n", ret);
		goto dev_file_err3;
	}
	ret = device_create_file(plat->dev, &dev_attr_kodet);
	if(ret) {
		printk("device_create_file err:%d\n", ret);
		goto dev_file_err4;
	}
	#endif
		
	ret = device_create_file(plat->dev, &dev_attr_opendoor);
	if(ret) {
		printk("device_create_file err:%d\n", ret);
		goto dev_file_err5;
	}
	
	
	//init_waitqueue_head(&ddata->wait);
	init_waitqueue_head(&ddata->call_wait);
	init_timer(&ddata->call_timer);
	ddata->call_timer.function = &call_timer_handle;
	ddata->call_timer.data = (unsigned long)ddata;
	rk3036_tve->timer_flags = 10;
	
	return 0;
dev_file_err5:
	device_remove_file(plat->dev, &dev_attr_kodet);	
dev_file_err4:
	device_remove_file(plat->dev, &dev_attr_ledctl);
dev_file_err3:
	device_remove_file(plat->dev, &dev_attr_capval);
dev_file_err2:
	device_remove_file(plat->dev, &dev_attr_spkctl);
dev_file_err1:
	device_remove_file(plat->dev, &dev_attr_enable);	
dev_file_err:
   	device_destroy(plat->cls, plat->dev_t);
device_err:
	class_destroy(plat->cls);
class_err:
	cdev_del(&plat->cdev);
cdev_err:
	unregister_chrdev_region(plat->dev_t, 1);
chrdev_err:
	return ret;
}
static int custom_driver_startup(struct rk3036_tve *rk3036_tve) {
	int error, ret;            
        struct input_dev *input_dev = input_allocate_device();   //分配一个输入设备   
        if (!input_dev) {    
              
                 printk(KERN_ERR" Not enough memory\n");    
                 error = -ENOMEM;    
                 goto err_free_dev;    
        }    
  
        input_dev->name ="DoorCallKey";
        __set_bit(EV_KEY, input_dev->evbit);//注册设备支持的事件类型  
        __set_bit(KEY_RIGHT, input_dev->keybit); //注册设备支持的按键  
        __set_bit(KEY_LEFT, input_dev->keybit);
	
        error = input_register_device(input_dev);  //注册输入设备  
        if (error) {   
              
                 printk(KERN_ERR" Failed to register device\n");    
                 goto err_free_dev;    
        }
	rk3036_tve->input_dev = input_dev;
	/***************************************/
	rk3036_tve->call_irq = gpio_to_irq(rk3036_tve->call_gpio);
	if (rk3036_tve->call_irq < 0) {
		ret = rk3036_tve->call_irq;
		printk("irq for gpio %d error(%d)\n", rk3036_tve->call_gpio, ret);
		gpio_free(rk3036_tve->call_gpio);
		return ret;
	} else {
		ret = request_threaded_irq(rk3036_tve->call_irq, NULL, call_irq_func, IRQF_TRIGGER_LOW|IRQF_ONESHOT, "call_irq_func", rk3036_tve);
		//ret = request_irq(rk3036_tve->call_irq, call_irq_func, IRQF_TRIGGER_LOW|IRQF_ONESHOT, "call_irq_func", rk3036_tve);
		if (ret < 0) {
			printk( "call_irq: request irq failed\n");
			return -ENODEV;
		}
		printk( "call_irq: request irq success\n");
	}
#if 1	
	rk3036_tve->call_irq1 = gpio_to_irq(rk3036_tve->call_gpio1);
	if (rk3036_tve->call_irq1 < 0) {
		ret = rk3036_tve->call_irq1;
		printk("irq for gpio %d error(%d)\n", rk3036_tve->call_gpio1, ret);
		gpio_free(rk3036_tve->call_gpio1);
		return ret;
	} else {
		//ret = request_threaded_irq(rk3036_tve->call_irq1, NULL, call_irq_func1, IRQF_TRIGGER_LOW|IRQF_ONESHOT, "call_irq_func1", rk3036_tve);
		ret = request_irq(rk3036_tve->call_irq1, call_irq_func1, /*IRQF_TRIGGER_HIGH|IRQF_ONESHOT */IRQF_TRIGGER_RISING, "call_irq_func1", rk3036_tve);
		if (ret < 0) {
			printk( "call_irq: request irq1 failed\n");
			return -ENODEV;
		}
		printk( "call_irq: request irq1 success\n");
	}
#endif	
#if 0	
	rk3036_tve->met_irq = gpio_to_irq(rk3036_tve->met_gpio);
	if (rk3036_tve->met_irq < 0) {
		ret = rk3036_tve->met_irq;
		printk("irq for gpio %d error(%d)\n", rk3036_tve->met_gpio, ret);
		gpio_free(rk3036_tve->met_gpio);
		return ret;
	} else {
		ret = request_threaded_irq(rk3036_tve->met_irq, NULL, call_irq_func2, IRQF_TRIGGER_LOW|IRQF_ONESHOT, "call_irq_func2", rk3036_tve);
		//ret = request_irq(rk3036_tve->met_irq, call_irq_func2, IRQF_TRIGGER_LOW|IRQF_ONESHOT, "call_irq_func2", rk3036_tve);
		if (ret < 0) {
			printk( "call_irq: request irq2 failed\n");
			return -ENODEV;
		}
		printk( "call_irq: request irq2 success\n");
	}
#endif
	#ifdef HAVE_WORK_CTL
	//tasklet_init(&rk3036_tve->call_tasklet, custom_opendoor_control, (unsigned long)rk3036_tve);
	INIT_WORK(&rk3036_tve->work, custom_opendoor_control);
	INIT_WORK(&rk3036_tve->call_work, custom_delay_function);
	init_completion(&rk3036_tve->com);
	hrtimer_init(&rk3036_tve->htimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	rk3036_tve->htimer.function = hr_opendoor_callback;
	//tasklet_hi_schedule(&rk3036_tve->call_tasklet);
	//schedule_work(&rk3036_tve->work);
	#endif
        return 0;    
    
err_free_dev:    
        input_free_device(input_dev);
        return error;
}
static int rk3036_tve_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *res;
	const struct of_device_id *match;
	int i;
	int ret;

	match = of_match_node(rk3036_tve_dt_ids, np);
	if (!match)
		return PTR_ERR(match);

	rk3036_tve = devm_kzalloc(&pdev->dev,
				  sizeof(struct rk3036_tve), GFP_KERNEL);
	if (!rk3036_tve) {
		dev_err(&pdev->dev, "rk3036 tv encoder device kmalloc fail!");
		return -ENOMEM;
	}

	if (!strcmp(match->compatible, "rockchip,rk3036-tve")) {
		rk3036_tve->soctype = SOC_RK3036;
		rk3036_tve->inputformat = INPUT_FORMAT_RGB;
	} else if (!strcmp(match->compatible, "rockchip,rk312x-tve")) {
		rk3036_tve->soctype = SOC_RK312X;
		rk3036_tve->inputformat = INPUT_FORMAT_YUV;
	} else if (!strcmp(match->compatible, "rockchip,rk322x-tve")) {
		rk3036_tve->soctype = SOC_RK322X;
		rk3036_tve->inputformat = INPUT_FORMAT_YUV;
	} else {
		dev_err(&pdev->dev, "It is not a valid tv encoder!");
		return -ENOMEM;
	}

	ret = rk3036_tve_parse_dt(np, rk3036_tve);
	if (ret) {
		dev_err(&pdev->dev, "TVE parse dts error!");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, rk3036_tve);
	rk3036_tve->dev = &pdev->dev;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rk3036_tve->reg_phy_base = res->start;
	rk3036_tve->len = resource_size(res);
	rk3036_tve->regbase = ioremap(res->start, rk3036_tve->len);
	if (IS_ERR(rk3036_tve->regbase)) {
		dev_err(&pdev->dev,
			"rk3036 tv encoder device map registers failed!");
		return PTR_ERR(rk3036_tve->regbase);
	}
	if (rk3036_tve->soctype == SOC_RK322X) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		rk3036_tve->len = resource_size(res);
		rk3036_tve->vdacbase = devm_ioremap(rk3036_tve->dev,
						    res->start,
						    rk3036_tve->len);
		if (IS_ERR(rk3036_tve->vdacbase)) {
			dev_err(&pdev->dev,
				"rk3036 tv encoder device dac map registers failed!");
			return PTR_ERR(rk3036_tve->vdacbase);
		}
		rk3036_tve->dac_clk =
				devm_clk_get(rk3036_tve->dev, "pclk_vdac");
		if (IS_ERR(rk3036_tve->dac_clk)) {
			dev_err(&pdev->dev,
				"Unable to get vdac_clk\n");
			return PTR_ERR(rk3036_tve->dac_clk);
		}
		clk_prepare_enable(rk3036_tve->dac_clk);
		if (cvbsformat < 0)
			rk322x_dac_init();
	}
	mutex_init(&rk3036_tve->tve_lock);
	INIT_LIST_HEAD(&(rk3036_tve->modelist));
	for (i = 0; i < ARRAY_SIZE(rk3036_cvbs_mode); i++)
		fb_add_videomode(&rk3036_cvbs_mode[i], &(rk3036_tve->modelist));
	 if (cvbsformat >= 0) {
		rk3036_tve->mode =
			(struct fb_videomode *)&rk3036_cvbs_mode[cvbsformat];
		rk3036_tve->enable = 1;
		tve_switch_fb(rk3036_tve->mode, 1);
	} else {
		rk3036_tve->mode = (struct fb_videomode *)&rk3036_cvbs_mode[1];
	}
	rk3036_tve->ddev =
		rk_display_device_register(&display_cvbs, &pdev->dev, NULL);
	rk_display_device_enable(rk3036_tve->ddev);

	fb_register_client(&tve_fb_notifier);
	cvbsformat = -1;
	dev_info(&pdev->dev, "%s tv encoder probe ok\n", match->compatible);
	/**************************************************/
	printk("call_irq in probe function enable\n");
	custom_platfrom_hwinit(pdev);
	custom_driver_startup(rk3036_tve);
	return 0;
}

static void rk3036_tve_shutdown(struct platform_device *pdev)
{
}

static struct platform_driver rk3036_tve_driver = {
	.probe = rk3036_tve_probe,
	.remove = NULL,
	.driver = {
		.name = "rk3036-tve",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rk3036_tve_dt_ids),
	},
	.shutdown = rk3036_tve_shutdown,
};

static int __init rk3036_tve_init(void)
{
	return platform_driver_register(&rk3036_tve_driver);
}

static void __exit rk3036_tve_exit(void)
{
	platform_driver_unregister(&rk3036_tve_driver);
}

module_init(rk3036_tve_init);
module_exit(rk3036_tve_exit);

/* Module information */
MODULE_DESCRIPTION("ROCKCHIP RK3036 TV Encoder ");
MODULE_LICENSE("GPL");
