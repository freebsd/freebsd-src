/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: vpsctl.h 126 2013-04-07 15:55:54Z klaus $
 *
 */

#ifndef _VPSCTL_H
#define _VPSCTL_H

#define _PATH_VPSDEV	"/dev/vps"
#define _PATH_CONFDIR	"/etc/vps"

#define DEFAULTLEN	0x40

int priv_ston(const char *);
const char *priv_ntos(int);

struct vps_instinfo;
struct vps_arg_ifmove;
struct vps_arg_snapst;

struct vps_dumpinfo_ext {
        int magic;
        int version;
        time_t time;
        int size;
};

/*
 * This is a structure which holds the values set in config
 * files and is used only in the vpsctl command.
 */
struct epair_cf {
	struct epair_cf *next;
	int idx;
	int ifidx;
	char *ifconfig;
};
struct netif_addr {
	int af;
	const char *str;
	struct sockaddr *sock;
	union {
		struct in_addr in;
		struct in6_addr in6;
	} addr;
};
struct netif_cf {
	struct netif_cf *next;
	int idx;
	int ifidx;
	int ifaddr_cnt;
	struct netif_addr *ifaddr[10];
};
struct ip_network {
	int af;
	const char *str;
	union {
		struct in_addr in;
		struct in6_addr in6;
	} addr;
	union {
		struct in_addr in;
		/*struct in6_addr in6;*/
		u_int8_t in6;
	} mask;
};
struct vps_conf {
	char name[MAXHOSTNAMELEN];
	char fsroot[MAXPATHLEN];
	char fsroot_priv[MAXPATHLEN];
	char initproc[MAXPATHLEN];
	char cmd_mountroot[MAXPATHLEN];
	char cmd_unmountroot[MAXPATHLEN];
	char devfs_ruleset[DEFAULTLEN];
	char network_announce[DEFAULTLEN];
	char network_revoke[DEFAULTLEN];
	struct epair_cf *epair;
	struct netif_cf *netif;
	struct ip_network *ip_networks[10];
	int ip_networks_cnt;
	char *priv_allow;
	char *priv_nosys;
	char *priv_deny;
	char *limits;
};

#endif /* _VPSCTL_H */
