/*
 * db_file_system.h
 *
 *  Created on: Feb 13, 2017
 *      Author: iotto
 */

#ifndef DB_FILE_SYSTEM_H_
#define DB_FILE_SYSTEM_H_

#include <memory>
#include <map>

#include <sqlite3.h>
//#include "base/memory/weak_ptr.h"
//#include "db_file.h"
//#include "db_path.h"

#ifndef DB_DEFAULT_BLOCK_SIZE
# define DB_DEFAULT_BLOCK_SIZE (4*1024)
#endif

#ifndef DB_DEFAULT_NAME
# define DB_DEFAULT_NAME "sqlite.db"
#endif

#ifndef DB_DEFAULT_OPEN_FLAGS
# define DB_DEFAULT_OPEN_FLAGS (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
#endif

namespace xwalk {
namespace tenta {
namespace fs {

class DbPath;
class DbFile;
class DbBlock;

class DbFileSystem {
 public:
  /**
   * File open/create operations
   */
  enum IOMode {
    IO_UNDEFINED = 0,
    IO_CREATE_IF_NOT_EXISTS = 1 << 0,
    IO_TRUNCATE_IF_EXISTS = 1 << 1,
    IO_OPEN_EXISTING = 1 << 2,
    IO_CREATE_NO_MATTER = (IO_CREATE_IF_NOT_EXISTS | IO_TRUNCATE_IF_EXISTS),
    IO_OPEN_NO_MATTER = (IO_CREATE_IF_NOT_EXISTS | IO_OPEN_EXISTING)
  };

  DbFileSystem();
  virtual ~DbFileSystem();

  /**
   * Init File system
   * @see set_db_path_name
   * @see set_db_open_flags
   * @see set_db_key
   * @return true if success; false if error @see error()
   */
  virtual bool Init();

  // close this filesystem
  virtual bool Close();

  /**
   * Create file with ioMode
   * @param file File with full path name to create
   * @param ioMode bitmap for operation @see IOMode; default (IO_CREATE_IF_NOT_EXISTS | IO_TRUNCATE_IF_EXISTS)
   * @param blkSize block size in db for file; default 4K (4096)
   * @return true if success; false otherwise @see error()
   */
  bool FileCreate(const std::string& file, std::weak_ptr<DbFile> & out,
                  int ioMode = IO_CREATE_NO_MATTER, int blkSize = DB_DEFAULT_BLOCK_SIZE);

  /**
   * Open file with ioMode
   * @param file File with full path name to open
   * @param out Result storage
   * @param ioMode Bitmap for operation @see IOMode; default (IO_CREATE_IF_NOT_EXISTS | IO_OPEN_EXISTING)
   * @return true if success; false otherwise @see error()
   */
  bool FileOpen(const std::string& file, std::weak_ptr<DbFile> & out,
                int ioMode = IO_OPEN_NO_MATTER, int blkSize = DB_DEFAULT_BLOCK_SIZE);

  /**
   * Close file
   * @param file
   * @return true if success; false otherwise @see error()
   */
  bool FileClose(DbFile * const file);


  void MkDir();
  void Pwd();
  std::unique_ptr<DbPath> Cd(const std::string &path);

  std::unique_ptr<DbFile> FileOpen(const std::string& file,
                                   const int *result = nullptr);
  void FileDelete(const std::string& file, const int *result = nullptr);
  bool IsFileExists(const std::string& file, const int *result = nullptr);
  int FileLength(const std::string& file, const int *result = nullptr);

  // Return last error code
  int error() const {
    return _err;
  }

  // Set encryption key for db to be encrypted
  void set_db_key(const std::string& key, const std::string& license) {
    _db_key = key;
    _db_lic = license;
  }

  // Set path and name for db, default DB_DEFAULT_NAME "sqlite.db"
  void set_db_path_name(const std::string& dbName) {
    _db_file = dbName;
  }

  // Default flags are DB_DEFAULT_OPEN_FLAGS (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
  void set_db_open_flags(int flags) {
    _db_open_flags = flags;
  }

 protected:
  friend class DbFile;  // access protected methods for file operations

  /**
   * Init database using _db_file and _init_flags
   * close existing if allready initalized
   */
  bool initDb();
  // Create tables and all that's db related
  bool createDb();

  /**
   * Create path in db
   * @param file
   * @param out
   * @return true if success; false otherwise @see error()
   */
  bool dbGetCreatePath(const std::string& path, DbPath * const out);
  // true if path found
  // if out != nullptr will fill with path data
  bool dbGetPathInfo(const std::string& fullPath, DbPath * const out);
  bool dbInsertNewPath(const std::string& pathName, const std::string& fullPath,
                       uint64_t &parent_id);

  //
  /**
   * Create |file| in directory |fill_out->_path|
   * @param file
   * @param ioMode
   * @param out Must already have a valid path!
   * @param blkSize block size in db for this file
   * @return true if success; false otherwise @see error()
   */
  bool dbGetCreateFile(const std::string& file, int ioMode, DbFile * const out, int blkSize);
  bool dbGetFileInfo(const std::string& file, DbFile * const out);
  bool dbInsertNewFile(const std::string& file, DbFile * const out, int blkSize);
  bool dbTruncateFile(DbFile * const out, int blkSize);
  /**
   * Increment file size by |incBy|
   * @param file using file_id
   * @param incBy NewFileSize=oldSize+incBy
   * @return true if success; false otherwise @see error()
   */
  bool dbIncFileSize(DbFile * const file, int incBy);

  /**
   * Set the files size (Handle with care, since file blocks may still be available
   * @param file
   * @param newSize
   * @return true if success; false otherwise @see error()
   */
  bool dbSetFileSize(DbFile * const file, int newSize);

  /**
   * Read file's content
   * @param file
   * @param dst
   * @param inOutLen
   * @param offset
   * @return true if success; false otherwise @see error()
   */
  bool dbReadFile(DbFile * const file, char* const dst, int * const inOutLen,
                  int offset);
  /**
   * Fill blk with last file block ordered by offset
   *
   * @param file
   * @param blk
   * @return true if success; false otherwise @see error()
   */
  bool dbGetLastBlock(DbFile * const file, DbBlock * const blk);

  /**
   * Get file block containing data from offset
   * @param file File
   * @param blk File block to fill
   * @param offset
   * @return true if success; false otherwise @see error()
   */
  bool dbGetBlockByOffset(DbFile * const file, DbBlock * const blk, int offset);

  /**
   * Write/update database block (block_id must be valid)
   * @param file
   * @param blk
   * @return true if success; false otherwise @see error()
   */
  bool dbUpdateBlock(DbFile * const file, DbBlock * const blk);

  /**
   * Insert new block to db
   * @param file
   * @param blk
   * @return
   */
  bool dbInsertBlock(DbFile * const file, DbBlock * const blk);

  /**
   * Delete file blocks after offset
   *
   * @param file
   * @param offset
   * @return
   */
  bool dbDeleteBlocksAfter(DbFile * const file, int offset);

  int _err;  // error code for last operation
  std::string _db_key;  // db encryption key
  std::string _db_lic;  // License
  std::string _db_file;  // db file path and name
  int _db_open_flags;  // flags for init db
  sqlite3 * _db;
  bool _initialized;  // true if initialized

  std::map<std::string, std::shared_ptr<DbFile>> _opened_files;
  typedef std::map<std::string, std::shared_ptr<DbFile>>::iterator It_of;

  typedef std::pair<std::string, std::shared_ptr<DbFile>> tOpenedFiles;

  std::shared_ptr<DbFileSystem> _this_shared;
  /* Inner classes */
  struct NoDelete {
    inline void operator()(DbFileSystem* ptr) const {
    }
  };
};

} /* namespace fs */
} /* namespace tenta */
} /* namespace xwalk */

#endif /* DB_FILE_SYSTEM_H_ */
