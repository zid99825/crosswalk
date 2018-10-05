// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/renderer/android/xwalk_permission_client.h"

#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/gurl.h"
#include "meta_logging.h"

namespace xwalk {

XWalkPermissionClient::XWalkPermissionClient(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  render_frame->GetWebFrame()->SetContentSettingsClient(this);
}

XWalkPermissionClient::~XWalkPermissionClient() {
}

void XWalkPermissionClient::OnDestruct() {
  delete this;
}

bool XWalkPermissionClient::AllowStorage(bool local) {
  TENTA_LOG_NET(INFO) << "iotto " << __func__ << " local=" << local;
//  if (local) {
//    return false;
//  }
  return true;
}
}  // namespace xwalk
