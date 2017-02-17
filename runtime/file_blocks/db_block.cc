/*
 * db_block.cc
 *
 *  Created on: Feb 14, 2017
 *      Author: iotto
 */

#include "db_block.h"

namespace xwalk {
namespace tenta {
namespace fs {

DbBlock::DbBlock(int capacity) {
  reset(0, capacity);
}

DbBlock::~DbBlock()
{
}

void DbBlock::reset(int newOffset, int newCapacity) {
  block_id = -1;
  data.clear();
  data_len = 0;
  capacity = newCapacity;
  offset = newOffset;
}

} /* namespace fs */
} /* namespace tenta */
} /* namespace xwalk */
