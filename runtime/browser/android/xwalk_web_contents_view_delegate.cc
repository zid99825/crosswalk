// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Copyright (c) 2014 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/android/xwalk_web_contents_view_delegate.h"

#include "base/command_line.h"
//#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "ui/gfx/color_space.h"

namespace xwalk {

XWalkWebContentsViewDelegate::XWalkWebContentsViewDelegate(
    content::WebContents* web_contents)
    /*: web_contents_(web_contents)*/ {
}

XWalkWebContentsViewDelegate::~XWalkWebContentsViewDelegate() {
}

/*
 * TODO(iotto)
content::WebDragDestDelegate* AwWebContentsViewDelegate::GetDragDestDelegate() {
  // GetDragDestDelegate is a pure virtual method from WebContentsViewDelegate
  // and must have an implementation although android doesn't use it.
  NOTREACHED();
  return NULL;
}
*/

void XWalkWebContentsViewDelegate::OverrideDisplayColorSpace(gfx::ColorSpace* color_space) {
  // TODO(ccameron): WebViews that are embedded in WCG windows will want to
  // override the display color space to gfx::ColorSpace::CreateExtendedSRGB().
  // This situation is not yet detected.
  // https://crbug.com/735658
  *color_space = gfx::ColorSpace::CreateSRGB();
}

void XWalkWebContentsViewDelegate::ShowContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // TODO(iotto): Fix this!
//  if (params.is_editable && params.selection_text.empty()) {
//    content::ContentViewCore* content_view_core =
//        content::ContentViewCore::FromWebContents(web_contents_);
//    if (content_view_core) {
//      content_view_core->ShowPastePopup(params);
//    }
//  }
}

content::WebDragDestDelegate*
    XWalkWebContentsViewDelegate::GetDragDestDelegate() {
  return NULL;
}

}  // namespace xwalk
