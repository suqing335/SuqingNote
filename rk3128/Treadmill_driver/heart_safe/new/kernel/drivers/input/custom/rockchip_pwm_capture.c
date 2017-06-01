
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <asm-generic/uaccess.h>
#include "rockchip_pwm_capture.h"
#include "yc_arrqueue.h"

#define BIT8_READ

/*sys/module/rk_pwm_capturectl/parameters,
modify code_print to change the value*/

static int rk_capture_print_code;
module_param_named(code_print, rk_capture_print_code, int, 0644);
#define DBG_CODE(args...) \
	do { \
		if (1) { \
			pr_info(args); \
		} \
	} while (0)

static int rk_capture_pwm_dbg_level;
module_param_named(dbg_level, rk_capture_pwm_dbg_level, int, 0644);
#define DBG(args...) \
	do { \
		if (1) { \
			pr_info(args); \
		} \
	} while (0)

#define DBG_ERR(args...) 	printk(args);

struct fm_platform {
    struct cdev cdev;
    dev_t dev_t;
    struct class *cls;
    struct device *dev;
};

struct rkxx_capturectl_drvdata {
	void __iomem *base;
	int irq;
	int capture_pwm_id;
	int handle_cpu_id;
	int wakeup;
	int clk_rate;
	unsigned long period;
	unsigned long temp_period;
	int pwm_freq_nstime;
	struct tasklet_struct capture_tasklet;
	struct wake_lock capturectl_wake_lock;
	
	/**add by cfj**/
	unsigned int pwm_gpio;
	unsigned int pwm_flag;
	unsigned int htcnt;
	int file_cnt;
	int pcount;
	int isempty_slp_oque;
	struct hrtimer htimer;//高精定时器，用于产生pwm波形
	struct yc_queue *pwm_out_queue;//存储pwm波形的us时间数值
	struct yc_queue *ishort_queue;//存储上层输入的数据，将会转成pwm_out_queue数据发出去
	struct yc_queue *ochar_queue;//存储解析pwm波形后的数据，上传到上层应用
	struct fm_platform fm_pltf;//字符设备的结构体
	//struct work_struct pwm_work;
	struct tasklet_struct loadpwm_tasklet;
//	struct task_struct *thread;
//	struct completion comp;
	wait_queue_head_t wait;
	spinlock_t file_lock;
	spinlock_t send_lock;
};

static struct rkxx_capturectl_drvdata *pdata = NULL;

//static unsigned char check_recever_data[] = {0xa1, 0xa2, 0xa3, 0xa4, 0xa6, 0xa7, 0xb7, 0xb5, 0x55, 0xb1, 0xb3, 0xcc};
#ifdef BIT8_READ
unsigned char pwm_data[8];
#else
unsigned char pwm_data[16];
#endif
#define HRTIMER_PWM_SEND 	1

#if 0//test
#define  startlen_mix	(4000-120)
#define  startlen_max	(4000+120)

#define  higthlen_mix 	(2400-120)
#define  higthlen_max 	(2400+120)

#define lowlen_mix 	(7200-120)
#define lowlen_max 	(7200+120)
static unsigned int start_pwm[2] = {22000, 4000};
static unsigned int higth_pwm[2] = {7200, 2400};
static unsigned int low_pwm[2] = {2400, 7200};
#else
#define  startlen_mix	(4000-150)
#define  startlen_max	(4000+150)

#define  higthlen_mix 	(2400-150)
#define  higthlen_max 	(2400+150)

#define lowlen_mix 	(7200-150)
#define lowlen_max 	(7200+150)	

static unsigned int start_pwm[2] = {7180, 2380};//us
static unsigned int higth_pwm[2] = {2380, 750};
static unsigned int low_pwm[2] = {750, 2380};
#endif
//static unsigned short pwm_end = 0;
//static unsigned int isempty_count = 0;
/*函数声明*/
static int fm_pltf_cdev_destroy(struct rkxx_capturectl_drvdata *ddata);

#if defined(HRTIMER_PWM_SEND)
static void load_send_pwm_func(unsigned long  data) {
	int ret, i;
	unsigned short pwm_end;
	struct rkxx_capturectl_drvdata *ddata = (struct rkxx_capturectl_drvdata *)data;
	if (!queue_isempty(ddata->ishort_queue)) {
		ret = dequeue(ddata->ishort_queue, &pwm_end);
		if (ret < 0) {
			DBG_ERR("dequeue ishort_queue failed\n");
		}
		/*for(i = 0; i < sizeof(unsigned short)*8; i++) {//小端方式往下发
			if ((pwm_end >> i) & 0x1) {*/
		for(i = (sizeof(unsigned short)*8 - 1); i >= 0; i--) {//大端方式往下发
			if (pwm_end & (0x1 << i)) {
				enqueue(ddata->pwm_out_queue, &higth_pwm[0]);
				enqueue(ddata->pwm_out_queue, &higth_pwm[1]);
			} else {
				enqueue(ddata->pwm_out_queue, &low_pwm[0]);
				enqueue(ddata->pwm_out_queue, &low_pwm[1]);
			}
		}
		//isempty_count = 0;
		DBG("equeue pwm_out_queue : 0x%x, i= %d\n", pwm_end, i);
	} else {
	/******当没有指令时，每个500ms会再发一次上次发的指令*****************/	
	/*	if (++isempty_count >= 52) {
			isempty_count = 0;
			if (pwm_end > 0) {
				for(i = (sizeof(unsigned short)*8 - 1); i >= 0; i--) {
					if (pwm_end & (0x1 << i)) {
						enqueue(ddata->pwm_out_queue, &higth_pwm[0]);
						enqueue(ddata->pwm_out_queue, &higth_pwm[1]);
					} else {
						enqueue(ddata->pwm_out_queue, &low_pwm[0]);
						enqueue(ddata->pwm_out_queue, &low_pwm[1]);
					}
				}
			}
		}*/
		/*删除这部分内容，这部分内容到apk发*/
	}
/*	if (ddata->isempty_slp_oque && !queue_isempty(ddata->ochar_queue)) {
		wake_up_interruptible(&ddata->wait);
		printk("wake up read func\n");
	}*/
}

static enum hrtimer_restart hrdown_callback(struct hrtimer *timer){
	int ret;
	unsigned int loadtmp;
	struct rkxx_capturectl_drvdata *ddata = container_of(timer, struct rkxx_capturectl_drvdata, htimer);
	if (!queue_isempty(ddata->pwm_out_queue) && ddata->htcnt) {
		ret = dequeue(ddata->pwm_out_queue, &loadtmp);
		if (ret < 0) {
			DBG_ERR("dequeue pwm_out_queue failed\n");
		}
		ddata->pwm_flag = !ddata->pwm_flag;
		gpio_set_value(ddata->pwm_gpio, ddata->pwm_flag);
		hrtimer_start(&ddata->htimer, ktime_set(0, loadtmp*1000), HRTIMER_MODE_REL);//定时器时间到了以后又进来，再取下一个数
	} else {
		//发送头波
		if (ddata->htcnt == 1) {//发送头波并且发低电平
			ddata->pwm_flag = 0;
			gpio_set_value(ddata->pwm_gpio, ddata->pwm_flag);
			hrtimer_start(&ddata->htimer, ktime_set(0, start_pwm[0]*1000), HRTIMER_MODE_REL);
			tasklet_hi_schedule(&ddata->loadpwm_tasklet);
		} else if (ddata->htcnt == 0) {//发送头波并且发送高电平
			ddata->pwm_flag = 1;
			gpio_set_value(ddata->pwm_gpio, ddata->pwm_flag);
			hrtimer_start(&ddata->htimer, ktime_set(0, start_pwm[1]*1000), HRTIMER_MODE_REL);
		}
		ddata->htcnt = !ddata->htcnt;
	}
	return HRTIMER_NORESTART;
}
#endif
#if 0
static int pwm_send_thread_func(void *arg) {
	int i;
	int j;
	int ret;
	unsigned int tmp;
	struct rkxx_capturectl_drvdata *ddata = arg;
	do{
		if (!queue_isempty(ddata->ishort_queue)) {
			spin_lock(&ddata->send_lock);
			while(!queue_isempty(ddata->ishort_queue)) {
				ret = dequeue(ddata->ishort_queue, &tmp);
				if (ret < 0) {
					DBG_ERR("dequeue ishort_queue failed\n");
				}
				for(i = 0; i < sizeof(unsigned short); i++) {
					if ((tmp >> i) & 0x1) {
						for(j = 0; j < 2; j++){
							//enqueue(ddata->pwm_out_queue, &higth_pwm[j]);
							pwm_s[i*2+j] = higth_pwm[j];
						}
					} else {
						for(j = 0; j < 2; j++){
							//enqueue(ddata->pwm_out_queue, &low_pwm[j]);
							pwm_s[i*2+j] = low_pwm[j];
						}
					}
				}					
			
			}
			spin_unlock(&ddata->send_lock);
		} else {
			ddata->isempty_slp_oque = 1;
			wait_event_interruptible(ddata->wait, !queue_isempty(ddata->ishort_queue));
			ddata->isempty_slp_oque = 0;
			DBG("pwm_send_thread_func wake up\n");			
		}
	}while (!kthread_should_stop());
	
	return 0;
}
#endif
#ifdef BIT8_READ
static unsigned char binary_to_8bit_hex(unsigned char *data) {
	unsigned char i, tmp = 0;
	for(i = 0; i < sizeof(unsigned char)*8; i++) {
	//	tmp |= data[i] << i;//小端的接收方式	
		tmp |= data[i] << (sizeof(unsigned char)*8 - 1 - i);//大端的接收方式
	}
	return tmp;
}
#if 0
static unsigned int check_8bit_hex(unsigned char data) {
	unsigned char i;
	for(i = 0; i < sizeof(check_recever_data)/sizeof(check_recever_data[0]); i++) {
		if (data == check_recever_data[i])
			return 1;
	}
	return 0;
}
#endif
#else
static unsigned short binary_to_16bit_hex(unsigned char *data) {
	unsigned char i;
	unsigned short tmp = 0;
	for(i = 0; i < sizeof(unsigned short)*8; i++) {
		tmp |= data[i] << i;
	}
	return tmp;
}
#endif

static void rk_pwm_capturectl_do_something(unsigned long  data){
#ifdef BIT8_READ	
	unsigned char tx_num;
#else
	unsigned short tx_num;
#endif	
	struct rkxx_capturectl_drvdata *ddata = (struct rkxx_capturectl_drvdata *)data;
	
	//printk("ustime = %lu\n", ddata->period);
	if ((ddata->period > startlen_mix) && (ddata->period < startlen_max)) {
		ddata->pcount = 0;
	} else if ((ddata->period > higthlen_mix) && (ddata->period < higthlen_max)) {
		pwm_data[ddata->pcount++] = 1;
	} else if ((ddata->period > lowlen_mix) && (ddata->period < lowlen_max)) {
		pwm_data[ddata->pcount++] = 0;
	} else {
		ddata->pcount = 0;
	}
#ifdef BIT8_READ	
	if (ddata->pcount >= 8) {		
		ddata->pcount = 0;
		tx_num = binary_to_8bit_hex(pwm_data);
	#if 0	
		if (check_8bit_hex(tx_num)) {
			if (!queue_isfull(ddata->ochar_queue)) {
				if (enqueue(ddata->ochar_queue, &tx_num) < 0) {
					DBG_ERR("enqueue  ochar_queue failed\n");
					enqueue(ddata->ochar_queue, &tx_num);
				}
			} else {
				DBG_ERR("ochar_queue isfull\n");
			}
		}
	#endif
		if (tx_num >= 0 && tx_num < 0xff) {
			if (!queue_isfull(ddata->ochar_queue)) {
				if (enqueue(ddata->ochar_queue, &tx_num) < 0) {
					DBG_ERR("enqueue  ochar_queue failed\n");
					enqueue(ddata->ochar_queue, &tx_num);
				}
			} else {
				DBG_ERR("ochar_queue isfull\n");
			}
		}
		if (ddata->isempty_slp_oque && !queue_isempty(ddata->ochar_queue)) {
			wake_up_interruptible(&ddata->wait);
			DBG("wake up read func\n");
		}
		DBG("tx_num = 0x%x\n", tx_num);		
	}
#else
	if (ddata->pcount >= 16) {		
		ddata->pcount = 0;
		tx_num = binary_to_16bit_hex(pwm_data);
		//if (!queue_isfull(ddata->ochar_queue)) {
		//	if (enqueue(ddata->ochar_queue, &tx_num) < 0) {
		//		DBG_ERR("enqueue  ochar_queue failed\n");
		//		enqueue(ddata->ochar_queue, &tx_num);
		//	}
		//} else {
		//	DBG_ERR("ochar_queue isfull\n");
		//}		
		DBG("tx_num = 0x%x\n", tx_num);		
	}

#endif	
}


static irqreturn_t rockchip_pwm_irq(int irq, void *dev_id)
{
	struct rkxx_capturectl_drvdata *ddata = dev_id;
	int val;
	int temp_hpr;
	int temp_lpr;
	int temp_period;
	unsigned int id = ddata->capture_pwm_id;
	//DBG("rockchip_pwm_irq start\n");
	if (id > 3)
		return IRQ_NONE;
	val = readl_relaxed(ddata->base + PWM_REG_INTSTS(id));
	//DBG("rockchip_pwm_irq PWM_CH_INT(id)=%lu, ppl=%lu, val=%d\n",PWM_CH_INT(id), PWM_CH_POL(id), val);
	if ((val & PWM_CH_INT(id)) == 0)
		return IRQ_NONE;
	//DBG("rockchip_pwm_irq val\n");
	if ((val & PWM_CH_POL(id)) == 0) {
		temp_hpr = readl_relaxed(ddata->base + PWM_REG_HPR);
		//DBG("hpr=%d\n", temp_hpr);
		temp_lpr = readl_relaxed(ddata->base + PWM_REG_LPR);
		//DBG("lpr=%d\n", temp_lpr);
		temp_period = ddata->pwm_freq_nstime * temp_lpr / 1000;
		if (temp_period > RK_PWM_TIME_BIT0_MIN) {
			ddata->period = ddata->temp_period
			    + ddata->pwm_freq_nstime * temp_hpr / 1000;
			tasklet_hi_schedule(&ddata->capture_tasklet);
			ddata->temp_period = 0;
			//DBG("period+ =%ld\n", ddata->period);
		} else {
			ddata->temp_period += ddata->pwm_freq_nstime
			    * (temp_hpr + temp_lpr) / 1000;
		}
	}
	writel_relaxed(PWM_CH_INT(id), ddata->base + PWM_REG_INTSTS(id));
	
	wake_lock_timeout(&ddata->capturectl_wake_lock, HZ);
	//DBG("rockchip_pwm_irq end\n");
	return IRQ_HANDLED;
}



static int rk_pwm_capturectl_hw_init(struct rkxx_capturectl_drvdata *ddata)
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
	switch (ddata->capture_pwm_id) {
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

/***********************************************************
*	我自定义的函数，及字符设备模型
***********************************************************/
#if 0
static int pop_queue_value(struct yc_queue *handle, unsigned char *data, int len) {
	int i;
	for (i = 0; i < len; i++) {
		if (!queue_isempty(handle)) {
			if (dequeue(handle, &data[i]) < 0) {
				return i;
			}
		} else {
			return i;
		}
	}
	return i;/*返回取出的数量*/
}

static int push_queue_value(struct yc_queue *handle, unsigned char *data, int len) {
	int i;
	for (i = 0; i < len; i++) {
		if (!queue_isfull(handle)) {
			if (enqueue(handle, &data[i]) < 0) {
				return i;
			}
		} else {
			return i;
		}
	}
	return i;/*返回存入的数量*/
}
#endif
static long fm_ops_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	return 0;
}
static ssize_t fm_ops_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {	
	int i;
	int ret;
	int copy_len;
	unsigned short tmp;
	struct rkxx_capturectl_drvdata *ddata = (struct rkxx_capturectl_drvdata *)filp->private_data;
	
	if (!ddata)
		return -1;
	if (!ddata->ishort_queue) {
		DBG_ERR("ishort_queue is NULL\n");
		return -2;
	}
	copy_len = len / sizeof(unsigned short);
	DBG("fm_ops_write start copy_len = %d\n", copy_len);
	//copy_len = (len > INPUT_SIZE ? INPUT_SIZE : len);
	for (i = 0; i < copy_len; i++) {
		ret = copy_from_user(&tmp, buf+i*sizeof(unsigned short), sizeof(unsigned short));
		if (ret) {
			DBG_ERR("fm_ops_write copy_to_user failed\n");
			return (i * sizeof(unsigned short));
		}
		//存数据到队列中
		if(!queue_isfull(ddata->ishort_queue)) {
			if (enqueue(ddata->ishort_queue, &tmp) < 0) {
				DBG_ERR("ishort_queue enqueue failed\n");
				return (i * sizeof(unsigned short));
			}
		} else {
			DBG_ERR("ishort_queue isfull\n");
			return (i * sizeof(unsigned short));
		}	
	}
	//if (i > 0 && ddata->isempty_slp_oque)
	//	wake_up_interruptible(&ddata->wait);
	DBG("fm_ops_write end\n");
	return (i * sizeof(unsigned short));
}
static ssize_t fm_ops_read(struct file *filp, char *buf, size_t len, loff_t *off) {	
	int i;
	int ret;
	int copy_len;
	unsigned char tmp;
	struct rkxx_capturectl_drvdata *ddata = filp->private_data;
	
	if (!ddata)
		return -1;

	if (!ddata->ochar_queue) {
		DBG_ERR("ochar_queue is NULL\n");
		return -2;
	}
	copy_len = len > OUTPUT_SIZE ? OUTPUT_SIZE : len;
	
	DBG("fm_ops_read start\n");
	for (i = 0; i < copy_len; i++) {
		//从队列中取数据
		if(!queue_isempty(ddata->ochar_queue)) {
			if (dequeue(ddata->ochar_queue, &tmp) < 0) {
				DBG_ERR("ochar_queue dequeue failed\n");
				return i;
			}			
		} else {
			ddata->isempty_slp_oque = 1;
			wait_event_interruptible(ddata->wait, !queue_isempty(ddata->ochar_queue));
			ddata->isempty_slp_oque = 0;
			if (dequeue(ddata->ochar_queue, &tmp) < 0) {
				DBG_ERR("ochar_queue dequeue failed\n");
				return i;
			}
			
			//DBG_ERR("ochar_queue isempty\n");
			//return i;
		}
		DBG("fm_ops_read data : %x\n", tmp);
		ret = copy_to_user(buf+i*sizeof(unsigned char), &tmp, sizeof(unsigned char));
		if (ret) {
			DBG_ERR("fm_ops_read copy_to_user failed\n");
			return i;
		}		
	}
	DBG("fm_ops_read end\n");
	return i;
}

static int fm_ops_open(struct inode *inode, struct file *filp) {
//	unsigned int flags;
	struct fm_platform *plat = container_of(inode->i_cdev, struct fm_platform, cdev);
	struct rkxx_capturectl_drvdata *ddata = container_of(plat, struct rkxx_capturectl_drvdata, fm_pltf);

	if (ddata->file_cnt == 0) {
		if (spin_trylock(&ddata->file_lock)){
			if (ddata->file_cnt == 0) {
				ddata->file_cnt++;
			} else {
				spin_unlock(&ddata->file_lock);
				return -1;
			}
		} else {
			return -1;
		}
		spin_unlock(&ddata->file_lock);
	} else {
		return -1;
	} 
	filp->private_data = (void *)ddata;
	//	DBG("fm_ops_open ddata = 0x%p, pdata =0x%p\n", ddata, pdata);
	//	flags = filp->f_flags & 0x0f;
	//	DBG("fm_ops_open f_flags=0x%x, 0x%x\n", filp->f_flags, flags);
	return 0;
}
static int fm_ops_release(struct inode *inode, struct file *filp) {
	struct fm_platform *plat = container_of(inode->i_cdev, struct fm_platform, cdev);
	struct rkxx_capturectl_drvdata *ddata = container_of(plat, struct rkxx_capturectl_drvdata, fm_pltf);
	if (ddata->file_cnt) {
		ddata->file_cnt--;
	}
	filp->private_data = NULL;
	DBG("fm_ops_release\n");
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

static int rockchip_pwm_custom(struct rkxx_capturectl_drvdata *ddata){
	////创建一个队列，用来给定时器发数据的。
	ddata->pwm_out_queue = queue_create(sizeof(unsigned int), sizeof(unsigned short)*8*2);//bit*2，高低电平
	if (!ddata->pwm_out_queue) {
		DBG_ERR("create pwm_out_queue failed\n");
		goto pwm_out_err;
	}
	ddata->ishort_queue = queue_create(sizeof(unsigned short), INPUT_SIZE);
	if (!ddata->ishort_queue) {
		DBG_ERR("create ishort_queue failed\n");
		goto ishort_err;
	}
	ddata->ochar_queue = queue_create(sizeof(unsigned char), OUTPUT_SIZE);
	if (!ddata->ochar_queue) {
		DBG_ERR("create ochar_queue failed\n");
		goto ochar_err;
	}
	DBG("create queue success\n");
	ddata->file_cnt = 0;
	ddata->pwm_flag = 1;
	ddata->pcount = 0;
	ddata->isempty_slp_oque = 0;
	ddata->htcnt = 1;
#if defined(HRTIMER_PWM_SEND)
	////初始化高精度定时器，用来发pwm
	hrtimer_init(&ddata->htimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        ddata->htimer.function = hrdown_callback;
        //hrtimer_start(&ddata->htimer, ktime_set(0, 7200*1000), HRTIMER_MODE_REL);
	hrtimer_start(&ddata->htimer, ktime_set(1, 200*1000*1000), HRTIMER_MODE_REL);
	DBG("init hrtimer success\n");
	
	/*创建一个工作队列，用于给定时器转发数据，从ishort_queue中把数据发给pwm_out_queue,让定时器把pwm发出去*/
	//INIT_WORK(&ddata->pwm_work, send_startpwm_work_func);
	tasklet_init(&ddata->loadpwm_tasklet, load_send_pwm_func, (unsigned long)ddata);
#endif
#if 0
	spin_lock_init(&ddata->file_lock);
	spin_lock_init(&ddata->send_lock);
	ddata->thread = kthread_run(pwm_send_thread_func, (void *)ddata, "pwm_thread");
	if (IS_ERR(ddata->thread)){
		DBG_ERR("Failed to create pwm_thread thread");
		goto thread_err;
	}
#endif	
	init_waitqueue_head(&ddata->wait);
	//init_completion(&ddata->comp);//初始化信号量
	
	return 0;
#if 0	
thread_err:
	#if defined(HRTIMER_PWM_SEND)
	hrtimer_cancel(&ddata->htimer);
	#endif
	queue_destroy(ddata->ochar_queue);
#endif
ochar_err:
	queue_destroy(ddata->ishort_queue);	
ishort_err:
	queue_destroy(ddata->pwm_out_queue);
pwm_out_err:
	fm_pltf_cdev_destroy(ddata);
	return -1;
}
static int rockchip_pwm_platfrom_hwinit(struct platform_device *pdev) {
	int ret;
	struct fm_platform *plat;
	struct rkxx_capturectl_drvdata *ddata = platform_get_drvdata(pdev);
	
	plat = &ddata->fm_pltf;
	ret = alloc_chrdev_region(&plat->dev_t, 0, 1, PWMC_NAME);
	if (ret) {
		DBG_ERR("alloc dev_t failed\n");
		goto chrdev_err;
	}
	
	DBG("alloc %s:%d:%d\n", PWMC_NAME, MAJOR(plat->dev_t), MINOR(plat->dev_t));
	
	cdev_init(&plat->cdev, &fm_ops);
	plat->cdev.owner = THIS_MODULE;
	plat->cdev.ops = &fm_ops;
	
	ret = cdev_add(&plat->cdev, plat->dev_t, 1);
	if (ret) {
		DBG_ERR("add dev_t failed\n");
		goto cdev_err;
	}
	
	plat->cls = class_create(THIS_MODULE, PWMC_NAME);
	if (IS_ERR(plat->cls)) {
		ret = PTR_ERR(plat->cls);
		DBG_ERR("class_create err:%d\n", ret);
		goto class_err;
	}

	plat->dev = device_create(plat->cls, NULL, plat->dev_t, NULL, PWMC_NAME);
	if (IS_ERR(plat->dev)) {
                ret = PTR_ERR(plat->dev);
		DBG_ERR("device_create err:%d\n", ret);
                goto device_err;
        }
	
	return 0;
   	
device_err:
	class_destroy(plat->cls);
class_err:
	cdev_del(&plat->cdev);
cdev_err:
	unregister_chrdev_region(plat->dev_t, 1);
chrdev_err:
	return ret;
}

static int rk_pwm_capture_probe(struct platform_device *pdev)
{
	struct rkxx_capturectl_drvdata *ddata;
	struct device_node *np = pdev->dev.of_node;
	struct resource *r;

	struct clk *clk;
	struct cpumask cpumask;
	int irq;
	int ret;
	int cpu_id;
	int pwm_id;
	int pwm_freq;
	enum of_gpio_flags g_flags;

	pr_err(".. rk pwm capturectl v1.1 init\n");
	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		dev_err(&pdev->dev, "no memory resources defined\n");
		return -ENODEV;
	}
	ddata = devm_kzalloc(&pdev->dev, sizeof(struct rkxx_capturectl_drvdata),
			     GFP_KERNEL);
	if (!ddata) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}
	
	ddata->temp_period = 0;
	ddata->base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(ddata->base))
		return PTR_ERR(ddata->base);
	clk = devm_clk_get(&pdev->dev, "pclk_pwm");
	if (IS_ERR(clk))
		return PTR_ERR(clk);
	platform_set_drvdata(pdev, ddata);
	
	
	wake_lock_init(&ddata->capturectl_wake_lock,
		       WAKE_LOCK_SUSPEND, "rk29_pwm_capture");
	ret = clk_prepare_enable(clk);
	if (ret)
		return ret;
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "cannot find IRQ\n");
		return irq;
	}
	ddata->irq = irq;
	ddata->wakeup = 1;
	of_property_read_u32(np, "capture_pwm_id", &pwm_id);
	ddata->capture_pwm_id = pwm_id;
	DBG("capturectl: capture pwm id=0x%x\n", pwm_id);
	of_property_read_u32(np, "handle_cpu_id", &cpu_id);
	ddata->handle_cpu_id = cpu_id;
	DBG("capturectl: handle cpu id=0x%x\n", cpu_id);
	
	tasklet_init(&ddata->capture_tasklet, rk_pwm_capturectl_do_something,
		     (unsigned long)ddata);

	device_init_wakeup(&pdev->dev, 1);
	enable_irq_wake(irq);
	cpumask_clear(&cpumask);
	cpumask_set_cpu(cpu_id, &cpumask);
	irq_set_affinity(irq, &cpumask);
	ret = devm_request_irq(&pdev->dev, irq, rockchip_pwm_irq,
			       IRQF_NO_SUSPEND, "rk_pwm_capture_irq", ddata);
	if (ret) {
		dev_err(&pdev->dev, "cannot claim IRQ %d\n", irq);
		return ret;
	}
	DBG("request_irq: irq=%d\n", irq);
	rk_pwm_capturectl_hw_init(ddata);
	pwm_freq = clk_get_rate(clk) / 64;
	ddata->pwm_freq_nstime = 1000000000 / pwm_freq;
	DBG("rk_pwm_probe success pwm_freq=%d,pwm_freq_nstime=%d\n", pwm_freq, ddata->pwm_freq_nstime);
	
	////////////////////////////////////////////////////////////////////////////////////
	
	ddata->pwm_gpio = of_get_named_gpio_flags(np, "pwmc-gpio", 0, &g_flags);
	if (!gpio_is_valid(ddata->pwm_gpio)) {
		DBG_ERR("rest: Can not read property: %s->gpios.\n", __func__);
	}

	ret = gpio_request(ddata->pwm_gpio, "NULL");
	if (ret) {
		DBG_ERR("request pwm_gpio fail:%d\n", ddata->pwm_gpio);
		return -1;
	}
	gpio_direction_output(ddata->pwm_gpio, 0);
	
	ret = rockchip_pwm_platfrom_hwinit(pdev);
	if (ret != 0) {
		return ret;
	}
	ret = rockchip_pwm_custom(ddata);//cfj
	if (ret != 0){
		fm_pltf_cdev_destroy(ddata);
		return ret;
	}
	pdata = ddata;
	printk("(%s: %d) success\n", __func__, __LINE__);
	return ret;
}

static int fm_pltf_cdev_destroy(struct rkxx_capturectl_drvdata *ddata)
{
	/*字符设备的释放和销毁*/
	device_destroy(ddata->fm_pltf.cls, ddata->fm_pltf.dev_t);
	class_destroy(ddata->fm_pltf.cls);
	cdev_del(&ddata->fm_pltf.cdev);
	unregister_chrdev_region(ddata->fm_pltf.dev_t, 1);
	//kfree(ddata->fm_pltf);
	return 0;
}

static int rk_pwm_capture_remove(struct platform_device *pdev)
{	
	struct rkxx_capturectl_drvdata *ddata = platform_get_drvdata(pdev);
	
	free_irq(ddata->irq, NULL);//释放中断
	hrtimer_cancel(&ddata->htimer);	//关闭高静定时器
	queue_destroy(ddata->pwm_out_queue);//释放队列
	fm_pltf_cdev_destroy(ddata);
//	kthread_stop(ddata->thread);
	kfree(ddata);
	return 0;
}

#ifdef CONFIG_PM
static int capturectl_suspend(struct device *dev)
{
	int cpu = 0;
	struct cpumask cpumask;
	struct platform_device *pdev = to_platform_device(dev);
	struct rkxx_capturectl_drvdata *ddata = platform_get_drvdata(pdev);
	
	cpumask_clear(&cpumask);
	cpumask_set_cpu(cpu, &cpumask);
	irq_set_affinity(ddata->irq, &cpumask);
	return 0;
}


static int capturectl_resume(struct device *dev)
{
	struct cpumask cpumask;
	struct platform_device *pdev = to_platform_device(dev);
	struct rkxx_capturectl_drvdata *ddata = platform_get_drvdata(pdev);

	cpumask_clear(&cpumask);
	cpumask_set_cpu(ddata->handle_cpu_id, &cpumask);
	irq_set_affinity(ddata->irq, &cpumask);
	return 0;
}

static const struct dev_pm_ops capturectl_pm_ops = {
	.suspend_late = capturectl_suspend,
	.resume_early = capturectl_resume,
};
#endif

static const struct of_device_id rk_pwm_capture_of_match[] = {
	{ .compatible =  "rockchip,capturectl-pwm"},
	{ }
};

MODULE_DEVICE_TABLE(of, rk_pwm_capture_of_match);

static struct platform_driver rk_pwm_capture_driver = {
	.driver = {
		.name = "capturectl-pwm",
		.of_match_table = rk_pwm_capture_of_match,
#ifdef CONFIG_PM
		.pm = &capturectl_pm_ops,
#endif
	},
	.probe = rk_pwm_capture_probe,
	.remove = rk_pwm_capture_remove,
};

module_platform_driver(rk_pwm_capture_driver);

MODULE_LICENSE("GPL");
