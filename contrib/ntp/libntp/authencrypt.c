/*
 * DES interface for rsaref2.0
 *
 * These routines implement an interface for the RSA Laboratories
 * implementation of the Data Encryption Standard (DES) algorithm
 * operating in Cipher-Block Chaining (CBC) mode. This algorithm is
 * included in the rsaref2.0 package available from RSA in the US and
 * foreign countries. Further information is available at www.rsa.com.
 */

#include "ntp_machine.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef DES
#include "ntp_types.h"
#include "ntp_fp.h"
#include "ntp_string.h"
#include "global.h"
#include "des.h"
#include "ntp_stdlib.h"

#define BLOCK_OCTETS	8	/* message digest size */
#define MAXTPKT 	128 /* max packet size */


/*
 * DESauthencrypt - generate DES-CBC message authenticator
 *
 * Returns length of authenticator field.
 */
int
DESauthencrypt(
	u_char *key,		/* key pointer */
	u_int32 *pkt,		/* packet pointer */
	int length		/* packet length */
	)
{
	DES_CBC_CTX ctx;
	u_int32 tpkt[MAXTPKT];
	u_int32 work[2];
	int i, j;

	/*
	 * DES-CBC with zero IV. Note the encrypted text is discarded.
	 */
	work[0] = work[1] = 0;
	DES_CBCInit(&ctx, key, (u_char *)work, 1);
	DES_CBCUpdate(&ctx, (u_char *)tpkt, (u_char *)pkt,
		(u_int)length);
	i = length / 4 + 1;
	j = i - 3;
	pkt[i++] = (u_int32)htonl(tpkt[j++]);
	pkt[i] = (u_int32)htonl(tpkt[j]);
	return (BLOCK_OCTETS + 4);
}


/*
 * DESauthdecrypt - verify DES message authenticator
 *
 * Returns one if authenticator valid, zero if invalid.
 */
int
DESauthdecrypt(
	u_char *key,		/* key pointer */
	u_int32 *pkt,		/* packet pointer */
	int length, 	/* packet length */
	int size		/* size of MAC field */
	)
{
	DES_CBC_CTX ctx;
	u_int32 tpkt[MAXTPKT];
	u_int32 work[2];
	int i, j;

	/*
	 * DES-CBC with zero IV. Note the encrypted text is discarded.
	 */
	if (size != BLOCK_OCTETS + 4)
		return (0);
	work[0] = work[1] = 0;
	DES_CBCInit (&ctx, key, (u_char *)work, 1);
	DES_CBCUpdate (&ctx, (u_char *)tpkt, (u_char *)pkt,
		(u_int)length);
	i = length / 4 + 1;
	j = i - 3;
	if ((u_int32)ntohl(pkt[i++]) == tpkt[j++] &&
		(u_int32)ntohl(pkt[i]) == tpkt[j])
		return (1);
	return (0);
}
#else
int authencrypt_bs;
#endif /* DES */
