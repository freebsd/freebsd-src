/* $OpenBSD: check.c,v 1.2 1999/10/04 21:46:27 espie Exp $ */
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

/* Simple code for a stand-alone package checker */
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include "stand.h"
#include "pgp.h"
#include "gzip.h"
#include "extern.h"

struct checker {
	void *context;
	void (*add)(void *, const char *, size_t);
	int (*get)(void *);
	int status;
};

#define MAX_CHECKERS 20

int 
check_signature(file, userid, envp, filename)
	/*@dependent@*/FILE *file;
	const char *userid;	
	char *envp[];
	/*@observer@*/const char *filename;
{
	struct signature *sign;
	struct mygzip_header h;
	int status;
	char buffer[1024];
	size_t length;
	struct checker checker[MAX_CHECKERS];
	struct signature *sweep;
	int i, j;

	status = read_header_and_diagnose(file, &h, &sign, filename);
	if (status != 1)
		return PKG_UNSIGNED;

	for (sweep = sign, i = 0;  
		sweep != NULL && i < MAX_CHECKERS;
		sweep=sweep->next, i++) {
		switch(sweep->type) {
		case TAG_OLD:
			fprintf(stderr, "File %s uses old signatures, no longer supported\n",
				filename);
			checker[i].context = NULL;
			break;
		case TAG_X509: 
			checker[i].context = new_x509_checker(&h, sweep, userid, envp, filename);
			checker[i].add = x509_add;
			checker[i].get = x509_sign_ok;
			break;
		case TAG_SHA1: 
			checker[i].context = new_sha1_checker(&h, sweep, userid, envp, filename);
			checker[i].add = sha1_add;
			checker[i].get = sha1_sign_ok;
			break;
		case TAG_PGP: 
			checker[i].context = new_pgp_checker(&h, sweep, userid, envp, filename);
			checker[i].add = pgp_add;
			checker[i].get = pgp_sign_ok;
			break;
		default:
			abort();
		}
	}
	while ((length = fread(buffer, 1, sizeof buffer, file)) > 0) {
	    for (j = 0; j < i; j++) {
		if (checker[j].context) {
		    (*checker[j].add)(checker[j].context, buffer, length);
		}
	    }
	}
//	for (j = i-1; j >= 0; j--)
	for (j = 0; j < i; j++) {
	    if (checker[j].context) {
		checker[j].status = (*checker[j].get)(checker[j].context);
	    } else {
		checker[j].status = PKG_SIGERROR;
	    }
	}
	free_signature(sign);
	return checker[0].status;
}

