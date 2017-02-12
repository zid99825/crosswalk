/*
 * fs_delegate.h
 *
 *  Created on: Feb 6, 2017
 *      Author: iotto
 */

#ifndef FS_DELEGATE_H_
#define FS_DELEGATE_H_

#include <string>

namespace xwalk {
namespace tenta {

/**
 * Delegate File System operations
 */
class FsDelegate {
 public:
  virtual ~FsDelegate();

  /**
   * Init Virtual File system on root path (real FS)
   * openFlag defaults to 0 [owerwrite in subclass]
   */
  virtual int Init(const std::string& rootPath, int openFlags = 0) = 0;

  /**
   * Open file in virtual file system
   */
  virtual int FileOpen(const std::string& fileName, int mode /*bitmap OpenMode*/) = 0;
  /**
   * Delete Block file from virtual file system
   */
  virtual int FileDelete(const std::string& fileName, bool permanent = true) = 0;
  /**
   * Close block file in virtual file system
   */
  virtual int FileClose(const std::string& fileName) = 0;

  /**
   * Get file length from db
   * @return <0 if error, else file length
   */
  virtual int FileLength(const std::string& fileName) = 0;

  /**
   * Append content to file
   * @return 0 if success; < 0 if error
   */
  virtual int AppendData(const std::string& fileName, const char* data, int len) = 0;

  /**
   * Read file from offset, max inOutLen
   * @return 0 if success; < 0 if error
   * inOutLen has the length of data written
   */
  virtual int ReadData(const std::string& fileName, char* dst, int *inOutLen, int offset) = 0;

  /**
   * Write file content
   */
  virtual int WriteData(const std::string& fileName, const char *src, int len, int offset) = 0;

  int GetBlock();
  int WriteBlock();
  int CreateBlock();

  enum OpenMode {
    OM_NORMAL = 0,
    OM_CREATE_IF_NOT_EXISTS = 1 << 0,
    OM_TRUNCATE_IF_EXISTS = 1 << 1
  };

  /**
   * Block of Data
   */
  struct Block {
    char* data;  // storage for data
    int dataLen;  // length of usefull data
    int capacity;  //

   protected:
    int blockId;
  };

 protected:
  FsDelegate();
};

} /* namespace tenta */
} /* namespace xwalk */

#endif /* FS_DELEGATE_H_ */
