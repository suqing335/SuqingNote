林工，你好。rk3126 主控只有两路pwm, pwm0，pwm1，我们想用其中一路去读下控输给主控的pwm波形，其中最小脉冲宽度只有0.8us。如果可以实现，能帮忙配一下dts信息，以及提示一下驱动要修改的地方吗？
#5由 FAE 林崇森 更新于 8 天 之前

Comment
IR是使用PWM来作硬件时钟的，PWM还是一样还是pwm输出
drivers/input/remotectl/rockchip_pwm_remotectl.c
#6由 FAE 林崇森 更新于 8 天 之前

Comment
remotectl: pwm@20050030 { //pwm3用20050030 pwm2为20050020 pwm1为20050010
 compatible = "rockchip,remotectl-pwm";
 reg = <0x20050030 0x10>; //与上面对应 pwm3用20050030 pwm2为20050020 pwm1为20050010
 #pwm-cells = <2>;
 pinctrl-names = "default";
 pinctrl-0 = <&pwm3_pin>; //对应改pwmX_pin
 clocks = <&clk_gates7 10>;
 clock-names = "pclk_pwm";
 remote_pwm_id = <3>; //对应3为pwm3 2为pwm2
 interrupts = <GIC_SPI 30 IRQ_TYPE_LEVEL_HIGH>;
 status = "okay";
 };
#7由 展 网 更新于 6 天 之前

Comment
林工，你好。我用其他的pwm，dts中的中断ID不用改吗？驱动中probe函数有获取中断号的动作。
irq = platform_get_irq(pdev, 0);
if (ret < 0) {
dev_err(&pdev->dev, "cannot find IRQ\n");
return ret;
}
#8由 FAE 林崇森 更新于 6 天 之前

Comment
这中断与pwm没有关系的。
如只是开启pwm的参考drivers/video/backlight/pwm_bl.c设一下dts参数，现对应调用pwm_enable就可以
IR是使用PWM来作硬件时钟的，PWM还是一样作为输出。
#9由 展 网 更新于 6 天 之前

Comment
但这样就有一个疑问
drivers/input/remotectl/rockchip_pwm_remotectl.c 中probe函数获取的中断号是怎么来的呢？ 我在3126上打印过这个irq是60，不知道3128上是否一样。
irq = platform_get_irq(pdev, 0);
if (ret < 0) {
dev_err(&pdev->dev, "cannot find IRQ\n");
return ret;
}
rk312x.dtsi中有
remotectl: pwm@20050030 {
compatible = "rockchip,remotectl-pwm";
reg = <0x20050030 0x10>;
#pwm-cells = <2>;
pinctrl-names = "default";
pinctrl-0 = <&pwm3_pin>;
clocks = <&clk_gates7 10>;
clock-names = "pclk_pwm";
remote_pwm_id = <3>;
interrupts = <GIC_SPI 30 IRQ_TYPE_LEVEL_HIGH>;
status = "okay";
};
rk3128-box.dts中也没有关于中断的东西
&remotectl {
handle_cpu_id = <1>;
ir_key1{
rockchip,usercode = <0x4040>;
rockchip,key_table =
<0xf2 KEY_REPLY>,
<0xba KEY_BACK>,
<0xf4 KEY_UP>,
<0xf1 KEY_DOWN>,
<0xef KEY_LEFT>,
<0xee KEY_RIGHT>,
<0xbd KEY_HOME>,
<0xea KEY_VOLUMEUP>,
<0xe3 KEY_VOLUMEDOWN>,
<0xe2 KEY_SEARCH>,
<0xb2 KEY_POWER>,
<0xbc KEY_MUTE>,
<0xec KEY_MENU>,
<0xbf 0x190>,
<0xe0 0x191>,
<0xe1 0x192>,
<0xe9 183>,
<0xe6 248>,
<0xe8 185>,
<0xe7 186>,
<0xf0 388>,
<0xbe 0x175>;
};
ir_key2{
rockchip,usercode = <0xff00>;
rockchip,key_table =
<0xf9 KEY_HOME>,
<0xbf KEY_BACK>,
<0xfb KEY_MENU>,
<0xaa KEY_REPLY>,
<0xb9 KEY_UP>,
<0xe9 KEY_DOWN>,
<0xb8 KEY_LEFT>,
<0xea KEY_RIGHT>,
<0xeb KEY_VOLUMEDOWN>,
<0xef KEY_VOLUMEUP>,
<0xf7 KEY_MUTE>,
<0xe7 KEY_POWER>,
<0xfc KEY_POWER>,
<0xa9 KEY_VOLUMEDOWN>,
<0xa8 KEY_VOLUMEDOWN>,
<0xe0 KEY_VOLUMEDOWN>,
<0xa5 KEY_VOLUMEDOWN>,
<0xab 183>,
<0xb7 388>,
<0xf8 184>,
<0xaf 185>,
<0xed KEY_VOLUMEDOWN>,
<0xee 186>,
<0xb3 KEY_VOLUMEDOWN>,
<0xf1 KEY_VOLUMEDOWN>,
<0xf2 KEY_VOLUMEDOWN>,
<0xf3 KEY_SEARCH>,
<0xb4 KEY_VOLUMEDOWN>,
<0xbe KEY_SEARCH>;
};
ir_key3{
rockchip,usercode = <0x1dcc>;
rockchip,key_table =
<0xee KEY_REPLY>,
<0xf0 KEY_BACK>,
<0xf8 KEY_UP>,
<0xbb KEY_DOWN>,
<0xef KEY_LEFT>,
<0xed KEY_RIGHT>,
<0xfc KEY_HOME>,
<0xf1 KEY_VOLUMEUP>,
<0xfd KEY_VOLUMEDOWN>,
<0xb7 KEY_SEARCH>,
<0xff KEY_POWER>,
<0xf3 KEY_MUTE>,
<0xbf KEY_MENU>,
<0xf9 0x191>,
<0xf5 0x192>,
<0xb3 388>,
<0xbe KEY_1>,
<0xba KEY_2>,
<0xb2 KEY_3>,
<0xbd KEY_4>,
<0xf9 KEY_5>,
<0xb1 KEY_6>,
<0xfc KEY_7>,
<0xf8 KEY_8>,
<0xb0 KEY_9>,
<0xb6 KEY_0>,
<0xb5 KEY_BACKSPACE>;
};
};
只有rk312x.dtsi中有设中断ID，其他地方都没有关于中断的设置了，所以我就不明白rockchip_pwm_remotectl.c代码中获取的irq是哪来的
#10由 FAE 林崇森 更新于 6 天 之前

Comment
那irq与pwm是无关的！
此irq是用于IR时序分析用的。pwm不获取中断,只是用于作clk输出

来源： https://redmine.rockchip.com.cn/issues/110309

=======================================================================

remotectl: pwm@20050030 { //pwm3用20050030 pwm2为20050020 pwm1为20050010
 compatible = "rockchip,remotectl-pwm";
 reg = <0x20050030 0x10>; //与上面对应 pwm3用20050030 pwm2为20050020 pwm1为20050010
 #pwm-cells = <2>;
 pinctrl-names = "default";
 pinctrl-0 = <&pwm3_pin>; //对应改pwmX_pin
 clocks = <&clk_gates7 10>;
 clock-names = "pclk_pwm";
 remote_pwm_id = <3>; //对应3为pwm3 2为pwm2
 interrupts = <GIC_SPI 30 IRQ_TYPE_LEVEL_HIGH>;//中断号不变
 status = "okay";
 };
例如pwm0:
arch\arm\boot\dts\rk312x.dtsi
	capturectl: pwm@20050000 {
		compatible = "rockchip,capturectl-pwm";
		reg = <0x20050000 0x10>;
		#pwm-cells = <2>;
		pinctrl-names = "default";
		pinctrl-0 = <&pwm0_pin>;
		clocks = <&clk_gates7 10>;
		clock-names = "pclk_pwm";
		remote_pwm_id = <0>;
		interrupts = <GIC_SPI 30 IRQ_TYPE_LEVEL_HIGH>;
		status = "disabled";
	};
arch\arm\boot\dts\rk3128-86v.dts
&capturectl {
	handle_cpu_id = <2>;
	status = "okay";
};
附件为pwm捕捉的示例代码
 #define IRQ_TYPE_NONE           0x00000000     未指明类型
    #define IRQ_TYPE_EDGE_RISING    0x00000001     上升沿触发
    #define IRQ_TYPE_EDGE_FALLING   0x00000002     下降沿触发
    #define IRQ_TYPE_EDGE_BOTH      (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING)
    #define IRQ_TYPE_LEVEL_HIGH     0x00000004     高电平触发
    #define IRQ_TYPE_LEVEL_LOW      0x00000008     低电平触发
    #define IRQ_TYPE_SENSE_MASK     0x0000000f    
    #define IRQ_TYPE_PROBE          0x00000010

来源： http://blog.sina.com.cn/s/blog_6fafa4a20101bz6x.html
