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
 *  Pavlin Ivanov Radoslavov (pavlin@catarina.usc.edu)
 *
 *  $Id: vif.c,v 1.5 2000/05/18 15:29:40 itojun Exp $
 */
/*
 * Part of this program has been derived from mrouted.
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE.mrouted".
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 * $FreeBSD: src/usr.sbin/pim6dd/vif.c,v 1.1.2.1 2000/07/15 07:36:30 kris Exp $
 */

#include "defs.h"

/*
 * Exported variables.
 */
struct uvif	uvifs[MAXMIFS]; /* array of all virtual interfaces          */
vifi_t		numvifs;	/* Number of vifs in use                    */
int             vifs_down;      /* 1=>some interfaces are down              */
int             phys_vif;       /* An enabled vif                           */
int		udp_socket;	/* Since the honkin' kernel doesn't support */
				/* ioctls on raw IP sockets, we need a UDP  */
				/* socket as well as our IGMP (raw) socket. */
				/* How dumb.                                */
int             total_interfaces; /* Number of all interfaces: including the
				   * non-configured, but excluding the
				   * loopback interface and the non-multicast
				   * capable interfaces.
				   */
if_set		if_nullset;	/* an interface face that has all-0 bit
				 * (for comparison)
				 */

/*
 * Forward declarations.
 */
static void start_vif      __P((vifi_t vifi));
static void stop_vif       __P((vifi_t vifi));
static void start_all_vifs __P((void));


void
init_vifs()
{
    vifi_t vifi;
    struct uvif *v;
    int enabled_vifs;
	
    numvifs    = 0;
    vifs_down = FALSE;

    /*
     * Configure the vifs based on the interface configuration of
     * the kernel and the contents of the configuration file.
     * (Open a UDP socket for ioctl use in the config procedures if
     * the kernel can't handle IOCTL's on the MLD socket.)
     */
#ifdef IOCTL_OK_ON_RAW_SOCKET
    udp_socket = mld6_socket;
#else
    if ((udp_socket = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
	log(LOG_ERR, errno, "UDP6 socket");
#endif

    /*
     * Clean up all vifs
     */
    for (vifi = 0, v = uvifs; vifi < MAXMIFS; ++vifi, ++v) {
	memset(v, 0, sizeof(*v));
	v->uv_flags		= 0;
	v->uv_metric		= DEFAULT_METRIC;
	v->uv_admetric		= 0;
	v->uv_rate_limit	= DEFAULT_PHY_RATE_LIMIT;
	strncpy(v->uv_name, "", IFNAMSIZ);
	v->uv_groups		= (struct listaddr *)NULL;
	v->uv_dvmrp_neighbors   = (struct listaddr *)NULL;
	NBRM_CLRALL(v->uv_nbrmap);
	v->uv_querier           = (struct listaddr *)NULL;
	v->uv_prune_lifetime    = 0;
	v->uv_acl               = (struct vif_acl *)NULL;
	v->uv_leaf_timer        = 0;
	v->uv_addrs		= (struct phaddr *)NULL;
	v->uv_filter		= (struct vif_filter *)NULL;
	v->uv_pim_hello_timer   = 0;
	v->uv_gq_timer          = 0;
	v->uv_pim_neighbors	= (struct pim_nbr_entry *)NULL;
	v->uv_local_pref        = default_source_preference;
	v->uv_local_metric      = default_source_metric;
    }

    log(LOG_INFO, 0, "Getting ifs from kernel");
    config_vifs_from_kernel();
    log(LOG_INFO, 0, "Getting ifs from %s", configfilename);
    config_vifs_from_file();

    /*
     * Quit if there are fewer than two enabled vifs or there is a vif
     * which has no link local address.
     */
    enabled_vifs    = 0;
    phys_vif        = -1;
	 
    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	if (v->uv_flags & (VIFF_DISABLED | VIFF_DOWN))
	    continue;
	if (v->uv_linklocal == NULL)
		log(LOG_ERR, 0,
		    "there is no link-local address on vif#%d", vifi);
	if (phys_vif == -1) {
	    struct phaddr *p;

	    /*
	     * If this vif has a global address, set its id
	     * to phys_vif.
	     */
	    for(p = v->uv_addrs; p; p = p->pa_next) {
		if (!IN6_IS_ADDR_LINKLOCAL(&p->pa_addr.sin6_addr) &&
		    !IN6_IS_ADDR_SITELOCAL(&p->pa_addr.sin6_addr)) {
		    phys_vif = vifi;
		    break;
		}
	    }
	}
	enabled_vifs++;
    }

    if (enabled_vifs < 2)
	log(LOG_ERR, 0, "can't forward: %s",
	    enabled_vifs == 0 ? "no enabled ifs" : "only one enabled if");

    memset(&if_nullset, 0, sizeof(if_nullset));
    k_init_pim(mld6_socket);	/* Call to kernel to initiliaze structures */

    start_all_vifs();
}


static void
start_all_vifs()
{
    vifi_t vifi;
    struct uvif *v;

    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	/* Start vif if not DISABLED or DOWN */
	if (v->uv_flags & (VIFF_DISABLED | VIFF_DOWN)) {
	    if (v->uv_flags & VIFF_DISABLED)
		log(LOG_INFO, 0,
		    "%s is DISABLED; if #%u out of service", 
		    v->uv_name, vifi);
	    else
		log(LOG_INFO, 0,
		    "%s is DOWN; if #%u out of service", 
		    v->uv_name, vifi);
	    }
	else
	    start_vif(vifi);
    }
}



/*
 * stop all vifs
 */
void
stop_all_vifs()
{
    vifi_t vifi;
    struct uvif *v;

    for (vifi = 0, v=uvifs; vifi < numvifs; ++vifi, ++v) {
	if (!(v->uv_flags &  VIFF_DOWN)) {
	    stop_vif(vifi);
	}
    }
}


/*
 * Initialize the vif and add to the kernel. The vif can be either
 * physical, tunnel (tunnels will be used in the future
 * when this code becomes PIM multicast boarder router.)
 */
static void 
start_vif(vifi)
    vifi_t vifi;
{
    struct uvif *v;
    u_long random_delay;

    v		    = &uvifs[vifi];
    /* Initialy no router on any vif */
    v->uv_flags = (v->uv_flags | VIFF_DR | VIFF_NONBRS) & ~VIFF_DOWN;
    v->uv_pim_hello_timer = 1 + RANDOM() % PIM_TIMER_HELLO_PERIOD;
    /* TODO: CHECK THE TIMERS!!!!! Set or reset? */
    v->uv_gq_timer  = 0;
    v->uv_pim_neighbors = (pim_nbr_entry_t *)NULL;
    
    /* Tell kernel to add, i.e. start this vif */
    k_add_vif(mld6_socket, vifi, &uvifs[vifi]);   
    log(LOG_INFO, 0, "%s comes up; if #%u now in service", v->uv_name, vifi);
    
    /*
     * Join the PIM multicast group on the interface.
     */
    k_join(mld6_socket, &allpim6routers_group.sin6_addr, v->uv_ifindex);
	
    /*
     * Join the ALL-ROUTERS multicast group on the interface.
     * This allows mtrace requests to loop back if they are run
     * on the multicast router.
     */
    k_join(mld6_socket, &allrouters_group.sin6_addr, v->uv_ifindex);
	
    /*
     * Until neighbors are discovered, assume responsibility for sending
     * periodic group membership queries to the subnet.  Send the first
     * query.
     */
    v->uv_flags |= VIFF_QUERIER;
    if (!v->uv_querier) {
	v->uv_querier = (struct listaddr *)malloc(sizeof(*v->uv_querier));
	memset(v->uv_querier, 0, sizeof(*v->uv_querier));
    }
    v->uv_querier->al_addr = v->uv_linklocal->pa_addr;
    v->uv_querier->al_timer = MLD6_OTHER_QUERIER_PRESENT_INTERVAL;
    time(&v->uv_querier->al_ctime); /* reset timestamp */
    query_groups(v);

    /*
     * To avoid synchronization among routers booting simultaneously, set
     * the hello timer to a random value between 1 to PIM_TIMER_HELLO_PERIOD.
     */
    random_delay = 1 + (random() % (long)(PIM_TIMER_HELLO_PERIOD - 1));
    v->uv_pim_hello_timer = random_delay;
}


/*
 * Stop a vif (either physical interface or tunnel).
 * If we are running only PIM we don't have tunnels.
 */
static void 
stop_vif(vifi)
    vifi_t vifi;
{
    struct uvif *v;
    struct listaddr *a;
    register pim_nbr_entry_t *n, *next;
    struct vif_acl *acl;
    
    /*
     * TODO: make sure that the kernel viftable is 
     * consistent with the daemon table
     */	
    v = &uvifs[vifi];
    k_leave(mld6_socket, &allpim6routers_group.sin6_addr, v->uv_ifindex);
    k_leave(mld6_socket, &allrouters_group.sin6_addr, v->uv_ifindex);
    /*
     * Discard all group addresses.  (No need to tell kernel;
     * the k_del_vif() call will clean up kernel state.)
     */
    while (v->uv_groups != NULL) {
	a = v->uv_groups;
	v->uv_groups = a->al_next;
	free((char *)a);
    }
    
    /*
     * TODO: inform (eventually) the neighbors I am going down by sending
     * PIM_HELLO with holdtime=0 so someone else should become a DR.
     */ 

    /* TODO: dummy! Implement it!! Any problems if don't use it? */
    delete_vif_from_mrt(vifi);
    
    /*
     * Delete the interface from the kernel's vif structure.
     */
    k_del_vif(mld6_socket, vifi);
    v->uv_flags     = (v->uv_flags & ~VIFF_DR & ~VIFF_QUERIER & ~VIFF_NONBRS )
	              | VIFF_DOWN;
    v->uv_pim_hello_timer = 0;
    v->uv_gq_timer  = 0;
    for (n = v->uv_pim_neighbors; n != NULL; n = next) {
	next = n->next;	/* Free the space for each neighbour */
	free((char *)n);
    }
    v->uv_pim_neighbors = NULL;

    /* TODO: currently not used */
   /* The Access Control List (list with the scoped addresses) */
    while (v->uv_acl != NULL) {
	acl = v->uv_acl;
	v->uv_acl = acl->acl_next;
	free((char *)acl);
    }

    vifs_down = TRUE;
    log(LOG_INFO, 0,
	"%s goes down; if #%u out of service", v->uv_name, vifi);
}		

/*
 * return the max global Ipv6 address of an UP and ENABLED interface
 * other than the MIFF_REGISTER interface.
*/
struct sockaddr_in6 *
max_global_address()
{
	vifi_t vifi;
	struct uvif *v;
	struct phaddr *p;
	struct phaddr *pmax = NULL;

	for(vifi=0,v=uvifs;vifi< numvifs;++vifi,++v)
	{
		if(v->uv_flags & (VIFF_DISABLED | VIFF_DOWN | MIFF_REGISTER))
			continue;
		/*
		 * take first the max global address of the interface
		 * (without link local) => aliasing
		 */
		for(p=v->uv_addrs;p!=NULL;p=p->pa_next)
		{
			/*
			 * If this is the first global address, take it anyway.
			 */
			if (pmax == NULL) {
				if (!IN6_IS_ADDR_LINKLOCAL(&p->pa_addr.sin6_addr) &&
				    !IN6_IS_ADDR_SITELOCAL(&p->pa_addr.sin6_addr))
					pmax = p;
			}
			else {
				if (inet6_lessthan(&pmax->pa_addr,
						   &p->pa_addr) &&
				    !IN6_IS_ADDR_LINKLOCAL(&p->pa_addr.sin6_addr) &&
				    !IN6_IS_ADDR_SITELOCAL(&p->pa_addr.sin6_addr))
					pmax=p;	
			}
		}
	}

	return(pmax ? &pmax->pa_addr : NULL);
}

struct sockaddr_in6 *
uv_global(vifi)
	vifi_t vifi;
{
	struct uvif *v = &uvifs[vifi];
	struct phaddr *p;

	for (p = v->uv_addrs; p; p = p->pa_next) {
		if (!IN6_IS_ADDR_LINKLOCAL(&p->pa_addr.sin6_addr) &&
		    !IN6_IS_ADDR_SITELOCAL(&p->pa_addr.sin6_addr))
			return(&p->pa_addr);
	}

	return(NULL);
}

/*
 * See if any interfaces have changed from up state to down, or vice versa,
 * including any non-multicast-capable interfaces that are in use as local
 * tunnel end-points.  Ignore interfaces that have been administratively
 * disabled.
 */
void
check_vif_state()
{
    register vifi_t vifi;
    register struct uvif *v;
    struct ifreq ifr;
    static int checking_vifs = 0;

    /*
     * XXX: TODO: True only for DVMRP?? Check.
     * If we get an error while checking, (e.g. two interfaces go down
     * at once, and we decide to send a prune out one of the failed ones)
     * then don't go into an infinite loop!
     */
    if (checking_vifs)
	return;

    vifs_down = FALSE;
    checking_vifs = 1;
    /* TODO: Check all potential interfaces!!! */
    /* Check the physical interfaces only */
    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	if (v->uv_flags & VIFF_DISABLED)
	    continue;
	
	strncpy(ifr.ifr_name, v->uv_name, IFNAMSIZ);
	/* get the interface flags */
	if (ioctl(udp_socket, SIOCGIFFLAGS, (char *)&ifr) < 0)
	    log(LOG_ERR, errno,
		"check_vif_state: ioctl SIOCGIFFLAGS for %s", ifr.ifr_name);

	if (v->uv_flags & VIFF_DOWN) {
	    if (ifr.ifr_flags & IFF_UP) {
		start_vif(vifi);
	    }
	    else vifs_down = TRUE;
	}
	else {
	    if (!(ifr.ifr_flags & IFF_UP)) {
		log(LOG_NOTICE, 0,
		    "%s has gone down; if #%u taken out of service",
		    v->uv_name, vifi);
		stop_vif(vifi);
		vifs_down = TRUE;
	    }
	}
    }
    checking_vifs = 0;
}


/*
 * If the source is directly connected to us, find the vif number for
 * the corresponding physical interface (tunnels excluded).
 * Local addresses are excluded.
 * Return the vif number or NO_VIF if not found.
 */
vifi_t 
find_vif_direct(src)
    struct sockaddr_in6 *src;
{
    vifi_t vifi;
    register struct uvif *v;
    register struct phaddr *p;
	
    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	if (v->uv_flags & (VIFF_DISABLED | VIFF_DOWN | VIFF_TUNNEL))
	    continue;
	for (p = v->uv_addrs; p; p = p->pa_next) {
		if (inet6_equal(src, &p->pa_addr))
			return(NO_VIF);
		if (inet6_match_prefix(src, &p->pa_prefix, &p->pa_subnetmask))
			return(vifi);
	}
    }
    return (NO_VIF);
} 


/*
 * Checks if src is local address. If "yes" return the vif index,
 * otherwise return value is NO_VIF.
 */
vifi_t
local_address(src)
    struct sockaddr_in6 *src;
{
    vifi_t vifi;
    register struct uvif *v;
    register struct phaddr *p;
    
    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	if (v->uv_flags & (VIFF_DISABLED | VIFF_DOWN))
	    continue;
	for (p = v->uv_addrs; p; p = p->pa_next) {
		if (inet6_equal(src, &p->pa_addr))
			return(vifi);
	}
    }   
    /* Returning NO_VIF means not a local address */
    return (NO_VIF);	
}

/*
 * If the source is directly connected, or is local address,
 * find the vif number for the corresponding physical interface
 * (tunnels excluded).
 * Return the vif number or NO_VIF if not found.
 */
vifi_t 
find_vif_direct_local(src)
    struct sockaddr_in6 *src;
{
    vifi_t vifi;
    register struct uvif *v;
    register struct phaddr *p;
	
    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	if (v->uv_flags & (VIFF_DISABLED | VIFF_DOWN | VIFF_TUNNEL))
	    continue;
	for (p = v->uv_addrs; p; p = p->pa_next) {
		if (inet6_equal(src, &p->pa_addr) ||
		    inet6_match_prefix(src, &p->pa_prefix, &p->pa_subnetmask))
		return(vifi);
	}
    }
    return (NO_VIF);
} 
