// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/app/android/xwalk_main_delegate_android.h"

#include <memory>
#include <string>

#include "base/android/apk_assets.h"
#include "base/android/locale_utils.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/base_paths_android.h"
#include "base/path_service.h"
#include "base/files/file.h"
#include "base/posix/global_descriptors.h"
#include "cc/base/switches.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_switches.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"
#include "xwalk/runtime/common/android/xwalk_descriptors.h"
#include "xwalk/runtime/common/android/xwalk_globals_android.h"
#include "xwalk/runtime/common/xwalk_content_client.h"
#include "xwalk/runtime/common/xwalk_paths.h"
#include "xwalk/runtime/common/xwalk_resource_delegate.h"
#include "xwalk/runtime/gpu/xwalk_content_gpu_client.h"

namespace xwalk {

namespace {
void InitIcuAndResourceBundleBrowserSide() {
  // TODO(iotto): load manually instead of LOAD_COMMON_RESOURCES
  // rename in resources Build.gn chrome_100 to xwalk_100
//  void ResourceBundle::LoadCommonResources() {
//    base::FilePath disk_path;
//    base::PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &disk_path);
//    disk_path = disk_path.AppendASCII("chrome_100_percent.pak");
//    bool success =
//        LoadFromApkOrFile("assets/chrome_100_percent.pak", &disk_path,
//                          &g_chrome_100_percent_fd, &g_chrome_100_percent_region);
//    DCHECK(success);
//
//    AddDataPackFromFileRegion(base::File(g_chrome_100_percent_fd),
//                              g_chrome_100_percent_region, SCALE_FACTOR_100P);
//  }
  ui::SetLocalePaksStoredInApk(true);
  std::string locale = ui::ResourceBundle::InitSharedInstanceWithLocale(
      base::android::GetDefaultLocaleString(), NULL,
      ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  if (locale.empty()) {
    LOG(WARNING) << __func__ << " Failed to load locale .pak from apk.";
  }
  base::i18n::SetICUDefaultLocale(locale);

  // Try to directly mmap the resources.pak from the apk. Fall back to load
  // from file, using PATH_SERVICE, otherwise.
  base::FilePath pak_file_path;
  base::PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_file_path);
  pak_file_path = pak_file_path.AppendASCII("xwalk.pak");
  ui::LoadMainAndroidPackFile("assets/xwalk.pak", pak_file_path);
}

void InitResourceBundleRendererSide() {
  LOG(INFO) << "iotto " << __func__;
  auto* global_descriptors = base::GlobalDescriptors::GetInstance();
  int pak_fd = global_descriptors->Get(kXWalkMainPakDescriptor);
  base::MemoryMappedFile::Region pak_region = global_descriptors->GetRegion(kXWalkMainPakDescriptor);
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(base::File(pak_fd), pak_region);

  std::pair<int, ui::ScaleFactor> extra_paks[] = { { kXWalkMainPakDescriptor, ui::SCALE_FACTOR_NONE }, {
      kXWalk100PakDescriptor, ui::SCALE_FACTOR_100P } };

  for (const auto& pak_info : extra_paks) {
    pak_fd = global_descriptors->Get(pak_info.first);
    pak_region = global_descriptors->GetRegion(pak_info.first);
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(base::File(pak_fd), pak_region, pak_info.second);
  }
}
}
// TODO(iotto): Continue
//namespace {
//gpu::SyncPointManager* GetSyncPointManager() {
//  DCHECK(DeferredGpuCommandService::GetInstance());
//  return DeferredGpuCommandService::GetInstance()->sync_point_manager();
//}
//gpu::SharedImageManager* GetSharedImageManager() {
//  DCHECK(DeferredGpuCommandService::GetInstance());
//  const bool enable_shared_image =
//      base::CommandLine::ForCurrentProcess()->HasSwitch(
//          switches::kWebViewEnableSharedImage);
//  return enable_shared_image
//             ? DeferredGpuCommandService::GetInstance()->shared_image_manager()
//             : nullptr;
//}
//}  // namespace

XWalkMainDelegateAndroid::XWalkMainDelegateAndroid() {
}

XWalkMainDelegateAndroid::~XWalkMainDelegateAndroid() {
}

bool XWalkMainDelegateAndroid::BasicStartupComplete(int* exit_code) {
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();

  command_line.AppendSwitch(cc::switches::kDisableCheckerImaging);

  // Always disable the unsandbox GPU process for DX12 and Vulkan Info
  // collection to avoid interference. This GPU process is launched 15
  // seconds after chrome starts.
  command_line.AppendSwitch(switches::kDisableGpuProcessForDX12VulkanInfoCollection);

  content_client_.reset(new XWalkContentClient);
  content::SetContentClient(content_client_.get());

//  logging::LoggingSettings settings;
//  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
//  logging::InitLogging(settings);
//  logging::SetLogItems(true /* Process ID */, true /* Thread ID */,
//                       true /* Timestamp */, false /* Tick count */);

  // command_line.AppendSwitch(switches::kEnablePartialRaster);
  RegisterPathProvider();
  return false;
}

void XWalkMainDelegateAndroid::PreSandboxStartup() {
  LOG(INFO) << "iotto " << __func__;
#if defined(ARCH_CPU_ARM_FAMILY)
  // Create an instance of the CPU class to parse /proc/cpuinfo and cache
  // cpu_brand info for ARM platform.
  base::CPU cpu_info;
#endif
  InitResourceBundle();
}

int XWalkMainDelegateAndroid::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (process_type.empty()) {
    browser_runner_ = content::BrowserMainRunner::Create();
    int exit_code = browser_runner_->Initialize(main_function_params);
    DCHECK_LT(exit_code, 0);

    return 0;
  }
  return -1;
}

// This function is called only on the browser process.
void XWalkMainDelegateAndroid::PostEarlyInitialization(bool is_running_tests) {
  InitIcuAndResourceBundleBrowserSide();
}

void XWalkMainDelegateAndroid::InitResourceBundle() {
//  AW style
  const base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();

  std::string process_type = command_line.GetSwitchValueASCII(switches::kProcessType);
  const bool is_browser_process = process_type.empty();
  if (!is_browser_process) {
    base::i18n::SetICUDefaultLocale(command_line.GetSwitchValueASCII(switches::kLang));
  }

  if (process_type == switches::kRendererProcess) {
    InitResourceBundleRendererSide();
  }
}

// TODO(iotto): Continue implementation
//content::ContentGpuClient* XWalkMainDelegateAndroid::CreateContentGpuClient() {
//  content_gpu_client_ = std::make_unique<XWalkContentGpuClient>(
//      base::BindRepeating(&GetSyncPointManager),
//      base::BindRepeating(&GetSharedImageManager));
//  return content_gpu_client_.get();
//}

}  // namespace xwalk
