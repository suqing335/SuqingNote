#直流驱动器的控制
驱动的思路：
(1).获取驱动器的数据：
用硬件pwm 捕获功能，根据rk3128平台IR的捕获来实现的。
pwm catpure的波形是如图direct.png
pwm捕获只能捕获波形的高电平时间，刚好这个波形的头波，1波，0波的高电平时间都不一样，可以捕获。

(2).发送数据到直流驱动器：
用gpio模拟，纯软件的，这是个大问题。因为受系统调度的影响。
一开始做的时候大家都没想到这是最难的问题，都认为可以用pwm去发，真的做完捕获时写发送这块代码的时候傻了，发现pwm用不了只能用gpio了。

##在probe中创建三个队列
###用于上层写数据的缓冲区（write）
ishort_queue --> input queue 数据类型 short 16bit 最大储存24个short    

###用于上层读数据的缓冲区(read)
ochar_queue --> output queue 数据类型 char  8bit 最大储存24个char  

pwm_out_queue --> pwm output queue 输出波形到驱动器。保存short中每一位 '1' or '0' 波形的高电平与低电平时间。数据类型为int 32bit  一次存储32个int。当pwm_out_queue为空的时候一次将一个short的数据转换成'1' or '0' 的波形时间。然后用htimer定时改变gpio状态，把数据发出去。这个发送方式也要注意，是高位先发，最后发低位。一开始做的时候没问清楚，这里卡了一下。


