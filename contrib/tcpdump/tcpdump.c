/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996
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
char copyright[] =
    "@(#) Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996\n\
The Regents of the University of California.  All rights reserved.\n";
static  char rcsid[] =
    "@(#)$Header: tcpdump.c,v 1.114 96/07/17 00:12:40 leres Exp $ (LBL)";
#endif

/*
 * tcpdump - monitor tcp/ip traffic on an ethernet.
 *
 * First written in 1987 by Van Jacobson, Lawrence Berkeley Laboratory.
 * Mercilessly hacked and occasionally improved since then via the
 * combined efforts of Van, Steve McCanne and Craig Leres of LBL.
 */

#include <sys/types.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <pcap.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "interface.h"
#include "addrtoname.h"
#include "machdep.h"

int fflag;			/* don't translate "foreign" IP address */
int nflag;			/* leave addresses as numbers */
int Nflag;			/* remove domains from printed host names */
int pflag;			/* don't go promiscuous */
int qflag;			/* quick (shorter) output */
int tflag = 1;			/* print packet arrival time */
int eflag;			/* print ethernet header */
int vflag;			/* verbose */
int xflag;			/* print packet in hex */
int Oflag = 1;			/* run filter code optimizer */
int Sflag;			/* print raw TCP sequence numbers */
int packettype;

int dflag;			/* print filter code */

char *program_name;

int32_t thiszone;		/* seconds offset from gmt to local time */

/* Externs */
extern void bpf_dump(struct bpf_program *, int);

/* Forwards */
RETSIGTYPE cleanup(int);
extern __dead void usage(void) __attribute__((volatile));

/* Length of saved portion of packet. */
int snaplen = DEFAULT_SNAPLEN;

struct printer {
	pcap_handler f;
	int type;
};

/* XXX needed if using old bpf.h */
#ifndef DLT_ATM_RFC1483
#define DLT_ATM_RFC1483 11
#endif

static struct printer printers[] = {
	{ ether_if_print,	DLT_EN10MB },
	{ ether_if_print,	DLT_IEEE802 },
	{ sl_if_print,		DLT_SLIP },
	{ ppp_if_print,		DLT_PPP },
	{ fddi_if_print,	DLT_FDDI },
	{ null_if_print,	DLT_NULL },
	{ atm_if_print,		DLT_ATM_RFC1483 },
	{ NULL,			0 },
};

static pcap_handler
lookup_printer(int type)
{
	struct printer *p;

	for (p = printers; p->f; ++p)
		if (type == p->type)
			return p->f;

	error("unknown data link type 0x%x", type);
	/* NOTREACHED */
}

static pcap_t *pd;

extern int optind;
extern int opterr;
extern char *optarg;

int
main(int argc, char **argv)
{
	register int cnt, op, i;
	bpf_u_int32 localnet, netmask;
	register char *cp, *infile, *cmdbuf, *device, *RFileName, *WFileName;
	pcap_handler printer;
	struct bpf_program fcode;
	u_char *pcap_userdata;
	char ebuf[PCAP_ERRBUF_SIZE];

	cnt = -1;
	device = NULL;
	infile = NULL;
	RFileName = NULL;
	WFileName = NULL;
	if ((cp = strrchr(argv[0], '/')) != NULL)
		program_name = cp + 1;
	else
		program_name = argv[0];

	if (abort_on_misalignment(ebuf) < 0)
		error(ebuf);

	opterr = 0;
	while ((op = getopt(argc, argv, "c:defF:i:lnNOpqr:s:StT:vw:xY")) != EOF)
		switch (op) {
		case 'c':
			cnt = atoi(optarg);
			if (cnt <= 0)
				error("invalid packet count %s", optarg);
			break;

		case 'd':
			++dflag;
			break;

		case 'e':
			++eflag;
			break;

		case 'f':
			++fflag;
			break;

		case 'F':
			infile = optarg;
			break;

		case 'i':
			device = optarg;
			break;

		case 'l':
#ifdef HAVE_SETLINEBUF
			setlinebuf(stdout);
#else
			setvbuf(stdout, NULL, _IOLBF, 0);
#endif
			break;

		case 'n':
			++nflag;
			break;

		case 'N':
			++Nflag;
			break;

		case 'O':
			Oflag = 0;
			break;

		case 'p':
			++pflag;
			break;

		case 'q':
			++qflag;
			break;

		case 'r':
			RFileName = optarg;
			break;

		case 's':
			snaplen = atoi(optarg);
			if (snaplen <= 0)
				error("invalid snaplen %s", optarg);
			break;

		case 'S':
			++Sflag;
			break;

		case 't':
			--tflag;
			break;

		case 'T':
			if (strcasecmp(optarg, "vat") == 0)
				packettype = PT_VAT;
			else if (strcasecmp(optarg, "wb") == 0)
				packettype = PT_WB;
			else if (strcasecmp(optarg, "rpc") == 0)
				packettype = PT_RPC;
			else if (strcasecmp(optarg, "rtp") == 0)
				packettype = PT_RTP;
			else if (strcasecmp(optarg, "rtcp") == 0)
				packettype = PT_RTCP;
			else
				error("unknown packet type `%s'", optarg);
			break;

		case 'v':
			++vflag;
			break;

		case 'w':
			WFileName = optarg;
			break;
#ifdef YYDEBUG
		case 'Y':
			{
			/* Undocumented flag */
			extern int yydebug;
			yydebug = 1;
			}
			break;
#endif
		case 'x':
			++xflag;
			break;

		default:
			usage();
			/* NOTREACHED */
		}

	if (tflag > 0)
		thiszone = gmt2local();

	if (RFileName != NULL) {
		/*
		 * We don't need network access, so set it back to the user id.
		 * Also, this prevents the user from reading anyone's
		 * trace file.
		 */
		setuid(getuid());

		pd = pcap_open_offline(RFileName, ebuf);
		if (pd == NULL)
			error(ebuf);
		localnet = 0;
		netmask = 0;
		if (fflag != 0)
			error("-f and -r options are incompatible");
	} else {
		if (device == NULL) {
			device = pcap_lookupdev(ebuf);
			if (device == NULL)
				error(ebuf);
		}
		pd = pcap_open_live(device, snaplen, !pflag, 1000, ebuf);
		if (pd == NULL)
			error(ebuf);
		i = pcap_snapshot(pd);
		if (snaplen < i) {
			warning("snaplen raised from %d to %d", snaplen, i);
			snaplen = i;
		}
		if (pcap_lookupnet(device, &localnet, &netmask, ebuf) < 0)
			error(ebuf);
		/*
		 * Let user own process after socket has been opened.
		 */
		setuid(getuid());
	}
	if (infile)
		cmdbuf = read_infile(infile);
	else
		cmdbuf = copy_argv(&argv[optind]);

	if (pcap_compile(pd, &fcode, cmdbuf, Oflag, netmask) < 0)
		error(pcap_geterr(pd));
	if (dflag) {
		bpf_dump(&fcode, dflag);
		exit(0);
	}
	init_addrtoname(fflag, localnet, netmask);

	(void)signal(SIGTERM, cleanup);
	(void)signal(SIGINT, cleanup);
	(void)signal(SIGHUP, cleanup);

	if (pcap_setfilter(pd, &fcode) < 0)
		error(pcap_geterr(pd));
	if (WFileName) {
		pcap_dumper_t *p = pcap_dump_open(pd, WFileName);
		if (p == NULL)
			error(pcap_geterr(pd));
		printer = pcap_dump;
		pcap_userdata = (u_char *)p;
	} else {
		printer = lookup_printer(pcap_datalink(pd));
		pcap_userdata = 0;
	}
	if (RFileName == NULL) {
		(void)fprintf(stderr, "%s: listening on %s\n",
		    program_name, device);
		(void)fflush(stderr);
	}
	if (pcap_loop(pd, cnt, printer, pcap_userdata) < 0) {
		(void)fprintf(stderr, "%s: pcap_loop: %s\n",
		    program_name, pcap_geterr(pd));
		exit(1);
	}
	pcap_close(pd);
	exit(0);
}

/* make a clean exit on interrupts */
RETSIGTYPE
cleanup(int signo)
{
	struct pcap_stat stat;

	/* Can't print the summary if reading from a savefile */
	if (pd != NULL && pcap_file(pd) == NULL) {
		(void)fflush(stdout);
		putc('\n', stderr);
		if (pcap_stats(pd, &stat) < 0)
			(void)fprintf(stderr, "pcap_stats: %s\n",
			    pcap_geterr(pd));
		else {
			(void)fprintf(stderr, "%d packets received by filter\n",
			    stat.ps_recv);
			(void)fprintf(stderr, "%d packets dropped by kernel\n",
			    stat.ps_drop);
		}
	}
	exit(0);
}

/* Like default_print() but data need not be aligned */
void
default_print_unaligned(register const u_char *cp, register u_int length)
{
	register u_int i, s;
	register int nshorts;

	nshorts = (u_int) length / sizeof(u_short);
	i = 0;
	while (--nshorts >= 0) {
		if ((i++ % 8) == 0)
			(void)printf("\n\t\t\t");
		s = *cp++;
		(void)printf(" %02x%02x", s, *cp++);
	}
	if (length & 1) {
		if ((i % 8) == 0)
			(void)printf("\n\t\t\t");
		(void)printf(" %02x", *cp);
	}
}

void
default_print(register const u_char *bp, register u_int length)
{
	register const u_short *sp;
	register u_int i;
	register int nshorts;

	if ((long)bp & 1) {
		default_print_unaligned(bp, length);
		return;
	}
	sp = (u_short *)bp;
	nshorts = (u_int) length / sizeof(u_short);
	i = 0;
	while (--nshorts >= 0) {
		if ((i++ % 8) == 0)
			(void)printf("\n\t\t\t");
		(void)printf(" %04x", ntohs(*sp++));
	}
	if (length & 1) {
		if ((i % 8) == 0)
			(void)printf("\n\t\t\t");
		(void)printf(" %02x", *(u_char *)sp);
	}
}

__dead void
usage()
{
	extern char version[];

	(void)fprintf(stderr, "Version %s\n", version);
	(void)fprintf(stderr,
"Usage: tcpdump [-deflnNOpqStvx] [-c count] [ -F file ]\n");
	(void)fprintf(stderr,
"\t\t[ -i interface ] [ -r file ] [ -s snaplen ]\n");
	(void)fprintf(stderr,
"\t\t[ -T type ] [ -w file ] [ expression ]\n");
	exit(-1);
}
