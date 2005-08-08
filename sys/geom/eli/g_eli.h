/*-
 * Copyright (c) 2005 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_G_ELI_H_
#define	_G_ELI_H_

#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <crypto/sha2/sha2.h>
#include <opencrypto/cryptodev.h>
#ifdef _KERNEL
#include <sys/bio.h>
#include <sys/libkern.h>
#include <geom/geom.h>
#else
#include <stdio.h>
#include <string.h>
#include <strings.h>
#endif
#ifndef _OpenSSL_
#include <sys/md5.h>
#endif

#define	G_ELI_CLASS_NAME	"ELI"
#define	G_ELI_MAGIC		"GEOM::ELI"
#define	G_ELI_SUFFIX		".eli"

/*
 * Version history:
 * 0 - Initial version number.
 */
#define	G_ELI_VERSION		0

/* Use random, onetime keys. */
#define	G_ELI_FLAG_ONETIME	0x00000001
/* Ask for the passphrase from the kernel, before mounting root. */
#define	G_ELI_FLAG_BOOT		0x00000002
/* Detach on last close, if we were open for writing. */
#define	G_ELI_FLAG_WO_DETACH	0x00000004
/* Detach on last close. */
#define	G_ELI_FLAG_RW_DETACH	0x00000008
/* Provider was open for writing. */
#define	G_ELI_FLAG_WOPEN	0x00010000
/* Destroy device. */
#define	G_ELI_FLAG_DESTROY	0x00020000

#define	SHA512_MDLEN		64

#define	G_ELI_MAXMKEYS		2
#define	G_ELI_MAXKEYLEN		64
#define	G_ELI_USERKEYLEN	G_ELI_MAXKEYLEN
#define	G_ELI_DATAKEYLEN	G_ELI_MAXKEYLEN
#define	G_ELI_IVKEYLEN		G_ELI_MAXKEYLEN
#define	G_ELI_SALTLEN		64
#define	G_ELI_DATAIVKEYLEN	(G_ELI_DATAKEYLEN + G_ELI_IVKEYLEN)
/* Data-Key, IV-Key, HMAC_SHA512(Derived-Key, Data-Key+IV-Key) */
#define	G_ELI_MKEYLEN		(G_ELI_DATAIVKEYLEN + SHA512_MDLEN)

#ifdef _KERNEL
extern u_int g_eli_debug;
extern u_int g_eli_overwrites;

#define	G_ELI_CRYPTO_HW		1
#define	G_ELI_CRYPTO_SW		2

#define	G_ELI_DEBUG(lvl, ...)	do {					\
	if (g_eli_debug >= (lvl)) {					\
		printf("GEOM_ELI");					\
		if (g_eli_debug > 0)					\
			printf("[%u]", lvl);				\
		printf(": ");						\
		printf(__VA_ARGS__);					\
		printf("\n");						\
	}								\
} while (0)
#define	G_ELI_LOGREQ(lvl, bp, ...)	do {				\
	if (g_eli_debug >= (lvl)) {					\
		printf("GEOM_ELI");					\
		if (g_eli_debug > 0)					\
			printf("[%u]", lvl);				\
		printf(": ");						\
		printf(__VA_ARGS__);					\
		printf(" ");						\
		g_print_bio(bp);					\
		printf("\n");						\
	}								\
} while (0)

struct g_eli_worker {
	struct g_eli_softc	*w_softc;
	struct proc		*w_proc;
	u_int			 w_number;
	uint64_t		 w_sid;
	LIST_ENTRY(g_eli_worker) w_next;
};

struct g_eli_softc {
	struct g_geom	*sc_geom;
	u_int		 sc_crypto;
	uint8_t		 sc_datakey[G_ELI_DATAKEYLEN];
	uint8_t		 sc_ivkey[G_ELI_IVKEYLEN];
	SHA256_CTX	 sc_ivctx;
	u_int		 sc_algo;
	u_int		 sc_keylen;
	int		 sc_nkey;
	uint32_t	 sc_flags;

	/* Only for software cryptography. */
	struct bio_queue_head sc_queue;
	struct mtx	 sc_queue_mtx;
	LIST_HEAD(, g_eli_worker) sc_workers;
};
#define	sc_name		 sc_geom->name
#endif	/* _KERNEL */

struct g_eli_metadata {
	char		md_magic[16];	/* Magic value. */
	uint32_t	md_version;	/* Version number. */
	uint32_t	md_flags;	/* Additional flags. */
	uint16_t	md_algo;	/* Encryption algorithm. */
	uint16_t	md_keylen;	/* Key length. */
	uint64_t	md_provsize;	/* Provider's size. */
	uint32_t	md_sectorsize;	/* Sector size. */
	uint8_t		md_keys;	/* Available keys. */
	int32_t		md_iterations;	/* Number of iterations for PKCS#5v2 */
	uint8_t		md_salt[G_ELI_SALTLEN]; /* Salt. */
			/* Encrypted master key (IV-key, Data-key, HMAC). */
	uint8_t		md_mkeys[G_ELI_MAXMKEYS * G_ELI_MKEYLEN];
	u_char		md_hash[16];	/* MD5 hash. */
};
#ifndef _OpenSSL_
static __inline void
eli_metadata_encode(struct g_eli_metadata *md, u_char *data)
{
	MD5_CTX ctx;
	u_char *p;

	p = data;
	bcopy(md->md_magic, p, sizeof(md->md_magic)); p += sizeof(md->md_magic);
	le32enc(p, md->md_version);	p += sizeof(md->md_version);
	le32enc(p, md->md_flags);	p += sizeof(md->md_flags);
	le16enc(p, md->md_algo);	p += sizeof(md->md_algo);
	le16enc(p, md->md_keylen);	p += sizeof(md->md_keylen);
	le64enc(p, md->md_provsize);	p += sizeof(md->md_provsize);
	le32enc(p, md->md_sectorsize);	p += sizeof(md->md_sectorsize);
	*p = md->md_keys;		p += sizeof(md->md_keys);
	le32enc(p, md->md_iterations);	p += sizeof(md->md_iterations);
	bcopy(md->md_salt, p, sizeof(md->md_salt)); p += sizeof(md->md_salt);
	bcopy(md->md_mkeys, p, sizeof(md->md_mkeys)); p += sizeof(md->md_mkeys);
	MD5Init(&ctx);
	MD5Update(&ctx, data, p - data);
	MD5Final(md->md_hash, &ctx);
	bcopy(md->md_hash, p, sizeof(md->md_hash));
}
static __inline int
eli_metadata_decode_v0(const u_char *data, struct g_eli_metadata *md)
{
	MD5_CTX ctx;
	const u_char *p;

	p = data + sizeof(md->md_magic) + sizeof(md->md_version);
	md->md_flags = le32dec(p);	p += sizeof(md->md_flags);
	md->md_algo = le16dec(p);	p += sizeof(md->md_algo);
	md->md_keylen = le16dec(p);	p += sizeof(md->md_keylen);
	md->md_provsize = le64dec(p);	p += sizeof(md->md_provsize);
	md->md_sectorsize = le32dec(p);	p += sizeof(md->md_sectorsize);
	md->md_keys = *p;		p += sizeof(md->md_keys);
	md->md_iterations = le32dec(p);	p += sizeof(md->md_iterations);
	bcopy(p, md->md_salt, sizeof(md->md_salt)); p += sizeof(md->md_salt);
	bcopy(p, md->md_mkeys, sizeof(md->md_mkeys)); p += sizeof(md->md_mkeys);
	MD5Init(&ctx);
	MD5Update(&ctx, data, p - data);
	MD5Final(md->md_hash, &ctx);
	if (bcmp(md->md_hash, p, 16) != 0)
		return (EINVAL);
	return (0);
}
static __inline int
eli_metadata_decode(const u_char *data, struct g_eli_metadata *md)
{
	int error;

	bcopy(data, md->md_magic, sizeof(md->md_magic));
	md->md_version = le32dec(data + sizeof(md->md_magic));
	switch (md->md_version) {
	case 0:
		error = eli_metadata_decode_v0(data, md);
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}
#endif	/* !_OpenSSL */

static __inline u_int
g_eli_str2algo(const char *name)
{

	if (strcasecmp("null", name) == 0)
		return (CRYPTO_NULL_CBC);
	else if (strcasecmp("aes", name) == 0)
		return (CRYPTO_AES_CBC);
	else if (strcasecmp("blowfish", name) == 0)
		return (CRYPTO_BLF_CBC);
	else if (strcasecmp("3des", name) == 0)
		return (CRYPTO_3DES_CBC);
	return (CRYPTO_ALGORITHM_MIN - 1);
}

static __inline const char *
g_eli_algo2str(u_int algo)
{

	switch (algo) {
	case CRYPTO_NULL_CBC:
		return ("NULL");
	case CRYPTO_AES_CBC:
		return ("AES");
	case CRYPTO_BLF_CBC:
		return ("Blowfish");
	case CRYPTO_3DES_CBC:
		return ("3DES");
	}
	return ("unknown");
}

static __inline void
eli_metadata_dump(const struct g_eli_metadata *md)
{
	static const char hex[] = "0123456789abcdef";
	char str[sizeof(md->md_mkeys) * 2 + 1];
	u_int i;

	printf("     magic: %s\n", md->md_magic);
	printf("   version: %u\n", (u_int)md->md_version);
	printf("     flags: 0x%x\n", (u_int)md->md_flags);
	printf("      algo: %s\n", g_eli_algo2str(md->md_algo));
	printf("    keylen: %u\n", (u_int)md->md_keylen);
	printf("  provsize: %ju\n", (uintmax_t)md->md_provsize);
	printf("sectorsize: %u\n", (u_int)md->md_sectorsize);
	printf("      keys: 0x%02x\n", (u_int)md->md_keys);
	printf("iterations: %u\n", (u_int)md->md_iterations);
	bzero(str, sizeof(str));
	for (i = 0; i < sizeof(md->md_salt); i++) {
		str[i * 2] = hex[md->md_salt[i] >> 4];
		str[i * 2 + 1] = hex[md->md_salt[i] & 0x0f];
	}
	printf("        Salt: %s\n", str);
	bzero(str, sizeof(str));
	for (i = 0; i < sizeof(md->md_mkeys); i++) {
		str[i * 2] = hex[md->md_mkeys[i] >> 4];
		str[i * 2 + 1] = hex[md->md_mkeys[i] & 0x0f];
	}
	printf("Master Key: %s\n", str);
	bzero(str, sizeof(str));
	for (i = 0; i < 16; i++) {
		str[i * 2] = hex[md->md_hash[i] >> 4];
		str[i * 2 + 1] = hex[md->md_hash[i] & 0x0f];
	}
	printf("    MD5 hash: %s\n", str);
}

static __inline u_int
g_eli_keylen(u_int algo, u_int keylen)
{

	switch (algo) {
	case CRYPTO_NULL_CBC:
		if (keylen == 0)
			keylen = 64 * 8;
		else {
			if (keylen > 64 * 8)
				keylen = 0;
		}
		return (keylen);
	case CRYPTO_AES_CBC:
		switch (keylen) {
		case 0:
			return (128);
		case 128:
		case 192:
		case 256:
			return (keylen);
		default:
			return (0);
		}
	case CRYPTO_BLF_CBC:
		if (keylen == 0)
			return (128);
		if (keylen < 128 || keylen > 448)
			return (0);
		if ((keylen % 32) != 0)
			return (0);
		return (keylen);
	case CRYPTO_3DES_CBC:
		if (keylen == 0 || keylen == 192)
			return (192);
		return (0);
	default:
		return (0);
	}
}

#ifdef _KERNEL
int g_eli_read_metadata(struct g_class *mp, struct g_provider *pp,
    struct g_eli_metadata *md);
struct g_geom *g_eli_create(struct gctl_req *req, struct g_class *mp,
    struct g_provider *bpp, const struct g_eli_metadata *md,
    const u_char *mkey, int nkey);
int g_eli_destroy(struct g_eli_softc *sc, boolean_t force);

int g_eli_access(struct g_provider *pp, int dr, int dw, int de);
void g_eli_config(struct gctl_req *req, struct g_class *mp, const char *verb);
#endif

void g_eli_mkey_hmac(unsigned char *mkey, const unsigned char *key);
int g_eli_mkey_decrypt(const struct g_eli_metadata *md,
    const unsigned char *key, unsigned char *mkey, unsigned *nkeyp);
int g_eli_mkey_encrypt(unsigned algo, const unsigned char *key, unsigned keylen,
    unsigned char *mkey);

int g_eli_crypto_encrypt(u_int algo, u_char *data, size_t datasize,
    const u_char *key, size_t keysize);
int g_eli_crypto_decrypt(u_int algo, u_char *data, size_t datasize,
    const u_char *key, size_t keysize);

struct hmac_ctx {
	SHA512_CTX	shactx;
	u_char		k_opad[128];
};

void g_eli_crypto_hmac_init(struct hmac_ctx *ctx, const uint8_t *hkey,
    size_t hkeylen);
void g_eli_crypto_hmac_update(struct hmac_ctx *ctx, const uint8_t *data,
    size_t datasize);
void g_eli_crypto_hmac_final(struct hmac_ctx *ctx, uint8_t *md, size_t mdsize);
void g_eli_crypto_hmac(const uint8_t *hkey, size_t hkeysize,
    const uint8_t *data, size_t datasize, uint8_t *md, size_t mdsize);
#endif	/* !_G_ELI_H_ */
