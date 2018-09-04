##查案selinux权限
	getenforce
##设置selinux权限
	setenforce

	diff --git a/system/core/init/init.c b/system/core/init/init.c
	old mode 100644
	new mode 100755
	index 2bad88d..90ca948
	--- a/system/core/init/init.c
	+++ b/system/core/init/init.c
	@@ -971,7 +971,7 @@ static bool selinux_is_enforcing(void)
	 {
	 #ifdef ALLOW_DISABLE_SELINUX
	     char tmp[PROP_VALUE_MAX];
	-
	+       return false;
	     if (property_get("ro.boot.selinux", tmp) == 0) {
	         /* Property is not set.  Assume enforcing */
	         return true;
