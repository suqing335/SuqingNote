#蜂鸣器开关框架
借用rockchip_custom.c中现成的杂设备框架，然后在/sys/misc/下生成设备节点。

	ret = device_create_file(misc->miscdev.this_device, &dev_attr_buzz);
	if(ret) {
		printk("device_create_file 1 err:%d\n", ret);
		goto dev_file_err1;
	}

#hardware层
封装了2个接口用于控制蜂鸣器打开和关闭，及获取状态

	int (*dev_setbuzzer)(struct direct_device* dev, int enable);
	int (*dev_getbuzzer)(struct direct_device* dev);

#framework jni
封装了2个接口用于控制蜂鸣器打开和关闭，及获取状态

	static jboolean getDeviceBuzzerStatus(JNIEnv *env, jobject clazz) {
		int ret;
		ret = device->dev_getbuzzer(device);
		return ret ? true : false;
	}
	static jint setDeviceBuzzerStatus(JNIEnv *env, jobject clazz, jboolean enable) {
		return device->dev_setbuzzer(device, enable ? 1 : 0);
	}
#framework java
封装了2个接口用于控制蜂鸣器打开和关闭，及获取状态
	public abstract int setBuzzerStatus(boolean enable);
    public abstract boolean getBuzzerStatus();

获取到那个服务就能使用这些接口