/*
 *   Copyright (c) 1998,1999 Martin Husemann. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the author nor the names of any co-contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *   4. Altered versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software and/or documentation.
 *   
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b daemon - network monitor protocol definition
 *	------------------------------------------------
 *
 *	$Id: monitor.h,v 1.16 1999/12/13 21:25:26 hm Exp $
 *
 * $FreeBSD: src/usr.sbin/i4b/isdnmonitor/monitor.h,v 1.6 1999/12/14 21:07:42 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 21:52:18 1999]
 *
 *---------------------------------------------------------------------------*/

#ifndef _MONITOR_H_
#define _MONITOR_H_

#define DEF_MONPORT     451             /* default monitor TCP port     */

#ifdef __hpux
#define	u_int8_t	ubit8
#define	u_int32_t	ubit32
#endif
#ifdef WIN32
#define	u_int8_t	BYTE
#define	u_int32_t	DWORD
#endif

/*
 * The monitor client connects to the isdnd daemon process via a tcp/ip
 * connection from a remote machine or via a local (unix domain) socket.
 * The daemon accepts multiple connections and verifies access rights.
 * On connection establishment the daemon sends initial data telling
 * the client the current configuration: number and type of available
 * controllers, current connections, channel and interface states
 * and the clients access privileges. The client sends an event mask
 * telling the daemon which events it is interested in. If the client
 * has appropriate rights he may send commands to the daemon.
 *
 * All multi-byte values are in network byte order!
 */

/* All data packets transfered are declared as arrays of u_int8_t */

/* max stringlength used in this protocol */
#define	I4B_MAX_MON_STRING		256

/* max command size from client to server */
#define	I4B_MAX_MON_CLIENT_CMD		16

/* Version of the monitor protocol described here */
#define	MPROT_VERSION			0	/* major version no */
#define	MPROT_REL			5	/* release no */

/*
 * Client access rights
 */
#define	I4B_CA_COMMAND_FULL		1	/* may send any command */
#define	I4B_CA_COMMAND_RESTRICTED	2	/* may send 'harmless' commands */
#define	I4B_CA_EVNT_CHANSTATE		16	/* may watch b-channel states */
#define	I4B_CA_EVNT_CALLIN		32	/* may watch incoming calls */
#define	I4B_CA_EVNT_CALLOUT		64	/* may watch outgoing calls */
#define	I4B_CA_EVNT_I4B			128	/* may watch isdnd actions */

/*
 * General layout of a command packet. All commands have this common
 * prefix. It is prepared by the macro I4B_PREP_CMD (s.b.)
 */
#define	I4B_MON_CMD			0	/* 2 byte: command code */
#define	I4B_MON_CMD_LEN			2	/* 2 byte: packet length */
#define	I4B_MON_CMD_HDR			4	/* size of header */

/*
 * Currently events look the same as commands. We do not make
 * any guarantee this will remain the same, so a different set
 * of macros is used when describing events. Events are prepared
 * by I4B_PREP_EVNT (s.b.)
 */
#define	I4B_MON_EVNT			0	/* 2 byte: event code */
#define	I4B_MON_EVNT_LEN		2	/* 2 byte: packet length */
#define	I4B_MON_EVNT_HDR		4	/* size of header */

/* Initial data send by daemon after connection is established */
#define	I4B_MON_IDATA_SIZE		I4B_MON_EVNT_HDR+12
#define	I4B_MON_IDATA_CODE		0			/* event code */
#define	I4B_MON_IDATA_VERSMAJOR		I4B_MON_EVNT_HDR+0	/* 2 byte: isdnd major version */
#define	I4B_MON_IDATA_VERSMINOR		I4B_MON_EVNT_HDR+2	/* 2 byte: isdnd minor version */
#define	I4B_MON_IDATA_NUMCTRL		I4B_MON_EVNT_HDR+4	/* 2 byte: number of controllers */
#define	I4B_MON_IDATA_NUMENTR		I4B_MON_EVNT_HDR+6	/* 2 byte: number of controllers */
#define	I4B_MON_IDATA_CLACCESS		I4B_MON_EVNT_HDR+8	/* 4 byte: client rights */

/* followed by this for every controller */
#define	I4B_MON_ICTRL_SIZE		I4B_MON_EVNT_HDR+I4B_MAX_MON_STRING+8
#define	I4B_MON_ICTRL_CODE		1					/* event code */
#define	I4B_MON_ICTRL_NAME		I4B_MON_EVNT_HDR+0			/* string: name of controller */
#define	I4B_MON_ICTRL_BUSID		I4B_MON_EVNT_HDR+I4B_MAX_MON_STRING+0	/* 2 byte: isdn bus id (reservered) */
#define	I4B_MON_ICTRL_FLAGS		I4B_MON_EVNT_HDR+I4B_MAX_MON_STRING+2	/* 4 byte: controller flags (not yet defined) */
#define	I4B_MON_ICTRL_NCHAN		I4B_MON_EVNT_HDR+I4B_MAX_MON_STRING+6	/* 2 byte: number of b channels on this controller */

/* followed by this for every entry */
#define	I4B_MON_IDEV_SIZE		I4B_MON_EVNT_HDR+I4B_MAX_MON_STRING+2
#define	I4B_MON_IDEV_CODE		2					/* event code */
#define	I4B_MON_IDEV_NAME		I4B_MON_EVNT_HDR+0			/* string: name of device */
#define	I4B_MON_IDEV_STATE		I4B_MON_EVNT_HDR+I4B_MAX_MON_STRING+0	/* 2 byte: state of device */

/*
 * The client sets it's protocol version and event mask (usually once after
 * connection establishement)
 */
#define	I4B_MON_CCMD_SETMASK		0x7e			/* command code */
#define	I4B_MON_ICLIENT_SIZE		I4B_MON_CMD_HDR+8
#define	I4B_MON_ICLIENT_VERMAJOR	I4B_MON_CMD_HDR+0	/* 2 byte: protocol major version (always 0 for now) */
#define	I4B_MON_ICLIENT_VERMINOR	I4B_MON_CMD_HDR+2	/* 2 byte: protocol minor version (always 0 for now) */
#define	I4B_MON_ICLIENT_EVENTS		I4B_MON_CMD_HDR+4	/* 4 byte: client event mask */

/* The client requests a list of monitor rights */
#define	I4B_MON_DUMPRIGHTS_CODE		1
#define	I4B_MON_DUMPRIGHTS_SIZE		I4B_MON_CMD_HDR		/* no parameters */

/*
 * in response to a I4B_MON_DUMPRIGHTS_CODE command, the daemon sends
 * this event:
 */
#define	I4B_MON_DRINI_CODE		2	/* event code */
#define	I4B_MON_DRINI_SIZE		I4B_MON_EVNT_HDR+2	/* size of packet */
#define	I4B_MON_DRINI_COUNT		I4B_MON_EVNT_HDR+0	/* 2 byte: number of records */

/* followed by this for each record anounced above */
#define	I4B_MON_DR_CODE			3
#define	I4B_MON_DR_SIZE			I4B_MON_EVNT_HDR+13
#define	I4B_MON_DR_RIGHTS		I4B_MON_EVNT_HDR+0	/* 4 byte: rights mask */
#define I4B_MON_DR_NET			I4B_MON_EVNT_HDR+4	/* 4 byte: network address */
#define	I4B_MON_DR_MASK			I4B_MON_EVNT_HDR+8	/* 4 byte: network mask */
#define	I4B_MON_DR_LOCAL		I4B_MON_EVNT_HDR+12	/* 1 byte: non-zero if local socket */

/* The client requests a list of monitor connections */
#define	I4B_MON_DUMPMCONS_CODE		2
#define	I4B_MON_DUMPMCONS_SIZE		I4B_MON_CMD_HDR		/* no parameters */

/*
 * in response to a I4B_MON_DUMPMCONS_CODE command, the daemon sends
 * this event:
 */
#define	I4B_MON_DCINI_CODE		4	/* event code */
#define	I4B_MON_DCINI_SIZE		I4B_MON_EVNT_HDR+2	/* size of packet */
#define	I4B_MON_DCINI_COUNT		I4B_MON_EVNT_HDR+0	/* 2 byte: number of records */

/* followed by this for each record anounced above */
#define	I4B_MON_DC_CODE			5
#define	I4B_MON_DC_SIZE			I4B_MON_EVNT_HDR+8
#define	I4B_MON_DC_RIGHTS		I4B_MON_EVNT_HDR+0	/* 4 byte: rights mask */
#define I4B_MON_DC_WHO			I4B_MON_EVNT_HDR+4	/* 4 byte: network address */

/* The client requests a config file rescan */
#define	I4B_MON_CFGREREAD_CODE		3
#define	I4B_MON_CFGREREAD_SIZE		I4B_MON_CMD_HDR		/* no parameters */

/* The client requests to hangup a connection */
#define	I4B_MON_HANGUP_CODE		4
#define	I4B_MON_HANGUP_SIZE		I4B_MON_CMD_HDR+8
#define	I4B_MON_HANGUP_CTRL		I4B_MON_CMD_HDR+0	/* controller */
#define	I4B_MON_HANGUP_CHANNEL		I4B_MON_CMD_HDR+4	/* channel */

/* The daemon sends a logfile event */
#define I4B_MON_LOGEVNT_CODE		6
#define	I4B_MON_LOGEVNT_SIZE		I4B_MON_EVNT_HDR+8+2*I4B_MAX_MON_STRING
#define	I4B_MON_LOGEVNT_TSTAMP		I4B_MON_EVNT_HDR+0	/* 4 byte: timestamp */
#define	I4B_MON_LOGEVNT_PRIO		I4B_MON_EVNT_HDR+4	/* 4 byte: syslog priority */
#define	I4B_MON_LOGEVNT_WHAT		I4B_MON_EVNT_HDR+8	/* followed by 2 strings: 'what' and 'message' */
#define	I4B_MON_LOGEVNT_MSG		I4B_MON_EVNT_HDR+8+I4B_MAX_MON_STRING

/* The daemon sends a charge event */
#define I4B_MON_CHRG_CODE		7
#define	I4B_MON_CHRG_SIZE		I4B_MON_EVNT_HDR+20
#define	I4B_MON_CHRG_TSTAMP		I4B_MON_EVNT_HDR+0	/* 4 byte: timestamp */
#define	I4B_MON_CHRG_CTRL		I4B_MON_EVNT_HDR+4	/* 4 byte: channel charged */
#define	I4B_MON_CHRG_CHANNEL		I4B_MON_EVNT_HDR+8	/* 4 byte: channel charged */
#define	I4B_MON_CHRG_UNITS		I4B_MON_EVNT_HDR+12	/* 4 byte: new charge value */
#define	I4B_MON_CHRG_ESTIMATED		I4B_MON_EVNT_HDR+16	/* 4 byte: 0 = charge by network, 1 = calculated estimate */

/* The daemon sends a connect event */
#define	I4B_MON_CONNECT_CODE		8
#define	I4B_MON_CONNECT_SIZE		I4B_MON_EVNT_HDR+16+4*I4B_MAX_MON_STRING
#define	I4B_MON_CONNECT_TSTAMP		I4B_MON_EVNT_HDR+0	/* 4 byte: time stamp */
#define	I4B_MON_CONNECT_DIR		I4B_MON_EVNT_HDR+4	/* 4 byte: direction (0 = incoming, 1 = outgoing) */
#define	I4B_MON_CONNECT_CTRL		I4B_MON_EVNT_HDR+8	/* 4 byte: channel connected */
#define	I4B_MON_CONNECT_CHANNEL		I4B_MON_EVNT_HDR+12	/* 4 byte: channel connected */
#define	I4B_MON_CONNECT_CFGNAME		I4B_MON_EVNT_HDR+16	/* name of config entry */
#define	I4B_MON_CONNECT_DEVNAME		I4B_MON_EVNT_HDR+16+I4B_MAX_MON_STRING	/* name of device used for connection */
#define	I4B_MON_CONNECT_REMPHONE	I4B_MON_EVNT_HDR+16+2*I4B_MAX_MON_STRING	/* remote phone no. */
#define	I4B_MON_CONNECT_LOCPHONE	I4B_MON_EVNT_HDR+16+3*I4B_MAX_MON_STRING	/* local phone no. */

/* The daemon sends a disconnect event */
#define	I4B_MON_DISCONNECT_CODE		9
#define	I4B_MON_DISCONNECT_SIZE		I4B_MON_EVNT_HDR+12
#define	I4B_MON_DISCONNECT_TSTAMP	I4B_MON_EVNT_HDR+0	/* 4 byte: time stamp */
#define	I4B_MON_DISCONNECT_CTRL		I4B_MON_EVNT_HDR+4	/* 4 byte: channel disconnected */
#define	I4B_MON_DISCONNECT_CHANNEL	I4B_MON_EVNT_HDR+8	/* 4 byte: channel disconnected */

/* The daemon sends an up/down event */
#define	I4B_MON_UPDOWN_CODE		10
#define	I4B_MON_UPDOWN_SIZE		I4B_MON_EVNT_HDR+16
#define	I4B_MON_UPDOWN_TSTAMP		I4B_MON_EVNT_HDR+0	/* 4 byte: time stamp */
#define	I4B_MON_UPDOWN_CTRL		I4B_MON_EVNT_HDR+4	/* 4 byte: channel disconnected */
#define	I4B_MON_UPDOWN_CHANNEL		I4B_MON_EVNT_HDR+8	/* 4 byte: channel disconnected */
#define	I4B_MON_UPDOWN_ISUP		I4B_MON_EVNT_HDR+12	/* 4 byte: interface is up */

/* The daemon sends a L1/L2 status change event */
#define	I4B_MON_L12STAT_CODE		11
#define	I4B_MON_L12STAT_SIZE		I4B_MON_EVNT_HDR+16
#define	I4B_MON_L12STAT_TSTAMP		I4B_MON_EVNT_HDR+0	/* 4 byte: time stamp */
#define	I4B_MON_L12STAT_CTRL		I4B_MON_EVNT_HDR+4	/* 4 byte: controller */
#define	I4B_MON_L12STAT_LAYER		I4B_MON_EVNT_HDR+8	/* 4 byte: layer */
#define	I4B_MON_L12STAT_STATE		I4B_MON_EVNT_HDR+12	/* 4 byte: state */

/* The daemon sends a TEI change event */
#define	I4B_MON_TEI_CODE		12
#define	I4B_MON_TEI_SIZE		I4B_MON_EVNT_HDR+12
#define	I4B_MON_TEI_TSTAMP		I4B_MON_EVNT_HDR+0	/* 4 byte: time stamp */
#define	I4B_MON_TEI_CTRL		I4B_MON_EVNT_HDR+4	/* 4 byte: controller */
#define	I4B_MON_TEI_TEI			I4B_MON_EVNT_HDR+8	/* 4 byte: tei */

/* The daemon sends an accounting message event */
#define	I4B_MON_ACCT_CODE		13
#define	I4B_MON_ACCT_SIZE		I4B_MON_EVNT_HDR+28
#define	I4B_MON_ACCT_TSTAMP		I4B_MON_EVNT_HDR+0	/* 4 byte: time stamp */
#define	I4B_MON_ACCT_CTRL		I4B_MON_EVNT_HDR+4	/* 4 byte: controller */
#define	I4B_MON_ACCT_CHAN		I4B_MON_EVNT_HDR+8	/* 4 byte: channel */
#define	I4B_MON_ACCT_OBYTES		I4B_MON_EVNT_HDR+12	/* 4 byte: outbytes */
#define	I4B_MON_ACCT_OBPS		I4B_MON_EVNT_HDR+16	/* 4 byte: outbps */
#define	I4B_MON_ACCT_IBYTES		I4B_MON_EVNT_HDR+20	/* 4 byte: inbytes */
#define	I4B_MON_ACCT_IBPS		I4B_MON_EVNT_HDR+24	/* 4 byte: inbps */

/* macros for setup/decoding of protocol packets */

/* clear a record */
#define	I4B_CLEAR(r)	memset(&(r), 0, sizeof(r));

/* prepare a record as event or command */
#define	I4B_PREP_EVNT(r, e)	{			\
	I4B_CLEAR(r);					\
	I4B_PUT_2B(r, I4B_MON_EVNT, e);			\
	I4B_PUT_2B(r, I4B_MON_EVNT_LEN, sizeof(r));	\
}
#define	I4B_PREP_CMD(r, c)	{			\
	I4B_CLEAR(r);					\
	I4B_PUT_2B(r, I4B_MON_CMD, c);			\
	I4B_PUT_2B(r, I4B_MON_CMD_LEN, sizeof(r));	\
}

/* put 1, 2 or 4 bytes in network byte order into a record at offset off */
#define	I4B_PUT_1B(r, off, val)	{ ((u_int8_t*)(r))[off] = (val) & 0x00ff; }
#define	I4B_PUT_2B(r, off, val) { I4B_PUT_1B(r, off, val >> 8); I4B_PUT_1B(r, off+1, val); }
#define	I4B_PUT_4B(r, off, val) { I4B_PUT_1B(r, off, val >> 24); I4B_PUT_1B(r, off+1, val >> 16); I4B_PUT_1B(r, off+2, val >> 8); I4B_PUT_1B(r, off+3, val); }

/* get 1, 2 or 4 bytes in network byte order from a record at offset off */
#define	I4B_GET_1B(r, off)	(((u_int8_t*)(r))[off])
#define	I4B_GET_2B(r, off)	((((u_int8_t*)(r))[off]) << 8) | (((u_int8_t*)(r))[off+1])
#define	I4B_GET_4B(r, off)	((((u_int8_t*)(r))[off]) << 24) | ((((u_int8_t*)(r))[off+1]) << 16) | ((((u_int8_t*)(r))[off+2]) << 8) | (((u_int8_t*)(r))[off+3])

/*
 * put a string into record r at offset off, make sure it's not to long
 * and proper terminate it
 */
#define	I4B_PUT_STR(r, off, str)	{		\
	strncpy((r)+(off), (str), I4B_MAX_MON_STRING);	\
	(r)[(off)+I4B_MAX_MON_STRING-1] = (u_int8_t)0;     }

#endif /* _MONITOR_H_ */

