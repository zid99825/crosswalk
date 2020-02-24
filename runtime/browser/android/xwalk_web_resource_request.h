/*
 * xwalk_web_resource_request.h
 *
 *  Created on: Feb 22, 2020
 *      Author: iotto
 */

#ifndef XWALK_RUNTIME_BROWSER_ANDROID_XWALK_WEB_RESOURCE_REQUEST_H_
#define XWALK_RUNTIME_BROWSER_ANDROID_XWALK_WEB_RESOURCE_REQUEST_H_

#include <string>
#include <vector>

#include "base/optional.h"

namespace network {
struct ResourceRequest;
}

namespace xwalk {

struct XWalkWebResourceRequest {
 public:
  explicit XWalkWebResourceRequest(const network::ResourceRequest& request);

  // Add default copy/move/assign operators. Adding explicit destructor
  // prevents generating move operator.
  XWalkWebResourceRequest(XWalkWebResourceRequest&& other);
  XWalkWebResourceRequest& operator=(XWalkWebResourceRequest&& other);

  XWalkWebResourceRequest();
  virtual ~XWalkWebResourceRequest();

  std::string url;
  std::string method;
  bool is_main_frame;
  bool has_user_gesture;
  std::vector<std::string> header_names;
  std::vector<std::string> header_values;
  base::Optional<bool> is_renderer_initiated;
};

} /* namespace xwalk */

#endif /* XWALK_RUNTIME_BROWSER_ANDROID_XWALK_WEB_RESOURCE_REQUEST_H_ */
