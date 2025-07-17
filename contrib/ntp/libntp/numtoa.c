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


/* 
 * Convert a refid & stratum to a string.  If stratum is negative and the
 * refid consists entirely of graphic chars, up to an optional
 * terminating zero, display as text similar to stratum 0 & 1.
 */
const char *
refid_str(
	u_int32	refid,
	int	stratum
	)
{
	char *	text;
	size_t	tlen;
	char *	cp;
	int	printable;

	/*
	 * ntpd can have stratum = 0 and refid 127.0.0.1 in orphan mode.
	 * https://bugs.ntp.org/3854.  Mirror the refid logic in timer().
	 */
	if (0 == stratum && LOOPBACKADR_N == refid) {
		return ".ORPH.";
	}
	printable = FALSE;
	if (stratum < 2) {
		text = lib_getbuf();
		text[0] = '.';
		memcpy(&text[1], &refid, sizeof(refid));
		text[1 + sizeof(refid)] = '\0';
		tlen = strlen(text);
		text[tlen] = '.';
		text[tlen + 1] = '\0';
		/*
		 * Now make sure the contents are 'graphic'.
		 *
		 * This refid is expected to be up to 4 printable ASCII.
		 * isgraph() is similar to isprint() but excludes space.
		 * If any character is not graphic, replace it with a '?'.
		 * This will at least alert the viewer of a problem.
		 */
		for (cp = text + 1; '\0' != *cp; ++cp) {
			if (!isgraph((int)*cp)) {
				printable = FALSE;
				*cp = '?';
			}
		}
		if (   (stratum < 0 && printable)
		    || stratum < 2) {
			return text;
		}
	}
	return numtoa(refid);
}

