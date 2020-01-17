/*
 * xwalk_content_gpu_client.cc
 *
 *  Created on: Jan 16, 2020
 *      Author: iotto
 */

#include "xwalk/runtime/gpu/xwalk_content_gpu_client.h"

namespace xwalk {

XWalkContentGpuClient::XWalkContentGpuClient(const GetSyncPointManagerCallback& sync_point_manager_callback,
                                             const GetSharedImageManagerCallback& shared_image_manager_callback)
    : sync_point_manager_callback_(sync_point_manager_callback),
      shared_image_manager_callback_(shared_image_manager_callback) {
}

XWalkContentGpuClient::~XWalkContentGpuClient() {
  // TODO Auto-generated destructor stub
}

gpu::SyncPointManager* XWalkContentGpuClient::GetSyncPointManager() {
  return sync_point_manager_callback_.Run();
}

gpu::SharedImageManager* XWalkContentGpuClient::GetSharedImageManager() {
  return shared_image_manager_callback_.Run();
}

} /* namespace xwalk */
