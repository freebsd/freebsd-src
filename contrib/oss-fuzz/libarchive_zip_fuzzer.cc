/*
 * ZIP format fuzzer for libarchive
 * Tests ZIP with various compression methods and encryption
 */
#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "archive.h"
#include "archive_entry.h"
#include "fuzz_helpers.h"

static constexpr size_t kMaxInputSize = 512 * 1024;



extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
  if (len == 0 || len > kMaxInputSize) {
    return 0;
  }

  struct archive *a = archive_read_new();
  if (a == NULL) {
    return 0;
  }

  archive_read_support_format_zip(a);
  archive_read_support_filter_all(a);

  // Add passphrase for encrypted ZIPs
  archive_read_add_passphrase(a, "password");
  archive_read_add_passphrase(a, "test");
  archive_read_add_passphrase(a, "");

  // Enable ZIP options
  archive_read_set_options(a, "zip:ignorecrc32");

  Buffer buffer = {buf, len, 0};
  if (archive_read_open(a, &buffer, NULL, reader_callback, NULL) != ARCHIVE_OK) {
    archive_read_free(a);
    return 0;
  }

  std::vector<uint8_t> data_buffer(4096, 0);
  struct archive_entry *entry;

  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    archive_entry_pathname(entry);
    archive_entry_pathname_w(entry);
    archive_entry_size(entry);
    archive_entry_mtime(entry);
    archive_entry_mode(entry);
    archive_entry_is_encrypted(entry);
    archive_entry_is_data_encrypted(entry);
    archive_entry_is_metadata_encrypted(entry);

    // Check compression name
    archive_format_name(a);
    archive_filter_name(a, 0);

    ssize_t r;
    while ((r = archive_read_data(a, data_buffer.data(), data_buffer.size())) > 0)
      ;
  }

  archive_read_free(a);
  return 0;
}
