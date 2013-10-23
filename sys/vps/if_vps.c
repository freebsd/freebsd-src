/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
 *
 * Based on:
 *
 * ''if_epair.c'':
 *
 * Copyright (c) 2008 The FreeBSD Foundation
 * Copyright (c) 2009 Bjoern A. Zeeb <bz@FreeBSD.org>
 * All rights reserved.
 *
 * This software was developed by CK Software GmbH under sponsorship
 * from the FreeBSD Foundation.

 * ''if_loop.c'':
 *
 * Copyright (c) 1982, 1986, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

static const char vpsid[] =
    "$Id: if_vps.c 169 2013-06-11 14:10:34Z klaus $";

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_global.h"

#ifdef VPS

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/refcount.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet6/in6_var.h>

#include "vps_user.h"
#include "vps.h"
#include "vps2.h"

#define IFNAME "vps"

static MALLOC_DEFINE(M_VPS_IF, IFNAME,
    "Virtual Private Systems virtual network interfaces");

#ifdef DIAGNOSTIC

#define DBGIF	if (debug_if) printf

static int debug_if = 0;
SYSCTL_INT(_debug, OID_AUTO, vps_if_debug, CTLFLAG_RW, &debug_if, 0, "");

#else

#define DBGIF(x, ...)

#endif /* DIAGNOSTIC */

static struct mtx vps_if_mtx;

static int vps_if_refcnt = 0;

struct vps_if_softc {
        TAILQ_ENTRY(vps_if_softc)  vps_if_list;
        struct ifnet    *ifp;
        struct ifnet    *oifp;
        u_int           refcount;
        void            (*if_qflush)(struct ifnet *);
};

static TAILQ_HEAD(,vps_if_softc) vps_if_head =
    TAILQ_HEAD_INITIALIZER(vps_if_head);

static int vps_if_clone_match(struct if_clone *, const char *);
static int vps_if_clone_create(struct if_clone *, char *, size_t, caddr_t);
static int vps_if_clone_destroy(struct if_clone *, struct ifnet *);

static struct if_clone *vps_if_cloner;
#ifdef VIMAGE
static VNET_DEFINE(struct if_clone *, vps_if_cloner);
#define V_vps_if_cloner   VNET(vps_if_cloner)
#endif

struct vps_if_rtentry {
	struct radix_node rt_nodes[2];	/* tree glue, and other values */
	struct radix_node_head *rt_rnh;
	struct ifnet *rt_ifp;
	struct ifaddr *rt_ifa;
	union {
		struct sockaddr u_addr;
		struct sockaddr_in u_in4addr;
		struct sockaddr_in6 u_in6addr;
	} rt_uaddr;
	union {
		struct sockaddr u_mask;
		struct sockaddr_in u_in4mask;
		struct sockaddr_in6 u_in6mask;
	} rt_umask;
	struct vps *rt_vps;
};

void *vps_if_routehead_ip4;
void *vps_if_routehead_ip6;

static int
vps_if_inithead(void)
{

	rn_inithead(&vps_if_routehead_ip4, 0);
	rn_inithead(&vps_if_routehead_ip6, 0);

	return (0);
}

static int
vps_if_detachhead(void)
{

	rn_detachhead(&vps_if_routehead_ip6);
	rn_detachhead(&vps_if_routehead_ip4);

	return (0);
}

static int
vps_if_addroute(struct sockaddr *addr, struct sockaddr *mask,
    struct ifnet *ifp)
{
	struct radix_node_head *rnh;
	struct radix_node *rn;
	struct vps *vps;
	struct vps_if_rtentry *vrt;
	struct in_addr netmask4;
	struct sockaddr_in *addr4, *mask4;
	struct sockaddr_in6 *addr6, *mask6;

	vps = curthread->td_vps;

	vrt = malloc(sizeof(*vrt), M_VPS_IF, M_WAITOK | M_ZERO);
	vrt->rt_ifp = ifp;
	vrt->rt_ifa = NULL;
	vrt->rt_vps = vps;

	switch (addr->sa_family) {
	case AF_INET:
		rnh = vps_if_routehead_ip4;
		addr4 = (struct sockaddr_in *)addr;
		mask4 = (struct sockaddr_in *)mask;
		vrt->rt_rnh = rnh;
		vrt->rt_uaddr.u_in4addr = *(struct sockaddr_in *)addr;
		vrt->rt_umask.u_in4mask = *(struct sockaddr_in *)mask;

		DBGIF("%s: addr=%08x mask=%08x ifp=%p [%s]\n",
			__func__, addr4->sin_addr.s_addr,
			mask4->sin_addr.s_addr, ifp, ifp->if_xname);

		netmask4.s_addr = 0xffffffff;
		if (vps_ip4_check(vps, &addr4->sin_addr, &netmask4) != 0) {
			DBGIF("%s: vps_ip4_check EPERM\n", __func__);
			return (EPERM);
		}
		break;

	case AF_INET6:
		rnh = vps_if_routehead_ip6;
		addr6 = (struct sockaddr_in6 *)addr;
		mask6 = (struct sockaddr_in6 *)mask;
		vrt->rt_rnh = rnh;
		vrt->rt_uaddr.u_in6addr = *(struct sockaddr_in6 *)addr;
		vrt->rt_umask.u_in6mask = *(struct sockaddr_in6 *)mask;

		/*
		DBGIF("%s: addr=%16D mask=%16D ifp=%p [%s]\n",
			__func__, &addr6->sin6_addr, ":",
			&mask6->sin6_addr, ":",
			ifp, ifp->if_xname);
		*/

		if (vps_ip6_check(vps, &addr6->sin6_addr, 128) != 0) {
			DBGIF("%s: vps_ip6_check EPERM\n", __func__);
			return (EPERM);
		}
		break;

	default:
		free(vrt, M_VPS_IF);
		return (EOPNOTSUPP);
		break;
	}

	RADIX_NODE_HEAD_LOCK(rnh);

	rn = rnh->rnh_deladdr(&vrt->rt_uaddr.u_addr,
		&vrt->rt_umask.u_mask,
		rnh);
	if (rn != NULL) {
		DBGIF("%s: warning: old matching route found --> deleted\n",
			__func__);
		free(rn, M_VPS_IF);
	}

	rn = rnh->rnh_addaddr(&vrt->rt_uaddr.u_addr,
		&vrt->rt_umask.u_mask,
		rnh, vrt->rt_nodes);

	RADIX_NODE_HEAD_UNLOCK(rnh);

	return (0);
}

static int
vps_if_delroute(struct sockaddr *addr, struct sockaddr *mask,
    struct ifnet *ifp)
{
	struct radix_node_head *rnh;
	struct radix_node *rn;
	struct vps_if_rtentry *vrt;

	switch (addr->sa_family) {
	case AF_INET:
		rnh = vps_if_routehead_ip4;
		break;
	case AF_INET6:
		rnh = vps_if_routehead_ip6;
		break;
	default:
		return (0);
	}

	RADIX_NODE_HEAD_LOCK(rnh);

	rn = rnh->rnh_deladdr(addr, mask, rnh);

	RADIX_NODE_HEAD_UNLOCK(rnh);

	if (rn == NULL) {
		DBGIF("%s: rn not found\n", __func__);
		return (ESRCH);
	}

	if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
		panic("%s: rtrequest delete", __func__);

	vrt = (struct vps_if_rtentry *)rn;

	free(vrt, M_VPS_IF);

	return (0);
}

static int
vps_if_purgeroute_one(struct radix_node *rn, void *arg)
{
	struct vps_if_rtentry *vrt = (struct vps_if_rtentry *)rn;
	struct ifnet *ifp = arg;

	if (vrt->rt_ifp != ifp)
		return (0);

	vrt->rt_rnh->rnh_deladdr(&vrt->rt_uaddr.u_addr,
		&vrt->rt_umask.u_mask, vrt->rt_rnh);

	free(vrt, M_VPS_IF);

	DBGIF("%s: freed one route\n", __func__);

	return (0);
}

static int
vps_if_purgeroutes(struct ifnet *ifp)
{
	struct radix_node_head *rnh;

	DBGIF("%s: ifp=%p\n", __func__, ifp);

	rnh = vps_if_routehead_ip4;
	RADIX_NODE_HEAD_LOCK(rnh);
	(void)(rnh)->rnh_walktree(rnh,
		vps_if_purgeroute_one, ifp);
	RADIX_NODE_HEAD_UNLOCK(rnh);

	rnh = vps_if_routehead_ip6;
	RADIX_NODE_HEAD_LOCK(rnh);
	(void)(rnh)->rnh_walktree(rnh,
		vps_if_purgeroute_one, ifp);
	RADIX_NODE_HEAD_UNLOCK(rnh);

	return (0);
}

static struct vps_if_rtentry *
vps_if_lookup(const struct sockaddr *dst2)
{
	struct radix_node_head *rnh;
	struct radix_node *rn;
	struct vps_if_rtentry *vrt;
	struct sockaddr *dst;
	char sockaddr_buf[SOCK_MAXADDRLEN];

	dst = (struct sockaddr *)sockaddr_buf;
	memcpy(dst, dst2, dst2->sa_len);

	switch (dst->sa_family) {
	case AF_INET:
		rnh = vps_if_routehead_ip4;
		DBGIF("%s: dst: %08x\n",
			__func__,
			((struct sockaddr_in *)dst)->sin_addr.s_addr);
		break;
	case AF_INET6:
		rnh = vps_if_routehead_ip6;
		/*
		DBGIF("%s: dst: %16D\n",
			__func__,
			&((struct sockaddr_in6 *)dst)->sin6_addr, ":");
		*/
		break;
	default:
		return (NULL);
	}

	rn = rnh->rnh_matchaddr(dst, rnh);
	if (rn && ((rn->rn_flags & RNF_ROOT) == 0)) {
		;//DBGIF("%s: rn=%p\n", __func__, rn);
	} else {
		;//DBGIF("%s: RNF_ROOT or rn=%p\n", __func__, rn);
	}

	vrt = (struct vps_if_rtentry *)rn;

	/*
	 * If interface and therefore routes still exist somewhere in
	 * a half dead vps instance, better not use the route.
	 */
	if (vrt != NULL && ((vrt->rt_ifp->if_flags & IFF_UP) == 0))
		return (NULL);

	return (vrt);
}

static struct ifnet *
vps_if_get_if_by_addr_v4(const struct sockaddr *dst, struct mbuf *m)
{
	struct vps_if_rtentry *vrt;
	struct ifnet *ifp;

	vrt = vps_if_lookup(dst);
	if (vrt == NULL) {
                /* Nothing found, so use the default (first) interface. */
        	mtx_lock(&vps_if_mtx);
		ifp = TAILQ_FIRST(&vps_if_head)->ifp;
        	mtx_unlock(&vps_if_mtx);
		return (ifp);
	}

	return (vrt->rt_ifp);
}

static struct ifnet *
vps_if_get_if_by_addr_v6(const struct sockaddr *dst, struct mbuf *m)
{
	struct vps_if_rtentry *vrt;

	vrt = vps_if_lookup(dst);
	if (vrt == NULL) {
                /* Nothing found, so use the default (first) interface. */
                return ((TAILQ_FIRST(&vps_if_head))->ifp);
	}

	return (vrt->rt_ifp);
}

static int
vps_if_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
    struct route *rt)
{
        struct ifnet *oifp;
        int error;
        struct vps_if_softc *sc;
        int isr;

        switch (dst->sa_family) {
        case AF_INET:
                oifp = vps_if_get_if_by_addr_v4(dst, m);
                break;
        case AF_INET6:
                oifp = vps_if_get_if_by_addr_v6(dst, m);
                break;
        default:
                DBGIF("%s: af=%d unexpected\n", __func__, dst->sa_family);
                m_freem(m);
                return (EAFNOSUPPORT);
        }

        if (oifp == NULL) {
                ifp->if_oerrors++;
                m_freem(m);
                return (EHOSTUNREACH);
        }
        sc = ifp->if_softc;

        /*
         * In case the outgoing interface is not usable,
         * drop the packet.
         */
        if ((oifp->if_drv_flags & IFF_DRV_RUNNING) == 0 ||
                (oifp->if_flags & IFF_UP) == 0) {
                ifp->if_oerrors++;
                m_freem(m);
                return (EHOSTUNREACH);
        }

	/* Loop protection. */
	if (oifp == ifp) {
		DBGIF("%s: LOOP ! oifp == ifp == %p\n", __func__, ifp);
                ifp->if_oerrors++;
                m_freem(m);
		return (EHOSTUNREACH);
	}

        DBGIF("%s: packet if %p/%s (vnet %p) -> if %p/%s (vnet %p)\n",
	    __func__, ifp, ifp->if_xname, ifp->if_vnet,
	    oifp, oifp->if_xname, oifp->if_vnet);

        /* Deliver to upper layer protocol */
        switch (dst->sa_family) {
#ifdef INET
        case AF_INET:
                isr = NETISR_IP;
                break;
#endif
#ifdef INET6
        case AF_INET6:
                m->m_flags |= M_LOOP;
                isr = NETISR_IPV6;
                break;
#endif
        default:
                DBGIF("%s: can't handle af=%d\n", __func__, dst->sa_family);
                m_freem(m);
                return (EAFNOSUPPORT);
        }

        refcount_acquire(&sc->refcount);
        m->m_pkthdr.rcvif = oifp;
	KASSERT(m->m_pkthdr.rcvif->if_dname[0] == 'v',
	    ("%s: m->m_pkthdr.rcvif->if_dname = [%s]\n",
	    __func__, m->m_pkthdr.rcvif->if_dname));
        oifp->if_ipackets++;
        oifp->if_ibytes += m->m_pkthdr.len;

        CURVNET_SET_QUIET(oifp->if_vnet);
        error = netisr_queue(isr, m);   /* mbuf is free'd on failure. */
        CURVNET_RESTORE();

        refcount_release(&sc->refcount);

        return (error);
}

static void
vps_if_ioctl2(u_long cmd, caddr_t data, struct ifnet *ifp,
    struct thread *td)
{
	struct in_aliasreq *ifra;
	struct in6_aliasreq *ifra6;

	DBGIF("%s: cmd=%08lx data=%p ifp=%p td=%p\n",
	    __func__, cmd, data, ifp, td);

        switch (cmd) {
	case SIOCSIFADDR:
	case SIOCAIFADDR:
		ifra = (struct in_aliasreq *)data;

		DBGIF("%s: ADD SIOCAIFADDR/SIOCSIFADDR ifra=%p "
		    "addr=%08x mask=%08x\n",
		    __func__, ifra, ifra->ifra_addr.sin_addr.s_addr,
		    ifra->ifra_mask.sin_addr.s_addr);

		vps_if_addroute((struct sockaddr *)&ifra->ifra_addr,
			(struct sockaddr *)&ifra->ifra_mask, ifp);

		break;

	case SIOCDIFADDR:
		ifra = (struct in_aliasreq *)data;

		DBGIF("%s: DEL SIOCDIFADDR ifra=%p addr=%08x mask=%08x\n",
			__func__, ifra, ifra->ifra_addr.sin_addr.s_addr,
			ifra->ifra_mask.sin_addr.s_addr);

		vps_if_delroute((struct sockaddr *)&ifra->ifra_addr,
			(struct sockaddr *)&ifra->ifra_mask, ifp);

		break;

	case SIOCAIFADDR_IN6:
		ifra6 = (struct in6_aliasreq *)data;

		DBGIF("%s: ADD SIOCAIFADDR_IN6: addr=%16D mask=%16D\n",
			__func__, (char*)&ifra6->ifra_addr.sin6_addr, ":",
			(char*)&ifra6->ifra_prefixmask.sin6_addr, ":");

		vps_if_addroute((struct sockaddr *)&ifra6->ifra_addr,
			(struct sockaddr *)&ifra6->ifra_prefixmask, ifp);

		break;

	case SIOCDIFADDR_IN6:
		ifra6 = (struct in6_aliasreq *)data;

		DBGIF("%s: DEL SIOCDIFADDR_IN6: addr=%16D mask=%16D\n",
			__func__, (char*)&ifra6->ifra_addr.sin6_addr, ":",
			(char*)&ifra6->ifra_prefixmask.sin6_addr, ":");

		vps_if_delroute((struct sockaddr *)&ifra6->ifra_addr,
			(struct sockaddr *)&ifra6->ifra_prefixmask, ifp);

		break;

        default:
		/*
                DBGIF("%s: cmd=%08lx\n", __func__, cmd);
		*/
		break;
        }

}


static int
vps_if_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
        struct ifreq *ifr;
        struct ifaddr *ifa;
        int error;

        ifr = (struct ifreq *)data;
        error = 0;

	DBGIF("%s: cmd=%08lx data=%p ifp=%p\n",
	    __func__, cmd, data, ifp);

        switch (cmd) {
        case SIOCSIFADDR:
                ifp->if_flags |= IFF_UP;
                ifp->if_drv_flags |= IFF_DRV_RUNNING;
                ifa = (struct ifaddr *)data;
                /*
                 * Everything else is done at a higher level.
                 */
                break;

	case SIOCAIFADDR:
	case SIOCDIFADDR:
	case SIOCAIFADDR_IN6:
	case SIOCDIFADDR_IN6:
		break;

        case SIOCADDMULTI:
        case SIOCDELMULTI:
                if (ifr == 0) {
                        error = EAFNOSUPPORT;           /* XXX */
                        break;
                }
                switch (ifr->ifr_addr.sa_family) {

#ifdef INET
                case AF_INET:
                        break;
#endif
#ifdef INET6
                case AF_INET6:
                        break;
#endif

                default:
                        error = EAFNOSUPPORT;
                        break;
                }
                break;

        case SIOCSIFMTU:
                ifp->if_mtu = ifr->ifr_mtu;
                break;

        case SIOCSIFFLAGS:
                break;

        default:
                error = EINVAL;
        }

        return (error);
}

static void
vps_if_init(void *dummy __unused)
{
}


static int
vps_if_clone_match(struct if_clone *ifc, const char *name)
{
        const char *cp;

        DBGIF("name='%s'\n", name);

        /*
         * Our base name is vps.
         * Our interfaces will be named vps<n>.
         * So accept anything of the following list:
         * - vps
         * - vps<n>
         */
        if (strncmp(IFNAME, name, sizeof(IFNAME)-1) != 0)
                return (0);

        for (cp = name + sizeof(IFNAME) - 1; *cp != '\0'; cp++) {
                if (*cp < '0' || *cp > '9')
                        return (0);
        }

        return (1);
}

static int
vps_if_clone_create(struct if_clone *ifc, char *name, size_t len,
    caddr_t params)
{
        struct vps_if_softc *sc;
        struct ifnet *ifp;
        struct ifaddr *ifa;
        struct sockaddr_dl *sdl;
        int error, unit, wildcard;
	u_int8_t ll[8];
        char *dp;

        error = ifc_name2unit(name, &unit);
        if (error != 0)
                return (error);
        wildcard = (unit < 0);

        error = ifc_alloc_unit(ifc, &unit);
        if (error != 0)
                return (error);
        /*
         * If no unit had been given, we need to adjust the ifName.
         */
        for (dp = name; *dp != '\0'; dp++);
        if (wildcard) {
                error = snprintf(dp, len - (dp - name), "%d", unit);
                if (error > len - (dp - name) - 1) {
                        /* ifName too long. */
                        ifc_free_unit(ifc, unit);
                        return (ENOSPC);
                }
                dp += error;
        }
        *dp = '\0';

        /* Allocate memory for interface */
        sc = malloc(sizeof (struct vps_if_softc), M_VPS_IF,
	    M_WAITOK | M_ZERO);
        refcount_init(&sc->refcount, 1);
        sc->ifp = if_alloc(IFT_PROPVIRTUAL);
        if (sc->ifp == NULL) {
                free(sc, M_VPS_IF);
                ifc_free_unit(ifc, unit);
                return (ENOSPC);
        }

        /* Finish initialization of interface <n>. */
        ifp = sc->ifp;
        ifp->if_softc = sc;
        ifp->if_dname = IFNAME;
        ifp->if_dunit = unit;
	snprintf(ifp->if_xname, IFNAMSIZ, "%s%d", ifp->if_dname,
	    ifp->if_dunit);
	DBGIF("%s: ifp->if_xname=[%s]\n", __func__, ifp->if_xname);
        ifp->if_flags = IFF_MULTICAST;
        ifp->if_ioctl = vps_if_ioctl;
        ifp->if_init  = vps_if_init;
        ifp->if_output = vps_if_output;
        ifp->if_snd.ifq_maxlen = ifqmaxlen;
        ifp->if_mtu = IP_MAXPACKET;
        if_attach(ifp);
        bpfattach(ifp, DLT_NULL, sizeof(u_int32_t));
        ifp->if_baudrate = ULONG_MAX; // IF_Gbps(10UL);       /* arbitrary maximum */

	ifp->if_pspare[2] = (void *)vps_if_ioctl2;

	/* XXX do something random ... */
	{
		int tmpticks = ticks;

		memcpy(&ll[0], &curthread->td_vps, 4);
		memcpy(&ll[4], &tmpticks, 4);
	}
        ifa = ifp->if_addr;
        KASSERT(ifa != NULL, ("%s: no lladdr!\n", __func__));
        sdl = (struct sockaddr_dl *)ifa->ifa_addr;
        sdl->sdl_type = IFT_PROPVIRTUAL;
        sdl->sdl_alen = sizeof(ll);
        bcopy(&ll, LLADDR(sdl), sdl->sdl_alen);

        mtx_lock(&vps_if_mtx);
        TAILQ_INSERT_TAIL(&vps_if_head, sc, vps_if_list);
        mtx_unlock(&vps_if_mtx);

        /* Tell the world, that we are ready to rock. */
        sc->ifp->if_drv_flags |= IFF_DRV_RUNNING;

	refcount_acquire(&vps_if_refcnt);

	DBGIF("%s: ifp->if_xname=[%s]\n", __func__, ifp->if_xname);

        return (0);
}

static int
vps_if_clone_destroy(struct if_clone *ifc, struct ifnet *ifp)
{
        struct vps_if_softc *sc;
        int unit;

        DBGIF("ifp=%p\n", ifp);

        unit = ifp->if_dunit;
        sc = ifp->if_softc;

        mtx_lock(&vps_if_mtx);
        TAILQ_REMOVE(&vps_if_head, sc, vps_if_list);
        mtx_unlock(&vps_if_mtx);

        DBGIF("ifp=%p\n", ifp);
        ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
        if_detach(ifp);
        /*
         * Wait for all packets to be dispatched to if_input.
         * The numbers can only go down as the interfaces are
         * detached so there is no need to use atomics.
         */
        DBGIF("sc refcnt=%u\n", sc->refcount);
        KASSERT(sc->refcount == 1,
            ("%s: sc->refcount!=1: %d",
            __func__, sc->refcount));

	/* Have all internal routes purged. */
	vps_if_purgeroutes(ifp);

        /* Finish cleaning up. Free them and release the unit. */
        if_free(ifp);
        free(sc, M_VPS_IF);
        ifc_free_unit(ifc, unit);

	refcount_release(&vps_if_refcnt);

        return (0);
}

static int
vps_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	CURVNET_SET(vnet0);

        switch (type) {
        case MOD_LOAD:
                /* For now limit us to one global mutex and one inq. */
		mtx_init(&vps_if_mtx, "if_vps", NULL, MTX_DEF);
		vps_if_inithead();
#ifndef VIMAGE
		vps_if_cloner = if_clone_advanced(IFNAME, 0,
		    vps_if_clone_match, vps_if_clone_create,
		     vps_if_clone_destroy);
#endif
		refcount_init(&vps_if_refcnt, 0);
                if (bootverbose)
                        printf("%s initialized.\n", IFNAME);
                break;
        case MOD_UNLOAD:
		if (vps_if_refcnt > 0)
			return (EBUSY);
#ifndef VIMAGE
                if_clone_detach(vps_if_cloner);
#endif
		vps_if_detachhead();
                mtx_destroy(&vps_if_mtx);
                if (bootverbose)
                        printf("%s unloaded.\n", IFNAME);
                break;
        default:
                error = EOPNOTSUPP;
		break;
        }

	CURVNET_RESTORE();

        return (error);
}

static moduledata_t vps_mod = {
        "if_vps",
        vps_modevent,
        0
};

DECLARE_MODULE(if_vps, vps_mod, SI_SUB_VNET_DONE, SI_ORDER_ANY);
MODULE_VERSION(if_vps, 1);

#ifdef VIMAGE
static void
vnet_vps_if_init(const void *unused __unused)
{

        vps_if_cloner = if_clone_advanced(IFNAME, 0, vps_if_clone_match,
                    vps_if_clone_create, vps_if_clone_destroy);
        V_vps_if_cloner = vps_if_cloner;
}
VNET_SYSINIT(vnet_vps_if_init, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_vps_if_init, NULL);

static void
vnet_vps_if_uninit(const void *unused __unused)
{

        if_clone_detach(V_vps_if_cloner);
}
VNET_SYSUNINIT(vnet_vps_if_uninit, SI_SUB_PROTO_IFATTACHDOMAIN,
    SI_ORDER_FIRST, vnet_vps_if_uninit, NULL);
#endif

#endif /* VPS */

/* EOF */
