1. 配置5640 Mono 输出音频
rt3224_speaker_mono_normal_controls
rt3224_speaker_mono_incall_controls
rt3224_speaker_mono_ringtone_controls
rt3224_speaker_mono_voip_controls
结合static const struct snd_soc_dapm_route rt3261_dapm_routes[] = {};
static const struct snd_soc_dapm_widget rt3261_dapm_widgets[] = {};
这几个数组，和ALC 5640 codec 数据手册 中第16， 17页中的流程图来配置

下面的代码和打电话调音量有关。
2.在kernel/sound/soc/codecs/rt3261.c
diff --git a/sound/soc/codecs/rt3261.c b/sound/soc/codecs/rt3261.c
index ec68a10..2cf7b26 100755
--- a/sound/soc/codecs/rt3261.c
+++ b/sound/soc/codecs/rt3261.c
@@ -1298,8 +1298,11 @@ static const struct snd_kcontrol_new rt3261_snd_controls[] = {
                RT3261_L_MUTE_SFT, RT3261_R_MUTE_SFT, 1, 1),
        SOC_DOUBLE("OUT Channel Switch", RT3261_OUTPUT,
                RT3261_VOL_L_SFT, RT3261_VOL_R_SFT, 1, 1),
-       SOC_DOUBLE_TLV("OUT Playback Volume", RT3261_OUTPUT,
-               RT3261_L_VOL_SFT, RT3261_R_VOL_SFT, 39, 1, out_vol_tlv),
+       /*SOC_DOUBLE_TLV("OUT Playback Volume", RT3261_OUTPUT,
+               RT3261_L_VOL_SFT, RT3261_R_VOL_SFT, 39, 1, out_vol_tlv),*///delete by cfj
+       SOC_DOUBLE_EXT_TLV("OUT Playback Volume", RT3261_OUTPUT,
+               RT3261_L_VOL_SFT, RT3261_R_VOL_SFT, RT3261_VOL_RSCL_RANGE, 0,
+               rt3261_vol_rescale_get, rt3261_vol_rescale_put, out_vol_tlv)

3.在hardware/rockchip/audio/legacy_hal/AudioHardware.cpp
status_t AudioHardware::setVoiceVolume(float volume)
{
    ALOGV("setVoiceVolume() volume %f", volume);
    android::AutoMutex lock(mLock);
    if (AudioSystem::MODE_IN_CALL == mMode) {
        uint32_t device = AudioSystem::DEVICE_OUT_EARPIECE;
        char ctlName[44] = "";
        if (mOutput != 0) {
            device = mOutput->device();
        }
        ALOGV("setVoiceVolume() route(%d)", device);
        switch (device) {
            case AudioSystem::DEVICE_OUT_EARPIECE:
                ALOGV("earpiece call volume");
                strcpy(ctlName, "Earpiece Playback Volume");
                break;
            case AudioSystem::DEVICE_OUT_SPEAKER: //add by cfj Mono
		ALOGV("speaker call volume");
                strcpy(ctlName, "OUT Playback Volume");
		break;
	    //case AudioSystem::DEVICE_OUT_SPEAKER:
	    case AudioSystem::DEVICE_OUT_WIRED_HEADPHONE:/*add by cfj*/
                ALOGV("speaker call volume");
                strcpy(ctlName, "Speaker Playback Volume");
                break;
            case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO:
            case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
            case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
                ALOGV("bluetooth call volume");
                break;
            case AudioSystem::DEVICE_OUT_WIRED_HEADSET:
	    case AudioSystem::DEVICE_OUT_WIRED_HEADSHANK:
            /*case AudioSystem::DEVICE_OUT_WIRED_HEADPHONE: // Use receive path with 3 pole headset.*/
                ALOGV("headset call volume");
                strcpy(ctlName, "Headphone Playback Volume");
                break;
            default:
                ALOGW("Call volume setting error!!!0x%08x \n", device);
                strcpy(ctlName, "Earpiece Playback Volume");
                break;
        }
        TRACE_DRIVER_IN(DRV_MIXER_SEL)
        route_set_voice_volume(ctlName, volume);
        TRACE_DRIVER_OUT
    }
    return NO_ERROR;
}
4.在hardware/rockchip/audio/legacy_hal/alsa_mixer.c
char *volume_controls_name_table[] = {
    "Earpiece Playback Volume",
    "Speaker Playback Volume",
    "Headphone Playback Volume",
    "OUT Playback Volume",//add by cfj 解决5640 codec Mono 打电话调声音大小失败问题 
    "PCM Playback Volume",
    "Mic Capture Volume",
};




