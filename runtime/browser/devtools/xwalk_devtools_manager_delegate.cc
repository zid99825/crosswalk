// Copyright 2013 The Chromium Authors. All rights reserved.
// Copyright 2015 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/devtools/xwalk_devtools_manager_delegate.h"

#include <string>
#include <vector>

#include "base/atomicops.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
//#include "components/devtools_discovery/basic_target_descriptor.h"
//#include "components/devtools_discovery/devtools_discovery_manager.h"
//#include "components/devtools_http_handler/devtools_http_handler.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_content_client.h"
#include "grit/xwalk_resources.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_server_socket.h"
#include "net/log/net_log_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "xwalk/runtime/common/xwalk_content_client.h"
#include "xwalk/runtime/browser/runtime.h"
#include "xwalk/runtime/browser/xwalk_browser_context.h"


namespace xwalk {

namespace {

const int kBackLog = 10;
const char kLocalHost[] = "127.0.0.1";
base::subtle::Atomic32 g_last_used_port;

uint16_t GetInspectorPort() {
  const base::CommandLine& command_line =
    *base::CommandLine::ForCurrentProcess();
  // See if the user specified a port on the command line (useful for
  // automation). If not, use an ephemeral port by specifying 0.
  uint16_t port = 0;
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    int temp_port;
    std::string port_str =
      command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    if (base::StringToInt(port_str, &temp_port) &&
      temp_port > 0 && temp_port < 65535) {
      port = static_cast<uint16_t>(temp_port);
    } else {
      DLOG(WARNING) << "Invalid http debugger port number " << temp_port;
    }
  }
  return port;
}

class TCPServerSocketFactory
    : public content::DevToolsSocketFactory {
 public:
  TCPServerSocketFactory(const std::string& address, uint16_t port)
      : address_(address), port_(port) {
  }

 private:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));
    if (socket->ListenWithAddressAndPort(address_, port_, kBackLog) != net::OK)
      return std::unique_ptr<net::ServerSocket>();

    net::IPEndPoint endpoint;
    if (socket->GetLocalAddress(&endpoint) == net::OK)
      base::subtle::NoBarrier_Store(&g_last_used_port, endpoint.port());

    return socket;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* name) override {
    return nullptr;
  }

  std::string address_;
  uint16_t port_;

  DISALLOW_COPY_AND_ASSIGN(TCPServerSocketFactory);
};

std::unique_ptr<content::DevToolsSocketFactory>
CreateSocketFactory(uint16_t port) {
  return std::unique_ptr<content::DevToolsSocketFactory>(
      new TCPServerSocketFactory(kLocalHost, port));
}
/*
scoped_refptr<content::DevToolsAgentHost>
CreateNewShellTarget(XWalkBrowserContext* browser_context, const GURL& url) {
  Runtime* runtime = Runtime::Create(browser_context);
  return content::DevToolsAgentHost::GetOrCreateFor(runtime->web_contents());
}

class XWalkDevToolsDelegate :
    public devtools_http_handler::DevToolsHttpHandlerDelegate {
 public:
  explicit XWalkDevToolsDelegate(XWalkBrowserContext* browser_context);
  ~XWalkDevToolsDelegate() override;

  // devtools_http_handler::DevToolsHttpHandlerDelegate implementation.
  std::string GetDiscoveryPageHTML() override;
  std::string GetFrontendResource(const std::string& path) override;
  std::string GetPageThumbnailData(const GURL& url) override;
  content::DevToolsExternalAgentProxyDelegate*
      HandleWebSocketConnection(const std::string& path) override;

 private:
  XWalkBrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(XWalkDevToolsDelegate);
};

XWalkDevToolsDelegate::XWalkDevToolsDelegate(
    XWalkBrowserContext* browser_context)
    : browser_context_(browser_context) {
  devtools_discovery::DevToolsDiscoveryManager::GetInstance()->
      SetCreateCallback(base::Bind(&CreateNewShellTarget,
                                   base::Unretained(browser_context_)));
}

XWalkDevToolsDelegate::~XWalkDevToolsDelegate() {
  devtools_discovery::DevToolsDiscoveryManager::GetInstance()->
      SetCreateCallback(
          devtools_discovery::DevToolsDiscoveryManager::CreateCallback());
}

std::string XWalkDevToolsDelegate::GetDiscoveryPageHTML() {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_DEVTOOLS_FRONTEND_PAGE_HTML).as_string();
}

std::string XWalkDevToolsDelegate::GetFrontendResource(
    const std::string& path) {
  return content::DevToolsFrontendHost::GetFrontendResource(path).as_string();
}

std::string XWalkDevToolsDelegate::GetPageThumbnailData(const GURL& url) {
  return std::string();
}

content::DevToolsExternalAgentProxyDelegate*
XWalkDevToolsDelegate::HandleWebSocketConnection(const std::string& path) {
  return nullptr;
}
*/
}  // namespace

XWalkDevToolsManagerDelegate::XWalkDevToolsManagerDelegate(XWalkBrowserContext* browserContext)
  : _browser_context(browserContext)
{
}

// static
int XWalkDevToolsManagerDelegate::GetHttpHandlerPort() {
  return base::subtle::NoBarrier_Load(&g_last_used_port);
}

//static
void XWalkDevToolsManagerDelegate::StartHttpHandler(XWalkBrowserContext* browserContext) {
  std::string frontend_url;
/*
 #if defined(OS_ANDROID)
   frontend_url = base::StringPrintf(kFrontEndURL, GetWebKitRevision().c_str());
 #endif
*/
  content::DevToolsAgentHost::StartRemoteDebuggingServer(
    CreateSocketFactory(GetInspectorPort()),
    frontend_url,
    browserContext->GetPath(),
    base::FilePath());
}

// static
void XWalkDevToolsManagerDelegate::StopHttpHandler() {
  content::DevToolsAgentHost::StopRemoteDebuggingServer();
}

scoped_refptr<content::DevToolsAgentHost> 
XWalkDevToolsManagerDelegate::CreateNewTarget(const GURL& url) {
  Runtime* runtime = Runtime::Create(_browser_context);
  return content::DevToolsAgentHost::GetOrCreateFor(runtime->web_contents());
}

std::string XWalkDevToolsManagerDelegate::GetDiscoveryPageHTML() {
#if defined(OS_ANDROID)
  return std::string();
#else
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_DEVTOOLS_FRONTEND_PAGE_HTML).as_string();
#endif
}

std::string XWalkDevToolsManagerDelegate::GetFrontendResource(
    const std::string& path) {
#if defined(OS_ANDROID)
  return std::string();
#else
  return content::DevToolsFrontendHost::GetFrontendResource(path).as_string();
#endif

}

XWalkDevToolsManagerDelegate::~XWalkDevToolsManagerDelegate() {
}

}  // namespace xwalk
