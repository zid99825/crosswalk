/*
 * file_block_sqlite.cc
 *
 *  Created on: Feb 10, 2017
 *      Author: iotto
 */

#include <xwalk/runtime/file_block_db/file_block_sqlite.h>

namespace xwalk {
namespace tenta {

FileBlockSqlite::FileBlockSqlite() {
  // TODO Auto-generated constructor stub

}

FileBlockSqlite::~FileBlockSqlite() {
  // TODO Auto-generated destructor stub
}

int FileBlockSqlite::Open(const std::string& rootPath, const std::string& fileName, FsDelegate::OpenMode mode, const std::string& encKey) {
  // create delegate
  _fileName = fileName;
//
//  _sqlite.
//  _fsDelegate.reset();
//  DbIt found = _dbHandlers.find(rootPath);
//  if (found != _dbHandlers.end()) {
//    out = std::weak_ptr<FsDelegateSqlite>(found->second);
//    return 0;  // TODO define return codes
//  }
//
//  std::shared_ptr<FsDelegateSqlite> sp(new FsDelegateSqlite());
//  int result = sp->Init(rootPath);
//
//  if (result == 0) {
//    _dbHandlers.insert(InPair(rootPath, sp));
//    out = std::weak_ptr<FsDelegateSqlite>(sp);
//  }
//
//  return result;
//
//  if (result == 0) {
//    TShDelegate d = _fsDelegate.lock();
//    result = d->FileOpen(fileName, FsDelegate::OpenMode::OM_CREATE_IF_NOT_EXISTS);
//  }
//
//  return result;
}

} /* namespace tenta */
} /* namespace xwalk */
