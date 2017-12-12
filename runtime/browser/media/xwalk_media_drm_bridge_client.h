/*
 * xwalk_media_drm_bridge_client.h
 *
 *  Created on: Dec 1, 2017
 *      Author: iotto
 */

#ifndef XWALK_RUNTIME_BROWSER_MEDIA_XWALK_MEDIA_DRM_BRIDGE_CLIENT_H_
#define XWALK_RUNTIME_BROWSER_MEDIA_XWALK_MEDIA_DRM_BRIDGE_CLIENT_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/cdm/common/widevine_drm_delegate_android.h"
#include "media/base/android/media_drm_bridge_client.h"

namespace xwalk {

class XWalkMediaDrmBridgeClient : public media::MediaDrmBridgeClient {
 public:
  XWalkMediaDrmBridgeClient();
  ~XWalkMediaDrmBridgeClient() override;

 private:
  // media::MediaDrmBridgeClient implementation:
  media::MediaDrmBridgeDelegate* GetMediaDrmBridgeDelegate(
      const std::vector<uint8_t>& scheme_uuid) override;

  cdm::WidevineDrmDelegateAndroid widevine_delegate_;

  DISALLOW_COPY_AND_ASSIGN(XWalkMediaDrmBridgeClient);
};

} /* namespace xwalk */

#endif /* XWALK_RUNTIME_BROWSER_MEDIA_XWALK_MEDIA_DRM_BRIDGE_CLIENT_H_ */
