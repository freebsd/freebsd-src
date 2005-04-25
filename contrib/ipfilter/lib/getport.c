/*	$NetBSD$	*/

#include "ipf.h"

int getport(fr, name, port)
frentry_t *fr;
char *name;
u_short *port;
{
	struct protoent *p;
	struct servent *s;
	u_short p1;

	if (fr == NULL || fr->fr_type != FR_T_IPF) {
		s = getservbyname(name, NULL);
		if (s != NULL) {
			*port = s->s_port;
			return 0;
		}
		return -1;
	}

	if ((fr->fr_flx & FI_TCPUDP) != 0) {
		/*
		 * If a rule is "tcp/udp" then check that both TCP and UDP
		 * mappings for this protocol name match ports.
		 */
		s = getservbyname(name, "tcp");
		if (s == NULL)
			return -1;
		p1 = s->s_port;
		s = getservbyname(name, "udp");
		if (s == NULL || s->s_port != p1)
			return -1;
		*port = p1;
		return 0;
	}

	p = getprotobynumber(fr->fr_proto);
	s = getservbyname(name, p ? p->p_name : NULL);
	if (s != NULL) {
		*port = s->s_port;
		return 0;
	}
	return -1;
}
