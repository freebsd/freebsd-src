/*
 * ethernet address conversion and lookup routines
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Center for Telecommunications Research
 * Columbia University, New York City
 *
 * This code is public domain. There is no copyright. There are no
 * distribution or usage restrictions. There are no strings attached.
 *
 * Have a party.
 *
 *	$Id: ether_addr.c,v 1.2 1995/03/03 22:20:15 wpaul Exp $
 */


#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <sys/param.h>

#ifndef _PATH_ETHERS
#define _PATH_ETHERS "/etc/ethers"
#endif

/*
 * This should be defined in <netinet/if_ether.h> but it isn't.
 */
struct ether_addr {
	unsigned char octet[6];
};

extern int yp_get_default_domain();
extern int yp_match();
char *yp_domain;

/*
 * Parse a string of text containing an ethernet address and hostname
 * and separate it into its component parts.
 */
int ether_line(l, e, hostname)
        char *l;
	struct ether_addr *e;
	char *hostname;
{
        int i, o[6];

        i = sscanf(l, "%x:%x:%x:%x:%x:%x %s", &o[0], &o[1], &o[2],
                                              &o[3], &o[4], &o[5],
					      hostname);
	if (i != 7)
                return (i);

        for (i=0; i<6; i++)
                e->octet[i] = o[i];
        return (0);
}

/*
 * Convert an ASCII representation of an ethernet address to
 * binary form.
 */
struct ether_addr *ether_aton(a)
        char *a;
{
        int i;
	static struct ether_addr o;

        i = sscanf(a, "%x:%x:%x:%x:%x:%x", o.octet[0], o.octet[1], o.octet[2],
                                           o.octet[3], o.octet[4], o.octet[5]);
        if (i != 6)
                return (NULL);
        return ((struct ether_addr *)&o);
}

/*
 * Convert a binary representation of an ethernet address to
 * an ASCII string.
 */
char *ether_ntoa(n)
        struct ether_addr *n;
{
        int i;
	static char a[18];

        i = sprintf(a,"%x:%x:%x:%x:%x:%x",n->octet[0],n->octet[1],n->octet[2],
                                          n->octet[3],n->octet[4],n->octet[5]);
        if (i < 11)
                return (NULL);
        return ((char *)&a);
}

/*
 * Map an ethernet address to a hostname. Use either /etc/ethers or
 * NIS/YP.
 */

int ether_ntohost(hostname, e)
	char *hostname;
	struct ether_addr *e;
{
	FILE *fp;
	char buf[BUFSIZ];
	struct ether_addr local_ether;
	char local_host[MAXHOSTNAMELEN];
	char *result;
	int resultlen;
	char *ether_a;

	if ((fp = fopen(_PATH_ETHERS, "r")) == NULL)
		return (1);

	while (fgets(buf,BUFSIZ,fp)) {
		if (buf[0] == '#')
			continue;
		if (buf[0] == '+') {
			fclose(fp);  /* Can ignore /etc/ethers from here on. */
			if (yp_get_default_domain(&yp_domain))
				return(1);
			ether_a = ether_ntoa(e);
			if (yp_match(yp_domain, "ethers.byaddr", ether_a,
				strlen(ether_a), &result, &resultlen))
				return(1);
			if (!ether_line(result, &local_ether, &local_host)) {
				strcpy(hostname, (char *)&local_host);
				return(0);
			} else
				return(1);
		}
		if (!ether_line(&buf, &local_ether, &local_host)) {
			if (!bcmp((char *)&local_ether.octet[0],
				(char *)&e->octet[0], 6)) {
			/* We have a match */
				strcpy(hostname, (char *)&local_host);
				fclose(fp);
				return(0);
			}
		}
	}
fclose(fp);
return (1);
}

/*
 * Map a hostname to an ethernet address using /etc/ethers or
 * NIS/YP.
 */
int ether_hostton(hostname, e)
	char *hostname;
	struct ether_addr *e;
{
	FILE *fp;
	char buf[BUFSIZ];
	struct ether_addr local_ether;
	char local_host[MAXHOSTNAMELEN];
	char *result;
	int resultlen;

	if ((fp = fopen(_PATH_ETHERS, "r")) == NULL)
		return (1);

	while (fgets(buf,BUFSIZ,fp)) {
		if (buf[0] == '#')
			continue;
		if (buf[0] == '+') {
			fclose(fp);  /* Can ignore /etc/ethers from here on. */
			if (yp_get_default_domain(&yp_domain))
				return(1);
			if (yp_match(yp_domain, "ethers.byname", hostname,
				strlen(hostname), &result, &resultlen))
				return(1);
			if (!ether_line(result, &local_ether, &local_host)) {
				bcopy((char *)&local_ether.octet[0],
					(char *)&e->octet[0], 6);
				return(0);
			} else
				return(1);
		}
		if (!ether_line(&buf, &local_ether, &local_host)) {
			if (!strcmp(hostname, (char *)&local_host)) {
				/* We have a match */
				bcopy((char *)&local_ether.octet[0],
					(char *)&e->octet[0], 6);
				fclose(fp);
				return(0);
			}
		}
	}
fclose(fp);
return (1);
}
