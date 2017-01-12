/*
 * dummy_native_test.h
 *
 *  Created on: Dec 13, 2016
 *      Author: iotto
 */

#ifndef XWALK_RUNTIME_BROWSER_ANDROID_TENTA_DUMMY_NATIVE_TEST_H_
#define XWALK_RUNTIME_BROWSER_ANDROID_TENTA_DUMMY_NATIVE_TEST_H_

#include <jni.h>
#include "base/macros.h"

namespace xwalk {
namespace tenta {

class DummyNativeTest {
public:
	DummyNativeTest();

protected:
	virtual ~DummyNativeTest();

private:
	DISALLOW_COPY_AND_ASSIGN(DummyNativeTest);
};

bool RegisterTentaDummyNativeTest(JNIEnv* env);

} // namespace tenta
} // namespace xwalk

#endif /* XWALK_RUNTIME_BROWSER_ANDROID_TENTA_DUMMY_NATIVE_TEST_H_ */
