/*	BSDI inet.c,v 2.3 1995/10/24 02:19:29 prb Exp	*/
/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.bin/netstat/inet6.c,v 1.3.2.2 2000/08/07 16:45:42 ume Exp $
 */

#ifndef lint
/*
static char sccsid[] = "@(#)inet6.c	8.4 (Berkeley) 4/20/94";
*/
#endif /* not lint */

#ifdef INET6
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>

#include <net/route.h>
#include <net/if.h>
#include <net/if_var.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/in_systm.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#include <netinet6/pim6_var.h>

#include <arpa/inet.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "netstat.h"

struct	socket sockb;

char	*inet6name __P((struct in6_addr *));
void	inet6print __P((struct in6_addr *, int, char *, int));

static char ntop_buf[INET6_ADDRSTRLEN];

static	char *ip6nh[] = {
	"hop by hop",
	"ICMP",
	"IGMP",
	"#3",
	"IP",
	"#5",
	"TCP",
	"#7",
	"#8",
	"#9",
	"#10",
	"#11",
	"#12",
	"#13",
	"#14",
	"#15",
	"#16",
	"UDP",
	"#18",
	"#19",	
	"#20",
	"#21",
	"IDP",
	"#23",
	"#24",
	"#25",
	"#26",
	"#27",
	"#28",
	"TP",	
	"#30",
	"#31",
	"#32",
	"#33",
	"#34",
	"#35",
	"#36",
	"#37",
	"#38",
	"#39",	
	"#40",
	"IP6",
	"#42",
	"routing",
	"fragment",
	"#45",
	"#46",
	"#47",
	"#48",
	"#49",	
	"ESP",
	"AH",
	"#52",
	"#53",
	"#54",
	"#55",
	"#56",
	"#57",
	"ICMP6",
	"no next header",	
	"destination option",
	"#61",
	"#62",
	"#63",
	"#64",
	"#65",
	"#66",
	"#67",
	"#68",
	"#69",	
	"#70",
	"#71",
	"#72",
	"#73",
	"#74",
	"#75",
	"#76",
	"#77",
	"#78",
	"#79",	
	"ISOIP",
	"#81",
	"#82",
	"#83",
	"#84",
	"#85",
	"#86",
	"#87",
	"#88",
	"OSPF",	
	"#80",
	"#91",
	"#92",
	"#93",
	"#94",
	"#95",
	"#96",
	"Ethernet",
	"#98",
	"#99",	
	"#100",
	"#101",
	"#102",
	"PIM",
	"#104",
	"#105",
	"#106",
	"#107",
	"#108",
	"#109",	
	"#110",
	"#111",
	"#112",
	"#113",
	"#114",
	"#115",
	"#116",
	"#117",
	"#118",
	"#119",	
	"#120",
	"#121",
	"#122",
	"#123",
	"#124",
	"#125",
	"#126",
	"#127",
	"#128",
	"#129",	
	"#130",
	"#131",
	"#132",
	"#133",
	"#134",
	"#135",
	"#136",
	"#137",
	"#138",
	"#139",	
	"#140",
	"#141",
	"#142",
	"#143",
	"#144",
	"#145",
	"#146",
	"#147",
	"#148",
	"#149",	
	"#150",
	"#151",
	"#152",
	"#153",
	"#154",
	"#155",
	"#156",
	"#157",
	"#158",
	"#159",	
	"#160",
	"#161",
	"#162",
	"#163",
	"#164",
	"#165",
	"#166",
	"#167",
	"#168",
	"#169",	
	"#170",
	"#171",
	"#172",
	"#173",
	"#174",
	"#175",
	"#176",
	"#177",
	"#178",
	"#179",	
	"#180",
	"#181",
	"#182",
	"#183",
	"#184",
	"#185",
	"#186",
	"#187",
	"#188",
	"#189",	
	"#180",
	"#191",
	"#192",
	"#193",
	"#194",
	"#195",
	"#196",
	"#197",
	"#198",
	"#199",	
	"#200",
	"#201",
	"#202",
	"#203",
	"#204",
	"#205",
	"#206",
	"#207",
	"#208",
	"#209",	
	"#210",
	"#211",
	"#212",
	"#213",
	"#214",
	"#215",
	"#216",
	"#217",
	"#218",
	"#219",	
	"#220",
	"#221",
	"#222",
	"#223",
	"#224",
	"#225",
	"#226",
	"#227",
	"#228",
	"#229",	
	"#230",
	"#231",
	"#232",
	"#233",
	"#234",
	"#235",
	"#236",
	"#237",
	"#238",
	"#239",	
	"#240",
	"#241",
	"#242",
	"#243",
	"#244",
	"#245",
	"#246",
	"#247",
	"#248",
	"#249",	
	"#250",
	"#251",
	"#252",
	"#253",
	"#254",
	"#255",
};

/*
 * Dump IP6 statistics structure.
 */
void
ip6_stats(off, name)
	u_long off;
	char *name;
{
	struct ip6stat ip6stat;
	int first, i;

	if (off == 0)
		return;

	kread(off, (char *)&ip6stat, sizeof (ip6stat));
	printf("%s:\n", name);

#define	p(f, m) if (ip6stat.f || sflag <= 1) \
    printf(m, (unsigned long long)ip6stat.f, plural(ip6stat.f))
#define	p1a(f, m) if (ip6stat.f || sflag <= 1) \
    printf(m, (unsigned long long)ip6stat.f)

	p(ip6s_total, "\t%llu total packet%s received\n");
	p1a(ip6s_toosmall, "\t%llu with size smaller than minimum\n");
	p1a(ip6s_tooshort, "\t%llu with data size < data length\n");
	p1a(ip6s_badoptions, "\t%llu with bad options\n");
	p1a(ip6s_badvers, "\t%llu with incorrect version number\n");
	p(ip6s_fragments, "\t%llu fragment%s received\n");
	p(ip6s_fragdropped, "\t%llu fragment%s dropped (dup or out of space)\n");
	p(ip6s_fragtimeout, "\t%llu fragment%s dropped after timeout\n");
	p(ip6s_fragoverflow, "\t%llu fragment%s that exceeded limit\n");
	p(ip6s_reassembled, "\t%llu packet%s reassembled ok\n");
	p(ip6s_delivered, "\t%llu packet%s for this host\n");
	p(ip6s_forward, "\t%llu packet%s forwarded\n");
	p(ip6s_cantforward, "\t%llu packet%s not forwardable\n");
	p(ip6s_redirectsent, "\t%llu redirect%s sent\n");
	p(ip6s_localout, "\t%llu packet%s sent from this host\n");
	p(ip6s_rawout, "\t%llu packet%s sent with fabricated ip header\n");
	p(ip6s_odropped, "\t%llu output packet%s dropped due to no bufs, etc.\n");
	p(ip6s_noroute, "\t%llu output packet%s discarded due to no route\n");
	p(ip6s_fragmented, "\t%llu output datagram%s fragmented\n");
	p(ip6s_ofragments, "\t%llu fragment%s created\n");
	p(ip6s_cantfrag, "\t%llu datagram%s that can't be fragmented\n");
	p(ip6s_badscope, "\t%llu packet%s that violated scope rules\n");
	p(ip6s_notmember, "\t%llu multicast packet%s which we don't join\n");
	for (first = 1, i = 0; i < 256; i++)
		if (ip6stat.ip6s_nxthist[i] != 0) {
			if (first) {
				printf("\tInput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %llu\n", ip6nh[i],
			    (unsigned long long)ip6stat.ip6s_nxthist[i]);
		}
	printf("\tMbuf statistics:\n");
	printf("\t\t%llu one mbuf\n", (unsigned long long)ip6stat.ip6s_m1);
	for (first = 1, i = 0; i < 32; i++) {
		char ifbuf[IFNAMSIZ];
		if (ip6stat.ip6s_m2m[i] != 0) {		
			if (first) {
				printf("\t\ttwo or more mbuf:\n");
				first = 0;
			}
			printf("\t\t\t%s= %llu\n",
			    if_indextoname(i, ifbuf),
			    (unsigned long long)ip6stat.ip6s_m2m[i]);
		}
	}
	printf("\t\t%llu one ext mbuf\n",
	    (unsigned long long)ip6stat.ip6s_mext1);
	printf("\t\t%llu two or more ext mbuf\n",
	    (unsigned long long)ip6stat.ip6s_mext2m);	
	p(ip6s_exthdrtoolong,
	    "\t%llu packet%s whose headers are not continuous\n");
	p(ip6s_nogif, "\t%llu tunneling packet%s that can't find gif\n");
	p(ip6s_toomanyhdr,
	    "\t%llu packet%s discarded due to too may headers\n");

	/* for debugging source address selection */
#define PRINT_SCOPESTAT(s,i) do {\
		switch(i) { /* XXX hardcoding in each case */\
		case 1:\
			p(s, "\t\t%llu node-local%s\n");\
			break;\
		case 2:\
			p(s,"\t\t%llu link-local%s\n");\
			break;\
		case 5:\
			p(s,"\t\t%llu site-local%s\n");\
			break;\
		case 14:\
			p(s,"\t\t%llu global%s\n");\
			break;\
		default:\
			printf("\t\t%llu addresses scope=%x\n",\
			    (unsigned long long)ip6stat.s, i);\
		}\
	} while (0);

	p(ip6s_sources_none,
	  "\t%llu failure%s of source address selection\n");
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat.ip6s_sources_sameif[i]) {
			if (first) {
				printf("\tsource addresses on an outgoing I/F\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_sameif[i], i);
		}
	}
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat.ip6s_sources_otherif[i]) {
			if (first) {
				printf("\tsource addresses on a non-outgoing I/F\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_otherif[i], i);
		}
	}
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat.ip6s_sources_samescope[i]) {
			if (first) {
				printf("\tsource addresses of same scope\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_samescope[i], i);
		}
	}
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat.ip6s_sources_otherscope[i]) {
			if (first) {
				printf("\tsource addresses of a different scope\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_otherscope[i], i);
		}
	}
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat.ip6s_sources_deprecated[i]) {
			if (first) {
				printf("\tdeprecated source addresses\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_deprecated[i], i);
		}
	}

	p1a(ip6s_forward_cachehit, "\t%llu forward cache hit\n");
	p1a(ip6s_forward_cachemiss, "\t%llu forward cache miss\n");
#undef p
#undef p1a
}

/*
 * Dump IPv6 per-interface statistics based on RFC 2465.
 */
void
ip6_ifstats(ifname)
	char *ifname;
{
	struct in6_ifreq ifr;
	int s;
#define	p(f, m) if (ifr.ifr_ifru.ifru_stat.f || sflag <= 1) \
    printf(m, (unsigned long long)ifr.ifr_ifru.ifru_stat.f, plural(ifr.ifr_ifru.ifru_stat.f))
#define	p_5(f, m) if (ifr.ifr_ifru.ifru_stat.f || sflag <= 1) \
    printf(m, (unsigned long long)ip6stat.f)

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		perror("Warning: socket(AF_INET6)");
		return;
	}

	strcpy(ifr.ifr_name, ifname);
	printf("ip6 on %s:\n", ifr.ifr_name);

	if (ioctl(s, SIOCGIFSTAT_IN6, (char *)&ifr) < 0) {
		perror("Warning: ioctl(SIOCGIFSTAT_IN6)");
		goto end;
	}

	p(ifs6_in_receive, "\t%llu total input datagram%s\n");
	p(ifs6_in_hdrerr, "\t%llu datagram%s with invalid header received\n");
	p(ifs6_in_toobig, "\t%llu datagram%s exceeded MTU received\n");
	p(ifs6_in_noroute, "\t%llu datagram%s with no route received\n");
	p(ifs6_in_addrerr, "\t%llu datagram%s with invalid dst received\n");
	p(ifs6_in_protounknown, "\t%llu datagram%s with unknown proto received\n");
	p(ifs6_in_truncated, "\t%llu truncated datagram%s received\n");
	p(ifs6_in_discard, "\t%llu input datagram%s discarded\n");
	p(ifs6_in_deliver,
	  "\t%llu datagram%s delivered to an upper layer protocol\n");
	p(ifs6_out_forward, "\t%llu datagram%s forwarded to this interface\n");
	p(ifs6_out_request,
	  "\t%llu datagram%s sent from an upper layer protocol\n");
	p(ifs6_out_discard, "\t%llu total discarded output datagram%s\n");
	p(ifs6_out_fragok, "\t%llu output datagram%s fragmented\n");
	p(ifs6_out_fragfail, "\t%llu output datagram%s failed on fragment\n");
	p(ifs6_out_fragcreat, "\t%llu output datagram%s succeeded on fragment\n");
	p(ifs6_reass_reqd, "\t%llu incoming datagram%s fragmented\n");
	p(ifs6_reass_ok, "\t%llu datagram%s reassembled\n");
	p(ifs6_reass_fail, "\t%llu datagram%s failed on reassembling\n");
	p(ifs6_in_mcast, "\t%llu multicast datagram%s received\n");
	p(ifs6_out_mcast, "\t%llu multicast datagram%s sent\n");

  end:
	close(s);

#undef p
#undef p_5
}

static	char *icmp6names[] = {
	"#0",
	"unreach",
	"packet too big",
	"time exceed",
	"parameter problem",
	"#5",
	"#6",
	"#7",
	"#8",
	"#9",
	"#10",
	"#11",
	"#12",
	"#13",
	"#14",
	"#15",
	"#16",
	"#17",
	"#18",
	"#19",	
	"#20",
	"#21",
	"#22",
	"#23",
	"#24",
	"#25",
	"#26",
	"#27",
	"#28",
	"#29",	
	"#30",
	"#31",
	"#32",
	"#33",
	"#34",
	"#35",
	"#36",
	"#37",
	"#38",
	"#39",	
	"#40",
	"#41",
	"#42",
	"#43",
	"#44",
	"#45",
	"#46",
	"#47",
	"#48",
	"#49",	
	"#50",
	"#51",
	"#52",
	"#53",
	"#54",
	"#55",
	"#56",
	"#57",
	"#58",
	"#59",	
	"#60",
	"#61",
	"#62",
	"#63",
	"#64",
	"#65",
	"#66",
	"#67",
	"#68",
	"#69",	
	"#70",
	"#71",
	"#72",
	"#73",
	"#74",
	"#75",
	"#76",
	"#77",
	"#78",
	"#79",	
	"#80",
	"#81",
	"#82",
	"#83",
	"#84",
	"#85",
	"#86",
	"#87",
	"#88",
	"#89",	
	"#80",
	"#91",
	"#92",
	"#93",
	"#94",
	"#95",
	"#96",
	"#97",
	"#98",
	"#99",	
	"#100",
	"#101",
	"#102",
	"#103",
	"#104",
	"#105",
	"#106",
	"#107",
	"#108",
	"#109",	
	"#110",
	"#111",
	"#112",
	"#113",
	"#114",
	"#115",
	"#116",
	"#117",
	"#118",
	"#119",	
	"#120",
	"#121",
	"#122",
	"#123",
	"#124",
	"#125",
	"#126",
	"#127",
	"echo",
	"echo reply",	
	"multicast listener query",
	"multicast listener report",
	"multicast listener done",
	"router solicitation",
	"router advertisment",
	"neighbor solicitation",
	"neighbor advertisment",
	"redirect",
	"router renumbering",
	"node information request",
	"node information reply",
	"#141",
	"#142",
	"#143",
	"#144",
	"#145",
	"#146",
	"#147",
	"#148",
	"#149",	
	"#150",
	"#151",
	"#152",
	"#153",
	"#154",
	"#155",
	"#156",
	"#157",
	"#158",
	"#159",	
	"#160",
	"#161",
	"#162",
	"#163",
	"#164",
	"#165",
	"#166",
	"#167",
	"#168",
	"#169",	
	"#170",
	"#171",
	"#172",
	"#173",
	"#174",
	"#175",
	"#176",
	"#177",
	"#178",
	"#179",	
	"#180",
	"#181",
	"#182",
	"#183",
	"#184",
	"#185",
	"#186",
	"#187",
	"#188",
	"#189",	
	"#180",
	"#191",
	"#192",
	"#193",
	"#194",
	"#195",
	"#196",
	"#197",
	"#198",
	"#199",	
	"#200",
	"#201",
	"#202",
	"#203",
	"#204",
	"#205",
	"#206",
	"#207",
	"#208",
	"#209",	
	"#210",
	"#211",
	"#212",
	"#213",
	"#214",
	"#215",
	"#216",
	"#217",
	"#218",
	"#219",	
	"#220",
	"#221",
	"#222",
	"#223",
	"#224",
	"#225",
	"#226",
	"#227",
	"#228",
	"#229",	
	"#230",
	"#231",
	"#232",
	"#233",
	"#234",
	"#235",
	"#236",
	"#237",
	"#238",
	"#239",	
	"#240",
	"#241",
	"#242",
	"#243",
	"#244",
	"#245",
	"#246",
	"#247",
	"#248",
	"#249",	
	"#250",
	"#251",
	"#252",
	"#253",
	"#254",
	"#255",
};

/*
 * Dump ICMP6 statistics.
 */
void
icmp6_stats(off, name)
	u_long off;
	char *name;
{
	struct icmp6stat icmp6stat;
	register int i, first;

	if (off == 0)
		return;
	kread(off, (char *)&icmp6stat, sizeof (icmp6stat));
	printf("%s:\n", name);

#define	p(f, m) if (icmp6stat.f || sflag <= 1) \
    printf(m, (unsigned long long)icmp6stat.f, plural(icmp6stat.f))
#define p_5(f, m) printf(m, (unsigned long long)icmp6stat.f)

	p(icp6s_error, "\t%llu call%s to icmp_error\n");
	p(icp6s_canterror,
	    "\t%llu error%s not generated because old message was icmp error or so\n");
	p(icp6s_toofreq,
	  "\t%llu error%s not generated because rate limitation\n");
	for (first = 1, i = 0; i < 256; i++)
		if (icmp6stat.icp6s_outhist[i] != 0) {
			if (first) {
				printf("\tOutput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %llu\n", icmp6names[i],
			    (unsigned long long)icmp6stat.icp6s_outhist[i]);
		}
	p(icp6s_badcode, "\t%llu message%s with bad code fields\n");
	p(icp6s_tooshort, "\t%llu message%s < minimum length\n");
	p(icp6s_checksum, "\t%llu bad checksum%s\n");
	p(icp6s_badlen, "\t%llu message%s with bad length\n");
	for (first = 1, i = 0; i < ICMP6_MAXTYPE; i++)
		if (icmp6stat.icp6s_inhist[i] != 0) {
			if (first) {
				printf("\tInput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %llu\n", icmp6names[i],
			    (unsigned long long)icmp6stat.icp6s_inhist[i]);
		}
	printf("\tHistgram of error messages to be generated:\n");
	p_5(icp6s_odst_unreach_noroute, "\t\t%llu no route\n");
	p_5(icp6s_odst_unreach_admin, "\t\t%llu administratively prohibited\n");
	p_5(icp6s_odst_unreach_beyondscope, "\t\t%llu beyond scope\n");
	p_5(icp6s_odst_unreach_addr, "\t\t%llu address unreachable\n");
	p_5(icp6s_odst_unreach_noport, "\t\t%llu port unreachable\n");
	p_5(icp6s_opacket_too_big, "\t\t%llu packet too big\n");
	p_5(icp6s_otime_exceed_transit, "\t\t%llu time exceed transit\n");
	p_5(icp6s_otime_exceed_reassembly, "\t\t%llu time exceed reassembly\n");
	p_5(icp6s_oparamprob_header, "\t\t%llu erroneous header field\n");
	p_5(icp6s_oparamprob_nextheader, "\t\t%llu unrecognized next header\n");
	p_5(icp6s_oparamprob_option, "\t\t%llu unrecognized option\n");
	p_5(icp6s_oredirect, "\t\t%llu redirect\n");
	p_5(icp6s_ounknown, "\t\t%llu unknown\n");

	p(icp6s_reflect, "\t%llu message response%s generated\n");
	p(icp6s_nd_toomanyopt, "\t%llu message%s with too many ND options\n");
#undef p
#undef p_5
}

/*
 * Dump ICMPv6 per-interface statistics based on RFC 2466.
 */
void
icmp6_ifstats(ifname)
	char *ifname;
{
	struct in6_ifreq ifr;
	int s;
#define	p(f, m) if (ifr.ifr_ifru.ifru_icmp6stat.f || sflag <= 1) \
    printf(m, (unsigned long long)ifr.ifr_ifru.ifru_icmp6stat.f, plural(ifr.ifr_ifru.ifru_icmp6stat.f))

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		perror("Warning: socket(AF_INET6)");
		return;
	}

	strcpy(ifr.ifr_name, ifname);
	printf("icmp6 on %s:\n", ifr.ifr_name);

	if (ioctl(s, SIOCGIFSTAT_ICMP6, (char *)&ifr) < 0) {
		perror("Warning: ioctl(SIOCGIFSTAT_ICMP6)");
		goto end;
	}

	p(ifs6_in_msg, "\t%llu total input message%s\n");
	p(ifs6_in_error, "\t%llu total input error message%s\n"); 
	p(ifs6_in_dstunreach, "\t%llu input destination unreachable error%s\n");
	p(ifs6_in_adminprohib, "\t%llu input administratively prohibited error%s\n");
	p(ifs6_in_timeexceed, "\t%llu input time exceeded error%s\n");
	p(ifs6_in_paramprob, "\t%llu input parameter problem error%s\n");
	p(ifs6_in_pkttoobig, "\t%llu input packet too big error%s\n");
	p(ifs6_in_echo, "\t%llu input echo request%s\n");
	p(ifs6_in_echoreply, "\t%llu input echo reply%s\n");
	p(ifs6_in_routersolicit, "\t%llu input router solicitation%s\n");
	p(ifs6_in_routeradvert, "\t%llu input router advertisement%s\n");
	p(ifs6_in_neighborsolicit, "\t%llu input neighbor solicitation%s\n");
	p(ifs6_in_neighboradvert, "\t%llu input neighbor advertisement%s\n");
	p(ifs6_in_redirect, "\t%llu input redirect%s\n");
	p(ifs6_in_mldquery, "\t%llu input MLD query%s\n");
	p(ifs6_in_mldreport, "\t%llu input MLD report%s\n");
	p(ifs6_in_mlddone, "\t%llu input MLD done%s\n");

	p(ifs6_out_msg, "\t%llu total output message%s\n");
	p(ifs6_out_error, "\t%llu total output error message%s\n");
	p(ifs6_out_dstunreach, "\t%llu output destination unreachable error%s\n");
	p(ifs6_out_adminprohib, "\t%llu output administratively prohibited error%s\n");
	p(ifs6_out_timeexceed, "\t%llu output time exceeded error%s\n");
	p(ifs6_out_paramprob, "\t%llu output parameter problem error%s\n");
	p(ifs6_out_pkttoobig, "\t%llu output packet too big error%s\n");
	p(ifs6_out_echo, "\t%llu output echo request%s\n");
	p(ifs6_out_echoreply, "\t%llu output echo reply%s\n");
	p(ifs6_out_routersolicit, "\t%llu output router solicitation%s\n");
	p(ifs6_out_routeradvert, "\t%llu output router advertisement%s\n");
	p(ifs6_out_neighborsolicit, "\t%llu output neighbor solicitation%s\n");
	p(ifs6_out_neighboradvert, "\t%llu output neighbor advertisement%s\n");
	p(ifs6_out_redirect, "\t%llu output redirect%s\n");
	p(ifs6_out_mldquery, "\t%llu output MLD query%s\n");
	p(ifs6_out_mldreport, "\t%llu output MLD report%s\n");
	p(ifs6_out_mlddone, "\t%llu output MLD done%s\n");

  end:
	close(s);
#undef p
}

/*
 * Dump PIM statistics structure.
 */
void
pim6_stats(off, name)
	u_long off;
	char *name;
{
	struct pim6stat pim6stat;

	if (off == 0)
		return;
	kread(off, (char *)&pim6stat, sizeof(pim6stat));
	printf("%s:\n", name);

#define	p(f, m) if (pim6stat.f || sflag <= 1) \
    printf(m, (unsigned long long)pim6stat.f, plural(pim6stat.f))
	p(pim6s_rcv_total, "\t%llu message%s received\n");
	p(pim6s_rcv_tooshort, "\t%llu message%s received with too few bytes\n");
	p(pim6s_rcv_badsum, "\t%llu message%s received with bad checksum\n");
	p(pim6s_rcv_badversion, "\t%llu message%s received with bad version\n");
	p(pim6s_rcv_registers, "\t%llu register%s received\n");
	p(pim6s_rcv_badregisters, "\t%llu bad register%s received\n");
	p(pim6s_snd_registers, "\t%llu register%s sent\n");
#undef p
}

/*
 * Pretty print an Internet address (net address + port).
 * If the nflag was specified, use numbers instead of names.
 */
#define GETSERVBYPORT6(port, proto, ret)\
{\
	if (strcmp((proto), "tcp6") == 0)\
		(ret) = getservbyport((int)(port), "tcp");\
	else if (strcmp((proto), "udp6") == 0)\
		(ret) = getservbyport((int)(port), "udp");\
	else\
		(ret) = getservbyport((int)(port), (proto));\
};

void
inet6print(in6, port, proto, numeric)
	register struct in6_addr *in6;
	int port;
	char *proto;
	int numeric;
{
	struct servent *sp = 0;
	char line[80], *cp;
	int width;

	sprintf(line, "%.*s.", lflag ? 39 :
		(Aflag && !numeric) ? 12 : 16, inet6name(in6));
	cp = index(line, '\0');
	if (!numeric && port)
		GETSERVBYPORT6(port, proto, sp);
	if (sp || port == 0)
		sprintf(cp, "%.8s", sp ? sp->s_name : "*");
	else
		sprintf(cp, "%d", ntohs((u_short)port));
	width = lflag ? 45 : Aflag ? 18 : 22;
	printf("%-*.*s ", width, width, line);
}

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */

char *
inet6name(in6p)
	struct in6_addr *in6p;
{
	register char *cp;
	static char line[50];
	struct hostent *hp;
	static char domain[MAXHOSTNAMELEN + 1];
	static int first = 1;

	if (first && !nflag) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = index(domain, '.')))
			(void) strcpy(domain, cp + 1);
		else
			domain[0] = 0;
	}
	cp = 0;
	if (!nflag && !IN6_IS_ADDR_UNSPECIFIED(in6p)) {
		hp = gethostbyaddr((char *)in6p, sizeof(*in6p), AF_INET6);
		if (hp) {
			if ((cp = index(hp->h_name, '.')) &&
			    !strcmp(cp + 1, domain))
				*cp = 0;
			cp = hp->h_name;
		}
	}
	if (IN6_IS_ADDR_UNSPECIFIED(in6p))
		strcpy(line, "*");
	else if (cp)
		strcpy(line, cp);
	else 
		sprintf(line, "%s",
			inet_ntop(AF_INET6, (void *)in6p, ntop_buf,
				sizeof(ntop_buf)));
	return (line);
}
#endif /*INET6*/
