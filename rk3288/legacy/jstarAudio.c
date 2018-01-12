#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cutils/log.h>
#include <cutils/config_utils.h>

#include "alsa_audio.h"
#include "jstarAudio.h"

#include "./inc/speak_config.h"
#include "./inc/speak_off_config.h"
#include "./inc/headset_config.h"
#include "./inc/incall_config.h"
#include "./inc/main_mic_config.h"
#include "./inc/base_config.h"
#include "./inc/voip_config.h"

#define ZL380TW_PATH	"/dev/zl380tw"
#define ARRAY_LEN	(1024)

#define msleep(x) 	usleep(x*1000)

char w_value[4] = {0};
unsigned int spk_status = 0;
unsigned int hp_status = 0;
unsigned int mic_status = 0;
unsigned int spk_status_old = 0;
unsigned int hp_status_old = 0;
unsigned int mic_status_old = 0;
unsigned short (*dsp_config_current)[2] = base_config_array;
unsigned short (*dsp_config_old)[2] = base_config_array;

unsigned short dsp_reset_array[2] = {0x0006, 0x0002};
/***
base_config_array数组始终和dsp ram内的数据一直。也就是当前dsp内的数据
每一份配置和base_config_array数据比较就知道需要修改哪些寄存器的值。
**/
#if 1
static int vproc_HbiWrite(int fd, unsigned short cmd, unsigned int numwords, unsigned short *pData) {
	int status;
	int i = 0, j = 2;
	int length;
	unsigned char buf[8];
	if ((numwords == 0) || (numwords > 126)) {
		ALOGE("number of words is out of range. Maximum is 126\n");
		return -1;
	}

	buf[0] = (cmd >> 8) & 0xff;
	buf[1] = (cmd & 0xFF);
	
	//ALOGE("%s,buf[]=0x%02x, 0x%02x, \n", __func__, buf[0], buf[1]);
	
	for (i = 0; i < numwords; i++) {
		buf[j]   = (pData[i] >> 8) & 0xFF;
		buf[j+1] = (pData[i] & 0xFF);
		//ALOGE("%s,0x%02x, 0x%02x, \n", __func__, buf[j], buf[j+1]);
		j += 2;
	}
	//ALOGE("%s,numBytes send = 0x%02x, ", __func__, (numwords+1)*2);
	
	length = (numwords+1)*2;
	i = 0;
	do {
		status = write(fd, buf, length);
		if (status < 0) {
			ALOGE("write func faile i %d\n", i);
			return -2;
		}
		i++;
	} while(status != length && i < 5);
	return 0;
}

static int audio_write_config_array(const unsigned short (*config_array)[2], int length) {
	
	int i;
	int status;
	int fd;
	unsigned short value;
	fd = open(ZL380TW_PATH, O_RDWR);
	if(fd < 0) {
		ALOGE("open file node %s faile, fd : %d\n", ZL380TW_PATH, fd);
		return -1;
	}
	
	/*send the config to the device RAM*/
	for (i = 0; i < length; i++) {
		if(base_config_array[i][1] != config_array[i][1]) {
			value = config_array[i][1];
			status = vproc_HbiWrite(fd, config_array[i][0], 1, &value);
			if (status != 0) {
				/**关闭文件节点**/
				close(fd);
				ALOGE("%s, faile\n", __func__);
				return -1;
			}
			base_config_array[i][1] = config_array[i][1];
		}
	}
	
	status = vproc_HbiWrite(fd, dsp_reset_array[0], 1, &dsp_reset_array[1]);
	if (status != 0) {
		/**关闭文件节点**/
		close(fd);
		ALOGE("%s, faile\n", __func__);
		return -1;
	}
	close(fd);
	//ALOGD("%s,write sucess\n", __func__);	
	return 0;
}

static int set_config_on_route(unsigned int route) {
	int length;
	int status;
	ALOGE("%s, route : %d\n", __func__, route);
	switch(route) {
	case SPEAKER_NORMAL_ROUTE:
	case HEADPHONE_NORMAL_ROUTE:
	case HEADSET_NORMAL_ROUTE:
	case HDMI_NORMAL_ROUTE:
	case SPEAKER_HEADPHONE_NORMAL_ROUTE:
		//ALOGD("SPEAKER_NORMAL_ROUTE config start\n");
		if (dsp_config_current != speak_config_array){
			dsp_config_old = (dsp_config_current != speak_off_config_array) ?
						dsp_config_current : dsp_config_old;
			dsp_config_current = speak_config_array;
			length = sizeof(speak_config_array)/sizeof(speak_config_array[0]);
			status = audio_write_config_array(dsp_config_current, length);
		}
		//ALOGD("SPEAKER_NORMAL_ROUTE config end\n");
		break;
	case SPEAKER_INCALL_ROUTE:	
	case EARPIECE_INCALL_ROUTE:
	case HEADPHONE_INCALL_ROUTE:
	case HEADSET_INCALL_ROUTE:
		//ALOGD("SPEAKER_INCALL_ROUTE config start\n");
		if (dsp_config_current != incall_config_array){
			dsp_config_old = (dsp_config_current != speak_off_config_array) ?
						dsp_config_current : dsp_config_old;
			dsp_config_current = incall_config_array;
			length = sizeof(incall_config_array)/sizeof(incall_config_array[0]);
			status = audio_write_config_array(dsp_config_current, length);
		}
		//ALOGD("SPEAKER_INCALL_ROUTE config end\n");
		break;

	case HEADSET_VOIP_ROUTE:
	case SPEAKER_VOIP_ROUTE:
	case EARPIECE_VOIP_ROUTE:
	case HEADPHONE_VOIP_ROUTE:
		//ALOGD("SPEAKER_VOIP_ROUTE config start\n");
		if (dsp_config_current != voip_config_array){
			dsp_config_old = (dsp_config_current != speak_off_config_array) ?
						dsp_config_current : dsp_config_old;
			dsp_config_current = voip_config_array;
			length = sizeof(voip_config_array)/sizeof(voip_config_array[0]);
			status = audio_write_config_array(dsp_config_current, length);
		}
		//ALOGD("SPEAKER_VOIP_ROUTE config end\n");
		break;
	
	case HEADSET_RINGTONE_ROUTE:
	case SPEAKER_RINGTONE_ROUTE:
	case EARPIECE_RINGTONE_ROUTE:
	case SPEAKER_HEADPHONE_RINGTONE_ROUTE:
		//ALOGD("SPEAKER_RINGTONE_ROUTE config start\n");
		if (dsp_config_current != speak_config_array){
			dsp_config_old = (dsp_config_current != speak_off_config_array) ?
						dsp_config_current : dsp_config_old;
			dsp_config_current = speak_config_array;
			length = sizeof(speak_config_array)/sizeof(speak_config_array[0]);
			status = audio_write_config_array(dsp_config_current, length);
		}
		//ALOGD("SPEAKER_RINGTONE_ROUTE config end\n");
		break;
	case MAIN_MIC_CAPTURE_ROUTE:
	case HANDS_FREE_MIC_CAPTURE_ROUTE:
		//ALOGD("MAIN_MIC_CAPTURE_ROUTE config start\n");
		if (dsp_config_current != main_mic_config_array){
			dsp_config_old = (dsp_config_current != speak_off_config_array) ?
						dsp_config_current : dsp_config_old;
			dsp_config_current = main_mic_config_array;
			length = sizeof(main_mic_config_array)/sizeof(main_mic_config_array[0]);
			status = audio_write_config_array(dsp_config_current, length);
		}
		//ALOGD("MAIN_MIC_CAPTURE_ROUTE config end\n");
		break;
	case INCALL_OFF_ROUTE:
	case CAPTURE_OFF_ROUTE:
	case VOIP_OFF_ROUTE:
		//ALOGD("XXXX_OFF_ROUTE config start\n");
		if(dsp_config_old != NULL) {
			dsp_config_current = dsp_config_old;
			status = audio_write_config_array(dsp_config_current, ARRAY_LEN);
		}
		//ALOGD("XXXX_OFF_ROUTE config end\n");
		break;
	case PLAYBACK_OFF_ROUTE:
		//ALOGD("PLAYBACK_OFF_ROUTE config start\n");
		
		if (dsp_config_current != speak_off_config_array) {
			if(dsp_config_current == main_mic_config_array){
				/*dsp_config_old = (dsp_config_current != speak_off_config_array) ?
							dsp_config_current : dsp_config_old;
				dsp_config_current = main_mic_config_array;*/
				length = sizeof(main_mic_config_array)/sizeof(main_mic_config_array[0]);
				status = audio_write_config_array(dsp_config_current, length);
			} else {
				dsp_config_old = (dsp_config_current != speak_off_config_array) ?
						dsp_config_current : dsp_config_old;
				dsp_config_current = speak_off_config_array;
				length = sizeof(speak_off_config_array)/sizeof(speak_off_config_array[0]);
				status = audio_write_config_array(dsp_config_current, length);
			}
		}
		//ALOGD("PLAYBACK_OFF_ROUTE config end\n");
		break;
	default :
		//ALOGD("default config\n");
		length = sizeof(speak_config_array)/sizeof(speak_config_array[0]);
		status = audio_write_config_array(speak_config_array, length);
		break;
	}
	return status;
}
#else 
static int set_config_on_route(unsigned int route){ return 0;}
#endif
int set_config_status_route(unsigned int route) {
	
	FILE *file = NULL;
	
	set_config_on_route(route);
	
	switch (route) {
	case SPEAKER_NORMAL_ROUTE:
	case EARPIECE_NORMAL_ROUTE:
	case SPEAKER_HEADPHONE_NORMAL_ROUTE:
		hp_status = 0;
		spk_status = 1;
		break;
	case HEADSET_NORMAL_ROUTE:
	case HEADPHONE_NORMAL_ROUTE:
		hp_status = 1;
		spk_status = 0;
		break;
	case SPEAKER_INCALL_ROUTE:
	case EARPIECE_INCALL_ROUTE:
		mic_status = 0;
		hp_status = 0;
		spk_status = 1;
		break;
	case HEADSET_INCALL_ROUTE:
	case HEADPHONE_INCALL_ROUTE:
		mic_status = 1;
		hp_status = 1;
		spk_status = 0;
		break;
	case SPEAKER_RINGTONE_ROUTE:
	case HEADSET_RINGTONE_ROUTE:
	case EARPIECE_RINGTONE_ROUTE:
	case HEADPHONE_RINGTONE_ROUTE:
	case SPEAKER_HEADPHONE_RINGTONE_ROUTE:	
		hp_status = 1;
		spk_status = 1;
		break;
	case SPEAKER_VOIP_ROUTE:
	case EARPIECE_VOIP_ROUTE:
		mic_status = 0;
		hp_status = 0;
		spk_status = 1;
		break;
	case HEADSET_VOIP_ROUTE:
	case HEADPHONE_VOIP_ROUTE:
		mic_status = 1;
		hp_status = 1;
		spk_status = 0;
		break;
	case BLUETOOTH_SOC_MIC_CAPTURE_ROUTE:
	case BLUETOOTH_NORMAL_ROUTE:
	case BLUETOOTH_INCALL_ROUTE:
	case BLUETOOTH_VOIP_ROUTE:
		break;
	case MAIN_MIC_CAPTURE_ROUTE:
		mic_status = 0;
		hp_status = 0;
		spk_status = 1;
		break;
	case HANDS_FREE_MIC_CAPTURE_ROUTE:
		mic_status = 1;
		hp_status = 1;
		spk_status = 0;
		break;
	case PLAYBACK_OFF_ROUTE:
		spk_status = 0;
		break;
	case CAPTURE_OFF_ROUTE:
		break;
	case INCALL_OFF_ROUTE:
		break;
	case VOIP_OFF_ROUTE:
		break;
	case HDMI_NORMAL_ROUTE:

	case USB_NORMAL_ROUTE:

	case USB_CAPTURE_ROUTE:
		break;
		
	default:
		ALOGE("get_route_config() Error route %d", route);
		return 0;
	}
	
	if((hp_status != hp_status_old && hp_status)
		|| (spk_status != spk_status_old && spk_status)
		|| (mic_status != mic_status_old)) {
		msleep(300);
	}
/*	if((hp_status != hp_status_old)
		|| (spk_status != spk_status_old)
		|| (mic_status != mic_status_old)) {
		msleep(300);
	}*/
/*****************************************************/	
	if(hp_status != hp_status_old) {
		/******close headset*******/
		file = fopen(HEADSET_CONTROL_NAME, "r+");
		if(file != NULL) {
			sprintf(w_value, "%d", hp_status);
			fwrite(w_value, sizeof(char), 1, file);
			fclose(file);
			hp_status_old = hp_status;
		}
	}
	if(spk_status != spk_status_old) {
		/******open speak*******/
		file = fopen(SPEAK_CONTROL_NAME, "r+");
		if(file != NULL) {
			sprintf(w_value, "%d", spk_status);
			fwrite(w_value, sizeof(char), 1, file);
			fclose(file);
			spk_status_old = spk_status;
		}
	}
	if(mic_status != mic_status_old) {
		/******open mic*******/
		file = fopen(MAIN_MIC_CONTROL_NAME, "r+");
		if(file != NULL) {
			sprintf(w_value, "%d", mic_status);
			fwrite(w_value, sizeof(char), 1, file);
			fclose(file);
			mic_status_old = mic_status;
		}
	}
/*****************************************************/		
	return 0;
}


