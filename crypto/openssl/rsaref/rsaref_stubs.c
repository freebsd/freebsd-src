/*
 * $FreeBSD$
 *
 * Copyright (c) 2000
 *	Jordan Hubbard.  All rights reserved.
 *
 * Stub functions for RSA code.  If you link with this code, you will
 * get a full set of weak symbol references to the rsaref library
 * functions which are required by openssl.  These can then be occluded
 * by the real rsaref library by explicitly linking with it or, failing
 * that, these stub functions will also attempt to find an appropriate
 * rsaref library in the search path and do the link-up at runtime.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE. SO THERE.
 *
 */

#ifndef NO_RSA

#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <openssl/rsa.h>

#define VERBOSE_STUBS	/* undef if you don't want missing rsaref reported */

#ifdef PIC
#include <dlfcn.h>

#define RSA_SHLIB	"librsaref.so"	/* be more exact if you need to */

static void *
getsym(const char *sym)
{
    static void *rsalib;
    static int whined;
    void *ret = NULL;

    if (!rsalib)
	rsalib = dlopen(RSA_SHLIB, RTLD_LAZY);
    if (rsalib)
	ret = dlsym(rsalib, sym);
#ifdef VERBOSE_STUBS
    if (!ret && !whined) {
	if (isatty(STDERR_FILENO)) {
	    fprintf(stderr, "** %s: Unable to find an RSAREF shared library (%s).\n", sym, RSA_SHLIB);
	    fprintf(stderr, "** Install the /usr/ports/security/rsaref port or package and run this\n");
	    fprintf(stderr, "** program again. See the OpenSSL chapter in the FreeBSD Handbook, located at\n");
	    fprintf(stderr, "** http://www.freebsd.org/handbook/openssl.html, for more information.\n");
	} else {
	    syslog(LOG_ERR, "** %s: Unable to find an RSAREF shared library \
(%s). Install the /usr/ports/security/rsaref port or package and run this \
program again. See the OpenSSL chapter in the FreeBSD Handbook, located at \
http://www.freebsd.org/handbook/openssl.html, for more information.", \
sym, RSA_SHLIB);
	}
	whined = 1;
     }
#endif
     return ret;
}

int
RSAPrivateDecrypt_stub(unsigned char *output, unsigned int *outlen,
    unsigned char *input, int inputlen, void *RSAkey)
{
    static int (*sym)(unsigned char *, unsigned int *, unsigned char *, int, void *);

    if (sym || (sym = getsym("RSAPrivateDecrypt")))
	return sym(output, outlen, input, inputlen, RSAkey);
    return 0;
}
__weak_reference(RSAPrivateDecrypt_stub, RSAPrivateDecrypt);


int
RSAPrivateEncrypt_stub(unsigned char *output, unsigned int *outlen,
    unsigned char *input, int inputlen, void *RSAkey)
{
    static int (*sym)(unsigned char *, unsigned int *, unsigned char *, int, void *);

    if (sym || (sym = getsym("RSAPrivateEncrypt")))
	return sym(output, outlen, input, inputlen, RSAkey);
    return 0;
}
__weak_reference(RSAPrivateEncrypt_stub, RSAPrivateEncrypt);

int
RSAPublicDecrypt_stub(unsigned char *output, unsigned int *outlen,
    unsigned char *input, int inputlen, void *RSAkey)
{
    static int (*sym)(unsigned char *, unsigned int *, unsigned char *, int, void *);

    if (sym || (sym = getsym("RSAPublicDecrypt")))
	return sym(output, outlen, input, inputlen, RSAkey);
    return 0;
}
__weak_reference(RSAPublicDecrypt_stub, RSAPublicDecrypt);

int
RSAPublicEncrypt_stub(unsigned char *output, unsigned int *outlen,
    unsigned char *input, int inputlen, void *RSAkey, void *randomStruct)
{
    static int (*sym)(unsigned char *, unsigned int *, unsigned char *, int,
	void *, void *);

    if (sym || (sym = getsym("RSAPublicEncrypt")))
	return sym(output, outlen, input, inputlen, RSAkey, randomStruct);
    return 0;
}
__weak_reference(RSAPublicEncrypt_stub, RSAPublicEncrypt);

int
R_GetRandomBytesNeeded_stub(unsigned int *bytesNeeded, void *randomStruct) 
{
    static int (*sym)(unsigned int *, void *);

    if (sym || (sym = getsym("R_GetRandomBytesNeeded")))
	return sym(bytesNeeded, randomStruct);
    return 0;
}
__weak_reference(R_GetRandomBytesNeeded_stub, R_GetRandomBytesNeeded);

void
R_RandomFinal_stub(void *randomStruct)
{
    static void (*sym)(void *);

    if (sym || (sym = getsym("R_RandomFinal")))
	sym(randomStruct);
}
__weak_reference(R_RandomFinal_stub, R_RandomFinal);

int
R_RandomInit_stub(void *randomStruct)
{
    static int (*sym)(void *);

    if (sym || (sym = getsym("R_RandomInit")))
	sym(randomStruct);
    return 0;
}
__weak_reference(R_RandomInit_stub, R_RandomInit);

int
R_RandomUpdate_stub(void *randomStruct,
    unsigned char *block, unsigned int blockLen) 
{
    static int (*sym)(void *, unsigned char *, unsigned int);

    if (sym || (sym = getsym("R_RandomUpdate")))
	sym(randomStruct, block, blockLen);
    return 0;
}
__weak_reference(R_RandomUpdate_stub, R_RandomUpdate);

int
RSA_libversion()
{
	return RSALIB_RSAREF;
}

#else	/* !PIC */

/* Failsafe glue for static linking.  Link but complain like hell. */

/* actually, this creates all sorts of ld(1) problems, forget it for now */

#endif	/* !PIC */

#endif	/* !NO_RSA */
