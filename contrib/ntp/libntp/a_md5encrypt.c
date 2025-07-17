/*
 *	digest support for NTP, MD5 and with OpenSSL more
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ntp_fp.h"
#include "ntp_string.h"
#include "ntp_stdlib.h"
#include "ntp.h"
#include "isc/string.h"

typedef struct {
	const void *	buf;
	size_t		len;
} robuffT;

typedef struct {
	void *		buf;
	size_t		len;
} rwbuffT;

#if defined(OPENSSL) && defined(ENABLE_CMAC)
static size_t
cmac_ctx_size(
	CMAC_CTX *	ctx
	)
{
	size_t mlen = 0;

	if (ctx) {
		EVP_CIPHER_CTX * 	cctx;
		if (NULL != (cctx = CMAC_CTX_get0_cipher_ctx (ctx)))
			mlen = EVP_CIPHER_CTX_block_size(cctx);
	}
	return mlen;
}
#endif	/* OPENSSL && ENABLE_CMAC */


/*
 * Allocate and initialize a digest context.  As a speed optimization,
 * take an idea from ntpsec and cache the context to avoid malloc/free
 * overhead in time-critical paths.  ntpsec also caches the algorithms
 * with each key.
 * This is not thread-safe, but that is
 * not a problem at present.
 */
static EVP_MD_CTX *
get_md_ctx(
	int		nid
	)
{
#ifndef OPENSSL
	static MD5_CTX	md5_ctx;

	DEBUG_INSIST(NID_md5 == nid);
	MD5Init(&md5_ctx);

	return &md5_ctx;
#else
	if (!EVP_DigestInit(digest_ctx, EVP_get_digestbynid(nid))) {
		msyslog(LOG_ERR, "%s init failed", OBJ_nid2sn(nid));
		return NULL;
	}

	return digest_ctx;
#endif	/* OPENSSL */
}


static size_t
make_mac(
	const rwbuffT *	digest,
	int		ktype,
	const robuffT *	key,
	const robuffT *	msg
	)
{
	/*
	 * Compute digest of key concatenated with packet. Note: the
	 * key type and digest type have been verified when the key
	 * was created.
	 */
	size_t	retlen = 0;

#ifdef OPENSSL

	INIT_SSL();

	/* Check if CMAC key type specific code required */
#   ifdef ENABLE_CMAC
	if (ktype == NID_cmac) {
		CMAC_CTX *	ctx    = NULL;
		void const *	keyptr = key->buf;
		u_char		keybuf[AES_128_KEY_SIZE];

		/* adjust key size (zero padded buffer) if necessary */
		if (AES_128_KEY_SIZE > key->len) {
			memcpy(keybuf, keyptr, key->len);
			zero_mem((keybuf + key->len),
				 (AES_128_KEY_SIZE - key->len));
			keyptr = keybuf;
		}

		if (NULL == (ctx = CMAC_CTX_new())) {
			msyslog(LOG_ERR, "MAC encrypt: CMAC %s CTX new failed.", CMAC);
			goto cmac_fail;
		}
		if (!CMAC_Init(ctx, keyptr, AES_128_KEY_SIZE, EVP_aes_128_cbc(), NULL)) {
			msyslog(LOG_ERR, "MAC encrypt: CMAC %s Init failed.",    CMAC);
			goto cmac_fail;
		}
		if (cmac_ctx_size(ctx) > digest->len) {
			msyslog(LOG_ERR, "MAC encrypt: CMAC %s buf too small.",  CMAC);
			goto cmac_fail;
		}
		if (!CMAC_Update(ctx, msg->buf, msg->len)) {
			msyslog(LOG_ERR, "MAC encrypt: CMAC %s Update failed.",  CMAC);
			goto cmac_fail;
		}
		if (!CMAC_Final(ctx, digest->buf, &retlen)) {
			msyslog(LOG_ERR, "MAC encrypt: CMAC %s Final failed.",   CMAC);
			retlen = 0;
		}
	  cmac_fail:
		if (ctx)
			CMAC_CTX_free(ctx);
	}
	else
#   endif /* ENABLE_CMAC */
	{	/* generic MAC handling */
		EVP_MD_CTX *	ctx;
		u_int		uilen = 0;

		ctx = get_md_ctx(ktype);
		if (NULL == ctx) {
			goto mac_fail;
		}
		if ((size_t)EVP_MD_CTX_size(ctx) > digest->len) {
			msyslog(LOG_ERR, "MAC encrypt: MAC %s buf too small.",
				OBJ_nid2sn(ktype));
			goto mac_fail;
		}
		if (!EVP_DigestUpdate(ctx, key->buf, (u_int)key->len)) {
			msyslog(LOG_ERR, "MAC encrypt: MAC %s Digest Update key failed.",
				OBJ_nid2sn(ktype));
			goto mac_fail;
		}
		if (!EVP_DigestUpdate(ctx, msg->buf, (u_int)msg->len)) {
			msyslog(LOG_ERR, "MAC encrypt: MAC %s Digest Update data failed.",
				OBJ_nid2sn(ktype));
			goto mac_fail;
		}
		if (!EVP_DigestFinal(ctx, digest->buf, &uilen)) {
			msyslog(LOG_ERR, "MAC encrypt: MAC %s Digest Final failed.",
				OBJ_nid2sn(ktype));
			uilen = 0;
		}
	  mac_fail:
		retlen = (size_t)uilen;
	}

#else /* !OPENSSL follows */

	if (NID_md5 == ktype) {
		EVP_MD_CTX *	ctx;

		ctx = get_md_ctx(ktype);
		if (digest->len < MD5_LENGTH) {
			msyslog(LOG_ERR, "%s", "MAC encrypt: MAC md5 buf too small.");
		} else {
			MD5Init(ctx);
			MD5Update(ctx, (const void *)key->buf, key->len);
			MD5Update(ctx, (const void *)msg->buf, msg->len);
			MD5Final(digest->buf, ctx);
			retlen = MD5_LENGTH;
		}
	} else {
		msyslog(LOG_ERR, "MAC encrypt: invalid key type %d", ktype);
	}

#endif /* !OPENSSL */

	return retlen;
}


/*
 * MD5authencrypt - generate message digest
 *
 * Returns 0 on failure or length of MAC including key ID.
 */
size_t
MD5authencrypt(
	int		type,	/* hash algorithm */
	const u_char *	key,	/* key pointer */
	size_t		klen,	/* key length */
	u_int32 *	pkt,	/* packet pointer */
	size_t		length	/* packet length */
	)
{
	u_char	digest[EVP_MAX_MD_SIZE];
	rwbuffT digb = { digest, sizeof(digest) };
	robuffT keyb = { key, klen };
	robuffT msgb = { pkt, length };
	size_t	dlen;

	dlen = make_mac(&digb, type, &keyb, &msgb);
	if (0 == dlen) {
		return 0;
	}
	memcpy((u_char *)pkt + length + KEY_MAC_LEN, digest,
	       min(dlen, MAX_MDG_LEN));
	return (dlen + KEY_MAC_LEN);
}


/*
 * MD5authdecrypt - verify MD5 message authenticator
 *
 * Returns one if digest valid, zero if invalid.
 */
int
MD5authdecrypt(
	int		type,	/* hash algorithm */
	const u_char *	key,	/* key pointer */
	size_t		klen,	/* key length */
	u_int32	*	pkt,	/* packet pointer */
	size_t		length,	/* packet length */
	size_t		size,	/* MAC size */
	keyid_t		keyno   /* key id (for err log) */
	)
{
	u_char	digest[EVP_MAX_MD_SIZE];
	rwbuffT digb = { digest, sizeof(digest) };
	robuffT keyb = { key, klen };
	robuffT msgb = { pkt, length };
	size_t	dlen = 0;

	dlen = make_mac(&digb, type, &keyb, &msgb);
	if (0 == dlen || size != dlen + KEY_MAC_LEN) {
		msyslog(LOG_ERR,
			"MAC decrypt: MAC length error: %u not %u for key %u",
			(u_int)size, (u_int)(dlen + KEY_MAC_LEN), keyno);
		return FALSE;
	}
	return !isc_tsmemcmp(digest,
		 (u_char *)pkt + length + KEY_MAC_LEN, dlen);
}

/*
 * Calculate the reference id from the address. If it is an IPv4
 * address, use it as is. If it is an IPv6 address, do a md5 on
 * it and use the bottom 4 bytes.
 * The result is in network byte order for IPv4 addreseses.  For
 * IPv6, ntpd long differed in the hash calculated on big-endian
 * vs. little-endian because the first four bytes of the MD5 hash
 * were used as a u_int32 without any byte swapping.  This broke
 * the refid-based loop detection between mixed-endian systems.
 * In order to preserve behavior on the more-common little-endian
 * systems, the hash is now byte-swapped on big-endian systems to
 * match the little-endian hash.  This is ugly but it seems better
 * than changing the IPv6 refid calculation on the more-common
 * systems.
 * This is not thread safe, not a problem so far.
 */
u_int32
addr2refid(sockaddr_u *addr)
{
	static MD5_CTX	md5_ctx;
	union u_tag {
		u_char		digest[MD5_DIGEST_LENGTH];
		u_int32		addr_refid;
	} u;

	if (IS_IPV4(addr)) {
		return (NSRCADR(addr));
	}
	/* MD5 is not used for authentication here. */
	MD5Init(&md5_ctx);
	MD5Update(&md5_ctx, (void *)&SOCK_ADDR6(addr), sizeof(SOCK_ADDR6(addr)));
	MD5Final(u.digest, &md5_ctx);
#ifdef WORDS_BIGENDIAN
	u.addr_refid = BYTESWAP32(u.addr_refid);
#endif
	return u.addr_refid;
}
