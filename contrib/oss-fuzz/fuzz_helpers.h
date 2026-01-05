// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIBARCHIVE_FUZZ_HELPERS_H_
#define LIBARCHIVE_FUZZ_HELPERS_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ftw.h>
#include <unistd.h>

#include "archive.h"

// Default maximum input size for fuzzers
static constexpr size_t kDefaultMaxInputSize = 256 * 1024;  // 256KB

// Buffer structure for archive reading callbacks
struct Buffer {
  const uint8_t* data;
  size_t size;
  size_t pos;
};

// Archive read callback function
static la_ssize_t reader_callback(struct archive* a, void* client_data,
                                  const void** buffer) {
  (void)a;
  Buffer* buf = static_cast<Buffer*>(client_data);

  if (buf->pos >= buf->size) {
    return 0;  // EOF
  }

  *buffer = buf->data + buf->pos;
  size_t remaining = buf->size - buf->pos;
  buf->pos = buf->size;  // Consume all remaining data
  return static_cast<la_ssize_t>(remaining);
}

// Helper class for consuming fuzz data in structured ways
class DataConsumer {
 public:
  DataConsumer(const uint8_t* data, size_t size)
      : data_(data), size_(size), pos_(0) {}

  bool empty() const { return pos_ >= size_; }
  size_t remaining() const { return size_ - pos_; }

  uint8_t consume_byte() {
    if (pos_ >= size_) return 0;
    return data_[pos_++];
  }

  uint16_t consume_u16() {
    uint16_t val = 0;
    if (pos_ + 2 <= size_) {
      val = static_cast<uint16_t>(data_[pos_]) |
            (static_cast<uint16_t>(data_[pos_ + 1]) << 8);
      pos_ += 2;
    }
    return val;
  }

  uint32_t consume_u32() {
    uint32_t val = 0;
    if (pos_ + 4 <= size_) {
      val = static_cast<uint32_t>(data_[pos_]) |
            (static_cast<uint32_t>(data_[pos_ + 1]) << 8) |
            (static_cast<uint32_t>(data_[pos_ + 2]) << 16) |
            (static_cast<uint32_t>(data_[pos_ + 3]) << 24);
      pos_ += 4;
    }
    return val;
  }

  int64_t consume_i64() {
    int64_t val = 0;
    if (pos_ + 8 <= size_) {
      for (int i = 0; i < 8; i++) {
        val |= static_cast<int64_t>(data_[pos_ + i]) << (8 * i);
      }
      pos_ += 8;
    }
    return val;
  }

  // Consume a null-terminated string up to max_len characters
  // Returns pointer to internal buffer (valid until next consume_string call)
  const char* consume_string(size_t max_len) {
    if (max_len > sizeof(string_buf_) - 1) {
      max_len = sizeof(string_buf_) - 1;
    }
    size_t avail = size_ - pos_;
    size_t len = (avail < max_len) ? avail : max_len;
    size_t actual_len = 0;

    while (actual_len < len && pos_ < size_) {
      char c = static_cast<char>(data_[pos_++]);
      if (c == '\0') break;
      string_buf_[actual_len++] = c;
    }
    string_buf_[actual_len] = '\0';
    return string_buf_;
  }

  // Consume raw bytes into a buffer
  size_t consume_bytes(void* out, size_t len) {
    size_t avail = size_ - pos_;
    size_t to_copy = (avail < len) ? avail : len;
    if (to_copy > 0) {
      memcpy(out, data_ + pos_, to_copy);
      pos_ += to_copy;
    }
    return to_copy;
  }

  // Get remaining data as a buffer
  const uint8_t* remaining_data() const {
    return data_ + pos_;
  }

 private:
  const uint8_t* data_;
  size_t size_;
  size_t pos_;
  char string_buf_[512];
};

// Callback for nftw to remove files/directories
static int remove_callback(const char* fpath, const struct stat* sb,
                           int typeflag, struct FTW* ftwbuf) {
  (void)sb;
  (void)typeflag;
  (void)ftwbuf;
  return remove(fpath);
}

// Recursively remove a directory tree (safer than system("rm -rf ..."))
static int remove_directory_tree(const char* path) {
  // nftw with FTW_DEPTH processes directory contents before the directory itself
  return nftw(path, remove_callback, 64, FTW_DEPTH | FTW_PHYS);
}

#endif  // LIBARCHIVE_FUZZ_HELPERS_H_
