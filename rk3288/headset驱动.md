#headset 驱动
##主要是添加一些设备节点，用来控制IO口，并初始化这些IO口

注册了一个misc设备， /sys/class/misc/spkcontrol/

控制耳机以及功放的节点就是这个文件生成

这些文件在 Hardware/rockchip/audio/legacy/jstarAudio.c中使用。