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
RCSID("$OpenBSD: authfile.c,v 1.19 2000/09/07 20:27:49 deraadt Exp $");
RCSID("$FreeBSD$");

#include <openssl/bn.h>
#include <openssl/dsa.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/evp.h>

#include "xmalloc.h"
#include "buffer.h"
#include "bufaux.h"
#include "cipher.h"
#include "ssh.h"
#include "key.h"

/* Version identification string for identity files. */
#define AUTHFILE_ID_STRING "SSH PRIVATE KEY FILE FORMAT 1.1\n"

/*
 * Saves the authentication (private) key in a file, encrypting it with
 * passphrase.  The identification of the file (lowest 64 bits of n) will
 * precede the key to provide identification of the key without needing a
 * passphrase.
 */

int
save_private_key_rsa(const char *filename, const char *passphrase,
    RSA *key, const char *comment)
{
	Buffer buffer, encrypted;
	char buf[100], *cp;
	int fd, i;
	CipherContext cipher;
	int cipher_type;
	u_int32_t rand;

	/*
	 * If the passphrase is empty, use SSH_CIPHER_NONE to ease converting
	 * to another cipher; otherwise use SSH_AUTHFILE_CIPHER.
	 */
	if (strcmp(passphrase, "") == 0)
		cipher_type = SSH_CIPHER_NONE;
	else
		cipher_type = SSH_AUTHFILE_CIPHER;

	/* This buffer is used to built the secret part of the private key. */
	buffer_init(&buffer);

	/* Put checkbytes for checking passphrase validity. */
	rand = arc4random();
	buf[0] = rand & 0xff;
	buf[1] = (rand >> 8) & 0xff;
	buf[2] = buf[0];
	buf[3] = buf[1];
	buffer_append(&buffer, buf, 4);

	/*
	 * Store the private key (n and e will not be stored because they
	 * will be stored in plain text, and storing them also in encrypted
	 * format would just give known plaintext).
	 */
	buffer_put_bignum(&buffer, key->d);
	buffer_put_bignum(&buffer, key->iqmp);
	buffer_put_bignum(&buffer, key->q);	/* reverse from SSL p */
	buffer_put_bignum(&buffer, key->p);	/* reverse from SSL q */

	/* Pad the part to be encrypted until its size is a multiple of 8. */
	while (buffer_len(&buffer) % 8 != 0)
		buffer_put_char(&buffer, 0);

	/* This buffer will be used to contain the data in the file. */
	buffer_init(&encrypted);

	/* First store keyfile id string. */
	cp = AUTHFILE_ID_STRING;
	for (i = 0; cp[i]; i++)
		buffer_put_char(&encrypted, cp[i]);
	buffer_put_char(&encrypted, 0);

	/* Store cipher type. */
	buffer_put_char(&encrypted, cipher_type);
	buffer_put_int(&encrypted, 0);	/* For future extension */

	/* Store public key.  This will be in plain text. */
	buffer_put_int(&encrypted, BN_num_bits(key->n));
	buffer_put_bignum(&encrypted, key->n);
	buffer_put_bignum(&encrypted, key->e);
	buffer_put_string(&encrypted, comment, strlen(comment));

	/* Allocate space for the private part of the key in the buffer. */
	buffer_append_space(&encrypted, &cp, buffer_len(&buffer));

	cipher_set_key_string(&cipher, cipher_type, passphrase);
	cipher_encrypt(&cipher, (unsigned char *) cp,
		       (unsigned char *) buffer_ptr(&buffer),
		       buffer_len(&buffer));
	memset(&cipher, 0, sizeof(cipher));

	/* Destroy temporary data. */
	memset(buf, 0, sizeof(buf));
	buffer_free(&buffer);

	fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd < 0)
		return 0;
	if (write(fd, buffer_ptr(&encrypted), buffer_len(&encrypted)) !=
	    buffer_len(&encrypted)) {
		debug("Write to key file %.200s failed: %.100s", filename,
		      strerror(errno));
		buffer_free(&encrypted);
		close(fd);
		remove(filename);
		return 0;
	}
	close(fd);
	buffer_free(&encrypted);
	return 1;
}

/* save DSA key in OpenSSL PEM format */

int
save_private_key_dsa(const char *filename, const char *passphrase,
    DSA *dsa, const char *comment)
{
	FILE *fp;
	int fd;
	int success = 1;
	int len = strlen(passphrase);

	if (len > 0 && len <= 4) {
		error("passphrase too short: %d bytes", len);
		errno = 0;
		return 0;
	}
	fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd < 0) {
		debug("open %s failed", filename);
		return 0;
	}
	fp = fdopen(fd, "w");
	if (fp == NULL ) {
		debug("fdopen %s failed", filename);
		close(fd);
		return 0;
	}
	if (len > 0) {
		if (!PEM_write_DSAPrivateKey(fp, dsa, EVP_des_ede3_cbc(),
		    (char *)passphrase, strlen(passphrase), NULL, NULL))
			success = 0;
	} else {
		if (!PEM_write_DSAPrivateKey(fp, dsa, NULL,
		    NULL, 0, NULL, NULL))
			success = 0;
	}
	fclose(fp);
	return success;
}

int
save_private_key(const char *filename, const char *passphrase, Key *key,
    const char *comment)
{
	switch (key->type) {
	case KEY_RSA:
		return save_private_key_rsa(filename, passphrase, key->rsa, comment);
		break;
	case KEY_DSA:
		return save_private_key_dsa(filename, passphrase, key->dsa, comment);
		break;
	default:
		break;
	}
	return 0;
}

/*
 * Loads the public part of the key file.  Returns 0 if an error was
 * encountered (the file does not exist or is not readable), and non-zero
 * otherwise.
 */

int
load_public_key_rsa(const char *filename, RSA * pub, char **comment_return)
{
	int fd, i;
	off_t len;
	Buffer buffer;
	char *cp;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return 0;
	len = lseek(fd, (off_t) 0, SEEK_END);
	lseek(fd, (off_t) 0, SEEK_SET);

	buffer_init(&buffer);
	buffer_append_space(&buffer, &cp, len);

	if (read(fd, cp, (size_t) len) != (size_t) len) {
		debug("Read from key file %.200s failed: %.100s", filename,
		    strerror(errno));
		buffer_free(&buffer);
		close(fd);
		return 0;
	}
	close(fd);

	/* Check that it is at least big enought to contain the ID string. */
	if (len < strlen(AUTHFILE_ID_STRING) + 1) {
		debug("Bad key file %.200s.", filename);
		buffer_free(&buffer);
		return 0;
	}
	/*
	 * Make sure it begins with the id string.  Consume the id string
	 * from the buffer.
	 */
	for (i = 0; i < (unsigned int) strlen(AUTHFILE_ID_STRING) + 1; i++)
		if (buffer_get_char(&buffer) != (u_char) AUTHFILE_ID_STRING[i]) {
			debug("Bad key file %.200s.", filename);
			buffer_free(&buffer);
			return 0;
		}
	/* Skip cipher type and reserved data. */
	(void) buffer_get_char(&buffer);	/* cipher type */
	(void) buffer_get_int(&buffer);		/* reserved */

	/* Read the public key from the buffer. */
	buffer_get_int(&buffer);
	/* XXX alloc */
	if (pub->n == NULL)
		pub->n = BN_new();
	buffer_get_bignum(&buffer, pub->n);
	/* XXX alloc */
	if (pub->e == NULL)
		pub->e = BN_new();
	buffer_get_bignum(&buffer, pub->e);
	if (comment_return)
		*comment_return = buffer_get_string(&buffer, NULL);
	/* The encrypted private part is not parsed by this function. */

	buffer_free(&buffer);

	return 1;
}

/* load public key from private-key file */
int
load_public_key(const char *filename, Key * key, char **comment_return)
{
	switch (key->type) {
	case KEY_RSA:
		return load_public_key_rsa(filename, key->rsa, comment_return);
		break;
	case KEY_DSA:
	default:
		break;
	}
	return 0;
}

/*
 * Loads the private key from the file.  Returns 0 if an error is encountered
 * (file does not exist or is not readable, or passphrase is bad). This
 * initializes the private key.
 * Assumes we are called under uid of the owner of the file.
 */

int
load_private_key_rsa(int fd, const char *filename,
    const char *passphrase, RSA * prv, char **comment_return)
{
	int i, check1, check2, cipher_type;
	off_t len;
	Buffer buffer, decrypted;
	char *cp;
	CipherContext cipher;
	BN_CTX *ctx;
	BIGNUM *aux;

	len = lseek(fd, (off_t) 0, SEEK_END);
	lseek(fd, (off_t) 0, SEEK_SET);

	buffer_init(&buffer);
	buffer_append_space(&buffer, &cp, len);

	if (read(fd, cp, (size_t) len) != (size_t) len) {
		debug("Read from key file %.200s failed: %.100s", filename,
		      strerror(errno));
		buffer_free(&buffer);
		close(fd);
		return 0;
	}
	close(fd);

	/* Check that it is at least big enought to contain the ID string. */
	if (len < strlen(AUTHFILE_ID_STRING) + 1) {
		debug("Bad key file %.200s.", filename);
		buffer_free(&buffer);
		return 0;
	}
	/*
	 * Make sure it begins with the id string.  Consume the id string
	 * from the buffer.
	 */
	for (i = 0; i < (unsigned int) strlen(AUTHFILE_ID_STRING) + 1; i++)
		if (buffer_get_char(&buffer) != (unsigned char) AUTHFILE_ID_STRING[i]) {
			debug("Bad key file %.200s.", filename);
			buffer_free(&buffer);
			return 0;
		}
	/* Read cipher type. */
	cipher_type = buffer_get_char(&buffer);
	(void) buffer_get_int(&buffer);	/* Reserved data. */

	/* Read the public key from the buffer. */
	buffer_get_int(&buffer);
	prv->n = BN_new();
	buffer_get_bignum(&buffer, prv->n);
	prv->e = BN_new();
	buffer_get_bignum(&buffer, prv->e);
	if (comment_return)
		*comment_return = buffer_get_string(&buffer, NULL);
	else
		xfree(buffer_get_string(&buffer, NULL));

	/* Check that it is a supported cipher. */
	if (((cipher_mask1() | SSH_CIPHER_NONE | SSH_AUTHFILE_CIPHER) &
	     (1 << cipher_type)) == 0) {
		debug("Unsupported cipher %.100s used in key file %.200s.",
		      cipher_name(cipher_type), filename);
		buffer_free(&buffer);
		goto fail;
	}
	/* Initialize space for decrypted data. */
	buffer_init(&decrypted);
	buffer_append_space(&decrypted, &cp, buffer_len(&buffer));

	/* Rest of the buffer is encrypted.  Decrypt it using the passphrase. */
	cipher_set_key_string(&cipher, cipher_type, passphrase);
	cipher_decrypt(&cipher, (unsigned char *) cp,
		       (unsigned char *) buffer_ptr(&buffer),
		       buffer_len(&buffer));

	buffer_free(&buffer);

	check1 = buffer_get_char(&decrypted);
	check2 = buffer_get_char(&decrypted);
	if (check1 != buffer_get_char(&decrypted) ||
	    check2 != buffer_get_char(&decrypted)) {
		if (strcmp(passphrase, "") != 0)
			debug("Bad passphrase supplied for key file %.200s.", filename);
		/* Bad passphrase. */
		buffer_free(&decrypted);
fail:
		BN_clear_free(prv->n);
		prv->n = NULL;
		BN_clear_free(prv->e);
		prv->e = NULL;
		if (comment_return)
			xfree(*comment_return);
		return 0;
	}
	/* Read the rest of the private key. */
	prv->d = BN_new();
	buffer_get_bignum(&decrypted, prv->d);
	prv->iqmp = BN_new();
	buffer_get_bignum(&decrypted, prv->iqmp);	/* u */
	/* in SSL and SSH p and q are exchanged */
	prv->q = BN_new();
	buffer_get_bignum(&decrypted, prv->q);		/* p */
	prv->p = BN_new();
	buffer_get_bignum(&decrypted, prv->p);		/* q */

	ctx = BN_CTX_new();
	aux = BN_new();

	BN_sub(aux, prv->q, BN_value_one());
	prv->dmq1 = BN_new();
	BN_mod(prv->dmq1, prv->d, aux, ctx);

	BN_sub(aux, prv->p, BN_value_one());
	prv->dmp1 = BN_new();
	BN_mod(prv->dmp1, prv->d, aux, ctx);

	BN_clear_free(aux);
	BN_CTX_free(ctx);

	buffer_free(&decrypted);

	return 1;
}

int
load_private_key_dsa(int fd, const char *passphrase, Key *k, char **comment_return)
{
	DSA *dsa;
	BIO *in;
	FILE *fp;

	in = BIO_new(BIO_s_file());
	if (in == NULL) {
		error("BIO_new failed");
		return 0;
	}
	fp = fdopen(fd, "r");
	if (fp == NULL) {
		error("fdopen failed");
		return 0;
	}
	BIO_set_fp(in, fp, BIO_NOCLOSE);
	dsa = PEM_read_bio_DSAPrivateKey(in, NULL, NULL, (char *)passphrase);
	if (dsa == NULL) {
		debug("PEM_read_bio_DSAPrivateKey failed");
	} else {
		/* replace k->dsa with loaded key */
		DSA_free(k->dsa);
		k->dsa = dsa;
	}
	BIO_free(in);
	fclose(fp);
	if (comment_return)
		*comment_return = xstrdup("dsa w/o comment");
	debug("read DSA private key done");
#ifdef DEBUG_DSS
	DSA_print_fp(stderr, dsa, 8);
#endif
	return dsa != NULL ? 1 : 0;
}

int
load_private_key(const char *filename, const char *passphrase, Key *key,
    char **comment_return)
{
	int fd;
	int ret = 0;
	struct stat st;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return 0;

	/* check owner and modes */
	if (fstat(fd, &st) < 0 ||
	    (st.st_uid != 0 && st.st_uid != getuid()) ||
	    (st.st_mode & 077) != 0) {
		close(fd);
		error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		error("@         WARNING: UNPROTECTED PRIVATE KEY FILE!          @");
		error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		error("Bad ownership or mode(0%3.3o) for '%s'.",
		      st.st_mode & 0777, filename);
		error("It is recommended that your private key files are NOT accessible by others.");
		return 0;
	}
	switch (key->type) {
	case KEY_RSA:
		if (key->rsa->e != NULL) {
			BN_clear_free(key->rsa->e);
			key->rsa->e = NULL;
		}
		if (key->rsa->n != NULL) {
			BN_clear_free(key->rsa->n);
			key->rsa->n = NULL;
		}
		ret = load_private_key_rsa(fd, filename, passphrase,
		     key->rsa, comment_return);
		break;
	case KEY_DSA:
		ret = load_private_key_dsa(fd, passphrase, key, comment_return);
	default:
		break;
	}
	close(fd);
	return ret;
}

int
do_load_public_key(const char *filename, Key *k, char **commentp)
{
	FILE *f;
	unsigned int bits;
	char line[1024];
	char *cp;

	f = fopen(filename, "r");
	if (f != NULL) {
		while (fgets(line, sizeof(line), f)) {
			line[sizeof(line)-1] = '\0';
			cp = line;
			switch(*cp){
			case '#':
			case '\n':
			case '\0':
				continue;
			}
			/* Skip leading whitespace. */
			for (; *cp && (*cp == ' ' || *cp == '\t'); cp++)
				;
			if (*cp) {
				bits = key_read(k, &cp);
				if (bits != 0) {
					if (commentp)
						*commentp=xstrdup(filename);
					fclose(f);
					return 1;
				}
			}
		}
		fclose(f);
	}
	return 0;
}

/* load public key from pubkey file */
int
try_load_public_key(const char *filename, Key *k, char **commentp)
{
	char pub[MAXPATHLEN];

	if (do_load_public_key(filename, k, commentp) == 1)
		return 1;
	if (strlcpy(pub, filename, sizeof pub) >= MAXPATHLEN)
		return 0;
	if (strlcat(pub, ".pub", sizeof pub) >= MAXPATHLEN)
		return 0;
	if (do_load_public_key(pub, k, commentp) == 1)
		return 1;
	return 0;
}
