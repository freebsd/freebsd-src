/*
 * Copyright (c) 2000 Lennert Buytenhek
 *
 * This software may be distributed either under the terms of the
 * BSD-style license that accompanies tcpdump or the GNU General
 * Public License
 *
 * Format and print IEEE 802.1d spanning tree protocol packets.
 * Contributed by Lennert Buytenhek <buytenh@gnu.org>
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-stp.c,v 1.13 2003/11/16 09:36:38 guy Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"

static void
stp_print_bridge_id(const u_char *p)
{
	printf("%.2x%.2x.%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
	       p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
}

static void
stp_print_config_bpdu(const u_char *p)
{
	printf("config ");
	if (p[7] & 1)
		printf("TOP_CHANGE ");
	if (p[7] & 0x80)
		printf("TOP_CHANGE_ACK ");

	stp_print_bridge_id(p+20);
	printf(".%.2x%.2x ", p[28], p[29]);

	printf("root ");
	stp_print_bridge_id(p+8);

	printf(" pathcost %i ", (p[16] << 24) | (p[17] << 16) | (p[18] << 8) | p[19]);

	printf("age %i ", p[30]);
	printf("max %i ", p[32]);
	printf("hello %i ", p[34]);
	printf("fdelay %i ", p[36]);
}

static void
stp_print_tcn_bpdu(void)
{
	printf("tcn");
}

/*
 * Print 802.1d packets.
 */
void
stp_print(const u_char *p, u_int length)
{
	if (length < 7)
		goto trunc;

	printf("802.1d ");
	if (p[2] != 0x03 || p[3] || p[4] || p[5]) {
		printf("unknown version");
		return;
	}

	switch (p[6])
	{
	case 0x00:
		if (length < 10)
			goto trunc;
		stp_print_config_bpdu(p);
		break;

	case 0x80:
		stp_print_tcn_bpdu();
		break;

	default:
		printf("unknown type %i", p[6]);
		break;
	}

	return;
trunc:
	printf("[|stp %d]", length);
}
