/*
 * file_block.cc
 *
 *  Created on: Feb 3, 2017
 *      Author: iotto
 */

#include "file_block.h"

namespace xwalk {
namespace tenta {

FileBlock::FileBlock() {
}

FileBlock::FileBlock(FsFactory *factory) {
  set_factory(factory);
}

FileBlock::~FileBlock() {
  // TODO Auto-generated destructor stub
}

int FileBlock::Open(const std::string& rootPath, const std::string& fileName/*, int rwMode*/) {
  // create delegate
  _fileName = fileName;

  _fsDelegate.reset();
  int result = _fsFactory->CreateDelegate(rootPath, _fsDelegate);

  if (result == 0) {
    TShDelegate d = _fsDelegate.lock();
    result = d->FileOpen(fileName, FsDelegate::OpenMode::OM_CREATE_IF_NOT_EXISTS);
  }

  return result;
}

/**
 *
 */
int FileBlock::Open(const std::string& rootPath, const std::string& fileName, FsDelegate::OpenMode mode, const std::string& encKey) {
  // create delegate
  _fileName = fileName;
  _fsDelegate.reset();

  int result = _fsFactory->CreateDelegate(rootPath, _fsDelegate);

  if (result == 0) {
    TShDelegate d = _fsDelegate.lock();
    result = d->FileOpen(fileName, mode);
  }

  return result;
}

/**
 *
 */
int FileBlock::Delete(bool permanent/* = true*/) {
  if (_fsDelegate.expired()) {
    return -200;  // TODO
  }

  TShDelegate d = _fsDelegate.lock();
  return d->FileDelete(_fileName, permanent);
}

/**
 *
 */
int FileBlock::Close() {
  if (_fsDelegate.expired()) {
    return -200;  // TODO
  }

  TShDelegate d = _fsDelegate.lock();
  return d->FileClose(_fileName);
}

/**
 *
 */
int FileBlock::Append(const char* data, int len) {
  if (_fsDelegate.expired()) {
    return -200;  // TODO
  }

  TShDelegate d = _fsDelegate.lock();
  return d->AppendData(_fileName, data, len);
}

/**
 *
 */
int FileBlock::Read(char* dst, int *inOutLen, int offset) {
  if (_fsDelegate.expired()) {
    return -200;  // TODO
  }

  TShDelegate d = _fsDelegate.lock();
  return d->ReadData(_fileName, dst, inOutLen, offset);
}

/**
 *
 */
int FileBlock::Write(const char* data, int len, int offset) {
  if (_fsDelegate.expired()) {
    return -200;  // TODO
  }

  TShDelegate d = _fsDelegate.lock();
  return d->WriteData(_fileName, data, len, offset);
}

/**
 *
 */
int FileBlock::Length() {
  if (_fsDelegate.expired()) {
    return -200;  // TODO
  }

  TShDelegate d = _fsDelegate.lock();
  return d->FileLength(_fileName);
}

} /* namespace tenta */
} /* namespace xwalk */
