/*
 *  md5crypt - MD5 based authentication routines
 */

#include "ntp_types.h"
#include "ntp_string.h"
#include "md5.h"
#include "ntp_stdlib.h"

extern u_long cache_keyid;
extern char *cache_key;
extern int cache_keylen;

/*
 * Stat counters, imported from data base module
 */
extern U_LONG authencryptions;
extern U_LONG authdecryptions;
extern U_LONG authkeyuncached;
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
MD5authencrypt(keyno, pkt, length)
    u_long keyno;
    U_LONG *pkt;
    int length;		/* length of encrypted portion of packet */
{
    MD5_CTX ctx;
    int len;		/* in 4 byte quantities */

    authencryptions++;

    if (keyno != cache_keyid) {
	authkeyuncached++;
	if (!authhavekey(keyno)) {
	    authnokey++;
	    return 0;
	}
    }

    len = length / sizeof(U_LONG);

    /*
     *  Generate the authenticator.
     */
    MD5Init(&ctx);
    MD5Update(&ctx, cache_key, cache_keylen);
    MD5Update(&ctx, (char *)pkt, length);
    MD5Final(&ctx);

    memmove((char *)&pkt[NOCRYPT_LONGS + len],
	    (char *)ctx.digest,
	    BLOCK_OCTETS);
    return (4 + BLOCK_OCTETS);	/* return size of key and MAC  */
}
