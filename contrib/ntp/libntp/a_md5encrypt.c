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

#include <stdio.h>

#include "ntp_types.h"
#include "ntp_string.h"
#include "global.h"
#include "md5.h"
#include "ntp_stdlib.h"

#define BLOCK_OCTETS	16	/* message digest size */


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
