/*
 * is_tar() -- figure out whether file is a tar archive.
 *
 * Stolen (by the author!) from the public domain tar program:
 * Public Domain version written 26 Aug 1985 John Gilmore (ihnp4!hoptoad!gnu).
 *
 * @(#)list.c 1.18 9/23/86 Public Domain - gnu
 * $Id: is_tar.c,v 1.17 2002/07/03 18:26:38 christos Exp $
 *
 * Comments changed and some code/comments reformatted
 * for file command by Ian Darwin.
 */

#include "file.h"
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include "tar.h"

#ifndef lint
FILE_RCSID("@(#)$Id: is_tar.c,v 1.17 2002/07/03 18:26:38 christos Exp $")
#endif

#define	isodigit(c)	( ((c) >= '0') && ((c) <= '7') )

static int from_oct(int, char *);	/* Decode octal number */

/*
 * Return 
 *	0 if the checksum is bad (i.e., probably not a tar archive), 
 *	1 for old UNIX tar file,
 *	2 for Unix Std (POSIX) tar file.
 */
int
is_tar(unsigned char *buf, int nbytes)
{
	union record *header = (union record *)buf;
	int	i;
	int	sum, recsum;
	char	*p;

	if (nbytes < sizeof(union record))
		return 0;

	recsum = from_oct(8,  header->header.chksum);

	sum = 0;
	p = header->charptr;
	for (i = sizeof(union record); --i >= 0;) {
		/*
		 * We can't use unsigned char here because of old compilers,
		 * e.g. V7.
		 */
		sum += 0xFF & *p++;
	}

	/* Adjust checksum to count the "chksum" field as blanks. */
	for (i = sizeof(header->header.chksum); --i >= 0;)
		sum -= 0xFF & header->header.chksum[i];
	sum += ' '* sizeof header->header.chksum;	

	if (sum != recsum)
		return 0;	/* Not a tar archive */
	
	if (0==strcmp(header->header.magic, TMAGIC)) 
		return 2;		/* Unix Standard tar archive */

	return 1;			/* Old fashioned tar archive */
}


/*
 * Quick and dirty octal conversion.
 *
 * Result is -1 if the field is invalid (all blank, or nonoctal).
 */
static int
from_oct(int digs, char *where)
{
	int	value;

	while (isspace((unsigned char)*where)) {	/* Skip spaces */
		where++;
		if (--digs <= 0)
			return -1;		/* All blank field */
	}
	value = 0;
	while (digs > 0 && isodigit(*where)) {	/* Scan til nonoctal */
		value = (value << 3) | (*where++ - '0');
		--digs;
	}

	if (digs > 0 && *where && !isspace((unsigned char)*where))
		return -1;			/* Ended on non-space/nul */

	return value;
}
