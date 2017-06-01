
#include <hardware/direct_current.h>
#include <cutils/log.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>



static const char THE_DEVICE[] = "/dev/pwm_capmode";
static const char *FAN_CTL_CALSS_PATH = "/sys/class/misc/safelock/pwmvalue";
static const char *BUZZ_CTL_CALSS_PATH = "/sys/class/misc/safelock/buzz";

static int device_exists() {
    int fd;

    fd = open(THE_DEVICE, O_RDWR);
    if(fd < 0) {
        ALOGE("Direct file does not exist : %d", fd);
        return 0;
    }
    close(fd);
    return 1;
}

static int device_open(struct direct_device* dev) {
	int fd = open(THE_DEVICE, O_RDWR);
	if (fd < 0) {
		ALOGE("open %s failed\n", THE_DEVICE);
		return fd;
	}
	dev->devfd = fd;
	return 0;
}
static int device_close(struct direct_device* dev) {	
	if (dev->devfd > 0) {
		close(dev->devfd);
		dev->devfd = -1;
	}
	return 0;
}
static int direct_close(hw_device_t *dev) {
	struct direct_device* direct = (struct direct_device*)dev;
	if (direct) {
		device_close(direct);
		free(direct);
	}
	return 0;
}
/**
 * device_read
 * @return : On success, the number of bytes read is returned;
 * On error, -1 is returned
 */
static int device_read(struct direct_device* dev, char *buffer, int length) {
	return read(dev->devfd, buffer, length);
}
/**
 * device_write
 * @return : On success, the number of bytes written is returned;
 * On error, -1 is returned
 */
static int device_write(struct direct_device* dev, const char *buffer, int length) {
	return write(dev->devfd, buffer, length);
}
/**
 * set_fan_speed
 * @return : On success, return 0; On error, return -1
 */
static int set_fan_speed(struct direct_device* dev, const int speed) {
	int ret, wret;
	char buffer[16];
	int fd = open(FAN_CTL_CALSS_PATH, O_RDWR);
	if (fd < 0) {
		ALOGE("open %s failed\n", FAN_CTL_CALSS_PATH);
		return -1;
	}
	ret = sprintf(buffer, "%d\n", speed);
	wret = write(fd, buffer, ret);
	if (wret != ret) {
		ALOGE("write %s data failed\n", FAN_CTL_CALSS_PATH);
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}
/**
 * get_fan_speed
 * @return : On success, return read value; On error, return -1  
 */
static int get_fan_speed(struct direct_device* dev) {
	int ret;
	char buffer[16];
	int fd = open(FAN_CTL_CALSS_PATH, O_RDWR);
	if (fd < 0) {
		ALOGE("open %s failed\n", FAN_CTL_CALSS_PATH);
		return -1;
	}
	ret = read(fd, buffer, 6);
	if (ret > 0) {
		close(fd);
		return atoi(buffer);
	}
	close(fd);
	return -1;
}

/**
 * set_buzzer_status
 * @return : On success, return 0; On error, return -1
 */
static int set_buzzer_status(struct direct_device* dev, const int enable) {
	int ret;
	char temp;
	int fd = open(BUZZ_CTL_CALSS_PATH, O_RDWR);
	if (fd < 0) {
		ALOGE("open %s failed\n", BUZZ_CTL_CALSS_PATH);
		return -1;
	}
	temp = enable ? '1' : '0'; 
	ret = write(fd, &temp, 1);
	if (ret < 0) {
		ALOGE("write %s data failed\n", BUZZ_CTL_CALSS_PATH);
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}
/**
 * get_buzzer_status
 * @return : On success, return read value; On error, return -1  
 */
static int get_buzzer_status(struct direct_device* dev) {
	int ret;
	char temp;
	int fd = open(BUZZ_CTL_CALSS_PATH, O_RDWR);
	if (fd < 0) {
		ALOGE("open %s failed\n", BUZZ_CTL_CALSS_PATH);
		return -1;
	}
	ret = read(fd, temp, 1);
	if (ret < 0) {
		ALOGE("read %s data failed\n", BUZZ_CTL_CALSS_PATH);
		close(fd);
		return -1;
	}
	close(fd);
	return temp == '1' ? 1: 0;
}

static int open_direct(const struct hw_module_t* module, const char* name, struct hw_device_t** device) {
	//if (!strcmp(name, DIRECT_CONTROLLER)) {
		if (!device_exists()) {
			ALOGE("Direct device does not exist. Cannot start direct");
			return -ENODEV;
		}
		direct_device_t *direct = calloc(1, sizeof(direct_device_t));
		if (!direct) {
			ALOGE("Can not allocate memory for the direct device");
			return -ENOMEM;
		}
		direct->common.tag = HARDWARE_DEVICE_TAG;
		direct->common.module = (hw_module_t *) module;
		direct->common.version = DIRECT_API_VERSION;
		direct->common.close = direct_close;
		direct->dev_open = device_open;
		direct->dev_close = device_close;
		direct->dev_read = device_read;
		direct->dev_write = device_write;
		direct->dev_setspeed = set_fan_speed;
		direct->dev_getspeed = get_fan_speed;
		direct->dev_setbuzzer = set_buzzer_status;
		direct->dev_getbuzzer = get_buzzer_status;
		direct->devfd = -1;
		*device = (hw_device_t*) direct;

	//} else {
	//	return -EINVAL;
	//}
	return 0;
}






static struct hw_module_methods_t direct_module_methods = {
    .open = open_direct
};

struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .module_api_version = DIRECT_API_VERSION,
    .hal_api_version = HARDWARE_HAL_API_VERSION,
    .id = DIRECT_HARDWARE_MODULE_ID,
    .name = "Default direct HAL",
    .author = "The Android Open Source Project",
    .methods = &direct_module_methods,
};