/* $FreeBSD$ */
/*-
 * Copyright (c) 2000 Softweyr LLC, South Jordan, Utah, USA.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Softweyr LLC ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL Softweyr LLC BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

#include "stand.h"
#include "gzip.h"
#include "extern.h"


/*
 * Default names for the signing key and certificate(s) for verification.
 */
#define CERTFILE "/etc/ssl/pkg.crt"
#define KEYFILE  "/etc/ssl/pkg.key"


/*
 * Private context for X.509 signature checker
 */
struct x509_checker
{
	const char * id;
	const char * filename;

	struct signature * signature;

	STACK_OF(X509) * certs;
	EVP_MD_CTX rsa_ctx, dsa_ctx;
	int has_rsa, has_dsa;
};


static void key_from_name(char *, const char *);

/*
 * Initialize an X.509 "checker" context.
 */
void *
new_x509_checker(h, sign, userid, envp, filename)
	struct mygzip_header *h;
	struct signature *sign;
	const char *userid;	
	char *envp[];
	/*@observer@*/const char *filename;
{
    	FILE * fp;
	struct x509_checker * me;
	char certfile[PATH_MAX + 1] = CERTFILE;
	char * cp;
	X509 * x509;

	assert(sign->type == TAG_X509);

	/*
	 * Make sure data conforms to what we can handle.  We do not write a
	 * trailing null onto the signature like some other types, because
	 * the X.509 signature is binary data.
	 */
	if (sign->length > MAXID) {
	    warnx("Corrupted X.509 header in %s", filename);
	    return 0;
	}

	me = malloc(sizeof *me);
	if (me == NULL) {
	    warn("Cannot allocate x509_checker");
	    return 0;
	}
	me->id = sign->data;
	me->filename = filename;
	me->signature = sign;
	me->has_rsa = 0;
	me->has_dsa = 0;

	key_from_name(certfile, userkey);

	/*
	 * Load just the crypto library error strings.
	 */
	ERR_load_crypto_strings();

	/*
	 * Load the stack of X.509 certs we will compare against.
	 *
	 * KLUDGE: this needs to be fleshed out a bit.  We can do better
	 * than hard-coding the location of the cert key file.
	 */
	me->certs = sk_X509_new_null();

	fp = fopen(certfile, "r");
	if (fp == NULL) {
	    warnx("Cannot open public key %s", certfile);
	    return 0;
	}

	if (verbose)
	    printf("Loading certificates from %s:\n", certfile);

	while (x509 = PEM_read_X509(fp, NULL, NULL, 0))	{
	    sk_X509_push(me->certs, x509);

	    switch (EVP_PKEY_type(X509_get_pubkey(x509)->type))
	    {
	    case EVP_PKEY_RSA:
		me->has_rsa = 1;
		break;

	    case EVP_PKEY_DSA:
		me->has_dsa = 1;
		break;

	    default:
		warnx("Uknown certificate type");
		return 0;
	    }

	    /*
	     * By default, print the contents of the cert we matched so the
	     * user can decide if she is willing to accept a package from
	     * whoever signed this.
	     */
	    if (!quiet)
		X509_print_fp(stdout, x509);
	}
	fclose(fp);
  
	/*
	 * Initialize the verification contexts for both RSA and DSA.
	 */
	if (me->has_rsa) EVP_VerifyInit(&me->rsa_ctx, EVP_sha1());
	if (me->has_dsa) EVP_VerifyInit(&me->dsa_ctx, EVP_dss1());

	return me;
}


/*
 * "Add" another data block to an existing checker.
 */
void 
x509_add(arg, buffer, length)
	void *arg;
	const char *buffer;
	size_t length;
{
	struct x509_checker * me = arg;

	if (me->has_rsa) EVP_VerifyUpdate(&me->rsa_ctx, buffer, length);
	if (me->has_dsa) EVP_VerifyUpdate(&me->dsa_ctx, buffer, length);
}


/*
 * Finalize an existing checker and verify the signature matches one of the
 * certs in our stack.
 */
int
x509_sign_ok(arg)
	void *arg;
{
	struct x509_checker * n = arg;
	X509 * x509;
	EVP_PKEY * pkey;
	EVP_MD_CTX * md_ctx;
	int status;

	if (verbose)
	    printf("\n\n-------\n\nChecking package signature:\n");

	while ((x509 = sk_X509_pop(n->certs)) != NULL) {
	    /*
	     * Get public key from cert.
	     */
	    pkey = X509_get_pubkey(x509);
	    if (pkey == NULL) {
		warnx("Getting public key:");
		ERR_print_errors_fp(stderr);
		continue;
	    }

	    if (verbose)
		X509_print_fp(stdout, x509);

	    switch (EVP_PKEY_type(pkey->type))
	    {
	    case EVP_PKEY_RSA:
		md_ctx = &n->rsa_ctx;
		break;

	    case EVP_PKEY_DSA:
		md_ctx = &n->dsa_ctx;
		break;

	    default:
	    }

	    status = EVP_VerifyFinal(md_ctx,
				     n->signature->data,
				     n->signature->length,
				     pkey);

	    EVP_PKEY_free(pkey);
	    X509_free(x509);

	    if (status == 1) {
		fprintf(stderr, "X.509 signature matched\n");

		/*
		 * KLUDGE: Does this free the rest of the certs, or just the
		 * stack itself?  Enquiring minds want to know.
		 */
		sk_X509_free(n->certs);
		return PKG_GOODSIG;
	    }
	}

	warnx("Verifying signature:");
	ERR_print_errors_fp(stderr);
	sk_X509_free(n->certs);
	return PKG_BADSIG;
}


/*
 * Sign the specified filename into sign.
 */
int 
retrieve_x509_marker(filename, sign, userid)
	const char * filename;
	struct signature ** sign;
	const char * userid;
{
	struct signature * n;
	struct mygzip_header h;
	FILE * f, * keyf;
	char buffer[1024];
	ssize_t length;
	int err;
	int sig_len = 4096;
	unsigned char * sig_buf;
	EVP_MD_CTX md_ctx;
	EVP_MD * md_type;
	EVP_PKEY * pkey;

	char keyfile[PATH_MAX + 1] = KEYFILE;
	char * kp;

	key_from_name(keyfile, userkey);

	f = fopen(filename, "r");
	if (f == NULL) {
	    free(n);
	    return 0;
	}
	if (gzip_read_header(f, &h, sign) == GZIP_NOT_GZIP) {
	    warnx("File %s is not a gzip file\n", filename);
	    fclose(f);
	    free(n);
	    return 0;
	}

	/*
	 * Sign the remaining data:
	 * Load just the crypto library error strings.
	 */
	ERR_load_crypto_strings();
	
	/*
	 * Read private key.
	 */
	keyf = fopen(keyfile, "r");
	if (keyf == NULL)
	{
	    warnx("Cannot open private key %s.", keyfile);
	    return 0;
	}
	
	pkey = PEM_read_PrivateKey(keyf, NULL, NULL, 0);
	fclose(keyf);
	
	if (pkey == NULL)
	{ 
	    warnx("Reading private key %s:", keyfile);
	    ERR_print_errors_fp(stderr);
	    return 0;
	}
	
	/*
	 * Do the signature.  The remaining bytes of the GZIP file are the
	 * compressed tar image, which is what we are signing.
	 */
	switch (EVP_PKEY_type(pkey->type))
	{
	case EVP_PKEY_RSA:
	    md_type = EVP_sha1();
printf("*** It's an RSA key.\n");
	    break;

	case EVP_PKEY_DSA:
	    md_type = EVP_dss1();
printf("@@@ It's a DSA key, yippee!\n");
	    break;

	default:
	    warnx("Uknown key type");
	    return 0;
	}

	EVP_SignInit(&md_ctx, md_type);

	while ((length = fread(buffer, 1, sizeof buffer, f)) > 0)
		EVP_SignUpdate(&md_ctx, buffer, length);

	sig_buf = malloc(sig_len);
	if (sig_buf == NULL) {
	    warnx("Cannot allocated %u bytes for signature buffer", sig_len);
	    return 0;
	}

	err = EVP_SignFinal(&md_ctx, sig_buf, &sig_len, pkey);
	
	if (err != 1)
	{
	    warnx("Creating signature:");
	    ERR_print_errors_fp(stderr);
	    return 0;
	}
	
	EVP_PKEY_free(pkey);
	
	/*
	 * Stuff the signature onto the head of the chain of signatures in
	 * the package.
	 */
	n = malloc(sizeof *n);
	if (n == NULL) {
	    warnx("Cannot allocate %u bytes for new signature", sizeof *n);
	    return 0;
	}
	n->data = sig_buf;
	n->length = sig_len;
	n->type = TAG_X509;
	memcpy(n->tag, x509tag, sizeof x509tag);
	sign_fill_tag(n);
	n->next = *sign;
	*sign = n;

	/*
	 * Report our success.
	 */
	return 1;
}


static void
key_from_name(char * filename, const char * ident)
{
	char * cp;

	/*
	 * If an alternate keyfile was specified, treat it as the name of an
	 * alternate private key with which to sign or verify the package.
	 */
	if (ident) {
	    printf("Using alternate key/cert \"%s\".\n", ident);
	    if (strchr(ident, '/')) {
		/*
		 * The user specified a path, take it verbatim.
		 */
		strncpy(filename, ident, PATH_MAX);
	    } else {
		cp = dirname(filename);
		if (cp == NULL) {
		    warnx("Key directory not correctly specified.");
		    return;
		}
		snprintf(filename, PATH_MAX, "%s/%s", cp, ident);
	    }
	}

	if (verbose)
	    printf("Key is \"%s\".\n", filename);
}
