/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * config.c,v 3.8.4.10 1998/01/06 01:57:41 fenner Exp
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include "defs.h"
#include <ifaddrs.h>

/*
 * Query the kernel to find network interfaces that are multicast-capable
 * and install them in the uvifs array.
 */
void
config_vifs_from_kernel()
{
    struct ifaddrs *ifa, *ifap;
    register struct uvif *v;
    register vifi_t vifi;
    u_int32 addr, mask, subnet;
    int flags;

    if (getifaddrs(&ifap) < 0)
	log(LOG_ERR, errno, "getifaddrs");
    /*
     * Loop through all of the interfaces.
     */
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
	/*
	 * Ignore any interface for an address family other than IP.
	 */
	if (ifa->ifa_addr->sa_family != AF_INET)
	    continue;

	addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;

	/*
	 * Ignore loopback interfaces and interfaces that do not support
	 * multicast.
	 */
	flags = ifa->ifa_flags;
	if ((flags & (IFF_LOOPBACK|IFF_MULTICAST)) != IFF_MULTICAST)
	    continue;

	/*
	 * Ignore any interface whose address and mask do not define a
	 * valid subnet number, or whose address is of the form {subnet,0}
	 * or {subnet,-1}.
	 */
	mask = ((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr.s_addr;
	subnet = addr & mask;
	if (!inet_valid_subnet(subnet, mask) ||
	    addr == subnet ||
	    addr == (subnet | ~mask)) {
	    log(LOG_WARNING, 0,
		"ignoring %s, has invalid address (%s) and/or mask (%s)",
		ifa->ifa_name, inet_fmt(addr, s1), inet_fmt(mask, s2));
	    continue;
	}

	/*
	 * Ignore any interface that is connected to the same subnet as
	 * one already installed in the uvifs array.
	 */
	for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	    if (strcmp(v->uv_name, ifa->ifa_name) == 0) {
		log(LOG_DEBUG, 0, "skipping %s (%s on subnet %s) (alias for vif#%u?)",
			v->uv_name, inet_fmt(addr, s1),
			inet_fmts(subnet, mask, s2), vifi);
		break;
	    }
	    if ((addr & v->uv_subnetmask) == v->uv_subnet ||
		(v->uv_subnet & mask) == subnet) {
		log(LOG_WARNING, 0, "ignoring %s, same subnet as %s",
		    ifa->ifa_name, v->uv_name);
		break;
	    }
	}
	if (vifi != numvifs) continue;

	/*
	 * If there is room in the uvifs array, install this interface.
	 */
	if (numvifs == MAXVIFS) {
	    log(LOG_WARNING, 0, "too many vifs, ignoring %s", ifa->ifa_name);
	    continue;
	}
	v  = &uvifs[numvifs];
	zero_vif(v, 0);
	v->uv_lcl_addr    = addr;
	v->uv_subnet      = subnet;
	v->uv_subnetmask  = mask;
	v->uv_subnetbcast = subnet | ~mask;
	strlcpy(v->uv_name, ifa->ifa_name, sizeof(v->uv_name));

	if (flags & IFF_POINTOPOINT)
	    v->uv_flags |= VIFF_REXMIT_PRUNES;

	log(LOG_INFO,0,"installing %s (%s on subnet %s) as vif #%u - rate=%d",
	    v->uv_name, inet_fmt(addr, s1), inet_fmts(subnet, mask, s2),
	    numvifs, v->uv_rate_limit);

	++numvifs;

	/*
	 * If the interface is not yet up, set the vifs_down flag to
	 * remind us to check again later.
	 */
	if (!(flags & IFF_UP)) {
	    v->uv_flags |= VIFF_DOWN;
	    vifs_down = TRUE;
	}
    }

    freeifaddrs(ifap);
}
