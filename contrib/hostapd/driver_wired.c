/*
 * Host AP (software wireless LAN access point) user space daemon for
 * Host AP kernel driver / Kernel driver communication
 * Copyright (c) 2002-2005, Jouni Malinen <jkmaline@cc.hut.fi>
 * Copyright (c) 2004, Gunter Burchardt <tira@isx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>

#ifdef USE_KERNEL_HEADERS
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>   /* The L2 protocols */
#include <linux/if_arp.h>
#include <linux/if.h>
#else /* USE_KERNEL_HEADERS */
#include <net/if_arp.h>
#include <net/if.h>
#include <netpacket/packet.h>
#endif /* USE_KERNEL_HEADERS */

#include "hostapd.h"
#include "ieee802_1x.h"
#include "eloop.h"
#include "sta_info.h"
#include "driver.h"
#include "accounting.h"


struct wired_driver_data {
	struct driver_ops ops;
	struct hostapd_data *hapd;

	int sock; /* raw packet socket for driver access */
	int dhcp_sock; /* socket for dhcp packets */
	int use_pae_group_addr;
};

static const struct driver_ops wired_driver_ops;


#define WIRED_EAPOL_MULTICAST_GROUP	{0x01,0x80,0xc2,0x00,0x00,0x03}


/* TODO: detecting new devices should eventually be changed from using DHCP
 * snooping to trigger on any packet from a new layer 2 MAC address, e.g.,
 * based on ebtables, etc. */

struct dhcp_message {
	u_int8_t op;
	u_int8_t htype;
	u_int8_t hlen;
	u_int8_t hops;
	u_int32_t xid;
	u_int16_t secs;
	u_int16_t flags;
	u_int32_t ciaddr;
	u_int32_t yiaddr;
	u_int32_t siaddr;
	u_int32_t giaddr;
	u_int8_t chaddr[16];
	u_int8_t sname[64];
	u_int8_t file[128];
	u_int32_t cookie;
	u_int8_t options[308]; /* 312 - cookie */
};


static void wired_possible_new_sta(struct hostapd_data *hapd, u8 *addr)
{
	struct sta_info *sta;

	sta = ap_get_sta(hapd, addr);
	if (sta)
		return;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "Data frame from unknown STA "
		      MACSTR " - adding a new STA\n", MAC2STR(addr));
	sta = ap_sta_add(hapd, addr);
	if (sta) {
		hostapd_new_assoc_sta(hapd, sta, 0);
		accounting_sta_get_id(hapd, sta);
	} else {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "Failed to add STA entry "
			      "for " MACSTR "\n", MAC2STR(addr));
	}
}


static void handle_data(struct hostapd_data *hapd, char *buf, size_t len)
{
	struct ieee8023_hdr *hdr;
	u8 *pos, *sa;
	size_t left;

	/* must contain at least ieee8023_hdr 6 byte source, 6 byte dest,
	 * 2 byte ethertype */
	if (len < 14) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_VERBOSE, "handle_data: too short "
			      "(%lu)\n", (unsigned long) len);
		return;
	}

	hdr = (struct ieee8023_hdr *) buf;

	switch (ntohs(hdr->ethertype)) {
		case ETH_P_PAE:
			HOSTAPD_DEBUG(HOSTAPD_DEBUG_VERBOSE,
				      "Received EAPOL packet\n");
			sa = hdr->src;
			wired_possible_new_sta(hapd, sa);

			pos = (u8 *) (hdr + 1);
			left = len - sizeof(*hdr);

			ieee802_1x_receive(hapd, sa, pos, left);
		break;

	default:
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
			      "Unknown ethertype 0x%04x in data frame\n",
			      ntohs(hdr->ethertype));
		break;
	}
}


static void handle_read(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct hostapd_data *hapd = (struct hostapd_data *) eloop_ctx;
	int len;
	unsigned char buf[3000];

	len = recv(sock, buf, sizeof(buf), 0);
	if (len < 0) {
		perror("recv");
		return;
	}
	
	handle_data(hapd, buf, len);
}


static void handle_dhcp(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct hostapd_data *hapd = (struct hostapd_data *) eloop_ctx;
	int len;
	unsigned char buf[3000];
	struct dhcp_message *msg;
	u8 *mac_address;

	len = recv(sock, buf, sizeof(buf), 0);
	if (len < 0) {
		perror("recv"); 
		return;
	}

	/* must contain at least dhcp_message->chaddr */
	if (len < 44) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_VERBOSE, "handle_dhcp: too short "
			      "(%d)\n", len);
		return;
	}
	
	msg = (struct dhcp_message *) buf;
	mac_address = (u8 *) &(msg->chaddr);
	
	HOSTAPD_DEBUG(HOSTAPD_DEBUG_VERBOSE,
		      "Got DHCP broadcast packet from " MACSTR "\n",
		      MAC2STR(mac_address));

	wired_possible_new_sta(hapd, mac_address);
}


static int wired_init_sockets(struct wired_driver_data *drv)
{
	struct hostapd_data *hapd = drv->hapd;
	struct ifreq ifr;
	struct sockaddr_ll addr;
	struct sockaddr_in addr2;
	struct packet_mreq mreq;
	u8 multicastgroup_eapol[6] = WIRED_EAPOL_MULTICAST_GROUP;
	int n = 1;

	drv->sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_PAE));
	if (drv->sock < 0) {
		perror("socket[PF_PACKET,SOCK_RAW]");
		return -1;
	}

	if (eloop_register_read_sock(drv->sock, handle_read, hapd, NULL)) {
		printf("Could not register read socket\n");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s",
	hapd->conf->iface);
	if (ioctl(drv->sock, SIOCGIFINDEX, &ifr) != 0) {
		perror("ioctl(SIOCGIFINDEX)");
		return -1;
	}

	
	memset(&addr, 0, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_ifindex = ifr.ifr_ifindex;
	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "Opening raw packet socket for ifindex %d\n",
		      addr.sll_ifindex);

	if (bind(drv->sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind");
		return -1;
	}

	/* filter multicast address */
	memset(&mreq, 0, sizeof(mreq));
	mreq.mr_ifindex = ifr.ifr_ifindex;
	mreq.mr_type = PACKET_MR_MULTICAST;
	mreq.mr_alen = 6;
	memcpy(mreq.mr_address, multicastgroup_eapol, mreq.mr_alen);

	if (setsockopt(drv->sock, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq,
		       sizeof(mreq)) < 0) {
		perror("setsockopt[SOL_SOCKET,PACKET_ADD_MEMBERSHIP]");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", hapd->conf->iface);
	if (ioctl(drv->sock, SIOCGIFHWADDR, &ifr) != 0) {
		perror("ioctl(SIOCGIFHWADDR)");
		return -1;
	}

	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
		printf("Invalid HW-addr family 0x%04x\n",
		       ifr.ifr_hwaddr.sa_family);
		return -1;
	}
	memcpy(hapd->own_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	/* setup dhcp listen socket for sta detection */
	if ((drv->dhcp_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("socket call failed for dhcp");
		return -1;
	}

	if (eloop_register_read_sock(drv->dhcp_sock, handle_dhcp, hapd, NULL))
	{
		printf("Could not register read socket\n");
		return -1;
	}
	
	memset(&addr2, 0, sizeof(addr2));
	addr2.sin_family = AF_INET;
	addr2.sin_port = htons(67);
	addr2.sin_addr.s_addr = INADDR_ANY;

	if (setsockopt(drv->dhcp_sock, SOL_SOCKET, SO_REUSEADDR, (char *) &n,
		       sizeof(n)) == -1) {
		perror("setsockopt[SOL_SOCKET,SO_REUSEADDR]");
		return -1;
	}
	if (setsockopt(drv->dhcp_sock, SOL_SOCKET, SO_BROADCAST, (char *) &n,
		       sizeof(n)) == -1) {
		perror("setsockopt[SOL_SOCKET,SO_BROADCAST]");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_ifrn.ifrn_name, hapd->conf->iface, IFNAMSIZ);
	if (setsockopt(drv->dhcp_sock, SOL_SOCKET, SO_BINDTODEVICE,
		       (char *) &ifr, sizeof(ifr)) < 0) {
		perror("setsockopt[SOL_SOCKET,SO_BINDTODEVICE]");
		return -1;
	}

	if (bind(drv->dhcp_sock, (struct sockaddr *) &addr2,
		 sizeof(struct sockaddr)) == -1) {
		perror("bind");
		return -1;
	}

	return 0;
}


static int wired_send_eapol(void *priv, u8 *addr,
			    u8 *data, size_t data_len, int encrypt)
{
	struct wired_driver_data *drv = priv;
	u8 pae_group_addr[ETH_ALEN] = WIRED_EAPOL_MULTICAST_GROUP;
	struct ieee8023_hdr *hdr;
	size_t len;
	u8 *pos;
	int res;

	len = sizeof(*hdr) + data_len;
	hdr = malloc(len);
	if (hdr == NULL) {
		printf("malloc() failed for wired_send_eapol(len=%lu)\n",
		       (unsigned long) len);
		return -1;
	}

	memset(hdr, 0, len);
	memcpy(hdr->dest, drv->use_pae_group_addr ? pae_group_addr : addr,
	       ETH_ALEN);
	memcpy(hdr->src, drv->hapd->own_addr, ETH_ALEN);
	hdr->ethertype = htons(ETH_P_PAE);

	pos = (u8 *) (hdr + 1);
	memcpy(pos, data, data_len);

	res = send(drv->sock, (u8 *) hdr, len, 0);
	free(hdr);

	if (res < 0) {
		perror("wired_send_eapol: send");
		printf("wired_send_eapol - packet len: %lu - failed\n",
		       (unsigned long) len);
	}

	return res;
}


static int wired_driver_init(struct hostapd_data *hapd)
{
	struct wired_driver_data *drv;

	drv = malloc(sizeof(struct wired_driver_data));
	if (drv == NULL) {
		printf("Could not allocate memory for wired driver data\n");
		return -1;
	}

	memset(drv, 0, sizeof(*drv));
	drv->ops = wired_driver_ops;
	drv->hapd = hapd;
	drv->use_pae_group_addr = hapd->conf->use_pae_group_addr;

	if (wired_init_sockets(drv))
		return -1;

	hapd->driver = &drv->ops;
	return 0;
}


static void wired_driver_deinit(void *priv)
{
	struct wired_driver_data *drv = priv;

	drv->hapd->driver = NULL;

	if (drv->sock >= 0)
		close(drv->sock);
	
	if (drv->dhcp_sock >= 0)
		close(drv->dhcp_sock);

	free(drv);
}


static const struct driver_ops wired_driver_ops = {
	.name = "wired",
	.init = wired_driver_init,
	.deinit = wired_driver_deinit,
	.send_eapol = wired_send_eapol,
};

void wired_driver_register(void)
{
	driver_register(wired_driver_ops.name, &wired_driver_ops);
}
