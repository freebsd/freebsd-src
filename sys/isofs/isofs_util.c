/*
 *	$Id: isofs_util.c,v 1.4 1993/12/19 00:51:06 wollman Exp $
 */

#include "param.h"
#include "systm.h"

int
isonum_711 (p)
char *p;
{
	return (*p & 0xff);
}

int
isonum_712 (p)
	char *p;
{
	int val;

	val = *p;
	if (val & 0x80)
		val |= ~0xff;
	return (val);
}

int
isonum_721 (p)
	char *p;
{
	return ((p[0] & 0xff) | ((p[1] & 0xff) << 8));
}

int
isonum_722 (p)
char *p;
{
	return (((p[0] & 0xff) << 8) | (p[1] & 0xff));
}

int
isonum_723 (p)
char *p;
{
#if 0
	if (p[0] != p[3] || p[1] != p[2]) {
		fprintf (stderr, "invalid format 7.2.3 number\n");
		exit (1);
	}
#endif
	return (isonum_721 (p));
}

int
isonum_731 (p)
unsigned char *p;
{
	return ((p[0] & 0xff)
		| ((p[1] & 0xff) << 8)
		| ((p[2] & 0xff) << 16)
		| ((p[3] & 0xff) << 24));
}

int
isonum_732 (p)
unsigned char *p;
{
	return (((p[0] & 0xff) << 24)
		| ((p[1] & 0xff) << 16)
		| ((p[2] & 0xff) << 8)
		| (p[3] & 0xff));
}

int
isonum_733 (p)
unsigned char *p;
{
	int i;

#if 0
	for (i = 0; i < 4; i++) {
		if (p[i] != p[7-i]) {
			fprintf (stderr, "bad format 7.3.3 number\n");
			exit (1);
		}
	}
#endif
	return (isonum_731 (p));
}

/*
 * translate and compare a filename
 */
int
isofncmp(char *fn, int fnlen, char *isofn, int isolen) {
	int fnidx;

	fnidx = 0;
	for (fnidx = 0; fnidx < isolen; fnidx++, fn++) {
		char c = *isofn++;

		if (fnidx > fnlen)
			return (0);

		if (c >= 'A' && c <= 'Z') {
			if (c + ('a' - 'A') !=  *fn)
				return(0);
			else
				continue;
		}
		if (c == ';')
			return ((fnidx == fnlen));
		if (c != *fn)
			return (0);
	}
	return (1);
}

/*
 * translate a filename
 */
void
isofntrans(char *infn, int infnlen, char *outfn, short *outfnlen) {
	int fnidx;

	fnidx = 0;
	for (fnidx = 0; fnidx < infnlen; fnidx++) {
		char c = *infn++;

		if (c >= 'A' && c <= 'Z')
			*outfn++ = c + ('a' - 'A');
		else if (c == ';') {
			*outfnlen = fnidx;
			return;
		} else
			*outfn++ = c;
	}
	*outfnlen = infnlen;
}
