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


    This software is placed into the public domain with no restrictions
    on its distribution.

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

    $FreeBSD$
*/

/* Includes */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include "alias_local.h"

#define FTP_CONTROL_PORT_NUMBER 21
#define MAX_MESSAGE_SIZE	128

enum ftp_message_type {
    FTP_PORT_COMMAND,
    FTP_EPRT_COMMAND,
    FTP_227_REPLY,
    FTP_229_REPLY,
    FTP_UNKNOWN_MESSAGE
};

static int ParseFtpPortCommand(char *, int);
static int ParseFtpEprtCommand(char *, int);
static int ParseFtp227Reply(char *, int);
static int ParseFtp229Reply(char *, int);
static void NewFtpMessage(struct ip *, struct alias_link *, int, int);

static struct in_addr true_addr;	/* in network byte order. */
static u_short true_port;		/* in host byte order. */

void
AliasHandleFtpOut(
struct ip *pip,	  /* IP packet to examine/patch */
struct alias_link *link, /* The link to go through (aliased port) */
int maxpacketsize  /* The maximum size this packet can grow to (including headers) */)
{
    int hlen, tlen, dlen;
    char *sptr;
    struct tcphdr *tc;
    int ftp_message_type;

/* Calculate data length of TCP packet */
    tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));
    hlen = (pip->ip_hl + tc->th_off) << 2;
    tlen = ntohs(pip->ip_len);
    dlen = tlen - hlen;

/* Place string pointer and beginning of data */
    sptr = (char *) pip;
    sptr += hlen;

/*
 * Check that data length is not too long and previous message was
 * properly terminated with CRLF.
 */
    if (dlen <= MAX_MESSAGE_SIZE && GetLastLineCrlfTermed(link)) {
	ftp_message_type = FTP_UNKNOWN_MESSAGE;

	if (ntohs(tc->th_dport) == FTP_CONTROL_PORT_NUMBER) {
/*
 * When aliasing a client, check for the PORT/EPRT command.
 */
	    if (ParseFtpPortCommand(sptr, dlen))
		ftp_message_type = FTP_PORT_COMMAND;
	    else if (ParseFtpEprtCommand(sptr, dlen))
		ftp_message_type = FTP_EPRT_COMMAND;
	} else {
/*
 * When aliasing a server, check for the 227/229 reply.
 */
	    if (ParseFtp227Reply(sptr, dlen))
		ftp_message_type = FTP_227_REPLY;
	    else if (ParseFtp229Reply(sptr, dlen))
		ftp_message_type = FTP_229_REPLY;
	}

	if (ftp_message_type != FTP_UNKNOWN_MESSAGE)
	    NewFtpMessage(pip, link, maxpacketsize, ftp_message_type);
    }

/* Track the msgs which are CRLF term'd for PORT/PASV FW breach */

    if (dlen) {                  /* only if there's data */
      sptr = (char *) pip; 	 /* start over at beginning */
      tlen = ntohs(pip->ip_len); /* recalc tlen, pkt may have grown */
      SetLastLineCrlfTermed(link,
			    (sptr[tlen-2] == '\r') && (sptr[tlen-1] == '\n'));
    }
}

static int
ParseFtpPortCommand(char *sptr, int dlen)
{
    char ch;
    int i, state;
    u_int32_t addr;
    u_short port;
    u_int8_t octet;

    /* Format: "PORT A,D,D,R,PO,RT". */

    /* Return if data length is too short. */
    if (dlen < 18)
	return 0;

    addr = port = octet = 0;
    state = -4;
    for (i = 0; i < dlen; i++) {
	ch = sptr[i];
	switch (state) {
	case -4: if (ch == 'P') state++; else return 0; break;
	case -3: if (ch == 'O') state++; else return 0; break;
	case -2: if (ch == 'R') state++; else return 0; break;
	case -1: if (ch == 'T') state++; else return 0; break;

	case 0:
	    if (isspace(ch))
		break;
	    else
		state++;
	case 1: case 3: case 5: case 7: case 9: case 11:
	    if (isdigit(ch)) {
		octet = ch - '0';
		state++;
	    } else
		return 0;
	    break;
	case 2: case 4: case 6: case 8:
	    if (isdigit(ch))
		octet = 10 * octet + ch - '0';
            else if (ch == ',') {
		addr = (addr << 8) + octet;
		state++;
	    } else
		return 0;
	    break;
	case 10: case 12:
	    if (isdigit(ch))
		octet = 10 * octet + ch - '0';
	    else if (ch == ',' || state == 12) {
		port = (port << 8) + octet;
		state++;
	    } else
		return 0;
	    break;
	}
    }

    if (state == 13) {
	true_addr.s_addr = htonl(addr);
	true_port = port;
	return 1;
    } else
	return 0;
}

static int
ParseFtpEprtCommand(char *sptr, int dlen)
{
    char ch, delim;
    int i, state;
    u_int32_t addr;
    u_short port;
    u_int8_t octet;

    /* Format: "EPRT |1|A.D.D.R|PORT|". */

    /* Return if data length is too short. */
    if (dlen < 18)
	return 0;

    addr = port = octet = 0;
    delim = '|';			/* XXX gcc -Wuninitialized */
    state = -4;
    for (i = 0; i < dlen; i++) {
	ch = sptr[i];
	switch (state)
	{
	case -4: if (ch == 'E') state++; else return 0; break;
	case -3: if (ch == 'P') state++; else return 0; break;
	case -2: if (ch == 'R') state++; else return 0; break;
	case -1: if (ch == 'T') state++; else return 0; break;

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
		return 0;
	    break;
	case 2:
	    if (ch == delim)
		state++;
	    else
		return 0;
	    break;
	case 3: case 5: case 7: case 9:
	    if (isdigit(ch)) {
		octet = ch - '0';
		state++;
	    } else
		return 0;
	    break;
	case 4: case 6: case 8: case 10:
	    if (isdigit(ch))
		octet = 10 * octet + ch - '0';
            else if (ch == '.' || state == 10) {
		addr = (addr << 8) + octet;
		state++;
	    } else
		return 0;
	    break;
	case 11:
	    if (isdigit(ch)) {
		port = ch - '0';
		state++;
	    } else
		return 0;
	    break;
	case 12:
	    if (isdigit(ch))
		port = 10 * port + ch - '0';
	    else if (ch == delim)
		state++;
	    else
		return 0;
	    break;
	}
    }

    if (state == 13) {
	true_addr.s_addr = htonl(addr);
	true_port = port;
	return 1;
    } else
	return 0;
}

static int
ParseFtp227Reply(char *sptr, int dlen)
{
    char ch;
    int i, state;
    u_int32_t addr;
    u_short port;
    u_int8_t octet;

    /* Format: "227 Entering Passive Mode (A,D,D,R,PO,RT)" */

    /* Return if data length is too short. */
    if (dlen < 17)
	return 0;

    addr = port = octet = 0;

    state = -3;
    for (i = 0; i < dlen; i++) {
        ch = sptr[i];
        switch (state)
        {
        case -3: if (ch == '2') state++; else return 0; break;
        case -2: if (ch == '2') state++; else return 0; break;
        case -1: if (ch == '7') state++; else return 0; break;

	case 0:
	    if (ch == '(')
		state++;
	    break;
	case 1: case 3: case 5: case 7: case 9: case 11:
	    if (isdigit(ch)) {
		octet = ch - '0';
		state++;
	    } else
		return 0;
	    break;
	case 2: case 4: case 6: case 8:
	    if (isdigit(ch))
		octet = 10 * octet + ch - '0';
            else if (ch == ',') {
		addr = (addr << 8) + octet;
		state++;
	    } else
		return 0;
	    break;
	case 10: case 12:
	    if (isdigit(ch))
		octet = 10 * octet + ch - '0';
	    else if (ch == ',' || (state == 12 && ch == ')')) {
		port = (port << 8) + octet;
		state++;
	    } else
		return 0;
	    break;
	}
    }

    if (state == 13) {
        true_port = port;
        true_addr.s_addr = htonl(addr);
	return 1;
    } else
	return 0;
}

static int
ParseFtp229Reply(char *sptr, int dlen)
{
    char ch, delim;
    int i, state;
    u_short port;

    /* Format: "229 Entering Extended Passive Mode (|||PORT|)" */

    /* Return if data length is too short. */
    if (dlen < 11)
	return 0;

    port = 0;
    delim = '|';			/* XXX gcc -Wuninitialized */

    state = -3;
    for (i = 0; i < dlen; i++) {
	ch = sptr[i];
	switch (state)
	{
	case -3: if (ch == '2') state++; else return 0; break;
	case -2: if (ch == '2') state++; else return 0; break;
	case -1: if (ch == '9') state++; else return 0; break;

	case 0:
	    if (ch == '(')
		state++;
	    break;
	case 1:
	    delim = ch;
	    state++;
	    break;
	case 2: case 3:
	    if (ch == delim)
		state++;
	    else
		return 0;
	    break;
	case 4:
	    if (isdigit(ch)) {
		port = ch - '0';
		state++;
	    } else
		return 0;
	    break;
	case 5:
	    if (isdigit(ch))
		port = 10 * port + ch - '0';
	    else if (ch == delim)
		state++;
	    else
		return 0;
	    break;
	case 6:
	    if (ch == ')')
		state++;
	    else
		return 0;
	    break;
	}
    }

    if (state == 7) {
	true_port = port;
	return 1;
    } else
	return 0;
}

static void
NewFtpMessage(struct ip *pip,
              struct alias_link *link,
              int maxpacketsize,
              int ftp_message_type)
{
    struct alias_link *ftp_link;

/* Security checks. */
    if (ftp_message_type != FTP_229_REPLY &&
	pip->ip_src.s_addr != true_addr.s_addr)
	return;

    if (true_port < IPPORT_RESERVED)
	return;

/* Establish link to address and port found in FTP control message. */
    ftp_link = FindUdpTcpOut(true_addr, GetDestAddress(link),
                             htons(true_port), 0, IPPROTO_TCP, 1);

    if (ftp_link != NULL)
    {
        int slen, hlen, tlen, dlen;
        struct tcphdr *tc;

#ifndef NO_FW_PUNCH
	if (ftp_message_type == FTP_PORT_COMMAND ||
	    ftp_message_type == FTP_EPRT_COMMAND) {
	    /* Punch hole in firewall */
	    PunchFWHole(ftp_link);
	}
#endif

/* Calculate data length of TCP packet */
        tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));
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
            alias_address = GetAliasAddress(link);
            ptr = (u_char *) &alias_address.s_addr;
            a1 = *ptr++; a2=*ptr++; a3=*ptr++; a4=*ptr;

	    alias_port = GetAliasPort(ftp_link);

	    switch (ftp_message_type)
	    {
	    case FTP_PORT_COMMAND:
	    case FTP_227_REPLY:
		/* Decompose alias port into pair format. */
		ptr = (char *) &alias_port;
		p1 = *ptr++; p2=*ptr;

		if (ftp_message_type == FTP_PORT_COMMAND) {
		    /* Generate PORT command string. */
		    sprintf(stemp, "PORT %d,%d,%d,%d,%d,%d\r\n",
			    a1,a2,a3,a4,p1,p2);
		} else {
		    /* Generate 227 reply string. */
		    sprintf(stemp,
			    "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
			    a1,a2,a3,a4,p1,p2);
		}
		break;
	    case FTP_EPRT_COMMAND:
		/* Generate EPRT command string. */
		sprintf(stemp, "EPRT |1|%d.%d.%d.%d|%d|\r\n",
			a1,a2,a3,a4,ntohs(alias_port));
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
            sptr = (char *) pip; sptr += hlen;
            strncpy(sptr, stemp, maxpacketsize-hlen);
        }

/* Save information regarding modified seq and ack numbers */
        {
            int delta;

            SetAckModified(link);
            delta = GetDeltaSeqOut(pip, link);
            AddSeq(pip, link, delta+slen-dlen);
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
    }
    else
    {
#ifdef DEBUG
        fprintf(stderr,
        "PacketAlias/HandleFtpOut: Cannot allocate FTP data port\n");
#endif
    }
}
