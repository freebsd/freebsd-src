/* numtoa.c,v 3.1 1993/07/06 01:08:38 jbj Exp
 * numtoa - return asciized network numbers store in local array space
 */
#include <stdio.h>

#include "ntp_fp.h"
#include "lib_strbuf.h"
#include "ntp_stdlib.h"

char *
numtoa(num)
	U_LONG num;
{
	register U_LONG netnum;
	register char *buf;

	netnum = ntohl(num);
	LIB_GETBUF(buf);

	(void) sprintf(buf, "%d.%d.%d.%d", (netnum>>24)&0xff,
	    (netnum>>16)&0xff, (netnum>>8)&0xff, netnum&0xff);

	return buf;
}
