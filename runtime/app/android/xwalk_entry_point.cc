// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_delegate.h"
#include "xwalk/runtime/app/android/xwalk_jni_registrar.h"
#include "xwalk/runtime/app/android/xwalk_main_delegate_android.h"
#include "xwalk/third_party/tenta/meta_fs/jni/register_jni.h"
#include "xwalk/third_party/tenta/chromium_cache/register_jni.h"

namespace {

bool RegisterJNI(JNIEnv* env) {
  bool retVal =xwalk::RegisterJni(env);
  retVal = retVal && tenta::fs::RegisterJni(env);
  retVal = retVal && tenta::fs::cache::RegisterJni(env);

  return retVal;
}

bool Init() {
  content::SetContentMainDelegate(new xwalk::XWalkMainDelegateAndroid());
  return true;
}

}  // namespace

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);

  JNIEnv* env = base::android::AttachCurrentThread();

  if (!content::android::OnJNIOnLoadRegisterJNI(env) || !RegisterJNI(env)
    || !Init() ) {
    return -1;
  }

  return JNI_VERSION_1_4;
}
