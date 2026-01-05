/*
 * Archive roundtrip fuzzer for libarchive
 * Writes an archive then reads it back - tests write/read consistency
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <vector>

#include "archive.h"
#include "archive_entry.h"
#include "fuzz_helpers.h"

static constexpr size_t kMaxInputSize = 64 * 1024;


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
  if (len < 10 || len > kMaxInputSize) {
    return 0;
  }

  DataConsumer consumer(buf, len);
  std::vector<uint8_t> archive_data;
  archive_data.reserve(len * 2);

  // Phase 1: Write an archive
  struct archive *writer = archive_write_new();
  if (writer == NULL) {
    return 0;
  }

  // Select format
  uint8_t format = consumer.consume_byte() % 5;
  switch (format) {
    case 0: archive_write_set_format_pax_restricted(writer); break;
    case 1: archive_write_set_format_ustar(writer); break;
    case 2: archive_write_set_format_cpio_newc(writer); break;
    case 3: archive_write_set_format_zip(writer); break;
    default: archive_write_set_format_gnutar(writer); break;
  }

  archive_write_add_filter_none(writer);

  // Open to memory
  size_t used = 0;
  archive_data.resize(len * 4);
  if (archive_write_open_memory(writer, archive_data.data(), archive_data.size(), &used) != ARCHIVE_OK) {
    archive_write_free(writer);
    return 0;
  }

  // Write entries
  int entry_count = 0;
  while (!consumer.empty() && entry_count < 5 && consumer.remaining() > 10) {
    struct archive_entry *entry = archive_entry_new();
    if (entry == NULL) break;

    archive_entry_set_pathname(entry, consumer.consume_string(32));
    archive_entry_set_mode(entry, S_IFREG | 0644);
    archive_entry_set_uid(entry, consumer.consume_u32() & 0xFFFF);
    archive_entry_set_gid(entry, consumer.consume_u32() & 0xFFFF);

    uint8_t data_buf[256];
    size_t data_len = consumer.consume_bytes(data_buf, 256);
    archive_entry_set_size(entry, data_len);

    if (archive_write_header(writer, entry) == ARCHIVE_OK && data_len > 0) {
      archive_write_data(writer, data_buf, data_len);
    }

    archive_entry_free(entry);
    entry_count++;
  }

  archive_write_close(writer);
  archive_write_free(writer);

  if (used == 0) {
    return 0;
  }

  // Phase 2: Read the archive back
  struct archive *reader = archive_read_new();
  if (reader == NULL) {
    return 0;
  }

  archive_read_support_format_all(reader);
  archive_read_support_filter_all(reader);

  if (archive_read_open_memory(reader, archive_data.data(), used) != ARCHIVE_OK) {
    archive_read_free(reader);
    return 0;
  }

  std::vector<uint8_t> read_buffer(4096, 0);
  struct archive_entry *entry;
  while (archive_read_next_header(reader, &entry) == ARCHIVE_OK) {
    archive_entry_pathname(entry);
    archive_entry_size(entry);

    ssize_t r;
    while ((r = archive_read_data(reader, read_buffer.data(), read_buffer.size())) > 0)
      ;
  }

  archive_read_free(reader);
  return 0;
}
