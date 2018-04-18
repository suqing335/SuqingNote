###在2.2,2.3版本上重写下面方法就能重写home键
    public void onAttachedToWindow() {
        this.getWindow().setType(WindowManager.LayoutParams.TYPE_KEYGUARD);
        super.onAttachedToWindow();
    }

    但是在4.0以上就不能用了。
     刚刚发现4.0上还有一种方法可以屏蔽和重写Home键，而且非常简单。代码如下：
    public static final int FLAG_HOMEKEY_DISPATCHED = 0x80000000;
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        this.getWindow().setFlags(FLAG_HOMEKEY_DISPATCHED, FLAG_HOMEKEY_DISPATCHED);//关键代码
        setContentView(R.layout.main);
    }

    再重写onKey事件即可。
