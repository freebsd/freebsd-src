/*
 * Copyright (c) 1999 Global Technology Associates, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/kgzip/kgzld.c,v 1.2 1999/08/28 01:16:43 peter Exp $
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "elfhdr.h"
#include "kgzip.h"

#define KGZOFF sizeof(struct kgz_elfhdr)

#define align(x, y) (((x) + (y) - 1) & ~((y) - 1))

/*
 * Link KGZ file and loader.
 */
void
kgzld(struct kgz_hdr * kh, const char *f1, const char *f2)
{
    char addr[16];
    struct iodesc idi;
    pid_t pid;
    size_t n;
    int status;

    if (strcmp(kh->ident, "KGZ")) {
	if ((idi.fd = open(idi.fname = f1, O_RDONLY)) == -1)
	    err(1, "%s", idi.fname);
	n = xread(&idi, kh, sizeof(*kh), KGZOFF);
	xclose(&idi);
	if (n != sizeof(*kh) || strcmp(kh->ident, "KGZ"))
	    errx(1, "%s: Invalid format", idi.fname);
    }
    sprintf(addr, "%#x", align(kh->dload + kh->dsize, 0x1000));
    switch (pid = fork()) {
    case -1:
	err(1, NULL);
    case 0:
	execlp("ld", "ld", "-Ttext", addr, "-o", f2, loader, f1, NULL);
	warn(NULL);
	_exit(1);
    default:
	if ((pid = waitpid(pid, &status, 0)) == -1)
	    err(1, NULL);
	if (WIFSIGNALED(status) || WEXITSTATUS(status))
	    exit(1);
    }
}
