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
#include "base/android/library_loader/library_loader_hooks.h"

#ifdef TENTA_CHROMIUM_BUILD
#include "xwalk/third_party/tenta/meta_fs/jni/register_jni.h"
#include "xwalk/third_party/tenta/chromium_cache/register_jni.h"
#include "xwalk/third_party/tenta/crosswalk_extensions/register_jni.h"
#endif

namespace xwalk {
namespace {

bool RegisterJNI(JNIEnv* env) {
  bool retVal =xwalk::RegisterJni(env);
#ifdef TENTA_CHROMIUM_BUILD
  retVal = retVal && tenta::fs::RegisterJni(env);
  retVal = retVal && tenta::fs::cache::RegisterJni(env);
  retVal = retVal && tenta::ext::RegisterJni(env);
#endif
  return retVal;
}
} // namespace

bool OnJNIOnLoadRegisterJNI(JNIEnv* env) {
  if (!content::android::OnJNIOnLoadRegisterJNI(env)) {
    return false;
  }

  if (!RegisterJNI(env)) {
    return false;
  }

  return true;
}

bool OnJNIOnLoadInit() {
  if (!content::android::OnJNIOnLoadInit()) {
    return false;
  }

  //TODO(iotto)
//  base::android::SetVersionNumber(PRODUCT_VERSION);
  content::SetContentMainDelegate(new xwalk::XWalkMainDelegateAndroid());
  
  // Initialize url lib here while we are still single-threaded, in case we use
  // CookieManager before initializing Chromium (which would normally have done
  // this). It's safe to call this multiple times.
  url::Initialize();
  return true;
}

} // namespace xwalk

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
//  base::android::SetJniRegistrationType(base::android::NO_JNI_REGISTRATION);
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();

  if ( !xwalk::OnJNIOnLoadRegisterJNI(env) ) {
    return -1;
  }


  base::android::SetNativeInitializationHook(&xwalk::OnJNIOnLoadInit);

  return JNI_VERSION_1_4;
}
