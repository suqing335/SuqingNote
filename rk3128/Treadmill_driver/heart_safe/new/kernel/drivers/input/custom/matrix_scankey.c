
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

#if 0
	matrix: keyscan {
		compatible = "matrix-keyscan";
		/**X1~X6*/
		key_det_io0 = <&gpio1 GPIO_B3 GPIO_ACTIVE_HIGH>;
		key_det_io1 = <&gpio1 GPIO_B0 GPIO_ACTIVE_HIGH>;
		key_det_io2 = <&gpio1 GPIO_B4 GPIO_ACTIVE_HIGH>;
		key_det_io3 = <&gpio0 GPIO_B6 GPIO_ACTIVE_HIGH>;
		key_det_io4 = <&gpio0 GPIO_B5 GPIO_ACTIVE_HIGH>;
		key_det_io5 = <&gpio0 GPIO_B3 GPIO_ACTIVE_HIGH>;
		/**Y1~Y6*/
		key_det_io6 = <&gpio0 GPIO_B4 GPIO_ACTIVE_HIGH>;
		key_det_io7 = <&gpio0 GPIO_B1 GPIO_ACTIVE_HIGH>;
		key_det_io8 = <&gpio0 GPIO_B0 GPIO_ACTIVE_HIGH>;
		key_det_io9 = <&gpio1 GPIO_A3 GPIO_ACTIVE_HIGH>;
		key_det_io10 = <&gpio0 GPIO_A2 GPIO_ACTIVE_HIGH>;
		key_det_io11 = <&gpio0 GPIO_D6 GPIO_ACTIVE_HIGH>;
		status = "okay";
	};
#endif	

/*定义6x4矩阵 6行4列*/
#define 	COLY		4
#define 	ROWX		6
//有按键按下后多少久去扫描一次按键(ms)
#define 	TIMER_DELAY 	40
/*每一个按键所代表的按键值在这个数据中设置*/
static const unsigned short matrix_keycode[(COLY)*(ROWX)] = {
	KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,
	KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
	KEY_K, KEY_L, KEY_M, KEY_N
};

struct key_unit {
	unsigned char  oldval;
	unsigned char  keyval;//按键按下
	unsigned short keycode; //按键对应的按键值
};
struct matrix_unit {
	unsigned char scanx[ROWX];//扫描gpio获取X向的值
	unsigned char scany[COLY];//扫描gpio获取Y向的值
	struct key_unit keyunit[COLY][ROWX];//每一个按键都有一个存储单元
};

struct misc_keyscan {
	struct matrix_unit 	matrix;
	struct input_dev 	*input;
	struct delayed_work 	keywork;
	unsigned int 		gpiox[ROWX];
	unsigned int 		gpioy[COLY];
	unsigned int 		io_irq;
	unsigned int 		irq;
	unsigned int 		keyirqstat;
	int 			keynum;//记录多少个按键按下
};
static struct misc_keyscan *pmatrix = NULL;
static int matrix_key_report(struct input_dev *input, unsigned short keycode, unsigned char keyval) {	
	input_report_key(input, (unsigned int)keycode, (int)keyval);
	input_sync(input);
	//printk("report keycode = %d, keyval = %d\n", keycode, keyval);
	return 0;
}

static int matrix_check_key(struct misc_keyscan *keymatrix) {
	int col, row;
	int num = 0;
	unsigned char value;
	struct matrix_unit *matrix = &keymatrix->matrix;

	/*******************************************************/
	for (col = 0; col < COLY; col++) {
		for (row = 0; row < ROWX; row++) {
			/**与上一次状态不一样就上报按键**/
			value = matrix->keyunit[col][row].keyval;
			
			if (matrix->keyunit[col][row].oldval != value) {
				matrix_key_report(keymatrix->input, matrix->keyunit[col][row].keycode, value);
				matrix->keyunit[col][row].oldval = value;//替换旧值
			}
			
			if (value) {
				printk("--key : %d---\n", matrix->keyunit[col][row].keycode);
				++num;
			}
			
		}
	}
	keymatrix->keynum = num;
	printk("--report key end---\n");
	return 0;

}

static void matrix_custom_keyscan(struct work_struct *work) {
	int i, j, k, tag;
	unsigned int sumx;
	unsigned int sumy;
	unsigned int value;
	int col_index[COLY];
	int row_index[ROWX];
	struct delayed_work *dwork = container_of(work, struct delayed_work, work);
	struct misc_keyscan *keymatrix = container_of(dwork, struct misc_keyscan, keywork);
	struct matrix_unit *matrix = &keymatrix->matrix;
	tag = 0;
keyloop:
	sumx = 0;
	sumy = 0;
	k = 0;
	/**X输出0,Y输入，扫描Y值**/
	for (i = 0; i < COLY; i++) {
		/**获取到的值取非，以后更好判断按键值*/
		value = !gpio_get_value(keymatrix->gpioy[i]);
		if (value) {
			sumy += 1;
			col_index[k++] = i;
		}
		matrix->scany[i] = value;
	}
#if 0	
	if (sumy && tag) {
		msleep(5);
		/**X输出0,Y输入，扫描Y值**/
		for (i = 0; i < COLY; i++) {
			/**获取到的值取非，以后更好判断按键值*/
			value = !gpio_get_value(keymatrix->gpioy[i]);
			if (value) {
				sumy += 1;
				col_index[k++] = i;
			}
			matrix->scany[i] = value;
		}
	}
#endif
	if (sumy) {
		/**X输入，Y输出0*/
		for (i = 0; i < COLY; i++) {
			gpio_direction_output(keymatrix->gpioy[i], 0);
		}
		for (i = 0; i < ROWX; i++) {
			gpio_direction_input(keymatrix->gpiox[i]);
		}
		msleep(10);
		k = 0;
		/**Y输出0,X输入，扫描X值**/
		for (i = 0; i < ROWX; i++) {
			value = !gpio_get_value(keymatrix->gpiox[i]);
			if (value) {
				sumx += 1;
				row_index[k++] = i;
			}
			matrix->scanx[i] = value;
		}
	} else {/**检测到Y没有按下时，X值为0*/
		for (i = 0; i < ROWX; i++) {
			matrix->scanx[i] = 0;
		}
		sumx = 0;
	}
	/**分析按键值**/
	for (i = 0; i < COLY; i++) {
		for (j = 0; j < ROWX; j++) {
			matrix->keyunit[i][j].keyval = matrix->scany[i] && matrix->scanx[j]; //
		}	
	}
	/**判断按键是否有效*/
	if ((sumx == 1) || (sumy == 1)) {
		matrix_check_key(keymatrix);
		//keymatrix->keynum = 1;
	} else if (!sumy) {
		matrix_check_key(keymatrix);
		//keymatrix->keynum = 0;
	} else if ((sumx == 2) && (sumy == 2)) {
		gpio_direction_output(keymatrix->gpioy[col_index[0]], 1);
		msleep(10);
		/**再一次扫描对应的两个gpio X值
		 * 如果第一个值就是被拉低，说明(row_index[0],col_index[1])点才是真实的
		 * 推出 (row_index[1], col_index[0]),也是真实被按下的。相反就是
		 * (row_index[1], col_index[1]),(row_index[0], col_index[0])
		 */
		//value = gpio_get_value(keymatrix->gpiox[row_index[0]]);
		if (!gpio_get_value(keymatrix->gpiox[row_index[0]])) {
			/**把虚拟出来的按键，从隐射表中去除*/
			matrix->keyunit[col_index[0]][row_index[0]].keyval = 0;
			matrix->keyunit[col_index[1]][row_index[1]].keyval = 0;
		} else /*if(!gpio_get_value(keymatrix->gpiox[row_index[1]]))*/ {
			matrix->keyunit[col_index[1]][row_index[0]].keyval = 0;
			matrix->keyunit[col_index[0]][row_index[1]].keyval = 0;
		}
		matrix_check_key(pmatrix);
	} else {
		keymatrix->keynum = 0;
	}
	
	if (sumy) {
		/**Y输入，X输出0*/
		for (i = 0; i < COLY; i++) {
			gpio_direction_input(keymatrix->gpioy[i]);
		}
		for (i = 0; i < ROWX; i++) {
			gpio_direction_output(keymatrix->gpiox[i], 0);
		}
	}
	printk("keynum = %d\n", keymatrix->keynum);
	if (keymatrix->keynum) {
		msleep(30);//20ms 后再扫一次
		tag = 1;
		goto keyloop;
	} else {
		msleep(10);
		enable_irq(keymatrix->irq);
		keymatrix->keyirqstat = 0;
	}
}

static irqreturn_t keyscanf_intterupt(int irq, void *dev_id) {
	struct misc_keyscan *keymatrix = (struct misc_keyscan *)dev_id;
	//printk("keyscanf_intterupt start\n");
	if (!keymatrix->keyirqstat) {
		//keymatrix->keyirqstat = 1;
		disable_irq_nosync(keymatrix->irq);
		keymatrix->keyirqstat = 1;
	} else {
		return IRQ_HANDLED;
	}
	/**调度工作队列去扫描按键**/
	schedule_delayed_work(&keymatrix->keywork, msecs_to_jiffies(20));
	return IRQ_HANDLED;
}

static int matrix_irq_request(struct misc_keyscan *keymatrix) {
	int ret;
	keymatrix->irq = gpio_to_irq(keymatrix->io_irq);
	if (keymatrix->irq < 0) {
		ret = keymatrix->irq;
		printk("irq for gpio %d error(%d)\n", keymatrix->io_irq, ret);
		gpio_free(keymatrix->io_irq);
		return ret;
	} else {
		//ret = request_irq(keymatrix->irq, keyscanf_intterupt, IRQF_TRIGGER_FALLING|IRQF_ONESHOT, "keyscanf", keymatrix);
		ret = request_irq(keymatrix->irq, keyscanf_intterupt, IRQF_TRIGGER_LOW|IRQF_ONESHOT, "keyscanf", keymatrix);
		if (ret < 0) {
			printk( "gpio_irq: request irq failed\n");
			ret = -ENODEV;
			free_irq(keymatrix->irq, keymatrix);
			gpio_free(keymatrix->io_irq);
			return ret;
		}
		disable_irq_nosync(keymatrix->irq);
		printk( "request irq success\n");
	}

	return ret;
}
static int matrix_input_device(struct misc_keyscan *misc, char *name) {
	int error, i, j;
        struct input_dev *input = input_allocate_device();   //分配一个输入设备   
        if (!input) {    
              
                 printk(KERN_ERR" Not enough memory\n");    
                 error = -ENOMEM;    
                 goto err_free_dev;    
        }    
  
        input->name = name;
        __set_bit(EV_KEY, input->evbit);//注册设备支持的事件类型  
        //__set_bit(KEY_RIGHT, input->keybit); //注册设备支持的按键  
        //__set_bit(KEY_LEFT, input->keybit);
	for (i = 0; i < ROWX; i++) {
		for (j = 0; j < ROWX; j++) {
			__set_bit(matrix_keycode[i*(ROWX)+j], input->keybit);
			misc->matrix.keyunit[i][j].keycode = matrix_keycode[i*(ROWX)+j];
			misc->matrix.keyunit[i][j].oldval = 0;
		}
	}
	
        error = input_register_device(input);  //注册输入设备  
        if (error) {
                 printk(KERN_ERR" Failed to register device\n");    
                 goto err_free_dev; 
        }
	misc->input = input;

        return error;    
    
err_free_dev:    
        input_free_device(input);
        return error;
}
static int matrix_parse_dt(struct device_node *np, struct misc_keyscan *keymatrix) {
	int ret, i;
	char buf[16];
	enum of_gpio_flags gpio_flags;
	/**key**/
	for (i = 0; i < ROWX; i++) {
		sprintf(buf, "key_det_io%d", i);
		keymatrix->gpiox[i] = of_get_named_gpio_flags(np, buf, 0, &gpio_flags);
		ret = gpio_request(keymatrix->gpiox[i], buf);
		if (ret < 0) {
			printk("%s request gpio %d failed\n", buf, i);
			return -ENODEV;
		}
		//gpio_direction_input(keymatrix->gpiox[i]);
		gpio_direction_output(keymatrix->gpiox[i], 0);
	}
	
	for (i = ROWX; i < (COLY+ROWX); i++) {
		sprintf(buf, "key_det_io%d", i);
		keymatrix->gpioy[i-ROWX] = of_get_named_gpio_flags(np, buf, 0, &gpio_flags);
		ret = gpio_request(keymatrix->gpioy[i-ROWX], buf);
		if (ret < 0) {
			printk("%s request gpio %d failed\n", buf, i);
			return -ENODEV;
		}
		//gpio_direction_output(keymatrix->gpioy[i], 0);
		gpio_direction_input(keymatrix->gpioy[i-ROWX]);
	}
	
	/**获取irq gpio */
	keymatrix->io_irq = of_get_named_gpio_flags(np, "key_det_io11", 0, &gpio_flags);
	ret = gpio_request(keymatrix->io_irq, "key_det_io11");
	if (ret < 0) {
		printk("request gpio failed\n");
		return -ENODEV;
	}
	gpio_direction_input(keymatrix->io_irq);
	
	printk("matrix parse dt success\n");
	return 0;
}

static int matrix_custom_probe(struct platform_device *pdev) {
	int ret;
	struct misc_keyscan *keymatrix = NULL;
	struct device_node *np = pdev->dev.of_node;
	
	keymatrix = kzalloc(sizeof(struct misc_keyscan), GFP_KERNEL);
	if (!keymatrix) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	/*初始化keymatrix*/
	memset(keymatrix, 0, sizeof(struct misc_keyscan));
	
	ret = matrix_parse_dt(np, keymatrix);//dts
	if (ret < 0) {
		printk("failed to find platform dts\n");
		goto err_parse;
	}
	
	ret = matrix_input_device(keymatrix, "matrix-keyscan");
	if (ret < 0) {
		goto err_alloc_input;
	}
		
	ret = matrix_irq_request(keymatrix);
	if (ret < 0) {
		printk("matrix irq request failed\n");
		goto err_irq;
	}
	//初始化工作队列
	INIT_DELAYED_WORK(&keymatrix->keywork, matrix_custom_keyscan);
	platform_set_drvdata(pdev, keymatrix);
	pmatrix = keymatrix;
	enable_irq(keymatrix->irq);
	printk("matrix_custom_probe success\n");
	return ret;
err_irq:
	input_unregister_device(keymatrix->input);
	input_free_device(keymatrix->input);
err_alloc_input:	
err_parse:
	kfree(keymatrix);
err_alloc:
	return ret;
}
static int matrix_custom_remove(struct platform_device *pdev) {
	int i;
	struct misc_keyscan *keymatrix = platform_get_drvdata(pdev);
	for (i = 0; i < (ROWX); i++) {
		gpio_free(keymatrix->gpiox[i]);
	}
	for (i = 0; i < COLY; i++) {
		gpio_free(keymatrix->gpioy[i]);
	}
	free_irq(keymatrix->irq, keymatrix);
	flush_delayed_work(&keymatrix->keywork);
	cancel_delayed_work_sync(&keymatrix->keywork);
	//cancel_work_sync(&keymatrix->keywork);
	input_unregister_device(keymatrix->input);
//	del_timer(&keymatrix->keytimer);
	kfree(keymatrix);
	return 0;
}



static struct of_device_id matrix_custom_of_match[] = {
	{ .compatible = "matrix-keyscan"},
	{ }
};
MODULE_DEVICE_TABLE(of, matrix_custom_of_match);

static struct platform_driver matrix_custom_driver = {
	.driver		= {
		.name		= "matrix-keyscan",
		.owner		= THIS_MODULE,
		.pm		= NULL,
		.of_match_table	= of_match_ptr(matrix_custom_of_match),
	},
	.probe		= matrix_custom_probe,
	.remove		= matrix_custom_remove,
};

module_platform_driver(matrix_custom_driver);

MODULE_DESCRIPTION("matrix keyscan Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:matrix keyscan");