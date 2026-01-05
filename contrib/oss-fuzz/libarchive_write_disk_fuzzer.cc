/*
 * Archive write disk fuzzer for libarchive
 * Tests extraction to filesystem
 * Security-critical: path traversal, permission handling, symlink attacks
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

static constexpr size_t kMaxInputSize = 64 * 1024;

static char g_temp_dir[256] = {0};

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
  (void)argc;
  (void)argv;
  // Create a temporary directory for extraction
  snprintf(g_temp_dir, sizeof(g_temp_dir), "/tmp/fuzz_extract_XXXXXX");
  if (mkdtemp(g_temp_dir) == NULL) {
    g_temp_dir[0] = '\0';
  }
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
  if (len == 0 || len > kMaxInputSize) {
    return 0;
  }

  if (g_temp_dir[0] == '\0') {
    return 0;
  }

  DataConsumer consumer(buf, len);

  struct archive *disk = archive_write_disk_new();
  if (disk == NULL) {
    return 0;
  }

  // Configure write disk options
  uint8_t opt_flags = consumer.consume_byte();
  int flags = 0;
  if (opt_flags & 0x01) flags |= ARCHIVE_EXTRACT_TIME;
  if (opt_flags & 0x02) flags |= ARCHIVE_EXTRACT_PERM;
  if (opt_flags & 0x04) flags |= ARCHIVE_EXTRACT_ACL;
  if (opt_flags & 0x08) flags |= ARCHIVE_EXTRACT_FFLAGS;
  if (opt_flags & 0x10) flags |= ARCHIVE_EXTRACT_OWNER;
  if (opt_flags & 0x20) flags |= ARCHIVE_EXTRACT_XATTR;
  if (opt_flags & 0x40) flags |= ARCHIVE_EXTRACT_SECURE_SYMLINKS;
  if (opt_flags & 0x80) flags |= ARCHIVE_EXTRACT_SECURE_NODOTDOT;

  archive_write_disk_set_options(disk, flags);
  archive_write_disk_set_standard_lookup(disk);

  // Create entries to extract
  int entry_count = 0;
  while (!consumer.empty() && entry_count < 5 && consumer.remaining() > 20) {
    struct archive_entry *entry = archive_entry_new();
    if (entry == NULL) break;

    // Build a safe path within our temp directory
    char safe_path[512];
    const char *name = consumer.consume_string(32);
    snprintf(safe_path, sizeof(safe_path), "%s/%s", g_temp_dir, name);

    // Sanitize path to prevent traversal
    char *p = safe_path;
    while (*p) {
      if (p[0] == '.' && p[1] == '.') {
        p[0] = '_';
        p[1] = '_';
      }
      p++;
    }

    archive_entry_set_pathname(entry, safe_path);

    uint8_t ftype = consumer.consume_byte() % 3;
    mode_t mode;
    switch (ftype) {
      case 0: mode = S_IFREG | 0644; break;
      case 1: mode = S_IFDIR | 0755; break;
      default: mode = S_IFREG | 0644; break;
    }
    archive_entry_set_mode(entry, mode);

    archive_entry_set_uid(entry, 1000);
    archive_entry_set_gid(entry, 1000);
    archive_entry_set_mtime(entry, consumer.consume_i64(), 0);

    // Write the entry header
    if (archive_write_header(disk, entry) == ARCHIVE_OK) {
      if (S_ISREG(mode)) {
        uint8_t data_buf[256];
        size_t data_len = consumer.consume_bytes(data_buf, 256);
        archive_entry_set_size(entry, data_len);
        if (data_len > 0) {
          archive_write_data(disk, data_buf, data_len);
        }
      }
      archive_write_finish_entry(disk);
    }

    archive_entry_free(entry);
    entry_count++;
  }

  archive_write_close(disk);
  archive_write_free(disk);

  // Clean up extracted files using nftw (safer than system())
  remove_directory_tree(g_temp_dir);
  // Recreate the temp directory for next iteration
  mkdir(g_temp_dir, 0700);

  return 0;
}
