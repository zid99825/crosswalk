/*
 * fs_delegate_sqlite.h
 *
 *  Created on: Feb 6, 2017
 *      Author: iotto
 */

#ifndef SQLITE_FS_DELEGATE_SQLITE_H_
#define SQLITE_FS_DELEGATE_SQLITE_H_

#include <memory>
#include "xwalk/runtime/file_block_db/base/fs_delegate.h"
#include "sqlite3.h"

#define SQL_ERROR(err) (-err)
namespace base {
class FilePath;
}

namespace xwalk {
namespace tenta {

class FsDelegateSqlite : public FsDelegate {
 public:
  // typedef for functions to be passed to CallbackHere (called from cbDelegate)
  typedef int (FsDelegateSqlite::*cbFunction)(int nrCol, char** values, char** columns, void* extra);
  enum FileStatus {
    FS_CLOSED = 0,
    FS_OPEN_RW = 1 << 0,
    FS_OPEN_READ = 1 << 1,
    FS_OPEN_WRITE = 1 << 2,
    FS_DELETED = 1 << 3,
    FS_INVALID = 1 << 7,
    FS_MASK = 0x87    // Note: update FS_MASK if more flags are added
  };

  /**
   * Block represented in db
   */
  struct FSBlock {  // b.block_id, b.block_data, b.data_len, b.capacity, fb.offset
    int offset;
    int capacity;
    std::string data;
    int data_len;
    int block_id = -1;  // TODO make protected

   public:
    FSBlock(int capacity = SIZE_4K) {
      reset(0, capacity);
    }
    void reset(int newOffset = 0, int newCapacity = SIZE_4K) {
      block_id = -1;
      data.clear();
      data_len = 0;
      capacity = newCapacity;
      offset = newOffset;
    }

    /**
     * Known block capacity
     */
    enum Capacity {
      SIZE_4B = 4,
      SIZE_16B = 16,
      SIZE_1K = 1024,
      SIZE_4K = 4 * 1024
    };
  };

 public:
  FsDelegateSqlite(int blkSize = FSBlock::SIZE_4K);  // TODO make constructor for block size
  virtual ~FsDelegateSqlite();

  void set_key(const std::string& key, const std::string& license) {
    _key = key;
    _lic = license;
  }

  void set_db_name(const std::string& dbName) {
    _dbName = dbName;
  }

  void set_blk_size(int blkSize) {
    _blkSize = blkSize;
  }
  /**
   * Init File system on root path
   * openFlags = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE
   */
  virtual int Init(const std::string& rootPath, int openFlags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE) override;
  virtual int FileOpen(const std::string& fileName, int mode /*bitmap OpenMode*/) override;
  virtual int FileDelete(const std::string& fileName, bool permanent /*=true*/) override;
  virtual int FileClose(const std::string& fileName) override;
  /**
   * Get file length from db
   * @return <0 if error, else file length
   */
  virtual int FileLength(const std::string& fileName) override;
  virtual int AppendData(const std::string& fileName, const char* data, int len) override;
  virtual int ReadData(const std::string& fileName, char* dst, int *inOutLen, int offset) override;
  virtual int WriteData(const std::string& fileName, const char *src, int len, int offset) override;

 protected:
  // ***** db functions *****
  virtual int dbGetFileStatus(const std::string& fileName);
  virtual int dbSetFileStatus(const std::string& fileName, FileStatus fs);
  virtual int dbSetFileSize(const std::string& fileName, int newSize);
  virtual int dbGetFileLength(const std::string& fileName);
  // Create new file entry
  virtual int dbCreateFile(const std::string& fileName);
  // Update entry in db for file; Truncate content if mode
  virtual int dbOpenFile(const std::string& fileName, int mode/*bitmap OpenMode*/);
  virtual int dbGetLastBlock(const std::string& fileName, std::unique_ptr<FSBlock>& out);
  virtual int dbUpdateBlock(const std::string& fileName, const std::unique_ptr<FSBlock>& in);
  virtual int dbInsertBlock(const std::string& fileName, const std::unique_ptr<FSBlock>& in);
  virtual int dbIncFileSize(const std::string& fileName, int incBy);
  virtual int dbGetBlockByOffset(const std::string& fileName, std::unique_ptr<FSBlock>& out, int offset);
  /**
   * Delete file blocks after offset, including offset too
   */
  virtual int dbDeleteBlocksAfter(const std::string& fileName, int offset);

  // ***** callback functions *****
  int cbJustPrint(int nrCol, char** values, char** columns, void* extra);
  int cbGetStatus(int nrCol, char** values, char** columns, void* extra);
  int cbGetLastBlock(int nrCol, char** values, char** columns, void* extra);

  // ***** helpers *****
  virtual int createPaths(const base::FilePath& rootPath);
  virtual int initDb();
  int execQuery(const char* sql, cbFunction handler = nullptr, void* extra = nullptr);

 private:
  /**
   * static function to pass for _exec statement with propper CallbackHere filled
   */
  static int cbDelegate(void* cbHere, int nrCol, char** values, char** columns);

 protected:
  sqlite3 * _db;
  int _fStatus;  // current file status
  int _fStatusMask;
  int _blkSize;  // size of blocks in db default FSBlock::SIZE_4K

  std::string _key;
  std::string _lic;
  std::string _dbName;

  struct CallbackHere {
    FsDelegateSqlite* _this;
    cbFunction _cb;  // callback
    void *extra;
  };

};

} /* namespace tenta */
} /* namespace xwalk */

#endif /* SQLITE_FS_DELEGATE_SQLITE_H_ */
