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

#define UMASK 022

#ifndef S_IFLNK
#define	S_IFLNK     0120000
#endif

/*
 * Exercise security checks that should prevent certain
 * writes.
 */

DEFINE_TEST(test_write_disk_secure)
{
	struct archive *a;
	struct archive_entry *ae;
#if defined(HAVE_LCHMOD) && defined(HAVE_SYMLINK) && \
    defined(S_IRUSR) && defined(S_IWUSR) && defined(S_IXUSR)
	int working_lchmod;
#endif

	if (!canSymlink()) {
		skipping("Can't test symlinks on this filesystem");
		return;
	}

	/* Start with a known umask. */
	assertUmask(UMASK);

	/* Create an archive_write_disk object. */
	assert((a = archive_write_disk_new()) != NULL);

	/* Write a regular dir to it. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir");
	archive_entry_set_mode(ae, S_IFDIR | 0777);
	assert(0 == archive_write_header(a, ae));
	archive_entry_free(ae);
	assert(0 == archive_write_finish_entry(a));

	/* Write a symlink to the dir above. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir");
	archive_entry_set_mode(ae, S_IFLNK | 0777);
	archive_entry_set_symlink(ae, "dir");
	archive_entry_set_symlink_type(ae, AE_SYMLINK_TYPE_DIRECTORY);
	archive_write_disk_set_options(a, 0);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));

	/*
	 * Without security checks, we should be able to
	 * extract a file through the link.
	 */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir/filea");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));

	/* But with security checks enabled, this should fail. */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir/fileb");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_write_disk_set_options(a, ARCHIVE_EXTRACT_SECURE_SYMLINKS);
	failure("Extracting a file through a symlink should fail here.");
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);
	assert(0 == archive_write_finish_entry(a));

	/* These tests hardcode the location of /tmp. skip them on Windows for now. */
#if !defined(_WIN32) || defined(__CYGWIN__)
	/* Write an absolute symlink to /tmp. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "/tmp/libarchive_test-test_write_disk_secure-absolute_symlink");
	archive_entry_set_mode(ae, S_IFLNK | 0777);
	archive_entry_set_symlink(ae, "/tmp");
	archive_entry_set_symlink_type(ae, AE_SYMLINK_TYPE_DIRECTORY);
	archive_write_disk_set_options(a, 0);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));

	/* With security checks enabled, this should fail. */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "/tmp/libarchive_test-test_write_disk_secure-absolute_symlink/libarchive_test-test_write_disk_secure-absolute_symlink_path.tmp");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_write_disk_set_options(a, ARCHIVE_EXTRACT_SECURE_SYMLINKS);
	failure("Extracting a file through an absolute symlink should fail here.");
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertFileNotExists("/tmp/libarchive_test-test_write_disk_secure-absolute_symlink/libarchive_test-test_write_disk_secure-absolute_symlink_path.tmp");
	assert(0 == unlink("/tmp/libarchive_test-test_write_disk_secure-absolute_symlink"));
	unlink("/tmp/libarchive_test-test_write_disk_secure-absolute_symlink_path.tmp");
#endif

	/* Create another link. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir2");
	archive_entry_set_mode(ae, S_IFLNK | 0777);
	archive_entry_set_symlink(ae, "dir");
	archive_entry_set_symlink_type(ae, AE_SYMLINK_TYPE_DIRECTORY);
	archive_write_disk_set_options(a, 0);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));

	/*
	 * With symlink check and unlink option, it should remove
	 * the link and create the dir.
	 */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir2/filec");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_write_disk_set_options(a, ARCHIVE_EXTRACT_SECURE_SYMLINKS | ARCHIVE_EXTRACT_UNLINK);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assert(0 == archive_write_finish_entry(a));

	/* Create a nested symlink. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir/nested_link_to_dir");
	archive_entry_set_mode(ae, S_IFLNK | 0777);
	archive_entry_set_symlink(ae, "../dir");
	archive_entry_set_symlink_type(ae, AE_SYMLINK_TYPE_DIRECTORY);
	archive_write_disk_set_options(a, 0);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));

	/* But with security checks enabled, this should fail. */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "dir/nested_link_to_dir/filed");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_write_disk_set_options(a, ARCHIVE_EXTRACT_SECURE_SYMLINKS);
	failure("Extracting a file through a symlink should fail here.");
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);
	assert(0 == archive_write_finish_entry(a));

	/*
	 * Without security checks, extracting a dir over a link to a
	 * dir should follow the link.
	 */
	/* Create a symlink to a dir. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir3");
	archive_entry_set_mode(ae, S_IFLNK | 0777);
	archive_entry_set_symlink(ae, "dir");
	archive_entry_set_symlink_type(ae, AE_SYMLINK_TYPE_DIRECTORY);
	archive_write_disk_set_options(a, 0);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	/* Extract a dir whose name matches the symlink. */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir3");
	archive_entry_set_mode(ae, S_IFDIR | 0777);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	/* Verify link was followed. */
	assertIsSymlink("link_to_dir3", "dir", 1);
	archive_entry_free(ae);

	/*
	 * As above, but a broken link, so the link should get replaced.
	 */
	/* Create a symlink to a dir. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir4");
	archive_entry_set_mode(ae, S_IFLNK | 0777);
	archive_entry_set_symlink(ae, "nonexistent_dir");
	archive_entry_set_symlink_type(ae, AE_SYMLINK_TYPE_DIRECTORY);
	archive_write_disk_set_options(a, 0);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	/* Extract a dir whose name matches the symlink. */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir4");
	archive_entry_set_mode(ae, S_IFDIR | 0777);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	/* Verify link was replaced. */
	assertIsDir("link_to_dir4", -1);
	archive_entry_free(ae);

	/*
	 * As above, but a link to a non-dir, so the link should get replaced.
	 */
	/* Create a regular file and a symlink to it */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "non_dir");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_write_disk_set_options(a, 0);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	/* Create symlink to the file. */
	archive_entry_copy_pathname(ae, "link_to_dir5");
	archive_entry_set_mode(ae, S_IFLNK | 0777);
	archive_entry_set_symlink(ae, "non_dir");
	archive_entry_set_symlink_type(ae, AE_SYMLINK_TYPE_FILE);
	archive_write_disk_set_options(a, 0);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	/* Extract a dir whose name matches the symlink. */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir5");
	archive_entry_set_mode(ae, S_IFDIR | 0777);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	/* Verify link was replaced. */
	assertIsDir("link_to_dir5", -1);
	archive_entry_free(ae);

	/* These tests hardcode the location of /tmp. skip them on Windows for now. */
#if !defined(_WIN32) || defined(__CYGWIN__)
	/*
	 * Without security checks, we should be able to
	 * extract an absolute path.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "/tmp/libarchive_test-test_write_disk_secure-absolute_path.tmp");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	assertFileExists("/tmp/libarchive_test-test_write_disk_secure-absolute_path.tmp");
	assert(0 == unlink("/tmp/libarchive_test-test_write_disk_secure-absolute_path.tmp"));

	/* But with security checks enabled, this should fail. */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "/tmp/libarchive_test-test_write_disk_secure-absolute_path.tmp");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_write_disk_set_options(a, ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS);
	failure("Extracting an absolute path should fail here.");
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);
	assert(0 == archive_write_finish_entry(a));
	assertFileNotExists("/tmp/libarchive_test-test_write_disk_secure-absolute_path.tmp");
#endif

	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Test the entries on disk. */
	assertIsDir("dir", 0755);
	assertIsSymlink("link_to_dir", "dir", 1);

#if defined(HAVE_SYMLINK) && defined(HAVE_LCHMOD) && \
    defined(S_IRUSR) && defined(S_IWUSR) && defined(S_IXUSR)
	/* Verify if we are able to lchmod() */
	if (symlink("dir", "testlink_to_dir") == 0) {
		if (lchmod("testlink_to_dir",
		    S_IRUSR | S_IWUSR | S_IXUSR) != 0) {
			switch (errno) {
				case ENOTSUP:
				case ENOSYS:
#if ENOTSUP != EOPNOTSUPP
				case EOPNOTSUPP:
#endif
					working_lchmod = 0;
					break;
				default:
					working_lchmod = 1;
			}
		} else
			working_lchmod = 1;
	} else
		working_lchmod = 0;

	if (working_lchmod) {
		struct stat st;
		assert(0 == lstat("link_to_dir", &st));
		failure("link_to_dir: st.st_mode=%o", st.st_mode);
		assert((st.st_mode & 07777) == 0755);
	}
#endif

	assertIsReg("dir/filea", 0755);
	assertFileNotExists("dir/fileb");
	assertIsDir("link_to_dir2", 0755);
	assertIsReg("link_to_dir2/filec", 0755);
	assertFileNotExists("dir/filed");
}

/*
 * This is a simplified variant of the above test which never turns off secure
 * symlinks. It is designed to test quirks in the Windows implementation of
 * archive_write_disk; however, its behavior under test should not be exclusive
 * to Windows.
 */
DEFINE_TEST(test_write_disk_secure_symlinks_only)
{
	struct archive *a;
	struct archive_entry *ae;
	const int default_options = ARCHIVE_EXTRACT_SECURE_SYMLINKS;

	if (!canSymlink()) {
		skipping("Can't test symlinks on this filesystem");
		return;
	}

	/* Start with a known umask. */
	assertUmask(UMASK);

	/* Create an archive_write_disk object. */
	assert((a = archive_write_disk_new()) != NULL);
	archive_write_disk_set_options(a, default_options);

	/* Write a regular dir to it. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir");
	archive_entry_set_mode(ae, S_IFDIR | 0777);
	assert(0 == archive_write_header(a, ae));
	archive_entry_free(ae);
	assert(0 == archive_write_finish_entry(a));
	assertIsDir("dir", -1);

	/* Write a symlink to the dir above. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir");
	archive_entry_set_mode(ae, S_IFLNK | 0777);
	archive_entry_set_symlink(ae, "dir");
	archive_entry_set_symlink_type(ae, AE_SYMLINK_TYPE_DIRECTORY);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	assertIsSymlink("link_to_dir", "dir", 1);

	/* With security checks enabled, this should fail. */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir/fileb");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	failure("Extracting a file through a symlink should fail here.");
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);
	assert(0 == archive_write_finish_entry(a));
	assertFileNotExists("dir/fileb");

	/* Create another link. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir2");
	archive_entry_set_mode(ae, S_IFLNK | 0777);
	archive_entry_set_symlink(ae, "dir");
	archive_entry_set_symlink_type(ae, AE_SYMLINK_TYPE_DIRECTORY);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	assertIsSymlink("link_to_dir2", "dir", 1);

	/*
	 * With symlink check and unlink option, it should remove
	 * the link and create the dir.
	 */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir2/filec");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_write_disk_set_options(a, default_options | ARCHIVE_EXTRACT_UNLINK);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assert(0 == archive_write_finish_entry(a));
	assertIsDir("link_to_dir2", -1);
	assertIsReg("link_to_dir2/filec", -1);

	/*
	 * Restore the prior security mode.
	 */
	archive_write_disk_set_options(a, default_options);

	/* Create a nested symlink. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir/nested_link_to_dir");
	archive_entry_set_mode(ae, S_IFLNK | 0777);
	archive_entry_set_symlink(ae, "../dir");
	archive_entry_set_symlink_type(ae, AE_SYMLINK_TYPE_DIRECTORY);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	assertIsSymlink("dir/nested_link_to_dir", "../dir", -1);

	/* With security checks enabled, this should fail. */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "dir/nested_link_to_dir/filed");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	failure("Extracting a file through a symlink should fail here.");
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);
	assert(0 == archive_write_finish_entry(a));
	assertFileNotExists("dir/filed");

	/* Create a symlink to a dir. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir3");
	archive_entry_set_mode(ae, S_IFLNK | 0777);
	archive_entry_set_symlink(ae, "dir");
	archive_entry_set_symlink_type(ae, AE_SYMLINK_TYPE_DIRECTORY);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	assertIsSymlink("link_to_dir3", "dir", 1);
	/* Extract a dir whose name matches the symlink. */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir3");
	archive_entry_set_mode(ae, S_IFDIR | 0777);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	archive_entry_free(ae);
	assertIsDir("link_to_dir3", -1);

	/*
	 * As above, but a broken link, so the link should get replaced.
	 */

	/* Create a symlink to a dir. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir4");
	archive_entry_set_mode(ae, S_IFLNK | 0777);
	archive_entry_set_symlink(ae, "nonexistent_dir");
	archive_entry_set_symlink_type(ae, AE_SYMLINK_TYPE_DIRECTORY);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	assertIsSymlink("link_to_dir4", "nonexistent_dir", 1);
	/* Extract a dir whose name matches the symlink. */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir4");
	archive_entry_set_mode(ae, S_IFDIR | 0777);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	archive_entry_free(ae);
	assertIsDir("link_to_dir4", -1);

	/*
	 * As above, but a link to a non-dir, so the link should get replaced.
	 * (file is named "link_to_dir" because we are transforming a link into a dir,)
	 */
	/* Create a regular file and a symlink to it */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "non_dir");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	assertIsReg("non_dir", -1);
	/* Create symlink to the file. */
	archive_entry_copy_pathname(ae, "link_to_dir5");
	archive_entry_set_mode(ae, S_IFLNK | 0777);
	archive_entry_set_symlink(ae, "non_dir");
	archive_entry_set_symlink_type(ae, AE_SYMLINK_TYPE_FILE);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	assertIsSymlink("link_to_dir5", "non_dir", 0);
	/* Extract a dir whose name matches the symlink. */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir5");
	archive_entry_set_mode(ae, S_IFDIR | 0777);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	archive_entry_free(ae);
	assertIsDir("link_to_dir5", -1);

	/*
	 * Create a link to a (technically safe) directory, then replace it, then write through it.
	 * Exercises the safety cache to ensure that it does not treat new entries as safe.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir6");
	archive_entry_set_mode(ae, S_IFLNK | 0777);
	archive_entry_set_symlink(ae, "dir");
	archive_entry_set_symlink_type(ae, AE_SYMLINK_TYPE_DIRECTORY);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	assertIsSymlink("link_to_dir6", "dir", 1);
	/* Replace it. */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir6");
	archive_entry_set_mode(ae, S_IFLNK | 0777);
	archive_entry_set_symlink(ae, "dir");
	archive_entry_set_symlink_type(ae, AE_SYMLINK_TYPE_DIRECTORY);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	assertIsSymlink("link_to_dir6", "dir", 1);
	/* Extract through it. */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir6/filee");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertFileNotExists("dir/filee");

	/*
	 * Create an empty directory, then replace it, then write through it.
	 * Exercises the safety cache to ensure that it does not treat new entries as safe.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir7");
	archive_entry_set_mode(ae, S_IFDIR | 0777);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	assertIsDir("link_to_dir7", -1);
	/* Replace it. */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir7");
	archive_entry_set_mode(ae, S_IFLNK | 0777);
	archive_entry_set_symlink(ae, "dir");
	archive_entry_set_symlink_type(ae, AE_SYMLINK_TYPE_DIRECTORY);
	assert(0 == archive_write_header(a, ae));
	assert(0 == archive_write_finish_entry(a));
	assertIsSymlink("link_to_dir7", "dir", 1);
	/* Extract through it. */
	assert(archive_entry_clear(ae) != NULL);
	archive_entry_copy_pathname(ae, "link_to_dir7/filef");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertFileNotExists("dir/filef");

	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
}
