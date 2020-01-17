// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/app/android/xwalk_main_delegate_android.h"

#include <memory>
#include <string>

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
#include "content/public/browser/browser_main_runner.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"
#include "xwalk/runtime/common/android/xwalk_globals_android.h"
#include "xwalk/runtime/common/xwalk_content_client.h"
#include "xwalk/runtime/common/xwalk_paths.h"
#include "xwalk/runtime/common/xwalk_resource_delegate.h"
#include "xwalk/runtime/gpu/xwalk_content_gpu_client.h"

namespace xwalk {

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
  content_client_.reset(new XWalkContentClient);
  content::SetContentClient(content_client_.get());

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);
  logging::SetLogItems(true /* Process ID */, true /* Thread ID */,
                       true /* Timestamp */, false /* Tick count */);

  RegisterPathProvider();
  return false;
}

void XWalkMainDelegateAndroid::PreSandboxStartup() {
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

void XWalkMainDelegateAndroid::InitResourceBundle() {
  ui::SetLocalePaksStoredInApk(true);

  base::FilePath pak_file;
  base::FilePath pak_dir;
  bool got_path = base::PathService::Get(base::DIR_ANDROID_APP_DATA, &pak_dir);
  DCHECK(got_path);
  pak_dir = pak_dir.Append(FILE_PATH_LITERAL("paks"));
  pak_file = pak_dir.Append(FILE_PATH_LITERAL(kXWalkPakFilePath));

  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
  pak_file = pak_dir.Append(FILE_PATH_LITERAL("xwalk_100_percent.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_file, ui::SCALE_FACTOR_100P);
}

// TODO(iotto): Continue implementation
//content::ContentGpuClient* XWalkMainDelegateAndroid::CreateContentGpuClient() {
//  content_gpu_client_ = std::make_unique<XWalkContentGpuClient>(
//      base::BindRepeating(&GetSyncPointManager),
//      base::BindRepeating(&GetSharedImageManager));
//  return content_gpu_client_.get();
//}

}  // namespace xwalk
