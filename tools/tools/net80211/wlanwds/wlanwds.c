/*-
 * Copyright (c) 2006-2007 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD$
 */

/*
 * Test app to demonstrate how to handle dynamic WDS links:
 * o monitor 802.11 events for wds discovery events
 * o create wds vap's in response to wds discovery events
 *   and launch a script to handle adding the vap to the
 *   bridge, etc.
 * o destroy wds vap's when station leaves
 *
 * Note we query only internal state which means if we don't see
 * a vap created we won't handle leave/delete properly.  Also there
 * are several fixed pathnames/strings.  Some require fixing
 * kernel support (e.g. sysctl to find parent device of a vap).
 *
 * Code liberaly swiped from wlanwatch; probably should nuke printfs.
 */
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <net/if.h>
#include "net/if_media.h"
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netatalk/at.h>
#include "net80211/ieee80211_ioctl.h"
#include "net80211/ieee80211_freebsd.h"
#include <arpa/inet.h>
#include <netdb.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <ifaddrs.h>

#define	IEEE80211_ADDR_EQ(a1,a2)	(memcmp(a1,a2,IEEE80211_ADDR_LEN) == 0)
#define	IEEE80211_ADDR_COPY(dst,src)	memcpy(dst,src,IEEE80211_ADDR_LEN)

struct wds {
	struct wds *next;
	uint8_t	bssid[IEEE80211_ADDR_LEN];	/* bssid of associated sta */
	char	ifname[IFNAMSIZ];		/* vap interface name */
};
static struct wds *wds;

static	const char *bridge = "bridge0";
static	const char *parent = "mv0";		/* XXX no sysctl to find this */
static	const char *script = "/usr/local/bin/wdsup";
static	int verbose = 0;

static	void handle_rtmsg(struct rt_msghdr *rtm, int msglen);
static	void wds_discovery(const char *ifname,
		const uint8_t bssid[IEEE80211_ADDR_LEN]);
static	void wds_destroy(const char *ifname);
static	void wds_leave(const uint8_t bssid[IEEE80211_ADDR_LEN]);
static	int wds_vap_create(const char *ifname, struct wds *);
static	int wds_vap_destroy(const char *ifname);

int
main(int argc, char *argv[])
{
	int n, s, c;
	char msg[2048];

	while ((c = getopt(argc, argv, "b:p:s:vn")) != -1)
		switch (c) {
		case 'b':
			bridge = optarg;
			break;
		case 'p':
			parent = optarg;
			break;
		case 's':
			script = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
			errx(1, "usage: %s [-b <bridgename>] [-p <parentname>] [-s <set_scriptname>]\n"
				" [-v (for verbose)]\n", argv[0]);
			/*NOTREACHED*/
		}

	s = socket(PF_ROUTE, SOCK_RAW, 0);
	if (s < 0)
		err(EX_OSERR, "socket");
	for(;;) {
		n = read(s, msg, 2048);
		handle_rtmsg((struct rt_msghdr *)msg, n);
	}
	return 0;
}

static const char *
ether_sprintf(const uint8_t mac[6])
{
	static char buf[32];

	snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return buf;
}

static void
handle_rtmsg(struct rt_msghdr *rtm, int msglen)
{
	struct if_announcemsghdr *ifan;
	time_t now = time(NULL);
	char *cnow = ctime(&now);

	if (rtm->rtm_version != RTM_VERSION) {
		(void) printf("routing message version %d not understood\n",
		    rtm->rtm_version);
		return;
	}
	switch (rtm->rtm_type) {
	case RTM_IFANNOUNCE:
		ifan = (struct if_announcemsghdr *)rtm;
		if (!verbose)
			break;
		printf("%.19s RTM_IFANNOUNCE: if# %d, what: ",
			cnow, ifan->ifan_index);
		switch (ifan->ifan_what) {
		case IFAN_ARRIVAL:
			printf("arrival");
			break;
		case IFAN_DEPARTURE:
			printf("departure");
			wds_destroy(ifan->ifan_name);
			break;
		default:
			printf("#%d", ifan->ifan_what);
			break;
		}
		printf("\n");
		break;
	case RTM_IEEE80211:
#define	V(type)	((struct type *)(&ifan[1]))
		ifan = (struct if_announcemsghdr *)rtm;
		switch (ifan->ifan_what) {
		case RTM_IEEE80211_LEAVE:
			if (verbose)
				printf("%.19s %s station leave", cnow,
				    ether_sprintf(V(ieee80211_leave_event)->iev_addr));
			wds_leave(V(ieee80211_leave_event)->iev_addr);
			if (verbose)
				printf("\n");
			break;
		case RTM_IEEE80211_WDS:
			if (verbose)
				printf("%.19s %s wds discovery", cnow,
				    ether_sprintf(V(ieee80211_wds_event)->iev_addr));
			/* XXX wlan0 */
			wds_discovery("wlan0", V(ieee80211_wds_event)->iev_addr);
			if (verbose)
				printf("\n");
			break;
		case RTM_IEEE80211_ASSOC:
		case RTM_IEEE80211_REASSOC:
		case RTM_IEEE80211_DISASSOC:
		case RTM_IEEE80211_JOIN:
		case RTM_IEEE80211_REJOIN:
		case RTM_IEEE80211_SCAN:
		case RTM_IEEE80211_REPLAY:
		case RTM_IEEE80211_MICHAEL:
			break;
		default:
			if (verbose)
				printf("%.19s RTM_IEEE80211: if# %d, what: #%d\n", cnow,
					ifan->ifan_index, ifan->ifan_what);
			break;
		}
		break;
#undef V
	}
}

static void
wds_discovery(const char *ifname, const uint8_t bssid[IEEE80211_ADDR_LEN])
{
	struct wds *p;

	for (p = wds; p != NULL; p = p->next)
		if (IEEE80211_ADDR_EQ(p->bssid, bssid)) {
			if (verbose)
				printf(" (already created)");
			return;
		}
	p = malloc(sizeof(struct wds));
	if (p == NULL) {
		warn("%s: malloc", __func__);
		return;
	}
	IEEE80211_ADDR_COPY(p->bssid, bssid);
	/* XXX mv0: no sysctl to find parent device */
	if (wds_vap_create(parent, p) >= 0) {
		char cmd[1024];
		int status;

		/*
		 * Add to table.
		 */
		p->next = wds;
		wds = p;
		if (verbose)
			printf(" (create %s)", p->ifname);
		/*
		 * XXX launch script to setup bridge, etc.
		 */
		snprintf(cmd, sizeof(cmd), "%s %s %s",
			script, p->ifname, bridge);
		status = system(cmd);
		if (status)
			warnx("vap setup script %s exited with status %d\n",
				script, status);
	} else
		free(p);
}

static void
wds_destroy(const char *ifname)
{
	struct wds *p, **pp;

	for (pp = &wds; (p = *pp) != NULL; pp = &p->next)
		if (strncmp(p->ifname, ifname, IFNAMSIZ) == 0)
			break;
	/* XXX check for device directly */
	if (p == NULL)		/* not ours/known */
		return;
	*pp = p->next;
	if (wds_vap_destroy(p->ifname) >= 0)
		if (verbose)
			printf(" (wds vap destroyed)");
	free(p);
}

static void
wds_leave(const uint8_t bssid[IEEE80211_ADDR_LEN])
{
	struct wds *p, **pp;

	for (pp = &wds; (p = *pp) != NULL; pp = &p->next)
		if (IEEE80211_ADDR_EQ(p->bssid, bssid))
			break;
	/* XXX fall back to check device */
	if (p == NULL)		/* not ours/known */
		return;
	*pp = p->next;
	if (wds_vap_destroy(p->ifname) >= 0)
		printf(" (wds vap destroyed)");
	free(p);
}

static int
wds_vap_create(const char *parent, struct wds *p)
{
	struct ieee80211_clone_params cp;
	struct ifreq ifr;
	int s, status;

	memset(&cp, 0, sizeof(cp));
	strncpy(cp.icp_parent, parent, IFNAMSIZ);
	cp.icp_opmode = IEEE80211_M_WDS;
	IEEE80211_ADDR_COPY(cp.icp_bssid, p->bssid);

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, "wlan", IFNAMSIZ);
	ifr.ifr_data = (void *) &cp;

	status = -1;
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s >= 0) {
		if (ioctl(s, SIOCIFCREATE2, &ifr) >= 0) {
			strlcpy(p->ifname, ifr.ifr_name, IFNAMSIZ);
			status = 0;
		} else {
			warn("SIOCIFCREATE2("
			    "mode %u flags 0x%x parent %s bssid %s)",
			    cp.icp_opmode, cp.icp_flags, parent,
			    ether_sprintf(cp.icp_bssid));
		}
		close(s);
	} else
		warn("socket(SOCK_DRAGM)");
	return status;
}

static int
wds_vap_destroy(const char *ifname)
{
	struct ieee80211req ifr;
	int s, status;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		warn("socket(SOCK_DRAGM)");
		return -1;
	}
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.i_name, ifname, IFNAMSIZ);
	if (ioctl(s, SIOCIFDESTROY, &ifr) < 0) {
		warn("ioctl(SIOCIFDESTROY)");
		status = -1;
	} else
		status = 0;
	close(s);
	return status;
}
