/*
 * Archive match fuzzer for libarchive
 * Tests pattern matching, time matching, and owner matching
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "archive.h"
#include "archive_entry.h"
#include "fuzz_helpers.h"

static constexpr size_t kMaxInputSize = 32 * 1024;


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
  if (len == 0 || len > kMaxInputSize) {
    return 0;
  }

  DataConsumer consumer(buf, len);

  struct archive *match = archive_match_new();
  if (match == NULL) {
    return 0;
  }

  // Add various match patterns
  while (!consumer.empty() && consumer.remaining() > 5) {
    uint8_t match_type = consumer.consume_byte() % 6;

    switch (match_type) {
      case 0: {
        // Pattern exclusion
        const char *pattern = consumer.consume_string(64);
        archive_match_exclude_pattern(match, pattern);
        break;
      }
      case 1: {
        // Pattern inclusion
        const char *pattern = consumer.consume_string(64);
        archive_match_include_pattern(match, pattern);
        break;
      }
      case 2: {
        // Time comparison (newer than)
        int64_t sec = consumer.consume_i64();
        int64_t nsec = consumer.consume_i64() % 1000000000;
        archive_match_include_time(match, ARCHIVE_MATCH_MTIME | ARCHIVE_MATCH_NEWER,
                                   sec, nsec);
        break;
      }
      case 3: {
        // Time comparison (older than)
        int64_t sec = consumer.consume_i64();
        int64_t nsec = consumer.consume_i64() % 1000000000;
        archive_match_include_time(match, ARCHIVE_MATCH_MTIME | ARCHIVE_MATCH_OLDER,
                                   sec, nsec);
        break;
      }
      case 4: {
        // UID inclusion
        int64_t uid = consumer.consume_i64() & 0xFFFF;
        archive_match_include_uid(match, uid);
        break;
      }
      case 5: {
        // GID inclusion
        int64_t gid = consumer.consume_i64() & 0xFFFF;
        archive_match_include_gid(match, gid);
        break;
      }
    }
  }

  // Create a test entry and check if it matches
  struct archive_entry *entry = archive_entry_new();
  if (entry) {
    archive_entry_set_pathname(entry, "test/file.txt");
    archive_entry_set_mtime(entry, 1234567890, 0);
    archive_entry_set_uid(entry, 1000);
    archive_entry_set_gid(entry, 1000);
    archive_entry_set_mode(entry, 0644 | 0100000);  // Regular file

    // Test matching
    archive_match_path_excluded(match, entry);
    archive_match_time_excluded(match, entry);
    archive_match_owner_excluded(match, entry);
    archive_match_excluded(match, entry);

    archive_entry_free(entry);
  }

  archive_match_free(match);
  return 0;
}
