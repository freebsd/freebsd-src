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

static void
create_tree(void)
{
	char buff[260];
	char buff2[260];
	int i;
	int fd;

	assertEqualInt(0, mkdir("original", 0775));
	chdir("original");

	buff[0] = 'f';
	buff[1] = '_';
	for (i = 0; i < 200; i++) {
		/* Create a file named "f_abcdef..." */
		buff[i + 2] = 'a' + (i % 26);
		buff[i + 3] = '\0';
		fd = open(buff, O_CREAT | O_WRONLY, 0644);
		assert(fd >= 0);
		assertEqualInt(i + 3, write(fd, buff, strlen(buff)));
		close(fd);

		/* Create a link named "l_abcdef..." to the above. */
		strcpy(buff2, buff);
		buff2[0] = 'l';
		assertEqualInt(0, link(buff, buff2));

		/* Create a link named "m_abcdef..." to the above. */
		strcpy(buff2, buff);
		buff2[0] = 'm';
		assertEqualInt(0, link(buff, buff2));

		/* Create a symlink named "s_abcdef..." to the above. */
		buff2[0] = 's';
		assertEqualInt(0, symlink(buff, buff2));

		/* Create a dir named "d_abcdef...". */
		buff2[0] = 'd';
		assertEqualInt(0, mkdir(buff2, 0775));
	}

	chdir("..");
}

#define LIMIT_NONE 0
#define LIMIT_USTAR 1

static void
verify_tree(int limit)
{
	struct stat st, st2;
	char filename[260];
	char contents[260];
	int i, r;
	int fd;
	int len;
	const char *p;
	DIR *d;
	struct dirent *de;

	/* Generate the names we know should be there and verify them. */
	for (i = 0; i < 200; i++) {
		/* Verify a file named "f_abcdef..." */
		filename[0] = 'f';
		filename[1] = '_';
		filename[i + 2] = 'a' + (i % 26);
		filename[i + 3] = '\0';
		if (limit != LIMIT_USTAR || strlen(filename) <= 100) {
			fd = open(filename, O_RDONLY);
			failure("Couldn't open \"%s\": %s",
			    filename, strerror(errno));
			if (assert(fd >= 0)) {
				len = read(fd, contents, i + 10);
				close(fd);
				assertEqualInt(len, i + 3);
				/* Verify contents of 'contents' */
				contents[len] = '\0';
				failure("Each test file contains its own name");
				assertEqualString(filename, contents);
				/* stat() file and get dev/ino for next check */
				assertEqualInt(0, lstat(filename, &st));
			}
		}

		/*
		 * ustar allows 100 chars for links, and we have
		 * "original/" as part of the name, so the link
		 * names here can't exceed 91 chars.
		 */
		if (limit != LIMIT_USTAR || strlen(filename) <= 91) {
			/* Verify hardlink "l_abcdef..." */
			filename[0] = 'l';
			assertEqualInt(0, (r = lstat(filename, &st2)));
			if (r == 0) {
				assertEqualInt(st2.st_dev, st.st_dev);
				assertEqualInt(st2.st_ino, st.st_ino);
			}

			/* Verify hardlink "m_abcdef..." */
			filename[0] = 'm';
			assertEqualInt(0, (r = lstat(filename, &st2)));
			if (r == 0) {
				assertEqualInt(st2.st_dev, st.st_dev);
				assertEqualInt(st2.st_ino, st.st_ino);
			}
		}

		/*
		 * Symlink text doesn't include the 'original/' prefix,
		 * so the limit here is 100 characters.
		 */
		/* Verify symlink "s_abcdef..." */
		filename[0] = 's';
		if (limit != LIMIT_USTAR || strlen(filename) <= 100) {
			/* This is a symlink. */
			failure("Couldn't stat %s (length %d)",
			    filename, strlen(filename));
			if (assertEqualInt(0, lstat(filename, &st2))) {
				assert(S_ISLNK(st2.st_mode));
				/* This is a symlink to the file above. */
				failure("Couldn't stat %s", filename);
				if (assertEqualInt(0, stat(filename, &st2))) {
					assertEqualInt(st2.st_dev, st.st_dev);
					assertEqualInt(st2.st_ino, st.st_ino);
				}
			}
		}

		/* Verify dir "d_abcdef...". */
		filename[0] = 'd';
		if (limit != LIMIT_USTAR || strlen(filename) < 100) {
			/* This is a dir. */
			failure("Couldn't stat %s (length %d)",
			    filename, strlen(filename));
			if (assertEqualInt(0, lstat(filename, &st2))) {
				if (assert(S_ISDIR(st2.st_mode))) {
					/* TODO: opendir/readdir this
					 * directory and make sure
					 * it's empty.
					 */
				}
			}
		}
	}

	/* Now make sure nothing is there that shouldn't be. */
	d = opendir(".");
	while ((de = readdir(d)) != NULL) {
		p = de->d_name;
		switch(p[0]) {
		case 'l': case 'm':
			if (limit == LIMIT_USTAR) {
				failure("strlen(p) = %d", strlen(p));
				assert(strlen(p) < 92);
			}
		case 'd':
			if (limit == LIMIT_USTAR) {
				failure("strlen(p)=%d", strlen(p));
				assert(strlen(p) < 100);
			}
		case 'f': case 's':
			if (limit == LIMIT_USTAR) {
				failure("strlen(p)=%d", strlen(p));
				assert(strlen(p) < 101);
			}
			/* Our files have very particular filename patterns. */
			assert(p[1] == '_' && p[2] == 'a');
			assert(p[2] == 'a');
			p += 2;
			for (i = 0; p[i] != '\0' && i < 200; i++)
				assert(p[i] == 'a' + (i % 26));
			assert(p[i] == '\0');
			break;
		case '.':
			assert(p[1] == '\0' || (p[1] == '.' && p[2] == '\0'));
			break;
		default:
			failure("File %s shouldn't be here", p);
			assert(0);
		}

	}
	closedir(d);
}

static void
copy_basic(void)
{
	int r;

	assertEqualInt(0, mkdir("plain", 0775));
	chdir("plain");

	/*
	 * Use the tar program to create an archive.
	 */
	r = systemf("%s cf archive -C .. original >pack.out 2>pack.err",
	    testprog);
	failure("Error invoking \"%s cf\"", testprog);
	assertEqualInt(r, 0);

	/* Verify that nothing went to stdout or stderr. */
	assertEmptyFile("pack.err");
	assertEmptyFile("pack.out");

	/*
	 * Use tar to unpack the archive into another directory.
	 */
	r = systemf("%s xf archive >unpack.out 2>unpack.err", testprog);
	failure("Error invoking %s xf archive", testprog);
	assertEqualInt(r, 0);

	/* Verify that nothing went to stdout or stderr. */
	assertEmptyFile("unpack.err");
	assertEmptyFile("unpack.out");

	chdir("original");
	verify_tree(LIMIT_NONE);
	chdir("../..");
}

static void
copy_ustar(void)
{
	const char *target = "ustar";
	int r;

	assertEqualInt(0, mkdir(target, 0775));
	chdir(target);

	/*
	 * Use the tar program to create an archive.
	 */
	r = systemf("%s cf archive --format=ustar -C .. original >pack.out 2>pack.err",
	    testprog);
	failure("Error invoking \"%s cf archive --format=ustar\"", testprog);
	assertEqualInt(r, 0);

	/* Verify that nothing went to stdout. */
	assertEmptyFile("pack.out");

	/*
	 * Use tar to unpack the archive into another directory.
	 */
	r = systemf("%s xf archive >unpack.out 2>unpack.err", testprog);
	failure("Error invoking %s xf archive", testprog);
	assertEqualInt(r, 0);

	/* Verify that nothing went to stdout or stderr. */
	assertEmptyFile("unpack.err");
	assertEmptyFile("unpack.out");

	chdir("original");
	verify_tree(LIMIT_USTAR);
	chdir("../..");
}

DEFINE_TEST(test_copy)
{
	int oldumask;

	oldumask = umask(0);

	create_tree(); /* Create sample files in "original" dir. */

	/* Test simple "tar -c | tar -x" pipeline copy. */
	copy_basic();

	/* Same, but constrain to ustar format. */
	copy_ustar();

	umask(oldumask);
}
