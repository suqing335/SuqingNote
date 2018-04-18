##关键点
	 &disp_timings {
	        native-mode = <&timing0>;
	        status = "disabled";//把这个关闭，不然lvds没有输出
	 };

###这个不确定，是否一定要ONE_DUAL
	&fb {
	        rockchip,disp-mode = <ONE_DUAL>;
	        rockchip,uboot-logo-on = <1>;
	 };
###最好把cvbs关闭，因为插cvbs时，lvds时钟信号会减小很多，而且和cvbs也是有冲突的，我们是一定要关闭disp_timings ，cvbs是一定要打开
	 &tve {
	        status = "disabled";
	        test_mode = <0>;
	 };

###时钟太高会导致数据有点乱，会有水纹出来，把时钟降至55时就好了
	timing0: timing0 {
					screen-type = <SCREEN_LVDS>;
					lvds-format = <LVDS_8BIT_1>;
					out-face    = <OUT_P888>;
					/* Min   Typ   Max Unit
					 * Clock Frequency fclk  44.9  51.2  63 MHz
					 */
					clock-frequency = <55000000>;
					hactive = <1024>;			  /* Horizontal display area thd 1024       DCLK 			*/
					vactive = <600>;			  /* Vertical display area tvd   600 		H 				*/
					hback-porch = <90>;		  	  /* HS Width +Back Porch   160  160   160  DCLK (Thw+ thbp)*/
					hfront-porch = <160>;		  /* HS front porch thfp    16   160   216  DCLK 		 	*/
					vback-porch = <13>;			  /* VS front porch tvfp 	1 	 12    127  H  			 	*/
					vfront-porch = <12>;		  /* VS Width+Back Porch    23   23    23   H (Tvw+ tvbp)	*/
					hsync-len = <70>;			  /* HS Pulse Width thw 	1 	  -    140  DCLK 		 	*/
					vsync-len = <10>;			  /* VS Pulse Width tvw 	1 	  - 	20  H	 		 	*/
					hsync-active = <0>;
					vsync-active = <0>;
					de-active = <0>;
					pixelclk-active = <0>;
					swap-rb = <0>;
					swap-rg = <0>;
					swap-gb = <0>;
	};

