// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

import android.net.Uri;

import java.util.Map;

//TODO(iotto) : @XWalkAPI(impl = XWalkWebResourceRequest.class, createInternally = true)
public class XWalkWebResourceRequestHandler implements XWalkWebResourceRequest {
    private final XWalkContentsClientBridge.WebResourceRequestInner mRequest;

    // Never use this constructor.
    // It is only used in WebResourceRequestHandlerBridge.
    XWalkWebResourceRequestHandler() {
        mRequest = null;
    }

    XWalkWebResourceRequestHandler(
            XWalkContentsClientBridge.WebResourceRequestInner request) {
        mRequest = request;
    }

//TODO(iotto) :     @XWalkAPI
    public Uri getUrl() {
        return Uri.parse(mRequest.url);
    }

//TODO(iotto) :     @XWalkAPI
    public boolean isForMainFrame() {
        return mRequest.isMainFrame;
    }

//TODO(iotto) :     @XWalkAPI
    public boolean hasGesture() {
        return mRequest.hasUserGesture;
    }

//TODO(iotto) :     @XWalkAPI
    public String getMethod() {
        return mRequest.method;
    }

//TODO(iotto) :     @XWalkAPI
    public Map<String, String> getRequestHeaders() {
        return mRequest.requestHeaders;
    }
}
