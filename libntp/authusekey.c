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
int
authusekey(
	keyid_t keyno,
	int keytype,
	const u_char *str
	)
{
	const u_char *cp;
	int len;

	cp = str;
	len = strlen((const char *)cp);
	if (len == 0)
		return 0;

	MD5auth_setkey(keyno, keytype, str, (int)strlen((const char *)str));
	return 1;
}
