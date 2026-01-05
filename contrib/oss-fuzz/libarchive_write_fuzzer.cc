/*
 * Archive write fuzzer for libarchive
 * Tests archive creation and writing code paths
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <vector>

#include "archive.h"
#include "archive_entry.h"
#include "fuzz_helpers.h"

static constexpr size_t kMaxInputSize = 64 * 1024;  // 64KB

// Simple data consumer

// Memory write callback
static std::vector<uint8_t> *g_output = nullptr;

static ssize_t write_callback(struct archive *a, void *client_data, const void *buffer, size_t length) {
  (void)a;
  (void)client_data;
  if (g_output && length > 0) {
    const uint8_t *buf = static_cast<const uint8_t*>(buffer);
    g_output->insert(g_output->end(), buf, buf + length);
  }
  return length;
}

static int close_callback(struct archive *a, void *client_data) {
  (void)a;
  (void)client_data;
  return ARCHIVE_OK;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
  if (len == 0 || len > kMaxInputSize) {
    return 0;
  }

  DataConsumer consumer(buf, len);
  std::vector<uint8_t> output;
  g_output = &output;

  struct archive *a = archive_write_new();
  if (a == NULL) {
    return 0;
  }

  // Select format based on input
  uint8_t format_choice = consumer.consume_byte() % 8;
  switch (format_choice) {
    case 0: archive_write_set_format_pax_restricted(a); break;
    case 1: archive_write_set_format_gnutar(a); break;
    case 2: archive_write_set_format_ustar(a); break;
    case 3: archive_write_set_format_cpio_newc(a); break;
    case 4: archive_write_set_format_zip(a); break;
    case 5: archive_write_set_format_7zip(a); break;
    case 6: archive_write_set_format_xar(a); break;
    default: archive_write_set_format_pax(a); break;
  }

  // Select compression based on input
  uint8_t filter_choice = consumer.consume_byte() % 6;
  switch (filter_choice) {
    case 0: archive_write_add_filter_gzip(a); break;
    case 1: archive_write_add_filter_bzip2(a); break;
    case 2: archive_write_add_filter_xz(a); break;
    case 3: archive_write_add_filter_zstd(a); break;
    case 4: archive_write_add_filter_none(a); break;
    default: archive_write_add_filter_none(a); break;
  }

  // Open for writing to memory
  if (archive_write_open(a, NULL, NULL, write_callback, close_callback) != ARCHIVE_OK) {
    archive_write_free(a);
    g_output = nullptr;
    return 0;
  }

  // Create entries based on remaining input
  int entry_count = 0;
  while (!consumer.empty() && entry_count < 10 && consumer.remaining() > 20) {
    struct archive_entry *entry = archive_entry_new();
    if (entry == NULL) break;

    // Set entry properties
    archive_entry_set_pathname(entry, consumer.consume_string(64));

    uint8_t ftype = consumer.consume_byte() % 4;
    mode_t mode;
    switch (ftype) {
      case 0: mode = S_IFREG | 0644; break;
      case 1: mode = S_IFDIR | 0755; break;
      case 2: mode = S_IFLNK | 0777; break;
      default: mode = S_IFREG | 0644; break;
    }
    archive_entry_set_mode(entry, mode);

    archive_entry_set_uid(entry, consumer.consume_u32() & 0xFFFF);
    archive_entry_set_gid(entry, consumer.consume_u32() & 0xFFFF);
    archive_entry_set_mtime(entry, consumer.consume_i64(), 0);

    // For regular files, write some data
    if (S_ISREG(mode)) {
      uint8_t data_buf[1024];
      size_t data_len = consumer.consume_bytes(data_buf, 1024);
      archive_entry_set_size(entry, data_len);

      if (archive_write_header(a, entry) == ARCHIVE_OK && data_len > 0) {
        archive_write_data(a, data_buf, data_len);
      }
    } else if (S_ISLNK(mode)) {
      archive_entry_set_symlink(entry, consumer.consume_string(64));
      archive_entry_set_size(entry, 0);
      archive_write_header(a, entry);
    } else {
      archive_entry_set_size(entry, 0);
      archive_write_header(a, entry);
    }

    archive_entry_free(entry);
    entry_count++;
  }

  archive_write_close(a);
  archive_write_free(a);
  g_output = nullptr;
  return 0;
}
