/*	$FreeBSD: src/contrib/ipfilter/lib/getproto.c,v 1.2.2.1 2006/08/24 07:37:05 guido Exp $	*/

#include "ipf.h"

int getproto(name)
char *name;
{
	struct protoent *p;
	char *s;

	for (s = name; *s != '\0'; s++)
		if (!ISDIGIT(*s))
			break;
	if (*s == '\0')
		return atoi(name);

#ifdef _AIX51
	/*
	 * For some bogus reason, "ip" is 252 in /etc/protocols on AIX 5
	 */
	if (!strcasecmp(name, "ip"))
		return 0;
#endif

	p = getprotobyname(name);
	if (p != NULL)
		return p->p_proto;
	return -1;
}
