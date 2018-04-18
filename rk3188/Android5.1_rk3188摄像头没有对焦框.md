
	kernel/include/linux/videodev2.h
	-#define V4L2_CID_CAMERA_CLASS_BASE_ROCK                (V4L2_CID_CAMERA_CLASS_BASE + 30)
	+//#define V4L2_CID_CAMERA_CLASS_BASE_ROCK              (V4L2_CID_CAMERA_CLASS_BASE + 30)
	+//change by cfj (camera zoom)
	+#define V4L2_CID_CAMERA_CLASS_BASE_ROCK                (V4L2_CID_CAMERA_CLASS_BASE + 40)
	V4L2_CID_CAMERA_CLASS_BASE_ROCK这是一个基值，这个值不对，好几个值都有问题
	V4L2_CID_FOCUSZONE 这个值就和Android Hardware中的值对不上，hardware中获取的头文件在bionic/libc中 这里V4L2_CID_CAMERA_CLASS_BASE_ROCK这个值变了，所以在CameraHal/CameraSocAdapter.cpp中
	void CameraSOCAdapter::initDefaultParameters(int camFd) 初始化参数的时候没有获取到
	focus.id = V4L2_CID_FOCUSZONE;
	// focus area settings
	        if (!ioctl(mCamFd, VIDIOC_QUERYCTRL, &focus)) { //change by cfj
	                params.set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS,"1");
	        } else {
	                params.set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS,"0");
	        }
	在kernel 3.10中没有这个问题。
