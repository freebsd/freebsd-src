/*
 * btsockstat.c
 *
 * Copyright (c) 2001-2002 Maksim Yevmenkin <m_evmenkin@yahoo.com>
 * All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: btsockstat.c,v 1.2 2002/09/16 19:40:14 max Exp $
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/callout.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/if_var.h>

#include <bitstring.h>
#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>

#include <ng_hci.h>
#include <ng_l2cap.h>
#include <ng_btsocket.h>
#include <ng_btsocket_hci_raw.h>
#include <ng_btsocket_l2cap.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void	hcirawpr   (kvm_t *kvmd, u_long addr);
static void	l2caprawpr (kvm_t *kvmd, u_long addr);
static void	l2cappr    (kvm_t *kvmd, u_long addr);
static void	l2caprtpr  (kvm_t *kvmd, u_long addr);

static kvm_t *	kopen      (char const *memf);
static int	kread      (kvm_t *kvmd, u_long addr, char *buffer, int size);

static void	usage      (void);

/*
 * List of symbols
 */

static struct nlist	nl[] = {
#define N_HCI_RAW	0
	{ "_ng_btsocket_hci_raw_sockets" },
#define N_L2CAP_RAW	1
	{ "_ng_btsocket_l2cap_raw_sockets" },
#define N_L2CAP		2
	{ "_ng_btsocket_l2cap_sockets" },
#define N_L2CAP_RAW_RT	3
	{ "_ng_btsocket_l2cap_raw_rt" },
#define N_L2CAP_RT	4
	{ "_ng_btsocket_l2cap_rt" },
	{ "" },
};

/*
 * Main
 */

int
main(int argc, char *argv[])
{
	int	 opt, proto = -1, route = 0;
	kvm_t	*kvmd = NULL;
	char	*memf = NULL;

	while ((opt = getopt(argc, argv, "hM:p:r")) != -1) {
		switch (opt) {
		case 'M':
			memf = optarg;
			break;

		case 'p':
			if (strcasecmp(optarg, "hci_raw") == 0)
				proto = N_HCI_RAW;
			else if (strcasecmp(optarg, "l2cap_raw") == 0)
				proto = N_L2CAP_RAW;
			else if (strcasecmp(optarg, "l2cap") == 0)
				proto = N_L2CAP;
			else
				usage();
				/* NOT REACHED */
			break;

		case 'r':
			route = 1;
			break;

		case 'h':
		default:
			usage();
			/* NOT REACHED */
		}
	}

	if (proto == N_HCI_RAW && route)
		usage();
		/* NOT REACHED */

	kvmd = kopen(memf);
	if (kvmd == NULL)
		return (1);

	switch (proto) {
	case N_HCI_RAW:
		hcirawpr(kvmd, nl[N_HCI_RAW].n_value);
		break;

	case N_L2CAP_RAW:
		if (route)
			l2caprtpr(kvmd, nl[N_L2CAP_RAW_RT].n_value);
		else
			l2caprawpr(kvmd, nl[N_L2CAP_RAW].n_value);
		break;

	case N_L2CAP:
		if (route) 
			l2caprtpr(kvmd, nl[N_L2CAP_RT].n_value);
		else
			l2cappr(kvmd, nl[N_L2CAP].n_value);
		break;

	default:
		if (route) {
			l2caprtpr(kvmd, nl[N_L2CAP_RAW_RT].n_value);
			l2caprtpr(kvmd, nl[N_L2CAP_RT].n_value);
		} else {
			hcirawpr(kvmd, nl[N_HCI_RAW].n_value);
			l2caprawpr(kvmd, nl[N_L2CAP_RAW].n_value);
			l2cappr(kvmd, nl[N_L2CAP].n_value);
		}
		break;
	}

	return (kvm_close(kvmd));
} /* main */

/*
 * Print raw HCI sockets
 */

static void
hcirawpr(kvm_t *kvmd, u_long addr)
{
	ng_btsocket_hci_raw_pcb_p	this = NULL, next = NULL;
	ng_btsocket_hci_raw_pcb_t	pcb;
	struct socket			so;
	int				first = 1;

	if (addr == 0)
		return;

        if (kread(kvmd, addr, (char *) &this, sizeof(this)) < 0)
		return;

	for ( ; this != NULL; this = next) {
		if (kread(kvmd, (u_long) this, (char *) &pcb, sizeof(pcb)) < 0)
			return;
		if (kread(kvmd, (u_long) pcb.so, (char *) &so, sizeof(so)) < 0)
			return;

		next = LIST_NEXT(&pcb, next);

		if (first) {
			first = 0;
			fprintf(stdout,
"Active raw HCI sockets\n" \
"%-8.8s %-8.8s %-6.6s %-6.6s %-6.6s %-16.16s\n",
				"Socket",
				"PCB",
				"Flags",
				"Recv-Q",
				"Send-Q",
				"Local address");
		}

		if (pcb.addr.hci_node[0] == 0) {
			pcb.addr.hci_node[0] = '*';
			pcb.addr.hci_node[1] = 0;
		}

		fprintf(stdout,
"%-8.8x %-8.8x %-6.6x %6d %6d %-16.16s\n",
			(int) pcb.so,
			(int) this,
			pcb.flags,
			so.so_rcv.sb_cc,
			so.so_snd.sb_cc,
			pcb.addr.hci_node);
	}
} /* hcirawpr */

/*
 * Print raw L2CAP sockets
 */

static void
l2caprawpr(kvm_t *kvmd, u_long addr)
{
	ng_btsocket_l2cap_raw_pcb_p	this = NULL, next = NULL;
	ng_btsocket_l2cap_raw_pcb_t	pcb;
	struct socket			so;
	int				first = 1;
	char				bdaddr[32];

	if (addr == 0)
		return;

        if (kread(kvmd, addr, (char *) &this, sizeof(this)) < 0)
		return;

	for ( ; this != NULL; this = next) {
		if (kread(kvmd, (u_long) this, (char *) &pcb, sizeof(pcb)) < 0)
			return;
		if (kread(kvmd, (u_long) pcb.so, (char *) &so, sizeof(so)) < 0)
			return;

		next = LIST_NEXT(&pcb, next);

		if (first) {
			first = 0;
			fprintf(stdout, 
"Active raw L2CAP sockets\n" \
"%-8.8s %-8.8s %-6.6s %-6.6s %-18.18s\n",
				"Socket",
				"PCB",
				"Recv-Q",
				"Send-Q",
				"Local address");
		}

		if (memcmp(&pcb.src, NG_HCI_BDADDR_ANY, sizeof(pcb.src)) == 0) {
			bdaddr[0] = '*';
			bdaddr[1] = 0;
		} else
			snprintf(bdaddr, sizeof(bdaddr),
"%02x:%02x:%02x:%02x:%02x:%02x",
				pcb.src.b[5], pcb.src.b[4], pcb.src.b[3],
				pcb.src.b[2], pcb.src.b[1], pcb.src.b[0]);

		fprintf(stdout,
"%-8.8x %-8.8x %6d %6d %-18.18s\n",
			(int) pcb.so,
			(int) this,
			so.so_rcv.sb_cc,
			so.so_snd.sb_cc,
			bdaddr);
	}
} /* l2caprawpr */

/*
 * Print L2CAP sockets
 */

static void
l2cappr(kvm_t *kvmd, u_long addr)
{
	static char const * const	states[] = {
	/* NG_BTSOCKET_L2CAP_CLOSED */		"CLOSED",
	/* NG_BTSOCKET_L2CAP_CONNECTING */	"CON",
	/* NG_BTSOCKET_L2CAP_CONFIGURING */	"CONFIG",
	/* NG_BTSOCKET_L2CAP_OPEN */		"OPEN",
	/* NG_BTSOCKET_L2CAP_DISCONNECTING */	"DISCON"
	};
#define state2str(x) \
	(((x) >= sizeof(states)/sizeof(states[0]))? "UNKNOWN" : states[(x)])

	ng_btsocket_l2cap_pcb_p	this = NULL, next = NULL;
	ng_btsocket_l2cap_pcb_t	pcb;
	struct socket		so;
	int			first = 1;
	char			local[32], remote[32];


	if (addr == 0)
		return;

        if (kread(kvmd, addr, (char *) &this, sizeof(this)) < 0)
		return;

	for ( ; this != NULL; this = next) {
		if (kread(kvmd, (u_long) this, (char *) &pcb, sizeof(pcb)) < 0)
			return;
		if (kread(kvmd, (u_long) pcb.so, (char *) &so, sizeof(so)) < 0)
			return;

		next = LIST_NEXT(&pcb, next);

		if (first) {
			first = 0;
			fprintf(stdout,
"Active L2CAP sockets\n" \
"%-8.8s %-6.6s %-6.6s %-24.24s %-18.18s %-5.5s %s\n",
				"PCB",
				"Recv-Q",
				"Send-Q",
				"Local address/PSM",
				"Foreign address",
				"CID",
				"State");
		}

		if (memcmp(&pcb.src, NG_HCI_BDADDR_ANY, sizeof(pcb.src)) == 0)
			snprintf(local, sizeof(local), "*/%d", pcb.psm);
		else
			snprintf(local, sizeof(local),
"%02x:%02x:%02x:%02x:%02x:%02x/%d",
				pcb.src.b[5], pcb.src.b[4], pcb.src.b[3],
				pcb.src.b[2], pcb.src.b[1], pcb.src.b[0],
				pcb.psm);

		if (memcmp(&pcb.dst, NG_HCI_BDADDR_ANY, sizeof(pcb.dst)) == 0) {
			remote[0] = '*';
			remote[1] = 0;
		} else
			snprintf(remote, sizeof(remote),
"%02x:%02x:%02x:%02x:%02x:%02x",
				pcb.dst.b[5], pcb.dst.b[4], pcb.dst.b[3],
				pcb.dst.b[2], pcb.dst.b[1], pcb.dst.b[0]);

		fprintf(stdout,
"%-8.8x %6d %6d %-24.24s %-18.18s %-5d %s\n",
			(int) this,
			so.so_rcv.sb_cc,
			so.so_snd.sb_cc,
			local,
			remote,
			pcb.cid,
			(so.so_options & SO_ACCEPTCONN)?
				"LISTEN" : state2str(pcb.state));
	}
} /* l2cappr */

/*
 * Print L2CAP routing table
 */

static void
l2caprtpr(kvm_t *kvmd, u_long addr)
{
	ng_btsocket_l2cap_rtentry_p	this = NULL, next = NULL;
	ng_btsocket_l2cap_rtentry_t	rt;
	int				first = 1;
	char				bdaddr[32];

	if (addr == 0)
		return;

	if (kread(kvmd, addr, (char *) &this, sizeof(this)) < 0)
		return;

	for ( ; this != NULL; this = next) {
		if (kread(kvmd, (u_long) this, (char *) &rt, sizeof(rt)) < 0)
			return;

		next = LIST_NEXT(&rt, next);

		if (first) {
			first = 0;
			fprintf(stdout,
"Known %sL2CAP routes\n", (addr == nl[N_L2CAP_RAW_RT].n_value)?  "raw " : "");
			fprintf(stdout,
"%-8.8s %-8.8s %-18.18s\n",	"RTentry",
				"Hook",
				"BD_ADDR");
		}

		if (memcmp(&rt.src, NG_HCI_BDADDR_ANY, sizeof(rt.src)) == 0) {
			bdaddr[0] = '-';
			bdaddr[1] = 0;
		} else
			snprintf(bdaddr, sizeof(bdaddr),
"%02x:%02x:%02x:%02x:%02x:%02x", rt.src.b[5], rt.src.b[4], rt.src.b[3],
				 rt.src.b[2], rt.src.b[1], rt.src.b[0]);

		fprintf(stdout,
"%-8.8x %-8.8x %-18.18s\n",
			(int) this,
			(int) rt.hook,
			bdaddr);
	}
} /* l2caprtpr */

/*
 * Open kvm
 */

static kvm_t *
kopen(char const *memf)
{
	kvm_t	*kvmd = NULL;
	char	 errbuf[_POSIX2_LINE_MAX];

	/*
	 * Discard setgid privileges if not the running kernel so that 
	 * bad guys can't print interesting stuff from kernel memory.
	 */

	if (memf != NULL)
		setgid(getgid());   

	kvmd = kvm_openfiles(NULL, memf, NULL, O_RDONLY, errbuf);
	if (kvmd == NULL) {
		warnx("kvm_openfiles: %s", errbuf);
		return (NULL);
	}

	if (kvm_nlist(kvmd, nl) < 0) {
		warnx("kvm_nlist: %s", kvm_geterr(kvmd));
		goto fail;
	}

	if (nl[0].n_type == 0) {
		warnx("kvm_nlist: no namelist");
		goto fail;
	}

	return (kvmd);
fail:
	kvm_close(kvmd);

	return (NULL);
} /* kopen */

/*
 * Read kvm
 */

static int
kread(kvm_t *kvmd, u_long addr, char *buffer, int size)
{
	if (kvmd == NULL || buffer == NULL)
		return (-1);

	if (kvm_read(kvmd, addr, buffer, size) != size) {
		warnx("kvm_read: %s", kvm_geterr(kvmd));
		return (-1);
	}

	return (0);
} /* kread */

/*
 * Print usage and exit
 */

static void
usage(void)
{
	fprintf(stdout, "Usage: btsockstat [-M core ] [-p proto] [-r]\n");
	exit(255);
} /* usage */

