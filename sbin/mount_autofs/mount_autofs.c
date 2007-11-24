/*
 * Copyright (c) 2004 Alfred Perlstein <alfred@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: mount_autofs.c,v 1.5 2004/09/08 08:12:21 bright Exp $
 * $FreeBSD$
 */
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/uio.h>

void usage(void);

const char *progname;

void
usage(void) {

	errx(1, "usage: %s node", progname);
}
int	mymount(const char *type, const char *dir, int flags, void *data);

#if __FreeBSD_version < 600000
int
mymount(const char *type, const char *dir, int flags, void *data)
{

	return (mount(type, dir, flags, data));
}
#else
void	ioset(struct iovec *iovp, const char *str);

void
ioset(struct iovec *iovp, const char *str)
{

	iovp->iov_base = __DECONST(char *, str);
	iovp->iov_len = strlen(str) + 1;
}

int
mymount(
	const char *type,
	const char *dir,
	int flags __unused,
	void *data __unused
)
{
	struct iovec iov[4], *iovp;

	iovp = &iov[0];
	ioset(iovp++, "fstype");
	ioset(iovp++, type);
	ioset(iovp++, "fspath");
	ioset(iovp++, dir);
	return (nmount(iov, 4, 0));
}
#endif

int
main(int argc, char **argv)
{
	int error;
	int ch;

	progname = argv[0];

	while ((ch = getopt(argc, argv, "o:")) != -1) {
		/* just eat opts for now */
		switch (ch) {
		case '?':
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 2) {
		usage();
	}

	error = mymount("autofs", argv[1], 0, NULL);
	if (error)
		perror("mount");
	return (error == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
