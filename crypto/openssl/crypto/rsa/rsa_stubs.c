/*-
 * Copyright (c) 2000 Peter Wemm <peter@FreeBSD.org>
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
 * SUCH DAMAGE. SO THERE.
 *
 * $FreeBSD$
 */

#ifndef NO_RSA

#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include "cryptlib.h"
#include <openssl/rsa.h>

#define VERBOSE_STUBS	/* undef if you don't want missing rsaref reported */

#ifdef PIC
#include <dlfcn.h>

#define RSAUSA_SHLIB	"librsaUSA.so"
#define RSAINTL_SHLIB	"librsaINTL.so"

static void *
getsym(const char *sym)
{
    static void *rsalib;
    static int whined;
    void *ret = NULL;

    if (!rsalib)
	rsalib = dlopen(RSAINTL_SHLIB, RTLD_LAZY);
    if (!rsalib)
	rsalib = dlopen(RSAUSA_SHLIB, RTLD_LAZY);
    if (rsalib)
	ret = dlsym(rsalib, sym);
#ifdef VERBOSE_STUBS
    if (!ret && !whined) {
	if (isatty(STDERR_FILENO)) {
	    fprintf(stderr, "** %s: Unable to find an RSA implementation shared library.\n", sym);
	    fprintf(stderr, "** Install either the USA (%s) or International (%s)\n", RSAUSA_SHLIB, RSAINTL_SHLIB);
	    fprintf(stderr, "** RSA library on your system and run this program again.\n");
	    fprintf(stderr, "** See the OpenSSL chapter in the FreeBSD Handbook, located at\n");
	    fprintf(stderr, "** http://www.freebsd.org/handbook/openssl.html, for more information.\n");
	} else {
	    syslog(LOG_ERR, "%s: Unable to find an RSA implementation shared \
library. Install either the USA (%s) or International (%s) RSA library on \
your system and run this program again. See the OpenSSL chapter in the \
FreeBSD Handbook, located at http://www.freebsd.org/handbook/openssl.html, \
for more information.", sym, RSAUSA_SHLIB, RSAINTL_SHLIB);
	}
	whined = 1;
     }
#endif
     return ret;
}

RSA_METHOD *
RSA_PKCS1_stub(void)
{
    static RSA_METHOD * (*sym)(void);

    if (sym || (sym = getsym("RSA_PKCS1")))
	return sym();
    return NULL;
}
__weak_reference(RSA_PKCS1_stub, RSA_PKCS1);

void
ERR_load_RSA_strings_stub(void)
{
    static void (*sym)(void);

    if (sym || (sym = getsym("ERR_load_RSA_strings")))
	sym();
}
__weak_reference(ERR_load_RSA_strings_stub, ERR_load_RSA_strings);

int
RSA_libversion_stub(void)
{
    static void (*sym)(void);

    if (sym || (sym = getsym("RSA_libversion")))
	sym();
}
__weak_reference(RSA_libversion_stub, RSA_libversion);

#else	/* !PIC */

/* Sigh, just get your own libs, ld(1) doesn't deal with weaks here */

#endif	/* !PIC */
#endif	/* NO_RSA */
