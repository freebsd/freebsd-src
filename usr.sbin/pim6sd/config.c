/*
 *  Copyright (c) 1998 by the University of Southern California.
 *  All rights reserved.
 *
 *  Permission to use, copy, modify, and distribute this software and
 *  its documentation in source and binary forms for lawful
 *  purposes and without fee is hereby granted, provided
 *  that the above copyright notice appear in all copies and that both
 *  the copyright notice and this permission notice appear in supporting
 *  documentation, and that any documentation, advertising materials,
 *  and other materials related to such distribution and use acknowledge
 *  that the software was developed by the University of Southern
 *  California and/or Information Sciences Institute.
 *  The name of the University of Southern California may not
 *  be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 *  THE UNIVERSITY OF SOUTHERN CALIFORNIA DOES NOT MAKE ANY REPRESENTATIONS
 *  ABOUT THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.  THIS SOFTWARE IS
 *  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND
 *  NON-INFRINGEMENT.
 *
 *  IN NO EVENT SHALL USC, OR ANY OTHER CONTRIBUTOR BE LIABLE FOR ANY
 *  SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, WHETHER IN CONTRACT,
 *  TORT, OR OTHER FORM OF ACTION, ARISING OUT OF OR IN CONNECTION WITH,
 *  THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  Other copyrights might apply to parts of this software and are so
 *  noted when applicable.
 */
/*
 *  Questions concerning this software should be directed to
 *  Mickael Hoerdt (hoerdt@clarinet.u-strasbg.fr) LSIIT Strasbourg.
 *
 */
/*
 * This program has been derived from pim6dd.
 * The pim6dd program is covered by the license in the accompanying file
 * named "LICENSE.pim6dd".
 */
/*
 * This program has been derived from pimd.
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
 *
 */
/*
 * Part of this program has been derived from mrouted.
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE.mrouted".
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 * $FreeBSD: src/usr.sbin/pim6sd/config.c,v 1.1.2.1 2000/07/15 07:36:35 kris Exp $
 */


#include <sys/ioctl.h>
#include <syslog.h>
#include <stdlib.h>
#include "vif.h"
#include "pim6.h"
#include "inet6.h"
#include "rp.h"
#include "pimd.h"
#include "timer.h"
#include "route.h"
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <net/if_var.h>
#endif
#include <netinet6/in6_var.h>
#ifdef HAVE_GETIFADDRS
#include <ifaddrs.h>
#endif 
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "config.h"
#include <arpa/inet.h>
#include <stdio.h>
#include "debug.h"

void add_phaddr(struct uvif *v, struct sockaddr_in6 *addr,
		struct in6_addr *mask);

void
config_vifs_from_kernel()
{
	register struct uvif *v;
	register vifi_t vifi;
	int i;
	struct sockaddr_in6 addr;
	struct in6_addr mask;
	short flags;
#ifdef HAVE_GETIFADDRS
	struct ifaddrs *ifap, *ifa;
#else
	int n;
	int num_ifreq = 64;
	struct ifconf ifc;
	struct ifreq *ifrp,*ifend;
#endif

	total_interfaces= 0;	/* The total number of physical interfaces */

#ifdef HAVE_GETIFADDRS
	if (getifaddrs(&ifap))
		log(LOG_ERR, errno, "getifaddrs");

	/*
	 * Loop through all of the interfaces.
	 */
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		struct in6_ifreq ifr6;

		/*
		 * Ignore any interface for an address family other than IPv6.
		 */
		if (ifa->ifa_addr->sa_family != AF_INET6) {
			/* Eventually may have IPv6 address later */
			total_interfaces++;
			continue;
		}

		memcpy(&addr, ifa->ifa_addr, sizeof(struct sockaddr_in6));

		flags = ifa->ifa_flags;


		/*
		 * Get netmask of the address.
		 */
		memcpy(&mask,
		       &((struct sockaddr_in6 *)ifa->ifa_netmask)->sin6_addr,
		       sizeof(mask));

		/*
		 * Get IPv6 specific flags, and ignore an anycast address.
		 * XXX: how about a deprecated, tentative, duplicated or
		 * detached address?
		 */
		memcpy(ifr6.ifr_name, ifa->ifa_name, sizeof(ifr6.ifr_name));
		ifr6.ifr_addr = *(struct sockaddr_in6 *)ifa->ifa_addr;
		if (ioctl(udp_socket, SIOCGIFAFLAG_IN6, &ifr6) < 0) {
			log(LOG_ERR, errno, "ioctl SIOCGIFAFLAG_IN6 for %s",
			    inet6_fmt(&ifr6.ifr_addr.sin6_addr));
		}
		else {
			if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_ANYCAST) {
				log(LOG_DEBUG, 0, "config_vifs_from_kernel: "
				    "%s on %s is an anycast address, ignored",
				    inet6_fmt(&ifr6.ifr_addr.sin6_addr),
				    ifa->ifa_name);
				continue;
			}
		}

		if (IN6_IS_ADDR_LINKLOCAL(&addr.sin6_addr))
		{
			addr.sin6_scope_id = if_nametoindex(ifa->ifa_name);
#ifdef __KAME__
			/*
			 * Hack for KAME kernel.
			 * Set sin6_scope_id field of a link local address and clear
			 * the index embedded in the address.
			 */
			/* clear interface index */
			addr.sin6_addr.s6_addr[2] = 0;
			addr.sin6_addr.s6_addr[3] = 0;
#endif
		}

		/*
		 * If the address is connected to the same subnet as one
		 * already installed in the uvifs array, just add the address
		 * to the list of addresses of the uvif.
		 */
		for(vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v)
		{
			if(strcmp(v->uv_name, ifa->ifa_name) == 0 )
			{
				add_phaddr(v, &addr, &mask);
				break;
			}
		}	

		if (vifi != numvifs)
			continue;

		/*
		 * If there is room in the uvifs array, install this interface.
		 */
		if (numvifs == MAXMIFS)
		{
			log(LOG_WARNING, 0,
			    "too many vifs, ignoring %s", ifa->ifa_name);
			continue;
		}

		/*
		 * Everyone below is a potential vif interface.
		 * We don't care if it has wrong configuration or not
		 * configured at all.
		 */
		total_interfaces++;

		v  = &uvifs[numvifs];
		v->uv_dst_addr = allpim6routers_group;
		v->uv_subnetmask = mask;
		strncpy(v->uv_name, ifa->ifa_name, IFNAMSIZ);
		v->uv_ifindex = if_nametoindex(v->uv_name);
		add_phaddr(v, &addr,&mask);
	
		/* prefix local calc. (and what about add_phaddr?...) */
		for (i = 0; i < sizeof(struct in6_addr); i++)
			v->uv_prefix.sin6_addr.s6_addr[i] =
				addr.sin6_addr.s6_addr[i] & mask.s6_addr[i];
	
		if(flags & IFF_POINTOPOINT)
			v->uv_flags |=(VIFF_REXMIT_PRUNES | VIFF_POINT_TO_POINT);

		/*
		 * Disable multicast routing on loopback interfaces and
		 * interfaces that do not support multicast. But they are
		 * still necessary, since global addresses maybe assigned only
		 * on such interfaces.
		 */
		if ((flags & IFF_LOOPBACK) != 0 ||
		    (flags & IFF_MULTICAST) == 0)
			v->uv_flags |= VIFF_DISABLED;

		IF_DEBUG(DEBUG_IF)
			log(LOG_DEBUG,0,
			    "Installing %s (%s on subnet %s) ,"
			    "as vif #%u - rate = %d",
			    v->uv_name,inet6_fmt(&addr.sin6_addr),
			    net6name(&v->uv_prefix.sin6_addr,&mask),
			    numvifs,v->uv_rate_limit);

		++numvifs;

		
		if( !(flags & IFF_UP)) 
		{
			v->uv_flags |= VIFF_DOWN;
			vifs_down = TRUE;
		}

	}

	freeifaddrs(ifap);
#else  /* !HAVE_GETIFADDRS */
	ifc.ifc_len = num_ifreq * sizeof (struct ifreq);
	ifc.ifc_buf = calloc(ifc.ifc_len,sizeof(char));
	while (ifc.ifc_buf) {
		caddr_t newbuf;

		if (ioctl(udp_socket,SIOCGIFCONF,(char *)&ifc) <0)
		      log(LOG_ERR, errno, "ioctl SIOCGIFCONF");	
		/*
		 * If the buffer was large enough to hold all the addresses
		 * then break out, otherwise increase the buffer size and
		 * try again.
		 *
		 * The only way to know that we definitely had enough space
		 * is to know that there was enough space for at least one
		 * more struct ifreq. ???
		 */
		if( (num_ifreq * sizeof (struct ifreq)) >=
		    ifc.ifc_len + sizeof(struct ifreq))
			break;

		num_ifreq *= 2;
		ifc.ifc_len = num_ifreq * sizeof(struct ifreq);
		newbuf = realloc(ifc.ifc_buf, ifc.ifc_len);
		if (newbuf == NULL)
			free(ifc.ifc_buf);
		ifc.ifc_buf = newbuf;
	}
	if (ifc.ifc_buf == NULL)
	    log(LOG_ERR, 0, "config_vifs_from_kernel: ran out of memory");
	

	ifrp = (struct ifreq *) ifc.ifc_buf;
	ifend = (struct ifreq * ) (ifc.ifc_buf + ifc.ifc_len);

	/*
	 * Loop through all of the interfaces.
	 */
	for(;ifrp < ifend;ifrp = (struct ifreq *)((char *)ifrp+n))
	{
		struct ifreq ifr;
		struct in6_ifreq ifr6;

#ifdef HAVE_SA_LEN
		n = ifrp->ifr_addr.sa_len + sizeof(ifrp->ifr_name);
		if(n < sizeof(*ifrp))
			n=sizeof(*ifrp);
#else
		n=sizeof(*ifrp);
#endif 

		/*
		 * Ignore any interface for an address family other than IPv6.
		 */
		if ( ifrp->ifr_addr.sa_family != AF_INET6)
		{
			/* Eventually may have IP address later */
			total_interfaces++;
			continue;
		}

		memcpy(&addr,&ifrp->ifr_addr,sizeof(struct sockaddr_in6));

		/*
		 * Need a template to preserve address info that is
		 * used below to locate the next entry.  (Otherwise,
		 * SIOCGIFFLAGS stomps over it because the requests
		 * are returned in a union.)
		 */
		memcpy(ifr.ifr_name,ifrp->ifr_name,sizeof(ifr.ifr_name));
		memcpy(ifr6.ifr_name,ifrp->ifr_name,sizeof(ifr6.ifr_name));

		if(ioctl(udp_socket,SIOCGIFFLAGS,(char *)&ifr) <0)
        	log(LOG_ERR, errno, "ioctl SIOCGIFFLAGS for %s", ifr.ifr_name);
		flags = ifr.ifr_flags;

#if 0
		/*
		 * Ignore loopback interfaces and interfaces that do not
		 * support multicast.
		 */
		if((flags & (IFF_LOOPBACK | IFF_MULTICAST ))!= IFF_MULTICAST)
			continue;
#endif

		/*
		 * Get netmask of the address.
		 */
		ifr6.ifr_addr = *(struct sockaddr_in6 *)&ifrp->ifr_addr;
		if(ioctl(udp_socket, SIOCGIFNETMASK_IN6, (char *)&ifr6) <0)
			log(LOG_ERR, errno, "ioctl SIOCGIFNETMASK_IN6 for %s",
			    inet6_fmt(&ifr6.ifr_addr.sin6_addr));
		memcpy(&mask,&ifr6.ifr_addr.sin6_addr,sizeof(mask));

		/*
		 * Get IPv6 specific flags, and ignore an anycast address.
		 * XXX: how about a deprecated, tentative, duplicated or
		 * detached address?
		 */
		ifr6.ifr_addr = *(struct sockaddr_in6 *)&ifrp->ifr_addr;
		if (ioctl(udp_socket, SIOCGIFAFLAG_IN6, &ifr6) < 0) {
			log(LOG_ERR, errno, "ioctl SIOCGIFAFLAG_IN6 for %s",
			    inet6_fmt(&ifr6.ifr_addr.sin6_addr));
		}
		else {
			if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_ANYCAST) {
				log(LOG_DEBUG, 0, "config_vifs_from_kernel: "
				    "%s on %s is an anycast address, ignored",
				    inet6_fmt(&ifr6.ifr_addr.sin6_addr),
				    ifr.ifr_name);
				continue;
			}
		}

		if (IN6_IS_ADDR_LINKLOCAL(&addr.sin6_addr))
		{
			addr.sin6_scope_id = if_nametoindex(ifrp->ifr_name);
#ifdef __KAME__
			/*
			 * Hack for KAME kernel.
			 * Set sin6_scope_id field of a link local address and clear
			 * the index embedded in the address.
			 */
			/* clear interface index */
			addr.sin6_addr.s6_addr[2] = 0;
			addr.sin6_addr.s6_addr[3] = 0;
#endif
		}

		/*
		 * If the address is connected to the same subnet as one
		 * already installed in the uvifs array, just add the address
		 * to the list of addresses of the uvif.
		 */
		for(vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v)
		{
			if( strcmp(v->uv_name , ifr.ifr_name) == 0 )
			{
				add_phaddr(v, &addr,&mask);
				break;
			}
		}	

		if( vifi != numvifs )
			continue;

		/*
		 * If there is room in the uvifs array, install this interface.
		 */
		if( numvifs == MAXMIFS )
		{
			log(LOG_WARNING, 0,
			    "too many vifs, ignoring %s", ifr.ifr_name);	
			continue;
		}		

		/*
		 * Everyone below is a potential vif interface.
		 * We don't care if it has wrong configuration or not
		 * configured at all.
		 */
		total_interfaces++;

		v  = &uvifs[numvifs];
		v->uv_dst_addr = allpim6routers_group;
		v->uv_subnetmask = mask;
		strncpy ( v->uv_name , ifr.ifr_name,IFNAMSIZ);
		v->uv_ifindex = if_nametoindex(v->uv_name);
		add_phaddr(v,&addr,&mask);
	
		/* prefix local calc. (and what about add_phaddr?...) */
		for (i = 0; i < sizeof(struct in6_addr); i++)
			v->uv_prefix.sin6_addr.s6_addr[i] =
				addr.sin6_addr.s6_addr[i] & mask.s6_addr[i];
	
		if(flags & IFF_POINTOPOINT)
			v->uv_flags |=(VIFF_REXMIT_PRUNES | VIFF_POINT_TO_POINT);

		/*
		 * Disable multicast routing on loopback interfaces and
		 * interfaces that do not support multicast. But they are
		 * still necessary, since global addresses maybe assigned only
		 * on such interfaces.
		 */
		if ((flags & IFF_LOOPBACK) != 0 || (flags & IFF_MULTICAST) == 0)
			v->uv_flags |= VIFF_DISABLED;

		IF_DEBUG(DEBUG_IF)
			log(LOG_DEBUG,0,
			    "Installing %s (%s on subnet %s) ,"
			    "as vif #%u - rate = %d",
			    v->uv_name,inet6_fmt(&addr.sin6_addr),
			    net6name(&v->uv_prefix.sin6_addr,&mask),
			    numvifs,v->uv_rate_limit);

		++numvifs;

		
		if( !(flags & IFF_UP)) 
		{
			v->uv_flags |= VIFF_DOWN;
			vifs_down = TRUE;
		}

	}
#endif /* HAVE_GETIFADDRS */
}

void
add_phaddr(struct uvif *v,struct sockaddr_in6 *addr,struct in6_addr *mask)
{
	struct phaddr *pa;
	int i;
	
	if( (pa=malloc(sizeof(*pa))) == NULL)
		        log(LOG_ERR, 0, "add_phaddr: memory exhausted");


	memset(pa,0,sizeof(*pa));
	pa->pa_addr= *addr;
	pa->pa_subnetmask = *mask;

	for(i = 0; i < sizeof(struct in6_addr); i++)
		pa->pa_prefix.sin6_addr.s6_addr[i] =
			addr->sin6_addr.s6_addr[i] & mask->s6_addr[i];
	pa->pa_prefix.sin6_scope_id = addr->sin6_scope_id;


	if(IN6_IS_ADDR_LINKLOCAL(&addr->sin6_addr)) {
		if(v->uv_linklocal)
            log(LOG_WARNING, 0,
               "add_phaddr: found more than one link-local "
               "address on %s",
               v->uv_name);

	v->uv_linklocal = pa;
	}

	pa->pa_next = v->uv_addrs;
	v->uv_addrs = pa;
}
