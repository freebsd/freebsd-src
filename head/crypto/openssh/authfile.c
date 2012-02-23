/* $OpenBSD: authfile.c,v 1.92 2011/06/14 22:49:18 markus Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * This file contains functions for reading and writing identity files, and
 * for reading the passphrase from the user.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 *
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/uio.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

/* compatibility with old or broken OpenSSL versions */
#include "openbsd-compat/openssl-compat.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xmalloc.h"
#include "cipher.h"
#include "buffer.h"
#include "key.h"
#include "ssh.h"
#include "log.h"
#include "authfile.h"
#include "rsa.h"
#include "misc.h"
#include "atomicio.h"

#define MAX_KEY_FILE_SIZE	(1024 * 1024)

/* Version identification string for SSH v1 identity files. */
static const char authfile_id_string[] =
    "SSH PRIVATE KEY FILE FORMAT 1.1\n";

/*
 * Serialises the authentication (private) key to a blob, encrypting it with
 * passphrase.  The identification of the blob (lowest 64 bits of n) will
 * precede the key to provide identification of the key without needing a
 * passphrase.
 */
static int
key_private_rsa1_to_blob(Key *key, Buffer *blob, const char *passphrase,
    const char *comment)
{
	Buffer buffer, encrypted;
	u_char buf[100], *cp;
	int i, cipher_num;
	CipherContext ciphercontext;
	Cipher *cipher;
	u_int32_t rnd;

	/*
	 * If the passphrase is empty, use SSH_CIPHER_NONE to ease converting
	 * to another cipher; otherwise use SSH_AUTHFILE_CIPHER.
	 */
	cipher_num = (strcmp(passphrase, "") == 0) ?
	    SSH_CIPHER_NONE : SSH_AUTHFILE_CIPHER;
	if ((cipher = cipher_by_number(cipher_num)) == NULL)
		fatal("save_private_key_rsa: bad cipher");

	/* This buffer is used to built the secret part of the private key. */
	buffer_init(&buffer);

	/* Put checkbytes for checking passphrase validity. */
	rnd = arc4random();
	buf[0] = rnd & 0xff;
	buf[1] = (rnd >> 8) & 0xff;
	buf[2] = buf[0];
	buf[3] = buf[1];
	buffer_append(&buffer, buf, 4);

	/*
	 * Store the private key (n and e will not be stored because they
	 * will be stored in plain text, and storing them also in encrypted
	 * format would just give known plaintext).
	 */
	buffer_put_bignum(&buffer, key->rsa->d);
	buffer_put_bignum(&buffer, key->rsa->iqmp);
	buffer_put_bignum(&buffer, key->rsa->q);	/* reverse from SSL p */
	buffer_put_bignum(&buffer, key->rsa->p);	/* reverse from SSL q */

	/* Pad the part to be encrypted until its size is a multiple of 8. */
	while (buffer_len(&buffer) % 8 != 0)
		buffer_put_char(&buffer, 0);

	/* This buffer will be used to contain the data in the file. */
	buffer_init(&encrypted);

	/* First store keyfile id string. */
	for (i = 0; authfile_id_string[i]; i++)
		buffer_put_char(&encrypted, authfile_id_string[i]);
	buffer_put_char(&encrypted, 0);

	/* Store cipher type. */
	buffer_put_char(&encrypted, cipher_num);
	buffer_put_int(&encrypted, 0);	/* For future extension */

	/* Store public key.  This will be in plain text. */
	buffer_put_int(&encrypted, BN_num_bits(key->rsa->n));
	buffer_put_bignum(&encrypted, key->rsa->n);
	buffer_put_bignum(&encrypted, key->rsa->e);
	buffer_put_cstring(&encrypted, comment);

	/* Allocate space for the private part of the key in the buffer. */
	cp = buffer_append_space(&encrypted, buffer_len(&buffer));

	cipher_set_key_string(&ciphercontext, cipher, passphrase,
	    CIPHER_ENCRYPT);
	cipher_crypt(&ciphercontext, cp,
	    buffer_ptr(&buffer), buffer_len(&buffer));
	cipher_cleanup(&ciphercontext);
	memset(&ciphercontext, 0, sizeof(ciphercontext));

	/* Destroy temporary data. */
	memset(buf, 0, sizeof(buf));
	buffer_free(&buffer);

	buffer_append(blob, buffer_ptr(&encrypted), buffer_len(&encrypted));
	buffer_free(&encrypted);

	return 1;
}

/* convert SSH v2 key in OpenSSL PEM format */
static int
key_private_pem_to_blob(Key *key, Buffer *blob, const char *_passphrase,
    const char *comment)
{
	int success = 0;
	int blen, len = strlen(_passphrase);
	u_char *passphrase = (len > 0) ? (u_char *)_passphrase : NULL;
#if (OPENSSL_VERSION_NUMBER < 0x00907000L)
	const EVP_CIPHER *cipher = (len > 0) ? EVP_des_ede3_cbc() : NULL;
#else
	const EVP_CIPHER *cipher = (len > 0) ? EVP_aes_128_cbc() : NULL;
#endif
	const u_char *bptr;
	BIO *bio;

	if (len > 0 && len <= 4) {
		error("passphrase too short: have %d bytes, need > 4", len);
		return 0;
	}
	if ((bio = BIO_new(BIO_s_mem())) == NULL) {
		error("%s: BIO_new failed", __func__);
		return 0;
	}
	switch (key->type) {
	case KEY_DSA:
		success = PEM_write_bio_DSAPrivateKey(bio, key->dsa,
		    cipher, passphrase, len, NULL, NULL);
		break;
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
		success = PEM_write_bio_ECPrivateKey(bio, key->ecdsa,
		    cipher, passphrase, len, NULL, NULL);
		break;
#endif
	case KEY_RSA:
		success = PEM_write_bio_RSAPrivateKey(bio, key->rsa,
		    cipher, passphrase, len, NULL, NULL);
		break;
	}
	if (success) {
		if ((blen = BIO_get_mem_data(bio, &bptr)) <= 0)
			success = 0;
		else
			buffer_append(blob, bptr, blen);
	}
	BIO_free(bio);
	return success;
}

/* Save a key blob to a file */
static int
key_save_private_blob(Buffer *keybuf, const char *filename)
{
	int fd;

	if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0) {
		error("open %s failed: %s.", filename, strerror(errno));
		return 0;
	}
	if (atomicio(vwrite, fd, buffer_ptr(keybuf),
	    buffer_len(keybuf)) != buffer_len(keybuf)) {
		error("write to key file %s failed: %s", filename,
		    strerror(errno));
		close(fd);
		unlink(filename);
		return 0;
	}
	close(fd);
	return 1;
}

/* Serialise "key" to buffer "blob" */
static int
key_private_to_blob(Key *key, Buffer *blob, const char *passphrase,
    const char *comment)
{
	switch (key->type) {
	case KEY_RSA1:
		return key_private_rsa1_to_blob(key, blob, passphrase, comment);
	case KEY_DSA:
	case KEY_ECDSA:
	case KEY_RSA:
		return key_private_pem_to_blob(key, blob, passphrase, comment);
	default:
		error("%s: cannot save key type %d", __func__, key->type);
		return 0;
	}
}

int
key_save_private(Key *key, const char *filename, const char *passphrase,
    const char *comment)
{
	Buffer keyblob;
	int success = 0;

	buffer_init(&keyblob);
	if (!key_private_to_blob(key, &keyblob, passphrase, comment))
		goto out;
	if (!key_save_private_blob(&keyblob, filename))
		goto out;
	success = 1;
 out:
	buffer_free(&keyblob);
	return success;
}

/*
 * Parse the public, unencrypted portion of a RSA1 key.
 */
static Key *
key_parse_public_rsa1(Buffer *blob, char **commentp)
{
	Key *pub;
	Buffer copy;

	/* Check that it is at least big enough to contain the ID string. */
	if (buffer_len(blob) < sizeof(authfile_id_string)) {
		debug3("Truncated RSA1 identifier");
		return NULL;
	}

	/*
	 * Make sure it begins with the id string.  Consume the id string
	 * from the buffer.
	 */
	if (memcmp(buffer_ptr(blob), authfile_id_string,
	    sizeof(authfile_id_string)) != 0) {
		debug3("Incorrect RSA1 identifier");
		return NULL;
	}
	buffer_init(&copy);
	buffer_append(&copy, buffer_ptr(blob), buffer_len(blob));
	buffer_consume(&copy, sizeof(authfile_id_string));

	/* Skip cipher type and reserved data. */
	(void) buffer_get_char(&copy);		/* cipher type */
	(void) buffer_get_int(&copy);		/* reserved */

	/* Read the public key from the buffer. */
	(void) buffer_get_int(&copy);
	pub = key_new(KEY_RSA1);
	buffer_get_bignum(&copy, pub->rsa->n);
	buffer_get_bignum(&copy, pub->rsa->e);
	if (commentp)
		*commentp = buffer_get_string(&copy, NULL);
	/* The encrypted private part is not parsed by this function. */
	buffer_free(&copy);

	return pub;
}

/* Load a key from a fd into a buffer */
int
key_load_file(int fd, const char *filename, Buffer *blob)
{
	u_char buf[1024];
	size_t len;
	struct stat st;

	if (fstat(fd, &st) < 0) {
		error("%s: fstat of key file %.200s%sfailed: %.100s", __func__,
		    filename == NULL ? "" : filename,
		    filename == NULL ? "" : " ",
		    strerror(errno));
		return 0;
	}
	if ((st.st_mode & (S_IFSOCK|S_IFCHR|S_IFIFO)) == 0 &&
	    st.st_size > MAX_KEY_FILE_SIZE) {
 toobig:
		error("%s: key file %.200s%stoo large", __func__,
		    filename == NULL ? "" : filename,
		    filename == NULL ? "" : " ");
		return 0;
	}
	buffer_init(blob);
	for (;;) {
		if ((len = atomicio(read, fd, buf, sizeof(buf))) == 0) {
			if (errno == EPIPE)
				break;
			debug("%s: read from key file %.200s%sfailed: %.100s",
			    __func__, filename == NULL ? "" : filename,
			    filename == NULL ? "" : " ", strerror(errno));
			buffer_clear(blob);
			bzero(buf, sizeof(buf));
			return 0;
		}
		buffer_append(blob, buf, len);
		if (buffer_len(blob) > MAX_KEY_FILE_SIZE) {
			buffer_clear(blob);
			bzero(buf, sizeof(buf));
			goto toobig;
		}
	}
	bzero(buf, sizeof(buf));
	if ((st.st_mode & (S_IFSOCK|S_IFCHR|S_IFIFO)) == 0 &&
	    st.st_size != buffer_len(blob)) {
		debug("%s: key file %.200s%schanged size while reading",
		    __func__, filename == NULL ? "" : filename,
		    filename == NULL ? "" : " ");
		buffer_clear(blob);
		return 0;
	}

	return 1;
}

/*
 * Loads the public part of the ssh v1 key file.  Returns NULL if an error was
 * encountered (the file does not exist or is not readable), and the key
 * otherwise.
 */
static Key *
key_load_public_rsa1(int fd, const char *filename, char **commentp)
{
	Buffer buffer;
	Key *pub;

	buffer_init(&buffer);
	if (!key_load_file(fd, filename, &buffer)) {
		buffer_free(&buffer);
		return NULL;
	}

	pub = key_parse_public_rsa1(&buffer, commentp);
	if (pub == NULL)
		debug3("Could not load \"%s\" as a RSA1 public key", filename);
	buffer_free(&buffer);
	return pub;
}

/* load public key from private-key file, works only for SSH v1 */
Key *
key_load_public_type(int type, const char *filename, char **commentp)
{
	Key *pub;
	int fd;

	if (type == KEY_RSA1) {
		fd = open(filename, O_RDONLY);
		if (fd < 0)
			return NULL;
		pub = key_load_public_rsa1(fd, filename, commentp);
		close(fd);
		return pub;
	}
	return NULL;
}

static Key *
key_parse_private_rsa1(Buffer *blob, const char *passphrase, char **commentp)
{
	int check1, check2, cipher_type;
	Buffer decrypted;
	u_char *cp;
	CipherContext ciphercontext;
	Cipher *cipher;
	Key *prv = NULL;
	Buffer copy;

	/* Check that it is at least big enough to contain the ID string. */
	if (buffer_len(blob) < sizeof(authfile_id_string)) {
		debug3("Truncated RSA1 identifier");
		return NULL;
	}

	/*
	 * Make sure it begins with the id string.  Consume the id string
	 * from the buffer.
	 */
	if (memcmp(buffer_ptr(blob), authfile_id_string,
	    sizeof(authfile_id_string)) != 0) {
		debug3("Incorrect RSA1 identifier");
		return NULL;
	}
	buffer_init(&copy);
	buffer_append(&copy, buffer_ptr(blob), buffer_len(blob));
	buffer_consume(&copy, sizeof(authfile_id_string));

	/* Read cipher type. */
	cipher_type = buffer_get_char(&copy);
	(void) buffer_get_int(&copy);	/* Reserved data. */

	/* Read the public key from the buffer. */
	(void) buffer_get_int(&copy);
	prv = key_new_private(KEY_RSA1);

	buffer_get_bignum(&copy, prv->rsa->n);
	buffer_get_bignum(&copy, prv->rsa->e);
	if (commentp)
		*commentp = buffer_get_string(&copy, NULL);
	else
		(void)buffer_get_string_ptr(&copy, NULL);

	/* Check that it is a supported cipher. */
	cipher = cipher_by_number(cipher_type);
	if (cipher == NULL) {
		debug("Unsupported RSA1 cipher %d", cipher_type);
		buffer_free(&copy);
		goto fail;
	}
	/* Initialize space for decrypted data. */
	buffer_init(&decrypted);
	cp = buffer_append_space(&decrypted, buffer_len(&copy));

	/* Rest of the buffer is encrypted.  Decrypt it using the passphrase. */
	cipher_set_key_string(&ciphercontext, cipher, passphrase,
	    CIPHER_DECRYPT);
	cipher_crypt(&ciphercontext, cp,
	    buffer_ptr(&copy), buffer_len(&copy));
	cipher_cleanup(&ciphercontext);
	memset(&ciphercontext, 0, sizeof(ciphercontext));
	buffer_free(&copy);

	check1 = buffer_get_char(&decrypted);
	check2 = buffer_get_char(&decrypted);
	if (check1 != buffer_get_char(&decrypted) ||
	    check2 != buffer_get_char(&decrypted)) {
		if (strcmp(passphrase, "") != 0)
			debug("Bad passphrase supplied for RSA1 key");
		/* Bad passphrase. */
		buffer_free(&decrypted);
		goto fail;
	}
	/* Read the rest of the private key. */
	buffer_get_bignum(&decrypted, prv->rsa->d);
	buffer_get_bignum(&decrypted, prv->rsa->iqmp);		/* u */
	/* in SSL and SSH v1 p and q are exchanged */
	buffer_get_bignum(&decrypted, prv->rsa->q);		/* p */
	buffer_get_bignum(&decrypted, prv->rsa->p);		/* q */

	/* calculate p-1 and q-1 */
	rsa_generate_additional_parameters(prv->rsa);

	buffer_free(&decrypted);

	/* enable blinding */
	if (RSA_blinding_on(prv->rsa, NULL) != 1) {
		error("%s: RSA_blinding_on failed", __func__);
		goto fail;
	}
	return prv;

fail:
	if (commentp)
		xfree(*commentp);
	key_free(prv);
	return NULL;
}

static Key *
key_parse_private_pem(Buffer *blob, int type, const char *passphrase,
    char **commentp)
{
	EVP_PKEY *pk = NULL;
	Key *prv = NULL;
	char *name = "<no key>";
	BIO *bio;

	if ((bio = BIO_new_mem_buf(buffer_ptr(blob),
	    buffer_len(blob))) == NULL) {
		error("%s: BIO_new_mem_buf failed", __func__);
		return NULL;
	}
	
	pk = PEM_read_bio_PrivateKey(bio, NULL, NULL, (char *)passphrase);
	BIO_free(bio);
	if (pk == NULL) {
		debug("%s: PEM_read_PrivateKey failed", __func__);
		(void)ERR_get_error();
	} else if (pk->type == EVP_PKEY_RSA &&
	    (type == KEY_UNSPEC||type==KEY_RSA)) {
		prv = key_new(KEY_UNSPEC);
		prv->rsa = EVP_PKEY_get1_RSA(pk);
		prv->type = KEY_RSA;
		name = "rsa w/o comment";
#ifdef DEBUG_PK
		RSA_print_fp(stderr, prv->rsa, 8);
#endif
		if (RSA_blinding_on(prv->rsa, NULL) != 1) {
			error("%s: RSA_blinding_on failed", __func__);
			key_free(prv);
			prv = NULL;
		}
	} else if (pk->type == EVP_PKEY_DSA &&
	    (type == KEY_UNSPEC||type==KEY_DSA)) {
		prv = key_new(KEY_UNSPEC);
		prv->dsa = EVP_PKEY_get1_DSA(pk);
		prv->type = KEY_DSA;
		name = "dsa w/o comment";
#ifdef DEBUG_PK
		DSA_print_fp(stderr, prv->dsa, 8);
#endif
#ifdef OPENSSL_HAS_ECC
	} else if (pk->type == EVP_PKEY_EC &&
	    (type == KEY_UNSPEC||type==KEY_ECDSA)) {
		prv = key_new(KEY_UNSPEC);
		prv->ecdsa = EVP_PKEY_get1_EC_KEY(pk);
		prv->type = KEY_ECDSA;
		if ((prv->ecdsa_nid = key_ecdsa_key_to_nid(prv->ecdsa)) == -1 ||
		    key_curve_nid_to_name(prv->ecdsa_nid) == NULL ||
		    key_ec_validate_public(EC_KEY_get0_group(prv->ecdsa),
		    EC_KEY_get0_public_key(prv->ecdsa)) != 0 ||
		    key_ec_validate_private(prv->ecdsa) != 0) {
			error("%s: bad ECDSA key", __func__);
			key_free(prv);
			prv = NULL;
		}
		name = "ecdsa w/o comment";
#ifdef DEBUG_PK
		if (prv != NULL && prv->ecdsa != NULL)
			key_dump_ec_key(prv->ecdsa);
#endif
#endif /* OPENSSL_HAS_ECC */
	} else {
		error("%s: PEM_read_PrivateKey: mismatch or "
		    "unknown EVP_PKEY save_type %d", __func__, pk->save_type);
	}
	if (pk != NULL)
		EVP_PKEY_free(pk);
	if (prv != NULL && commentp)
		*commentp = xstrdup(name);
	debug("read PEM private key done: type %s",
	    prv ? key_type(prv) : "<unknown>");
	return prv;
}

Key *
key_load_private_pem(int fd, int type, const char *passphrase,
    char **commentp)
{
	Buffer buffer;
	Key *prv;

	buffer_init(&buffer);
	if (!key_load_file(fd, NULL, &buffer)) {
		buffer_free(&buffer);
		return NULL;
	}
	prv = key_parse_private_pem(&buffer, type, passphrase, commentp);
	buffer_free(&buffer);
	return prv;
}

int
key_perm_ok(int fd, const char *filename)
{
	struct stat st;

	if (fstat(fd, &st) < 0)
		return 0;
	/*
	 * if a key owned by the user is accessed, then we check the
	 * permissions of the file. if the key owned by a different user,
	 * then we don't care.
	 */
#ifdef HAVE_CYGWIN
	if (check_ntsec(filename))
#endif
	if ((st.st_uid == getuid()) && (st.st_mode & 077) != 0) {
		error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		error("@         WARNING: UNPROTECTED PRIVATE KEY FILE!          @");
		error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		error("Permissions 0%3.3o for '%s' are too open.",
		    (u_int)st.st_mode & 0777, filename);
		error("It is required that your private key files are NOT accessible by others.");
		error("This private key will be ignored.");
		return 0;
	}
	return 1;
}

static Key *
key_parse_private_type(Buffer *blob, int type, const char *passphrase,
    char **commentp)
{
	switch (type) {
	case KEY_RSA1:
		return key_parse_private_rsa1(blob, passphrase, commentp);
	case KEY_DSA:
	case KEY_ECDSA:
	case KEY_RSA:
	case KEY_UNSPEC:
		return key_parse_private_pem(blob, type, passphrase, commentp);
	default:
		error("%s: cannot parse key type %d", __func__, type);
		break;
	}
	return NULL;
}

Key *
key_load_private_type(int type, const char *filename, const char *passphrase,
    char **commentp, int *perm_ok)
{
	int fd;
	Key *ret;
	Buffer buffer;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		debug("could not open key file '%s': %s", filename,
		    strerror(errno));
		if (perm_ok != NULL)
			*perm_ok = 0;
		return NULL;
	}
	if (!key_perm_ok(fd, filename)) {
		if (perm_ok != NULL)
			*perm_ok = 0;
		error("bad permissions: ignore key: %s", filename);
		close(fd);
		return NULL;
	}
	if (perm_ok != NULL)
		*perm_ok = 1;

	buffer_init(&buffer);
	if (!key_load_file(fd, filename, &buffer)) {
		buffer_free(&buffer);
		close(fd);
		return NULL;
	}
	close(fd);
	ret = key_parse_private_type(&buffer, type, passphrase, commentp);
	buffer_free(&buffer);
	return ret;
}

Key *
key_parse_private(Buffer *buffer, const char *filename,
    const char *passphrase, char **commentp)
{
	Key *pub, *prv;

	/* it's a SSH v1 key if the public key part is readable */
	pub = key_parse_public_rsa1(buffer, commentp);
	if (pub == NULL) {
		prv = key_parse_private_type(buffer, KEY_UNSPEC,
		    passphrase, NULL);
		/* use the filename as a comment for PEM */
		if (commentp && prv)
			*commentp = xstrdup(filename);
	} else {
		key_free(pub);
		/* key_parse_public_rsa1() has already loaded the comment */
		prv = key_parse_private_type(buffer, KEY_RSA1, passphrase,
		    NULL);
	}
	return prv;
}

Key *
key_load_private(const char *filename, const char *passphrase,
    char **commentp)
{
	Key *prv;
	Buffer buffer;
	int fd;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		debug("could not open key file '%s': %s", filename,
		    strerror(errno));
		return NULL;
	}
	if (!key_perm_ok(fd, filename)) {
		error("bad permissions: ignore key: %s", filename);
		close(fd);
		return NULL;
	}

	buffer_init(&buffer);
	if (!key_load_file(fd, filename, &buffer)) {
		buffer_free(&buffer);
		close(fd);
		return NULL;
	}
	close(fd);

	prv = key_parse_private(&buffer, filename, passphrase, commentp);
	buffer_free(&buffer);
	return prv;
}

static int
key_try_load_public(Key *k, const char *filename, char **commentp)
{
	FILE *f;
	char line[SSH_MAX_PUBKEY_BYTES];
	char *cp;
	u_long linenum = 0;

	f = fopen(filename, "r");
	if (f != NULL) {
		while (read_keyfile_line(f, filename, line, sizeof(line),
			    &linenum) != -1) {
			cp = line;
			switch (*cp) {
			case '#':
			case '\n':
			case '\0':
				continue;
			}
			/* Abort loading if this looks like a private key */
			if (strncmp(cp, "-----BEGIN", 10) == 0)
				break;
			/* Skip leading whitespace. */
			for (; *cp && (*cp == ' ' || *cp == '\t'); cp++)
				;
			if (*cp) {
				if (key_read(k, &cp) == 1) {
					cp[strcspn(cp, "\r\n")] = '\0';
					if (commentp) {
						*commentp = xstrdup(*cp ?
						    cp : filename);
					}
					fclose(f);
					return 1;
				}
			}
		}
		fclose(f);
	}
	return 0;
}

/* load public key from ssh v1 private or any pubkey file */
Key *
key_load_public(const char *filename, char **commentp)
{
	Key *pub;
	char file[MAXPATHLEN];

	/* try rsa1 private key */
	pub = key_load_public_type(KEY_RSA1, filename, commentp);
	if (pub != NULL)
		return pub;

	/* try rsa1 public key */
	pub = key_new(KEY_RSA1);
	if (key_try_load_public(pub, filename, commentp) == 1)
		return pub;
	key_free(pub);

	/* try ssh2 public key */
	pub = key_new(KEY_UNSPEC);
	if (key_try_load_public(pub, filename, commentp) == 1)
		return pub;
	if ((strlcpy(file, filename, sizeof file) < sizeof(file)) &&
	    (strlcat(file, ".pub", sizeof file) < sizeof(file)) &&
	    (key_try_load_public(pub, file, commentp) == 1))
		return pub;
	key_free(pub);
	return NULL;
}

/* Load the certificate associated with the named private key */
Key *
key_load_cert(const char *filename)
{
	Key *pub;
	char *file;

	pub = key_new(KEY_UNSPEC);
	xasprintf(&file, "%s-cert.pub", filename);
	if (key_try_load_public(pub, file, NULL) == 1) {
		xfree(file);
		return pub;
	}
	xfree(file);
	key_free(pub);
	return NULL;
}

/* Load private key and certificate */
Key *
key_load_private_cert(int type, const char *filename, const char *passphrase,
    int *perm_ok)
{
	Key *key, *pub;

	switch (type) {
	case KEY_RSA:
	case KEY_DSA:
	case KEY_ECDSA:
		break;
	default:
		error("%s: unsupported key type", __func__);
		return NULL;
	}

	if ((key = key_load_private_type(type, filename, 
	    passphrase, NULL, perm_ok)) == NULL)
		return NULL;

	if ((pub = key_load_cert(filename)) == NULL) {
		key_free(key);
		return NULL;
	}

	/* Make sure the private key matches the certificate */
	if (key_equal_public(key, pub) == 0) {
		error("%s: certificate does not match private key %s",
		    __func__, filename);
	} else if (key_to_certified(key, key_cert_is_legacy(pub)) != 0) {
		error("%s: key_to_certified failed", __func__);
	} else {
		key_cert_copy(pub, key);
		key_free(pub);
		return key;
	}

	key_free(key);
	key_free(pub);
	return NULL;
}

/*
 * Returns 1 if the specified "key" is listed in the file "filename",
 * 0 if the key is not listed or -1 on error.
 * If strict_type is set then the key type must match exactly,
 * otherwise a comparison that ignores certficiate data is performed.
 */
int
key_in_file(Key *key, const char *filename, int strict_type)
{
	FILE *f;
	char line[SSH_MAX_PUBKEY_BYTES];
	char *cp;
	u_long linenum = 0;
	int ret = 0;
	Key *pub;
	int (*key_compare)(const Key *, const Key *) = strict_type ?
	    key_equal : key_equal_public;

	if ((f = fopen(filename, "r")) == NULL) {
		if (errno == ENOENT) {
			debug("%s: keyfile \"%s\" missing", __func__, filename);
			return 0;
		} else {
			error("%s: could not open keyfile \"%s\": %s", __func__,
			    filename, strerror(errno));
			return -1;
		}
	}

	while (read_keyfile_line(f, filename, line, sizeof(line),
		    &linenum) != -1) {
		cp = line;

		/* Skip leading whitespace. */
		for (; *cp && (*cp == ' ' || *cp == '\t'); cp++)
			;

		/* Skip comments and empty lines */
		switch (*cp) {
		case '#':
		case '\n':
		case '\0':
			continue;
		}

		pub = key_new(KEY_UNSPEC);
		if (key_read(pub, &cp) != 1) {
			key_free(pub);
			continue;
		}
		if (key_compare(key, pub)) {
			ret = 1;
			key_free(pub);
			break;
		}
		key_free(pub);
	}
	fclose(f);
	return ret;
}

