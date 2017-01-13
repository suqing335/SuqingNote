#ifndef __MCUKEY_TEST_H_
#define __MCUKEY_TEST_H_
void *mcukey_test(void *argc);
//extern int set_gkey(unsigned int code);
//extern int g_key_test;
//extern int manual_p_y;
struct mcukey {
	unsigned int code;
	char *name;
	int state;
};

struct mcukey_msg {
	int result;
	int x;
	int y;
	int w;
	int h;
};

#endif
