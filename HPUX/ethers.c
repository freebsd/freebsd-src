/*      $NetBSD: ethers.c,v 1.17 2000/01/22 22:19:14 mycroft Exp $      */

/*
 * ethers(3N) a la Sun.
 *
 * Written by Roland McGrath <roland@frob.com> 10/14/93.
 * Public domain.
 */

#if defined(__hpux) && (HPUXREV >= 1111) && !defined(_KERNEL)
# include <sys/kern_svcs.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if_arp.h>

#include <stdio.h>
#include <errno.h>

#include "ip_compat.h"


struct ether_addr *
ether_aton(s)
	const char *s;
{
	static struct ether_addr n;
	u_int i[6];

	if (sscanf(s, " %x:%x:%x:%x:%x:%x ", &i[0], &i[1],
	    &i[2], &i[3], &i[4], &i[5]) == 6) {
		n.ether_addr_octet[0] = (u_char)i[0];
		n.ether_addr_octet[1] = (u_char)i[1];
		n.ether_addr_octet[2] = (u_char)i[2];
		n.ether_addr_octet[3] = (u_char)i[3];
		n.ether_addr_octet[4] = (u_char)i[4];
		n.ether_addr_octet[5] = (u_char)i[5];
		return &n;
	}
	return NULL;
}


int
ether_hostton(hostname, e)
	const char *hostname;
	struct ether_addr *e;
{
	FILE *f;
	char *p;
	size_t len;
	char try[MAXHOSTNAMELEN + 1];
	char line[512];
#ifdef YP
	int hostlen = strlen(hostname);
#endif

	f = fopen("/etc/ethers", "r");
	if (f==NULL)
		return -1;

	while ((p = fgets(line, sizeof(line), f)) != NULL) {
		if (p[len - 1] != '\n')
			continue;	       /* skip lines w/o \n */
		p[--len] = '\0';
#ifdef YP
		/* A + in the file means try YP now.  */
		if (len == 1 && *p == '+') {
			char *ypbuf, *ypdom;
			int ypbuflen;

			if (yp_get_default_domain(&ypdom))
				continue;
			if (yp_match(ypdom, "ethers.byname", hostname, hostlen,
			    &ypbuf, &ypbuflen))
				continue;
			if (ether_line(ypbuf, e, try) == 0) {
				free(ypbuf);
				(void)fclose(f);
				return 0;
			}
			free(ypbuf);
			continue;
		}
#endif
		if (ether_line(p, e, try) == 0 && strcmp(hostname, try) == 0) {
			(void)fclose(f);
			return 0;
		}
	}
	(void)fclose(f);
	errno = ENOENT;
	return -1;
}


int
ether_line(l, e, hostname)
	const char *l;
	struct ether_addr *e;
	char *hostname;
{
	u_int i[6];
	static char buf[sizeof " %x:%x:%x:%x:%x:%x %s\\n" + 21];
		/* XXX: 21 == strlen (ASCII representation of 2^64) */

	if (! buf[0])
		snprintf(buf, sizeof buf, " %%x:%%x:%%x:%%x:%%x:%%x %%%ds\\n",
		    MAXHOSTNAMELEN);
	if (sscanf(l, buf,
	    &i[0], &i[1], &i[2], &i[3], &i[4], &i[5], hostname) == 7) {
		e->ether_addr_octet[0] = (u_char)i[0];
		e->ether_addr_octet[1] = (u_char)i[1];
		e->ether_addr_octet[2] = (u_char)i[2];
		e->ether_addr_octet[3] = (u_char)i[3];
		e->ether_addr_octet[4] = (u_char)i[4];
		e->ether_addr_octet[5] = (u_char)i[5];
		return 0;
	}
	errno = EINVAL;
	return -1;
}
