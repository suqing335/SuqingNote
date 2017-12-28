#u-boot 降频
使用rk818 pmu 一定要加电池，不然需要在bat+接一个4.2V，必须能输出大电流的。
rk818默认是出bat处取电，dc只是辅助，所以会导致一个情况，uboot时电流过大会导致跑不起来，uboot logo都没显，如果bat+输出的电流太小会进入限流模式，导致cpu电流很小，开不起来。

解决的方式是给arm降频，模式cpu 600M， ddr 300M
把cpu降至408M

    diff --git a/arch/arm/cpu/armv7/rk32xx/clock-rk3288.c b/arch/arm/cpu/armv7/rk32xx/clock-rk3288.c
	index 62ec8cf..0207a0c 100755
	--- a/arch/arm/cpu/armv7/rk32xx/clock-rk3288.c
	+++ b/arch/arm/cpu/armv7/rk32xx/clock-rk3288.c
	@@ -13,7 +13,8 @@ DECLARE_GLOBAL_DATA_PTR;
	 
	 
	 /* ARM/General/Codec pll freq config */
	-#define CONFIG_RKCLK_APLL_FREQ         600 /* MHZ */
	+//#define CONFIG_RKCLK_APLL_FREQ               600 /* MHZ */
	+#define CONFIG_RKCLK_APLL_FREQ         408 /* MHZ */
	 
	 #ifdef CONFIG_PRODUCT_BOX
	 #define CONFIG_RKCLK_GPLL_FREQ         300 /* MHZ */
	@@ -154,6 +155,7 @@ static struct pll_clk_set apll_clks[] = {
        _APLL_SET_CLKS(1008000,1, 84, 2,        1, 4, 2,                2, 4, 4),
        _APLL_SET_CLKS(816000, 1, 68, 2,        1, 4, 2,                2, 4, 4),
        _APLL_SET_CLKS(600000, 1, 50, 2,        1, 4, 2,                2, 4, 4),
	+       _APLL_SET_CLKS(408000, 1, 30, 2,        1, 4, 2,                2, 4, 4),
	 };
 
 

 
 
