Android 4.4 允许应用以两种方式进入全屏模式: 横置屏幕 (Lean Back) 和沉浸模式(Immersive)。无论使用何种方式，进入全屏模式后所有的系统栏都会被隐藏。这两种方式的不同在于用户需要用不同的方式调出隐藏的系统栏。
在android4.4及以上版本中为setSystemUiVisibility()方法引入了一个新的flag:SYSTEM_UI_FLAG_IMMERSIVE，它可以使你的app实现真正意义上的全屏体验。当SYSTEM_UI_FLAG_IMMERSIVE、SYSTEM_UI_FLAG_HIDE_NAVIGATION 和SYSTEM_UI_FLAG_FULLSCREEN三个flag一起使用的时候，可以隐藏状态栏与导航栏，同时让你的app可以捕捉到用户的所有触摸屏事件。

当沉浸式全屏模式启用的时候，你的activity会继续接受各类的触摸事件。用户可以通过在状态栏与导航栏原来区域的边缘向内滑动让系统栏重新显示。这个操作清空了SYSTEM_UI_FLAG_HIDE_NAVIGATION(和SYSTEM_UI_FLAG_FULLSCREEN，如果有的话)两个标志，因此系统栏重新变得可见。如果设置了的话，这个操作同时也触发了View.OnSystemUiVisibilityChangeListener。然而， 如果你想让系统栏在一段时间后自动隐藏的话，你应该使用SYSTEM_UI_FLAG_IMMERSIVE_STICKY标签。请注意，'sticky'版本的标签不会触发任何的监听器，因为在这个模式下展示的系统栏是处于暂时的状态。
图1展示了各种不同的“沉浸式”状态
public void toggleHideyBar() {  
  
            // BEGIN_INCLUDE (get_current_ui_flags)  
            // The UI options currently enabled are represented by a bitfield.  
            // getSystemUiVisibility() gives us that bitfield.  
            int uiOptions = getWindow().getDecorView().getSystemUiVisibility();  
            int newUiOptions = uiOptions;  
            // END_INCLUDE (get_current_ui_flags)  
            // BEGIN_INCLUDE (toggle_ui_flags)  
            boolean isImmersiveModeEnabled =  
                    ((uiOptions | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY) == uiOptions);  
            if (isImmersiveModeEnabled) {  
                Log.i("123", "Turning immersive mode mode off. ");  
            } else {  
                Log.i("123", "Turning immersive mode mode on.");  
            }  
  
            // Navigation bar hiding:  Backwards compatible to ICS.  
            if (Build.VERSION.SDK_INT >= 14) {  
                newUiOptions ^= View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;  
            }  
  
            // Status bar hiding: Backwards compatible to Jellybean  
            if (Build.VERSION.SDK_INT >= 16) {  
                newUiOptions ^= View.SYSTEM_UI_FLAG_FULLSCREEN;  
            }  
  
            // Immersive mode: Backward compatible to KitKat.  
            // Note that this flag doesn't do anything by itself, it only augments the behavior  
            // of HIDE_NAVIGATION and FLAG_FULLSCREEN.  For the purposes of this sample  
            // all three flags are being toggled together.  
            // Note that there are two immersive mode UI flags, one of which is referred to as "sticky".  
            // Sticky immersive mode differs in that it makes the navigation and status bars  
            // semi-transparent, and the UI flag does not get cleared when the user interacts with  
            // the screen.  
            if (Build.VERSION.SDK_INT >= 18) {  
                newUiOptions ^= View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;  
            }  
            
//         getWindow().getDecorView().setSystemUiVisibility(newUiOptions);//上边状态栏和底部状态栏滑动都可以调出状态栏  
            getWindow().getDecorView().setSystemUiVisibility(4108);//这里的4108可防止从底部滑动调出底部导航栏  
            //END_INCLUDE (set_ui_flags)  
        }  

