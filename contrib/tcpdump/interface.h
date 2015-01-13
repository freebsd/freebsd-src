/*
 * Copyright (c) 1988-2002
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
 * @(#) $Header: /tcpdump/master/tcpdump/interface.h,v 1.285 2008-08-16 11:36:20 hannes Exp $ (LBL)
 */

#ifndef tcpdump_interface_h
#define tcpdump_interface_h

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>

/* snprintf et al */

#include <stdarg.h>

#if HAVE_STDINT_H
#include <stdint.h>
#endif

#if !defined(HAVE_SNPRINTF)
int snprintf(char *, size_t, const char *, ...)
     __attribute__((format(printf, 3, 4)));
#endif

#if !defined(HAVE_VSNPRINTF)
int vsnprintf(char *, size_t, const char *, va_list)
     __attribute__((format(printf, 3, 0)));
#endif

#ifndef HAVE_STRLCAT
extern size_t strlcat(char *, const char *, size_t);
#endif
#ifndef HAVE_STRLCPY
extern size_t strlcpy(char *, const char *, size_t);
#endif

#ifndef HAVE_STRDUP
extern char *strdup(const char *);
#endif

#ifndef HAVE_STRSEP
extern char *strsep(char **, const char *);
#endif

#define PT_VAT		1	/* Visual Audio Tool */
#define PT_WB		2	/* distributed White Board */
#define PT_RPC		3	/* Remote Procedure Call */
#define PT_RTP		4	/* Real-Time Applications protocol */
#define PT_RTCP		5	/* Real-Time Applications control protocol */
#define PT_SNMP		6	/* Simple Network Management Protocol */
#define PT_CNFP		7	/* Cisco NetFlow protocol */
#define PT_TFTP		8	/* trivial file transfer protocol */
#define PT_AODV		9	/* Ad-hoc On-demand Distance Vector Protocol */
#define PT_CARP		10	/* Common Address Redundancy Protocol */
#define PT_RADIUS	11	/* RADIUS authentication Protocol */
#define PT_ZMTP1	12	/* ZeroMQ Message Transport Protocol 1.0 */
#define PT_VXLAN	13	/* Virtual eXtensible Local Area Network */

#ifndef min
#define min(a,b) ((a)>(b)?(b):(a))
#endif
#ifndef max
#define max(a,b) ((b)>(a)?(b):(a))
#endif

#define ESRC(ep) ((ep)->ether_shost)
#define EDST(ep) ((ep)->ether_dhost)

#ifndef NTOHL
#define NTOHL(x)	(x) = ntohl(x)
#define NTOHS(x)	(x) = ntohs(x)
#define HTONL(x)	(x) = htonl(x)
#define HTONS(x)	(x) = htons(x)
#endif
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#define DISSECTOR_DECLARE(func)		\
	extern void func;		\
	extern void _##func;
#define DISSECTOR_DECLARE_UINT(func)	\
	extern void func;		\
	extern u_int _##func;


extern const char *program_name;/* used to generate self-identifying messages */

extern int32_t thiszone;	/* seconds offset from gmt to local time */

/*
 * True if  "l" bytes of "var" were captured.
 *
 * The "snapend - (l) <= snapend" checks to make sure "l" isn't so large
 * that "snapend - (l)" underflows.
 *
 * The check is for <= rather than < because "l" might be 0.
 */
#define TTEST2(var, l) (snapend - (l) <= snapend && \
			(const u_char *)&(var) <= snapend - (l))

/* True if "var" was captured */
#define TTEST(var) TTEST2(var, sizeof(var))

/* Bail if "l" bytes of "var" were not captured */
#define TCHECK2(var, l) if (!TTEST2(var, l)) goto trunc

/* Bail if "var" was not captured */
#define TCHECK(var) TCHECK2(var, sizeof(var))

extern void ts_print(const struct timeval *);
extern void relts_print(int);

extern int fn_print(const u_char *, const u_char *);
extern int fn_printn(const u_char *, u_int, const u_char *);
extern int fn_printzp(const u_char *, u_int, const u_char *);
extern int fn_print_str(const u_char *s);
extern int mask2plen(u_int32_t);
extern const char *tok2strary_internal(const char **, int, const char *, int);
#define	tok2strary(a,f,i) tok2strary_internal(a, sizeof(a)/sizeof(a[0]),f,i)

extern const char *inet_ntop_cap(int af,
    const void * restrict src, char * restrict dst,
    socklen_t size);

extern const char *dnaddr_string(u_short);

extern void error(const char *, ...)
    __attribute__((noreturn, format (printf, 1, 2)));
extern void warning(const char *, ...) __attribute__ ((format (printf, 1, 2)));

extern char *read_infile(char *);
extern char *copy_argv(char **);

extern void safeputchar(int);
extern void safeputs(const u_char *, int);

extern const char *isonsap_string(const u_char *, register u_int);
extern const char *protoid_string(const u_char *);
extern const char *ipxsap_string(u_short);
extern const char *dnname_string(u_short);
extern const char *dnnum_string(u_short);

/* checksum routines */
extern void init_checksum(void);
extern u_int16_t verify_crc10_cksum(u_int16_t, const u_char *, int);
extern u_int16_t create_osi_cksum(const u_char *, int, int);

/* The printer routines. */

#include <pcap.h>

extern int print_unknown_data(const u_char *, const char *,int);
extern void ascii_print(const u_char *, u_int);
extern void hex_and_ascii_print_with_offset(const char *, const u_char *, u_int, u_int);
extern void hex_and_ascii_print(const char *, const u_char *, u_int);
extern void hex_print_with_offset(const char *, const u_char *, u_int, u_int);
extern void hex_print(const char *, const u_char *, u_int);
extern void raw_print(const struct pcap_pkthdr *, const u_char *, u_int);
DISSECTOR_DECLARE(telnet_print(const u_char *, u_int));
extern int llc_print(const u_char *, u_int, u_int, const u_char *,
	const u_char *, u_short *);
extern int snap_print(const u_char *, u_int, u_int, u_int);
DISSECTOR_DECLARE(aarp_print(const u_char *, u_int));
DISSECTOR_DECLARE(aodv_print(const u_char *, u_int, int));
DISSECTOR_DECLARE(atalk_print(const u_char *, u_int));
DISSECTOR_DECLARE(atm_print(u_int, u_int, u_int, const u_char *, u_int, u_int));
extern u_int atm_if_print(const struct pcap_pkthdr *, const u_char *);
extern u_int sunatm_if_print(const struct pcap_pkthdr *, const u_char *);
DISSECTOR_DECLARE(oam_print(const u_char *, u_int, u_int));
DISSECTOR_DECLARE(bootp_print(const u_char *, u_int));
DISSECTOR_DECLARE(bgp_print(const u_char *, int));
DISSECTOR_DECLARE(beep_print(const u_char *, u_int));
DISSECTOR_DECLARE(cnfp_print(const u_char *, const u_char *));
DISSECTOR_DECLARE(decnet_print(const u_char *, u_int, u_int));
extern void default_print(const u_char *, u_int);
DISSECTOR_DECLARE(dvmrp_print(const u_char *, u_int));
DISSECTOR_DECLARE(egp_print(const u_char *, u_int));
extern u_int enc_if_print(const struct pcap_pkthdr *, const u_char *);
extern u_int enc_if_print(const struct pcap_pkthdr *, const u_char *);
extern u_int pflog_if_print(const struct pcap_pkthdr *, const u_char *);
DISSECTOR_DECLARE(pfsync_ip_print(const u_char *, u_int));
extern u_int arcnet_if_print(const struct pcap_pkthdr *, const u_char *);
extern u_int arcnet_linux_if_print(const struct pcap_pkthdr *, const u_char *);
DISSECTOR_DECLARE_UINT(token_print(const u_char *, u_int, u_int));
extern u_int token_if_print(const struct pcap_pkthdr *, const u_char *);
DISSECTOR_DECLARE(fddi_print(const u_char *, u_int, u_int));
extern u_int fddi_if_print(const struct pcap_pkthdr *, const u_char *);
extern u_int fr_if_print(const struct pcap_pkthdr *, const u_char *);
extern u_int mfr_if_print(const struct pcap_pkthdr *, const u_char *);
DISSECTOR_DECLARE_UINT(fr_print(const u_char *, u_int));
DISSECTOR_DECLARE_UINT(mfr_print(const u_char *, u_int));
extern char *q922_string(const u_char *);
extern u_int ieee802_11_if_print(const struct pcap_pkthdr *, const u_char *);
extern u_int ieee802_11_radio_if_print(const struct pcap_pkthdr *,
	const u_char *);
extern u_int ap1394_if_print(const struct pcap_pkthdr *, const u_char *);
extern u_int ieee802_11_radio_avs_if_print(const struct pcap_pkthdr *,
	const u_char *);
DISSECTOR_DECLARE(gre_print(const u_char *, u_int));
DISSECTOR_DECLARE(icmp_print(const u_char *, u_int, const u_char *, int));
DISSECTOR_DECLARE(igmp_print(const u_char *, u_int));
DISSECTOR_DECLARE(igrp_print(const u_char *, u_int, const u_char *));
extern void ipN_print(const u_char *, u_int); /* XXX: not sandboxed, no point */
extern u_int ipfc_if_print(const struct pcap_pkthdr *, const u_char *);
DISSECTOR_DECLARE(ipx_print(const u_char *, u_int));
DISSECTOR_DECLARE(isoclns_print(const u_char *, u_int, u_int));
DISSECTOR_DECLARE(krb_print(const u_char *));
DISSECTOR_DECLARE_UINT(llap_print(const u_char *, u_int));
extern u_int ltalk_if_print(const struct pcap_pkthdr *, const u_char *);
DISSECTOR_DECLARE(msdp_print(const u_char *, u_int));
DISSECTOR_DECLARE(nfsreply_print(const u_char *, u_int, const u_char *));
DISSECTOR_DECLARE(nfsreq_print(const u_char *, u_int, const u_char *));
DISSECTOR_DECLARE(ns_print(const u_char *, u_int, int));
extern const u_char *ns_nprint(const u_char *, const u_char *);
DISSECTOR_DECLARE(ntp_print(const u_char *, u_int));
extern u_int null_if_print(const struct pcap_pkthdr *, const u_char *);
DISSECTOR_DECLARE(ospf_print(const u_char *, u_int, const u_char *));
DISSECTOR_DECLARE(olsr_print (const u_char *, u_int, int));
DISSECTOR_DECLARE(pimv1_print(const u_char *, u_int));
DISSECTOR_DECLARE(cisco_autorp_print(const u_char *, u_int));
DISSECTOR_DECLARE(rsvp_print(const u_char *, u_int));
DISSECTOR_DECLARE(ldp_print(const u_char *, u_int));
DISSECTOR_DECLARE(lldp_print(const u_char *, u_int));
DISSECTOR_DECLARE(rpki_rtr_print(const u_char *, u_int));
DISSECTOR_DECLARE(lmp_print(const u_char *, u_int));
DISSECTOR_DECLARE(lspping_print(const u_char *, u_int));
DISSECTOR_DECLARE(lwapp_control_print(const u_char *, u_int, int));
DISSECTOR_DECLARE(lwapp_data_print(const u_char *, u_int));
DISSECTOR_DECLARE(eigrp_print(const u_char *, u_int));
DISSECTOR_DECLARE(mobile_print(const u_char *, u_int));
DISSECTOR_DECLARE(pim_print(const u_char *, u_int, u_int));
DISSECTOR_DECLARE_UINT(pppoe_print(const u_char *, u_int));
DISSECTOR_DECLARE_UINT(ppp_print(const u_char *, u_int));
extern u_int ppp_if_print(const struct pcap_pkthdr *, const u_char *);
extern u_int ppp_hdlc_if_print(const struct pcap_pkthdr *, const u_char *);
extern u_int ppp_bsdos_if_print(const struct pcap_pkthdr *, const u_char *);
extern u_int pppoe_if_print(const struct pcap_pkthdr *, const u_char *);
extern u_int prism_if_print(const struct pcap_pkthdr *, const u_char *);
DISSECTOR_DECLARE(q933_print(const u_char *, u_int));
extern int vjc_print(const u_char *, u_short); /* XXX: Not sandboxed, simple */
DISSECTOR_DECLARE(vqp_print(const u_char *, register u_int));
extern u_int raw_if_print(const struct pcap_pkthdr *, const u_char *);
DISSECTOR_DECLARE(rip_print(const u_char *, u_int));
extern u_int sl_if_print(const struct pcap_pkthdr *, const u_char *);
DISSECTOR_DECLARE(lane_print(const u_char *, u_int, u_int));
extern u_int lane_if_print(const struct pcap_pkthdr *, const u_char *);
extern u_int cip_if_print(const struct pcap_pkthdr *, const u_char *);
extern u_int sl_bsdos_if_print(const struct pcap_pkthdr *, const u_char *);
extern u_int chdlc_if_print(const struct pcap_pkthdr *, const u_char *);
DISSECTOR_DECLARE_UINT(chdlc_print(const u_char *, u_int));
extern u_int juniper_atm1_print(const struct pcap_pkthdr *, const u_char *);
extern u_int juniper_atm2_print(const struct pcap_pkthdr *, const u_char *);
extern u_int juniper_mfr_print(const struct pcap_pkthdr *, const u_char *);
extern u_int juniper_mlfr_print(const struct pcap_pkthdr *, const u_char *);
extern u_int juniper_mlppp_print(const struct pcap_pkthdr *, const u_char *);
extern u_int juniper_pppoe_print(const struct pcap_pkthdr *, const u_char *);
extern u_int juniper_pppoe_atm_print(const struct pcap_pkthdr *, const u_char *);
extern u_int juniper_ggsn_print(const struct pcap_pkthdr *, const u_char *);
extern u_int juniper_es_print(const struct pcap_pkthdr *, const u_char *);
extern u_int juniper_monitor_print(const struct pcap_pkthdr *, const u_char *);
extern u_int juniper_services_print(const struct pcap_pkthdr *, const u_char *);
extern u_int juniper_ether_print(const struct pcap_pkthdr *, const u_char *);
extern u_int juniper_ppp_print(const struct pcap_pkthdr *, const u_char *);
extern u_int juniper_frelay_print(const struct pcap_pkthdr *, const u_char *);
extern u_int juniper_chdlc_print(const struct pcap_pkthdr *, const u_char *);
extern u_int sll_if_print(const struct pcap_pkthdr *, const u_char *);
DISSECTOR_DECLARE(snmp_print(const u_char *, u_int));
DISSECTOR_DECLARE(sunrpcrequest_print(const u_char *, u_int, const u_char *));
extern u_int symantec_if_print(const struct pcap_pkthdr *, const u_char *);
DISSECTOR_DECLARE(tcp_print(const u_char *, u_int, const u_char *, int));
DISSECTOR_DECLARE(tftp_print(const u_char *, u_int));
DISSECTOR_DECLARE(timed_print(const u_char *));
DISSECTOR_DECLARE(udld_print(const u_char *, u_int));
DISSECTOR_DECLARE(udp_print(const u_char *, u_int, const u_char *, int));
DISSECTOR_DECLARE(vtp_print(const u_char *, u_int));
DISSECTOR_DECLARE(wb_print(const u_char *, u_int));
extern int ah_print(const u_char *);
extern int ipcomp_print(const u_char *, int *);
DISSECTOR_DECLARE(rx_print(const u_char *, int, int, int, const u_char *));
DISSECTOR_DECLARE(netbeui_print(u_short, const u_char *, int));
DISSECTOR_DECLARE(ipx_netbios_print(const u_char *, u_int));
DISSECTOR_DECLARE(nbt_tcp_print(const u_char *, int));
DISSECTOR_DECLARE(nbt_udp137_print(const u_char *, int));
DISSECTOR_DECLARE(nbt_udp138_print(const u_char *, int));
DISSECTOR_DECLARE(smb_tcp_print(const u_char *, int));
extern char *smb_errstr(int, int);
extern const char *nt_errstr(u_int32_t);
extern void print_data(const u_char *, int); /* Don't sandbox */
DISSECTOR_DECLARE(l2tp_print(const u_char *, u_int));
DISSECTOR_DECLARE(vrrp_print(const u_char *, u_int, int));
DISSECTOR_DECLARE(carp_print(const u_char *, u_int, int));
DISSECTOR_DECLARE(slow_print(const u_char *, u_int));
DISSECTOR_DECLARE(sflow_print(const u_char *, u_int));
DISSECTOR_DECLARE(mpcp_print(const u_char *, u_int));
DISSECTOR_DECLARE(cfm_print(const u_char *, u_int));
DISSECTOR_DECLARE(pgm_print(const u_char *, u_int, const u_char *));
DISSECTOR_DECLARE(cdp_print(const u_char *, u_int, u_int));
DISSECTOR_DECLARE(dtp_print(const u_char *, u_int));
DISSECTOR_DECLARE(stp_print(const u_char *, u_int));
DISSECTOR_DECLARE(radius_print(const u_char *, u_int));
DISSECTOR_DECLARE(lwres_print(const u_char *, u_int));
DISSECTOR_DECLARE(pptp_print(const u_char *));
DISSECTOR_DECLARE(dccp_print(const u_char *, const u_char *, u_int));
DISSECTOR_DECLARE(sctp_print(const u_char *, const u_char *, u_int));
DISSECTOR_DECLARE(forces_print(const u_char *, u_int));
DISSECTOR_DECLARE(mpls_print(const u_char *, u_int));
DISSECTOR_DECLARE(zephyr_print(const u_char *, int));
DISSECTOR_DECLARE(zmtp1_print(const u_char *, u_int));
DISSECTOR_DECLARE(hsrp_print(const u_char *, u_int));
DISSECTOR_DECLARE(bfd_print(const u_char *, u_int, u_int));
DISSECTOR_DECLARE(sip_print(const u_char *, u_int));
DISSECTOR_DECLARE(syslog_print(const u_char *, u_int));
extern u_int bt_if_print(const struct pcap_pkthdr *, const u_char *);
extern u_int usb_linux_48_byte_print(const struct pcap_pkthdr *, const u_char *);
extern u_int usb_linux_64_byte_print(const struct pcap_pkthdr *, const u_char *);
DISSECTOR_DECLARE(vxlan_print(const u_char *, u_int));
DISSECTOR_DECLARE(otv_print(const u_char *, u_int));


#ifdef INET6
extern void ip6_opt_print(const u_char *, int);
extern int hbhopt_print(const u_char *);
extern int dstopt_print(const u_char *);
extern int frag6_print(const u_char *, const u_char *);
extern int mobility_print(const u_char *, const u_char *);
DISSECTOR_DECLARE(ripng_print(const u_char *, unsigned int));
extern int rt6_print(const u_char *, const u_char *);
DISSECTOR_DECLARE(ospf6_print(const u_char *, u_int));
DISSECTOR_DECLARE(dhcp6_print(const u_char *, u_int));
DISSECTOR_DECLARE(babel_print(const u_char *, u_int));
extern int mask62plen(const u_char *);
#endif /*INET6*/

struct cksum_vec {
	const u_char *	ptr;
	int		len;
};
extern u_int16_t in_cksum(const struct cksum_vec *, int);
extern u_int16_t in_cksum_shouldbe(u_int16_t, u_int16_t);

#ifndef HAVE_BPF_DUMP
struct bpf_program;

extern void bpf_dump(const struct bpf_program *, int);

#endif

#include "netdissect.h"

/* forward compatibility */

#ifndef NETDISSECT_REWORKED
extern netdissect_options *gndo;

#define bflag gndo->ndo_bflag 
#define eflag gndo->ndo_eflag 
#define fflag gndo->ndo_fflag 
#define jflag gndo->ndo_jflag
#define Kflag gndo->ndo_Kflag 
#define nflag gndo->ndo_nflag 
#define Nflag gndo->ndo_Nflag 
#define Oflag gndo->ndo_Oflag 
#define pflag gndo->ndo_pflag 
#define qflag gndo->ndo_qflag 
#define Rflag gndo->ndo_Rflag 
#define sflag gndo->ndo_sflag 
#define Sflag gndo->ndo_Sflag 
#define tflag gndo->ndo_tflag 
#define Uflag gndo->ndo_Uflag 
#define uflag gndo->ndo_uflag 
#define vflag gndo->ndo_vflag 
#define xflag gndo->ndo_xflag 
#define Xflag gndo->ndo_Xflag 
#define Cflag gndo->ndo_Cflag 
#define Gflag gndo->ndo_Gflag 
#define Aflag gndo->ndo_Aflag 
#define Bflag gndo->ndo_Bflag 
#define Iflag gndo->ndo_Iflag 
#define suppress_default_print gndo->ndo_suppress_default_print
#define packettype gndo->ndo_packettype
#define sigsecret gndo->ndo_sigsecret
#define Wflag gndo->ndo_Wflag
#define WflagChars gndo->ndo_WflagChars
#define Cflag_count gndo->ndo_Cflag_count
#define Gflag_count gndo->ndo_Gflag_count
#define Gflag_time gndo->ndo_Gflag_time 
#define Hflag gndo->ndo_Hflag
#define snaplen     gndo->ndo_snaplen
#define snapend     gndo->ndo_snapend

#endif
