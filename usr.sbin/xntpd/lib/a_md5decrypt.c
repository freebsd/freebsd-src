/* authmd5decrypt.c,v 3.1 1993/07/06 01:07:53 jbj Exp
 *  md5crypt - MD5 based authentication routines
 */

#include "ntp_types.h"
#include "ntp_string.h"
#include "md5.h"
#include "ntp_stdlib.h"

extern U_LONG cache_keyid;
extern char *cache_key;
extern int cache_keylen;

/*
 * Stat counters, imported from data base module
 */
extern U_LONG authencryptions;
extern U_LONG authdecryptions;
extern U_LONG authkeyuncached;
extern U_LONG authdecryptok;
extern U_LONG authnokey;

/*
 * For our purposes an NTP packet looks like:
 *
 *	a variable amount of encrypted data, multiple of 8 bytes, followed by:
 *	NOCRYPT_OCTETS worth of unencrypted data, followed by:
 *	BLOCK_OCTETS worth of ciphered checksum.
 */ 
#define	NOCRYPT_OCTETS	4
#define	BLOCK_OCTETS	16

#define	NOCRYPT_LONGS	((NOCRYPT_OCTETS)/sizeof(U_LONG))
#define	BLOCK_LONGS	((BLOCK_OCTETS)/sizeof(U_LONG))

int
MD5authdecrypt(keyno, pkt, length)
    U_LONG keyno;
    const U_LONG *pkt;
    int length;		/* length of variable data in octets */
{
    MD5_CTX ctx;

    authdecryptions++;

    if (keyno != cache_keyid) {
	authkeyuncached++;
	if (!authhavekey(keyno))
	    return 0;
    }

    MD5Init(&ctx);
    MD5Update(&ctx, cache_key, cache_keylen);
    MD5Update(&ctx, (char *)pkt, length);
    MD5Final(&ctx);

    return (!memcmp((char *)ctx.digest,
		    (char *)pkt + length + 4,
		    BLOCK_OCTETS));
}
