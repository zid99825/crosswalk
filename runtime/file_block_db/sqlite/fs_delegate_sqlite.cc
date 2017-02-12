/*
 * fs_delegate_sqlite.cc
 *
 *  Created on: Feb 6, 2017
 *      Author: iotto
 */

#include <iostream>
#include <sys/stat.h>
#include "fs_delegate_sqlite.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/logging.h"

namespace xwalk {
namespace tenta {

static std::string g_sqlStore;
//static char stmBuff[1024];  // buffer to prepare statements
//const int stmBuffSize = sizeof(stmBuff) / sizeof(stmBuff[0]);

FsDelegateSqlite::FsDelegateSqlite(int blkSize)
    : _fStatusMask(FS_MASK),
      _blkSize(blkSize) {
  _dbName = "sqlite.db";
}

FsDelegateSqlite::~FsDelegateSqlite() {
  // TODO Auto-generated destructor stub
  if (_db != nullptr) {
    sqlite3_close(_db);
  }
}

int FsDelegateSqlite::Init(const std::string& rootPath, int openFlags) {

  int result = 1;

  base::FilePath rPath(rootPath);

  // create root path
  result = createPaths(rPath);
  if (result != 0) {
    return result;
  }

  rPath = rPath.Append(_dbName);

  const char* fullP = rPath.value().c_str();

//  int openFlags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
  result = sqlite3_open_v2(fullP, &_db, openFlags, nullptr);

  if (result != SQLITE_OK) {
    LOG(ERROR) << fullP << " " << sqlite3_errstr(result);

    return SQL_ERROR(result);  // make negative error codes
  }

  int vSet;
  result = sqlite3_db_config(_db, SQLITE_DBCONFIG_ENABLE_FKEY, 1, &vSet);
  if (result != SQLITE_OK) {
    LOG(ERROR) << "foreign key " << sqlite3_errstr(result);
    return SQL_ERROR(result);  // make negative error codes
  }

  result = sqlite3_db_config(_db, SQLITE_DBCONFIG_ENABLE_TRIGGER, 1, &vSet);
  if (result != SQLITE_OK) {
    LOG(ERROR) << "trigger " << sqlite3_errstr(result);
    return SQL_ERROR(result);  // make negative error codes
  }

  // encrypt
  if (result == SQLITE_OK && !_key.empty()) {
    std::string tmp;
    tmp.assign("PRAGMA key='").append(_key).append("';");
    result = execQuery(tmp.c_str());

    if (result == SQLITE_OK) {
      tmp.assign("PRAGMA lic='").append(_lic).append("';");
      result = execQuery(tmp.c_str());
    }
  }

  if (result == SQLITE_OK) {
    // init db
    result = initDb();
  }

  if (result != SQLITE_OK) {
    LOG(ERROR) << "Db init " << sqlite3_errstr(result);
  }
  return SQL_ERROR(result);
}

/**
 * Open file in db
 */
int FsDelegateSqlite::FileOpen(const std::string& fileName, int mode /*bitmap OpenMode*/) {

  int result = 1;

  result = dbGetFileStatus(fileName);

  if (result > 0) {
    result &= _fStatusMask;

    // check state
    if (result == FS_INVALID /*&& (mode & OM_CREATE_IF_NOT_EXISTS)*/) {
      // TODO time to insert if mode has OM_CREATE_IF_NOT_EXISTS
      result = dbCreateFile(fileName);

    } else /*if (result == FS_CLOSED)*/{  // TODO check RW status separately
      // ok to open file
      result = dbOpenFile(fileName, mode);
    }
  } else {
  }

  return SQL_ERROR(result);
}

/**
 * Delete file from db
 */
int FsDelegateSqlite::FileDelete(const std::string& fileName, bool permanent/* = true*/) {

  int result = 1;

  if (permanent) {
    base::SStringPrintf(&g_sqlStore, "DELETE FROM files "
                        "WHERE file_path=\"%s\";",
                        fileName.c_str());

    result = execQuery(g_sqlStore.c_str(), nullptr);
  } else {

    result = dbSetFileStatus(fileName, FS_DELETED);
  }

  return SQL_ERROR(result);
}

/**
 * Set file state to closed
 */
int FsDelegateSqlite::FileClose(const std::string& fileName) {
  int result = dbSetFileStatus(fileName, FS_CLOSED);

  return SQL_ERROR(result);
}

/**
 *
 */
int FsDelegateSqlite::FileLength(const std::string& fileName) {
  return dbGetFileLength(fileName);
}

/**
 *
 */
int FsDelegateSqlite::AppendData(const std::string& fileName, const char* data, int len) {
  int result = 1;

  std::unique_ptr<FSBlock> lastB(new FSBlock(_blkSize));
  int remainB = len;  // byte remained to save
  int nextOffset = 0;
  int oneBC = _blkSize;  // default block capacity
  int freeB = 0;  // free bytes in data buffer
  int maxB = 0;  // max amount to copy

  result = dbGetLastBlock(fileName, lastB);
  if (result == SQLITE_OK) {
//    LOG(INFO) << "last block id " << lastB->block_id;

    if (lastB->block_id > 0) {  // got valid block_id
      freeB = lastB->capacity - lastB->data_len;
      nextOffset = lastB->offset;  // update offset to last offset
      oneBC = lastB->capacity;

      if (freeB > 0) {
        // fill in and push max available or len
        maxB = freeB > remainB ? remainB : freeB;

        lastB->data.append(data, maxB);
        data += maxB;
        remainB -= maxB;
        lastB->data_len += maxB;

        // push to db
        result = dbUpdateBlock(fileName, lastB);
      }
      // next blocks offset (makes sense only if buffer overflow, otherwise will be ignored)
      nextOffset += oneBC;
    }

    while ((result == SQLITE_OK) && (remainB > 0)) {
      // create new block and push to db
      lastB->reset(nextOffset, oneBC);
      freeB = lastB->capacity;  // amount can be copied to data
      maxB = freeB > remainB ? remainB : freeB;

      lastB->data.append(data, maxB);
      data += maxB;
      remainB -= maxB;
      lastB->data_len += maxB;

      // push to db
      result = dbInsertBlock(fileName, lastB);

      nextOffset += oneBC;
    }

    // update file length+len
    if (result == SQLITE_OK) {
      result = dbIncFileSize(fileName, len);
    }
  } else {
    LOG(ERROR) << "Panic!! " << sqlite3_errstr(result);
  }

  return SQL_ERROR(result);
}

/**
 *
 */
int FsDelegateSqlite::ReadData(const std::string& fileName, char* dst, int *inOutLen, int offset) {
  int result = 1;
  int remainB = *inOutLen;  // TODO check for null
  int savedB = 0;  // number of bytes pushed to output
//AND offset >=?2 AND `offset` <?2

  const char * sql = "SELECT block_id,offset,capacity,data_len,data_block FROM file_blocks"
      " LEFT JOIN files using(file_id)"
      " WHERE file_path=?"
      " ORDER BY offset ASC"
      " LIMIT ? OFFSET ?;";

//  std::unique_ptr<FSBlock> fBlk(new FSBlock(_blkSize));
  int oneBC = _blkSize;  // default block capacity, TODO make constant

  int limitOffs = offset / oneBC;  // SELECT LIMIT's OFFSET
  int dataOffset = offset % oneBC;  // data start in block
  int limit = remainB / oneBC + 1;  // SELECT's LIMIT
  sqlite3_stmt *stm;
  int bindPos;

  result = sqlite3_prepare_v2(_db, sql, -1, &stm, nullptr);
  if (result != SQLITE_OK) {
    LOG(ERROR) << "sqlite3_prepare_v2 " << sqlite3_errstr(result);
  } else {

    if (result == SQLITE_OK) {  // if kept for easy copy/paste
      bindPos = 1;
      result = sqlite3_bind_text(stm, bindPos, fileName.c_str(), fileName.length(),
      SQLITE_STATIC);
    }

    if (result == SQLITE_OK) {
      bindPos = 2;
      result = sqlite3_bind_int(stm, bindPos, limit);
    }

    if (result == SQLITE_OK) {
      bindPos = 3;
      result = sqlite3_bind_int(stm, bindPos, limitOffs);
    }

    if (result != SQLITE_OK) {
      LOG(ERROR) << "Read sqlite3_bind pos_" << bindPos << " " << sqlite3_errstr(result);
    }
  }

  if (result == SQLITE_OK) {
    const char *pInData;  // read from
    char *pOutData = dst;   // write to
    int maxB;  // max amount to copy
    int blobLen;

    // time to execute
    while (result != SQLITE_DONE || remainB > 0) {
      result = sqlite3_step(stm);

      if (result == SQLITE_ROW) {
        // process row
        // read row data and push to dst
        pInData = static_cast<const char*>(sqlite3_column_blob(stm, 4));
        blobLen = sqlite3_column_bytes(stm, 4);  // TODO compare against data_len

        if (pInData != nullptr) {
          pInData += dataOffset;  //
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

      } else if (result == SQLITE_BUSY) {
        sqlite3_sleep(100);  // TODO check if used correctly, make constant
        continue;  // TODO maybe sleep or smthng

      } else {
        break;  // Something went wrong or done
      }

    }  // end while

    if (result != SQLITE_DONE) {
      LOG(ERROR) << "Read sqlite3_step " << result << " " << sqlite3_errstr(result);
    }
    result = sqlite3_finalize(stm);

  }

  *inOutLen = savedB;

  return SQL_ERROR(result);
}

/**
 *
 */
int FsDelegateSqlite::WriteData(const std::string& fileName, const char *src, int len, int offset) {
  int result = 1;
  std::unique_ptr<FSBlock> fsBlk(new FSBlock(_blkSize));

  result = dbGetBlockByOffset(fileName, fsBlk, offset);

  if (result == SQLITE_OK) {
    int fileSize = 0;

    if (fsBlk->block_id != -1) {  // block allready exists
      int dataOffset = offset % _blkSize;  // where data starts for |offset| in block

      if (fsBlk->data_len < dataOffset) {
        LOG(ERROR) << "Write gap in data, use Insert instead!";

        result = SQLITE_INTERNAL;
      } else {
        if (fsBlk->data_len != dataOffset) {  // need to update db
          fsBlk->data_len = dataOffset;  // usefull data (drop if any after)
          fileSize = fsBlk->offset * _blkSize + dataOffset;

          // update file size
          result = dbSetFileSize(fileName, fileSize);

          if (result == SQLITE_OK) {
            // update current block
            result = dbUpdateBlock(fileName, fsBlk);
          }

          if (result == SQLITE_OK) {
            // delete blocks after fsBlk->offset if any
            dbDeleteBlocksAfter(fileName, (fsBlk->offset + 1));  // don't delete me!
          }
        } else {
          // no need to change anything
        }
      }
    }  // fsBlk->block_id != -1
  }

  if (result == SQLITE_OK) {
    result = AppendData(fileName, src, len);
  }

  return SQL_ERROR(result);
}

/**
 *
 */
int FsDelegateSqlite::dbGetBlockByOffset(const std::string& fileName, std::unique_ptr<FSBlock>& out, int offset) {
  int result = 1;

  const char * sql = "SELECT block_id,offset,capacity,data_len,data_block FROM file_blocks"
      " LEFT JOIN files using(file_id)"
      " WHERE file_path=?"
      " AND offset =?;";

  int dataOffset = offset % _blkSize;  // where data starts for |offset| in block
  int dbOffset = offset - dataOffset;  // block offset in db

  sqlite3_stmt *stm;
  int bindPos;  // bind parameter position to count where error occurred

  result = sqlite3_prepare_v2(_db, sql, -1, &stm, nullptr);
  if (result != SQLITE_OK) {
    LOG(ERROR) << "dbGetBlockByOffset sqlite3_prepare_v2 " << sqlite3_errstr(result);
  } else {

    if (result == SQLITE_OK) {  // if kept for easy copy/paste
      bindPos = 1;
      result = sqlite3_bind_text(stm, bindPos, fileName.c_str(), fileName.length(),
      SQLITE_STATIC);
    }

    if (result == SQLITE_OK) {
      bindPos = 2;
      result = sqlite3_bind_int(stm, bindPos, dbOffset);
    }

    if (result != SQLITE_OK) {
      LOG(ERROR) << "dbGetBlockByOffset sqlite3_bind pos_" << bindPos << " " << sqlite3_errstr(result);
    }
  }

  if (result == SQLITE_OK) {
    const char *pInData;  // read from
//    char *pOutData = dst;   // write to
//    int maxB;  // max amount to copy
    int blobLen;
    bool gotData = false;

    // time to execute
    while (result != SQLITE_DONE) {
      result = sqlite3_step(stm);

      if (result == SQLITE_ROW && !gotData) {
        // process row block_id,offset,capacity,data_len,data_block
        // read row data and push to dst
        pInData = static_cast<const char*>(sqlite3_column_blob(stm, 4));
        blobLen = sqlite3_column_bytes(stm, 4);  // TODO compare against data_len

        if (pInData != nullptr) {
          out->data.assign(pInData, blobLen);

          out->block_id = sqlite3_column_int(stm, 0);
          out->offset = sqlite3_column_int(stm, 1);
          out->capacity = sqlite3_column_int(stm, 2);  // TODO assert if block size != _blkSize
          out->data_len = blobLen;

          gotData = true;
        } else {
          LOG(ERROR) << "dbGetBlockByOffset got NULL blob";
          break;
        }

      } else if (result == SQLITE_BUSY) {
        sqlite3_sleep(100);  // TODO check if used correctly, make constant
        continue;  // TODO maybe sleep or smthng

      } else {
        break;  // Something went wrong or done
      }

    }  // end while

    if (result != SQLITE_DONE) {
      LOG(ERROR) << "Read sqlite3_step " << result << " " << sqlite3_errstr(result);
    }
    result = sqlite3_finalize(stm);

  }

  return result;
}

/**
 *
 */
int FsDelegateSqlite::dbIncFileSize(const std::string& fileName, int incBy) {
  int result = -1;

  base::SStringPrintf(&g_sqlStore, "UPDATE files SET file_length=file_length+%d WHERE file_path='%s';", incBy, fileName.c_str());
  result = execQuery(g_sqlStore.c_str(), nullptr);

  return result;
}

/**
 *
 */
int FsDelegateSqlite::dbInsertBlock(const std::string& fileName, const std::unique_ptr<FSBlock>& in) {

  // need statement and stuff
  int result = -1;
  int bindPos = -1;
  sqlite3_stmt *stm;

  const char* sql = "INSERT INTO file_blocks(file_id,offset,capacity,data_len,data_block) "
      "VALUES((SELECT file_id FROM files WHERE file_path=?),?,?,?,?);";

  result = sqlite3_prepare_v2(_db, sql, -1, &stm, nullptr);
  if (result != SQLITE_OK) {
    LOG(ERROR) << "sqlite3_prepare_v2 " << sqlite3_errstr(result);
  } else {

    if (result == SQLITE_OK) {  // if kept for easy copy/paste
      bindPos = 1;
      result = sqlite3_bind_text(stm, bindPos, fileName.c_str(), fileName.length(),
      SQLITE_STATIC);
    }

    if (result == SQLITE_OK) {
      bindPos = 2;
      result = sqlite3_bind_int(stm, bindPos, in->offset);
    }

    if (result == SQLITE_OK) {
      bindPos = 3;
      result = sqlite3_bind_int(stm, bindPos, in->capacity);
    }

    if (result == SQLITE_OK) {
      bindPos = 4;
      result = sqlite3_bind_int(stm, bindPos, in->data_len);
    }

    if (result == SQLITE_OK) {
      bindPos = 5;
      result = sqlite3_bind_blob(stm, bindPos, in->data.c_str(), in->data_len,
      SQLITE_STATIC);
    }

    if (result != SQLITE_OK) {
      LOG(ERROR) << "dbInsertBlock sqlite3_bind pos_" << bindPos << " " << sqlite3_errstr(result);
    }
  }

  if (result == SQLITE_OK) {  // continue, we have the bindings
    result = sqlite3_step(stm);

    if (result != SQLITE_DONE) {
      LOG(INFO) << "dbInsertBlock sqlite3_step " << result << " " << sqlite3_errstr(result);
    }
    result = sqlite3_finalize(stm);
  }

  return result;
}

/**
 *
 */
int FsDelegateSqlite::dbUpdateBlock(const std::string& fileName, const std::unique_ptr<FSBlock>& in) {

  // need statement and stuff
  int result = -1;
  int bindPos = -1;
  sqlite3_stmt *stm;

  const char* sql = "UPDATE file_blocks SET data_block=?,data_len=?"
      " WHERE block_id=?;";

  result = sqlite3_prepare_v2(_db, sql, -1, &stm, nullptr);
  if (result != SQLITE_OK) {
    LOG(ERROR) << "sqlite3_prepare_v2 " << sqlite3_errstr(result);
  } else {
    result = sqlite3_bind_int(stm, 3, in->block_id);
    bindPos = 3;

    if (result == SQLITE_OK) {
      result = sqlite3_bind_int(stm, 2, in->data_len);
      bindPos = 2;
    }

    if (result == SQLITE_OK) {
      result = sqlite3_bind_blob(stm, 1, in->data.c_str(), in->data_len,
      SQLITE_STATIC);
      bindPos = 1;
    }

    if (result != SQLITE_OK) {
      LOG(ERROR) << "sqlite3_bind pos_" << bindPos << " " << sqlite3_errstr(result);
    }
  }

  if (result == SQLITE_OK) {  // continue, we have the bindings
    result = sqlite3_step(stm);

    result = sqlite3_finalize(stm);
  }

  return result;
}

/**
 *
 */
int FsDelegateSqlite::dbGetLastBlock(const std::string& fileName, std::unique_ptr<FSBlock>& out) {
  int result = -1;
  base::SStringPrintf(&g_sqlStore, "SELECT block_id,offset,capacity,data_len,data_block FROM file_blocks b"
                      " LEFT JOIN files f USING(file_id)"
                      " WHERE file_path=\"%s\""
                      " ORDER BY offset DESC"
                      " LIMIT 1;",
                      fileName.c_str());

  result = execQuery(g_sqlStore.c_str(), &FsDelegateSqlite::cbGetLastBlock, out.get());

  return result;
}

/**
 *
 */
int FsDelegateSqlite::dbOpenFile(const std::string& fileName, int mode/*bitmap OpenMode*/) {
  int result = -1;

  if (mode & OM_TRUNCATE_IF_EXISTS) {
    base::SStringPrintf(&g_sqlStore, "UPDATE files SET status=%d, file_length=0 "
                        "WHERE file_path=\"%s\";",
                        FS_OPEN_RW, fileName.c_str());

    result = execQuery(g_sqlStore.c_str(), nullptr);
    if (result == SQLITE_OK) {
      _fStatus = FS_OPEN_RW;
      // delete blocks
      result = dbDeleteBlocksAfter(fileName, 0);
    }
  } else {
    result = dbSetFileStatus(fileName, FS_OPEN_RW);
  }

  return result;
}

/**
 *
 */
int FsDelegateSqlite::dbCreateFile(const std::string& fileName) {
  base::SStringPrintf(&g_sqlStore, "INSERT INTO files (`file_path`, `status`) VALUES(\"%s\", %d);", fileName.c_str(), FS_OPEN_RW);

  return execQuery(g_sqlStore.c_str(), &FsDelegateSqlite::cbJustPrint);
}

/**
 * Read file status from db if file exists
 * @return <0 if error, else return FileStatus
 */
int FsDelegateSqlite::dbGetFileStatus(const std::string& fileName) {
  _fStatus = FS_INVALID;

  int result = -1;
  base::SStringPrintf(&g_sqlStore, "SELECT `status` FROM files WHERE `file_path`=\"%s\";", fileName.c_str());
// TODO check result
  result = execQuery(g_sqlStore.c_str(), &FsDelegateSqlite::cbGetStatus, &_fStatus);

  if (result != SQLITE_OK) {
    LOG(INFO) << "file " << fileName << " not in db" << sqlite3_errstr(result);
  }

  return _fStatus;
}

/**
 *
 */
int FsDelegateSqlite::dbGetFileLength(const std::string& fileName) {
  _fStatus = FS_INVALID;

  int result = -1;
  int length;

  base::SStringPrintf(&g_sqlStore, "SELECT file_length FROM files WHERE file_path=\"%s\";", fileName.c_str());
  result = execQuery(g_sqlStore.c_str(), &FsDelegateSqlite::cbGetStatus, &length);

  if (result == SQLITE_OK) {
    return length;
  }

  return SQL_ERROR(result);  // negative value

}

/**
 *
 */
int FsDelegateSqlite::dbSetFileStatus(const std::string& fileName, FileStatus fs) {
  int result = -1;

  base::SStringPrintf(&g_sqlStore, "UPDATE files SET status=%d WHERE file_path=\"%s\";", fs, fileName.c_str());
  result = execQuery(g_sqlStore.c_str(), nullptr);

  return result;
}

/**
 *
 */
int FsDelegateSqlite::dbSetFileSize(const std::string& fileName, int newSize) {
  int result = -1;

  base::SStringPrintf(&g_sqlStore, "UPDATE files SET file_length=%d WHERE file_path=\"%s\";", newSize, fileName.c_str());
  result = execQuery(g_sqlStore.c_str(), nullptr);

  return result;
}

/**
 * Delete file blocks after offset, including offset too
 */
int FsDelegateSqlite::dbDeleteBlocksAfter(const std::string& fileName, int offset) {
  int result = -1;

  base::SStringPrintf(&g_sqlStore, "DELETE FROM file_blocks WHERE file_id IN"
                      " (SELECT file_id FROM files WHERE file_path=\"%s\")"
                      " AND offset >=%d;",
                      fileName.c_str(), offset);

  result = execQuery(g_sqlStore.c_str(), nullptr);

  return result;
}

/**
 * Execute sql query using handler for response
 */
int FsDelegateSqlite::execQuery(const char* sql, cbFunction handler, void* extra) {
  CallbackHere cbPrint = { this, handler == nullptr ? &FsDelegateSqlite::cbJustPrint : handler, extra };
  int result = -1;

  result = sqlite3_exec(_db, sql, &cbDelegate, &cbPrint, nullptr);

  if (result != SQLITE_OK) {
    LOG(ERROR) << sqlite3_errstr(result) << " '" << sql << "'";
  }

  return result;
}

/**
 * Create path with parents
 */
int FsDelegateSqlite::createPaths(const base::FilePath& rootPath) {
  if (base::CreateDirectoryAndGetError(rootPath, nullptr)) {
    return 0;  // TODO
  }

  return -102;  // TODO
}

/**
 * Initialize db structure
 */
int FsDelegateSqlite::initDb() {
  int result = -1;

  const char * createSql = "PRAGMA foreign_keys = ON;"
      "CREATE TABLE IF NOT EXISTS files("
      "file_id integer primary key autoincrement,"
      "file_path text unique,file_length integer default 0,"
      "status integer(2) default 0);"
      "CREATE TABLE IF NOT EXISTS file_blocks("
      "block_id integer primary key autoincrement,"
      "file_id integer references files ON DELETE CASCADE,"
      "offset integer default 0,capacity integer default 4096,"
      "data_len integer default 0,"
      "data_block text,"
      "UNIQUE(file_id,offset));";

  result = execQuery(createSql, &FsDelegateSqlite::cbJustPrint);

  if (result != SQLITE_OK) {
    LOG(ERROR) << "DB init " << sqlite3_errstr(result);
  }

  return SQL_ERROR(result);
}

//static
int FsDelegateSqlite::cbDelegate(void* cbIn, int nrCol, char** values, char** columns) {
  if (cbIn != nullptr) {
    CallbackHere * cbHere = static_cast<CallbackHere*>(cbIn);
    FsDelegateSqlite::cbFunction f = (cbHere->_cb);

    return ((cbHere->_this)->*(f))(nrCol, values, columns, cbHere->extra);
  }
  return SQLITE_ERROR;
}

int FsDelegateSqlite::cbJustPrint(int nrCol, char** values, char** columns, void* extra) {
  int i;
  for (i = 0; i < nrCol; i++) {
    LOG(INFO) << columns[i] << "=" << (values[i] ? values[i] : "NULL");
  }
  return 0;
}

/**
 *
 */
int FsDelegateSqlite::cbGetStatus(int nrCol, char** values, char** columns, void* extra) {
  if (extra != nullptr) {
    int *p = static_cast<int*>(extra);
    *p = atoi(values[0]);
  }
//  _fStatus = atoi(values[0]); // TODO remove

//  printf("file status returned %d\n", _fStatus);

  return SQLITE_OK;
}

/**
 *
 */
int FsDelegateSqlite::cbGetLastBlock(int nrCol, char** values, char** columns, void* extra) {
  // block_id,offset,capacity,data_len,data_block
  if (extra != nullptr && values[0] != nullptr) {
    FSBlock * fb = static_cast<FSBlock*>(extra);

    fb->block_id = atoi(values[0]);
    fb->offset = atoi(values[1]);
    fb->capacity = atoi(values[2]);

    int len = atoi(values[3]);
    fb->data.assign(values[4], 0, len);
    fb->data_len = len;

    return SQLITE_OK;
  }
  return -1;
}

} /* namespace tenta */
} /* namespace xwalk */
