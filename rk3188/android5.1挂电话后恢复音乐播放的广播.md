
#####在frameworks/base/services/core/java/com/android/server/TelephonyRegistry.java
	private void broadcastCallStateChanged(int state, String incomingNumber, int subId) {
	Intent intent = new Intent(TelephonyManager.ACTION_PHONE_STATE_CHANGED);
	        intent.putExtra(PhoneConstants.STATE_KEY,
	                DefaultPhoneNotifier.convertCallState(state).toString());
	        if (!TextUtils.isEmpty(incomingNumber)) {
	            intent.putExtra(TelephonyManager.EXTRA_INCOMING_NUMBER, incomingNumber);
	        }
	        intent.putExtra(PhoneConstants.SUBSCRIPTION_KEY, subId);
	        mContext.sendBroadcastAsUser(intent, UserHandle.ALL,
	                android.Manifest.permission.READ_PHONE_STATE)
####电话的三中状态 在frameworks/base/telephony/java/android/telephony/TelephonyManager.java
	 /** Device call state: No activity. */
	    public static final int CALL_STATE_IDLE = 0;
	    /** Device call state: Ringing. A new call arrived and is
	     *  ringing or waiting. In the latter case, another call is
	     *  already active. */
	    public static final int CALL_STATE_RINGING = 1;
	    /** Device call state: Off-hook. At least one call exists
	      * that is dialing, active, or on hold, and no calls are ringing
	      * or waiting. */
	    public static final int CALL_STATE_OFFHOOK = 2;
####在音乐播放器中需要加入权限
	uses-permission android:name="android.permission.READ_PHONE_STATE"

