/* keyparity.c,v 3.1 1993/07/06 01:04:57 jbj Exp
 * keyparity - add parity bits to key and/or change an ascii key to binary
 */

#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>

#include "ntp_string.h"
#include "ntp_stdlib.h"

#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

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

#define	STD_PARITY_BITS	0x01010101

char *progname;
int debug;

int ntpflag = 0;
int stdflag = 0;
int asciiflag = 0;
int ntpoutflag = 0;
int gotoopt = 0;

static	int	parity	P((U_LONG *));
static	int	decodekey P((int, char *, U_LONG *));
static	void	output	P((U_LONG *, int));

/*
 * main - parse arguments and handle options
 */
void
main(argc, argv)
int argc;
char *argv[];
{
	int c;
	int errflg = 0;
	int keytype;
	U_LONG key[2];
	extern int optind;
	extern char *optarg;

	progname = argv[0];
	while ((c = getopt_l(argc, argv, "adno:s")) != EOF)
		switch (c) {
		case 'a':
			asciiflag = 1;
			break;
		case 'd':
			++debug;
			break;
		case 'n':
			ntpflag = 1;
			break;
		case 's':
			stdflag = 1;
			break;
		case 'o':
			if (*optarg == 'n') {
				ntpoutflag = 1;
				gotoopt = 1;
			} else if (*optarg == 's') {
				ntpoutflag = 0;
				gotoopt = 1;
			} else {
				(void) fprintf(stderr,
				    "%s: output format must be `n' or `s'\n",
				    progname);
				errflg++;
			}
			break;
		default:
			errflg++;
			break;
		}
	if (errflg || optind == argc) {
		(void) fprintf(stderr,
		    "usage: %s -n|-s [-a] [-o n|s] key [...]\n",
		    progname);
		exit(2);
	}

	if (!ntpflag && !stdflag) {
		(void) fprintf(stderr,
		    "%s: one of either the -n or -s flags must be specified\n",
		    progname);
		exit(2);
	}

	if (ntpflag && stdflag) {
		(void) fprintf(stderr,
		    "%s: only one of the -n and -s flags may be specified\n",
		    progname);
		exit(2);
	}

	if (!gotoopt) {
		if (ntpflag)
			ntpoutflag = 1;
	}

	if (asciiflag)
		keytype = KEY_TYPE_ASCII;
	else if (ntpflag)
		keytype = KEY_TYPE_NTP;
	else
		keytype = KEY_TYPE_STD;

	for (; optind < argc; optind++) {
		if (!decodekey(keytype, argv[optind], key)) {
			(void) fprintf(stderr,
			    "%s: format of key %s invalid\n",
			    progname, argv[optind]);
			exit(1);
		}
		(void) parity(key);
		output(key, ntpoutflag);
	}
	exit(0);
}



/*
 * parity - set parity on a key/check for odd parity
 */
static int
parity(key)
	U_LONG *key;
{
	U_LONG mask;
	int parity_err;
	int bitcount;
	int half;
	int byte;
	int i;

	/*
	 * Go through counting bits in each byte.  Check to see if
	 * each parity bit was set correctly.  If not, note the error
	 * and set it right.
	 */
	parity_err = 0;
	for (half = 0; half < 2; half++) {		/* two halves of key */
		mask = 0x80000000;
		for (byte = 0; byte < 4; byte++) {	/* 4 bytes per half */
			bitcount = 0;
			for (i = 0; i < 7; i++) {	/* 7 data bits / byte */
				if (key[half] & mask)
					bitcount++;
				mask >>= 1;
			}

			/*
			 * If bitcount is even, parity must be set.  If
			 * bitcount is odd, parity must be clear.
			 */
			if ((bitcount & 0x1) == 0) {
				if (!(key[half] & mask)) {
					parity_err++;
					key[half] |= mask;
				}
			} else {
				if (key[half] & mask) {
					parity_err++;
					key[half] &= ~mask;
				}
			}
			mask >>= 1;
		}
	}

	/*
	 * Return the result of the parity check.
	 */
	return (parity_err == 0);
}


static int
decodekey(keytype, str, key)
	int keytype;
	char *str;
	U_LONG *key;
{
	u_char keybytes[8];
	char *cp;
	char *xdigit;
	int len;
	int i;
	static char *hex = "0123456789abcdef";

	cp = str;
	len = strlen(cp);
	if (len == 0)
		return 0;

	switch(keytype) {
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
			key[i>>3] |= (U_LONG)(xdigit - hex) & 0xf;
		}

		/*
		 * If this is an NTP format key, put it into NBS format
		 */
		if (keytype == KEY_TYPE_NTP) {
			for (i = 0; i < 2; i++)
				key[i] = ((key[i] << 1) & ~STD_PARITY_BITS)
				    | ((key[i] >> 7) & STD_PARITY_BITS);
		}
		break;
	
	case KEY_TYPE_ASCII:
		/*
		 * Make up key from ascii representation
		 */
		bzero(keybytes, sizeof(keybytes));
		for (i = 0; i < 8 && i < len; i++)
			keybytes[i] = *cp++ << 1;
		key[0] = keybytes[0] << 24 | keybytes[1] << 16
		    | keybytes[2] << 8 | keybytes[3];
		key[1] = keybytes[4] << 24 | keybytes[5] << 16
		    | keybytes[6] << 8 | keybytes[7];
		break;
	
	default:
		/* Oh, well */
		return 0;
	}

	return 1;
}


/*
 * output - print a hex key on the standard output
 */
static void
output(key, ntpformat)
	U_LONG *key;
	int ntpformat;
{
	int i;

	if (ntpformat) {
		for (i = 0; i < 2; i++)
			key[i] = ((key[i] & ~STD_PARITY_BITS) >> 1)
			    | ((key[i] & STD_PARITY_BITS) << 7);
	}
	(void) printf("%08x%08x\n", key[0], key[1]);
}
