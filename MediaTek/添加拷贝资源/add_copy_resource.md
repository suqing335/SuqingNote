##添加拷贝资源的服务
	在MediaProvider.apk com.android.providers.media.MediaScannerReceiver
	这里接收开机完成广播后去启动CopyFileService

	服务等内容在add_copy_resource.patch

	这里需要注意的是：所拷贝的文件及文件夹必须有读的权限，不然file.listFiles()会因为没有权限而返回null

##添加拷贝资源到system/media/下
	添加此文件夹vendor/mediatek/proprietary/custom/sasin_S09/apps/MediaFile/
