#ifndef __DIRECT_CURRENT_
#define __DIRECT_CURRENT_

#include <hardware/hardware.h>

#define 	DIRECT_API_VERSION 		HARDWARE_MODULE_API_VERSION(10, 0)
#define 	DIRECT_CONTROLLER 		"direct_"
#define 	DIRECT_HARDWARE_MODULE_ID  	"direct"


struct direct_device;
typedef struct direct_device {
	
	struct hw_device_t common;
	
	int devfd;
	
	int (*dev_open)(struct direct_device* dev);

	int (*dev_close)(struct direct_device* dev);

	int (*dev_read)(struct direct_device* dev, char *buffer, int length);

	int (*dev_write)(struct direct_device* dev, const char *buffer, int length);
	int (*dev_setspeed)(struct direct_device* dev, int speed);
	int (*dev_getspeed)(struct direct_device* dev);
	
	int (*dev_setbuzzer)(struct direct_device* dev, int enable);
	int (*dev_getbuzzer)(struct direct_device* dev);
       
} direct_device_t;

#endif/*__DIRECT_CURRENT_*/