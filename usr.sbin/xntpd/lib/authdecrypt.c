/* authdecrypt.c,v 3.1 1993/07/06 01:07:44 jbj Exp
 * authdecrypt - routine to decrypt a packet to see if this guy knows our key.
 */
#include "ntp_stdlib.h"

/*
 * For our purposes an NTP packet looks like:
 *
 *	a variable amount of unencrypted data, multiple of 8 bytes, followed by:
 *	NOCRYPT_OCTETS worth of unencrypted data, followed by:
 *	BLOCK_OCTETS worth of ciphered checksum.
 */ 
#define	NOCRYPT_OCTETS	4
#define	BLOCK_OCTETS	8

#define	NOCRYPT_LONGS	((NOCRYPT_OCTETS)/sizeof(U_LONG))
#define	BLOCK_LONGS	((BLOCK_OCTETS)/sizeof(U_LONG))

/*
 * Imported from the key data base module
 */
extern U_LONG cache_keyid;	/* cached key ID */
extern u_char DEScache_dkeys[];	/* cached decryption keys */
extern u_char DESzerodkeys[];	/* zero key decryption keys */

/*
 * Stat counters, imported from data base module
 */
extern U_LONG authdecryptions;
extern U_LONG authkeyuncached;
extern U_LONG authdecryptok;

int
DESauthdecrypt(keyno, pkt, length)
	U_LONG keyno;
	const U_LONG *pkt;
	int length;	/* length of variable data in octets */
{
	register const U_LONG *pd;
	register int i;
	register u_char *keys;
	register int longlen;
	U_LONG work[2];

	authdecryptions++;
	
	if (keyno == 0)
		keys = DESzerodkeys;
	else {
		if (keyno != cache_keyid) {
			authkeyuncached++;
			if (!authhavekey(keyno))
				return 0;
		}
		keys = DEScache_dkeys;
	}

	/*
	 * Get encryption block data in host byte order and decrypt it.
	 */
	longlen = length / sizeof(U_LONG);
	pd = pkt + longlen;		/* points at NOCRYPT area */
	work[0] = *(pd + NOCRYPT_LONGS);
	work[1] = *(pd + NOCRYPT_LONGS + 1);

	if (longlen & 0x1) {
		DESauth_des(work, keys);
		work[0] ^= *(--pd);
	}

	for (i = longlen/2; i > 0; i--) {
		DESauth_des(work, keys);
		work[1] ^= *(--pd);
		work[0] ^= *(--pd);
	}

	/*
	 * Success if the encryption data is zero
	 */
	if ((work[0] == 0) && (work[1] == 0)) {
		authdecryptok++;
		return 1;
	}
	return 0;
}
