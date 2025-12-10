/*-
 * Copyright (c) 2015 Dmitry Chagin <dchagin@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ctype.h>
#include <sys/eventhandler.h>
#include <sys/jail.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/vnet.h>

#include <compat/linux/linux.h>
#include <compat/linux/linux_common.h>
#include <compat/linux/linux_mib.h>

_Static_assert(LINUX_IFNAMSIZ == IFNAMSIZ, "Linux IFNAMSIZ");

static bool use_real_ifnames = false;
SYSCTL_BOOL(_compat_linux, OID_AUTO, use_real_ifnames, CTLFLAG_RWTUN,
    &use_real_ifnames, 0,
    "Use FreeBSD interface names instead of generating ethN aliases");

VNET_DEFINE_STATIC(struct unrhdr *, linux_eth_unr);
#define	V_linux_eth_unr	VNET(linux_eth_unr)

static eventhandler_tag ifnet_arrival_tag;
static eventhandler_tag ifnet_departure_tag;

static void
linux_ifnet_arrival(void *arg __unused, struct ifnet *ifp)
{
	if (ifp->if_type == IFT_ETHER)
		ifp->if_linux_ethno = alloc_unr(V_linux_eth_unr);
}

static void
linux_ifnet_departure(void *arg __unused, struct ifnet *ifp)
{
	if (ifp->if_type == IFT_ETHER)
		free_unr(V_linux_eth_unr, ifp->if_linux_ethno);
}

void
linux_ifnet_init(void)
{
	ifnet_arrival_tag = EVENTHANDLER_REGISTER(ifnet_arrival_event,
	    linux_ifnet_arrival, NULL, EVENTHANDLER_PRI_FIRST);
	ifnet_departure_tag = EVENTHANDLER_REGISTER(ifnet_departure_event,
	    linux_ifnet_departure, NULL, EVENTHANDLER_PRI_LAST);
}

void
linux_ifnet_uninit(void)
{
	EVENTHANDLER_DEREGISTER(ifnet_arrival_event, ifnet_arrival_tag);
	EVENTHANDLER_DEREGISTER(ifnet_departure_event, ifnet_departure_tag);
}

static void
linux_ifnet_vnet_init(void *arg __unused)
{
	struct epoch_tracker et;
	struct if_iter it;
	if_t ifp;

	V_linux_eth_unr = new_unrhdr(0, INT_MAX, NULL);
	NET_EPOCH_ENTER(et);
	for (ifp = if_iter_start(&it); ifp != NULL; ifp = if_iter_next(&it))
		linux_ifnet_arrival(NULL, ifp);
	NET_EPOCH_EXIT(et);
}
VNET_SYSINIT(linux_ifnet_vnet_init, SI_SUB_PROTO_IF, SI_ORDER_ANY,
    linux_ifnet_vnet_init, NULL);

static void
linux_ifnet_vnet_uninit(void *arg __unused)
{
	/*
	 * At a normal vnet shutdown all interfaces are gone at this point.
	 * But when we kldunload linux.ko, the vnet_deregister_sysuninit()
	 * would call this function for the default vnet.
	 */
	if (IS_DEFAULT_VNET(curvnet))
		clear_unrhdr(V_linux_eth_unr);
	delete_unrhdr(V_linux_eth_unr);
}
VNET_SYSUNINIT(linux_ifnet_vnet_uninit, SI_SUB_PROTO_IF, SI_ORDER_ANY,
    linux_ifnet_vnet_uninit, NULL);

/*
 * Translate a FreeBSD interface name to a Linux interface name
 * by interface index, and return the number of bytes copied to lxname.
 */
int
ifname_bsd_to_linux_idx(u_int idx, char *lxname, size_t len)
{
	struct epoch_tracker et;
	struct ifnet *ifp;
	int ret;

	ret = 0;
	CURVNET_SET(TD_TO_VNET(curthread));
	NET_EPOCH_ENTER(et);
	ifp = ifnet_byindex(idx);
	if (ifp != NULL)
		ret = ifname_bsd_to_linux_ifp(ifp, lxname, len);
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
	return (ret);
}

/*
 * Translate a FreeBSD interface name to a Linux interface name,
 * and return the number of bytes copied to lxname, 0 if interface
 * not found, -1 on error.
 */
int
ifname_bsd_to_linux_ifp(const struct ifnet *ifp, char *lxname, size_t len)
{
	/*
	 * Linux loopback interface name is lo (not lo0),
	 * we translate lo to lo0, loX to loX.
	 */
	if (ifp->if_type == IFT_LOOP &&
	    strncmp(ifp->if_xname, "lo0", IFNAMSIZ) == 0)
		return (strlcpy(lxname, "lo", len));

	/* Short-circuit non ethernet interfaces. */
	if (ifp->if_type != IFT_ETHER || use_real_ifnames)
		return (strlcpy(lxname, ifp->if_xname, len));

	/* Determine the (relative) unit number for ethernet interfaces. */
	return (snprintf(lxname, len, "eth%d", ifp->if_linux_ethno));
}

/*
 * Translate a Linux interface name to a FreeBSD interface name,
 * and return the associated ifnet structure
 * bsdname and lxname need to be least IFNAMSIZ bytes long, but
 * can point to the same buffer.
 */
struct ifname_linux_to_ifp_cb_s {
	bool		is_lo;
	bool		is_eth;
	int		unit;
	const char	*lxname;
	if_t		ifp;
};

static int
ifname_linux_to_ifp_cb(if_t ifp, void *arg)
{
	struct ifname_linux_to_ifp_cb_s *cbs = arg;

	NET_EPOCH_ASSERT();

	/*
	 * Allow Linux programs to use FreeBSD names. Don't presume
	 * we never have an interface named "eth", so don't make
	 * the test optional based on is_eth.
	 */
	if (strncmp(if_name(ifp), cbs->lxname, LINUX_IFNAMSIZ) == 0)
		goto out;
	if (cbs->is_eth && ifp->if_type == IFT_ETHER &&
	    ifp->if_linux_ethno == cbs->unit)
		goto out;
	if (cbs->is_lo && ifp->if_type == IFT_LOOP)
		goto out;
	return (0);

out:
	cbs->ifp = ifp;
	return (1);
}

struct ifnet *
ifname_linux_to_ifp(const char *lxname)
{
	struct ifname_linux_to_ifp_cb_s arg = {
		.lxname = lxname,
	};
	int len;
	char *ep;

	NET_EPOCH_ASSERT();

	for (len = 0; len < LINUX_IFNAMSIZ; ++len)
		if (!isalpha(lxname[len]) || lxname[len] == '\0')
			break;
	if (len == 0 || len == LINUX_IFNAMSIZ)
		return (NULL);
	/*
	 * Linux loopback interface name is lo (not lo0),
	 * we translate lo to lo0, loX to loX.
	 */
	arg.is_lo = (len == 2 && strncmp(lxname, "lo", LINUX_IFNAMSIZ) == 0);
	arg.unit = (int)strtoul(lxname + len, &ep, 10);
	if ((ep == NULL || ep == lxname + len || ep >= lxname + LINUX_IFNAMSIZ) &&
	    arg.is_lo == 0)
		return (NULL);
	arg.is_eth = (len == 3 && strncmp(lxname, "eth", len) == 0);

	if_foreach(ifname_linux_to_ifp_cb, &arg);
	return (arg.ifp);
}

int
ifname_linux_to_bsd(struct thread *td, const char *lxname, char *bsdname)
{
	struct epoch_tracker et;
	struct ifnet *ifp;

	CURVNET_SET(TD_TO_VNET(td));
	NET_EPOCH_ENTER(et);
	ifp = ifname_linux_to_ifp(lxname);
	if (ifp != NULL && bsdname != NULL)
		strlcpy(bsdname, if_name(ifp), IFNAMSIZ);
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
	return (ifp != NULL ? 0 : EINVAL);
}

unsigned short
linux_ifflags(struct ifnet *ifp)
{
	unsigned short flags;

	NET_EPOCH_ASSERT();

	flags = if_getflags(ifp) | if_getdrvflags(ifp);
	return (bsd_to_linux_ifflags(flags));
}

unsigned short
bsd_to_linux_ifflags(int fl)
{
	unsigned short flags = 0;

	if (fl & IFF_UP)
		flags |= LINUX_IFF_UP;
	if (fl & IFF_BROADCAST)
		flags |= LINUX_IFF_BROADCAST;
	if (fl & IFF_DEBUG)
		flags |= LINUX_IFF_DEBUG;
	if (fl & IFF_LOOPBACK)
		flags |= LINUX_IFF_LOOPBACK;
	if (fl & IFF_POINTOPOINT)
		flags |= LINUX_IFF_POINTOPOINT;
	if (fl & IFF_DRV_RUNNING)
		flags |= LINUX_IFF_RUNNING;
	if (fl & IFF_NOARP)
		flags |= LINUX_IFF_NOARP;
	if (fl & IFF_PROMISC)
		flags |= LINUX_IFF_PROMISC;
	if (fl & IFF_ALLMULTI)
		flags |= LINUX_IFF_ALLMULTI;
	if (fl & IFF_MULTICAST)
		flags |= LINUX_IFF_MULTICAST;
	return (flags);
}

static u_int
linux_ifhwaddr_cb(void *arg, struct ifaddr *ifa, u_int count)
{
	struct sockaddr_dl *sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	struct l_sockaddr *lsa = arg;

	if (count > 0)
		return (0);
	if (sdl->sdl_type != IFT_ETHER)
		return (0);
	bzero(lsa, sizeof(*lsa));
	lsa->sa_family = LINUX_ARPHRD_ETHER;
	bcopy(LLADDR(sdl), lsa->sa_data, LINUX_IFHWADDRLEN);
	return (1);
}

int
linux_ifhwaddr(struct ifnet *ifp, struct l_sockaddr *lsa)
{

	NET_EPOCH_ASSERT();

	if (ifp->if_type == IFT_LOOP) {
		bzero(lsa, sizeof(*lsa));
		lsa->sa_family = LINUX_ARPHRD_LOOPBACK;
		return (0);
	}
	if (ifp->if_type != IFT_ETHER)
		return (ENOENT);
	if (if_foreach_addr_type(ifp, AF_LINK, linux_ifhwaddr_cb, lsa) > 0)
		return (0);
	return (ENOENT);
}
