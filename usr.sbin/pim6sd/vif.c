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
 * $FreeBSD: src/usr.sbin/pim6sd/vif.c,v 1.1.2.1 2000/07/15 07:36:37 kris Exp $
 */

#include <sys/ioctl.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include "vif.h"
#include "mld6.h"
#include "pim6.h"
#include "pimd.h"
#include "route.h"
#include "config.h"
#include "inet6.h"
#include "kern.h"
#include "mld6_proto.h"
#include "pim6_proto.h"
#include "mrt.h"
#include "debug.h"
#include "timer.h"

struct uvif	uvifs[MAXMIFS];	/*the list of virtualsinterfaces */
vifi_t numvifs;				/*total number of interface */
int vifs_down;
vifi_t reg_vif_num;		   /*register interface*/
int phys_vif; /* An enabled vif that has a global address */
int udp_socket;
int total_interfaces;
if_set			if_nullset;
if_set			if_result;

int init_reg_vif __P((void));
void start_all_vifs __P((void));
void start_vif __P((vifi_t vifi));
void stop_vif __P((vifi_t vivi));
int update_reg_vif __P((vifi_t register_vifi));

extern int cfparse __P((int, int));

void init_vifs()
{
	vifi_t vifi;
	struct uvif *v;
	int enabled_vifs;

	numvifs = 0;
	reg_vif_num = NO_VIF;

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

	/* clean all the interfaces ... */

	for(vifi = 0,v=uvifs; vifi < MAXVIFS; ++ vifi, ++v)
	{
		memset(v,0,sizeof(*v)); /* everything is zeroed  => NULL , pointer NULL , addrANY ...) */
		v->uv_metric = DEFAULT_METRIC;
		v->uv_rate_limit = DEFAULT_PHY_RATE_LIMIT;
		strncpy(v->uv_name,"",IFNAMSIZ);
		v->uv_local_pref = default_source_preference;
		v->uv_local_metric = default_source_metric;
	}
	IF_DEBUG(DEBUG_IF)
		log(LOG_DEBUG,0,"Interfaces world initialized...");
	IF_DEBUG(DEBUG_IF)
		log(LOG_DEBUG,0,"Getting vifs from kernel");
	config_vifs_from_kernel();
	if (max_global_address() == NULL)
		log(LOG_ERR, 0, "There's no global address");
	IF_DEBUG(DEBUG_IF)
		log(LOG_DEBUG,0,"Getting vifs from %s",configfilename);

	/* read config from file */
	if (cfparse(1, 0) != 0)
		log(LOG_ERR, 0, "fatal error in parsing the config file");

	enabled_vifs = 0;
	phys_vif = -1;

	for( vifi = 0, v = uvifs ; vifi < numvifs ; ++ vifi,++v)
	{
		if(v->uv_flags & (VIFF_DISABLED | VIFF_DOWN | MIFF_REGISTER))
			continue;
		if(v->uv_linklocal == NULL)
			log(LOG_ERR,0,"there is no link-local address on vif %s",v->uv_name);
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
		log(LOG_ERR,0,"can't forward: %s",
		enabled_vifs == 0 ? "no enabled vifs" : "only one enabled vif" );

	memset(&if_nullset,0,sizeof(if_nullset));
	k_init_pim(mld6_socket);	
	IF_DEBUG(DEBUG_PIM_DETAIL)
		log(LOG_DEBUG,0,"Pim kernel initialization done");


	/* Add a dummy virtual interface to support Registers in the kernel. */
 	init_reg_vif();

	start_all_vifs();

}
int init_reg_vif()
{
	struct uvif *v;
	vifi_t i;

	v = &uvifs[numvifs];
	if (( numvifs+1 ) == MAXVIFS )
	{
     /* Exit the program! The PIM router must have a Register vif */
    log(LOG_ERR, 0,
        "cannot install the Register vif: too many interfaces");
    /* To make lint happy */
    return (FALSE);
	}

    /*
     * So far in PIM we need only one register vif and we save its number in
     * the global reg_vif_num.
     */


	reg_vif_num = numvifs;

    /* Use the address of the first available physical interface to
     * create the register vif.
     */

	for(i =0 ; i < numvifs ; i++)
	{
		if(uvifs[i].uv_flags & (VIFF_DOWN | VIFF_DISABLED | MIFF_REGISTER))
			continue;
	else
		break;
	}
	if( i >= numvifs)
	{
		  log(LOG_ERR, 0, "No physical interface enabled");
    	return -1;
    }

	
	memcpy(v,&uvifs[i],sizeof(*v));
	strncpy(v->uv_name,"register_mif0",IFNAMSIZ);
	v->uv_flags = MIFF_REGISTER;

#ifdef PIM_EXPERIMENTAL
	v->uv_flags |= MIFF_REGISTER_KERNEL_ENCAP;
#endif

    IF_DEBUG(DEBUG_IF)
		log(LOG_DEBUG,0,"Interface %s (subnet %s) ,installed on vif #%u - rate = %d",
    	v->uv_name,net6name(&v->uv_prefix.sin6_addr,&v->uv_subnetmask),
    	reg_vif_num,v->uv_rate_limit);

	numvifs++;
	total_interfaces++;
	return 0;	
}

void start_all_vifs()
{
	vifi_t vifi;
	struct uvif *v;
	u_int action;


   /* Start first the NON-REGISTER vifs */

	for(action=0; ;action = MIFF_REGISTER )
	{
		for(vifi= 0,v = uvifs;vifi < numvifs ; ++vifi, ++v)
		{
			if (( v->uv_flags & MIFF_REGISTER ) ^ action )
        /* If starting non-registers but the vif is a register
         * or if starting registers, but the interface is not
         * a register, then just continue.
         */
				continue;

			if ( v->uv_flags & (VIFF_DISABLED | VIFF_DOWN ))
			{
				IF_DEBUG(DEBUG_IF)
				{
					if ( v-> uv_flags & VIFF_DISABLED)
						log(LOG_DEBUG,0,"%s is DISABLED ; vif #%u out of service",v->uv_name,vifi); 
					else
						log(LOG_DEBUG,0,"%s is DOWN ; vif #%u out of service",v->uv_name,vifi);
				}	
			}
			else
				start_vif(vifi);
		}
		if ( action == MIFF_REGISTER)
			break;
	}	
}

/*
 * Initialize the vif and add to the kernel. The vif can be either
 * physical, register or tunnel (tunnels will be used in the future
 * when this code becomes PIM multicast boarder router.
 */


void start_vif (vifi_t vifi)
{
	struct uvif *v;

	v = &uvifs[vifi];

	/* Initialy no router on any vif */

	if( v-> uv_flags & MIFF_REGISTER)
		v->uv_flags = v->uv_flags & ~VIFF_DOWN;
	else
	{
		v->uv_flags = (v->uv_flags | VIFF_DR | VIFF_NONBRS) & ~ VIFF_DOWN;
		v->uv_pim_hello_timer = 1 + RANDOM() % pim_hello_period;
		v->uv_jp_timer = 1 + RANDOM() % pim_join_prune_period;
	}

	/* Tell kernel to add, i.e. start this vif */

	k_add_vif(mld6_socket,vifi,&uvifs[vifi]);
	IF_DEBUG(DEBUG_IF)
		log(LOG_DEBUG,0,"%s comes up ,vif #%u now in service",v->uv_name,vifi);

	if (!(v->uv_flags & MIFF_REGISTER)) {
	    /*
	     * Join the PIM multicast group on the interface.
	     */
	    k_join(mld6_socket, &allpim6routers_group.sin6_addr,
		   v->uv_ifindex);

	    /*
	     * Join the ALL-ROUTERS multicast group on the interface.
	     * This allows mtrace requests to loop back if they are run
	     * on the multicast router.this allow receiving mld6 messages too.
	     */
	    k_join(mld6_socket, &allrouters_group.sin6_addr, v->uv_ifindex);

	    /*
	     * Until neighbors are discovered, assume responsibility for sending
	     * periodic group membership queries to the subnet.  Send the first
	     * query.
	     */
	    v->uv_flags |= VIFF_QUERIER;
	    if (!v->uv_querier) {
		v->uv_querier = (struct listaddr *)malloc(sizeof(struct listaddr));
		memset(v->uv_querier, 0, sizeof(struct listaddr));
	    }
	    v->uv_querier->al_addr = v->uv_linklocal->pa_addr;
	    v->uv_querier->al_timer = MLD6_OTHER_QUERIER_PRESENT_INTERVAL;
	    time(&v->uv_querier->al_ctime); /* reset timestamp */
	    query_groups(v);
  
	    /*
	     * Send a probe via the new vif to look for neighbors.
	     */
	    send_pim6_hello(v, pim_hello_holdtime);
	}
}

/*
 * Stop a vif (either physical interface, tunnel or
 * register.) If we are running only PIM we don't have tunnels.
 */ 


void stop_vif( vifi_t vifi )
{
	struct uvif *v;
	struct listaddr *a;
	register pim_nbr_entry_t *n;
	register pim_nbr_entry_t *next;
	struct vif_acl *acl;

 
    /*
     * TODO: make sure that the kernel viftable is
     * consistent with the daemon table
     */

	v=&uvifs[vifi];
	if( !( v->uv_flags&MIFF_REGISTER ) )
	{
		k_leave( mld6_socket , &allpim6routers_group.sin6_addr , v->uv_ifindex );
		k_leave( mld6_socket , &allrouters_group.sin6_addr , v->uv_ifindex );
    /*
     * Discard all group addresses.  (No need to tell kernel;
     * the k_del_vif() call will clean up kernel state.)
     */

		while( v->uv_groups!=NULL )
		{
			a=v->uv_groups;
			v->uv_groups=a->al_next;
			free((char *)a);
		}
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

	k_del_vif( mld6_socket , vifi );
	v->uv_flags=(v->uv_flags & ~VIFF_DR & ~VIFF_QUERIER & ~VIFF_NONBRS) | VIFF_DOWN;
	if( !(v->uv_flags & MIFF_REGISTER ))
	{
    	RESET_TIMER(v->uv_pim_hello_timer);
    	RESET_TIMER(v->uv_jp_timer);
    	RESET_TIMER(v->uv_gq_timer);

		for( n=v->uv_pim_neighbors ; n!=NULL ; n = next )
		{
			next=n->next;			/* Free the space for each neighbour */
			free((char *)n);
		}
		v->uv_pim_neighbors=NULL;
	}

	
   /* TODO: currently not used */
   /* The Access Control List (list with the scoped addresses) */

	while( v->uv_acl!=NULL )
	{
		acl=v->uv_acl;
		v->uv_acl=acl->acl_next;
		free((char *)acl);
	}

	vifs_down=TRUE;

	IF_DEBUG(DEBUG_IF)
		log( LOG_DEBUG ,0,"%s goes down , vif #%u out of service" , v->uv_name , vifi);
}

/*
 * Update the register vif in the multicast routing daemon and the
 * kernel because the interface used initially to get its local address
 * is DOWN. register_vifi is the index to the Register vif which needs
 * to be updated. As a result the Register vif has a new uv_lcl_addr and
 * is UP (virtually :))
 */
int
update_reg_vif( vifi_t register_vifi )
{
    register struct uvif *v;
    register vifi_t vifi;

    /* Find the first useable vif with solid physical background */
    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	if (v->uv_flags & (VIFF_DISABLED | VIFF_DOWN | VIFF_TUNNEL
			   | MIFF_REGISTER))
	    continue;
        /* Found. Stop the bogus Register vif first */
	stop_vif(register_vifi);
	uvifs[register_vifi].uv_linklocal->pa_addr =
	    uvifs[vifi].uv_linklocal->pa_addr;
	start_vif(register_vifi);
	IF_DEBUG(DEBUG_PIM_REGISTER | DEBUG_IF)
	    log(LOG_NOTICE, 0, "%s has come up; vif #%u now in service",
		uvifs[register_vifi].uv_name, register_vifi);
	return 0;
    }
    vifs_down = TRUE;
    log(LOG_WARNING, 0, "Cannot start Register vif: %s",
	uvifs[vifi].uv_name);
    return(-1);
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
 * Check if the interface exists in the mif table. If true 
 * return the highest address of the interface else return NULL.
 */
struct sockaddr_in6 *
local_iface(char *ifname)
{
	register struct uvif *v;
	vifi_t vifi;
	struct phaddr *p;
	struct phaddr *pmax = NULL;

	for(vifi=0,v=uvifs;vifi<numvifs;++vifi,++v)
	{
		if (v->uv_flags & (VIFF_DISABLED | VIFF_DOWN | MIFF_REGISTER))
			continue;
		if(EQUAL(v->uv_name, ifname))
		{
			for(p=v->uv_addrs; p!=NULL; p=p->pa_next)
			{
				if (!IN6_IS_ADDR_LINKLOCAL(&p->pa_addr.sin6_addr)&&
				    !IN6_IS_ADDR_SITELOCAL(&p->pa_addr.sin6_addr)) {
					/*
					 * If this is the first global address
					 * or larger than the current MAX global
					 * address, remember it.
					 */
					if (pmax == NULL ||
					    inet6_lessthan(&pmax->pa_addr,
							   &p->pa_addr))
						pmax = p;
				}
			}
			if (pmax)
				return(&pmax->pa_addr);
		}
	}

	return NULL;
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
    static int checking_vifs=0;

    /*
     * XXX: TODO: True only for DVMRP?? Check.
     * If we get an error while checking, (e.g. two interfaces go down
     * at once, and we decide to send a prune out one of the failed ones)
     * then don't go into an infinite loop!
     */
    if( checking_vifs )
	return;

    vifs_down=FALSE;
    checking_vifs=TRUE;

    /* TODO: Check all potential interfaces!!! */
    /* Check the physical and tunnels only */
    for( vifi=0 , v=uvifs ; vifi<numvifs ; ++vifi , ++v )
    {
	if( v->uv_flags & ( VIFF_DISABLED|MIFF_REGISTER	) )
	    continue;

	strncpy( ifr.ifr_name , v->uv_name , IFNAMSIZ );
  
	/* get the interface flags */
	if( ioctl( udp_socket , SIOCGIFFLAGS , (char *)&ifr )<0 )
	    log(LOG_ERR, errno,
        	"check_vif_state: ioctl SIOCGIFFLAGS for %s", ifr.ifr_name);

	if( v->uv_flags & VIFF_DOWN )
	{
	    if ( ifr.ifr_flags & IFF_UP )
	    {
		start_vif( vifi );
	    }
	    else
		vifs_down=TRUE;
	}
	else
	{
	    if( !( ifr.ifr_flags & IFF_UP ))
	    {
		log( LOG_NOTICE ,0,
		     "%s has gone down ; vif #%u taken out of  service",
		     v->uv_name , vifi );
		stop_vif ( vifi );
		vifs_down = TRUE;
	    }
	}
    }

    /* Check the register(s) vif(s) */
    for( vifi=0 , v=uvifs ; vifi<numvifs ; ++vifi , ++v )
    {
	register vifi_t vifi2;
	register struct uvif *v2;
	int found;

	if( !(v->uv_flags & MIFF_REGISTER ) )
	    continue;
	else
	{
	    found=0;

	    /* Find a physical vif with the same IP address as the
	     * Register vif.
	     */
	    for( vifi2=0 , v2=uvifs ; vifi2<numvifs ; ++vifi2 , ++v2 )
	    {
		if( v2->uv_flags & ( VIFF_DISABLED|VIFF_DOWN|VIFF_TUNNEL|MIFF_REGISTER ))
		    continue;
		if( IN6_ARE_ADDR_EQUAL( &v->uv_linklocal->pa_addr.sin6_addr,
					&v2->uv_linklocal->pa_addr.sin6_addr ))
		{
		    found=1;
		    break;
		}
	    }
	    if(!found)
		/* The physical interface with the IP address as the Register
		 * vif is probably DOWN. Get a replacement.
		 */
		update_reg_vif( vifi );
	}
    }
    checking_vifs=0;
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
   
    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) 
    {
    	if (v->uv_flags & (VIFF_DISABLED | VIFF_DOWN | VIFF_TUNNEL|MIFF_REGISTER))
        	continue;
	for (p = v->uv_addrs; p; p = p->pa_next) 
	{
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
	if (v->uv_flags & (VIFF_DISABLED | VIFF_DOWN | MIFF_REGISTER))
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
    	if (v->uv_flags & (VIFF_DISABLED | VIFF_DOWN | VIFF_TUNNEL |MIFF_REGISTER))
        	continue;
    	for (p = v->uv_addrs; p; p = p->pa_next) {
        	if (inet6_equal(src, &p->pa_addr) ||
            	inet6_match_prefix(src, &p->pa_prefix, &p->pa_subnetmask))  
        		return(vifi);
    	}
    }
    return (NO_VIF);
}

int
vif_forwarder(if_set *p1 , if_set *p2)
{
	int idx;

	for(idx=0 ; idx < sizeof(*p1)/sizeof(fd_mask) ; idx++)
	{
		if (p1->ifs_bits[idx] & p2->ifs_bits[idx])
			return(TRUE);
		
	}

	/* (p1 & p2) is empty. We're not the forwarder */
	return(FALSE);
}

if_set *
vif_and(if_set *p1 , if_set *p2, if_set *result)
{
	int idx;

	IF_ZERO(result);

	for(idx=0 ; idx < sizeof(*p1)/sizeof(fd_mask) ; idx++)
	{
		result->ifs_bits[idx] = p1->ifs_bits[idx] & p2->ifs_bits[idx];
	}

	return(result);
}

if_set *
vif_xor(if_set *p1 , if_set *p2, if_set *result)
{
	int idx;

	IF_ZERO(result);

	for(idx=0 ; idx < sizeof(*p1)/sizeof(fd_mask) ; idx++)
	{
		result->ifs_bits[idx] =
			p1->ifs_bits[idx] ^ p2->ifs_bits[idx];
	}

	return(result);
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

struct uvif *
find_vif(ifname)
	char *ifname;
{
	struct uvif *v;
	vifi_t vifi;

	for (vifi = 0, v = uvifs; vifi < numvifs ; ++vifi , ++v) {
		if (strcasecmp(v->uv_name, ifname) == 0)
			return(v);
	}

	return(NULL);
}
