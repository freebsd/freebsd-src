/* -*- Mode: c; tab-width: 8; indent-tabs-mode: 1; c-basic-offset: 8; -*- */
/*
 * Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/libpcap/inet.c,v 1.45 2001/10/28 20:40:43 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#include <sys/time.h>				/* concession to AIX */

struct mbuf;
struct rtentry;
#include <net/if.h>
#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#else
#define INT_MAX		2147483647
#endif
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif

#include "pcap-int.h"

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

/* Not all systems have IFF_LOOPBACK */
#ifdef IFF_LOOPBACK
#define ISLOOPBACK(name, flags) ((flags) & IFF_LOOPBACK)
#else
#define ISLOOPBACK(name, flags) ((name)[0] == 'l' && (name)[1] == 'o' && \
    (isdigit((unsigned char)((name)[2])) || (name)[2] == '\0'))
#endif

/*
 * This is fun.
 *
 * In older BSD systems, socket addresses were fixed-length, and
 * "sizeof (struct sockaddr)" gave the size of the structure.
 * All addresses fit within a "struct sockaddr".
 *
 * In newer BSD systems, the socket address is variable-length, and
 * there's an "sa_len" field giving the length of the structure;
 * this allows socket addresses to be longer than 2 bytes of family
 * and 14 bytes of data.
 *
 * Some commercial UNIXes use the old BSD scheme, and some might use
 * the new BSD scheme.
 *
 * GNU libc uses neither scheme, but has an "SA_LEN()" macro that
 * determines the size based on the address family.
 */
#ifndef SA_LEN
#ifdef HAVE_SOCKADDR_SA_LEN
#define SA_LEN(addr)	((addr)->sa_len)
#else /* HAVE_SOCKADDR_SA_LEN */
#define SA_LEN(addr)	(sizeof (struct sockaddr))
#endif /* HAVE_SOCKADDR_SA_LEN */
#endif /* SA_LEN */

/*
 * Description string for the "any" device.
 */
static const char any_descr[] = "Pseudo-device that captures on all interfaces";

static struct sockaddr *
dup_sockaddr(struct sockaddr *sa)
{
	struct sockaddr *newsa;
	unsigned int size;
	
	size = SA_LEN(sa);
	if ((newsa = malloc(size)) == NULL)
		return (NULL);
	return (memcpy(newsa, sa, size));
}

static int
get_instance(char *name)
{
	char *cp, *endcp;
	int n;

	if (strcmp(name, "any") == 0) {
		/*
		 * Give the "any" device an artificially high instance
		 * number, so it shows up after all other non-loopback
		 * interfaces.
		 */
		return INT_MAX;
	}

	endcp = name + strlen(name);
	for (cp = name; cp < endcp && !isdigit((unsigned char)*cp); ++cp)
		continue;

	if (isdigit((unsigned char)*cp))
		n = atoi(cp);
	else
		n = 0;
	return (n);
}

static int
add_or_find_if(pcap_if_t **curdev_ret, pcap_if_t **alldevs, char *name,
    u_int flags, const char *description, char *errbuf)
{
	pcap_t *p;
	pcap_if_t *curdev, *prevdev, *nextdev;
	int this_instance;

	/*
	 * Can we open this interface for live capture?
	 */
	p = pcap_open_live(name, 68, 0, 0, errbuf);
	if (p == NULL) {
		/*
		 * No.  Don't bother including it.
		 * Don't treat this as an error, though.
		 */
		*curdev_ret = NULL;
		return (0);
	}
	pcap_close(p);

	/*
	 * Is there already an entry in the list for this interface?
	 */
	for (curdev = *alldevs; curdev != NULL; curdev = curdev->next) {
		if (strcmp(name, curdev->name) == 0)
			break;	/* yes, we found it */
	}
	if (curdev == NULL) {
		/*
		 * No, we didn't find it.
		 * Allocate a new entry.
		 */
		curdev = malloc(sizeof(pcap_if_t));
		if (curdev == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			return (-1);
		}
		
		/*
		 * Fill in the entry.
		 */
		curdev->next = NULL;
		curdev->name = malloc(strlen(name) + 1);
		strcpy(curdev->name, name);
		if (description != NULL) {
			/*
			 * We have a description for this interface.
			 */
			curdev->description = malloc(strlen(description) + 1);
			strcpy(curdev->description, description);
		} else {
			/*
			 * We don't.
			 */
			curdev->description = NULL;
		}
		curdev->addresses = NULL;	/* list starts out as empty */
		curdev->flags = 0;
		if (ISLOOPBACK(name, flags))
			curdev->flags |= PCAP_IF_LOOPBACK;

		/*
		 * Add it to the list, in the appropriate location.
		 * First, get the instance number of this interface.
		 */
		this_instance = get_instance(name);

		/*
		 * Now look for the last interface with an instance number
		 * less than or equal to the new interface's instance
		 * number - except that non-loopback interfaces are
		 * arbitrarily treated as having interface numbers less
		 * than those of loopback interfaces, so the loopback
		 * interfaces are put at the end of the list.
		 *
		 * We start with "prevdev" being NULL, meaning we're before
		 * the first element in the list.
		 */
		prevdev = NULL;
		for (;;) {
			/*
			 * Get the interface after this one.
			 */
			if (prevdev == NULL) {
				/*
				 * The next element is the first element.
				 */
				nextdev = *alldevs;
			} else
				nextdev = prevdev->next;

			/*
			 * Are we at the end of the list?
			 */
			if (nextdev == NULL) {
				/*
				 * Yes - we have to put the new entry
				 * after "prevdev".
				 */
				break;
			}

			/*
			 * Is the new interface a non-loopback interface
			 * and the next interface a loopback interface?
			 */
			if (!(curdev->flags & PCAP_IF_LOOPBACK) &&
			    (nextdev->flags & PCAP_IF_LOOPBACK)) {
				/*
				 * Yes, we should put the new entry
				 * before "nextdev", i.e. after "prevdev".
				 */
				break;
			}

			/*
			 * Is the new interface's instance number less
			 * than the next interface's instance number,
			 * and is it the case that the new interface is a
			 * non-loopback interface or the next interface is
			 * a loopback interface?
			 *
			 * (The goal of both loopback tests is to make
			 * sure that we never put a loopback interface
			 * before any non-loopback interface and that we
			 * always put a non-loopback interface before all
			 * loopback interfaces.)
			 */
			if (this_instance < get_instance(nextdev->name) &&
			    (!(curdev->flags & PCAP_IF_LOOPBACK) ||
			       (nextdev->flags & PCAP_IF_LOOPBACK))) {
				/*
				 * Yes - we should put the new entry
				 * before "nextdev", i.e. after "prevdev".
				 */
				break;
			}

			prevdev = nextdev;
		}

		/*
		 * Insert before "nextdev".
		 */
		curdev->next = nextdev;

		/*
		 * Insert after "prevdev" - unless "prevdev" is null,
		 * in which case this is the first interface.
		 */
		if (prevdev == NULL) {
			/*
			 * This is the first interface.  Pass back a
			 * pointer to it, and put "curdev" before
			 * "nextdev".
			 */
			*alldevs = curdev;
		} else
			prevdev->next = curdev;
	}
	
	*curdev_ret = curdev;
	return (0);
}

static int
add_addr_to_iflist(pcap_if_t **alldevs, char *name, u_int flags,
    struct sockaddr *addr, struct sockaddr *netmask,
    struct sockaddr *broadaddr, struct sockaddr *dstaddr, char *errbuf)
{
	pcap_if_t *curdev;
	pcap_addr_t *curaddr, *prevaddr, *nextaddr;

	if (add_or_find_if(&curdev, alldevs, name, flags, NULL, errbuf) == -1) {
		/*
		 * Error - give up.
		 */
		return (-1);
	}
	if (curdev == NULL) {
		/*
		 * Device wasn't added because it can't be opened.
		 * Not a fatal error.
		 */
		return (0);
	}

	/*
	 * "curdev" is an entry for this interface; add an entry for this
	 * address to its list of addresses.
	 *
	 * Allocate the new entry and fill it in.
	 */
	curaddr = malloc(sizeof(pcap_addr_t));
	if (curaddr == NULL) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "malloc: %s", pcap_strerror(errno));
		return (-1);
	}

	curaddr->next = NULL;
	if (addr != NULL) {
		curaddr->addr = dup_sockaddr(addr);
		if (curaddr->addr == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			free(curaddr);
			return (-1);
		}
	} else
		curaddr->addr = NULL;

	if (netmask != NULL) {
		curaddr->netmask = dup_sockaddr(netmask);
		if (curaddr->netmask == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			free(curaddr);
			return (-1);
		}
	} else
		curaddr->netmask = NULL;
		
	if (broadaddr != NULL) {
		curaddr->broadaddr = dup_sockaddr(broadaddr);
		if (curaddr->broadaddr == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			free(curaddr);
			return (-1);
		}
	} else
		curaddr->broadaddr = NULL;
		
	if (dstaddr != NULL) {
		curaddr->dstaddr = dup_sockaddr(dstaddr);
		if (curaddr->dstaddr == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			free(curaddr);
			return (-1);
		}
	} else
		curaddr->dstaddr = NULL;
		
	/*
	 * Find the end of the list of addresses.
	 */
	for (prevaddr = curdev->addresses; prevaddr != NULL; prevaddr = nextaddr) {
		nextaddr = prevaddr->next;
		if (nextaddr == NULL) {
			/*
			 * This is the end of the list.
			 */
			break;
		}
	}

	if (prevaddr == NULL) {
		/*
		 * The list was empty; this is the first member.
		 */
		curdev->addresses = curaddr;
	} else {
		/*
		 * "prevaddr" is the last member of the list; append
		 * this member to it.
		 */
		prevaddr->next = curaddr;
	}

	return (0);
}

static int
pcap_add_if(pcap_if_t **devlist, char *name, u_int flags,
    const char *description, char *errbuf)
{
	pcap_if_t *curdev;

	return (add_or_find_if(&curdev, devlist, name, flags, description,
	    errbuf));
}

/*
 * Get a list of all interfaces that are up and that we can open.
 * Returns -1 on error, 0 otherwise.
 * The list, as returned through "alldevsp", may be null if no interfaces
 * were up and could be opened.
 */
#ifdef HAVE_IFADDRS_H
int
pcap_findalldevs(pcap_if_t **alldevsp, char *errbuf)
{
	pcap_if_t *devlist = NULL;
	struct ifaddrs *ifap, *ifa;
	struct sockaddr *broadaddr, *dstaddr;
	int ret = 0;

	/*
	 * Get the list of interface addresses.
	 *
	 * Note: this won't return information about interfaces
	 * with no addresses; are there any such interfaces
	 * that would be capable of receiving packets?
	 * (Interfaces incapable of receiving packets aren't
	 * very interesting from libpcap's point of view.)
	 *
	 * LAN interfaces will probably have link-layer
	 * addresses; I don't know whether all implementations
	 * of "getifaddrs()" now, or in the future, will return
	 * those.
	 */
	if (getifaddrs(&ifap) != 0) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "getifaddrs: %s", pcap_strerror(errno));
		return (-1);
	}
	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		/*
		 * Is this interface up?
		 */
		if (!(ifa->ifa_flags & IFF_UP)) {
			/*
			 * No, so don't add it to the list.
			 */
			continue;
		}

		/*
		 * "ifa_broadaddr" may be non-null even on
		 * non-broadcast interfaces; "ifa_dstaddr"
		 * was, on at least one FreeBSD 4.1 system,
		 * non-null on a non-point-to-point
		 * interface.
		 */
		if (ifa->ifa_flags & IFF_BROADCAST)
			broadaddr = ifa->ifa_broadaddr;
		else
			broadaddr = NULL;
		if (ifa->ifa_flags & IFF_POINTOPOINT)
			dstaddr = ifa->ifa_dstaddr;
		else
			dstaddr = NULL;

		/*
		 * Add information for this address to the list.
		 */
		if (add_addr_to_iflist(&devlist, ifa->ifa_name,
		    ifa->ifa_flags, ifa->ifa_addr, ifa->ifa_netmask,
		    broadaddr, dstaddr, errbuf) < 0) {
			ret = -1;
			break;
		}
	}

	freeifaddrs(ifap);

	if (ret != -1) {
		/*
		 * We haven't had any errors yet; add the "any" device,
		 * if we can open it.
		 */
		if (pcap_add_if(&devlist, "any", 0, any_descr, errbuf) < 0)
			ret = -1;
	}

	if (ret == -1) {
		/*
		 * We had an error; free the list we've been constructing.
		 */
		if (devlist != NULL) {
			pcap_freealldevs(devlist);
			devlist = NULL;
		}
	}

	*alldevsp = devlist;
	return (ret);
}
#else /* HAVE_IFADDRS_H */
#ifdef HAVE_PROC_NET_DEV
/*
 * Get from "/proc/net/dev" all interfaces listed there; if they're
 * already in the list of interfaces we have, that won't add another
 * instance, but if they're not, that'll add them.
 *
 * We don't bother getting any addresses for them; it appears you can't
 * use SIOCGIFADDR on Linux to get IPv6 addresses for interfaces, and,
 * although some other types of addresses can be fetched with SIOCGIFADDR,
 * we don't bother with them for now.
 *
 * We also don't fail if we couldn't open "/proc/net/dev"; we just leave
 * the list of interfaces as is.
 */
static int
scan_proc_net_dev(pcap_if_t **devlistp, int fd, char *errbuf)
{
	FILE *proc_net_f;
	char linebuf[512];
	int linenum;
	unsigned char *p;
	char name[512];	/* XXX - pick a size */
	char *q, *saveq;
	struct ifreq ifrflags;
	int ret = 0;

	proc_net_f = fopen("/proc/net/dev", "r");
	if (proc_net_f == NULL)
		return (0);

	for (linenum = 1;
	    fgets(linebuf, sizeof linebuf, proc_net_f) != NULL; linenum++) {
		/*
		 * Skip the first two lines - they're headers.
		 */
		if (linenum <= 2)
			continue;

		p = &linebuf[0];

		/*
		 * Skip leading white space.
		 */
		while (*p != '\0' && isspace(*p))
			p++;
		if (*p == '\0' || *p == '\n')
			continue;	/* blank line */

		/*
		 * Get the interface name.
		 */
		q = &name[0];
		while (*p != '\0' && !isspace(*p)) {
			if (*p == ':') {
				/*
				 * This could be the separator between a
				 * name and an alias number, or it could be
				 * the separator between a name with no 
				 * alias number and the next field.
				 *
				 * If there's a colon after digits, it
				 * separates the name and the alias number,
				 * otherwise it separates the name and the
				 * next field.
				 */
				saveq = q;
				while (isdigit(*p))
					*q++ = *p++;
				if (*p != ':') {
					/*
					 * That was the next field,
					 * not the alias number.
					 */
					q = saveq;
				}
				break;
			} else
				*q++ = *p++;
		}
		*q = '\0';

		/*
		 * Get the flags for this interface, and skip it if
		 * it's not up.
		 */
		strncpy(ifrflags.ifr_name, name, sizeof(ifrflags.ifr_name));
		if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifrflags) < 0) {
			if (errno == ENXIO)
				continue;
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "SIOCGIFFLAGS: %.*s: %s",
			    (int)sizeof(ifrflags.ifr_name),
			    ifrflags.ifr_name,
			    pcap_strerror(errno));
			ret = -1;
			break;
		}
		if (!(ifrflags.ifr_flags & IFF_UP))
			continue;

		/*
		 * Add an entry for this interface, with no addresses.
		 */
		if (pcap_add_if(devlistp, name, ifrflags.ifr_flags, NULL,
		    errbuf) == -1) {
			/*
			 * Failure.
			 */
			ret = -1;
			break;
		}
	}
	if (ret != -1) {
		/*
		 * Well, we didn't fail for any other reason; did we
		 * fail due to an error reading the file?
		 */
		if (ferror(proc_net_f)) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "Error reading /proc/net/dev: %s",
			    pcap_strerror(errno));
			ret = -1;
		}
	}

	(void)fclose(proc_net_f);
	return (ret);
}
#endif /* HAVE_PROC_NET_DEV */

int
pcap_findalldevs(pcap_if_t **alldevsp, char *errbuf)
{
	pcap_if_t *devlist = NULL;
	register int fd;
	register struct ifreq *ifrp, *ifend, *ifnext;
	int n;
	struct ifconf ifc;
	char *buf = NULL;
	unsigned buf_size;
	struct ifreq ifrflags, ifrnetmask, ifrbroadaddr, ifrdstaddr;
	struct sockaddr *netmask, *broadaddr, *dstaddr;
	int ret = 0;

	/*
	 * Create a socket from which to fetch the list of interfaces.
	 */
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "socket: %s", pcap_strerror(errno));
		return (-1);
	}

	/*
	 * Start with an 8K buffer, and keep growing the buffer until
	 * we get the entire interface list or fail to get it for some
	 * reason other than EINVAL (which is presumed here to mean
	 * "buffer is too small").
	 */
	buf_size = 8192;
	for (;;) {
		buf = malloc(buf_size);
		if (buf == NULL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "malloc: %s", pcap_strerror(errno));
			(void)close(fd);
			return (-1);
		}

		ifc.ifc_len = buf_size;
		ifc.ifc_buf = buf;
		memset(buf, 0, buf_size);
		if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0
		    && errno != EINVAL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "SIOCGIFCONF: %s", pcap_strerror(errno));
			(void)close(fd);
			free(buf);
			return (-1);
		}
		if (ifc.ifc_len < buf_size)
			break;
		free(buf);
		buf_size *= 2;
	}

	ifrp = (struct ifreq *)buf;
	ifend = (struct ifreq *)(buf + ifc.ifc_len);

	for (; ifrp < ifend; ifrp = ifnext) {
		n = SA_LEN(&ifrp->ifr_addr) + sizeof(ifrp->ifr_name);
		if (n < sizeof(*ifrp))
			ifnext = ifrp + 1;
		else
			ifnext = (struct ifreq *)((char *)ifrp + n);

		/*
		 * Get the flags for this interface, and skip it if it's
		 * not up.
		 */
		strncpy(ifrflags.ifr_name, ifrp->ifr_name,
		    sizeof(ifrflags.ifr_name));
		if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifrflags) < 0) {
			if (errno == ENXIO)
				continue;
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "SIOCGIFFLAGS: %.*s: %s",
			    (int)sizeof(ifrflags.ifr_name),
			    ifrflags.ifr_name,
			    pcap_strerror(errno));
			ret = -1;
			break;
		}
		if (!(ifrflags.ifr_flags & IFF_UP))
			continue;

		/*
		 * Get the netmask for this address on this interface.
		 */
		strncpy(ifrnetmask.ifr_name, ifrp->ifr_name,
		    sizeof(ifrnetmask.ifr_name));
		memcpy(&ifrnetmask.ifr_addr, &ifrp->ifr_addr,
		    sizeof(ifrnetmask.ifr_addr));
		if (ioctl(fd, SIOCGIFNETMASK, (char *)&ifrnetmask) < 0) {
			if (errno == EADDRNOTAVAIL) {
				/*
				 * Not available.
				 */
				netmask = NULL;
			} else {
				(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
				    "SIOCGIFNETMASK: %.*s: %s",
				    (int)sizeof(ifrnetmask.ifr_name),
				    ifrnetmask.ifr_name,
				    pcap_strerror(errno));
				ret = -1;
				break;
			}
		} else
			netmask = &ifrnetmask.ifr_addr;

		/*
		 * Get the broadcast address for this address on this
		 * interface (if any).
		 */
		if (ifrflags.ifr_flags & IFF_BROADCAST) {
			strncpy(ifrbroadaddr.ifr_name, ifrp->ifr_name,
			    sizeof(ifrbroadaddr.ifr_name));
			memcpy(&ifrbroadaddr.ifr_addr, &ifrp->ifr_addr,
			    sizeof(ifrbroadaddr.ifr_addr));
			if (ioctl(fd, SIOCGIFBRDADDR,
			    (char *)&ifrbroadaddr) < 0) {
				if (errno == EADDRNOTAVAIL) {
					/*
					 * Not available.
					 */
					broadaddr = NULL;
				} else {
					(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
					    "SIOCGIFBRDADDR: %.*s: %s",
					    (int)sizeof(ifrbroadaddr.ifr_name),
					    ifrbroadaddr.ifr_name,
					    pcap_strerror(errno));
					ret = -1;
					break;
				}
			} else
				broadaddr = &ifrbroadaddr.ifr_broadaddr;
		} else {
			/*
			 * Not a broadcast interface, so no broadcast
			 * address.
			 */
			broadaddr = NULL;
		}

		/*
		 * Get the destination address for this address on this
		 * interface (if any).
		 */
		if (ifrflags.ifr_flags & IFF_POINTOPOINT) {
			strncpy(ifrdstaddr.ifr_name, ifrp->ifr_name,
			    sizeof(ifrdstaddr.ifr_name));
			memcpy(&ifrdstaddr.ifr_addr, &ifrp->ifr_addr,
			    sizeof(ifrdstaddr.ifr_addr));
			if (ioctl(fd, SIOCGIFDSTADDR,
			    (char *)&ifrdstaddr) < 0) {
				if (errno == EADDRNOTAVAIL) {
					/*
					 * Not available.
					 */
					dstaddr = NULL;
				} else {
					(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
					    "SIOCGIFDSTADDR: %.*s: %s",
					    (int)sizeof(ifrdstaddr.ifr_name),
					    ifrdstaddr.ifr_name,
					    pcap_strerror(errno));
					ret = -1;
					break;
				}
			} else
				dstaddr = &ifrdstaddr.ifr_dstaddr;
		} else
			dstaddr = NULL;

		/*
		 * Add information for this address to the list.
		 */
		if (add_addr_to_iflist(&devlist, ifrp->ifr_name,
		    ifrflags.ifr_flags, &ifrp->ifr_addr,
		    netmask, broadaddr, dstaddr, errbuf) < 0) {
			ret = -1;
			break;
		}
	}
	free(buf);

#ifdef HAVE_PROC_NET_DEV
	if (ret != -1) {
		/*
		 * We haven't had any errors yet; now read "/proc/net/dev",
		 * and add to the list of interfaces all interfaces listed
		 * there that we don't already have, because, on Linux,
		 * SIOCGIFCONF reports only interfaces with IPv4 addresses,
		 * so you need to read "/proc/net/dev" to get the names of
		 * the rest of the interfaces.
		 */
		ret = scan_proc_net_dev(&devlist, fd, errbuf);
	}
#endif
	(void)close(fd);

	if (ret != -1) {
		/*
		 * We haven't had any errors yet; add the "any" device,
		 * if we can open it.
		 */
		if (pcap_add_if(&devlist, "any", 0, any_descr, errbuf) < 0) {
			/*
			 * Oops, we had a fatal error.
			 */
			ret = -1;
		}
	}

	if (ret == -1) {
		/*
		 * We had an error; free the list we've been constructing.
		 */
		if (devlist != NULL) {
			pcap_freealldevs(devlist);
			devlist = NULL;
		}
	}

	*alldevsp = devlist;
	return (ret);
}
#endif /* HAVE_IFADDRS_H */

/*
 * Free a list of interfaces.
 */
void
pcap_freealldevs(pcap_if_t *alldevs)
{
	pcap_if_t *curdev, *nextdev;
	pcap_addr_t *curaddr, *nextaddr;

	for (curdev = alldevs; curdev != NULL; curdev = nextdev) {
		nextdev = curdev->next;

		/*
		 * Free all addresses.
		 */
		for (curaddr = curdev->addresses; curaddr != NULL; curaddr = nextaddr) {
			nextaddr = curaddr->next;
			if (curaddr->addr)
				free(curaddr->addr);
			if (curaddr->netmask)
				free(curaddr->netmask);
			if (curaddr->broadaddr)
				free(curaddr->broadaddr);
			if (curaddr->dstaddr)
				free(curaddr->dstaddr);
			free(curaddr);
		}

		/*
		 * Free the name string.
		 */
		free(curdev->name);

		/*
		 * Free the description string, if any.
		 */
		if (curdev->description != NULL)
			free(curdev->description);

		/*
		 * Free the interface.
		 */
		free(curdev);
	}
}

/*
 * Return the name of a network interface attached to the system, or NULL
 * if none can be found.  The interface must be configured up; the
 * lowest unit number is preferred; loopback is ignored.
 */
char *
pcap_lookupdev(errbuf)
	register char *errbuf;
{
	pcap_if_t *alldevs;
/* for old BSD systems, including bsdi3 */
#ifndef IF_NAMESIZE
#define IF_NAMESIZE IFNAMSIZ
#endif
	static char device[IF_NAMESIZE + 1];
	char *ret;

	if (pcap_findalldevs(&alldevs, errbuf) == -1)
		return (NULL);
	
	if (alldevs == NULL || (alldevs->flags & PCAP_IF_LOOPBACK)) {
		/*
		 * There are no devices on the list, or the first device
		 * on the list is a loopback device, which means there
		 * are no non-loopback devices on the list.  This means
		 * we can't return any device.
		 *
		 * XXX - why not return a loopback device?  If we can't
		 * capture on it, it won't be on the list, and if it's
		 * on the list, there aren't any non-loopback devices,
		 * so why not just supply it as the default device?
		 */
		(void)strlcpy(errbuf, "no suitable device found",
		    PCAP_ERRBUF_SIZE);
		ret = NULL;
	} else {
		/*
		 * Return the name of the first device on the list.
		 */
		(void)strlcpy(device, alldevs->name, sizeof(device));
		ret = device;
	}

	pcap_freealldevs(alldevs);
	return (ret);
}

int
pcap_lookupnet(device, netp, maskp, errbuf)
	register char *device;
	register bpf_u_int32 *netp, *maskp;
	register char *errbuf;
{
	register int fd;
	register struct sockaddr_in *sin;
	struct ifreq ifr;

	/* 
	 * The pseudo-device "any" listens on all interfaces and therefore
	 * has the network address and -mask "0.0.0.0" therefore catching
	 * all traffic. Using NULL for the interface is the same as "any".
	 */
	if (!device || strcmp(device, "any") == 0) {
		*netp = *maskp = 0;
		return 0;
	}

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE, "socket: %s",
		    pcap_strerror(errno));
		return (-1);
	}
	memset(&ifr, 0, sizeof(ifr));
#ifdef linux
	/* XXX Work around Linux kernel bug */
	ifr.ifr_addr.sa_family = AF_INET;
#endif
	(void)strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
	if (ioctl(fd, SIOCGIFADDR, (char *)&ifr) < 0) {
		if (errno == EADDRNOTAVAIL) {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "%s: no IPv4 address assigned", device);
		} else {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "SIOCGIFADDR: %s: %s",
			    device, pcap_strerror(errno));
		}
		(void)close(fd);
		return (-1);
	}
	sin = (struct sockaddr_in *)&ifr.ifr_addr;
	*netp = sin->sin_addr.s_addr;
	if (ioctl(fd, SIOCGIFNETMASK, (char *)&ifr) < 0) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "SIOCGIFNETMASK: %s: %s", device, pcap_strerror(errno));
		(void)close(fd);
		return (-1);
	}
	(void)close(fd);
	*maskp = sin->sin_addr.s_addr;
	if (*maskp == 0) {
		if (IN_CLASSA(*netp))
			*maskp = IN_CLASSA_NET;
		else if (IN_CLASSB(*netp))
			*maskp = IN_CLASSB_NET;
		else if (IN_CLASSC(*netp))
			*maskp = IN_CLASSC_NET;
		else {
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "inet class for 0x%x unknown", *netp);
			return (-1);
		}
	}
	*netp &= *maskp;
	return (0);
}
