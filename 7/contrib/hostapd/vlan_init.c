/*
 * hostapd / VLAN initialization
 * Copyright 2003, Instant802 Networks, Inc.
 * Copyright 2005-2006, Devicescape Software, Inc.
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

#include "includes.h"

#include "hostapd.h"
#include "driver.h"
#include "vlan_init.h"


#ifdef CONFIG_FULL_DYNAMIC_VLAN

#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <linux/if_vlan.h>
typedef __uint64_t __u64;
typedef __uint32_t __u32;
typedef __int32_t __s32;
typedef __uint16_t __u16;
typedef __int16_t __s16;
typedef __uint8_t __u8;
#include <linux/if_bridge.h>

#include "priv_netlink.h"
#include "eloop.h"


struct full_dynamic_vlan {
	int s; /* socket on which to listen for new/removed interfaces. */
};


static int ifconfig_helper(const char *if_name, int up)
{
	int fd;
	struct ifreq ifr;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket[AF_INET,SOCK_STREAM]");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	if (ioctl(fd, SIOCGIFFLAGS, &ifr) != 0) {
		perror("ioctl[SIOCGIFFLAGS]");
		close(fd);
		return -1;
	}

	if (up)
		ifr.ifr_flags |= IFF_UP;
	else
		ifr.ifr_flags &= ~IFF_UP;

	if (ioctl(fd, SIOCSIFFLAGS, &ifr) != 0) {
		perror("ioctl[SIOCSIFFLAGS]");
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}


static int ifconfig_up(const char *if_name)
{
	return ifconfig_helper(if_name, 1);
}


static int ifconfig_down(const char *if_name)
{
	return ifconfig_helper(if_name, 0);
}


/*
 * These are only available in recent linux headers (without the leading
 * underscore).
 */
#define _GET_VLAN_REALDEV_NAME_CMD	8
#define _GET_VLAN_VID_CMD		9

/* This value should be 256 ONLY. If it is something else, then hostapd
 * might crash!, as this value has been hard-coded in 2.4.x kernel
 * bridging code.
 */
#define MAX_BR_PORTS      		256

static int br_delif(const char *br_name, const char *if_name)
{
	int fd;
	struct ifreq ifr;
	unsigned long args[2];
	int if_index;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket[AF_INET,SOCK_STREAM]");
		return -1;
	}

	if_index = if_nametoindex(if_name);

	if (if_index == 0) {
		printf("Failure determining interface index for '%s'\n",
		       if_name);
		close(fd);
		return -1;
	}

	args[0] = BRCTL_DEL_IF;
	args[1] = if_index;

	strncpy(ifr.ifr_name, br_name, sizeof(ifr.ifr_name));
	ifr.ifr_data = (__caddr_t) args;

	if (ioctl(fd, SIOCDEVPRIVATE, &ifr) < 0 && errno != EINVAL) {
		/* No error if interface already removed. */
		perror("ioctl[SIOCDEVPRIVATE,BRCTL_DEL_IF]");
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}


/*
	Add interface 'if_name' to the bridge 'br_name'

	returns -1 on error
	returns 1 if the interface is already part of the bridge
	returns 0 otherwise
*/
static int br_addif(const char *br_name, const char *if_name)
{
	int fd;
	struct ifreq ifr;
	unsigned long args[2];
	int if_index;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket[AF_INET,SOCK_STREAM]");
		return -1;
	}

	if_index = if_nametoindex(if_name);

	if (if_index == 0) {
		printf("Failure determining interface index for '%s'\n",
		       if_name);
		close(fd);
		return -1;
	}

	args[0] = BRCTL_ADD_IF;
	args[1] = if_index;

	strncpy(ifr.ifr_name, br_name, sizeof(ifr.ifr_name));
	ifr.ifr_data = (__caddr_t) args;

	if (ioctl(fd, SIOCDEVPRIVATE, &ifr) < 0) {
		if (errno == EBUSY) {
			/* The interface is already added. */
			close(fd);
			return 1;
		}

		perror("ioctl[SIOCDEVPRIVATE,BRCTL_ADD_IF]");
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}


static int br_delbr(const char *br_name)
{
	int fd;
	unsigned long arg[2];

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket[AF_INET,SOCK_STREAM]");
		return -1;
	}

	arg[0] = BRCTL_DEL_BRIDGE;
	arg[1] = (unsigned long) br_name;

	if (ioctl(fd, SIOCGIFBR, arg) < 0 && errno != ENXIO) {
		/* No error if bridge already removed. */
		perror("ioctl[BRCTL_DEL_BRIDGE]");
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}


/*
	Add a bridge with the name 'br_name'.

	returns -1 on error
	returns 1 if the bridge already exists
	returns 0 otherwise
*/
static int br_addbr(const char *br_name)
{
	int fd;
	unsigned long arg[2];

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket[AF_INET,SOCK_STREAM]");
		return -1;
	}

	arg[0] = BRCTL_ADD_BRIDGE;
	arg[1] = (unsigned long) br_name;

	if (ioctl(fd, SIOCGIFBR, arg) < 0) {
 		if (errno == EEXIST) {
			/* The bridge is already added. */
			close(fd);
			return 1;
		} else {
			perror("ioctl[BRCTL_ADD_BRIDGE]");
			close(fd);
			return -1;
		}
	}

	close(fd);
	return 0;
}


static int br_getnumports(const char *br_name)
{
	int fd;
	int i;
	int port_cnt = 0;
	unsigned long arg[4];
	int ifindices[MAX_BR_PORTS];
	struct ifreq ifr;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket[AF_INET,SOCK_STREAM]");
		return -1;
	}

	arg[0] = BRCTL_GET_PORT_LIST;
	arg[1] = (unsigned long) ifindices;
	arg[2] = MAX_BR_PORTS;
	arg[3] = 0;

	memset(ifindices, 0, sizeof(ifindices));
	strncpy(ifr.ifr_name, br_name, sizeof(ifr.ifr_name));
	ifr.ifr_data = (__caddr_t) arg;

	if (ioctl(fd, SIOCDEVPRIVATE, &ifr) < 0) {
		perror("ioctl[SIOCDEVPRIVATE,BRCTL_GET_PORT_LIST]");
		close(fd);
		return -1;
	}

	for (i = 1; i < MAX_BR_PORTS; i++) {
		if (ifindices[i] > 0) {
			port_cnt++;
		}
	}

	close(fd);
	return port_cnt;
}


static int vlan_rem(const char *if_name)
{
	int fd;
	struct vlan_ioctl_args if_request;

	if ((strlen(if_name) + 1) > sizeof(if_request.device1)) {
		fprintf(stderr, "Interface name to long.\n");
		return -1;
	}

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket[AF_INET,SOCK_STREAM]");
		return -1;
	}

	memset(&if_request, 0, sizeof(if_request));

	strcpy(if_request.device1, if_name);
	if_request.cmd = DEL_VLAN_CMD;

	if (ioctl(fd, SIOCSIFVLAN, &if_request) < 0) {
		perror("ioctl[SIOCSIFVLAN,DEL_VLAN_CMD]");
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}


/*
	Add a vlan interface with VLAN ID 'vid' and tagged interface
	'if_name'.

	returns -1 on error
	returns 1 if the interface already exists
	returns 0 otherwise
*/
static int vlan_add(const char *if_name, int vid)
{
	int fd;
	struct vlan_ioctl_args if_request;

	ifconfig_up(if_name);

	if ((strlen(if_name) + 1) > sizeof(if_request.device1)) {
		fprintf(stderr, "Interface name to long.\n");
		return -1;
	}

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket[AF_INET,SOCK_STREAM]");
		return -1;
	}

	memset(&if_request, 0, sizeof(if_request));

	/* Determine if a suitable vlan device already exists. */

	snprintf(if_request.device1, sizeof(if_request.device1), "vlan%d",
		 vid);

	if_request.cmd = _GET_VLAN_VID_CMD;

	if (ioctl(fd, SIOCSIFVLAN, &if_request) == 0) {

		if (if_request.u.VID == vid) {
			if_request.cmd = _GET_VLAN_REALDEV_NAME_CMD;

			if (ioctl(fd, SIOCSIFVLAN, &if_request) == 0
			    && strncmp(if_request.u.device2, if_name,
				       sizeof(if_request.u.device2)) == 0) {
				close(fd);
				return 1;
			}
		}
	}

	/* A suitable vlan device does not already exist, add one. */

	memset(&if_request, 0, sizeof(if_request));
	strcpy(if_request.device1, if_name);
	if_request.u.VID = vid;
	if_request.cmd = ADD_VLAN_CMD;

	if (ioctl(fd, SIOCSIFVLAN, &if_request) < 0) {
		perror("ioctl[SIOCSIFVLAN,ADD_VLAN_CMD]");
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}


static int vlan_set_name_type(unsigned int name_type)
{
	int fd;
	struct vlan_ioctl_args if_request;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket[AF_INET,SOCK_STREAM]");
		return -1;
	}

	memset(&if_request, 0, sizeof(if_request));

	if_request.u.name_type = name_type;
	if_request.cmd = SET_VLAN_NAME_TYPE_CMD;
	if (ioctl(fd, SIOCSIFVLAN, &if_request) < 0) {
		perror("ioctl[SIOCSIFVLAN,SET_VLAN_NAME_TYPE_CMD]");
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}


static void vlan_newlink(char *ifname, struct hostapd_data *hapd)
{
	char vlan_ifname[IFNAMSIZ];
	char br_name[IFNAMSIZ];
	struct hostapd_vlan *vlan = hapd->conf->vlan;
	char *tagged_interface = hapd->conf->ssid.vlan_tagged_interface;

	while (vlan) {
		if (strcmp(ifname, vlan->ifname) == 0) {

			snprintf(br_name, sizeof(br_name), "brvlan%d",
				 vlan->vlan_id);

			if (!br_addbr(br_name))
				vlan->clean |= DVLAN_CLEAN_BR;

			ifconfig_up(br_name);

			if (tagged_interface) {

				if (!vlan_add(tagged_interface, vlan->vlan_id))
					vlan->clean |= DVLAN_CLEAN_VLAN;

				snprintf(vlan_ifname, sizeof(vlan_ifname),
					 "vlan%d", vlan->vlan_id);

				if (!br_addif(br_name, vlan_ifname))
					vlan->clean |= DVLAN_CLEAN_VLAN_PORT;

				ifconfig_up(vlan_ifname);
			}

			if (!br_addif(br_name, ifname))
				vlan->clean |= DVLAN_CLEAN_WLAN_PORT;

			ifconfig_up(ifname);

			break;
		}
		vlan = vlan->next;
	}
}


static void vlan_dellink(char *ifname, struct hostapd_data *hapd)
{
	char vlan_ifname[IFNAMSIZ];
	char br_name[IFNAMSIZ];
	struct hostapd_vlan *first, *prev, *vlan = hapd->conf->vlan;
	char *tagged_interface = hapd->conf->ssid.vlan_tagged_interface;
	int numports;

	first = prev = vlan;

	while (vlan) {
		if (strcmp(ifname, vlan->ifname) == 0) {
			snprintf(br_name, sizeof(br_name), "brvlan%d",
				 vlan->vlan_id);

			if (tagged_interface) {
				snprintf(vlan_ifname, sizeof(vlan_ifname),
					 "vlan%d", vlan->vlan_id);

				numports = br_getnumports(br_name);
				if (numports == 1) {
					br_delif(br_name, vlan_ifname);

					vlan_rem(vlan_ifname);

					ifconfig_down(br_name);
					br_delbr(br_name);
				}
			}

			if (vlan == first) {
				hapd->conf->vlan = vlan->next;
			} else {
				prev->next = vlan->next;
			}
			free(vlan);

			break;
		}
		prev = vlan;
		vlan = vlan->next;
	}
}


static void
vlan_read_ifnames(struct nlmsghdr *h, size_t len, int del,
		  struct hostapd_data *hapd)
{
	struct ifinfomsg *ifi;
	int attrlen, nlmsg_len, rta_len;
	struct rtattr *attr;

	if (len < sizeof(*ifi))
		return;

	ifi = NLMSG_DATA(h);

	nlmsg_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

	attrlen = h->nlmsg_len - nlmsg_len;
	if (attrlen < 0)
		return;

	attr = (struct rtattr *) (((char *) ifi) + nlmsg_len);

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		char ifname[IFNAMSIZ + 1];

		if (attr->rta_type == IFLA_IFNAME) {
			int n = attr->rta_len - rta_len;
			if (n < 0)
				break;

			memset(ifname, 0, sizeof(ifname));

			if ((size_t) n > sizeof(ifname))
				n = sizeof(ifname);
			memcpy(ifname, ((char *) attr) + rta_len, n);

			if (del)
				vlan_dellink(ifname, hapd);
			else
				vlan_newlink(ifname, hapd);
		}

		attr = RTA_NEXT(attr, attrlen);
	}
}


static void vlan_event_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	char buf[8192];
	int left;
	struct sockaddr_nl from;
	socklen_t fromlen;
	struct nlmsghdr *h;
	struct hostapd_data *hapd = eloop_ctx;

	fromlen = sizeof(from);
	left = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT,
			(struct sockaddr *) &from, &fromlen);
	if (left < 0) {
		if (errno != EINTR && errno != EAGAIN)
			perror("recvfrom(netlink)");
		return;
	}

	h = (struct nlmsghdr *) buf;
	while (left >= (int) sizeof(*h)) {
		int len, plen;

		len = h->nlmsg_len;
		plen = len - sizeof(*h);
		if (len > left || plen < 0) {
			printf("Malformed netlink message: "
			       "len=%d left=%d plen=%d", len, left, plen);
			break;
		}

		switch (h->nlmsg_type) {
		case RTM_NEWLINK:
			vlan_read_ifnames(h, plen, 0, hapd);
			break;
		case RTM_DELLINK:
			vlan_read_ifnames(h, plen, 1, hapd);
			break;
		}

		len = NLMSG_ALIGN(len);
		left -= len;
		h = (struct nlmsghdr *) ((char *) h + len);
	}

	if (left > 0) {
		printf("%d extra bytes in the end of netlink message",
		       left);
	}
}


static struct full_dynamic_vlan *
full_dynamic_vlan_init(struct hostapd_data *hapd)
{
	struct sockaddr_nl local;
	struct full_dynamic_vlan *priv;

	priv = malloc(sizeof(*priv));

	if (priv == NULL)
		return NULL;

	memset(priv, 0, sizeof(*priv));

	vlan_set_name_type(VLAN_NAME_TYPE_PLUS_VID_NO_PAD);

	priv->s = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (priv->s < 0) {
		perror("socket(PF_NETLINK,SOCK_RAW,NETLINK_ROUTE)");
		free(priv);
		return NULL;
	}

	memset(&local, 0, sizeof(local));
	local.nl_family = AF_NETLINK;
	local.nl_groups = RTMGRP_LINK;
	if (bind(priv->s, (struct sockaddr *) &local, sizeof(local)) < 0) {
		perror("bind(netlink)");
		close(priv->s);
		free(priv);
		return NULL;
	}

	if (eloop_register_read_sock(priv->s, vlan_event_receive, hapd, NULL))
	{
		close(priv->s);
		free(priv);
		return NULL;
	}

	return priv;
}


static void full_dynamic_vlan_deinit(struct full_dynamic_vlan *priv)
{
	if (priv == NULL)
		return;
	eloop_unregister_read_sock(priv->s);
	close(priv->s);
	free(priv);
}
#endif /* CONFIG_FULL_DYNAMIC_VLAN */


int vlan_setup_encryption_dyn(struct hostapd_data *hapd,
			      struct hostapd_ssid *mssid, const char *dyn_vlan)
{
        int i;

        if (dyn_vlan == NULL)
		return 0;

	/* Static WEP keys are set here; IEEE 802.1X and WPA uses their own
	 * functions for setting up dynamic broadcast keys. */
	for (i = 0; i < 4; i++) {
		if (mssid->wep.key[i] &&
		    hostapd_set_encryption(dyn_vlan, hapd, "WEP", NULL,
					   i, mssid->wep.key[i],
					   mssid->wep.len[i],
					   i == mssid->wep.idx)) {
			printf("VLAN: Could not set WEP encryption for "
			       "dynamic VLAN.\n");
			return -1;
		}
	}

	return 0;
}


static int vlan_dynamic_add(struct hostapd_data *hapd,
			    struct hostapd_vlan *vlan)
{
	while (vlan) {
		if (vlan->vlan_id != VLAN_ID_WILDCARD &&
		    hostapd_if_add(hapd, HOSTAPD_IF_VLAN, vlan->ifname, NULL))
		{
			if (errno != EEXIST) {
				printf("Could not add VLAN iface: %s: %s\n",
				       vlan->ifname, strerror(errno));
				return -1;
			}
		}

		vlan = vlan->next;
	}

	return 0;
}


static void vlan_dynamic_remove(struct hostapd_data *hapd,
				struct hostapd_vlan *vlan)
{
	struct hostapd_vlan *next;

	while (vlan) {
		next = vlan->next;

		if (vlan->vlan_id != VLAN_ID_WILDCARD &&
		    hostapd_if_remove(hapd, HOSTAPD_IF_VLAN, vlan->ifname,
				      NULL)) {
			printf("Could not remove VLAN iface: %s: %s\n",
			       vlan->ifname, strerror(errno));
		}
#ifdef CONFIG_FULL_DYNAMIC_VLAN
		if (vlan->clean)
			vlan_dellink(vlan->ifname, hapd);
#endif /* CONFIG_FULL_DYNAMIC_VLAN */

		vlan = next;
	}
}


int vlan_init(struct hostapd_data *hapd)
{
	if (vlan_dynamic_add(hapd, hapd->conf->vlan))
		return -1;

#ifdef CONFIG_FULL_DYNAMIC_VLAN
	hapd->full_dynamic_vlan = full_dynamic_vlan_init(hapd);
#endif /* CONFIG_FULL_DYNAMIC_VLAN */

        return 0;
}


void vlan_deinit(struct hostapd_data *hapd)
{
	vlan_dynamic_remove(hapd, hapd->conf->vlan);

#ifdef CONFIG_FULL_DYNAMIC_VLAN
	full_dynamic_vlan_deinit(hapd->full_dynamic_vlan);
#endif /* CONFIG_FULL_DYNAMIC_VLAN */
}


int vlan_reconfig(struct hostapd_data *hapd, struct hostapd_config *oldconf,
		  struct hostapd_bss_config *oldbss)
{
	vlan_dynamic_remove(hapd, oldbss->vlan);
	if (vlan_dynamic_add(hapd, hapd->conf->vlan))
		return -1;

	return 0;
}


struct hostapd_vlan * vlan_add_dynamic(struct hostapd_data *hapd,
				       struct hostapd_vlan *vlan,
				       int vlan_id)
{
	struct hostapd_vlan *n;
	char *ifname, *pos;

	if (vlan == NULL || vlan_id <= 0 || vlan_id > MAX_VLAN_ID ||
	    vlan->vlan_id != VLAN_ID_WILDCARD)
		return NULL;

	ifname = strdup(vlan->ifname);
	if (ifname == NULL)
		return NULL;
	pos = strchr(ifname, '#');
	if (pos == NULL) {
		free(ifname);
		return NULL;
	}
	*pos++ = '\0';

	n = malloc(sizeof(*n));
	if (n == NULL) {
		free(ifname);
		return NULL;
	}

	memset(n, 0, sizeof(*n));
	n->vlan_id = vlan_id;
	n->dynamic_vlan = 1;

	snprintf(n->ifname, sizeof(n->ifname), "%s%d%s", ifname, vlan_id, pos);
	free(ifname);

	if (hostapd_if_add(hapd, HOSTAPD_IF_VLAN, n->ifname, NULL)) {
		free(n);
		return NULL;
	}

	n->next = hapd->conf->vlan;
	hapd->conf->vlan = n;

	return n;
}


int vlan_remove_dynamic(struct hostapd_data *hapd, int vlan_id)
{
	struct hostapd_vlan *vlan;

	if (vlan_id <= 0 || vlan_id > MAX_VLAN_ID)
		return 1;

	vlan = hapd->conf->vlan;
	while (vlan) {
		if (vlan->vlan_id == vlan_id && vlan->dynamic_vlan > 0) {
			vlan->dynamic_vlan--;
			break;
		}
		vlan = vlan->next;
	}

	if (vlan == NULL)
		return 1;

	if (vlan->dynamic_vlan == 0)
		hostapd_if_remove(hapd, HOSTAPD_IF_VLAN, vlan->ifname, NULL);

	return 0;
}
