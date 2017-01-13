
arch/arm/mach-rk3188/board-rk3188-sdk.c
.pre_div = 30 * 1000,  // pwm output clk: 30k; 频率太低，调节屏的亮度时会闪屏
////添加778转换IC
+#if defined (CONFIG_TC358768_RGB2MIPI)
+//#include "../../../drivers/video/display/transmitter/mipi_dsi.h"
+#include "../../../drivers/video/rockchip/transmitter/mipi_dsi.h"
+#define TC358768_RST_PIN                               RK30_PIN0_PB4 //RK30_PIN1_PD7
+#define TC358768_RST_PIN_EFFECT_VALUE   GPIO_LOW
+#define TC358768_RST_PIN_MUX_MODE          0 //GPIO1D_GPIO1D7
+#define TC358768_RST_PIN_MUX_NAME      NULL //GPIO1D7_CIF1CLKOUT_NAME
+
+#define TC358768_POWER_ON_PIN                  RK30_PIN6_PB0
+#define TC358768_POWER_ON_LEVEL                GPIO_HIGH
+#define TC358768_POWER_PIN_MUX_MODE        0
+#define TC358768_POWER_PIN_MUX_NAME     NULL
+
+struct tc358768_t tc358768_platform_data = {
+       .id = 0x4401,
+       .reset = {
+               .reset_pin = TC358768_RST_PIN,
+               //.mux_name = TC358768_RST_PIN_MUX_NAME,
+               //.mux_mode = TC358768_RST_PIN_MUX_MODE,
+               .effect_value = TC358768_RST_PIN_EFFECT_VALUE,
+       },
+       .vddio = {
+               .enable_pin = INVALID_GPIO, //TC358768_POWER_ON_PIN,
+               //.mux_name = TC358768_POWER_PIN_MUX_NAME,
+               //.mux_mode = TC358768_POWER_PIN_MUX_MODE,
+               .effect_value = TC358768_POWER_ON_LEVEL,
+       },
+
+       .vddc = {
+               .enable_pin = INVALID_GPIO,
+       },
+       
+       .vdd_mipi = {
+               .enable_pin = INVALID_GPIO,
+       },
+
+};
+#endif
////在i2c2列表注册
+#if defined (CONFIG_TC358768_RGB2MIPI)
+{
+       .type           = "tc358768",
+       .addr           = 0x0e,
+       .flags          = 0,
+       .platform_data = &tc358768_platform_data,
+},
+#endif
+#if defined (CONFIG_GS_MMA8452)
+       {
+               .type           = "gs_mma8452",
+               .addr           = 0x1d,
+               .flags          = 0,
+               .irq            = MMA8452_INT_PIN,
+               .platform_data = &mma8452_info,
+       },
+#endif
drivers/video/rockchip/screen/lcd_tl5001_mipi.c
#define DCLK_POL                1 //这个会影响屏的显示效果，如果反了可能会有红点
drivers/video/rockchip/screen/rk_screen.c 中的set_lcd_info函数要注销，不然和lcd_tl5001_mipi.c中的冲突了
详细的初始化参数在TC358768(A)XBG_778XBG_DPI-DSI_Tv42p_nm1 - 副本.xls表中，有设置初始化778转换IC的参数，也有屏的初始化参数。

drivers/video/rockchip/transmitter/tc358768.c
drivers/video/rockchip/transmitter/mipi_dsi.c
drivers/video/rockchip/transmitter/mipi_dsi.h
要做一些相应的修改，就是编译时有些变量等重定义等等。
