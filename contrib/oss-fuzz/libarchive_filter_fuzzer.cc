/*
 * Compression filter fuzzer for libarchive
 * Tests decompression of gzip, bzip2, xz, lzma, zstd, lz4, etc.
 */
#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "archive.h"
#include "archive_entry.h"
#include "fuzz_helpers.h"

static constexpr size_t kMaxInputSize = 256 * 1024;



extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
  if (len == 0 || len > kMaxInputSize) {
    return 0;
  }

  struct archive *a = archive_read_new();
  if (a == NULL) {
    return 0;
  }

  // Enable raw format (just decompress, no archive format)
  archive_read_support_format_raw(a);

  // Enable all compression filters
  archive_read_support_filter_all(a);

  Buffer buffer = {buf, len, 0};
  if (archive_read_open(a, &buffer, NULL, reader_callback, NULL) != ARCHIVE_OK) {
    archive_read_free(a);
    return 0;
  }

  std::vector<uint8_t> data_buffer(8192, 0);
  struct archive_entry *entry;

  if (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    // Get filter info
    int filter_count = archive_filter_count(a);
    for (int i = 0; i < filter_count; i++) {
      archive_filter_name(a, i);
      archive_filter_code(a, i);
      archive_filter_bytes(a, i);
    }

    // Read all decompressed data
    ssize_t total = 0;
    ssize_t r;
    while ((r = archive_read_data(a, data_buffer.data(), data_buffer.size())) > 0) {
      total += r;
      // Limit total decompressed size to prevent zip bombs
      if (total > 10 * 1024 * 1024) {
        break;
      }
    }
  }

  archive_read_free(a);
  return 0;
}
