/*
 *	MD5 interface for rsaref2.0
 *
 * These routines implement an interface for the RSA Laboratories
 * implementation of the Message Digest 5 (MD5) algorithm. This
 * algorithm is included in the rsaref2.0 package available from RSA in
 * the US and foreign countries. Further information is available at
 * www.rsa.com.
 */

#include "ntp_machine.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef MD5
#include <stdio.h>

#include "ntp_types.h"
#include "ntp_fp.h"
#include "ntp_string.h"
#include "global.h"
#include "md5.h"
#include "ntp_stdlib.h"

#define BLOCK_OCTETS	16	/* message digest size */
#define NTP_MAXKEY	65535	/* max identifier from ntp.h */


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
	MD5_CTX ctx;
	u_char digest[BLOCK_OCTETS];
	int i;

	/*
	 * MD5 with key identifier concatenated with packet.
	 */
	MD5Init(&ctx);
	MD5Update(&ctx, key, (u_int)cache_keylen);
	MD5Update(&ctx, (u_char *)pkt, (u_int)length);
	MD5Final(digest, &ctx);
	i = length / 4;
	memmove((char *)&pkt[i + 1], (char *)digest, BLOCK_OCTETS);
	return (BLOCK_OCTETS + 4);
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
	int length, 	/* packet length */
	int size		/* MAC size */
	)
{
	MD5_CTX ctx;
	u_char digest[BLOCK_OCTETS];

	/*
	 * MD5 with key identifier concatenated with packet.
	 */
	if (size != BLOCK_OCTETS + 4)
		return (0);
	MD5Init(&ctx);
	MD5Update(&ctx, key, (u_int)cache_keylen);
	MD5Update(&ctx, (u_char *)pkt, (u_int)length);
	MD5Final(digest, &ctx);
	return (!memcmp((char *)digest, (char *)pkt + length + 4,
		BLOCK_OCTETS));
}


/*
 * session_key - generate session key from supplied plaintext.
 *
 * Returns hashed session key for validation.
 */
u_long
session_key(
	u_int32 srcadr, 	/* source address */
	u_int32 dstadr, 	/* destination address */
	u_long keyno,		/* key identifier */
	u_long lifetime 	/* key lifetime */
	)
{
	MD5_CTX ctx;
	u_int32 header[3];
	u_long keyid;
	u_char digest[BLOCK_OCTETS];

	/*
	 * Generate the session key and retrieve the hash for later. If
	 * the lifetime is greater than zero, call the key trusted.
	 */
	header[0] = htonl(srcadr);
	header[1] = htonl(dstadr);
	header[2] = htonl(keyno);
	MD5Init(&ctx);
	MD5Update(&ctx, (u_char *)header, sizeof(header));
	MD5Final(digest, &ctx);
	memcpy(&keyid, digest, 4);
	if (lifetime != 0) {
		MD5auth_setkey(keyno, digest, BLOCK_OCTETS);
		authtrust(keyno, (int)lifetime);
	}
#ifdef DEBUG
	if (debug > 1)
		printf(
			"session_key: from %s to %s keyid %08lx hash %08lx life %ld\n",
			numtoa(htonl(srcadr)), numtoa(htonl(dstadr)), keyno,
			keyid, lifetime);
#endif
	return (keyid);
}
#endif /* MD5 */
