/*-SPDX-License-Identifier: BSD-2-Clause
 * Copyright (C) 2024 by наб <nabijaczleweli@nabijaczleweli.xyz>
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

static const char data[] = "!<arch>\narchivemount.1/ 0           0     0     644     0         `\n";


DEFINE_TEST(test_ar_mode)
{
	struct archive * ar = archive_read_new();
	assertEqualInt(archive_read_support_format_all(ar), ARCHIVE_OK);
	assertEqualInt(archive_read_open_memory(ar, data, sizeof(data) - 1), ARCHIVE_OK);

	struct archive_entry * entry;
	assertEqualIntA(ar, archive_read_next_header(ar, &entry), ARCHIVE_OK);
	assertEqualIntA(ar, archive_entry_mode(entry), S_IFREG | 0644);

	archive_read_free(ar);
}
