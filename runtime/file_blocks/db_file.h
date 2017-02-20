/*
 * db_file.h
 *
 *  Created on: Feb 13, 2017
 *      Author: iotto
 */

#ifndef SQLITE_DB_FILE_H_
#define SQLITE_DB_FILE_H_

#include <cstdint>
#include <memory>
#include <string>

namespace xwalk {
namespace tenta {
namespace fs {
class DbFileSystem;
class DbPath;

class DbFile {
 public:

  virtual ~DbFile();

  /**
   * Append content to current file
   * @param data
   * @param len
   * @return true if success; false otherwise @see error()
   */
  bool Append(const char* data, int len);

  /**
   * Write content starting from offset
   * Data after offset will be deleted
   *
   * @param data
   * @param len
   * @param offset
   * @return true if success; false otherwise @see error()
   */
  bool Write(const char* data, int len, int offset);

  /**
   * Read data starting at |offset| and length of |inOutLen|
   * @param dst Destination buffer
   * @param inOutLen IN(destination size), OUT(Nr of bytes copied)
   * @param offset Offset in file
   * @return true if success; false otherwise @see error()
   */
  bool Read(char* const dst, int * const inOutLen, int offset);

  bool Delete();

  /**
   * Close file.
   * No op is allowed after close
   * @return true if success; false otherwise @see error()
   */
  bool Close();

// Get file name
  const char* get_name() const {
    return _name.c_str();
  }

  const char* get_full_name() const;

  int get_length() const {
    return _length;
  }

  int get_status() const {
    return _status;
  }

  int error() {
    return _err;
  }

 private:
  friend class DbFileSystem;  // only DbFileSystem can create
  DbFile(const std::string& name, std::shared_ptr<DbFileSystem> dbfs);
  void set_path();

  int _err;

  uint64_t _file_id;
  std::string _name;
  int _length;
  int _status;
  int _blk_size;
  std::unique_ptr<DbPath> _path;
  std::weak_ptr<DbFileSystem> _weak_dbfs;
};

} /* namespace fs */
} /* namespace tenta */
} /* namespace xwalk */

#endif /* SQLITE_DB_FILE_H_ */
