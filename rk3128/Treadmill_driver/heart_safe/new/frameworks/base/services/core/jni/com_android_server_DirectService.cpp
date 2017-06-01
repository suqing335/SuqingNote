/*
 * Copyright (C) 2009 The Android Open Source Project
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

#define LOG_TAG "JNITreadService"

#include "jni.h"
#include "JNIHelp.h"
#include "android_runtime/AndroidRuntime.h"

#include <utils/misc.h>
#include <utils/Log.h>
#include <hardware/direct_current.h>

#include <stdio.h>

namespace android
{
struct direct_device* device;
static jint getDeviceValue1(JNIEnv *env, jobject clazz, jbyteArray buffer, jint length) {
	int ret;
	jboolean is_copy;
	if (!device)
		return -1;
	jbyte* readbuffer = env->GetByteArrayElements(buffer, &is_copy);
	if (!readbuffer) {
		ALOGE("getDeviceValue GetByteArrayElements fialed\n");
		return -1;
	}
	ret = device->dev_read(device, (char *)readbuffer, length);
	//env->ReleaseByteArrayElements(buffer, readbuffer, 0);
	/**ReleaseByteArrayElements 第三个参数决定是否释放readbuffer指向的空间
	 * 如果为0 则释放，JNI_COMMIT 不释放。
	 */
	if (is_copy) {
		/**is_copy = JNI_TRUE, 拷贝了数据，需要释放数据**/
		env->ReleaseByteArrayElements(buffer, readbuffer, 0);
	} else {
		env->ReleaseByteArrayElements(buffer, readbuffer, JNI_COMMIT);
	}
	return ret;
}
static jbyteArray getDeviceValue2(JNIEnv *env, jobject clazz, jint length) {
	int ret;
	jbyte* BufferTemp;
	jbyteArray byteArray = NULL;
	if (!device)
		return NULL;
	BufferTemp = (jbyte*)malloc(length);
	if (!BufferTemp) {
		ALOGE("temp alloc size failed\n");
		return NULL;
	}
	ret = device->dev_read(device, (char *)BufferTemp, length);
	if (ret > 0) {
		byteArray = env->NewByteArray(ret);
		if (byteArray == NULL) {
			ALOGE("Unable to allocate byte array for byteArray");
			return NULL;
		}
		env->SetByteArrayRegion(byteArray, 0, ret, (const jbyte*)BufferTemp);
		free(BufferTemp);
		return byteArray;
	}
	return NULL;
}

static jint setDeviceValue(JNIEnv *env, jobject clazz,jbyteArray buffer, jint length) {
	int ret;
	jboolean is_copy;
	if (!device)
		return -1;
	jbyte* writebuffer = env->GetByteArrayElements(buffer, &is_copy);
	if (!writebuffer) {
		ALOGE("setDeviceValue GetByteArrayElements fialed\n");
		return -1;
	}
	ret = device->dev_write(device, (const char *)writebuffer, length);
	
	if (is_copy) {
		env->ReleaseByteArrayElements(buffer, writebuffer, 0);
	} else {
		env->ReleaseByteArrayElements(buffer, writebuffer, JNI_COMMIT);
	}
	
	return ret;
}

static jboolean getDeviceBuzzerStatus(JNIEnv *env, jobject clazz) {
	int ret;
	ret = device->dev_getbuzzer(device);
	return ret ? true : false;
}
static jint setDeviceBuzzerStatus(JNIEnv *env, jobject clazz, jboolean enable) {
	return device->dev_setbuzzer(device, enable ? 1 : 0);
}

static jint openDevice(JNIEnv *env, jobject clazz) {
	int ret;
	struct hw_module_t * module;
	ret = hw_get_module(DIRECT_HARDWARE_MODULE_ID, (const struct hw_module_t **)&module);
        if(ret) {
                ALOGE("get module failed\n");
                return -1;
        }

        ret = module->methods->open(module, DIRECT_CONTROLLER, (struct hw_device_t**)&device);
        if(ret) {
                ALOGE("device open failed\n");
                return -1;
        }
	ret = device->dev_open(device);
	if (ret < 0) {
		ALOGE("open direct device failed\n");
		return -1;
	}
	return 0;
}
static jint closeDevice(JNIEnv *env, jobject clazz) {
	if (!device)
		return -1;
	device->dev_close(device);
	return 0;
}

static jint setDeviceFanSpeedValue(JNIEnv *env, jobject clazz, jint speed) {
	if (!device)
		return -1;
	return device->dev_setspeed(device, speed);
}
static jint getDeviceFanSpeedValue(JNIEnv *env, jobject clazz) {
	if (!device)
		return -1;
	return device->dev_getspeed(device);
}

static JNINativeMethod method_table[] = {
	{ "closeDevice", "()I", (void*)closeDevice },
	{ "openDevice", "()I", (void*)openDevice },
	{ "setDeviceValue", "([BI)I", (void*)setDeviceValue },
	{ "getDeviceValue1", "([BI)I", (void*)getDeviceValue1 },
	{ "getDeviceValue2", "(I)[B", (void*)getDeviceValue2 },
	{ "setDeviceFanSpeedValue", "(I)I", (void*)setDeviceFanSpeedValue },
	{ "getDeviceFanSpeedValue", "()I", (void*)getDeviceFanSpeedValue },
	{ "setDeviceBuzzerStatus", "(Z)I", (void*)setDeviceBuzzerStatus },
	{ "getDeviceBuzzerStatus", "()Z", (void*)getDeviceBuzzerStatus }
};

int register_android_server_DirectService(JNIEnv *env)
{
    return jniRegisterNativeMethods(env, "com/android/server/DirectService",
            method_table, NELEM(method_table));
}

};