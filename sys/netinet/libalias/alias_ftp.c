/*-
 * Copyright (c) 2001 Charles Mott <cm@linktel.net>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
    Alias_ftp.c performs special processing for FTP sessions under
    TCP.  Specifically, when a PORT/EPRT command from the client
    side or 227/229 reply from the server is sent, it is intercepted
    and modified.  The address is changed to the gateway machine
    and an aliasing port is used.

    For this routine to work, the message must fit entirely into a
    single TCP packet.  This is typically the case, but exceptions
    can easily be envisioned under the actual specifications.

    Probably the most troubling aspect of the approach taken here is
    that the new message will typically be a different length, and
    this causes a certain amount of bookkeeping to keep track of the
    changes of sequence and acknowledgment numbers, since the client
    machine is totally unaware of the modification to the TCP stream.


    References: RFC 959, RFC 2428.

    Initial version:  August, 1996  (cjm)

    Version 1.6
	 Brian Somers and Martin Renters identified an IP checksum
	 error for modified IP packets.

    Version 1.7:  January 9, 1996 (cjm)
	 Differential checksum computation for change
	 in IP packet length.

    Version 2.1:  May, 1997 (cjm)
	 Very minor changes to conform with
	 local/global/function naming conventions
	 within the packet aliasing module.

    Version 3.1:  May, 2000 (eds)
	 Add support for passive mode, alias the 227 replies.

    See HISTORY file for record of revisions.
*/

/* Includes */
#ifdef _KERNEL
#include <sys/param.h>
#include <sys/ctype.h>
#include <sys/libkern.h>
#else
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#endif

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#ifdef _KERNEL
#include <netinet/libalias/alias.h>
#include <netinet/libalias/alias_local.h>
#else
#include "alias_local.h"
#endif

#define FTP_CONTROL_PORT_NUMBER 21
#define MAX_MESSAGE_SIZE	128

/* FTP protocol flags. */
#define WAIT_CRLF		0x01

enum ftp_message_type {
	FTP_PORT_COMMAND,
	FTP_EPRT_COMMAND,
	FTP_227_REPLY,
	FTP_229_REPLY,
	FTP_UNKNOWN_MESSAGE
};

static int	ParseFtpPortCommand(struct libalias *la, char *, int);
static int	ParseFtpEprtCommand(struct libalias *la, char *, int);
static int	ParseFtp227Reply(struct libalias *la, char *, int);
static int	ParseFtp229Reply(struct libalias *la, char *, int);
static void	NewFtpMessage(struct libalias *la, struct ip *, struct alias_link *, int, int);

void
AliasHandleFtpOut(
    struct libalias *la,
    struct ip *pip,		/* IP packet to examine/patch */
    struct alias_link *lnk,	/* The link to go through (aliased port) */
    int maxpacketsize		/* The maximum size this packet can grow to
	(including headers) */ )
{
	int hlen, tlen, dlen, pflags;
	char *sptr;
	struct tcphdr *tc;
	int ftp_message_type;

/* Calculate data length of TCP packet */
	tc = (struct tcphdr *)ip_next(pip);
	hlen = (pip->ip_hl + tc->th_off) << 2;
	tlen = ntohs(pip->ip_len);
	dlen = tlen - hlen;

/* Place string pointer and beginning of data */
	sptr = (char *)pip;
	sptr += hlen;

/*
 * Check that data length is not too long and previous message was
 * properly terminated with CRLF.
 */
	pflags = GetProtocolFlags(lnk);
	if (dlen <= MAX_MESSAGE_SIZE && !(pflags & WAIT_CRLF)) {
		ftp_message_type = FTP_UNKNOWN_MESSAGE;

		if (ntohs(tc->th_dport) == FTP_CONTROL_PORT_NUMBER) {
/*
 * When aliasing a client, check for the PORT/EPRT command.
 */
			if (ParseFtpPortCommand(la, sptr, dlen))
				ftp_message_type = FTP_PORT_COMMAND;
			else if (ParseFtpEprtCommand(la, sptr, dlen))
				ftp_message_type = FTP_EPRT_COMMAND;
		} else {
/*
 * When aliasing a server, check for the 227/229 reply.
 */
			if (ParseFtp227Reply(la, sptr, dlen))
				ftp_message_type = FTP_227_REPLY;
			else if (ParseFtp229Reply(la, sptr, dlen)) {
				ftp_message_type = FTP_229_REPLY;
				la->true_addr.s_addr = pip->ip_src.s_addr;
			}
		}

		if (ftp_message_type != FTP_UNKNOWN_MESSAGE)
			NewFtpMessage(la, pip, lnk, maxpacketsize, ftp_message_type);
	}
/* Track the msgs which are CRLF term'd for PORT/PASV FW breach */

	if (dlen) {		/* only if there's data */
		sptr = (char *)pip;	/* start over at beginning */
		tlen = ntohs(pip->ip_len);	/* recalc tlen, pkt may
						 * have grown */
		if (sptr[tlen - 2] == '\r' && sptr[tlen - 1] == '\n')
			pflags &= ~WAIT_CRLF;
		else
			pflags |= WAIT_CRLF;
		SetProtocolFlags(lnk, pflags);
	}
}

static int
ParseFtpPortCommand(struct libalias *la, char *sptr, int dlen)
{
	char ch;
	int i, state;
	u_int32_t addr;
	u_short port;
	u_int8_t octet;

	/* Format: "PORT A,D,D,R,PO,RT". */

	/* Return if data length is too short. */
	if (dlen < 18)
		return (0);

	addr = port = octet = 0;
	state = -4;
	for (i = 0; i < dlen; i++) {
		ch = sptr[i];
		switch (state) {
		case -4:
			if (ch == 'P')
				state++;
			else
				return (0);
			break;
		case -3:
			if (ch == 'O')
				state++;
			else
				return (0);
			break;
		case -2:
			if (ch == 'R')
				state++;
			else
				return (0);
			break;
		case -1:
			if (ch == 'T')
				state++;
			else
				return (0);
			break;

		case 0:
			if (isspace(ch))
				break;
			else
				state++;
		case 1:
		case 3:
		case 5:
		case 7:
		case 9:
		case 11:
			if (isdigit(ch)) {
				octet = ch - '0';
				state++;
			} else
				return (0);
			break;
		case 2:
		case 4:
		case 6:
		case 8:
			if (isdigit(ch))
				octet = 10 * octet + ch - '0';
			else if (ch == ',') {
				addr = (addr << 8) + octet;
				state++;
			} else
				return (0);
			break;
		case 10:
		case 12:
			if (isdigit(ch))
				octet = 10 * octet + ch - '0';
			else if (ch == ',' || state == 12) {
				port = (port << 8) + octet;
				state++;
			} else
				return (0);
			break;
		}
	}

	if (state == 13) {
		la->true_addr.s_addr = htonl(addr);
		la->true_port = port;
		return (1);
	} else
		return (0);
}

static int
ParseFtpEprtCommand(struct libalias *la, char *sptr, int dlen)
{
	char ch, delim;
	int i, state;
	u_int32_t addr;
	u_short port;
	u_int8_t octet;

	/* Format: "EPRT |1|A.D.D.R|PORT|". */

	/* Return if data length is too short. */
	if (dlen < 18)
		return (0);

	addr = port = octet = 0;
	delim = '|';		/* XXX gcc -Wuninitialized */
	state = -4;
	for (i = 0; i < dlen; i++) {
		ch = sptr[i];
		switch (state) {
		case -4:
			if (ch == 'E')
				state++;
			else
				return (0);
			break;
		case -3:
			if (ch == 'P')
				state++;
			else
				return (0);
			break;
		case -2:
			if (ch == 'R')
				state++;
			else
				return (0);
			break;
		case -1:
			if (ch == 'T')
				state++;
			else
				return (0);
			break;

		case 0:
			if (!isspace(ch)) {
				delim = ch;
				state++;
			}
			break;
		case 1:
			if (ch == '1')	/* IPv4 address */
				state++;
			else
				return (0);
			break;
		case 2:
			if (ch == delim)
				state++;
			else
				return (0);
			break;
		case 3:
		case 5:
		case 7:
		case 9:
			if (isdigit(ch)) {
				octet = ch - '0';
				state++;
			} else
				return (0);
			break;
		case 4:
		case 6:
		case 8:
		case 10:
			if (isdigit(ch))
				octet = 10 * octet + ch - '0';
			else if (ch == '.' || state == 10) {
				addr = (addr << 8) + octet;
				state++;
			} else
				return (0);
			break;
		case 11:
			if (isdigit(ch)) {
				port = ch - '0';
				state++;
			} else
				return (0);
			break;
		case 12:
			if (isdigit(ch))
				port = 10 * port + ch - '0';
			else if (ch == delim)
				state++;
			else
				return (0);
			break;
		}
	}

	if (state == 13) {
		la->true_addr.s_addr = htonl(addr);
		la->true_port = port;
		return (1);
	} else
		return (0);
}

static int
ParseFtp227Reply(struct libalias *la, char *sptr, int dlen)
{
	char ch;
	int i, state;
	u_int32_t addr;
	u_short port;
	u_int8_t octet;

	/* Format: "227 Entering Passive Mode (A,D,D,R,PO,RT)" */

	/* Return if data length is too short. */
	if (dlen < 17)
		return (0);

	addr = port = octet = 0;

	state = -3;
	for (i = 0; i < dlen; i++) {
		ch = sptr[i];
		switch (state) {
		case -3:
			if (ch == '2')
				state++;
			else
				return (0);
			break;
		case -2:
			if (ch == '2')
				state++;
			else
				return (0);
			break;
		case -1:
			if (ch == '7')
				state++;
			else
				return (0);
			break;

		case 0:
			if (ch == '(')
				state++;
			break;
		case 1:
		case 3:
		case 5:
		case 7:
		case 9:
		case 11:
			if (isdigit(ch)) {
				octet = ch - '0';
				state++;
			} else
				return (0);
			break;
		case 2:
		case 4:
		case 6:
		case 8:
			if (isdigit(ch))
				octet = 10 * octet + ch - '0';
			else if (ch == ',') {
				addr = (addr << 8) + octet;
				state++;
			} else
				return (0);
			break;
		case 10:
		case 12:
			if (isdigit(ch))
				octet = 10 * octet + ch - '0';
			else if (ch == ',' || (state == 12 && ch == ')')) {
				port = (port << 8) + octet;
				state++;
			} else
				return (0);
			break;
		}
	}

	if (state == 13) {
		la->true_port = port;
		la->true_addr.s_addr = htonl(addr);
		return (1);
	} else
		return (0);
}

static int
ParseFtp229Reply(struct libalias *la, char *sptr, int dlen)
{
	char ch, delim;
	int i, state;
	u_short port;

	/* Format: "229 Entering Extended Passive Mode (|||PORT|)" */

	/* Return if data length is too short. */
	if (dlen < 11)
		return (0);

	port = 0;
	delim = '|';		/* XXX gcc -Wuninitialized */

	state = -3;
	for (i = 0; i < dlen; i++) {
		ch = sptr[i];
		switch (state) {
		case -3:
			if (ch == '2')
				state++;
			else
				return (0);
			break;
		case -2:
			if (ch == '2')
				state++;
			else
				return (0);
			break;
		case -1:
			if (ch == '9')
				state++;
			else
				return (0);
			break;

		case 0:
			if (ch == '(')
				state++;
			break;
		case 1:
			delim = ch;
			state++;
			break;
		case 2:
		case 3:
			if (ch == delim)
				state++;
			else
				return (0);
			break;
		case 4:
			if (isdigit(ch)) {
				port = ch - '0';
				state++;
			} else
				return (0);
			break;
		case 5:
			if (isdigit(ch))
				port = 10 * port + ch - '0';
			else if (ch == delim)
				state++;
			else
				return (0);
			break;
		case 6:
			if (ch == ')')
				state++;
			else
				return (0);
			break;
		}
	}

	if (state == 7) {
		la->true_port = port;
		return (1);
	} else
		return (0);
}

static void
NewFtpMessage(struct libalias *la, struct ip *pip,
    struct alias_link *lnk,
    int maxpacketsize,
    int ftp_message_type)
{
	struct alias_link *ftp_lnk;

/* Security checks. */
	if (pip->ip_src.s_addr != la->true_addr.s_addr)
		return;

	if (la->true_port < IPPORT_RESERVED)
		return;

/* Establish link to address and port found in FTP control message. */
	ftp_lnk = FindUdpTcpOut(la, la->true_addr, GetDestAddress(lnk),
	    htons(la->true_port), 0, IPPROTO_TCP, 1);

	if (ftp_lnk != NULL) {
		int slen, hlen, tlen, dlen;
		struct tcphdr *tc;

#ifndef NO_FW_PUNCH
		/* Punch hole in firewall */
		PunchFWHole(ftp_lnk);
#endif

/* Calculate data length of TCP packet */
		tc = (struct tcphdr *)ip_next(pip);
		hlen = (pip->ip_hl + tc->th_off) << 2;
		tlen = ntohs(pip->ip_len);
		dlen = tlen - hlen;

/* Create new FTP message. */
		{
			char stemp[MAX_MESSAGE_SIZE + 1];
			char *sptr;
			u_short alias_port;
			u_char *ptr;
			int a1, a2, a3, a4, p1, p2;
			struct in_addr alias_address;

/* Decompose alias address into quad format */
			alias_address = GetAliasAddress(lnk);
			ptr = (u_char *) & alias_address.s_addr;
			a1 = *ptr++;
			a2 = *ptr++;
			a3 = *ptr++;
			a4 = *ptr;

			alias_port = GetAliasPort(ftp_lnk);

			switch (ftp_message_type) {
			case FTP_PORT_COMMAND:
			case FTP_227_REPLY:
				/* Decompose alias port into pair format. */
				ptr = (char *)&alias_port;
				p1 = *ptr++;
				p2 = *ptr;

				if (ftp_message_type == FTP_PORT_COMMAND) {
					/* Generate PORT command string. */
					sprintf(stemp, "PORT %d,%d,%d,%d,%d,%d\r\n",
					    a1, a2, a3, a4, p1, p2);
				} else {
					/* Generate 227 reply string. */
					sprintf(stemp,
					    "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
					    a1, a2, a3, a4, p1, p2);
				}
				break;
			case FTP_EPRT_COMMAND:
				/* Generate EPRT command string. */
				sprintf(stemp, "EPRT |1|%d.%d.%d.%d|%d|\r\n",
				    a1, a2, a3, a4, ntohs(alias_port));
				break;
			case FTP_229_REPLY:
				/* Generate 229 reply string. */
				sprintf(stemp, "229 Entering Extended Passive Mode (|||%d|)\r\n",
				    ntohs(alias_port));
				break;
			}

/* Save string length for IP header modification */
			slen = strlen(stemp);

/* Copy modified buffer into IP packet. */
			sptr = (char *)pip;
			sptr += hlen;
			strncpy(sptr, stemp, maxpacketsize - hlen);
		}

/* Save information regarding modified seq and ack numbers */
		{
			int delta;

			SetAckModified(lnk);
			delta = GetDeltaSeqOut(pip, lnk);
			AddSeq(pip, lnk, delta + slen - dlen);
		}

/* Revise IP header */
		{
			u_short new_len;

			new_len = htons(hlen + slen);
			DifferentialChecksum(&pip->ip_sum,
			    &new_len,
			    &pip->ip_len,
			    1);
			pip->ip_len = new_len;
		}

/* Compute TCP checksum for revised packet */
		tc->th_sum = 0;
		tc->th_sum = TcpChecksum(pip);
	} else {
#ifdef LIBALIAS_DEBUG
		fprintf(stderr,
		    "PacketAlias/HandleFtpOut: Cannot allocate FTP data port\n");
#endif
	}
}
