/*
 * Archive entry fuzzer for libarchive
 * Targets archive_entry_* functions including ACL, linkify, and metadata
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "archive.h"
#include "archive_entry.h"
#include "fuzz_helpers.h"

static constexpr size_t kMaxInputSize = 64 * 1024;  // 64KB

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
  if (len == 0 || len > kMaxInputSize) {
    return 0;
  }

  DataConsumer consumer(buf, len);

  struct archive_entry *entry = archive_entry_new();
  if (entry == NULL) {
    return 0;
  }

  // Set basic entry properties
  archive_entry_set_pathname(entry, consumer.consume_string(256));
  archive_entry_set_size(entry, consumer.consume_i64());
  archive_entry_set_mode(entry, consumer.consume_u32());
  archive_entry_set_uid(entry, consumer.consume_u32());
  archive_entry_set_gid(entry, consumer.consume_u32());
  archive_entry_set_mtime(entry, consumer.consume_i64(), 0);
  archive_entry_set_atime(entry, consumer.consume_i64(), 0);
  archive_entry_set_ctime(entry, consumer.consume_i64(), 0);
  archive_entry_set_birthtime(entry, consumer.consume_i64(), 0);

  // Set various string fields
  archive_entry_set_uname(entry, consumer.consume_string(64));
  archive_entry_set_gname(entry, consumer.consume_string(64));
  archive_entry_set_symlink(entry, consumer.consume_string(256));
  archive_entry_set_hardlink(entry, consumer.consume_string(256));

  // Exercise ACL functions (low coverage targets)
  int acl_type = consumer.consume_byte() & 0x0F;
  int acl_permset = consumer.consume_u32();
  int acl_tag = consumer.consume_byte() & 0x0F;
  int acl_qual = consumer.consume_u32();
  const char *acl_name = consumer.consume_string(64);

  archive_entry_acl_add_entry(entry, acl_type, acl_permset, acl_tag, acl_qual, acl_name);

  // Add more ACL entries based on remaining data
  while (!consumer.empty() && consumer.remaining() > 10) {
    acl_type = consumer.consume_byte() & 0x0F;
    acl_permset = consumer.consume_u32();
    acl_tag = consumer.consume_byte() & 0x0F;
    acl_qual = consumer.consume_u32();
    acl_name = consumer.consume_string(32);
    archive_entry_acl_add_entry(entry, acl_type, acl_permset, acl_tag, acl_qual, acl_name);
  }

  // Exercise ACL text conversion functions (archive_acl_to_text_* are uncovered)
  ssize_t text_len;
  char *acl_text = archive_entry_acl_to_text(entry, &text_len, ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
  if (acl_text) {
    // Parse the text back
    archive_entry_acl_from_text(entry, acl_text, ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
    free(acl_text);
  }

  acl_text = archive_entry_acl_to_text(entry, &text_len, ARCHIVE_ENTRY_ACL_TYPE_DEFAULT);
  if (acl_text) {
    free(acl_text);
  }

  acl_text = archive_entry_acl_to_text(entry, &text_len, ARCHIVE_ENTRY_ACL_TYPE_NFS4);
  if (acl_text) {
    free(acl_text);
  }

  // Exercise wide character versions
  wchar_t *acl_text_w = archive_entry_acl_to_text_w(entry, &text_len, ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
  if (acl_text_w) {
    free(acl_text_w);
  }

  // Get pathname variants
  archive_entry_pathname(entry);
  archive_entry_pathname_w(entry);
  archive_entry_pathname_utf8(entry);

  // Clone the entry
  struct archive_entry *entry2 = archive_entry_clone(entry);
  if (entry2) {
    archive_entry_free(entry2);
  }

  // Clear and reuse
  archive_entry_clear(entry);

  archive_entry_free(entry);
  return 0;
}
