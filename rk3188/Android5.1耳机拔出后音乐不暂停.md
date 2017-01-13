
frameworks/base/media/java/android/media/AudioService.java
 int mBecomingNoisyIntentDevices =
            /*AudioSystem.DEVICE_OUT_WIRED_HEADSET | AudioSystem.DEVICE_OUT_WIRED_HEADPHONE |*///delete by cfj
            AudioSystem.DEVICE_OUT_ALL_A2DP | AudioSystem.DEVICE_OUT_HDMI |
            AudioSystem.DEVICE_OUT_ANLG_DOCK_HEADSET | AudioSystem.DEVICE_OUT_DGTL_DOCK_HEADSET |
            AudioSystem.DEVICE_OUT_ALL_USB | AudioSystem.DEVICE_OUT_LINE;
    // must be called before removing the device from mConnectedDevices
    private int checkSendBecomingNoisyIntent(int device, int state) {
        int delay = 0;
        if ((state == 0) && ((device & mBecomingNoisyIntentDevices) != 0)) {
            int devices = 0;
            for (int dev : mConnectedDevices.keySet()) {
                if (((dev & AudioSystem.DEVICE_BIT_IN) == 0) &&
                        ((dev & mBecomingNoisyIntentDevices) != 0)) {
                   devices |= dev;
                }
            }
            if (devices == device) {
                sendMsg(mAudioHandler,
                        MSG_BROADCAST_AUDIO_BECOMING_NOISY,
                        SENDMSG_REPLACE,
                        0,
                        0,
                        null,
                        0);
                delay = 1000;
            }
        }
暂停关键在MSG_BROADCAST_AUDIO_BECOMING_NOISY这个值 不进入这个if语句就可以了
