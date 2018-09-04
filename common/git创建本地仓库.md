##创建git用户
	adduser git

##到git家目录
	cd /home/git/

##创建仓库
	mkdir CompanyAppCode

	cd CompanyAppCode
##把仓库链接到其他盘上
	ln -s /Backup_disk/jhf/CompanyAppCode/repositories repositories

	cd repositories

##初始化一个本地仓库
	git --bare init apptest.git

##在客户端链接这个仓库 add the remote

	git remote add origin git@192.168.0.107:/Backup_disk/jhf/CompanyAppCode/repositories/apptest.git

	---git remote add origin URL_TO_YOUR_REPO---

##把客户端的提交放到远程仓库中
##push the exising git reop to the remote
	git push -u origin --all

###效果如下
	suqin@DESKTOP-6CEBA4H MINGW64 /d/studiospace/serverTest (master)
	$ git push -u origin --all
	git@192.168.0.107's password:
	Counting objects: 90, done.
	Delta compression using up to 4 threads.
	Compressing objects: 100% (72/72), done.
	Writing objects: 100% (90/90), 151.64 KiB | 2.08 MiB/s, done.
	Total 90 (delta 2), reused 0 (delta 0)
	To 192.168.0.107:/Backup_disk/jhf/CompanyAppCode/repositories/apptest.git
	 * [new branch]      master -> master
	Branch 'master' set up to track remote branch 'master' from 'origin'.
	
	suqin@DESKTOP-6CEBA4H MINGW64 /d/studiospace/serverTest (master)
	$ git branch
	* master
	
	suqin@DESKTOP-6CEBA4H MINGW64 /d/studiospace/serverTest (master)
	$ git branch -a
	* master
	  remotes/origin/master
	
	suqin@DESKTOP-6CEBA4H MINGW64 /d/studiospace/serverTest (master)
	$


##pstn4Gapp
	git remote add origin git@192.168.0.107:/Backup_disk/jhf/CompanyAppCode/repositories/pstn4Gapp.git
	git push -u origin --all


##pstnapp
	git remote add origin git@192.168.0.107:/Backup_disk/jhf/CompanyAppCode/repositories/pstnapp.git
	git push -u origin --all

##EntranceGuardSystem
	git remote add origin git@192.168.0.107:/Backup_disk/jhf/CompanyAppCode/repositories/EntranceGuardSystem.git/

##VoipS09
	git remote add origin git@192.168.0.107:/Backup_disk/jhf/CompanyAppCode/repositories/VoipS09.git	

##SipDial2
	git remote add origin git@192.168.0.107:/Backup_disk/jhf/CompanyAppCode/repositories/SipDial2.git



	
