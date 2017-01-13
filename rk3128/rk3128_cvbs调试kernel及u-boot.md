关键点
&fb {
       rockchip,disp-mode = <NO_DUAL>;//如果选NOE_DUAL则会不显示
//       rockchip,disp-policy = <DISPLAY_POLICY_BOX>;
       rockchip,uboot-logo-on = <1>;
};
&disp_timings {
        native-mode = <&pal_cvbs>;//这里只能选pal_cvbs,ntsc_cvbs，这里和tve驱动中的数组顺序有关。
	//status = "disabled";
};
&tve {
	status = "okay";
	test_mode = <0>;
};
 rockchip,disp-mode = <NO_DUAL>;//如果选NOE_DUAL就不显示
native-mode = <&pal_cvbs>;//这里只能选pal_cvbs,ntsc_cvbs，这里和tve驱动中的数组顺序有关。
static const struct fb_videomode rk3036_cvbs_mode[] = {
	/*name		refresh	xres	yres	pixclock	h_bp	h_fp	v_bp	v_fp	h_pw	v_pw			polariry				PorI		flag*/
/*	{"NTSC",        60,     720,    480,    27000000,       57,     19,     19,     0,      62,     3,      FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,   FB_VMODE_INTERLACED,    0},
	{"PAL",         50,     720,    576,    27000000,       69,     12,     19,     2,      63,     3,      FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,   FB_VMODE_INTERLACED,    0},
*/	{"NTSC",	60,	720,	480,	27000000,	43,	33,	19,	0,	62,	3,	FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,	FB_VMODE_INTERLACED,	0},
	{"PAL",		50,	720,	576,	27000000,	48,	33,	19,	2,	63,	3,	FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,	FB_VMODE_INTERLACED,	0},
};
现在默认是pal制式

u-boot 默认没有编译tve代码
需要在u-boot\include\configs\rk_default_config.h
打开#define CONFIG_PRODUCT_BOX
tve代码中显示模式默认为RGB，需要改成COLOR_YCBCR
diff --git a/drivers/video/rk3036_tve.c b/drivers/video/rk3036_tve.c
index a68d048..e31df06 100755
--- a/drivers/video/rk3036_tve.c
+++ b/drivers/video/rk3036_tve.c
@@ -192,7 +192,7 @@ static void rk3036_tve_init_panel(vidinfo_t *panel)
                printf("SCREEN_TVOUT\n");
        } else {
                panel->screen_type = SCREEN_TVOUT;
-               panel->color_mode = COLOR_RGB;
+               panel->color_mode = COLOR_YCBCR;
                printf("SCREEN_TVOUT\n");
        }
 
diff --git a/include/configs/rk_default_config.h b/include/configs/rk_default_config.h
index 5583559..b5636c1 100755
--- a/include/configs/rk_default_config.h
+++ b/include/configs/rk_default_config.h
@@ -392,6 +392,8 @@
 
 /* rk display module */
 #ifdef CONFIG_LCD
+/* add by cfj */
+#define CONFIG_PRODUCT_BOX
 
 #define CONFIG_RK_FB
 #ifndef CONFIG_PRODUCT_BOX


