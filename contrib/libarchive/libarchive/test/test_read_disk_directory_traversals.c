/*-
 * Copyright (c) 2010 Michihiro NAKAJIMA
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

#include <limits.h>
#if defined(_WIN32) && !defined(__CYGWIN__)
# if !defined(__BORLANDC__)
#  define getcwd _getcwd
# endif
#endif

/*
 * Test if the current filesytem is mounted with noatime option.
 */
static int
atimeIsUpdated(void)
{
	const char *fn = "fs_noatime";
	struct stat st;

	if (!assertMakeFile(fn, 0666, "a"))
		return (0);
	if (!assertUtimes(fn, 1, 0, 1, 0))
		return (0);
	/* Test the file contents in order to update its atime. */
	if (!assertTextFileContents("a", fn))
		return (0);
	if (stat(fn, &st) != 0)
		return (0);
	/* Is atime updated? */
	if (st.st_atime > 1)
		return (1);
	return (0);
}

static void
test_basic(void)
{
	struct archive *a;
	struct archive_entry *ae;
	const void *p;
	char *initial_cwd, *cwd;
	size_t size;
	int64_t offset;
	int file_count;

	assertMakeDir("dir1", 0755);
	assertMakeFile("dir1/file1", 0644, "0123456789");
	assertMakeFile("dir1/file2", 0644, "hello world");
	assertMakeDir("dir1/sub1", 0755);
	assertMakeFile("dir1/sub1/file1", 0644, "0123456789");
	assertMakeDir("dir1/sub2", 0755);
	assertMakeFile("dir1/sub2/file1", 0644, "0123456789");
	assertMakeFile("dir1/sub2/file2", 0644, "0123456789");
	assertMakeDir("dir1/sub2/sub1", 0755);
	assertMakeDir("dir1/sub2/sub2", 0755);
	assertMakeDir("dir1/sub2/sub3", 0755);
	assertMakeFile("dir1/sub2/sub3/file", 0644, "xyz");
	file_count = 12;

	assert((ae = archive_entry_new()) != NULL);
	assert((a = archive_read_disk_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "dir1"));

	while (file_count--) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
		if (strcmp(archive_entry_pathname(ae), "dir1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/file1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 10);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 10);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "0123456789", 10);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 10);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/file2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 11);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 11);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "hello world", 11);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 11);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub1/file1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 10);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 10);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "0123456789", 10);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 10);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub2/file1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 10);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 10);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "0123456789", 10);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 10);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub2/file2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 10);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 10);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "0123456789", 10);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 10);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub2/sub1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub2/sub2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub2/sub3") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub2/sub3/file") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 3);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 3);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "xyz", 3);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 3);
		}
		if (archive_entry_filetype(ae) == AE_IFDIR) {
			/* Descend into the current object */
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_disk_descend(a));
		}
	}
	/* There is no entry. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header2(a, ae));

	/* Close the disk object. */
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));

	/*
	 * Test that call archive_read_disk_open_w, wchar_t version.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open_w(a, L"dir1"));

	file_count = 12;
	while (file_count--) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
		if (wcscmp(archive_entry_pathname_w(ae), L"dir1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (wcscmp(archive_entry_pathname_w(ae),
		    L"dir1/file1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 10);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 10);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "0123456789", 10);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 10);
		} else if (wcscmp(archive_entry_pathname_w(ae),
		    L"dir1/file2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 11);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 11);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "hello world", 11);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 11);
		} else if (wcscmp(archive_entry_pathname_w(ae),
		    L"dir1/sub1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (wcscmp(archive_entry_pathname_w(ae),
		    L"dir1/sub1/file1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 10);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 10);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "0123456789", 10);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 10);
		} else if (wcscmp(archive_entry_pathname_w(ae),
		    L"dir1/sub2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (wcscmp(archive_entry_pathname_w(ae),
		    L"dir1/sub2/file1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 10);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 10);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "0123456789", 10);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 10);
		} else if (wcscmp(archive_entry_pathname_w(ae),
		    L"dir1/sub2/file2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 10);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 10);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "0123456789", 10);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 10);
		} else if (wcscmp(archive_entry_pathname_w(ae),
		    L"dir1/sub2/sub1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (wcscmp(archive_entry_pathname_w(ae),
		    L"dir1/sub2/sub2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (wcscmp(archive_entry_pathname_w(ae),
		    L"dir1/sub2/sub3") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (wcscmp(archive_entry_pathname_w(ae),
		    L"dir1/sub2/sub3/file") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 3);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 3);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "xyz", 3);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 3);
		}
		if (archive_entry_filetype(ae) == AE_IFDIR) {
			/* Descend into the current object */
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_disk_descend(a));
		}
	}
	/* There is no entry. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header2(a, ae));

	/* Close the disk object. */
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));

	/*
	 * Test that call archive_read_disk_open with a regular file.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "dir1/file1"));

	/* dir1/file1 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
	assertEqualString(archive_entry_pathname(ae), "dir1/file1");
	assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
	assertEqualInt(archive_entry_size(ae), 10);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_data_block(a, &p, &size, &offset));
	assertEqualInt((int)size, 10);
	assertEqualInt((int)offset, 0);
	assertEqualMem(p, "0123456789", 10);

	/* There is no entry. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header2(a, ae));

	/* Close the disk object. */
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));


#if defined(_WIN32) && !defined(__CYGWIN__)
	/*
	 * Test for wildcard '*' or '?'
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "dir1/*1"));

	/* dir1/file1 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
	assertEqualString(archive_entry_pathname(ae), "dir1/file1");
	assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
	assertEqualInt(archive_entry_size(ae), 10);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_data_block(a, &p, &size, &offset));
	assertEqualInt((int)size, 10);
	assertEqualInt((int)offset, 0);
	assertEqualMem(p, "0123456789", 10);

	/* dir1/sub1 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
	assertEqualString(archive_entry_pathname(ae), "dir1/sub1");
	assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);

	/* Descend into the current object */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_descend(a));

	/* dir1/sub1/file1 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
	assertEqualString(archive_entry_pathname(ae), "dir1/sub1/file1");
	assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
	assertEqualInt(archive_entry_size(ae), 10);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_data_block(a, &p, &size, &offset));
	assertEqualInt((int)size, 10);
	assertEqualInt((int)offset, 0);
	assertEqualMem(p, "0123456789", 10);

	/* There is no entry. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header2(a, ae));

	/* Close the disk object. */
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
#endif

	/*
	 * We should be on the initial directory where we performed
	 * archive_read_disk_new() after we perfome archive_read_free()
	 *  even if we broke off the directory traversals.
	 */

	/* Save current working directory. */
#ifdef PATH_MAX
	initial_cwd = getcwd(NULL, PATH_MAX);/* Solaris getcwd needs the size. */
#else
	initial_cwd = getcwd(NULL, 0);
#endif

	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "dir1"));

	/* Step in a deep directory. */
	file_count = 12;
	while (file_count--) {
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_next_header2(a, ae));
		if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub1/file1") == 0)
			/*
			 * We are on an another directory at this time.
			 */
			break;
		if (archive_entry_filetype(ae) == AE_IFDIR) {
			/* Descend into the current object */
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_disk_descend(a));
		}
	}
	/* Destroy the disk object. */
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* We should be on the initial working directory. */
	failure(
	    "Current working directory does not return to the initial"
	    "directory");
#ifdef PATH_MAX
	cwd = getcwd(NULL, PATH_MAX);/* Solaris getcwd needs the size. */
#else
	cwd = getcwd(NULL, 0);
#endif
	assertEqualString(initial_cwd, cwd);
	free(initial_cwd);
	free(cwd);

	archive_entry_free(ae);
}

static void
test_symlink_hybrid(void)
{
	struct archive *a;
	struct archive_entry *ae;
	const void *p;
	size_t size;
	int64_t offset;
	int file_count;

	if (!canSymlink()) {
		skipping("Can't test symlinks on this filesystem");
		return;
	}

	/*
	 * Create a sample archive.
	 */
	assertMakeDir("h", 0755);
	assertChdir("h");
	assertMakeDir("d1", 0755);
	assertMakeSymlink("ld1", "d1");
	assertMakeFile("d1/file1", 0644, "d1/file1");
	assertMakeFile("d1/file2", 0644, "d1/file2");
	assertMakeSymlink("d1/link1", "file1");
	assertMakeSymlink("d1/linkX", "fileX");
	assertMakeSymlink("link2", "d1/file2");
	assertMakeSymlink("linkY", "d1/fileY");
	assertChdir("..");

	assert((ae = archive_entry_new()) != NULL);
	assert((a = archive_read_disk_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_set_symlink_hybrid(a));

	/*
	 * Specified file is a symbolic link file.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "h/ld1"));
	file_count = 5;

	while (file_count--) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
		if (strcmp(archive_entry_pathname(ae), "h/ld1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "h/ld1/file1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 8);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 8);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "d1/file1", 8);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 8);
		} else if (strcmp(archive_entry_pathname(ae),
		    "h/ld1/file2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 8);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 8);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "d1/file2", 8);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 8);
		} else if (strcmp(archive_entry_pathname(ae),
		    "h/ld1/link1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFLNK);
		} else if (strcmp(archive_entry_pathname(ae),
		    "h/ld1/linkX") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFLNK);
		}
		if (archive_entry_filetype(ae) == AE_IFDIR) {
			/* Descend into the current object */
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_disk_descend(a));
		}
	}
	/* There is no entry. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header2(a, ae));
	/* Close the disk object. */
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));

	/*
	 * Specified file is a directory and it has symbolic files.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "h"));
	file_count = 9;

	while (file_count--) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
		if (strcmp(archive_entry_pathname(ae), "h") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae), "h/d1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "h/d1/file1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 8);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 8);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "d1/file1", 8);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 8);
		} else if (strcmp(archive_entry_pathname(ae),
		    "h/d1/file2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 8);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 8);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "d1/file2", 8);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 8);
		} else if (strcmp(archive_entry_pathname(ae), "h/ld1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFLNK);
		} else if (strcmp(archive_entry_pathname(ae),
		    "h/d1/link1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFLNK);
		} else if (strcmp(archive_entry_pathname(ae),
		    "h/d1/linkX") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFLNK);
		} else if (strcmp(archive_entry_pathname(ae),
		    "h/link2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFLNK);
		} else if (strcmp(archive_entry_pathname(ae),
		    "h/linkY") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFLNK);
		}
		if (archive_entry_filetype(ae) == AE_IFDIR) {
			/* Descend into the current object */
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_disk_descend(a));
		}
	}
	/* There is no entry. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header2(a, ae));
	/* Close the disk object. */
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	/* Destroy the disk object. */
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	archive_entry_free(ae);
}

static void
test_symlink_logical(void)
{
	struct archive *a;
	struct archive_entry *ae;
	const void *p;
	size_t size;
	int64_t offset;
	int file_count;

	if (!canSymlink()) {
		skipping("Can't test symlinks on this filesystem");
		return;
	}

	/*
	 * Create a sample archive.
	 */
	assertMakeDir("l", 0755);
	assertChdir("l");
	assertMakeDir("d1", 0755);
	assertMakeSymlink("ld1", "d1");
	assertMakeFile("d1/file1", 0644, "d1/file1");
	assertMakeFile("d1/file2", 0644, "d1/file2");
	assertMakeSymlink("d1/link1", "file1");
	assertMakeSymlink("d1/linkX", "fileX");
	assertMakeSymlink("link2", "d1/file2");
	assertMakeSymlink("linkY", "d1/fileY");
	assertChdir("..");

	assert((ae = archive_entry_new()) != NULL);
	assert((a = archive_read_disk_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_set_symlink_logical(a));

	/*
	 * Specified file is a symbolic link file.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "l/ld1"));
	file_count = 5;

	while (file_count--) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
		if (strcmp(archive_entry_pathname(ae), "l/ld1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "l/ld1/file1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 8);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 8);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "d1/file1", 8);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 8);
		} else if (strcmp(archive_entry_pathname(ae),
		    "l/ld1/file2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 8);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 8);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "d1/file2", 8);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 8);
		} else if (strcmp(archive_entry_pathname(ae),
		    "l/ld1/link1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 8);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 8);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "d1/file1", 8);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 8);
		} else if (strcmp(archive_entry_pathname(ae),
		    "l/ld1/linkX") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFLNK);
		}
		if (archive_entry_filetype(ae) == AE_IFDIR) {
			/* Descend into the current object */
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_disk_descend(a));
		}
	}
	/* There is no entry. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header2(a, ae));
	/* Close the disk object. */
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));

	/*
	 * Specified file is a directory and it has symbolic files.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "l"));
	file_count = 13;

	while (file_count--) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
		if (strcmp(archive_entry_pathname(ae), "l") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae), "l/d1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "l/d1/file1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 8);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 8);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "d1/file1", 8);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 8);
		} else if (strcmp(archive_entry_pathname(ae),
		    "l/d1/file2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 8);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 8);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "d1/file2", 8);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 8);
		} else if (strcmp(archive_entry_pathname(ae),
		    "l/d1/link1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 8);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 8);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "d1/file1", 8);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 8);
		} else if (strcmp(archive_entry_pathname(ae),
		    "l/d1/linkX") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFLNK);
		} else if (strcmp(archive_entry_pathname(ae), "l/ld1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "l/ld1/file1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 8);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 8);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "d1/file1", 8);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 8);
		} else if (strcmp(archive_entry_pathname(ae),
		    "l/ld1/file2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 8);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 8);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "d1/file2", 8);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 8);
		} else if (strcmp(archive_entry_pathname(ae),
		    "l/ld1/link1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 8);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 8);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "d1/file1", 8);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 8);
		} else if (strcmp(archive_entry_pathname(ae),
		    "l/ld1/linkX") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFLNK);
		} else if (strcmp(archive_entry_pathname(ae),
		    "l/link2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 8);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 8);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "d1/file2", 8);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 8);
		} else if (strcmp(archive_entry_pathname(ae),
		    "l/linkY") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFLNK);
		}
		if (archive_entry_filetype(ae) == AE_IFDIR) {
			/* Descend into the current object */
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_disk_descend(a));
		}
	}
	/* There is no entry. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header2(a, ae));
	/* Close the disk object. */
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	/* Destroy the disk object. */
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	archive_entry_free(ae);
}

static void
test_symlink_logical_loop(void)
{
	struct archive *a;
	struct archive_entry *ae;
	const void *p;
	size_t size;
	int64_t offset;
	int file_count;

	if (!canSymlink()) {
		skipping("Can't test symlinks on this filesystem");
		return;
	}

	/*
	 * Create a sample archive.
	 */
	assertMakeDir("l2", 0755);
	assertChdir("l2");
	assertMakeDir("d1", 0755);
	assertMakeDir("d1/d2", 0755);
	assertMakeDir("d1/d2/d3", 0755);
	assertMakeDir("d2", 0755);
	assertMakeFile("d2/file1", 0644, "d2/file1");
	assertMakeSymlink("d1/d2/ld1", "../../d1");
	assertMakeSymlink("d1/d2/ld2", "../../d2");
	assertChdir("..");

	assert((ae = archive_entry_new()) != NULL);
	assert((a = archive_read_disk_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_set_symlink_logical(a));

	/*
	 * Specified file is a symbolic link file.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "l2/d1"));
	file_count = 6;

	while (file_count--) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
		if (strcmp(archive_entry_pathname(ae), "l2/d1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae), "l2/d1/d2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae), "l2/d1/d2/d3") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae), "l2/d1/d2/ld1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFLNK);
		} else if (strcmp(archive_entry_pathname(ae), "l2/d1/d2/ld2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "l2/d1/d2/ld2/file1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 8);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 8);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "d2/file1", 8);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 8);
		}
		if (archive_entry_filetype(ae) == AE_IFDIR) {
			/* Descend into the current object */
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_disk_descend(a));
		}
	}
	/* There is no entry. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header2(a, ae));
	/* Destroy the disk object. */
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	archive_entry_free(ae);
}

static void
test_restore_atime(void)
{
	struct archive *a;
	struct archive_entry *ae;
	const void *p;
	size_t size;
	int64_t offset;
	int file_count;

	if (!atimeIsUpdated()) {
		skipping("Can't test restoring atime on this filesystem");
		return;
	}

	assertMakeDir("at", 0755);
	assertMakeFile("at/f1", 0644, "0123456789");
	assertMakeFile("at/f2", 0644, "hello world");
	assertMakeFile("at/fe", 0644, NULL);
	assertUtimes("at/f1", 886600, 0, 886600, 0);
	assertUtimes("at/f2", 886611, 0, 886611, 0);
	assertUtimes("at/fe", 886611, 0, 886611, 0);
	assertUtimes("at", 886622, 0, 886622, 0);
	file_count = 4;

	assert((ae = archive_entry_new()) != NULL);
	assert((a = archive_read_disk_new()) != NULL);

	/*
	 * Test1: Traversals without archive_read_disk_set_atime_restored().
	 */
	failure("Directory traversals should work as well");
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "at"));
	while (file_count--) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
		if (strcmp(archive_entry_pathname(ae), "at") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae), "at/f1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 10);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 10);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "0123456789", 10);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 10);
		} else if (strcmp(archive_entry_pathname(ae), "at/f2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 11);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 11);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "hello world", 11);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 11);
		} else if (strcmp(archive_entry_pathname(ae), "at/fe") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 0);
		}
		if (archive_entry_filetype(ae) == AE_IFDIR) {
			/* Descend into the current object */
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_disk_descend(a));
		}
	}
	/* There is no entry. */
	failure("There must be no entry");
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header2(a, ae));

	failure("Atime must not be restored");
	assertFileAtimeRecent("at");
	assertFileAtimeRecent("at/f1");
	assertFileAtimeRecent("at/f2");
	failure("The atime of a empty file must not be changed");
	assertFileAtime("at/fe", 886611, 0);

	/* Close the disk object. */
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));

	/*
	 * Test2: Traversals with archive_read_disk_set_atime_restored().
	 */
	assertUtimes("at/f1", 886600, 0, 886600, 0);
	assertUtimes("at/f2", 886611, 0, 886611, 0);
	assertUtimes("at/fe", 886611, 0, 886611, 0);
	assertUtimes("at", 886622, 0, 886622, 0);
	file_count = 4;
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_set_atime_restored(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "at"));

	failure("Directory traversals should work as well");
	while (file_count--) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
		if (strcmp(archive_entry_pathname(ae), "at") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae), "at/f1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 10);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 10);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "0123456789", 10);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 10);
		} else if (strcmp(archive_entry_pathname(ae), "at/f2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 11);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 11);
			assertEqualInt((int)offset, 0);
			assertEqualMem(p, "hello world", 11);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 11);
		} else if (strcmp(archive_entry_pathname(ae), "at/fe") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 0);
		}
		if (archive_entry_filetype(ae) == AE_IFDIR) {
			/* Descend into the current object */
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_disk_descend(a));
		}
	}
	/* There is no entry. */
	failure("There must be no entry");
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header2(a, ae));

	failure("Atime must be restored");
	assertFileAtime("at", 886622, 0);
	assertFileAtime("at/f1", 886600, 0);
	assertFileAtime("at/f2", 886611, 0);
	failure("The atime of a empty file must not be changed");
	assertFileAtime("at/fe", 886611, 0);

	/* Destroy the disk object. */
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	archive_entry_free(ae);
}

DEFINE_TEST(test_read_disk_directory_traversals)
{
	/* Basic test. */
	test_basic();
	/* Test hybird mode; follow symlink initially, then not. */
	test_symlink_hybrid();
	/* Test logcal mode; follow all symlinks. */
	test_symlink_logical();
	/* Test logcal mode; prevent loop in symlinks. */ 
	test_symlink_logical_loop();
	/* Test to restore atime. */
	test_restore_atime();
}
