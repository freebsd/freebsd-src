/*
 * authusekey - decode a key from ascii and use it
 */
#include <stdio.h>
#include <ctype.h>

#include "ntp_types.h"
#include "ntp_string.h"
#include "ntp_stdlib.h"

/*
 * Types of ascii representations for keys.  "Standard" means a 64 bit
 * hex number in NBS format, i.e. with the low order bit of each byte
 * a parity bit.  "NTP" means a 64 bit key in NTP format, with the
 * high order bit of each byte a parity bit.  "Ascii" means a 1-to-8
 * character string whose ascii representation is used as the key.
 */
#ifdef	DES
#define	KEY_TYPE_STD	1
#define	KEY_TYPE_NTP	2
#define	KEY_TYPE_ASCII	3

#define	STD_PARITY_BITS	((unsigned)0x01010101)

#endif

#ifdef	MD5
#define	KEY_TYPE_MD5	4
#endif

int
authusekey(
	u_long keyno,
	int keytype,
	const u_char *str
	)
{
#ifdef DES
	u_int32 key[2];
	u_char keybytes[8];
	char *xdigit;
	int i;
	static const char *hex = "0123456789abcdef";
#endif
	const u_char *cp;
	int len;

	cp = str;
	len = strlen((const char *)cp);
	if (len == 0)
	    return 0;

	switch(keytype) {
#ifdef	DES
	    case KEY_TYPE_STD:
	    case KEY_TYPE_NTP:
		if (len != 16)		/* Lazy.  Should define constant */
		    return 0;
		/*
		 * Decode hex key.
		 */
		key[0] = 0;
		key[1] = 0;
		for (i = 0; i < 16; i++) {
			if (!isascii(*cp))
			    return 0;
			xdigit = strchr(hex, isupper(*cp) ? tolower(*cp) : *cp);
			cp++;
			if (xdigit == 0)
			    return 0;
			key[i>>3] <<= 4;
			key[i>>3] |= (u_int32)(xdigit - hex) & 0xf;
		}

		/*
		 * If this is an NTP format key, put it into NBS format
		 */
		if (keytype == KEY_TYPE_NTP) {
			for (i = 0; i < 2; i++)
			    key[i] = ((key[i] << 1) & ~STD_PARITY_BITS)
				    | ((key[i] >> 7) & STD_PARITY_BITS);
		}

		/*
		 * Check the parity, reject the key if the check fails
		 */
		if (!DESauth_parity(key)) {
			return 0;
		}
		
		/*
		 * We can't find a good reason not to use this key.
		 * So use it.
		 */
		DESauth_setkey(keyno, key);
		break;
	
	    case KEY_TYPE_ASCII:
		/*
		 * Make up key from ascii representation
		 */
		memset((char *) keybytes, 0, sizeof(keybytes));
		for (i = 0; i < 8 && i < len; i++)
		    keybytes[i] = *cp++ << 1;
		key[0] = (u_int32)keybytes[0] << 24 | (u_int32)keybytes[1] << 16
			| (u_int32)keybytes[2] << 8 | (u_int32)keybytes[3];
		key[1] = (u_int32)keybytes[4] << 24 | (u_int32)keybytes[5] << 16
			| (u_int32)keybytes[6] << 8 | (u_int32)keybytes[7];
		
		/*
		 * Set parity on key
		 */
		(void)DESauth_parity(key);

		/*
		 * Now set key in.
		 */
		DESauth_setkey(keyno, key);
		break;
#endif

#ifdef	MD5
	    case KEY_TYPE_MD5:
		MD5auth_setkey(keyno, str, (int)strlen((const char *)str));
		break;
#endif

	    default:
		/* Oh, well */
		return 0;
	}

	return 1;
}
