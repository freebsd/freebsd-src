/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

#ifndef _ASM_IA64_SN_IFCONFIG_NET_H
#define _ASM_IA64_SN_IFCONFIG_NET_H

#define NETCONFIG_FILE "/tmp/ifconfig_net"
#define POUND_CHAR                   '#'
#define MAX_LINE_LEN 128
#define MAXPATHLEN 128

struct ifname_num {
	long next_eth;
	long next_fddi;
	long next_hip;
	long next_tr;
	long next_fc;
	long size;
};

struct ifname_MAC {
	char name[16];
	unsigned char           dev_addr[7];
	unsigned char           addr_len;       /* hardware address length      */
};

#endif /* _ASM_IA64_SN_IFCONFIG_NET_H */
