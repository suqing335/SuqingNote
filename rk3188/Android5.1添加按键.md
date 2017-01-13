在kernel/include/linux/input.h 中添加对应的键值
diff --git a/include/linux/input.h b/include/linux/input.h
old mode 100644
new mode 100755
index a207923..16a9b09
--- a/include/linux/input.h
+++ b/include/linux/input.h
@@ -441,6 +441,9 @@ struct input_keymap_entry {
 #define KEY_WIMAX              246
 #define KEY_RFKILL             247     /* Key that controls all radios */
 
+#define KEY_ENDCALL            248
+#define KEY_PAD_IN             249
+#define KEY_PAD_OUT            250
 /* Code 255 is reserved for special needs of AT keyboard driver */
 
 #define BTN_MISC               0x100

在kernel/arch/arm/mach-rk3188/board-rk3188-sdk.c 中注册一个按键
static struct rk29_keys_button key_button[] = {
{
                .desc   = "padout",
                .code   = KEY_PAD_OUT,//KEY_VOLUMEDOWN,
		.adc_value      = 900,
                .gpio   = INVALID_GPIO,
                .active_low = PRESS_LEV_LOW,
        },
};
在kernel/drivers/headset_observe/rk_headset_irq_hook_adc.c注册成按键，并上报
input_set_capability(headset->input_dev, EV_KEY, KEY_PAD_IN);//pad in
input_set_capability(headset->input_dev, EV_KEY, KEY_PAD_OUT);//pad out
				input_report_key(headset_info->input_dev,KEY_PAD_OUT, 1);
				input_sync(headset_info->input_dev);
				mdelay(50);
				input_report_key(headset_info->input_dev,KEY_PAD_OUT, 0);
				input_sync(headset_info->input_dev);

在framework/base/core/java/android/view/KeyEvent.java
diff --git a/core/java/android/view/KeyEvent.java b/core/java/android/view/KeyEvent.java
index f3b69ae..477d96c 100755
--- a/core/java/android/view/KeyEvent.java
+++ b/core/java/android/view/KeyEvent.java
@@ -772,9 +772,14 @@ public class KeyEvent extends InputEvent implements Parcelable {
     public static final int KEYCODE_TV_MEDIA_PLAY = 273;
     public static final int KEYCODE_TV_MEDIA_PAUSE = 274;
 //$_rbox_$_modify_$ end
-
-    private static final int LAST_KEYCODE = KEYCODE_TV_MEDIA_PAUSE;
-
+       //add by cfj
+    public static final int KEYCODE_PAD_IN = 275;
+    public static final int KEYCODE_PAD_OUT = 276;
+       
+       
+    //private static final int LAST_KEYCODE = KEYCODE_TV_MEDIA_PAUSE;
+    private static final int LAST_KEYCODE = KEYCODE_PAD_OUT;
+    
     // NOTE: If you add a new keycode here you must also add it to:
     //  isSystem()
     //  isWakeKey()
在framework/base/core/res/res/values/attrs.xml
diff --git a/core/res/res/values/attrs.xml b/core/res/res/values/attrs.xml
old mode 100644
new mode 100755
index 75157be..1106e23
--- a/core/res/res/values/attrs.xml
+++ b/core/res/res/values/attrs.xml
@@ -1792,6 +1792,10 @@
         <enum name="KEYCODE_TV_MEDIA_PLAY" value="273" />
         <enum name="KEYCODE_TV_MEDIA_PAUSE" value="274" />
 <!--$_rbox_$_modify_$_end-->
+<!--add by cfj-->
+       <enum name="KEYCODE_PAD_IN" value="275" />
+       <enum name="KEYCODE_PAD_OUT" value="276" />
+<!--add by end-->
     </attr>
 
     <!-- ***************************************************************** -->
在framework/base/data/keyboards/
diff --git a/data/keyboards/Generic.kl b/data/keyboards/Generic.kl
old mode 100644
new mode 100755
index f10ba96..4d8c7e5
--- a/data/keyboards/Generic.kl
+++ b/data/keyboards/Generic.kl
@@ -19,7 +19,9 @@
 # Do not edit the generic key layout to support a specific keyboard; instead, create
 # a new key layout file with the required keyboard configuration.
 #
-
+key 248   ENDCALL
+key 249   PAD_IN
+key 250   PAD_OUT
 key 1     ESCAPE
 key 2     1
 key 3     2
diff --git a/data/keyboards/qwerty.kl b/data/keyboards/qwerty.kl
old mode 100644
new mode 100755
index 58bf654..2866045
--- a/data/keyboards/qwerty.kl
+++ b/data/keyboards/qwerty.kl
@@ -20,6 +20,8 @@
 #
 
 key 399   GRAVE
+key 249   PAD_IN
+key 250   PAD_OUT
 key 2     1
 key 3     2
 key 4     3
在framework/base/policy/src/com/android/internal/policy/impl/PhoneWindowManager.java
diff --git a/policy/src/com/android/internal/policy/impl/PhoneWindowManager.java b/policy/src/com/android/internal/policy/impl/PhoneWindowManager.java
index 1b8c11b..0f29157 100755
--- a/policy/src/com/android/internal/policy/impl/PhoneWindowManager.java
+++ b/policy/src/com/android/internal/policy/impl/PhoneWindowManager.java
@@ -5065,6 +5066,30 @@ public class PhoneWindowManager implements WindowManagerPolicy {
                     msg.sendToTarget();
                 }
             }
+               case KeyEvent.KEYCODE_PAD_IN:{
+                       /*if (down) {
+                               Intent intent = new Intent();
+                               intent.setAction("android.intent.action.PADSTATUS");
+                               intent.putExtra("Padstatus", "PAD_IN"); 
+                               mContext.sendBroadcast(intent);
+                               Log.d(TAG, "KeyEvent.KEYCODE_PAD_IN start");
+                       }*/
+                       result &= ~ACTION_PASS_TO_USER;
+                       Log.d(TAG, "KeyEvent keycode : "+event.getKeyCode()+" key : "+event.getAction());
+                       break;
+               }
+               case KeyEvent.KEYCODE_PAD_OUT:{
+                       /*if (down) {
+                               Intent intent = new Intent();
+                               intent.setAction("android.intent.action.PADSTATUS");
+                               intent.putExtra("Padstatus", "PAD_OUT"); 
+                               mContext.sendBroadcast(intent);
+                               Log.d(TAG, "KeyEvent.KEYCODE_PAD_OUT start");
+                       }*/
+                       result &= ~ACTION_PASS_TO_USER;
+                       Log.d(TAG, "KeyEvent keycode : "+event.getKeyCode()+" key : "+event.getAction());
+                       break;
+               }
         }
 
         if (useHapticFeedback) {
在frameworks/native/include/
diff --git a/include/android/keycodes.h b/include/android/keycodes.h
old mode 100644
new mode 100755
index ff998df..d9f093c
--- a/include/android/keycodes.h
+++ b/include/android/keycodes.h
@@ -307,7 +307,9 @@ enum {
     AKEYCODE_TV_KEYMOUSE_UP = 262,
     AKEYCODE_TV_KEYMOUSE_DOWN = 263,
     AKEYCODE_TV_KEYMOUSE_MODE_SWITCH = 264,
-    AKEYCODE_HELP            = 259
+    AKEYCODE_HELP            = 259,
+    AKEYCODE_PAD_IN            = 275,
+    AKEYCODE_PAD_OUT           = 276
 
     // NOTE: If you add a new keycode here you must also add it to several other files.
     //       Refer to frameworks/base/core/java/android/view/KeyEvent.java for the full list.
diff --git a/include/input/InputEventLabels.h b/include/input/InputEventLabels.h
old mode 100644
new mode 100755
index d7a06dd..9a7339a
--- a/include/input/InputEventLabels.h
+++ b/include/input/InputEventLabels.h
@@ -304,6 +304,8 @@ static const InputEventLabel KEYCODES[] = {
     DEFINE_KEYCODE(TV_KEYMOUSE_DOWN),
     DEFINE_KEYCODE(TV_KEYMOUSE_MODE_SWITCH),
     DEFINE_KEYCODE(HELP),
+    DEFINE_KEYCODE(PAD_IN),
+    DEFINE_KEYCODE(PAD_OUT),
 
     { NULL, 0 }
 };



