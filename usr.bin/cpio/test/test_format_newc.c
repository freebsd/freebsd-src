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

static int
is_hex(const char *p, size_t l)
{
	while (l > 0) {
		if ((*p >= '0' && *p <= '9')
		    || (*p >= 'a' && *p <= 'f')
		    || (*p >= 'A' && *p <= 'F'))
		{
			--l;
			++p;
		} else
			return (0);

	}
	return (1);
}

static int
from_hex(const char *p, size_t l)
{
	int r = 0;

	while (l > 0) {
		r *= 16;
		if (*p >= 'a' && *p <= 'f')
			r += *p + 10 - 'a';
		else if (*p >= 'A' && *p <= 'F')
			r += *p + 10 - 'A';
		else
			r += *p - '0';
		--l;
		++p;
	}
	return (r);
}

DEFINE_TEST(test_format_newc)
{
	int fd, list;
	int r;
	int devmajor, devminor, ino, gid;
	time_t t, t2, now;
	char *p, *e;
	size_t s, fs, ns;
	mode_t oldmask;

	oldmask = umask(0);

	/*
	 * Create an assortment of files.
	 * TODO: Extend this to cover more filetypes.
	 */
	list = open("list", O_CREAT | O_WRONLY, 0644);

	/* "file1" */
	fd = open("file1", O_CREAT | O_WRONLY, 0644);
	assert(fd >= 0);
	assertEqualInt(10, write(fd, "123456789", 10));
	close(fd);
	assertEqualInt(6, write(list, "file1\n", 6));

	/* "hardlink" */
	assertEqualInt(0, link("file1", "hardlink"));
	assertEqualInt(9, write(list, "hardlink\n", 9));

	/* Another hardlink, but this one won't be archived. */
	assertEqualInt(0, link("file1", "hardlink2"));

	/* "symlink" */
	assertEqualInt(0, symlink("file1", "symlink"));
	assertEqualInt(8, write(list, "symlink\n", 8));

	/* "dir" */
	assertEqualInt(0, mkdir("dir", 0775));
	assertEqualInt(4, write(list, "dir\n", 4));

	/* Record some facts about what we just created: */
	now = time(NULL); /* They were all created w/in last two seconds. */

	/* Use the cpio program to create an archive. */
	close(list);
	r = systemf("%s -o --format=newc <list >newc.out 2>newc.err",
	    testprog);
	if (!assertEqualInt(r, 0))
		return;

	/* Verify that nothing went to stderr. */
	assertTextFileContents("2 blocks\n", "newc.err");

	/* Verify that stdout is a well-formed cpio file in "newc" format. */
	p = slurpfile(&s, "newc.out");
	assertEqualInt(s, 1024);
	e = p;

	/*
	 * Some of these assertions could be stronger, but it's
	 * a little tricky because they depend on the local environment.
	 */

	/* First entry is "file1" */
	assert(is_hex(e, 110)); /* Entire header is octal digits. */
	assertEqualMem(e + 0, "070701", 6); /* Magic */
	ino = from_hex(e + 6, 8); /* ino */
#if defined(_WIN32) && !defined(__CYGWIN__)
	/* Group members bits and others bits do not work. */ 
	assertEqualInt(0x8180, from_hex(e + 14, 8) & 0xffc0); /* Mode */
#else
	assertEqualInt(0x81a4, from_hex(e + 14, 8)); /* Mode */
#endif
	assertEqualInt(from_hex(e + 22, 8), getuid()); /* uid */
	gid = from_hex(e + 30, 8); /* gid */
	assertEqualMem(e + 38, "00000003", 8); /* nlink */
	t = from_hex(e + 46, 8); /* mtime */
	failure("t=0x%08x now=0x%08x=%d", t, now, now);
	assert(t <= now); /* File wasn't created in future. */
	failure("t=0x%08x now - 2=0x%08x = %d", t, now - 2, now - 2);
	assert(t >= now - 2); /* File was created w/in last 2 secs. */
	failure("newc format stores body only with last appearance of a link\n"
	    "       first appearance should be empty, so this file size\n"
	    "       field should be zero");
	assertEqualInt(0, from_hex(e + 54, 8)); /* File size */
	fs = from_hex(e + 54, 8);
	fs += 3 & -fs;
	devmajor = from_hex(e + 62, 8); /* devmajor */
	devminor = from_hex(e + 70, 8); /* devminor */
	assert(is_hex(e + 78, 8)); /* rdevmajor */
	assert(is_hex(e + 86, 8)); /* rdevminor */
	assertEqualMem(e + 94, "00000006", 8); /* Name size */
	ns = from_hex(e + 94, 8);
	ns += 3 & (-ns - 2);
	assertEqualInt(0, from_hex(e + 102, 8)); /* check field */
	assertEqualMem(e + 110, "file1\0", 6); /* Name contents */
	/* Since there's another link, no file contents here. */
	/* But add in file size so that an error here doesn't cascade. */
	e += 110 + fs + ns;

	/* "symlink" pointing to "file1" */
	assert(is_hex(e, 110));
	assertEqualMem(e + 0, "070701", 6); /* Magic */
	assert(is_hex(e + 6, 8)); /* ino */
#if !defined(_WIN32) || defined(__CYGWIN__)
	/* On Windows, symbolic link and group members bits and 
	 * others bits do not work. */ 
	assertEqualInt(0xa1ff, from_hex(e + 14, 8)); /* Mode */
#endif
	assertEqualInt(from_hex(e + 22, 8), getuid()); /* uid */
	assertEqualInt(gid, from_hex(e + 30, 8)); /* gid */
	assertEqualMem(e + 38, "00000001", 8); /* nlink */
	t2 = from_hex(e + 46, 8); /* mtime */
	failure("First entry created at t=0x%08x this entry created at t2=0x%08x", t, t2);
	assert(t2 == t || t2 == t + 1); /* Almost same as first entry. */
#if defined(_WIN32) && !defined(__CYGWIN__)
	/* Symbolic link does not work. */
	assertEqualMem(e + 54, "0000000a", 8); /* File size */
#else
	assertEqualMem(e + 54, "00000005", 8); /* File size */
#endif
	fs = from_hex(e + 54, 8);
	fs += 3 & -fs;
	assertEqualInt(devmajor, from_hex(e + 62, 8)); /* devmajor */
	assertEqualInt(devminor, from_hex(e + 70, 8)); /* devminor */
	assert(is_hex(e + 78, 8)); /* rdevmajor */
	assert(is_hex(e + 86, 8)); /* rdevminor */
	assertEqualMem(e + 94, "00000008", 8); /* Name size */
	ns = from_hex(e + 94, 8);
	ns += 3 & (-ns - 2);
	assertEqualInt(0, from_hex(e + 102, 8)); /* check field */
	assertEqualMem(e + 110, "symlink\0\0\0", 10); /* Name contents */
#if !defined(_WIN32) || defined(__CYGWIN__)
	assertEqualMem(e + 110 + ns, "file1\0\0\0", 8); /* symlink target */
#endif
	e += 110 + fs + ns;

	/* "dir" */
	assert(is_hex(e, 110));
	assertEqualMem(e + 0, "070701", 6); /* Magic */
	assert(is_hex(e + 6, 8)); /* ino */
#if defined(_WIN32) && !defined(__CYGWIN__)
	/* Group members bits and others bits do not work. */ 
	assertEqualInt(0x41c0, from_hex(e + 14, 8) & 0xffc0); /* Mode */
#else
	assertEqualInt(0x41fd, from_hex(e + 14, 8)); /* Mode */
#endif
	assertEqualInt(from_hex(e + 22, 8), getuid()); /* uid */
	assertEqualInt(gid, from_hex(e + 30, 8)); /* gid */
#ifndef NLINKS_INACCURATE_FOR_DIRS
	assertEqualMem(e + 38, "00000002", 8); /* nlink */
#endif
	t2 = from_hex(e + 46, 8); /* mtime */
	failure("First entry created at t=0x%08x this entry created at t2=0x%08x", t, t2);
	assert(t2 == t || t2 == t + 1); /* Almost same as first entry. */
	assertEqualMem(e + 54, "00000000", 8); /* File size */
	fs = from_hex(e + 54, 8);
	fs += 3 & -fs;
	assertEqualInt(devmajor, from_hex(e + 62, 8)); /* devmajor */
	assertEqualInt(devminor, from_hex(e + 70, 8)); /* devminor */
	assert(is_hex(e + 78, 8)); /* rdevmajor */
	assert(is_hex(e + 86, 8)); /* rdevminor */
	assertEqualMem(e + 94, "00000004", 8); /* Name size */
	ns = from_hex(e + 94, 8);
	ns += 3 & (-ns - 2);
	assertEqualInt(0, from_hex(e + 102, 8)); /* check field */
	assertEqualMem(e + 110, "dir\0\0\0", 6); /* Name contents */
	e += 110 + fs + ns;

	/* Hardlink identical to "file1" */
	/* Since we only wrote two of the three links to this
	 * file, this link should get deferred by the hardlink logic. */
	assert(is_hex(e, 110));
	assertEqualMem(e + 0, "070701", 6); /* Magic */
	failure("If these aren't the same, then the hardlink detection failed to match them.");
	assertEqualInt(ino, from_hex(e + 6, 8)); /* ino */
#if defined(_WIN32) && !defined(__CYGWIN__)
	/* Group members bits and others bits do not work. */ 
	assertEqualInt(0x8180, from_hex(e + 14, 8) & 0xffc0); /* Mode */
#else
	assertEqualInt(0x81a4, from_hex(e + 14, 8)); /* Mode */
#endif
	assertEqualInt(from_hex(e + 22, 8), getuid()); /* uid */
	assertEqualInt(gid, from_hex(e + 30, 8)); /* gid */
	assertEqualMem(e + 38, "00000003", 8); /* nlink */
	t2 = from_hex(e + 46, 8); /* mtime */
	failure("First entry created at t=0x%08x this entry created at t2=0x%08x", t, t2);
	assert(t2 == t || t2 == t + 1); /* Almost same as first entry. */
	assertEqualInt(10, from_hex(e + 54, 8)); /* File size */
	fs = from_hex(e + 54, 8);
	fs += 3 & -fs;
	assertEqualInt(devmajor, from_hex(e + 62, 8)); /* devmajor */
	assertEqualInt(devminor, from_hex(e + 70, 8)); /* devminor */
	assert(is_hex(e + 78, 8)); /* rdevmajor */
	assert(is_hex(e + 86, 8)); /* rdevminor */
	assertEqualMem(e + 94, "00000009", 8); /* Name size */
	ns = from_hex(e + 94, 8);
	ns += 3 & (-ns - 2);
	assertEqualInt(0, from_hex(e + 102, 8)); /* check field */
	assertEqualMem(e + 110, "hardlink\0\0", 10); /* Name contents */
	assertEqualMem(e + 110 + ns, "123456789\0\0\0", 12); /* File contents */
	e += 110 + ns + fs;

	/* Last entry is end-of-archive marker. */
	assert(is_hex(e, 110));
	assertEqualMem(e + 0, "070701", 6); /* Magic */
	assertEqualMem(e + 8, "00000000", 8); /* ino */
	assertEqualMem(e + 14, "00000000", 8); /* mode */
	assertEqualMem(e + 22, "00000000", 8); /* uid */
	assertEqualMem(e + 30, "00000000", 8); /* gid */
	assertEqualMem(e + 38, "00000001", 8); /* nlink */
	assertEqualMem(e + 46, "00000000", 8); /* mtime */
	assertEqualMem(e + 54, "00000000", 8); /* size */
	assertEqualMem(e + 62, "00000000", 8); /* devmajor */
	assertEqualMem(e + 70, "00000000", 8); /* devminor */
	assertEqualMem(e + 78, "00000000", 8); /* rdevmajor */
	assertEqualMem(e + 86, "00000000", 8); /* rdevminor */
	assertEqualInt(11, from_hex(e + 94, 8)); /* name size */
	assertEqualMem(e + 102, "00000000", 8); /* check field */
	assertEqualMem(e + 110, "TRAILER!!!\0\0", 12); /* Name */

	free(p);

	umask(oldmask);
}
