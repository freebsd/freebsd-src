/* numtohost.c,v 3.1 1993/07/06 01:08:40 jbj Exp
 * numtohost - convert network number to host name.
 */
#include <string.h>
#include <netdb.h>

#include "ntp_fp.h"
#include "lib_strbuf.h"
#include "ntp_stdlib.h"

#define	LOOPBACKNET	0x7f000000
#define	LOOPBACKHOST	0x7f000001
#define	LOOPBACKNETMASK	0xff000000

char *
numtohost(netnum)
	U_LONG netnum;
{
	char *bp;
	struct hostent *hp;

	/*
	 * This is really gross, but saves lots of hanging looking for
	 * hostnames for the radio clocks.  Don't bother looking up
	 * addresses on the loopback network except for the loopback
	 * host itself.
	 */
	if ((((ntohl(netnum) & LOOPBACKNETMASK) == LOOPBACKNET)
	    && (ntohl(netnum) != LOOPBACKHOST))
	    || ((hp = gethostbyaddr((char *)&netnum, sizeof netnum, AF_INET))
	      == 0))
		return numtoa(netnum);
	
	LIB_GETBUF(bp);
	
	bp[LIB_BUFLENGTH-1] = '\0';
	(void) strncpy(bp, hp->h_name, LIB_BUFLENGTH-1);
	return bp;
}
