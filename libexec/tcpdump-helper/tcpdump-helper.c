/*-
 * Copyright (c) 2014 SRI International
 * Copyright (c) 2012-2014 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 *
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 2000
 *      The Regents of the University of California.  All rights reserved.
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
 * Support for splitting captures into multiple files with a maximum
 * file size:
 *
 * Copyright (c) 2001
 *      Seth Webster <swebster@sst.ll.mit.edu>
 */

#include "config.h"

#include <sys/types.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <cheri/cheri_system.h>
#include <cheri/cheri_invoke.h>

#include <stdlib.h>
#include <string.h>
#include <md5.h>

#include "cheri_tcpdump_system.h"
#include "tcpdump-helper.h"

#include "netdissect.h"
#include "interface.h"
#include "print.h"

struct print_info printinfo;
netdissect_options Gndo;
netdissect_options *gndo = &Gndo;
struct cheri_object *gpso;

const char *program_name;

void	pawned(void);

int	invoke(register_t op, register_t arg1, register_t arg2,
	    register_t arg3, register_t arg4, register_t arg5,
	    netdissect_options *ndo,
	    const char *ndo_espsecret,
	    const struct pcap_pkthdr *h,
	    const u_char *sp,
	    struct cheri_object *proto_sandbox_objects,
	    const u_char *sp2,
	    void* carg1, void *carg2);
static void	dispatch_dissector(register_t op, u_int length,
    register_t arg2, register_t arg3, register_t arg4, register_t arg5,
    netdissect_options *ndo, const u_char *bp, const u_char *bp2,
    void *carg1, void *carg2);

static int
invoke_init(bpf_u_int32 localnet, bpf_u_int32 netmask,
    const netdissect_options *ndo,
    const char *ndo_espsecret)
{
	size_t espsec_len;

/* XXXBD: broken, use system default
	cheri_system_methodnum_puts = CHERI_TCPDUMP_PUTS;
	cheri_system_methodnum_putchar = CHERI_TCPDUMP_PUTCHAR;
*/

	program_name = "tcpdump-helper"; /* XXX: copy from parent? */

	/*
	 * Make a copy of the parent's netdissect_options.  Most of the
	 * items are unchanged until the next init or per-packet.  The
	 * exceptions are related to IPSec decryption and we punt on
	 * those for now and allow them to be reinitalized on a
	 * per-sandbox basis.
	 */
	memcpy_c(gndo, ndo, sizeof(netdissect_options));
	if (ndo->ndo_espsecret != NULL) { /* XXX: check the real thing */
		if (gndo->ndo_espsecret != NULL)
			free(gndo->ndo_espsecret);

		espsec_len = cheri_getlen((void *)ndo_espsecret);
		gndo->ndo_espsecret = malloc(espsec_len);
		if (gndo->ndo_espsecret == NULL)
			abort();
		memcpy_c(cheri_ptr(gndo->ndo_espsecret, espsec_len),
		    ndo_espsecret, espsec_len);
	}
	gndo->ndo_printf = tcpdump_printf;
	gndo->ndo_default_print = ndo_default_print;
	gndo->ndo_error = ndo_error;
	gndo->ndo_warning = ndo_warning;

	init_print(localnet, netmask);

	printinfo.ndo_type = 1;
	printinfo.ndo = gndo;
	printinfo.p.ndo_printer = lookup_ndo_printer(gndo->ndo_dlt);
	if (printinfo.p.ndo_printer == NULL) {
		printinfo.p.printer = lookup_printer(gndo->ndo_dlt);
		printinfo.ndo_type = 0;
		if (printinfo.p.printer == NULL) {
			gndo->ndo_dltname =
			    pcap_datalink_val_to_name(gndo->ndo_dlt);
			if (gndo->ndo_dltname != NULL)
				error("packet printing is not supported for link type %s: use -w",
				      gndo->ndo_dltname);
		else
			error("packet printing is not supported for link type %d: use -w", gndo->ndo_dlt);
		}
	}

	return (0);
}

/*
 * Sandbox entry point.  An init method sets up global state.  
 * The print_packet method invokes the top level packet printing method
 * selected by init.
 *
 * c1 and c2 hold the system code and data capablities.  c3 holds the
 * parent's netdissect_options structure and c4 holes IPSec decryption
 * keys.  They are only used for init.  c5 holds a struct pcap_pkthdr and
 * c6 the packet body.   They are used only by print_packet.
 */
int
invoke(register_t op, register_t arg1, register_t arg2,
    register_t arg3, register_t arg4, register_t arg5,
    netdissect_options *ndo,
    const char *ndo_espsecret,
    const struct pcap_pkthdr *h, const u_char *sp,
    struct cheri_object *proto_sandbox_objects, const u_char *sp2,
    void * carg1, void *carg2)
{
	int ret;

	gpso = proto_sandbox_objects;

	ret = 0;

	switch (op) {
	case TCPDUMP_HELPER_OP_INIT:
#ifdef DEBUG
		printf("calling invoke_init\n");
#endif
		return (invoke_init(arg1, arg2, ndo, ndo_espsecret));

	case TCPDUMP_HELPER_OP_PRINT_PACKET:
#ifdef DEBUG
		/* XXX printf broken here */
		printf("printing a packet of length 0x%x\n", h->caplen);
		printf("sp b:%016jx l:%016zx o:%jx\n",
		    cheri_getbase((void *)sp),
		    cheri_getlen((void *)sp),
		    cheri_getoffset((void *)sp));
#endif
		assert(h->caplen == cheri_getlen((void *)sp));

		/*
		 * XXXBD: Hack around the need to not store the packet except
		 * on the stack.  Should really avoid this somehow...
		 */
		gndo->ndo_packetp = malloc(h->caplen);
		if (gndo->ndo_packetp == NULL)
			error("failed to malloc packet space\n");
		/* XXXBD: void* cast works around type bug */
		memcpy_c((void *)gndo->ndo_packetp, sp, h->caplen);
		gndo->ndo_snapend = gndo->ndo_packetp + h->caplen;

		if (printinfo.ndo_type)
			ret = (*printinfo.p.ndo_printer)(printinfo.ndo,
			     h, gndo->ndo_packetp);
		else
			ret = (*printinfo.p.printer)(h, gndo->ndo_packetp);

		/* XXX: what else to reset? */
		free((void*)(gndo->ndo_packetp));
		gndo->ndo_packetp = NULL;
		snapend = NULL;
		break;

	case TCPDUMP_HELPER_OP_HAS_PRINTER:
		return (has_printer(arg1));

	default:
		dispatch_dissector(op, arg1, arg2, arg3, arg4, arg5,
		    ndo, sp, sp2, carg1, carg2);
		break;

	}

	return (ret);
}

static void
dispatch_dissector(register_t op, u_int length, register_t arg2,
    register_t arg3 _U_, register_t arg4 _U_, register_t arg5 _U_,
    netdissect_options *ndo,
    const u_char *bp, const u_char *bp2, void *carg1 _U_, void *carg2 _U_)
{
	snapend = bp + length; /* set to end of capability? */

	switch (op) {
	case TCPDUMP_HELPER_OP_EAP_PRINT:
		_eap_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_ARP_PRINT:
		_arp_print(ndo, bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_TIPC_PRINT:
		_tipc_print(ndo, bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_MSNLB_PRINT:
		_msnlb_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_ICMP6_PRINT:
		_icmp6_print(ndo, bp, length, bp2, arg2);
		break;

	case TCPDUMP_HELPER_OP_ISAKMP_PRINT:
		_isakmp_print(ndo, bp, length, bp2);
		break;

	case TCPDUMP_HELPER_OP_ISAKMP_RFC3948_PRINT:
		_isakmp_rfc3948_print(ndo, bp, length, bp2);
		break;

	case TCPDUMP_HELPER_OP_IP_PRINT:
		_ip_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_IP_PRINT_INNER:
		_ip_print_inner(ndo, bp, length, arg2, bp2);
		break;

	case TCPDUMP_HELPER_OP_RRCP_PRINT:
		_rrcp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_TELNET_PRINT:
		_telnet_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_AARP_PRINT:
		_aarp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_AODV_PRINT:
		_aodv_print(bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_ATALK_PRINT:
		_atalk_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_ATM_PRINT:
		_atm_print(arg2, arg3, arg4, bp, length, arg5);
		break;

	case TCPDUMP_HELPER_OP_OAM_PRINT:
		_oam_print(bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_BOOTP_PRINT:
		_bootp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_BGP_PRINT:
		_bgp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_BEEP_PRINT:
		_beep_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_CNFP_PRINT:
		_cnfp_print(bp, bp2);
		break;

	case TCPDUMP_HELPER_OP_DECNET_PRINT:
		_decnet_print(bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_DVMRP_PRINT:
		_dvmrp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_EGP_PRINT:
		_egp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_PFSYNC_IP_PRINT:
		_pfsync_ip_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_FDDI_PRINT:
		_fddi_print(bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_GRE_PRINT:
		_gre_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_ICMP_PRINT:
		_icmp_print(bp, length, bp2, arg2);
		break;

	case TCPDUMP_HELPER_OP_IGMP_PRINT:
		_igmp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_IGRP_PRINT:
		_igrp_print(bp, length, bp2);
		break;

	case TCPDUMP_HELPER_OP_IPX_PRINT:
		_ipx_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_ISOCLNS_PRINT:
		_isoclns_print(bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_KRB_PRINT:
		_krb_print(bp);
		break;

	case TCPDUMP_HELPER_OP_MSDP_PRINT:
		_msdp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_NFSREPLY_PRINT:
		_nfsreply_print(bp, length, bp2);
		break;

	case TCPDUMP_HELPER_OP_NFSREQ_PRINT:
		_nfsreq_print(bp, length, bp2);
		break;

	case TCPDUMP_HELPER_OP_NS_PRINT:
		_ns_print(bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_NTP_PRINT:
		_ntp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_OSPF_PRINT:
		_ospf_print(bp, length, bp2);
		break;

	case TCPDUMP_HELPER_OP_OLSR_PRINT:
		_olsr_print(bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_PIMV1_PRINT:
		_pimv1_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_CISCO_AUTORP_PRINT:
		_cisco_autorp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_RSVP_PRINT:
		_rsvp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_LDP_PRINT:
		_ldp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_LLDP_PRINT:
		_lldp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_RPKI_RTR_PRINT:
		_rpki_rtr_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_LMP_PRINT:
		_lmp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_LSPPING_PRINT:
		_lspping_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_LWAPP_CONTROL_PRINT:
		_lwapp_control_print(bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_LWAPP_DATA_PRINT:
		_lwapp_data_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_EIGRP_PRINT:
		_eigrp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_MOBILE_PRINT:
		_mobile_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_PIM_PRINT:
		_pim_print(bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_Q933_PRINT:
		_q933_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_VQP_PRINT:
		_vqp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_RIP_PRINT:
		_rip_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_LANE_PRINT:
		_lane_print(bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_SNMP_PRINT:
		_snmp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_SUNRPCREQUEST_PRINT:
		_sunrpcrequest_print(bp, length, bp2);
		break;

	case TCPDUMP_HELPER_OP_TCP_PRINT:
		_tcp_print(bp, length, bp2, arg2);
		break;

	case TCPDUMP_HELPER_OP_TFTP_PRINT:
		_tftp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_TIMED_PRINT:
		_timed_print(bp);
		break;

	case TCPDUMP_HELPER_OP_UDLD_PRINT:
		_udld_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_UDP_PRINT:
		_udp_print(bp, length, bp2, arg2);
		break;

	case TCPDUMP_HELPER_OP_VTP_PRINT:
		_vtp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_WB_PRINT:
		_wb_print(bp, length);
		break;

#if 0
	case TCPDUMP_HELPER_OP_RX_PRINT:
		_rx_print(bp, length, arg2, arg3, bp2);
		break;
#endif

	case TCPDUMP_HELPER_OP_NETBEUI_PRINT:
		_netbeui_print(arg2, bp, length);
		break;

	case TCPDUMP_HELPER_OP_IPX_NETBIOS_PRINT:
		_ipx_netbios_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_NBT_TCP_PRINT:
		_nbt_tcp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_NBT_UDP137_PRINT:
		_nbt_udp137_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_NBT_UDP138_PRINT:
		_nbt_udp138_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_SMB_TCP_PRINT:
		_smb_tcp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_L2TP_PRINT:
		_l2tp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_VRRP_PRINT:
		_vrrp_print(bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_CARP_PRINT:
		_carp_print(bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_SLOW_PRINT:
		_slow_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_SFLOW_PRINT:
		_sflow_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_MPCP_PRINT:
		_mpcp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_CFM_PRINT:
		_cfm_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_PGM_PRINT:
		_pgm_print(bp, length, bp2);
		break;

	case TCPDUMP_HELPER_OP_CDP_PRINT:
		_cdp_print(bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_DTP_PRINT:
		_dtp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_STP_PRINT:
		_stp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_RADIUS_PRINT:
		_radius_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_LWRES_PRINT:
		_lwres_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_PPTP_PRINT:
		_pptp_print(bp);
		break;

	case TCPDUMP_HELPER_OP_DCCP_PRINT:
		_dccp_print(bp, bp2, length);
		break;

	case TCPDUMP_HELPER_OP_SCTP_PRINT:
		_sctp_print(bp, bp2, length);
		break;

	case TCPDUMP_HELPER_OP_FORCES_PRINT:
		_forces_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_MPLS_PRINT:
		_mpls_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_ZEPHYR_PRINT:
		_zephyr_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_ZMTP1_PRINT:
		_zmtp1_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_HSRP_PRINT:
		_hsrp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_BFD_PRINT:
		_bfd_print(bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_SIP_PRINT:
		_sip_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_SYSLOG_PRINT:
		_syslog_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_VXLAN_PRINT:
		_vxlan_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_OTV_PRINT:
		_otv_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_TOKEN_PRINT:
		_token_print(bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_FR_PRINT:
		_fr_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_MFR_PRINT:
		_mfr_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_LLAP_PRINT:
		_llap_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_PPPOE_PRINT:
		_pppoe_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_PPP_PRINT:
		_ppp_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_CHDLC_PRINT:
		_chdlc_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_RIPNG_PRINT:
		_ripng_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_OSPF6_PRINT:
		_ospf6_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_DHCP6_PRINT:
		_dhcp6_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_BABEL_PRINT:
		_babel_print(bp, length);
		break;

	default:
		printf("unknown op %ld\n", op);
		abort();
	}
}

int
invoke_dissector(void *func, u_int length, register_t arg2,
    register_t arg3 _U_, register_t arg4 _U_, register_t arg5 _U_,
    netdissect_options *ndo, const u_char *bp, const u_char *bp2,
    void *carg1 _U_, void *carg2 _U_)
{
	register_t op;

	if (func == (void *)_eap_print)
		op = TCPDUMP_HELPER_OP_EAP_PRINT;
	else if (func == (void *)_arp_print)
		op = TCPDUMP_HELPER_OP_ARP_PRINT;
	else if (func == (void *)_tipc_print)
		op = TCPDUMP_HELPER_OP_TIPC_PRINT;
	else if (func == (void *)_msnlb_print)
		op = TCPDUMP_HELPER_OP_MSNLB_PRINT;
	else if (func == (void *)_icmp6_print)
		op = TCPDUMP_HELPER_OP_ICMP6_PRINT;
	else if (func == (void *)_isakmp_print)
		op = TCPDUMP_HELPER_OP_ISAKMP_PRINT;
	else if (func == (void *)_isakmp_rfc3948_print)
		op = TCPDUMP_HELPER_OP_ISAKMP_RFC3948_PRINT;
	else if (func == (void *)_ip_print)
		op = TCPDUMP_HELPER_OP_IP_PRINT;
	else if (func == (void *)_ip_print_inner)
		op = TCPDUMP_HELPER_OP_IP_PRINT_INNER;
	else if (func == (void *)_rrcp_print)
		op = TCPDUMP_HELPER_OP_RRCP_PRINT;

	else if (func == (void *)_telnet_print)
		op = TCPDUMP_HELPER_OP_TELNET_PRINT;
	else if (func == (void *)_aarp_print)
		op = TCPDUMP_HELPER_OP_AARP_PRINT;
	else if (func == (void *)_aodv_print)
		op = TCPDUMP_HELPER_OP_AODV_PRINT;
	else if (func == (void *)_atalk_print)
		op = TCPDUMP_HELPER_OP_ATALK_PRINT;
	else if (func == (void *)_atm_print)
		op = TCPDUMP_HELPER_OP_ATM_PRINT;
	else if (func == (void *)_oam_print)
		op = TCPDUMP_HELPER_OP_OAM_PRINT;
	else if (func == (void *)_bootp_print)
		op = TCPDUMP_HELPER_OP_BOOTP_PRINT;
	else if (func == (void *)_bgp_print)
		op = TCPDUMP_HELPER_OP_BGP_PRINT;
	else if (func == (void *)_beep_print)
		op = TCPDUMP_HELPER_OP_BEEP_PRINT;
	else if (func == (void *)_cnfp_print)
		op = TCPDUMP_HELPER_OP_CNFP_PRINT;
	else if (func == (void *)_decnet_print)
		op = TCPDUMP_HELPER_OP_DECNET_PRINT;
	else if (func == (void *)_dvmrp_print)
		op = TCPDUMP_HELPER_OP_DVMRP_PRINT;
	else if (func == (void *)_egp_print)
		op = TCPDUMP_HELPER_OP_EGP_PRINT;
	else if (func == (void *)_pfsync_ip_print)
		op = TCPDUMP_HELPER_OP_PFSYNC_IP_PRINT;
	else if (func == (void *)_fddi_print)
		op = TCPDUMP_HELPER_OP_FDDI_PRINT;
	else if (func == (void *)_gre_print)
		op = TCPDUMP_HELPER_OP_GRE_PRINT;
	else if (func == (void *)_icmp_print)
		op = TCPDUMP_HELPER_OP_ICMP_PRINT;
	else if (func == (void *)_igmp_print)
		op = TCPDUMP_HELPER_OP_IGMP_PRINT;
	else if (func == (void *)_igrp_print)
		op = TCPDUMP_HELPER_OP_IGRP_PRINT;
	else if (func == (void *)_ipx_print)
		op = TCPDUMP_HELPER_OP_IPX_PRINT;
	else if (func == (void *)_isoclns_print)
		op = TCPDUMP_HELPER_OP_ISOCLNS_PRINT;
	else if (func == (void *)_krb_print)
		op = TCPDUMP_HELPER_OP_KRB_PRINT;
	else if (func == (void *)_msdp_print)
		op = TCPDUMP_HELPER_OP_MSDP_PRINT;
	else if (func == (void *)_nfsreply_print)
		op = TCPDUMP_HELPER_OP_NFSREPLY_PRINT;
	else if (func == (void *)_nfsreq_print)
		op = TCPDUMP_HELPER_OP_NFSREQ_PRINT;
	else if (func == (void *)_ns_print)
		op = TCPDUMP_HELPER_OP_NS_PRINT;
	else if (func == (void *)_ntp_print)
		op = TCPDUMP_HELPER_OP_NTP_PRINT;
	else if (func == (void *)_ospf_print)
		op = TCPDUMP_HELPER_OP_OSPF_PRINT;
	else if (func == (void *)_olsr_print)
		op = TCPDUMP_HELPER_OP_OLSR_PRINT;
	else if (func == (void *)_pimv1_print)
		op = TCPDUMP_HELPER_OP_PIMV1_PRINT;
	else if (func == (void *)_cisco_autorp_print)
		op = TCPDUMP_HELPER_OP_CISCO_AUTORP_PRINT;
	else if (func == (void *)_rsvp_print)
		op = TCPDUMP_HELPER_OP_RSVP_PRINT;
	else if (func == (void *)_ldp_print)
		op = TCPDUMP_HELPER_OP_LDP_PRINT;
	else if (func == (void *)_lldp_print)
		op = TCPDUMP_HELPER_OP_LLDP_PRINT;
	else if (func == (void *)_rpki_rtr_print)
		op = TCPDUMP_HELPER_OP_RPKI_RTR_PRINT;
	else if (func == (void *)_lmp_print)
		op = TCPDUMP_HELPER_OP_LMP_PRINT;
	else if (func == (void *)_lspping_print)
		op = TCPDUMP_HELPER_OP_LSPPING_PRINT;
	else if (func == (void *)_lwapp_control_print)
		op = TCPDUMP_HELPER_OP_LWAPP_CONTROL_PRINT;
	else if (func == (void *)_lwapp_data_print)
		op = TCPDUMP_HELPER_OP_LWAPP_DATA_PRINT;
	else if (func == (void *)_eigrp_print)
		op = TCPDUMP_HELPER_OP_EIGRP_PRINT;
	else if (func == (void *)_mobile_print)
		op = TCPDUMP_HELPER_OP_MOBILE_PRINT;
	else if (func == (void *)_pim_print)
		op = TCPDUMP_HELPER_OP_PIM_PRINT;
	else if (func == (void *)_q933_print)
		op = TCPDUMP_HELPER_OP_Q933_PRINT;
	else if (func == (void *)_vqp_print)
		op = TCPDUMP_HELPER_OP_VQP_PRINT;
	else if (func == (void *)_rip_print)
		op = TCPDUMP_HELPER_OP_RIP_PRINT;
	else if (func == (void *)_lane_print)
		op = TCPDUMP_HELPER_OP_LANE_PRINT;
	else if (func == (void *)_snmp_print)
		op = TCPDUMP_HELPER_OP_SNMP_PRINT;
	else if (func == (void *)_sunrpcrequest_print)
		op = TCPDUMP_HELPER_OP_SUNRPCREQUEST_PRINT;
	else if (func == (void *)_tcp_print)
		op = TCPDUMP_HELPER_OP_TCP_PRINT;
	else if (func == (void *)_tftp_print)
		op = TCPDUMP_HELPER_OP_TFTP_PRINT;
	else if (func == (void *)_timed_print)
		op = TCPDUMP_HELPER_OP_TIMED_PRINT;
	else if (func == (void *)_udld_print)
		op = TCPDUMP_HELPER_OP_UDLD_PRINT;
	else if (func == (void *)_udp_print)
		op = TCPDUMP_HELPER_OP_UDP_PRINT;
	else if (func == (void *)_vtp_print)
		op = TCPDUMP_HELPER_OP_VTP_PRINT;
	else if (func == (void *)_wb_print)
		op = TCPDUMP_HELPER_OP_WB_PRINT;
#if 0
	else if (func == (void *)_rx_print)
		op = TCPDUMP_HELPER_OP_RX_PRINT;
#endif
	else if (func == (void *)_netbeui_print)
		op = TCPDUMP_HELPER_OP_NETBEUI_PRINT;
	else if (func == (void *)_ipx_netbios_print)
		op = TCPDUMP_HELPER_OP_IPX_NETBIOS_PRINT;
	else if (func == (void *)_nbt_tcp_print)
		op = TCPDUMP_HELPER_OP_NBT_TCP_PRINT;
	else if (func == (void *)_nbt_udp137_print)
		op = TCPDUMP_HELPER_OP_NBT_UDP137_PRINT;
	else if (func == (void *)_nbt_udp138_print)
		op = TCPDUMP_HELPER_OP_NBT_UDP138_PRINT;
	else if (func == (void *)_smb_tcp_print)
		op = TCPDUMP_HELPER_OP_SMB_TCP_PRINT;
	else if (func == (void *)_l2tp_print)
		op = TCPDUMP_HELPER_OP_L2TP_PRINT;
	else if (func == (void *)_vrrp_print)
		op = TCPDUMP_HELPER_OP_VRRP_PRINT;
	else if (func == (void *)_carp_print)
		op = TCPDUMP_HELPER_OP_CARP_PRINT;
	else if (func == (void *)_slow_print)
		op = TCPDUMP_HELPER_OP_SLOW_PRINT;
	else if (func == (void *)_sflow_print)
		op = TCPDUMP_HELPER_OP_SFLOW_PRINT;
	else if (func == (void *)_mpcp_print)
		op = TCPDUMP_HELPER_OP_MPCP_PRINT;
	else if (func == (void *)_cfm_print)
		op = TCPDUMP_HELPER_OP_CFM_PRINT;
	else if (func == (void *)_pgm_print)
		op = TCPDUMP_HELPER_OP_PGM_PRINT;
	else if (func == (void *)_cdp_print)
		op = TCPDUMP_HELPER_OP_CDP_PRINT;
	else if (func == (void *)_dtp_print)
		op = TCPDUMP_HELPER_OP_DTP_PRINT;
	else if (func == (void *)_stp_print)
		op = TCPDUMP_HELPER_OP_STP_PRINT;
	else if (func == (void *)_radius_print)
		op = TCPDUMP_HELPER_OP_RADIUS_PRINT;
	else if (func == (void *)_lwres_print)
		op = TCPDUMP_HELPER_OP_LWRES_PRINT;
	else if (func == (void *)_pptp_print)
		op = TCPDUMP_HELPER_OP_PPTP_PRINT;
	else if (func == (void *)_dccp_print)
		op = TCPDUMP_HELPER_OP_DCCP_PRINT;
	else if (func == (void *)_sctp_print)
		op = TCPDUMP_HELPER_OP_SCTP_PRINT;
	else if (func == (void *)_forces_print)
		op = TCPDUMP_HELPER_OP_FORCES_PRINT;
	else if (func == (void *)_mpls_print)
		op = TCPDUMP_HELPER_OP_MPLS_PRINT;
	else if (func == (void *)_zephyr_print)
		op = TCPDUMP_HELPER_OP_ZEPHYR_PRINT;
	else if (func == (void *)_zmtp1_print)
		op = TCPDUMP_HELPER_OP_ZMTP1_PRINT;
	else if (func == (void *)_hsrp_print)
		op = TCPDUMP_HELPER_OP_HSRP_PRINT;
	else if (func == (void *)_bfd_print)
		op = TCPDUMP_HELPER_OP_BFD_PRINT;
	else if (func == (void *)_sip_print)
		op = TCPDUMP_HELPER_OP_SIP_PRINT;
	else if (func == (void *)_syslog_print)
		op = TCPDUMP_HELPER_OP_SYSLOG_PRINT;
	else if (func == (void *)_vxlan_print)
		op = TCPDUMP_HELPER_OP_VXLAN_PRINT;
	else if (func == (void *)_otv_print)
		op = TCPDUMP_HELPER_OP_OTV_PRINT;
	else if (func == (void *)_token_print)
		op = TCPDUMP_HELPER_OP_TOKEN_PRINT;
	else if (func == (void *)_fr_print)
		op = TCPDUMP_HELPER_OP_FR_PRINT;
	else if (func == (void *)_mfr_print)
		op = TCPDUMP_HELPER_OP_MFR_PRINT;
	else if (func == (void *)_llap_print)
		op = TCPDUMP_HELPER_OP_LLAP_PRINT;
	else if (func == (void *)_pppoe_print)
		op = TCPDUMP_HELPER_OP_PPPOE_PRINT;
	else if (func == (void *)_ppp_print)
		op = TCPDUMP_HELPER_OP_PPP_PRINT;
	else if (func == (void *)_chdlc_print)
		op = TCPDUMP_HELPER_OP_CHDLC_PRINT;
	else if (func == (void *)_ripng_print)
		op = TCPDUMP_HELPER_OP_RIPNG_PRINT;
	else if (func == (void *)_ospf6_print)
		op = TCPDUMP_HELPER_OP_OSPF6_PRINT;
	else if (func == (void *)_dhcp6_print)
		op = TCPDUMP_HELPER_OP_DHCP6_PRINT;
	else if (func == (void *)_babel_print)
		op = TCPDUMP_HELPER_OP_BABEL_PRINT;
	else
		return (0);

	if (gpso != NULL &&
	    cheri_getlen(gpso) != 0) {
		if (0 != cheri_invoke(*gpso, op, length,
		    arg2, arg3, arg4, arg5, 0, 0,
		    ndo, NULL, NULL, (void *)bp,
		    cheri_incbase(gpso, sizeof(struct cheri_object)),
		    (void *)bp2,
		    carg1, carg2)) {
			printf("failure in sandbox op=%d\n", (int)op);
			abort();
		}
		return(1);
	} else
		return (0);

}

void
pawned(void)
{

	cheri_system_methodnum_puts = CHERI_TCPDUMP_PUTS_PAWNED;
	cheri_system_methodnum_putchar = CHERI_TCPDUMP_PUTCHAR_PAWNED;
	printf(">>> ATTACKER OUTPUT <<<");
}
