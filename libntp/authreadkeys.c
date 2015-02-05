/*
 * authreadkeys.c - routines to support the reading of the key file
 */
#include <config.h>
#include <stdio.h>
#include <ctype.h>

#include "ntp_fp.h"
#include "ntp.h"
#include "ntp_syslog.h"
#include "ntp_stdlib.h"

#ifdef OPENSSL
#include "openssl/objects.h"
#include "openssl/evp.h"
#endif	/* OPENSSL */

/* Forwards */
static char *nexttok (char **);

/*
 * nexttok - basic internal tokenizing routine
 */
static char *
nexttok(
	char	**str
	)
{
	register char *cp;
	char *starttok;

	cp = *str;

	/*
	 * Space past white space
	 */
	while (*cp == ' ' || *cp == '\t')
		cp++;
	
	/*
	 * Save this and space to end of token
	 */
	starttok = cp;
	while (*cp != '\0' && *cp != '\n' && *cp != ' '
	       && *cp != '\t' && *cp != '#')
		cp++;
	
	/*
	 * If token length is zero return an error, else set end of
	 * token to zero and return start.
	 */
	if (starttok == cp)
		return NULL;
	
	if (*cp == ' ' || *cp == '\t')
		*cp++ = '\0';
	else
		*cp = '\0';
	
	*str = cp;
	return starttok;
}


/*
 * authreadkeys - (re)read keys from a file.
 */
int
authreadkeys(
	const char *file
	)
{
	FILE	*fp;
	char	*line;
	char	*token;
	keyid_t	keyno;
	int	keytype;
	char	buf[512];		/* lots of room for line */
	u_char	keystr[32];		/* Bug 2537 */
	size_t	len;
	size_t	j;

	/*
	 * Open file.  Complain and return if it can't be opened.
	 */
	fp = fopen(file, "r");
	if (fp == NULL) {
		msyslog(LOG_ERR, "authreadkeys: file %s: %m",
		    file);
		return (0);
	}
	INIT_SSL();

	/*
	 * Remove all existing keys
	 */
	auth_delkeys();

	/*
	 * Now read lines from the file, looking for key entries
	 */
	while ((line = fgets(buf, sizeof buf, fp)) != NULL) {
		token = nexttok(&line);
		if (token == NULL)
			continue;
		
		/*
		 * First is key number.  See if it is okay.
		 */
		keyno = atoi(token);
		if (keyno == 0) {
			msyslog(LOG_ERR,
			    "authreadkeys: cannot change key %s", token);
			continue;
		}

		if (keyno > NTP_MAXKEY) {
			msyslog(LOG_ERR,
			    "authreadkeys: key %s > %d reserved for Autokey",
			    token, NTP_MAXKEY);
			continue;
		}

		/*
		 * Next is keytype. See if that is all right.
		 */
		token = nexttok(&line);
		if (token == NULL) {
			msyslog(LOG_ERR,
			    "authreadkeys: no key type for key %d", keyno);
			continue;
		}
#ifdef OPENSSL
		/*
		 * The key type is the NID used by the message digest 
		 * algorithm. There are a number of inconsistencies in
		 * the OpenSSL database. We attempt to discover them
		 * here and prevent use of inconsistent data later.
		 */
		keytype = keytype_from_text(token, NULL);
		if (keytype == 0) {
			msyslog(LOG_ERR,
			    "authreadkeys: invalid type for key %d", keyno);
			continue;
		}
		if (EVP_get_digestbynid(keytype) == NULL) {
			msyslog(LOG_ERR,
			    "authreadkeys: no algorithm for key %d", keyno);
			continue;
		}
#else	/* !OPENSSL follows */

		/*
		 * The key type is unused, but is required to be 'M' or
		 * 'm' for compatibility.
		 */
		if (!(*token == 'M' || *token == 'm')) {
			msyslog(LOG_ERR,
			    "authreadkeys: invalid type for key %d", keyno);
			continue;
		}
		keytype = KEY_TYPE_MD5;
#endif	/* !OPENSSL */

		/*
		 * Finally, get key and insert it. If it is longer than 20
		 * characters, it is a binary string encoded in hex;
		 * otherwise, it is a text string of printable ASCII
		 * characters.
		 */
		token = nexttok(&line);
		if (token == NULL) {
			msyslog(LOG_ERR,
			    "authreadkeys: no key for key %d", keyno);
			continue;
		}
		len = strlen(token);
		if (len <= 20) {	/* Bug 2537 */
			MD5auth_setkey(keyno, keytype, (u_char *)token, len);
		} else {
			char	hex[] = "0123456789abcdef";
			u_char	temp;
			char	*ptr;
			size_t	jlim;

			jlim = min(len, 2 * sizeof(keystr));
			for (j = 0; j < jlim; j++) {
				ptr = strchr(hex, tolower((unsigned char)token[j]));
				if (ptr == NULL)
					break;	/* abort decoding */
				temp = (u_char)(ptr - hex);
				if (j & 1)
					keystr[j / 2] |= temp;
				else
					keystr[j / 2] = temp << 4;
			}
			if (j < jlim) {
				msyslog(LOG_ERR,
					"authreadkeys: invalid hex digit for key %d", keyno);
				continue;
			}
			MD5auth_setkey(keyno, keytype, keystr, jlim / 2);
		}
	}
	fclose(fp);
	return (1);
}
