/*
 * Copyright (c) 1990 The Regents of the University of California.
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
#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char rcsid[] =
    "@(#) $Header: /pub/FreeBSD/FreeBSD-CVS/src/usr.sbin/rarpd/rarpd.c,v 1.4.4.1 1995/08/05 08:04:30 davidg Exp $ (LBL)";
#endif


/*
 * rarpd - Reverse ARP Daemon
 *
 * Usage:	rarpd -a [ -f ] [ hostname ]
 *		rarpd [ -f ] interface [ hostname ]
 *
 * 'hostname' is optional solely for backwards compatibility with Sun's rarpd.
 * Currently, the argument is ignored.
 */

#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
/* SunOS 4.x defines this while 3.x does not. */
#ifdef __sys_types_h
#define SUNOS4
#endif
#include <sys/time.h>
#include <net/bpf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <netdb.h>

#ifdef SUNOS4
#include <dirent.h>
#else
#include <sys/dir.h>
#endif

/*
 * Map field names in ether_arp struct.  What a pain in the neck.
 */
#if !defined(SUNOS4) && !defined(__FreeBSD__)
#undef arp_sha
#undef arp_spa
#undef arp_tha
#undef arp_tpa
#define arp_sha arp_xsha
#define arp_spa arp_xspa
#define arp_tha arp_xtha
#define arp_tpa arp_xtpa
#endif

#ifndef __GNUC__
#define inline
#endif

extern int errno;
extern int ether_ntohost __P((char *, struct ether_addr *));

/*
 * The structure for each interface.
 */
struct if_info {
	int 	ii_fd;		/* BPF file descriptor */
	u_char	ii_eaddr[6];	/* Ethernet address of this interface */
	u_long	ii_ipaddr;	/* IP address of this interface */
	u_long	ii_netmask;	/* subnet or net mask */
	struct if_info *ii_next;
};

/*
 * The list of all interfaces that are being listened to.  rarp_loop()
 * "selects" on the descriptors in this list.
 */
struct if_info *iflist;

extern char *malloc();
extern void exit();

u_long ipaddrtonetmask();
void init_one();
void init_all();
void rarp_loop();
void lookup_eaddr();
void lookup_ipaddr();

void
main(argc, argv)
	int argc;
	char **argv;
{
	int op, pid;
	char *ifname, *hostname, *name;

	int aflag = 0;		/* listen on "all" interfaces  */
	int fflag = 0;		/* don't fork */

	extern char *optarg;
	extern int optind, opterr;

	if (name = strrchr(argv[0], '/'))
		++name;
	else
		name = argv[0];
	if (*name == '-')
		++name;

	/*
	 * All error reporting is done through syslogs.
	 */
	openlog(name, LOG_PID, LOG_DAEMON);

	opterr = 0;
	while ((op = getopt(argc, argv, "af")) != EOF) {
		switch (op) {
		case 'a':
			++aflag;
			break;

		case 'f':
			++fflag;
			break;

		default:
			usage();
			/* NOTREACHED */
		}
	}
	ifname = argv[optind++];
	hostname =  ifname ? argv[optind] : 0;
	if ((aflag && ifname) || (!aflag && ifname == 0))
		usage();

	if (aflag)
		init_all();
	else
		init_one(ifname);

	if (!fflag)
		if (daemon(0,0)) {
			perror("fork");
			exit(0);
		}
	rarp_loop();
}

/*
 * Add 'ifname' to the interface list.  Lookup its IP address and network
 * mask and Ethernet address, and open a BPF file for it.
 */
void
init_one(ifname)
	char *ifname;
{
	struct if_info *p;


	p = (struct if_info *)malloc(sizeof(*p));
	p->ii_next = iflist;
	iflist = p;

	p->ii_fd = rarp_open(ifname);
	lookup_eaddr(p->ii_fd, p->ii_eaddr);
	lookup_ipaddr(ifname, &p->ii_ipaddr, &p->ii_netmask);
}

/*
 * Initialize all "candidate" interfaces that are in the system
 * configuration list.  A "candidate" is up, not loopback and not
 * point to point.
 */
void
init_all()
{
	int fd;
	int ifflags;
	struct ifreq ibuf[8], tmp_ibuf, *ifptr, *n;
	struct ifconf ifc;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "socket: %m");
		exit(1);
	}
	ifc.ifc_len = sizeof ibuf;
	ifc.ifc_buf = (caddr_t)ibuf;
	if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0 ||
	    ifc.ifc_len < sizeof(struct ifreq)) {
		syslog(LOG_ERR, "SIOCGIFCONF: %m");
		exit(1);
	}
	ifptr = ifc.ifc_req;
	ifflags = ifptr->ifr_flags;
	n = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
	while (ifptr < n) {
		bcopy((char *)ifptr, (char *)&tmp_ibuf, sizeof(struct ifreq));
		if (ioctl(fd, SIOCGIFFLAGS, (char *)&tmp_ibuf) < 0) {
			syslog(LOG_ERR, "SIOCGIFFLAGS: %m");
			exit(1);
		}
		if (ifptr->ifr_flags == ifflags && (tmp_ibuf.ifr_flags &
			(IFF_UP | IFF_LOOPBACK | IFF_POINTOPOINT)) == IFF_UP)
			init_one(ifptr->ifr_name);
		if(ifptr->ifr_addr.sa_len)	/* Dohw! */
			ifptr = (struct ifreq *) ((caddr_t) ifptr +
			ifptr->ifr_addr.sa_len -
			sizeof(struct sockaddr));
		ifptr++;
	}
	(void)close(fd);
}

usage()
{
	(void)fprintf(stderr, "usage: rarpd [ -af ] [ interface ]\n");
	exit(1);
}

static int
bpf_open()
{
	int fd;
	int n = 0;
	char device[sizeof "/dev/bpf000"];

	/*
	 * Go through all the minors and find one that isn't in use.
	 */
	do {
		(void)sprintf(device, "/dev/bpf%d", n++);
		fd = open(device, O_RDWR);
	} while (fd < 0 && errno == EBUSY);

	if (fd < 0) {
		syslog(LOG_ERR, "%s: %m", device);
		exit(-1);
	}
	return fd;
}

/*
 * Open a BPF file and attach it to the interface named 'device'.
 * Set immediate mode, and set a filter that accepts only RARP requests.
 */
int
rarp_open(device)
	char *device;
{
	int fd;
	struct ifreq ifr;
	int immediate, link_type;

	static struct bpf_insn insns[] = {
                BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 12),
                BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, ETHERTYPE_REVARP, 0, 3),
                BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 20),
                BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, ARPOP_REVREQUEST, 0, 1),
                BPF_STMT(BPF_RET+BPF_K, sizeof(struct ether_arp) +
                                sizeof(struct ether_header)),
                BPF_STMT(BPF_RET+BPF_K, 0),
        };

        static struct bpf_program filter = {
                sizeof insns / sizeof(insns[0]),
                (struct bpf_insn *)&insns
        };

	fd = bpf_open();
	/*
	 * Set immediate mode so packets are processed as they arrive.
	 */
	immediate = 1;
	if (ioctl(fd, BIOCIMMEDIATE, &immediate) < 0) {
		syslog(LOG_ERR, "BIOCIMMEDIATE: %m");
		exit(1);
	}
	(void)strncpy(ifr.ifr_name, device, sizeof ifr.ifr_name);
	if (ioctl(fd, BIOCSETIF, (caddr_t)&ifr) < 0) {
		syslog(LOG_ERR, "BIOCSETIF: %m");
		exit(1);
	}
	/*
	 * Check that the data link layer is an Ethernet; this code won't
	 * work with anything else.
	 */
	if (ioctl(fd, BIOCGDLT, &link_type) < 0) {
		syslog(LOG_ERR, "BIOCGDLP: %m");
		exit(1);
	}
	if (link_type != DLT_EN10MB) {
		syslog(LOG_ERR, "%s not on ethernet", device);
		exit(1);
	}
	/*
	 * Set filter program.
	 */
	if (ioctl(fd, BIOCSETF, (caddr_t)&filter) < 0) {
		syslog(LOG_ERR, "BIOCSETF: %m");
		exit(1);
	}
	return fd;
}

/*
 * Perform various sanity checks on the RARP request packet.  Return
 * false on failure and log the reason.
 */
static int
rarp_check(p, len)
	u_char *p;
	int len;
{
	struct ether_header *ep = (struct ether_header *)p;
	struct ether_arp *ap = (struct ether_arp *)(p + sizeof(*ep));

	if (len < sizeof(*ep) + sizeof(*ap)) {
		syslog(LOG_ERR, "truncated request");
		return 0;
	}
	/*
	 * XXX This test might be better off broken out...
	 */
	if (ep->ether_type != htons(ETHERTYPE_REVARP) ||
	    ap->arp_hrd != htons(ARPHRD_ETHER) ||
	    ap->arp_op != htons(ARPOP_REVREQUEST) ||
	    ap->arp_pro != htons(ETHERTYPE_IP) ||
	    ap->arp_hln != 6 || ap->arp_pln != 4) {
		syslog(LOG_DEBUG, "request fails sanity check");
		return 0;
	}
	if (bcmp((char *)&ep->ether_shost, (char *)&ap->arp_sha, 6) != 0) {
		syslog(LOG_DEBUG, "ether/arp sender address mismatch");
		return 0;
	}
	if (bcmp((char *)&ap->arp_sha, (char *)&ap->arp_tha, 6) != 0) {
		syslog(LOG_DEBUG, "ether/arp target address mismatch");
		return 0;
	}
	return 1;
}

#ifndef FD_SETSIZE
#define FD_SET(n, fdp) ((fdp)->fds_bits[0] |= (1 << (n)))
#define FD_ISSET(n, fdp) ((fdp)->fds_bits[0] & (1 << (n)))
#define FD_ZERO(fdp) ((fdp)->fds_bits[0] = 0)
#endif

/*
 * Loop indefinitely listening for RARP requests on the
 * interfaces in 'iflist'.
 */
void
rarp_loop()
{
	struct bpf_hdr *bhp;
	u_char *pkt;
	int cc, fd;
	fd_set fds, listeners;
	int bufsize, maxfd = 0;
	struct if_info *ii;

	if (iflist == 0) {
		syslog(LOG_ERR, "no interfaces");
		exit(1);
	}
	if (ioctl(iflist->ii_fd, BIOCGBLEN, (caddr_t)&bufsize) < 0) {
		syslog(LOG_ERR, "BIOCGBLEN: %m");
		exit(1);
	}
	bhp = (struct bpf_hdr *)malloc((unsigned)bufsize);

	/*
	 * Find the highest numbered file descriptor for select().
	 * Initialize the set of descriptors to listen to.
	 */
	FD_ZERO(&fds);
	for (ii = iflist; ii; ii = ii->ii_next) {
		FD_SET(ii->ii_fd, &fds);
		if (ii->ii_fd > maxfd)
			maxfd = ii->ii_fd;
	}
	while (1) {
		listeners = fds;
		if (select(maxfd + 1, &listeners, (struct fd_set *)0,
			   (struct fd_set *)0, (struct timeval *)0) < 0) {
			syslog(LOG_ERR, "select: %m");
			exit(1);
		}
		for (ii = iflist; ii; ii = ii->ii_next) {
			fd = ii->ii_fd;
			if (FD_ISSET(fd, &listeners)) {
			again:
				cc = read(fd, (char *)bhp, bufsize);
				/*
				 * Due to a SunOS bug, after 2^31 bytes, the
				 * file offset overflows and read fails with
				 * EINVAL.  The lseek() to 0 will fix things.
				 */
				if (cc < 0) {
					if (errno == EINVAL &&
					    (long)(lseek(fd, 0L, SEEK_CUR) + bufsize) < 0) {
						(void)lseek(fd, 0, 0);
						goto again;
					}
					syslog(LOG_ERR, "read: %m");
					exit(1);
				}
				pkt = (u_char *)bhp + bhp->bh_hdrlen;

				if (rarp_check(pkt, (int)bhp->bh_datalen))
					rarp_process(ii, pkt);
			}
		}
	}
}

#ifndef TFTP_DIR
#define TFTP_DIR "/tftpboot"
#endif

/*
 * True if this server can boot the host whose IP address is 'addr'.
 * This check is made by looking in the tftp directory for the
 * configuration file.
 */
rarp_bootable(addr)
	u_long addr;
{

#ifdef SUNOS4
	register struct dirent *dent;
#else
	register struct direct *dent;
#endif
	register DIR *d;
	char ipname[9];
	static DIR *dd = 0;

	/*
	 * XXX   Need to htonl() the IP address or it'll
	 * come out backwards.
	 */
	(void)sprintf(ipname, "%08X", htonl(addr));
	/*
	 * If directory is already open, rewind it.  Otherwise, open it.
	 */
	if (d = dd)
		rewinddir(d);
	else {
		if (chdir(TFTP_DIR) == -1) {
			syslog(LOG_ERR, "chdir: %m");
			exit(1);
		}
		d = opendir(".");
		if (d == 0) {
			syslog(LOG_ERR, "opendir: %m");
			exit(1);
		}
		dd = d;
	}
	while (dent = readdir(d))
		if (strncmp(dent->d_name, ipname, 8) == 0)
			return 1;
	return 0;

}

/*
 * Given a list of IP addresses, 'alist', return the first address that
 * is on network 'net'; 'netmask' is a mask indicating the network portion
 * of the address.
 */
u_long
choose_ipaddr(alist, net, netmask)
	u_long **alist;
	u_long net;
	u_long netmask;
{
	for (; *alist; ++alist) {
		if ((**alist & netmask) == net)
			return **alist;
	}
	return 0;
}

/*
 * A one entry ip/ethernet address cache.
 */
static u_long cache_ipaddr;
static u_char cache_eaddr[6];

/*
 * Answer the RARP request in 'pkt', on the interface 'ii'.  'pkt' has
 * already been checked for validity.  The reply is overlaid on the request.
 */
rarp_process(ii, pkt)
	struct if_info *ii;
	u_char *pkt;
{
	struct ether_header *ep;
	struct hostent *hp;
	u_long target_ipaddr;
	char ename[256];

	ep = (struct ether_header *)pkt;
	/*
	 * If the address in the one element cache, don't bother
	 * looking up names.
	 */
	if (bcmp((char *)cache_eaddr, (char *)&ep->ether_shost, 6) == 0)
		target_ipaddr = cache_ipaddr;
	else {
		if (ether_ntohost(ename, (struct ether_addr *)&ep->ether_shost) != 0 ||
		    (hp = gethostbyname(ename)) == 0)
			return;
		/*
		 * Choose correct address from list.
		 */
		if (hp->h_addrtype != AF_INET) {
			syslog(LOG_ERR, "cannot handle non IP addresses");
			exit(1);
		}
		target_ipaddr = choose_ipaddr((u_long **)hp->h_addr_list,
					      ii->ii_ipaddr & ii->ii_netmask,
					      ii->ii_netmask);
		if (target_ipaddr == 0) {
			syslog(LOG_ERR, "cannot find %s on %08x",
			       ename, ii->ii_ipaddr & ii->ii_netmask);
			return;
		}
		bcopy((char *)&ep->ether_shost, (char *)cache_eaddr, 6);
		cache_ipaddr = target_ipaddr;
	}
	if (rarp_bootable(target_ipaddr))
		rarp_reply(ii, ep, target_ipaddr);
}

/*
 * Lookup the ethernet address of the interface attached to the BPF
 * file descriptor 'fd'; return it in 'eaddr'.
 */
void
lookup_eaddr(fd, eaddr)
	int fd;
	u_char *eaddr;
{
	struct ifreq ifr;

	/* Use BPF descriptor to get ethernet address. */
	if (ioctl(fd, SIOCGIFADDR, (char *)&ifr) < 0) {
		syslog(LOG_ERR, "SIOCGIFADDR: %m");
		exit(1);
	}
	bcopy((char *)&ifr.ifr_addr.sa_data[0], (char *)eaddr, 6);
}

/*
 * Lookup the IP address and network mask of the interface named 'ifname'.
 */
void
lookup_ipaddr(ifname, addrp, netmaskp)
	char *ifname;
	u_long *addrp;
	u_long *netmaskp;
{
	int fd;
	struct ifreq ifr;

	/* Use data gram socket to get IP address. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "socket: %m");
		exit(1);
	}
	(void)strncpy(ifr.ifr_name, ifname, sizeof ifr.ifr_name);
	if (ioctl(fd, SIOCGIFADDR, (char *)&ifr) < 0) {
		syslog(LOG_ERR, "SIOCGIFADDR: %m");
		exit(1);
	}
	*addrp = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
	if (ioctl(fd, SIOCGIFNETMASK, (char *)&ifr) < 0) {
		perror("SIOCGIFNETMASK");
		exit(1);
	}
	*netmaskp = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
	/*
	 * If SIOCGIFNETMASK didn't work, figure out a mask from
	 * the IP address class.
	 */
	if (*netmaskp == 0)
		*netmaskp = ipaddrtonetmask(*addrp);

	(void)close(fd);
}

/*
 * Poke the kernel arp tables with the ethernet/ip address combinataion
 * given.  When processing a reply, we must do this so that the booting
 * host (i.e. the guy running rarpd), won't try to ARP for the hardware
 * address of the guy being booted (he cannot answer the ARP).
 */
update_arptab(ep, ipaddr)
	u_char *ep;
	u_long ipaddr;
{
#ifdef SIOCSARP
	int s;
	struct arpreq request;
	struct sockaddr_in *sin;

	request.arp_flags = 0;
	sin = (struct sockaddr_in *)&request.arp_pa;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = ipaddr;
	request.arp_ha.sa_family = AF_UNSPEC;
	bcopy((char *)ep, (char *)request.arp_ha.sa_data, 6);

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (ioctl(s, SIOCSARP, (caddr_t)&request) < 0)
		syslog(LOG_ERR, "SIOCSARP: %m");
	(void)close(s);
#else
	if (arptab_set(ep, ipaddr) > 0)
		syslog(LOG_ERR, "couldn't update arp table");
#endif
}

/*
 * Build a reverse ARP packet and sent it out on the interface.
 * 'ep' points to a valid ARPOP_REVREQUEST.  The ARPOP_REVREPLY is built
 * on top of the request, then written to the network.
 *
 * RFC 903 defines the ether_arp fields as follows.  The following comments
 * are taken (more or less) straight from this document.
 *
 * ARPOP_REVREQUEST
 *
 * arp_sha is the hardware address of the sender of the packet.
 * arp_spa is undefined.
 * arp_tha is the 'target' hardware address.
 *   In the case where the sender wishes to determine his own
 *   protocol address, this, like arp_sha, will be the hardware
 *   address of the sender.
 * arp_tpa is undefined.
 *
 * ARPOP_REVREPLY
 *
 * arp_sha is the hardware address of the responder (the sender of the
 *   reply packet).
 * arp_spa is the protocol address of the responder (see the note below).
 * arp_tha is the hardware address of the target, and should be the same as
 *   that which was given in the request.
 * arp_tpa is the protocol address of the target, that is, the desired address.
 *
 * Note that the requirement that arp_spa be filled in with the responder's
 * protocol is purely for convenience.  For instance, if a system were to use
 * both ARP and RARP, then the inclusion of the valid protocol-hardware
 * address pair (arp_spa, arp_sha) may eliminate the need for a subsequent
 * ARP request.
 */
rarp_reply(ii, ep, ipaddr)
	struct if_info *ii;
	struct ether_header *ep;
	u_long ipaddr;
{
	int n;
	struct ether_arp *ap = (struct ether_arp *)(ep + 1);
	int len, raw_sock;

	update_arptab((u_char *)&ap->arp_sha, ipaddr);

	/*
	 * Build the rarp reply by modifying the rarp request in place.
	 */
	ap->arp_op = htons(ARPOP_REVREPLY);

	/*
	 * XXX   Using htons(ETHERTYPE_REVARP) doesn't work: you wind
	 * up transmitting 0x3580 instead of the correct value of
	 * 0x8035. What makes no sense is that the NetBSD people
	 * do in fact use htons(ETHERTYPE_REVARP) in their rarpd.
	 * (Thank god for tcpdump or I would never have figured this
	 * out.)
	 */
	ep->ether_type = ETHERTYPE_REVARP;

	bcopy((char *)&ap->arp_sha, (char *)&ep->ether_dhost, 6);
	bcopy((char *)ii->ii_eaddr, (char *)&ep->ether_shost, 6);
	bcopy((char *)ii->ii_eaddr, (char *)&ap->arp_sha, 6);

	bcopy((char *)&ipaddr, (char *)ap->arp_tpa, 4);
	/* Target hardware is unchanged. */
	bcopy((char *)&ii->ii_ipaddr, (char *)ap->arp_spa, 4);

	len = sizeof(*ep) + sizeof(*ap);
	n = write(ii->ii_fd, (char *)ep, len);
	if (n != len) {
		syslog(LOG_ERR, "write: only %d of %d bytes written", n, len);
	}
}

/*
 * Get the netmask of an IP address.  This routine is used if
 * SIOCGIFNETMASK doesn't work.
 */
u_long
ipaddrtonetmask(addr)
	u_long addr;
{
	if (IN_CLASSA(addr))
		return IN_CLASSA_NET;
	if (IN_CLASSB(addr))
		return IN_CLASSB_NET;
	if (IN_CLASSC(addr))
		return IN_CLASSC_NET;
	syslog(LOG_DEBUG, "unknown IP address class: %08X", addr);
	exit(1);
	/* NOTREACHED */
}
