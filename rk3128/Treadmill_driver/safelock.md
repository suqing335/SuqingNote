#安全开关框架

主要设计思路就是把开关的状态通过输入子系统上报上去，上层通过Sensor服务监听就可以了，不需要一直去读底层的状态。省系统资源。

##kernel层
rockchip_custom.c, 安全开关和pwm风扇的控制在一个文件里面
创建输入设备

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

注册杂设备

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

上报的方式有点特别，因为一直报同一个值，输入子系统不会上报，所以得上报不同的数据，> 10为安全开关脱落， < 10为安全开关合上

在中断函数中调度延时工作队列去处理，并上报安全开关状态

##hardware层

和其他的输入子系统一样的接口

##framework层

在Sensor.java中添加相应的 String 
