/* $OpenBSD: sign.c,v 1.3 1999/10/04 21:46:29 espie Exp $ */
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
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <pwd.h>
#include <assert.h>
#include "stand.h"
#include "pgp.h"
#include "gzip.h"
#include "extern.h"

#define COPY_TEMPLATE "%s.sign"

static int 
embed_signature_FILE(orig, dest, sign, filename)
	/*@temp@*/FILE *orig;
	/*@temp@*/FILE *dest; 
	struct signature *sign;
	const char *filename;
{
	struct mygzip_header h;
	int c;

	if (gzip_read_header(orig, &h, NULL) == GZIP_NOT_GZIP)
		return 0;

	if (gzip_write_header(dest, &h, sign) == 0)
		return 0;
	while ((c = fgetc(orig)) != EOF && fputc(c, dest) != EOF)
		;
	if (ferror(dest) != 0) 
		return 0;
	return 1;
}

static int 
embed_signature(filename, copy, sign)
	const char *filename;
	const char *copy; 
	struct signature *sign;
{
	FILE *orig, *dest;
	int success;
	
	success = 0;
	orig= fopen(filename, "r");
	if (orig) {
		dest = fopen(copy, "w");
		if (dest) {
			success = embed_signature_FILE(orig, dest, sign, filename);
			if (fclose(dest) != 0)
				success = 0;
		}
		if (fclose(orig) != 0)
			success = 0;
	}
	return success;
}

int 
sign(filename, type, userid, envp)
	const char *filename;
	const char *userid;
	int type;
	char *envp[];
{
	char *copy;
	int result;
	struct signature *sign;
	int success;

	sign = NULL;
	switch(type) {
	case TAG_PGP:
		success = retrieve_pgp_signature(filename, &sign, userid, envp);
		break;
	case TAG_SHA1:
		success = retrieve_sha1_marker(filename, &sign, userid);
		break;
	case TAG_X509:
		success = retrieve_x509_marker(filename, &sign, userid);
		break;
	}

	if (!success) {
		fprintf(stderr, "Problem signing %s\n", filename);
		free_signature(sign);
		return 0;
	}
	copy = malloc(strlen(filename)+sizeof(COPY_TEMPLATE));
	if (copy == NULL) {
		fprintf(stderr, "Can't allocate memory\n");
		free_signature(sign);
		return 0;
	}
	sprintf(copy, COPY_TEMPLATE, filename);
	result = embed_signature(filename, copy, sign);
	if (result == 0) {
		fprintf(stderr, "Can't embed signature in %s\n", filename);
	} else if (unlink(filename) != 0) {
		fprintf(stderr, "Can't unlink original %s\n", filename);
		result = 0;
	} else if (rename(copy, filename) != 0) {
		fprintf(stderr, "Can't rename new file %s\n", copy);
		result = 0;
	}
	free(copy);
	free_signature(sign);
	return result;
}

