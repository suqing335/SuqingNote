#心率驱动
##整体架构--（获取心率值，上报心率值）
###获取心率值



###上报心率值
是通过输入子系统上报的，采取温度sensor的架构上传 
hardware层st文件夹内一个转换的函数需要修改
####hardware
    void TemperatureSensor::processEvent(int code, int value)
	{
	    if (code == EVENT_TYPE_TEMPERATURE) {
	            mPendingEvent.pressure = value * CONVERT_B ;
				//LOGD("%s:value=%d\n",__FUNCTION__, value);
	    }
	}

在nusensors.h 中

	//#define CONVERT_B                   (1.0f/100.0f)
	#define CONVERT_B                   (1.0f)


####kernel
1.注册输入子系统，并将名字设为"temperature",input->name = "temperature".

	input->name ="temperature";/**这个名字必须和hardware temperature层对应，不然找不到数据*/
	set_bit(EV_ABS, input->evbit);		
	input_set_abs_params(input, ABS_THROTTLE, 100, 65535, 0, 0);/**上报温度*/

2.把设备注册到杂设备类中.

	static int heart_misc_device_register(struct misc_heart *heart) {
		int result;
		heart->fops.owner = THIS_MODULE;
		heart->fops.unlocked_ioctl = heart_dev_ioctl;
		heart->fops.open = heart_dev_open;
		heart->fops.release = heart_dev_release;
	
		heart->miscdev.minor = MISC_DYNAMIC_MINOR;
		heart->miscdev.name = "temperature";
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

miscdev.name = "temperature";这个就是设备文件的名字，/dev/temperature;这个名字也不能改，在hardware层要找这个名字打开设备。

hardware层ioctl的控制模式

	TEMPERATURE_IOCTL_GET_ENABLED
	TEMPERATURE_IOCTL_ENABLE
ioctl原型
	
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
						enable_irq(heart->ht_irq);
						enable_irq(heart->ht_w_irq);
						heart->status_cur = SENSOR_ON;
					}	
				} else {
					if(heart->status_cur == SENSOR_ON) {
						/*关闭中断后程序返回, 如果在中断处理程序中, 那么会继续将中断处理程序执行完*/
						disable_irq_nosync(heart->ht_irq);
						disable_irq_nosync(heart->ht_w_irq);
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
这样就完全支持hardware层的控制流程。
