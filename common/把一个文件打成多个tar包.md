### 打包命令
	tar jcf - rk312x_5.1/ | split -b 4000m -d -a 2 - rk312x_5.1_20180419.tar.bz

生成rk312x_5.1_20180419.tar.bz00 rk312x_5.1_20180419.tar.bz01等文件

###解包命令
	cat rk312x_5.1_20180419.tar.bz0* | tar -jx

###split命令详解
	split [OPTION]... [INPUT [PREFIX]]
	  
	-a N：生成长度为N的后缀，默认N=2
	-b N：每个小文件的N，即按文件大小切分文件。支持K,M,G,T(换算单位1024)或KB,MB,GB(换算单位1000)等，默认单位为字节
	-l N：每个小文件中有N行，即按行切分文件
	-d N：指定生成数值格式的后缀替代默认的字母后缀，数值从N开始，默认为0。例如两位长度的后缀01/02/03
	--additional-suffix=string：为每个小文件追加额外的后缀，例如加上".log"。有些老版本不支持该选项，在CentOS 7.2上已支持。
	  
	INPUT：指定待切分的输入文件，如要切分标准输入，则使用"-"
	PREFIX：指定小文件的前缀，如果未指定，则默认为"x"
	例如，将/etc/fstab按行切分，每5行切分一次，并指定小文件的前缀为"fs_"，后缀为数值后缀，且后缀长度为2。
	
	[root@linuxidc ~]# split -l 5 -d -a 2 /etc/fstab fs_
	
	[root@linuxidc ~]# ls
	fs_00  fs_01  fs_02

###split详解
	split
	语法：split [--help][--version][-][-l][-b][-C][-d][-a][要切割的文件][输出文件名]
	--version 显示版本信息
	- 或者-l,指定每多少行切割一次，用于文本文件分割
	-b 指定切割文件大小,单位m或k
	-C 与-b类似，但尽量维持每行完整性
	-d 使用数字而不是字母作为后缀名
	-a 指定后缀名的长度，默认为2位

	示例1
	将temp.tar.gz包按每个5M大小切割
	split -b 5m temp.tar.gz  temp.tar.gz.

	示例2
	使用| 管道将打包分割动作合并
	tar -zcf - temp | split -b 5m - temp.tar.gz.

###合并
	cat temp.tar.gz.* > temp.tar.gz

###合并并解压
	cat temp.tar.gz.*  | tar -zxv

