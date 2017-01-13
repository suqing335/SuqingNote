
.config
CONFIG_BP_AUTO=y
# CONFIG_RK29_SUPPORT_MODEM is not set
arch/arm/mach-rk3188/board-rk3188-sdk.c
+#if defined (CONFIG_BP_AUTO)
+#include <linux/bp-auto.h>
+#endif
+#if defined(CONFIG_BP_AUTO)
+static int bp_io_init(void)
+{
+/*      rk30_mux_api_set(GPIO1A7_UART1RTSN_SPI0TXD_NAME, GPIO1A_GPIO1A7);
+        rk30_mux_api_set(GPIO1A6_UART1RTSN_SPI0TXD_NAME, GPIO1A_GPIO1A6);
+        rk30_mux_api_set(GPIO3D4_UART3SOUT_NAME, GPIO3D_GPIO3D4);
+        rk30_mux_api_set(GPIO2C0_LCDCDATA16_GPSCLK_HSADCCLKOUT_NAME, GPIO2C_GPIO2C0);
+        rk30_mux_api_set(GPIO2C1_LCDC1DATA17_SMCBLSN0_HSADCDATA6_NAME, GPIO2C_GPIO2C1);
+        rk30_mux_api_set(GPIO2C1_LCDC1DATA17_SMCBLSN0_HSADCDATA6_NAME, GPIO2C_GPIO2C1);
+*/
+        return 0;
+}
+
+static int bp_io_deinit(void)
+{
+       
+       return 0;
+}
+
+static int bp_id_get(void)
+{      
+       return BP_ID_U7501;   //internally 3G modem ID, defined in  include\linux\Bp-auto.h
+} 
+
+struct bp_platform_data bp_auto_info = {       
+       .bp_id = BP_ID_U7501,
+       .init_platform_hw = bp_io_init, 
+       .exit_platform_hw = bp_io_deinit,
+       .get_bp_id      = bp_id_get,    
+       .bp_power       = RK30_PIN1_PA7,        // 3g_power
+       .bp_en          = RK30_PIN0_PA6,        // 3g_en
+       .bp_reset       = RK30_PIN1_PA6,
+       .bp_usb_en      = BP_UNKNOW_DATA,       //W_disable
+       .bp_uart_en     = BP_UNKNOW_DATA,       //EINT9
+       .bp_wakeup_ap   = RK30_PIN3_PD4,        //
+       .ap_wakeup_bp   = RK30_PIN0_PD7,
+       .ap_ready       = BP_UNKNOW_DATA,       //
+       .bp_ready       = BP_UNKNOW_DATA,
+       .gpio_valid     = 0,            //if 1:gpio is define in bp_auto_info,if 0:is not use gpio in bp_auto_info
+};
+
+struct platform_device device_bp_auto = {      
+        .name = "bp-auto",     
+       .id = -1,       
+       .dev            = {
+               .platform_data = &bp_auto_info,
+       }       
+    };
+#endi
这里 gpio_valid = 0; 也就是这些gpio值不会用到，而直接用驱动中的值
******************************************************************************
这里主要的问题是上电时序问题，有一个使能脚延时5秒的
gpio_set_value(bp->ops->bp_en, GPIO_LOW);
msleep(1000);
gpio_set_value(bp->ops->bp_en, GPIO_HIGH);
msleep(5000);

上电时的电流比较大，需要去搜网电流大，瞬间电流。
