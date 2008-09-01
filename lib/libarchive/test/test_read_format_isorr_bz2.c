/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
__FBSDID("$FreeBSD$");

/*
Execute the following to rebuild the data for this program:
   tail -n +32 test_read_format_isorr_bz2.c | /bin/sh

rm -rf /tmp/iso
mkdir /tmp/iso
mkdir /tmp/iso/dir
echo "hello" >/tmp/iso/file
ln /tmp/iso/file /tmp/iso/hardlink
(cd /tmp/iso; ln -s file symlink)
TZ=utc touch -afhm -t 197001020000.01 /tmp/iso /tmp/iso/file /tmp/iso/dir
TZ=utc touch -afhm -t 197001030000.02 /tmp/iso/symlink
mkhybrid -R -uid 1 -gid 2 /tmp/iso | bzip2 > test_read_format_isorr_bz2.iso.bz2
F=test_read_format_isorr_bz2.iso.bz2
uuencode $F $F > $F.uu
exit 1
 */

DEFINE_TEST(test_read_format_isorr_bz2)
{
	const char *refname = "test_read_format_isorr_bz2.iso.bz2";
	struct archive_entry *ae;
	struct archive *a;
	const void *p;
	size_t size;
	off_t offset;

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualInt(0, archive_read_support_compression_all(a));
	assertEqualInt(0, archive_read_support_format_all(a));
	assertEqualInt(0, archive_read_open_filename(a, refname, 10240));

	/* First entry is '.' root directory. */
	assertEqualInt(0, archive_read_next_header(a, &ae));
	assertEqualString(".", archive_entry_pathname(ae));
	assert(S_ISDIR(archive_entry_stat(ae)->st_mode));
	assertEqualInt(2048, archive_entry_size(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_mtime_nsec(ae));
	assertEqualInt(86401, archive_entry_ctime(ae));
	assertEqualInt(0, archive_entry_stat(ae)->st_nlink);
	assertEqualInt(0, archive_entry_uid(ae));
	assertEqualIntA(a, ARCHIVE_EOF,
	    archive_read_data_block(a, &p, &size, &offset));
	assertEqualInt(size, 0);

	/* A directory. */
	assertEqualInt(0, archive_read_next_header(a, &ae));
	assertEqualString("dir", archive_entry_pathname(ae));
	assert(S_ISDIR(archive_entry_stat(ae)->st_mode));
	assertEqualInt(2048, archive_entry_size(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(86401, archive_entry_atime(ae));
	assertEqualInt(2, archive_entry_stat(ae)->st_nlink);
	assertEqualInt(1, archive_entry_uid(ae));
	assertEqualInt(2, archive_entry_gid(ae));

	/* A regular file. */
	assertEqualInt(0, archive_read_next_header(a, &ae));
	assertEqualString("file", archive_entry_pathname(ae));
	assert(S_ISREG(archive_entry_stat(ae)->st_mode));
	assertEqualInt(6, archive_entry_size(ae));
	assertEqualInt(0, archive_read_data_block(a, &p, &size, &offset));
	assertEqualInt(6, size);
	assertEqualInt(0, offset);
	assertEqualInt(0, memcmp(p, "hello\n", 6));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(86401, archive_entry_atime(ae));
	assertEqualInt(2, archive_entry_stat(ae)->st_nlink);
	assertEqualInt(1, archive_entry_uid(ae));
	assertEqualInt(2, archive_entry_gid(ae));

	/* A hardlink to the regular file. */
	assertEqualInt(0, archive_read_next_header(a, &ae));
	assertEqualString("hardlink", archive_entry_pathname(ae));
	assert(S_ISREG(archive_entry_stat(ae)->st_mode));
	assertEqualString("file", archive_entry_hardlink(ae));
	assertEqualInt(6, archive_entry_size(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(86401, archive_entry_atime(ae));
	assertEqualInt(2, archive_entry_stat(ae)->st_nlink);
	assertEqualInt(1, archive_entry_uid(ae));
	assertEqualInt(2, archive_entry_gid(ae));

	/* A symlink to the regular file. */
	assertEqualInt(0, archive_read_next_header(a, &ae));
	assertEqualString("symlink", archive_entry_pathname(ae));
	assert(S_ISLNK(archive_entry_stat(ae)->st_mode));
	assertEqualString("file", archive_entry_symlink(ae));
	assertEqualInt(0, archive_entry_size(ae));
	assertEqualInt(172802, archive_entry_mtime(ae));
	assertEqualInt(172802, archive_entry_atime(ae));
	assertEqualInt(1, archive_entry_stat(ae)->st_nlink);
	assertEqualInt(1, archive_entry_uid(ae));
	assertEqualInt(2, archive_entry_gid(ae));

	/* End of archive. */
	assertEqualInt(ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualInt(archive_compression(a), ARCHIVE_COMPRESSION_BZIP2);
	assertEqualInt(archive_format(a), ARCHIVE_FORMAT_ISO9660_ROCKRIDGE);

	/* Close the archive. */
	assertEqualInt(0, archive_read_close(a));
#if ARCHIVE_VERSION_NUMBER < 2000000
	archive_read_finish(a);
#else
	assertEqualInt(0, archive_read_finish(a));
#endif
}


