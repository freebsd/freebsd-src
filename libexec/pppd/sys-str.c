/*
 * sys-str.c - System-dependent procedures for setting up
 * PPP interfaces on systems which use the STREAMS ppp interface.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * TODO:
 */

#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stream.h>
#include <sys/stropts.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_arp.h>
#include <netinet/in.h>

#include "pppd.h"
#include "ppp.h"
#include <net/ppp_str.h>

#ifndef ifr_mtu
#define ifr_mtu		ifr_metric
#endif

#define	MAXMODULES	10	/* max number of module names to save */
static struct	modlist {
    char	modname[FMNAMESZ+1];
} str_modules[MAXMODULES];
static int	str_module_count = 0;

extern int hungup;		/* has the physical layer been disconnected? */

/*
 * ppp_available - check if this kernel supports PPP.
 */
int
ppp_available()
{
    int fd, ret;

    fd = open("/dev/tty", O_RDONLY, 0);
    if (fd < 0)
	return 1;		/* can't find out - assume we have ppp */
    ret = ioctl(fd, I_FIND, "pppasync") >= 0;
    close(fd);
    return ret;
}


/*
 * establish_ppp - Turn the serial port into a ppp interface.
 */
void
establish_ppp()
{
    /* go through and save the name of all the modules, then pop em */
    for (;;) { 
	if (ioctl(fd, I_LOOK, str_modules[str_module_count].modname) < 0 ||
	    ioctl(fd, I_POP, 0) < 0)
	    break;
	MAINDEBUG((LOG_DEBUG, "popped stream module : %s",
		   str_modules[str_module_count].modname));
	str_module_count++;
    }

    MAINDEBUG((LOG_INFO, "about to push modules..."));

    /* now push the async/fcs module */
    if (ioctl(fd, I_PUSH, "pppasync") < 0) {
	syslog(LOG_ERR, "ioctl(I_PUSH, ppp_async): %m");
	die(1);
    }
    /* finally, push the ppp_if module that actually handles the */
    /* network interface */ 
    if (ioctl(fd, I_PUSH, "pppif") < 0) {
	syslog(LOG_ERR, "ioctl(I_PUSH, ppp_if): %m");
	die(1);
    }
    if (ioctl(fd, I_SETSIG, S_INPUT) < 0) {
	syslog(LOG_ERR, "ioctl(I_SETSIG, S_INPUT): %m");
	die(1);
    }
    /* read mode, message non-discard mode */
    if (ioctl(fd, I_SRDOPT, RMSGN) < 0) {
	syslog(LOG_ERR, "ioctl(I_SRDOPT, RMSGN): %m");
	die(1);
    }
    /* Flush any waiting messages, or we'll never get SIGPOLL */
    if (ioctl(fd, I_FLUSH, FLUSHRW) < 0) {
	syslog(LOG_ERR, "ioctl(I_FLUSH, FLUSHRW): %m");
	die(1);
    }
    /*
     * Find out which interface we were given.
     * (ppp_if handles this ioctl)
     */
    if (ioctl(fd, SIOCGETU, &ifunit) < 0) {
	syslog(LOG_ERR, "ioctl(SIOCGETU): %m");
	die(1);
    }

    /* if debug, set debug flags in driver */
    {
	int flags = debug ? 0x3 : 0;
	if (ioctl(fd, SIOCSIFDEBUG, &flags) < 0) {
	    syslog(LOG_ERR, "ioctl(SIOCSIFDEBUG): %m");
	}
    }

    MAINDEBUG((LOG_INFO, "done pushing modules, ifunit %d", ifunit));
}

/*
 * disestablish_ppp - Restore the serial port to normal operation.
 * It attempts to reconstruct the stream with the previously popped
 * modules.  This shouldn't call die() because it's called from die().
 */
void
disestablish_ppp()
{
    /*EMPTY*/

    if (hungup) {
	/* we can't push or pop modules after the stream has hung up */
	str_module_count = 0;
	return;
    }

    while (ioctl(fd, I_POP, 0) == 0)	/* pop any we pushed */
	;
  
    for (; str_module_count > 0; str_module_count--) {
	if (ioctl(fd, I_PUSH, str_modules[str_module_count-1].modname)) {
	    syslog(LOG_ERR, "str_restore: couldn't push module %s: %m",
		   str_modules[str_module_count-1].modname);
	} else {
	    MAINDEBUG((LOG_INFO, "str_restore: pushed module %s",
		       str_modules[str_module_count-1].modname));
	}
    }
}


/*
 * output - Output PPP packet.
 */
void
output(unit, p, len)
    int unit;
    u_char *p;
    int len;
{
    struct strbuf	str;

    if (unit != 0)
	MAINDEBUG((LOG_WARNING, "output: unit != 0!"));

    str.len = len;
    str.buf = (caddr_t) p;
    if(putmsg(fd, NULL, &str, 0) < 0) {
	syslog(LOG_ERR, "putmsg: %m");
	die(1);
    }
}


/*
 * read_packet - get a PPP packet from the serial device.
 */
int
read_packet(buf)
    u_char *buf;
{
    struct strbuf str;
    int len, i;

    str.maxlen = MTU+DLLHEADERLEN;
    str.buf = (caddr_t) buf;
    i = 0;
    len = getmsg(fd, NULL, &str, &i);
    if (len < 0) {
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    return -1;
	}
	syslog(LOG_ERR, "getmsg(fd) %m");
	die(1);
    }
    if (len) 
	MAINDEBUG((LOG_DEBUG, "getmsg returned 0x%x",len));

    if (str.len < 0) {
	MAINDEBUG((LOG_DEBUG, "getmsg short return length %d", str.len));
	return -1;
    }

    return str.len;
}


/*
 * ppp_send_config - configure the transmit characteristics of
 * the ppp interface.
 */
void
ppp_send_config(unit, mtu, asyncmap, pcomp, accomp)
    int unit, mtu;
    u_long asyncmap;
    int pcomp, accomp;
{
    char c;
    struct ifreq ifr;

    strncpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    ifr.ifr_mtu = mtu;
    if (ioctl(s, SIOCSIFMTU, (caddr_t) &ifr) < 0) {
	syslog(LOG_ERR, "ioctl(SIOCSIFMTU): %m");
	quit();
    }

    if(ioctl(fd, SIOCSIFASYNCMAP, (caddr_t) &asyncmap) < 0) {
	syslog(LOG_ERR, "ioctl(SIOCSIFASYNCMAP): %m");
	quit();
    }

    c = pcomp;
    if(ioctl(fd, SIOCSIFCOMPPROT, &c) < 0) {
	syslog(LOG_ERR, "ioctl(SIOCSIFCOMPPROT): %m");
	quit();
    }

    c = accomp;
    if(ioctl(fd, SIOCSIFCOMPAC, &c) < 0) {
	syslog(LOG_ERR, "ioctl(SIOCSIFCOMPAC): %m");
	quit();
    }
}

/*
 * ppp_recv_config - configure the receive-side characteristics of
 * the ppp interface.  At present this just sets the MRU.
 */
void
ppp_recv_config(unit, mru, asyncmap, pcomp, accomp)
    int unit, mru;
    u_long asyncmap;
    int pcomp, accomp;
{
    char c;

    if (ioctl(fd, SIOCSIFMRU, &mru) < 0) {
	syslog(LOG_ERR, "ioctl(SIOCSIFMRU): %m");
    }

#ifdef notyet
    if(ioctl(fd, SIOCSIFRASYNCMAP, (caddr_t) &asyncmap) < 0) {
	syslog(LOG_ERR, "ioctl(SIOCSIFRASYNCMAP): %m");
    }

    c = accomp;
    if(ioctl(fd, SIOCSIFRCOMPAC, &c) < 0) {
	syslog(LOG_ERR, "ioctl(SIOCSIFRCOMPAC): %m");
	quit();
    }
#endif	/* notyet */
}

/*
 * sifvjcomp - config tcp header compression
 */
int
sifvjcomp(u, vjcomp, cidcomp)
{
    char x = vjcomp;

    if(ioctl(fd, SIOCSIFVJCOMP, (caddr_t) &x) < 0) {
	syslog(LOG_ERR, "ioctl(SIOCSIFVJCOMP): %m");
	return 0;
    }
    return 1;
}

/*
 * sifup - Config the interface up.
 */
int
sifup(u)
{
    struct ifreq ifr;

    strncpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    if (ioctl(s, SIOCGIFFLAGS, (caddr_t) &ifr) < 0) {
	syslog(LOG_ERR, "ioctl (SIOCGIFFLAGS): %m");
	return 0;
    }
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(s, SIOCSIFFLAGS, (caddr_t) &ifr) < 0) {
	syslog(LOG_ERR, "ioctl(SIOCSIFFLAGS): %m");
	return 0;
    }
    return 1;
}

/*
 * sifdown - Config the interface down.
 */
int
sifdown(u)
{
    struct ifreq ifr;
    strncpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    if (ioctl(s, SIOCGIFFLAGS, (caddr_t) &ifr) < 0) {
	syslog(LOG_ERR, "ioctl (SIOCGIFFLAGS): %m");
	return 0;
    }
    ifr.ifr_flags &= ~IFF_UP;
    if (ioctl(s, SIOCSIFFLAGS, (caddr_t) &ifr) < 0) {
	syslog(LOG_ERR, "ioctl(SIOCSIFFLAGS): %m");
	return 0;
    }
    return 1;
}

/*
 * SET_SA_FAMILY - initialize a struct sockaddr, setting the sa_family field.
 */
#define SET_SA_FAMILY(addr, family)		\
    BZERO((char *) &(addr), sizeof(addr));	\
    addr.sa_family = (family);

/*
 * sifaddr - Config the interface IP addresses and netmask.
 */
int
sifaddr(u, o, h, m)
{
    int ret;
    struct ifreq ifr;

    ret = 1;
    strncpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    SET_SA_FAMILY(ifr.ifr_addr, AF_INET);
    ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr = o;
    if (ioctl(s, SIOCSIFADDR, (caddr_t) &ifr) < 0) {
	syslog(LOG_ERR, "ioctl(SIOCSIFADDR): %m");
	ret = 0;
    }
    ((struct sockaddr_in *) &ifr.ifr_dstaddr)->sin_addr.s_addr = h;
    if (ioctl(s, SIOCSIFDSTADDR, (caddr_t) &ifr) < 0) {
	syslog(LOG_ERR, "ioctl(SIOCSIFDSTADDR): %m");
	ret = 0;
    }
    if (m != 0) {
	((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr = m;
	syslog(LOG_INFO, "Setting interface mask to %s\n", ip_ntoa(m));
	if (ioctl(s, SIOCSIFNETMASK, (caddr_t) &ifr) < 0) {
	    syslog(LOG_ERR, "ioctl(SIOCSIFNETMASK): %m");
	    ret = 0;
	}
    }
    return ret;
}

/*
 * cifaddr - Clear the interface IP addresses, and delete routes
 * through the interface if possible.
 */
int
cifaddr(u, o, h)
{
    struct rtentry rt;

    SET_SA_FAMILY(rt.rt_dst, AF_INET);
    ((struct sockaddr_in *) &rt.rt_dst)->sin_addr.s_addr = h;
    SET_SA_FAMILY(rt.rt_gateway, AF_INET);
    ((struct sockaddr_in *) &rt.rt_gateway)->sin_addr.s_addr = o;
    rt.rt_flags = RTF_HOST;
    if (ioctl(s, SIOCDELRT, (caddr_t) &rt) < 0) {
	syslog(LOG_ERR, "ioctl(SIOCDELRT): %m");
	return 0;
    }
    return 1;
}

/*
 * sifdefaultroute - assign a default route through the address given.
 */
int
sifdefaultroute(u, g)
{
    struct rtentry rt;

    SET_SA_FAMILY(rt.rt_dst, AF_INET);
    SET_SA_FAMILY(rt.rt_gateway, AF_INET);
    ((struct sockaddr_in *) &rt.rt_gateway)->sin_addr.s_addr = g;
    rt.rt_flags = RTF_GATEWAY;
    if (ioctl(s, SIOCADDRT, &rt) < 0) {
	syslog(LOG_ERR, "default route ioctl(SIOCADDRT): %m");
	return 0;
    }
    return 1;
}

/*
 * cifdefaultroute - delete a default route through the address given.
 */
int
cifdefaultroute(u, g)
{
    struct rtentry rt;

    SET_SA_FAMILY(rt.rt_dst, AF_INET);
    SET_SA_FAMILY(rt.rt_gateway, AF_INET);
    ((struct sockaddr_in *) &rt.rt_gateway)->sin_addr.s_addr = g;
    rt.rt_flags = RTF_GATEWAY;
    if (ioctl(s, SIOCDELRT, &rt) < 0) {
	syslog(LOG_ERR, "default route ioctl(SIOCDELRT): %m");
	return 0;
    }
    return 1;
}

/*
 * sifproxyarp - Make a proxy ARP entry for the peer.
 */
int
sifproxyarp(unit, hisaddr)
    int unit;
    u_long hisaddr;
{
    struct arpreq arpreq;

    BZERO(&arpreq, sizeof(arpreq));

    /*
     * Get the hardware address of an interface on the same subnet
     * as our local address.
     */
    if (!get_ether_addr(hisaddr, &arpreq.arp_ha)) {
	syslog(LOG_ERR, "Cannot determine ethernet address for proxy ARP");
	return 0;
    }

    SET_SA_FAMILY(arpreq.arp_pa, AF_INET);
    ((struct sockaddr_in *) &arpreq.arp_pa)->sin_addr.s_addr = hisaddr;
    arpreq.arp_flags = ATF_PERM | ATF_PUBL;
    if (ioctl(s, SIOCSARP, (caddr_t)&arpreq) < 0) {
	syslog(LOG_ERR, "ioctl(SIOCSARP): %m");
	return 0;
    }

    return 1;
}

/*
 * cifproxyarp - Delete the proxy ARP entry for the peer.
 */
int
cifproxyarp(unit, hisaddr)
    int unit;
    u_long hisaddr;
{
    struct arpreq arpreq;

    BZERO(&arpreq, sizeof(arpreq));
    SET_SA_FAMILY(arpreq.arp_pa, AF_INET);
    ((struct sockaddr_in *) &arpreq.arp_pa)->sin_addr.s_addr = hisaddr;
    if (ioctl(s, SIOCDARP, (caddr_t)&arpreq) < 0) {
	syslog(LOG_ERR, "ioctl(SIOCDARP): %m");
	return 0;
    }
    return 1;
}

/*
 * get_ether_addr - get the hardware address of an interface on the
 * the same subnet as ipaddr.  Code borrowed from myetheraddr.c
 * in the cslip-2.6 distribution, which is subject to the following
 * copyright notice:
 *
 * Copyright (c) 1990, 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <fcntl.h>
#include <nlist.h>
#include <kvm.h>
#include <arpa/inet.h>

/* XXX SunOS 4.1 defines this and 3.5 doesn't... */
#ifdef _nlist_h
#define SUNOS4
#endif

#ifdef SUNOS4
#include <netinet/in_var.h>
#endif
#include <netinet/if_ether.h>

/* Cast a struct sockaddr to a structaddr_in */
#define SATOSIN(sa) ((struct sockaddr_in *)(sa))

/* Determine if "bits" is set in "flag" */
#define ALLSET(flag, bits) (((flag) & (bits)) == (bits))

static struct nlist nl[] = {
#define N_IFNET 0
	{ "_ifnet" },
	0
};

static void kread();

int
get_ether_addr(ipaddr, hwaddr)
    u_long ipaddr;
    struct sockaddr *hwaddr;
{
    register kvm_t *kd;
    register struct ifnet *ifp;
    register struct arpcom *ac;
    struct arpcom arpcom;
    struct in_addr *inp;
#ifdef SUNOS4
    register struct ifaddr *ifa;
    register struct in_ifaddr *in;
    union {
	struct ifaddr ifa;
	struct in_ifaddr in;
    } ifaddr;
#endif
    u_long addr, mask;

    /* Open kernel memory for reading */
    kd = kvm_open(0, 0, 0, O_RDONLY, NULL);
    if (kd == 0) {
	syslog(LOG_ERR, "kvm_open: %m");
	return 0;
    }

    /* Fetch namelist */
    if (kvm_nlist(kd, nl) != 0) {
	syslog(LOG_ERR, "kvm_nlist failed");
	return 0;
    }

    ac = &arpcom;
    ifp = &arpcom.ac_if;
#ifdef SUNOS4
    ifa = &ifaddr.ifa;
    in = &ifaddr.in;
#endif

    if (kvm_read(kd, nl[N_IFNET].n_value, (char *)&addr, sizeof(addr))
	!= sizeof(addr)) {
	syslog(LOG_ERR, "error reading ifnet addr");
	return 0;
    }
    for ( ; addr; addr = (u_long)ifp->if_next) {
	if (kvm_read(kd, addr, (char *)ac, sizeof(*ac)) != sizeof(*ac)) {
	    syslog(LOG_ERR, "error reading ifnet");
	    return 0;
	}

	/* Only look at configured, broadcast interfaces */
	if (!ALLSET(ifp->if_flags, IFF_UP | IFF_BROADCAST))
	    continue;
#ifdef SUNOS4
	/* This probably can't happen... */
	if (ifp->if_addrlist == 0)
	    continue;
#endif

	/* Get interface ip address */
#ifdef SUNOS4
	if (kvm_read(kd, (u_long)ifp->if_addrlist, (char *)&ifaddr,
		     sizeof(ifaddr)) != sizeof(ifaddr)) {
	    syslog(LOG_ERR, "error reading ifaddr");
	    return 0;
	}
	inp = &SATOSIN(&ifa->ifa_addr)->sin_addr;
#else
	inp = &SATOSIN(&ifp->if_addr)->sin_addr;
#endif

	/* Check if this interface on the right subnet */
#ifdef SUNOS4
	mask = in->ia_subnetmask;
#else
	mask = ifp->if_subnetmask;
#endif
	if ((ipaddr & mask) != (inp->s_addr & mask))
	    continue;

	/* Copy out the local ethernet address */
	hwaddr->sa_family = AF_UNSPEC;
	BCOPY((caddr_t) &arpcom.ac_enaddr, hwaddr->sa_data,
	      sizeof(arpcom.ac_enaddr));
	return 1;		/* success! */
    }

    /* couldn't find one */
    return 0;
}
