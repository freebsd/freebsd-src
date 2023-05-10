/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Isilon Systems, LLC.
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
 */

/*
 * netgdb.c
 * FreeBSD subsystem supporting debugging the FreeBSD kernel over the network.
 *
 * There are three pieces necessary to use NetGDB.
 *
 * First, a dedicated proxy server must be running to accept connections from
 * both NetGDB and gdb(1), and pass bidirectional traffic between the two
 * protocols.
 *
 * Second, The NetGDB client is activated much like ordinary 'gdb' and
 * similarly to 'netdump' in ddb(4).  Like other debugnet(4) clients
 * (netdump(4)), the network interface on the route to the proxy server must be
 * online and support debugnet(4).
 *
 * Finally, the remote (k)gdb(1) uses 'target remote <proxy>:<port>' to connect
 * to the proxy server.
 *
 * NetGDBv1 speaks the literal GDB remote serial protocol, and uses a 1:1
 * relationship between GDB packets and plain debugnet packets.  There is no
 * encryption utilized to keep debugging sessions private, so this is only
 * appropriate for local segments or trusted networks.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#ifndef DDB
#error "NetGDB cannot be used without DDB at this time"
#endif

#include <sys/param.h>
#include <sys/kdb.h>
#include <sys/sbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/ttydefaults.h>

#include <machine/gdb_machdep.h>

#ifdef DDB
#include <ddb/ddb.h>
#include <ddb/db_command.h>
#include <ddb/db_lex.h>
#endif

#include <net/debugnet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>

#include <gdb/gdb.h>
#include <gdb/gdb_int.h>
#include <gdb/netgdb.h>

FEATURE(netgdb, "NetGDB support");
SYSCTL_NODE(_debug_gdb, OID_AUTO, netgdb, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
    "NetGDB parameters");

static unsigned netgdb_debug;
SYSCTL_UINT(_debug_gdb_netgdb, OID_AUTO, debug, CTLFLAG_RWTUN,
    &netgdb_debug, 0,
    "Debug message verbosity (0: off; 1: on)");

#define	NETGDB_DEBUG(f, ...) do {						\
	if (netgdb_debug > 0)							\
		printf(("%s [%s:%d]: " f), __func__, __FILE__, __LINE__, ##	\
		    __VA_ARGS__);						\
} while (false)

static void netgdb_fini(void);

/* Runtime state. */
static char netgdb_rxbuf[GDB_BUFSZ + 16];	/* Some overhead for framing. */
static struct sbuf netgdb_rxsb;
static ssize_t netgdb_rx_off;

static struct debugnet_pcb *netgdb_conn;
static struct gdb_dbgport *netgdb_prev_dbgport;
static int *netgdb_prev_kdb_inactive;

/* TODO(CEM) disable ack mode */

/*
 * Receive non-TX ACK packets on the client port.
 *
 * The mbuf chain will have all non-debugnet framing headers removed
 * (ethernet, inet, udp).  It will start with a debugnet_msg_hdr, of
 * which the header is guaranteed to be contiguous.  If m_pullup is
 * used, the supplied in-out mbuf pointer should be updated
 * appropriately.
 *
 * If the handler frees the mbuf chain, it should set the mbuf pointer
 * to NULL.  Otherwise, the debugnet input framework will free the
 * chain.
 */
static void
netgdb_rx(struct debugnet_pcb *pcb, struct mbuf **mb)
{
	const struct debugnet_msg_hdr *dnh;
	struct mbuf *m;
	uint32_t rlen, count;
	int error;

	m = *mb;
	dnh = mtod(m, const void *);

	if (ntohl(dnh->mh_type) == DEBUGNET_FINISHED) {
		sbuf_putc(&netgdb_rxsb, CTRL('C'));
		return;
	}

	if (ntohl(dnh->mh_type) != DEBUGNET_DATA) {
		printf("%s: Got unexpected debugnet message %u\n",
		    __func__, ntohl(dnh->mh_type));
		return;
	}

	rlen = ntohl(dnh->mh_len);
#define	_SBUF_FREESPACE(s)	((s)->s_size - ((s)->s_len + 1))
	if (_SBUF_FREESPACE(&netgdb_rxsb) < rlen) {
		NETGDB_DEBUG("Backpressure: Not ACKing RX of packet that "
		    "would overflow our buffer (%zd/%zd used).\n",
		    netgdb_rxsb.s_len, netgdb_rxsb.s_size);
		return;
	}
#undef _SBUF_FREESPACE

	error = debugnet_ack_output(pcb, dnh->mh_seqno);
	if (error != 0) {
		printf("%s: Couldn't ACK rx packet %u; %d\n", __func__,
		    ntohl(dnh->mh_seqno), error);
		/*
		 * Sender will re-xmit, and assuming the condition is
		 * transient, we'll process the packet's contentss later.
		 */
		return;
	}

	m_adj(m, sizeof(*dnh));
	dnh = NULL;

	/*
	 * Inlined m_apply -- why isn't there a macro or inline function
	 * version?
	 */
	while (m != NULL && m->m_len == 0)
		m = m->m_next;
	while (rlen > 0) {
		MPASS(m != NULL && m->m_len >= 0);
		count = min((uint32_t)m->m_len, rlen);
		(void)sbuf_bcat(&netgdb_rxsb, mtod(m, const void *), count);
		rlen -= count;
		m = m->m_next;
	}
}

/*
 * The following routines implement a pseudo GDB debugport (an emulated serial
 * driver that the MI gdb(4) code does I/O with).
 */

static int
netgdb_dbg_getc(void)
{
	int c;

	while (true) {
		/* Pull bytes off any currently cached packet first. */
		if (netgdb_rx_off < sbuf_len(&netgdb_rxsb)) {
			c = netgdb_rxsb.s_buf[netgdb_rx_off];
			netgdb_rx_off++;
			break;
		}

		/* Reached EOF?  Reuse buffer. */
		sbuf_clear(&netgdb_rxsb);
		netgdb_rx_off = 0;

		/* Check for CTRL-C on console/serial, if any. */
		if (netgdb_prev_dbgport != NULL) {
			c = netgdb_prev_dbgport->gdb_getc();
			if (c == CTRL('C'))
				break;
		}

		debugnet_network_poll(netgdb_conn);
	}

	if (c == CTRL('C')) {
		netgdb_fini();
		/* Caller gdb_getc() will print that we got ^C. */
	}
	return (c);
}

static void
netgdb_dbg_sendpacket(const void *buf, size_t len)
{
	struct debugnet_proto_aux aux;
	int error;

	MPASS(len <= UINT32_MAX);

	/*
	 * GDB packet boundaries matter.  debugnet_send() fragments a single
	 * request into many sequential debugnet messages.  Mark full packet
	 * length and offset for potential reassembly by the proxy.
	 */
	aux = (struct debugnet_proto_aux) {
		.dp_aux2 = len,
	};

	error = debugnet_send(netgdb_conn, DEBUGNET_DATA, buf, len, &aux);
	if (error != 0) {
		printf("%s: Network error: %d; trying to switch back to ddb.\n",
		    __func__, error);
		netgdb_fini();

		if (kdb_dbbe_select("ddb") != 0)
			printf("The ddb backend could not be selected.\n");
		else {
			printf("using longjmp, hope it works!\n");
			kdb_reenter();
		}
	}

}

/* Just used for + / - GDB-level ACKs. */
static void
netgdb_dbg_putc(int i)
{
	char c;

	c = i;
	netgdb_dbg_sendpacket(&c, 1);

}

static struct gdb_dbgport netgdb_gdb_dbgport = {
	.gdb_name = "netgdb",
	.gdb_getc = netgdb_dbg_getc,
	.gdb_putc = netgdb_dbg_putc,
	.gdb_term = netgdb_fini,
	.gdb_sendpacket = netgdb_dbg_sendpacket,
	.gdb_dbfeatures = GDB_DBGP_FEAT_WANTTERM | GDB_DBGP_FEAT_RELIABLE,
};

static void
netgdb_init(void)
{
	struct kdb_dbbe *be, **iter;

	/*
	 * Force enable GDB.  (If no other debugports were registered at boot,
	 * KDB thinks it doesn't exist.)
	 */
	SET_FOREACH(iter, kdb_dbbe_set) {
		be = *iter;
		if (strcmp(be->dbbe_name, "gdb") != 0)
			continue;
		if (be->dbbe_active == -1) {
			netgdb_prev_kdb_inactive = &be->dbbe_active;
			be->dbbe_active = 0;
		}
		break;
	}

	/* Force netgdb debugport. */
	netgdb_prev_dbgport = gdb_cur;
	gdb_cur = &netgdb_gdb_dbgport;

	sbuf_new(&netgdb_rxsb, netgdb_rxbuf, sizeof(netgdb_rxbuf),
	    SBUF_FIXEDLEN);
	netgdb_rx_off = 0;
}

static void
netgdb_fini(void)
{

	/* TODO: tear down conn gracefully? */
	if (netgdb_conn != NULL) {
		debugnet_free(netgdb_conn);
		netgdb_conn = NULL;
	}

	sbuf_delete(&netgdb_rxsb);

	gdb_cur = netgdb_prev_dbgport;

	if (netgdb_prev_kdb_inactive != NULL) {
		*netgdb_prev_kdb_inactive = -1;
		netgdb_prev_kdb_inactive = NULL;
	}
}

#ifdef DDB
/*
 * Usage: netgdb -s <server> [-g <gateway -c <localip> -i <interface>]
 *
 * Order is not significant.
 *
 * Currently, this command does not support configuring encryption or
 * compression.
 */
DB_COMMAND_FLAGS(netgdb, db_netgdb_cmd, CS_OWN)
{
	struct debugnet_ddb_config params;
	struct debugnet_conn_params dcp;
	struct debugnet_pcb *pcb;
	int error;

	if (!KERNEL_PANICKED()) {
		/* TODO: This limitation should be removed in future work. */
		printf("%s: netgdb is currently limited to use only after a "
		    "panic.  Sorry.\n", __func__);
		return;
	}

	error = debugnet_parse_ddb_cmd("netgdb", &params);
	if (error != 0) {
		db_printf("Error configuring netgdb: %d\n", error);
		return;
	}

	/*
	 * Must initialize netgdb_rxsb before debugnet_connect(), because we
	 * might be getting rx handler callbacks from the send->poll path
	 * during debugnet_connect().
	 */
	netgdb_init();

	if (!params.dd_has_client)
		params.dd_client = INADDR_ANY;
	if (!params.dd_has_gateway)
		params.dd_gateway = INADDR_ANY;

	dcp = (struct debugnet_conn_params) {
		.dc_ifp = params.dd_ifp,
		.dc_client = params.dd_client,
		.dc_server = params.dd_server,
		.dc_gateway = params.dd_gateway,
		.dc_herald_port = NETGDB_HERALDPORT,
		.dc_client_port = NETGDB_CLIENTPORT,
		.dc_herald_aux2 = NETGDB_PROTO_V1,
		.dc_rx_handler = netgdb_rx,
	};

	error = debugnet_connect(&dcp, &pcb);
	if (error != 0) {
		printf("failed to contact netgdb server: %d\n", error);
		netgdb_fini();
		return;
	}

	netgdb_conn = pcb;

	if (kdb_dbbe_select("gdb") != 0) {
		db_printf("The remote GDB backend could not be selected.\n");
		netgdb_fini();
		return;
	}

	/*
	 * Mark that we are done in ddb(4).  Return -> kdb_trap() should
	 * re-enter with the new backend.
	 */
	db_cmd_loop_done = 1;
	gdb_return_to_ddb = true;
	db_printf("(detaching GDB will return control to DDB)\n");
#if 0
	/* Aspirational, but does not work reliably. */
	db_printf("(ctrl-c will return control to ddb)\n");
#endif
}
#endif /* DDB */
