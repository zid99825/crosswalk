// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/renderer/xwalk_content_renderer_client.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "components/visitedlink/renderer/visitedlink_slave.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/common/content_features.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/common/url_loader_throttle.h"
#include "components/data_reduction_proxy/content/renderer/content_previews_render_frame_observer.h"
#include "components/safe_browsing/renderer/websocket_sb_handshake_throttle.h"
#include "grit/xwalk_application_resources.h"
#include "grit/xwalk_sysapps_resources.h"
#include "net/base/net_errors.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/WebKit/public/platform/scheduler/renderer_process_type.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "xwalk/application/common/constants.h"
#include "xwalk/application/renderer/application_native_module.h"
#include "xwalk/extensions/common/xwalk_extension_switches.h"
#include "xwalk/extensions/renderer/xwalk_js_module.h"
#include "xwalk/runtime/common/xwalk_common_messages.h"
#include "xwalk/runtime/common/xwalk_localized_error.h"
#include "xwalk/runtime/renderer/isolated_file_system.h"
#include "xwalk/runtime/renderer/pepper/pepper_helper.h"
#include "services/service_manager/public/cpp/binder_registry.h"
//#include "services/service_manager/public/cpp/interface_registry.h"

#if defined(OS_ANDROID)
#include "components/cdm/renderer/android_key_systems.h"
#include "xwalk/runtime/browser/android/net/url_constants.h"
#include "xwalk/runtime/common/android/xwalk_render_view_messages.h"
#include "xwalk/runtime/renderer/android/xwalk_permission_client.h"
#include "xwalk/runtime/renderer/android/xwalk_render_thread_observer.h"
#include "xwalk/runtime/renderer/android/xwalk_render_frame_ext.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#endif

#if !defined(DISABLE_NACL)
#include "components/nacl/renderer/nacl_helper.h"
#endif

using content::RenderThread;

namespace xwalk {

namespace {

xwalk::XWalkContentRendererClient* g_renderer_client;

class XWalkFrameHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<XWalkFrameHelper> {
 public:
  XWalkFrameHelper(
      content::RenderFrame* render_frame,
      extensions::XWalkExtensionRendererController* extension_controller)
      : content::RenderFrameObserver(render_frame),
        content::RenderFrameObserverTracker<XWalkFrameHelper>(render_frame),
        extension_controller_(extension_controller) {}
  ~XWalkFrameHelper() override {}

  // RenderFrameObserver implementation.
  void DidCreateScriptContext(v8::Handle<v8::Context> context,
                              int world_id) override {
    if (extension_controller_)
      extension_controller_->DidCreateScriptContext(
          render_frame()->GetWebFrame(), context);
  }
  void WillReleaseScriptContext(v8::Handle<v8::Context> context,
                                int world_id) override {
    if (extension_controller_)
      extension_controller_->WillReleaseScriptContext(
          render_frame()->GetWebFrame(), context);
  }

  void OnDestruct() override {
    delete this;
  }

 private:
  extensions::XWalkExtensionRendererController* extension_controller_;

  DISALLOW_COPY_AND_ASSIGN(XWalkFrameHelper);
};

}  // namespace

XWalkContentRendererClient* XWalkContentRendererClient::Get() {
  return g_renderer_client;
}

XWalkContentRendererClient::XWalkContentRendererClient() {
  DCHECK(!g_renderer_client);
  g_renderer_client = this;
}

XWalkContentRendererClient::~XWalkContentRendererClient() {
  g_renderer_client = NULL;
}

void XWalkContentRendererClient::RenderThreadStarted() {
  LOG(INFO) << __func__;
  content::RenderThread* thread = content::RenderThread::Get();

  thread->SetRendererProcessType(blink::scheduler::RendererProcessType::kRenderer);

  xwalk_render_thread_observer_.reset(new XWalkRenderThreadObserver);
  thread->AddObserver(xwalk_render_thread_observer_.get());

  auto registry = base::MakeUnique<service_manager::BinderRegistry>();

  visited_link_slave_.reset(new visitedlink::VisitedLinkSlave);
  registry->AddInterface(visited_link_slave_->GetBindCallback(),
                           base::ThreadTaskRunnerHandle::Get());

  content::ChildThread::Get()
      ->GetServiceManagerConnection()
      ->AddConnectionFilter(base::MakeUnique<content::SimpleConnectionFilter>(
          std::move(registry)));


  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kXWalkDisableExtensions))
    extension_controller_.reset(
        new extensions::XWalkExtensionRendererController(this));
}

#if defined(OS_ANDROID)
bool XWalkContentRendererClient::HandleNavigation(
    content::RenderFrame* render_frame,
    bool is_content_initiated,
    bool render_view_was_created_by_renderer,
    blink::WebFrame* frame,
    const blink::WebURLRequest& request,
    blink::WebNavigationType type,
    blink::WebNavigationPolicy default_policy,
    bool is_redirect) {
  LOG(INFO) << __func__;
#if TENTA_LOG_ENABLE == 1
  LOG(INFO) << "!!! " << __func__ << " is_content_initiated=" << is_content_initiated
      << " render_view_was_created_by_renderer=" << render_view_was_created_by_renderer
      << " is_redirect=" << is_redirect
      << " type=" << type
      << " is_main_frame=" << !frame->Parent()
      << " url=" << request.Url();
#endif
  // Only GETs can be overridden.
  if (!request.HttpMethod().Equals("GET"))
    return false;

  // Any navigation from loadUrl, and goBack/Forward are considered application-
  // initiated and hence will not yield a shouldOverrideUrlLoading() callback.
  // Webview classic does not consider reload application-initiated so we
  // continue the same behavior.
  // TODO(sgurun) is_content_initiated is normally false for cross-origin
  // navigations but since android_webview does not swap out renderers, this
  // works fine. This will stop working if android_webview starts swapping out
  // renderers on navigation.
  bool application_initiated =
      !is_content_initiated || type == blink::kWebNavigationTypeBackForward;

  // Don't offer application-initiated navigations unless it's a redirect.
  if (application_initiated && !is_redirect)
    return false;

  bool is_main_frame = !frame->Parent();
  const GURL& gurl = request.Url();
  // For HTTP schemes, only top-level navigations can be overridden. Similarly,
  // WebView Classic lets app override only top level about:blank navigations.
  // So we filter out non-top about:blank navigations here.
  if (!is_main_frame &&
      (gurl.SchemeIs(url::kHttpScheme) || gurl.SchemeIs(url::kHttpsScheme) ||
       gurl.SchemeIs(url::kAboutScheme)))
    return false;
  // use NavigationInterception throttle to handle the call as that can
  // be deferred until after the java side has been constructed.
  //
  // TODO(nick): |render_view_was_created_by_renderer| was plumbed in to
  // preserve the existing code behavior, but it doesn't appear to be correct.
  // In particular, this value will be true for the initial navigation of a
  // RenderView created via window.open(), but it will also be true for all
  // subsequent navigations in that RenderView, no matter how they are
  // initiated.
  if (render_view_was_created_by_renderer) {
    return false;
  }

  // TODO(iotto) analyse how to replace opener_id with render_view_was_created_by_renderer

  // use NavigationInterception throttle to handle the call as that can
  // be deferred until after the java side has been constructed.
//  if (opener_id != MSG_ROUTING_NONE)
//    return false;

  bool ignore_navigation = false;
  base::string16 url = request.Url().GetString().Utf16();
  bool has_user_gesture = request.HasUserGesture();

  int render_frame_id = render_frame->GetRoutingID();
  RenderThread::Get()->Send(new XWalkViewHostMsg_ShouldOverrideUrlLoading(
      render_frame_id, url, has_user_gesture, is_redirect, is_main_frame,
      &ignore_navigation));
  return ignore_navigation;
}
#endif

void XWalkContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  LOG(INFO) << __func__;
  new XWalkFrameHelper(render_frame, extension_controller_.get());
  new XWalkRenderFrameExt(render_frame);
#if defined(OS_ANDROID)
  new XWalkPermissionClient(render_frame);
#endif

#if defined(ENABLE_PLUGINS)
  new PepperHelper(render_frame);
#endif

#if !defined(DISABLE_NACL)
  new nacl::NaClHelper(render_frame);
#endif

  // The following code was copied from
  // android_webview/renderer/aw_content_renderer_client.cc
#if defined(OS_ANDROID)
  // TODO(jam): when the frame tree moves into content and parent() works at
  // RenderFrame construction, simplify this by just checking parent().
  content::RenderFrame* parent_frame =
      render_frame->GetRenderView()->GetMainRenderFrame();
  if (parent_frame && parent_frame != render_frame) {
    // Avoid any race conditions from having the browser's UI thread tell the IO
    // thread that a subframe was created.
    RenderThread::Get()->Send(new XWalkViewHostMsg_SubFrameCreated(
        parent_frame->GetRoutingID(), render_frame->GetRoutingID()));
  }

  if (render_frame->IsMainFrame())
    new data_reduction_proxy::ContentPreviewsRenderFrameObserver(render_frame);

#endif
}

void XWalkContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  LOG(INFO) << __func__;
#if defined(OS_ANDROID)
  // TODO (iotto) replaced with XWalkRenderFrameExt
//  XWalkRenderViewExt::RenderViewCreated(render_view);
#endif
}

void XWalkContentRendererClient::DidCreateModuleSystem(
    extensions::XWalkModuleSystem* module_system) {
  std::unique_ptr<extensions::XWalkNativeModule> app_module(
      new application::ApplicationNativeModule());
  module_system->RegisterNativeModule("application", std::move(app_module));
  std::unique_ptr<extensions::XWalkNativeModule> isolated_file_system_module(
      new extensions::IsolatedFileSystem());
  module_system->RegisterNativeModule("isolated_file_system",
      std::move(isolated_file_system_module));
  module_system->RegisterNativeModule("sysapps_common",
      extensions::CreateJSModuleFromResource(IDR_XWALK_SYSAPPS_COMMON_API));
  module_system->RegisterNativeModule("widget_common",
      extensions::CreateJSModuleFromResource(
          IDR_XWALK_APPLICATION_WIDGET_COMMON_API));
}

bool XWalkContentRendererClient::IsExternalPepperPlugin(
    const std::string& module_name) {
  // TODO(bbudge) remove this when the trusted NaCl plugin has been removed.
  // We must defer certain plugin events for NaCl instances since we switch
  // from the in-process to the out-of-process proxy after instantiating them.
  return module_name == "Native Client";
}

unsigned long long XWalkContentRendererClient::VisitedLinkHash(
    const char* canonical_url, size_t length) {
  return visited_link_slave_->ComputeURLFingerprint(canonical_url, length);
}

bool XWalkContentRendererClient::IsLinkVisited(unsigned long long link_hash) {
  return visited_link_slave_->IsVisited(link_hash);
}

bool XWalkContentRendererClient::WillSendRequest(
                       blink::WebLocalFrame* frame,
                       ui::PageTransition transition_type,
                       const blink::WebURL& url,
                       std::vector<std::unique_ptr<content::URLLoaderThrottle>>* throttles,
                       GURL* new_url) {
  LOG(INFO) << __func__;
#if TENTA_LOG_NET_ENABLE == 1
  LOG(INFO) << "XWalkContentRendererClient::WillSendRequest doc_url="
               << frame->GetDocument().Url().GetString().Utf8() << " url="
               << url.GetString().Utf8();
#endif
#if defined(OS_ANDROID)
  content::RenderFrame* render_frame =
      content::RenderFrame::FromWebFrame(frame);

  if ( render_frame == nullptr ) {
    return false; // no overwrite
  }

  int render_frame_id = render_frame->GetRoutingID();

  bool did_overwrite = false;
  std::string url_str = url.GetString().Utf8();
  std::string new_url_str;

  RenderThread::Get()->Send(new XWalkViewHostMsg_WillSendRequest(render_frame_id,
                                                                 url_str,
                                                                 transition_type,
                                                                 &new_url_str,
                                                                 &did_overwrite));

  if ( did_overwrite ) {
    *new_url = GURL(new_url_str);
#if TENTA_LOG_NET_ENABLE == 1
    LOG(INFO) << "XWalkContentRendererClient::WillSendRequest did_overwrite";
#endif
  }
  return did_overwrite;
#else
  // TODO(iotto) for other than android implement
/*  if (!xwalk_render_thread_observer_->IsWarpMode() &&
      !xwalk_render_thread_observer_->IsCSPMode())
    return false;

  GURL origin_url(frame->document().url());
  GURL app_url(xwalk_render_thread_observer_->app_url());
  // if under CSP mode.
  if (xwalk_render_thread_observer_->IsCSPMode()) {
    if (!origin_url.is_empty() && origin_url != first_party_for_cookies &&
        !xwalk_render_thread_observer_->CanRequest(app_url, url)) {
      LOG(INFO) << "[BLOCK] allow-navigation: " << url.spec();
      content::RenderThread::Get()->Send(new ViewMsg_OpenLinkExternal(url));
      *new_url = GURL();
      return true;
    }
    return false;
  }

  // if under WARP mode.
  if (url.GetOrigin() == app_url.GetOrigin() ||
      xwalk_render_thread_observer_->CanRequest(app_url, url)) {
    DLOG(INFO) << "[PASS] " << origin_url.spec() << " request " << url.spec();
    return false;
  }

  LOG(INFO) << "[BLOCK] " << origin_url.spec() << " request " << url.spec();
  *new_url = GURL();
  return true;
*/
#endif
}

void XWalkContentRendererClient::GetNavigationErrorStrings(
    content::RenderFrame* render_frame,
    const blink::WebURLRequest& failed_request,
    const blink::WebURLError& error,
    std::string* error_html,
    base::string16* error_description) {
  LOG(INFO) << __func__;
  // TODO(guangzhen): Check whether error_html is needed in xwalk runtime.

  if (error_description) {
    if (error.localized_description.IsEmpty())
      *error_description = base::ASCIIToUTF16(net::ErrorToString(error.reason));
    else
      *error_description = error.localized_description.Utf16();
  }
}

void XWalkContentRendererClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<::media::KeySystemProperties>>* key_systems) {
#if defined(OS_ANDROID)
  cdm::AddAndroidWidevine(key_systems);
  cdm::AddAndroidPlatformKeySystems(key_systems);
#endif  // defined(OS_ANDROID)
}

bool XWalkContentRendererClient::ShouldUseMediaPlayerForURL(const GURL& url) {
  LOG(INFO) << __func__;
  // TODO(iotto): Implement

//  // Android WebView needs to support codecs that Chrome does not, for these
//  // cases we must force the usage of Android MediaPlayer instead of Chrome's
//  // internal player.
//  //
//  // Note: Despite these extensions being forwarded for playback to MediaPlayer,
//  // HTMLMediaElement.canPlayType() will return empty for these containers.
//  // TODO(boliu): If this is important, extend media::MimeUtil for WebView.
//  //
//  // Format list mirrors:
//  // http://developer.android.com/guide/appendix/media-formats.html
//  //
//  // Enum and extension list are parallel arrays and must stay in sync. These
//  // enum values are written to logs. New enum values can be added, but existing
//  // enums must never be renumbered or deleted and reused.
//  enum MediaPlayerContainers {
//    CONTAINER_3GP = 0,
//    CONTAINER_TS = 1,
//    CONTAINER_MID = 2,
//    CONTAINER_XMF = 3,
//    CONTAINER_MXMF = 4,
//    CONTAINER_RTTTL = 5,
//    CONTAINER_RTX = 6,
//    CONTAINER_OTA = 7,
//    CONTAINER_IMY = 8,
//    MEDIA_PLAYER_CONTAINERS_COUNT,
//  };
//  static const char* kMediaPlayerExtensions[] = {
//      ".3gp", ".ts", ".mid", ".xmf", ".mxmf", ".rtttl", ".rtx", ".ota", ".imy"};
//  static_assert(arraysize(kMediaPlayerExtensions) ==
//                    MediaPlayerContainers::MEDIA_PLAYER_CONTAINERS_COUNT,
//                "Invalid enum or extension change.");
//
//  for (size_t i = 0; i < arraysize(kMediaPlayerExtensions); ++i) {
//    if (base::EndsWith(url.path(), kMediaPlayerExtensions[i],
//                       base::CompareCase::INSENSITIVE_ASCII)) {
//      UMA_HISTOGRAM_ENUMERATION(
//          "Media.WebView.UnsupportedContainer",
//          static_cast<MediaPlayerContainers>(i),
//          MediaPlayerContainers::MEDIA_PLAYER_CONTAINERS_COUNT);
//      return true;
//    }
//  }
  return false;
}

std::unique_ptr<blink::WebSocketHandshakeThrottle>
XWalkContentRendererClient::CreateWebSocketHandshakeThrottle() {
  LOG(INFO) << __func__;
  if (!UsingSafeBrowsingMojoService())
    return nullptr;
  return base::MakeUnique<safe_browsing::WebSocketSBHandshakeThrottle>(
      safe_browsing_.get());
}

bool XWalkContentRendererClient::UsingSafeBrowsingMojoService() {
  if (safe_browsing_)
    return true;
  LOG(INFO) << __func__ << " IsEnabled(features::kNetworkService)=" << base::FeatureList::IsEnabled(features::kNetworkService);
  if (!base::FeatureList::IsEnabled(features::kNetworkService))
    return false;
  RenderThread::Get()->GetConnector()->BindInterface(
      content::mojom::kBrowserServiceName, &safe_browsing_);
  return true;
}

}  // namespace xwalk
