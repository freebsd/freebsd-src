/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994
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
 *
 * @(#) $Header: /pub/FreeBSD/FreeBSD-CVS/src/usr.sbin/tcpdump/tcpdump/interface.h,v 1.3 1995/03/08 12:52:14 olah Exp $ (LBL)
 */

#ifdef __GNUC__
#define inline __inline
#ifndef __dead
#define __dead volatile
#endif
#else
#define inline
#define __dead
#endif

#include "os.h"			/* operating system stuff */
#include "md.h"			/* machine dependent stuff */

#ifndef SIGRET
#define SIGRET void		/* default */
#endif

struct token {
	int v;			/* value */
	char *s;		/* string */
};

extern int dflag;		/* print filter code */
extern int eflag;		/* print ethernet header */
extern int nflag;		/* leave addresses as numbers */
extern int Nflag;		/* remove domains from printed host names */
extern int qflag;		/* quick (shorter) output */
extern int Sflag;		/* print raw TCP sequence numbers */
extern int tflag;		/* print packet arrival time */
extern int vflag;		/* verbose */
extern int xflag;		/* print packet in hex */

extern char *program_name;	/* used to generate self-identifying messages */

extern int snaplen;
/* global pointers to beginning and end of current packet (during printing) */
extern const u_char *packetp;
extern const u_char *snapend;

extern int fddipad;	/* alignment offset for FDDI headers, in bytes */

/* Eliminate some bogus warnings. */
struct timeval;

typedef void (*printfunc)(u_char *, struct timeval *, int, int);

extern void ts_print(const struct timeval *);
extern int clock_sigfigs(void);
int gmt2local(void);

extern int fn_print(const u_char *, const u_char *);
extern int fn_printn(const u_char *, u_int, const u_char *);
extern const char *tok2str(const struct token *, const char *, int);
extern char *dnaddr_string(u_short);
extern char *savestr(const char *);

extern int initdevice(char *, int, int *);
extern void wrapup(int);

extern __dead void error(char *, ...);
extern void warning(char *, ...);

extern char *read_infile(char *);
extern char *copy_argv(char **);

extern void usage(void);
extern char *isonsap_string(const u_char *);
extern char *llcsap_string(u_char);
extern char *protoid_string(const u_char *);
extern char *dnname_string(u_short);
extern char *dnnum_string(u_short);

/* The printer routines. */

struct pcap_pkthdr;

extern void ether_if_print(u_char *, const struct pcap_pkthdr *,
			   const u_char *);
extern void fddi_if_print(u_char *, const struct pcap_pkthdr *, const u_char*);
extern void null_if_print(u_char *, const struct pcap_pkthdr *, const u_char*);
extern void ppp_if_print(u_char *, const struct pcap_pkthdr *, const u_char *);
extern void sl_if_print(u_char *, const struct pcap_pkthdr *, const u_char *);

extern void arp_print(const u_char *, int, int);
extern void ip_print(const u_char *, int);
extern void tcp_print(const u_char *, int, const u_char *);
extern void udp_print(const u_char *, int, const u_char *);
extern void icmp_print(const u_char *, const u_char *);
extern void default_print(const u_char *, int);
extern void default_print_unaligned(const u_char *, int);

extern void aarp_print(const u_char *, int);
extern void atalk_print(const u_char *, int);
extern void bootp_print(const u_char *, int, u_short, u_short);
extern void decnet_print(const u_char *, int, int);
extern void egp_print(const u_char *, int, const u_char *);
extern int ether_encap_print(u_short, const u_char *, int, int);
extern void ipx_print(const u_char *, int length);
extern void isoclns_print(const u_char *, int, int,
			  const u_char *, const u_char *);
extern int llc_print(const u_char *, int, int, const u_char *, const u_char *);
extern void nfsreply_print(const u_char *, int, const u_char *);
extern void nfsreq_print(const u_char *, int, const u_char *);
extern void ns_print(const u_char *, int);
extern void ntp_print(const u_char *, int);
extern void ospf_print(const u_char *, int, const u_char *);
extern void rip_print(const u_char *, int);
extern void snmp_print(const u_char *, int);
extern void sunrpcrequest_print(const u_char *, int, const u_char *);
extern void tftp_print(const u_char *, int);
extern void wb_print(const void *, int);

#define min(a,b) ((a)>(b)?(b):(a))
#define max(a,b) ((b)>(a)?(b):(a))

/*
 * The default snapshot length.  This value allows most printers to print
 * useful information while keeping the amount of unwanted data down.
 * In particular, it allows for an ethernet header, tcp/ip header, and
 * 14 bytes of data (assuming no ip options).
 */
#define DEFAULT_SNAPLEN 68

#ifndef BIG_ENDIAN
#define BIG_ENDIAN 4321
#define LITTLE_ENDIAN 1234
#endif
