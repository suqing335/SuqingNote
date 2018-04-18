##重要的文件	
	u-boot\include\configs\rk_default_config.h
	所有的编译选项都在这里，添加和删除都在这，如果注释一个编译选项要用 /* */， 不能用//这个注释

##打开
	/* add by cfj */
	#define CONFIG_PRODUCT_BOX
	在u-boot中打开tve

	在u-boot 打开cvbs需要把u-boot\drivers\video\rk3036_tve.h 中 把ntsc设为默认
	#define TVOUT_DEAULT TVOUT_CVBS_NTSC
	而且u-boot和kernel的制式要一致，不然图像会花
