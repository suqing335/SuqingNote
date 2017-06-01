#风扇的控制
用pwm无极调速，控制风扇的转速

##dts是一大难点，曾经困扰很久

	misc: safelock_fan {
		pwms = <&pwm1 0 25000>;
		pwm-names = "fancontrol";
        compatible = "misc-safelock_fan";
		safe-io = <&gpio1 GPIO_B6 GPIO_ACTIVE_LOW>;
		fan-en = <&gpio1 GPIO_C5 GPIO_ACTIVE_HIGH>;
		buzz-io = <&gpio3 GPIO_B3 GPIO_ACTIVE_LOW>;
		status = "okay";
	};

pwms = <&pwm1 0 25000> 第一个值是对应的pwm1,第二个参数是通道，第三个是周期

在dts中必须把pwm1打开

	&pwm1 {
	        status = "okay";
	};

原来看背光的写法没有打开pwm0，以为不用打开，其实背光的pwm0默认是在rk312x-sdk.dtsi中打开的。所以用了pwm1就需要把这句打开pwm1

##driver

rk的默认框架是只有一个背光用pwm的 所以在pwm-rockchip.c  \#define NUM_PWM		                  1

需要把 \#define NUM_PWM 定义成2以上

获取pwm

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
这里增加了一个pwm，所以背光处的获取pwm devm_pwm_get也要加上name 参数不能为NULL

封了一些接口控制pwm, /sys/下面的节点控制。

#hardware
封装了2个接口用于控制和获取pwm-fan速度

	int (*dev_setspeed)(struct direct_device* dev, int speed);
	int (*dev_getspeed)(struct direct_device* dev);


#framework jni
封装了2个接口用于控制和获取pwm-fan速度

	static jint setDeviceFanSpeedValue(JNIEnv *env, jobject clazz, jint speed) {
		if (!device)
			return -1;
		return device->dev_setspeed(device, speed);
	}
	static jint getDeviceFanSpeedValue(JNIEnv *env, jobject clazz) {
		if (!device)
			return -1;
		return device->dev_getspeed(device);
	}

#framework java
封装了2个接口用于控制和获取pwm-fan速度

	public abstract int setFanSpeedValue(int speed);
    public abstract int getFanSpeedValue();


获取到那个服务就能使用这些接口



