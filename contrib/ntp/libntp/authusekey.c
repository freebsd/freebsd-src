/*
 * authusekey - decode a key from ascii and use it
 */
#include <config.h>
#include <stdio.h>
#include <ctype.h>

#include "ntp_types.h"
#include "ntp_string.h"
#include "ntp_stdlib.h"

/*
 * Only used by ntp{q,dc} to set the key/algo/secret triple to use.
 * Uses the same decoding scheme ntpd uses for keys in the key file.
 */
int
authusekey(
	keyid_t keyno,
	int keytype,
	const u_char *str
	)
{
	size_t	len;
	u_char	buf[AUTHPWD_MAXSECLEN];

	len = authdecodepw(buf, sizeof(buf), (const char*)str,
			   AUTHPWD_UNSPEC);
	if (len < 1 || len > sizeof(buf))
		return 0;

	MD5auth_setkey(keyno, keytype, buf, len, NULL);
	memset(buf, 0, sizeof(buf));
	return 1;
}
