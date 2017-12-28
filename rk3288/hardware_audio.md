#Hardware audio

由于用Dsp做codec，所以以前的那套框架没用了，只能在上面加一些东西。

注册一个es8316

	&i2c2 {
		es8316: es8316@10 {
			compatible = "es8316";
			//spk-gpio = <&gpio0 GPIO_B3 GPIO_ACTIVE_HIGH>;
			//hp-con-gpio = <&gpio0 GPIO_B1 GPIO_ACTIVE_HIGH>;
			//hp-det-gpio = <&gpio7 GPIO_B4 IRQ_TYPE_EDGE_FALLING>;
			reg = <0x10>;
		};
	};

这个驱动i2c不通也可以成功注册codec，产生codec 节点 RKES8316
只要有节点，主控的i2s和dsp的i2s就是通的。

rk3288默认使用tinyalsa_hal,这个库不带通话功能，要使用legacy_hal库


##软件设计思路是
上层切换不同的route 在hardware层把相应的配置写入dsp内
烧入dsp flash内的配置保存一份在base_config_array数组中，使上层有一份dsp arm的寄存器数据，这样写入其他配置时比较这个数组，不同的值写到dsp内，并保存在base_config_array中。（这样就要求每次重启，dsp必须复位,重新加载flash内的配置）

并使用了一个当前指针，和一个old指针，实现了和codec一样的逻辑.以后只需要在不同route中加载不同的配置，加入不同的配置头文件就可以了。

因为有了base_config_array做一个dsp arm 映射一样，所以上层写数组很快就结束了。

具体的可以看源码和补丁。

我在zl380tw中把i2c写的函数改了，指定了i2c速度 300 。





