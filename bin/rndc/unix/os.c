/*
 * Copyright (C) 2004, 2005  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: os.c,v 1.6.18.2 2005/04/29 00:15:41 marka Exp $ */

/*! \file */

#include <config.h>

#include <rndc/os.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

int
set_user(FILE *fd, const char *user) {
	struct passwd *pw;

	pw = getpwnam(user);
	if (pw == NULL) {
		errno = EINVAL;
		return (-1);
	}
	return (fchown(fileno(fd), pw->pw_uid, -1));
}

FILE *
safe_create(const char *filename) {
	int fd;
	FILE *f;
        struct stat sb;
	int flags = O_WRONLY;

        if (stat(filename, &sb) == -1) {
                if (errno != ENOENT)
			return (NULL);
		flags = O_WRONLY | O_CREAT | O_EXCL;
        } else if ((sb.st_mode & S_IFREG) == 0) {
		errno = EOPNOTSUPP;
		return (NULL);
	} else
		flags = O_WRONLY | O_TRUNC;

	fd = open(filename, flags, S_IRUSR | S_IWUSR);
	if (fd == -1)
		return (NULL);
	f = fdopen(fd, "w");
	if (f == NULL)
		close(fd);
	return (f);
}
