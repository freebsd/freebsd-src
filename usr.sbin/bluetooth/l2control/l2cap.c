/*
 * l2cap.c
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
 * $Id: l2cap.c,v 1.6 2002/09/04 21:30:40 max Exp $
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <bitstring.h>
#include <errno.h>
#include <ng_hci.h>
#include <ng_l2cap.h>
#include <ng_btsocket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "l2control.h"

#define	SIZE(x)	(sizeof((x))/sizeof((x)[0]))

/* Send read_node_flags command to the node */
static int
l2cap_read_node_flags(int s, int argc, char **argv)
{
	struct ng_btsocket_l2cap_raw_node_flags	r;

	memset(&r, 0, sizeof(r));
	if (ioctl(s, SIOC_L2CAP_NODE_GET_FLAGS, &r, sizeof(r)) < 0)
		return (ERROR);

	fprintf(stdout, "BD_ADDR: %x:%x:%x:%x:%x:%x\n",
		r.src.b[5], r.src.b[4], r.src.b[3],
		r.src.b[2], r.src.b[1], r.src.b[0]);
	fprintf(stdout, "Connectionless traffic flags:\n");
	fprintf(stdout, "\tSDP: %s\n",
		(r.flags & NG_L2CAP_CLT_SDP_DISABLED)? "disabled" : "enabled");
	fprintf(stdout, "\tRFCOMM: %s\n",
		(r.flags & NG_L2CAP_CLT_RFCOMM_DISABLED)? "disabled":"enabled");
	fprintf(stdout, "\tTCP: %s\n",
		(r.flags & NG_L2CAP_CLT_TCP_DISABLED)? "disabled" : "enabled");

	return (OK);
} /* l2cap_read_node_flags */

/* Send read_debug_level command to the node */
static int
l2cap_read_debug_level(int s, int argc, char **argv)
{
	struct ng_btsocket_l2cap_raw_node_debug	r;

	memset(&r, 0, sizeof(r));
	if (ioctl(s, SIOC_L2CAP_NODE_GET_DEBUG, &r, sizeof(r)) < 0)
		return (ERROR);

	fprintf(stdout, "BD_ADDR: %x:%x:%x:%x:%x:%x\n",
		r.src.b[5], r.src.b[4], r.src.b[3],
		r.src.b[2], r.src.b[1], r.src.b[0]);
	fprintf(stdout, "Debug level: %d\n", r.debug);

	return (OK);
} /* l2cap_read_debug_level */

/* Send write_debug_level command to the node */
static int
l2cap_write_debug_level(int s, int argc, char **argv)
{
	struct ng_btsocket_l2cap_raw_node_debug	r;

	memset(&r, 0, sizeof(r));
	switch (argc) {
	case 1:
		r.debug = atoi(argv[0]);
		break;

	default:
		return (USAGE);
	}

	if (ioctl(s, SIOC_L2CAP_NODE_SET_DEBUG, &r, sizeof(r)) < 0)
		return (ERROR);

	return (OK);
} /* l2cap_write_debug_level */

/* Send read_connection_list command to the node */
static int
l2cap_read_connection_list(int s, int argc, char **argv)
{
	static char const * const	state[] = {
		/* NG_L2CAP_CON_CLOSED */	"CLOSED",
		/* NG_L2CAP_W4_LP_CON_CFM */	"W4_LP_CON_CFM",
		/* NG_L2CAP_CON_OPEN */		"OPEN"
	};
#define con_state2str(x)	((x) >= SIZE(state)? "UNKNOWN" : state[(x)])

	struct ng_btsocket_l2cap_raw_con_list	r;
	int					n, error = OK;

	memset(&r, 0, sizeof(r));
	r.num_connections = NG_L2CAP_MAX_CON_NUM;
	r.connections = calloc(NG_L2CAP_MAX_CON_NUM,
				sizeof(ng_l2cap_node_con_ep));
	if (r.connections == NULL) {
		errno = ENOMEM;
		return (ERROR);
	}

	if (ioctl(s, SIOC_L2CAP_NODE_GET_CON_LIST, &r, sizeof(r)) < 0) {
		error = ERROR;
		goto out;
	}

	fprintf(stdout, "BD_ADDR: %x:%x:%x:%x:%x:%x\n",
		r.src.b[5], r.src.b[4], r.src.b[3],
		r.src.b[2], r.src.b[1], r.src.b[0]);
	fprintf(stdout, "L2CAP connections:\n");
	fprintf(stdout, 
"Remote BD_ADDR    Handle Flags Pending State\n");
	for (n = 0; n < r.num_connections; n++) {
		fprintf(stdout,
			"%02x:%02x:%02x:%02x:%02x:%02x " \
			" %5d " \
			"%2.2s %2.2s " \
			"%7d " \
			"%s\n",
			r.connections[n].remote.b[5],
			r.connections[n].remote.b[4],
			r.connections[n].remote.b[3],
			r.connections[n].remote.b[2],
			r.connections[n].remote.b[1],
			r.connections[n].remote.b[0],
			r.connections[n].con_handle, 
			((r.connections[n].flags & NG_L2CAP_CON_TX)? "TX" : ""),
			((r.connections[n].flags & NG_L2CAP_CON_RX)? "RX" : ""),
			r.connections[n].pending,
			con_state2str(r.connections[n].state));
	}
out:
	free(r.connections);

	return (error);
} /* l2cap_read_connection_list */

/* Send read_channel_list command to the node */
static int
l2cap_read_channel_list(int s, int argc, char **argv)
{
	static char const * const	state[] = {
		/* NG_L2CAP_CLOSED */			"CLOSED",
		/* NG_L2CAP_W4_L2CAP_CON_RSP */		"W4_L2CAP_CON_RSP",
		/* NG_L2CAP_W4_L2CA_CON_RSP */		"W4_L2CA_CON_RSP",
		/* NG_L2CAP_CONFIG */			"CONFIG",
		/* NG_L2CAP_OPEN */			"OPEN",
		/* NG_L2CAP_W4_L2CAP_DISCON_RSP */	"W4_L2CAP_DISCON_RSP",
		/* NG_L2CAP_W4_L2CA_DISCON_RSP */	"W4_L2CA_DISCON_RSP"
	};
#define ch_state2str(x)	((x) >= SIZE(state)? "UNKNOWN" : state[(x)])

	struct ng_btsocket_l2cap_raw_chan_list	r;
	int					n, error = OK;

	memset(&r, 0, sizeof(r));
	r.num_channels = NG_L2CAP_MAX_CHAN_NUM;
	r.channels = calloc(NG_L2CAP_MAX_CHAN_NUM,
				sizeof(ng_l2cap_node_chan_ep));
	if (r.channels == NULL) {
		errno = ENOMEM;
		return (ERROR);
	}

	if (ioctl(s, SIOC_L2CAP_NODE_GET_CHAN_LIST, &r, sizeof(r)) < 0) {
		error = ERROR;
		goto out;
	}

	fprintf(stdout, "BD_ADDR: %x:%x:%x:%x:%x:%x\n",
		r.src.b[5], r.src.b[4], r.src.b[3],
		r.src.b[2], r.src.b[1], r.src.b[0]);
	fprintf(stdout, "L2CAP channels:\n");
	fprintf(stdout, 
"Remote BD_ADDR     SCID/ DCID   PSM  IMTU/ OMTU State\n");
	for (n = 0; n < r.num_channels; n++) {
		fprintf(stdout,
			"%02x:%02x:%02x:%02x:%02x:%02x " \
			"%5d/%5d %5d " \
			"%5d/%5d " \
			"%s\n",
			r.channels[n].remote.b[5], r.channels[n].remote.b[4], 
			r.channels[n].remote.b[3], r.channels[n].remote.b[2], 
			r.channels[n].remote.b[1], r.channels[n].remote.b[0],
			r.channels[n].scid, r.channels[n].dcid,
			r.channels[n].psm, r.channels[n].imtu,
			r.channels[n].omtu,
			ch_state2str(r.channels[n].state));
	}
out:
	free(r.channels);

	return (error);
} /* l2cap_read_channel_list */

struct l2cap_command	l2cap_commands[] = {
{
"read_node_flags",
"Get L2CAP node flags",
&l2cap_read_node_flags
},
{
"read_debug_level",
"Get L2CAP node debug level",
&l2cap_read_debug_level
},
{
"write_debug_level <level>",
"Set L2CAP node debug level",
&l2cap_write_debug_level
},
{
"read_connection_list",
"Read list of the L2CAP connections",
&l2cap_read_connection_list
},
{
"read_channel_list",
"Read list of the L2CAP channels",
&l2cap_read_channel_list
},
{
NULL,
}};

