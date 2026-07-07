/*
 * Archive write disk fuzzer for libarchive
 * Tests extraction to filesystem
 * Security-critical: path traversal, permission handling, symlink attacks
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "archive.h"
#include "archive_entry.h"
#include "fuzz_helpers.h"

static constexpr size_t kMaxInputSize = 64 * 1024;

static char g_temp_dir[256] = {0};
static char g_start_dir[512] = {0};
static char g_extract_dir[512] = {0};
static char g_outside_dir[512] = {0};
static char g_sentinel_path[512] = {0};

static int write_file(const char* path, const char* data) {
  FILE* f = fopen(path, "wb");
  size_t len = strlen(data);
  size_t written;
  if (f == NULL) {
    return -1;
  }
  written = fwrite(data, 1, len, f);
  if (fclose(f) != 0) {
    return -1;
  }
  return written == len ? 0 : -1;
}

static int sentinel_is_unchanged() {
  char buf[32];
  FILE* f = fopen(g_sentinel_path, "rb");
  if (f == NULL) {
    return 0;
  }
  size_t n = fread(buf, 1, sizeof(buf) - 1, f);
  fclose(f);
  buf[n] = '\0';
  return strcmp(buf, "outside-original") == 0;
}

static int reset_temp_tree() {
  if (g_start_dir[0] != '\0') {
    if (chdir(g_start_dir) != 0) {
      return -1;
    }
  }
  remove_directory_tree(g_temp_dir);
  if (mkdir(g_temp_dir, 0700) != 0) {
    return -1;
  }

  snprintf(g_extract_dir, sizeof(g_extract_dir), "%s/extract", g_temp_dir);
  snprintf(g_outside_dir, sizeof(g_outside_dir), "%s/outside", g_temp_dir);
  snprintf(g_sentinel_path, sizeof(g_sentinel_path), "%s/outside/sentinel",
           g_temp_dir);

  if (mkdir(g_extract_dir, 0700) != 0 || mkdir(g_outside_dir, 0700) != 0) {
    return -1;
  }

  char existing[512];
  char existing_dir[512];
  char nested_existing[512];
  snprintf(existing, sizeof(existing), "%s/extract/existing", g_temp_dir);
  snprintf(existing_dir, sizeof(existing_dir), "%s/extract/dir", g_temp_dir);
  snprintf(nested_existing, sizeof(nested_existing),
           "%s/extract/dir/existing", g_temp_dir);
  if (mkdir(existing_dir, 0700) != 0) {
    return -1;
  }

  return write_file(g_sentinel_path, "outside-original") ||
         write_file(existing, "inside") ||
         write_file(nested_existing, "inside");
}

static void consume_path(DataConsumer* consumer, char* out, size_t out_size) {
  static const char* const kPaths[] = {
      "file",
      "dir/file",
      "dir/./file",
      "dir//file",
      "link",
      "link/file",
      "link/./file",
      "link//file",
      "../outside/sentinel",
      "../outside/new-file",
      "./../outside/sentinel",
      "dir/../file",
      "hardlink",
      "hardlink-with-data",
      "symlink",
      "existing",
      "dir/existing",
  };
  uint8_t selector = consumer->consume_byte();
  if ((selector & 0x80) != 0) {
    snprintf(out, out_size, "%s/extract/absolute-%u", g_temp_dir, selector);
  } else if ((selector & 0x40) != 0) {
    snprintf(out, out_size, "%s/outside/sentinel", g_temp_dir);
  } else {
    snprintf(out, out_size, "%s",
             kPaths[selector % (sizeof(kPaths) / sizeof(kPaths[0]))]);
  }
}

static void consume_linkname(DataConsumer* consumer, char* out,
                             size_t out_size) {
  static const char* const kTargets[] = {
      "existing",
      "dir/existing",
      "link/sentinel",
      "link/./sentinel",
      "link//sentinel",
      "../outside/sentinel",
      "./../outside/sentinel",
      "dir/../existing",
      "missing-target",
  };
  uint8_t selector = consumer->consume_byte();
  if ((selector & 0x80) != 0) {
    snprintf(out, out_size, "%s/outside/sentinel", g_temp_dir);
  } else if ((selector & 0x40) != 0) {
    snprintf(out, out_size, "%s/extract/existing", g_temp_dir);
  } else {
    snprintf(out, out_size, "%s",
             kTargets[selector % (sizeof(kTargets) / sizeof(kTargets[0]))]);
  }
}

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
  (void)argc;
  (void)argv;
  if (getcwd(g_start_dir, sizeof(g_start_dir)) == NULL) {
    g_start_dir[0] = '\0';
  }
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

  if (reset_temp_tree() != 0 || chdir(g_extract_dir) != 0) {
    return 0;
  }

  DataConsumer consumer(buf, len);

  struct archive *disk = archive_write_disk_new();
  if (disk == NULL) {
    if (g_start_dir[0] != '\0') {
      if (chdir(g_start_dir) != 0) {
        return 0;
      }
    }
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
  if (opt_flags & 0x40) flags |= ARCHIVE_EXTRACT_UNLINK;
  if (opt_flags & 0x80) flags |= ARCHIVE_EXTRACT_SAFE_WRITES;

  flags |= ARCHIVE_EXTRACT_SECURE_SYMLINKS |
           ARCHIVE_EXTRACT_SECURE_NODOTDOT |
           ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS;

  archive_write_disk_set_options(disk, flags);
  archive_write_disk_set_standard_lookup(disk);

  // Create entries to extract
  int entry_count = 0;
  while (!consumer.empty() && entry_count < 5 && consumer.remaining() > 20) {
    struct archive_entry *entry = archive_entry_new();
    if (entry == NULL) break;

    char pathname[512];
    char linkname[512];
    consume_path(&consumer, pathname, sizeof(pathname));
    archive_entry_set_pathname(entry, pathname);

    uint8_t ftype = consumer.consume_byte() % 4;
    mode_t mode;
    switch (ftype) {
      case 0: mode = S_IFREG | 0644; break;
      case 1: mode = S_IFDIR | 0755; break;
      case 2: mode = S_IFLNK | 0777; break;
      default: mode = S_IFREG | 0644; break;
    }
    archive_entry_set_mode(entry, mode);

    if (S_ISLNK(mode)) {
      consume_linkname(&consumer, linkname, sizeof(linkname));
      archive_entry_set_symlink(entry, linkname);
      archive_entry_set_size(entry, 0);
    } else if (ftype == 3) {
      consume_linkname(&consumer, linkname, sizeof(linkname));
      archive_entry_set_hardlink(entry, linkname);
    }

    archive_entry_set_uid(entry, 1000);
    archive_entry_set_gid(entry, 1000);
    archive_entry_set_mtime(entry, consumer.consume_i64(), 0);

    uint8_t data_buf[256];
    size_t data_len = 0;
    if (S_ISREG(mode)) {
      data_len = consumer.consume_bytes(data_buf, 256);
      archive_entry_set_size(entry, data_len);
    }

    // Write the entry header
    if (archive_write_header(disk, entry) == ARCHIVE_OK) {
      if (S_ISREG(mode) && data_len > 0) {
        archive_write_data(disk, data_buf, data_len);
      }
      archive_write_finish_entry(disk);
    }

    archive_entry_free(entry);
    entry_count++;
  }

  archive_write_close(disk);
  archive_write_free(disk);

  if (!sentinel_is_unchanged()) {
    abort();
  }

  if (g_start_dir[0] != '\0') {
    if (chdir(g_start_dir) != 0) {
      abort();
    }
  }

  // Clean up extracted files using nftw (safer than system())
  remove_directory_tree(g_temp_dir);
  // Recreate the temp directory for next iteration
  mkdir(g_temp_dir, 0700);

  return 0;
}
