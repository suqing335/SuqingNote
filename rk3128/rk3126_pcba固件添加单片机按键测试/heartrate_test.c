#include<stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <linux/input.h>
#include <fcntl.h>
#include <dirent.h>
#include "common.h"
#include "heartrate_test.h"
#include "test_case.h"
#include "language.h"

#define	RUN 	1
#define STOP	0

int heart_rate_status = STOP;
static struct testcase_info *tc_info;
int heart_rate_value = 0;

//pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
//pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void *heartrate_test(void *argc)
{
	int i = 0;
	int code;
	int run = 1;
	
	tc_info = (struct testcase_info *)argc;

	if (tc_info->y <= 0)
		tc_info->y = get_cur_print_y();
	ui_print_xy_rgba(0, tc_info->y, 255, 255, 0, 255, "%s\n", PCBA_HEARTRATE);
	while(1) {
		if (heart_rate_status){
			if (!heart_rate_value) {
				ui_print_xy_rgba(0, tc_info->y, 255, 255, 0, 255, "%s\n", PCBA_HEARTRATE);
			} else { 
				ui_print_xy_rgba(0, tc_info->y, 0, 255, 0, 255, "%s [%d]\n", PCBA_HEARTRATE, heart_rate_value);
				heart_rate_status = STOP;

			}
		}else {
			usleep(500*1000);
		}
	}
	return NULL;
}
