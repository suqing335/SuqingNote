不屏蔽这#define RKIO_I2C0_PHYS          0x20072000也可以
因为//{ .regs = (uint32_t)RKIO_I2C0_BASE, 0 }这个最终会去找这个物理地址，把这里屏蔽了就可以了
diff --git a/arch/arm/include/asm/arch-rk32xx/io-rk312X.h b/arch/arm/include/asm/arch-rk32xx/io-rk312X.h
index ff647d6..5a93a14 100755
--- a/arch/arm/include/asm/arch-rk32xx/io-rk312X.h
+++ b/arch/arm/include/asm/arch-rk32xx/io-rk312X.h
@@ -78,7 +78,7 @@
 #define RKIO_UART1_PHYS         0x20064000
 #define RKIO_UART2_PHYS         0x20068000
 #define RKIO_SARADC_PHYS        0x2006C000
-#define RKIO_I2C0_PHYS          0x20072000
+//#define RKIO_I2C0_PHYS          0x20072000
 #define RKIO_SPI_PHYS           0x20074000
 #define RKIO_DMAC_PHYS          0x20078000
 #define RKIO_GPIO0_PHYS         0x2007C000
diff --git a/drivers/i2c/rk_i2c.c b/drivers/i2c/rk_i2c.c
index c8812c4..b0d718f 100755
--- a/drivers/i2c/rk_i2c.c
+++ b/drivers/i2c/rk_i2c.c
@@ -125,7 +125,7 @@ struct rk_i2c {
 
 #ifdef CONFIG_I2C_MULTI_BUS
 static struct rk_i2c rki2c_base[I2C_BUS_MAX] = {
-       { .regs = (uint32_t)RKIO_I2C0_BASE, 0 },
+       //{ .regs = (uint32_t)RKIO_I2C0_BASE, 0 },
        { .regs = (uint32_t)RKIO_I2C1_BASE, 0 },
        { .regs = (uint32_t)RKIO_I2C2_BASE, 0 },
        { .regs = (uint32_t)RKIO_I2C3_BASE, 0 },
diff --git a/drivers/video/rockchip_fb.c b/drivers/video/rockchip_fb.c
index 4077ab0..8eb7a6d 100755
--- a/drivers/video/rockchip_fb.c
+++ b/drivers/video/rockchip_fb.c
@@ -146,7 +146,7 @@ void rk_fb_vidinfo_to_screen(vidinfo_t *vid, struct rk_screen *screen)
 void rk_backlight_ctrl(int brightness)
 {
 #ifdef CONFIG_RK_PWM_BL
-       //rk_pwm_bl_config(brightness); //change by cfj 20160617
+       rk_pwm_bl_config(brightness); //change by cfj 20160617
 #endif

