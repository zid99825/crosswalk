// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Copyright (c) 2014 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/android/xwalk_icon_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/favicon_url.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

using content::BrowserThread;
using content::WebContents;

namespace xwalk {

XWalkIconHelper::XWalkIconHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      listener_(NULL),
      _this_weak(this) {
}

XWalkIconHelper::~XWalkIconHelper() {
  _this_weak.InvalidateWeakPtrs();
}

void XWalkIconHelper::SetListener(Listener* listener) {
  listener_ = listener;
}

void XWalkIconHelper::DownloadIcon(const GURL& icon_url) {
  web_contents()->DownloadImage(icon_url, true, 0, false,
      base::Bind(&XWalkIconHelper::DownloadFaviconCallback,
                 _this_weak.GetWeakPtr()));
}

void XWalkIconHelper::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (std::vector<content::FaviconURL>::const_iterator i = candidates.begin();
       i != candidates.end(); ++i) {
    if (!i->icon_url.is_valid())
      continue;

    switch (i->icon_type) {
      case content::FaviconURL::IconType::kFavicon:
//        if (listener_) listener_->OnIconAvailable(i->icon_url);
        // TODO(iotto) : Ask Clinet if favicons should be downloaded
        web_contents()->DownloadImage(i->icon_url, true,  // Is a favicon
                                      0,  // No maximum size
                                      false,  // Normal cache policy
                                      base::Bind(&XWalkIconHelper::DownloadFaviconCallback, _this_weak.GetWeakPtr()));
        break;
      case content::FaviconURL::IconType::kTouchIcon:
        break;
      case content::FaviconURL::IconType::kTouchPrecomposedIcon:
        break;
      case content::FaviconURL::IconType::kInvalid:
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

void XWalkIconHelper::DownloadFaviconCallback(
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (http_status_code == 404 || bitmaps.size() == 0) return;

  if (listener_) listener_->OnReceivedIcon(image_url, bitmaps[0]);
}

}  // namespace xwalk
