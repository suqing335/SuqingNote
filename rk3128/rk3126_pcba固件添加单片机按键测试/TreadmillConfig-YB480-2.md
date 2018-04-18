	// 创建人：张国华
	// 创建日期：2016-7-12
	//———————————————————————————————————————————————————
	// 更新机型说明：
	// 1 在U盘中创建“TreadmillConfigNoTouchNoWifi”文件夹。文件夹名称区分大小写。
	// 2 将特定机型的配置文件放到这个文件夹下。
	// 3 将U盘插入电子表中。
	// 4 拔掉安全开关。
	// 5 给系统上电。
	// 6 系统自动更新配置文件。
	// 7 系统提示“数据传送完成”后，放上安全开关。更新完成。
	// 8 在配置界面，可通过速度＋、速度－键，查看配置详情
	// 注意：
	//	1 在系统更新配置文件的过程中，切勿进行任何操作。
	// 	2 U盘正常使用时，要删除“TreadmillConfigNoTouchNoWifi”文件夹
	//	3 “TreadmillConfigNoTouchNoWifi”文件下，只能有一个“TreadmillConfig-xxxx.txt”文件。
	//———————————————————————————————————————————————————
	// 配置文件说明
	// 1 文件名的前半部分“TreadmillConfig”不能改写，后半部分根据具体机型命名。
	// 2 “//“标示本行不起作用
	// 3 所有英文字段不能被改写（机型行除外）。
	// 4 填写时确认输入法处于英文状态。
	// 5 可以输入空格
	// 6 对于logo、ui选项，只能选择一个,对其它项用“//“屏蔽
	// 7 可以有空行。
	// 8 每行以回车行结束。
	// 9 unit -> 0 表示公制  0xa5 表示英制
	// 10 incline_reset -> 0 表示停机时复位 ->0x69表示停机时不复位
	// 11 不要输入中文
	
	type  : YB480-2
	apk_version : 2016-07-28
	dzb_version : 2016-07-28
	ble_customer_id : 8
	ble_model_id : 480
	//logo  : mydo
	//logo:zhongyang
	//logo:monark
	logo:orient
	//ui : mydo
	ui:zhongyang
	
	min_speed  : 10
	max_display_speed : 160
	max_real_speed : 150
	max_incline : 15
	unit : 0
	//unit : 0xa5
	//incline_reset : 0
	incline_reset : 0x69
	
	
	start_key : 28
	stop_key : 35
	program_key : 14
	mode_key : 38
	speed_add_key : 33
	speed_sub_key : 34
	incline_add_key : 26
	incline_sub_key : 27
	hand_speed_add: 37
	hand_speed_sub: 36
	hand_incline_add : 29
	hand_incline_sub : 30
	
	home_key : 0
	return_key : 0
	mute_key : 0
	play_pause_key : 10
	pre_song_key : 9
	next_song_key : 11
	volume_sub_key : 0
	volume_add_key : 0
	quick_key : 0
	slow_key : 0
	switch_key : 13
	
	fan_key : 12
	
	speed_1_key : 30,19
	speed_2_key : 60,18
	speed_3_key : 90,17
	
	incline_1_key : 3,22
	incline_2_key : 6,21
	incline_3_key : 9,20
	
	
	driver : DC
	//driver : AC
	
	/////////////////////////////////////////直流驱动器
	option : 0x76
	// b7: 0-x 1-计步 1-ERP 1-220V 1-倒置 1-无感 1-AD扬升 1-光感
	overcurrent_dc : 20
	force : 100
	
	/////////////直流无感
	pwml : 140
	pwmm : 130
	pwmh : 170
	overload : 10
	
	/////////////直流有感
	proption_dc : 0x6b5b
	pwms : 50
	
	steps_s0 : 2
	steps_s1 : 10
	
	/////////////////////////////////////////交流变频器
	motor_res : 450
	rated_voltage : 220
	rated_freq : 5000
	overcurrent_ac : 13
	acc_time : 25
	proption_ac : 0x2512
	pole_ac : 4
