/*
    Alias_ftp.c performs special processing for FTP sessions under
    TCP.  Specifically, when a PORT command from the client side
    is sent, it is intercepted and modified.  The address is changed
    to the gateway machine and an aliasing port is used.

    For this routine to work, the PORT command must fit entirely
    into a single TCP packet.  This is typically the case, but exceptions
    can easily be envisioned under the actual specifications.

    Probably the most troubling aspect of the approach taken here is
    that the new PORT command will typically be a different length, and
    this causes a certain amount of bookkeeping to keep track of the
    changes of sequence and acknowledgment numbers, since the client
    machine is totally unaware of the modification to the TCP stream.


    This software is placed into the public domain with no restrictions
    on its distribution.

    Initial version:  August, 1996  (cjm)

    Version 1.6
         Brian Somers and Martin Renters identified an IP checksum
         error for modified IP packets.

    Version 1.7:  January 9, 1996 (cjm)
         Differental checksum computation for change
         in IP packet length.
   
    Version 2.1:  May, 1997 (cjm)
         Very minor changes to conform with
         local/global/function naming conventions
         withing the packet alising module.
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

static void NewFtpPortCommand(struct ip *, struct alias_link *, struct in_addr, u_short, int);



void
AliasHandleFtpOut(
struct ip *pip,	  /* IP packet to examine/patch */
struct alias_link *link, /* The link to go through (aliased port) */
int maxpacketsize  /* The maximum size this packet can grow to (including headers) */)
{
    int hlen, tlen, dlen;
    struct in_addr true_addr;
    u_short true_port;
    char *sptr;
    struct tcphdr *tc;
        
/* Calculate data length of TCP packet */
    tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));
    hlen = (pip->ip_hl + tc->th_off) << 2;
    tlen = ntohs(pip->ip_len);
    dlen = tlen - hlen;

/* Return is data length is too long or too short */
    if (dlen<10 || dlen>80)
        return;

/* Place string pointer and beginning of data */
    sptr = (char *) pip;  
    sptr += hlen;

/* Parse through string using state diagram method */
    {
        char ch, zero;
        int i, state;
        u_long a1, a2, a3, a4;
        u_short p1, p2; 

        a1=0; a2=0; a3=0; a4=0; p1=0; p2=0;
        zero = '0';
        state=-4;
        for (i=0; i<dlen; i++)
        {
            ch = sptr[i];
            switch (state)
            {
                case -4: if (ch == 'P') state=-3; else return; break;
                case -3: if (ch == 'O') state=-2; else return; break;
                case -2: if (ch == 'R') state=-1; else return; break;
                case -1: if (ch == 'T') state= 0; else return; break;

                case 0 :
                    if (isdigit(ch)) {a1=ch-zero; state=1 ;} break;
                case 1 :
                    if (isdigit(ch)) a1=10*a1+ch-zero; else state=2 ; break;
                case 2 :
                    if (isdigit(ch)) {a2=ch-zero; state=3 ;} break;
                case 3 :
                    if (isdigit(ch)) a2=10*a2+ch-zero; else state=4 ; break;
                case 4 :
                    if (isdigit(ch)) {a3=ch-zero; state=5 ;} break;
                case 5 :
                    if (isdigit(ch)) a3=10*a3+ch-zero; else state=6 ; break;
                case 6 :
                    if (isdigit(ch)) {a4=ch-zero; state=7 ;} break;
                case 7 :
                    if (isdigit(ch)) a4=10*a4+ch-zero; else state=8 ; break;
                case 8 :
                    if (isdigit(ch)) {p1=ch-zero; state=9 ;} break;
                case 9 :
                    if (isdigit(ch)) p1=10*p1+ch-zero; else state=10; break;
                case 10:
                    if (isdigit(ch)) {p2=ch-zero; state=11;} break;
                case 11:
                    if (isdigit(ch)) p2=10*p2+ch-zero; break;
            }
        }

        if (state == 11)
        {
            true_port = htons((p1<<8) + p2);
            true_addr.s_addr = htonl((a1<<24) + (a2<<16) +(a3<<8) + a4);
            NewFtpPortCommand(pip, link, true_addr, true_port, maxpacketsize);
        }
    }
}

static void
NewFtpPortCommand(struct ip *pip,
                  struct alias_link *link,
                  struct in_addr true_addr,
                  u_short true_port,
                  int maxpacketsize)
{ 
    struct alias_link *ftp_link;

/* Establish link to address and port found in PORT command */
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
            ptr = (char *) &alias_address;
            a1 = *ptr++; a2=*ptr++; a3=*ptr++; a4=*ptr;

/* Decompose alias port into pair format */
            alias_port = GetAliasPort(ftp_link);
            ptr = (char *) &alias_port;
            p1 = *ptr++; p2=*ptr;

/* Generate command string */
            sprintf(stemp, "PORT %d,%d,%d,%d,%d,%d\r\n",
                     a1,a2,a3,a4,p1,p2);

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
        fprintf(stderr,
        "PacketAlias/HandleFtpOut: Cannot allocate FTP data port\n");
    }
}
