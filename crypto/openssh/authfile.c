/*
 * 
 * authfile.c
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Mon Mar 27 03:52:05 1995 ylo
 * 
 * This file contains functions for reading and writing identity files, and
 * for reading the passphrase from the user.
 * 
 */

#include "includes.h"
RCSID("$Id: authfile.c,v 1.11 1999/12/06 19:11:15 deraadt Exp $");

#include <ssl/bn.h>
#include "xmalloc.h"
#include "buffer.h"
#include "bufaux.h"
#include "cipher.h"
#include "ssh.h"

/* Version identification string for identity files. */
#define AUTHFILE_ID_STRING "SSH PRIVATE KEY FILE FORMAT 1.1\n"

/*
 * Saves the authentication (private) key in a file, encrypting it with
 * passphrase.  The identification of the file (lowest 64 bits of n) will
 * precede the key to provide identification of the key without needing a
 * passphrase.
 */

int
save_private_key(const char *filename, const char *passphrase,
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

	cipher_set_key_string(&cipher, cipher_type, passphrase, 1);
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

/*
 * Loads the public part of the key file.  Returns 0 if an error was
 * encountered (the file does not exist or is not readable), and non-zero
 * otherwise.
 */

int
load_public_key(const char *filename, RSA * pub,
		char **comment_return)
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
	pub->n = BN_new();
	buffer_get_bignum(&buffer, pub->n);
	pub->e = BN_new();
	buffer_get_bignum(&buffer, pub->e);
	if (comment_return)
		*comment_return = buffer_get_string(&buffer, NULL);
	/* The encrypted private part is not parsed by this function. */

	buffer_free(&buffer);

	return 1;
}

/*
 * Loads the private key from the file.  Returns 0 if an error is encountered
 * (file does not exist or is not readable, or passphrase is bad). This
 * initializes the private key.
 * Assumes we are called under uid of the owner of the file.
 */

int
load_private_key(const char *filename, const char *passphrase,
		 RSA * prv, char **comment_return)
{
	int fd, i, check1, check2, cipher_type;
	off_t len;
	Buffer buffer, decrypted;
	char *cp;
	CipherContext cipher;
	BN_CTX *ctx;
	BIGNUM *aux;
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
	if (((cipher_mask() | SSH_CIPHER_NONE | SSH_AUTHFILE_CIPHER) &
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
	cipher_set_key_string(&cipher, cipher_type, passphrase, 0);
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
		BN_clear_free(prv->e);
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
