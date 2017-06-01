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

import android.os.ParcelFileDescriptor;

/** @hide */
interface IDirectManager {
	int setValue1(in byte[] buffer, int length);
	int setValue2(in byte[] buffer);
	int getValue1(inout byte[] buffer, int length);
	byte[] getValue2(int length);
	
	int setFanSpeedValue(int speed);
	int getFanSpeedValue();
	
	int setBuzzerStatus(boolean enable);
	boolean getBuzzerStatus();
}
