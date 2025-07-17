/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Netflix Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/event.h>
#include <sys/ktls.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <crypto/cryptodev.h>
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <libutil.h>
#include <netdb.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <atf-c.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

static void
require_ktls(void)
{
	size_t len;
	bool enable;

	len = sizeof(enable);
	if (sysctlbyname("kern.ipc.tls.enable", &enable, &len, NULL, 0) == -1) {
		if (errno == ENOENT)
			atf_tc_skip("kernel does not support TLS offload");
		atf_libc_error(errno, "Failed to read kern.ipc.tls.enable");
	}

	if (!enable)
		atf_tc_skip("Kernel TLS is disabled");
}

#define	ATF_REQUIRE_KTLS()	require_ktls()

static void
check_tls_mode(const atf_tc_t *tc, int s, int sockopt)
{
	if (atf_tc_get_config_var_as_bool_wd(tc, "ktls.require_ifnet", false)) {
		socklen_t len;
		int mode;

		len = sizeof(mode);
		if (getsockopt(s, IPPROTO_TCP, sockopt, &mode, &len) == -1)
			atf_libc_error(errno, "Failed to fetch TLS mode");

		if (mode != TCP_TLS_MODE_IFNET)
			atf_tc_skip("connection did not use ifnet TLS");
	}

	if (atf_tc_get_config_var_as_bool_wd(tc, "ktls.require_toe", false)) {
		socklen_t len;
		int mode;

		len = sizeof(mode);
		if (getsockopt(s, IPPROTO_TCP, sockopt, &mode, &len) == -1)
			atf_libc_error(errno, "Failed to fetch TLS mode");

		if (mode != TCP_TLS_MODE_TOE)
			atf_tc_skip("connection did not use TOE TLS");
	}
}

static void __printflike(2, 3)
debug(const atf_tc_t *tc, const char *fmt, ...)
{
	if (!atf_tc_get_config_var_as_bool_wd(tc, "ktls.debug", false))
		return;

	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

static void
debug_hexdump(const atf_tc_t *tc, const void *buf, int length,
    const char *label)
{
	if (!atf_tc_get_config_var_as_bool_wd(tc, "ktls.debug", false))
		return;

	if (label != NULL)
		printf("%s:\n", label);
	hexdump(buf, length, NULL, 0);
}

static char
rdigit(void)
{
	/* ASCII printable values between 0x20 and 0x7e */
	return (0x20 + random() % (0x7f - 0x20));
}

static char *
alloc_buffer(size_t len)
{
	char *buf;
	size_t i;

	if (len == 0)
		return (NULL);
	buf = malloc(len);
	for (i = 0; i < len; i++)
		buf[i] = rdigit();
	return (buf);
}

static bool
socketpair_tcp(int sv[2])
{
	struct pollfd pfd;
	struct sockaddr_in sin;
	socklen_t len;
	int as, cs, ls;

	ls = socket(PF_INET, SOCK_STREAM, 0);
	if (ls == -1) {
		warn("socket() for listen");
		return (false);
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(ls, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		warn("bind");
		close(ls);
		return (false);
	}

	if (listen(ls, 1) == -1) {
		warn("listen");
		close(ls);
		return (false);
	}

	len = sizeof(sin);
	if (getsockname(ls, (struct sockaddr *)&sin, &len) == -1) {
		warn("getsockname");
		close(ls);
		return (false);
	}

	cs = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (cs == -1) {
		warn("socket() for connect");
		close(ls);
		return (false);
	}

	if (connect(cs, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		if (errno != EINPROGRESS) {
			warn("connect");
			close(ls);
			close(cs);
			return (false);
		}
	}

	as = accept4(ls, NULL, NULL, SOCK_NONBLOCK);
	if (as == -1) {
		warn("accept4");
		close(ls);
		close(cs);
		return (false);
	}

	close(ls);

	pfd.fd = cs;
	pfd.events = POLLOUT;
	pfd.revents = 0;
	ATF_REQUIRE_INTEQ(1, poll(&pfd, 1, INFTIM));
	ATF_REQUIRE_INTEQ(POLLOUT, pfd.revents);

	sv[0] = cs;
	sv[1] = as;
	return (true);
}

static bool
echo_socket(const atf_tc_t *tc, int sv[2])
{
	const char *cause, *host, *port;
	struct addrinfo hints, *ai, *tofree;
	int error, flags, s;

	host = atf_tc_get_config_var(tc, "ktls.host");
	port = atf_tc_get_config_var_wd(tc, "ktls.port", "echo");
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	error = getaddrinfo(host, port, &hints, &tofree);
	if (error != 0) {
		warnx("getaddrinfo(%s:%s) failed: %s", host, port,
		    gai_strerror(error));
		return (false);
	}

	cause = NULL;
	for (ai = tofree; ai != NULL; ai = ai->ai_next) {
		s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s == -1) {
			cause = "socket";
			error = errno;
			continue;
		}

		if (connect(s, ai->ai_addr, ai->ai_addrlen) == -1) {
			cause = "connect";
			error = errno;
			close(s);
			continue;
		}

		freeaddrinfo(tofree);

		ATF_REQUIRE((flags = fcntl(s, F_GETFL)) != -1);
		flags |= O_NONBLOCK;
		ATF_REQUIRE(fcntl(s, F_SETFL, flags) != -1);

		sv[0] = s;
		sv[1] = s;
		return (true);
	}

	warnc(error, "%s", cause);
	freeaddrinfo(tofree);
	return (false);
}

static bool
open_sockets(const atf_tc_t *tc, int sv[2])
{
	if (atf_tc_has_config_var(tc, "ktls.host"))
		return (echo_socket(tc, sv));
	else
		return (socketpair_tcp(sv));
}

static void
close_sockets(int sv[2])
{
	if (sv[0] != sv[1])
		ATF_REQUIRE(close(sv[1]) == 0);
	ATF_REQUIRE(close(sv[0]) == 0);
}

static void
close_sockets_ignore_errors(int sv[2])
{
	if (sv[0] != sv[1])
		close(sv[1]);
	close(sv[0]);
}

static void
fd_set_blocking(int fd)
{
	int flags;

	ATF_REQUIRE((flags = fcntl(fd, F_GETFL)) != -1);
	flags &= ~O_NONBLOCK;
	ATF_REQUIRE(fcntl(fd, F_SETFL, flags) != -1);
}

static bool
cbc_crypt(const EVP_CIPHER *cipher, const char *key, const char *iv,
    const char *input, char *output, size_t size, int enc)
{
	EVP_CIPHER_CTX *ctx;
	int outl, total;

	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL) {
		warnx("EVP_CIPHER_CTX_new failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		return (false);
	}
	if (EVP_CipherInit_ex(ctx, cipher, NULL, (const u_char *)key,
	    (const u_char *)iv, enc) != 1) {
		warnx("EVP_CipherInit_ex failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		EVP_CIPHER_CTX_free(ctx);
		return (false);
	}
	EVP_CIPHER_CTX_set_padding(ctx, 0);
	if (EVP_CipherUpdate(ctx, (u_char *)output, &outl,
	    (const u_char *)input, size) != 1) {
		warnx("EVP_CipherUpdate failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		EVP_CIPHER_CTX_free(ctx);
		return (false);
	}
	total = outl;
	if (EVP_CipherFinal_ex(ctx, (u_char *)output + outl, &outl) != 1) {
		warnx("EVP_CipherFinal_ex failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		EVP_CIPHER_CTX_free(ctx);
		return (false);
	}
	total += outl;
	if ((size_t)total != size) {
		warnx("decrypt size mismatch: %zu vs %d", size, total);
		EVP_CIPHER_CTX_free(ctx);
		return (false);
	}
	EVP_CIPHER_CTX_free(ctx);
	return (true);
}

static bool
cbc_encrypt(const EVP_CIPHER *cipher, const char *key, const char *iv,
    const char *input, char *output, size_t size)
{
	return (cbc_crypt(cipher, key, iv, input, output, size, 1));
}

static bool
cbc_decrypt(const EVP_CIPHER *cipher, const char *key, const char *iv,
    const char *input, char *output, size_t size)
{
	return (cbc_crypt(cipher, key, iv, input, output, size, 0));
}

static bool
compute_hash(const EVP_MD *md, const void *key, size_t key_len, const void *aad,
    size_t aad_len, const void *buffer, size_t len, void *digest,
    u_int *digest_len)
{
	HMAC_CTX *ctx;

	ctx = HMAC_CTX_new();
	if (ctx == NULL) {
		warnx("HMAC_CTX_new failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		return (false);
	}
	if (HMAC_Init_ex(ctx, key, key_len, md, NULL) != 1) {
		warnx("HMAC_Init_ex failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		HMAC_CTX_free(ctx);
		return (false);
	}
	if (HMAC_Update(ctx, aad, aad_len) != 1) {
		warnx("HMAC_Update (aad) failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		HMAC_CTX_free(ctx);
		return (false);
	}
	if (HMAC_Update(ctx, buffer, len) != 1) {
		warnx("HMAC_Update (payload) failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		HMAC_CTX_free(ctx);
		return (false);
	}
	if (HMAC_Final(ctx, digest, digest_len) != 1) {
		warnx("HMAC_Final failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		HMAC_CTX_free(ctx);
		return (false);
	}
	HMAC_CTX_free(ctx);
	return (true);
}

static bool
verify_hash(const EVP_MD *md, const void *key, size_t key_len, const void *aad,
    size_t aad_len, const void *buffer, size_t len, const void *digest)
{
	unsigned char digest2[EVP_MAX_MD_SIZE];
	u_int digest_len;

	if (!compute_hash(md, key, key_len, aad, aad_len, buffer, len, digest2,
	    &digest_len))
		return (false);
	if (memcmp(digest, digest2, digest_len) != 0) {
		warnx("HMAC mismatch");
		return (false);
	}
	return (true);
}

static bool
aead_encrypt(const EVP_CIPHER *cipher, const char *key, const char *nonce,
    const void *aad, size_t aad_len, const char *input, char *output,
    size_t size, char *tag, size_t tag_len)
{
	EVP_CIPHER_CTX *ctx;
	int outl, total;

	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL) {
		warnx("EVP_CIPHER_CTX_new failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		return (false);
	}
	if (EVP_EncryptInit_ex(ctx, cipher, NULL, (const u_char *)key,
	    (const u_char *)nonce) != 1) {
		warnx("EVP_EncryptInit_ex failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		EVP_CIPHER_CTX_free(ctx);
		return (false);
	}
	EVP_CIPHER_CTX_set_padding(ctx, 0);
	if (aad != NULL) {
		if (EVP_EncryptUpdate(ctx, NULL, &outl, (const u_char *)aad,
		    aad_len) != 1) {
			warnx("EVP_EncryptUpdate for AAD failed: %s",
			    ERR_error_string(ERR_get_error(), NULL));
			EVP_CIPHER_CTX_free(ctx);
			return (false);
		}
	}
	if (EVP_EncryptUpdate(ctx, (u_char *)output, &outl,
	    (const u_char *)input, size) != 1) {
		warnx("EVP_EncryptUpdate failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		EVP_CIPHER_CTX_free(ctx);
		return (false);
	}
	total = outl;
	if (EVP_EncryptFinal_ex(ctx, (u_char *)output + outl, &outl) != 1) {
		warnx("EVP_EncryptFinal_ex failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		EVP_CIPHER_CTX_free(ctx);
		return (false);
	}
	total += outl;
	if ((size_t)total != size) {
		warnx("encrypt size mismatch: %zu vs %d", size, total);
		EVP_CIPHER_CTX_free(ctx);
		return (false);
	}
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, tag_len, tag) !=
	    1) {
		warnx("EVP_CIPHER_CTX_ctrl(EVP_CTRL_AEAD_GET_TAG) failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		EVP_CIPHER_CTX_free(ctx);
		return (false);
	}
	EVP_CIPHER_CTX_free(ctx);
	return (true);
}

static bool
aead_decrypt(const EVP_CIPHER *cipher, const char *key, const char *nonce,
    const void *aad, size_t aad_len, const char *input, char *output,
    size_t size, const char *tag, size_t tag_len)
{
	EVP_CIPHER_CTX *ctx;
	int outl, total;
	bool valid;

	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL) {
		warnx("EVP_CIPHER_CTX_new failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		return (false);
	}
	if (EVP_DecryptInit_ex(ctx, cipher, NULL, (const u_char *)key,
	    (const u_char *)nonce) != 1) {
		warnx("EVP_DecryptInit_ex failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		EVP_CIPHER_CTX_free(ctx);
		return (false);
	}
	EVP_CIPHER_CTX_set_padding(ctx, 0);
	if (aad != NULL) {
		if (EVP_DecryptUpdate(ctx, NULL, &outl, (const u_char *)aad,
		    aad_len) != 1) {
			warnx("EVP_DecryptUpdate for AAD failed: %s",
			    ERR_error_string(ERR_get_error(), NULL));
			EVP_CIPHER_CTX_free(ctx);
			return (false);
		}
	}
	if (EVP_DecryptUpdate(ctx, (u_char *)output, &outl,
	    (const u_char *)input, size) != 1) {
		warnx("EVP_DecryptUpdate failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		EVP_CIPHER_CTX_free(ctx);
		return (false);
	}
	total = outl;
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, tag_len,
	    __DECONST(char *, tag)) != 1) {
		warnx("EVP_CIPHER_CTX_ctrl(EVP_CTRL_AEAD_SET_TAG) failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		EVP_CIPHER_CTX_free(ctx);
		return (false);
	}
	valid = (EVP_DecryptFinal_ex(ctx, (u_char *)output + outl, &outl) == 1);
	total += outl;
	if ((size_t)total != size) {
		warnx("decrypt size mismatch: %zu vs %d", size, total);
		EVP_CIPHER_CTX_free(ctx);
		return (false);
	}
	if (!valid)
		warnx("tag mismatch");
	EVP_CIPHER_CTX_free(ctx);
	return (valid);
}

static void
build_tls_enable(const atf_tc_t *tc, int cipher_alg, size_t cipher_key_len,
    int auth_alg, int minor, uint64_t seqno, struct tls_enable *en)
{
	u_int auth_key_len, iv_len;

	memset(en, 0, sizeof(*en));

	switch (cipher_alg) {
	case CRYPTO_AES_CBC:
		if (minor == TLS_MINOR_VER_ZERO)
			iv_len = AES_BLOCK_LEN;
		else
			iv_len = 0;
		break;
	case CRYPTO_AES_NIST_GCM_16:
		if (minor == TLS_MINOR_VER_TWO)
			iv_len = TLS_AEAD_GCM_LEN;
		else
			iv_len = TLS_1_3_GCM_IV_LEN;
		break;
	case CRYPTO_CHACHA20_POLY1305:
		iv_len = TLS_CHACHA20_IV_LEN;
		break;
	default:
		iv_len = 0;
		break;
	}
	switch (auth_alg) {
	case CRYPTO_SHA1_HMAC:
		auth_key_len = SHA1_HASH_LEN;
		break;
	case CRYPTO_SHA2_256_HMAC:
		auth_key_len = SHA2_256_HASH_LEN;
		break;
	case CRYPTO_SHA2_384_HMAC:
		auth_key_len = SHA2_384_HASH_LEN;
		break;
	default:
		auth_key_len = 0;
		break;
	}
	en->cipher_key = alloc_buffer(cipher_key_len);
	debug_hexdump(tc, en->cipher_key, cipher_key_len, "cipher key");
	en->iv = alloc_buffer(iv_len);
	if (iv_len != 0)
		debug_hexdump(tc, en->iv, iv_len, "iv");
	en->auth_key = alloc_buffer(auth_key_len);
	if (auth_key_len != 0)
		debug_hexdump(tc, en->auth_key, auth_key_len, "auth key");
	en->cipher_algorithm = cipher_alg;
	en->cipher_key_len = cipher_key_len;
	en->iv_len = iv_len;
	en->auth_algorithm = auth_alg;
	en->auth_key_len = auth_key_len;
	en->tls_vmajor = TLS_MAJOR_VER_ONE;
	en->tls_vminor = minor;
	be64enc(en->rec_seq, seqno);
	debug(tc, "seqno: %ju\n", (uintmax_t)seqno);
}

static void
free_tls_enable(struct tls_enable *en)
{
	free(__DECONST(void *, en->cipher_key));
	free(__DECONST(void *, en->iv));
	free(__DECONST(void *, en->auth_key));
}

static const EVP_CIPHER *
tls_EVP_CIPHER(const struct tls_enable *en)
{
	switch (en->cipher_algorithm) {
	case CRYPTO_AES_CBC:
		switch (en->cipher_key_len) {
		case 128 / 8:
			return (EVP_aes_128_cbc());
		case 256 / 8:
			return (EVP_aes_256_cbc());
		default:
			return (NULL);
		}
		break;
	case CRYPTO_AES_NIST_GCM_16:
		switch (en->cipher_key_len) {
		case 128 / 8:
			return (EVP_aes_128_gcm());
		case 256 / 8:
			return (EVP_aes_256_gcm());
		default:
			return (NULL);
		}
		break;
	case CRYPTO_CHACHA20_POLY1305:
		return (EVP_chacha20_poly1305());
	default:
		return (NULL);
	}
}

static const EVP_MD *
tls_EVP_MD(const struct tls_enable *en)
{
	switch (en->auth_algorithm) {
	case CRYPTO_SHA1_HMAC:
		return (EVP_sha1());
	case CRYPTO_SHA2_256_HMAC:
		return (EVP_sha256());
	case CRYPTO_SHA2_384_HMAC:
		return (EVP_sha384());
	default:
		return (NULL);
	}
}

static size_t
tls_header_len(struct tls_enable *en)
{
	size_t len;

	len = sizeof(struct tls_record_layer);
	switch (en->cipher_algorithm) {
	case CRYPTO_AES_CBC:
		if (en->tls_vminor != TLS_MINOR_VER_ZERO)
			len += AES_BLOCK_LEN;
		return (len);
	case CRYPTO_AES_NIST_GCM_16:
		if (en->tls_vminor == TLS_MINOR_VER_TWO)
			len += sizeof(uint64_t);
		return (len);
	case CRYPTO_CHACHA20_POLY1305:
		return (len);
	default:
		return (0);
	}
}

static size_t
tls_mac_len(struct tls_enable *en)
{
	switch (en->cipher_algorithm) {
	case CRYPTO_AES_CBC:
		switch (en->auth_algorithm) {
		case CRYPTO_SHA1_HMAC:
			return (SHA1_HASH_LEN);
		case CRYPTO_SHA2_256_HMAC:
			return (SHA2_256_HASH_LEN);
		case CRYPTO_SHA2_384_HMAC:
			return (SHA2_384_HASH_LEN);
		default:
			return (0);
		}
	case CRYPTO_AES_NIST_GCM_16:
		return (AES_GMAC_HASH_LEN);
	case CRYPTO_CHACHA20_POLY1305:
		return (POLY1305_HASH_LEN);
	default:
		return (0);
	}
}

/* Includes maximum padding for MTE. */
static size_t
tls_trailer_len(struct tls_enable *en)
{
	size_t len;

	len = tls_mac_len(en);
	if (en->cipher_algorithm == CRYPTO_AES_CBC)
		len += AES_BLOCK_LEN;
	if (en->tls_vminor == TLS_MINOR_VER_THREE)
		len++;
	return (len);
}

/* Minimum valid record payload size for a given cipher suite. */
static size_t
tls_minimum_record_payload(struct tls_enable *en)
{
	size_t len;

	len = tls_header_len(en);
	if (en->cipher_algorithm == CRYPTO_AES_CBC)
		len += roundup2(tls_mac_len(en) + 1, AES_BLOCK_LEN);
	else
		len += tls_mac_len(en);
	if (en->tls_vminor == TLS_MINOR_VER_THREE)
		len++;
	return (len - sizeof(struct tls_record_layer));
}

/* 'len' is the length of the payload application data. */
static void
tls_mte_aad(struct tls_enable *en, size_t len,
    const struct tls_record_layer *hdr, uint64_t seqno, struct tls_mac_data *ad)
{
	ad->seq = htobe64(seqno);
	ad->type = hdr->tls_type;
	ad->tls_vmajor = hdr->tls_vmajor;
	ad->tls_vminor = hdr->tls_vminor;
	ad->tls_length = htons(len);
}

static void
tls_12_aead_aad(struct tls_enable *en, size_t len,
    const struct tls_record_layer *hdr, uint64_t seqno,
    struct tls_aead_data *ad)
{
	ad->seq = htobe64(seqno);
	ad->type = hdr->tls_type;
	ad->tls_vmajor = hdr->tls_vmajor;
	ad->tls_vminor = hdr->tls_vminor;
	ad->tls_length = htons(len);
}

static void
tls_13_aad(struct tls_enable *en, const struct tls_record_layer *hdr,
    uint64_t seqno, struct tls_aead_data_13 *ad)
{
	ad->type = hdr->tls_type;
	ad->tls_vmajor = hdr->tls_vmajor;
	ad->tls_vminor = hdr->tls_vminor;
	ad->tls_length = hdr->tls_length;
}

static void
tls_12_gcm_nonce(struct tls_enable *en, const struct tls_record_layer *hdr,
    char *nonce)
{
	memcpy(nonce, en->iv, TLS_AEAD_GCM_LEN);
	memcpy(nonce + TLS_AEAD_GCM_LEN, hdr + 1, sizeof(uint64_t));
}

static void
tls_13_nonce(struct tls_enable *en, uint64_t seqno, char *nonce)
{
	static_assert(TLS_1_3_GCM_IV_LEN == TLS_CHACHA20_IV_LEN,
	    "TLS 1.3 nonce length mismatch");
	memcpy(nonce, en->iv, TLS_1_3_GCM_IV_LEN);
	*(uint64_t *)(nonce + 4) ^= htobe64(seqno);
}

/*
 * Decrypt a TLS record 'len' bytes long at 'src' and store the result at
 * 'dst'.  If the TLS record header length doesn't match or 'dst' doesn't
 * have sufficient room ('avail'), fail the test.
 */
static size_t
decrypt_tls_aes_cbc_mte(const atf_tc_t *tc, struct tls_enable *en,
    uint64_t seqno, const void *src, size_t len, void *dst, size_t avail,
    uint8_t *record_type)
{
	const struct tls_record_layer *hdr;
	struct tls_mac_data aad;
	const char *iv;
	char *buf;
	size_t hdr_len, mac_len, payload_len;
	int padding;

	hdr = src;
	hdr_len = tls_header_len(en);
	mac_len = tls_mac_len(en);
	ATF_REQUIRE_INTEQ(TLS_MAJOR_VER_ONE, hdr->tls_vmajor);
	ATF_REQUIRE_INTEQ(en->tls_vminor, hdr->tls_vminor);
	debug(tc, "decrypting MTE record seqno %ju:\n", (uintmax_t)seqno);
	debug_hexdump(tc, src, len, NULL);

	/* First, decrypt the outer payload into a temporary buffer. */
	payload_len = len - hdr_len;
	buf = malloc(payload_len);
	if (en->tls_vminor == TLS_MINOR_VER_ZERO)
		iv = en->iv;
	else
		iv = (void *)(hdr + 1);
	debug_hexdump(tc, iv, AES_BLOCK_LEN, "iv");
	ATF_REQUIRE(cbc_decrypt(tls_EVP_CIPHER(en), en->cipher_key, iv,
	    (const u_char *)src + hdr_len, buf, payload_len));
	debug_hexdump(tc, buf, payload_len, "decrypted buffer");

	/*
	 * Copy the last encrypted block to use as the IV for the next
	 * record for TLS 1.0.
	 */
	if (en->tls_vminor == TLS_MINOR_VER_ZERO)
		memcpy(__DECONST(uint8_t *, en->iv), (const u_char *)src +
		    (len - AES_BLOCK_LEN), AES_BLOCK_LEN);

	/*
	 * Verify trailing padding and strip.
	 *
	 * The kernel always generates the smallest amount of padding.
	 */
	padding = buf[payload_len - 1] + 1;
	ATF_REQUIRE_MSG(padding > 0 && padding <= AES_BLOCK_LEN,
	    "invalid padding %d", padding);
	ATF_REQUIRE_MSG(payload_len >= mac_len + padding,
	    "payload_len (%zu) < mac_len (%zu) + padding (%d)", payload_len,
	    mac_len, padding);
	payload_len -= padding;

	/* Verify HMAC. */
	payload_len -= mac_len;
	tls_mte_aad(en, payload_len, hdr, seqno, &aad);
	debug_hexdump(tc, &aad, sizeof(aad), "aad");
	ATF_REQUIRE(verify_hash(tls_EVP_MD(en), en->auth_key, en->auth_key_len,
	    &aad, sizeof(aad), buf, payload_len, buf + payload_len));

	ATF_REQUIRE_MSG(payload_len <= avail, "payload_len (%zu) < avail (%zu)",
	    payload_len, avail);
	memcpy(dst, buf, payload_len);
	*record_type = hdr->tls_type;
	return (payload_len);
}

static size_t
decrypt_tls_12_aead(const atf_tc_t *tc, struct tls_enable *en, uint64_t seqno,
    const void *src, size_t len, void *dst, uint8_t *record_type)
{
	const struct tls_record_layer *hdr;
	struct tls_aead_data aad;
	char nonce[12];
	size_t hdr_len, mac_len, payload_len;

	hdr = src;

	hdr_len = tls_header_len(en);
	mac_len = tls_mac_len(en);
	payload_len = len - (hdr_len + mac_len);
	ATF_REQUIRE_INTEQ(TLS_MAJOR_VER_ONE, hdr->tls_vmajor);
	ATF_REQUIRE_INTEQ(TLS_MINOR_VER_TWO, hdr->tls_vminor);
	debug(tc, "decrypting TLS 1.2 record seqno %ju:\n", (uintmax_t)seqno);
	debug_hexdump(tc, src, len, NULL);

	tls_12_aead_aad(en, payload_len, hdr, seqno, &aad);
	debug_hexdump(tc, &aad, sizeof(aad), "aad");
	if (en->cipher_algorithm == CRYPTO_AES_NIST_GCM_16)
		tls_12_gcm_nonce(en, hdr, nonce);
	else
		tls_13_nonce(en, seqno, nonce);
	debug_hexdump(tc, nonce, sizeof(nonce), "nonce");

	ATF_REQUIRE(aead_decrypt(tls_EVP_CIPHER(en), en->cipher_key, nonce,
	    &aad, sizeof(aad), (const char *)src + hdr_len, dst, payload_len,
	    (const char *)src + hdr_len + payload_len, mac_len));

	*record_type = hdr->tls_type;
	return (payload_len);
}

static size_t
decrypt_tls_13_aead(const atf_tc_t *tc, struct tls_enable *en, uint64_t seqno,
    const void *src, size_t len, void *dst, uint8_t *record_type)
{
	const struct tls_record_layer *hdr;
	struct tls_aead_data_13 aad;
	char nonce[12];
	char *buf;
	size_t hdr_len, mac_len, payload_len;

	hdr = src;

	hdr_len = tls_header_len(en);
	mac_len = tls_mac_len(en);
	payload_len = len - (hdr_len + mac_len);
	ATF_REQUIRE_MSG(payload_len >= 1,
	    "payload_len (%zu) too short: len %zu hdr_len %zu mac_len %zu",
	    payload_len, len, hdr_len, mac_len);
	ATF_REQUIRE_INTEQ(TLS_RLTYPE_APP, hdr->tls_type);
	ATF_REQUIRE_INTEQ(TLS_MAJOR_VER_ONE, hdr->tls_vmajor);
	ATF_REQUIRE_INTEQ(TLS_MINOR_VER_TWO, hdr->tls_vminor);
	debug(tc, "decrypting TLS 1.3 record seqno %ju:\n", (uintmax_t)seqno);
	debug_hexdump(tc, src, len, NULL);

	tls_13_aad(en, hdr, seqno, &aad);
	debug_hexdump(tc, &aad, sizeof(aad), "aad");
	tls_13_nonce(en, seqno, nonce);
	debug_hexdump(tc, nonce, sizeof(nonce), "nonce");

	/*
	 * Have to use a temporary buffer for the output due to the
	 * record type as the last byte of the trailer.
	 */
	buf = malloc(payload_len);

	ATF_REQUIRE(aead_decrypt(tls_EVP_CIPHER(en), en->cipher_key, nonce,
	    &aad, sizeof(aad), (const char *)src + hdr_len, buf, payload_len,
	    (const char *)src + hdr_len + payload_len, mac_len));
	debug_hexdump(tc, buf, payload_len, "decrypted buffer");

	/* Trim record type. */
	*record_type = buf[payload_len - 1];
	payload_len--;

	memcpy(dst, buf, payload_len);
	free(buf);

	return (payload_len);
}

static size_t
decrypt_tls_aead(const atf_tc_t *tc, struct tls_enable *en, uint64_t seqno,
    const void *src, size_t len, void *dst, size_t avail, uint8_t *record_type)
{
	const struct tls_record_layer *hdr;
	size_t payload_len;

	hdr = src;
	ATF_REQUIRE_INTEQ(len, ntohs(hdr->tls_length) + sizeof(*hdr));

	payload_len = len - (tls_header_len(en) + tls_trailer_len(en));
	ATF_REQUIRE_MSG(payload_len <= avail, "payload_len (%zu) > avail (%zu)",
	    payload_len, avail);

	if (en->tls_vminor == TLS_MINOR_VER_TWO) {
		ATF_REQUIRE_INTEQ(payload_len, decrypt_tls_12_aead(tc, en,
		    seqno, src, len, dst, record_type));
	} else {
		ATF_REQUIRE_INTEQ(payload_len, decrypt_tls_13_aead(tc, en,
		    seqno, src, len, dst, record_type));
	}

	return (payload_len);
}

static size_t
decrypt_tls_record(const atf_tc_t *tc, struct tls_enable *en, uint64_t seqno,
    const void *src, size_t len, void *dst, size_t avail, uint8_t *record_type)
{
	if (en->cipher_algorithm == CRYPTO_AES_CBC)
		return (decrypt_tls_aes_cbc_mte(tc, en, seqno, src, len, dst,
		    avail, record_type));
	else
		return (decrypt_tls_aead(tc, en, seqno, src, len, dst, avail,
		    record_type));
}

/*
 * Encrypt a TLS record of type 'record_type' with payload 'len' bytes
 * long at 'src' and store the result at 'dst'.  If 'dst' doesn't have
 * sufficient room ('avail'), fail the test.  'padding' is the amount
 * of additional padding to include beyond any amount mandated by the
 * cipher suite.
 */
static size_t
encrypt_tls_aes_cbc_mte(const atf_tc_t *tc, struct tls_enable *en,
    uint8_t record_type, uint64_t seqno, const void *src, size_t len, void *dst,
    size_t avail, size_t padding)
{
	struct tls_record_layer *hdr;
	struct tls_mac_data aad;
	char *buf, *iv;
	size_t hdr_len, mac_len, record_len;
	u_int digest_len, i;

	ATF_REQUIRE_INTEQ(0, padding % 16);

	hdr = dst;
	buf = dst;

	debug(tc, "encrypting MTE record seqno %ju:\n", (uintmax_t)seqno);
	hdr_len = tls_header_len(en);
	mac_len = tls_mac_len(en);
	padding += (AES_BLOCK_LEN - (len + mac_len) % AES_BLOCK_LEN);
	ATF_REQUIRE_MSG(padding > 0 && padding <= 255, "invalid padding (%zu)",
	    padding);

	record_len = hdr_len + len + mac_len + padding;
	ATF_REQUIRE_MSG(record_len <= avail, "record_len (%zu) > avail (%zu): "
	    "hdr_len %zu, len %zu, mac_len %zu, padding %zu", record_len,
	    avail, hdr_len, len, mac_len, padding);

	hdr->tls_type = record_type;
	hdr->tls_vmajor = TLS_MAJOR_VER_ONE;
	hdr->tls_vminor = en->tls_vminor;
	hdr->tls_length = htons(record_len - sizeof(*hdr));
	iv = (char *)(hdr + 1);
	for (i = 0; i < AES_BLOCK_LEN; i++)
		iv[i] = rdigit();
	debug_hexdump(tc, iv, AES_BLOCK_LEN, "explicit IV");

	/* Copy plaintext to ciphertext region. */
	memcpy(buf + hdr_len, src, len);

	/* Compute HMAC. */
	tls_mte_aad(en, len, hdr, seqno, &aad);
	debug_hexdump(tc, &aad, sizeof(aad), "aad");
	debug_hexdump(tc, src, len, "plaintext");
	ATF_REQUIRE(compute_hash(tls_EVP_MD(en), en->auth_key, en->auth_key_len,
	    &aad, sizeof(aad), src, len, buf + hdr_len + len, &digest_len));
	ATF_REQUIRE_INTEQ(mac_len, digest_len);

	/* Store padding. */
	for (i = 0; i < padding; i++)
		buf[hdr_len + len + mac_len + i] = padding - 1;
	debug_hexdump(tc, buf + hdr_len + len, mac_len + padding,
	    "MAC and padding");

	/* Encrypt the record. */
	ATF_REQUIRE(cbc_encrypt(tls_EVP_CIPHER(en), en->cipher_key, iv,
	    buf + hdr_len, buf + hdr_len, len + mac_len + padding));
	debug_hexdump(tc, dst, record_len, "encrypted record");

	return (record_len);
}

static size_t
encrypt_tls_12_aead(const atf_tc_t *tc, struct tls_enable *en,
    uint8_t record_type, uint64_t seqno, const void *src, size_t len, void *dst)
{
	struct tls_record_layer *hdr;
	struct tls_aead_data aad;
	char nonce[12];
	size_t hdr_len, mac_len, record_len;

	hdr = dst;

	debug(tc, "encrypting TLS 1.2 record seqno %ju:\n", (uintmax_t)seqno);
	hdr_len = tls_header_len(en);
	mac_len = tls_mac_len(en);
	record_len = hdr_len + len + mac_len;

	hdr->tls_type = record_type;
	hdr->tls_vmajor = TLS_MAJOR_VER_ONE;
	hdr->tls_vminor = TLS_MINOR_VER_TWO;
	hdr->tls_length = htons(record_len - sizeof(*hdr));
	if (en->cipher_algorithm == CRYPTO_AES_NIST_GCM_16)
		memcpy(hdr + 1, &seqno, sizeof(seqno));

	tls_12_aead_aad(en, len, hdr, seqno, &aad);
	debug_hexdump(tc, &aad, sizeof(aad), "aad");
	if (en->cipher_algorithm == CRYPTO_AES_NIST_GCM_16)
		tls_12_gcm_nonce(en, hdr, nonce);
	else
		tls_13_nonce(en, seqno, nonce);
	debug_hexdump(tc, nonce, sizeof(nonce), "nonce");

	debug_hexdump(tc, src, len, "plaintext");
	ATF_REQUIRE(aead_encrypt(tls_EVP_CIPHER(en), en->cipher_key, nonce,
	    &aad, sizeof(aad), src, (char *)dst + hdr_len, len,
	    (char *)dst + hdr_len + len, mac_len));
	debug_hexdump(tc, dst, record_len, "encrypted record");

	return (record_len);
}

static size_t
encrypt_tls_13_aead(const atf_tc_t *tc, struct tls_enable *en,
    uint8_t record_type, uint64_t seqno, const void *src, size_t len, void *dst,
    size_t padding)
{
	struct tls_record_layer *hdr;
	struct tls_aead_data_13 aad;
	char nonce[12];
	char *buf;
	size_t hdr_len, mac_len, record_len;

	hdr = dst;

	debug(tc, "encrypting TLS 1.3 record seqno %ju:\n", (uintmax_t)seqno);
	hdr_len = tls_header_len(en);
	mac_len = tls_mac_len(en);
	record_len = hdr_len + len + 1 + padding + mac_len;

	hdr->tls_type = TLS_RLTYPE_APP;
	hdr->tls_vmajor = TLS_MAJOR_VER_ONE;
	hdr->tls_vminor = TLS_MINOR_VER_TWO;
	hdr->tls_length = htons(record_len - sizeof(*hdr));

	tls_13_aad(en, hdr, seqno, &aad);
	debug_hexdump(tc, &aad, sizeof(aad), "aad");
	tls_13_nonce(en, seqno, nonce);
	debug_hexdump(tc, nonce, sizeof(nonce), "nonce");

	/*
	 * Have to use a temporary buffer for the input so that the record
	 * type can be appended.
	 */
	buf = malloc(len + 1 + padding);
	memcpy(buf, src, len);
	buf[len] = record_type;
	memset(buf + len + 1, 0, padding);
	debug_hexdump(tc, buf, len + 1 + padding, "plaintext + type + padding");

	ATF_REQUIRE(aead_encrypt(tls_EVP_CIPHER(en), en->cipher_key, nonce,
	    &aad, sizeof(aad), buf, (char *)dst + hdr_len, len + 1 + padding,
	    (char *)dst + hdr_len + len + 1 + padding, mac_len));
	debug_hexdump(tc, dst, record_len, "encrypted record");

	free(buf);

	return (record_len);
}

static size_t
encrypt_tls_aead(const atf_tc_t *tc, struct tls_enable *en,
    uint8_t record_type, uint64_t seqno, const void *src, size_t len, void *dst,
    size_t avail, size_t padding)
{
	size_t record_len;

	record_len = tls_header_len(en) + len + padding + tls_trailer_len(en);
	ATF_REQUIRE_MSG(record_len <= avail, "record_len (%zu) > avail (%zu): "
	    "header %zu len %zu padding %zu trailer %zu", record_len, avail,
	    tls_header_len(en), len, padding, tls_trailer_len(en));

	if (en->tls_vminor == TLS_MINOR_VER_TWO) {
		ATF_REQUIRE_INTEQ(0, padding);
		ATF_REQUIRE_INTEQ(record_len, encrypt_tls_12_aead(tc, en,
		    record_type, seqno, src, len, dst));
	} else
		ATF_REQUIRE_INTEQ(record_len, encrypt_tls_13_aead(tc, en,
		    record_type, seqno, src, len, dst, padding));

	return (record_len);
}

static size_t
encrypt_tls_record(const atf_tc_t *tc, struct tls_enable *en,
    uint8_t record_type, uint64_t seqno, const void *src, size_t len, void *dst,
    size_t avail, size_t padding)
{
	if (en->cipher_algorithm == CRYPTO_AES_CBC)
		return (encrypt_tls_aes_cbc_mte(tc, en, record_type, seqno, src,
		    len, dst, avail, padding));
	else
		return (encrypt_tls_aead(tc, en, record_type, seqno, src, len,
		    dst, avail, padding));
}

static void
test_ktls_transmit_app_data(const atf_tc_t *tc, struct tls_enable *en,
    uint64_t seqno, size_t len)
{
	struct kevent ev;
	struct tls_record_layer *hdr;
	char *plaintext, *decrypted, *outbuf;
	size_t decrypted_len, outbuf_len, outbuf_cap, record_len, written;
	ssize_t rv;
	int kq, sockets[2];
	uint8_t record_type;

	plaintext = alloc_buffer(len);
	debug_hexdump(tc, plaintext, len, "plaintext");
	decrypted = malloc(len);
	outbuf_cap = tls_header_len(en) + TLS_MAX_MSG_SIZE_V10_2 +
	    tls_trailer_len(en);
	outbuf = malloc(outbuf_cap);
	hdr = (struct tls_record_layer *)outbuf;

	ATF_REQUIRE((kq = kqueue()) != -1);

	ATF_REQUIRE_MSG(open_sockets(tc, sockets), "failed to create sockets");

	ATF_REQUIRE(setsockopt(sockets[1], IPPROTO_TCP, TCP_TXTLS_ENABLE, en,
	    sizeof(*en)) == 0);
	check_tls_mode(tc, sockets[1], TCP_TXTLS_MODE);

	EV_SET(&ev, sockets[0], EVFILT_READ, EV_ADD, 0, 0, NULL);
	ATF_REQUIRE(kevent(kq, &ev, 1, NULL, 0, NULL) == 0);
	EV_SET(&ev, sockets[1], EVFILT_WRITE, EV_ADD, 0, 0, NULL);
	ATF_REQUIRE(kevent(kq, &ev, 1, NULL, 0, NULL) == 0);

	decrypted_len = 0;
	outbuf_len = 0;
	written = 0;

	while (decrypted_len != len) {
		ATF_REQUIRE(kevent(kq, NULL, 0, &ev, 1, NULL) == 1);

		switch (ev.filter) {
		case EVFILT_WRITE:
			/* Try to write any remaining data. */
			rv = write(ev.ident, plaintext + written,
			    len - written);
			ATF_REQUIRE_MSG(rv > 0,
			    "failed to write to socket");
			written += rv;
			if (written == len) {
				ev.flags = EV_DISABLE;
				ATF_REQUIRE(kevent(kq, &ev, 1, NULL, 0,
				    NULL) == 0);
			}
			break;

		case EVFILT_READ:
			ATF_REQUIRE((ev.flags & EV_EOF) == 0);

			/*
			 * Try to read data for the next TLS record
			 * into outbuf.  Start by reading the header
			 * to determine how much additional data to
			 * read.
			 */
			if (outbuf_len < sizeof(struct tls_record_layer)) {
				rv = read(ev.ident, outbuf + outbuf_len,
				    sizeof(struct tls_record_layer) -
				    outbuf_len);
				ATF_REQUIRE_MSG(rv > 0,
				    "failed to read from socket");
				outbuf_len += rv;

				if (outbuf_len ==
				    sizeof(struct tls_record_layer)) {
					debug(tc, "TLS header for seqno %ju:\n",
					    (uintmax_t)seqno);
					debug_hexdump(tc, outbuf, outbuf_len,
					    NULL);
				}
			}

			if (outbuf_len < sizeof(struct tls_record_layer))
				break;

			record_len = sizeof(struct tls_record_layer) +
			    ntohs(hdr->tls_length);
			debug(tc, "record_len %zu outbuf_cap %zu\n",
			    record_len, outbuf_cap);
			ATF_REQUIRE(record_len <= outbuf_cap);
			ATF_REQUIRE(record_len > outbuf_len);
			rv = read(ev.ident, outbuf + outbuf_len,
			    record_len - outbuf_len);
			if (rv == -1 && errno == EAGAIN)
				break;
			ATF_REQUIRE_MSG(rv > 0,
			    "failed to read from socket: %s", strerror(errno));

			outbuf_len += rv;
			if (outbuf_len == record_len) {
				decrypted_len += decrypt_tls_record(tc, en,
				    seqno, outbuf, outbuf_len,
				    decrypted + decrypted_len,
				    len - decrypted_len, &record_type);
				ATF_REQUIRE_INTEQ(TLS_RLTYPE_APP, record_type);

				seqno++;
				outbuf_len = 0;
			}
			break;
		}
	}

	ATF_REQUIRE_MSG(written == decrypted_len,
	    "read %zu decrypted bytes, but wrote %zu", decrypted_len, written);

	ATF_REQUIRE(memcmp(plaintext, decrypted, len) == 0);

	free(outbuf);
	free(decrypted);
	free(plaintext);

	close_sockets(sockets);
	ATF_REQUIRE(close(kq) == 0);
}

static void
ktls_send_control_message(int fd, uint8_t type, void *data, size_t len)
{
	struct msghdr msg;
	struct cmsghdr *cmsg;
	char cbuf[CMSG_SPACE(sizeof(type))];
	struct iovec iov;

	memset(&msg, 0, sizeof(msg));

	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = IPPROTO_TCP;
	cmsg->cmsg_type = TLS_SET_RECORD_TYPE;
	cmsg->cmsg_len = CMSG_LEN(sizeof(type));
	*(uint8_t *)CMSG_DATA(cmsg) = type;

	iov.iov_base = data;
	iov.iov_len = len;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	ATF_REQUIRE_INTEQ((ssize_t)len, sendmsg(fd, &msg, 0));
}

static void
test_ktls_transmit_control(const atf_tc_t *tc, struct tls_enable *en,
    uint64_t seqno, uint8_t type, size_t len)
{
	struct tls_record_layer *hdr;
	char *plaintext, *decrypted, *outbuf;
	size_t outbuf_cap, payload_len, record_len;
	ssize_t rv;
	int sockets[2];
	uint8_t record_type;

	ATF_REQUIRE(len <= TLS_MAX_MSG_SIZE_V10_2);

	plaintext = alloc_buffer(len);
	decrypted = malloc(len);
	outbuf_cap = tls_header_len(en) + len + tls_trailer_len(en);
	outbuf = malloc(outbuf_cap);
	hdr = (struct tls_record_layer *)outbuf;

	ATF_REQUIRE_MSG(open_sockets(tc, sockets), "failed to create sockets");

	ATF_REQUIRE(setsockopt(sockets[1], IPPROTO_TCP, TCP_TXTLS_ENABLE, en,
	    sizeof(*en)) == 0);
	check_tls_mode(tc, sockets[1], TCP_TXTLS_MODE);

	fd_set_blocking(sockets[0]);
	fd_set_blocking(sockets[1]);

	ktls_send_control_message(sockets[1], type, plaintext, len);

	/*
	 * First read the header to determine how much additional data
	 * to read.
	 */
	rv = read(sockets[0], outbuf, sizeof(struct tls_record_layer));
	ATF_REQUIRE_INTEQ(sizeof(struct tls_record_layer), rv);
	payload_len = ntohs(hdr->tls_length);
	record_len = payload_len + sizeof(struct tls_record_layer);
	ATF_REQUIRE_MSG(record_len <= outbuf_cap,
	    "record_len (%zu) > outbuf_cap (%zu)", record_len, outbuf_cap);
	rv = read(sockets[0], outbuf + sizeof(struct tls_record_layer),
	    payload_len);
	ATF_REQUIRE_INTEQ((ssize_t)payload_len, rv);

	rv = decrypt_tls_record(tc, en, seqno, outbuf, record_len, decrypted,
	    len, &record_type);

	ATF_REQUIRE_MSG((ssize_t)len == rv,
	    "read %zd decrypted bytes, but wrote %zu", rv, len);
	ATF_REQUIRE_INTEQ(type, record_type);

	ATF_REQUIRE(memcmp(plaintext, decrypted, len) == 0);

	free(outbuf);
	free(decrypted);
	free(plaintext);

	close_sockets(sockets);
}

static void
test_ktls_transmit_empty_fragment(const atf_tc_t *tc, struct tls_enable *en,
    uint64_t seqno)
{
	struct tls_record_layer *hdr;
	char *outbuf;
	size_t outbuf_cap, payload_len, record_len;
	ssize_t rv;
	int sockets[2];
	uint8_t record_type;

	outbuf_cap = tls_header_len(en) + tls_trailer_len(en);
	outbuf = malloc(outbuf_cap);
	hdr = (struct tls_record_layer *)outbuf;

	ATF_REQUIRE_MSG(open_sockets(tc, sockets), "failed to create sockets");

	ATF_REQUIRE(setsockopt(sockets[1], IPPROTO_TCP, TCP_TXTLS_ENABLE, en,
	    sizeof(*en)) == 0);
	check_tls_mode(tc, sockets[1], TCP_TXTLS_MODE);

	fd_set_blocking(sockets[0]);
	fd_set_blocking(sockets[1]);

	/*
	 * A write of zero bytes should send an empty fragment only for
	 * TLS 1.0, otherwise an error should be raised.
	 */
	rv = write(sockets[1], NULL, 0);
	if (rv == 0) {
		ATF_REQUIRE_INTEQ(CRYPTO_AES_CBC, en->cipher_algorithm);
		ATF_REQUIRE_INTEQ(TLS_MINOR_VER_ZERO, en->tls_vminor);
	} else {
		ATF_REQUIRE_INTEQ(-1, rv);
		ATF_REQUIRE_ERRNO(EINVAL, true);
		goto out;
	}

	/*
	 * First read the header to determine how much additional data
	 * to read.
	 */
	rv = read(sockets[0], outbuf, sizeof(struct tls_record_layer));
	ATF_REQUIRE_INTEQ(sizeof(struct tls_record_layer), rv);
	payload_len = ntohs(hdr->tls_length);
	record_len = payload_len + sizeof(struct tls_record_layer);
	ATF_REQUIRE_MSG(record_len <= outbuf_cap,
	    "record_len (%zu) > outbuf_cap (%zu)", record_len, outbuf_cap);
	rv = read(sockets[0], outbuf + sizeof(struct tls_record_layer),
	    payload_len);
	ATF_REQUIRE_INTEQ((ssize_t)payload_len, rv);

	rv = decrypt_tls_record(tc, en, seqno, outbuf, record_len, NULL, 0,
	    &record_type);

	ATF_REQUIRE_MSG(rv == 0,
	    "read %zd decrypted bytes for an empty fragment", rv);
	ATF_REQUIRE_INTEQ(TLS_RLTYPE_APP, record_type);

out:
	free(outbuf);

	close_sockets(sockets);
}

static size_t
ktls_receive_tls_record(struct tls_enable *en, int fd, uint8_t record_type,
    void *data, size_t len)
{
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct tls_get_record *tgr;
	char cbuf[CMSG_SPACE(sizeof(*tgr))];
	struct iovec iov;
	ssize_t rv;

	memset(&msg, 0, sizeof(msg));

	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);

	iov.iov_base = data;
	iov.iov_len = len;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	ATF_REQUIRE((rv = recvmsg(fd, &msg, 0)) > 0);

	ATF_REQUIRE((msg.msg_flags & (MSG_EOR | MSG_CTRUNC)) == MSG_EOR);

	cmsg = CMSG_FIRSTHDR(&msg);
	ATF_REQUIRE(cmsg != NULL);
	ATF_REQUIRE_INTEQ(IPPROTO_TCP, cmsg->cmsg_level);
	ATF_REQUIRE_INTEQ(TLS_GET_RECORD, cmsg->cmsg_type);
	ATF_REQUIRE_INTEQ(CMSG_LEN(sizeof(*tgr)), cmsg->cmsg_len);

	tgr = (struct tls_get_record *)CMSG_DATA(cmsg);
	ATF_REQUIRE_INTEQ(record_type, tgr->tls_type);
	ATF_REQUIRE_INTEQ(en->tls_vmajor, tgr->tls_vmajor);
	/* XXX: Not sure if this is what OpenSSL expects? */
	if (en->tls_vminor == TLS_MINOR_VER_THREE)
		ATF_REQUIRE_INTEQ(TLS_MINOR_VER_TWO, tgr->tls_vminor);
	else
		ATF_REQUIRE_INTEQ(en->tls_vminor, tgr->tls_vminor);
	ATF_REQUIRE_INTEQ(htons(rv), tgr->tls_length);

	return (rv);
}

static void
test_ktls_receive_app_data(const atf_tc_t *tc, struct tls_enable *en,
    uint64_t seqno, size_t len, size_t padding)
{
	struct kevent ev;
	char *plaintext, *received, *outbuf;
	size_t outbuf_cap, outbuf_len, outbuf_sent, received_len, todo, written;
	ssize_t rv;
	int kq, sockets[2];

	plaintext = alloc_buffer(len);
	received = malloc(len);
	outbuf_cap = tls_header_len(en) + TLS_MAX_MSG_SIZE_V10_2 +
	    tls_trailer_len(en);
	outbuf = malloc(outbuf_cap);

	ATF_REQUIRE((kq = kqueue()) != -1);

	ATF_REQUIRE_MSG(open_sockets(tc, sockets), "failed to create sockets");

	ATF_REQUIRE(setsockopt(sockets[0], IPPROTO_TCP, TCP_RXTLS_ENABLE, en,
	    sizeof(*en)) == 0);
	check_tls_mode(tc, sockets[0], TCP_RXTLS_MODE);

	EV_SET(&ev, sockets[0], EVFILT_READ, EV_ADD, 0, 0, NULL);
	ATF_REQUIRE(kevent(kq, &ev, 1, NULL, 0, NULL) == 0);
	EV_SET(&ev, sockets[1], EVFILT_WRITE, EV_ADD, 0, 0, NULL);
	ATF_REQUIRE(kevent(kq, &ev, 1, NULL, 0, NULL) == 0);

	received_len = 0;
	outbuf_len = 0;
	written = 0;

	while (received_len != len) {
		ATF_REQUIRE(kevent(kq, NULL, 0, &ev, 1, NULL) == 1);

		switch (ev.filter) {
		case EVFILT_WRITE:
			/*
			 * Compose the next TLS record to send.
			 */
			if (outbuf_len == 0) {
				ATF_REQUIRE(written < len);
				todo = len - written;
				if (todo > TLS_MAX_MSG_SIZE_V10_2 - padding)
					todo = TLS_MAX_MSG_SIZE_V10_2 - padding;
				outbuf_len = encrypt_tls_record(tc, en,
				    TLS_RLTYPE_APP, seqno, plaintext + written,
				    todo, outbuf, outbuf_cap, padding);
				outbuf_sent = 0;
				written += todo;
				seqno++;
			}

			/*
			 * Try to write the remainder of the current
			 * TLS record.
			 */
			rv = write(ev.ident, outbuf + outbuf_sent,
			    outbuf_len - outbuf_sent);
			ATF_REQUIRE_MSG(rv > 0,
			    "failed to write to socket: %s", strerror(errno));
			outbuf_sent += rv;
			if (outbuf_sent == outbuf_len) {
				outbuf_len = 0;
				if (written == len) {
					ev.flags = EV_DISABLE;
					ATF_REQUIRE(kevent(kq, &ev, 1, NULL, 0,
					    NULL) == 0);
				}
			}
			break;

		case EVFILT_READ:
			ATF_REQUIRE((ev.flags & EV_EOF) == 0);

			rv = ktls_receive_tls_record(en, ev.ident,
			    TLS_RLTYPE_APP, received + received_len,
			    len - received_len);
			received_len += rv;
			break;
		}
	}

	ATF_REQUIRE_MSG(written == received_len,
	    "read %zu decrypted bytes, but wrote %zu", received_len, written);

	ATF_REQUIRE(memcmp(plaintext, received, len) == 0);

	free(outbuf);
	free(received);
	free(plaintext);

	close_sockets(sockets);
	ATF_REQUIRE(close(kq) == 0);
}

static void
ktls_receive_tls_error(int fd, int expected_error)
{
	struct msghdr msg;
	struct tls_get_record *tgr;
	char cbuf[CMSG_SPACE(sizeof(*tgr))];
	char buf[64];
	struct iovec iov;

	memset(&msg, 0, sizeof(msg));

	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	ATF_REQUIRE(recvmsg(fd, &msg, 0) == -1);
	if (expected_error != 0)
		ATF_REQUIRE_ERRNO(expected_error, true);
}

static void
test_ktls_receive_corrupted_record(const atf_tc_t *tc, struct tls_enable *en,
    uint64_t seqno, size_t len, ssize_t offset)
{
	char *plaintext, *outbuf;
	size_t outbuf_cap, outbuf_len;
	ssize_t rv;
	int sockets[2];

	ATF_REQUIRE(len <= TLS_MAX_MSG_SIZE_V10_2);

	plaintext = alloc_buffer(len);
	outbuf_cap = tls_header_len(en) + len + tls_trailer_len(en);
	outbuf = malloc(outbuf_cap);

	ATF_REQUIRE_MSG(open_sockets(tc, sockets), "failed to create sockets");

	ATF_REQUIRE(setsockopt(sockets[0], IPPROTO_TCP, TCP_RXTLS_ENABLE, en,
	    sizeof(*en)) == 0);
	check_tls_mode(tc, sockets[0], TCP_RXTLS_MODE);

	fd_set_blocking(sockets[0]);
	fd_set_blocking(sockets[1]);

	outbuf_len = encrypt_tls_record(tc, en, TLS_RLTYPE_APP, seqno,
	    plaintext, len, outbuf, outbuf_cap, 0);

	/* A negative offset is an offset from the end. */
	if (offset < 0)
		offset += outbuf_len;
	outbuf[offset] ^= 0x01;

	rv = write(sockets[1], outbuf, outbuf_len);
	ATF_REQUIRE_INTEQ((ssize_t)outbuf_len, rv);

	ktls_receive_tls_error(sockets[0], EBADMSG);

	free(outbuf);
	free(plaintext);

	close_sockets_ignore_errors(sockets);
}

static void
test_ktls_receive_corrupted_iv(const atf_tc_t *tc, struct tls_enable *en,
    uint64_t seqno, size_t len)
{
	ATF_REQUIRE(tls_header_len(en) > sizeof(struct tls_record_layer));

	/* Corrupt the first byte of the explicit IV after the header. */
	test_ktls_receive_corrupted_record(tc, en, seqno, len,
	    sizeof(struct tls_record_layer));
}

static void
test_ktls_receive_corrupted_data(const atf_tc_t *tc, struct tls_enable *en,
    uint64_t seqno, size_t len)
{
	ATF_REQUIRE(len > 0);

	/* Corrupt the first ciphertext byte after the header. */
	test_ktls_receive_corrupted_record(tc, en, seqno, len,
	    tls_header_len(en));
}

static void
test_ktls_receive_corrupted_mac(const atf_tc_t *tc, struct tls_enable *en,
    uint64_t seqno, size_t len)
{
	size_t offset;

	/* Corrupt the first byte of the MAC. */
	if (en->cipher_algorithm == CRYPTO_AES_CBC)
		offset = tls_header_len(en) + len;
	else
		offset = -tls_mac_len(en);
	test_ktls_receive_corrupted_record(tc, en, seqno, len, offset);
}

static void
test_ktls_receive_corrupted_padding(const atf_tc_t *tc, struct tls_enable *en,
    uint64_t seqno, size_t len)
{
	ATF_REQUIRE_INTEQ(CRYPTO_AES_CBC, en->cipher_algorithm);

	/* Corrupt the last byte of the padding. */
	test_ktls_receive_corrupted_record(tc, en, seqno, len, -1);
}

static void
test_ktls_receive_truncated_record(const atf_tc_t *tc, struct tls_enable *en,
    uint64_t seqno, size_t len)
{
	char *plaintext, *outbuf;
	size_t outbuf_cap, outbuf_len;
	ssize_t rv;
	int sockets[2];

	ATF_REQUIRE(len <= TLS_MAX_MSG_SIZE_V10_2);

	plaintext = alloc_buffer(len);
	outbuf_cap = tls_header_len(en) + len + tls_trailer_len(en);
	outbuf = malloc(outbuf_cap);

	ATF_REQUIRE_MSG(open_sockets(tc, sockets), "failed to create sockets");

	ATF_REQUIRE(setsockopt(sockets[0], IPPROTO_TCP, TCP_RXTLS_ENABLE, en,
	    sizeof(*en)) == 0);
	check_tls_mode(tc, sockets[0], TCP_RXTLS_MODE);

	fd_set_blocking(sockets[0]);
	fd_set_blocking(sockets[1]);

	outbuf_len = encrypt_tls_record(tc, en, TLS_RLTYPE_APP, seqno,
	    plaintext, len, outbuf, outbuf_cap, 0);

	rv = write(sockets[1], outbuf, outbuf_len / 2);
	ATF_REQUIRE_INTEQ((ssize_t)(outbuf_len / 2), rv);

	ATF_REQUIRE(shutdown(sockets[1], SHUT_WR) == 0);

	ktls_receive_tls_error(sockets[0], EMSGSIZE);

	free(outbuf);
	free(plaintext);

	close_sockets_ignore_errors(sockets);
}

static void
test_ktls_receive_bad_major(const atf_tc_t *tc, struct tls_enable *en,
    uint64_t seqno, size_t len)
{
	struct tls_record_layer *hdr;
	char *plaintext, *outbuf;
	size_t outbuf_cap, outbuf_len;
	ssize_t rv;
	int sockets[2];

	ATF_REQUIRE(len <= TLS_MAX_MSG_SIZE_V10_2);

	plaintext = alloc_buffer(len);
	outbuf_cap = tls_header_len(en) + len + tls_trailer_len(en);
	outbuf = malloc(outbuf_cap);

	ATF_REQUIRE_MSG(open_sockets(tc, sockets), "failed to create sockets");

	ATF_REQUIRE(setsockopt(sockets[0], IPPROTO_TCP, TCP_RXTLS_ENABLE, en,
	    sizeof(*en)) == 0);
	check_tls_mode(tc, sockets[0], TCP_RXTLS_MODE);

	fd_set_blocking(sockets[0]);
	fd_set_blocking(sockets[1]);

	outbuf_len = encrypt_tls_record(tc, en, TLS_RLTYPE_APP, seqno,
	    plaintext, len, outbuf, outbuf_cap, 0);

	hdr = (void *)outbuf;
	hdr->tls_vmajor++;

	rv = write(sockets[1], outbuf, outbuf_len);
	ATF_REQUIRE_INTEQ((ssize_t)outbuf_len, rv);

	ktls_receive_tls_error(sockets[0], EINVAL);

	free(outbuf);
	free(plaintext);

	close_sockets_ignore_errors(sockets);
}

static void
test_ktls_receive_bad_minor(const atf_tc_t *tc, struct tls_enable *en,
    uint64_t seqno, size_t len)
{
	struct tls_record_layer *hdr;
	char *plaintext, *outbuf;
	size_t outbuf_cap, outbuf_len;
	ssize_t rv;
	int sockets[2];

	ATF_REQUIRE(len <= TLS_MAX_MSG_SIZE_V10_2);

	plaintext = alloc_buffer(len);
	outbuf_cap = tls_header_len(en) + len + tls_trailer_len(en);
	outbuf = malloc(outbuf_cap);

	ATF_REQUIRE_MSG(open_sockets(tc, sockets), "failed to create sockets");

	ATF_REQUIRE(setsockopt(sockets[0], IPPROTO_TCP, TCP_RXTLS_ENABLE, en,
	    sizeof(*en)) == 0);
	check_tls_mode(tc, sockets[0], TCP_RXTLS_MODE);

	fd_set_blocking(sockets[0]);
	fd_set_blocking(sockets[1]);

	outbuf_len = encrypt_tls_record(tc, en, TLS_RLTYPE_APP, seqno,
	    plaintext, len, outbuf, outbuf_cap, 0);

	hdr = (void *)outbuf;
	hdr->tls_vminor++;

	rv = write(sockets[1], outbuf, outbuf_len);
	ATF_REQUIRE_INTEQ((ssize_t)outbuf_len, rv);

	ktls_receive_tls_error(sockets[0], EINVAL);

	free(outbuf);
	free(plaintext);

	close_sockets_ignore_errors(sockets);
}

static void
test_ktls_receive_bad_type(const atf_tc_t *tc, struct tls_enable *en,
    uint64_t seqno, size_t len)
{
	struct tls_record_layer *hdr;
	char *plaintext, *outbuf;
	size_t outbuf_cap, outbuf_len;
	ssize_t rv;
	int sockets[2];

	ATF_REQUIRE(len <= TLS_MAX_MSG_SIZE_V10_2);
	ATF_REQUIRE_INTEQ(TLS_MINOR_VER_THREE, en->tls_vminor);

	plaintext = alloc_buffer(len);
	outbuf_cap = tls_header_len(en) + len + tls_trailer_len(en);
	outbuf = malloc(outbuf_cap);

	ATF_REQUIRE_MSG(open_sockets(tc, sockets), "failed to create sockets");

	ATF_REQUIRE(setsockopt(sockets[0], IPPROTO_TCP, TCP_RXTLS_ENABLE, en,
	    sizeof(*en)) == 0);
	check_tls_mode(tc, sockets[0], TCP_RXTLS_MODE);

	fd_set_blocking(sockets[0]);
	fd_set_blocking(sockets[1]);

	outbuf_len = encrypt_tls_record(tc, en, 0x21 /* Alert */, seqno,
	    plaintext, len, outbuf, outbuf_cap, 0);

	hdr = (void *)outbuf;
	hdr->tls_type = TLS_RLTYPE_APP + 1;

	rv = write(sockets[1], outbuf, outbuf_len);
	ATF_REQUIRE_INTEQ((ssize_t)outbuf_len, rv);

	ktls_receive_tls_error(sockets[0], EINVAL);

	free(outbuf);
	free(plaintext);

	close_sockets_ignore_errors(sockets);
}

static void
test_ktls_receive_bad_size(const atf_tc_t *tc, struct tls_enable *en,
    uint64_t seqno, size_t len)
{
	struct tls_record_layer *hdr;
	char *outbuf;
	size_t outbuf_len;
	ssize_t rv;
	int sockets[2];

	outbuf_len = sizeof(*hdr) + len;
	outbuf = calloc(1, outbuf_len);

	ATF_REQUIRE_MSG(open_sockets(tc, sockets), "failed to create sockets");

	ATF_REQUIRE(setsockopt(sockets[0], IPPROTO_TCP, TCP_RXTLS_ENABLE, en,
	    sizeof(*en)) == 0);
	check_tls_mode(tc, sockets[0], TCP_RXTLS_MODE);

	fd_set_blocking(sockets[0]);
	fd_set_blocking(sockets[1]);

	hdr = (void *)outbuf;
	hdr->tls_vmajor = en->tls_vmajor;
	if (en->tls_vminor == TLS_MINOR_VER_THREE)
		hdr->tls_vminor = TLS_MINOR_VER_TWO;
	else
		hdr->tls_vminor = en->tls_vminor;
	hdr->tls_type = TLS_RLTYPE_APP;
	hdr->tls_length = htons(len);

	rv = write(sockets[1], outbuf, outbuf_len);
	ATF_REQUIRE_INTEQ((ssize_t)outbuf_len, rv);

	/*
	 * The other end may notice the error and drop the connection
	 * before this executes resulting in shutdown() failing with
	 * either ENOTCONN or ECONNRESET.  Ignore this error if it
	 * occurs.
	 */
	if (shutdown(sockets[1], SHUT_WR) != 0) {
		ATF_REQUIRE_MSG(errno == ENOTCONN || errno == ECONNRESET,
		    "shutdown() failed: %s", strerror(errno));
	}

	ktls_receive_tls_error(sockets[0], EMSGSIZE);

	free(outbuf);

	close_sockets_ignore_errors(sockets);
}

#define	TLS_10_TESTS(M)							\
	M(aes128_cbc_1_0_sha1, CRYPTO_AES_CBC, 128 / 8,			\
	    CRYPTO_SHA1_HMAC, TLS_MINOR_VER_ZERO)			\
	M(aes256_cbc_1_0_sha1, CRYPTO_AES_CBC, 256 / 8,			\
	    CRYPTO_SHA1_HMAC, TLS_MINOR_VER_ZERO)

#define	TLS_13_TESTS(M)							\
	M(aes128_gcm_1_3, CRYPTO_AES_NIST_GCM_16, 128 / 8, 0,		\
	    TLS_MINOR_VER_THREE)					\
	M(aes256_gcm_1_3, CRYPTO_AES_NIST_GCM_16, 256 / 8, 0,		\
	    TLS_MINOR_VER_THREE)					\
	M(chacha20_poly1305_1_3, CRYPTO_CHACHA20_POLY1305, 256 / 8, 0,	\
	    TLS_MINOR_VER_THREE)

#define	AES_CBC_NONZERO_TESTS(M)					\
	M(aes128_cbc_1_1_sha1, CRYPTO_AES_CBC, 128 / 8,			\
	    CRYPTO_SHA1_HMAC, TLS_MINOR_VER_ONE)			\
	M(aes256_cbc_1_1_sha1, CRYPTO_AES_CBC, 256 / 8,			\
	    CRYPTO_SHA1_HMAC, TLS_MINOR_VER_ONE)			\
	M(aes128_cbc_1_2_sha1, CRYPTO_AES_CBC, 128 / 8,			\
	    CRYPTO_SHA1_HMAC, TLS_MINOR_VER_TWO)			\
	M(aes256_cbc_1_2_sha1, CRYPTO_AES_CBC, 256 / 8,			\
	    CRYPTO_SHA1_HMAC, TLS_MINOR_VER_TWO)			\
	M(aes128_cbc_1_2_sha256, CRYPTO_AES_CBC, 128 / 8,		\
	    CRYPTO_SHA2_256_HMAC, TLS_MINOR_VER_TWO)			\
	M(aes256_cbc_1_2_sha256, CRYPTO_AES_CBC, 256 / 8,		\
	    CRYPTO_SHA2_256_HMAC, TLS_MINOR_VER_TWO)			\
	M(aes128_cbc_1_2_sha384, CRYPTO_AES_CBC, 128 / 8,		\
	    CRYPTO_SHA2_384_HMAC, TLS_MINOR_VER_TWO)			\
	M(aes256_cbc_1_2_sha384, CRYPTO_AES_CBC, 256 / 8,		\
	    CRYPTO_SHA2_384_HMAC, TLS_MINOR_VER_TWO)			\

#define	AES_CBC_TESTS(M)						\
	TLS_10_TESTS(M)							\
	AES_CBC_NONZERO_TESTS(M)

#define AES_GCM_12_TESTS(M)						\
	M(aes128_gcm_1_2, CRYPTO_AES_NIST_GCM_16, 128 / 8, 0,		\
	    TLS_MINOR_VER_TWO)						\
	M(aes256_gcm_1_2, CRYPTO_AES_NIST_GCM_16, 256 / 8, 0,		\
	    TLS_MINOR_VER_TWO)

#define AES_GCM_TESTS(M)						\
	AES_GCM_12_TESTS(M)						\
	M(aes128_gcm_1_3, CRYPTO_AES_NIST_GCM_16, 128 / 8, 0,		\
	    TLS_MINOR_VER_THREE)					\
	M(aes256_gcm_1_3, CRYPTO_AES_NIST_GCM_16, 256 / 8, 0,		\
	    TLS_MINOR_VER_THREE)

#define CHACHA20_TESTS(M)						\
	M(chacha20_poly1305_1_2, CRYPTO_CHACHA20_POLY1305, 256 / 8, 0,	\
	    TLS_MINOR_VER_TWO)						\
	M(chacha20_poly1305_1_3, CRYPTO_CHACHA20_POLY1305, 256 / 8, 0,	\
	    TLS_MINOR_VER_THREE)

#define GEN_TRANSMIT_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, name, len)					\
ATF_TC_WITHOUT_HEAD(ktls_transmit_##cipher_name##_##name);		\
ATF_TC_BODY(ktls_transmit_##cipher_name##_##name, tc)			\
{									\
	struct tls_enable en;						\
	uint64_t seqno;							\
									\
	ATF_REQUIRE_KTLS();						\
	seqno = random();						\
	build_tls_enable(tc, cipher_alg, key_size, auth_alg, minor,	\
	    seqno, &en);						\
	test_ktls_transmit_app_data(tc, &en, seqno, len);		\
	free_tls_enable(&en);						\
}

#define ADD_TRANSMIT_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, name)					\
	ATF_TP_ADD_TC(tp, ktls_transmit_##cipher_name##_##name);

#define GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, name, type, len)				\
ATF_TC_WITHOUT_HEAD(ktls_transmit_##cipher_name##_##name);		\
ATF_TC_BODY(ktls_transmit_##cipher_name##_##name, tc)			\
{									\
	struct tls_enable en;						\
	uint64_t seqno;							\
									\
	ATF_REQUIRE_KTLS();						\
	seqno = random();						\
	build_tls_enable(tc, cipher_alg, key_size, auth_alg, minor,	\
	    seqno, &en);						\
	test_ktls_transmit_control(tc, &en, seqno, type, len);		\
	free_tls_enable(&en);						\
}

#define ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, name)					\
	ATF_TP_ADD_TC(tp, ktls_transmit_##cipher_name##_##name);

#define GEN_TRANSMIT_EMPTY_FRAGMENT_TEST(cipher_name, cipher_alg,	\
	    key_size, auth_alg, minor)					\
ATF_TC_WITHOUT_HEAD(ktls_transmit_##cipher_name##_empty_fragment);	\
ATF_TC_BODY(ktls_transmit_##cipher_name##_empty_fragment, tc)		\
{									\
	struct tls_enable en;						\
	uint64_t seqno;							\
									\
	ATF_REQUIRE_KTLS();						\
	seqno = random();						\
	build_tls_enable(tc, cipher_alg, key_size, auth_alg, minor,	\
	    seqno, &en);						\
	test_ktls_transmit_empty_fragment(tc, &en, seqno);		\
	free_tls_enable(&en);						\
}

#define ADD_TRANSMIT_EMPTY_FRAGMENT_TEST(cipher_name, cipher_alg,	\
	    key_size, auth_alg, minor)					\
	ATF_TP_ADD_TC(tp, ktls_transmit_##cipher_name##_empty_fragment);

#define GEN_TRANSMIT_TESTS(cipher_name, cipher_alg, key_size, auth_alg,	\
	    minor)							\
	GEN_TRANSMIT_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, short, 64)					\
	GEN_TRANSMIT_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, long, 64 * 1024)				\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, control, 0x21 /* Alert */, 32)

#define ADD_TRANSMIT_TESTS(cipher_name, cipher_alg, key_size, auth_alg,	\
	    minor)							\
	ADD_TRANSMIT_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, short)					\
	ADD_TRANSMIT_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, long)					\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, control)

/*
 * For each supported cipher suite, run three transmit tests:
 *
 * - a short test which sends 64 bytes of application data (likely as
 *   a single TLS record)
 *
 * - a long test which sends 64KB of application data (split across
 *   multiple TLS records)
 *
 * - a control test which sends a single record with a specific
 *   content type via sendmsg()
 */
AES_CBC_TESTS(GEN_TRANSMIT_TESTS);
AES_GCM_TESTS(GEN_TRANSMIT_TESTS);
CHACHA20_TESTS(GEN_TRANSMIT_TESTS);

#define GEN_TRANSMIT_PADDING_TESTS(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_1, 0x21 /* Alert */, 1)		\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_2, 0x21 /* Alert */, 2)		\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_3, 0x21 /* Alert */, 3)		\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_4, 0x21 /* Alert */, 4)		\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_5, 0x21 /* Alert */, 5)		\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_6, 0x21 /* Alert */, 6)		\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_7, 0x21 /* Alert */, 7)		\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_8, 0x21 /* Alert */, 8)		\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_9, 0x21 /* Alert */, 9)		\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_10, 0x21 /* Alert */, 10)		\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_11, 0x21 /* Alert */, 11)		\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_12, 0x21 /* Alert */, 12)		\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_13, 0x21 /* Alert */, 13)		\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_14, 0x21 /* Alert */, 14)		\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_15, 0x21 /* Alert */, 15)		\
	GEN_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_16, 0x21 /* Alert */, 16)

#define ADD_TRANSMIT_PADDING_TESTS(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_1)					\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_2)					\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_3)					\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_4)					\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_5)					\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_6)					\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_7)					\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_8)					\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_9)					\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_10)				\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_11)				\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_12)				\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_13)				\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_14)				\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_15)				\
	ADD_TRANSMIT_CONTROL_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_16)

/*
 * For AES-CBC MTE cipher suites using padding, add tests of messages
 * with each possible padding size.  Note that the padding_<N> tests
 * do not necessarily test <N> bytes of padding as the padding is a
 * function of the cipher suite's MAC length.  However, cycling
 * through all of the payload sizes from 1 to 16 should exercise all
 * of the possible padding lengths for each suite.
 */
AES_CBC_TESTS(GEN_TRANSMIT_PADDING_TESTS);

/*
 * Test "empty fragments" which are TLS records with no payload that
 * OpenSSL can send for TLS 1.0 connections.
 */
AES_CBC_TESTS(GEN_TRANSMIT_EMPTY_FRAGMENT_TEST);
AES_GCM_TESTS(GEN_TRANSMIT_EMPTY_FRAGMENT_TEST);
CHACHA20_TESTS(GEN_TRANSMIT_EMPTY_FRAGMENT_TEST);

static void
test_ktls_invalid_transmit_cipher_suite(const atf_tc_t *tc,
    struct tls_enable *en)
{
	int sockets[2];

	ATF_REQUIRE_MSG(open_sockets(tc, sockets), "failed to create sockets");

	ATF_REQUIRE_ERRNO(EINVAL, setsockopt(sockets[1], IPPROTO_TCP,
	    TCP_TXTLS_ENABLE, en, sizeof(*en)) == -1);

	close_sockets(sockets);
}

#define GEN_INVALID_TRANSMIT_TEST(name, cipher_alg, key_size, auth_alg,	\
	    minor)							\
ATF_TC_WITHOUT_HEAD(ktls_transmit_invalid_##name);			\
ATF_TC_BODY(ktls_transmit_invalid_##name, tc)				\
{									\
	struct tls_enable en;						\
	uint64_t seqno;							\
									\
	ATF_REQUIRE_KTLS();						\
	seqno = random();						\
	build_tls_enable(tc, cipher_alg, key_size, auth_alg, minor,	\
	    seqno, &en);						\
	test_ktls_invalid_transmit_cipher_suite(tc, &en);		\
	free_tls_enable(&en);						\
}

#define ADD_INVALID_TRANSMIT_TEST(name, cipher_alg, key_size, auth_alg, \
	    minor)							\
	ATF_TP_ADD_TC(tp, ktls_transmit_invalid_##name);

#define	INVALID_CIPHER_SUITES(M)					\
	M(aes128_cbc_1_0_sha256, CRYPTO_AES_CBC, 128 / 8,		\
	    CRYPTO_SHA2_256_HMAC, TLS_MINOR_VER_ZERO)			\
	M(aes128_cbc_1_0_sha384, CRYPTO_AES_CBC, 128 / 8,		\
	    CRYPTO_SHA2_384_HMAC, TLS_MINOR_VER_ZERO)			\
	M(aes128_gcm_1_0, CRYPTO_AES_NIST_GCM_16, 128 / 8, 0,		\
	    TLS_MINOR_VER_ZERO)						\
	M(chacha20_poly1305_1_0, CRYPTO_CHACHA20_POLY1305, 256 / 8, 0,	\
	    TLS_MINOR_VER_ZERO)						\
	M(aes128_cbc_1_1_sha256, CRYPTO_AES_CBC, 128 / 8,		\
	    CRYPTO_SHA2_256_HMAC, TLS_MINOR_VER_ONE)			\
	M(aes128_cbc_1_1_sha384, CRYPTO_AES_CBC, 128 / 8,		\
	    CRYPTO_SHA2_384_HMAC, TLS_MINOR_VER_ONE)			\
	M(aes128_gcm_1_1, CRYPTO_AES_NIST_GCM_16, 128 / 8, 0,		\
	    TLS_MINOR_VER_ONE)						\
	M(chacha20_poly1305_1_1, CRYPTO_CHACHA20_POLY1305, 256 / 8, 0,	\
	    TLS_MINOR_VER_ONE)						\
	M(aes128_cbc_1_3_sha1, CRYPTO_AES_CBC, 128 / 8,			\
	    CRYPTO_SHA1_HMAC, TLS_MINOR_VER_THREE)			\
	M(aes128_cbc_1_3_sha256, CRYPTO_AES_CBC, 128 / 8,		\
	    CRYPTO_SHA2_256_HMAC, TLS_MINOR_VER_THREE)			\
	M(aes128_cbc_1_3_sha384, CRYPTO_AES_CBC, 128 / 8,		\
	    CRYPTO_SHA2_384_HMAC, TLS_MINOR_VER_THREE)

/*
 * Ensure that invalid cipher suites are rejected for transmit.
 */
INVALID_CIPHER_SUITES(GEN_INVALID_TRANSMIT_TEST);

#define GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, name, len, padding)			\
ATF_TC_WITHOUT_HEAD(ktls_receive_##cipher_name##_##name);		\
ATF_TC_BODY(ktls_receive_##cipher_name##_##name, tc)			\
{									\
	struct tls_enable en;						\
	uint64_t seqno;							\
									\
	ATF_REQUIRE_KTLS();						\
	seqno = random();						\
	build_tls_enable(tc, cipher_alg, key_size, auth_alg, minor,	\
	    seqno, &en);						\
	test_ktls_receive_app_data(tc, &en, seqno, len, padding);	\
	free_tls_enable(&en);						\
}

#define ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, name)					\
	ATF_TP_ADD_TC(tp, ktls_receive_##cipher_name##_##name);

#define GEN_RECEIVE_BAD_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, len)					\
ATF_TC_WITHOUT_HEAD(ktls_receive_##cipher_name##_bad_data);		\
ATF_TC_BODY(ktls_receive_##cipher_name##_bad_data, tc)			\
{									\
	struct tls_enable en;						\
	uint64_t seqno;							\
									\
	ATF_REQUIRE_KTLS();						\
	seqno = random();						\
	build_tls_enable(tc, cipher_alg, key_size, auth_alg, minor,	\
	    seqno, &en);						\
	test_ktls_receive_corrupted_data(tc, &en, seqno, len);		\
	free_tls_enable(&en);						\
}

#define ADD_RECEIVE_BAD_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ATF_TP_ADD_TC(tp, ktls_receive_##cipher_name##_bad_data);

#define GEN_RECEIVE_BAD_MAC_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, len)					\
ATF_TC_WITHOUT_HEAD(ktls_receive_##cipher_name##_bad_mac);		\
ATF_TC_BODY(ktls_receive_##cipher_name##_bad_mac, tc)			\
{									\
	struct tls_enable en;						\
	uint64_t seqno;							\
									\
	ATF_REQUIRE_KTLS();						\
	seqno = random();						\
	build_tls_enable(tc, cipher_alg, key_size, auth_alg, minor,	\
	    seqno, &en);						\
	test_ktls_receive_corrupted_mac(tc, &en, seqno, len);		\
	free_tls_enable(&en);						\
}

#define ADD_RECEIVE_BAD_MAC_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ATF_TP_ADD_TC(tp, ktls_receive_##cipher_name##_bad_mac);

#define GEN_RECEIVE_TRUNCATED_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, len)					\
ATF_TC_WITHOUT_HEAD(ktls_receive_##cipher_name##_truncated_record);	\
ATF_TC_BODY(ktls_receive_##cipher_name##_truncated_record, tc)		\
{									\
	struct tls_enable en;						\
	uint64_t seqno;							\
									\
	ATF_REQUIRE_KTLS();						\
	seqno = random();						\
	build_tls_enable(tc, cipher_alg, key_size, auth_alg, minor,	\
	    seqno, &en);						\
	test_ktls_receive_truncated_record(tc, &en, seqno, len);	\
	free_tls_enable(&en);						\
}

#define ADD_RECEIVE_TRUNCATED_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ATF_TP_ADD_TC(tp, ktls_receive_##cipher_name##_truncated_record);

#define GEN_RECEIVE_BAD_MAJOR_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, len)					\
ATF_TC_WITHOUT_HEAD(ktls_receive_##cipher_name##_bad_major);		\
ATF_TC_BODY(ktls_receive_##cipher_name##_bad_major, tc)			\
{									\
	struct tls_enable en;						\
	uint64_t seqno;							\
									\
	ATF_REQUIRE_KTLS();						\
	seqno = random();						\
	build_tls_enable(tc, cipher_alg, key_size, auth_alg, minor,	\
	    seqno, &en);						\
	test_ktls_receive_bad_major(tc, &en, seqno, len);		\
	free_tls_enable(&en);						\
}

#define ADD_RECEIVE_BAD_MAJOR_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ATF_TP_ADD_TC(tp, ktls_receive_##cipher_name##_bad_major);

#define GEN_RECEIVE_BAD_MINOR_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, len)					\
ATF_TC_WITHOUT_HEAD(ktls_receive_##cipher_name##_bad_minor);		\
ATF_TC_BODY(ktls_receive_##cipher_name##_bad_minor, tc)			\
{									\
	struct tls_enable en;						\
	uint64_t seqno;							\
									\
	ATF_REQUIRE_KTLS();						\
	seqno = random();						\
	build_tls_enable(tc, cipher_alg, key_size, auth_alg, minor,	\
	    seqno, &en);						\
	test_ktls_receive_bad_minor(tc, &en, seqno, len);		\
	free_tls_enable(&en);						\
}

#define ADD_RECEIVE_BAD_MINOR_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ATF_TP_ADD_TC(tp, ktls_receive_##cipher_name##_bad_minor);

#define GEN_RECEIVE_BAD_SIZE_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, name, len)					\
ATF_TC_WITHOUT_HEAD(ktls_receive_##cipher_name##_##name);		\
ATF_TC_BODY(ktls_receive_##cipher_name##_##name, tc)			\
{									\
	struct tls_enable en;						\
	uint64_t seqno;							\
									\
	ATF_REQUIRE_KTLS();						\
	seqno = random();						\
	build_tls_enable(tc, cipher_alg, key_size, auth_alg, minor,	\
	    seqno, &en);						\
	test_ktls_receive_bad_size(tc, &en, seqno, (len));		\
	free_tls_enable(&en);						\
}

#define ADD_RECEIVE_BAD_SIZE_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, name)					\
	ATF_TP_ADD_TC(tp, ktls_receive_##cipher_name##_##name);

#define GEN_RECEIVE_TESTS(cipher_name, cipher_alg, key_size, auth_alg,	\
	    minor)							\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, short, 64, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, long, 64 * 1024, 0)			\
	GEN_RECEIVE_BAD_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, 64)					\
	GEN_RECEIVE_BAD_MAC_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, 64)					\
	GEN_RECEIVE_TRUNCATED_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, 64)					\
	GEN_RECEIVE_BAD_MAJOR_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, 64)					\
	GEN_RECEIVE_BAD_MINOR_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, 64)					\
	GEN_RECEIVE_BAD_SIZE_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, small_record,				\
	    tls_minimum_record_payload(&en) - 1)			\
	GEN_RECEIVE_BAD_SIZE_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, oversized_record,				\
	    TLS_MAX_MSG_SIZE_V10_2 * 2)

#define ADD_RECEIVE_TESTS(cipher_name, cipher_alg, key_size, auth_alg,	\
	    minor)							\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, short)					\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, long)					\
	ADD_RECEIVE_BAD_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ADD_RECEIVE_BAD_MAC_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ADD_RECEIVE_TRUNCATED_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ADD_RECEIVE_BAD_MAJOR_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ADD_RECEIVE_BAD_MINOR_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ADD_RECEIVE_BAD_SIZE_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, small_record)				\
	ADD_RECEIVE_BAD_SIZE_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, oversized_record)

/*
 * For each supported cipher suite, run several receive tests:
 *
 * - a short test which sends 64 bytes of application data (likely as
 *   a single TLS record)
 *
 * - a long test which sends 64KB of application data (split across
 *   multiple TLS records)
 *
 * - a test with corrupted payload data in a single TLS record
 *
 * - a test with a corrupted MAC in a single TLS record
 *
 * - a test with a truncated TLS record
 *
 * - tests with invalid TLS major and minor versions
 *
 * - a tests with a record whose is one less than the smallest valid
 *   size
 *
 * - a test with an oversized TLS record
 */
AES_CBC_NONZERO_TESTS(GEN_RECEIVE_TESTS);
AES_GCM_TESTS(GEN_RECEIVE_TESTS);
CHACHA20_TESTS(GEN_RECEIVE_TESTS);

#define	GEN_RECEIVE_MTE_PADDING_TESTS(cipher_name, cipher_alg,		\
	    key_size, auth_alg, minor)					\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_1, 1, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_2, 2, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_3, 3, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_4, 4, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_5, 5, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_6, 6, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_7, 7, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_8, 8, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_9, 9, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_10, 10, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_11, 11, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_12, 12, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_13, 13, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_14, 14, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_15, 15, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_16, 16, 0)				\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_16_extra, 16, 16)			\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_32_extra, 16, 32)

#define ADD_RECEIVE_MTE_PADDING_TESTS(cipher_name, cipher_alg,		\
	    key_size, auth_alg, minor)					\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_1)					\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_2)					\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_3)					\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_4)					\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_5)					\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_6)					\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_7)					\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_8)					\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_9)					\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_10)				\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_11)				\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_12)				\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_13)				\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_14)				\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_15)				\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_16)				\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_16_extra)				\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, padding_32_extra)

#define GEN_RECEIVE_BAD_PADDING_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, len)					\
ATF_TC_WITHOUT_HEAD(ktls_receive_##cipher_name##_bad_padding);		\
ATF_TC_BODY(ktls_receive_##cipher_name##_bad_padding, tc)		\
{									\
	struct tls_enable en;						\
	uint64_t seqno;							\
									\
	ATF_REQUIRE_KTLS();						\
	seqno = random();						\
	build_tls_enable(tc, cipher_alg, key_size, auth_alg, minor,	\
	    seqno, &en);						\
	test_ktls_receive_corrupted_padding(tc, &en, seqno, len);	\
	free_tls_enable(&en);						\
}

#define ADD_RECEIVE_BAD_PADDING_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ATF_TP_ADD_TC(tp, ktls_receive_##cipher_name##_bad_padding);

#define	GEN_RECEIVE_MTE_TESTS(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	GEN_RECEIVE_MTE_PADDING_TESTS(cipher_name, cipher_alg,		\
	    key_size, auth_alg, minor)					\
	GEN_RECEIVE_BAD_PADDING_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, 64)					\
	GEN_RECEIVE_BAD_SIZE_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, non_block_size,				\
	    tls_minimum_record_payload(&en) + 1)

#define	ADD_RECEIVE_MTE_TESTS(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ADD_RECEIVE_MTE_PADDING_TESTS(cipher_name, cipher_alg,		\
	    key_size, auth_alg, minor)					\
	ADD_RECEIVE_BAD_PADDING_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ADD_RECEIVE_BAD_SIZE_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, non_block_size)

/*
 * For AES-CBC MTE cipher suites using padding, add tests of messages
 * with each possible padding size.  Note that the padding_<N> tests
 * do not necessarily test <N> bytes of padding as the padding is a
 * function of the cipher suite's MAC length.  However, cycling
 * through all of the payload sizes from 1 to 16 should exercise all
 * of the possible padding lengths for each suite.
 *
 * Two additional tests check for additional padding with an extra
 * 16 or 32 bytes beyond the normal padding.
 *
 * Another test checks for corrupted padding.
 *
 * Another test checks for a record whose payload is not a multiple of
 * the AES block size.
 */
AES_CBC_NONZERO_TESTS(GEN_RECEIVE_MTE_TESTS);

#define GEN_RECEIVE_BAD_IV_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
ATF_TC_WITHOUT_HEAD(ktls_receive_##cipher_name##_bad_iv);		\
ATF_TC_BODY(ktls_receive_##cipher_name##_bad_iv, tc)			\
{									\
	struct tls_enable en;						\
	uint64_t seqno;							\
									\
	ATF_REQUIRE_KTLS();						\
	seqno = random();						\
	build_tls_enable(tc, cipher_alg, key_size, auth_alg, minor,	\
	    seqno, &en);						\
	test_ktls_receive_corrupted_iv(tc, &en, seqno, 64);		\
	free_tls_enable(&en);						\
}

#define ADD_RECEIVE_BAD_IV_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ATF_TP_ADD_TC(tp, ktls_receive_##cipher_name##_bad_iv);

#define	GEN_RECEIVE_EXPLICIT_IV_TESTS(cipher_name, cipher_alg,		\
	    key_size, auth_alg, minor)					\
	GEN_RECEIVE_BAD_IV_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	GEN_RECEIVE_BAD_SIZE_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, short_header,				\
	    sizeof(struct tls_record_layer) + 1)

#define	ADD_RECEIVE_EXPLICIT_IV_TESTS(cipher_name, cipher_alg,		\
	    key_size, auth_alg, minor)					\
	ADD_RECEIVE_BAD_IV_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ADD_RECEIVE_BAD_SIZE_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, short_header)

/*
 * For cipher suites with an explicit IV, run a receive test where the
 * explicit IV has been corrupted.  Also run a receive test that sends
 * a short record without a complete IV.
 */
AES_CBC_NONZERO_TESTS(GEN_RECEIVE_EXPLICIT_IV_TESTS);
AES_GCM_12_TESTS(GEN_RECEIVE_EXPLICIT_IV_TESTS);

#define GEN_RECEIVE_BAD_TYPE_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, len)					\
ATF_TC_WITHOUT_HEAD(ktls_receive_##cipher_name##_bad_type);		\
ATF_TC_BODY(ktls_receive_##cipher_name##_bad_type, tc)			\
{									\
	struct tls_enable en;						\
	uint64_t seqno;							\
									\
	ATF_REQUIRE_KTLS();						\
	seqno = random();						\
	build_tls_enable(tc, cipher_alg, key_size, auth_alg, minor,	\
	    seqno, &en);						\
	test_ktls_receive_bad_type(tc, &en, seqno, len);		\
	free_tls_enable(&en);						\
}

#define ADD_RECEIVE_BAD_TYPE_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ATF_TP_ADD_TC(tp, ktls_receive_##cipher_name##_bad_type);

#define GEN_RECEIVE_TLS13_TESTS(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, short_padded, 64, 16)			\
	GEN_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, long_padded, 64 * 1024, 15)		\
	GEN_RECEIVE_BAD_TYPE_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, 64)

#define ADD_RECEIVE_TLS13_TESTS(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, short_padded)				\
	ADD_RECEIVE_APP_DATA_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor, long_padded)				\
	ADD_RECEIVE_BAD_TYPE_TEST(cipher_name, cipher_alg, key_size,	\
	    auth_alg, minor)

/*
 * For TLS 1.3 cipher suites, run two additional receive tests which
 * use add padding to each record.  Also run a test that uses an
 * invalid "outer" record type.
 */
TLS_13_TESTS(GEN_RECEIVE_TLS13_TESTS);

static void
test_ktls_invalid_receive_cipher_suite(const atf_tc_t *tc,
    struct tls_enable *en)
{
	int sockets[2];

	ATF_REQUIRE_MSG(open_sockets(tc, sockets), "failed to create sockets");

	ATF_REQUIRE_ERRNO(EINVAL, setsockopt(sockets[1], IPPROTO_TCP,
	    TCP_RXTLS_ENABLE, en, sizeof(*en)) == -1);

	close_sockets(sockets);
}

#define GEN_INVALID_RECEIVE_TEST(name, cipher_alg, key_size, auth_alg,	\
	    minor)							\
ATF_TC_WITHOUT_HEAD(ktls_receive_invalid_##name);			\
ATF_TC_BODY(ktls_receive_invalid_##name, tc)				\
{									\
	struct tls_enable en;						\
	uint64_t seqno;							\
									\
	ATF_REQUIRE_KTLS();						\
	seqno = random();						\
	build_tls_enable(tc, cipher_alg, key_size, auth_alg, minor,	\
	    seqno, &en);						\
	test_ktls_invalid_receive_cipher_suite(tc, &en);		\
	free_tls_enable(&en);						\
}

#define ADD_INVALID_RECEIVE_TEST(name, cipher_alg, key_size, auth_alg,	\
	    minor)							\
	ATF_TP_ADD_TC(tp, ktls_receive_invalid_##name);

/*
 * Ensure that invalid cipher suites are rejected for receive.
 */
INVALID_CIPHER_SUITES(GEN_INVALID_RECEIVE_TEST);

static void
test_ktls_unsupported_receive_cipher_suite(const atf_tc_t *tc,
    struct tls_enable *en)
{
	int sockets[2];

	ATF_REQUIRE_MSG(open_sockets(tc, sockets), "failed to create sockets");

	ATF_REQUIRE_ERRNO(EPROTONOSUPPORT, setsockopt(sockets[1], IPPROTO_TCP,
	    TCP_RXTLS_ENABLE, en, sizeof(*en)) == -1);

	close_sockets(sockets);
}

#define GEN_UNSUPPORTED_RECEIVE_TEST(name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
ATF_TC_WITHOUT_HEAD(ktls_receive_unsupported_##name);			\
ATF_TC_BODY(ktls_receive_unsupported_##name, tc)			\
{									\
	struct tls_enable en;						\
	uint64_t seqno;							\
									\
	ATF_REQUIRE_KTLS();						\
	seqno = random();						\
	build_tls_enable(tc, cipher_alg, key_size, auth_alg, minor,	\
	    seqno, &en);						\
	test_ktls_unsupported_receive_cipher_suite(tc, &en);		\
	free_tls_enable(&en);						\
}

#define ADD_UNSUPPORTED_RECEIVE_TEST(name, cipher_alg, key_size,	\
	    auth_alg, minor)						\
	ATF_TP_ADD_TC(tp, ktls_receive_unsupported_##name);

/*
 * Ensure that valid cipher suites not supported for receive are
 * rejected.
 */
TLS_10_TESTS(GEN_UNSUPPORTED_RECEIVE_TEST);

/*
 * Try to perform an invalid sendto(2) on a TXTLS-enabled socket, to exercise
 * KTLS error handling in the socket layer.
 */
ATF_TC_WITHOUT_HEAD(ktls_sendto_baddst);
ATF_TC_BODY(ktls_sendto_baddst, tc)
{
	char buf[32];
	struct sockaddr_in dst;
	struct tls_enable en;
	ssize_t n;
	int s;

	ATF_REQUIRE_KTLS();

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	ATF_REQUIRE(s >= 0);

	build_tls_enable(tc, CRYPTO_AES_NIST_GCM_16, 128 / 8, 0,
	    TLS_MINOR_VER_THREE, (uint64_t)random(), &en);

	ATF_REQUIRE(setsockopt(s, IPPROTO_TCP, TCP_TXTLS_ENABLE, &en,
	    sizeof(en)) == 0);

	memset(&dst, 0, sizeof(dst));
	dst.sin_family = AF_INET;
	dst.sin_len = sizeof(dst);
	dst.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	dst.sin_port = htons(12345);

	memset(buf, 0, sizeof(buf));
	n = sendto(s, buf, sizeof(buf), 0, (struct sockaddr *)&dst,
	    sizeof(dst));

	/* Can't transmit to the broadcast address over TCP. */
	ATF_REQUIRE_ERRNO(EACCES, n == -1);
	ATF_REQUIRE(close(s) == 0);
}

/*
 * Make sure that listen(2) returns an error for KTLS-enabled sockets, and
 * verify that an attempt to enable KTLS on a listening socket fails.
 */
ATF_TC_WITHOUT_HEAD(ktls_listening_socket);
ATF_TC_BODY(ktls_listening_socket, tc)
{
	struct tls_enable en;
	struct sockaddr_in sin;
	int s;

	ATF_REQUIRE_KTLS();

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	ATF_REQUIRE(s >= 0);
	build_tls_enable(tc, CRYPTO_AES_NIST_GCM_16, 128 / 8, 0,
	    TLS_MINOR_VER_THREE, (uint64_t)random(), &en);
	ATF_REQUIRE(setsockopt(s, IPPROTO_TCP, TCP_TXTLS_ENABLE, &en,
	    sizeof(en)) == 0);
	ATF_REQUIRE_ERRNO(EINVAL, listen(s, 1) == -1);
	ATF_REQUIRE(close(s) == 0);

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	ATF_REQUIRE(s >= 0);
	build_tls_enable(tc, CRYPTO_AES_NIST_GCM_16, 128 / 8, 0,
	    TLS_MINOR_VER_THREE, (uint64_t)random(), &en);
	ATF_REQUIRE(setsockopt(s, IPPROTO_TCP, TCP_RXTLS_ENABLE, &en,
	    sizeof(en)) == 0);
	ATF_REQUIRE_ERRNO(EINVAL, listen(s, 1) == -1);
	ATF_REQUIRE(close(s) == 0);

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	ATF_REQUIRE(s >= 0);
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	ATF_REQUIRE(bind(s, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	ATF_REQUIRE(listen(s, 1) == 0);
	build_tls_enable(tc, CRYPTO_AES_NIST_GCM_16, 128 / 8, 0,
	    TLS_MINOR_VER_THREE, (uint64_t)random(), &en);
	ATF_REQUIRE_ERRNO(ENOTCONN,
	    setsockopt(s, IPPROTO_TCP, TCP_TXTLS_ENABLE, &en, sizeof(en)) != 0);
	ATF_REQUIRE_ERRNO(ENOTCONN,
	    setsockopt(s, IPPROTO_TCP, TCP_RXTLS_ENABLE, &en, sizeof(en)) != 0);
	ATF_REQUIRE(close(s) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	/* Transmit tests */
	AES_CBC_TESTS(ADD_TRANSMIT_TESTS);
	AES_GCM_TESTS(ADD_TRANSMIT_TESTS);
	CHACHA20_TESTS(ADD_TRANSMIT_TESTS);
	AES_CBC_TESTS(ADD_TRANSMIT_PADDING_TESTS);
	AES_CBC_TESTS(ADD_TRANSMIT_EMPTY_FRAGMENT_TEST);
	AES_GCM_TESTS(ADD_TRANSMIT_EMPTY_FRAGMENT_TEST);
	CHACHA20_TESTS(ADD_TRANSMIT_EMPTY_FRAGMENT_TEST);
	INVALID_CIPHER_SUITES(ADD_INVALID_TRANSMIT_TEST);

	/* Receive tests */
	TLS_10_TESTS(ADD_UNSUPPORTED_RECEIVE_TEST);
	AES_CBC_NONZERO_TESTS(ADD_RECEIVE_TESTS);
	AES_GCM_TESTS(ADD_RECEIVE_TESTS);
	CHACHA20_TESTS(ADD_RECEIVE_TESTS);
	AES_CBC_NONZERO_TESTS(ADD_RECEIVE_MTE_TESTS);
	AES_CBC_NONZERO_TESTS(ADD_RECEIVE_EXPLICIT_IV_TESTS);
	AES_GCM_12_TESTS(ADD_RECEIVE_EXPLICIT_IV_TESTS);
	TLS_13_TESTS(ADD_RECEIVE_TLS13_TESTS);
	INVALID_CIPHER_SUITES(ADD_INVALID_RECEIVE_TEST);

	/* Miscellaneous */
	ATF_TP_ADD_TC(tp, ktls_sendto_baddst);
	ATF_TP_ADD_TC(tp, ktls_listening_socket);

	return (atf_no_error());
}
