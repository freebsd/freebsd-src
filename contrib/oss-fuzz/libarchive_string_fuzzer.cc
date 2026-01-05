/*
 * Archive string/encoding conversion fuzzer for libarchive
 * Tests character encoding conversions which are often vulnerability sources
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

#include "archive.h"
#include "archive_entry.h"
#include "fuzz_helpers.h"

static constexpr size_t kMaxInputSize = 32 * 1024;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
  if (len == 0 || len > kMaxInputSize) {
    return 0;
  }

  struct archive_entry *entry = archive_entry_new();
  if (entry == NULL) {
    return 0;
  }

  // Reserve some bytes for control
  if (len < 4) {
    archive_entry_free(entry);
    return 0;
  }

  uint8_t test_type = buf[0];
  const char *str = reinterpret_cast<const char*>(buf + 1);
  size_t str_len = len - 1;

  // Ensure null termination for string operations
  char *safe_str = static_cast<char*>(malloc(str_len + 1));
  if (safe_str == NULL) {
    archive_entry_free(entry);
    return 0;
  }
  memcpy(safe_str, str, str_len);
  safe_str[str_len] = '\0';

  // Test various string functions based on type
  switch (test_type % 10) {
    case 0:
      // Pathname conversions
      archive_entry_set_pathname(entry, safe_str);
      archive_entry_pathname(entry);
      archive_entry_pathname_w(entry);
      archive_entry_pathname_utf8(entry);
      break;

    case 1:
      // Symlink conversions
      archive_entry_set_symlink(entry, safe_str);
      archive_entry_symlink(entry);
      archive_entry_symlink_w(entry);
      archive_entry_symlink_utf8(entry);
      break;

    case 2:
      // Hardlink conversions
      archive_entry_set_hardlink(entry, safe_str);
      archive_entry_hardlink(entry);
      archive_entry_hardlink_w(entry);
      archive_entry_hardlink_utf8(entry);
      break;

    case 3:
      // Username conversions
      archive_entry_set_uname(entry, safe_str);
      archive_entry_uname(entry);
      archive_entry_uname_w(entry);
      archive_entry_uname_utf8(entry);
      break;

    case 4:
      // Group name conversions
      archive_entry_set_gname(entry, safe_str);
      archive_entry_gname(entry);
      archive_entry_gname_w(entry);
      archive_entry_gname_utf8(entry);
      break;

    case 5:
      // Copy functions
      archive_entry_copy_pathname(entry, safe_str);
      archive_entry_copy_symlink(entry, safe_str);
      archive_entry_copy_hardlink(entry, safe_str);
      break;

    case 6:
      // UTF-8 specific
      archive_entry_update_pathname_utf8(entry, safe_str);
      archive_entry_update_symlink_utf8(entry, safe_str);
      archive_entry_update_hardlink_utf8(entry, safe_str);
      break;

    case 7:
      // Fflags text
      archive_entry_copy_fflags_text(entry, safe_str);
      archive_entry_fflags_text(entry);
      break;

    case 8:
      // ACL text parsing
      archive_entry_acl_from_text(entry, safe_str, ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
      archive_entry_acl_from_text(entry, safe_str, ARCHIVE_ENTRY_ACL_TYPE_DEFAULT);
      archive_entry_acl_from_text(entry, safe_str, ARCHIVE_ENTRY_ACL_TYPE_NFS4);
      break;

    case 9: {
      // Wide character operations
      size_t wlen = str_len;
      wchar_t *wstr = static_cast<wchar_t*>(malloc((wlen + 1) * sizeof(wchar_t)));
      if (wstr) {
        mbstowcs(wstr, safe_str, wlen);
        wstr[wlen] = L'\0';

        archive_entry_copy_pathname_w(entry, wstr);
        archive_entry_pathname_w(entry);

        archive_entry_copy_symlink_w(entry, wstr);
        archive_entry_symlink_w(entry);

        free(wstr);
      }
      break;
    }
  }

  // Clone and compare
  struct archive_entry *entry2 = archive_entry_clone(entry);
  if (entry2) {
    archive_entry_free(entry2);
  }

  free(safe_str);
  archive_entry_free(entry);
  return 0;
}
