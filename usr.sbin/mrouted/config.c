/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: config.c,v 1.2 1994/09/08 02:51:12 wollman Exp $
 */


#include "defs.h"


char *configfilename = "/etc/mrouted.conf";

extern int cache_lifetime;
extern int max_prune_lifetime;

/*
 * Forward declarations.
 */
static char *next_word();


/*
 * Query the kernel to find network interfaces that are multicast-capable
 * and install them in the uvifs array.
 */
void config_vifs_from_kernel()
{
    struct ifreq ifbuf[32];
    struct ifreq *ifrp, *ifend, *mp;
    struct ifconf ifc;
    register struct uvif *v;
    register vifi_t vifi;
    int i, n;
    u_long addr, mask, subnet;
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
	addr = ((struct sockaddr_in *)&ifrp->ifr_addr)->sin_addr.s_addr;
	if (ifrp->ifr_addr.sa_family != AF_INET)
	    continue;

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
		"ignoring %s, has invalid address (%s) and/or mask (%08x)",
		ifr.ifr_name, inet_fmt(addr, s1), ntohl(mask));
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
	v->uv_rate_limit  = DEFAULT_RATE_LIMIT;
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

static struct ifreq *
ifconfaddr(ifcp, a)
    struct ifconf *ifcp;
    u_long a;
{
    int n;
    struct ifreq *ifrp = (struct ifreq *)ifcp->ifc_buf;
    struct ifreq *ifend = (struct ifreq *)((char *)ifrp + ifcp->ifc_len);

    while (ifrp < ifend) {
	    if (ifrp->ifr_addr.sa_family == AF_INET &&
		((struct sockaddr_in *)&ifrp->ifr_addr)->sin_addr.s_addr == a)
		    return (ifrp);
#if BSD >= 199006
		n = ifrp->ifr_addr.sa_len + sizeof(ifrp->ifr_name);
		if (n < sizeof(*ifrp))
			++ifrp;
		else
			ifrp = (struct ifreq *)((char *)ifrp + n);
#else
		++ifrp;
#endif
    }
    return (0);
}

/*
 * Checks if the string constitutes a valid interface name
 */
static u_long valid_if(w)
char *w;
{
    register vifi_t vifi;
    register struct uvif *v;

    for (vifi = 0, v = uvifs; vifi < numvifs; vifi++, v++)
	if (EQUAL(v->uv_name, w))
	    return v->uv_lcl_addr;

    return NULL;
}

/*
 * Read the config file to learn about tunnel vifs and
 * non-default phyint parameters.
 */
void config_vifs_from_file()
{
    FILE *f;
    char linebuf[100];
    char *w, *s, c;
    u_long lcl_addr, rmt_addr;
    struct ifconf ifc;
    struct ifreq *ifr;
    struct ifreq ffr;
    int i;
    u_int n;
    struct ifreq ifbuf[32];
    vifi_t vifi;
    struct uvif *v;
    u_char order = 0;
    vifi_t prev_vif = NO_VIF;

    f = fopen(configfilename, "r");
    if (f == NULL) {
	if (errno != ENOENT)
	    log(LOG_ERR, errno, "can't open %s", configfilename);
	return;
    }

    ifc.ifc_buf = (char *)ifbuf;
    ifc.ifc_len = sizeof(ifbuf);
    if (ioctl(udp_socket, SIOCGIFCONF, (char *)&ifc) < 0)
	log(LOG_ERR, errno, "ioctl SIOCGIFCONF");

    while (fgets(linebuf, sizeof(linebuf), f) != NULL) {

	s = linebuf;
	if (EQUAL((w = next_word(&s)), "")) {
	    /*
	     * blank or comment line; ignore
	     */
	}

	/* Set the cache_lifetime for kernel entries */
 	else if (EQUAL(w, "cache_lifetime")) {
 	    if (EQUAL((w = next_word(&s)), "")) {
 		log(LOG_ERR, 0,
 		    "missing cache_lifetime value in %s",
 		    configfilename);
 		continue;
 	    }
 	    if(sscanf(w, "%u%c", &n, &c) != 1 ||
	       n < 300 || n > 86400 ) {
		log(LOG_ERR, 0,
		    "invalid cache_lifetime '%s' (300<n>86400) in %s",
		    w, configfilename);
		break;
	    }
	    prev_vif = NO_VIF;
 	    cache_lifetime = n;	
 	    max_prune_lifetime = cache_lifetime * 2;
 	}

	/* Check if pruning is to be turned off */
 	else if (EQUAL(w, "pruning")) {
 	    if (!EQUAL((w = next_word(&s)), "off") &&
		!EQUAL(w, "on")) {
 		log(LOG_ERR, 0,
 		    "invalid word '%s' in %s",
 		    w, configfilename);
		continue;
 	    }
	    if (EQUAL(w, "off"))
		pruning = 0;

	    prev_vif = NO_VIF;
 	}

	/* Check for boundary statements (as continuation of a prev. line) */
 	else if (EQUAL(w, "boundary") && prev_vif != NO_VIF) {
	    register struct vif_acl *v_acl;
	    register u_long baddr;
	    
	    v = &uvifs[prev_vif];

	    if (EQUAL((w = next_word(&s)), "")) {
		log(LOG_ERR, 0,
		    "missing group address for boundary %s in %s",
		    inet_fmt(lcl_addr, s1), configfilename);
		w = "garbage";
		break;
	    }
	    
	    if ((sscanf(w, "%[0-9.]/%d", s1, &n) != 2) ||
		n < 0 || n> 32) {
		log(LOG_ERR, 0,
		    "incorrect boundary format %s in %s",
		    w, configfilename);
		w = "garbage";
		break;
	    }
	    
	    if ((baddr = inet_parse(s1)) == 0xffffffff ||
		(ntohl(baddr) & 0xff000000) != 0xef000000) {
		log(LOG_ERR, 0,
		    "incorrect boundary address %s in %s",
		    s1, configfilename);
		continue;
	    }
	    
	    v_acl = (struct vif_acl *)malloc(sizeof(struct vif_acl));
	    if (v_acl == NULL)
		log(LOG_ERR, 0,
		    "out of memory");
	    VAL_TO_MASK(v_acl->acl_mask, n);
	    v_acl->acl_addr   = baddr & v_acl->acl_mask;
	    
	    /*
	     * link into data structure
	     */
	    v_acl->acl_next = v->uv_acl;
	    v->uv_acl = v_acl;
 	}

	else if (EQUAL(w, "phyint")) {
	    /*
	     * phyint <local-addr> [disable] [metric <m>] [threshold <t>]
	     *                                              [rate_limit <b>]
	     */

	    /*
	     * Check if phyint was the first line - scream if not
	     */
	    if (order) {
		log(LOG_ERR, 0, 
		    "phyint stmnts should occur before tunnel stmnts in %s",
		    configfilename);
		continue;
	    }

	    /*
	     * Parse the local address.
	     */
	    if (EQUAL((w = next_word(&s)), "")) {
		log(LOG_ERR, 0,
		    "missing phyint address in %s",
		    configfilename);
		continue;
	    }

	    if (isalpha(*w) && !(lcl_addr = valid_if(w))) {
		log(LOG_ERR, 0,
		    "invalid phyint name '%s' in %s",
		    w, configfilename);
		continue;
	    }

	    if (isdigit(*w)) {
		if ((lcl_addr = inet_parse(w)) == 0xffffffff ||
		    !inet_valid_host(lcl_addr)) {
		    log(LOG_ERR, 0,
			"invalid phyint address '%s' in %s",
			w, configfilename);
		    continue;
		}
	    }

	    /*
	     * Look up the vif with the specified local address.
	     */
	    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
		if (!(v->uv_flags & VIFF_TUNNEL) &&
		    lcl_addr == v->uv_lcl_addr) {
		    break;
		}
	    }

	    if (vifi == numvifs) {
		log(LOG_ERR, 0,
		    "phyint %s in %s is not a configured interface",
		    inet_fmt(lcl_addr, s1), configfilename);
		continue;
	    }

	    /*
	     * Look for "disable", "metric", "threshold", "rate_limit"
	     * and "boundary" options.
	     */
	    prev_vif = vifi;

	    while (!EQUAL((w = next_word(&s)), "")) {
		if (EQUAL(w, "disable")) {
		    v->uv_flags |= VIFF_DISABLED;
		}
		else if (EQUAL(w, "metric")) {
		    if(EQUAL((w = next_word(&s)), "")) {
			log(LOG_ERR, 0,
			    "missing metric for phyint %s in %s",
			    inet_fmt(lcl_addr, s1), configfilename);
			w = "garbage";
			break;
		    }
		    if(sscanf(w, "%u%c", &n, &c) != 1 ||
			      n < 1 || n >= UNREACHABLE ) {
			log(LOG_ERR, 0,
			    "invalid metric '%s' for phyint %s in %s",
			    w, inet_fmt(lcl_addr, s1), configfilename);
			break;
		    }
		    v->uv_metric = n;
		}
		else if (EQUAL(w, "threshold")) {
		    if(EQUAL((w = next_word(&s)), "")) {
			log(LOG_ERR, 0,
			    "missing threshold for phyint %s in %s",
			    inet_fmt(lcl_addr, s1), configfilename);
			w = "garbage";
			break;
		    }
		    if(sscanf(w, "%u%c", &n, &c) != 1 ||
			      n < 1 || n > 255 ) {
			log(LOG_ERR, 0,
			    "invalid threshold '%s' for phyint %s in %s",
			    w, inet_fmt(lcl_addr, s1), configfilename);
			break;
		    }
		    v->uv_threshold = n;
		}
		else if (EQUAL(w, "rate_limit")) {
		    if (EQUAL((w = next_word(&s)), "")) {
		       log(LOG_ERR, 0,
			    "missing rate_limit for phyint %s in %s",
			    inet_fmt(rmt_addr, s1), configfilename);
			w = "garbage";
			break;
		    }
		    if(sscanf(w, "%u%c", &n, &c) != 1 ||
			      n < 0 || n > MAX_RATE_LIMIT ) {
			log(LOG_ERR, 0,
			    "invalid rate limit '%s' for phyint %s in %s",
			    w, inet_fmt(lcl_addr, s1), configfilename);
		       break;
		    }
		    v->uv_rate_limit = n;
		}
		else if (EQUAL(w, "boundary")) {
		    register struct vif_acl *v_acl;
		    register u_long baddr;

		    if (EQUAL((w = next_word(&s)), "")) {
			log(LOG_ERR, 0,
			    "missing group address for boundary %s in %s",
			    inet_fmt(lcl_addr, s1), configfilename);
			w = "garbage";
			break;
		    }

		    if ((sscanf(w, "%[0-9.]/%d", s1, &n) != 2) ||
			n < 0 || n> 32) {
			log(LOG_ERR, 0,
			    "incorrect boundary format %s in %s",
			    w, configfilename);
			w = "garbage";
			break;
		    }

		    if ((baddr = inet_parse(s1)) == 0xffffffff ||
			(ntohl(baddr) & 0xef000000) != 0xef000000) {
			log(LOG_ERR, 0,
			    "incorrect boundary address %s in %s",
			    s1, configfilename);
			continue;
		    }

		    v_acl = (struct vif_acl *)malloc(sizeof(struct vif_acl));
		    if (v_acl == NULL)
			log(LOG_ERR, 0,
			    "out of memory");
		    VAL_TO_MASK(v_acl->acl_mask, n);
		    v_acl->acl_addr   = baddr & v_acl->acl_mask;

		    /*
		     * link into data structure
		     */
		    v_acl->acl_next = v->uv_acl;
		    v->uv_acl = v_acl;
		}
		else {
		    log(LOG_ERR, 0,
			"invalid keyword (%s) in %s",
			w, configfilename);
		    break;
		}
	    }
	    if (!EQUAL(w, "")) continue;
	}

	else if (EQUAL(w, "tunnel")) {
	    /*
	     * tunnel <local-addr> <remote-addr> [srcrt] [metric <m>]
	     *                              [threshold <t>] [rate_limit <b>]
	     */

	    order++;

	    /*
	     * Parse the local address.
	     */
	    if (EQUAL((w = next_word(&s)), "")) {
		log(LOG_ERR, 0,
		    "missing tunnel local address in %s",
		    configfilename);
		continue;
	    }
	    if ((lcl_addr = inet_parse(w)) == 0xffffffff ||
		!inet_valid_host(lcl_addr)) {
		log(LOG_ERR, 0,
		    "invalid tunnel local address '%s' in %s",
		    w, configfilename);
		continue;
	    }

	    /*
	     * Make sure the local address is one of ours.
	     */
	    ifr = ifconfaddr(&ifc, lcl_addr);
	    if (ifr == 0) {
		log(LOG_ERR, 0,
		    "tunnel local address %s in %s is not one of ours",
		    inet_fmt(lcl_addr, s1), configfilename);
		continue;
	    }

	    /*
	     * Make sure the local address doesn't name a loopback interface..
	     */
	    strncpy(ffr.ifr_name, ifr->ifr_name, IFNAMSIZ);
	    if (ioctl(udp_socket, SIOCGIFFLAGS, (char *)&ffr) < 0) {
		log(LOG_ERR, errno,
		    "ioctl SIOCGIFFLAGS for %s", ffr.ifr_name);
	    }
	    if (ffr.ifr_flags & IFF_LOOPBACK) {
		log(LOG_ERR, 0,
		    "tunnel local address %s in %s is a loopback interface",
		    inet_fmt(lcl_addr, s1), configfilename);
		continue;
	    }

	    /*
	     * Parse the remote address.
	     */
	    if (EQUAL((w = next_word(&s)), "")) {
		log(LOG_ERR, 0,
		    "missing tunnel remote address in %s",
		    configfilename);
		continue;
	    }
	    if ((rmt_addr = inet_parse(w)) == 0xffffffff ||
		!inet_valid_host(rmt_addr)) {
		log(LOG_ERR, 0,
		    "invalid tunnel remote address %s in %s",
		    w, configfilename);
		continue;
	    }

	    /*
	     * Make sure the remote address is not one of ours.
	     */
	    if (ifconfaddr(&ifc, rmt_addr) != 0) {
		log(LOG_ERR, 0,
		    "tunnel remote address %s in %s is one of ours",
		    inet_fmt(rmt_addr, s1), configfilename);
		continue;
	    }

	    /*
	     * Make sure the remote address has not been used for another
	     * tunnel and does not belong to a subnet to which we have direct
	     * access on an enabled phyint.
	     */
	    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
		if (v->uv_flags & VIFF_TUNNEL) {
		    if (rmt_addr == v->uv_rmt_addr) {
			log(LOG_ERR, 0,
			    "duplicate tunnel remote address %s in %s",
			    inet_fmt(rmt_addr, s1), configfilename);
			break;
		    }
		}
		else if (!(v->uv_flags & VIFF_DISABLED)) {
		    if ((rmt_addr & v->uv_subnetmask) == v->uv_subnet) {
			log(LOG_ERR, 0,
			    "unnecessary tunnel remote address %s in %s",
			    inet_fmt(rmt_addr, s1), configfilename);
			break;
		    }
		}
	    }
	    if (vifi != numvifs) continue;

	    /*
	     * OK, let's initialize a uvif structure for the tunnel.
	     */
	    if (numvifs == MAXVIFS) {
		log(LOG_ERR, 0, "too many vifs, ignoring tunnel to %s",
		    inet_fmt(rmt_addr, s1));
		continue;
	    }
	    v  = &uvifs[numvifs];
	    v->uv_flags       = VIFF_TUNNEL;
	    v->uv_metric      = DEFAULT_METRIC;
	    v->uv_rate_limit  = DEFAULT_RATE_LIMIT;
	    v->uv_threshold   = DEFAULT_THRESHOLD;
	    v->uv_lcl_addr    = lcl_addr;
	    v->uv_rmt_addr    = rmt_addr;
	    v->uv_subnet      = 0;
	    v->uv_subnetmask  = 0;
	    v->uv_subnetbcast = 0;
	    strncpy(v->uv_name, ffr.ifr_name, IFNAMSIZ);
	    v->uv_groups      = NULL;
	    v->uv_neighbors   = NULL;
	    v->uv_acl         = NULL;

	    /*
	     * set variable to define which interface 
	     */
	    prev_vif = numvifs;

	    /*
	     * Look for "metric", "threshold", "srcrt", "rate_limit"
	     * and "boundary" options.
	     */
	    while (!EQUAL((w = next_word(&s)), "")) {
		if (EQUAL(w, "metric")) {
		    if(EQUAL((w = next_word(&s)), "")) {
			log(LOG_ERR, 0,
			    "missing metric for tunnel to %s in %s",
			    inet_fmt(rmt_addr, s1), configfilename);
			w = "garbage";
			break;
		    }
		    if(sscanf(w, "%u%c", &n, &c) != 1 ||
			      n < 1 || n >= UNREACHABLE ) {
			log(LOG_ERR, 0,
			    "invalid metric '%s' for tunnel to %s in %s",
			    w, inet_fmt(rmt_addr, s1), configfilename);
			break;
		    }
		    v->uv_metric = n;
		}
		else if (EQUAL(w, "threshold")) {
		    if(EQUAL((w = next_word(&s)), "")) {
			log(LOG_ERR, 0,
			    "missing threshold for tunnel to %s in %s",
			    inet_fmt(rmt_addr, s1), configfilename);
			w = "garbage";
			break;
		    }
		    if(sscanf(w, "%u%c", &n, &c) != 1 ||
			      n < 1 || n > 255 ) {
			log(LOG_ERR, 0,
			    "invalid threshold '%s' for tunnel to %s in %s",
			    w, inet_fmt(rmt_addr, s1), configfilename);
			break;
		    }
		    v->uv_threshold = n;
		}
		else if (EQUAL(w, "srcrt") || EQUAL(w, "sourceroute")) {
		    v->uv_flags |= VIFF_SRCRT;
		}
		else if (EQUAL(w, "rate_limit")) {
		    if (EQUAL((w = next_word(&s)), "")) {
		       log(LOG_ERR, 0,
			    "missing rate_limit for tunnel to %s in %s",
			    inet_fmt(rmt_addr, s1), configfilename);
			w = "garbage";
			break;
		    }
		    if(sscanf(w, "%u%c", &n, &c) != 1 ||
			      n < 0 || n > MAX_RATE_LIMIT ) {
			log(LOG_ERR, 0,
			    "invalid rate_limit '%s' for tunnel to %s in %s",
			    w, inet_fmt(rmt_addr, s1), configfilename);
		       break;
		    }
		    v->uv_rate_limit = n;
		}
		else if (EQUAL(w, "boundary")) {
		    register struct vif_acl *v_acl;
		    register u_long baddr;

		    if (EQUAL((w = next_word(&s)), "")) {
			log(LOG_ERR, 0,
			    "missing group address for tunnel to %s in %s",
			    inet_fmt(rmt_addr, s1), configfilename);
			w = "garbage";
			break;
		    }

		    if ((sscanf(w, "%[0-9.]/%d", s1, &n) != 2) ||
			n < 0 || n> 32) {
			log(LOG_ERR, 0,
			    "incorrect format '%s' for tunnel to %s in %s",
			    w, inet_fmt(rmt_addr, s1), configfilename);
			break;
		    }

		    if ((baddr = inet_parse(s1)) == 0xffffffff ||
			(ntohl(baddr) & 0xef000000) != 0xef000000) {
			log(LOG_ERR, 0,
			    "incorrect address %s for tunnel to %s in %s",
			    s1, inet_fmt(rmt_addr, s1), configfilename);
			continue;
		    }

		    v_acl = (struct vif_acl *)malloc(sizeof(struct vif_acl));
		    if (v_acl == NULL)
			log(LOG_ERR, 0,
			    "out of memory");
		    VAL_TO_MASK(v_acl->acl_mask, n);
		    v_acl->acl_addr   = baddr & v_acl->acl_mask;

		    /*
		     * link into data structure
		     */
		    v_acl->acl_next = v->uv_acl;
		    v->uv_acl = v_acl;
		}
		else {
		    log(LOG_ERR, 0,
			"invalid keyword (%s) in %s",
			w, configfilename);
		    break;
		}
	    }
	    if (!EQUAL(w, "")) continue;

	    log(LOG_INFO, 0,
		"installing %stunnel from %s to %s as vif #%u - rate=%d",
		v->uv_flags & VIFF_SRCRT? "srcrt " : "",
		inet_fmt(lcl_addr, s1), inet_fmt(rmt_addr, s2),
		numvifs, v->uv_rate_limit);

	    ++numvifs;

	    if (!(ffr.ifr_flags & IFF_UP)) {
		v->uv_flags |= VIFF_DOWN;
		vifs_down = TRUE;
	    }
	}

	else {
	    log(LOG_ERR, 0,
		"unknown command '%s' in %s", w, configfilename);
	}
    }

    close(f);
}


/*
 * Return a pointer to the next "word" in the string to which '*s' points,
 * lower-cased and null terminated, and advance '*s' to point beyond the word.
 * Words are separated by blanks and/or tabs, and the input string is
 * considered to terminate at a newline, '#' (comment), or null character.
 * If no words remain, a pointer to a null string ("") is returned.  
 * Warning: This function clobbers the input string.
 */
static char *next_word(s)
    char **s;
{
    char *w;

    w = *s;
    while (*w == ' ' || *w == '\t')
	++w;

    *s = w;
    for(;;) {
	switch (**s) {

	    case ' '  :
	    case '\t' : **s = '\0';
			++*s;
			return (w);

	    case '\n' :
	    case '#'  : **s = '\0';
			return (w);

	    case '\0' : return (w);

	    default   : if (isascii(**s) && isupper(**s))
			    **s = tolower(**s);
			++*s;
	}
    }
}
