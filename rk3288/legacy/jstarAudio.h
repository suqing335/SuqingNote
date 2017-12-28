
#ifndef __JSTAR_AUDIO_H_
#define __JSTAR_AUDIO_H_



#define SPEAK_CONTROL_NAME 	"/sys/class/misc/spkconctrol/spkctl"
#define HEADSET_CONTROL_NAME 	"/sys/class/misc/spkconctrol/hpctl"
#define HEADSET_DETECT_NAME 	"/sys/class/misc/spkconctrol/hpdet"

int set_config_on_route(unsigned int route);
int set_config_on_status(unsigned int route);

#endif /**__JSTAR_AUDIO_H_**/