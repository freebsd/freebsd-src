/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2011-2012 Michihiro NAKAJIMA
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

static void
test_read_format_mtree1(void)
{
	const char reffile[] = "test_read_format_mtree.mtree";
	char buff[16];
	struct archive_entry *ae;
	struct archive *a;
	FILE *f;
	/* Compute max 64-bit signed twos-complement value
	 * without relying on overflow.  This assumes that long long
	 * is at least 64 bits. */
	static const long long max_int64 = ((((long long)1) << 62) - 1) + (((long long)1) << 62);
	time_t min_time;
	volatile time_t t;

	extract_reference_file(reffile);

	/*
	 * An access error occurred on some platform when mtree
	 * format handling open a directory. It is for through
	 * the routine which open a directory that we create
	 * "dir" and "dir2" directories.
	 */
	assertMakeDir("dir", 0775);
	assertMakeDir("dir2", 0775);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_options(a, "mtree:checkfs"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 11));

	/*
	 * Read "file", whose data is available on disk.
	 */
	f = fopen("file", "wb");
	assert(f != NULL);
	assertEqualInt(3, fwrite("hi\n", 1, 3, f));
	fclose(f);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(archive_format(a), ARCHIVE_FORMAT_MTREE);
	assertEqualString(archive_entry_pathname(ae), "file");
	assertEqualInt(archive_entry_uid(ae), 18);
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0123);
	assertEqualInt(archive_entry_size(ae), 3);
	assertEqualInt(3, archive_read_data(a, buff, 3));
	assertEqualMem(buff, "hi\n", 3);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir");
	assertEqualInt(AE_IFDIR, archive_entry_filetype(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir/file with space");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "file with space");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3a");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3a/indir3a");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/fullindir2");
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/indir2");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3b");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3b/indir3b");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3b/filename\\with_esc\b\t\fapes");
	assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "notindir");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/emptyfile");
	assertEqualInt(archive_entry_size(ae), 0);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/smallfile");
	assertEqualInt(archive_entry_size(ae), 1);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	/* TODO: Mtree reader should probably return ARCHIVE_WARN for this. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/toosmallfile");
	assertEqualInt(archive_entry_size(ae), 0);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/bigfile");
	assertEqualInt(archive_entry_size(ae), max_int64);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/toobigfile");
	/* Size in mtree is max_int64 + 1; should return max_int64. */
	assertEqualInt(archive_entry_size(ae), max_int64);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/veryoldfile");
	/* The value in the file is MIN_INT64_T, but time_t may be narrower. */
	/* Verify min_time is the smallest possible time_t. */
	min_time = archive_entry_mtime(ae);
	assert(min_time <= 0);
	/* Simply asserting min_time - 1 > 0 breaks with some compiler optimizations. */
	t = (time_t)((uintmax_t)min_time - 1);
	assert(t > 0);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	/* toooldfile is 1 sec older, which should overflow and get returned
	 * with the same value. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/toooldfile");
	assertEqualInt(archive_entry_mtime(ae), min_time);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	/* md5digest */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/md5file");
	assertEqualMem(archive_entry_digest(ae, ARCHIVE_ENTRY_DIGEST_MD5),
	    "\xd4\x1d\x8c\xd9\x8f\x00\xb2\x04\xe9\x80\x09\x98\xec\xf8\x42\x7e",
	    16);

	/* rmd160digest */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/rmd160file");
	assertEqualMem(archive_entry_digest(ae, ARCHIVE_ENTRY_DIGEST_RMD160),
	    "\xda\x39\xa3\xee\x5e\x6b\x4b\x0d\x32\x55\xbf\xef\x95\x60\x18\x90"
	    "\xaf\xd8\x07\x09", 20);

	/* sha1digest */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/sha1file");
	assertEqualMem(archive_entry_digest(ae, ARCHIVE_ENTRY_DIGEST_SHA1),
	    "\xda\x39\xa3\xee\x5e\x6b\x4b\x0d\x32\x55\xbf\xef\x95\x60\x18\x90"
	    "\xaf\xd8\x07\x09", 20);

	/* sha256digest */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/sha256file");
	assertEqualMem(archive_entry_digest(ae, ARCHIVE_ENTRY_DIGEST_SHA256),
	    "\xe3\xb0\xc4\x42\x98\xfc\x1c\x14\x9a\xfb\xf4\xc8\x99\x6f\xb9\x24"
	    "\x27\xae\x41\xe4\x64\x9b\x93\x4c\xa4\x95\x99\x1b\x78\x52\xb8\x55",
	    32);

	/* sha384digest */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/sha384file");
	assertEqualMem(archive_entry_digest(ae, ARCHIVE_ENTRY_DIGEST_SHA384),
	    "\x38\xb0\x60\xa7\x51\xac\x96\x38\x4c\xd9\x32\x7e\xb1\xb1\xe3\x6a"
	    "\x21\xfd\xb7\x11\x14\xbe\x07\x43\x4c\x0c\xc7\xbf\x63\xf6\xe1\xda"
	    "\x27\x4e\xde\xbf\xe7\x6f\x65\xfb\xd5\x1a\xd2\xf1\x48\x98\xb9\x5b",
	    48);

	/* sha512digest */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/sha512file");
	assertEqualMem(archive_entry_digest(ae, ARCHIVE_ENTRY_DIGEST_SHA512),
	    "\xcf\x83\xe1\x35\x7e\xef\xb8\xbd\xf1\x54\x28\x50\xd6\x6d\x80\x07"
	    "\xd6\x20\xe4\x05\x0b\x57\x15\xdc\x83\xf4\xa9\x21\xd3\x6c\xe9\xce"
	    "\x47\xd0\xd1\x3c\x5d\x85\xf2\xb0\xff\x83\x18\xd2\x87\x7e\xec\x2f"
	    "\x63\xb9\x31\xbd\x47\x41\x7a\x81\xa5\x38\x32\x7a\xf9\x27\xda\x3e",
	    64);

	/* md5 digest is too short */
	assertEqualIntA(a, ARCHIVE_WARN, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/md5tooshort");
	assertMemoryFilledWith(archive_entry_digest(ae, ARCHIVE_ENTRY_DIGEST_MD5),
	    16, 0x00);

	/* md5 digest is too long */
	assertEqualIntA(a, ARCHIVE_WARN, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/md5toolong");
	assertMemoryFilledWith(archive_entry_digest(ae, ARCHIVE_ENTRY_DIGEST_MD5),
	    16, 0x00);

	/* md5 digest is uppercase hex */
	assertEqualIntA(a, ARCHIVE_WARN, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/md5caphex");
	assertMemoryFilledWith(archive_entry_digest(ae, ARCHIVE_ENTRY_DIGEST_MD5),
	    16, 0x00);

	/* md5 digest is not hex */
	assertEqualIntA(a, ARCHIVE_WARN, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/md5nothex");
	assertMemoryFilledWith(archive_entry_digest(ae, ARCHIVE_ENTRY_DIGEST_MD5),
	    16, 0x00);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(30, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_read_format_mtree2(void)
{
	static char archive[] =
	    "#mtree\n"
	    "d type=dir content=.\n";
	struct archive_entry *ae;
	struct archive *a;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_options(a, "mtree:checkfs"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive, sizeof(archive)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(archive_format(a), ARCHIVE_FORMAT_MTREE);
	assertEqualString(archive_entry_pathname(ae), "d");
	assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Reported to libarchive.googlecode.com as Issue 121.
 */
static void
test_read_format_mtree3(void)
{
	static char archive[] =
	    "#mtree\n"
	    "a type=file contents=file\n"
	    "b type=link link=a\n"
	    "c type=file contents=file\n";
	struct archive_entry *ae;
	struct archive *a;

	assertMakeDir("mtree3", 0777);
	assertChdir("mtree3");
	assertMakeFile("file", 0644, "file contents");

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_options(a, "mtree:checkfs"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive, sizeof(archive)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "a");
	assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "b");
	assertEqualInt(archive_entry_filetype(ae), AE_IFLNK);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "c");
	assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(3, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	assertChdir("..");
}

DEFINE_TEST(test_read_format_mtree)
{
	test_read_format_mtree1();
	test_read_format_mtree2();
	test_read_format_mtree3();
}

DEFINE_TEST(test_read_format_mtree_filenames_only)
{
	static char archive[] =
	    "/set type=file mode=0644\n"
	    "./a\n"
	    "./b\n"
	    "./c\n"
	    "./d\n"
	    "./e\n"
	    "./f mode=0444\n";
	struct archive_entry *ae;
	struct archive *a;

	assertMakeFile("file", 0644, "file contents");

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_options(a, "mtree:checkfs"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive, sizeof(archive)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./a");	
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./b");
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./c");
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./d");
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./e");
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./f");
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0444);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(6, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_mtree_nochange)
{
	static char archive[] =
	    "#mtree\n"
	    "./a type=file mode=0644 time=123\n"
	    "./b type=file mode=0644 time=234\n"
	    "./c type=file mode=0644 time=345\n";
	static char archive2[] =
	    "#mtree\n"
	    "./a type=file mode=0644 time=123 nochange\n"
	    "./b type=file mode=0644 time=234\n"
	    "./c type=file mode=0644 time=345 nochange\n";
	struct archive_entry *ae;
	struct archive *a;

	assertMakeFile("a", 0640, "12345");
	assertMakeFile("b", 0664, "123456");
	assertMakeFile("c", 0755, "1234567");

	/*
	 * Test 1. Read a mtree archive without `nochange' keyword.
	 */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_options(a, "mtree:checkfs"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive, sizeof(archive)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./a");
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertEqualInt(archive_entry_mtime(ae), 123);
	assertEqualInt(archive_entry_size(ae), 5);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./b");
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertEqualInt(archive_entry_mtime(ae), 234);
	assertEqualInt(archive_entry_size(ae), 6);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./c");
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertEqualInt(archive_entry_mtime(ae), 345);
	assertEqualInt(archive_entry_size(ae), 7);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(3, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/*
	 * Test 2. Read a mtree archive with `nochange' keyword.
	 */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_options(a, "mtree:checkfs"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive2, sizeof(archive2)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./a");
#if !defined(_WIN32) || defined(__CYGWIN__)
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0640);
#endif
	assert(archive_entry_mtime(ae) != 123);
	assertEqualInt(archive_entry_size(ae), 5);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./b");
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertEqualInt(archive_entry_mtime(ae), 234);
	assertEqualInt(archive_entry_size(ae), 6);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./c");
#if !defined(_WIN32) || defined(__CYGWIN__)
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0755);
#endif
	assert(archive_entry_mtime(ae) != 345);
	assertEqualInt(archive_entry_size(ae), 7);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(3, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_mtree_nomagic_v1_form)
{
	const char reffile[] = "test_read_format_mtree_nomagic.mtree";
	char buff[16];
	struct archive_entry *ae;
	struct archive *a;
	FILE *f;

	extract_reference_file(reffile);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_options(a, "mtree:checkfs"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 11));

	/*
	 * Read "file", whose data is available on disk.
	 */
	f = fopen("file", "wb");
	assert(f != NULL);
	assertEqualInt(3, fwrite("hi\n", 1, 3, f));
	fclose(f);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(archive_format(a), ARCHIVE_FORMAT_MTREE);
	assertEqualString(archive_entry_pathname(ae), "file");
	assertEqualInt(archive_entry_uid(ae), 18);
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0123);
	assertEqualInt(archive_entry_size(ae), 3);
	assertEqualInt(3, archive_read_data(a, buff, 3));
	assertEqualMem(buff, "hi\n", 3);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir");
	assertEqualInt(AE_IFDIR, archive_entry_filetype(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir/file with space");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "file with space");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3a");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3a/indir3a");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/fullindir2");
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/indir2");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3b");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3b/indir3b");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "notindir");
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(12, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Test for a format that NetBSD mtree -C generates.
 */
DEFINE_TEST(test_read_format_mtree_nomagic_v2_form)
{
	const char reffile[] = "test_read_format_mtree_nomagic2.mtree";
	char buff[16];
	struct archive_entry *ae;
	struct archive *a;
	FILE *f;

	extract_reference_file(reffile);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_options(a, "mtree:checkfs"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 11));

	/*
	 * Read "file", whose data is available on disk.
	 */
	f = fopen("file", "wb");
	assert(f != NULL);
	assertEqualInt(3, fwrite("hi\n", 1, 3, f));
	fclose(f);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(archive_format(a), ARCHIVE_FORMAT_MTREE);
	assertEqualString(archive_entry_pathname(ae), "./file");
	assertEqualInt(archive_entry_uid(ae), 18);
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0123);
	assertEqualInt(archive_entry_size(ae), 3);
	assertEqualInt(3, archive_read_data(a, buff, 3));
	assertEqualMem(buff, "hi\n", 3);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./dir");
	assertEqualInt(archive_entry_mode(ae), AE_IFDIR | 0755);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./dir/file with space");
	assertEqualInt(archive_entry_uid(ae), 18);
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./file with space");
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./dir2");
	assertEqualInt(archive_entry_mode(ae), AE_IFDIR | 0755);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./dir2/dir3a");
	assertEqualInt(archive_entry_mode(ae), AE_IFDIR | 0755);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(6, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Test for a format that NetBSD mtree -D generates.
 */
DEFINE_TEST(test_read_format_mtree_nomagic_v2_netbsd_form)
{
	const char reffile[] = "test_read_format_mtree_nomagic3.mtree";
	char buff[16];
	struct archive_entry *ae;
	struct archive *a;
	FILE *f;

	extract_reference_file(reffile);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_options(a, "mtree:checkfs"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 11));

	/*
	 * Read "file", whose data is available on disk.
	 */
	f = fopen("file", "wb");
	assert(f != NULL);
	assertEqualInt(3, fwrite("hi\n", 1, 3, f));
	fclose(f);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(archive_format(a), ARCHIVE_FORMAT_MTREE);
	assertEqualString(archive_entry_pathname(ae), "./file");
	assertEqualInt(archive_entry_uid(ae), 18);
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0123);
	assertEqualInt(archive_entry_size(ae), 3);
	assertEqualInt(3, archive_read_data(a, buff, 3));
	assertEqualMem(buff, "hi\n", 3);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./dir");
	assertEqualInt(archive_entry_mode(ae), AE_IFDIR | 0755);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./dir/file with space");
	assertEqualInt(archive_entry_uid(ae), 18);
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./file with space");
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./dir2");
	assertEqualInt(archive_entry_mode(ae), AE_IFDIR | 0755);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "./dir2/dir3a");
	assertEqualInt(archive_entry_mode(ae), AE_IFDIR | 0755);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(6, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * We should get a warning if the contents file doesn't exist.
 */
DEFINE_TEST(test_read_format_mtree_nonexistent_contents_file)
{
	static char archive[] =
	    "#mtree\n"
	    "a type=file contents=nonexistent_file\n";
	struct archive_entry *ae;
	struct archive *a;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_options(a, "mtree:checkfs"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive, sizeof(archive)));
	assertEqualIntA(a, ARCHIVE_WARN, archive_read_next_header(a, &ae));
	assert(strlen(archive_error_string(a)) > 0);
	assertEqualString(archive_entry_pathname(ae), "a");
	assertEqualInt(archive_entry_filetype(ae), AE_IFREG);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Check mtree file with non-printable ascii characters
 */
DEFINE_TEST(test_read_format_mtree_noprint)
{
	const char reffile[] = "test_read_format_mtree_noprint.mtree";
	struct archive_entry *ae;
	struct archive *a;

	extract_reference_file(reffile);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 11));

	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assertEqualString("Can't parse line 3", archive_error_string(a));

	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Check mtree file with tab characters, which are supported but not printable
 */
DEFINE_TEST(test_read_format_mtree_tab)
{
	static char archive[] =
	    "#mtree\n"
	    "\ta\ttype=file\n";
	struct archive_entry *ae;
	struct archive *a;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive, sizeof(archive)));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "a");
	assertEqualInt(archive_entry_filetype(ae), AE_IFREG);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
