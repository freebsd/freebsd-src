/* $FreeBSD$ */
/* $OpenBSD: extern.h,v 1.3 1999/10/07 16:30:32 espie Exp $ */
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

/*
 * Convention: all functions that operate on a FILE * also take a filename
 * for diagnostic purposes.  The file can be connected to a pipe, so
 *	- don't rewind
 *	- don't reopen from filename.
 */

struct mygzip_header;
struct signature;

/* main.c */
extern int verbose;
extern int quiet;
extern char *userkey;

/* common.c */
extern int read_header_and_diagnose __P((FILE *file, \
	/*@out@*/struct mygzip_header *h, /*@null@*/struct signature **sign, \
	const char *filename));
extern int reap __P((pid_t pid));

/* sign.c */
extern int sign __P((/*@observer@*/const char *filename, int type, \
	/*@null@*/const char *userid, char *envp[]));

/* check.c */
extern int check_signature __P((/*@dependent@*/FILE *file, \
	/*@null@*/const char *userid, char *envp[], \
	/*@observer@*/const char *filename));

#define PKG_BADSIG 0
#define PKG_GOODSIG 1
#define PKG_UNSIGNED 2
#define PKG_SIGNED 4
#define PKG_SIGERROR 8
#define PKG_SIGUNKNOWN	16

typedef /*@observer@*/char *pchar;

#define MAXID	512
/* sha1.c */
#define SHA1_DB_NAME	"/var/db/pkg/SHA1"

extern void *new_sha1_checker __P((struct mygzip_header *h, \
	struct signature *sign, const char *userid, char *envp[], \
	const char *filename));

extern void sha1_add __P((void *arg, const char *buffer, \
	size_t length));

extern int sha1_sign_ok __P((void *arg));

extern int retrieve_sha1_marker __P((const char *filename, \
	struct signature **sign, const char *userid));

/* x509.c */
#define X509_DB_NAME	"/var/db/pkg/X509"

extern void *new_x509_checker __P((struct mygzip_header *h, \
	struct signature *sign, const char *userid, char *envp[], \
	const char *filename));

extern void x509_add __P((void *arg, const char *buffer, \
	size_t length));

extern int x509_sign_ok __P((void *arg));

extern int retrieve_x509_marker __P((const char *filename, \
	struct signature **sign, const char *userid));
