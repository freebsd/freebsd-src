/*
 * 
 * cipher.c
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Wed Apr 19 17:41:39 1995 ylo
 * 
 */

#include "includes.h"
RCSID("$Id: cipher.c,v 1.19 2000/02/22 15:19:29 markus Exp $");

#include "ssh.h"
#include "cipher.h"

#include <ssl/md5.h>

/*
 * What kind of tripple DES are these 2 routines?
 *
 * Why is there a redundant initialization vector?
 *
 * If only iv3 was used, then, this would till effect have been
 * outer-cbc. However, there is also a private iv1 == iv2 which
 * perhaps makes differential analysis easier. On the other hand, the
 * private iv1 probably makes the CRC-32 attack ineffective. This is a
 * result of that there is no longer any known iv1 to use when
 * choosing the X block.
 */
void
SSH_3CBC_ENCRYPT(des_key_schedule ks1,
		 des_key_schedule ks2, des_cblock * iv2,
		 des_key_schedule ks3, des_cblock * iv3,
		 unsigned char *dest, unsigned char *src,
		 unsigned int len)
{
	des_cblock iv1;

	memcpy(&iv1, iv2, 8);

	des_cbc_encrypt(src, dest, len, ks1, &iv1, DES_ENCRYPT);
	memcpy(&iv1, dest + len - 8, 8);

	des_cbc_encrypt(dest, dest, len, ks2, iv2, DES_DECRYPT);
	memcpy(iv2, &iv1, 8);	/* Note how iv1 == iv2 on entry and exit. */

	des_cbc_encrypt(dest, dest, len, ks3, iv3, DES_ENCRYPT);
	memcpy(iv3, dest + len - 8, 8);
}

void
SSH_3CBC_DECRYPT(des_key_schedule ks1,
		 des_key_schedule ks2, des_cblock * iv2,
		 des_key_schedule ks3, des_cblock * iv3,
		 unsigned char *dest, unsigned char *src,
		 unsigned int len)
{
	des_cblock iv1;

	memcpy(&iv1, iv2, 8);

	des_cbc_encrypt(src, dest, len, ks3, iv3, DES_DECRYPT);
	memcpy(iv3, src + len - 8, 8);

	des_cbc_encrypt(dest, dest, len, ks2, iv2, DES_ENCRYPT);
	memcpy(iv2, dest + len - 8, 8);

	des_cbc_encrypt(dest, dest, len, ks1, &iv1, DES_DECRYPT);
	/* memcpy(&iv1, iv2, 8); */
	/* Note how iv1 == iv2 on entry and exit. */
}

/*
 * SSH uses a variation on Blowfish, all bytes must be swapped before
 * and after encryption/decryption. Thus the swap_bytes stuff (yuk).
 */
static void
swap_bytes(const unsigned char *src, unsigned char *dst_, int n)
{
	/* dst must be properly aligned. */
	u_int32_t *dst = (u_int32_t *) dst_;
	union {
		u_int32_t i;
		char c[4];
	} t;

	/* Process 8 bytes every lap. */
	for (n = n / 8; n > 0; n--) {
		t.c[3] = *src++;
		t.c[2] = *src++;
		t.c[1] = *src++;
		t.c[0] = *src++;
		*dst++ = t.i;

		t.c[3] = *src++;
		t.c[2] = *src++;
		t.c[1] = *src++;
		t.c[0] = *src++;
		*dst++ = t.i;
	}
}

void (*cipher_attack_detected) (const char *fmt,...) = fatal;

static inline void
detect_cbc_attack(const unsigned char *src,
		  unsigned int len)
{
	return;

	log("CRC-32 CBC insertion attack detected");
	cipher_attack_detected("CRC-32 CBC insertion attack detected");
}

/*
 * Names of all encryption algorithms.
 * These must match the numbers defined in cipher.h.
 */
static char *cipher_names[] =
{
	"none",
	"idea",
	"des",
	"3des",
	"tss",
	"rc4",
	"blowfish"
};

/*
 * Returns a bit mask indicating which ciphers are supported by this
 * implementation.  The bit mask has the corresponding bit set of each
 * supported cipher.
 */

unsigned int 
cipher_mask()
{
	unsigned int mask = 0;
	mask |= 1 << SSH_CIPHER_3DES;		/* Mandatory */
	mask |= 1 << SSH_CIPHER_BLOWFISH;
	return mask;
}

/* Returns the name of the cipher. */

const char *
cipher_name(int cipher)
{
	if (cipher < 0 || cipher >= sizeof(cipher_names) / sizeof(cipher_names[0]) ||
	    cipher_names[cipher] == NULL)
		fatal("cipher_name: bad cipher number: %d", cipher);
	return cipher_names[cipher];
}

/*
 * Parses the name of the cipher.  Returns the number of the corresponding
 * cipher, or -1 on error.
 */

int
cipher_number(const char *name)
{
	int i;
	for (i = 0; i < sizeof(cipher_names) / sizeof(cipher_names[0]); i++)
		if (strcmp(cipher_names[i], name) == 0 &&
		    (cipher_mask() & (1 << i)))
			return i;
	return -1;
}

/*
 * Selects the cipher, and keys if by computing the MD5 checksum of the
 * passphrase and using the resulting 16 bytes as the key.
 */

void 
cipher_set_key_string(CipherContext *context, int cipher,
		      const char *passphrase, int for_encryption)
{
	MD5_CTX md;
	unsigned char digest[16];

	MD5_Init(&md);
	MD5_Update(&md, (const unsigned char *) passphrase, strlen(passphrase));
	MD5_Final(digest, &md);

	cipher_set_key(context, cipher, digest, 16, for_encryption);

	memset(digest, 0, sizeof(digest));
	memset(&md, 0, sizeof(md));
}

/* Selects the cipher to use and sets the key. */

void 
cipher_set_key(CipherContext *context, int cipher,
	       const unsigned char *key, int keylen, int for_encryption)
{
	unsigned char padded[32];

	/* Set cipher type. */
	context->type = cipher;

	/* Get 32 bytes of key data.  Pad if necessary.  (So that code
	   below does not need to worry about key size). */
	memset(padded, 0, sizeof(padded));
	memcpy(padded, key, keylen < sizeof(padded) ? keylen : sizeof(padded));

	/* Initialize the initialization vector. */
	switch (cipher) {
	case SSH_CIPHER_NONE:
		/*
		 * Has to stay for authfile saving of private key with no
		 * passphrase
		 */
		break;

	case SSH_CIPHER_3DES:
		/*
		 * Note: the least significant bit of each byte of key is
		 * parity, and must be ignored by the implementation.  16
		 * bytes of key are used (first and last keys are the same).
		 */
		if (keylen < 16)
			error("Key length %d is insufficient for 3DES.", keylen);
		des_set_key((void *) padded, context->u.des3.key1);
		des_set_key((void *) (padded + 8), context->u.des3.key2);
		if (keylen <= 16)
			des_set_key((void *) padded, context->u.des3.key3);
		else
			des_set_key((void *) (padded + 16), context->u.des3.key3);
		memset(context->u.des3.iv2, 0, sizeof(context->u.des3.iv2));
		memset(context->u.des3.iv3, 0, sizeof(context->u.des3.iv3));
		break;

	case SSH_CIPHER_BLOWFISH:
		BF_set_key(&context->u.bf.key, keylen, padded);
		memset(context->u.bf.iv, 0, 8);
		break;

	default:
		fatal("cipher_set_key: unknown cipher: %s", cipher_name(cipher));
	}
	memset(padded, 0, sizeof(padded));
}

/* Encrypts data using the cipher. */

void 
cipher_encrypt(CipherContext *context, unsigned char *dest,
	       const unsigned char *src, unsigned int len)
{
	if ((len & 7) != 0)
		fatal("cipher_encrypt: bad plaintext length %d", len);

	switch (context->type) {
	case SSH_CIPHER_NONE:
		memcpy(dest, src, len);
		break;

	case SSH_CIPHER_3DES:
		SSH_3CBC_ENCRYPT(context->u.des3.key1,
				 context->u.des3.key2, &context->u.des3.iv2,
				 context->u.des3.key3, &context->u.des3.iv3,
				 dest, (unsigned char *) src, len);
		break;

	case SSH_CIPHER_BLOWFISH:
		swap_bytes(src, dest, len);
		BF_cbc_encrypt(dest, dest, len,
		               &context->u.bf.key, context->u.bf.iv,
			       BF_ENCRYPT);
		swap_bytes(dest, dest, len);
		break;

	default:
		fatal("cipher_encrypt: unknown cipher: %s", cipher_name(context->type));
	}
}

/* Decrypts data using the cipher. */

void 
cipher_decrypt(CipherContext *context, unsigned char *dest,
	       const unsigned char *src, unsigned int len)
{
	if ((len & 7) != 0)
		fatal("cipher_decrypt: bad ciphertext length %d", len);

	switch (context->type) {
	case SSH_CIPHER_NONE:
		memcpy(dest, src, len);
		break;

	case SSH_CIPHER_3DES:
		/* CRC-32 attack? */
		SSH_3CBC_DECRYPT(context->u.des3.key1,
				 context->u.des3.key2, &context->u.des3.iv2,
				 context->u.des3.key3, &context->u.des3.iv3,
				 dest, (unsigned char *) src, len);
		break;

	case SSH_CIPHER_BLOWFISH:
		detect_cbc_attack(src, len);
		swap_bytes(src, dest, len);
		BF_cbc_encrypt((void *) dest, dest, len,
			       &context->u.bf.key, context->u.bf.iv,
			       BF_DECRYPT);
		swap_bytes(dest, dest, len);
		break;

	default:
		fatal("cipher_decrypt: unknown cipher: %s", cipher_name(context->type));
	}
}
