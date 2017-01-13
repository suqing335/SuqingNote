#include<stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include <linux/input.h>
#include <fcntl.h>
#include <dirent.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "common.h"
#include "mcukey_test.h"
#include "test_case.h"
#include "language.h"
#define test_bit(bit, array)    (array[bit/8] & (1<<(bit%8)))

#define PASSNUM    18
//uint8_t keybitmask[(KEY_MAX + 1) / 8];
//struct key key_code[KEY_MAX];
//uint8_t key_cnt = 0;		/*key counter */
//unsigned int gkey = 0;
//static pthread_mutex_t gkeymutex = PTHREAD_MUTEX_INITIALIZER;
unsigned char ubuffer[20] = {0};
unsigned char tmpchar[20] = {0};
unsigned char keybuffer[20] = {0};
int g_mcukey_test = 0;
static struct testcase_info *tc_info;
//add heart rate
extern int heart_rate_value;
extern int heart_rate_status;
extern pthread_mutex_t mut;
extern pthread_cond_t cond;
#define	RUN 	1
#define STOP	0

#define NR(x) 	(sizeof(x)/sizeof(unsigned char))
static int keycnt = 0;
//unsigned char general_key_st= {0xfb, 0xa4, 0x20};
unsigned char general_key[] = {0xfb, 0xa4, 0x20, 28, 35, 14, 38, 33, 34, 26, 27, 37, 36, 29, 30, 0, 0xfc};
unsigned char amuse_key[] = {0xfb, 0xa4, 0x21, 0, 0, 0, 10, 9, 11, 0, 0, 0, 0, 13, 0, 0, 0xfc};
//unsigned char speed_shortcut1[] = {0xfb, 0xa4, 0x30, 30, 19, 60, 18, 90, 17, 0, 0, 0, 0, 0, 0, 0, 0xfc}; 
unsigned char speed_shortcut1[] = {0xfb, 0xa4, 0x30, 30, 19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xfc}; 
unsigned char speed_shortcut2[] = {0xfb, 0xa4, 0x31, 60, 18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xfc}; 
unsigned char speed_shortcut3[] = {0xfb, 0xa4, 0x32, 90, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xfc}; 

//unsigned char incline_key1[] = {0xfb, 0xa4, 0x40, 3, 22, 6, 21, 9, 22, 0, 0, 0, 0, 0, 0, 0, 0xfc};
unsigned char incline_key1[] = {0xfb, 0xa4, 0x40, 3, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xfc};
unsigned char incline_key2[] = {0xfb, 0xa4, 0x41, 6, 21, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xfc};
unsigned char incline_key3[] = {0xfb, 0xa4, 0x42, 9, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xfc};

struct mcukey key_array[20] = {
	{.code = 0xb, .name = "s1",.state = 0},
	{.code = 0x1, .name = "s2",.state = 0},
	{.code = 0xa, .name = "s3",.state = 0},
	{.code = 0xff, .name = "s4",.state = 0},
	{.code = 0x2a, .name = "s5",.state = 0},
	{.code = 0x45, .name = "s6",.state = 0},
	{.code = 0x46, .name = "s7",.state = 0},
	{.code = 0x14, .name = "s8",.state = 0},
	{.code = 0x17, .name = "s9",.state = 0},
	{.code = 0x1a, .name = "s10",.state = 0},
	{.code = 0x34, .name = "s11",.state = 0},
	{.code = 0x31, .name = "s12",.state = 0},
	{.code = 0x2e, .name = "s13",.state = 0},
	{.code = 0xe,  .name = "s14",.state = 0},
	{.code = 0xd,  .name = "s15",.state = 0},
	{.code = 0xf,  .name = "s16",.state = 0},
	{.code = 0x10, .name = "s17",.state = 0},
	{.code = 0x47, .name = "s18",.state = 0},
	{.code = 0x48, .name = "s19",.state = 0},
	{.code = 0xff, .name = "",   .state = 0},
};

static int get_baudrate(int baudrate) {
	switch(baudrate) {
	case 300: return B300;
	case 600: return B600;
	case 1200: return B1200;
	case 1800: return B1800;
	case 2400: return B2400;
	case 4800: return B4800;
	case 9600: return B9600;
	case 19200: return B19200;
	case 38400: return B38400;
	case 57600: return B57600;
	case 115200: return B115200;
	default: return B9600;
	}
}
static int set_uart_speed(int fd, int speed) {
	/* Configure device */
	int spd = get_baudrate(speed);
	{
		struct termios cfg;
		printf("Configuring host serial port\n");
		if (tcgetattr(fd, &cfg))
		{
			printf("tcgetattr() failed\n");
			//close(fd);
			/* TODO: throw an exception */
			return -1;
		}

		cfmakeraw(&cfg);
		cfsetispeed(&cfg, spd);
		cfsetospeed(&cfg, spd);

		if (tcsetattr(fd, TCSANOW, &cfg))
		{
			printf("tcsetattr() failed\n");
			//close(fd);
			/* TODO: throw an exception */
			return -1;
		}
	}	
	return 0;
}

static int set_key_ui(int flag){
	int i;
	if (flag) {
		for (i = 0; i < NR(key_array); i++) {
			if (ubuffer[3] == key_array[i].code) {
				ui_print_xy_rgba(0, tc_info->y, 255, 255, 0, 255, "%s [%s]\n", PCBA_MCUKEY, key_array[i].name);
				break;
			}
		}
	} else {
		ui_print_xy_rgba(0, tc_info->y, 0, 255, 0, 255, "%s PASS\n", PCBA_MCUKEY);
	} 
	return 0;
}
static int set_key_array(){
	int i;
	//printf("code = %d state = %d\n", ubuffer[3], ubuffer[2]);
	for (i = 0; i < NR(key_array); i++) {
		if (ubuffer[3] == key_array[i].code) {
			if (!key_array[i].state) {
				key_array[i].state = 1;
				set_key_ui(1);
				keycnt++;
			}
		}
	}
	return 0;
}
static int check_key_sum() {
	return (((ubuffer[1] + ubuffer[2] + ubuffer[3]) & 0xff) == ubuffer[4]); 
}
static int sum_add(unsigned char *data, int length){
	int i, sum = 0;
	for(i = 1; i < length; i++) {
		sum += data[i];
	}
	return (sum & 0xff);
}
static void switch_mcukey()
{	
	if ((ubuffer[5] == 0xfe)) {
		if (check_key_sum() && ubuffer[2]) {
			set_key_array();
		}
		//printf("switch_mcukey --check_key = %d ubuffer[2] = %d\n", ret, ubuffer[2]);
	}
}
static void log_printf(unsigned char *data, int length){
	int i;
	for (i = 0; i < length; i++) {
		printf("---***%d***---\n", data[i]);
	}
}
static int mcukey_code_probe(void)
{
	int fd;
	char name[80];
	int i, len = 5;
	int a = 5;
	int passflag = PASSNUM;
	while(--a) {
		fd = open("/dev/ttyS1", O_RDWR);
		if (fd < 0){
			printf("open /dev/ttyS1 failed !!!\n");
			perror("/dev/ttyS1");
			
		} else {
			break;
		}
		sleep(3);
	}
	if (a <= 0) {
		return -1;
	}
	set_uart_speed(fd, 9600);
	printf("open /dev/ttyS1 success!!! fd =%d----\n", fd);
	
	{
		printf("---wirte key config---\n");
		general_key[NR(general_key)-2] = sum_add(general_key, NR(general_key)-2);
		//tcflush(fd, TCIOFLUSH);
	//	memset(ubuffer, 0, sizeof(ubuffer));
		for(i = 0; i < NR(general_key); i++) {
			write(fd, &general_key[i], 1);
			usleep(1000);
		}
	/*	for(i = 0; i < NR(general_key); i++) {
			read(fd, &ubuffer[i], 1);
			usleep(1000);
		}
		
		log_printf(general_key, NR(general_key));
		printf("---wirte general_key---\n");
		
		log_printf(ubuffer, NR(general_key));
		printf("---read general_key---\n");
	*/	
		//tcflush(fd, TCIOFLUSH);
	//	memset(ubuffer, 0, sizeof(ubuffer));
		amuse_key[NR(amuse_key)-2] = sum_add(amuse_key, NR(amuse_key)-2);
		for(i = 0; i < NR(amuse_key); i++) {
			write(fd, &amuse_key[i], 1);
			usleep(1000);
		}
	/*	for(i = 0; i < NR(amuse_key); i++) {
			read(fd, &ubuffer[i], 1);
			usleep(1000);
		}
		log_printf(amuse_key, NR(amuse_key));
		printf("---wirte amuse_key---\n");
		
		log_printf(ubuffer, NR(amuse_key));
		printf("---amuse_key---\n");
	*/	
		memset(ubuffer, 0, sizeof(ubuffer));
		speed_shortcut1[NR(speed_shortcut1)-2] = sum_add(speed_shortcut1, NR(speed_shortcut1)-2);
		for(i = 0; i < NR(speed_shortcut1); i++) {
			write(fd, &speed_shortcut1[i], 1);
			usleep(1000);
		}
	/*	for(i = 0; i < NR(speed_shortcut1); i++) {
			read(fd, &ubuffer[i], 1);
			usleep(1000);
		}
		log_printf(speed_shortcut1, NR(speed_shortcut1));
		printf("---wirte speed_shortcut1---\n");
		
		log_printf(ubuffer, NR(speed_shortcut1));
		printf("---speed_shortcut1---\n");
	*/
		speed_shortcut2[NR(speed_shortcut2)-2] = sum_add(speed_shortcut2, NR(speed_shortcut2)-2);
		for(i = 0; i < NR(speed_shortcut2); i++) {
			write(fd, &speed_shortcut2[i], 1);
			usleep(1000);
		}
		speed_shortcut3[NR(speed_shortcut3)-2] = sum_add(speed_shortcut3, NR(speed_shortcut3)-2);
		for(i = 0; i < NR(speed_shortcut3); i++) {
			write(fd, &speed_shortcut3[i], 1);
			usleep(1000);
		}

		
	//	memset(ubuffer, 0, sizeof(ubuffer));
		incline_key1[NR(incline_key1)-2] = sum_add(incline_key1, NR(incline_key1)-2);
		for(i = 0; i < NR(incline_key1); i++) {
			write(fd, &incline_key1[i], 1);
			usleep(1000);
		}
	/*	for(i = 0; i < NR(incline_key1); i++) {
			read(fd, &ubuffer[i], 1);
			usleep(1000);
		}
		log_printf(incline_key1, NR(incline_key1));
		printf("---wirte incline_key1---\n");
		
		log_printf(ubuffer, NR(incline_key1));
		printf("---incline_key1---\n")
	*/	
		incline_key2[NR(incline_key2)-2] = sum_add(incline_key2, NR(incline_key2)-2);
		for(i = 0; i < NR(incline_key2); i++) {
			write(fd, &incline_key2[i], 1);
			usleep(1000);
		}
		incline_key3[NR(incline_key3)-2] = sum_add(incline_key3, NR(incline_key3)-2);
		for(i = 0; i < NR(incline_key3); i++) {
			write(fd, &incline_key3[i], 1);
			usleep(1000);
		}
		
	}
	while(1) {
		memset(ubuffer, 0, sizeof(ubuffer));
		read(fd, ubuffer, 1);
		if (ubuffer[0] == 0xfd) {
			
			for (i = 1; i <= len; i++) {
				read(fd, &ubuffer[i], 1);
			}
			if (ubuffer[1] == 0x20 && ubuffer[3] <= 0x48){
				//printf("ubuffer[%d] = %x --\n", 0, ubuffer[0]);
				/*for(i = 0; i <= len; i++) {
				
					printf("ubuffer[%d] = %x --\n", i, ubuffer[i]);
				}*/
				//printf("--key_values = %x --\n", ubuffer[3]);
				switch_mcukey();
				//ui_print_xy_rgba(0, tc_info->y, 255, 255, 0, 255, "%s [%x]\n", PCBA_MCUKEY, ubuffer[3]);
				printf("*********key vaule = 0x%x******\n", ubuffer[3]);
			} else if (ubuffer[1] == 0x40 && check_key_sum()){
				heart_rate_value = ubuffer[3];
				if (heart_rate_status == STOP) {
					//pthread_mutex_lock(&mut);
					heart_rate_status = RUN;
					//pthread_cond_signal(&cond);
					//printf("pthread run!\n");
					//pthread_mutex_unlock(&mut);
				}
				printf("*********heart vaule = 0x%x******\n", ubuffer[3]);
			}
			//printf("*********key vaule = 0x%x******\n", ubuffer[3]);
		}
		if (passflag >= PASSNUM && keycnt >= PASSNUM){
			set_key_ui(0);
			passflag--;
		}
	}
	
	close(fd);

	return 0;
}
void *mcukey_test(void *argc)
{
	int i = 0;
	int code;
	int run = 1;

	tc_info = (struct testcase_info *)argc;

	if (tc_info->y <= 0)
		tc_info->y = get_cur_print_y();
	//printf("----------mcukey_test ---- tc_info->y =%d\n", tc_info->y);
	//ui_print_xy_rgba(0, tc_info->y, 255, 0, 0, 255, "%s\n", uichar);
	ui_print_xy_rgba(0, tc_info->y, 255, 255, 0, 255, "%s\n", PCBA_MCUKEY);
	mcukey_code_probe();
	g_mcukey_test = 1;

	return NULL;
}
