在packages/apps/Settings/src/com/android/settings/DataUsageSummary.java
主要的问题是在于，默认的那张卡，和读到的卡不一样，导致这个图标没有显示出来。
-                        setMobileDataEnabled(getSubId(currentTab), true);
+                        setMobileDataEnabled(getSubId(mCurrentTab) -1, true);

主要在这几个函数
private boolean isMobileDataAvailable(int subId)
详细情况可以参考附件中的java文件

