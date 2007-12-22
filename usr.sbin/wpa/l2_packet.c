/*
 * WPA Supplicant - Layer2 packet handling
 * Copyright (c) 2003-2004, Jouni Malinen <jkmaline@cc.hut.fi>
 * Copyright (c) 2005, Sam Leffler <sam@errno.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 *
 * $FreeBSD$
 */

/*
 * FreeBSD-specific implementation.
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pcap.h>

#include "common.h"
#include "eloop.h"
#include "l2_packet.h"

static const u8 pae_group_addr[ETH_ALEN] =
	{ 0x01, 0x80, 0xc2, 0x00, 0x00, 0x03 };

struct l2_packet_data {
	pcap_t *pcap;
	char ifname[100];
	u8 own_addr[ETH_ALEN];
	void (*rx_callback)(void *ctx, const u8 *src_addr,
			    const u8 *buf, size_t len);
	void *rx_callback_ctx;
	int l2_hdr; /* whether to include layer 2 (Ethernet) header data
		     * buffers */
};

int
l2_packet_get_own_addr(struct l2_packet_data *l2, u8 *addr)
{
	memcpy(addr, l2->own_addr, ETH_ALEN);
	return 0;
}

int
l2_packet_get_ip_addr(struct l2_packet_data *l2, char *buf, size_t len)
{
	pcap_if_t *devs, *dev;
	struct pcap_addr *addr;
	struct sockaddr_in *saddr;
	int found = 0;
	char err[PCAP_ERRBUF_SIZE + 1];

	if (pcap_findalldevs(&devs, err) < 0) {
		wpa_printf(MSG_DEBUG, "pcap_findalldevs: %s\n", err);
		return -1;
	}

	for (dev = devs; dev && !found; dev = dev->next) {
		if (strcmp(dev->name, l2->ifname) != 0)
			continue;

		addr = dev->addresses;
		while (addr) {
			saddr = (struct sockaddr_in *) addr->addr;
			if (saddr && saddr->sin_family == AF_INET) {
				snprintf(buf, len, "%s",
					 inet_ntoa(saddr->sin_addr));
				found = 1;
				break;
			}
			addr = addr->next;
		}
	}

	pcap_freealldevs(devs);

	return found ? 0 : -1;
}

void
l2_packet_notify_auth_start(struct l2_packet_data *l2)
{
}

int
l2_packet_send(struct l2_packet_data *l2,
	const u8 *dst_addr, u16 proto, const u8 *buf, size_t len)
{
	if (!l2->l2_hdr) {
		int ret;
		struct l2_ethhdr *eth = malloc(sizeof(*eth) + len);
		if (eth == NULL)
			return -1;
		memcpy(eth->h_dest, dst_addr, ETH_ALEN);
		memcpy(eth->h_source, l2->own_addr, ETH_ALEN);
		eth->h_proto = htons(proto);
		memcpy(eth + 1, buf, len);
		ret = pcap_inject(l2->pcap, (u8 *) eth, len + sizeof(*eth));
		free(eth);
		return ret;
	} else
		return pcap_inject(l2->pcap, buf, len);
}


static void
l2_packet_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct l2_packet_data *l2 = eloop_ctx;
	pcap_t *pcap = sock_ctx;
	struct pcap_pkthdr hdr;
	const u_char *packet;
	struct l2_ethhdr *ethhdr;
	unsigned char *buf;
	size_t len;

	packet = pcap_next(pcap, &hdr);

	if (packet == NULL || hdr.caplen < sizeof(*ethhdr))
		return;

	ethhdr = (struct l2_ethhdr *) packet;
	if (l2->l2_hdr) {
		buf = (unsigned char *) ethhdr;
		len = hdr.caplen;
	} else {
		buf = (unsigned char *) (ethhdr + 1);
		len = hdr.caplen - sizeof(*ethhdr);
	}
	l2->rx_callback(l2->rx_callback_ctx, ethhdr->h_source, buf, len);
}

static int
l2_packet_init_libpcap(struct l2_packet_data *l2, unsigned short protocol)
{
	bpf_u_int32 pcap_maskp, pcap_netp;
	char pcap_filter[200], pcap_err[PCAP_ERRBUF_SIZE];
	struct bpf_program pcap_fp;

	pcap_lookupnet(l2->ifname, &pcap_netp, &pcap_maskp, pcap_err);
	l2->pcap = pcap_open_live(l2->ifname, 2500, 0, 10, pcap_err);
	if (l2->pcap == NULL) {
		fprintf(stderr, "pcap_open_live: %s\n", pcap_err);
		fprintf(stderr, "ifname='%s'\n", l2->ifname);
		return -1;
	}
	if (pcap_datalink(l2->pcap) != DLT_EN10MB &&
	    pcap_set_datalink(l2->pcap, DLT_EN10MB) < 0) {
		fprintf(stderr, "pcap_set_datalink(DLT_EN10MB): %s\n",
			pcap_geterr(l2->pcap));
		return -1;
	}
	snprintf(pcap_filter, sizeof(pcap_filter),
		 "not ether src " MACSTR " and "
		 "( ether dst " MACSTR " or ether dst " MACSTR " ) and "
		 "ether proto 0x%x",
		 MAC2STR(l2->own_addr), /* do not receive own packets */
		 MAC2STR(l2->own_addr), MAC2STR(pae_group_addr),
		 protocol);
	if (pcap_compile(l2->pcap, &pcap_fp, pcap_filter, 1, pcap_netp) < 0) {
		fprintf(stderr, "pcap_compile: %s\n", pcap_geterr(l2->pcap));
		return -1;
	}

	if (pcap_setfilter(l2->pcap, &pcap_fp) < 0) {
		fprintf(stderr, "pcap_setfilter: %s\n", pcap_geterr(l2->pcap));
		return -1;
	}

	pcap_freecode(&pcap_fp);
	/*
	 * When libpcap uses BPF we must enable "immediate mode" to
	 * receive frames right away; otherwise the system may
	 * buffer them for us.
	 */
	{ unsigned int on = 1;
	  if (ioctl(pcap_fileno(l2->pcap), BIOCIMMEDIATE, &on) < 0) {
		fprintf(stderr, "%s: cannot enable immediate mode on "
			"interface %s: %s\n",
			__func__, l2->ifname, strerror(errno));
		/* XXX should we fail? */
	  }
	}

	eloop_register_read_sock(pcap_get_selectable_fd(l2->pcap),
				 l2_packet_receive, l2, l2->pcap);

	return 0;
}

static void
l2_packet_deinit_libpcap(struct l2_packet_data *l2)
{
	if (l2->pcap != NULL) {
		eloop_unregister_read_sock(pcap_get_selectable_fd(l2->pcap));
		pcap_close(l2->pcap);
		l2->pcap = NULL;
	}
}

static int
eth_get(const char *device, u8 ea[ETH_ALEN])
{
	struct if_msghdr *ifm;
	struct sockaddr_dl *sdl;
	u_char *p, *buf;
	size_t len;
	int mib[] = { CTL_NET, AF_ROUTE, 0, AF_LINK, NET_RT_IFLIST, 0 };

	if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0)
		return -1;
	if ((buf = malloc(len)) == NULL)
		return -1;
	if (sysctl(mib, 6, buf, &len, NULL, 0) < 0) {
		free(buf);
		return -1;
	}
	for (p = buf; p < buf + len; p += ifm->ifm_msglen) {
		ifm = (struct if_msghdr *)p;
		sdl = (struct sockaddr_dl *)(ifm + 1);
		if (ifm->ifm_type != RTM_IFINFO ||
		    (ifm->ifm_addrs & RTA_IFP) == 0)
			continue;
		if (sdl->sdl_family != AF_LINK || sdl->sdl_nlen == 0 ||
		    memcmp(sdl->sdl_data, device, sdl->sdl_nlen) != 0)
			continue;
		memcpy(ea, LLADDR(sdl), sdl->sdl_alen);
		break;
	}
	free(buf);

	if (p >= buf + len) {
		errno = ESRCH;
		return -1;
	}
	return 0;
}

struct l2_packet_data *
l2_packet_init(const char *ifname, const u8 *own_addr, unsigned short protocol,
	void (*rx_callback)(void *ctx, const u8 *src_addr,
			    const u8 *buf, size_t len),
	void *rx_callback_ctx, int l2_hdr)
{
	struct l2_packet_data *l2;

	l2 = malloc(sizeof(struct l2_packet_data));
	if (l2 == NULL)
		return NULL;
	memset(l2, 0, sizeof(*l2));
	strncpy(l2->ifname, ifname, sizeof(l2->ifname));
	l2->rx_callback = rx_callback;
	l2->rx_callback_ctx = rx_callback_ctx;
	l2->l2_hdr = l2_hdr;

	if (eth_get(l2->ifname, l2->own_addr) < 0) {
		fprintf(stderr, "Failed to get link-level address for "
			"interface '%s'.\n", l2->ifname);
		free(l2);
		return NULL;
	}

	if (l2_packet_init_libpcap(l2, protocol) != 0) {
		free(l2);
		return NULL;
	}
	return l2;
}

void
l2_packet_deinit(struct l2_packet_data *l2)
{
	if (l2 != NULL) {
		l2_packet_deinit_libpcap(l2);
		free(l2);
	}
}
