/*
 * fs_factory.h
 *
 *  Created on: Feb 6, 2017
 *      Author: iotto
 */

#ifndef FS_FACTORY_H_
#define FS_FACTORY_H_

#include <memory>
#include "fs_delegate.h"

namespace xwalk {
namespace tenta {

class FsFactory {
 public:
  FsFactory();
  virtual ~FsFactory();
  /**
   * FsDelegate life is in you're hands
   * Delete when not used!
   */
  virtual int CreateDelegate(
      const std::string& rootPath, std::weak_ptr<FsDelegate> &out) = 0;

};

} /* namespace tenta */
} /* namespace xwalk */

#endif /* FS_FACTORY_H_ */
