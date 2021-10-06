/*-
 * Copyright (c) 2017 Chelsio Communications, Inc.
 * All rights reserved.
 * Copyright (c) 2021 The FreeBSD Foundation
 * Written by: John Baldwin <jhb@FreeBSD.org>
 *
 * Portions of this software were developed by Ararat River
 * Consulting, LLC under sponsorship of the FreeBSD Foundation.
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
 * SUCH DAMAGE.
 */
/*-
 * Copyright (c) 2004 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD$
 */

/*
 * A different tool for checking hardware crypto support.  Whereas
 * cryptotest is focused on simple performance numbers, this tool is
 * focused on correctness.  For each crypto operation, it performs the
 * operation once in software via OpenSSL and a second time via
 * OpenCrypto and compares the results.
 *
 * cryptocheck [-vz] [-A aad length] [-a algorithm] [-d dev] [-I IV length]
 *	       [size ...]
 *
 * Options:
 *	-v	Verbose.
 *	-z	Run all algorithms on a variety of buffer sizes.
 *
 * Supported algorithms:
 *	all		Run all tests
 *	hash		Run all hash tests
 *	mac		Run all mac tests
 *	cipher		Run all cipher tests
 *	eta		Run all encrypt-then-authenticate tests
 *	aead		Run all authenticated encryption with associated data
 *			tests
 *
 * Hashes:
 *	sha1		SHA-1
 *	sha224		224-bit SHA-2
 *	sha256		256-bit SHA-2
 *	sha384		384-bit SHA-2
 *	sha512		512-bit	SHA-2
 *	blake2b		Blake2-B
 *	blake2s		Blake2-S
 *
 * MACs:
 *	sha1hmac	SHA-1 HMAC
 *	sha224hmac	224-bit SHA-2 HMAC
 *	sha256hmac	256-bit SHA-2 HMAC
 *	sha384hmac	384-bit SHA-2 HMAC
 *	sha512hmac	512-bit	SHA-2 HMAC
 *	gmac		128-bit GMAC
 *	gmac192		192-bit GMAC
 *	gmac256		256-bit GMAC
 *	poly1305
 *
 * Ciphers:
 *	aes-cbc		128-bit AES-CBC
 *	aes-cbc192	192-bit	AES-CBC
 *	aes-cbc256	256-bit AES-CBC
 *	aes-ctr		128-bit AES-CTR
 *	aes-ctr192	192-bit AES-CTR
 *	aes-ctr256	256-bit AES-CTR
 *	aes-xts		128-bit AES-XTS
 *	aes-xts256	256-bit AES-XTS
 *	chacha20
 *
 * Encrypt then Authenticate:
 *	<cipher>+<mac>
 *
 * Authenticated Encryption with Associated Data:
 *	aes-gcm		128-bit AES-GCM
 *	aes-gcm192	192-bit AES-GCM
 *	aes-gcm256	256-bit AES-GCM
 *	aes-ccm		128-bit AES-CCM
 *	aes-ccm192	192-bit AES-CCM
 *	aes-ccm256	256-bit AES-CCM
 *	chacha20-poly1305 Chacha20 with Poly1305 per RFC 8439
 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <libutil.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/hmac.h>

#include <crypto/cryptodev.h>

struct ocf_session {
	int fd;
	int ses;
	int crid;
};

static const struct alg {
	const char *name;
	int cipher;
	int mac;
	enum { T_HASH, T_HMAC, T_GMAC, T_DIGEST, T_CIPHER, T_ETA, T_AEAD } type;
	int key_len;
	int tag_len;
	u_int iv_sizes[8];
	const EVP_CIPHER *(*evp_cipher)(void);
	const EVP_MD *(*evp_md)(void);
	int pkey;
} algs[] = {
	{ .name = "sha1", .mac = CRYPTO_SHA1, .type = T_HASH,
	  .evp_md = EVP_sha1 },
	{ .name = "sha224", .mac = CRYPTO_SHA2_224, .type = T_HASH,
	  .evp_md = EVP_sha224 },
	{ .name = "sha256", .mac = CRYPTO_SHA2_256, .type = T_HASH,
	  .evp_md = EVP_sha256 },
	{ .name = "sha384", .mac = CRYPTO_SHA2_384, .type = T_HASH,
	  .evp_md = EVP_sha384 },
	{ .name = "sha512", .mac = CRYPTO_SHA2_512, .type = T_HASH,
	  .evp_md = EVP_sha512 },
	{ .name = "sha1hmac", .mac = CRYPTO_SHA1_HMAC, .type = T_HMAC,
	  .evp_md = EVP_sha1 },
	{ .name = "sha224hmac", .mac = CRYPTO_SHA2_224_HMAC, .type = T_HMAC,
	  .evp_md = EVP_sha224 },
	{ .name = "sha256hmac", .mac = CRYPTO_SHA2_256_HMAC, .type = T_HMAC,
	  .evp_md = EVP_sha256 },
	{ .name = "sha384hmac", .mac = CRYPTO_SHA2_384_HMAC, .type = T_HMAC,
	  .evp_md = EVP_sha384 },
	{ .name = "sha512hmac", .mac = CRYPTO_SHA2_512_HMAC, .type = T_HMAC,
	  .evp_md = EVP_sha512 },
	{ .name = "blake2b", .mac = CRYPTO_BLAKE2B, .type = T_HASH,
	  .evp_md = EVP_blake2b512 },
	{ .name = "blake2s", .mac = CRYPTO_BLAKE2S, .type = T_HASH,
	  .evp_md = EVP_blake2s256 },
	{ .name = "gmac", .mac = CRYPTO_AES_NIST_GMAC, .type = T_GMAC,
	  .tag_len = AES_GMAC_HASH_LEN, .evp_cipher = EVP_aes_128_gcm },
	{ .name = "gmac192", .mac = CRYPTO_AES_NIST_GMAC, .type = T_GMAC,
	  .tag_len = AES_GMAC_HASH_LEN, .evp_cipher = EVP_aes_192_gcm },
	{ .name = "gmac256", .mac = CRYPTO_AES_NIST_GMAC, .type = T_GMAC,
	  .tag_len = AES_GMAC_HASH_LEN, .evp_cipher = EVP_aes_256_gcm },
	{ .name = "poly1305", .mac = CRYPTO_POLY1305, .type = T_DIGEST,
	  .key_len = POLY1305_KEY_LEN, .pkey = EVP_PKEY_POLY1305 },
	{ .name = "aes-cbc", .cipher = CRYPTO_AES_CBC, .type = T_CIPHER,
	  .evp_cipher = EVP_aes_128_cbc },
	{ .name = "aes-cbc192", .cipher = CRYPTO_AES_CBC, .type = T_CIPHER,
	  .evp_cipher = EVP_aes_192_cbc },
	{ .name = "aes-cbc256", .cipher = CRYPTO_AES_CBC, .type = T_CIPHER,
	  .evp_cipher = EVP_aes_256_cbc },
	{ .name = "aes-ctr", .cipher = CRYPTO_AES_ICM, .type = T_CIPHER,
	  .evp_cipher = EVP_aes_128_ctr },
	{ .name = "aes-ctr192", .cipher = CRYPTO_AES_ICM, .type = T_CIPHER,
	  .evp_cipher = EVP_aes_192_ctr },
	{ .name = "aes-ctr256", .cipher = CRYPTO_AES_ICM, .type = T_CIPHER,
	  .evp_cipher = EVP_aes_256_ctr },
	{ .name = "aes-xts", .cipher = CRYPTO_AES_XTS, .type = T_CIPHER,
	  .evp_cipher = EVP_aes_128_xts },
	{ .name = "aes-xts256", .cipher = CRYPTO_AES_XTS, .type = T_CIPHER,
	  .evp_cipher = EVP_aes_256_xts },
	{ .name = "chacha20", .cipher = CRYPTO_CHACHA20, .type = T_CIPHER,
	  .evp_cipher = EVP_chacha20 },
	{ .name = "aes-gcm", .cipher = CRYPTO_AES_NIST_GCM_16, .type = T_AEAD,
	  .tag_len = AES_GMAC_HASH_LEN, .iv_sizes = { AES_GCM_IV_LEN },
	  .evp_cipher = EVP_aes_128_gcm },
	{ .name = "aes-gcm192", .cipher = CRYPTO_AES_NIST_GCM_16,
	  .type = T_AEAD, .tag_len = AES_GMAC_HASH_LEN,
	  .iv_sizes = { AES_GCM_IV_LEN }, .evp_cipher = EVP_aes_192_gcm },
	{ .name = "aes-gcm256", .cipher = CRYPTO_AES_NIST_GCM_16,
	  .type = T_AEAD, .tag_len = AES_GMAC_HASH_LEN,
	  .iv_sizes = { AES_GCM_IV_LEN }, .evp_cipher = EVP_aes_256_gcm },
	{ .name = "aes-ccm", .cipher = CRYPTO_AES_CCM_16, .type = T_AEAD,
	  .tag_len = AES_CBC_MAC_HASH_LEN, .iv_sizes = { 12, 7, 8, 9, 10, 11, 13 },
	  .evp_cipher = EVP_aes_128_ccm },
	{ .name = "aes-ccm192", .cipher = CRYPTO_AES_CCM_16, .type = T_AEAD,
	  .tag_len = AES_CBC_MAC_HASH_LEN, .iv_sizes = { 12, 7, 8, 9, 10, 11, 13 },
	  .evp_cipher = EVP_aes_192_ccm },
	{ .name = "aes-ccm256", .cipher = CRYPTO_AES_CCM_16, .type = T_AEAD,
	  .tag_len = AES_CBC_MAC_HASH_LEN, .iv_sizes = { 12, 7, 8, 9, 10, 11, 13 },
	  .evp_cipher = EVP_aes_256_ccm },
	{ .name = "chacha20-poly1305", .cipher = CRYPTO_CHACHA20_POLY1305,
	  .type = T_AEAD, .tag_len = POLY1305_HASH_LEN,
	  .iv_sizes = { CHACHA20_POLY1305_IV_LEN, 8 },
	  .evp_cipher = EVP_chacha20_poly1305 },
};

static bool testall, verbose;
static int requested_crid;
static size_t aad_sizes[48], sizes[EALG_MAX_BLOCK_LEN * 2];
static u_int naad_sizes, nsizes;
static u_int iv_size;

static void
usage(void)
{
	fprintf(stderr,
	    "usage: cryptocheck [-vz] [-A aad size] [-a algorithm]\n"
	    "                   [-d dev] [-I IV size] [size ...]\n");
	exit(1);
}

static const struct alg *
find_alg(const char *name)
{
	u_int i;

	for (i = 0; i < nitems(algs); i++)
		if (strcasecmp(algs[i].name, name) == 0)
			return (&algs[i]);
	return (NULL);
}

static struct alg *
build_eta(const struct alg *cipher, const struct alg *mac)
{
	struct alg *eta;
	char *name;

	assert(cipher->type == T_CIPHER);
	assert(mac->type == T_HMAC);
	eta = calloc(1, sizeof(*eta));
	asprintf(&name, "%s+%s", cipher->name, mac->name);
	eta->name = name;
	eta->cipher = cipher->cipher;
	eta->mac = mac->mac;
	eta->type = T_ETA;
	eta->evp_cipher = cipher->evp_cipher;
	eta->evp_md = mac->evp_md;
	return (eta);
}

static void
free_eta(struct alg *eta)
{
	free(__DECONST(char *, eta->name));
	free(eta);
}

static struct alg *
build_eta_name(const char *name)
{
	const struct alg *cipher, *mac;
	const char *mac_name;
	char *cp, *cipher_name;

	cp = strchr(name, '+');
	cipher_name = strndup(name, cp - name);
	mac_name = cp + 1;
	cipher = find_alg(cipher_name);
	free(cipher_name);
	if (cipher == NULL || cipher->type != T_CIPHER)
		errx(1, "Invalid cipher %s", cipher_name);
	mac = find_alg(mac_name);
	if (mac == NULL || mac->type != T_HMAC)
		errx(1, "Invalid hmac %s", mac_name);
	return (build_eta(cipher, mac));
}

static int
devcrypto(void)
{
	static int fd = -1;

	if (fd < 0) {
		fd = open("/dev/crypto", O_RDWR | O_CLOEXEC, 0);
		if (fd < 0)
			err(1, "/dev/crypto");
	}
	return (fd);
}

/*
 * Called on exit to change kern.cryptodevallowsoft back to 0
 */
#define CRYPT_SOFT_ALLOW	"kern.cryptodevallowsoft"

static void
reset_user_soft(void)
{
	int off = 0;
	sysctlbyname(CRYPT_SOFT_ALLOW, NULL, NULL, &off, sizeof(off));
}

static void
enable_user_soft(void)
{
	int curstate;
	int on = 1;
	size_t cursize = sizeof(curstate);

	if (sysctlbyname(CRYPT_SOFT_ALLOW, &curstate, &cursize,
		&on, sizeof(on)) == 0) {
		if (curstate == 0)
			atexit(reset_user_soft);
	}
}

static int
crlookup(const char *devname)
{
	struct crypt_find_op find;

	if (strncmp(devname, "soft", 4) == 0) {
		enable_user_soft();
		return CRYPTO_FLAG_SOFTWARE;
	}

	find.crid = -1;
	strlcpy(find.name, devname, sizeof(find.name));
	if (ioctl(devcrypto(), CIOCFINDDEV, &find) == -1)
		err(1, "ioctl(CIOCFINDDEV)");
	return (find.crid);
}

static const char *
crfind(int crid)
{
	static struct crypt_find_op find;

	if (crid == CRYPTO_FLAG_SOFTWARE)
		return ("soft");
	else if (crid == CRYPTO_FLAG_HARDWARE)
		return ("unknown");

	bzero(&find, sizeof(find));
	find.crid = crid;
	if (ioctl(devcrypto(), CIOCFINDDEV, &find) == -1)
		err(1, "ioctl(CIOCFINDDEV): crid %d", crid);
	return (find.name);
}

static char
rdigit(void)
{
	const char a[] = {
		0x10,0x54,0x11,0x48,0x45,0x12,0x4f,0x13,0x49,0x53,0x14,0x41,
		0x15,0x16,0x4e,0x55,0x54,0x17,0x18,0x4a,0x4f,0x42,0x19,0x01
	};
	return 0x20+a[random()%nitems(a)];
}

static char *
alloc_buffer(size_t len)
{
	char *buf;
	size_t i;

	buf = malloc(len);
	for (i = 0; i < len; i++)
		buf[i] = rdigit();
	return (buf);
}

static char *
generate_iv(size_t len, const struct alg *alg)
{
	char *iv;

	iv = alloc_buffer(len);
	switch (alg->cipher) {
	case CRYPTO_AES_ICM:
		/* Clear the low 32 bits of the IV to hold the counter. */
		iv[len - 4] = 0;
		iv[len - 3] = 0;
		iv[len - 2] = 0;
		iv[len - 1] = 0;
		break;
	case CRYPTO_AES_XTS:
		/*
		 * Clear the low 64-bits to only store a 64-bit block
		 * number.
		 */
		iv[len - 8] = 0;
		iv[len - 7] = 0;
		iv[len - 6] = 0;
		iv[len - 5] = 0;
		iv[len - 4] = 0;
		iv[len - 3] = 0;
		iv[len - 2] = 0;
		iv[len - 1] = 0;
		break;
	}
	return (iv);
}

static void
ocf_init_sop(struct session2_op *sop)
{
	memset(sop, 0, sizeof(*sop));
	sop->crid = requested_crid;
}

static bool
ocf_init_session(struct session2_op *sop, const char *type, const char *name,
    struct ocf_session *ses)
{
	int fd;

	fd = devcrypto();
	if (ioctl(fd, CIOCGSESSION2, sop) < 0) {
		warn("cryptodev %s %s not supported for device %s",
		    type, name, crfind(sop->crid));
		ses->fd = -1;
		return (false);
	}
	ses->fd = fd;
	ses->ses = sop->ses;
	ses->crid = sop->crid;
	return (true);
}

static void
ocf_destroy_session(struct ocf_session *ses)
{
	if (ses->fd == -1)
		return;

	if (ioctl(ses->fd, CIOCFSESSION, &ses->ses) < 0)
		warn("ioctl(CIOCFSESSION)");
}

static void
ocf_init_cop(const struct ocf_session *ses, struct crypt_op *cop)
{
	memset(cop, 0, sizeof(*cop));
	cop->ses = ses->ses;
}

static void
ocf_init_caead(const struct ocf_session *ses, struct crypt_aead *caead)
{
	memset(caead, 0, sizeof(*caead));
	caead->ses = ses->ses;
}

static bool
ocf_hash(const struct alg *alg, const char *buffer, size_t size, char *digest,
    int *cridp)
{
	struct ocf_session ses;
	struct session2_op sop;
	struct crypt_op cop;

	ocf_init_sop(&sop);
	sop.mac = alg->mac;
	if (!ocf_init_session(&sop, "HASH", alg->name, &ses))
		return (false);

	ocf_init_cop(&ses, &cop);
	cop.op = 0;
	cop.len = size;
	cop.src = buffer;
	cop.mac = digest;

	if (ioctl(ses.fd, CIOCCRYPT, &cop) < 0) {
		warn("cryptodev %s (%zu) HASH failed for device %s", alg->name,
		    size, crfind(ses.crid));
		ocf_destroy_session(&ses);
		return (false);
	}

	*cridp = ses.crid;
	ocf_destroy_session(&ses);
	return (true);
}

static void
openssl_hash(const struct alg *alg, const EVP_MD *md, const void *buffer,
    size_t size, void *digest_out, unsigned *digest_sz_out)
{
	EVP_MD_CTX *mdctx;
	const char *errs;
	int rc;

	errs = "";

	mdctx = EVP_MD_CTX_create();
	if (mdctx == NULL)
		goto err_out;

	rc = EVP_DigestInit_ex(mdctx, md, NULL);
	if (rc != 1)
		goto err_out;

	rc = EVP_DigestUpdate(mdctx, buffer, size);
	if (rc != 1)
		goto err_out;

	rc = EVP_DigestFinal_ex(mdctx, digest_out, digest_sz_out);
	if (rc != 1)
		goto err_out;

	EVP_MD_CTX_destroy(mdctx);
	return;

err_out:
	errx(1, "OpenSSL %s HASH failed%s: %s", alg->name, errs,
	    ERR_error_string(ERR_get_error(), NULL));
}

static void
run_hash_test(const struct alg *alg, size_t size)
{
	const EVP_MD *md;
	char *buffer;
	u_int digest_len;
	int crid;
	char control_digest[EVP_MAX_MD_SIZE], test_digest[EVP_MAX_MD_SIZE];

	memset(control_digest, 0x3c, sizeof(control_digest));
	memset(test_digest, 0x3c, sizeof(test_digest));

	md = alg->evp_md();
	assert((size_t)EVP_MD_size(md) <= sizeof(control_digest));

	buffer = alloc_buffer(size);

	/* OpenSSL HASH. */
	digest_len = sizeof(control_digest);
	openssl_hash(alg, md, buffer, size, control_digest, &digest_len);

	/* cryptodev HASH. */
	if (!ocf_hash(alg, buffer, size, test_digest, &crid))
		goto out;
	if (memcmp(control_digest, test_digest, sizeof(control_digest)) != 0) {
		if (memcmp(control_digest, test_digest, EVP_MD_size(md)) == 0)
			printf("%s (%zu) mismatch in trailer:\n",
			    alg->name, size);
		else
			printf("%s (%zu) mismatch:\n", alg->name, size);
		printf("control:\n");
		hexdump(control_digest, sizeof(control_digest), NULL, 0);
		printf("test (cryptodev device %s):\n", crfind(crid));
		hexdump(test_digest, sizeof(test_digest), NULL, 0);
		goto out;
	}

	if (verbose)
		printf("%s (%zu) matched (cryptodev device %s)\n",
		    alg->name, size, crfind(crid));

out:
	free(buffer);
}

static bool
ocf_hmac(const struct alg *alg, const char *buffer, size_t size,
    const char *key, size_t key_len, char *digest, int *cridp)
{
	struct ocf_session ses;
	struct session2_op sop;
	struct crypt_op cop;

	ocf_init_sop(&sop);
	sop.mackeylen = key_len;
	sop.mackey = key;
	sop.mac = alg->mac;
	if (!ocf_init_session(&sop, "HMAC", alg->name, &ses))
		return (false);

	ocf_init_cop(&ses, &cop);
	cop.op = 0;
	cop.len = size;
	cop.src = buffer;
	cop.mac = digest;

	if (ioctl(ses.fd, CIOCCRYPT, &cop) < 0) {
		warn("cryptodev %s (%zu) HMAC failed for device %s", alg->name,
		    size, crfind(ses.crid));
		ocf_destroy_session(&ses);
		return (false);
	}

	*cridp = ses.crid;
	ocf_destroy_session(&ses);
	return (true);
}

static void
run_hmac_test(const struct alg *alg, size_t size)
{
	const EVP_MD *md;
	char *key, *buffer;
	u_int key_len, digest_len;
	int crid;
	char control_digest[EVP_MAX_MD_SIZE], test_digest[EVP_MAX_MD_SIZE];

	memset(control_digest, 0x3c, sizeof(control_digest));
	memset(test_digest, 0x3c, sizeof(test_digest));

	md = alg->evp_md();
	key_len = EVP_MD_size(md);
	assert((size_t)EVP_MD_size(md) <= sizeof(control_digest));

	key = alloc_buffer(key_len);
	buffer = alloc_buffer(size);

	/* OpenSSL HMAC. */
	digest_len = sizeof(control_digest);
	if (HMAC(md, key, key_len, (u_char *)buffer, size,
	    (u_char *)control_digest, &digest_len) == NULL)
		errx(1, "OpenSSL %s (%zu) HMAC failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));

	/* cryptodev HMAC. */
	if (!ocf_hmac(alg, buffer, size, key, key_len, test_digest, &crid))
		goto out;
	if (memcmp(control_digest, test_digest, sizeof(control_digest)) != 0) {
		if (memcmp(control_digest, test_digest, EVP_MD_size(md)) == 0)
			printf("%s (%zu) mismatch in trailer:\n",
			    alg->name, size);
		else
			printf("%s (%zu) mismatch:\n", alg->name, size);
		printf("control:\n");
		hexdump(control_digest, sizeof(control_digest), NULL, 0);
		printf("test (cryptodev device %s):\n", crfind(crid));
		hexdump(test_digest, sizeof(test_digest), NULL, 0);
		goto out;
	}

	if (verbose)
		printf("%s (%zu) matched (cryptodev device %s)\n",
		    alg->name, size, crfind(crid));

out:
	free(buffer);
	free(key);
}

static void
openssl_cipher(const struct alg *alg, const EVP_CIPHER *cipher, const char *key,
    const char *iv, const char *input, char *output, size_t size, int enc)
{
	EVP_CIPHER_CTX *ctx;
	int outl, total;

	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL)
		errx(1, "OpenSSL %s (%zu) ctx new failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	if (EVP_CipherInit_ex(ctx, cipher, NULL, (const u_char *)key,
	    (const u_char *)iv, enc) != 1)
		errx(1, "OpenSSL %s (%zu) ctx init failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	EVP_CIPHER_CTX_set_padding(ctx, 0);
	if (EVP_CipherUpdate(ctx, (u_char *)output, &outl,
	    (const u_char *)input, size) != 1)
		errx(1, "OpenSSL %s (%zu) cipher update failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	total = outl;
	if (EVP_CipherFinal_ex(ctx, (u_char *)output + outl, &outl) != 1)
		errx(1, "OpenSSL %s (%zu) cipher final failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	total += outl;
	if ((size_t)total != size)
		errx(1, "OpenSSL %s (%zu) cipher size mismatch: %d", alg->name,
		    size, total);
	EVP_CIPHER_CTX_free(ctx);
}

static bool
ocf_init_cipher_session(const struct alg *alg, const char *key, size_t key_len,
    struct ocf_session *ses)
{
	struct session2_op sop;

	ocf_init_sop(&sop);
	sop.keylen = key_len;
	sop.key = key;
	sop.cipher = alg->cipher;
	return (ocf_init_session(&sop, "cipher", alg->name, ses));
}

static bool
ocf_cipher(const struct ocf_session *ses, const struct alg *alg, const char *iv,
    const char *input, char *output, size_t size, int op)
{
	struct crypt_op cop;

	ocf_init_cop(ses, &cop);
	cop.op = op;
	cop.len = size;
	cop.src = input;
	cop.dst = output;
	cop.iv = iv;

	if (ioctl(ses->fd, CIOCCRYPT, &cop) < 0) {
		warn("cryptodev %s (%zu) cipher failed for device %s",
		    alg->name, size, crfind(ses->crid));
		return (false);
	}

	return (true);
}

static void
run_cipher_test(const struct alg *alg, size_t size)
{
	struct ocf_session ses;
	const EVP_CIPHER *cipher;
	char *buffer, *cleartext, *ciphertext;
	char *iv, *key;
	u_int iv_len, key_len;

	cipher = alg->evp_cipher();
	if (size % EVP_CIPHER_block_size(cipher) != 0) {
		if (verbose)
			printf(
			    "%s (%zu): invalid buffer size (block size %d)\n",
			    alg->name, size, EVP_CIPHER_block_size(cipher));
		return;
	}

	/*
	 * XTS requires at least one full block so that any partial
	 * block at the end has cipher text to steal.  Hardcoding the
	 * AES block size isn't ideal, but OpenSSL doesn't have a
	 * notion of a "native" block size.
	 */
	if (EVP_CIPHER_mode(cipher) == EVP_CIPH_XTS_MODE &&
	    size < AES_BLOCK_LEN) {
		if (verbose)
			printf("%s (%zu): invalid buffer size\n", alg->name,
			    size);
		return;
	}

	key_len = EVP_CIPHER_key_length(cipher);
	iv_len = EVP_CIPHER_iv_length(cipher);

	key = alloc_buffer(key_len);
	iv = generate_iv(iv_len, alg);
	cleartext = alloc_buffer(size);
	buffer = malloc(size);
	ciphertext = malloc(size);

	/* OpenSSL cipher. */
	openssl_cipher(alg, cipher, key, iv, cleartext, ciphertext, size, 1);
	if (size > 0 && memcmp(cleartext, ciphertext, size) == 0)
		warnx("OpenSSL %s (%zu): cipher text unchanged", alg->name,
		    size);
	openssl_cipher(alg, cipher, key, iv, ciphertext, buffer, size, 0);
	if (memcmp(cleartext, buffer, size) != 0) {
		printf("OpenSSL %s (%zu): cipher mismatch:", alg->name, size);
		printf("original:\n");
		hexdump(cleartext, size, NULL, 0);
		printf("decrypted:\n");
		hexdump(buffer, size, NULL, 0);
		exit(1);
	}

	if (!ocf_init_cipher_session(alg, key, key_len, &ses))
		goto out;

	/* OCF encrypt. */
	if (!ocf_cipher(&ses, alg, iv, cleartext, buffer, size, COP_ENCRYPT))
		goto out;
	if (memcmp(ciphertext, buffer, size) != 0) {
		printf("%s (%zu) encryption mismatch:\n", alg->name, size);
		printf("control:\n");
		hexdump(ciphertext, size, NULL, 0);
		printf("test (cryptodev device %s):\n", crfind(ses.crid));
		hexdump(buffer, size, NULL, 0);
		goto out;
	}

	/* OCF decrypt. */
	if (!ocf_cipher(&ses, alg, iv, ciphertext, buffer, size, COP_DECRYPT))
		goto out;
	if (memcmp(cleartext, buffer, size) != 0) {
		printf("%s (%zu) decryption mismatch:\n", alg->name, size);
		printf("control:\n");
		hexdump(cleartext, size, NULL, 0);
		printf("test (cryptodev device %s):\n", crfind(ses.crid));
		hexdump(buffer, size, NULL, 0);
		goto out;
	}

	if (verbose)
		printf("%s (%zu) matched (cryptodev device %s)\n",
		    alg->name, size, crfind(ses.crid));

out:
	ocf_destroy_session(&ses);
	free(ciphertext);
	free(buffer);
	free(cleartext);
	free(iv);
	free(key);
}

static bool
ocf_init_eta_session(const struct alg *alg, const char *cipher_key,
    size_t cipher_key_len, const char *auth_key, size_t auth_key_len,
    struct ocf_session *ses)
{
	struct session2_op sop;

	ocf_init_sop(&sop);
	sop.keylen = cipher_key_len;
	sop.key = cipher_key;
	sop.cipher = alg->cipher;
	sop.mackeylen = auth_key_len;
	sop.mackey = auth_key;
	sop.mac = alg->mac;
	return (ocf_init_session(&sop, "ETA", alg->name, ses));
}

static int
ocf_eta(const struct ocf_session *ses, const char *iv, size_t iv_len,
    const char *aad, size_t aad_len, const char *input, char *output,
    size_t size, char *digest, int op)
{
	int ret;

	if (aad_len != 0) {
		struct crypt_aead caead;

		ocf_init_caead(ses, &caead);
		caead.op = op;
		caead.len = size;
		caead.aadlen = aad_len;
		caead.ivlen = iv_len;
		caead.src = input;
		caead.dst = output;
		caead.aad = aad;
		caead.tag = digest;
		caead.iv = iv;

		ret = ioctl(ses->fd, CIOCCRYPTAEAD, &caead);
	} else {
		struct crypt_op cop;

		ocf_init_cop(ses, &cop);
		cop.op = op;
		cop.len = size;
		cop.src = input;
		cop.dst = output;
		cop.mac = digest;
		cop.iv = iv;

		ret = ioctl(ses->fd, CIOCCRYPT, &cop);
	}

	if (ret < 0)
		return (errno);
	return (0);
}

static void
run_eta_test(const struct alg *alg, size_t aad_len, size_t size)
{
	struct ocf_session ses;
	const EVP_CIPHER *cipher;
	const EVP_MD *md;
	char *buffer, *cleartext, *ciphertext;
	char *iv, *auth_key, *cipher_key;
	u_int iv_len, auth_key_len, cipher_key_len, digest_len;
	int error;
	char control_digest[EVP_MAX_MD_SIZE], test_digest[EVP_MAX_MD_SIZE];

	cipher = alg->evp_cipher();
	if (size % EVP_CIPHER_block_size(cipher) != 0) {
		if (verbose)
			printf(
		    "%s (%zu, %zu): invalid buffer size (block size %d)\n",
			    alg->name, aad_len, size,
			    EVP_CIPHER_block_size(cipher));
		return;
	}

	/* See comment in run_cipher_test. */
	if (EVP_CIPHER_mode(cipher) == EVP_CIPH_XTS_MODE &&
	    size < AES_BLOCK_LEN) {
		if (verbose)
			printf("%s (%zu): invalid buffer size\n", alg->name,
			    size);
		return;
	}

	memset(control_digest, 0x3c, sizeof(control_digest));
	memset(test_digest, 0x3c, sizeof(test_digest));

	md = alg->evp_md();

	cipher_key_len = EVP_CIPHER_key_length(cipher);
	iv_len = EVP_CIPHER_iv_length(cipher);
	auth_key_len = EVP_MD_size(md);

	cipher_key = alloc_buffer(cipher_key_len);
	iv = generate_iv(iv_len, alg);
	auth_key = alloc_buffer(auth_key_len);
	cleartext = alloc_buffer(aad_len + size);
	buffer = malloc(aad_len + size);
	ciphertext = malloc(aad_len + size);

	/* OpenSSL encrypt + HMAC. */
	if (aad_len != 0)
		memcpy(ciphertext, cleartext, aad_len);
	openssl_cipher(alg, cipher, cipher_key, iv, cleartext + aad_len,
	    ciphertext + aad_len, size, 1);
	if (size > 0 && memcmp(cleartext + aad_len, ciphertext + aad_len,
	    size) == 0)
		warnx("OpenSSL %s (%zu, %zu): cipher text unchanged",
		    alg->name, aad_len, size);
	digest_len = sizeof(control_digest);
	if (HMAC(md, auth_key, auth_key_len, (u_char *)ciphertext,
	    aad_len + size, (u_char *)control_digest, &digest_len) == NULL)
		errx(1, "OpenSSL %s (%zu, %zu) HMAC failed: %s", alg->name,
		    aad_len, size, ERR_error_string(ERR_get_error(), NULL));

	if (!ocf_init_eta_session(alg, cipher_key, cipher_key_len, auth_key,
	    auth_key_len, &ses))
		goto out;

	/* OCF encrypt + HMAC. */
	error = ocf_eta(&ses, iv, iv_len, aad_len != 0 ? cleartext : NULL,
	    aad_len, cleartext + aad_len, buffer + aad_len, size, test_digest,
	    COP_ENCRYPT);
	if (error != 0) {
		warnc(error, "cryptodev %s (%zu, %zu) ETA failed for device %s",
		    alg->name, aad_len, size, crfind(ses.crid));
		goto out;
	}
	if (memcmp(ciphertext + aad_len, buffer + aad_len, size) != 0) {
		printf("%s (%zu, %zu) encryption mismatch:\n", alg->name,
		    aad_len, size);
		printf("control:\n");
		hexdump(ciphertext + aad_len, size, NULL, 0);
		printf("test (cryptodev device %s):\n", crfind(ses.crid));
		hexdump(buffer + aad_len, size, NULL, 0);
		goto out;
	}
	if (memcmp(control_digest, test_digest, sizeof(control_digest)) != 0) {
		if (memcmp(control_digest, test_digest, EVP_MD_size(md)) == 0)
			printf("%s (%zu, %zu) enc hash mismatch in trailer:\n",
			    alg->name, aad_len, size);
		else
			printf("%s (%zu, %zu) enc hash mismatch:\n", alg->name,
			    aad_len, size);
		printf("control:\n");
		hexdump(control_digest, sizeof(control_digest), NULL, 0);
		printf("test (cryptodev device %s):\n", crfind(ses.crid));
		hexdump(test_digest, sizeof(test_digest), NULL, 0);
		goto out;
	}

	/* OCF HMAC + decrypt. */
	error = ocf_eta(&ses, iv, iv_len, aad_len != 0 ? ciphertext : NULL,
	    aad_len, ciphertext + aad_len, buffer + aad_len, size, test_digest,
	    COP_DECRYPT);
	if (error != 0) {
		warnc(error, "cryptodev %s (%zu, %zu) ETA failed for device %s",
		    alg->name, aad_len, size, crfind(ses.crid));
		goto out;
	}
	if (memcmp(cleartext + aad_len, buffer + aad_len, size) != 0) {
		printf("%s (%zu, %zu) decryption mismatch:\n", alg->name,
		    aad_len, size);
		printf("control:\n");
		hexdump(cleartext, size, NULL, 0);
		printf("test (cryptodev device %s):\n", crfind(ses.crid));
		hexdump(buffer, size, NULL, 0);
		goto out;
	}

	/* Verify OCF HMAC + decrypt fails with busted MAC. */
	test_digest[0] ^= 0x1;
	error = ocf_eta(&ses, iv, iv_len, aad_len != 0 ? ciphertext : NULL,
	    aad_len, ciphertext + aad_len, buffer + aad_len, size, test_digest,
	    COP_DECRYPT);
	if (error != EBADMSG) {
		if (error != 0)
			warnc(error,
		    "cryptodev %s (%zu, %zu) corrupt tag failed for device %s",
			    alg->name, aad_len, size, crfind(ses.crid));
		else
			warnx(
	    "cryptodev %s (%zu, %zu) corrupt tag didn't fail for device %s",
			    alg->name, aad_len, size, crfind(ses.crid));
		goto out;
	}

	if (verbose)
		printf("%s (%zu, %zu) matched (cryptodev device %s)\n",
		    alg->name, aad_len, size, crfind(ses.crid));

out:
	ocf_destroy_session(&ses);
	free(ciphertext);
	free(buffer);
	free(cleartext);
	free(auth_key);
	free(iv);
	free(cipher_key);
}

static void
openssl_gmac(const struct alg *alg, const EVP_CIPHER *cipher, const char *key,
    const char *iv, const char *input, size_t size, char *tag)
{
	EVP_CIPHER_CTX *ctx;
	int outl;

	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL)
		errx(1, "OpenSSL %s (%zu) ctx new failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	if (EVP_EncryptInit_ex(ctx, cipher, NULL, (const u_char *)key,
	    (const u_char *)iv) != 1)
		errx(1, "OpenSSL %s (%zu) ctx init failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	EVP_CIPHER_CTX_set_padding(ctx, 0);
	if (EVP_EncryptUpdate(ctx, NULL, &outl, (const u_char *)input,
		size) != 1)
		errx(1, "OpenSSL %s (%zu) update failed: %s",
		    alg->name, size, ERR_error_string(ERR_get_error(), NULL));
	if (EVP_EncryptFinal_ex(ctx, NULL, &outl) != 1)
		errx(1, "OpenSSL %s (%zu) final failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, alg->tag_len,
	    tag) != 1)
		errx(1, "OpenSSL %s (%zu) get tag failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	EVP_CIPHER_CTX_free(ctx);
}

static bool
ocf_mac(const struct alg *alg, const char *input, size_t size, const char *key,
    size_t key_len, const char *iv, char *tag, int *cridp)
{
	struct ocf_session ses;
	struct session2_op sop;
	struct crypt_op cop;

	ocf_init_sop(&sop);
	sop.mackeylen = key_len;
	sop.mackey = key;
	sop.mac = alg->mac;
	if (!ocf_init_session(&sop, "MAC", alg->name, &ses))
		return (false);

	ocf_init_cop(&ses, &cop);
	cop.op = 0;
	cop.len = size;
	cop.src = input;
	cop.mac = tag;
	cop.iv = iv;

	if (ioctl(ses.fd, CIOCCRYPT, &cop) < 0) {
		warn("cryptodev %s (%zu) failed for device %s", alg->name,
		    size, crfind(ses.crid));
		ocf_destroy_session(&ses);
		return (false);
	}

	*cridp = ses.crid;
	ocf_destroy_session(&ses);
	return (true);
}

static void
run_gmac_test(const struct alg *alg, size_t size)
{
	const EVP_CIPHER *cipher;
	char *iv, *key, *buffer;
	u_int iv_len, key_len;
	int crid;
	char control_tag[AES_GMAC_HASH_LEN], test_tag[AES_GMAC_HASH_LEN];

	cipher = alg->evp_cipher();

	memset(control_tag, 0x3c, sizeof(control_tag));
	memset(test_tag, 0x3c, sizeof(test_tag));

	key_len = EVP_CIPHER_key_length(cipher);
	iv_len = EVP_CIPHER_iv_length(cipher);

	key = alloc_buffer(key_len);
	iv = generate_iv(iv_len, alg);
	buffer = alloc_buffer(size);

	/* OpenSSL GMAC. */
	openssl_gmac(alg, cipher, key, iv, buffer, size, control_tag);

	/* OCF GMAC. */
	if (!ocf_mac(alg, buffer, size, key, key_len, iv, test_tag, &crid))
		goto out;
	if (memcmp(control_tag, test_tag, sizeof(control_tag)) != 0) {
		printf("%s (%zu) mismatch:\n", alg->name, size);
		printf("control:\n");
		hexdump(control_tag, sizeof(control_tag), NULL, 0);
		printf("test (cryptodev device %s):\n", crfind(crid));
		hexdump(test_tag, sizeof(test_tag), NULL, 0);
		goto out;
	}

	if (verbose)
		printf("%s (%zu) matched (cryptodev device %s)\n",
		    alg->name, size, crfind(crid));

out:
	free(buffer);
	free(iv);
	free(key);
}

static void
openssl_digest(const struct alg *alg, const char *key, u_int key_len,
    const char *input, size_t size, char *tag, u_int tag_len)
{
	EVP_MD_CTX *mdctx;
	EVP_PKEY *pkey;
	size_t len;

	pkey = EVP_PKEY_new_raw_private_key(alg->pkey, NULL, key, key_len);
	if (pkey == NULL)
		errx(1, "OpenSSL %s (%zu) pkey new failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	mdctx = EVP_MD_CTX_new();
	if (mdctx == NULL)
		errx(1, "OpenSSL %s (%zu) ctx new failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	if (EVP_DigestSignInit(mdctx, NULL, NULL, NULL, pkey) != 1)
		errx(1, "OpenSSL %s (%zu) digest sign init failed: %s",
		    alg->name, size, ERR_error_string(ERR_get_error(), NULL));
	if (EVP_DigestSignUpdate(mdctx, input, size) != 1)
		errx(1, "OpenSSL %s (%zu) digest update failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	len = tag_len;
	if (EVP_DigestSignFinal(mdctx, tag, &len) != 1)
		errx(1, "OpenSSL %s (%zu) digest final failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	EVP_MD_CTX_free(mdctx);
	EVP_PKEY_free(pkey);
}

static void
run_digest_test(const struct alg *alg, size_t size)
{
	char *key, *buffer;
	u_int key_len;
	int crid;
	char control_tag[EVP_MAX_MD_SIZE], test_tag[EVP_MAX_MD_SIZE];

	memset(control_tag, 0x3c, sizeof(control_tag));
	memset(test_tag, 0x3c, sizeof(test_tag));

	key_len = alg->key_len;

	key = alloc_buffer(key_len);
	buffer = alloc_buffer(size);

	/* OpenSSL Poly1305. */
	openssl_digest(alg, key, key_len, buffer, size, control_tag,
	    sizeof(control_tag));

	/* OCF Poly1305. */
	if (!ocf_mac(alg, buffer, size, key, key_len, NULL, test_tag, &crid))
		goto out;
	if (memcmp(control_tag, test_tag, sizeof(control_tag)) != 0) {
		printf("%s (%zu) mismatch:\n", alg->name, size);
		printf("control:\n");
		hexdump(control_tag, sizeof(control_tag), NULL, 0);
		printf("test (cryptodev device %s):\n", crfind(crid));
		hexdump(test_tag, sizeof(test_tag), NULL, 0);
		goto out;
	}

	if (verbose)
		printf("%s (%zu) matched (cryptodev device %s)\n",
		    alg->name, size, crfind(crid));

out:
	free(buffer);
	free(key);
}

static void
openssl_aead_encrypt(const struct alg *alg, const EVP_CIPHER *cipher,
    const char *key, const char *iv, size_t iv_len, const char *aad,
    size_t aad_len, const char *input, char *output, size_t size, char *tag)
{
	EVP_CIPHER_CTX *ctx;
	int outl, total;

	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL)
		errx(1, "OpenSSL %s (%zu) ctx new failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	if (EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1)
		errx(1, "OpenSSL %s (%zu) ctx init failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, iv_len, NULL) != 1)
		errx(1, "OpenSSL %s (%zu) setting iv length failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	if (EVP_EncryptInit_ex(ctx, NULL, NULL, (const u_char *)key,
	    (const u_char *)iv) != 1)
		errx(1, "OpenSSL %s (%zu) ctx init failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	EVP_CIPHER_CTX_set_padding(ctx, 0);
	if (aad != NULL) {
		if (EVP_EncryptUpdate(ctx, NULL, &outl, (const u_char *)aad,
		    aad_len) != 1)
			errx(1, "OpenSSL %s (%zu) aad update failed: %s",
			    alg->name, size,
			    ERR_error_string(ERR_get_error(), NULL));
	}
	if (EVP_EncryptUpdate(ctx, (u_char *)output, &outl,
	    (const u_char *)input, size) != 1)
		errx(1, "OpenSSL %s (%zu) encrypt update failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	total = outl;
	if (EVP_EncryptFinal_ex(ctx, (u_char *)output + outl, &outl) != 1)
		errx(1, "OpenSSL %s (%zu) encrypt final failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	total += outl;
	if ((size_t)total != size)
		errx(1, "OpenSSL %s (%zu) encrypt size mismatch: %d", alg->name,
		    size, total);
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, alg->tag_len,
	    tag) != 1)
		errx(1, "OpenSSL %s (%zu) get tag failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	EVP_CIPHER_CTX_free(ctx);
}

#ifdef notused
static bool
openssl_aead_decrypt(const struct alg *alg, const EVP_CIPHER *cipher,
    const char *key, const char *iv, const char *aad, size_t aad_len,
    const char *input, char *output, size_t size, char *tag)
{
	EVP_CIPHER_CTX *ctx;
	int outl, total;
	bool valid;

	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL)
		errx(1, "OpenSSL %s (%zu) ctx new failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	if (EVP_DecryptInit_ex(ctx, cipher, NULL, (const u_char *)key,
	    (const u_char *)iv) != 1)
		errx(1, "OpenSSL %s (%zu) ctx init failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	EVP_CIPHER_CTX_set_padding(ctx, 0);
	if (aad != NULL) {
		if (EVP_DecryptUpdate(ctx, NULL, &outl, (const u_char *)aad,
		    aad_len) != 1)
			errx(1, "OpenSSL %s (%zu) aad update failed: %s",
			    alg->name, size,
			    ERR_error_string(ERR_get_error(), NULL));
	}
	if (EVP_DecryptUpdate(ctx, (u_char *)output, &outl,
	    (const u_char *)input, size) != 1)
		errx(1, "OpenSSL %s (%zu) decrypt update failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	total = outl;
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, alg->tag_len,
	    tag) != 1)
		errx(1, "OpenSSL %s (%zu) get tag failed: %s", alg->name,
		    size, ERR_error_string(ERR_get_error(), NULL));
	valid = (EVP_DecryptFinal_ex(ctx, (u_char *)output + outl, &outl) != 1);
	total += outl;
	if (total != size)
		errx(1, "OpenSSL %s (%zu) decrypt size mismatch: %d", alg->name,
		    size, total);
	EVP_CIPHER_CTX_free(ctx);
	return (valid);
}
#endif

static void
openssl_ccm_encrypt(const struct alg *alg, const EVP_CIPHER *cipher,
    const char *key, const char *iv, size_t iv_len, const char *aad,
    size_t aad_len, const char *input, char *output, size_t size, char *tag)
{
	EVP_CIPHER_CTX *ctx;
	int outl, total;

	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL)
		errx(1, "OpenSSL %s/%zu (%zu, %zu) ctx new failed: %s",
		    alg->name, iv_len, aad_len, size,
		    ERR_error_string(ERR_get_error(), NULL));
	if (EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1)
		errx(1, "OpenSSL %s/%zu (%zu, %zu) ctx init failed: %s",
		    alg->name, iv_len, aad_len, size,
		    ERR_error_string(ERR_get_error(), NULL));
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, iv_len, NULL) != 1)
		errx(1,
		    "OpenSSL %s/%zu (%zu, %zu) setting iv length failed: %s",
		    alg->name, iv_len, aad_len, size,
		    ERR_error_string(ERR_get_error(), NULL));
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, AES_CBC_MAC_HASH_LEN, NULL) != 1)
		errx(1,
		    "OpenSSL %s/%zu (%zu, %zu) setting tag length failed: %s",
		    alg->name, iv_len, aad_len, size,
		    ERR_error_string(ERR_get_error(), NULL));
	if (EVP_EncryptInit_ex(ctx, NULL, NULL, (const u_char *)key,
	    (const u_char *)iv) != 1)
		errx(1, "OpenSSL %s/%zu (%zu, %zu) ctx init failed: %s",
		    alg->name, iv_len, aad_len, size,
		    ERR_error_string(ERR_get_error(), NULL));
	if (EVP_EncryptUpdate(ctx, NULL, &outl, NULL, size) != 1)
		errx(1,
		    "OpenSSL %s/%zu (%zu, %zu) unable to set data length: %s",
		    alg->name, iv_len, aad_len, size,
		    ERR_error_string(ERR_get_error(), NULL));

	if (aad != NULL) {
		if (EVP_EncryptUpdate(ctx, NULL, &outl, (const u_char *)aad,
		    aad_len) != 1)
			errx(1,
			    "OpenSSL %s/%zu (%zu, %zu) aad update failed: %s",
			    alg->name, iv_len, aad_len, size,
			    ERR_error_string(ERR_get_error(), NULL));
	}
	if (EVP_EncryptUpdate(ctx, (u_char *)output, &outl,
	    (const u_char *)input, size) != 1)
		errx(1, "OpenSSL %s/%zu (%zu, %zu) encrypt update failed: %s",
		    alg->name, iv_len, aad_len, size,
		    ERR_error_string(ERR_get_error(), NULL));
	total = outl;
	if (EVP_EncryptFinal_ex(ctx, (u_char *)output + outl, &outl) != 1)
		errx(1, "OpenSSL %s/%zu (%zu, %zu) encrypt final failed: %s",
		    alg->name, iv_len, aad_len, size,
		    ERR_error_string(ERR_get_error(), NULL));
	total += outl;
	if ((size_t)total != size)
		errx(1, "OpenSSL %s/%zu (%zu, %zu) encrypt size mismatch: %d",
		    alg->name, iv_len, aad_len, size, total);
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, AES_CBC_MAC_HASH_LEN,
	    tag) != 1)
		errx(1, "OpenSSL %s/%zu (%zu, %zu) get tag failed: %s",
		    alg->name, iv_len, aad_len, size,
		    ERR_error_string(ERR_get_error(), NULL));
	EVP_CIPHER_CTX_free(ctx);
}

static bool
ocf_init_aead_session(const struct alg *alg, const char *key, size_t key_len,
    size_t iv_len, struct ocf_session *ses)
{
	struct session2_op sop;

	ocf_init_sop(&sop);
	sop.keylen = key_len;
	sop.key = key;
	sop.cipher = alg->cipher;
	sop.ivlen = iv_len;
	return (ocf_init_session(&sop, "AEAD", alg->name, ses));
}

static int
ocf_aead(const struct ocf_session *ses, const char *iv, size_t iv_len,
    const char *aad, size_t aad_len, const char *input, char *output,
    size_t size, char *tag, int op)
{
	struct crypt_aead caead;

	ocf_init_caead(ses, &caead);
	caead.op = op;
	caead.len = size;
	caead.aadlen = aad_len;
	caead.ivlen = iv_len;
	caead.src = input;
	caead.dst = output;
	caead.aad = aad;
	caead.tag = tag;
	caead.iv = iv;

	if (ioctl(ses->fd, CIOCCRYPTAEAD, &caead) < 0)
		return (errno);
	return (0);
}

#define	AEAD_MAX_TAG_LEN				\
	MAX(MAX(AES_GMAC_HASH_LEN, AES_CBC_MAC_HASH_LEN), POLY1305_HASH_LEN)

static size_t
max_ccm_buffer_length(size_t iv_len)
{
	const u_int L = 15 - iv_len;

	switch (L) {
	case 2:
		return (0xffff);
	case 3:
		return (0xffffff);
#ifdef __LP64__
	case 4:
		return (0xffffffff);
	case 5:
		return (0xffffffffff);
	case 6:
		return (0xffffffffffff);
	case 7:
		return (0xffffffffffffff);
	default:
		return (0xffffffffffffffff);
#else
	default:
		return (0xffffffff);
#endif
	}
}

static void
run_aead_test(const struct alg *alg, size_t aad_len, size_t size,
    size_t iv_len)
{
	struct ocf_session ses;
	const EVP_CIPHER *cipher;
	char *aad, *buffer, *cleartext, *ciphertext;
	char *iv, *key;
	u_int key_len;
	int error;
	char control_tag[AEAD_MAX_TAG_LEN], test_tag[AEAD_MAX_TAG_LEN];

	cipher = alg->evp_cipher();
	if (size % EVP_CIPHER_block_size(cipher) != 0) {
		if (verbose)
			printf(
		    "%s/%zu (%zu, %zu): invalid buffer size (block size %d)\n",
			    alg->name, iv_len, aad_len, size,
			    EVP_CIPHER_block_size(cipher));
		return;
	}

	if (EVP_CIPHER_mode(cipher) == EVP_CIPH_CCM_MODE &&
	    size > max_ccm_buffer_length(iv_len)) {
		if (verbose)
			printf("%s/%zu (%zu, %zu): invalid buffer size\n",
			    alg->name, iv_len, aad_len, size);
		return;
	}

	memset(control_tag, 0x3c, sizeof(control_tag));
	memset(test_tag, 0x3c, sizeof(test_tag));

	key_len = EVP_CIPHER_key_length(cipher);

	key = alloc_buffer(key_len);
	iv = generate_iv(iv_len, alg);
	cleartext = alloc_buffer(size);
	buffer = malloc(size);
	ciphertext = malloc(size);
	if (aad_len != 0)
		aad = alloc_buffer(aad_len);
	else
		aad = NULL;

	/* OpenSSL encrypt */
	if (EVP_CIPHER_mode(cipher) == EVP_CIPH_CCM_MODE)
		openssl_ccm_encrypt(alg, cipher, key, iv, iv_len, aad,
		    aad_len, cleartext, ciphertext, size, control_tag);
	else
		openssl_aead_encrypt(alg, cipher, key, iv, iv_len, aad,
		    aad_len, cleartext, ciphertext, size, control_tag);

	if (!ocf_init_aead_session(alg, key, key_len, iv_len, &ses))
		goto out;

	/* OCF encrypt */
	error = ocf_aead(&ses, iv, iv_len, aad, aad_len, cleartext, buffer,
	    size, test_tag, COP_ENCRYPT);
	if (error != 0) {
		warnc(error, "cryptodev %s/%zu (%zu, %zu) failed for device %s",
		    alg->name, iv_len, aad_len, size, crfind(ses.crid));
		goto out;
	}
	if (memcmp(ciphertext, buffer, size) != 0) {
		printf("%s/%zu (%zu, %zu) encryption mismatch:\n", alg->name,
		    iv_len, aad_len, size);
		printf("control:\n");
		hexdump(ciphertext, size, NULL, 0);
		printf("test (cryptodev device %s):\n", crfind(ses.crid));
		hexdump(buffer, size, NULL, 0);
		goto out;
	}
	if (memcmp(control_tag, test_tag, sizeof(control_tag)) != 0) {
		printf("%s/%zu (%zu, %zu) enc tag mismatch:\n", alg->name,
		    iv_len, aad_len, size);
		printf("control:\n");
		hexdump(control_tag, sizeof(control_tag), NULL, 0);
		printf("test (cryptodev device %s):\n", crfind(ses.crid));
		hexdump(test_tag, sizeof(test_tag), NULL, 0);
		goto out;
	}

	/* OCF decrypt */
	error = ocf_aead(&ses, iv, iv_len, aad, aad_len, ciphertext,
	    buffer, size, control_tag, COP_DECRYPT);
	if (error != 0) {
		warnc(error, "cryptodev %s/%zu (%zu, %zu) failed for device %s",
		    alg->name, iv_len, aad_len, size, crfind(ses.crid));
		goto out;
	}
	if (memcmp(cleartext, buffer, size) != 0) {
		printf("%s/%zu (%zu, %zu) decryption mismatch:\n", alg->name,
		    iv_len, aad_len, size);
		printf("control:\n");
		hexdump(cleartext, size, NULL, 0);
		printf("test (cryptodev device %s):\n", crfind(ses.crid));
		hexdump(buffer, size, NULL, 0);
		goto out;
	}

	/* Verify OCF decrypt fails with busted tag. */
	test_tag[0] ^= 0x1;
	error = ocf_aead(&ses, iv, iv_len, aad, aad_len, ciphertext,
	    buffer, size, test_tag, COP_DECRYPT);
	if (error != EBADMSG) {
		if (error != 0)
			warnc(error,
		    "cryptodev %s/%zu (%zu, %zu) corrupt tag failed for device %s",
			    alg->name, iv_len, aad_len, size, crfind(ses.crid));
		else
			warnx(
	    "cryptodev %s/%zu (%zu, %zu) corrupt tag didn't fail for device %s",
			    alg->name, iv_len, aad_len, size, crfind(ses.crid));
		goto out;
	}

	if (verbose)
		printf("%s/%zu (%zu, %zu) matched (cryptodev device %s)\n",
		    alg->name, iv_len, aad_len, size, crfind(ses.crid));

out:
	ocf_destroy_session(&ses);
	free(aad);
	free(ciphertext);
	free(buffer);
	free(cleartext);
	free(iv);
	free(key);
}

static void
run_test(const struct alg *alg, size_t aad_len, size_t size, size_t iv_len)
{

	switch (alg->type) {
	case T_HASH:
		run_hash_test(alg, size);
		break;
	case T_HMAC:
		run_hmac_test(alg, size);
		break;
	case T_GMAC:
		run_gmac_test(alg, size);
		break;
	case T_DIGEST:
		run_digest_test(alg, size);
		break;
	case T_CIPHER:
		run_cipher_test(alg, size);
		break;
	case T_ETA:
		run_eta_test(alg, aad_len, size);
		break;
	case T_AEAD:
		run_aead_test(alg, aad_len, size, iv_len);
		break;
	}
}

static void
run_test_sizes(const struct alg *alg)
{
	u_int i, j, k;

	switch (alg->type) {
	default:
		for (i = 0; i < nsizes; i++)
			run_test(alg, 0, sizes[i], 0);
		break;
	case T_ETA:
		for (i = 0; i < naad_sizes; i++)
			for (j = 0; j < nsizes; j++)
				run_test(alg, aad_sizes[i], sizes[j], 0);
		break;
	case T_AEAD:
		for (i = 0; i < naad_sizes; i++) {
			for (j = 0; j < nsizes; j++) {
				if (iv_size != 0)
					run_test(alg, aad_sizes[i], sizes[j],
					    iv_size);
				else if (testall) {
					for (k = 0; alg->iv_sizes[k] != 0; k++)
						run_test(alg, aad_sizes[i],
						    sizes[j], alg->iv_sizes[k]);
				} else
					run_test(alg, aad_sizes[i], sizes[j],
					    alg->iv_sizes[0]);
			}
		}
		break;
	}
}

static void
run_hash_tests(void)
{
	u_int i;

	for (i = 0; i < nitems(algs); i++)
		if (algs[i].type == T_HASH)
			run_test_sizes(&algs[i]);
}

static void
run_mac_tests(void)
{
	u_int i;

	for (i = 0; i < nitems(algs); i++)
		if (algs[i].type == T_HMAC || algs[i].type == T_GMAC ||
		    algs[i].type == T_DIGEST)
			run_test_sizes(&algs[i]);
}

static void
run_cipher_tests(void)
{
	u_int i;

	for (i = 0; i < nitems(algs); i++)
		if (algs[i].type == T_CIPHER)
			run_test_sizes(&algs[i]);
}

static void
run_eta_tests(void)
{
	const struct alg *cipher, *mac;
	struct alg *eta;
	u_int i, j;

	for (i = 0; i < nitems(algs); i++) {
		cipher = &algs[i];
		if (cipher->type != T_CIPHER)
			continue;
		for (j = 0; j < nitems(algs); j++) {
			mac = &algs[j];
			if (mac->type != T_HMAC)
				continue;
			eta = build_eta(cipher, mac);
			run_test_sizes(eta);
			free_eta(eta);
		}
	}
}

static void
run_aead_tests(void)
{
	u_int i;

	for (i = 0; i < nitems(algs); i++)
		if (algs[i].type == T_AEAD)
			run_test_sizes(&algs[i]);
}

int
main(int ac, char **av)
{
	const char *algname;
	const struct alg *alg;
	struct alg *eta;
	char *cp;
	size_t base_size;
	u_int i;
	int ch;

	algname = NULL;
	requested_crid = CRYPTO_FLAG_HARDWARE;
	testall = false;
	verbose = false;
	iv_size = 0;
	while ((ch = getopt(ac, av, "A:a:d:I:vz")) != -1)
		switch (ch) {
		case 'A':
			if (naad_sizes >= nitems(aad_sizes)) {
				warnx("Too many AAD sizes, ignoring extras");
				break;
			}
			aad_sizes[naad_sizes] = strtol(optarg, &cp, 0);
			if (*cp != '\0')
				errx(1, "Bad AAD size %s", optarg);
			naad_sizes++;
			break;
		case 'a':
			algname = optarg;
			break;
		case 'd':
			requested_crid = crlookup(optarg);
			break;
		case 'I':
			iv_size = strtol(optarg, &cp, 0);
			if (*cp != '\0')
				errx(1, "Bad IV size %s", optarg);
			break;
		case 'v':
			verbose = true;
			break;
		case 'z':
			testall = true;
			break;
		default:
			usage();
		}
	ac -= optind;
	av += optind;
	nsizes = 0;
	while (ac > 0) {
		if (nsizes >= nitems(sizes)) {
			warnx("Too many sizes, ignoring extras");
			break;
		}
		sizes[nsizes] = strtol(av[0], &cp, 0);
		if (*cp != '\0')
			errx(1, "Bad size %s", av[0]);
		nsizes++;
		ac--;
		av++;
	}

	if (algname == NULL)
		errx(1, "Algorithm required");

	if (naad_sizes == 0) {
		if (testall) {
			for (i = 0; i <= 32; i++) {
				aad_sizes[naad_sizes] = i;
				naad_sizes++;
			}

			base_size = 32;
			while (base_size * 2 < 512) {
				base_size *= 2;
				assert(naad_sizes < nitems(aad_sizes));
				aad_sizes[naad_sizes] = base_size;
				naad_sizes++;
			}
		} else {
			aad_sizes[0] = 0;
			naad_sizes = 1;
		}
	}

	if (nsizes == 0) {
		if (testall) {
			for (i = 1; i <= EALG_MAX_BLOCK_LEN; i++) {
				sizes[nsizes] = i;
				nsizes++;
			}

			for (i = EALG_MAX_BLOCK_LEN + 8;
			     i <= EALG_MAX_BLOCK_LEN * 2; i += 8) {
				sizes[nsizes] = i;
				nsizes++;
			}

			base_size = EALG_MAX_BLOCK_LEN * 2;
			while (base_size * 2 < 240 * 1024) {
				base_size *= 2;
				assert(nsizes < nitems(sizes));
				sizes[nsizes] = base_size;
				nsizes++;
			}

			if (sizes[nsizes - 1] < 240 * 1024) {
				assert(nsizes < nitems(sizes));
				sizes[nsizes] = 240 * 1024;
				nsizes++;
			}
		} else {
			sizes[0] = 16;
			nsizes = 1;
		}
	}

	if (strcasecmp(algname, "hash") == 0)
		run_hash_tests();
	else if (strcasecmp(algname, "mac") == 0)
		run_mac_tests();
	else if (strcasecmp(algname, "cipher") == 0)
		run_cipher_tests();
	else if (strcasecmp(algname, "eta") == 0)
		run_eta_tests();
	else if (strcasecmp(algname, "aead") == 0)
		run_aead_tests();
	else if (strcasecmp(algname, "all") == 0) {
		run_hash_tests();
		run_mac_tests();
		run_cipher_tests();
		run_eta_tests();
		run_aead_tests();
	} else if (strchr(algname, '+') != NULL) {
		eta = build_eta_name(algname);
		run_test_sizes(eta);
		free_eta(eta);
	} else {
		alg = find_alg(algname);
		if (alg == NULL)
			errx(1, "Invalid algorithm %s", algname);
		run_test_sizes(alg);
	}

	return (0);
}
