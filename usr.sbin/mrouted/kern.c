/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: kern.c,v 1.2 1994/09/08 02:51:17 wollman Exp $
 */


#include "defs.h"


void k_set_rcvbuf(bufsize)
    int bufsize;
{
    if (setsockopt(igmp_socket, SOL_SOCKET, SO_RCVBUF,
		   (char *)&bufsize, sizeof(bufsize)) < 0)
	log(LOG_ERR, errno, "setsockopt SO_RCVBUF %u", bufsize);
}


void k_hdr_include(bool)
    int bool;
{
#ifdef IP_HDRINCL
    if (setsockopt(igmp_socket, IPPROTO_IP, IP_HDRINCL,
		   (char *)&bool, sizeof(bool)) < 0)
	log(LOG_ERR, errno, "setsockopt IP_HDRINCL %u", bool);
#endif
}


void k_set_ttl(t)
    int t;
{
    u_char ttl;

    ttl = t;
    if (setsockopt(igmp_socket, IPPROTO_IP, IP_MULTICAST_TTL,
		   (char *)&ttl, sizeof(ttl)) < 0)
	log(LOG_ERR, errno, "setsockopt IP_MULTICAST_TTL %u", ttl);
}


void k_set_loop(l)
    int l;
{
    u_char loop;

    loop = l;
    if (setsockopt(igmp_socket, IPPROTO_IP, IP_MULTICAST_LOOP,
		   (char *)&loop, sizeof(loop)) < 0)
	log(LOG_ERR, errno, "setsockopt IP_MULTICAST_LOOP %u", loop);
}


void k_set_if(ifa)
    u_long ifa;
{
    struct in_addr adr;

    adr.s_addr = ifa;
    if (setsockopt(igmp_socket, IPPROTO_IP, IP_MULTICAST_IF,
		   (char *)&adr, sizeof(adr)) < 0)
	log(LOG_ERR, errno, "setsockopt IP_MULTICAST_IF %s",
	    		    inet_fmt(ifa, s1));
}


void k_join(grp, ifa)
    u_long grp;
    u_long ifa;
{
    struct ip_mreq mreq;

    mreq.imr_multiaddr.s_addr = grp;
    mreq.imr_interface.s_addr = ifa;

    if (setsockopt(igmp_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		   (char *)&mreq, sizeof(mreq)) < 0)
	log(LOG_WARNING, errno, "can't join group %s on interface %s",
				inet_fmt(grp, s1), inet_fmt(ifa, s2));
}


void k_leave(grp, ifa)
    u_long grp;
    u_long ifa;
{
    struct ip_mreq mreq;

    mreq.imr_multiaddr.s_addr = grp;
    mreq.imr_interface.s_addr = ifa;

    if (setsockopt(igmp_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP,
		   (char *)&mreq, sizeof(mreq)) < 0)
	log(LOG_WARNING, errno, "can't leave group %s on interface %s",
				inet_fmt(grp, s1), inet_fmt(ifa, s2));
}


void k_init_dvmrp()
{
    if (setsockopt(igmp_socket, IPPROTO_IP, DVMRP_INIT,
		   (char *)NULL, 0) < 0)
	log(LOG_ERR, errno, "can't enable DVMRP routing in kernel");
}


void k_stop_dvmrp()
{
    if (setsockopt(igmp_socket, IPPROTO_IP, DVMRP_DONE,
		   (char *)NULL, 0) < 0)
	log(LOG_WARNING, errno, "can't disable DVMRP routing in kernel");
}


void k_add_vif(vifi, v)
    vifi_t vifi;
    struct uvif *v;
{
    struct vifctl vc;

    vc.vifc_vifi            = vifi;
    vc.vifc_flags           = v->uv_flags & VIFF_KERNEL_FLAGS;
    vc.vifc_threshold       = v->uv_threshold;
    vc.vifc_rate_limit	    = v->uv_rate_limit;
    vc.vifc_lcl_addr.s_addr = v->uv_lcl_addr;
    vc.vifc_rmt_addr.s_addr = v->uv_rmt_addr;

    if (setsockopt(igmp_socket, IPPROTO_IP, DVMRP_ADD_VIF,
		   (char *)&vc, sizeof(vc)) < 0)
	log(LOG_ERR, errno, "setsockopt DVMRP_ADD_VIF");
}


void k_del_vif(vifi)
    vifi_t vifi;
{
    if (setsockopt(igmp_socket, IPPROTO_IP, DVMRP_DEL_VIF,
		   (char *)&vifi, sizeof(vifi)) < 0)
	log(LOG_ERR, errno, "setsockopt DVMRP_DEL_VIF");
}


/*
 * Adds a (source, mcastgrp) entry to the kernel
 */
void k_add_rg(kt)
    struct ktable *kt;
{
    struct mfcctl mc;

    /* copy table values so that setsockopt can process it */
    COPY_TABLES(kt, mc);

    /* write to kernel space */
    if (setsockopt(igmp_socket, IPPROTO_IP, DVMRP_ADD_MFC,
		   (char *)&mc, sizeof(mc)) < 0)
	log(LOG_WARNING, errno, "setsockopt DVMRP_ADD_MFC");
}


/*
 * Deletes a (source, mcastgrp) entry from the kernel
 */
void k_del_rg(kt)
    struct ktable *kt;
{
    struct mfcctl mc;

    /* copy table values so that setsockopt can process it */
    COPY_TABLES(kt, mc);

    /* write to kernel space */
    if (setsockopt(igmp_socket, IPPROTO_IP, DVMRP_DEL_MFC,
		   (char *)&mc, sizeof(mc)) < 0)
	log(LOG_WARNING, errno, "setsockopt DVMRP_DEL_MFC");
}
