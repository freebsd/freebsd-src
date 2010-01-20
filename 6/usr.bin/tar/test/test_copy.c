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
	assertEqualInt(0, mkdir("f", 0775));
	assertEqualInt(0, mkdir("l", 0775));
	assertEqualInt(0, mkdir("m", 0775));
	assertEqualInt(0, mkdir("s", 0775));
	assertEqualInt(0, mkdir("d", 0775));

	for (i = 0; i < 200; i++) {
		buff[0] = 'f';
		buff[1] = '/';
		/* Create a file named "f/abcdef..." */
		buff[i + 2] = 'a' + (i % 26);
		buff[i + 3] = '\0';
		fd = open(buff, O_CREAT | O_WRONLY, 0644);
		assert(fd >= 0);
		assertEqualInt(i + 3, write(fd, buff, strlen(buff)));
		close(fd);

		/* Create a link named "l/abcdef..." to the above. */
		strcpy(buff2, buff);
		buff2[0] = 'l';
		assertEqualInt(0, link(buff, buff2));

		/* Create a link named "m/abcdef..." to the above. */
		strcpy(buff2, buff);
		buff2[0] = 'm';
		assertEqualInt(0, link(buff, buff2));

		/* Create a symlink named "s/abcdef..." to the above. */
		strcpy(buff2 + 3, buff);
		buff[0] = 's';
		buff2[0] = '.';
		buff2[1] = '.';
		buff2[2] = '/';
		assertEqualInt(0, symlink(buff2, buff));

		/* Create a dir named "d/abcdef...". */
		buff[0] = 'd';
		assertEqualInt(0, mkdir(buff, 0775));
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
	char name1[260];
	char name2[260];
	char contents[260];
	int i, j, r;
	int fd;
	int len;
	const char *p, *dp;
	DIR *d;
	struct dirent *de;

	/* Generate the names we know should be there and verify them. */
	for (i = 1; i < 200; i++) {
		/* Generate a base name of the correct length. */
		for (j = 0; j < i; ++j)
			filename[j] = 'a' + (j % 26);
#if 0
		for (n = i; n > 0; n /= 10)
			filename[--j] = '0' + (n % 10);
#endif
		filename[i] = '\0';

		/* Verify a file named "f/abcdef..." */
		strcpy(name1, "f/");
		strcat(name1, filename);
		if (limit != LIMIT_USTAR || strlen(filename) <= 100) {
			fd = open(name1, O_RDONLY);
			failure("Couldn't open \"%s\": %s",
			    name1, strerror(errno));
			if (assert(fd >= 0)) {
				len = read(fd, contents, i + 10);
				close(fd);
				assertEqualInt(len, i + 2);
				/* Verify contents of 'contents' */
				contents[len] = '\0';
				failure("Each test file contains its own name");
				assertEqualString(name1, contents);
				/* stat() for dev/ino for next check */
				assertEqualInt(0, lstat(name1, &st));
			}
		}

		/*
		 * ustar allows 100 chars for links, and we have
		 * "original/" as part of the name, so the link
		 * names here can't exceed 91 chars.
		 */
		strcpy(name2, "l/");
		strcat(name2, filename);
		if (limit != LIMIT_USTAR || strlen(name2) <= 100) {
			/* Verify hardlink "l/abcdef..." */
			assertEqualInt(0, (r = lstat(name2, &st2)));
			if (r == 0) {
				assertEqualInt(st2.st_dev, st.st_dev);
				assertEqualInt(st2.st_ino, st.st_ino);
			}

			/* Verify hardlink "m_abcdef..." */
			name2[0] = 'm';
			assertEqualInt(0, (r = lstat(name2, &st2)));
			if (r == 0) {
				assertEqualInt(st2.st_dev, st.st_dev);
				assertEqualInt(st2.st_ino, st.st_ino);
			}
		}

		/*
		 * Symlink text doesn't include the 'original/' prefix,
		 * so the limit here is 100 characters.
		 */
		/* Verify symlink "s/abcdef..." */
		strcpy(name2, "../s/");
		strcat(name2, filename);
		if (limit != LIMIT_USTAR || strlen(name2) <= 100) {
			/* This is a symlink. */
			failure("Couldn't stat %s (length %d)",
			    filename, strlen(filename));
			if (assertEqualInt(0, lstat(name2 + 3, &st2))) {
				assert(S_ISLNK(st2.st_mode));
				/* This is a symlink to the file above. */
				failure("Couldn't stat %s", name2 + 3);
				if (assertEqualInt(0, stat(name2 + 3, &st2))) {
					assertEqualInt(st2.st_dev, st.st_dev);
					assertEqualInt(st2.st_ino, st.st_ino);
				}
			}
		}

		/* Verify dir "d/abcdef...". */
		strcpy(name1, "d/");
		strcat(name1, filename);
		if (limit != LIMIT_USTAR || strlen(filename) < 100) {
			/* This is a dir. */
			failure("Couldn't stat %s (length %d)",
			    name1, strlen(filename));
			if (assertEqualInt(0, lstat(name1, &st2))) {
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
	for (dp = "dflms"; *dp != '\0'; ++dp) {
		char dir[2];
		dir[0] = *dp; dir[1] = '\0';
		d = opendir(dir);
		failure("Unable to open dir '%s'", dir);
		if (!assert(d != NULL))
			continue;
		while ((de = readdir(d)) != NULL) {
			p = de->d_name;
			switch(dp[0]) {
			case 'l': case 'm':
				if (limit == LIMIT_USTAR) {
					failure("strlen(p) = %d", strlen(p));
					assert(strlen(p) <= 100);
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
				if (p[0] != '.' || (p[1] != '.' && p[1] != '\0')) {
					for (i = 0; p[i] != '\0' && i < 200; i++) {
						failure("i=%d, p[i]='%c' 'a'+(i%%26)='%c'", i, p[i], 'a' + (i % 26));
						assertEqualInt(p[i], 'a' + (i % 26));
					}
					assert(p[i] == '\0');
				}
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
}

static void
copy_basic(void)
{
	int r;

	assertEqualInt(0, mkdir("plain", 0775));
	assertEqualInt(0, chdir("plain"));

	/*
	 * Use the tar program to create an archive.
	 */
	r = systemf("%s cf archive -C ../original f d l m s >pack.out 2>pack.err",
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

	verify_tree(LIMIT_NONE);
	assertEqualInt(0, chdir(".."));
}

static void
copy_ustar(void)
{
	const char *target = "ustar";
	int r;

	assertEqualInt(0, mkdir(target, 0775));
	assertEqualInt(0, chdir(target));

	/*
	 * Use the tar program to create an archive.
	 */
	r = systemf("%s cf archive --format=ustar -C ../original f d l m s >pack.out 2>pack.err",
	    testprog);
	failure("Error invoking \"%s cf archive --format=ustar\"", testprog);
	assertEqualInt(r, 0);

	/* Verify that nothing went to stdout. */
	assertEmptyFile("pack.out");
	/* Stderr is non-empty, since there are a bunch of files
	 * with filenames too long to archive. */

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
