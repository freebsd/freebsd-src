/*
 * rarpd support routines
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
 *	$Id: ether_addr.c,v 1.1.1.1 1995/03/02 06:41:40 wpaul Exp $
 */


#include <stdio.h>

#ifndef _PATH_ETHERS
#define _PATH_ETHERS "/etc/ethers"
#endif

/*
 * This should be defined in <netinet/if_ether.h> but it isn't.
 */
struct ether_addr {
	unsigned char octet[6];
};

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
                return (-1);

        for (i=0; i<6; i++)
                e->octet[i] = o[i];
        return (0);
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
	static char buf[BUFSIZ];
	static struct ether_addr local_ether;
	static char *local_host;
	static char *result;
	int resultlen, i;
	extern int yp_get_default_domain();
	extern int yp_match();
	static char *yp_domain;
	static char ether_a[BUFSIZ];

	if ((fp = fopen(_PATH_ETHERS, "r")) == NULL) {
                        perror(_PATH_ETHERS);
                        return (-1);
	}

	while (fgets(buf,BUFSIZ,fp)) {
		if (buf[0] == '+') {
			if (yp_get_default_domain(&yp_domain))
				return(-1);
			sprintf(ether_a,"%x:%x:%x:%x:%x:%x",
						e->octet[0], e->octet[1],
						e->octet[2], e->octet[3],
						e->octet[4], e->octet[5]);
			if (yp_match(yp_domain, "ethers.byaddr", ether_a,
				strlen(ether_a),&result, &resultlen))
				return(-1);
			if (ether_line(result, &local_ether,
					&local_host) == 0) {
				strcpy(hostname, (char *)&local_host);
				return(0);
			} else
				return(-1);
		}
		if (ether_line(&buf, &local_ether, &local_host) == 0) {
			for (i = 0; i < 6; i++)
				if (local_ether.octet[i] != e->octet[i])
					goto nomatch;
			/* We have a match */
			strcpy(hostname, (char *)&local_host);
			fclose(fp);
			return(0);
		}
nomatch:
	}

return (-1);
}

int ether_print(cp)
        u_char *cp;
{
        printf("%x:%x:%x:%x:%x:%x", cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
}

int ether_aton(a, n)
        char *a;
        u_char *n;
{
        int i, o[6];

        i = sscanf(a, "%x:%x:%x:%x:%x:%x", &o[0], &o[1], &o[2],
                                           &o[3], &o[4], &o[5]);
        if (i != 6) {
                return (i);
        }
        for (i=0; i<6; i++)
                n[i] = o[i];
        return (0);
}
