/*
    Alias_ftp.c performs special processing for FTP sessions under
    TCP.  Specifically, when a PORT command from the client side
    or PASV/227 reply from the server is sent, it is intercepted
    and modified.  The address is changed to the gateway machine
    and an aliasing port is used.

    For this routine to work, the PORT/227 must fit entirely
    into a single TCP packet.  This is typically the case, but exceptions
    can easily be envisioned under the actual specifications.

    Probably the most troubling aspect of the approach taken here is
    that the new PORT/227 will typically be a different length, and
    this causes a certain amount of bookkeeping to keep track of the
    changes of sequence and acknowledgment numbers, since the client
    machine is totally unaware of the modification to the TCP stream.

    This version also supports the EPRT command, which is functionally
    equivalent to the PORT command, but was designed to support both
    IPv4 and IPv6 addresses.  See RFC 2428 for specifications.


    This software is placed into the public domain with no restrictions
    on its distribution.

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
#define MIN_227_REPLY	16
#define MAX_227_REPLY	128

static int  ParseFtpPortCommand(char *, int, struct ip *, struct alias_link *, int);
static void ParseFtpEprtCommand(char *, int, struct ip *, struct alias_link *, int);
static void NewFtpPortCommand(struct ip *, struct alias_link *, struct in_addr, u_short, int, int);

static int  ParseFtp227Reply(char *, int, struct ip *, struct alias_link *, int);
static void NewFtp227Reply(struct ip *, struct alias_link *, struct in_addr, u_short, int);


void
AliasHandleFtpOut(
struct ip *pip,	  /* IP packet to examine/patch */
struct alias_link *link, /* The link to go through (aliased port) */
int maxpacketsize  /* The maximum size this packet can grow to (including headers) */)
{
    int hlen, tlen, dlen;
    char *sptr;
    struct tcphdr *tc;
        
/* Calculate data length of TCP packet */
    tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));
    hlen = (pip->ip_hl + tc->th_off) << 2;
    tlen = ntohs(pip->ip_len);
    dlen = tlen - hlen;

/* Place string pointer and beginning of data */
    sptr = (char *) pip;  
    sptr += hlen;

/* When aliasing a client, check for the PORT/EPRT command */
    if (ntohs(tc->th_dport) == FTP_CONTROL_PORT_NUMBER) { 
/* Parse through string using state diagram method */
        if (!ParseFtpPortCommand(sptr, dlen, pip, link, maxpacketsize))
            ParseFtpEprtCommand(sptr, dlen, pip, link, maxpacketsize);
    } else {
        ParseFtp227Reply(sptr, dlen, pip, link, maxpacketsize);
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
ParseFtpPortCommand(
char *sptr,
int dlen,
struct ip *pip,	  /* IP packet to examine/patch */
struct alias_link *link, /* The link to go through (aliased port) */
int maxpacketsize  /* The maximum size this packet can grow to (including headers) */)
{
    struct in_addr true_addr;
    u_short true_port;
    char ch;
    int i, state;
    u_long a1, a2, a3, a4;
    u_short p1, p2; 

    /* Return if data length is too long or too short */
    if (dlen<10 || dlen>80) 
	return 0;

    a1=0; a2=0; a3=0; a4=0; p1=0; p2=0;
    state=-4;
    for (i=0; i<dlen; i++)
    {
	ch = sptr[i];
	switch (state)
	{
	case -4: if (ch == 'P') state++; else return 0; break;
	case -3: if (ch == 'O') state++; else return 0; break;
	case -2: if (ch == 'R') state++; else return 0; break;
	case -1: if (ch == 'T') state++; else return 0; break;

	case 0 :
	    if (isdigit(ch)) {a1=ch-'0'; state++;} break;
	case 1 :
	    if (isdigit(ch)) a1=10*a1+ch-'0'; else state++; break;
	case 2 :
	    if (isdigit(ch)) {a2=ch-'0'; state++;} break;
	case 3 :
	    if (isdigit(ch)) a2=10*a2+ch-'0'; else state++; break;
	case 4 :
	    if (isdigit(ch)) {a3=ch-'0'; state++;} break;
	case 5 :
	    if (isdigit(ch)) a3=10*a3+ch-'0'; else state++; break;
	case 6 :
	    if (isdigit(ch)) {a4=ch-'0'; state++;} break;
	case 7 :
	    if (isdigit(ch)) a4=10*a4+ch-'0'; else state++; break;
	case 8 :
	    if (isdigit(ch)) {p1=ch-'0'; state++;} break;
	case 9 :
	    if (isdigit(ch)) p1=10*p1+ch-'0'; else state++; break;
	case 10:
	    if (isdigit(ch)) {p2=ch-'0'; state++;} break;
	case 11:
	    if (isdigit(ch)) p2=10*p2+ch-'0'; break;
	}
    }

    if (state == 11 && GetLastLineCrlfTermed(link))
    {
	true_port = htons((p1<<8) + p2);
	true_addr.s_addr = htonl((a1<<24) + (a2<<16) +(a3<<8) + a4);
	if ((pip->ip_src.s_addr == true_addr.s_addr) &&
	    ((p1<<8) >= IPPORT_RESERVED))
		NewFtpPortCommand(pip, link, true_addr, true_port, maxpacketsize, 0);
	return 1;
    }
    else
	return 0;
}

static void
ParseFtpEprtCommand(
char *sptr,
int dlen,
struct ip *pip,	  /* IP packet to examine/patch */
struct alias_link *link, /* The link to go through (aliased port) */
int maxpacketsize  /* The maximum size this packet can grow to (including headers) */)
{
    struct in_addr true_addr;
    u_short true_port;
    char ch, delim;
    int i, state;
    u_long a1, a2, a3, a4;
    u_short pt;

    /* Return if data length is too long or too short */
    if (dlen<10 || dlen>80) 
	return;

    a1=0; a2=0; a3=0; a4=0; pt=0;
    delim='|';				/* XXX gcc -Wuninitialized */
    state=-4;
    for (i=0; i<dlen; i++)
    {
	ch = sptr[i];
	switch (state)
	{
	case -4: if (ch == 'E') state++; else return; break;
	case -3: if (ch == 'P') state++; else return; break;
	case -2: if (ch == 'R') state++; else return; break;
	case -1: if (ch == 'T') state++; else return; break;

	case 0 :
	    if (!isspace(ch)) {delim=ch; state++;} break;
	case 1 :
	    if (ch=='1') /* IPv4 address */ state++; else return; break;
	case 2 :
	    if (ch==delim) state++; else return; break;
	case 3 :
	    if (isdigit(ch)) {a1=ch-'0'; state++;} else return; break;
	case 4 :
	    if (isdigit(ch)) a1=10*a1+ch-'0';
	    else if (ch=='.') state++;
	    else return;
	    break;
	case 5 :
	    if (isdigit(ch)) {a2=ch-'0'; state++;} else return; break;
	case 6 :
	    if (isdigit(ch)) a2=10*a2+ch-'0';
	    else if (ch=='.') state++;
	    else return;
	    break;
	case 7:
	    if (isdigit(ch)) {a3=ch-'0'; state++;} else return; break;
	case 8 :
	    if (isdigit(ch)) a3=10*a3+ch-'0';
	    else if (ch=='.') state++;
	    else return;
	    break;
	case 9 :
	    if (isdigit(ch)) {a4=ch-'0'; state++;} else return; break;
	case 10:
	    if (isdigit(ch)) a4=10*a4+ch-'0';
	    else if (ch==delim) state++;
	    else return;
	    break;
	case 11:
	    if (isdigit(ch)) {pt=ch-'0'; state++;} else return; break;
	case 12:
	    if (isdigit(ch)) pt=10*pt+ch-'0';
	    else if (ch==delim) state++;
	    else return;
	    break;
	}
    }

    if (state == 13 && GetLastLineCrlfTermed(link))
    {
	true_port = htons(pt);
	true_addr.s_addr = htonl((a1<<24) + (a2<<16) +(a3<<8) + a4);
	if ((pip->ip_src.s_addr == true_addr.s_addr) &&
	    (pt >= IPPORT_RESERVED))
		NewFtpPortCommand(pip, link, true_addr, true_port, maxpacketsize, 1);
    }
}

static int
ParseFtp227Reply(char *sptr,
                 int dlen,
                 struct ip *pip, 	  /* IP packet to examine/patch */
                 struct alias_link *link, /* The link to go through (aliased port) */
                 int maxpacketsize)	  /* The maximum size this packet can grow */
					  /* to (including headers) */
{
    struct in_addr true_addr;
    u_short true_port;
    char ch;
    int i, state;
    u_long a1, a2, a3, a4;
    u_short p1, p2; 


    /* Return if wrong size, in case more packet types added later */ 
    if (dlen<MIN_227_REPLY || dlen>MAX_227_REPLY) 
        return 0;

    a1=0; a2=0; a3=0; a4=0; p1=0; p2=0;
    state=-3;
    for (i=0; i<dlen; i++)
    {
        ch = sptr[i];
        switch (state)
        {
        case -3: if (ch == '2') state++; else return 0; break;
        case -2: if (ch == '2') state++; else return 0; break;
        case -1: if (ch == '7') state++; else return 0; break;

        case 0 :
            if (isdigit(ch)) {a1=ch-'0'; state++;} break;
        case 1 :
            if (isdigit(ch)) a1=10*a1+ch-'0'; else state++; break;
        case 2 :
            if (isdigit(ch)) {a2=ch-'0'; state++;} break;
        case 3 :
            if (isdigit(ch)) a2=10*a2+ch-'0'; else state++; break;
        case 4 :
            if (isdigit(ch)) {a3=ch-'0'; state++;} break;
        case 5 :
            if (isdigit(ch)) a3=10*a3+ch-'0'; else state++; break;
        case 6 :
            if (isdigit(ch)) {a4=ch-'0'; state++;} break;
        case 7 :
            if (isdigit(ch)) a4=10*a4+ch-'0'; else state++; break;
        case 8 :
            if (isdigit(ch)) {p1=ch-'0'; state++;} break;
        case 9 :
            if (isdigit(ch)) p1=10*p1+ch-'0'; else state++; break;
        case 10:
            if (isdigit(ch)) {p2=ch-'0'; state++;} break;
        case 11:
            if (isdigit(ch)) p2=10*p2+ch-'0'; break;
        }
    }

    if (state == 11 && GetLastLineCrlfTermed(link))
    {
        true_port = htons((p1<<8) + p2);
        true_addr.s_addr = htonl((a1<<24) + (a2<<16) +(a3<<8) + a4);
	if ((pip->ip_src.s_addr == true_addr.s_addr) &&
	    ((p1<<8) >= IPPORT_RESERVED))
		NewFtp227Reply(pip, link, true_addr, true_port, maxpacketsize);
	return 1;
    }
    else
	return 0;
}

static void
NewFtpPortCommand(struct ip *pip,
                  struct alias_link *link,
                  struct in_addr true_addr,
                  u_short true_port,
                  int maxpacketsize,
                  int is_eprt)
{ 
    struct alias_link *ftp_link;

/* Establish link to address and port found in PORT command */
    ftp_link = FindUdpTcpOut(true_addr, GetDestAddress(link),
                             true_port, 0, IPPROTO_TCP);

    if (ftp_link != NULL)
    {
        int slen, hlen, tlen, dlen;
        struct tcphdr *tc;

#ifndef NO_FW_PUNCH
/* Punch hole in firewall */
        PunchFWHole(ftp_link);
#endif

/* Calculate data length of TCP packet */
        tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));
        hlen = (pip->ip_hl + tc->th_off) << 2;
        tlen = ntohs(pip->ip_len);
        dlen = tlen - hlen;

/* Create new PORT command */
        {
            char stemp[80];
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

	    if (is_eprt) {
/* Generate EPRT command string */
		sprintf(stemp, "EPRT |1|%d.%d.%d.%d|%d|\r\n",
			a1,a2,a3,a4,ntohs(alias_port));
	    } else {
/* Decompose alias port into pair format */
		ptr = (char *) &alias_port;
		p1 = *ptr++; p2=*ptr;

/* Generate PORT command string */
		sprintf(stemp, "PORT %d,%d,%d,%d,%d,%d\r\n",
			a1,a2,a3,a4,p1,p2);
	    }

/* Save string length for IP header modification */
            slen = strlen(stemp);

/* Copy into IP packet */
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
        "PacketAlias/HandleFtpOut, PORT: Cannot allocate FTP data port\n");
#endif
    }
}

static void
NewFtp227Reply(struct ip 	*pip,
            struct alias_link 	*link,
            struct in_addr 	true_addr,
            u_short 		true_port,
            int 		maxpacketsize)
{ 
    struct alias_link *ftp_link;

/* Establish link to address and port found in 227 reply */
    ftp_link = FindUdpTcpOut(true_addr, GetDestAddress(link),
                             true_port, 0, IPPROTO_TCP);

    if (ftp_link != NULL)
    {
        int slen, hlen, tlen, dlen;
        struct tcphdr *tc;

/* Calculate data length of TCP packet */
        tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));
        hlen = (pip->ip_hl + tc->th_off) << 2;
        tlen = ntohs(pip->ip_len);
        dlen = tlen - hlen;

/* Create new 227 reply */
        {
            char stemp[MAX_227_REPLY+1];
            char *sptr;
            u_short alias_port;
            u_char *ptr;
            int a1, a2, a3, a4, p1, p2; 
            struct in_addr alias_address;
        
/* Decompose alias address into quad format */
            alias_address = GetAliasAddress(link);
            ptr = (u_char *) &alias_address.s_addr;
            a1 = *ptr++; a2=*ptr++; a3=*ptr++; a4=*ptr;

/* Decompose alias port into pair format */
            alias_port = GetAliasPort(ftp_link);
            ptr = (char *) &alias_port;
            p1 = *ptr++; p2=*ptr;

/* Generate command string */
            sprintf(stemp, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
                     a1,a2,a3,a4,p1,p2);

/* Save string length for IP header modification */
            slen = strlen(stemp);

/* Copy modified buffer into IP packet */
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
        "PacketAlias/HandleFtpOut, 227: Cannot allocate FTP data port\n");
#endif
    }
}
