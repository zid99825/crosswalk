/*
 * fs_factory_sqlite.h
 *
 *  Created on: Feb 6, 2017
 *      Author: iotto
 */

#ifndef SQLITE_FS_FACTORY_SQLITE_H_
#define SQLITE_FS_FACTORY_SQLITE_H_

#include <map>

#include "base/fs_factory.h"
#include "fs_delegate_sqlite.h"

namespace xwalk {
namespace tenta {

class FsFactorySqlite : public FsFactory {
 public:
  FsFactorySqlite();
  virtual ~FsFactorySqlite();

  virtual int CreateDelegate(const std::string& rootPath,
                             std::weak_ptr<FsDelegate> &out);

 private:
  std::map<std::string, std::shared_ptr<FsDelegateSqlite>> _dbHandlers;
  typedef std::map<std::string, std::shared_ptr<FsDelegateSqlite>>::iterator DbIt;
  typedef std::pair<std::string, std::shared_ptr<FsDelegateSqlite>> InPair;
};

} /* namespace tenta */
} /* namespace xwalk */

#endif /* SQLITE_FS_FACTORY_SQLITE_H_ */
