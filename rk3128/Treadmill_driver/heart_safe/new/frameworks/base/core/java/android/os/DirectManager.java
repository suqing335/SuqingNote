/*
 * Copyright (C) 2011 The Android Open Source Project
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


package android.os;

import android.content.Context;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.os.IDirectManager;
import android.util.Log;

import java.io.IOException;

/**
 * @hide
 */
public class DirectManager extends Direct {
	private static final String TAG = "DirectManager";

	private final Context mContext;
	private final IDirectManager mService;

	/**
	 * {@hide}
	 */
	public DirectManager(Context context, IDirectManager service) {
		mContext = context;
		mService = service;
	}
	/**
	 * getValue Get the data uploaded by the direct-current controller
	 * @param length The number of get size.
	 * @return the byte array for the created jni
	 * @hide
	 */
	@Override 
	public byte[] getValue(int length) {
		try {
			return mService.getValue2(length);
		} catch (RemoteException e) {
			Log.e(TAG, "RemoteException in getValue", e);
			return null;
		}
	}
	/**
	 * getValue Get the data uploaded by the direct-current controller
	 * @param buffer Store read data
	 * @param length attempts to read up to bytes.
	 * @return the number of read up to bytes
	 * @hide
	 */
	@Override
	public int getValue(byte[] buffer, int length) {
		try {
			return mService.getValue1(buffer, length);
		} catch (RemoteException e) {
			Log.e(TAG, "RemoteException in getValue", e);
			return -1;
		}
	}
	/**
	 * setValue set data to the direct-current controller
	 * @param buffer Store write data
	 * @return the number of writes up to bytes
	 * @hide
	 */
	@Override
	public int setValue(byte[] buffer) {
		try {
			return mService.setValue2(buffer);
		} catch (RemoteException e) {
			Log.e(TAG, "RemoteException in getValue", e);
			return -1;
		}
	}
	/**
	 * setValue set data to the direct-current controller
	 * @param buffer Store write data
	 * @param length attempts to write up to bytes.
	 * @return the number of writes up to bytes
	 * @hide
	 */
	@Override
	public int setValue(byte[] buffer, int length) {
		try {
			return mService.setValue1(buffer, length);
		} catch (RemoteException e) {
			Log.e(TAG, "RemoteException in getValue", e);
			return -1;
		}
	}
	/**
	 * setFanSpeedValue set fan speed
	 * @param speed
	 * @return  "0" is success, or "-1" is failed
	 * @hide
	 */
	@Override
	public int setFanSpeedValue(int speed) {
		try {
			return mService.setFanSpeedValue(speed);
		} catch (RemoteException e) {
			Log.e(TAG, "RemoteException in setFanSpeedValue");
			return -1;
		}
	}
	/**
	 * getFanSpeedValue get fan speed
	 * @return  "-1" is failed, otherwise is success.
	 * @hide
	 */
	@Override
	public int getFanSpeedValue() {
		try {
			return mService.getFanSpeedValue();
		} catch (RemoteException e) {
			Log.e(TAG, "RemoteException in getFanSpeedValue");
			return -1;
		}
	}
	
	/**
	 * setBuzzerStatus set buzzer status
	 * @param speed
	 * @return  "0" is success, or "-1" is failed
	 * @hide
	 */
	@Override
	public int setBuzzerStatus(boolean enable) {
		try {
			return mService.setBuzzerStatus(enable);
		} catch (RemoteException e) {
			Log.e(TAG, "RemoteException in setBuzzerStatus");
			return -1;
		}
	}
	/**
	 * getBuzzerStatus get buzzer status
	 * @return  "true" or "false"
	 * @hide
	 */
	@Override
	public boolean getBuzzerStatus() {
		try {
			return mService.getBuzzerStatus();
		} catch (RemoteException e) {
			Log.e(TAG, "RemoteException in getBuzzerStatus");
			return false;
		}
	}

}
