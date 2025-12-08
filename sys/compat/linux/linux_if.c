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
#include <sys/ctype.h>
#include <sys/jail.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <compat/linux/linux.h>
#include <compat/linux/linux_common.h>
#include <compat/linux/linux_mib.h>

_Static_assert(LINUX_IFNAMSIZ == IFNAMSIZ, "Linux IFNAMSIZ");

static bool use_real_ifnames = false;
SYSCTL_BOOL(_compat_linux, OID_AUTO, use_real_ifnames, CTLFLAG_RWTUN,
    &use_real_ifnames, 0,
    "Use FreeBSD interface names instead of generating ethN aliases");

/*
 * Criteria for interface name translation
 */
#define	IFP_IS_ETH(ifp)		(if_gettype(ifp) == IFT_ETHER)
#define	IFP_IS_LOOP(ifp)	(if_gettype(ifp) == IFT_LOOP)

/*
 * Translate a FreeBSD interface name to a Linux interface name
 * by interface name, and return the number of bytes copied to lxname.
 */
int
ifname_bsd_to_linux_name(const char *bsdname, char *lxname, size_t len)
{
	struct epoch_tracker et;
	struct ifnet *ifp;
	int ret;

	CURVNET_ASSERT_SET();

	ret = 0;
	NET_EPOCH_ENTER(et);
	ifp = ifunit(bsdname);
	if (ifp != NULL)
		ret = ifname_bsd_to_linux_ifp(ifp, lxname, len);
	NET_EPOCH_EXIT(et);
	return (ret);
}

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
struct ifname_bsd_to_linux_ifp_cb_s {
	struct ifnet	*ifp;
	int		ethno;
	char		*lxname;
	size_t		len;
};

static int
ifname_bsd_to_linux_ifp_cb(if_t ifp, void *arg)
{
	struct ifname_bsd_to_linux_ifp_cb_s *cbs = arg;

	if (ifp == cbs->ifp)
		return (snprintf(cbs->lxname, cbs->len, "eth%d", cbs->ethno));
	if (IFP_IS_ETH(ifp))
		cbs->ethno++;
	return (0);
}

int
ifname_bsd_to_linux_ifp(struct ifnet *ifp, char *lxname, size_t len)
{
	struct ifname_bsd_to_linux_ifp_cb_s arg = {
		.ifp = ifp,
		.ethno = 0,
		.lxname = lxname,
		.len = len,
	};

	NET_EPOCH_ASSERT();

	/*
	 * Linux loopback interface name is lo (not lo0),
	 * we translate lo to lo0, loX to loX.
	 */
	if (IFP_IS_LOOP(ifp) && strncmp(if_name(ifp), "lo0", IFNAMSIZ) == 0)
		return (strlcpy(lxname, "lo", len));

	/* Short-circuit non ethernet interfaces. */
	if (!IFP_IS_ETH(ifp) || use_real_ifnames)
		return (strlcpy(lxname, if_name(ifp), len));

	/* Determine the (relative) unit number for ethernet interfaces. */
	return (if_foreach(ifname_bsd_to_linux_ifp_cb, &arg));
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
	int		ethno;
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
	if (cbs->is_eth && IFP_IS_ETH(ifp) && cbs->unit == cbs->ethno)
		goto out;
	if (cbs->is_lo && IFP_IS_LOOP(ifp))
		goto out;
	if (IFP_IS_ETH(ifp))
		cbs->ethno++;
	return (0);

out:
	cbs->ifp = ifp;
	return (1);
}

struct ifnet *
ifname_linux_to_ifp(struct thread *td, const char *lxname)
{
	struct ifname_linux_to_ifp_cb_s arg = {
		.ethno = 0,
		.lxname = lxname,
		.ifp = NULL,
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
	ifp = ifname_linux_to_ifp(td, lxname);
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

	if (IFP_IS_LOOP(ifp)) {
		bzero(lsa, sizeof(*lsa));
		lsa->sa_family = LINUX_ARPHRD_LOOPBACK;
		return (0);
	}
	if (!IFP_IS_ETH(ifp))
		return (ENOENT);
	if (if_foreach_addr_type(ifp, AF_LINK, linux_ifhwaddr_cb, lsa) > 0)
		return (0);
	return (ENOENT);
}
