/*
 * file_block_sqlite.h
 *
 *  Created on: Feb 10, 2017
 *      Author: iotto
 */

#ifndef XWALK_RUNTIME_FILE_BLOCK_DB_FILE_BLOCK_SQLITE_H_
#define XWALK_RUNTIME_FILE_BLOCK_DB_FILE_BLOCK_SQLITE_H_

#include <memory>
#include <xwalk/runtime/file_block_db/file_block.h>

namespace xwalk {
namespace tenta {

class FileBlockSqlite : public FileBlock {
 public:
  FileBlockSqlite();
  virtual ~FileBlockSqlite();

  virtual int Open(const std::string& rootPath, const std::string& fileName, FsDelegate::OpenMode mode, const std::string& encKey);
  /**
   * Delete file
   */
  virtual int Delete(bool permanent = true) override;

  /**
   * Close file
   * @return 0 if success; < 0 if error
   */
  virtual int Close() override;

  /**
   * Return length of file
   * return <0 if error
   */
  virtual int Length() override;

  /**
   * Write data to file
   * If has content beyond offset, will delete first
   * @return 0 if success; < 0 if error
   */
  virtual int Write(const char* data, int len, int offset) override;

  /**
   * Append content to file
   * @return 0 if success; < 0 if error
   */
  virtual int Append(const char* data, int len) override;

  /**
   * Read inLen from file starting at offset
   * OutLen = data read
   * @return 0 if success; < 0 if error
   */
  virtual int Read(char* dst, int *inOutLen, int offset) override;

 protected:
  std::unique_ptr<FsDelegateSqlite> _sqlite;
};

} /* namespace tenta */
} /* namespace xwalk */

#endif /* XWALK_RUNTIME_FILE_BLOCK_DB_FILE_BLOCK_SQLITE_H_ */
