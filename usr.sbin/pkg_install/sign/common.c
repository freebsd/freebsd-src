/* $OpenBSD: common.c,v 1.3 1999/10/07 16:30:32 espie Exp $ */
/*-
 * Copyright (c) 1999 Marc Espie.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Marc Espie for the OpenBSD
 * Project.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS 
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "stand.h"
#include "gzip.h"
#include "pgp.h"
#include "extern.h"

/* Ensure consistent diagnostics */
int 
read_header_and_diagnose(file, h, sign, filename)
	FILE *file;
	struct mygzip_header *h;
	struct signature **sign;
	const char *filename;
{
	switch(gzip_read_header(file, h, sign)) {
	case GZIP_SIGNED:
		if (sign == NULL) {
			fprintf(stderr, "File %s is already signed\n", filename);
			return 0;
		} else
			return 1;
	case GZIP_UNSIGNED:
		if (sign != NULL) {
			fprintf(stderr, "File %s is not a signed gzip file\n", filename);
			return 0;
		} else
			return 1;
	case GZIP_NOT_GZIP:
		fprintf(stderr, "File %s is not a gzip file\n", filename);
		return 0;
	case GZIP_NOT_PGPSIGNED:
		fprintf(stderr, "File %s contains an unknown extension\n", filename);
		return 0;
	default:
		/* this should not happen */
		abort();
	}
}

int 
reap(pid)
	pid_t pid;
{
	int pstat;
	pid_t result;

	do {
		result = waitpid(pid, &pstat, 0);
	} while (result == -1 && errno == EINTR);
	return result == -1 ? -1 : pstat;
}

