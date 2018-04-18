###******整理思路就是在headset模式上的基础上加，加一个类似的声音通道，包括底层上报都是类似****************

#####这个笔记只是写了一个大概的框架，所有的细节在audio_shank.tar.gz 补丁中解压后用beyond compare比较一下，很直观可以看出细节。

###底层上报时，各个模式的状态
	headset --------> 1
	headphone ----> 2
	headshank ----> 4

headphone 模式本来没有单独的input输入的，和headset input 共用输入，现在也加出来了

####device\rockchip\common\audio_policy_rk30board.conf
这个文件主要是系统的音频配置文件，不参与编译

####frameworks\av\media\
####frameworks\av\services\
这两个文件夹很重要的衔接Java 和hardware的代码，很多逻辑映射关系在这里映射一次

####frameworks\base\media\java\android\media\
Java关键的代码都在这里，包括识别headshank状态，报给AudioServices,以及加入新状态的值的定义

####hardware\libhardware_legacy\include\hardware_legacy\AudioSystemLegacy.h
这个头文件的定义很关键，这里定义的很多值需要和Java中定义的值有映射关系，而且这些值用于hardware和framework/av

####hardware\rockchip\audio\legacy_hal\
这里是audio音频的所有对codec的对底层的接口，很多逻辑映射关系在这里映射一次
上层声音通道的值，到这里才会具体的切换到那个codec 的声音通道。
播放音乐时调节音量时调节上层的音量大小，经过i2s的数据就已经调节了音量。
而通话时的音量调节则在这里实现的。status_t AudioHardware::setVoiceVolume(float volume)

####kernel\drivers\headset_observe\rk_headset_irq_hook_adc.c
这个文件主要是实现了向上层上报 h2w的state, 实现上层的声音切换。

####system\core\include\system\audio.h
这个头文件很关键，hardware很多值在这里定义，包括很多在java中定义的值，这里要重新定义成名字不一样，但值必须一样。
hardware中实现的映射才不会乱，不然在framework/av中有打印出找不到这个device。

###eg:
	 audio.h :  AUDIO_DEVICE_OUT_WIRED_HEADSHANK	       = 0x400000, //add by cfj
	 AudioSystem.java : public static final int DEVICE_OUT_WIRED_HEADSHANK = 0x400000;//add by cfj
	java类中的变量不能给c++用，所以只能定义值一样的变量来对应上。





