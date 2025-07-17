/*
 * Linux ioctl helper functions for driver wrappers
 * Copyright (c) 2002-2010, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>

#include "utils/common.h"
#include "common/linux_bridge.h"
#include "linux_ioctl.h"


int linux_set_iface_flags(int sock, const char *ifname, int dev_up)
{
	struct ifreq ifr;
	int ret;

	if (sock < 0)
		return -1;

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);

	if (ioctl(sock, SIOCGIFFLAGS, &ifr) != 0) {
		ret = errno ? -errno : -999;
		wpa_printf(MSG_ERROR, "Could not read interface %s flags: %s",
			   ifname, strerror(errno));
		return ret;
	}

	if (dev_up) {
		if (ifr.ifr_flags & IFF_UP)
			return 0;
		ifr.ifr_flags |= IFF_UP;
	} else {
		if (!(ifr.ifr_flags & IFF_UP))
			return 0;
		ifr.ifr_flags &= ~IFF_UP;
	}

	if (ioctl(sock, SIOCSIFFLAGS, &ifr) != 0) {
		ret = errno ? -errno : -999;
		wpa_printf(MSG_ERROR, "Could not set interface %s flags (%s): "
			   "%s",
			   ifname, dev_up ? "UP" : "DOWN", strerror(errno));
		return ret;
	}

	return 0;
}


int linux_iface_up(int sock, const char *ifname)
{
	struct ifreq ifr;
	int ret;

	if (sock < 0)
		return -1;

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);

	if (ioctl(sock, SIOCGIFFLAGS, &ifr) != 0) {
		ret = errno ? -errno : -999;
		wpa_printf(MSG_ERROR, "Could not read interface %s flags: %s",
			   ifname, strerror(errno));
		return ret;
	}

	return !!(ifr.ifr_flags & IFF_UP);
}


int linux_get_ifhwaddr(int sock, const char *ifname, u8 *addr)
{
	struct ifreq ifr;

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(sock, SIOCGIFHWADDR, &ifr)) {
		wpa_printf(MSG_ERROR, "Could not get interface %s hwaddr: %s",
			   ifname, strerror(errno));
		return -1;
	}

	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
		wpa_printf(MSG_ERROR, "%s: Invalid HW-addr family 0x%04x",
			   ifname, ifr.ifr_hwaddr.sa_family);
		return -1;
	}
	os_memcpy(addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	return 0;
}


int linux_set_ifhwaddr(int sock, const char *ifname, const u8 *addr)
{
	struct ifreq ifr;

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	os_memcpy(ifr.ifr_hwaddr.sa_data, addr, ETH_ALEN);
	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;

	if (ioctl(sock, SIOCSIFHWADDR, &ifr)) {
		wpa_printf(MSG_DEBUG, "Could not set interface %s hwaddr: %s",
			   ifname, strerror(errno));
		return -1;
	}

	return 0;
}


int linux_br_add(int sock, const char *brname)
{
	if (ioctl(sock, SIOCBRADDBR, brname) < 0) {
		int saved_errno = errno;

		wpa_printf(MSG_DEBUG, "Could not add bridge %s: %s",
			   brname, strerror(errno));
		errno = saved_errno;
		return -1;
	}

	return 0;
}


int linux_br_del(int sock, const char *brname)
{
	if (ioctl(sock, SIOCBRDELBR, brname) < 0) {
		wpa_printf(MSG_DEBUG, "Could not remove bridge %s: %s",
			   brname, strerror(errno));
		return -1;
	}

	return 0;
}


int linux_br_add_if(int sock, const char *brname, const char *ifname)
{
	struct ifreq ifr;
	int ifindex;

	ifindex = if_nametoindex(ifname);
	if (ifindex == 0)
		return -1;

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, brname, IFNAMSIZ);
	ifr.ifr_ifindex = ifindex;
	if (ioctl(sock, SIOCBRADDIF, &ifr) < 0) {
		int saved_errno = errno;
		char in_br[IFNAMSIZ];

		wpa_printf(MSG_DEBUG, "Could not add interface %s into bridge "
			   "%s: %s", ifname, brname, strerror(errno));
		errno = saved_errno;

		/* If ioctl() returns EBUSY when adding an interface into the
		 * bridge, the interface might have already been added by an
		 * external operation, so check whether the interface is
		 * currently on the right bridge and ignore the error if it is.
		 */
		if (errno != EBUSY || linux_br_get(in_br, ifname) != 0 ||
		    os_strcmp(in_br, brname) != 0)
			return -1;
	}

	return 0;
}


int linux_br_del_if(int sock, const char *brname, const char *ifname)
{
	struct ifreq ifr;
	int ifindex;

	ifindex = if_nametoindex(ifname);
	if (ifindex == 0)
		return -1;

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, brname, IFNAMSIZ);
	ifr.ifr_ifindex = ifindex;
	if (ioctl(sock, SIOCBRDELIF, &ifr) < 0) {
		wpa_printf(MSG_DEBUG, "Could not remove interface %s from "
			   "bridge %s: %s", ifname, brname, strerror(errno));
		return -1;
	}

	return 0;
}


int linux_br_get(char *brname, const char *ifname)
{
	char path[128], brlink[128], *pos;
	ssize_t res;

	os_snprintf(path, sizeof(path), "/sys/class/net/%s/brport/bridge",
		    ifname);
	res = readlink(path, brlink, sizeof(brlink));
	if (res < 0 || (size_t) res >= sizeof(brlink))
		return -1;
	brlink[res] = '\0';
	pos = os_strrchr(brlink, '/');
	if (pos == NULL)
		return -1;
	pos++;
	os_strlcpy(brname, pos, IFNAMSIZ);
	return 0;
}


int linux_master_get(char *master_ifname, const char *ifname)
{
	char buf[128], masterlink[128], *pos;
	ssize_t res;

	/* check whether there is a master */
	os_snprintf(buf, sizeof(buf), "/sys/class/net/%s/master", ifname);

	res = readlink(buf, masterlink, sizeof(masterlink));
	if (res < 0 || (size_t) res >= sizeof(masterlink))
		return -1;

	masterlink[res] = '\0';

	pos = os_strrchr(masterlink, '/');
	if (pos == NULL)
		return -1;
	pos++;
	os_strlcpy(master_ifname, pos, IFNAMSIZ);
	return 0;
}
