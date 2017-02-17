/*
 * file_block.h
 *
 *  Created on: Feb 3, 2017
 *      Author: iotto
 */

#ifndef FILE_BLOCK_H_
#define FILE_BLOCK_H_

#include <string>
#include <memory>
#include "base/fs_factory.h"

namespace xwalk {
namespace tenta {

class FileBlock {
 public:
  FileBlock();
  FileBlock(FsFactory *factory);
  virtual ~FileBlock();

  /**
   * Set File System factory, to handle File operations
   */
  void set_factory(FsFactory *factory) {
    _fsFactory = factory;
  }

  /**
   * Open file
   */
  virtual int Open(const std::string& rootPath, const std::string& fileName/*, int rwMode*/);

  /**
   * Delete file
   */
  virtual int Delete(bool permanent = true);

  /**
   * Close file
   * @return 0 if success; < 0 if error
   */
  virtual int Close();

  /**
   * Return length of file
   * return <0 if error
   */
  virtual int Length();

  /**
   * Write data to file
   * If has content beyond offset, will delete first
   * @return 0 if success; < 0 if error
   */
  virtual int Write(const char* data, int len, int offset);

  /**
   * Append content to file
   * @return 0 if success; < 0 if error
   */
  virtual int Append(const char* data, int len);

  /**
   * Read inLen from file starting at offset
   * OutLen = data read
   * @return 0 if success; < 0 if error
   */
  virtual int Read(char* dst, int *inOutLen, int offset);

 protected:
  FsFactory* _fsFactory;
  std::weak_ptr<FsDelegate> _fsDelegate;
  typedef std::shared_ptr<FsDelegate> TShDelegate;

  std::string _fileName;
  std::unique_ptr<FsDelegateSqlite> _sqlite;
};

} /* namespace tenta */
} /* namespace xwalk */

#endif /* FILE_BLOCK_H_ */
