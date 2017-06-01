/*
 * Copyright (C) 2006 The Android Open Source Project
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

import android.app.ActivityThread;
import android.content.Context;
import android.media.AudioAttributes;

/**
 * Class that operates the vibrator on the device.
 * <p>
 * If your process exits, any vibration you started will stop.
 * </p>
 *
 * To obtain an instance of the system vibrator, call
 * {@link Context#getSystemService} with {@link Context#VIBRATOR_SERVICE} as the argument.
 */
public abstract class Direct {

    private final String mPackageName;

    /**
     * @hide to prevent subclassing from outside of the framework
     */
    public Direct() {
        mPackageName = ActivityThread.currentPackageName();
    }

    /**
     * @hide to prevent subclassing from outside of the framework
     */
    protected Direct(Context context) {
        mPackageName = context.getOpPackageName();
    }

    public abstract byte[] getValue(int length);
    public abstract int getValue(byte[] buffer, int length);
    public abstract int setValue(byte[] buffer);
    public abstract int setValue(byte[] buffer, int length);
    public abstract int setFanSpeedValue(int speed);
    public abstract int getFanSpeedValue();
    public abstract int setBuzzerStatus(boolean enable);
    public abstract boolean getBuzzerStatus();

}
