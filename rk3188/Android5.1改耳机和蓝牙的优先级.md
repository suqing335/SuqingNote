在frameworks\base\media\java\android\media\AudioService.java
    private void onSetWiredDeviceConnectionState(int device, int state, String name)
    {
        synchronized (mConnectedDevices) {
            if ((state == 0) && ((device == AudioSystem.DEVICE_OUT_WIRED_HEADSET) ||
                    /*(device == AudioSystem.DEVICE_OUT_WIRED_HEADPHONE) ||*/
                    (device == AudioSystem.DEVICE_OUT_LINE) /*||
		    (device == AudioSystem.DEVICE_OUT_WIRED_HEADSHANK)*/)) {
                setBluetoothA2dpOnInt(true);
            }
            boolean isUsb = ((device & ~AudioSystem.DEVICE_OUT_ALL_USB) == 0) ||
                            (((device & AudioSystem.DEVICE_BIT_IN) != 0) &&
                             ((device & ~AudioSystem.DEVICE_IN_ALL_USB) == 0));
            handleDeviceConnection((state == 1), device, (isUsb ? name : ""));
            if (state != 0) {
                if ((device == AudioSystem.DEVICE_OUT_WIRED_HEADSET) ||
                    /*(device == AudioSystem.DEVICE_OUT_WIRED_HEADPHONE) ||*///修改蓝牙优先
                    (device == AudioSystem.DEVICE_OUT_LINE) /*||
		    (device == AudioSystem.DEVICE_OUT_WIRED_HEADSHANK)*/) {
                    setBluetoothA2dpOnInt(false);
                }
                if ((device & mSafeMediaVolumeDevices) != 0) {
                    sendMsg(mAudioHandler,
                            MSG_CHECK_MUSIC_ACTIVE,
                            SENDMSG_REPLACE,
                            0,
                            0,
                            null,
                            MUSIC_ACTIVE_POLL_PERIOD_MS);
                }
                // Television devices without CEC service apply software volume on HDMI output
                if (isPlatformTelevision() && ((device & AudioSystem.DEVICE_OUT_HDMI) != 0)) {
                    mFixedVolumeDevices |= AudioSystem.DEVICE_OUT_HDMI;
                    checkAllFixedVolumeDevices();
                    if (mHdmiManager != null) {
                        synchronized (mHdmiManager) {
                            if (mHdmiPlaybackClient != null) {
                                mHdmiCecSink = false;
                                mHdmiPlaybackClient.queryDisplayStatus(mHdmiDisplayStatusCallback);
                            }
                        }
                    }
                }
            } else {
                if (isPlatformTelevision() && ((device & AudioSystem.DEVICE_OUT_HDMI) != 0)) {
                    if (mHdmiManager != null) {
                        synchronized (mHdmiManager) {
                            mHdmiCecSink = false;
                        }
                    }
                }
            }
            if (!isUsb && (device != AudioSystem.DEVICE_IN_WIRED_HEADSET) 
		    && (device != AudioSystem.DEVICE_IN_WIRED_HEADPHONE)
		&&(device != AudioSystem.DEVICE_IN_WIRED_HEADSHANK)) {
                sendDeviceConnectionIntent(device, state, name);
            }
        }
    }

