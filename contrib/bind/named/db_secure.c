#ifndef LINT
static char rcsid[] = "$Id: db_secure.c,v 8.7 1997/06/01 20:34:34 vixie Exp $";
#endif

/* this file was contributed by Gregory Neil Shapiro of WPI in August 1993 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <errno.h>

#include "named.h"

#ifdef SECURE_ZONES

#ifndef SECURE_ZONE_RR
#define SECURE_ZONE_RR "secure_zone"
#endif
#ifndef MASK_SEP
#define MASK_SEP ':'
#endif

int
build_secure_netlist(zp)
	struct zoneinfo *zp;
{
	struct netinfo *ntp = NULL, **netlistp, **end;
	char buf[MAXDNAME];
	struct hashbuf *htp;
	struct namebuf *snp;
	struct databuf *dp;
	const char *fname;
	char *dname, dnbuf[MAXDNAME];
	int errs = 0, securezone = 0;

	if (zp->secure_nets) {
		free_netlist(&zp->secure_nets);
	}
	netlistp = &zp->secure_nets;
	end = netlistp;
	strcat(strcat(strcpy(dnbuf, SECURE_ZONE_RR), "."), zp->z_origin);

	dname = dnbuf;
	htp = hashtab;
	if ((snp = nlookup(dname, &htp, &fname, 0)) == NULL) {
		dprintf(1, (ddt,
			    "build_secure_netlist(%s): FAIL on nlookup %s\n",
			    zp->z_origin, dname));
		zp->secure_nets=NULL;
		return(0);
	}
	/* A parent's RR's aren't valid */
	if (strcasecmp(NAME(*snp), SECURE_ZONE_RR)) {
	  zp->secure_nets=NULL;
	  return(0);
	}
	/* Collect secure nets into secure_nets */
	for (dp = snp->n_data; dp != NULL; dp = dp->d_next) {
		char *maskptr = NULL;
		if (!match(dp, zp->z_class, T_TXT)) {
			continue;
		}
		bzero(buf, sizeof(buf));
		bcopy(dp->d_data+1, buf, dp->d_size-1);
		maskptr=strchr(buf, MASK_SEP);
		if (maskptr) {
			*maskptr++ = 0;
		}
		dprintf(3, (ddt,
			    "build_secure_netlist(%s): Found secure zone %s\n",
			    zp->z_origin, buf));
		if (ntp == NULL) {
			ntp = (struct netinfo *)malloc(sizeof(struct netinfo));
			if (!ntp)
				panic(errno, "malloc(netinfo)");
		}
		if (!inet_aton(buf, &ntp->my_addr)) {
			syslog(LOG_INFO,
			       "build_secure_netlist (%s): Bad address: %s", 
			       zp->z_origin, buf);
			errs++;
			continue;	
		}
		if (maskptr && *maskptr) {
			if (*maskptr == 'h' || *maskptr == 'H') {
				ntp->mask = (u_int32_t)-1;
			} else {
                              if (!inet_aton(maskptr,
					     (struct in_addr *)&ntp->mask)) {
					dprintf(1, (ddt,
				   "build_secure_netlist (%s): Bad mask: %s\n",
						    zp->z_origin, maskptr));
					syslog(LOG_INFO,
				     "build_secure_netlist (%s): Bad mask: %s",
					       zp->z_origin, maskptr);
					errs++;
					continue;
				}	
			}    
		} else {
			ntp->mask = net_mask(ntp->my_addr);
		}
		if (ntp->my_addr.s_addr & ~(ntp->mask)) {
			syslog(LOG_INFO, 
		   "build_secure_netlist (%s): addr (%s) is not in mask (%#lx)",
			       zp->z_origin,
			       inet_ntoa(ntp->my_addr),
			       (u_long)ntp->mask);
			errs++;
		}
		ntp->next = NULL;
		ntp->addr = ntp->my_addr.s_addr & ntp->mask;

		/* Check for duplicates */
		if (addr_on_netlist(ntp->my_addr, *netlistp)) {
			syslog(LOG_INFO, 
			   "build_secure_netlist (%s): duplicate address %s\n",
			       zp->z_origin, inet_ntoa(ntp->my_addr));
			errs++;
			continue;
		}
		*end = ntp;
		end = &ntp->next;
		ntp = NULL;
		securezone++;
	}
	if (ntp) {
		free((char *)ntp);
	}
	if (!securezone) {
		zp->secure_nets=NULL;
	}

#ifdef DEBUG
	if (debug > 1) {
		for (ntp = *netlistp;  ntp != NULL;  ntp = ntp->next) {
			fprintf(ddt, "ntp x%lx addr x%lx mask x%lx",
				(u_long)ntp, (u_long)ntp->addr,
				(u_long)ntp->mask);
			fprintf(ddt, " my_addr %#lx",
				(u_long)ntp->my_addr.s_addr);
			fprintf(ddt, " %s", inet_ntoa(ntp->my_addr));
			fprintf(ddt, " next x%lx\n", (u_long)ntp->next);
		}
	}
#endif
	return (errs);
}
#endif /*SECURE_ZONES*/
