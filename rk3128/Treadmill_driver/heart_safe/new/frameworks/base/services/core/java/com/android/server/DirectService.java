/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.server;

import android.content.Context;
import android.os.IDirectManager;
import android.os.DirectManager;
import android.os.RemoteException;
import android.os.IBinder;
import android.os.Binder;
import android.util.Slog;

public class DirectService extends IDirectManager.Stub{
	
	static final String TAG = "DirectService";
	private final Context mContext;

	native static int closeDevice();
	native static int openDevice();
	native static int setDeviceValue(byte[] buffer, int length);
	native static int getDeviceValue1(byte[] buffer, int length);
	native static byte[] getDeviceValue2(int length);
	/**************************************************/
	native static int setDeviceFanSpeedValue(int speed);
	native static int getDeviceFanSpeedValue();
	
	/**************************************************/
	native static int setDeviceBuzzerStatus(boolean enable);
	native static boolean getDeviceBuzzerStatus();

	DirectService(Context context){
		mContext = context;
		openDevice();
		Slog.d(TAG, "open DirectService success");
	}
	/**
	 * direct-current controller interface
	 *
	 */
	@Override
	public int setValue1(byte[] buffer, int length) {
		return setDeviceValue(buffer, length);
	}
	@Override
	public int setValue2(byte[] buffer) {
		return setDeviceValue(buffer, buffer.length);
	}
	@Override
	public int getValue1(byte[] buffer, int length) {
		return getDeviceValue1(buffer, length);
	}
	@Override
	public byte[] getValue2(int length) {
		return getDeviceValue2(length);
	}
	
	/**
	 * controller fan speed interface
	 * int setFanSpeedValue(int speed)
	 * int getFanSpeedValue()
	 */
	@Override
	public int setFanSpeedValue(int speed) {
		return setDeviceFanSpeedValue(speed);
	}
	
	@Override
	public int getFanSpeedValue() {
		return getDeviceFanSpeedValue();
	}
	
	/**
	 * controller buzzer status interface
	 * int setBuzzerStatus(boolean enable)
	 * boolean getBuzzerStatus()
	 */
	@Override
	public int setBuzzerStatus(boolean enable) {
		return setDeviceBuzzerStatus(enable);
	}
	
	@Override
	public boolean getBuzzerStatus() {
		return getDeviceBuzzerStatus();
	}

	protected void finalize() throws Throwable {
		closeDevice();
		super.finalize();
		Slog.d(TAG, "close DirectService");
	}


};