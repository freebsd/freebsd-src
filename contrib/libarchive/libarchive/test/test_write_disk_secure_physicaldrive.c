/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 EndlssNightmare
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

/*
 * Verify that \\.\PhysicalDriveX paths are correctly rejected
 * by archive_write_disk on Windows.
 *
 * Before the fix, the PhysicalDrive validation contained a copy-paste
 * bug where array indices p[10] through p[16] were all written as p[9].
 * Since the conditions are ANDed and p[9] cannot simultaneously equal
 * 7 different characters, the check could never match — making the
 * PhysicalDrive blocking code dead code.
 */
DEFINE_TEST(test_write_disk_secure_physicaldrive)
{
#if !defined(_WIN32) || defined(__CYGWIN__)
	skipping("Windows-specific PhysicalDrive path test");
#else
	struct archive *a;
	struct archive_entry *ae;

	/* Create an archive_write_disk object. */
	assert((a = archive_write_disk_new()) != NULL);

	/*
	 * Test 1: \\.\PhysicalDrive0 should be rejected.
	 * Note: we intentionally do NOT set ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS
	 * because we are testing the device-path-specific check, not the
	 * broader absolute path check.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname_w(ae, L"\\\\.\\PhysicalDrive0");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_entry_set_size(ae, 0);
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);

	/*
	 * Test 2: \\.\PhysicalDrive9 should also be rejected.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname_w(ae, L"\\\\.\\PhysicalDrive9");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_entry_set_size(ae, 0);
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);

	/*
	 * Test 3: Case-insensitive — \\.\physicaldrive0 should be rejected.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname_w(ae, L"\\\\.\\physicaldrive0");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_entry_set_size(ae, 0);
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);

	/*
	 * Test 4: Mixed case — \\.\PHYSICALDRIVE5 should be rejected.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname_w(ae, L"\\\\.\\PHYSICALDRIVE5");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_entry_set_size(ae, 0);
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);

	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
#endif
}
