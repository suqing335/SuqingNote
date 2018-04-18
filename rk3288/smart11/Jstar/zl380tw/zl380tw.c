/*
 * zl380tw.c  --  zl380tw ALSA Soc Audio driver
 *
 * Copyright 2014 Microsemi Inc.
 *
 * This program is free software you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option)any later version.
 */




#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/firmware.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include "zl380tw.h"

//#define ZL380XX_TW_ENABLE_CHAR_DEV_DRIVER   /*Define this macro to create a character device driver*/

//#define MICROSEMI_HBI_I2C          /*Enable this macro if the HBI interface between the host CPU and the Twolf is I2C*/
//#ifdef MICROSEMI_HBI_I2C          
  //  #undef MICROSEMI_HBI_SPI      
    //#define MICROSEMI_I2C_ADDR 0x52  /*if DIN pin is tied to ground, else if DIN is tied to 3.3V address must be 0x52*/
    //#define CONTROLLER_I2C_BUS_NUM 0
//#else
  //  #define MICROSEMI_HBI_SPI
    /*Define the SPI master signal config*/
    //#define SPIM_CLK_SPEED  1000000
    //#define SPIM_CHIP_SELECT 1
    //#define SPIM_MODE SPI_MODE_3
    //#define SPIM_BUS_NUM 0   
//#endif


//#undef ZL380XX_TW_UPDATE_FIRMWARE /*define if you want to update current firmware with a new one at power up*/
//#ifdef ZL380XX_TW_UPDATE_FIRMWARE
/*NOTE: Rename the *s3 firmware file as per below or simply change the file name below as per the firmware file name*/
  //  #define  ZLS380_TWOLF "MICROSEMI_firmware.s3" /*compatible firmware image for your zl380xx device*/
//#endif

/*HBI access to the T-wolf must not be interrupted by another process*/
//#define PROTECT_CRITICAL_SECTION  /*define this macro to protect HBI critical section*/

/*Define the ZL380TW interrupt pin drive mode 1:TTL, 0: Open Drain(default)*/
//#define HBI_CONFIG_INT_PIN_DRIVE_MODE		0

#ifdef MICROSEMI_HBI_SPI
#include <linux/spi/spi.h>
#endif

#ifdef MICROSEMI_HBI_I2C
#include <linux/i2c.h>
#endif

#ifdef ZL380XX_TW_ENABLE_ALSA_CODEC_DRIVER
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#endif

#ifdef ZL380XX_TW_ENABLE_CHAR_DEV_DRIVER
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#endif

/*define this macro for critical region protection*/
#ifdef PROTECT_CRITICAL_SECTION
#include <linux/mutex.h>
#endif


#undef ZL38040_SAVE_FWR_TO_FLASH  /*undefine if no slave flash is connected to the zl380tw*/

#define MSCCDEBUG
#undef MSCCDEBUG2

#ifdef MSCCDEBUG
#define TW_DEBUG1(s, args...) \
	pr_err("%s %d: "s, __func__, __LINE__, ##args);

#else
#define TW_DEBUG1(s, args...)
#endif

#ifdef MSCCDEBUG2
#define TW_DEBUG2(s, args...) \
	pr_err("%s %d: "s, __func__, __LINE__, ##args);

#else
#define TW_DEBUG2(s, args...)
#endif


/*TWOLF HBI ACCESS MACROS-------------------------------*/
#define HBI_PAGED_READ(offset,length) \
    ((u16)(((u16)(offset) << 8) | (length)))
#define HBI_DIRECT_READ(offset,length) \
    ((u16)(0x8000 | ((u16)(offset) << 8) | (length)))
#define HBI_PAGED_WRITE(offset,length) \
    ((u16)(HBI_PAGED_READ(offset,length) | 0x0080))
#define HBI_DIRECT_WRITE(offset,length) \
    ((u16)(HBI_DIRECT_READ(offset,length) | 0x0080))
#define HBI_GLOBAL_DIRECT_WRITE(offset,length) \
    ((u16)(0xFC00 | ((offset) << 4) | (length)))
#define HBI_CONFIGURE(pinConfig) \
    ((u16)(0xFD00 | (pinConfig)))
#define HBI_SELECT_PAGE(page) \
    ((u16)(0xFE00 | (page)))
#define HBI_NO_OP \
    ((u16)0xFFFF)

/*HBI access type*/
#define TWOLF_HBI_READ 0
#define TWOLF_HBI_WRITE 1
/*HBI address type*/
#define TWOLF_HBI_DIRECT 2
#define TWOLF_HBI_PAGED  3

/* driver private data */
struct zl380tw {
#ifdef MICROSEMI_HBI_SPI
	struct spi_device	*spi;
#endif
#ifdef MICROSEMI_HBI_I2C
	struct i2c_client   *i2c;
#endif
	u8  *pData;
	int sysclk_rate;
#ifdef PROTECT_CRITICAL_SECTION
	struct list_head	device_entry;
#endif	
} *zl380tw_priv;

#ifdef ZL380XX_TW_ENABLE_CHAR_DEV_DRIVER
dev_t t_dev;

/* For registration of character device */
static struct cdev c_dev;
static int module_usage_count;
static ioctl_zl380tw zl380tw_ioctl_buf;
#endif

static unsigned twHBImaxTransferSize =(MAX_TWOLF_ACCESS_SIZE_IN_BYTES + 34);
module_param(twHBImaxTransferSize, uint, S_IRUGO);
MODULE_PARM_DESC(twHBImaxTransferSize, "total number of data bytes >= 256");


/* if mutual exclusion is required for your particular platform
 * then add mutex lock/unlock to this driver
 */
#ifdef PROTECT_CRITICAL_SECTION
static DEFINE_MUTEX(lock);
static DEFINE_MUTEX(zl380tw_list_lock);
static LIST_HEAD(zl380tw_list);
#endif

/* if mutual exclusion is required for your particular platform
 * then add mutex lock/unlock to this driver
 */

static void zl380twEnterCritical(void)
{
#ifdef PROTECT_CRITICAL_SECTION
	mutex_lock(&lock);
#endif
}

static void zl380twExitCritical(void)
{
#ifdef PROTECT_CRITICAL_SECTION
	mutex_unlock(&lock);
#endif
}
static int zl380tw_i2c_master_send(const struct i2c_client *client, const char *buf, int count)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.len = count;
	msg.buf = (char *)buf;
	msg.scl_rate = 300 * 1000;

	ret = i2c_transfer(adap, &msg, 1);

	/*
	 * If everything went ok (i.e. 1 msg transmitted), return #bytes
	 * transmitted, else error code.
	 */
	return (ret == 1) ? count : ret;
}

/*  write up to 252 bytes data */
/*  zl380tw_nbytes_wr()- rewrite one or up to 252 bytes to the device
 *  \param[in]     ptwdata     pointer to the data to write
 *  \param[in]     numbytes    the number of bytes to write
 *
  *  return ::status = the total number of bytes transferred successfully
 *                    or a negative error code
 */

static int zl380tw_nbytes_wr(struct zl380tw *zl380tw, int numbytes, u8 *pData) {

	int status;
#ifdef MICROSEMI_HBI_SPI
	struct spi_message msg;
	struct spi_transfer xfer = {
		.len = numbytes,
		.tx_buf = pData,
	};

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	status = spi_sync(zl380tw->spi, &msg);
#endif
#ifdef MICROSEMI_HBI_I2C
	//status = i2c_master_send(zl380tw->i2c, pData, numbytes);
	status = zl380tw_i2c_master_send(zl380tw->i2c, pData, numbytes);   
#endif 
	if (status < 0) 
		return -EFAULT;
	return 0;   
}

/* zl38040_hbi_rd16()- Decode the 16-bit T-WOLF Regs Host address into
 * page, offset and build the 16-bit command acordingly to the access type.
 * then read the 16-bit word data and store it in pdata
 *  \param[in]
 *                .addr     the 16-bit HBI address
 *                .pdata    pointer to the data buffer read or write
 *
 *  return ::status
 */

static int zl380tw_hbi_rd16(struct zl380tw *zl380tw, u16 addr, u16 *pdata) {
	u16 cmd;
	u8 page;
	u8 offset;
	u8 i = 0;
	u8 buf[4] = {0, 0, 0, 0};
	int status =0;
#ifdef MICROSEMI_HBI_I2C
	struct  i2c_msg     msg[2];
#endif   
	page = addr >> 8;
	offset = (addr & 0xFF)>>1;

	if (page == 0) /*Direct page access*/
	{
		cmd = HBI_DIRECT_READ(offset, 0);/*build the cmd*/
		buf[i++] = (cmd >> 8) & 0xFF;
		buf[i++] = (cmd & 0xFF);
	    
	} else { /*indirect page access*/
		if (page != 0xFF) {
			page  -=  1;
		}
		cmd = HBI_SELECT_PAGE(page);
		i = 0;
		/*select the page*/
		buf[i++] = (cmd >> 8) & 0xFF;
		buf[i++] = (cmd & 0xFF);
		cmd = HBI_PAGED_READ(offset, 0); /*build the cmd*/
		buf[i++] = (cmd >> 8) & 0xFF;
		buf[i++] = (cmd & 0xFF);
	}	
	/*perform the HBI access*/
           
#ifdef MICROSEMI_HBI_SPI
	status = spi_write_then_read(zl380tw->spi, buf, i, buf, 2);
#endif
#ifdef MICROSEMI_HBI_I2C
	memset(msg, 0, sizeof(msg));
	/* Make write msg 1st with write command */
	msg[0].addr = zl380tw->i2c->addr;
	msg[0].len  = i;
	msg[0].buf  = buf;
	msg[0].scl_rate = 300 * 1000;
	//    TW_DEBUG2("i2c addr = 0x%04x\n", msg[0].addr); 
	//    TW_DEBUG2("numbytes:%d, cmdword = 0x%02x, %02x\n", numbytes, msg[0].buf[0], msg[0].buf[1]); 
	/* Make read msg */
	msg[1].addr = zl380tw->i2c->addr;
	msg[1].len  = 2;
	msg[1].buf  = buf;
	msg[1].flags = I2C_M_RD;
	msg[1].scl_rate = 300 * 1000;
	status = i2c_transfer(zl380tw->i2c->adapter, msg, 2);
#endif
	if (status < 0) 
		return status;
	    
	*pdata = (buf[0]<<8) | buf[1] ; /* Byte_HI, Byte_LO */	
	return 0;
}

/* zl38040_hbi_wr16()- this function is used for single word access by the
 * ioctl read. It decodes the 16-bit T-WOLF Regs Host address into
 * page, offset and build the 16-bit command acordingly to the access type.
 * then write the command and data to the device
 *  \param[in]
 *                .addr      the 16-bit HBI address
 *                .data      the 16-bit word to write
 *
 *  return ::status
 */
static int zl380tw_hbi_wr16(struct zl380tw *zl380tw, u16 addr, u16 data) {
	u16 cmd;
	u8 page;
	u8 offset;
	u8 i = 0;
	u8 buf[6] = {0, 0, 0, 0, 0, 0};
	int status = 0;    
	page = addr >> 8;
	offset = (addr & 0xFF)>>1;
    
	if (page == 0) { /*Direct page access*/
		cmd = HBI_DIRECT_WRITE(offset, 0);/*build the cmd*/
		buf[i++] = (cmd >> 8) & 0xFF;
		buf[i++] = (cmd & 0xFF);    
	} else { /*indirect page access*/
		if (page != 0xFF) {
			page  -=  1;
		}
		cmd = HBI_SELECT_PAGE(page);
		i = 0;
		/*select the page*/
		buf[i++] = (cmd >> 8) & 0xFF;
		buf[i++] = (cmd & 0xFF);
		cmd = HBI_PAGED_WRITE(offset, 0); /*build the cmd*/
		buf[i++] = (cmd >> 8) & 0xFF;
		buf[i++] = (cmd & 0xFF);
	}
	buf[i++] = (data >> 8) & 0xFF ;
	buf[i++] = (data & 0xFF) ;
    	
	status = zl380tw_nbytes_wr(zl380tw, i, buf);
	if (status <0) 
		return status;
	return 0;
}
/*poll a specific bit of a register for clearance
* [paran in] addr: the 16-bit register to poll
* [paran in] bit: the bit position (0-15) to poll
* [paran in] timeout: the if bit is not cleared within timeout exit loop
*/
static int zl380tw_monitor_bit(struct zl380tw *zl380tw, u16 addr, u8 bit, u16 timeout) {
	u16 data = 0xBAD;
	do {
		zl380tw_hbi_rd16(zl380tw, addr, &data);
		msleep(10);
	} while ((((data & (1 << bit))>>bit) > 0) &&  (timeout-- >0));

	if (timeout <= 0) {
		TW_DEBUG1(" Operation Mode, in timeout = %d \n", timeout);
		return -1;
	}
	return 0;
}
/* zl380tw_reset(): use this function to reset the device.
 *
 *
 * Input Argument: mode  - the supported reset modes:
 *         VPROC_ZL38040_RST_HARDWARE_ROM,
 *         VPROC_ZL38040_RST_HARDWARE_ROM,
 *         VPROC_ZL38040_RST_SOFT,
 *         VPROC_ZL38040_RST_AEC
 *         VPROC_ZL38040_RST_TO_BOOT
 * Return:  type error code (0 = success, else= fail)
 */
static int zl380tw_reset(struct zl380tw *zl380tw, u16 mode) {

	u16 addr = CLK_STATUS_REG;
	u16 data = 0;
	int monitor_bit = -1; /*the bit (range 0 - 15) of that register to monitor*/

	/*PLATFORM SPECIFIC code*/
	if (mode  == ZL38040_RST_HARDWARE_RAM) {       /*hard reset*/
		/*hard reset*/
		data = 0x0005;
	} else if (mode == ZL38040_RST_HARDWARE_ROM) {  /*power on reset*/
		/*hard reset*/
		data = 0x0009;
	} else if (mode == ZL38040_RST_AEC) { /*AEC method*/
		addr = 0x0300;
		data = 0x0001;
		monitor_bit = 0;
	} else if (mode == ZL38040_RST_SOFTWARE) { /*soft reset*/
		addr = 0x0006;
		data = 0x0002;
		monitor_bit = 1;
	} else if (mode == ZL38040_RST_TO_BOOT) { /*reset to bootrom mode*/
		data = 0x0001;
	} else {
		TW_DEBUG1("Invalid reset type\n");
		return -EINVAL;
	}
	if (zl380tw_hbi_wr16(zl380tw, addr, data) < 0)
		return -EFAULT;
		msleep(50); /*wait for the HBI to settle*/

	if (monitor_bit >= 0) {
		if (zl380tw_monitor_bit(zl380tw, addr, monitor_bit, 1000) < 0)
			return -EFAULT;
	}    
	return 0;
}
/* tw_mbox_acquire(): use this function to
 *   check whether the host or the device owns the mailbox right
 *
 * Input Argument: None
 * Return: error code (0 = success, else= fail)
 */
static int zl380tw_mbox_acquire(struct zl380tw *zl380tw) {
	int status =0;
	/*Check whether the host owns the command register*/
	u16 i=0;
	u16 temp = 0x0BAD;
	for (i = 0; i < TWOLF_MBCMDREG_SPINWAIT; i++) {
		status = zl380tw_hbi_rd16(zl380tw, ZL38040_SW_FLAGS_REG, &temp);
		if ((status < 0)) {
			TW_DEBUG1("ERROR %d: \n", status);
			return status;
		}
		if (!(temp & ZL38040_SW_FLAGS_CMD)) {
			break;
		}
		msleep(10); /*release*/
		TW_DEBUG2("cmdbox =0x%04x timeout count = %d: \n", temp, i);
	}
	TW_DEBUG2("timeout count = %d: \n", i);
	if ((i>= TWOLF_MBCMDREG_SPINWAIT) && (temp != ZL38040_SW_FLAGS_CMD)) {
		return -EBUSY;
	}
	/*read the Host Command register*/
	return 0;
}

/* zl380tw_cmdreg_acquire(): use this function to
 *   check whether the last command is completed
 *
 * Input Argument: None
 * Return: error code (0 = success, else= fail)
 */
static int zl380tw_cmdreg_acquire(struct zl380tw *zl380tw) {
	int status =0;
	/*Check whether the host owns the command register*/
	u16 i=0;
	u16 temp = 0x0BAD;
	for (i = 0; i < TWOLF_MBCMDREG_SPINWAIT; i++) {
		status = zl380tw_hbi_rd16(zl380tw, ZL38040_CMD_REG, &temp);
		if ((status < 0)) {
			TW_DEBUG1("ERROR %d: \n", status);
			return status;
		}
		if (temp == ZL38040_CMD_IDLE) {
			break;
		}
		msleep(10); /*wait*/
		TW_DEBUG2("cmdReg =0x%04x timeout count = %d: \n", temp, i);
	}
	TW_DEBUG2("timeout count = %d: \n", i);
	if ((i>= TWOLF_MBCMDREG_SPINWAIT) && (temp != ZL38040_CMD_IDLE)) {
		return -EBUSY;
	}
	/*read the Host Command register*/
	return 0;
}
/* zl380tw_write_cmdreg(): use this function to
 *   access the host command register (mailbox access type)
 *
 * Input Argument: cmd - the command to send
 * Return: error code (0 = success, else= fail)
 */
static int zl380tw_write_cmdreg(struct zl380tw *zl380tw, u16 cmd) {
	int status = 0;
	u16 buf = cmd;
	/*Check whether the host owns the command register*/
	status = zl380tw_mbox_acquire(zl380tw);
	if ((status < 0)) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}
	/*write the command into the Host Command register*/

	status = zl380tw_hbi_wr16(zl380tw, ZL38040_CMD_REG, buf);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}
	/*Release the command reg*/
	buf = ZL38040_SW_FLAGS_CMD;
	status = zl380tw_hbi_wr16(zl380tw, ZL38040_SW_FLAGS_REG, buf);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}
	return zl380tw_cmdreg_acquire(zl380tw);
}

/* Write 16bit HBI Register */
/* zl380tw_wr16()- write a 16bit word
 *  \param[in]     cmdword  the 16-bit word to write
 *
 *  return ::status
 */
static int zl380tw_wr16(struct zl380tw *zl380tw, u16 cmdword) {

	u8 buf[2] = {0, 0};
	int status = 0;
#ifdef MICROSEMI_HBI_SPI
	struct spi_message msg;
	struct spi_transfer xfer = {
		.len = 2,
		.tx_buf = buf,
	};

	spi_message_init(&msg);
	buf[0] = (cmdword >> 8) & 0xFF ;
	buf[1] = (cmdword & 0xFF) ;

	spi_message_add_tail(&xfer, &msg);
	status = spi_sync(zl380tw->spi, &msg);
#endif
#ifdef MICROSEMI_HBI_I2C
	status = zl380tw_i2c_master_send(zl380tw->i2c, buf, 2);
	if (status == -1) {
		TW_DEBUG1("ERROR %d: \n", status);
	}
#endif
	return status;
}
/*To initialize the Twolf HBI interface
 * Param[in] - cmd_config  : the 16-bit HBI init command ored with the
 *                           8-bit configuration.
 *              The command format is cmd_config = 0xFD00 | CONFIG_VAL
 *              CONFIG_VAL: is you desired HBI config value
 *              (see device datasheet)
 */
static int zl380tw_hbi_init(struct zl380tw *zl380tw, u16 cmd_config) {
	return zl380tw_wr16(zl380tw, HBI_CONFIGURE(cmd_config));
}

#if defined(ZL380XX_TW_ENABLE_CHAR_DEV_DRIVER) || defined(ZL380XX_TW_UPDATE_FIRMWARE)
static int zl380tw_cmdresult_check(struct zl380tw *zl380tw) {
	int status = 0;
	u16 buf;
	status = zl380tw_hbi_rd16(zl380tw, ZL38040_CMD_PARAM_RESULT_REG, &buf);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}
	
	if (buf !=0) {
		TW_DEBUG1("Command failed...Resultcode = 0x%04x\n", buf);
		return buf;
	}
	return 0;
}

/*stop_fwr_to_bootmode - use this function to stop the firmware currently
 *running
 * And set the device in boot mode
 * \param[in] none
 *
 * \retval ::0 success
 * \retval ::-EINVAL or device error code
 */
static int zl380tw_stop_fwr_to_bootmode(struct zl380tw *zl380tw) {
	return zl380tw_write_cmdreg(zl380tw, ZL38040_CMD_FWR_STOP);
}

/*start_fwr_from_ram - use this function to start/restart the firmware
 * previously stopped with VprocTwolfFirmwareStop()
 * \param[in] none
 *
 * \retval ::0 success
 * \retval ::-EINVAL or device error code
 */
static int zl380tw_start_fwr_from_ram(struct zl380tw *zl380tw) {
	return zl380tw_write_cmdreg(zl380tw, ZL38040_CMD_FWR_GO);
}

/*tw_init_check_flash - use this function to check if there is a flash on board
 * and initialize it
 * \param[in] none
 *
 * \retval ::0 success
 * \retval ::-EINVAL or device error code
 */
static int zl380tw_init_check_flash(struct zl380tw *zl380tw) {
	/*if there is a flash on board initialize it*/
	return zl380tw_write_cmdreg(zl380tw, ZL38040_CMD_HOST_FLASH_INIT);
}

/*tw_erase_flash - use this function to erase all firmware image
* and related  config from flash
 * previously stopped with VprocTwolfFirmwareStop()
 * \param[in] none
 *
 * \retval ::0 success
 * \retval ::-EINVAL or device error code
 */
static int zl380tw_erase_flash(struct zl380tw *zl380tw) {
	int status =0;
	/*if there is a flash on board initialize it*/
	status = zl380tw_init_check_flash(zl380tw);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}
	/*erase all config/fwr*/
	status = zl380tw_hbi_wr16(zl380tw, ZL38040_CMD_PARAM_RESULT_REG, 0xAA55);
	if (status < 0) {
		return status;
	}
	/*erase firmware*/
	return zl380tw_write_cmdreg(zl380tw, 0x0009);
}
/*erase_fwrcfg_from_flash - use this function to erase a psecific firmware image
* and related  config from flash
 * previously stopped with VprocTwolfFirmwareStop()
 * Input Argument: image_number   - (range 1-14)
 *
 * \retval ::0 success
 * \retval ::-EINVAL or device error code
 */
static int zl380tw_erase_fwrcfg_from_flash(struct zl380tw *zl380tw, u16 image_number) {
	int status = 0;
	if (image_number <= 0) {
		return -EINVAL;
	}
	/*save the config/fwr to flash position image_number */
	status = zl380tw_stop_fwr_to_bootmode(zl380tw);
	if (status < 0) {
		return status;
	}
	msleep(50);
	/*if there is a flash on board initialize it*/
	status = zl380tw_init_check_flash(zl380tw);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}

	status = zl380tw_hbi_wr16(zl380tw, ZL38040_CMD_PARAM_RESULT_REG, image_number);
	if (status < 0) {
		return status;
	}
	status  = zl380tw_write_cmdreg(zl380tw, ZL38040_CMD_IMG_CFG_ERASE);
	if (status < 0) {
		return status;
	}
	return zl380tw_cmdresult_check(zl380tw);
}
/* tw_save_image_to_flash(): use this function to
 *     save both the config record and the firmware to flash. It Sets the bit
 *     which initiates a firmware save to flash when the device
 *     moves from a STOPPED state to a RUNNING state (by using the GO bit)
 *
 * Input Argument: None
 * \retval ::0 success
 * \retval ::-EINVAL or device error code
 */
static int zl380tw_save_image_to_flash(struct zl380tw *zl380tw)
{
	int status = 0;
	/*Save firmware to flash*/

	/*delete all applications on flash*/
	status = zl380tw_erase_flash(zl380tw);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: tw_erase_flash\n", status);
		return status;
	}
	/*save the image to flash*/
	status = zl380tw_write_cmdreg(zl380tw, ZL38040_CMD_IMG_CFG_SAVE);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: zl380tw_write_cmdreg\n", status);
		return status;
	}
	return zl380tw_cmdresult_check(zl380tw);
	/*return status;*/
}

/*load_fwr_from_flash - use this function to load a specific firmware image
* from flash
 * previously stopped with VprocTwolfFirmwareStop()
 * Input Argument: image_number   - (range 1-14)
 *
 * \retval ::0 success
 * \retval ::-EINVAL or device error code
 */
static int zl380tw_load_fwr_from_flash(struct zl380tw *zl380tw, u16 image_number) {
	int status = 0;
	if (image_number <= 0) {
		return -EINVAL;
	}
	/*if there is a flash on board initialize it*/
	status = zl380tw_init_check_flash(zl380tw);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}
	/*load the fwr to flash position image_number */
	status = zl380tw_hbi_wr16(zl380tw, ZL38040_CMD_PARAM_RESULT_REG, image_number);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}
	return zl380tw_write_cmdreg(zl380tw, ZL38040_CMD_IMG_LOAD);
}

/*zl380tw_load_fwrcfg_from_flash - use this function to load a specific firmware image
* related and config from flash
 * previously stopped with VprocTwolfFirmwareStop()
 * Input Argument: image_number   - (range 1-14)
 *
 * \retval ::0 success
 * \retval ::-EINVAL or device error code
 */
static int  zl380tw_load_fwrcfg_from_flash(struct zl380tw *zl380tw, u16 image_number) {
	int status = 0;
	if (image_number <= 0) {
		return -EINVAL;
	}
	/*if there is a flash on board initialize it*/
	status = zl380tw_init_check_flash(zl380tw);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}
	/*save the config to flash position image_number */
	status = zl380tw_hbi_wr16(zl380tw, ZL38040_CMD_PARAM_RESULT_REG, image_number);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}
	return zl380tw_write_cmdreg(zl380tw, ZL38040_CMD_IMG_CFG_LOAD);
}
/*zl380tw_load_cfg_from_flash - use this function to load a specific firmware image
* from flash
 *  * Input Argument: image_number   - (range 1-14)
 *
 * \retval ::0 success
 * \retval ::-EINVAL or device error code
 */
static int zl380tw_load_cfg_from_flash(struct zl380tw *zl380tw, u16 image_number) {
	int status = 0;
	if (image_number <= 0) {
		return -EINVAL;
	}
	/*if there is a flash on board initialize it*/
	status = zl380tw_init_check_flash(zl380tw);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}
	/*load the config from flash position image_number */
	status = zl380tw_hbi_wr16(zl380tw, ZL38040_CMD_PARAM_RESULT_REG, image_number);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}
	return zl380tw_write_cmdreg(zl380tw, ZL38040_CMD_CFG_LOAD);
}

/* save_cfg_to_flash(): use this function to
 *     save the config record to flash. It Sets the bit
 *     which initiates a config save to flash when the device
 *     moves from a STOPPED state to a RUNNING state (by using the GO bit)
 *
 * \retval ::0 success
 * \retval ::-EINVAL or device error code
 */
static int zl380tw_save_cfg_to_flash(struct zl380tw *zl380tw)
{
	int status = 0;
	u16 buf = 0;
	/*Save config to flash*/

	/*if there is a flash on board initialize it*/
	status = zl380tw_init_check_flash(zl380tw);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}
	/*check if a firmware exists on the flash*/
	status = zl380tw_hbi_rd16(zl380tw, ZL38040_FWR_COUNT_REG, &buf);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}
	if (buf < 0)
		return -EBADSLT; /*no firmware on flash to save config for*/

	/*save the config to flash position */
	status = zl380tw_hbi_wr16(zl380tw, ZL38040_CMD_PARAM_RESULT_REG, 0x0001);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}

	/*save the config to flash position */
	status = zl380tw_hbi_wr16(zl380tw, ZL38040_CMD_REG, 0x8002);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}

	/*save the config to flash position */
	status = zl380tw_hbi_wr16(zl380tw, ZL38040_SW_FLAGS_REG, 0x0004);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}
	status = zl380tw_cmdreg_acquire(zl380tw);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: tw_cmdreg_acquire\n", status);
		return status;
	}
	/*Verify wheter the operation completed sucessfully*/
	return zl380tw_cmdresult_check(zl380tw);
}

/*AsciiHexToHex() - to convert ascii char hex to integer hex
 * pram[in] - str - pointer to the char to convert.
 * pram[in] - len - the number of character to convert (2:u8, 4:u16, 8:u32).

 */
static unsigned int AsciiHexToHex(const char * str, unsigned char len)
{
	unsigned int val = 0;
	char c;
	unsigned char i = 0;
	for (i = 0; i< len; i++) {
		c = *str++;
		val <<= 4;

		if (c >= '0' && c <= '9') {
			val += c & 0x0F;
			continue;
		}

		c &= 0xDF;
		if (c >= 'A' && c <= 'F') {
			val += (c & 0x07) + 9;
			continue;
		}
		return 0;
	}
	return val;
}

/* These 3 functions provide an alternative method to loading an *.s3
 *  firmware image into the device
 * Procedure:
 * 1- Call zl380tw_boot_prepare() to put the device in boot mode
 * 2- Call the zl380tw_boot_Write() repeatedly by passing it a pointer
 *    to one line of the *.s3 image at a time until the full image (all lines)
 *    are transferred to the device successfully.
 *    When the transfer of a line is complete, this function will return the sta-
 *    tus VPROC_STATUS_BOOT_LOADING_MORE_DATA. Then when all lines of the image
 *    are transferred the function will return the status
 *         VPROC_STATUS_BOOT_LOADING_CMP
 * 3- zl380tw_boot_conclude() to complete and verify the completion status of
 *    the image boot loading process
 *
 */
static int zl380tw_boot_prepare(struct zl380tw *zl380tw) {
	u16 buf = 0;
	int status = 0;
	status = zl380tw_hbi_wr16(zl380tw, CLK_STATUS_REG, 1); /*go to boot rom mode*/
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}
	msleep(50); /*required for the reset to cmplete*/
	/*check whether the device has gone into boot mode as ordered*/
	status = zl380tw_hbi_rd16(zl380tw, (u16)ZL38040_CMD_PARAM_RESULT_REG, &buf);
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}

	if ((buf != 0xD3D3)) {
		TW_DEBUG1("ERROR: HBI is not accessible, cmdResultCheck = 0x%04x\n", buf);
		return  -EFAULT;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/

static int zl380tw_boot_conclude(struct zl380tw *zl380tw)
{
	int status = 0;
	status = zl380tw_write_cmdreg(zl380tw, ZL38040_CMD_HOST_LOAD_CMP); /*loading complete*/
	if (status < 0) {
		TW_DEBUG1("ERROR %d: \n", status);
		return status;
	}

	/*check whether the device has gone into boot mode as ordered*/
	return zl380tw_cmdresult_check(zl380tw);
}

/*----------------------------------------------------------------------------*/
/*  Read up to 256 bytes data */
/*  slave_zl380xx_nbytesrd()- read one or up to 256 bytes from the device
 *  \param[in]     ptwdata     pointer to the data read
 *  \param[in]     numbytes    the number of bytes to read
 *
 *  return ::status = the total number of bytes received successfully
 *                   or a negative error code
 */
static int zl380tw_nbytes_rd(struct zl380tw *zl380tw, u8 numbytes, u8 *pData, u8 hbiAddrType)
{
	int status = 0;
	int tx_len = (hbiAddrType == TWOLF_HBI_PAGED) ? 4 : 2; 
	u8 tx_buf[4] = {pData[0], pData[1], pData[2], pData[3]};

#ifdef MICROSEMI_HBI_SPI 
	struct spi_message msg;

	struct spi_transfer txfer = {
		.len = tx_len,
		.tx_buf = tx_buf,
	};
	struct spi_transfer rxfer = {
		.len = numbytes,
		.rx_buf = zl380tw->pData,
	};

	spi_message_init(&msg);
	spi_message_add_tail(&txfer, &msg);
	spi_message_add_tail(&rxfer, &msg);
	status = spi_sync(zl380tw->spi, &msg);

#endif
#ifdef MICROSEMI_HBI_I2C
	struct  i2c_msg     msg[2];
	memset(msg,0,sizeof(msg));

	msg[0].addr    = zl380tw->i2c->addr;
	msg[0].len     = tx_len;
	msg[0].buf     = tx_buf;

	msg[1].addr = zl380tw->i2c->addr;
	msg[1].len  = numbytes;
	msg[1].buf  = zl380tw->pData;
	msg[1].flags = I2C_M_RD;
	status = i2c_transfer(zl380tw->i2c->adapter, msg, 2);
#endif
	if (status <0) 
		return status;

	//#ifdef MSCCDEBUG2
	{
		int i = 0;	
		printk("RD: Numbytes = %d, addrType = %d\n", numbytes, hbiAddrType);
		for(i=0;i<numbytes;i++) {                
			printk("0x%02x, ", zl380tw->pData[i]);
		}
		printk("\n");
	}    
	//#endif	
	return 0;
}


static int zl380tw_hbi_access(struct zl380tw *zl380tw, u16 addr, u8 numbytes, u8 *pData, u8 hbiAccessType) {

	u16 cmd;
	u8 page = addr >> 8;
	u8 offset = (addr & 0xFF)>>1;
	int i = 0;
	u8 hbiAddrType = 0;	
	u8 buf[256];
	int status = 0;
	u8 numwords = (numbytes/2);

#ifdef MICROSEMI_HBI_SPI
	if (zl380tw->spi == NULL) {
		TW_DEBUG1("spi device is not available \n");
		return -EFAULT;
	}
#endif
#ifdef MICROSEMI_HBI_I2C
	if (zl380tw->i2c == NULL) {
		TW_DEBUG1("i2c device is not available \n");
		return -EFAULT;
	}
#endif

	if (pData == NULL)
		return -EINVAL;
	   
	if (!((hbiAccessType == TWOLF_HBI_WRITE) || (hbiAccessType == TWOLF_HBI_READ))) 
		return -EINVAL;    

	if (page == 0) { /*Direct page access*/
	
		if (hbiAccessType == TWOLF_HBI_WRITE) {
			cmd = HBI_DIRECT_WRITE(offset, numwords-1);/*build the cmd*/
		} else {
			cmd = HBI_DIRECT_READ(offset, numwords-1);/*build the cmd*/
		}
		buf[i++] = (cmd >> 8) & 0xFF ;
		buf[i++] = (cmd & 0xFF) ;
		hbiAddrType = TWOLF_HBI_DIRECT;
	} else { /*indirect page access*/
		i = 0;
		if (page != 0xFF) {
			page  -=  1;
		}
		cmd = HBI_SELECT_PAGE(page);
		/*select the page*/
		buf[i++] = (cmd >> 8) & 0xFF ;
		buf[i++] = (cmd & 0xFF) ;

		/*address on the page to access*/
		if (hbiAccessType == TWOLF_HBI_WRITE) {
			cmd = HBI_PAGED_WRITE(offset, numwords-1); 
		} else {
			cmd = HBI_PAGED_READ(offset, numwords-1);/*build the cmd*/
		}
		buf[i++] = (cmd >> 8) & 0xFF ;
		buf[i++] = (cmd & 0xFF) ;
		hbiAddrType = TWOLF_HBI_PAGED;
	}
	memcpy(&buf[i], pData, numbytes);
#ifdef MSCCDEBUG2
	{
		int j = 0;
		int displaynum = numbytes;
		if (hbiAccessType == TWOLF_HBI_WRITE) {
			displaynum = numbytes;
		} else { 
			displaynum = i;
		}
		printk("SENDING:: Numbytes = %d, accessType = %d\n", numbytes, hbiAccessType);
		for(j=0;j<numbytes;j++) {                
			printk("0x%02x, ", pData[j]);
		}
		printk("\n");
	}    
#endif		
	if (hbiAccessType == TWOLF_HBI_WRITE) {
		status = zl380tw_nbytes_wr(zl380tw, numbytes+i, buf);
	} else {
		printk("++++++++++++++++++++++++++++++++++++%d++++++++++++++++++++++++++++++++++\n",hbiAddrType);
		status = zl380tw_nbytes_rd(zl380tw, numbytes, buf, hbiAddrType);
	}
	if (status < 0)
		return -EFAULT;
		     
	return status;
}
/*
 * __ Upload a romcode  by blocks
 * the user app will call this function by passing it one line from the *.s3
 * file at a time.
 * when the firmware boot loading process find the execution address
 * in that block of data it will return the number 23
 * indicating that the transfer on the image data is completed, otherwise,
 * it will return 22 indicating tha tit expect more data.
 * If error, then a negative error code will be reported
 */
static int zl380tw_boot_Write(struct zl380tw *zl380tw, char *blockOfFwrData/*0: HBI; 1:FLASH*/) {
/*Use this method to load the actual *.s3 file line by line*/
	int status = 0;
	int rec_type, i=0, j=0;
	u8 numbytesPerLine = 0;
	u8 buf[MAX_TWOLF_FIRMWARE_SIZE_IN_BYTES];
	unsigned long address = 0;
	u8 page255Offset = 0x00;
	u16 cmd = 0;

	TW_DEBUG2("firmware line# = %d :: blockOfFwrData = %s\n",++myCounter, blockOfFwrData);

	if (blockOfFwrData == NULL) {
		TW_DEBUG1("blockOfFwrData[0] = %c\n", blockOfFwrData[0]);
		return -EINVAL;
	}
	/* if this line is not an srecord skip it */
	if (blockOfFwrData[0] != 'S') {
		TW_DEBUG1("blockOfFwrData[0] = %c\n", blockOfFwrData[0]);
		return -EINVAL;
	}
	/* get the srecord type */
	rec_type = blockOfFwrData[1] - '0';

	numbytesPerLine = AsciiHexToHex(&blockOfFwrData[2], 2);
	//TW_DEBUG2("numbytesPerLine = %d\n", numbytesPerLine);
	if (numbytesPerLine == 0) {
		TW_DEBUG1("blockOfFwrData[3] = %c\n", blockOfFwrData[3]);
		return -EINVAL;
	}

	/* skip non-existent srecord types and block header */
	if ((rec_type == 4) || (rec_type == 5) || (rec_type == 6) || (rec_type == 0)) {
		return TWOLF_STATUS_NEED_MORE_DATA;
	}

	/* get the info based on srecord type (skip checksum) */
	address = AsciiHexToHex(&blockOfFwrData[4], 8);
	buf[0] = (u8)((address >> 24) & 0xFF);
	buf[1] = (u8)((address >> 16) & 0xFF);
	buf[2] = (u8)((address >> 8) & 0xFF);
	buf[3] = 0;
	page255Offset = (u8)(address & 0xFF);
	
	/* store the execution address */
	if ((rec_type == 7) || (rec_type == 8) || (rec_type == 9)) {
		/* the address is the execution address for the program */
		//TW_DEBUG2("execAddr = 0x%08lx\n", address);
		/* program the program's execution start register */
		buf[3] = (u8)(address & 0xFF);
		status = zl380tw_hbi_access(zl380tw, ZL38040_FWR_EXEC_REG, 4, buf, TWOLF_HBI_WRITE);        
		if(status < 0) {
			TW_DEBUG1("ERROR % d: unable to program page 1 execution address\n", status);
			return status;
		}
		TW_DEBUG2("Loading firmware data complete...\n");
		return TWOLF_STATUS_BOOT_COMPLETE;  /*BOOT_COMPLETE Sucessfully*/
	}

	/* put the address into our global target addr */
	//TW_DEBUG2("TW_DEBUG2:gTargetAddr = 0x%08lx: \n", address);
	status = zl380tw_hbi_access(zl380tw, PAGE_255_BASE_HI_REG, 4, buf, TWOLF_HBI_WRITE);    
	if (status < 0) {
		TW_DEBUG1("ERROR %d: gTargetAddr = 0x%08lx: \n", status, address);
		return -EFAULT;
	}

	/* get the data bytes */
	j = 12;
	//TW_DEBUG2("buf[]= 0x%02x, 0x%02x, \n", buf[0], buf[1]);
	for (i = 0; i < numbytesPerLine - 5; i++) {
		buf[i] = AsciiHexToHex(&blockOfFwrData[j], 2);
		j +=2;
		//TW_DEBUG2("0x%02x, ", buf[i+4]);
	}
	/* write the data to the device */
	cmd = (u16)(0xFF<<8) | (u16)page255Offset;
	status = zl380tw_hbi_access(zl380tw, cmd, (numbytesPerLine - 5), buf, TWOLF_HBI_WRITE);    
	if(status < 0) {
		return status;
	}

	//TW_DEBUG2("Provide next block of data...\n");
	return TWOLF_STATUS_NEED_MORE_DATA; /*REQUEST STATUS_MORE_DATA*/
}
#endif
/*----------------------------------------------------------------------*
 *   The kernel driver functions are defined below
 *-------------------------DRIVER FUNCTIONs-----------------------------*/
/* zl380tw_ldfwr()
 * This function basically  will load the firmware into the Timberwolf device
 * at power up. this is convenient for host pluging system that does not have
 * a slave EEPROM/FLASH to store the firmware, therefore, and that
 * requires the device to be fully operational at power up
*/
#ifdef ZL380XX_TW_UPDATE_FIRMWARE
static int zl380tw_ldfwr(struct zl380tw *zl380tw) {
	u32 j =0;
	int status = 0;	
	const struct firmware *twfw;
	u8 numbytesPerLine = 0;
	u8 block_size = 0;
	
	zl380twEnterCritical();
	status = request_firmware(&twfw, ZLS380_TWOLF, &zl380tw->spi->dev);
	if (status) {
		TW_DEBUG1("err %d, request_firmware failed to load %s\n", status, ZLS380_TWOLF);
		zl380twExitCritical();
		return -EINVAL;
	}

	/*check validity of the S-record firmware file*/
	if (twfw->data[0] != 'S') {
		TW_DEBUG1("Invalid S-record %s file for this device\n", ZLS380_TWOLF);
		release_firmware(twfw);
		zl380twExitCritical();
		return -EINVAL;
	}

	zl38040->pData = kzalloc(MAX_TWOLF_FIRMWARE_SIZE_IN_BYTES, GFP_KERNEL);
	if (zl38040->pData == NULL) {
		dev_err(&zl38040->spi->dev, "can't allocate memory\n");
		status = -ENOMEM;
		goto fwr_cleanup;
	}

	status = zl380tw_boot_prepare(zl380tw);
	if (status < 0) {
		TW_DEBUG1("err %d, tw boot prepare failed\n", status);
		goto fwr_cleanup;
	}
	
	do {
		numbytesPerLine = AsciiHexToHex(&twfw->data[j+2], 2);
		block_size = (4 + (2*numbytesPerLine));
		
		memcpy(zl380tw->pData, &twfw->data[j], block_size);
		j += (block_size+2);

		status = zl380tw_boot_Write(zl380tw, zl380tw->pData);
		if ((status != (int)TWOLF_STATUS_NEED_MORE_DATA) && (status != (int)TWOLF_STATUS_BOOT_COMPLETE)) {
		      TW_DEBUG1(KERN_ERR "err %d, tw boot write failed\n", status);
		      goto fwr_cleanup;
		}

	} while((j < twfw->size) && (status != TWOLF_STATUS_BOOT_COMPLETE));

	status = zl380tw_boot_conclude(zl380tw);
	if (status < 0) {
		TW_DEBUG1("err %d, twfw->size = %d, tw boot conclude -firmware loading failed\n", status, twfw->size);
		goto fwr_cleanup;
	}
#ifdef ZL38040_SAVE_FWR_TO_FLASH
	status = zl380tw_save_image_to_flash(zl380tw);
	if (status < 0) {
		TW_DEBUG1("err %d, twfw->size = %d, saving firmware failed\n", status, twfw->size);
		goto fwr_cleanup;
	}
#endif  /*ZL38040_SAVE_FWR_TO_FLASH*/
	status = zl380tw_start_fwr_from_ram(zl380tw);
	if (status < 0) {
		TW_DEBUG1("err %d, twfw->size = %d, starting firmware failed\n", status, twfw->size);
		goto fwr_cleanup;
	}
	
fwr_cleanup:
	zl380twExitCritical();
	release_firmware(twfw);
	
	return status;
}
#endif


/*--------------------------------------------------------------------
 *    ALSA  SOC CODEC driver
 *--------------------------------------------------------------------*/

#ifdef ZL380XX_TW_ENABLE_ALSA_CODEC_DRIVER

/* ALSA soc codec default Timberwolf register settings
 * 3 Example audio cross-point configurations
 */

/*pure stereo bypass with no AEC
 * Record
 * MIC1 -> I2S-L
 * MIC2 -> I2S-R
 * Playback
 * I2S-L -> DAC1
 * I2S-R -> DAC2
 *reg 0x202 - 0x226
 */
#define CODEC_CONFIG_REG_NUM 19
u16 reg_stereo[] = {
	0x000F, 0x0010, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0005, 0x0006, 0x0001, 0x0002, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

/*4-port mode with AEC enabled  - ROUT to DAC1
 * MIC1 -> SIN (ADC)
 * I2S-L -> RIN (TDM Rx path)
 * SOUT -> I2S-L
 * ROUT -> DACx
 *reg 0x202 - 0x226
 */
u16 reg_aec[] = {
	0x0c05, 0x0010, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x000d, 0x0000, 0x000e, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x000d, 0x0001, 0x0005
};


/*Loopback mode ADC to DAC1
 * MIC1 -> DAC1
 *reg 0x202 - 0x226
 */
u16 reg_loopback[] = {
	0x0003, 0x0010, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0001, 0x0002, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x000d, 0x0000, 0x0000
};



/*Formatting for the Audio*/
#define zl380tw_DAI_RATES            (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_48000)
#define zl380tw_DAI_FORMATS          (SNDRV_PCM_FMTBIT_S16_LE)
#define zl380tw_DAI_CHANNEL_MIN      1
#define zl380tw_DAI_CHANNEL_MAX      2


static unsigned int zl380tw_reg_read(struct snd_soc_codec *codec, unsigned int reg) {
	u16 buf;
	unsigned int value = 0;
	struct zl380tw *zl380tw = snd_soc_codec_get_drvdata(codec);
	
	if (zl380tw_hbi_rd16(zl380tw, reg, &buf) < 0) {
		return -EIO;
	}
	value = buf;
	return value;   
}

static int zl380tw_reg_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int value) {	
	struct zl380tw *zl380tw = snd_soc_codec_get_drvdata(codec);
	if (zl380tw_hbi_wr16(zl380tw, reg, value) < 0) {
		return -EIO;
	}
	return 0;
}
/*ALSA- soc codec I2C/SPI read control functions*/
static int zl380tw_control_read(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {
	u16 val;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct zl380tw *zl380tw = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc = (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int mask = mc->max;
	unsigned int invert = mc->invert;

	zl380twEnterCritical();
	if (zl380tw_hbi_rd16(zl380tw, reg, &val)) {
		zl380twExitCritical();
		return -EIO;
	}
	
	zl380twExitCritical();
	
	ucontrol->value.integer.value[0] = ((val >> shift) & mask);
	
	if (invert) { 
		ucontrol->value.integer.value[0] = mask - ucontrol->value.integer.value[0];
	}
	return 0;
}
/*ALSA- soc codec I2C/SPI write control functions*/
static int zl380tw_control_write(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol) {

	u16 valt;	
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct zl380tw *zl380tw = snd_soc_codec_get_drvdata(codec);

	struct soc_mixer_control *mc = (struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int mask = mc->max;
	unsigned int invert = mc->invert;
	unsigned int val = (ucontrol->value.integer.value[0] & mask);

	if (invert) { 
		val = mask - val;
	}
	
	zl380twEnterCritical();
	if (zl380tw_hbi_rd16(zl380tw, reg, &valt) < 0) {
		zl380twExitCritical();
		return -EIO;
	}
	
	if (((valt >> shift) & mask) == val) {
		zl380twExitCritical();
		return 0;
	}
	
	valt &= ~(mask << shift);
	valt |= val << shift;

	if (zl380tw_reg_write(codec, reg, valt) < 0) {
		zl380twExitCritical();
		return -EIO;
	}
	zl380twExitCritical();
	return 0;
}

int zl380tw_mute_r(struct snd_soc_codec *codec, int on) {
	u16 val;
	struct zl380tw *zl380tw = snd_soc_codec_get_drvdata(codec);

	zl380twEnterCritical();

	if (zl380tw_hbi_rd16(zl380tw, ZL38040_AEC_CTRL_REG0, &val) < 0){
		zl380twExitCritical();
		return -EIO;
	}
	
	if (((val >> 7) & 1) == on){
		zl380twExitCritical();
		return 0;
	}
	
	val &= ~(1 << 7);
	val |= on << 7;

	if (zl380tw_reg_write(codec, ZL38040_AEC_CTRL_REG0, val) < 0){
		zl380twExitCritical();
		return -EIO;
	}

	zl380twExitCritical();
	return 0;
}

int zl380tw_mute_s(struct snd_soc_codec *codec, int on) {
	u16 val;
	struct zl380tw *zl380tw = snd_soc_codec_get_drvdata(codec);

	zl380twEnterCritical();

	if (zl380tw_hbi_rd16(zl380tw, ZL38040_AEC_CTRL_REG0, &val)){
		zl380twExitCritical();
		return -EIO;
	}
	
	if (((val >> 8) & 1) == on){
		zl380twExitCritical();
		return 0;
	}
	
	val &= ~(1 << 8);
	val |= on << 8;

	if (zl380tw_reg_write(codec, ZL38040_AEC_CTRL_REG0, val)){
		zl380twExitCritical();
		return -EIO;
	}

	zl380twExitCritical();
	return 0;
}


/* configure_codec() - configure the cross-point to either pure 2-channel stereo
 * or for 4-port mode with Achoustic Echo Canceller
 * mode: 0 -> Stereo bypass
 *       1 -> 4-port mode AEC
 *       2 ->
 */
static int zl380tw_configure_codec(struct snd_soc_codec *codec, u8 mode) {
	struct zl380tw *zl380tw = snd_soc_codec_get_drvdata(codec);
	u16 *pData;
	u8 mic_en = 1; /*Enable just MIC1*/

	int status = 0, i;
	u16 dac1 =0x0000, dac2 =0x0000;

	if (zl380tw_hbi_rd16(zl380tw, ZL38040_DAC1_EN_REG, &dac1)){
		zl380twExitCritical();
		return -EFAULT;
	}
	
	switch (mode) {
	case ZL38040_SINGLE_CHANNEL_AEC:
		pData = reg_aec;
		dac1 |= (ZL38040_DACx_P_EN | ZL38040_DACx_M_EN);
		break;
	case ZL38040_STEREO_BYPASS:
		pData = reg_stereo;
		mic_en = 3; /*Enable MIC1 and MIC2 for stereo recording*/
		if (zl380tw_hbi_rd16(zl380tw, ZL38040_DAC2_EN_REG, &dac2)){
			zl380twExitCritical();
			return -EFAULT;
		}
		dac1 |= (ZL38040_DACx_P_EN | ZL38040_DACx_M_EN);
		dac2 |= (ZL38040_DACx_P_EN | ZL38040_DACx_M_EN);
		break;
	case ZL38040_ADDA_LOOPBACK:
		pData = reg_loopback;
		dac1 |= (ZL38040_DACx_P_EN | ZL38040_DACx_M_EN);
		break;
	default: 
	    return -EINVAL;
	}
	
	for (i = 0; i <= CODEC_CONFIG_REG_NUM; i++) {
		if (zl380tw_hbi_wr16(zl380tw, ZL38040_CACHED_ADDR_LO +(2*i), pData[i]) < 0)
			return -1;
	}

	if (zl380tw_reg_write(codec, ZL38040_MIC_EN_REG, (u16)mic_en) < 0)
		return -1;

	if (zl380tw_reg_write(codec, ZL38040_DAC1_EN_REG, dac1) < 0)
		return -1;
	
	if (zl380tw_reg_write(codec, ZL38040_DAC2_EN_REG, dac2) < 0)
		return -1;

	//dev_info(codec->dev, "mode= %d: dac1=0x%04x, dac2=0x%04x, mic=0x%02x\n", mode, dac1, dac2, mic_en);

	status  = zl380tw_reset(zl380tw, ZL38040_RST_SOFTWARE); /*soft-reset*/
	if (status < 0) {
		return status;
	}

	return 0;
}

/*The DACx, I2Sx, TDMx Gains can be used in both AEC mode or Stereo bypass mode
 * however the AEC Gains can only be used when AEC is active.
 * Each input source of the cross-points has two input sources A and B.
 * The Gain for each source can be controlled independantly.
 */
static const struct snd_kcontrol_new zl380tw_snd_controls[] = {
		SOC_SINGLE_EXT("DAC1 GAIN INA", ZL38040_DAC1_GAIN_REG, 0, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("DAC2 GAIN INA", ZL38040_DAC2_GAIN_REG, 0, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),				
		SOC_SINGLE_EXT("I2S1L GAIN INA", ZL38040_I2S1L_GAIN_REG, 0, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("I2S1R GAIN INA", ZL38040_I2S1R_GAIN_REG, 0, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),				
		SOC_SINGLE_EXT("I2S2L GAIN INA", ZL38040_I2S2L_GAIN_REG, 0, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("I2S2R GAIN INA", ZL38040_I2S2R_GAIN_REG, 0, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),				
		SOC_SINGLE_EXT("TDMA3 GAIN INA", ZL38040_TDMA3_GAIN_REG, 0, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("TDMA4 GAIN INA", ZL38040_TDMA4_GAIN_REG, 0, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),				
		SOC_SINGLE_EXT("TDMB3 GAIN INA", ZL38040_TDMB3_GAIN_REG, 0, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("TDMB4 GAIN INA", ZL38040_TDMB4_GAIN_REG, 0, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("DAC1 GAIN INB", ZL38040_DAC1_GAIN_REG, 8, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("DAC2 GAIN INB", ZL38040_DAC2_GAIN_REG, 8, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),				
		SOC_SINGLE_EXT("I2S1L GAIN INB", ZL38040_I2S1L_GAIN_REG, 8, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("I2S1R GAIN INB", ZL38040_I2S1R_GAIN_REG, 8, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),				
		SOC_SINGLE_EXT("I2S2L GAIN INB", ZL38040_I2S2L_GAIN_REG, 8, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("I2S2R GAIN INB", ZL38040_I2S2R_GAIN_REG, 8, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),				
		SOC_SINGLE_EXT("TDMA3 GAIN INB", ZL38040_TDMA3_GAIN_REG, 8, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("TDMA4 GAIN INB", ZL38040_TDMA4_GAIN_REG, 8, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),				
		SOC_SINGLE_EXT("TDMB3 GAIN INB", ZL38040_TDMB3_GAIN_REG, 8, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("TDMB4 GAIN INB", ZL38040_TDMB4_GAIN_REG, 8, 0x6, 0,
				zl380tw_control_read, zl380tw_control_write),	
		SOC_SINGLE_EXT("AEC ROUT GAIN", ZL38040_USRGAIN, 0, 0x78, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("AEC ROUT GAIN EXT", ZL38040_USRGAIN, 7, 0x7, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("AEC SOUT GAIN", ZL38040_SYSGAIN, 8, 0xf, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("AEC MIC GAIN", ZL38040_SYSGAIN, 0, 0xff, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("MUTE SPEAKER ROUT", ZL38040_AEC_CTRL_REG0, 7, 1, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("MUTE MIC SOUT", ZL38040_AEC_CTRL_REG0, 8, 1, 0,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("AEC Bypass", ZL38040_AEC_CTRL_REG0, 4, 1, 1,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("AEC Audio enh bypass", ZL38040_AEC_CTRL_REG0, 5, 1, 1,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("AEC Master Bypass", ZL38040_AEC_CTRL_REG0, 1, 1, 1,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("ALC GAIN", ZL38040_AEC_CTRL_REG1, 12, 1, 1,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("ALC Disable", ZL38040_AEC_CTRL_REG1, 10, 1, 1,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("AEC Tail disable", ZL38040_AEC_CTRL_REG1, 12, 1, 1,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("AEC Comfort Noise", ZL38040_AEC_CTRL_REG1, 6, 1, 1,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("NLAEC Disable", ZL38040_AEC_CTRL_REG1, 14, 1, 1,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("AEC Adaptation", ZL38040_AEC_CTRL_REG1, 1, 1, 1,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("NLP Disable", ZL38040_AEC_CTRL_REG1, 5, 1, 1,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("Noise Inject", ZL38040_AEC_CTRL_REG1, 6, 1, 1,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("LEC Disable", ZL38040_LEC_CTRL_REG, 4, 1, 1,
				zl380tw_control_read, zl380tw_control_write),
		SOC_SINGLE_EXT("LEC Adaptation", ZL38040_LEC_CTRL_REG, 1, 1, 1,
				zl380tw_control_read, zl380tw_control_write),

};


int zl380tw_add_controls(struct snd_soc_codec *codec)
{
	return snd_soc_add_controls(codec, zl380tw_snd_controls,
			ARRAY_SIZE(zl380tw_snd_controls));
}

static int zl380tw_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
    struct zl380tw *zl380tw = snd_soc_codec_get_drvdata(codec);

 	unsigned int buf2, buf3, buf4;

 	unsigned int buf0 = zl380tw_reg_read(codec, ZL38040_TDMA_CLK_CFG_REG);

    zl380twEnterCritical();
 	buf2 = zl380tw_reg_read(codec, ZL38040_TDMA_CH1_CFG_REG);
 	buf3 = zl380tw_reg_read(codec, ZL38040_TDMA_CH2_CFG_REG);
 	buf4 = zl380tw_reg_read(codec, ZL38040_AEC_CTRL_REG0);

    if ((buf2 == 0x0BAD) || (buf3 == 0x0BAD)) {
	   	zl380twExitCritical();
        return -1;
    }
 	switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			buf2 |= (ZL38040_TDMA_16BIT_LIN);
			buf3 |= (ZL38040_TDMA_16BIT_LIN);
			buf3 |= 2;  /*If PCM use 2 timeslots*/
			break;
		case SNDRV_PCM_FORMAT_MU_LAW:
		    buf2 |= (ZL38040_TDMA_8BIT_ULAW);
			buf3 |= (ZL38040_TDMA_8BIT_ULAW);
			buf3 |= 1;  /*If PCM use 1 timeslots*/
		    break;
		case SNDRV_PCM_FORMAT_A_LAW:
		    buf2 |= (ZL38040_TDMA_8BIT_ALAW );
			buf3 |= (ZL38040_TDMA_8BIT_ALAW);
			buf3 |= 1;  /*If PCM use 1 timeslots*/
		    break;
		case SNDRV_PCM_FORMAT_S20_3LE:
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			break;
		default: {
			zl380twExitCritical();
			return -EINVAL;
		}
	}

    /* If the ZL38040 TDM is configured as a slace I2S/PCM,
	 * The rate settings are not necessary. The Zl38040 has the ability to
	 * auto-detect the master rate and configure itself accordingly
	 * If configured as master, the rate must be set acccordingly
	 */

    switch (params_rate(params)) {
		/* For 8KHz and 16KHz sampling use a single DAC out with
		 * AEC algorithm in 4-port mode enabled or in stere mode if AEC is disabled
		 * For 44.1 and 48KHz, use full 2-channel stereo mode
		 */
	    case 8000:
	    case 16000:
	        if ((buf4 & ZL38040_MASTER_BYPASS_EN) >> 1)
	            zl380tw_configure_codec(codec, ZL38040_STEREO_BYPASS);
	        else
		        zl380tw_configure_codec(codec, ZL38040_SINGLE_CHANNEL_AEC);
		    break;
		/*case 44100:  */
	    case 48000:
		    zl380tw_configure_codec(codec, ZL38040_STEREO_BYPASS);
		    break;
		default: {
			zl380twExitCritical();
			return -EINVAL;
		}
    }
    zl380tw_reg_write(codec, ZL38040_TDMA_CLK_CFG_REG, buf0);

	zl380tw_reg_write(codec, ZL38040_TDMA_CH1_CFG_REG, buf2);
	zl380tw_reg_write(codec, ZL38040_TDMA_CH2_CFG_REG, buf3);
	zl380tw_reset(zl380tw, ZL38040_RST_SOFTWARE);
    zl380twExitCritical();
	return 0;
}

static int zl380tw_mute(struct snd_soc_dai *codec_dai, int mute)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	/*zl380tw_mute_s(codec, mute);*/
	return zl380tw_mute_r(codec, mute);
}

static int zl380tw_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{

    struct snd_soc_codec *codec = codec_dai->codec;
    struct zl380tw *zl380tw = snd_soc_codec_get_drvdata(codec);
    unsigned int buf, buf0;

    zl380twEnterCritical();
    buf = zl380tw_reg_read(codec, ZL38040_TDMA_CFG_REG);
    buf0 = zl380tw_reg_read(codec, ZL38040_TDMA_CLK_CFG_REG);
    if ((buf == 0x0BAD) || (buf0 == 0x0BAD)) {
	   	zl380twExitCritical();
        return -1;
    }

	dev_info(codec_dai->dev, "zl380tw_set_dai_fmt\n");
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_DSP_B:
			break;
		case SND_SOC_DAIFMT_DSP_A:
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			if ((buf & ZL38040_TDMA_FSALIGN) >> 1)
		        buf &= (~ZL38040_TDMA_FSALIGN);
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			if (!((buf & ZL38040_TDMA_FSALIGN) >> 1))
		        buf |= (ZL38040_TDMA_FSALIGN);
			break;
		case SND_SOC_DAIFMT_I2S:
	        if (!((buf & ZL38040_TDM_I2S_CFG_VAL) >> 1)) {
                buf |= (ZL38040_TDM_I2S_CFG_VAL );
             }
			break;
		default: {
			zl380twExitCritical();
			return -EINVAL;
		}
	}

	/* set master/slave TDM interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBM_CFM:
			buf0 |= ZL38040_TDM_TDM_MASTER_VAL;
			break;
		case SND_SOC_DAIFMT_CBS_CFS:
			if ((buf0 & ZL38040_TDM_TDM_MASTER_VAL) >> 1) {
			    buf0 &= (~ZL38040_TDM_TDM_MASTER_VAL);
           }
			break;
		default: {
			zl380twExitCritical();
			return -EINVAL;
		}
	}

    zl380tw_reg_write(codec, ZL38040_TDMA_CFG_REG, buf);
    zl380tw_reg_write(codec, ZL38040_TDMA_CLK_CFG_REG, buf0);
	zl380tw_reset(zl380tw, ZL38040_RST_SOFTWARE);
	zl380twExitCritical();
	return 0;
}

static int zl380tw_set_dai_sysclk(struct snd_soc_dai *codec_dai, int clk_id,
		unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct zl380tw *zl380tw = snd_soc_codec_get_drvdata(codec);

 	zl380tw->sysclk_rate = freq;

	return 0;
}

static const struct snd_soc_dai_ops zl380tw_dai_ops = {
		.set_fmt        = zl380tw_set_dai_fmt,
		.set_sysclk     = zl380tw_set_dai_sysclk,
		.hw_params   	= zl380tw_hw_params,
		.digital_mute   = zl380tw_mute,

};

static struct snd_soc_dai_driver zl380tw_dai = {
		.name = "zl380tw-hifi",
		.playback = {
				.stream_name = "Playback",
				.channels_min = zl380tw_DAI_CHANNEL_MIN,
				.channels_max = zl380tw_DAI_CHANNEL_MAX,
				.rates = zl380tw_DAI_RATES,
				.formats = zl380tw_DAI_FORMATS,
		},
		.capture = {
				.stream_name = "Capture",
				.channels_min = zl380tw_DAI_CHANNEL_MIN,
				.channels_max = zl380tw_DAI_CHANNEL_MAX,
				.rates = zl380tw_DAI_RATES,
				.formats = zl380tw_DAI_FORMATS,
		},
		.ops = &zl380tw_dai_ops,
};

static int zl380tw_set_bias_level(struct snd_soc_codec *codec,
    enum snd_soc_bias_level level)
{
	struct zl380tw *zl380tw = snd_soc_codec_get_drvdata(codec);

    zl380twEnterCritical();
    switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		/*wake up from sleep*/
		zl380tw_hbi_init(zl380tw, (HBI_CONFIG_VAL | HBI_CONFIG_WAKE));
        msleep(50); 
		/*Clear the wake up bit*/
	    zl380tw_hbi_init(zl380tw, HBI_CONFIG_VAL);

		break;
	case SND_SOC_BIAS_OFF:
		 /*Low power sleep mode*/
		zl380tw_write_cmdreg(zl380tw, ZL38040_CMD_APP_SLEEP);

		break;
	}
	zl380twExitCritical();

	//codec->dapm.bias_level = level;
	return 0;
}

static int zl380tw_suspend(struct snd_soc_codec *codec, pm_message_t state)
{
	zl380twEnterCritical();
	zl380tw_set_bias_level(codec, SND_SOC_BIAS_OFF);
	zl380twExitCritical();
	return 0;
}

static int zl380tw_resume(struct snd_soc_codec *codec)
{
	zl380twEnterCritical();
	zl380tw_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
    zl380twExitCritical();
	return 0;
}

static int zl380tw_probe(struct snd_soc_codec *codec)
{
	dev_info(codec->dev, "Probing zl380tw SoC CODEC driver\n");
    return zl380tw_add_controls(codec);
}

static int zl380tw_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_zl380tw = {
	.probe =	zl380tw_probe,
	.remove =	zl380tw_remove,
	.suspend =	zl380tw_suspend,
	.resume =	zl380tw_resume,
    .read  = zl380tw_reg_read,
#ifdef ENABLE_REGISTER_CACHING
	.reg_cache_size = zl380tw_CACHE_NUM,
	.reg_cache_default = zl380tw_cache,
	.reg_cache_step = 1,
#endif
	.write = zl380tw_reg_write,
	.set_bias_level = zl380tw_set_bias_level,
	.reg_word_size = sizeof(u16),
};

#endif /*ZL380XX_TW_ENABLE_ALSA_CODEC_DRIVER*/

/*--------------------------------------------------------------------
 *    ALSA  SOC CODEC driver  - END
 *--------------------------------------------------------------------*/


/*--------------------------------------------------------------------
 *    CHARACTER type Host Interface driver 
 *--------------------------------------------------------------------*/
#ifdef ZL380XX_TW_ENABLE_CHAR_DEV_DRIVER

/* read 16bit HBI Register */
/* slave_rd16()- read a 16bit word
 *  \param[in]     pointer the to where to store the data
 *
 *  return ::status
 */
static int zl380tw_rd16(struct zl380tw *zl380tw, u16 *pdata)
{
	u8 buf[2] = {0, 0};
	int status = 0;
#ifdef MICROSEMI_HBI_SPI
	struct spi_message msg;
	struct spi_transfer xfer = {
		.len = 2,
		.rx_buf = buf,
	};

	spi_message_init(&msg);

	spi_message_add_tail(&xfer, &msg);
	status = spi_sync(zl380tw->spi, &msg);
#endif
#ifdef MICROSEMI_HBI_I2C
    status = i2c_master_recv(zl380tw->i2c, buf, 2);
#endif
	if (status < 0) {
		return status;
	}

    *pdata = (buf[0]<<8) | buf[1] ; /* Byte_HI, Byte_LO */
	return 0;
}


 static long zl380tw_io_ioctl(
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
   struct inode *i,
#endif
   struct file *filp,
   unsigned int cmd, unsigned long arg)
{
	int retval =0;
	u16 buf =0;
	struct zl380tw *zl380tw = filp->private_data;

	switch (cmd) {

	case TWOLF_HBI_WR16:
		if (copy_from_user(&zl380tw_ioctl_buf,
			               (ioctl_zl380tw *)arg,
			                sizeof(ioctl_zl380tw)))
			return -EFAULT;
		zl380twEnterCritical();
		retval = zl380tw_hbi_wr16(zl380tw, (u16)zl380tw_ioctl_buf.addr,
                                     zl380tw_ioctl_buf.data);
        zl380twExitCritical();
	break;
	case TWOLF_HBI_RD16:
		if (copy_from_user(&zl380tw_ioctl_buf,
			               (ioctl_zl380tw *)arg,
			                sizeof(ioctl_zl380tw)))
			return -EFAULT;
		zl380twEnterCritical();
		retval = zl380tw_hbi_rd16(zl380tw, (u16)zl380tw_ioctl_buf.addr,
                                    &zl380tw_ioctl_buf.data);
        zl380twExitCritical();
		if (!retval) {
			if (copy_to_user((ioctl_zl380tw *)arg,
			                  &zl380tw_ioctl_buf,
                              sizeof(ioctl_zl380tw)))
				return -EFAULT;
		} else
			return -EAGAIN;
	break;
	case TWOLF_BOOT_PREPARE :
        zl380twEnterCritical();
	    retval = zl380tw_boot_prepare(zl380tw);
	    zl380twExitCritical();
	break;
	case TWOLF_BOOT_SEND_MORE_DATA: {
		if (copy_from_user(zl380tw->pData,
			               (char *)arg,
			                256))
			return -EFAULT;
		zl380twEnterCritical();
		retval = zl380tw_boot_Write(zl380tw, zl380tw->pData);
        zl380twExitCritical();
	    break;
	}
	case TWOLF_BOOT_CONCLUDE :
        zl380twEnterCritical();
	    retval = zl380tw_boot_conclude(zl380tw);
	    zl380twExitCritical();
	break;

	case TWOLF_CMD_PARAM_REG_ACCESS :
		retval = __get_user(buf, (u16 __user *)arg);
		if (retval ==0) {
		    zl380twEnterCritical();
		    retval = zl380tw_write_cmdreg(zl380tw, buf);
		    zl380twExitCritical();
        }
	break;
	case TWOLF_CMD_PARAM_RESULT_CHECK :
        zl380twEnterCritical();
	    retval = zl380tw_cmdresult_check(zl380tw);
	    zl380twExitCritical();
	break;
	case TWOLF_RESET :
		retval = __get_user(buf, (u16 __user *)arg);
		if (retval ==0) {
	       zl380twEnterCritical();
		   retval = zl380tw_reset(zl380tw, buf);
		   zl380twExitCritical();
        }
	break;
	case TWOLF_SAVE_FWR_TO_FLASH :
         zl380twEnterCritical();
		 retval = zl380tw_save_image_to_flash(zl380tw);
		 zl380twExitCritical();
	break;
	case TWOLF_LOAD_FWR_FROM_FLASH :
         retval = __get_user(buf, (u16 __user *)arg);
         if (retval ==0) {
             zl380twEnterCritical();
		     retval = zl380tw_load_fwr_from_flash(zl380tw, buf);
		     zl380twExitCritical();
         }
	break;
	case TWOLF_SAVE_CFG_TO_FLASH :
             zl380twEnterCritical();
		     retval = zl380tw_save_cfg_to_flash(zl380tw);
		     zl380twExitCritical();
	break;
	case TWOLF_LOAD_CFG_FROM_FLASH :
         retval = __get_user(buf, (u16 __user *)arg);
         if (retval ==0) {
             zl380twEnterCritical();
		     retval = zl380tw_load_cfg_from_flash(zl380tw, buf);
		     zl380twExitCritical();
         }
	break;
	case TWOLF_ERASE_IMGCFG_FLASH :
         retval = __get_user(buf, (u16 __user *)arg);
         if (retval ==0) {
             zl380twEnterCritical();
		     retval = zl380tw_erase_fwrcfg_from_flash(zl380tw, buf);
		     zl380twExitCritical();
         }
	break;
	case TWOLF_LOAD_FWRCFG_FROM_FLASH :
         retval = __get_user(buf, (u16 __user *)arg);
         if (retval ==0) {
             zl380twEnterCritical();
		     retval = zl380tw_load_fwrcfg_from_flash(zl380tw, buf);
		     zl380twExitCritical();
         }
	break;
	case TWOLF_HBI_WR_ARB_SINGLE_WORD :
        retval = __get_user(buf, (u16 __user *)arg);
        if (retval ==0) {
            zl380twEnterCritical();
		    retval = zl380tw_wr16(zl380tw, buf);
		    zl380twExitCritical();
        }
	break;
	case TWOLF_HBI_RD_ARB_SINGLE_WORD :
         zl380twEnterCritical();
		 retval = zl380tw_rd16(zl380tw, &buf);
		 zl380twExitCritical();
         if (retval ==0)
            retval = __put_user(buf, (__u16 __user *)arg);
	break;
	case TWOLF_HBI_INIT :
         retval = __get_user(buf, (u16 __user *)arg);
         if (retval ==0) {
            zl380twEnterCritical();
		    retval = zl380tw_hbi_init(zl380tw, buf);
		    zl380twExitCritical();
         }
	break;
	case TWOLF_ERASE_ALL_FLASH :
         zl380twEnterCritical();
         retval = zl380tw_reset(zl380tw, ZL38040_RST_TO_BOOT);
         if (retval ==0)
		     retval = zl380tw_erase_flash(zl380tw);
		 zl380twExitCritical();
	break;
	case TWOLF_STOP_FWR :
         zl380twEnterCritical();
		 retval = zl380tw_stop_fwr_to_bootmode(zl380tw);
		 zl380twExitCritical();
	break;
	case TWOLF_START_FWR :
         zl380twEnterCritical();
		 retval = zl380tw_start_fwr_from_ram(zl380tw);
		 zl380twExitCritical();
	break;

	default:
		printk(KERN_DEBUG "ioctl: Invalid Command Value");
		retval = -EINVAL;
	}
	return retval;
}



/*----------------------------------------------------------------------*
 *   The ZL38040/05x/06x/08x kernel specific aceess functions are defined below
 *-------------------------ZL380xx FUNCTIONs-----------------------------*/

/* This function is best used to simply retrieve pending data following a
 * previously sent write command
 * The data is returned in bytes format. Up to 256 data bytes.
 */

static ssize_t zl380tw_io_read(struct file *filp, char __user *buf, size_t count,
                                                           loff_t *f_pos)
{
    /* This access uses the spi/i2c command frame format - where both
     * the whole data is read in one active chip_select
     */

	struct zl380tw	*zl380tw = filp->private_data;

	int	status = 0;
    u16 cmd = 0;
	if (count > twHBImaxTransferSize)
		return -EMSGSIZE;

#ifdef MICROSEMI_HBI_SPI
	if (zl380tw->spi == NULL) {
	    TW_DEBUG1("spi device is not available \n");
	    return -ESHUTDOWN;
    }
#endif    
#ifdef MICROSEMI_HBI_I2C
	if (zl380tw->i2c == NULL) {
	    TW_DEBUG1("zl380tw_io_read::i2c device is not available \n");
	    return -ESHUTDOWN;
    }
   
#endif
    zl380twEnterCritical();  
	if (copy_from_user(zl380tw->pData, buf, count)) {  
       zl380twExitCritical(); 
	   return -EFAULT;
    }
    cmd = (*(zl380tw->pData + 0) << 8) | (*(zl380tw->pData + 1));
    /*read the data*/
    status = zl380tw_hbi_access(zl380tw, 
                       cmd, count, zl380tw->pData, TWOLF_HBI_READ);
	if (status < 0) { 
          zl380twExitCritical();
          return -EFAULT; 
    }          
	if (copy_to_user(buf, zl380tw->pData, count)) {
          zl380twExitCritical();
		  return -EFAULT;
    }	  
    zl380twExitCritical();
	return 0;
}

int reset_soft_zl380_chip(void) {
	int ret;
	unsigned short cmd = 0x0006;
	unsigned char buf[4] = {0x0, 0x06, 0x0, 0x02};
/*	zl380tw_priv->pData[0] = 0x0;
	zl380tw_priv->pData[1] = 0x6;
	zl380tw_priv->pData[2] = 0x0;
	zl380tw_priv->pData[3] = 0x2;
*/	
	if(zl380tw_priv != NULL) {
		ret = zl380tw_hbi_access(zl380tw_priv, cmd, 2, &buf[2], TWOLF_HBI_WRITE);
	} else {
		ret = -1;
		printk("zl380tw_priv is NULL\n");
	}
	return ret;
}
EXPORT_SYMBOL_GPL(reset_soft_zl380_chip);
/* Write multiple bytes (up to 254) to the device
 * the data should be formatted as follows
 * cmd_type,
 * cmd_byte_low,
 * cmd_byte_hi,
 * data0_byte_low,
 * data0_byte_hi
 *  ...
 * datan_byte_low, datan_byte_hi   (n < 254)
 */

static ssize_t zl380tw_io_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
    /* This access use the spi/i2c command frame format - where both
     * the command and the data to write are sent in one active chip_select
     */
	struct zl380tw	*zl380tw = filp->private_data;


   // printk("77777777777777777777777777777777777777777777777\n");
	int status = 0;
	u16 cmd = 0;
    
	if (count > twHBImaxTransferSize) 
		return -EMSGSIZE;
    zl380twEnterCritical();  
	if (copy_from_user(zl380tw->pData, buf, count)) {
        zl380twExitCritical();
	    return -EFAULT;
    }    
	cmd = (*(zl380tw->pData + 0) << 8) | (*(zl380tw->pData + 1));
    TW_DEBUG2("count = %d\n",count);
    /*remove the cmd numbytes from the count*/        
    status = zl380tw_hbi_access(zl380tw, 
                       cmd, count-2, (zl380tw->pData+2), TWOLF_HBI_WRITE);	
    if (status < 0) {
       zl380twExitCritical();
       return -EFAULT;
    } 
    zl380twExitCritical();
	return status;
}

static int zl380tw_io_open(struct inode *inode, struct file *filp)
{

    //u8 ret; 
    //struct i2c_client *client; 
    //struct i2c_adapter *adap = i2c_get_adapter(CONTROLLER_I2C_BUS_NUM); // 1 means i2c-1 bus
    //my_client = i2c_new_dummy (adap, 0x69); // 0x69 - slave address on i2c bus
	//struct zl380tw *zl380tw = zl380tw_priv;
	printk(KERN_ERR "To open microsemi_slave_zl380xx device\n");
    if (module_usage_count) {
		printk(KERN_ERR "microsemi_slave_zl380xx device alrady opened\n");
		return -EBUSY;
	}


    /* Allocating memory for the data buffer */
	printk(KERN_ERR "Allocating memory for the data buffer1 \n");
    zl380tw_priv->pData = kmalloc(twHBImaxTransferSize, GFP_KERNEL);
	printk(KERN_ERR "Allocating memory for the data buffer2 \n");
    if (!zl380tw_priv->pData) {
        printk(KERN_ERR "Error allocating %d bytes pdata memory",
                                                 twHBImaxTransferSize);
		return -ENOMEM;
    }
    memset(zl380tw_priv->pData, 0, twHBImaxTransferSize);

	module_usage_count++;
	filp->private_data = zl380tw_priv;

	return 0;
}

static int zl380tw_io_close(struct inode *inode, struct file *filp)
{

	struct zl380tw *zl380tw = filp->private_data;
	filp->private_data = NULL;

	if (module_usage_count) {
		module_usage_count--;
	}

 
	if (!module_usage_count) {
                            
	   kfree((void *)zl380tw->pData);
	   zl380tw->pData = NULL;
    }

	return 0;
}

static const struct file_operations zl380tw_fops = {
	.owner =	THIS_MODULE,
	.open =		zl380tw_io_open,
	.read =     zl380tw_io_read,
	.write =    zl380tw_io_write,
	.release =	zl380tw_io_close,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
	.ioctl =	zl380tw_io_ioctl
#else
	.unlocked_ioctl =	zl380tw_io_ioctl
#endif
};

/*----------------------------------------------------------------------------*/



static struct class *zl380tw_class;
#endif /*ZL380XX_TW_ENABLE_CHAR_DEV_DRIVER */
 /*--------------------------------------------------------------------
 *    CHARACTER type Host Interface driver - END
 *--------------------------------------------------------------------*/

/*--------------------------------------------------------------------
* SPI driver registration
*--------------------------------------------------------------------*/
#if 0
/* Bind the client driver to a master adapter. This is normally done at your board init code via
 * i2c_register_board_info()
 *If you already have this done at your ./arch/../board init code with the i2c_register_board_info
 * then, ignore this function.....
 */
#ifdef MICROSEMI_HBI_SPI
static int zl380xx_slaveMaster_binding(struct spi_device *client) {
    int ret = 0;

    struct spi_board_info spi_device_info = {
        .modalias = "microsemi_zl380xx",
        .max_speed_hz = SPIM_CLK_SPEED,
        .bus_num = SPIM_BUS_NUM,
        .chip_select = SPIM_CHIP_SELECT,
        .mode = SPIM_MODE,
    };

    struct spi_master *master;

    /* get the master device, given SPI the bus number*/
    master = spi_busnum_to_master( spi_device_info.bus_num );
    if( !master )
        return -ENODEV;

    /* create a new slave device, given the master and device info*/
    client = spi_new_device( master, &spi_device_info );
    if( !client )
        return -ENODEV;

    return ret;
}
#endif

#ifdef 0
static int zl380xx_slaveMaster_binding(struct i2c_client *client) {
    int ret = 0;

    struct i2c_board_info i2c_device_info = {
    	I2C_BOARD_INFO("zl380tw", MICROSEMI_I2C_ADDR),
    	.platform_data	= NULL,
    };
    /*get the i2c adapter*/
    struct i2c_adapter *master = client->adapter;
    /*create a new client for that adapter*/
    client = i2c_new_device( master, &i2c_device_info );
    if( !client)
        return -ENODEV;


    return ret;
}
#endif
#endif


#ifdef MICROSEMI_HBI_SPI
static int __devinit zl380tw_spi_probe(struct spi_device *spi)
{

	int err;
	/* Allocate driver data */
	zl380tw_priv = kzalloc(sizeof(*zl380tw_priv), GFP_KERNEL);
	if (zl380tw_priv == NULL)
		return -ENOMEM;

	dev_dbg(&spi->dev, "probing zl380tw spi device\n");
	
#if 0	
    err =  zl380xx_slaveMaster_binding(spi);
    printk(KERN_ERR "ret =%d\n", err);
    err =0;    
#endif  
    spi->master->bus_num = SPIM_BUS_NUM;
	spi->mode = SPIM_MODE;
	spi->max_speed_hz = SPIM_CLK_SPEED;
	spi->chip_select = SPIM_CHIP_SELECT;

	spi->bits_per_word = 8;

	err = spi_setup(spi);
	if (err < 0) {
        kfree(zl380tw_priv);
		return err;
    }
	/* Initialize the driver data */
	spi_set_drvdata(spi, zl380tw_priv);
	zl380tw_priv->spi = spi;

#ifdef ZL380XX_TW_ENABLE_ALSA_CODEC_DRIVER
	err = snd_soc_register_codec(&spi->dev, &soc_codec_dev_zl380tw, &zl380tw_dai, 1);
	if(err < 0) {
		kfree(zl380tw_priv);
		dev_dbg(&spi->dev, "zl380tw spi device not created!!!\n");
		return err;
	}
#endif
	
#ifdef ZL380XX_TW_UPDATE_FIRMWARE
    if (zl380tw_ldfwr(zl380tw_priv) < 0) {
        dev_dbg(&spi->dev, "error loading the firmware into the codec\n");
#ifdef ZL380XX_TW_ENABLE_ALSA_CODEC_DRIVER        
        snd_soc_unregister_codec(&spi->dev);
#endif        
        kfree(zl380tw_priv);
        return -ENODEV;
    }
#endif
    dev_dbg(&spi->dev, "zl380tw codec device created...\n");
	return 0;
}

static int __devexit zl380tw_spi_remove(struct spi_device *spi)
{
#ifdef ZL380XX_TW_ENABLE_ALSA_CODEC_DRIVER       
	snd_soc_unregister_codec(&spi->dev);
#endif	
	kfree(spi_get_drvdata(spi));
	return 0;
}

static struct spi_driver zl380tw_spi_driver = {
	.driver = {
		.name = "zl380tw",
		.owner = THIS_MODULE,
	},
	.probe = zl380tw_spi_probe,
	.remove = __devexit_p(zl380tw_spi_remove),
};
#endif

#ifdef MICROSEMI_HBI_I2C
static int zl380tw_parse_dt(struct device_node *np) {
	int ret;
	unsigned int gpio;
	enum of_gpio_flags gpio_flags;
	/**key irq gpio */
	gpio = of_get_named_gpio_flags(np, "reset_gpio", 0, &gpio_flags);
	ret = gpio_request(gpio, "reset_gpio");
	if (ret < 0) {
		printk("request I/O reset_gpio : %d failed\n", gpio);
		return -ENODEV;
	}
	gpio_direction_output(gpio, 0);
	msleep(20);
	gpio_set_value(gpio, 1);
	return 0;
}
static int zl380tw_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	int err = 0;
	struct device_node *np = i2c->dev.of_node;
	/* Allocate driver data */
	zl380tw_priv = kzalloc(sizeof(*zl380tw_priv), GFP_KERNEL);
	if (zl380tw_priv == NULL)
		return -ENOMEM;
//printk("77777777777777777777777777777777777777777777777\n");
//printk("77777777777777777777777777777777777777777777777\n");
//printk("77777777777777777777777777777777777777777777777\n");
//printk("77777777777777777777777777777777777777777777777\n");
#if 0	
    err =  zl380xx_slaveMaster_binding(i2c);
    printk(KERN_ERR "ret =%d\n", err);
    err =0;    
#endif    	
    //i2c->addr = MICROSEMI_I2C_ADDR;
	zl380tw_parse_dt(np);
	i2c_set_clientdata(i2c, zl380tw_priv);
	zl380tw_priv->i2c = i2c;
	printk(KERN_ERR "i2c slave device address = 0x%04x\n", i2c->addr);

#ifdef ZL380XX_TW_ENABLE_ALSA_CODEC_DRIVER
	err = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_zl380tw, &zl380tw_dai, 1);
	if(err < 0) {
		kfree(zl380tw_priv);
		dev_dbg(&i2c->dev, "zl380tw I2c device not created!!!\n");
		return err;
	}
#endif
	
#ifdef ZL380XX_TW_UPDATE_FIRMWARE
    if (zl380tw_ldfwr(zl380tw_priv) < 0) {
        dev_dbg(&i2c->dev, "error loading the firmware into the codec\n");
#ifdef ZL380XX_TW_ENABLE_ALSA_CODEC_DRIVER        
        snd_soc_unregister_codec(&i2c->dev);
#endif        
        kfree(zl380tw_priv);
        return -ENODEV;
    }
#endif
    dev_dbg(&i2c->dev, "zl380tw I2C codec device created...\n");
	return err;
}

static int zl380tw_i2c_remove(struct i2c_client *i2c)
{
#ifdef ZL380XX_TW_ENABLE_ALSA_CODEC_DRIVER       
	snd_soc_unregister_codec(&i2c->dev);
#endif	
	kfree(i2c_get_clientdata(i2c));
	return 0;
}

static struct i2c_device_id zl380tw_id_table[] = {
    {"zl380tw", 0 },
    {}
 };
static struct of_device_id zl380tw_dt_ids[] = {
    { .compatible = "zl380tw" },
    { }
};
static struct i2c_driver zl380tw_i2c_driver = {
	.driver = {
		.name	= "zl380tw",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(zl380tw_dt_ids),
	},
	.probe = zl380tw_i2c_probe,
	.remove = zl380tw_i2c_remove,
	.id_table = zl380tw_id_table,
};

#endif


static int __init zl380tw_init(void)
{
       
#ifdef ZL380XX_TW_ENABLE_CHAR_DEV_DRIVER
    int status;
	struct device *dev;

	status = alloc_chrdev_region(&t_dev, FIRST_MINOR, MINOR_CNT,
                                                 "zl380tw");
	if (status < 0) {
		printk(KERN_ERR "Failed to register character device");
        return status;
    }

    /*create the device class*/
	zl380tw_class = class_create(THIS_MODULE, "zl380tw");
	if (zl380tw_class == NULL) {
        printk(KERN_ERR "Error %d creating class zl380tw", status);
		unregister_chrdev_region(t_dev, MINOR_CNT);
		return -1;
	}

	/* registration of character device */
	dev = device_create(zl380tw_class, NULL,
                    t_dev, NULL, "zl380tw");
	if (IS_ERR(dev)) {
        printk(KERN_ERR "Error %d creating device zl380tw", status);
        class_destroy(zl380tw_class);
		unregister_chrdev_region(t_dev, MINOR_CNT);
    }
    status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
    if (status  == 0) {
    	/* Initialize of character device */
    	cdev_init(&c_dev, &zl380tw_fops);
    	/* addding character device */
    	status = cdev_add(&c_dev, t_dev, MINOR_CNT);
    	if (status < 0) {
    		printk(KERN_ERR "Error %d adding zl380tw", status);
            class_destroy(zl380tw_class);
            device_destroy(zl380tw_class, t_dev);
            cdev_del(&c_dev);
    		unregister_chrdev_region(t_dev, MINOR_CNT);
    		return -1;
    	}
    	if (status < 0) {
            printk(KERN_ERR "Error %d registering microsemi zl380xx driver", status);
    		class_destroy(zl380tw_class);
            device_destroy(zl380tw_class, t_dev);
            cdev_del(&c_dev);
    		unregister_chrdev_region(t_dev, MINOR_CNT);
    	}
    }  	
#endif      
	
	//printk("77777777777777777777777777777777777777777777777\n");
	//printk("77777777777777777777777777777777777777777777777\n");
	//printk("77777777777777777777777777777777777777777777777\n");
	//printk("77777777777777777777777777777777777777777777777\n");
#ifdef MICROSEMI_HBI_SPI
	return spi_register_driver(&zl380tw_spi_driver);
#endif
#ifdef MICROSEMI_HBI_I2C
	//printk( "888888888888888888888888888888888888888888888\n");
	//printk("888888888888888888888888888888888888888888888\n");
	//printk("888888888888888888888888888888888888888888888\n");
    return i2c_add_driver(&zl380tw_i2c_driver);

#endif
}
module_init(zl380tw_init);

static void __exit zl380tw_exit(void)
{
#ifdef MICROSEMI_HBI_SPI
	spi_unregister_driver(&zl380tw_spi_driver);
#endif
#ifdef MICROSEMI_HBI_I2C
	i2c_del_driver(&zl380tw_i2c_driver);
#endif
#ifdef ZL380XX_TW_ENABLE_CHAR_DEV_DRIVER
    device_destroy(zl380tw_class, t_dev);
	class_destroy(zl380tw_class);
	cdev_del(&c_dev);
	unregister_chrdev_region(t_dev, MINOR_CNT);
#endif
}
module_exit(zl380tw_exit);

MODULE_AUTHOR("Jean Bony <jean.bony@microsemi.com>");
MODULE_DESCRIPTION(" Microsemi Timberwolf i2c/spi/char/alsa codec driver");
MODULE_LICENSE("GPL");
