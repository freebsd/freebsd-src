/*
 * Encrypted archive fuzzer for libarchive
 * Tests password/passphrase handling across formats
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <vector>

#include "archive.h"
#include "archive_entry.h"
#include "fuzz_helpers.h"

static constexpr size_t kMaxInputSize = 512 * 1024;

// Passphrase callback for testing
static const char *test_passphrases[] = {
  "password",
  "test",
  "123456",
  "",
  "secret",
  NULL
};

static int passphrase_idx = 0;

static const char* passphrase_callback(struct archive *a, void *client_data) {
  (void)a;
  (void)client_data;
  const char *pass = test_passphrases[passphrase_idx];
  if (pass != NULL) {
    passphrase_idx++;
  }
  return pass;
}



extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
  if (len == 0 || len > kMaxInputSize) {
    return 0;
  }

  // Reset passphrase index
  passphrase_idx = 0;

  struct archive *a = archive_read_new();
  if (a == NULL) {
    return 0;
  }

  // Enable all formats that support encryption
  archive_read_support_format_zip(a);
  archive_read_support_format_7zip(a);
  archive_read_support_format_rar(a);
  archive_read_support_format_rar5(a);
  archive_read_support_filter_all(a);

  // Set up passphrase callback
  archive_read_set_passphrase_callback(a, NULL, passphrase_callback);

  // Also add some static passphrases
  archive_read_add_passphrase(a, "password");
  archive_read_add_passphrase(a, "test123");

  Buffer buffer = {buf, len, 0};
  if (archive_read_open(a, &buffer, NULL, reader_callback, NULL) != ARCHIVE_OK) {
    archive_read_free(a);
    return 0;
  }

  std::vector<uint8_t> data_buffer(4096, 0);
  struct archive_entry *entry;
  int entry_count = 0;

  while (archive_read_next_header(a, &entry) == ARCHIVE_OK && entry_count < 100) {
    archive_entry_pathname(entry);

    // Check encryption status
    int is_encrypted = archive_entry_is_encrypted(entry);
    int is_data_encrypted = archive_entry_is_data_encrypted(entry);
    int is_meta_encrypted = archive_entry_is_metadata_encrypted(entry);
    (void)is_encrypted;
    (void)is_data_encrypted;
    (void)is_meta_encrypted;

    // Check if archive has encrypted entries
    archive_read_has_encrypted_entries(a);

    // Try to read data (may fail due to wrong password)
    ssize_t r;
    while ((r = archive_read_data(a, data_buffer.data(), data_buffer.size())) > 0)
      ;

    entry_count++;
  }

  archive_read_free(a);
  return 0;
}
