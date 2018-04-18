
	hardware/rockchip/camera/CameraHal/AppMsgNotifier.cpp
	int AppMsgNotifier::captureEncProcessPicture(FramInfo_s* frame){
	     int ret = 0;
	        int jpeg_w,jpeg_h,i;
	        unsigned int pictureSize;
	@@ -986,8 +999,9 @@ int AppMsgNotifier::captureEncProcessPicture(FramInfo_s* frame){
	                encodetype = JPEGENC_YUV420_SP;
	                pictureSize = ((jpeg_w+15)&(~15)) * ((jpeg_h+15)&(~15)) * 3/2;
	        }
	-       jpeg_w = (jpeg_w+15)&(~15);     
	-       jpeg_h = (jpeg_h+15)&(~15);     
	+       /*解决2592x1944分辨率多出8行数据问题*/
	+       //jpeg_w = (jpeg_w+15)&(~15);   
	+       //jpeg_h = (jpeg_h+15)&(~15);

pictureSize 是用来分配内存的，可以大于真实的图片大小
(jpeg_h+15)&(~15);这个公式对于不是16倍数的数，输出的结果会加8。改变了原来分辨率的大小
