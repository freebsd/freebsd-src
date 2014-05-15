/* $OpenBSD: authfile.c,v 1.103 2014/02/02 03:44:31 djm Exp $ */
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
 * Copyright (c) 2000, 2013 Markus Friedl.  All rights reserved.
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

#include "crypto_api.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_UTIL_H
#include <util.h>
#endif

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
#include "uuencode.h"

/* openssh private key file format */
#define MARK_BEGIN		"-----BEGIN OPENSSH PRIVATE KEY-----\n"
#define MARK_END		"-----END OPENSSH PRIVATE KEY-----\n"
#define KDFNAME			"bcrypt"
#define AUTH_MAGIC		"openssh-key-v1"
#define SALT_LEN		16
#define DEFAULT_CIPHERNAME	"aes256-cbc"
#define	DEFAULT_ROUNDS		16

#define MAX_KEY_FILE_SIZE	(1024 * 1024)

/* Version identification string for SSH v1 identity files. */
static const char authfile_id_string[] =
    "SSH PRIVATE KEY FILE FORMAT 1.1\n";

static int
key_private_to_blob2(Key *prv, Buffer *blob, const char *passphrase,
    const char *comment, const char *ciphername, int rounds)
{
	u_char *key, *cp, salt[SALT_LEN];
	size_t keylen, ivlen, blocksize, authlen;
	u_int len, check;
	int i, n;
	const Cipher *c;
	Buffer encoded, b, kdf;
	CipherContext ctx;
	const char *kdfname = KDFNAME;

	if (rounds <= 0)
		rounds = DEFAULT_ROUNDS;
	if (passphrase == NULL || !strlen(passphrase)) {
		ciphername = "none";
		kdfname = "none";
	} else if (ciphername == NULL)
		ciphername = DEFAULT_CIPHERNAME;
	else if (cipher_number(ciphername) != SSH_CIPHER_SSH2)
		fatal("invalid cipher");

	if ((c = cipher_by_name(ciphername)) == NULL)
		fatal("unknown cipher name");
	buffer_init(&kdf);
	blocksize = cipher_blocksize(c);
	keylen = cipher_keylen(c);
	ivlen = cipher_ivlen(c);
	authlen = cipher_authlen(c);
	key = xcalloc(1, keylen + ivlen);
	if (strcmp(kdfname, "none") != 0) {
		arc4random_buf(salt, SALT_LEN);
		if (bcrypt_pbkdf(passphrase, strlen(passphrase),
		    salt, SALT_LEN, key, keylen + ivlen, rounds) < 0)
			fatal("bcrypt_pbkdf failed");
		buffer_put_string(&kdf, salt, SALT_LEN);
		buffer_put_int(&kdf, rounds);
	}
	cipher_init(&ctx, c, key, keylen, key + keylen , ivlen, 1);
	explicit_bzero(key, keylen + ivlen);
	free(key);

	buffer_init(&encoded);
	buffer_append(&encoded, AUTH_MAGIC, sizeof(AUTH_MAGIC));
	buffer_put_cstring(&encoded, ciphername);
	buffer_put_cstring(&encoded, kdfname);
	buffer_put_string(&encoded, buffer_ptr(&kdf), buffer_len(&kdf));
	buffer_put_int(&encoded, 1);			/* number of keys */
	key_to_blob(prv, &cp, &len);			/* public key */
	buffer_put_string(&encoded, cp, len);

	explicit_bzero(cp, len);
	free(cp);

	buffer_free(&kdf);

	/* set up the buffer that will be encrypted */
	buffer_init(&b);

	/* Random check bytes */
	check = arc4random();
	buffer_put_int(&b, check);
	buffer_put_int(&b, check);

	/* append private key and comment*/
	key_private_serialize(prv, &b);
	buffer_put_cstring(&b, comment);

	/* padding */
	i = 0;
	while (buffer_len(&b) % blocksize)
		buffer_put_char(&b, ++i & 0xff);

	/* length */
	buffer_put_int(&encoded, buffer_len(&b));

	/* encrypt */
	cp = buffer_append_space(&encoded, buffer_len(&b) + authlen);
	if (cipher_crypt(&ctx, 0, cp, buffer_ptr(&b), buffer_len(&b), 0,
	    authlen) != 0)
		fatal("%s: cipher_crypt failed", __func__);
	buffer_free(&b);
	cipher_cleanup(&ctx);

	/* uuencode */
	len = 2 * buffer_len(&encoded);
	cp = xmalloc(len);
	n = uuencode(buffer_ptr(&encoded), buffer_len(&encoded),
	    (char *)cp, len);
	if (n < 0)
		fatal("%s: uuencode", __func__);

	buffer_clear(blob);
	buffer_append(blob, MARK_BEGIN, sizeof(MARK_BEGIN) - 1);
	for (i = 0; i < n; i++) {
		buffer_put_char(blob, cp[i]);
		if (i % 70 == 69)
			buffer_put_char(blob, '\n');
	}
	if (i % 70 != 69)
		buffer_put_char(blob, '\n');
	buffer_append(blob, MARK_END, sizeof(MARK_END) - 1);
	free(cp);

	return buffer_len(blob);
}

static Key *
key_parse_private2(Buffer *blob, int type, const char *passphrase,
    char **commentp)
{
	u_char *key = NULL, *cp, *salt = NULL, pad, last;
	char *comment = NULL, *ciphername = NULL, *kdfname = NULL, *kdfp;
	u_int keylen = 0, ivlen, blocksize, slen, klen, len, rounds, nkeys;
	u_int check1, check2, m1len, m2len;
	size_t authlen;
	const Cipher *c;
	Buffer b, encoded, copy, kdf;
	CipherContext ctx;
	Key *k = NULL;
	int dlen, ret, i;

	buffer_init(&b);
	buffer_init(&kdf);
	buffer_init(&encoded);
	buffer_init(&copy);

	/* uudecode */
	m1len = sizeof(MARK_BEGIN) - 1;
	m2len = sizeof(MARK_END) - 1;
	cp = buffer_ptr(blob);
	len = buffer_len(blob);
	if (len < m1len || memcmp(cp, MARK_BEGIN, m1len)) {
		debug("%s: missing begin marker", __func__);
		goto out;
	}
	cp += m1len;
	len -= m1len;
	while (len) {
		if (*cp != '\n' && *cp != '\r')
			buffer_put_char(&encoded, *cp);
		last = *cp;
		len--;
		cp++;
		if (last == '\n') {
			if (len >= m2len && !memcmp(cp, MARK_END, m2len)) {
				buffer_put_char(&encoded, '\0');
				break;
			}
		}
	}
	if (!len) {
		debug("%s: no end marker", __func__);
		goto out;
	}
	len = buffer_len(&encoded);
	if ((cp = buffer_append_space(&copy, len)) == NULL) {
		error("%s: buffer_append_space", __func__);
		goto out;
	}
	if ((dlen = uudecode(buffer_ptr(&encoded), cp, len)) < 0) {
		error("%s: uudecode failed", __func__);
		goto out;
	}
	if ((u_int)dlen > len) {
		error("%s: crazy uudecode length %d > %u", __func__, dlen, len);
		goto out;
	}
	buffer_consume_end(&copy, len - dlen);
	if (buffer_len(&copy) < sizeof(AUTH_MAGIC) ||
	    memcmp(buffer_ptr(&copy), AUTH_MAGIC, sizeof(AUTH_MAGIC))) {
		error("%s: bad magic", __func__);
		goto out;
	}
	buffer_consume(&copy, sizeof(AUTH_MAGIC));

	ciphername = buffer_get_cstring_ret(&copy, NULL);
	if (ciphername == NULL ||
	    (c = cipher_by_name(ciphername)) == NULL) {
		error("%s: unknown cipher name", __func__);
		goto out;
	}
	if ((passphrase == NULL || !strlen(passphrase)) &&
	    strcmp(ciphername, "none") != 0) {
		/* passphrase required */
		goto out;
	}
	kdfname = buffer_get_cstring_ret(&copy, NULL);
	if (kdfname == NULL ||
	    (!strcmp(kdfname, "none") && !strcmp(kdfname, "bcrypt"))) {
		error("%s: unknown kdf name", __func__);
		goto out;
	}
	if (!strcmp(kdfname, "none") && strcmp(ciphername, "none") != 0) {
		error("%s: cipher %s requires kdf", __func__, ciphername);
		goto out;
	}
	/* kdf options */
	kdfp = buffer_get_string_ptr_ret(&copy, &klen);
	if (kdfp == NULL) {
		error("%s: kdf options not set", __func__);
		goto out;
	}
	if (klen > 0) {
		if ((cp = buffer_append_space(&kdf, klen)) == NULL) {
			error("%s: kdf alloc failed", __func__);
			goto out;
		}
		memcpy(cp, kdfp, klen);
	}
	/* number of keys */
	if (buffer_get_int_ret(&nkeys, &copy) < 0) {
		error("%s: key counter missing", __func__);
		goto out;
	}
	if (nkeys != 1) {
		error("%s: only one key supported", __func__);
		goto out;
	}
	/* pubkey */
	if ((cp = buffer_get_string_ret(&copy, &len)) == NULL) {
		error("%s: pubkey not found", __func__);
		goto out;
	}
	free(cp); /* XXX check pubkey against decrypted private key */

	/* size of encrypted key blob */
	len = buffer_get_int(&copy);
	blocksize = cipher_blocksize(c);
	authlen = cipher_authlen(c);
	if (len < blocksize) {
		error("%s: encrypted data too small", __func__);
		goto out;
	}
	if (len % blocksize) {
		error("%s: length not multiple of blocksize", __func__);
		goto out;
	}

	/* setup key */
	keylen = cipher_keylen(c);
	ivlen = cipher_ivlen(c);
	key = xcalloc(1, keylen + ivlen);
	if (!strcmp(kdfname, "bcrypt")) {
		if ((salt = buffer_get_string_ret(&kdf, &slen)) == NULL) {
			error("%s: salt not set", __func__);
			goto out;
		}
		if (buffer_get_int_ret(&rounds, &kdf) < 0) {
			error("%s: rounds not set", __func__);
			goto out;
		}
		if (bcrypt_pbkdf(passphrase, strlen(passphrase), salt, slen,
		    key, keylen + ivlen, rounds) < 0) {
			error("%s: bcrypt_pbkdf failed", __func__);
			goto out;
		}
	}

	cp = buffer_append_space(&b, len);
	cipher_init(&ctx, c, key, keylen, key + keylen, ivlen, 0);
	ret = cipher_crypt(&ctx, 0, cp, buffer_ptr(&copy), len, 0, authlen);
	cipher_cleanup(&ctx);
	buffer_consume(&copy, len);

	/* fail silently on decryption errors */
	if (ret != 0) {
		debug("%s: decrypt failed", __func__);
		goto out;
	}

	if (buffer_len(&copy) != 0) {
		error("%s: key blob has trailing data (len = %u)", __func__,
		    buffer_len(&copy));
		goto out;
	}

	/* check bytes */
	if (buffer_get_int_ret(&check1, &b) < 0 ||
	    buffer_get_int_ret(&check2, &b) < 0) {
		error("check bytes missing");
		goto out;
	}
	if (check1 != check2) {
		debug("%s: decrypt failed: 0x%08x != 0x%08x", __func__,
		    check1, check2);
		goto out;
	}

	k = key_private_deserialize(&b);

	/* comment */
	comment = buffer_get_cstring_ret(&b, NULL);

	i = 0;
	while (buffer_len(&b)) {
		if (buffer_get_char_ret(&pad, &b) == -1 ||
		    pad != (++i & 0xff)) {
			error("%s: bad padding", __func__);
			key_free(k);
			k = NULL;
			goto out;
		}
	}

	if (k && commentp) {
		*commentp = comment;
		comment = NULL;
	}

	/* XXX decode pubkey and check against private */
 out:
	free(ciphername);
	free(kdfname);
	free(salt);
	free(comment);
	if (key)
		explicit_bzero(key, keylen + ivlen);
	free(key);
	buffer_free(&encoded);
	buffer_free(&copy);
	buffer_free(&kdf);
	buffer_free(&b);
	return k;
}

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
	const Cipher *cipher;
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
	if (cipher_crypt(&ciphercontext, 0, cp,
	    buffer_ptr(&buffer), buffer_len(&buffer), 0, 0) != 0)
		fatal("%s: cipher_crypt failed", __func__);
	cipher_cleanup(&ciphercontext);
	explicit_bzero(&ciphercontext, sizeof(ciphercontext));

	/* Destroy temporary data. */
	explicit_bzero(buf, sizeof(buf));
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
    const char *comment, int force_new_format, const char *new_format_cipher,
    int new_format_rounds)
{
	switch (key->type) {
	case KEY_RSA1:
		return key_private_rsa1_to_blob(key, blob, passphrase, comment);
	case KEY_DSA:
	case KEY_ECDSA:
	case KEY_RSA:
		if (force_new_format) {
			return key_private_to_blob2(key, blob, passphrase,
			    comment, new_format_cipher, new_format_rounds);
		}
		return key_private_pem_to_blob(key, blob, passphrase, comment);
	case KEY_ED25519:
		return key_private_to_blob2(key, blob, passphrase,
		    comment, new_format_cipher, new_format_rounds);
	default:
		error("%s: cannot save key type %d", __func__, key->type);
		return 0;
	}
}

int
key_save_private(Key *key, const char *filename, const char *passphrase,
    const char *comment, int force_new_format, const char *new_format_cipher,
    int new_format_rounds)
{
	Buffer keyblob;
	int success = 0;

	buffer_init(&keyblob);
	if (!key_private_to_blob(key, &keyblob, passphrase, comment,
	    force_new_format, new_format_cipher, new_format_rounds))
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
	buffer_clear(blob);
	for (;;) {
		if ((len = atomicio(read, fd, buf, sizeof(buf))) == 0) {
			if (errno == EPIPE)
				break;
			debug("%s: read from key file %.200s%sfailed: %.100s",
			    __func__, filename == NULL ? "" : filename,
			    filename == NULL ? "" : " ", strerror(errno));
			buffer_clear(blob);
			explicit_bzero(buf, sizeof(buf));
			return 0;
		}
		buffer_append(blob, buf, len);
		if (buffer_len(blob) > MAX_KEY_FILE_SIZE) {
			buffer_clear(blob);
			explicit_bzero(buf, sizeof(buf));
			goto toobig;
		}
	}
	explicit_bzero(buf, sizeof(buf));
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
	const Cipher *cipher;
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
	if (cipher_crypt(&ciphercontext, 0, cp,
	    buffer_ptr(&copy), buffer_len(&copy), 0, 0) != 0)
		fatal("%s: cipher_crypt failed", __func__);
	cipher_cleanup(&ciphercontext);
	explicit_bzero(&ciphercontext, sizeof(ciphercontext));
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
	if (commentp != NULL)
		free(*commentp);
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
	Key *k;

	switch (type) {
	case KEY_RSA1:
		return key_parse_private_rsa1(blob, passphrase, commentp);
	case KEY_DSA:
	case KEY_ECDSA:
	case KEY_RSA:
		return key_parse_private_pem(blob, type, passphrase, commentp);
	case KEY_ED25519:
		return key_parse_private2(blob, type, passphrase, commentp);
	case KEY_UNSPEC:
		if ((k = key_parse_private2(blob, type, passphrase, commentp)))
			return k;
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
		free(file);
		return pub;
	}
	free(file);
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
	case KEY_ED25519:
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
