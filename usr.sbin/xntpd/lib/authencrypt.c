/* authencrypt.c,v 3.1 1993/07/06 01:07:50 jbj Exp
 * authencrypt - compute and encrypt the mac field in an NTP packet
 */
#include "ntp_stdlib.h"

/*
 * For our purposes an NTP packet looks like:
 *
 *	a variable amount of encrypted data, multiple of 8 bytes, followed by:
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
extern u_char DEScache_ekeys[];	/* cached decryption keys */
extern u_char DESzeroekeys[];	/* zero key decryption keys */

/*
 * Stat counters from the database module
 */
extern U_LONG authencryptions;
extern U_LONG authkeyuncached;
extern U_LONG authnokey;

int
DESauthencrypt(keyno, pkt, length)
	U_LONG keyno;
	U_LONG *pkt;
	int length;	/* length of encrypted portion of packet */
{
	register U_LONG *pd;
	register int i;
	register u_char *keys;
	register int len;
	U_LONG work[2];

	authencryptions++;

	if (keyno == 0) {
		keys = DESzeroekeys;
	} else {
		if (keyno != cache_keyid) {
			authkeyuncached++;
			if (!authhavekey(keyno)) {
				authnokey++;
				return 0;
			}
		}
		keys = DEScache_ekeys;
	}

	/*
	 * Do the encryption.  Work our way forward in the packet, eight
	 * bytes at a time, encrypting as we go.  Note that the byte order
	 * issues are handled by the DES routine itself
	 */
	pd = pkt;
	work[0] = work[1] = 0;
	len = length / sizeof(U_LONG);

	for (i = (len/2); i > 0; i--) {
		work[0] ^= *pd++;
		work[1] ^= *pd++;
		DESauth_des(work, keys);
	}

	if (len & 0x1) {
		work[0] ^= *pd++;
		DESauth_des(work, keys);
	}

	/*
	 * Space past the keyid and stick the result back in the mac field
	 */
	pd += NOCRYPT_LONGS;
	*pd++ = work[0];
	*pd = work[1];

	return 4 + BLOCK_OCTETS;	/* return size of key and MAC  */
}
