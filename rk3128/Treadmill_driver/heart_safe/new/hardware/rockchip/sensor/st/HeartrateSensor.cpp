/*
 * Copyright (C) 2010 Motorola, Inc.
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

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>

#include <cutils/log.h>

#include "HeartrateSensor.h"

/*****************************************************************************/

HeartrateSensor::HeartrateSensor()
    : SensorBase(HR_DEVICE_NAME, "heartrate"),
      mEnabled(0),
      mInputReader(32),
      mHasPendingEvent(false)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_HR;
    mPendingEvent.type = SENSOR_TYPE_HEARTRATE;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));

     open_device();

    int flags = 0;
    if (!ioctl(dev_fd, HEARTRATE_IOCTL_GET_ENABLED, &flags)) {
        if (flags) {
            mEnabled = 1;
            setInitialState();
        }
    }

    if (!mEnabled) {
        close_device();
    }
}

HeartrateSensor::~HeartrateSensor() {
}

int HeartrateSensor::setInitialState() {
    struct input_absinfo absinfo;
    if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_HEARTRATE), &absinfo)) {
        mHasPendingEvent = true;
    }
    return 0;
}

int HeartrateSensor::enable(int32_t, int en) {
    int flags = en ? 1 : 0;
    int err = 0;
    if (flags != mEnabled) {
        if (!mEnabled) {
            open_device();
        }
        err = ioctl(dev_fd, HEARTRATE_IOCTL_ENABLE, &flags);
        err = err<0 ? -errno : 0;
        LOGE_IF(err, "HEARTRATE_IOCTL_ENABLE failed (%s)", strerror(-err));
        if (!err) {
            mEnabled = en ? 1 : 0;
            if (en) {
                setInitialState();
            }
        }
        if (!mEnabled) {
            close_device();
        }
    }
    return err;
}

bool HeartrateSensor::hasPendingEvents() const {
    return mHasPendingEvent;
}


int HeartrateSensor::setDelay(int32_t handle, int64_t ns)
{
    if (ns < 0)
        return -EINVAL;

    int delay = ns / 1000000;
    if (ioctl(dev_fd, HEARTRATE_IOCTL_SET_DELAY, &delay)) {
        return -errno;
    }
    return 0;
}

int HeartrateSensor::readEvents(sensors_event_t* data, int count)
{
    if (count < 1)
        return -EINVAL;

	 if (mHasPendingEvent) {
        mHasPendingEvent = false;
        mPendingEvent.timestamp = getTimestamp();
        *data = mPendingEvent;
        return mEnabled ? 1 : 0;
    }

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0)
        return n;
    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
        if (type == EV_ABS) {
            processEvent(event->code, event->value);
        } else if (type == EV_SYN) {
            int64_t time = timevalToNano(event->time);
            mPendingEvent.timestamp = time;
            if (mEnabled) {
                *data++ = mPendingEvent;
                count--;
                numEventReceived++;
            }
        } else {
            ALOGE("HeartrateSensor: unknown event (type=%d, code=%d)",
                    type, event->code);
        }
        mInputReader.next();
    }

    return numEventReceived;
}

void HeartrateSensor::processEvent(int code, int value)
{
    if (code == EVENT_TYPE_HEARTRATE) {
            mPendingEvent.pressure = value * CONVERT_B ;
	//LOGD("heart rate %s:value=%d\n",__FUNCTION__, value);
    }
}
