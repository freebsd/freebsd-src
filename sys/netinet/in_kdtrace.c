/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 Mark Johnston <markj@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sdt.h>

SDT_PROVIDER_DEFINE(mib);
SDT_PROVIDER_DEFINE(ip);
SDT_PROVIDER_DEFINE(tcp);
SDT_PROVIDER_DEFINE(udp);
SDT_PROVIDER_DEFINE(udplite);

#ifndef KDTRACE_NO_MIB_SDT
#define MIB_PROBE_IP(name) \
    SDT_PROBE_DEFINE1(mib, ip, count, name, \
        "int")

MIB_PROBE_IP(ips_total);
MIB_PROBE_IP(ips_badsum);
MIB_PROBE_IP(ips_tooshort);
MIB_PROBE_IP(ips_toosmall);
MIB_PROBE_IP(ips_badhlen);
MIB_PROBE_IP(ips_badlen);
MIB_PROBE_IP(ips_fragments);
MIB_PROBE_IP(ips_fragdropped);
MIB_PROBE_IP(ips_fragtimeout);
MIB_PROBE_IP(ips_forward);
MIB_PROBE_IP(ips_fastforward);
MIB_PROBE_IP(ips_cantforward);
MIB_PROBE_IP(ips_redirectsent);
MIB_PROBE_IP(ips_noproto);
MIB_PROBE_IP(ips_delivered);
MIB_PROBE_IP(ips_localout);
MIB_PROBE_IP(ips_odropped);
MIB_PROBE_IP(ips_reassembled);
MIB_PROBE_IP(ips_fragmented);
MIB_PROBE_IP(ips_ofragments);
MIB_PROBE_IP(ips_cantfrag);
MIB_PROBE_IP(ips_badoptions);
MIB_PROBE_IP(ips_noroute);
MIB_PROBE_IP(ips_badvers);
MIB_PROBE_IP(ips_rawout);
MIB_PROBE_IP(ips_toolong);
MIB_PROBE_IP(ips_notmember);
MIB_PROBE_IP(ips_nogif);
MIB_PROBE_IP(ips_badaddr);

#define MIB_PROBE_IP6(name) \
    SDT_PROBE_DEFINE1(mib, ip6, count, name, \
        "int")
#define MIB_PROBE2_IP6(name) \
    SDT_PROBE_DEFINE2(mib, ip6, count, name, \
        "int", "int")

MIB_PROBE_IP6(ip6s_total);
MIB_PROBE_IP6(ip6s_tooshort);
MIB_PROBE_IP6(ip6s_toosmall);
MIB_PROBE_IP6(ip6s_fragments);
MIB_PROBE_IP6(ip6s_fragdropped);
MIB_PROBE_IP6(ip6s_fragtimeout);
MIB_PROBE_IP6(ip6s_fragoverflow);
MIB_PROBE_IP6(ip6s_forward);
MIB_PROBE_IP6(ip6s_cantforward);
MIB_PROBE_IP6(ip6s_redirectsent);
MIB_PROBE_IP6(ip6s_delivered);
MIB_PROBE_IP6(ip6s_localout);
MIB_PROBE_IP6(ip6s_odropped);
MIB_PROBE_IP6(ip6s_reassembled);
MIB_PROBE_IP6(ip6s_atomicfrags);
MIB_PROBE_IP6(ip6s_fragmented);
MIB_PROBE_IP6(ip6s_ofragments);
MIB_PROBE_IP6(ip6s_cantfrag);
MIB_PROBE_IP6(ip6s_badoptions);
MIB_PROBE_IP6(ip6s_noroute);
MIB_PROBE_IP6(ip6s_badvers);
MIB_PROBE_IP6(ip6s_rawout);
MIB_PROBE_IP6(ip6s_badscope);
MIB_PROBE_IP6(ip6s_notmember);
MIB_PROBE2_IP6(ip6s_nxthist);
MIB_PROBE_IP6(ip6s_m1);
MIB_PROBE2_IP6(ip6s_m2m);
MIB_PROBE_IP6(ip6s_mext1);
MIB_PROBE_IP6(ip6s_mext2m);
MIB_PROBE_IP6(ip6s_exthdrtoolong);
MIB_PROBE_IP6(ip6s_nogif);
MIB_PROBE_IP6(ip6s_toomanyhdr);
MIB_PROBE_IP6(ip6s_sources_none);
MIB_PROBE2_IP6(ip6s_sources_sameif);
MIB_PROBE2_IP6(ip6s_sources_otherif);
MIB_PROBE2_IP6(ip6s_sources_samescope);
MIB_PROBE2_IP6(ip6s_sources_otherscope);
MIB_PROBE2_IP6(ip6s_sources_deprecated);
MIB_PROBE2_IP6(ip6s_sources_rule);

#define MIB_PROBE_ICMP(name) \
    SDT_PROBE_DEFINE1(mib, icmp, count, name, \
        "int")
#define MIB_PROBE2_ICMP(name) \
    SDT_PROBE_DEFINE2(mib, icmp, count, name, \
        "int", "int")

MIB_PROBE_ICMP(icps_error);
MIB_PROBE_ICMP(icps_oldshort);
MIB_PROBE_ICMP(icps_oldicmp);
MIB_PROBE2_ICMP(icps_outhist);
MIB_PROBE_ICMP(icps_badcode);
MIB_PROBE_ICMP(icps_tooshort);
MIB_PROBE_ICMP(icps_checksum);
MIB_PROBE_ICMP(icps_badlen);
MIB_PROBE_ICMP(icps_reflect);
MIB_PROBE2_ICMP(icps_inhist);
MIB_PROBE_ICMP(icps_bmcastecho);
MIB_PROBE_ICMP(icps_bmcasttstamp);
MIB_PROBE_ICMP(icps_badaddr);
MIB_PROBE_ICMP(icps_noroute);

#define MIB_PROBE_ICMP6(name) \
    SDT_PROBE_DEFINE1(mib, icmp6, count, name, \
        "int")
#define MIB_PROBE2_ICMP6(name) \
    SDT_PROBE_DEFINE2(mib, icmp6, count, name, \
        "int", "int")

MIB_PROBE_ICMP6(icp6s_error);
MIB_PROBE_ICMP6(icp6s_canterror);
MIB_PROBE_ICMP6(icp6s_toofreq);
MIB_PROBE2_ICMP6(icp6s_outhist);
MIB_PROBE_ICMP6(icp6s_badcode);
MIB_PROBE_ICMP6(icp6s_tooshort);
MIB_PROBE_ICMP6(icp6s_checksum);
MIB_PROBE_ICMP6(icp6s_badlen);
MIB_PROBE_ICMP6(icp6s_dropped);
MIB_PROBE_ICMP6(icp6s_reflect);
MIB_PROBE2_ICMP6(icp6s_inhist);
MIB_PROBE_ICMP6(icp6s_nd_toomanyopt);
MIB_PROBE_ICMP6(icp6s_odst_unreach_noroute);
MIB_PROBE_ICMP6(icp6s_odst_unreach_admin);
MIB_PROBE_ICMP6(icp6s_odst_unreach_beyondscope);
MIB_PROBE_ICMP6(icp6s_odst_unreach_addr);
MIB_PROBE_ICMP6(icp6s_odst_unreach_noport);
MIB_PROBE_ICMP6(icp6s_opacket_too_big);
MIB_PROBE_ICMP6(icp6s_otime_exceed_transit);
MIB_PROBE_ICMP6(icp6s_otime_exceed_reassembly);
MIB_PROBE_ICMP6(icp6s_oparamprob_header);
MIB_PROBE_ICMP6(icp6s_oparamprob_nextheader);
MIB_PROBE_ICMP6(icp6s_oparamprob_option);
MIB_PROBE_ICMP6(icp6s_oredirect);
MIB_PROBE_ICMP6(icp6s_ounknown);
MIB_PROBE_ICMP6(icp6s_pmtuchg);
MIB_PROBE_ICMP6(icp6s_nd_badopt);
MIB_PROBE_ICMP6(icp6s_badns);
MIB_PROBE_ICMP6(icp6s_badna);
MIB_PROBE_ICMP6(icp6s_badrs);
MIB_PROBE_ICMP6(icp6s_badra);
MIB_PROBE_ICMP6(icp6s_badredirect);
MIB_PROBE_ICMP6(icp6s_overflowdefrtr);
MIB_PROBE_ICMP6(icp6s_overflowprfx);
MIB_PROBE_ICMP6(icp6s_overflownndp);
MIB_PROBE_ICMP6(icp6s_overflowredirect);
MIB_PROBE_ICMP6(icp6s_invlhlim);

#define	MIB_PROBE_UDP(name)	SDT_PROBE_DEFINE1(mib, udp, count, name, "int")
MIB_PROBE_UDP(udps_ipackets);
MIB_PROBE_UDP(udps_hdrops);
MIB_PROBE_UDP(udps_badsum);
MIB_PROBE_UDP(udps_nosum);
MIB_PROBE_UDP(udps_badlen);
MIB_PROBE_UDP(udps_noport);
MIB_PROBE_UDP(udps_noportbcast);
MIB_PROBE_UDP(udps_fullsock);
MIB_PROBE_UDP(udps_pcbcachemiss);
MIB_PROBE_UDP(udps_pcbhashmiss);
MIB_PROBE_UDP(udps_opackets);
MIB_PROBE_UDP(udps_fastout);
MIB_PROBE_UDP(udps_noportmcast);
MIB_PROBE_UDP(udps_filtermcast);

#define	MIB_PROBE_TCP(name)	SDT_PROBE_DEFINE1(mib, tcp, count, name, "int")

MIB_PROBE_TCP(tcps_connattempt);
MIB_PROBE_TCP(tcps_accepts);
MIB_PROBE_TCP(tcps_connects);
MIB_PROBE_TCP(tcps_drops);
MIB_PROBE_TCP(tcps_conndrops);
MIB_PROBE_TCP(tcps_minmmsdrops);
MIB_PROBE_TCP(tcps_closed);
MIB_PROBE_TCP(tcps_segstimed);
MIB_PROBE_TCP(tcps_rttupdated);
MIB_PROBE_TCP(tcps_delack);
MIB_PROBE_TCP(tcps_timeoutdrop);
MIB_PROBE_TCP(tcps_rexmttimeo);
MIB_PROBE_TCP(tcps_persisttimeo);
MIB_PROBE_TCP(tcps_keeptimeo);
MIB_PROBE_TCP(tcps_keepprobe);
MIB_PROBE_TCP(tcps_keepdrops);
MIB_PROBE_TCP(tcps_progdrops);

MIB_PROBE_TCP(tcps_sndtotal);
MIB_PROBE_TCP(tcps_sndpack);
MIB_PROBE_TCP(tcps_sndbyte);
MIB_PROBE_TCP(tcps_sndrexmitpack);
MIB_PROBE_TCP(tcps_sndrexmitbyte);
MIB_PROBE_TCP(tcps_sndrexmitbad);
MIB_PROBE_TCP(tcps_sndacks);
MIB_PROBE_TCP(tcps_sndprobe);
MIB_PROBE_TCP(tcps_sndurg);
MIB_PROBE_TCP(tcps_sndwinup);
MIB_PROBE_TCP(tcps_sndctrl);

MIB_PROBE_TCP(tcps_rcvtotal);
MIB_PROBE_TCP(tcps_rcvpack);
MIB_PROBE_TCP(tcps_rcvbyte);
MIB_PROBE_TCP(tcps_rcvbadsum);
MIB_PROBE_TCP(tcps_rcvbadoff);
MIB_PROBE_TCP(tcps_rcvreassfull);
MIB_PROBE_TCP(tcps_rcvshort);
MIB_PROBE_TCP(tcps_rcvduppack);
MIB_PROBE_TCP(tcps_rcvdupbyte);
MIB_PROBE_TCP(tcps_rcvpartduppack);
MIB_PROBE_TCP(tcps_rcvpartdupbyte);
MIB_PROBE_TCP(tcps_rcvoopack);
MIB_PROBE_TCP(tcps_rcvoobyte);
MIB_PROBE_TCP(tcps_rcvpackafterwin);
MIB_PROBE_TCP(tcps_rcvbyteafterwin);
MIB_PROBE_TCP(tcps_rcvafterclose);
MIB_PROBE_TCP(tcps_rcvwinprobe);
MIB_PROBE_TCP(tcps_rcvdupack);
MIB_PROBE_TCP(tcps_rcvacktoomuch);
MIB_PROBE_TCP(tcps_rcvackpack);
MIB_PROBE_TCP(tcps_rcvackbyte);
MIB_PROBE_TCP(tcps_rcvwinupd);
MIB_PROBE_TCP(tcps_pawsdrop);
MIB_PROBE_TCP(tcps_predack);
MIB_PROBE_TCP(tcps_preddat);
MIB_PROBE_TCP(tcps_pcbackemiss);
MIB_PROBE_TCP(tcps_cachedrtt);
MIB_PROBE_TCP(tcps_cachedrttvar);
MIB_PROBE_TCP(tcps_cachedssthresh);
MIB_PROBE_TCP(tcps_usedrtt);
MIB_PROBE_TCP(tcps_usedrttvar);
MIB_PROBE_TCP(tcps_usedssthresh);
MIB_PROBE_TCP(tcps_persistdrop);
MIB_PROBE_TCP(tcps_badsyn);
MIB_PROBE_TCP(tcps_mturesent);
MIB_PROBE_TCP(tcps_listendrop);
MIB_PROBE_TCP(tcps_badrst);

MIB_PROBE_TCP(tcps_sc_added);
MIB_PROBE_TCP(tcps_sc_retransmitted);
MIB_PROBE_TCP(tcps_sc_dupsyn);
MIB_PROBE_TCP(tcps_sc_dropped);
MIB_PROBE_TCP(tcps_sc_completed);
MIB_PROBE_TCP(tcps_sc_bucketoverflow);
MIB_PROBE_TCP(tcps_sc_cacheoverflow);
MIB_PROBE_TCP(tcps_sc_reset);
MIB_PROBE_TCP(tcps_sc_stale);
MIB_PROBE_TCP(tcps_sc_aborted);
MIB_PROBE_TCP(tcps_sc_badack);
MIB_PROBE_TCP(tcps_sc_unreach);
MIB_PROBE_TCP(tcps_sc_zonefail);
MIB_PROBE_TCP(tcps_sc_sendcookie);
MIB_PROBE_TCP(tcps_sc_recvcookie);

MIB_PROBE_TCP(tcps_hc_added);
MIB_PROBE_TCP(tcps_hc_bucketoverflow);

MIB_PROBE_TCP(tcps_finwait2_drops);

MIB_PROBE_TCP(tcps_sack_recovery_episode);
MIB_PROBE_TCP(tcps_sack_rexmits);
MIB_PROBE_TCP(tcps_sack_rexmit_bytes);
MIB_PROBE_TCP(tcps_sack_rcv_blocks);
MIB_PROBE_TCP(tcps_sack_send_blocks);
MIB_PROBE_TCP(tcps_sack_lostrexmt);
MIB_PROBE_TCP(tcps_sack_sboverflow);

MIB_PROBE_TCP(tcps_ecn_rcvce);
MIB_PROBE_TCP(tcps_ecn_rcvect0);
MIB_PROBE_TCP(tcps_ecn_rcvect1);
MIB_PROBE_TCP(tcps_ecn_shs);
MIB_PROBE_TCP(tcps_ecn_rcwnd);

MIB_PROBE_TCP(tcps_sig_rcvgoodsig);
MIB_PROBE_TCP(tcps_sig_rcvbadsig);
MIB_PROBE_TCP(tcps_sig_err_buildsig);
MIB_PROBE_TCP(tcps_sig_err_sigopt);
MIB_PROBE_TCP(tcps_sig_err_nosigopt);

MIB_PROBE_TCP(tcps_pmtud_blackhole_activated);
MIB_PROBE_TCP(tcps_pmtud_blackhole_activated_min_mss);
MIB_PROBE_TCP(tcps_pmtud_blackhole_failed);

MIB_PROBE_TCP(tcps_tunneled_pkts);
MIB_PROBE_TCP(tcps_tunneled_errs);

MIB_PROBE_TCP(tcps_dsack_count);
MIB_PROBE_TCP(tcps_dsack_bytes);
MIB_PROBE_TCP(tcps_dsack_tlp_bytes);

MIB_PROBE_TCP(tcps_tw_recycles);
MIB_PROBE_TCP(tcps_tw_resets);
MIB_PROBE_TCP(tcps_tw_responds);

MIB_PROBE_TCP(tcps_ace_nect);
MIB_PROBE_TCP(tcps_ace_ect1);
MIB_PROBE_TCP(tcps_ace_ect0);
MIB_PROBE_TCP(tcps_ace_ce);

MIB_PROBE_TCP(tcps_ecn_sndect0);
MIB_PROBE_TCP(tcps_ecn_sndect1);

MIB_PROBE_TCP(tcps_tlpresends);
MIB_PROBE_TCP(tcps_tlpresend_bytes);

#endif

SDT_PROBE_DEFINE6_XLATE(ip, , , receive,
    "void *", "pktinfo_t *",
    "void *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct ifnet *", "ifinfo_t *",
    "struct ip *", "ipv4info_t *",
    "struct ip6_hdr *", "ipv6info_t *");

SDT_PROBE_DEFINE6_XLATE(ip, , , send,
    "void *", "pktinfo_t *",
    "void *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct ifnet *", "ifinfo_t *",
    "struct ip *", "ipv4info_t *",
    "struct ip6_hdr *", "ipv6info_t *");

SDT_PROBE_DEFINE5_XLATE(tcp, , , accept__established,
    "void *", "pktinfo_t *",
    "struct tcpcb *", "csinfo_t *",
    "struct mbuf *", "ipinfo_t *",
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfoh_t *");

SDT_PROBE_DEFINE5_XLATE(tcp, , , accept__refused,
    "void *", "pktinfo_t *",
    "struct tcpcb *", "csinfo_t *",
    "struct mbuf *", "ipinfo_t *",
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfo_t *");

SDT_PROBE_DEFINE5_XLATE(tcp, , , connect__established,
    "void *", "pktinfo_t *",
    "struct tcpcb *", "csinfo_t *",
    "struct mbuf *", "ipinfo_t *",
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfoh_t *");

SDT_PROBE_DEFINE5_XLATE(tcp, , , connect__refused,
    "void *", "pktinfo_t *",
    "struct tcpcb *", "csinfo_t *",
    "struct mbuf *", "ipinfo_t *",
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfoh_t *");

SDT_PROBE_DEFINE5_XLATE(tcp, , , connect__request,
    "void *", "pktinfo_t *",
    "struct tcpcb *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfo_t *");

SDT_PROBE_DEFINE5_XLATE(tcp, , , receive,
    "void *", "pktinfo_t *",
    "struct tcpcb *", "csinfo_t *",
    "struct mbuf *", "ipinfo_t *",
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfoh_t *");

SDT_PROBE_DEFINE5_XLATE(tcp, , , send,
    "void *", "pktinfo_t *",
    "struct tcpcb *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfo_t *");

SDT_PROBE_DEFINE1_XLATE(tcp, , , siftr,
    "struct pkt_node *", "siftrinfo_t *");

SDT_PROBE_DEFINE3_XLATE(tcp, , , debug__input,
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfoh_t *",
    "struct mbuf *", "ipinfo_t *");

SDT_PROBE_DEFINE3_XLATE(tcp, , , debug__output,
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfo_t *",
    "struct mbuf *", "ipinfo_t *");

SDT_PROBE_DEFINE2_XLATE(tcp, , , debug__user,
    "struct tcpcb *", "tcpsinfo_t *" ,
    "int", "int");

SDT_PROBE_DEFINE3_XLATE(tcp, , , debug__drop,
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfoh_t *",
    "struct mbuf *", "ipinfo_t *");

SDT_PROBE_DEFINE6_XLATE(tcp, , , state__change,
    "void *", "void *",
    "struct tcpcb *", "csinfo_t *",
    "void *", "void *",
    "struct tcpcb *", "tcpsinfo_t *",
    "void *", "void *",
    "int", "tcplsinfo_t *");

SDT_PROBE_DEFINE6_XLATE(tcp, , , receive__autoresize,
    "void *", "void *",
    "struct tcpcb *", "csinfo_t *",
    "struct mbuf *", "ipinfo_t *",
    "struct tcpcb *", "tcpsinfo_t *" ,
    "struct tcphdr *", "tcpinfoh_t *",
    "int", "int");

SDT_PROBE_DEFINE5_XLATE(udp, , , receive,
    "void *", "pktinfo_t *",
    "struct inpcb *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct inpcb *", "udpsinfo_t *",
    "struct udphdr *", "udpinfo_t *");

SDT_PROBE_DEFINE5_XLATE(udp, , , send,
    "void *", "pktinfo_t *",
    "struct inpcb *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct inpcb *", "udpsinfo_t *",
    "struct udphdr *", "udpinfo_t *");

SDT_PROBE_DEFINE5_XLATE(udplite, , , receive,
    "void *", "pktinfo_t *",
    "struct inpcb *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct inpcb *", "udplitesinfo_t *",
    "struct udphdr *", "udpliteinfo_t *");

SDT_PROBE_DEFINE5_XLATE(udplite, , , send,
    "void *", "pktinfo_t *",
    "struct inpcb *", "csinfo_t *",
    "uint8_t *", "ipinfo_t *",
    "struct inpcb *", "udplitesinfo_t *",
    "struct udphdr *", "udpliteinfo_t *");
