/*
 * fs_factory_sqlite.cc
 *
 *  Created on: Feb 6, 2017
 *      Author: iotto
 */

#include "fs_factory_sqlite.h"

namespace xwalk {
namespace tenta {

FsFactorySqlite::FsFactorySqlite() {
  // TODO Auto-generated constructor stub

}

FsFactorySqlite::~FsFactorySqlite() {
  // TODO Auto-generated destructor stub
}

int FsFactorySqlite::CreateDelegate(const std::string& rootPath,
                                    std::weak_ptr<FsDelegate> &out) {

  DbIt found = _dbHandlers.find(rootPath);
  if (found != _dbHandlers.end()) {
    out = std::weak_ptr<FsDelegateSqlite>(found->second);
    return 0;  // TODO define return codes
  }

  std::shared_ptr<FsDelegateSqlite> sp(new FsDelegateSqlite());
  int result = sp->Init(rootPath);

  if (result == 0) {
    _dbHandlers.insert(InPair(rootPath, sp));
    out = std::weak_ptr<FsDelegateSqlite>(sp);
  }

  return result;
}

} /* namespace tenta */
} /* namespace xwalk */
