/*
 * xwalk_render_frame_ext.h
 *
 *  Created on: Oct 13, 2017
 *      Author: iotto
 *
 *  Copy of AwRenderFrameExt
 */

#ifndef XWALK_RUNTIME_RENDERER_ANDROID_XWALK_RENDER_FRAME_EXT_H_
#define XWALK_RUNTIME_RENDERER_ANDROID_XWALK_RENDER_FRAME_EXT_H_

#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/origin.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {
enum WebMeaningfulLayout;
class WebFrameWidget;
class WebView;
}
namespace xwalk {

class XWalkRenderFrameExt : public content::RenderFrameObserver {
 public:
  XWalkRenderFrameExt(content::RenderFrame* render_frame);

 private:
  // will destroy in OnDestruct
  ~XWalkRenderFrameExt() override;

  // RenderFrameObserver:
  void DidCommitProvisionalLoad(bool is_new_navigation,
                                bool is_same_document_navigation) override;

  bool OnMessageReceived(const IPC::Message& message) override;
  void FocusedNodeChanged(const blink::WebNode& node) override;
  void OnDestruct() override;

  void OnDocumentHasImagesRequest(uint32_t id);
  void OnDoHitTest(const gfx::PointF& touch_center,
                   const gfx::SizeF& touch_area);

  void OnSetTextZoomLevel(double zoom_level);
  void OnSetTextZoomFactor(float zoom_factor);

  void OnResetScrollAndScaleState();

  void OnSetInitialPageScale(double page_scale_factor);

  void OnSetBackgroundColor(SkColor c);

  void OnSmoothScroll(int target_x, int target_y, int duration_ms);

  blink::WebView* GetWebView();
  blink::WebFrameWidget* GetWebFrameWidget();

  url::Origin last_origin_;

  std::unique_ptr<service_manager::BinderRegistry> registry_;

  DISALLOW_COPY_AND_ASSIGN(XWalkRenderFrameExt);
};

} /* namespace xwalk */

#endif /* XWALK_RUNTIME_RENDERER_ANDROID_XWALK_RENDER_FRAME_EXT_H_ */
