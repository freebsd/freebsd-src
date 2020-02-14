/*
 * Copyright (c) 2019 Darren Tucker
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TMPFILE "utimensat.tmp"
#define TMPFILE2 "utimensat.tmp2"

#ifndef AT_SYMLINK_NOFOLLOW
# define AT_SYMLINK_NOFOLLOW 0x80000000
#endif

int utimensat(int, const char *, const struct timespec[2], int);

void
fail(char *msg, long expect, long got)
{
	int saved_errno = errno;

	if (expect == got && got == 0)
		fprintf(stderr, "utimensat: %s: %s\n", msg,
		     strerror(saved_errno));
	else
		fprintf(stderr, "utimensat: %s: expected %ld got %ld\n",
		     msg, expect, got);
	exit(1);
}

int
main(void)
{
	int fd;
	struct stat sb;
	struct timespec ts[2];

	if ((fd = open(TMPFILE, O_CREAT, 0600)) == -1)
		fail("open", 0, 0);
	close(fd);

	ts[0].tv_sec = 12345678;
	ts[0].tv_nsec = 23456789;
	ts[1].tv_sec = 34567890;
	ts[1].tv_nsec = 45678901;
	if (utimensat(AT_FDCWD, TMPFILE, ts, AT_SYMLINK_NOFOLLOW) == -1)
		fail("utimensat", 0, 0);

	if (stat(TMPFILE, &sb) == -1)
		fail("stat", 0, 0 );
	if (sb.st_atime != 12345678)
		fail("st_atime", 0, 0 );
	if (sb.st_mtime != 34567890)
		fail("st_mtime", 0, 0 );
#if 0
	/*
	 * Results expected to be rounded to the nearest microsecond.
	 * Depends on timestamp precision in kernel and filesystem so
	 * disabled by default.
	 */
	if (sb.st_atim.tv_nsec != 23456000)
		fail("atim.tv_nsec", 23456000, sb.st_atim.tv_nsec);
	if (sb.st_mtim.tv_nsec != 45678000)
		fail("mtim.tv_nsec", 45678000, sb.st_mtim.tv_nsec);
#endif

	if (rename(TMPFILE, TMPFILE2) == -1)
		fail("rename", 0, 0);
	if (symlink(TMPFILE2, TMPFILE) == -1)
		fail("symlink", 0, 0);

	if (utimensat(AT_FDCWD, TMPFILE, ts, AT_SYMLINK_NOFOLLOW) != -1)
		fail("utimensat followed symlink", 0, 0);

	if (!(unlink(TMPFILE) == 0 && unlink(TMPFILE2) == 0))
		fail("unlink", 0, 0);
	exit(0);
}
