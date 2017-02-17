/*
 * db_file.cc
 *
 *  Created on: Feb 13, 2017
 *      Author: iotto
 */

#include <base/files/file_path.h>
#include <base/logging.h>
#include "db_block.h"
#include "db_errors.h"
#include "db_file.h"
#include "db_file_system.h"
#include "db_path.h"
#include <iostream>

//#include "net/quic/crypto/quic_random.h"

namespace xwalk {
namespace tenta {
namespace fs {

DbFile::DbFile(const std::string& name, std::shared_ptr<DbFileSystem> dbFs)
    : _name(name),
      _weak_dbfs(dbFs) {
}

DbFile::~DbFile() {
  Close();
}

const char* DbFile::get_full_name() const {
  base::FilePath fp(_path->_path_name);
  return fp.Append(_name).value().c_str();
}

/**
 * Write content starting from offset
 * Data after offset will be deleted
 *
 * @param data
 * @param len
 * @param offset
 * @return true if success; false otherwise @see error()
 */
bool DbFile::Write(const char* data, int len, int offset) {
  if (data == nullptr || len == 0) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "Read invalid parameters";
    return false;
  }

  std::shared_ptr<DbFileSystem> dbfs = _weak_dbfs.lock();

  if (_weak_dbfs.expired()) {
    _err = DB_ERR_INVALID_FS;
    return false;
  }

  // TODO transaction

  DbBlock blk;
  bool result = false;

  result = dbfs->dbGetBlockByOffset(this, &blk, offset);
  if (result) {
    int dataOffset = offset % _blk_size;  // where data starts for |offset| in block

    if (blk.data_len < dataOffset) {
      LOG(ERROR) << "Write gap in data! block length: " << blk.data_len
                 << " offset "
                 << dataOffset;

      _err = -101;  // TODO define ERROR file gap
      return false;
    }

    if (blk.data_len != dataOffset) {  // are we right at the end of this data block?
      int fileSize = 0;

      blk.data_len = dataOffset;  // usefull data (drop if any after)
      fileSize = blk.offset * _blk_size + dataOffset;  // recalculate file size

      // update file size
      result = dbfs->dbSetFileSize(this, fileSize);

      if (result) {
        // update current block, set new data length
        result = dbfs->dbUpdateBlock(this, &blk);
      }

      if (result) {
        // delete blocks after blk.offset if any
        result = dbfs->dbDeleteBlocksAfter(this, (blk.offset + 1));  // don't delete me!
      }
    }  //if ( blk.data_len != dataOffset)
  }  // get block by offset

  if (!result) {
    _err = dbfs->error();
    return false;
  }

  return Append(data, len);
}

/**
 * Read data starting at |offset| and length of |inOutLen|
 * @param dst Destination buffer
 * @param inOutLen IN(destination size), OUT(Nr of bytes copied)
 * @param offset Offset in file
 * @return true if success; false otherwise @see error()
 */
bool DbFile::Read(char* const dst, int * const inOutLen, int offset) {
  if (dst == nullptr || *inOutLen == 0) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "Read invalid parameters";
    return false;
  }
  std::shared_ptr<DbFileSystem> dbfs = _weak_dbfs.lock();

  if (_weak_dbfs.expired()) {
    _err = DB_ERR_INVALID_FS;
    return false;
  }

  if ( !dbfs->dbReadFile(this, dst, inOutLen, offset) )
  {
    _err = dbfs->error();
    return false;
  }

  return true;
}

/**
 * Append content to current file
 * @param data
 * @param len
 * @return true if success; false otherwise @see error()
 */
bool DbFile::Append(const char* data, int len) {
  if (data == nullptr || len == 0) {
    _err = DB_ERR_INVALID_PARAM;
    LOG(ERROR) << "Append invalid parameters";
    return false;
  }

  std::shared_ptr<DbFileSystem> dbfs = _weak_dbfs.lock();

  if (_weak_dbfs.expired()) {
    _err = DB_ERR_INVALID_FS;
    return false;
  }

  DbBlock blk;
  int nextOffset = 0;
  int oneBC = _blk_size;  // default block capacity
  int freeB = 0;  // free bytes in data buffer
  int maxB = 0;  // max amount to copy
  int remainB = len;  // byte remained to save

  bool result = false;

  result = dbfs->dbGetLastBlock(this, &blk);
  if (result) {
    if (blk.block_id > 0) {  // have last block so append to it
      freeB = blk.capacity - blk.data_len;
      nextOffset = blk.offset;  // update offset to last offset
      oneBC = blk.capacity;

      if (freeB > 0) {
        // fill in and push max available or len
        maxB = freeB > remainB ? remainB : freeB;

        blk.data.append(data, maxB);
        data += maxB;
        remainB -= maxB;
        blk.data_len += maxB;

        // push to db
        result = dbfs->dbUpdateBlock(this, &blk);
      }
      // next blocks offset (makes sense only if buffer overflow, otherwise will be ignored)
      nextOffset += oneBC;
    }

    while (result && (remainB > 0)) {
      // create new block and push to db
      blk.reset(nextOffset, oneBC);

      freeB = blk.capacity;  // amount can be copied to data
      maxB = freeB > remainB ? remainB : freeB;

      blk.data.assign(data, maxB);
      data += maxB;
      remainB -= maxB;
      blk.data_len += maxB;

      // push to db
      result = dbfs->dbInsertBlock(this, &blk);

      nextOffset += oneBC;
    }

    // update file length+len
    if (result) {
      result = dbfs->dbIncFileSize(this, len);
    }
  }

  if (!result) {
    _err = dbfs->_err;
    return false;
  }

  return true;
}

/**
 * Close file.
 * No op is allowed after close
 * @return true if success; false otherwise @see error()
 */
bool DbFile::Close() {
  std::shared_ptr<DbFileSystem> dbfs = _weak_dbfs.lock();

  if (_weak_dbfs.expired()) {
    _err = DB_ERR_INVALID_FS;
    return false;
  }

  if ( !dbfs->FileClose(this) )
  {
    _err = dbfs->error();
    return false;
  }

  return true;
}

} /* namespace fs */
} /* namespace tenta */
} /* namespace xwalk */
