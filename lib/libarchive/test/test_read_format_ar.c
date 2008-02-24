/*-
 * Copyright (c) 2007 Kai Wang
 * Copyright (c) 2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
__FBSDID("$FreeBSD: src/lib/libarchive/test/test_read_format_ar.c,v 1.4 2007/07/06 15:43:11 kientzle Exp $");

#if ARCHIVE_VERSION_STAMP >= 1009000
/*
 * This "archive" is created by "GNU ar". Here we try to verify
 * our GNU format handling functionality.
 */
static unsigned char archive[] = {
'!','<','a','r','c','h','>',10,'/','/',' ',' ',' ',' ',' ',' ',' ',
' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
' ',' ',' ',' ',' ','4','0',' ',' ',' ',' ',' ',' ',' ',' ','`',10,
'y','y','y','t','t','t','s','s','s','a','a','a','f','f','f','.','o',
'/',10,'h','h','h','h','j','j','j','j','k','k','k','k','l','l','l',
'l','.','o','/',10,10,'/','0',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
' ',' ',' ',' ','1','1','7','5','4','6','5','6','5','2',' ',' ','1',
'0','0','1',' ',' ','0',' ',' ',' ',' ',' ','1','0','0','6','4','4',
' ',' ','8',' ',' ',' ',' ',' ',' ',' ',' ',' ','`',10,'5','5','6',
'6','7','7','8','8','g','g','h','h','.','o','/',' ',' ',' ',' ',' ',
' ',' ',' ',' ','1','1','7','5','4','6','5','6','6','8',' ',' ','1',
'0','0','1',' ',' ','0',' ',' ',' ',' ',' ','1','0','0','6','4','4',
' ',' ','4',' ',' ',' ',' ',' ',' ',' ',' ',' ','`',10,'3','3','3',
'3','/','1','9',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
'1','1','7','5','4','6','5','7','1','3',' ',' ','1','0','0','1',' ',
' ','0',' ',' ',' ',' ',' ','1','0','0','6','4','4',' ',' ','9',' ',
' ',' ',' ',' ',' ',' ',' ',' ','`',10,'9','8','7','6','5','4','3',
'2','1',10};

char buff[64];
#endif

DEFINE_TEST(test_read_format_ar)
{
#if ARCHIVE_VERSION_STAMP < 1009000
	skipping("test_read_support_format_ar()");
#else
	struct archive_entry *ae;
	struct archive *a;
	assert((a = archive_read_new()) != NULL);
	assertA(0 == archive_read_support_compression_all(a));
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_open_memory(a, archive, sizeof(archive)));

	/* Filename table.  */
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("//", archive_entry_pathname(ae));
	assertEqualInt(0, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_uid(ae));
	assertEqualInt(0, archive_entry_gid(ae));
	assertEqualInt(40, archive_entry_size(ae));
	assertEqualIntA(a, 40, archive_read_data(a, buff, 50));
	assert(0 == memcmp(buff, "yyytttsssaaafff.o/\nhhhhjjjjkkkkllll.o/\n\n", 40));

	/* First Entry */
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("yyytttsssaaafff.o", archive_entry_pathname(ae));
	assertEqualInt(1175465652, archive_entry_mtime(ae));
	assertEqualInt(1001, archive_entry_uid(ae));
	assertEqualInt(0, archive_entry_gid(ae));
	assert(8 == archive_entry_size(ae));
	assertA(8 == archive_read_data(a, buff, 10));
	assert(0 == memcmp(buff, "55667788", 8));

	/* Second Entry */
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("gghh.o", archive_entry_pathname(ae));
	assertEqualInt(1175465668, archive_entry_mtime(ae));
	assertEqualInt(1001, archive_entry_uid(ae));
	assertEqualInt(0, archive_entry_gid(ae));
	assert(4 == archive_entry_size(ae));
	assertA(4 == archive_read_data(a, buff, 10));
	assert(0 == memcmp(buff, "3333", 4));

	/* Third Entry */
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("hhhhjjjjkkkkllll.o", archive_entry_pathname(ae));
	assertEqualInt(1175465713, archive_entry_mtime(ae));
	assertEqualInt(1001, archive_entry_uid(ae));
	assertEqualInt(0, archive_entry_gid(ae));
	assert(9 == archive_entry_size(ae));
	assertA(9 == archive_read_data(a, buff, 9));
	assert(0 == memcmp(buff, "987654321", 9));

	/* Test EOF */
	assertA(1 == archive_read_next_header(a, &ae));
	assert(0 == archive_read_close(a));
#if ARCHIVE_API_VERSION > 1
	assert(0 == archive_read_finish(a));
#else
	archive_read_finish(a);
#endif
#endif
}
