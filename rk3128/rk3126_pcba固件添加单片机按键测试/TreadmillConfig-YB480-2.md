// �����ˣ��Ź���
// �������ڣ�2016-7-12
//������������������������������������������������������������������������������������������������������
// ���»���˵����
// 1 ��U���д�����TreadmillConfigNoTouchNoWifi���ļ��С��ļ����������ִ�Сд��
// 2 ���ض����͵������ļ��ŵ�����ļ����¡�
// 3 ��U�̲�����ӱ��С�
// 4 �ε���ȫ���ء�
// 5 ��ϵͳ�ϵ硣
// 6 ϵͳ�Զ����������ļ���
// 7 ϵͳ��ʾ�����ݴ�����ɡ��󣬷��ϰ�ȫ���ء�������ɡ�
// 8 �����ý��棬��ͨ���ٶȣ����ٶȣ������鿴��������
// ע�⣺
//	1 ��ϵͳ���������ļ��Ĺ����У���������κβ�����
// 	2 U������ʹ��ʱ��Ҫɾ����TreadmillConfigNoTouchNoWifi���ļ���
//	3 ��TreadmillConfigNoTouchNoWifi���ļ��£�ֻ����һ����TreadmillConfig-xxxx.txt���ļ���
//������������������������������������������������������������������������������������������������������
// �����ļ�˵��
// 1 �ļ�����ǰ�벿�֡�TreadmillConfig�����ܸ�д����벿�ָ��ݾ������������
// 2 ��//����ʾ���в�������
// 3 ����Ӣ���ֶβ��ܱ���д�������г��⣩��
// 4 ��дʱȷ�����뷨����Ӣ��״̬��
// 5 ��������ո�
// 6 ����logo��uiѡ�ֻ��ѡ��һ��,���������á�//������
// 7 �����п��С�
// 8 ÿ���Իس��н�����
// 9 unit -> 0 ��ʾ����  0xa5 ��ʾӢ��
// 10 incline_reset -> 0 ��ʾͣ��ʱ��λ ->0x69��ʾͣ��ʱ����λ
// 11 ��Ҫ��������

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

/////////////////////////////////////////ֱ��������
option : 0x76
// b7: 0-x 1-�Ʋ� 1-ERP 1-220V 1-���� 1-�޸� 1-AD���� 1-���
overcurrent_dc : 20
force : 100

/////////////ֱ���޸�
pwml : 140
pwmm : 130
pwmh : 170
overload : 10

/////////////ֱ���и�
proption_dc : 0x6b5b
pwms : 50

steps_s0 : 2
steps_s1 : 10

/////////////////////////////////////////������Ƶ��
motor_res : 450
rated_voltage : 220
rated_freq : 5000
overcurrent_ac : 13
acc_time : 25
proption_ac : 0x2512
pole_ac : 4
