	--- a/telephony/java/android/telephony/SignalStrength.java
	+++ b/telephony/java/android/telephony/SignalStrength.java
	@@ -377,6 +377,7 @@ public class SignalStrength implements Parcelable {
	         mLteRsrq = ((mLteRsrq >= 3) && (mLteRsrq <= 20)) ? -mLteRsrq : SignalStrength.INVALID;
	         mLteRssnr = ((mLteRssnr >= -200) && (mLteRssnr <= 300)) ? mLteRssnr
	                 : SignalStrength.INVALID;
	+       mLteRssnr = SignalStrength.INVALID;
	         // Cqi no change
	         if (DBG) log("Signal after validate=" + this);
	}
Android 系统信号处理默认走的是4G LTE流程，不管模块是3G的还是LTE的，信号格显示永远只有一格，这是由于Android系统上层默认把参数mLteRssnr设置成-1导致的
在validateInput()函数中，把mLteRssnr的值置成无效
3G_for_RockChipSDK参考说明.pdf
