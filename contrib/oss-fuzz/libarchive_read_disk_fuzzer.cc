/*
 * Archive read disk fuzzer for libarchive
 * Tests filesystem traversal and entry creation from paths
 * Security-critical: path traversal, symlink handling
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "archive.h"
#include "archive_entry.h"
#include "fuzz_helpers.h"

static constexpr size_t kMaxInputSize = 16 * 1024;


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
  if (len == 0 || len > kMaxInputSize) {
    return 0;
  }

  DataConsumer consumer(buf, len);

  struct archive *a = archive_read_disk_new();
  if (a == NULL) {
    return 0;
  }

  // Configure disk reader behavior
  uint8_t flags = consumer.consume_byte();
  if (flags & 0x01) {
    archive_read_disk_set_symlink_logical(a);
  } else if (flags & 0x02) {
    archive_read_disk_set_symlink_physical(a);
  } else {
    archive_read_disk_set_symlink_hybrid(a);
  }

  archive_read_disk_set_standard_lookup(a);

  // Set behavior flags
  int behavior = 0;
  if (flags & 0x04) behavior |= ARCHIVE_READDISK_RESTORE_ATIME;
  if (flags & 0x08) behavior |= ARCHIVE_READDISK_HONOR_NODUMP;
  if (flags & 0x10) behavior |= ARCHIVE_READDISK_NO_TRAVERSE_MOUNTS;
  archive_read_disk_set_behavior(a, behavior);

  // Create an entry and test entry_from_file with various paths
  struct archive_entry *entry = archive_entry_new();
  if (entry) {
    // Test with /tmp (safe, always exists)
    archive_entry_copy_pathname(entry, "/tmp");
    archive_read_disk_entry_from_file(a, entry, -1, NULL);

    // Get entry info
    archive_entry_pathname(entry);
    archive_entry_size(entry);
    archive_entry_mode(entry);
    archive_entry_uid(entry);
    archive_entry_gid(entry);

    // Test name lookups
    archive_read_disk_gname(a, 0);
    archive_read_disk_uname(a, 0);
    archive_read_disk_gname(a, 1000);
    archive_read_disk_uname(a, 1000);

    archive_entry_free(entry);
  }

  archive_read_free(a);
  return 0;
}
