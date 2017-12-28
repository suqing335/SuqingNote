#ifndef __VPROCTW_HAL_
#define __VPROCTW_HAL_

#include <sys/ioctl.h>
#include <linux/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "zl380tw.h"
//#include <linux/spi/zl380tw.h>

int gTwolf_fd;
static int VprocHALInit(void);
static void VprocHALcleanup(void);
static int VprocHALread(unsigned char *pData, unsigned short numBytes);
static int VprocHALwrite(unsigned char *pData, unsigned short numBytes);
static int ioctlHALfunctions (unsigned int cmd, void* arg); 

#endif