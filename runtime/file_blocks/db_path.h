/*
 * db_path.h
 *
 *  Created on: Feb 13, 2017
 *      Author: iotto
 */

#ifndef REFACTOR_DB_PATH_H_
#define REFACTOR_DB_PATH_H_

#include <stdint.h>
#include <memory>
#include <ostream>
#include <string>

namespace xwalk {
namespace tenta {
namespace fs {
class DbFileSystem;
class DbFile;

class DbPath {
 public:
  virtual ~DbPath();

  std::string get_path() {
    return _path_name;
  }

 private:
  // can be accessed only by these
  friend std::ostream & operator<<(std::ostream & ostr, DbPath const * p);
  friend std::ostream & operator<<(std::ostream & ostr,
                                   std::unique_ptr<DbPath> const & p);
  friend class DbFileSystem;
  friend class DbFile;
  friend class std::unique_ptr<DbPath>;

  DbPath();

 private:
  // id(s) from db, must be kept private!
  uint64_t _path_id;
  uint64_t _parent_id;
  std::string _path_name;

};

inline std::ostream & operator<<(std::ostream & ostr, DbPath const * p) {
  ostr << "DbPath[id=" << p->_path_id << ",parent=" << p->_parent_id
       << ",path=\""
       << p->_path_name << "\"]";
  return ostr;
}

inline std::ostream & operator<<(std::ostream & ostr,
                                 std::unique_ptr<DbPath> const & p) {
  ostr << "DbPath[id=" << p->_path_id << ",parent=" << p->_parent_id
       << ",path=\""
       << p->_path_name << "\"]";
  return ostr;
}

} /* namespace fs */
} /* namespace tenta */
} /* namespace xwalk */

#endif /* REFACTOR_DB_PATH_H_ */
