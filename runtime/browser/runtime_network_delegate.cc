// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/runtime_network_delegate.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"
#include "net/url_request/url_request.h"
#include "meta_logging.h"

#if defined(OS_ANDROID)
#include "xwalk/runtime/browser/android/xwalk_contents_io_thread_client.h"
#include "xwalk/runtime/browser/android/xwalk_cookie_access_policy.h"
#endif

#ifdef TENTA_CHROMIUM_BUILD
#include "extensions/browser/info_map.h"
#include "xwalk/runtime/browser/xwalk_browser_context.h"
#include "xwalk/runtime/browser/xwalk_runner.h"

// tenta
#include "browser/tenta_extensions_network_delegate.h"


using namespace tenta::ext;
using namespace extensions;
#endif

using content::BrowserThread;

namespace xwalk {

RuntimeNetworkDelegate::RuntimeNetworkDelegate() {
#ifdef TENTA_CHROMIUM_BUILD
  _extensions_delegate.reset(new TentaExtensionsNetworkDelegate(XWalkRunner::GetInstance()->browser_context()));
#endif
}

RuntimeNetworkDelegate::~RuntimeNetworkDelegate() {
}

#ifdef TENTA_CHROMIUM_BUILD
void RuntimeNetworkDelegate::set_extension_info_map(InfoMap* extension_info_map) {
  _extensions_delegate->set_extension_info_map(extension_info_map);
}
#endif

int RuntimeNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
  // TODO(iotto): Insert DoNotTrack

#ifdef TENTA_CHROMIUM_BUILD
  _extensions_delegate->ForwardStartRequestStatus(request);

  return _extensions_delegate->OnBeforeURLRequest(request, callback, new_url);
#else
  return net::OK;
#endif
}

int RuntimeNetworkDelegate::OnBeforeStartTransaction(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
#ifdef TENTA_CHROMIUM_BUILD
  return _extensions_delegate->OnBeforeStartTransaction(request, callback, headers);
#else
  return net::OK;
#endif
}

void RuntimeNetworkDelegate::OnStartTransaction(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {
#ifdef TENTA_CHROMIUM_BUILD
  _extensions_delegate->OnStartTransaction(request, headers);
#endif
}

int RuntimeNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
#if defined(OS_ANDROID)
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int render_process_id, render_frame_id;
  if (content::ResourceRequestInfo::GetRenderFrameForRequest(
      request, &render_process_id, &render_frame_id)) {
    std::unique_ptr<XWalkContentsIoThreadClient> io_thread_client =
        XWalkContentsIoThreadClient::FromID(render_process_id, render_frame_id);
    if (io_thread_client.get()) {
      io_thread_client->OnReceivedResponseHeaders(request,
          original_response_headers);
    }
  }
#endif


#ifdef TENTA_CHROMIUM_BUILD
  return _extensions_delegate->OnHeadersReceived(request, callback, original_response_headers,
                                                 override_response_headers, allowed_unsafe_redirect_url);
#else
  return net::OK;
#endif
}

void RuntimeNetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                              const GURL& new_location) {
  TENTA_LOG_NET(INFO) << __func__ << " url=" << new_location.spec();
#ifdef TENTA_CHROMIUM_BUILD
  _extensions_delegate->OnBeforeRedirect(request, new_location);
#endif
}

void RuntimeNetworkDelegate::OnResponseStarted(net::URLRequest* request, int net_error) {
#ifdef TENTA_CHROMIUM_BUILD
  _extensions_delegate->OnResponseStarted(request, net_error);
#endif
}

void RuntimeNetworkDelegate::OnNetworkBytesReceived(net::URLRequest* request,
                                                    int64_t bytes_received) {
}

void RuntimeNetworkDelegate::OnCompleted(net::URLRequest* request,
                                         bool started, int net_error) {
#ifdef TENTA_CHROMIUM_BUILD
  _extensions_delegate->OnCompleted(request, started, net_error);
//  _extensions_delegate->ForwardProxyErrors(request, net_error);
  _extensions_delegate->ForwardDoneRequestStatus(request);
#endif
}

void RuntimeNetworkDelegate::OnURLRequestDestroyed(net::URLRequest* request) {
#ifdef TENTA_CHROMIUM_BUILD
  _extensions_delegate->OnURLRequestDestroyed(request);
#endif
}

void RuntimeNetworkDelegate::OnPACScriptError(int line_number,
                                              const base::string16& error) {
#ifdef TENTA_CHROMIUM_BUILD
  _extensions_delegate->OnPACScriptError(line_number, error);
#endif
}

RuntimeNetworkDelegate::AuthRequiredResponse
RuntimeNetworkDelegate::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    net::AuthCredentials* credentials) {
#ifdef TENTA_CHROMIUM_BUILD
  return _extensions_delegate->OnAuthRequired(request, auth_info, callback, credentials);
#else
  return AUTH_REQUIRED_RESPONSE_NO_ACTION;
#endif
}

bool RuntimeNetworkDelegate::OnCanGetCookies(
    const net::URLRequest& request,
    const net::CookieList& cookie_list) {
#if defined(OS_ANDROID)
  return XWalkCookieAccessPolicy::GetInstance()->OnCanGetCookies(
    request, cookie_list);
#else
  return true;
#endif
}

bool RuntimeNetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                            const net::CanonicalCookie& cookie,
                                            net::CookieOptions* options) {
#if defined(OS_ANDROID)
  return XWalkCookieAccessPolicy::GetInstance()->OnCanSetCookie(request,
                                                                cookie,
                                                                options);
#else
  return true;
#endif
}

bool RuntimeNetworkDelegate::OnCanAccessFile(
    const net::URLRequest& request, const base::FilePath& original_path,
    const base::FilePath& absolute_path) const {
  return true;
}

}  // namespace xwalk
