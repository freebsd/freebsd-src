/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: config.c,v 1.7 1996/01/06 21:09:39 peter Exp $
 */


#include "defs.h"


/*
 * Query the kernel to find network interfaces that are multicast-capable
 * and install them in the uvifs array.
 */
void
config_vifs_from_kernel()
{
    struct ifreq ifbuf[32];
    struct ifreq *ifrp, *ifend;
    struct ifconf ifc;
    register struct uvif *v;
    register vifi_t vifi;
    int n;
    u_int32 addr, mask, subnet;
    short flags;

    ifc.ifc_buf = (char *)ifbuf;
    ifc.ifc_len = sizeof(ifbuf);
    if (ioctl(udp_socket, SIOCGIFCONF, (char *)&ifc) < 0)
	log(LOG_ERR, errno, "ioctl SIOCGIFCONF");

    ifrp = (struct ifreq *)ifbuf;
    ifend = (struct ifreq *)((char *)ifbuf + ifc.ifc_len);
    /*
     * Loop through all of the interfaces.
     */
    for (; ifrp < ifend; ifrp = (struct ifreq *)((char *)ifrp + n)) {
	struct ifreq ifr;
#if BSD >= 199006
	n = ifrp->ifr_addr.sa_len + sizeof(ifrp->ifr_name);
	if (n < sizeof(*ifrp))
	    n = sizeof(*ifrp);
#else
	n = sizeof(*ifrp);
#endif
	/*
	 * Ignore any interface for an address family other than IP.
	 */
	if (ifrp->ifr_addr.sa_family != AF_INET)
	    continue;

	addr = ((struct sockaddr_in *)&ifrp->ifr_addr)->sin_addr.s_addr;

	/*
	 * Need a template to preserve address info that is
	 * used below to locate the next entry.  (Otherwise,
	 * SIOCGIFFLAGS stomps over it because the requests
	 * are returned in a union.)
	 */
	bcopy(ifrp->ifr_name, ifr.ifr_name, sizeof(ifr.ifr_name));

	/*
	 * Ignore loopback interfaces and interfaces that do not support
	 * multicast.
	 */
	if (ioctl(udp_socket, SIOCGIFFLAGS, (char *)&ifr) < 0)
	    log(LOG_ERR, errno, "ioctl SIOCGIFFLAGS for %s", ifr.ifr_name);
	flags = ifr.ifr_flags;
	if ((flags & (IFF_LOOPBACK|IFF_MULTICAST)) != IFF_MULTICAST) continue;

	/*
	 * Ignore any interface whose address and mask do not define a
	 * valid subnet number, or whose address is of the form {subnet,0}
	 * or {subnet,-1}.
	 */
	if (ioctl(udp_socket, SIOCGIFNETMASK, (char *)&ifr) < 0)
	    log(LOG_ERR, errno, "ioctl SIOCGIFNETMASK for %s", ifr.ifr_name);
	mask = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
	subnet = addr & mask;
	if (!inet_valid_subnet(subnet, mask) ||
	    addr == subnet ||
	    addr == (subnet | ~mask)) {
	    log(LOG_WARNING, 0,
		"ignoring %s, has invalid address (%s) and/or mask (%s)",
		ifr.ifr_name, inet_fmt(addr, s1), inet_fmt(mask, s2));
	    continue;
	}

	/*
	 * Ignore any interface that is connected to the same subnet as
	 * one already installed in the uvifs array.
	 */
	for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	    if ((addr & v->uv_subnetmask) == v->uv_subnet ||
		(v->uv_subnet & mask) == subnet) {
		log(LOG_WARNING, 0, "ignoring %s, same subnet as %s",
					ifr.ifr_name, v->uv_name);
		break;
	    }
	}
	if (vifi != numvifs) continue;

	/*
	 * If there is room in the uvifs array, install this interface.
	 */
	if (numvifs == MAXVIFS) {
	    log(LOG_WARNING, 0, "too many vifs, ignoring %s", ifr.ifr_name);
	    continue;
	}
	v  = &uvifs[numvifs];
	v->uv_flags       = 0;
	v->uv_metric      = DEFAULT_METRIC;
	v->uv_admetric	  = 0;
	v->uv_rate_limit  = DEFAULT_PHY_RATE_LIMIT;
	v->uv_threshold   = DEFAULT_THRESHOLD;
	v->uv_lcl_addr    = addr;
	v->uv_rmt_addr    = 0;
	v->uv_subnet      = subnet;
	v->uv_subnetmask  = mask;
	v->uv_subnetbcast = subnet | ~mask;
	strncpy(v->uv_name, ifr.ifr_name, IFNAMSIZ);
	v->uv_groups      = NULL;
	v->uv_neighbors   = NULL;
	v->uv_acl         = NULL;
	v->uv_addrs	  = NULL;

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
}
