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
 * $FreeBSD: src/usr.sbin/pim6sd/kern.c,v 1.1.2.1 2000/07/15 07:36:36 kris Exp $
 */

#include <sys/time.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet6/ip6_mroute.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <net/if_var.h>
#endif
#include <netinet6/in6_var.h>
#include <syslog.h>
#include "pimd.h"
#include "inet6.h"
#include "vif.h"
#include "mrt.h"
#include "debug.h"
#include "kern.h"


/*  
 * Open/init the multicast routing in the kernel and sets the MRT_ASSERT
 * flag in the kernel.
 *
 */


void 
k_init_pim(int socket)
{
    int             v = 1;

    if (setsockopt(socket, IPPROTO_IPV6, MRT6_INIT, (char *) &v, sizeof(int)) < 0)
	log(LOG_ERR, errno, "cannot enable multicast routing in kernel");

    if (setsockopt(socket, IPPROTO_IPV6, MRT6_PIM, (char *) &v, sizeof(int)) < 0)
	log(LOG_ERR, errno, "Pim kernel initialization");
}

/*
 * Stops the multicast routing in the kernel and resets the MRT_ASSERT flag
 * in the kernel.
 */

void
k_stop_pim(socket)
    int             socket;
{
    int             v = 0;

    if (setsockopt(socket, IPPROTO_IPV6, MRT6_PIM,
		   (char *) &v, sizeof(int)) < 0)
	log(LOG_ERR, errno, "Cannot reset PIM flag in kernel");

    if (setsockopt(socket, IPPROTO_IPV6, MRT6_DONE, (char *) NULL, 0) < 0)
	log(LOG_ERR, errno, "cannot disable multicast routing in kernel");

}

/* 
 * Set the socket receiving buffer. `bufsize` is the preferred size,
 * `minsize` is the smallest acceptable size.
 */ 

void 
k_set_rcvbuf(int socket, int bufsize, int minsize)
{
    int             delta = bufsize / 2;
    int             iter = 0;

    /*
     * Set the socket buffer.  If we can't set it as large as we
     * want, search around to try to find the highest acceptable
     * value.  The highest acceptable value being smaller than
     * minsize is a fatal error. 
     */


    if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, (char *) &bufsize, sizeof(bufsize)) < 0)
    {
	bufsize -= delta;
	while (1)
	{
	    iter++;
	    if (delta > 1)
		delta /= 2;
	    if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, (char *) &bufsize, sizeof(bufsize)) < 0)
		bufsize -= delta;
	    else
	    {
		if (delta < 1024)
		    break;
		bufsize += delta;
	    }
	}
	if (bufsize < minsize)
        	log(LOG_ERR, 0, "OS-allowed buffer size %u < app min %u",
        	bufsize, minsize);
        	/*NOTREACHED*/


    }
    IF_DEBUG(DEBUG_KERN)
		log(LOG_DEBUG,0,"Buffer reception size for socket %d : %d in %d iterations",socket, bufsize, iter);
}

/*  
 * Set the default Hop Limit for the multicast packets outgoing from this
 * socket.
 */ 

void 
k_set_hlim(int socket, int h)
{
    int             hlim = h;

    if (setsockopt(socket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char *) &hlim, sizeof(hlim)) < 0)
		log(LOG_ERR,errno,"k_set_hlim");

}

/*
 * Set/reset the IPV6_MULTICAST_LOOP. Set/reset is specified by "flag".
 */


void 
k_set_loop(int socket, int flag)
{
    u_int           loop;

    loop = flag;
    if (setsockopt(socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (char *) &loop, sizeof(loop)) < 0)
		log(LOG_ERR,errno,"k_set_loop");
}

/*
 * Set the IPV6_MULTICAST_IF option on local interface which has the
 * specified index.
 */  


void 
k_set_if(int socket, u_int ifindex)
{
    if (setsockopt(socket, IPPROTO_IPV6, IPV6_MULTICAST_IF,
		   (char *) &ifindex, sizeof(ifindex)) < 0)
	   log(LOG_ERR, errno, "setsockopt IPV6_MULTICAST_IF for %s",
        ifindex2str(ifindex));

}

/*
 * Join a multicast grp group on local interface ifa.
 */  

void 
k_join(int socket, struct in6_addr * grp, u_int ifindex)
{
    struct ipv6_mreq mreq;

    mreq.ipv6mr_multiaddr = *grp;
    mreq.ipv6mr_interface = ifindex;

    if (setsockopt(socket, IPPROTO_IPV6, IPV6_JOIN_GROUP,
		   (char *) &mreq, sizeof(mreq)) < 0)
	syslog(LOG_WARNING, "Cannot join group %s on interface %s",
	       inet6_fmt(grp), ifindex2str(ifindex));
}

/*
 * Leave a multicats grp group on local interface ifa.
 */  

void 
k_leave(int socket, struct in6_addr * grp, u_int ifindex)
{
    struct ipv6_mreq mreq;

    mreq.ipv6mr_multiaddr = *grp;
    mreq.ipv6mr_interface = ifindex;

    if (setsockopt(socket, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
		   (char *) &mreq, sizeof(mreq)) < 0)
	syslog(LOG_WARNING, "Cannot leave group %s on interface %s",
	       inet6_fmt(grp), ifindex2str(ifindex));
}

/* 
 * Add a virtual interface in the kernel.
 */

void 
k_add_vif(int socket, vifi_t vifi, struct uvif * v)
{
    struct mif6ctl  mc;

    mc.mif6c_mifi = vifi;
    mc.mif6c_flags = v->uv_flags;

    mc.mif6c_pifi = v->uv_ifindex;

    if ((v->uv_flags & MIFF_REGISTER))
		IF_DEBUG(DEBUG_PIM_REGISTER)
			log(LOG_DEBUG,0,"register vifi : %d , register pifi : %d ", vifi, v->uv_ifindex);

    if (setsockopt(socket, IPPROTO_IPV6, MRT6_ADD_MIF,
		   (char *) &mc, sizeof(mc)) < 0)
	log(LOG_ERR, errno, "setsockopt MRT6_ADD_MIF on mif %d", vifi);
}

/*
 * Delete a virtual interface in the kernel.
 */

void 
k_del_vif(int socket, vifi_t vifi)
{
    if (setsockopt(socket, IPPROTO_IPV6, MRT6_DEL_MIF,
		   (char *) &vifi, sizeof(vifi)) < 0)
	log(LOG_ERR, errno, "setsockopt MRT6_DEL_MIF on mif %d", vifi);
}

/*
 * Delete all MFC entries for particular routing entry from the kernel.
 */  

int 
k_del_mfc(int socket, struct sockaddr_in6 * source, struct sockaddr_in6 * group)
{
    struct mf6cctl  mc;

    mc.mf6cc_origin = *source;
    mc.mf6cc_mcastgrp = *group;

    pim6dstat.kern_del_cache++;
    if (setsockopt(socket, IPPROTO_IPV6, MRT6_DEL_MFC, (char *) &mc, sizeof(mc)) < 0)
    {
	pim6dstat.kern_del_cache_fail++;
	log(LOG_WARNING, errno, "setsockopt MRT6_DEL_MFC");	
	return FALSE;
    }

    syslog(LOG_DEBUG, "Deleted MFC entry : src %s ,grp %s", inet6_fmt(&source->sin6_addr),
	   inet6_fmt(&group->sin6_addr));

    return TRUE;
}

/*
 * Install/modify a MFC entry in the kernel
 */

int
k_chg_mfc(socket, source, group, iif, oifs, rp_addr)
    int             socket;
    struct sockaddr_in6 *source;
    struct sockaddr_in6 *group;
    vifi_t          iif;
    if_set         *oifs;
    struct sockaddr_in6 *rp_addr;
{
    struct mf6cctl  mc;
    vifi_t          vifi;
    struct uvif    *v;

    mc.mf6cc_origin = *source;
    mc.mf6cc_mcastgrp = *group;
    mc.mf6cc_parent = iif;


    IF_ZERO(&mc.mf6cc_ifset);

    for (vifi = 0, v = uvifs; vifi < numvifs; vifi++, v++)
    {
	if (IF_ISSET(vifi, oifs))
	    IF_SET(vifi, &mc.mf6cc_ifset);
	else
	    IF_CLR(vifi, &mc.mf6cc_ifset);
    }

#ifdef PIM_REG_KERNEL_ENCAP
    mc.mf6cc_rp_addr.s_addr = rp_addr;
#endif

    pim6dstat.kern_add_cache++;
    if (setsockopt(socket, IPPROTO_IPV6, MRT6_ADD_MFC, (char *) &mc,
		   sizeof(mc)) < 0)
    {
	pim6dstat.kern_add_cache_fail++;
	log(LOG_WARNING, errno,
	    "setsockopt MRT_ADD_MFC for source %s and group %s",
	    inet6_fmt(&source->sin6_addr), inet6_fmt(&group->sin6_addr));
	return (FALSE);
    }
    return (TRUE);
}




/*
 * Get packet counters for particular interface
 */
/*
 * XXX: TODO: currently not used, but keep just in case we need it later.
 */

int 
k_get_vif_count(vifi, retval)
    vifi_t          vifi;
    struct vif_count *retval;
{
    struct sioc_mif_req6 mreq;

    mreq.mifi = vifi;
    if (ioctl(udp_socket, SIOCGETMIFCNT_IN6, (char *) &mreq) < 0)
    {
	log(LOG_WARNING, errno, "SIOCGETMIFCNT_IN6 on vif %d", vifi);
	retval->icount = retval->ocount = retval->ibytes =
	    retval->obytes = 0xffffffff;
	return (1);
    }
    retval->icount = mreq.icount;
    retval->ocount = mreq.ocount;
    retval->ibytes = mreq.ibytes;
    retval->obytes = mreq.obytes;
    return (0);
}


/*
 * Gets the number of packets, bytes, and number of packets arrived on wrong
 * if in the kernel for particular (S,G) entry.
 */

int
k_get_sg_cnt(socket, source, group, retval)
    int             socket;	/* udp_socket */
    struct sockaddr_in6 *source;
    struct sockaddr_in6 *group;
    struct sg_count *retval;
{
    struct sioc_sg_req6 sgreq;

    sgreq.src = *source;
    sgreq.grp = *group;
    if (ioctl(socket, SIOCGETSGCNT_IN6, (char *) &sgreq) < 0)
    {
	pim6dstat.kern_sgcnt_fail++;
	log(LOG_WARNING, errno, "SIOCGETSGCNT_IN6 on (%s %s)",
	    inet6_fmt(&source->sin6_addr), inet6_fmt(&group->sin6_addr));
	retval->pktcnt = retval->bytecnt = retval->wrong_if = ~0;	/* XXX */
	return (1);
    }
    retval->pktcnt = sgreq.pktcnt;
    retval->bytecnt = sgreq.bytecnt;
    retval->wrong_if = sgreq.wrong_if;
    return (0);
}
