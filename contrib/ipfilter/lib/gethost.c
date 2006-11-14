/*	$FreeBSD$	*/

#include "ipf.h"

int gethost(name, hostp)
char *name;
u_32_t *hostp;
{
	struct hostent *h;
	struct netent *n;
	u_32_t addr;

	if (!strcmp(name, "test.host.dots")) {
		*hostp = htonl(0xfedcba98);
		return 0;
	}

	if (!strcmp(name, "<thishost>"))
		name = thishost;

	h = gethostbyname(name);
	if (h != NULL) {
		if ((h->h_addr != NULL) && (h->h_length == sizeof(addr))) {
			bcopy(h->h_addr, (char *)&addr, sizeof(addr));
			*hostp = addr;
			return 0;
		}
	}

	n = getnetbyname(name);
	if (n != NULL) {
		*hostp = (u_32_t)htonl(n->n_net & 0xffffffff);
		return 0;
	}
	return -1;
}
