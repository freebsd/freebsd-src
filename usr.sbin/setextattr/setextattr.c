/*-
 * Copyright (c) 1999, 2000, 2001 Robert N. M. Watson
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
 * $FreeBSD$
 */
/*
 * TrustedBSD Project - extended attribute support for UFS-like file systems
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/extattr.h>

#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
usage(void)
{

	fprintf(stderr, "setextattr [attrnamespace] [attrname] [filename] "
	    "[attrvalue]\n");
	exit(-1);
}

int
main(int argc, char *argv[])
{
	struct iovec    iov_buf;
	int	error, attrnamespace;

	if (argc != 5)
		usage();

	error = extattr_string_to_namespace(argv[1], &attrnamespace);
	if (error) {
		perror(argv[1]);
		return (-1);
	}

	iov_buf.iov_base = argv[4];
	iov_buf.iov_len = strlen(argv[4]);

	error = extattr_set_file(argv[3], attrnamespace, argv[2], &iov_buf, 1);
	if (error == -1) {
		perror(argv[3]);
		return (-1);
	}

	return (0);
}
