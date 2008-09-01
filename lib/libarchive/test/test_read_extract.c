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

#define BUFF_SIZE 1000000
#define FILE_BUFF_SIZE 100000

DEFINE_TEST(test_read_extract)
{
	struct archive_entry *ae;
	struct archive *a;
	struct stat st;
	size_t used;
	int i;
	char *buff, *file_buff;
	int fd;
	ssize_t bytes_read;

	buff = malloc(BUFF_SIZE);
	file_buff = malloc(FILE_BUFF_SIZE);

	/* Force the umask to something predictable. */
	umask(022);

	/* Create a new archive in memory containing various types of entries. */
	assert((a = archive_write_new()) != NULL);
	assertA(0 == archive_write_set_format_ustar(a));
	assertA(0 == archive_write_set_compression_none(a));
	assertA(0 == archive_write_open_memory(a, buff, BUFF_SIZE, &used));
	/* A directory to be restored with EXTRACT_PERM. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir_0775");
	archive_entry_set_mode(ae, S_IFDIR | 0775);
	assertA(0 == archive_write_header(a, ae));
	archive_entry_free(ae);
	/* A regular file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	for (i = 0; i < FILE_BUFF_SIZE; i++)
		file_buff[i] = (unsigned char)rand();
	archive_entry_set_size(ae, FILE_BUFF_SIZE);
	assertA(0 == archive_write_header(a, ae));
	assertA(FILE_BUFF_SIZE == archive_write_data(a, file_buff, FILE_BUFF_SIZE));
	archive_entry_free(ae);
	/* A directory that should obey umask when restored. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir");
	archive_entry_set_mode(ae, S_IFDIR | 0777);
	assertA(0 == archive_write_header(a, ae));
	archive_entry_free(ae);
	/* A file in the directory. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir/file");
	archive_entry_set_mode(ae, S_IFREG | 0700);
	assertA(0 == archive_write_header(a, ae));
	archive_entry_free(ae);
	/* A file in a dir that is not already in the archive. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir2/file");
	archive_entry_set_mode(ae, S_IFREG | 0000);
	assertA(0 == archive_write_header(a, ae));
	archive_entry_free(ae);
	/* A dir with a trailing /. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir3/.");
	archive_entry_set_mode(ae, S_IFDIR | 0710);
	assertA(0 == archive_write_header(a, ae));
	archive_entry_free(ae);
	/* Multiple dirs with a single entry. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir4/a/../b/../c/");
	archive_entry_set_mode(ae, S_IFDIR | 0711);
	assertA(0 == archive_write_header(a, ae));
	archive_entry_free(ae);
	/* A symlink. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "symlink");
	archive_entry_set_mode(ae, S_IFLNK | 0755);
	archive_entry_set_symlink(ae, "file");
	assertA(0 == archive_write_header(a, ae));
	archive_entry_free(ae);
	/* Close out the archive. */
	assertA(0 == archive_write_close(a));
#if ARCHIVE_VERSION_NUMBER < 2000000
	archive_write_finish(a);
#else
	assertA(0 == archive_write_finish(a));
#endif

	/* Extract the entries to disk. */
	assert((a = archive_read_new()) != NULL);
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_support_compression_all(a));
	assertA(0 == archive_read_open_memory(a, buff, BUFF_SIZE));
	/* Restore first entry with _EXTRACT_PERM. */
	failure("Error reading first entry", i);
	assertA(0 == archive_read_next_header(a, &ae));
	assertA(0 == archive_read_extract(a, ae, ARCHIVE_EXTRACT_PERM));
	/* Rest of entries get restored with no flags. */
	for (i = 0; i < 7; i++) {
		failure("Error reading entry %d", i+1);
		assertA(0 == archive_read_next_header(a, &ae));
		assertA(0 == archive_read_extract(a, ae, 0));
	}
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	assert(0 == archive_read_close(a));
#if ARCHIVE_VERSION_NUMBER < 2000000
	archive_read_finish(a);
#else
	assert(0 == archive_read_finish(a));
#endif

	/* Test the entries on disk. */
	/* This first entry was extracted with ARCHIVE_EXTRACT_PERM,
	 * so the permissions should have been restored exactly,
	 * including resetting the gid bit on those platforms
	 * where gid is inherited by subdirs. */
	assert(0 == stat("dir_0775", &st));
	failure("This was 0775 in archive, and should be 0775 on disk");
	assertEqualInt(st.st_mode, S_IFDIR | 0775);
	/* Everything else was extracted without ARCHIVE_EXTRACT_PERM,
	 * so there may be some sloppiness about gid bits on directories. */
	assert(0 == stat("file", &st));
	failure("st.st_mode=%o should be %o", st.st_mode, S_IFREG | 0755);
	assertEqualInt(st.st_mode, S_IFREG | 0755);
	failure("The file extracted to disk is the wrong size.");
	assert(st.st_size == FILE_BUFF_SIZE);
	fd = open("file", O_RDONLY);
	failure("The file on disk could not be opened.");
	assert(fd != 0);
	bytes_read = read(fd, buff, FILE_BUFF_SIZE);
	failure("The file contents read from disk are the wrong size");
	assert(bytes_read == FILE_BUFF_SIZE);
	failure("The file contents on disk do not match the file contents that were put into the archive.");
	assert(memcmp(buff, file_buff, FILE_BUFF_SIZE) == 0);
	assert(0 == stat("dir", &st));
	failure("This was 0777 in archive, but umask should make it 0755");
	/* If EXTRACT_PERM wasn't used, be careful to ignore sgid bit
	 * when checking dir modes, as some systems inherit sgid bit
	 * from the parent dir. */
	assertEqualInt(0755, st.st_mode & 0777);
	assert(0 == stat("dir/file", &st));
	assert(st.st_mode == (S_IFREG | 0700));
	assert(0 == stat("dir2", &st));
	assertEqualInt(0755, st.st_mode & 0777);
	assert(0 == stat("dir2/file", &st));
	assert(st.st_mode == (S_IFREG | 0000));
	assert(0 == stat("dir3", &st));
	assertEqualInt(0710, st.st_mode & 0777);
	assert(0 == stat("dir4", &st));
	assertEqualInt(0755, st.st_mode & 0777);
	assert(0 == stat("dir4/a", &st));
	assertEqualInt(0755, st.st_mode & 0777);
	assert(0 == stat("dir4/b", &st));
	assertEqualInt(0755, st.st_mode & 0777);
	assert(0 == stat("dir4/c", &st));
	assertEqualInt(0711, st.st_mode & 0777);
	assert(0 == lstat("symlink", &st));
	assert(S_ISLNK(st.st_mode));
#if HAVE_LCHMOD
	/* Systems that lack lchmod() can't set symlink perms, so skip this. */
	assert((st.st_mode & 07777) == 0755);
#endif
	assert(0 == stat("symlink", &st));
	assert(st.st_mode == (S_IFREG | 0755));

	free(buff);
	free(file_buff);
}
