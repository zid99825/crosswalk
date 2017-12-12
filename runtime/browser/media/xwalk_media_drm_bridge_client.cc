/*
 * xwalk_media_drm_bridge_client.cc
 *
 *  Created on: Dec 1, 2017
 *      Author: iotto
 */

#include "xwalk/runtime/browser/media/xwalk_media_drm_bridge_client.h"

namespace xwalk {

XWalkMediaDrmBridgeClient::XWalkMediaDrmBridgeClient() {
}

XWalkMediaDrmBridgeClient::~XWalkMediaDrmBridgeClient() {
}

media::MediaDrmBridgeDelegate*
XWalkMediaDrmBridgeClient::GetMediaDrmBridgeDelegate(
    const std::vector<uint8_t>& scheme_uuid) {
  if (scheme_uuid == widevine_delegate_.GetUUID())
    return &widevine_delegate_;
  return nullptr;
}

} /* namespace xwalk */
