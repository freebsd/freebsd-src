/*
 *	MD5 interface for rsaref2.0
 *
 * These routines implement an interface for the RSA Laboratories
 * implementation of the Message Digest 5 (MD5) algorithm. This
 * algorithm is included in the rsaref2.0 package available from RSA in
 * the US and foreign countries. Further information is available at
 * www.rsa.com.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ntp_fp.h"
#include "ntp_string.h"
#include "ntp_stdlib.h"

/* Disable the openssl md5 includes, because they'd clash with ours. */
/* #define NO_MD5 */
/* #define OPENSSL_NO_MD5 */
#undef OPENSSL

#include "ntp.h"
#include "global.h"
#include "ntp_md5.h"

/*
 * MD5authencrypt - generate MD5 message authenticator
 *
 * Returns length of authenticator field.
 */
int
MD5authencrypt(
	u_char *key,		/* key pointer */
	u_int32 *pkt,		/* packet pointer */
	int length		/* packet length */
	)
{
	MD5_CTX md5;
	u_char digest[16];

	/*
	 * MD5 with key identifier concatenated with packet.
	 */
	MD5Init(&md5);
	MD5Update(&md5, key, (u_int)cache_keylen);
	MD5Update(&md5, (u_char *)pkt, (u_int)length);
	MD5Final(digest, &md5);
	memmove((u_char *)pkt + length + 4, digest, 16);
	return (16 + 4);
}


/*
 * MD5authdecrypt - verify MD5 message authenticator
 *
 * Returns one if authenticator valid, zero if invalid.
 */
int
MD5authdecrypt(
	u_char *key,		/* key pointer */
	u_int32 *pkt,		/* packet pointer */
	int length,	 	/* packet length */
	int size		/* MAC size */
	)
{
	MD5_CTX md5;
	u_char digest[16];

	/*
	 * MD5 with key identifier concatenated with packet.
	 */
	MD5Init(&md5);
	MD5Update(&md5, key, (u_int)cache_keylen);
	MD5Update(&md5, (u_char *)pkt, (u_int)length);
	MD5Final(digest, &md5);
	if (size != 16 + 4)
		return (0);
	return (!memcmp(digest, (char *)pkt + length + 4, 16));
}

/*
 * Calculate the reference id from the address. If it is an IPv4
 * address, use it as is. If it is an IPv6 address, do a md5 on
 * it and use the bottom 4 bytes.
 */
u_int32
addr2refid(struct sockaddr_storage *addr)
{
	MD5_CTX md5;
	u_char digest[16];
	u_int32 addr_refid;

	if (addr->ss_family == AF_INET)
		return (GET_INADDR(*addr));

	MD5Init(&md5);
	MD5Update(&md5, (u_char *)&GET_INADDR6(*addr),
	    sizeof(struct in6_addr));
	MD5Final(digest, &md5);
	memcpy(&addr_refid, digest, 4);
	return (htonl(addr_refid));
}
