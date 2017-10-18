// Copyright (c) 2014 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.xwalk.core.internal;

import android.content.Context;
import android.util.Log;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;

class XWalkInternalResources {
    private static final String TAG = "XWalkInternalResources";

    private static boolean loaded = false;
    private final static String INTERNAL_RESOURCE_CLASSES[] = {
        "org.chromium.components.web_contents_delegate_android.R",
        "org.chromium.components.autofill.R",
        "org.chromium.android_webview.R",
        "org.chromium.content.R",
        "org.chromium.ui.R",
//        "android.support.v7.appcompat.R",
        "org.xwalk.core.internal.R"
    };
    private final static String GENERATED_RESOURCE_CLASS = "org.xwalk.core.R";

    // Doing org.chromium.content.R.<class>.<name> = org.xwalk.core.R.<class>.<name>
    // Use reflection to iterate over the target class is to avoid hardcode.
    private static void doResetIds(Context context) {
//        Log.d(TAG, "XWalkInternalResources.doResetIds");
        // internal classes are loaded with the same classLoader of XWalkInternalResources
        ClassLoader classLoader = XWalkInternalResources.class.getClassLoader();
        ClassLoader appClassLoader = context.getApplicationContext().getClassLoader();
        for (String resourceClass : INTERNAL_RESOURCE_CLASSES) {
            try {
                Class<?> internalResource = classLoader.loadClass(resourceClass);
                Class<?>[] innerClazzs = internalResource.getClasses();
                for (Class<?> innerClazz : innerClazzs) {
                    Class<?> generatedInnerClazz;
//                    Log.d("iotto", "innerClazz.getName() " + innerClazz.getName());
                    String generatedInnerClassName = innerClazz.getName().replace(
                            resourceClass, GENERATED_RESOURCE_CLASS);
//                    Log.d("iotto", "generatedInnerClassName " + generatedInnerClassName);
                    try {
                        generatedInnerClazz = appClassLoader.loadClass(generatedInnerClassName);
                    } catch (ClassNotFoundException e) {
                        Log.w(TAG, generatedInnerClassName + " (generatedInnerClassName) is not found.");
                        continue;
                    }
/*                    Field[] genFields = generatedInnerClazz.getFields();
                    Log.d("iotto", "Fields for: " + generatedInnerClassName);
                    for ( Field gf : genFields ) {
                        Log.d("iotto", "  field " + gf.getName());
                    }
*/
                    Field[] fields = innerClazz.getFields();
                    for (Field field : fields) {
//                        Log.d("iotto", "field " + field.getName());
                        // It's final means we are probably not used as library project.
                        if (Modifier.isFinal(field.getModifiers())) field.setAccessible(true);
                        try {
                            int value = generatedInnerClazz.getField(field.getName()).getInt(null);
//                            Log.d("iotto", "field " + field.getName() + ": oldInt " + field.getInt(null) + " newInt " + value);
                            field.setInt(null, value);
                        } catch (IllegalAccessException e) {
                            Log.w(TAG, generatedInnerClazz.getName() + "." +
                                    field.getName() + " (IllegalAccessException) is not accessable.");
                        } catch (IllegalArgumentException e) {
                            Log.w(TAG, generatedInnerClazz.getName() + "." +
                                    field.getName() + " (IllegalArgumentException) is not int.");
                        } catch (NoSuchFieldException e) {
                            Log.w(TAG, generatedInnerClazz.getName() + "." +
                                    field.getName() + " (NoSuchFieldException) is not found.");
                        }
                        if (Modifier.isFinal(field.getModifiers())) field.setAccessible(false);
                    }
                }
            } catch (ClassNotFoundException e) {
                Log.w(TAG, resourceClass + " (resourceClass) is not found.");
            }
        }
    }

    static void resetIds(Context context) {
        if (!loaded) {
            doResetIds(context);
            loaded = true;
        }
    }
}
