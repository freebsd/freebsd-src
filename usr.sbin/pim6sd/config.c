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
 * $FreeBSD$
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
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "config.h"
#include <arpa/inet.h>
#include <stdio.h>
#include "debug.h"

void add_phaddr(struct uvif *v , struct sockaddr_in6 *addr,struct in6_addr *mask);
char *next_word(char **s);
int wordToOption(char *word);
int parse_phyint(char *s);
int parse_candidateRP(char *s);
int parse_group_prefix(char *s);
int parseBSR(char *s);
int parse_reg_threshold(char *s);
int parse_data_threshold(char *s);
int parse_default_source_metric(char *s);
int parse_default_source_preference(char *s);
int parse_hello_period(char *s);
int parse_granularity(char *s);
int parse_jp_period(char *s);
int parse_data_timeout(char *s);
int parse_register_suppression_timeout(char *s);
int parse_probe_time(char *s);
int parse_assert_timeout(char *s);

void
config_vifs_from_kernel()
{
	struct ifreq *ifrp,*ifend;
	register struct uvif *v;
	register vifi_t vifi;
	int n,i;
	struct sockaddr_in6 addr;
	struct in6_addr mask;
	short flags;
	int num_ifreq = 64;
	struct ifconf ifc;

	total_interfaces= 0;	/* The total number of physical interfaces */

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

void
config_vifs_from_file()
{
	FILE *f;
	char linebuf[100];
	char *w,*s;
	struct ifconf ifc;
	int option;
	char ifbuf[BUFSIZ];
	u_int8 *data_ptr;

	if((f=fopen(configfilename,"r"))==NULL)
	{
		if( errno != ENOENT)
			log(LOG_ERR,errno,"Can't open %s",configfilename);
		log(LOG_WARNING,errno,"Can't open  %s",configfilename);
		return;
	}
	/*
	 * Note that sizeof(pim6_enocd_uni_addr_t) might be larger than
	 * the length of the Encoded-Unicast-address field(18 byte) due to
	 * some padding put in the compiler. However, it doesn't matter
	 * since we use the space just as a buffer(i.e not as the message).
	 */
	cand_rp_adv_message.buffer = (u_int8 *)malloc( 4 + sizeof(pim6_encod_uni_addr_t) +
								255*sizeof(pim6_encod_grp_addr_t));
	if(cand_rp_adv_message.buffer == NULL)
		log(LOG_ERR,errno,"Candrpadv Buffer allocation");

	cand_rp_adv_message.prefix_cnt_ptr = cand_rp_adv_message.buffer;

    /* By default, if no group_prefix configured, then prefix_cnt == 0
     * implies group_prefix = ff::/8 and masklen = 8.
     */

	*cand_rp_adv_message.prefix_cnt_ptr = 0;
	cand_rp_adv_message.insert_data_ptr = cand_rp_adv_message.buffer;

       /* TODO: XXX: HARDCODING!!! */
	cand_rp_adv_message.insert_data_ptr += (4 + 18);

	ifc.ifc_buf = ifbuf;
	ifc.ifc_len = sizeof(ifbuf);
	if(ioctl(udp_socket,SIOCGIFCONF,(char *)&ifc) < 0)
	 	log(LOG_ERR, errno, "ioctl SIOCGIFCONF");

	while( fgets(linebuf, sizeof(linebuf),f) != NULL )
	{
		s = linebuf;
		w = next_word(&s);
		option = wordToOption(w);
		switch(option)
		{
		case EMPTY:
			continue;
			break;
		case PHYINT:
			parse_phyint(s);
			break;
		case CANDIDATE_RP:
			parse_candidateRP(s);
			break;
		case GROUP_PREFIX:
			parse_group_prefix(s);
			break;
		case BOOTSTRAP_RP:
			parseBSR(s);
			break;
		case REG_THRESHOLD:
			parse_reg_threshold(s);
			break;
		case DATA_THRESHOLD:
			parse_data_threshold(s);
			break;
		case DEFAULT_SOURCE_METRIC:
			parse_default_source_metric(s);
			break;
		case DEFAULT_SOURCE_PREFERENCE :
			parse_default_source_preference(s);
			break;
		case HELLO_PERIOD :
			parse_hello_period(s);
			break;
		case GRANULARITY :
			parse_granularity(s);
			break;
		case JOIN_PRUNE_PERIOD :
			parse_jp_period(s);
			break;
		case DATA_TIMEOUT :
			parse_data_timeout(s);
			break;
		case REGISTER_SUPPRESSION_TIMEOUT :
			parse_register_suppression_timeout(s);
			break;
		case PROBE_TIME :
			parse_probe_time(s);
			break;
		case ASSERT_TIMEOUT:
			parse_assert_timeout(s);
			break;
		default:
	       log(LOG_WARNING, 0, "unknown command '%s' in %s",
        	w, configfilename);

		}
	}
	cand_rp_adv_message.message_size = cand_rp_adv_message.insert_data_ptr - cand_rp_adv_message.buffer;
	if (cand_rp_flag != FALSE)
	{
		my_cand_rp_holdtime = 2.5 * my_cand_rp_adv_period;

		/* TODO: HARDCODING! */
		data_ptr = cand_rp_adv_message.buffer + 1;	/* WARNING */
		PUT_BYTE(my_cand_rp_priority,data_ptr);
		PUT_HOSTSHORT(my_cand_rp_holdtime, data_ptr);
		PUT_EUADDR6(my_cand_rp_address.sin6_addr,data_ptr);
		IF_DEBUG(DEBUG_PIM_CAND_RP)
		{
			log(LOG_DEBUG, 0,
		    	"Local Cand-RP address is : %s",
		    	inet6_fmt(&my_cand_rp_address.sin6_addr));
	 		log(LOG_DEBUG, 0,
		    	"Local Cand-RP priority is : %u",my_cand_rp_priority);
			log(LOG_DEBUG, 0,
		    	"Local Cand-RP advertisement period is : %u sec.",
		    	my_cand_rp_adv_period);
		}
	}


	if( cand_bsr_flag!=FALSE)
	{
		IF_DEBUG(DEBUG_PIM_BOOTSTRAP)
		{
			log(LOG_DEBUG, 0,
		    	"Local BSR address: %s",
		    	inet6_fmt(&my_bsr_address.sin6_addr));
			log(LOG_DEBUG, 0,
		    	"Local BSR priority : %u",my_bsr_priority);
			log(LOG_DEBUG,0,
				"Local BSR period is : %u sec.",
				my_bsr_period);

		}

	}

	IF_DEBUG(DEBUG_SWITCH)
	{
		log(LOG_DEBUG,0,"reg_rate_limit set to %u (bits/s)" , pim_reg_rate_bytes);
		log(LOG_DEBUG,0,"reg_rate_interval set to  %u s.",pim_reg_rate_check_interval);
		log(LOG_DEBUG,0,"data_rate_limit set to %u (bits/s)" , pim_data_rate_bytes);
		log(LOG_DEBUG,0,"data_rate_interval set to %u s.",pim_data_rate_check_interval);
	}

	IF_DEBUG(DEBUG_PIM_HELLO)
	{
		log(LOG_DEBUG,0, "pim_hello_period set to: %u", pim_hello_period);
		log(LOG_DEBUG,0, "pim_hello_holdtime set to: %u", pim_hello_holdtime);
	}

	IF_DEBUG(DEBUG_PIM_JOIN_PRUNE)
	{
		log(LOG_DEBUG,0, "pim_join_prune_period set to: %u", pim_join_prune_period);
		log(LOG_DEBUG,0, "pim_join_prune_holdtime set to: %u", pim_join_prune_holdtime);
	}

	fclose(f);

}
/*
 * function name: wordToOption
 * input: char *word, a pointer to the word
 * output: int; a number corresponding to the code of the word
 * operation: converts the result of the string comparisons into numerics.
 * comments: called by config_vifs_from_file()
 */

int wordToOption(char *word)
{
    if (EQUAL(word, ""))
    return EMPTY;
    if (EQUAL(word, "phyint"))
    return PHYINT;
    if (EQUAL(word, "cand_rp"))
    return CANDIDATE_RP;
    if (EQUAL(word, "group_prefix"))
    return GROUP_PREFIX;
    if (EQUAL(word, "cand_bootstrap_router"))
    return BOOTSTRAP_RP;
    if (EQUAL(word, "switch_register_threshold"))
    return REG_THRESHOLD;
    if (EQUAL(word, "switch_data_threshold"))
    return DATA_THRESHOLD;
    if (EQUAL(word, "default_source_metric"))
    return DEFAULT_SOURCE_METRIC;
    if (EQUAL(word, "default_source_preference"))
    return DEFAULT_SOURCE_PREFERENCE;
    if (EQUAL(word, "hello_period"))
    return HELLO_PERIOD;
    if (EQUAL(word, "granularity"))
    return GRANULARITY;
    if (EQUAL(word, "join_prune_period"))
    return JOIN_PRUNE_PERIOD;
    if (EQUAL(word, "data_timeout"))
    return DATA_TIMEOUT;
    if (EQUAL(word, "register_suppression_timeout"))
    return REGISTER_SUPPRESSION_TIMEOUT;
    if (EQUAL(word, "probe_time"))
    return PROBE_TIME;
    if (EQUAL(word, "assert_timeout"))
    return PROBE_TIME;
    return UNKNOWN;
}

/*
 * function name: parse_phyint
 * input: char *s, pointing to the parsing point of the file
 * output: int (TRUE if the parsing was successful, o.w. FALSE)
 * operation: parses the physical interface file configurations, if any.
 * The general form is:
 *     phyint <ifname> [disable]  [preference <p>] [metric <m>]
 */


int parse_phyint(char *s)
{
	char *w,c,*ifname;
	vifi_t vifi;
	struct uvif *v;
	u_int n;

	if(EQUAL((w = next_word(&s)),""))
	{
		log(LOG_WARNING, 0, "Missing phyint name in %s", configfilename);
		return FALSE;
	}
	ifname = w;

	for (vifi = 0,v=uvifs;vifi <= numvifs ; ++vifi , ++v)
	{
		if(vifi == numvifs)
		{
		    log(LOG_WARNING, 0,
			"Invalid phyint name (maybe not configured..) '%s' "
			"in %s", w, configfilename);
			return FALSE;
		}

		if(strcmp(v->uv_name,ifname))
			continue;

		while(!EQUAL((w = next_word(&s)),""))
		{
			if(EQUAL(w,"disable"))
				v->uv_flags |=VIFF_DISABLED;
			else if (EQUAL(w, "nolistener"))
				v->uv_flags |= VIFF_NOLISTENER;
			else
			{
				if(EQUAL(w,"preference"))
				{
					if(EQUAL((w=next_word(&s)),""))
					{
						log(LOG_WARNING, 0,
						    "Missing preference for "
						    "phyint %s in %s",
						    ifname, configfilename);
					}
					else
					{
						if (sscanf(w,"%u%c",&n,&c) != 1 ||
						    n < 1 || n > 255 )
						{
							log(LOG_WARNING, 0,
							    "Invalid preference "
							    "'%s' for phyint %s "
							    "in %s",
							    w, ifname,
							    configfilename);
						}
						else
						{
							IF_DEBUG(DEBUG_ASSERT)
								log(LOG_DEBUG, 0,"Config setting default local preference on %d to %s",n,ifname);
							v->uv_local_pref = n;
						}
  					}
				}
				else
				{
					if(EQUAL(w, "metric"))
					{
						if(EQUAL((w = next_word(&s)), ""))
						{
							log(LOG_WARNING,0,
							    "Missing metric for "
							    "phyint %s in %s",
							    ifname,
							    configfilename);
						}
						else
						{
							if (sscanf(w, "%u%c", &n, &c) != 1 ||
							    n < 1 || n > 1024 )
							{
								log(LOG_WARNING,0,
								    "Invalid metric '%s' for phyint %s in %s",
								    w, ifname,configfilename);
							}
							else
							{
								IF_DEBUG(DEBUG_ASSERT)
									log(LOG_DEBUG,0,
									    "Config setting default local metric on %d to %s.",
									    n,ifname);
								v->uv_local_metric = n;
        					}

    					}
    				}
    			}
			}
		}
		break;
	}
   return(TRUE);
}

/*
 * function name: parse_candidateRP
 * input: char *s
 * output: int (TRUE if the parsing was successful, o.w. FALSE)
 * operation: parses the candidate RP information.
 *  The general form is:
 *      'cand_rp <ifname> [priority <number>] [time <number>]'.
 */
int
parse_candidateRP(char *s)
{
	char *w;
	struct sockaddr_in6 *sa6_rp;
	u_int time = PIM_DEFAULT_CAND_RP_ADV_PERIOD;
	u_int priority = PIM_DEFAULT_CAND_RP_PRIORITY;

	sa6_rp = NULL;
	cand_rp_flag = FALSE;

	my_cand_rp_adv_period = PIM_DEFAULT_CAND_RP_ADV_PERIOD;

	while(!EQUAL((w = next_word(&s)),""))
	{
		if((!EQUAL(w,"priority")) && (!EQUAL(w,"time")))
		{
			/*
			 * if the interface is specified and is valid
			 * we take the max global address of the interface
			 * (aliasing) else look at the end of the function.
			 */
			sa6_rp = local_iface(w);
			if(!sa6_rp)
				log(LOG_WARNING, 0,
				    "cand_rp '%s' in  %s is not configured."
				    "take the max local address the router..",
				    w, configfilename);
		}
		else
		{
			if (EQUAL(w,"priority"))
			{
				if (EQUAL((w = next_word(&s)),""))
				{
					log(LOG_WARNING,0,
					    "Missing priority ; set to default "
					    ": %d (0 is highest )",priority);
				}
				else
				{
					if (sscanf(w,"%u",&priority)!= 1 )
					{
						priority = PIM_DEFAULT_CAND_RP_PRIORITY;
						log(LOG_WARNING, 0,
						    "Invalid priority '%' "
						    "for cand_rp;set to default "
						    "(0 is highest) : %d",
						    w, priority);
					}
				}
			}
			else
			{
				if (EQUAL((w = next_word(&s)),""))
				{
					log(LOG_WARNING, 0,
					    "Missing cand_adv period ;"
					    "set to default : %d",time);
				}
				else
				{
					if (sscanf(w,"%u",&time)!= 1 )
					{
						time = PIM_DEFAULT_CAND_RP_ADV_PERIOD;
						log(LOG_WARNING, 0,
						    "Invalid cand_adv_period "
						    "'%s';set to default : %d",
						    w,time);

					}
					else
					{
						if( time > (my_cand_rp_adv_period = ~0))
							time = my_cand_rp_adv_period;
						else
							if(time <10)
								time = 10;
							else
								if (time > PIM_DEFAULT_CAND_RP_ADV_PERIOD)
									time = PIM_DEFAULT_CAND_RP_ADV_PERIOD;
						my_cand_rp_adv_period = time;
					}
				}
			}
		}
	}

	if(!sa6_rp)
		sa6_rp= max_global_address();

	my_cand_rp_address=*sa6_rp;
	my_cand_rp_priority = priority;
	my_cand_rp_adv_period = time;
	cand_rp_flag = TRUE;

	return TRUE;
}

/*
 * function name: parse_group_prefix
 * input: char *s
 * output: int
 * operation: parse group_prefix configured information.
 *  General form: 'group_prefix <group-addr>/<prefix_len>'.
 */
int
parse_group_prefix(char *s)
{
	char *w;
	struct in6_addr group_addr;
	u_int32 masklen=PIM_GROUP_PREFIX_DEFAULT_MASKLEN;

	w=next_word(&s);
	if (EQUAL(w,""))
	{
	    log(LOG_WARNING, 0,
		"Configuration error for 'group_prefix' in %s: no group_addr. "
		"Ignoring...", configfilename);
		return FALSE;
	}

	w=strtok(w,"/");

	if ( inet_pton(AF_INET6,w,(void *)&group_addr) != 1 )
	{
		log(LOG_WARNING, 0,
		    "Config error in %s : Bad ddress formatt.Ignoring..",
		    configfilename);
		return FALSE;
	}
	if (!IN6_IS_ADDR_MULTICAST(&group_addr))
	{
		log(LOG_WARNING,0,
		    "Config error in %s: '%s' is not a mcast addr.Ignoring",
		    configfilename,
		    inet6_fmt(&group_addr));
		return FALSE;
	}
	if (!(~(*cand_rp_adv_message.prefix_cnt_ptr)))
	{
		log(LOG_WARNING, 0,
		    "Too many group_prefix configured. Truncating...");
		return FALSE;
	}

		w=strtok(NULL,"/");
		if(w==NULL)
		{
			log(LOG_WARNING,0,
				"Config error in %s : missing group prefix.Ignoring..",
				configfilename);
			return  FALSE;
		}
		if ( sscanf(w,"%u",&masklen) ==1 )
		{
			if (masklen  > (sizeof(group_addr) * 8))
				masklen = (sizeof(group_addr)*8);
			else
			if (masklen <PIM_GROUP_PREFIX_DEFAULT_MASKLEN)
				masklen = PIM_GROUP_PREFIX_DEFAULT_MASKLEN;
		}


	PUT_EGADDR6(group_addr, (u_int8)masklen, 0,
		    cand_rp_adv_message.insert_data_ptr);
	(*cand_rp_adv_message.prefix_cnt_ptr)++;

	return TRUE;

}
/*
 * function name: parseBSR
 * input: char *s
 * output: int
 * operation: parse the candidate BSR configured information.
 *  General form:
 *  'cand_bootstrap_router <ifname> [priority <number>]'.
 *  this function is similar to parse_candrp
 */

int
parseBSR(char *s)
{
	char *w;
	struct sockaddr_in6 *sa6_bsr;
	u_int32 priority = PIM_DEFAULT_BSR_PRIORITY;
	u_int time = PIM_DEFAULT_BOOTSTRAP_PERIOD;
	my_bsr_period = PIM_DEFAULT_BOOTSTRAP_PERIOD;

	sa6_bsr = NULL;
	cand_bsr_flag = FALSE;

	while(!EQUAL((w = next_word(&s)),""))
	{
		if((!EQUAL(w,"priority")) && (!EQUAL(w,"time")))
		{

			sa6_bsr = local_iface(w);
			if(!sa6_bsr)
			{
				log(LOG_WARNING,0,
				    "cand_bootstrap_router '%s' in %s is not "
				    "configured.Take the max router address.",
				    w,configfilename);
			}
		}
		else
		{
			if(EQUAL(w,"priority"))
			{
			    if (EQUAL((w = next_word(&s)),""))
				{
					log(LOG_WARNING, 0,
					    "Missing priority for the bsr;set to "
					    "default (0 is lowest): %d",priority);
				}
				else
				{
					if (sscanf(w,"%u",&priority)!= 1 )
					{
						priority = PIM_DEFAULT_BSR_PRIORITY;
						log(LOG_WARNING, 0,
						    "Invalid priority '%s'for "
						    "the bsr;set to default : %d",
						    w, priority);
					}
					else
					{
						if( priority > (my_bsr_priority = ~0))
							priority = my_bsr_priority;
						my_bsr_priority = (u_int8)priority;
					}
				}
			}
			else
			{
				if( EQUAL((w=next_word(&s)),""))
				{
					log(LOG_WARNING,0,
						"Missing bsr period ;"
						"set to default : %d ",time);
				}
				else
				{
					if(sscanf(w,"%u",&time)!=1)
					{
						time=PIM_DEFAULT_BOOTSTRAP_PERIOD;
						log(LOG_WARNING,0,
						"Invalid bsr period"
						"'%s';set to default : %d",
						w,time);
					}
					else
						my_bsr_period=time;
				}
			}
		}
	}

        if(!sa6_bsr)
                sa6_bsr = max_global_address();

	my_bsr_address=*sa6_bsr;
        my_bsr_priority = priority;
       	MASKLEN_TO_MASK6(RP_DEFAULT_IPV6_HASHMASKLEN,my_bsr_hash_mask);
	cand_bsr_flag = TRUE;

      	return TRUE;
}

/*
 * function name: parse_reg_threshold
 * input: char *s
 * output: int (TRUE if successful, FALSE o.w.)
 * operation: reads and assigns the switch to the spt threshold
 * due to registers for the router, if used as RP.
 * Maybe extended to support different thresholds
 *        for different groups(prefixes).
 *        General form:
 *      'switch_register_threshold [rate <number> interval <number>]'.
 * comments: called by config_vifs_from_file()
 */


int parse_reg_threshold(char *s)
{
	char *w;
	u_int rate=PIM_DEFAULT_REG_RATE;
	u_int interval= PIM_DEFAULT_REG_RATE_INTERVAL;

	while(!EQUAL((w=next_word(&s)),""))
	{
		if(EQUAL(w,"rate"))
		{
			if(EQUAL((w=next_word(&s)),""))
			{
				log(LOG_WARNING,0,
				    "switch_register_threshold : missing rate ; "
				    "set to  default : %u (bits/s)",
				    rate);
			}
			else
			{
				if(sscanf(w,"%u",&rate)!=1)
				{
					rate = PIM_DEFAULT_REG_RATE;
					log(LOG_WARNING, 0,
					    "switch_register_threshold : "
					    "Invalid rate '%s' , set to defaut :"
					    " %u (bits/s)",
					    w,rate);
				}
			}
		}
		else
		{
			if(EQUAL(w,"interval"))
			{
				if(EQUAL((w = next_word(&s)),""))
				{
					log(LOG_WARNING,0,"switch_register_threshold : missing interval ; set to default : %u s.",
					interval);
				}
				else
				{
					if(sscanf(w,"%u",&interval) != 1)
					{
						interval = PIM_DEFAULT_REG_RATE_INTERVAL;
						log(LOG_WARNING,0,"switch_register_threshold : Invalid interval '%s' ; set to default : %u s.",
						w,interval);
					}
				}
			}
			else
			{
				log(LOG_WARNING,0,"swhitch_register_threshold : Invalid parameter %s",w);
			}
		}
	}

	if( interval < timer_interval)
	{
		interval = timer_interval;
		log(LOG_WARNING,0,"switch_register_threshold : Interval too short , set to default : %u s.",
		interval);
	}

	pim_reg_rate_bytes = (rate * interval ) /10;
	pim_reg_rate_check_interval = interval;

	return TRUE;

}
/*
 * function name: parse_data_threshold
 * input: char *s
 * output: int
 * operation: reads and assigns the switch to the spt threshold
 *        due to data packets, if used as DR.
 *        General form:
 *      'switch_data_threshold [rate <number> interval <number>]'.
 *		similar to register_threshold...
 */

int parse_data_threshold(char *s)
{
	char *w;
	u_int rate=PIM_DEFAULT_DATA_RATE;
	u_int interval= PIM_DEFAULT_DATA_RATE_INTERVAL;

	while(!EQUAL((w=next_word(&s)),""))
	{
		if(EQUAL(w,"rate"))
		{
			if(EQUAL((w=next_word(&s)),""))
			{
				log(LOG_WARNING,0,"switch_data_threshold : missing rate value ; set to defaut : %u (bits/s)",
				rate);
			}
			else
			{
				if(sscanf(w,"%u",&rate)!=1)
				{
					rate = PIM_DEFAULT_DATA_RATE;
					log(LOG_WARNING,0,"switch_data_threshold : Invalid rate '%s' ; set to default : %u (bits/s)",
					w,rate);
				}
			}
		}
		else
		{
			if(EQUAL(w,"interval"))
			{
				if(EQUAL((w = next_word(&s)),""))
				{
					log(LOG_WARNING,0,"switch_data_threshold : missing interval value ; set to default : %u s.",
					interval);
				}
				else
				{
					if(sscanf(w,"%u",&interval) != 1)
					{
						interval = PIM_DEFAULT_DATA_RATE_INTERVAL;
						log(LOG_WARNING,0,"switch_data_threshold : Invalid interval '%s' ; set to default : %u s.",
						w,interval);
					}
				}
			}
			else
			{
				log(LOG_WARNING,0,"swhitch_data_threshold :Invalid Parameter %s",w);
			}
		}
	}

	if( interval < timer_interval)
	{
		interval = timer_interval;
		log(LOG_WARNING,0,"switch_data_threshold : interval too short set to  default : %u s.",
		interval);
	}

	pim_data_rate_bytes = (rate * interval ) /10;
	pim_data_rate_check_interval = interval;

	return TRUE;

}

/*
 * function name: parse_default_source_metric
 * input: char *s
 * output: int
 * operation: reads and assigns the default source metric, if no reliable
 *        unicast routing information available.
 *        General form:
 *      'default_source_metric <number>'.
 *            default pref and metric statements should precede all phyint
 *            statements in the config file.
 */


int parse_default_source_metric(char *s)
{
	char *w;
	u_int value;
	vifi_t vifi;
	struct uvif *v;

	value = DEFAULT_LOCAL_METRIC;

	if (EQUAL((w = next_word(&s)), ""))
	{
		log(LOG_WARNING,0,
		"Missing source metric value ; set to default %u",
		value);
	}
	else
	{
		if (sscanf(w, "%u", &value) != 1)
		{
			value = DEFAULT_LOCAL_METRIC;
			log(LOG_WARNING,0,
			"Invalid source metric value '%s' ;set to default %u",
			w,value);
		}
		default_source_metric = value;
		log(LOG_INFO,0, "Default_source_metric is : %u", default_source_metric);

		for (vifi = 0, v = uvifs; vifi < MAXVIFS; ++vifi, ++v)
		{
			v->uv_local_metric = default_source_metric;
		}


	}
		return(TRUE);
}

/*
 * function name: parse_default_source_preference
 * input: char *s
 * output: int
 * operation: reads and assigns the default source preference, if no reliable
 *        unicast routing information available.
 *        General form:
 *      'default_source_preference <number>'.
 *            default pref and metric statements should precede all phyint
 *            statements in the config file.
 */

int parse_default_source_preference(char *s)
{
	char *w;
	u_int value;
	vifi_t vifi;
	struct uvif *v;

	value = DEFAULT_LOCAL_PREF;

	if (EQUAL((w = next_word(&s)), ""))
	{
		log(LOG_WARNING,0,
		"Missing source preference ; set to default %u",
		value);
	}
	else
	{
		if (sscanf(w, "%u", &value) != 1)
		{
			value = DEFAULT_LOCAL_PREF;
			log(LOG_WARNING,0,
			"Invalid source preference value '%s' ;set to default %u",
			w,value);
		}
		default_source_preference = value;
		log(LOG_INFO,0, "default_source_preference set to: %u", default_source_preference);

		for (vifi = 0, v = uvifs; vifi < MAXVIFS; ++vifi, ++v)
		{
			v->uv_local_pref = default_source_preference;
		}


	}
		return(TRUE);
}

/*
 * function name: parse_hello_period
 * input: char *s
 * output: int
 * operation: reads and assigns the hello period for a pim router
 *        General form:
 *      'hello_period <number> <coef>'.
 *	number is the period in second between 2 hello messages
 *  	and coef is the coef to deterimine the hello holdtime
 *    	default : 3.5
 */

int parse_hello_period(char *s)
{
	char *w;
	u_int hellop;
	float coef;

	hellop = PIM_TIMER_HELLO_PERIOD;
	coef = 3.5;

	if (EQUAL((w = next_word(&s)), ""))
	{
		log(LOG_WARNING,0,
		"Missing hello period ; set to default %u",
		hellop);
	}
	else
	{
		if (sscanf(w, "%u", &hellop) != 1)
		{
			hellop = PIM_TIMER_HELLO_PERIOD;
			log(LOG_WARNING,0,
			"Invalid hello period value '%s' ;set to default %u",
			w,hellop);
		}
		pim_hello_period = hellop;

		if (!EQUAL((w=next_word(&s)),""))
		{
			if (sscanf(w, "%f", &coef) != 1)
			{
				coef = 3.5;
				log(LOG_WARNING,0,
				"Invalid hello period coef '%s' ;set to default %.1f",
				w,coef);
			}
			if(coef<=1)
			{
				coef = 3.5;
				log(LOG_WARNING,0,
				"for hello period coef must be > 1;set to default %.1f",
				coef);
			}

		}


	}
		pim_hello_holdtime = coef*pim_hello_period;
		return(TRUE);
}
/*
 * function name: parse_jp_period
 * input: char *s
 * output: int
 * operation: reads and assigns the join/prune period for a pim router
 *        General form:
 *      'join_prune_period <number> <coef>'.
 *	number is the period in second between 2 join/prune messages
 *  	and coef is the coef to deterimine the join/prune holdtime
 *    	default : 3.5
 * This function is similar to the function above
 */

int parse_jp_period(char *s)
{
	char *w;
	u_int jpp;
	float coef;

	jpp = PIM_JOIN_PRUNE_PERIOD;
	coef = 3.5;

	if (EQUAL((w = next_word(&s)), ""))
	{
		log(LOG_WARNING,0,
		"Missing join/prune period ; set to default %u",
		jpp);
	}
	else
	{
		if (sscanf(w, "%u", &jpp) != 1)
		{
			jpp = PIM_JOIN_PRUNE_PERIOD;
			log(LOG_WARNING,0,
			"Invalid join/prune period value '%s' ;set to default %u",
			w,jpp);
		}

		pim_join_prune_period = jpp;

		if (!EQUAL((w=next_word(&s)),""))
		{
			if (sscanf(w, "%f", &coef) != 1)
			{
				coef = 3.5;
				log(LOG_WARNING,0,
				"Invalid join/prune period coef '%s' ;set to default %.1f",
				w,coef);
			}
			if(coef<=1)
			{
				coef = 3.5;
				log(LOG_WARNING,0,
				"for join/prune period coef must be > 1;set to default %.1f",
				coef);
			}

		}


	}
		pim_join_prune_holdtime = coef*pim_join_prune_period;
		return(TRUE);
}


/* function name : parse_granularity
 * input char *s
 * output int
 * operation : reads and assigns the granularity of the demon's timer
 * 		General form :
 * 		'granularity <number>
 * number is the period in seconds between each "tics" of the virtual time.
 * default : 5 s.
 */
int parse_granularity(char *s)
{
	char *w;
	u_int granu;

	granu = DEFAULT_TIMER_INTERVAL;

	if( EQUAL((w= next_word(&s)),""))
	{
		log(LOG_WARNING,0,
		"Missing timer granularity ; set to default %u",
		granu);
		return FALSE;
	}
	else
	{
		if( sscanf(w,"%u",&granu)!=1)
		{
			granu=DEFAULT_TIMER_INTERVAL;
			log(LOG_WARNING,0,
			"Invalid timer granularity value '%s' ; set to default %u",
			w,granu);
		}
		timer_interval = granu;
		if(granu < 1)
		{
			granu = DEFAULT_TIMER_INTERVAL;
			log(LOG_WARNING,0,
			"Timer granularity MUST be > 1! ; set to default %u",
			granu);
		}
	}

	timer_interval = granu;
	return TRUE;
}

/* function name : parse_data_timeout
 * input char *s
 * output int
 * operation : reads and assigns the data_timeout of each (S,G)
 * 		General form :
 * 		'data_timeout <number>
 * default : 210 s.
 */
int parse_data_timeout(char *s)
{
	char *w;
	u_int time;

	time = PIM_DATA_TIMEOUT;

	if( EQUAL((w= next_word(&s)),""))
	{
		log(LOG_WARNING,0,
		"Missing data timeout ; set to default %u",
		time);
		return FALSE;
	}
	else
	{
		if( sscanf(w,"%u",&time)!=1)
		{
			time=PIM_DATA_TIMEOUT;
			log(LOG_WARNING,0,
			"Invalid data timeout value '%s' ; set to default %u",
			w,time);
		}
		pim_data_timeout = time;
		if(time < 1)
		{
			time = PIM_DATA_TIMEOUT;
			log(LOG_WARNING,0,
			"Data timeout must be > 1! ; set to default %u",
			time);
		}
	}

	pim_data_timeout = time;
	return TRUE;
}

/* function name : parse_register_suppression_timeout
 * input char *s
 * output int
 * operation : reads and assigns the register_suppression_timeout
 * 		General form :
 * 		'register_suppression_timeout <number>
 * default : 60 s.
 */
int parse_register_suppression_timeout(char *s)
{
	char *w;
	u_int time;

	time = PIM_REGISTER_SUPPRESSION_TIMEOUT;

	if( EQUAL((w= next_word(&s)),""))
	{
		log(LOG_WARNING,0,
		"Missing register suppression timeout ; set to default %u",
		time);
		return FALSE;
	}
	else
	{
		if( sscanf(w,"%u",&time)!=1)
		{
			time=PIM_REGISTER_SUPPRESSION_TIMEOUT;
			log(LOG_WARNING,0,
			"Invalid register suppression timeout value '%s' ; set to default %u",
			w,time);
		}
		pim_register_suppression_timeout = time;
		if(time < 1)
		{
			time = PIM_REGISTER_SUPPRESSION_TIMEOUT;
			log(LOG_WARNING,0,
			"Register suppression timeout must be > 1! ; set to default %u",
			time);
		}
	}

	pim_register_suppression_timeout = time;
	return TRUE;
}

/* function name : parse_probe_time
 * input char *s
 * output int
 * operation : reads and assigns the probe_time for null-register
 * 		General form :
 * 		'probe_time <number>
 * default : 5 s.
 */
int parse_probe_time(char *s)
{
	char *w;
	u_int time;

	time = PIM_REGISTER_PROBE_TIME;

	if( EQUAL((w= next_word(&s)),""))
	{
		log(LOG_WARNING,0,
		"Missing register probe time ; set to default %u",
		time);
		return FALSE;
	}
	else
	{
		if( sscanf(w,"%u",&time)!=1)
		{
			time=PIM_REGISTER_PROBE_TIME;
			log(LOG_WARNING,0,
			"Invalid register probe time value '%s' ; set to default %u",
			w,time);
		}
		pim_register_probe_time = time;
		if(time < 1)
		{
			time = PIM_REGISTER_SUPPRESSION_TIMEOUT;
			log(LOG_WARNING,0,
			"Register probe time must be > 1! ; set to default %u",
			time);
		}
	}

	pim_register_probe_time = time;
	return TRUE;
}

/* function name : parse_assert_timeout
 * input char *s
 * output int
 * operation : reads and assigns the assert timeout
 * 		General form :
 * 		'assert_timeout <number>
 * default : 180 s.
 */
int parse_assert_timeout(char *s)
{
	char *w;
	u_int time;

	time = PIM_ASSERT_TIMEOUT;

	if( EQUAL((w= next_word(&s)),""))
	{
		log(LOG_WARNING,0,
		"Missing assert time out; set to default %u",
		time);
		return FALSE;
	}
	else
	{
		if( sscanf(w,"%u",&time)!=1)
		{
			time=PIM_ASSERT_TIMEOUT;
			log(LOG_WARNING,0,
			"Invalid assert time out value '%s' ; set to default %u",
			w,time);
		}
		pim_assert_timeout = time;
		if(time < 1)
		{
			time = PIM_ASSERT_TIMEOUT;
			log(LOG_WARNING,0,
			"Assert time out must be > 1! ; set to default %u",
			time);
		}
	}

	pim_assert_timeout = time;
	return TRUE;
}




char *next_word(char **s)
{
    char *w;

    w = *s;
    while (*w == ' ' || *w == '\t')
    w++;

    *s = w;
    for(;;) {
    switch (**s) {
    case ' '  :
    case '\t' :
        **s = '\0';
        (*s)++;
        return(w);
    case '\n' :
    case '#'  :
        **s = '\0';
        return(w);
    case '\0' :
        return(w);
    default   :
        if (isascii(**s) && isupper(**s))
        **s = tolower(**s);
        (*s)++;
    }
    }
}
