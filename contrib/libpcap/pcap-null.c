/*
 * Copyright (c) 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/libpcap/pcap-null.c,v 1.13 2000/10/28 00:01:29 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>			/* optionally get BSD define */

#include <string.h>

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#include "pcap-int.h"

static char nosup[] = "live packet capture not supported on this system";

int
pcap_stats(pcap_t *p, struct pcap_stat *ps)
{
	(void)snprintf(p->errbuf, sizeof(p->errbuf), "pcap_stats: %s", nosup);
	return (-1);
}

int
pcap_read(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	(void)snprintf(p->errbuf, sizeof(p->errbuf), "pcap_read: %s", nosup);
	return (-1);
}

pcap_t *
pcap_open_live(char *device, int snaplen, int promisc, int to_ms, char *ebuf)
{
	(void)strlcpy(ebuf, nosup, PCAP_ERRBUF_SIZE);
	return (NULL);
}

int
pcap_setfilter(pcap_t *p, struct bpf_program *fp)
{
	if (p->sf.rfile == NULL) {
		(void)snprintf(p->errbuf, sizeof(p->errbuf),
		    "pcap_setfilter: %s", nosup);
		return (-1);
	}
	if (install_bpf_program(p, fp) < 0)
		return (-1);
	return (0);
}
