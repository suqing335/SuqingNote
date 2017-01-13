framework/base/packages/SystemUI/src/com/android/systemui/statusbar/phone/PhoneStatusBarPolicy.java
--- a/packages/SystemUI/src/com/android/systemui/statusbar/phone/PhoneStatusBarPolicy.java
+++ b/packages/SystemUI/src/com/android/systemui/statusbar/phone/PhoneStatusBarPolicy.java
@@ -126,7 +126,7 @@ public class PhoneStatusBarPolicy {
         filter.addAction(TelephonyIntents.ACTION_SIM_STATE_CHANGED);
         filter.addAction(TelecomManager.ACTION_CURRENT_TTY_MODE_CHANGED);
         filter.addAction(Intent.ACTION_USER_SWITCHED);
-               filter.addAction(Intent.ACTION_HEADSET_PLUG);
+               //filter.addAction(Intent.ACTION_HEADSET_PLUG);//delete by cfj ///delete headset Icon
         mContext.registerReceiver(mIntentReceiver, filter, null, mHandler);
*******************不监听这个广播就可以了******************
