/*
 * authreadkeys.c - routines to support the reading of the key file
 */
#include <stdio.h>
#include <ctype.h>

#include "ntp_fp.h"
#include "ntp.h"
#include "ntp_stdlib.h"
#include "ntp_syslog.h"

#ifdef	DES
/*
 * Types of ascii representations for keys.  "Standard" means a 64 bit
 * hex number in NBS format, i.e. with the low order bit of each byte
 * a parity bit.  "NTP" means a 64 bit key in NTP format, with the
 * high order bit of each byte a parity bit.  "Ascii" means a 1-to-8
 * character string whose ascii representation is used as the key.
 */
#define	KEY_TYPE_STD	1
#define	KEY_TYPE_NTP	2
#define	KEY_TYPE_ASCII	3
#endif

#ifdef	MD5
/*
 *  Arbitrary long string of ASCII characters.
 */
#define	KEY_TYPE_MD5	4
#endif

/* Forwards */
static char *nexttok P((char **));

/*
 * nexttok - basic internal tokenizing routine
 */
static char *
nexttok(
	char **str
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
	    return 0;
	
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
	FILE *fp;
	char *line;
	char *token;
	u_long keyno;
	int keytype;
	char buf[512];		/* lots of room for line */

	/*
	 * Open file.  Complain and return if it can't be opened.
	 */
	fp = fopen(file, "r");
	if (fp == NULL) {
		msyslog(LOG_ERR, "can't open key file %s: %m", file);
		return 0;
	}

	/*
	 * Remove all existing keys
	 */
	auth_delkeys();

	/*
	 * Now read lines from the file, looking for key entries
	 */
	while ((line = fgets(buf, sizeof buf, fp)) != NULL) {
		token = nexttok(&line);
		if (token == 0)
		    continue;
		
		/*
		 * First is key number.  See if it is okay.
		 */
		keyno = atoi(token);
		if (keyno == 0) {
			msyslog(LOG_ERR,
				"cannot change keyid 0, key entry `%s' ignored",
				token);
			continue;
		}

		if (keyno > NTP_MAXKEY) {
			msyslog(LOG_ERR,
				"keyid's > %d reserved for autokey, key entry `%s' ignored",
				NTP_MAXKEY, token);
			continue;
		}

		/*
		 * Next is keytype.  See if that is all right.
		 */
		token = nexttok(&line);
		if (token == 0) {
			msyslog(LOG_ERR,
				"no key type for key number %ld, entry ignored",
				keyno);
			continue;
		}
		switch (*token) {
#ifdef	DES
		    case 'S':
		    case 's':
			keytype = KEY_TYPE_STD; break;

		    case 'N':
		    case 'n':
			keytype = KEY_TYPE_NTP; break;

		    case 'A':
		    case 'a':
			keytype = KEY_TYPE_ASCII; break;
#endif
#ifdef	MD5
		    case 'M':
		    case 'm':
			keytype = KEY_TYPE_MD5; break;
#endif
		    default:
			msyslog(LOG_ERR,
				"invalid key type for key number %ld, entry ignored",
				keyno);
			continue;
		}

		/*
		 * Finally, get key and insert it
		 */
		token = nexttok(&line);
		if (token == 0) {
			msyslog(LOG_ERR,
				"no key for number %ld entry, entry ignored",
				keyno);
		} else {
			switch(keytype) {
#ifdef	DES
			    case KEY_TYPE_STD:
			    case KEY_TYPE_NTP:
			    case KEY_TYPE_ASCII:
				if (!authusekey(keyno, keytype,
						(u_char *)token))
				    msyslog(LOG_ERR,
					    "format/parity error for DES key %ld, not used",
					    keyno);
				break;
#endif
#ifdef	MD5
			    case KEY_TYPE_MD5:
				if (!authusekey(keyno, keytype,
						(u_char *)token))
				    msyslog(LOG_ERR,
					    "format/parity error for MD5 key %ld, not used",
					    keyno);
				break;
#endif
			}
		}
	}
	(void) fclose(fp);
	return 1;
}
