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
    "@(#) $Header: /tcpdump/master/libpcap/inet.c,v 1.36 2000/09/20 15:10:29 torsten Exp $ (LBL)";
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
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif

#include "pcap-int.h"

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

/* Not all systems have IFF_LOOPBACK */
#ifdef IFF_LOOPBACK
#define ISLOOPBACK(p) ((p)->ifr_flags & IFF_LOOPBACK)
#define ISLOOPBACK_IFA(p) ((p)->ifa_flags & IFF_LOOPBACK)
#else
#define ISLOOPBACK(p) ((p)->ifr_name[0] == 'l' && (p)->ifr_name[1] == 'o' && \
    (isdigit((p)->ifr_name[2]) || (p)->ifr_name[2] == '\0'))
#define ISLOOPBACK_IFA(p) ((p)->ifa_name[0] == 'l' && (p)->ifa_name[1] == 'o' && \
    (isdigit((p)->ifa_name[2]) || (p)->ifa_name[2] == '\0'))
#endif

/*
 * Return the name of a network interface attached to the system, or NULL
 * if none can be found.  The interface must be configured up; the
 * lowest unit number is preferred; loopback is ignored.
 */
char *
pcap_lookupdev(errbuf)
	register char *errbuf;
{
#ifdef HAVE_IFADDRS_H
	struct ifaddrs *ifap, *ifa, *mp;
	int n, minunit;
	char *cp;
	static char device[IF_NAMESIZE + 1];

	if (getifaddrs(&ifap) != 0) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "getifaddrs: %s", pcap_strerror(errno));
		return NULL;
	}

	mp = NULL;
	minunit = 666;
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		const char *endcp;

		if ((ifa->ifa_flags & IFF_UP) == 0 || ISLOOPBACK_IFA(ifa))
			continue;

		endcp = ifa->ifa_name + strlen(ifa->ifa_name);
		for (cp = ifa->ifa_name; cp < endcp && !isdigit(*cp); ++cp)
			continue;

		if (isdigit (*cp)) {
			n = atoi(cp);
		} else {
			n = 0;
		}
		if (n < minunit) {
			minunit = n;
			mp = ifa;
		}
	}
	if (mp == NULL) {
		(void)strlcpy(errbuf, "no suitable device found",
		    PCAP_ERRBUF_SIZE);
#ifdef HAVE_FREEIFADDRS
		freeifaddrs(ifap);
#else
		free(ifap);
#endif
		return (NULL);
	}

	(void)strlcpy(device, mp->ifa_name, sizeof(device));
#ifdef HAVE_FREEIFADDRS
	freeifaddrs(ifap);
#else
	free(ifap);
#endif
	return (device);
#else
	register int fd, minunit, n;
	register char *cp;
	register struct ifreq *ifrp, *ifend, *ifnext, *mp;
	struct ifconf ifc;
	char *buf;
	struct ifreq ifr;
	static char device[sizeof(ifrp->ifr_name) + 1];
	unsigned buf_size;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
		    "socket: %s", pcap_strerror(errno));
		return (NULL);
	}

	buf_size = 8192;

	for (;;) {
		buf = malloc (buf_size);
		if (buf == NULL) {
			close (fd);
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "out of memory");
			return (NULL);
		}

		ifc.ifc_len = buf_size;
		ifc.ifc_buf = buf;
		memset (buf, 0, buf_size);
		if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0
		    && errno != EINVAL) {
			free (buf);
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "SIOCGIFCONF: %s", pcap_strerror(errno));
			(void)close(fd);
			return (NULL);
		}
		if (ifc.ifc_len < buf_size)
			break;
		free (buf);
		buf_size *= 2;
	}

	ifrp = (struct ifreq *)buf;
	ifend = (struct ifreq *)(buf + ifc.ifc_len);

	mp = NULL;
	minunit = 666;
	for (; ifrp < ifend; ifrp = ifnext) {
		const char *endcp;

#ifdef HAVE_SOCKADDR_SA_LEN
		n = ifrp->ifr_addr.sa_len + sizeof(ifrp->ifr_name);
		if (n < sizeof(*ifrp))
			ifnext = ifrp + 1;
		else
			ifnext = (struct ifreq *)((char *)ifrp + n);
		if (ifrp->ifr_addr.sa_family != AF_INET)
			continue;
#else
		ifnext = ifrp + 1;
#endif
		/*
		 * Need a template to preserve address info that is
		 * used below to locate the next entry.  (Otherwise,
		 * SIOCGIFFLAGS stomps over it because the requests
		 * are returned in a union.)
		 */
		strncpy(ifr.ifr_name, ifrp->ifr_name, sizeof(ifr.ifr_name));
		if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifr) < 0) {
			if (errno == ENXIO)
				continue;
			(void)snprintf(errbuf, PCAP_ERRBUF_SIZE,
			    "SIOCGIFFLAGS: %.*s: %s",
			    (int)sizeof(ifr.ifr_name), ifr.ifr_name,
			    pcap_strerror(errno));
			(void)close(fd);
			free (buf);
			return (NULL);
		}

		/* Must be up and not the loopback */
		if ((ifr.ifr_flags & IFF_UP) == 0 || ISLOOPBACK(&ifr))
			continue;

		endcp = ifrp->ifr_name + strlen(ifrp->ifr_name);
		for (cp = ifrp->ifr_name; cp < endcp && !isdigit(*cp); ++cp)
			continue;
		
		if (isdigit (*cp)) {
			n = atoi(cp);
		} else {
			n = 0;
		}
		if (n < minunit) {
			minunit = n;
			mp = ifrp;
		}
	}
	(void)close(fd);
	if (mp == NULL) {
		(void)strlcpy(errbuf, "no suitable device found",
		    PCAP_ERRBUF_SIZE);
		free(buf);
		return (NULL);
	}

	(void)strlcpy(device, mp->ifr_name, sizeof(device));
	free(buf);
	return (device);
#endif
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
