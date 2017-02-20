/*
 * db_file_system.cc
 *
 *  Created on: Feb 13, 2017
 *      Author: iotto
 */

#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include "db_block.h"
#include "db_errors.h"
#include "db_file.h"
#include "db_file_system.h"
#include "db_path.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace xwalk {
namespace tenta {
namespace fs {

DbFileSystem::DbFileSystem()
    : _db_file( DB_DEFAULT_NAME),
      _db_open_flags( DB_DEFAULT_OPEN_FLAGS),
      _this_shared(this, NoDelete()) {  // shared ptr has this, so will release when last count zero

}

DbFileSystem::~DbFileSystem() {
  Close();
}

bool DbFileSystem::Init() {
  base::FilePath fpRoot = base::FilePath(_db_file).DirName();

  if (fpRoot.value() != base::FilePath::kCurrentDirectory) {
    base::File::Error error;

    // create root path for db if any
    if (!base::CreateDirectoryAndGetError(fpRoot, &error)) {
      LOG(ERROR) << "Create root path " << fpRoot.value() << " "
                 << base::File::ErrorToString(error);

      _err = error;  // TODO declare them
      return false;
    }
  }

  if (!initDb()) {
    return false;
  }

  if (!createDb()) {
    return false;
  }

  return true;
}

bool DbFileSystem::Close() {
  _initialized = false;

  if (_db != nullptr) {
    int result = sqlite3_close_v2(_db);
    if (result != SQLITE_OK) {
      _err = result;
      return false;
    }
    _db = nullptr;
  }

  return true;
}

/**
 * Create file with ioMode
 * @param file File with full path name to create
 * @param ioMode bitmap for operation @see IOMode; default (IO_CREATE_IF_NOT_EXISTS | IO_TRUNCATE_IF_EXISTS)
 * @return true if success; false otherwise @see error()
 */
bool DbFileSystem::FileCreate(const std::string& file,
                              std::weak_ptr<DbFile> & out,
                              int ioMode, int blkSize) {
  // TODO check parameters
  if (file.empty()) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "FileCreate invalid parameters";
    return false;
  }
  // create virtual paths and get path_id
  std::unique_ptr<DbPath> file_path(new DbPath());
  base::FilePath fp_file(file);

  if (dbGetCreatePath(fp_file.DirName().value(), file_path.get())
      && file_path->_path_id != 0) {

    std::string normFile(fp_file.BaseName().value());

//    LOG(INFO) << "parent " << file_path.get() << " for file '"
//              << normFile
//              << "'";

    std::unique_ptr<DbFile> dbf(new DbFile(normFile, _this_shared));
    dbf->_path = std::move(file_path);

    if (dbGetCreateFile(normFile, ioMode, dbf.get(), blkSize)) {  // fill in with goodies from db
//      LOG(INFO) << "File exists " << dbf.get()->_file_id;

      std::shared_ptr<DbFile> sptr(dbf.release());

      _opened_files[sptr->get_full_name()] = sptr;

      out = std::weak_ptr<DbFile>(sptr);
      return true;
    } else {
      LOG(ERROR) << "File not created " << file << " e " << _err;
    }
  } else {
    LOG(ERROR) << "Path not created " << file << " e " << _err;
  }

  return false;
}

/**
 * Open file with ioMode
 * @param file File with full path name to open
 * @param out Result storage
 * @param ioMode Bitmap for operation @see IOMode; default (IO_CREATE_IF_NOT_EXISTS | IO_OPEN_EXISTING)
 * @return true if success; false otherwise @see error()
 */
bool DbFileSystem::FileOpen(const std::string& file,
                            std::weak_ptr<DbFile> & out,
                            int ioMode, int blkSize) {

  return FileCreate(file, out, ioMode, blkSize);  // ioMode differs
}

/**
 * Close file
 * @param file
 * @return true if success; false otherwise @see error()
 */
bool DbFileSystem::FileClose(DbFile * const file) {
  if (file == nullptr) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "FileClose invalid parameters";
    return false;
  }

  It_of found = _opened_files.find(file->get_full_name());

  if (found == _opened_files.end()) {
    _err = -120;  // TODO define not found
    return false;
  }

  _opened_files.erase(found);
  return true;
}

/**
 * Create |file| in directory |out->_path|
 *
 * @param file
 * @param ioMode
 * @param out Must already have a valid path!
 * @return true if success; false otherwise @see error()
 */
bool DbFileSystem::dbGetCreateFile(const std::string& file, int ioMode,
                                   DbFile * const out,
                                   int blkSize) {
  if ((file.empty()) || (out == nullptr) || (out->_path.get() == nullptr)
      || (out->_path->_path_id <= 0)) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "dbGetCreateFile invalid parameters";
    return false;
  }

  if (dbGetFileInfo(file, out)) {
    if (out->_file_id == 0) {
      if (ioMode & IO_CREATE_IF_NOT_EXISTS) {
        // do create the file
        return dbInsertNewFile(file, out, blkSize);
      } else {
        _err = -100;  // TODO define create flag not defined
        return false;
      }
    } else {
      // have a valid file
      if (ioMode & IO_TRUNCATE_IF_EXISTS) {
        // delete files content
        return dbTruncateFile(out, blkSize);
      }
      return true;  // file ready
    }
  } else {
    return false;  // error getting file info
  }

  return true;
}

//
bool DbFileSystem::dbGetFileInfo(const std::string& file, DbFile * const out) {
  if ((_db == nullptr) || file.empty() || out == nullptr
      || (out->_path->_path_id <= 0)) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "dbGetFileInfo invalid parameters";
    return false;
  }

  // we have the name and path_id
  const char* sql = "SELECT file_id,blk_size,file_length,status"
      " FROM files"
      " WHERE file_name=?"
      " AND path_id=?;";
  sqlite3_stmt *stm;
  int bind_pos = -1;
  out->_file_id = 0;

  _err = sqlite3_prepare_v2(_db, sql, -1, &stm, nullptr);
  if (_err != SQLITE_OK) {
    LOG(ERROR) << "sqlite3_prepare_v2 " << sqlite3_errstr(_err);
    return false;  // statement not created
  } else {
    if (_err == SQLITE_OK) {
      bind_pos = 1;
      _err = sqlite3_bind_text(stm, bind_pos, file.c_str(),
                               file.length(),
                               SQLITE_STATIC);
    }

    if (_err == SQLITE_OK) {
      bind_pos = 2;
      _err = sqlite3_bind_int64(stm, bind_pos, out->_path->_path_id);
    }

    if (_err != SQLITE_OK) {
      LOG(ERROR) << "sqlite3_bind pos_" << bind_pos << " "
                 << sqlite3_errstr(_err);
    }
  }  // if prepare

  if (_err == SQLITE_OK) {
    // time to get the values
    int sleepCnt = 10;  // wait max 1s

    while (_err != SQLITE_DONE) {
      _err = sqlite3_step(stm);

      if (_err == SQLITE_ROW) {
        out->_file_id = sqlite3_column_int64(stm, 0);  // file_id
        out->_blk_size = sqlite3_column_int(stm, 1);
        out->_length = sqlite3_column_int(stm, 2);
        out->_status = sqlite3_column_int(stm, 3);

      } else if (_err == SQLITE_BUSY) {
        if (--sleepCnt == 0) {
          break;
        }

        sqlite3_sleep(100);  // TODO check if used correctly, make constant
        continue;

      } else {
        break;  // Something went wrong or done
      }
    }  // end while
  }

  sqlite3_finalize(stm);

  if (_err != SQLITE_DONE) {
    return false;
  }

  return true;
}

//
bool DbFileSystem::dbInsertNewFile(const std::string& file,
                                   DbFile * const out,
                                   int blkSize) {
  if ((_db == nullptr) || file.empty() || out == nullptr
      || (out->_path.get() == nullptr)
      || (out->_path->_path_id <= 0) || (blkSize < 2)) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "dbInsertNewFile invalid parameters";
    return false;
  }

  out->_name = file;

  std::string sql;
  std::string idStr;
  base::SStringPrintf(&idStr, "%lu", out->_path->_path_id);

  base::SStringPrintf(
      &sql,
      "INSERT INTO files(path_id,file_name,blk_size,status,file_name_path)"
      " VALUES(%s,'%s',%d,%d,'%s');",
      idStr.c_str(), file.c_str(), blkSize, 0, out->get_full_name());  // TODO manage status (opened...)

  _err = sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, nullptr);

  if (_err != SQLITE_OK) {
    return false;
  }

  out->_file_id = sqlite3_last_insert_rowid(_db);
  out->_length = 0;
  out->_status = 0;  // TODO see above
  out->_blk_size = blkSize;

  return true;
}

/**
 *
 * @param file
 * @param out
 * @return
 */
bool DbFileSystem::dbTruncateFile(DbFile * const out, int blkSize) {
  if ((_db == nullptr) || (out == nullptr) || (out->_file_id <= 0)) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "dbTruncateFile invalid parameters";
    return false;
  }

  std::string sql;
  std::string fileIdStr;
  base::SStringPrintf(&fileIdStr, "%lu", out->_file_id);

  // TODO transaction
  base::SStringPrintf(&sql, "UPDATE files SET file_length=0,blk_size=%d WHERE file_id=%s;"
                      "DELETE FROM file_blocks WHERE file_id=%s;",
                      blkSize, fileIdStr.c_str(), fileIdStr.c_str());

  _err = sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, nullptr);

  if (_err != SQLITE_OK) {
    return false;
  }

  out->_length = 0;
  out->_blk_size = blkSize;
  return true;
}

/**
 * Increment file size by |incBy|
 * @param f using file_id
 * @param incBy NewFileSize=oldSize+incBy
 * @return true if success; false otherwise @see error()
 */
bool DbFileSystem::dbIncFileSize(DbFile * const file, int incBy) {
  if ((_db == nullptr) || (file == nullptr) || (file->_file_id <= 0)) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "dbIncFileSize invalid parameters";
    return false;
  }

  std::string sql;
  std::string fileIdStr;
  base::SStringPrintf(&fileIdStr, "%lu", file->_file_id);

  base::SStringPrintf(
      &sql, "UPDATE files SET file_length=file_length+%d WHERE file_id=%s",
      incBy,
      fileIdStr.c_str());

  _err = sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, nullptr);

  if (_err != SQLITE_OK) {
    return false;
  }

  file->_length = 0;
  return true;

}

/**
 * Set the files size (Handle with care, since file blocks may still be available
 * @param file
 * @param newSize
 * @return true if success; false otherwise @see error()
 */
bool DbFileSystem::dbSetFileSize(DbFile * const file, int newSize) {
  if ((_db == nullptr) || (file == nullptr) || (file->_file_id <= 0)) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "dbSetFileSize invalid parameters";
    return false;
  }

  std::string sql;
  std::string fileIdStr;
  base::SStringPrintf(&fileIdStr, "%lu", file->_file_id);

  base::SStringPrintf(
                      &sql,
                      "UPDATE files SET file_length=%d WHERE file_id=%s",
                      newSize,
                      fileIdStr.c_str());

  _err = sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, nullptr);

  if (_err != SQLITE_OK) {
    return false;
  }

  file->_length = newSize;
  return true;
}

/**
 * Read file's content
 * @param file
 * @param dst
 * @param inOutLen
 * @param offset
 * @return true if success; false otherwise @see error()
 */
bool DbFileSystem::dbReadFile(DbFile * const file, char* const dst,
                              int * const inOutLen,
                              int offset) {
  if ((_db == nullptr) || (file == nullptr) || (dst == nullptr)
      || (inOutLen == nullptr) || (*inOutLen == 0)) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "dbReadFile invalid parameters";
    return false;
  }

  const char * sql =
      "SELECT data_len,data_block FROM file_blocks"
          " WHERE file_id=?"
          " ORDER BY offset ASC"
          " LIMIT ? OFFSET ?;";

  int oneBC = file->_blk_size;  // default block capacity, TODO make constant
  int remainB = *inOutLen;  // TODO check for null
  int savedB = 0;  // number of bytes pushed to output
  int limitOffs = offset / oneBC;  // SELECT LIMIT's OFFSET
  int dataOffset = offset % oneBC;  // data start in block
  int limit = remainB / oneBC + 1;  // SELECT's LIMIT
  sqlite3_stmt *stm;
  int bind_pos = -1;

  _err = sqlite3_prepare_v2(_db, sql, -1, &stm, nullptr);
  if (_err != SQLITE_OK) {
    LOG(ERROR) << "sqlite3_prepare_v2 " << sqlite3_errstr(_err);
    return false;  // statement not created
  } else {
    if (_err == SQLITE_OK) {
      bind_pos = 1;
      _err = sqlite3_bind_int64(stm, bind_pos, file->_file_id);
    }

    if (_err == SQLITE_OK) {
      bind_pos = 2;
      _err = sqlite3_bind_int(stm, bind_pos, limit);
    }

    if (_err == SQLITE_OK) {
      bind_pos = 3;
      _err = sqlite3_bind_int(stm, bind_pos, limitOffs);
    }

    if (_err != SQLITE_OK) {
      LOG(ERROR) << "sqlite3_bind pos_" << bind_pos << " "
                 << sqlite3_errstr(_err);
    }
  }  // if prepare

  if (_err == SQLITE_OK) {
    const char *pInData;  // read from
    char *pOutData = dst;   // write to
    int maxB;  // max amount to copy
    int blobLen;
    int sleepCnt = 10;  // wait max 1s
    int data_len;

    while (_err != SQLITE_DONE || remainB > 0) {
      _err = sqlite3_step(stm);

      if (_err == SQLITE_ROW) {
        data_len = sqlite3_column_int(stm, 0);

        pInData = static_cast<const char*>(sqlite3_column_blob(stm, 1));
        blobLen = sqlite3_column_bytes(stm, 1);

        if ((blobLen != data_len) || (dataOffset > data_len)) {
          _err = DB_ERR_INCONSISTENT;
          LOG(ERROR) << "Invalid db structure for read datalen " << data_len
                     << " offset "
                     << dataOffset;
          break;
        }

        if (pInData != nullptr) {
          pInData += dataOffset;  // set pointer to desired
          blobLen -= dataOffset;
          maxB = remainB > blobLen ? blobLen : remainB;
          memcpy(pOutData, pInData, maxB);

          remainB -= maxB;
          savedB += maxB;
          dataOffset = 0;  // next time read from the begining
        } else {
          LOG(ERROR) << "Read got NULL ";
          break;
        }
      } else if (_err == SQLITE_BUSY) {
        if (--sleepCnt == 0) {
          break;
        }
        sqlite3_sleep(100);  // TODO check if used correctly, make constant
        continue;

      } else {
        break;  // Something went wrong or done
      }
    }  // end while
  }

  sqlite3_finalize(stm);

  if (_err != SQLITE_DONE) {
    return false;
  }

  *inOutLen = savedB;
  return true;
}

/**
 *
 */
bool DbFileSystem::dbGetCreatePath(const std::string& path,
                                   DbPath * const out) {
  if (path.empty() || out == nullptr) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "dbGetCreatePath invalid parameters";
    return false;
  }

  base::FilePath fptest(path);

  // TODO do recursive using by FilePath.DirName

//  fptest = fptest.DirName();  // base path
  uint64_t parent_id = 0;

  std::vector<base::FilePath::StringType> components;
  fptest.GetComponents(&components);
  // add current dir "." as first
  components.insert(components.begin(), base::FilePath::kCurrentDirectory);

// TODO  std::vector<uint64_t> parent_list; // to handle ".." if needed
  std::string buildUp;

  for (auto p : components) {
    buildUp.append(p);
    if (dbGetPathInfo(buildUp, out)) {
      if (out->_path_id == 0) {
        //need to create path use parent_id (if any)
        out->_parent_id = parent_id;
        dbInsertNewPath(p, buildUp, parent_id);
      } else {
        if (parent_id != 0 && parent_id != out->_parent_id) {
          LOG(ERROR) << "Inconsistent db old parent_id " << parent_id
                     << " curr parent_id "
                     << out->_parent_id;

          _err = DB_ERR_INCONSISTENT;
          return false;
        }
        parent_id = out->_path_id;
      }
    } else {
      break;
    }
    buildUp.append(base::FilePath::kSeparators);
  }

  out->_path_id = parent_id;
  out->_path_name = buildUp;
  return true;
}

/**
 *
 */
bool DbFileSystem::dbInsertNewPath(const std::string& pathName,
                                   const std::string& fullPath,
                                   uint64_t &parent_id) {
  if ((_db == nullptr) || (pathName.empty()) || (fullPath.empty())) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "dbInsertNewPath invalid parameters";
    return false;
  }
  std::string sql;
  std::string idStr;
  if (parent_id == 0) {
    idStr = "NULL";
  } else {
    base::SStringPrintf(&idStr, "%lu", parent_id);
  }

  base::SStringPrintf(&sql, "INSERT INTO path(parent_id,path_name,full_path)"
                      " VALUES(%s,'%s','%s');",
                      idStr.c_str(), pathName.c_str(), fullPath.c_str());

  _err = sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, nullptr);

  if (_err != SQLITE_OK) {
    return false;
  }

  parent_id = sqlite3_last_insert_rowid(_db);

  return true;
}

/**
 *
 */
bool DbFileSystem::dbGetPathInfo(const std::string& fullPath,
                                 DbPath * const out) {

  if ((_db == nullptr) || fullPath.empty() || out == nullptr) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "dbGetPathInfo invalid parameters";
    return false;
  }

  const char* sql = "SELECT path_id,parent_id,full_path"
      " FROM path"
      " WHERE full_path=?;";
//      " AND parent_id=?;";
  sqlite3_stmt *stm;
  int bind_pos = -1;
  out->_path_id = 0;

  _err = sqlite3_prepare_v2(_db, sql, -1, &stm, nullptr);
  if (_err != SQLITE_OK) {
    LOG(ERROR) << "sqlite3_prepare_v2 " << sqlite3_errstr(_err);
    return false;  // statement not created
  } else {
    bind_pos = 1;
    _err = sqlite3_bind_text(stm, bind_pos, fullPath.c_str(), fullPath.length(),
    SQLITE_STATIC);
//    bind_pos = 2;
//    _err = sqlite3_bind_text(stm, bind_pos, pathName.c_str(),
//                             pathName.length(),
//                             SQLITE_STATIC);

    if (_err != SQLITE_OK) {
      LOG(ERROR) << "sqlite3_bind pos_" << bind_pos << " "
                 << sqlite3_errstr(_err);
    }
  }  // if prepare

  if (_err == SQLITE_OK) {
// time to get the values
    int sleepCnt = 10;  // wait max 1s

    while (_err != SQLITE_DONE) {
      _err = sqlite3_step(stm);

      if (_err == SQLITE_ROW) {
        out->_path_id = sqlite3_column_int64(stm, 0);  // path_id
        out->_parent_id = sqlite3_column_int64(stm, 1);  // path_id
        out->_path_name.assign(
            reinterpret_cast<const char*>(sqlite3_column_text(stm, 2)));

      } else if (_err == SQLITE_BUSY) {
        if (--sleepCnt == 0) {
          break;
        }
        sqlite3_sleep(100);  // TODO check if used correctly, make constant
        continue;

      } else {
        break;  // Something went wrong or done
      }
    }  // end while
  }

  sqlite3_finalize(stm);

  if (_err != SQLITE_DONE) {
    return false;
  }

  return true;
}

/**
 * Fill blk with last file block ordered by offset
 *
 * @param file
 * @param blk
 * @return true if success; false otherwise @see error()
 */
bool DbFileSystem::dbGetLastBlock(DbFile * const file, DbBlock * const blk) {
  if ((_db == nullptr) || (file == nullptr) || (file->_file_id <= 0)
      || (blk == nullptr)) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "dbGetLastBlock invalid parameters";
    return false;
  }

  const char* sql =
      "SELECT block_id,offset,capacity,data_len,data_block FROM file_blocks"
          " WHERE file_id=?"
          " ORDER BY offset DESC"
          " LIMIT 1;";

  sqlite3_stmt *stm;
  int bind_pos = -1;

  _err = sqlite3_prepare_v2(_db, sql, -1, &stm, nullptr);
  if (_err != SQLITE_OK) {
    LOG(ERROR) << "sqlite3_prepare_v2 " << sqlite3_errstr(_err);
    return false;  // statement not created
  } else {
    if (_err == SQLITE_OK) {
      bind_pos = 1;
      _err = sqlite3_bind_int64(stm, bind_pos, file->_file_id);
    }

    if (_err != SQLITE_OK) {
      LOG(ERROR) << "sqlite3_bind pos_" << bind_pos << " "
                 << sqlite3_errstr(_err);
    }
  }  // if prepare

  if (_err == SQLITE_OK) {
    int sleepCnt = 10;  // wait 1s

    while (_err != SQLITE_DONE) {
      _err = sqlite3_step(stm);

      if (_err == SQLITE_ROW) {  // block_id,offset,capacity,data_len,data_block
        const char *pInData;  // read from
        int blobLen;

        blk->block_id = sqlite3_column_int64(stm, 0);
        blk->offset = sqlite3_column_int(stm, 1);
        blk->capacity = sqlite3_column_int(stm, 2);
        blk->data_len = sqlite3_column_int(stm, 3);

        pInData = static_cast<const char*>(sqlite3_column_blob(stm, 4));
        blobLen = sqlite3_column_bytes(stm, 4);

        if ((blobLen != blk->data_len)) {
          _err = DB_ERR_INCONSISTENT;
          LOG(ERROR) << "Invalid db structure for read datalen "
                     << blk->data_len
                     << " blobLen " << blobLen;
          break;
        }

        if (pInData != nullptr) {
          blk->data.assign(pInData, blobLen);
        }

      } else if (_err == SQLITE_BUSY) {
        if (--sleepCnt == 0) {
          break;
        }
        sqlite3_sleep(100);  // TODO check if used correctly, make constant
        continue;

      } else {
        break;  // Something went wrong or done
      }
    }  // end while
  }

  int tmpErr = sqlite3_finalize(stm);

  if (_err != SQLITE_DONE) {
    _err = tmpErr;
    return false;
  }

  return true;
}

/**
 * Get file block containing data from offset
 * @param file File
 * @param blk File block to fill
 * @param offset
 * @return true if success; false otherwise @see error()
 */
bool DbFileSystem::dbGetBlockByOffset(DbFile * const file, DbBlock * const blk,
                                      int offset) {
  // TODO check parameters
  if ((_db == nullptr) || (file == nullptr) || (file->_file_id <= 0)
      || (blk == nullptr)) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "dbGetBlockByOffset invalid parameters";
    return false;
  }
  const char * sql =
      "SELECT block_id,offset,capacity,data_len,data_block FROM file_blocks"
          " WHERE file_id=?"
          " AND offset =?;";

  int dataOffset = offset % file->_blk_size;  // where data starts for |offset| in block
  int dbOffset = offset - dataOffset;  // block offset in db

  sqlite3_stmt *stm;
  int bind_pos = -1;

  _err = sqlite3_prepare_v2(_db, sql, -1, &stm, nullptr);
  if (_err != SQLITE_OK) {
    LOG(ERROR) << "sqlite3_prepare_v2 " << sqlite3_errstr(_err);
    return false;  // statement not created
  } else {
    if (_err == SQLITE_OK) {
      bind_pos = 1;
      _err = sqlite3_bind_int64(stm, bind_pos, file->_file_id);
    }

    if (_err == SQLITE_OK) {
      bind_pos = 2;
      _err = sqlite3_bind_int(stm, bind_pos, dbOffset);
    }

    if (_err != SQLITE_OK) {
      LOG(ERROR) << "sqlite3_bind pos_" << bind_pos << " "
                 << sqlite3_errstr(_err);
    }
  }  // if prepare

  if (_err == SQLITE_OK) {
    int sleepCnt = 10;  // wait 1s

    while (_err != SQLITE_DONE) {
      _err = sqlite3_step(stm);

      if (_err == SQLITE_ROW) {  // block_id,offset,capacity,data_len,data_block
        const char *pInData;  // read from
        int blobLen;

        blk->block_id = sqlite3_column_int64(stm, 0);
        blk->offset = sqlite3_column_int(stm, 1);
        blk->capacity = sqlite3_column_int(stm, 2);
        blk->data_len = sqlite3_column_int(stm, 3);

        pInData = static_cast<const char*>(sqlite3_column_blob(stm, 4));
        blobLen = sqlite3_column_bytes(stm, 4);

        if ((blobLen != blk->data_len)) {
          _err = DB_ERR_INCONSISTENT;
          LOG(ERROR) << "Invalid db structure for read datalen "
                     << blk->data_len
                     << " blobLen " << blobLen;
          break;
        }

        if (pInData != nullptr) {
          blk->data.assign(pInData, blobLen);
        }

      } else if (_err == SQLITE_BUSY) {
        if (--sleepCnt == 0) {
          break;
        }
        sqlite3_sleep(100);  // TODO check if used correctly, make constant
        continue;

      } else {
        break;  // Something went wrong or done
      }
    }  // end while
  }

  int tmpErr = sqlite3_finalize(stm);

  if (_err != SQLITE_DONE) {
    _err = tmpErr;
    return false;
  }

  return true;
}
/**
 * Write/update database block (block_id must be valid)
 * @param file
 * @param blk
 * @return true if success; false otherwise @see error()
 */
bool DbFileSystem::dbUpdateBlock(DbFile * const file, DbBlock * const blk) {
  if ((_db == nullptr) || (file == nullptr) || (file->_file_id <= 0)
      || (blk == nullptr)
      || (blk->block_id <= 0) || blk->data_len > (int) blk->data.length()) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "dbUpdateBlock invalid parameters";
    return false;
  }

  int bind_pos = -1;
  sqlite3_stmt *stm;

  const char* sql = "UPDATE file_blocks SET data_len=?,data_block=?"
      " WHERE block_id=?;";

  _err = sqlite3_prepare_v2(_db, sql, -1, &stm, nullptr);
  if (_err != SQLITE_OK) {
    LOG(ERROR) << "sqlite3_prepare_v2 " << sqlite3_errstr(_err);
    return false;  // statement not created
  } else {
    if (_err == SQLITE_OK) {
      bind_pos = 3;
      _err = sqlite3_bind_int64(stm, bind_pos, blk->block_id);
    }

    if (_err == SQLITE_OK) {
      bind_pos = 1;
      _err = sqlite3_bind_int(stm, bind_pos, blk->data_len);
    }

    if (_err == SQLITE_OK) {
      bind_pos = 2;
      _err = sqlite3_bind_blob(stm, bind_pos, blk->data.c_str(),
                               blk->data_len,
                               SQLITE_STATIC);
    }

    if (_err != SQLITE_OK) {
      LOG(ERROR) << "sqlite3_bind pos_" << bind_pos << " "
                 << sqlite3_errstr(_err);
    }
  }  // if prepare

  if (_err == SQLITE_OK) {  // binding ok
    _err = sqlite3_step(stm);

    // TODO timeout
    int sleepCnt = 10;  // wait 1s

    while (_err != SQLITE_DONE) {
      if (_err == SQLITE_BUSY) {
        if (--sleepCnt == 0) {
          break;
        }
        sqlite3_sleep(100);  // TODO check if used correctly, make constant
        continue;
      } else {
        break;
      }
    }  // while
  }

  int tmpErr = sqlite3_finalize(stm);

  if (_err != SQLITE_DONE) {
    _err = tmpErr;
    return false;
  }

  return true;
}

/**
 * Insert new block to db
 * @param file
 * @param blk
 * @return
 */
bool DbFileSystem::dbInsertBlock(DbFile * const file, DbBlock * const blk) {
  if ((_db == nullptr) || (file == nullptr) || (file->_file_id <= 0)
      || (blk == nullptr)) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "dbInsertBlock invalid parameters";
    return false;
  }

  const char* sql =
      "INSERT INTO file_blocks(file_id,offset,capacity,data_len,data_block)"
          " VALUES(?,?,?,?,?);";
  int bind_pos = -1;
  sqlite3_stmt *stm;

  _err = sqlite3_prepare_v2(_db, sql, -1, &stm, nullptr);
  if (_err != SQLITE_OK) {
    LOG(ERROR) << "sqlite3_prepare_v2 " << sqlite3_errstr(_err);
    return false;  // statement not created
  } else {
    if (_err == SQLITE_OK) {
      bind_pos = 1;
      _err = sqlite3_bind_int64(stm, bind_pos, file->_file_id);
    }

    if (_err == SQLITE_OK) {
      bind_pos = 2;
      _err = sqlite3_bind_int(stm, bind_pos, blk->offset);
    }

    if (_err == SQLITE_OK) {
      bind_pos = 3;
      _err = sqlite3_bind_int(stm, bind_pos, blk->capacity);
    }

    if (_err == SQLITE_OK) {
      bind_pos = 4;
      _err = sqlite3_bind_int(stm, bind_pos, blk->data.length());
    }

    if (_err == SQLITE_OK) {
      bind_pos = 5;
      _err = sqlite3_bind_blob(stm, bind_pos, blk->data.c_str(),
                               blk->data.length(),
                               SQLITE_STATIC);
    }

    if (_err != SQLITE_OK) {
      LOG(ERROR) << "sqlite3_bind pos_" << bind_pos << " "
                 << sqlite3_errstr(_err);
    }
  }  // else prepare

  if (_err == SQLITE_OK) {  // binding ok
    _err = sqlite3_step(stm);

    // TODO timeout
    int sleepCnt = 10;  // wait 1s

    while (_err != SQLITE_DONE) {
      if (_err == SQLITE_BUSY) {
        if (--sleepCnt == 0) {
          break;
        }
        sqlite3_sleep(100);  // TODO check if used correctly, make constant
        continue;
      } else {
        break;
      }
    }  // while
  }

  int tmpErr = sqlite3_finalize(stm);

  if (_err != SQLITE_DONE) {
    _err = tmpErr;
    return false;
  }

  blk->block_id = sqlite3_last_insert_rowid(_db);
  return true;
}

/**
 * Delete file blocks after offset
 *
 * @param file
 * @param offset
 * @return
 */
bool DbFileSystem::dbDeleteBlocksAfter(DbFile * const file, int offset) {
  if ((_db == nullptr) || (file == nullptr) || (file->_file_id <= 0)) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "dbDeleteBlocksAfter invalid parameters";
    return false;
  }

  std::string sql;
  std::string fileIdStr;
  base::SStringPrintf(&fileIdStr, "%lu", file->_file_id);

  // TODO transaction
  base::SStringPrintf(&sql, "DELETE FROM file_blocks"
                      " WHERE file_id=%s AND offset >= %d;",
                      fileIdStr.c_str(),
                      offset);

  _err = sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, nullptr);

  if (_err != SQLITE_OK) {
    return false;
  }

  return true;
}

/**
 *
 */
bool DbFileSystem::initDb() {
  bool retVal = true;

  if (_initialized) {
    retVal = Close();
    if (!retVal) {
      return false;
    }
  }

  //  int openFlags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
  _err = sqlite3_open_v2(_db_file.c_str(), &_db, _db_open_flags, nullptr);

  if (_err != SQLITE_OK) {
    LOG(ERROR) << _db_file << " " << sqlite3_errstr(_err);
    return false;
  }

  int vSet;
  _err = sqlite3_db_config(_db,
  SQLITE_DBCONFIG_ENABLE_FKEY,
                           1, &vSet);
  if (_err != SQLITE_OK) {
    LOG(ERROR) << "foreign key " << sqlite3_errstr(_err);
    return false;
  }

  _err = sqlite3_db_config(_db,
  SQLITE_DBCONFIG_ENABLE_TRIGGER,
                           1, &vSet);
  if (_err != SQLITE_OK) {
    LOG(ERROR) << "trigger " << sqlite3_errstr(_err);
    return false;
  }

  // encrypt
  if (_err == SQLITE_OK && !_db_key.empty()) {
    std::string tmp;
    tmp.assign("PRAGMA key='").append(_db_key).append("';");
    _err = sqlite3_exec(_db, tmp.c_str(), nullptr, nullptr, nullptr);

    if (_err == SQLITE_OK) {
      tmp.assign("PRAGMA lic='").append(_db_lic).append("';");
      _err = sqlite3_exec(_db, tmp.c_str(), nullptr, nullptr, nullptr);
    }
  }

  if (_err != SQLITE_OK) {
    return false;
  }

  return true;
}

/**
 *
 */
bool DbFileSystem::createDb() {
  const char * createSql = "PRAGMA foreign_keys = ON;"
      "CREATE TABLE IF NOT EXISTS path("
      "path_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "parent_id INTEGER NULL REFERENCES path(path_id) ON DELETE SET NULL,"
      "path_name TEXT,"
      "full_path TEXT UNIQUE);"
      "CREATE TABLE IF NOT EXISTS files("
      "file_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "path_id INTEGER REFERENCES path ON DELETE CASCADE,"
      "file_name TEXT,"
      "file_length INTEGER DEFAULT 0,"
      "blk_size INTEGER DEFAULT 4096,"
      "status INTEGER(2) DEFAULT 0,"
      "file_name_path TEXT UNIQUE,"
      "UNIQUE(path_id,file_name));"
      "CREATE TABLE IF NOT EXISTS file_blocks("
      "block_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "file_id INTEGER REFERENCES files ON DELETE CASCADE,"
      "offset INTEGER DEFAULT 0,"
      "capacity INTEGER DEFAULT 4096,"
      "data_len INTEGER DEFAULT 0,"
      "data_block TEXT,"
      "UNIQUE(file_id,offset));"
      "CREATE TRIGGER IF NOT EXISTS on_delete_path_delete AFTER DELETE ON path"
      " BEGIN DELETE FROM path WHERE parent_id=old.path_id;END;";

  _err = sqlite3_exec(_db, createSql, nullptr, nullptr, nullptr);

  if (_err != SQLITE_OK) {
    LOG(ERROR) << "DB create " << sqlite3_errstr(_err);
    return false;
  }

  return true;
}

} /* namespace fs */
} /* namespace tenta */
} /* namespace xwalk */
