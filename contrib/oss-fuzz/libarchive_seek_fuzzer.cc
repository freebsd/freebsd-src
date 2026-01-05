/*
 * Archive seek/read fuzzer for libarchive
 * Tests seeking within archives and reading at random positions
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <vector>

#include "archive.h"
#include "archive_entry.h"
#include "fuzz_helpers.h"

static constexpr size_t kMaxInputSize = 256 * 1024;

struct SeekableBuffer {
  const uint8_t *buf;
  size_t len;
  size_t pos;
};

static ssize_t seek_read_callback(struct archive *a, void *client_data,
                                  const void **block) {
  (void)a;
  SeekableBuffer *buffer = reinterpret_cast<SeekableBuffer *>(client_data);
  if (buffer->pos >= buffer->len) {
    *block = NULL;
    return 0;
  }
  *block = buffer->buf + buffer->pos;
  size_t avail = buffer->len - buffer->pos;
  size_t to_read = (avail > 4096) ? 4096 : avail;
  buffer->pos += to_read;
  return to_read;
}

static la_int64_t seek_callback(struct archive *a, void *client_data,
                                la_int64_t offset, int whence) {
  (void)a;
  SeekableBuffer *buffer = reinterpret_cast<SeekableBuffer *>(client_data);
  la_int64_t new_pos;

  switch (whence) {
    case SEEK_SET:
      new_pos = offset;
      break;
    case SEEK_CUR:
      new_pos = static_cast<la_int64_t>(buffer->pos) + offset;
      break;
    case SEEK_END:
      new_pos = static_cast<la_int64_t>(buffer->len) + offset;
      break;
    default:
      return ARCHIVE_FATAL;
  }

  if (new_pos < 0) new_pos = 0;
  if (new_pos > static_cast<la_int64_t>(buffer->len))
    new_pos = static_cast<la_int64_t>(buffer->len);

  buffer->pos = static_cast<size_t>(new_pos);
  return new_pos;
}

static la_int64_t skip_callback(struct archive *a, void *client_data,
                                la_int64_t request) {
  (void)a;
  SeekableBuffer *buffer = reinterpret_cast<SeekableBuffer *>(client_data);
  size_t avail = buffer->len - buffer->pos;
  la_int64_t to_skip = (request > static_cast<la_int64_t>(avail))
                           ? static_cast<la_int64_t>(avail)
                           : request;
  buffer->pos += static_cast<size_t>(to_skip);
  return to_skip;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
  if (len == 0 || len > kMaxInputSize) {
    return 0;
  }

  struct archive *a = archive_read_new();
  if (a == NULL) {
    return 0;
  }

  // Enable formats that benefit from seeking
  archive_read_support_format_zip_seekable(a);
  archive_read_support_format_7zip(a);
  archive_read_support_format_rar(a);
  archive_read_support_format_rar5(a);
  archive_read_support_format_iso9660(a);
  archive_read_support_filter_all(a);

  SeekableBuffer buffer = {buf, len, 0};

  archive_read_set_read_callback(a, seek_read_callback);
  archive_read_set_seek_callback(a, seek_callback);
  archive_read_set_skip_callback(a, skip_callback);
  archive_read_set_callback_data(a, &buffer);

  if (archive_read_open1(a) != ARCHIVE_OK) {
    archive_read_free(a);
    return 0;
  }

  std::vector<uint8_t> data_buffer(4096, 0);
  struct archive_entry *entry;
  int entry_count = 0;

  while (archive_read_next_header(a, &entry) == ARCHIVE_OK && entry_count < 50) {
    archive_entry_pathname(entry);
    archive_entry_size(entry);

    // Read data which may trigger seeks
    ssize_t r;
    while ((r = archive_read_data(a, data_buffer.data(), data_buffer.size())) > 0)
      ;

    entry_count++;
  }

  archive_read_free(a);
  return 0;
}
