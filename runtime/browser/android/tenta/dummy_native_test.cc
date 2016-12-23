/*
 * dummy_native_test.cc
 *
 *  Created on: Dec 13, 2016
 *      Author: iotto
 */

#include <xwalk/runtime/browser/android/tenta/dummy_native_test.h>
#include "jni/DummyNativeTest_jni.h"

namespace xwalk {
namespace tenta {

DummyNativeTest::DummyNativeTest() {

	Java_DummyNativeTest_sFunc(nullptr);
}

DummyNativeTest::~DummyNativeTest() {
	// TODO Auto-generated destructor stub
}

bool RegisterTentaDummyNativeTest(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}	// tenta
} 	// xwalk
