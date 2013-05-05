/*	BSDI inet.c,v 2.3 1995/10/24 02:19:29 prb Exp	*/
/*-
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
 */

#if 0
#ifndef lint
static char sccsid[] = "@(#)inet6.c	8.4 (Berkeley) 4/20/94";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef INET6
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/sysctl.h>

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
#include <netinet6/raw_ip6.h>

#include <arpa/inet.h>
#include <netdb.h>

#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "netstat.h"

struct	socket sockb;

char	*inet6name(struct in6_addr *);

static char ntop_buf[INET6_ADDRSTRLEN];

static	const char *ip6nh[] = {
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
	"mobility",
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

static const char *srcrule_str[] = {
	"first candidate",
	"same address",
	"appropriate scope",
	"deprecated address",
	"home address",
	"outgoing interface",
	"matching label",
	"public/temporary address",
	"alive interface",
	"preferred interface",
	"rule #10",
	"rule #11",
	"rule #12",
	"rule #13",
	"longest match",
	"rule #15",
};

/*
 * Dump IP6 statistics structure.
 */
void
ip6_stats(u_long off, const char *name, int af1 __unused, int proto __unused)
{
	struct ip6stat ip6stat, zerostat;
	int first, i;
	size_t len;

	len = sizeof ip6stat;
	if (live) {
		memset(&ip6stat, 0, len);
		if (zflag)
			memset(&zerostat, 0, len);
		if (sysctlbyname("net.inet6.ip6.stats", &ip6stat, &len,
		    zflag ? &zerostat : NULL, zflag ? len : 0) < 0) {
			if (errno != ENOENT)
				warn("sysctl: net.inet6.ip6.stats");
			return;
		}
	} else
		kread(off, &ip6stat, len);

	printf("%s:\n", name);

#define	p(f, m) if (ip6stat.f || sflag <= 1) \
    printf(m, (uintmax_t)ip6stat.f, plural(ip6stat.f))
#define	p1a(f, m) if (ip6stat.f || sflag <= 1) \
    printf(m, (uintmax_t)ip6stat.f)

	p(ip6s_total, "\t%ju total packet%s received\n");
	p1a(ip6s_toosmall, "\t%ju with size smaller than minimum\n");
	p1a(ip6s_tooshort, "\t%ju with data size < data length\n");
	p1a(ip6s_badoptions, "\t%ju with bad options\n");
	p1a(ip6s_badvers, "\t%ju with incorrect version number\n");
	p(ip6s_fragments, "\t%ju fragment%s received\n");
	p(ip6s_fragdropped, "\t%ju fragment%s dropped (dup or out of space)\n");
	p(ip6s_fragtimeout, "\t%ju fragment%s dropped after timeout\n");
	p(ip6s_fragoverflow, "\t%ju fragment%s that exceeded limit\n");
	p(ip6s_reassembled, "\t%ju packet%s reassembled ok\n");
	p(ip6s_delivered, "\t%ju packet%s for this host\n");
	p(ip6s_forward, "\t%ju packet%s forwarded\n");
	p(ip6s_cantforward, "\t%ju packet%s not forwardable\n");
	p(ip6s_redirectsent, "\t%ju redirect%s sent\n");
	p(ip6s_localout, "\t%ju packet%s sent from this host\n");
	p(ip6s_rawout, "\t%ju packet%s sent with fabricated ip header\n");
	p(ip6s_odropped, "\t%ju output packet%s dropped due to no bufs, etc.\n");
	p(ip6s_noroute, "\t%ju output packet%s discarded due to no route\n");
	p(ip6s_fragmented, "\t%ju output datagram%s fragmented\n");
	p(ip6s_ofragments, "\t%ju fragment%s created\n");
	p(ip6s_cantfrag, "\t%ju datagram%s that can't be fragmented\n");
	p(ip6s_badscope, "\t%ju packet%s that violated scope rules\n");
	p(ip6s_notmember, "\t%ju multicast packet%s which we don't join\n");
	for (first = 1, i = 0; i < IP6S_HDRCNT; i++)
		if (ip6stat.ip6s_nxthist[i] != 0) {
			if (first) {
				printf("\tInput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %ju\n", ip6nh[i],
			    (uintmax_t)ip6stat.ip6s_nxthist[i]);
		}
	printf("\tMbuf statistics:\n");
	printf("\t\t%ju one mbuf\n", (uintmax_t)ip6stat.ip6s_m1);
	for (first = 1, i = 0; i < IP6S_M2MMAX; i++) {
		char ifbuf[IFNAMSIZ];
		if (ip6stat.ip6s_m2m[i] != 0) {
			if (first) {
				printf("\t\ttwo or more mbuf:\n");
				first = 0;
			}
			printf("\t\t\t%s= %ju\n",
			    if_indextoname(i, ifbuf),
			    (uintmax_t)ip6stat.ip6s_m2m[i]);
		}
	}
	printf("\t\t%ju one ext mbuf\n",
	    (uintmax_t)ip6stat.ip6s_mext1);
	printf("\t\t%ju two or more ext mbuf\n",
	    (uintmax_t)ip6stat.ip6s_mext2m);
	p(ip6s_exthdrtoolong,
	    "\t%ju packet%s whose headers are not contiguous\n");
	p(ip6s_nogif, "\t%ju tunneling packet%s that can't find gif\n");
	p(ip6s_toomanyhdr,
	    "\t%ju packet%s discarded because of too many headers\n");

	/* for debugging source address selection */
#define	PRINT_SCOPESTAT(s,i) do {\
		switch(i) { /* XXX hardcoding in each case */\
		case 1:\
			p(s, "\t\t%ju interface-local%s\n");\
			break;\
		case 2:\
			p(s,"\t\t%ju link-local%s\n");\
			break;\
		case 5:\
			p(s,"\t\t%ju site-local%s\n");\
			break;\
		case 14:\
			p(s,"\t\t%ju global%s\n");\
			break;\
		default:\
			printf("\t\t%ju addresses scope=%x\n",\
			    (uintmax_t)ip6stat.s, i);\
		}\
	} while (0);

	p(ip6s_sources_none,
	  "\t%ju failure%s of source address selection\n");
	for (first = 1, i = 0; i < IP6S_SCOPECNT; i++) {
		if (ip6stat.ip6s_sources_sameif[i]) {
			if (first) {
				printf("\tsource addresses on an outgoing I/F\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_sameif[i], i);
		}
	}
	for (first = 1, i = 0; i < IP6S_SCOPECNT; i++) {
		if (ip6stat.ip6s_sources_otherif[i]) {
			if (first) {
				printf("\tsource addresses on a non-outgoing I/F\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_otherif[i], i);
		}
	}
	for (first = 1, i = 0; i < IP6S_SCOPECNT; i++) {
		if (ip6stat.ip6s_sources_samescope[i]) {
			if (first) {
				printf("\tsource addresses of same scope\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_samescope[i], i);
		}
	}
	for (first = 1, i = 0; i < IP6S_SCOPECNT; i++) {
		if (ip6stat.ip6s_sources_otherscope[i]) {
			if (first) {
				printf("\tsource addresses of a different scope\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_otherscope[i], i);
		}
	}
	for (first = 1, i = 0; i < IP6S_SCOPECNT; i++) {
		if (ip6stat.ip6s_sources_deprecated[i]) {
			if (first) {
				printf("\tdeprecated source addresses\n");
				first = 0;
			}
			PRINT_SCOPESTAT(ip6s_sources_deprecated[i], i);
		}
	}

	printf("\tSource addresses selection rule applied:\n");
	for (i = 0; i < IP6S_RULESMAX; i++) {
		if (ip6stat.ip6s_sources_rule[i])
			printf("\t\t%ju %s\n",
			       (uintmax_t)ip6stat.ip6s_sources_rule[i],
			       srcrule_str[i]);
	}
#undef p
#undef p1a
}

/*
 * Dump IPv6 per-interface statistics based on RFC 2465.
 */
void
ip6_ifstats(char *ifname)
{
	struct in6_ifreq ifr;
	int s;
#define	p(f, m) if (ifr.ifr_ifru.ifru_stat.f || sflag <= 1) \
    printf(m, (uintmax_t)ifr.ifr_ifru.ifru_stat.f, plural(ifr.ifr_ifru.ifru_stat.f))
#define	p_5(f, m) if (ifr.ifr_ifru.ifru_stat.f || sflag <= 1) \
    printf(m, (uintmax_t)ip6stat.f)

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

	p(ifs6_in_receive, "\t%ju total input datagram%s\n");
	p(ifs6_in_hdrerr, "\t%ju datagram%s with invalid header received\n");
	p(ifs6_in_toobig, "\t%ju datagram%s exceeded MTU received\n");
	p(ifs6_in_noroute, "\t%ju datagram%s with no route received\n");
	p(ifs6_in_addrerr, "\t%ju datagram%s with invalid dst received\n");
	p(ifs6_in_protounknown, "\t%ju datagram%s with unknown proto received\n");
	p(ifs6_in_truncated, "\t%ju truncated datagram%s received\n");
	p(ifs6_in_discard, "\t%ju input datagram%s discarded\n");
	p(ifs6_in_deliver,
	  "\t%ju datagram%s delivered to an upper layer protocol\n");
	p(ifs6_out_forward, "\t%ju datagram%s forwarded to this interface\n");
	p(ifs6_out_request,
	  "\t%ju datagram%s sent from an upper layer protocol\n");
	p(ifs6_out_discard, "\t%ju total discarded output datagram%s\n");
	p(ifs6_out_fragok, "\t%ju output datagram%s fragmented\n");
	p(ifs6_out_fragfail, "\t%ju output datagram%s failed on fragment\n");
	p(ifs6_out_fragcreat, "\t%ju output datagram%s succeeded on fragment\n");
	p(ifs6_reass_reqd, "\t%ju incoming datagram%s fragmented\n");
	p(ifs6_reass_ok, "\t%ju datagram%s reassembled\n");
	p(ifs6_reass_fail, "\t%ju datagram%s failed on reassembly\n");
	p(ifs6_in_mcast, "\t%ju multicast datagram%s received\n");
	p(ifs6_out_mcast, "\t%ju multicast datagram%s sent\n");

  end:
	close(s);

#undef p
#undef p_5
}

static	const char *icmp6names[] = {
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
	"MLDv1 listener report",
	"MLDv1 listener done",
	"router solicitation",
	"router advertisement",
	"neighbor solicitation",
	"neighbor advertisement",
	"redirect",
	"router renumbering",
	"node information request",
	"node information reply",
	"inverse neighbor solicitation",
	"inverse neighbor advertisement",
	"MLDv2 listener report",
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
icmp6_stats(u_long off, const char *name, int af1 __unused, int proto __unused)
{
	struct icmp6stat icmp6stat, zerostat;
	int i, first;
	size_t len;

	len = sizeof icmp6stat;
	if (live) {
		memset(&icmp6stat, 0, len);
		if (zflag)
			memset(&zerostat, 0, len);
		if (sysctlbyname("net.inet6.icmp6.stats", &icmp6stat, &len,
		    zflag ? &zerostat : NULL, zflag ? len : 0) < 0) {
			if (errno != ENOENT)
				warn("sysctl: net.inet6.icmp6.stats");
			return;
		}
	} else
		kread(off, &icmp6stat, len);

	printf("%s:\n", name);

#define	p(f, m) if (icmp6stat.f || sflag <= 1) \
    printf(m, (uintmax_t)icmp6stat.f, plural(icmp6stat.f))
#define	p_5(f, m) if (icmp6stat.f || sflag <= 1) \
    printf(m, (uintmax_t)icmp6stat.f)

	p(icp6s_error, "\t%ju call%s to icmp6_error\n");
	p(icp6s_canterror,
	    "\t%ju error%s not generated in response to an icmp6 message\n");
	p(icp6s_toofreq,
	  "\t%ju error%s not generated because of rate limitation\n");
#define	NELEM (int)(sizeof(icmp6stat.icp6s_outhist)/sizeof(icmp6stat.icp6s_outhist[0]))
	for (first = 1, i = 0; i < NELEM; i++)
		if (icmp6stat.icp6s_outhist[i] != 0) {
			if (first) {
				printf("\tOutput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %ju\n", icmp6names[i],
			    (uintmax_t)icmp6stat.icp6s_outhist[i]);
		}
#undef NELEM
	p(icp6s_badcode, "\t%ju message%s with bad code fields\n");
	p(icp6s_tooshort, "\t%ju message%s < minimum length\n");
	p(icp6s_checksum, "\t%ju bad checksum%s\n");
	p(icp6s_badlen, "\t%ju message%s with bad length\n");
#define	NELEM (int)(sizeof(icmp6stat.icp6s_inhist)/sizeof(icmp6stat.icp6s_inhist[0]))
	for (first = 1, i = 0; i < NELEM; i++)
		if (icmp6stat.icp6s_inhist[i] != 0) {
			if (first) {
				printf("\tInput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %ju\n", icmp6names[i],
			    (uintmax_t)icmp6stat.icp6s_inhist[i]);
		}
#undef NELEM
	printf("\tHistogram of error messages to be generated:\n");
	p_5(icp6s_odst_unreach_noroute, "\t\t%ju no route\n");
	p_5(icp6s_odst_unreach_admin, "\t\t%ju administratively prohibited\n");
	p_5(icp6s_odst_unreach_beyondscope, "\t\t%ju beyond scope\n");
	p_5(icp6s_odst_unreach_addr, "\t\t%ju address unreachable\n");
	p_5(icp6s_odst_unreach_noport, "\t\t%ju port unreachable\n");
	p_5(icp6s_opacket_too_big, "\t\t%ju packet too big\n");
	p_5(icp6s_otime_exceed_transit, "\t\t%ju time exceed transit\n");
	p_5(icp6s_otime_exceed_reassembly, "\t\t%ju time exceed reassembly\n");
	p_5(icp6s_oparamprob_header, "\t\t%ju erroneous header field\n");
	p_5(icp6s_oparamprob_nextheader, "\t\t%ju unrecognized next header\n");
	p_5(icp6s_oparamprob_option, "\t\t%ju unrecognized option\n");
	p_5(icp6s_oredirect, "\t\t%ju redirect\n");
	p_5(icp6s_ounknown, "\t\t%ju unknown\n");

	p(icp6s_reflect, "\t%ju message response%s generated\n");
	p(icp6s_nd_toomanyopt, "\t%ju message%s with too many ND options\n");
	p(icp6s_nd_badopt, "\t%ju message%s with bad ND options\n");
	p(icp6s_badns, "\t%ju bad neighbor solicitation message%s\n");
	p(icp6s_badna, "\t%ju bad neighbor advertisement message%s\n");
	p(icp6s_badrs, "\t%ju bad router solicitation message%s\n");
	p(icp6s_badra, "\t%ju bad router advertisement message%s\n");
	p(icp6s_badredirect, "\t%ju bad redirect message%s\n");
	p(icp6s_pmtuchg, "\t%ju path MTU change%s\n");
#undef p
#undef p_5
}

/*
 * Dump ICMPv6 per-interface statistics based on RFC 2466.
 */
void
icmp6_ifstats(char *ifname)
{
	struct in6_ifreq ifr;
	int s;
#define	p(f, m) if (ifr.ifr_ifru.ifru_icmp6stat.f || sflag <= 1) \
    printf(m, (uintmax_t)ifr.ifr_ifru.ifru_icmp6stat.f, plural(ifr.ifr_ifru.ifru_icmp6stat.f))
#define	p2(f, m) if (ifr.ifr_ifru.ifru_icmp6stat.f || sflag <= 1) \
    printf(m, (uintmax_t)ifr.ifr_ifru.ifru_icmp6stat.f, pluralies(ifr.ifr_ifru.ifru_icmp6stat.f))

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

	p(ifs6_in_msg, "\t%ju total input message%s\n");
	p(ifs6_in_error, "\t%ju total input error message%s\n");
	p(ifs6_in_dstunreach, "\t%ju input destination unreachable error%s\n");
	p(ifs6_in_adminprohib, "\t%ju input administratively prohibited error%s\n");
	p(ifs6_in_timeexceed, "\t%ju input time exceeded error%s\n");
	p(ifs6_in_paramprob, "\t%ju input parameter problem error%s\n");
	p(ifs6_in_pkttoobig, "\t%ju input packet too big error%s\n");
	p(ifs6_in_echo, "\t%ju input echo request%s\n");
	p2(ifs6_in_echoreply, "\t%ju input echo repl%s\n");
	p(ifs6_in_routersolicit, "\t%ju input router solicitation%s\n");
	p(ifs6_in_routeradvert, "\t%ju input router advertisement%s\n");
	p(ifs6_in_neighborsolicit, "\t%ju input neighbor solicitation%s\n");
	p(ifs6_in_neighboradvert, "\t%ju input neighbor advertisement%s\n");
	p(ifs6_in_redirect, "\t%ju input redirect%s\n");
	p2(ifs6_in_mldquery, "\t%ju input MLD quer%s\n");
	p(ifs6_in_mldreport, "\t%ju input MLD report%s\n");
	p(ifs6_in_mlddone, "\t%ju input MLD done%s\n");

	p(ifs6_out_msg, "\t%ju total output message%s\n");
	p(ifs6_out_error, "\t%ju total output error message%s\n");
	p(ifs6_out_dstunreach, "\t%ju output destination unreachable error%s\n");
	p(ifs6_out_adminprohib, "\t%ju output administratively prohibited error%s\n");
	p(ifs6_out_timeexceed, "\t%ju output time exceeded error%s\n");
	p(ifs6_out_paramprob, "\t%ju output parameter problem error%s\n");
	p(ifs6_out_pkttoobig, "\t%ju output packet too big error%s\n");
	p(ifs6_out_echo, "\t%ju output echo request%s\n");
	p2(ifs6_out_echoreply, "\t%ju output echo repl%s\n");
	p(ifs6_out_routersolicit, "\t%ju output router solicitation%s\n");
	p(ifs6_out_routeradvert, "\t%ju output router advertisement%s\n");
	p(ifs6_out_neighborsolicit, "\t%ju output neighbor solicitation%s\n");
	p(ifs6_out_neighboradvert, "\t%ju output neighbor advertisement%s\n");
	p(ifs6_out_redirect, "\t%ju output redirect%s\n");
	p2(ifs6_out_mldquery, "\t%ju output MLD quer%s\n");
	p(ifs6_out_mldreport, "\t%ju output MLD report%s\n");
	p(ifs6_out_mlddone, "\t%ju output MLD done%s\n");

  end:
	close(s);
#undef p
}

/*
 * Dump PIM statistics structure.
 */
void
pim6_stats(u_long off, const char *name, int af1 __unused, int proto __unused)
{
	struct pim6stat pim6stat, zerostat;
	size_t len = sizeof pim6stat;

	if (live) {
		if (zflag)
			memset(&zerostat, 0, len);
		if (sysctlbyname("net.inet6.pim.stats", &pim6stat, &len,
		    zflag ? &zerostat : NULL, zflag ? len : 0) < 0) {
			if (errno != ENOENT)
				warn("sysctl: net.inet6.pim.stats");
			return;
		}
	} else {
		if (off == 0)
			return;
		kread(off, &pim6stat, len);
	}

	printf("%s:\n", name);

#define	p(f, m) if (pim6stat.f || sflag <= 1) \
    printf(m, (uintmax_t)pim6stat.f, plural(pim6stat.f))
	p(pim6s_rcv_total, "\t%ju message%s received\n");
	p(pim6s_rcv_tooshort, "\t%ju message%s received with too few bytes\n");
	p(pim6s_rcv_badsum, "\t%ju message%s received with bad checksum\n");
	p(pim6s_rcv_badversion, "\t%ju message%s received with bad version\n");
	p(pim6s_rcv_registers, "\t%ju register%s received\n");
	p(pim6s_rcv_badregisters, "\t%ju bad register%s received\n");
	p(pim6s_snd_registers, "\t%ju register%s sent\n");
#undef p
}

/*
 * Dump raw ip6 statistics structure.
 */
void
rip6_stats(u_long off, const char *name, int af1 __unused, int proto __unused)
{
	struct rip6stat rip6stat, zerostat;
	u_quad_t delivered;
	size_t len;

	len = sizeof(rip6stat);
	if (live) {
		if (zflag)
			memset(&zerostat, 0, len);
		if (sysctlbyname("net.inet6.ip6.rip6stats", &rip6stat, &len,
		    zflag ? &zerostat : NULL, zflag ? len : 0) < 0) {
			if (errno != ENOENT)
				warn("sysctl: net.inet6.ip6.rip6stats");
			return;
		}
	} else
		kread(off, &rip6stat, len);

	printf("%s:\n", name);

#define	p(f, m) if (rip6stat.f || sflag <= 1) \
    printf(m, (uintmax_t)rip6stat.f, plural(rip6stat.f))
	p(rip6s_ipackets, "\t%ju message%s received\n");
	p(rip6s_isum, "\t%ju checksum calculation%s on inbound\n");
	p(rip6s_badsum, "\t%ju message%s with bad checksum\n");
	p(rip6s_nosock, "\t%ju message%s dropped due to no socket\n");
	p(rip6s_nosockmcast,
	    "\t%ju multicast message%s dropped due to no socket\n");
	p(rip6s_fullsock,
	    "\t%ju message%s dropped due to full socket buffers\n");
	delivered = rip6stat.rip6s_ipackets -
		    rip6stat.rip6s_badsum -
		    rip6stat.rip6s_nosock -
		    rip6stat.rip6s_nosockmcast -
		    rip6stat.rip6s_fullsock;
	if (delivered || sflag <= 1)
		printf("\t%ju delivered\n", (uintmax_t)delivered);
	p(rip6s_opackets, "\t%ju datagram%s output\n");
#undef p
}

/*
 * Pretty print an Internet address (net address + port).
 * Take numeric_addr and numeric_port into consideration.
 */
#define	GETSERVBYPORT6(port, proto, ret)\
{\
	if (strcmp((proto), "tcp6") == 0)\
		(ret) = getservbyport((int)(port), "tcp");\
	else if (strcmp((proto), "udp6") == 0)\
		(ret) = getservbyport((int)(port), "udp");\
	else\
		(ret) = getservbyport((int)(port), (proto));\
};

void
inet6print(struct in6_addr *in6, int port, const char *proto, int numeric)
{
	struct servent *sp = 0;
	char line[80], *cp;
	int width;

	sprintf(line, "%.*s.", Wflag ? 39 :
		(Aflag && !numeric) ? 12 : 16, inet6name(in6));
	cp = strchr(line, '\0');
	if (!numeric && port)
		GETSERVBYPORT6(port, proto, sp);
	if (sp || port == 0)
		sprintf(cp, "%.15s", sp ? sp->s_name : "*");
	else
		sprintf(cp, "%d", ntohs((u_short)port));
	width = Wflag ? 45 : Aflag ? 18 : 22;
	printf("%-*.*s ", width, width, line);
}

/*
 * Construct an Internet address representation.
 * If the numeric_addr has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */

char *
inet6name(struct in6_addr *in6p)
{
	char *cp;
	static char line[50];
	struct hostent *hp;
	static char domain[MAXHOSTNAMELEN];
	static int first = 1;

	if (first && !numeric_addr) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = strchr(domain, '.')))
			(void) strcpy(domain, cp + 1);
		else
			domain[0] = 0;
	}
	cp = 0;
	if (!numeric_addr && !IN6_IS_ADDR_UNSPECIFIED(in6p)) {
		hp = gethostbyaddr((char *)in6p, sizeof(*in6p), AF_INET6);
		if (hp) {
			if ((cp = strchr(hp->h_name, '.')) &&
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
