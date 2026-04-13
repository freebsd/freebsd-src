/*-
 * Copyright (c) 2026 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"

DEFINE_TEST(test_read_format_rar5_loop_bug)
{
  const char *reffile = "test_read_format_rar5_loop_bug.rar";
  struct archive_entry *ae;
  struct archive *a;
  const void *buf;
  size_t size;
  la_int64_t offset;

  extract_reference_file(reffile);
  assert((a = archive_read_new()) != NULL);
  assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
  assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
  assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, reffile, 10240));

  // This has just one entry
  assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

  // Read blocks until the end of the entry
  while (ARCHIVE_OK == archive_read_data_block(a, &buf, &size, &offset)) {
  }

  assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

  assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
  assertEqualInt(ARCHIVE_OK, archive_free(a));
}
