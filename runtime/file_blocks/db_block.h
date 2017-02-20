/*
 * db_block.h
 *
 *  Created on: Feb 14, 2017
 *      Author: iotto
 */

#ifndef REFACTOR_DB_BLOCK_H_
#define REFACTOR_DB_BLOCK_H_

#include <string>

namespace xwalk {
namespace tenta {
namespace fs {

/*
 * Feb 14, 2017
 * xwalk::tenta::fs::DbBlock
 * 
 */
class DbBlock {
 public:
  DbBlock(int capacity = SIZE_4K);
  virtual ~DbBlock();

  void reset(int newOffset = 0, int newCapacity = SIZE_4K);

  int offset;
  int capacity;
  std::string data;
  int data_len;
  int block_id = -1;  // TODO make protected

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

} /* namespace fs */
} /* namespace tenta */
} /* namespace xwalk */

#endif /* REFACTOR_DB_BLOCK_H_ */
