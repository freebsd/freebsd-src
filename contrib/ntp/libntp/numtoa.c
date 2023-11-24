/*
 * numtoa - return asciized network numbers store in local array space
 */
#include <config.h>

#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>		/* ntohl */
#endif

#include <stdio.h>
#include <ctype.h>

#include "ntp_fp.h"
#include "lib_strbuf.h"
#include "ntp_stdlib.h"

char *
numtoa(
	u_int32 num
	)
{
	register u_int32 netnum;
	register char *buf;

	netnum = ntohl(num);
	LIB_GETBUF(buf);
	snprintf(buf, LIB_BUFLENGTH, "%lu.%lu.%lu.%lu",
		 ((u_long)netnum >> 24) & 0xff,
		 ((u_long)netnum >> 16) & 0xff,
		 ((u_long)netnum >> 8) & 0xff,
		 (u_long)netnum & 0xff);
	return buf;
}


/* Convert a refid & stratum to a string */
const char *
refid_str(
	u_int32	refid,
	int	stratum
	)
{
	char *	text;
	size_t	tlen;
	char *  cp;

	if (stratum > 1)
		return numtoa(refid);

	LIB_GETBUF(text);
	text[0] = '.';
	/* What if any non-NUL char is not printable? */
	memcpy(&text[1], &refid, sizeof(refid));
	text[1 + sizeof(refid)] = '\0';
	tlen = strlen(text);
	text[tlen] = '.';
	text[tlen + 1] = '\0';

	/*
	 * Now make sure the contents are 'graphic'.
	 *
	 * This refid is expected to be up to 4 ascii graphics.
	 * If any character is not a graphic, replace it with a space.
	 * This will at least alert the viewer of a problem.
	 */
	for (cp = text + 1; *cp; ++cp) {
		if (!isgraph((int)*cp)) {
			*cp = ' ';
		}
	}

	return text;
}

