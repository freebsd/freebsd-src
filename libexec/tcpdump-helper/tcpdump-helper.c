/*-
 * Copyright (c) 2014-2015 SRI International
 * Copyright (c) 2012-2015 Robert N. M. Watson
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

#include "tcpdump-stdinc.h"
#include "netdissect.h"
#include "interface.h"
#include "print.h"

#include "cheri_tcpdump_system.h"
#include "tcpdump-helper.h"

struct print_info printinfo;
netdissect_options Gndo;
netdissect_options *gndo = &Gndo;
struct cheri_object gnext_sandbox;
struct cheri_object cheri_tcpdump;

const char *program_name;

void	pawned(void);

/*
 * XXXRW: Too many immediate arguments?
 */
int	invoke(struct cheri_object co __unused, register_t v0 __unused,
	    register_t methodnum,
	    register_t arg1, register_t arg2,
	    register_t arg3, register_t arg4, register_t arg5,
	    netdissect_options *ndo,
	    const u_char *sp,
	    const u_char *sp2,
	    void* carg1, void *carg2)
	    __attribute__((cheri_ccall)); /* XXXRW: Will be ccheri_ccallee. */

static void	dispatch_dissector(register_t methodnum, u_int length,
    register_t arg2, register_t arg3, register_t arg4, register_t arg5,
    netdissect_options *ndo, const u_char *bp, const u_char *bp2,
    void *carg1, void *carg2);

int
cheri_tcpdump_sandbox_init(bpf_u_int32 localnet, bpf_u_int32 netmask,
    uint32_t timezone_offset, const netdissect_options *ndo,
    struct cheri_object next_sandbox)
{

/* XXXBD: broken, use system default
	cheri_system_methodnum_puts = CHERI_TCPDUMP_PUTS;
	cheri_system_methodnum_putchar = CHERI_TCPDUMP_PUTCHAR;
*/

	program_name = "tcpdump-helper"; /* XXX: copy from parent? */

	/*
	 * Make a copy of the parent's netdissect_options.  Most of the
	 * items are unchanged until the next init or per-packet.
	 */
	memcpy_c(gndo, ndo, sizeof(netdissect_options));
	gndo->ndo_printf = tcpdump_printf;
	gndo->ndo_default_print = ndo_default_print;
	gndo->ndo_error = ndo_error;
	gndo->ndo_warning = ndo_warning;

	init_print(localnet, netmask, timezone_offset);

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

	gnext_sandbox = next_sandbox;

	return (0);
}

int
cheri_sandbox_has_printer(int type)
{

	return (has_printer(type));
}

int
invoke(struct cheri_object co __unused, register_t v0 __unused,
    register_t methodnum, register_t arg1,
    register_t arg2, register_t arg3, register_t arg4, register_t arg5,
    netdissect_options *ndo,
    const u_char *sp,
    const u_char *sp2, void * carg1, void *carg2)
{

	dispatch_dissector(methodnum, arg1, arg2, arg3, arg4, arg5,
	    ndo, sp, sp2, carg1, carg2);

	return (0);
}

int
cheri_sandbox_pretty_print_packet(const struct pcap_pkthdr *h,
    const u_char *sp)
{
	int ret;

	ret = 0;

#ifdef DEBUG
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
	memcpy((void *)gndo->ndo_packetp, sp, h->caplen);
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

	return (ret);
}

static void
dispatch_dissector(register_t methodnum, u_int length, register_t arg2,
    register_t arg3 _U_, register_t arg4 _U_, register_t arg5 _U_,
    netdissect_options *ndo,
    const u_char *bp, const u_char *bp2, void *carg1 _U_, void *carg2 _U_)
{
	snapend = bp + length; /* set to end of capability? */

	switch (methodnum) {
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
		_telnet_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_AARP_PRINT:
		_aarp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_AODV_PRINT:
		_aodv_print(ndo, bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_ATALK_PRINT:
		_atalk_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_ATM_PRINT:
		_atm_print(ndo, arg2, arg3, arg4, bp, length, arg5);
		break;

	case TCPDUMP_HELPER_OP_OAM_PRINT:
		_oam_print(ndo, bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_BOOTP_PRINT:
		_bootp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_BGP_PRINT:
		_bgp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_BEEP_PRINT:
		_beep_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_CNFP_PRINT:
		_cnfp_print(ndo, bp, bp2);
		break;

	case TCPDUMP_HELPER_OP_DECNET_PRINT:
		_decnet_print(ndo, bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_DVMRP_PRINT:
		_dvmrp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_EGP_PRINT:
		_egp_print(ndo, bp, length);
		break;

	/* XXX-BD: not converted.  Is it in upstream? */
	case TCPDUMP_HELPER_OP_PFSYNC_IP_PRINT:
		_pfsync_ip_print(bp, length);
		break;

	case TCPDUMP_HELPER_OP_FDDI_PRINT:
		_fddi_print(ndo, bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_GRE_PRINT:
		_gre_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_ICMP_PRINT:
		_icmp_print(ndo, bp, length, bp2, arg2);
		break;

	case TCPDUMP_HELPER_OP_IGMP_PRINT:
		_igmp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_IGRP_PRINT:
		_igrp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_IPX_PRINT:
		_ipx_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_ISOCLNS_PRINT:
		_isoclns_print(ndo, bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_KRB_PRINT:
		_krb_print(ndo, bp);
		break;

	case TCPDUMP_HELPER_OP_MSDP_PRINT:
		_msdp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_NFSREPLY_PRINT:
		_nfsreply_print(ndo, bp, length, bp2);
		break;

	case TCPDUMP_HELPER_OP_NFSREQ_PRINT:
		_nfsreq_print_noaddr(ndo, bp, length, bp2);
		break;

	case TCPDUMP_HELPER_OP_NS_PRINT:
		_ns_print(ndo, bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_NTP_PRINT:
		_ntp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_OSPF_PRINT:
		_ospf_print(ndo, bp, length, bp2);
		break;

	case TCPDUMP_HELPER_OP_OLSR_PRINT:
		_olsr_print(ndo, bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_PIMV1_PRINT:
		_pimv1_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_CISCO_AUTORP_PRINT:
		_cisco_autorp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_RSVP_PRINT:
		_rsvp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_LDP_PRINT:
		_ldp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_LLDP_PRINT:
		_lldp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_RPKI_RTR_PRINT:
		_rpki_rtr_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_LMP_PRINT:
		_lmp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_LSPPING_PRINT:
		_lspping_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_LWAPP_CONTROL_PRINT:
		_lwapp_control_print(ndo, bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_LWAPP_DATA_PRINT:
		_lwapp_data_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_EIGRP_PRINT:
		_eigrp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_MOBILE_PRINT:
		_mobile_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_PIM_PRINT:
		_pim_print(ndo, bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_Q933_PRINT:
		_q933_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_VQP_PRINT:
		_vqp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_RIP_PRINT:
		_rip_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_LANE_PRINT:
		_lane_print(ndo, bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_SNMP_PRINT:
		_snmp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_SUNRPCREQUEST_PRINT:
		_sunrpcrequest_print(ndo, bp, length, bp2);
		break;

	case TCPDUMP_HELPER_OP_TCP_PRINT:
		_tcp_print(ndo, bp, length, bp2, arg2);
		break;

	case TCPDUMP_HELPER_OP_TFTP_PRINT:
		_tftp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_TIMED_PRINT:
		_timed_print(ndo, bp);
		break;

	case TCPDUMP_HELPER_OP_UDLD_PRINT:
		_udld_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_UDP_PRINT:
		_udp_print(ndo, bp, length, bp2, arg2);
		break;

	case TCPDUMP_HELPER_OP_VTP_PRINT:
		_vtp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_WB_PRINT:
		_wb_print(ndo, bp, length);
		break;

#if 0
	case TCPDUMP_HELPER_OP_RX_PRINT:
		_rx_print(ndo, bp, length, arg2, arg3, bp2);
		break;
#endif

	case TCPDUMP_HELPER_OP_NETBEUI_PRINT:
		_netbeui_print(ndo, arg2, bp, length);
		break;

	case TCPDUMP_HELPER_OP_IPX_NETBIOS_PRINT:
		_ipx_netbios_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_NBT_TCP_PRINT:
		_nbt_tcp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_NBT_UDP137_PRINT:
		_nbt_udp137_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_NBT_UDP138_PRINT:
		_nbt_udp138_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_SMB_TCP_PRINT:
		_smb_tcp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_L2TP_PRINT:
		_l2tp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_VRRP_PRINT:
		_vrrp_print(ndo, bp, length, bp2, arg2);
		break;

	case TCPDUMP_HELPER_OP_CARP_PRINT:
		_carp_print(ndo, bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_SLOW_PRINT:
		_slow_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_SFLOW_PRINT:
		_sflow_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_MPCP_PRINT:
		_mpcp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_CFM_PRINT:
		_cfm_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_PGM_PRINT:
		_pgm_print(ndo, bp, length, bp2);
		break;

	case TCPDUMP_HELPER_OP_CDP_PRINT:
		_cdp_print(ndo, bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_DTP_PRINT:
		_dtp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_STP_PRINT:
		_stp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_RADIUS_PRINT:
		_radius_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_LWRES_PRINT:
		_lwres_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_PPTP_PRINT:
		_pptp_print(ndo, bp);
		break;

	case TCPDUMP_HELPER_OP_DCCP_PRINT:
		_dccp_print(ndo, bp, bp2, length);
		break;

	case TCPDUMP_HELPER_OP_SCTP_PRINT:
		_sctp_print(ndo, bp, bp2, length);
		break;

	case TCPDUMP_HELPER_OP_FORCES_PRINT:
		_forces_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_MPLS_PRINT:
		_mpls_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_ZEPHYR_PRINT:
		_zephyr_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_ZMTP1_PRINT:
		_zmtp1_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_HSRP_PRINT:
		_hsrp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_BFD_PRINT:
		_bfd_print(ndo, bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_SIP_PRINT:
		_sip_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_SYSLOG_PRINT:
		_syslog_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_VXLAN_PRINT:
		_vxlan_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_OTV_PRINT:
		_otv_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_TOKEN_PRINT:
		_token_print(ndo, bp, length, arg2);
		break;

	case TCPDUMP_HELPER_OP_FR_PRINT:
		_fr_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_MFR_PRINT:
		_mfr_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_LLAP_PRINT:
		_llap_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_PPPOE_PRINT:
		_pppoe_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_PPP_PRINT:
		_ppp_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_CHDLC_PRINT:
		_chdlc_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_RIPNG_PRINT:
		_ripng_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_OSPF6_PRINT:
		_ospf6_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_DHCP6_PRINT:
		_dhcp6_print(ndo, bp, length);
		break;

	case TCPDUMP_HELPER_OP_BABEL_PRINT:
		_babel_print(ndo, bp, length);
		break;

	default:
		printf("unknown method %ld\n", methodnum);
		abort();
	}
}

int
invoke_dissector(void *func, u_int length, register_t arg2,
    register_t arg3 _U_, register_t arg4 _U_, register_t arg5 _U_,
    netdissect_options *ndo, const u_char *bp, const u_char *bp2,
    void *carg1 _U_, void *carg2 _U_)
{
	register_t methodnum;

	if (func == (void *)_eap_print)
		methodnum = TCPDUMP_HELPER_OP_EAP_PRINT;
	else if (func == (void *)_arp_print)
		methodnum = TCPDUMP_HELPER_OP_ARP_PRINT;
	else if (func == (void *)_tipc_print)
		methodnum = TCPDUMP_HELPER_OP_TIPC_PRINT;
	else if (func == (void *)_msnlb_print)
		methodnum = TCPDUMP_HELPER_OP_MSNLB_PRINT;
	else if (func == (void *)_icmp6_print)
		methodnum = TCPDUMP_HELPER_OP_ICMP6_PRINT;
	else if (func == (void *)_isakmp_print)
		methodnum = TCPDUMP_HELPER_OP_ISAKMP_PRINT;
	else if (func == (void *)_isakmp_rfc3948_print)
		methodnum = TCPDUMP_HELPER_OP_ISAKMP_RFC3948_PRINT;
	else if (func == (void *)_ip_print)
		methodnum = TCPDUMP_HELPER_OP_IP_PRINT;
	else if (func == (void *)_ip_print_inner)
		methodnum = TCPDUMP_HELPER_OP_IP_PRINT_INNER;
	else if (func == (void *)_rrcp_print)
		methodnum = TCPDUMP_HELPER_OP_RRCP_PRINT;

	else if (func == (void *)_telnet_print)
		methodnum = TCPDUMP_HELPER_OP_TELNET_PRINT;
	else if (func == (void *)_aarp_print)
		methodnum = TCPDUMP_HELPER_OP_AARP_PRINT;
	else if (func == (void *)_aodv_print)
		methodnum = TCPDUMP_HELPER_OP_AODV_PRINT;
	else if (func == (void *)_atalk_print)
		methodnum = TCPDUMP_HELPER_OP_ATALK_PRINT;
	else if (func == (void *)_atm_print)
		methodnum = TCPDUMP_HELPER_OP_ATM_PRINT;
	else if (func == (void *)_oam_print)
		methodnum = TCPDUMP_HELPER_OP_OAM_PRINT;
	else if (func == (void *)_bootp_print)
		methodnum = TCPDUMP_HELPER_OP_BOOTP_PRINT;
	else if (func == (void *)_bgp_print)
		methodnum = TCPDUMP_HELPER_OP_BGP_PRINT;
	else if (func == (void *)_beep_print)
		methodnum = TCPDUMP_HELPER_OP_BEEP_PRINT;
	else if (func == (void *)_cnfp_print)
		methodnum = TCPDUMP_HELPER_OP_CNFP_PRINT;
	else if (func == (void *)_decnet_print)
		methodnum = TCPDUMP_HELPER_OP_DECNET_PRINT;
	else if (func == (void *)_dvmrp_print)
		methodnum = TCPDUMP_HELPER_OP_DVMRP_PRINT;
	else if (func == (void *)_egp_print)
		methodnum = TCPDUMP_HELPER_OP_EGP_PRINT;
	else if (func == (void *)_pfsync_ip_print)
		methodnum = TCPDUMP_HELPER_OP_PFSYNC_IP_PRINT;
	else if (func == (void *)_fddi_print)
		methodnum = TCPDUMP_HELPER_OP_FDDI_PRINT;
	else if (func == (void *)_gre_print)
		methodnum = TCPDUMP_HELPER_OP_GRE_PRINT;
	else if (func == (void *)_icmp_print)
		methodnum = TCPDUMP_HELPER_OP_ICMP_PRINT;
	else if (func == (void *)_igmp_print)
		methodnum = TCPDUMP_HELPER_OP_IGMP_PRINT;
	else if (func == (void *)_igrp_print)
		methodnum = TCPDUMP_HELPER_OP_IGRP_PRINT;
	else if (func == (void *)_ipx_print)
		methodnum = TCPDUMP_HELPER_OP_IPX_PRINT;
	else if (func == (void *)_isoclns_print)
		methodnum = TCPDUMP_HELPER_OP_ISOCLNS_PRINT;
	else if (func == (void *)_krb_print)
		methodnum = TCPDUMP_HELPER_OP_KRB_PRINT;
	else if (func == (void *)_msdp_print)
		methodnum = TCPDUMP_HELPER_OP_MSDP_PRINT;
	else if (func == (void *)_nfsreply_print)
		methodnum = TCPDUMP_HELPER_OP_NFSREPLY_PRINT;
	else if (func == (void *)_nfsreq_print_noaddr)
		methodnum = TCPDUMP_HELPER_OP_NFSREQ_PRINT;
	else if (func == (void *)_ns_print)
		methodnum = TCPDUMP_HELPER_OP_NS_PRINT;
	else if (func == (void *)_ntp_print)
		methodnum = TCPDUMP_HELPER_OP_NTP_PRINT;
	else if (func == (void *)_ospf_print)
		methodnum = TCPDUMP_HELPER_OP_OSPF_PRINT;
	else if (func == (void *)_olsr_print)
		methodnum = TCPDUMP_HELPER_OP_OLSR_PRINT;
	else if (func == (void *)_pimv1_print)
		methodnum = TCPDUMP_HELPER_OP_PIMV1_PRINT;
	else if (func == (void *)_cisco_autorp_print)
		methodnum = TCPDUMP_HELPER_OP_CISCO_AUTORP_PRINT;
	else if (func == (void *)_rsvp_print)
		methodnum = TCPDUMP_HELPER_OP_RSVP_PRINT;
	else if (func == (void *)_ldp_print)
		methodnum = TCPDUMP_HELPER_OP_LDP_PRINT;
	else if (func == (void *)_lldp_print)
		methodnum = TCPDUMP_HELPER_OP_LLDP_PRINT;
	else if (func == (void *)_rpki_rtr_print)
		methodnum = TCPDUMP_HELPER_OP_RPKI_RTR_PRINT;
	else if (func == (void *)_lmp_print)
		methodnum = TCPDUMP_HELPER_OP_LMP_PRINT;
	else if (func == (void *)_lspping_print)
		methodnum = TCPDUMP_HELPER_OP_LSPPING_PRINT;
	else if (func == (void *)_lwapp_control_print)
		methodnum = TCPDUMP_HELPER_OP_LWAPP_CONTROL_PRINT;
	else if (func == (void *)_lwapp_data_print)
		methodnum = TCPDUMP_HELPER_OP_LWAPP_DATA_PRINT;
	else if (func == (void *)_eigrp_print)
		methodnum = TCPDUMP_HELPER_OP_EIGRP_PRINT;
	else if (func == (void *)_mobile_print)
		methodnum = TCPDUMP_HELPER_OP_MOBILE_PRINT;
	else if (func == (void *)_pim_print)
		methodnum = TCPDUMP_HELPER_OP_PIM_PRINT;
	else if (func == (void *)_q933_print)
		methodnum = TCPDUMP_HELPER_OP_Q933_PRINT;
	else if (func == (void *)_vqp_print)
		methodnum = TCPDUMP_HELPER_OP_VQP_PRINT;
	else if (func == (void *)_rip_print)
		methodnum = TCPDUMP_HELPER_OP_RIP_PRINT;
	else if (func == (void *)_lane_print)
		methodnum = TCPDUMP_HELPER_OP_LANE_PRINT;
	else if (func == (void *)_snmp_print)
		methodnum = TCPDUMP_HELPER_OP_SNMP_PRINT;
	else if (func == (void *)_sunrpcrequest_print)
		methodnum = TCPDUMP_HELPER_OP_SUNRPCREQUEST_PRINT;
	else if (func == (void *)_tcp_print)
		methodnum = TCPDUMP_HELPER_OP_TCP_PRINT;
	else if (func == (void *)_tftp_print)
		methodnum = TCPDUMP_HELPER_OP_TFTP_PRINT;
	else if (func == (void *)_timed_print)
		methodnum = TCPDUMP_HELPER_OP_TIMED_PRINT;
	else if (func == (void *)_udld_print)
		methodnum = TCPDUMP_HELPER_OP_UDLD_PRINT;
	else if (func == (void *)_udp_print)
		methodnum = TCPDUMP_HELPER_OP_UDP_PRINT;
	else if (func == (void *)_vtp_print)
		methodnum = TCPDUMP_HELPER_OP_VTP_PRINT;
	else if (func == (void *)_wb_print)
		methodnum = TCPDUMP_HELPER_OP_WB_PRINT;
#if 0
	else if (func == (void *)_rx_print)
		methodnum = TCPDUMP_HELPER_OP_RX_PRINT;
#endif
	else if (func == (void *)_netbeui_print)
		methodnum = TCPDUMP_HELPER_OP_NETBEUI_PRINT;
	else if (func == (void *)_ipx_netbios_print)
		methodnum = TCPDUMP_HELPER_OP_IPX_NETBIOS_PRINT;
	else if (func == (void *)_nbt_tcp_print)
		methodnum = TCPDUMP_HELPER_OP_NBT_TCP_PRINT;
	else if (func == (void *)_nbt_udp137_print)
		methodnum = TCPDUMP_HELPER_OP_NBT_UDP137_PRINT;
	else if (func == (void *)_nbt_udp138_print)
		methodnum = TCPDUMP_HELPER_OP_NBT_UDP138_PRINT;
	else if (func == (void *)_smb_tcp_print)
		methodnum = TCPDUMP_HELPER_OP_SMB_TCP_PRINT;
	else if (func == (void *)_l2tp_print)
		methodnum = TCPDUMP_HELPER_OP_L2TP_PRINT;
	else if (func == (void *)_vrrp_print)
		methodnum = TCPDUMP_HELPER_OP_VRRP_PRINT;
	else if (func == (void *)_carp_print)
		methodnum = TCPDUMP_HELPER_OP_CARP_PRINT;
	else if (func == (void *)_slow_print)
		methodnum = TCPDUMP_HELPER_OP_SLOW_PRINT;
	else if (func == (void *)_sflow_print)
		methodnum = TCPDUMP_HELPER_OP_SFLOW_PRINT;
	else if (func == (void *)_mpcp_print)
		methodnum = TCPDUMP_HELPER_OP_MPCP_PRINT;
	else if (func == (void *)_cfm_print)
		methodnum = TCPDUMP_HELPER_OP_CFM_PRINT;
	else if (func == (void *)_pgm_print)
		methodnum = TCPDUMP_HELPER_OP_PGM_PRINT;
	else if (func == (void *)_cdp_print)
		methodnum = TCPDUMP_HELPER_OP_CDP_PRINT;
	else if (func == (void *)_dtp_print)
		methodnum = TCPDUMP_HELPER_OP_DTP_PRINT;
	else if (func == (void *)_stp_print)
		methodnum = TCPDUMP_HELPER_OP_STP_PRINT;
	else if (func == (void *)_radius_print)
		methodnum = TCPDUMP_HELPER_OP_RADIUS_PRINT;
	else if (func == (void *)_lwres_print)
		methodnum = TCPDUMP_HELPER_OP_LWRES_PRINT;
	else if (func == (void *)_pptp_print)
		methodnum = TCPDUMP_HELPER_OP_PPTP_PRINT;
	else if (func == (void *)_dccp_print)
		methodnum = TCPDUMP_HELPER_OP_DCCP_PRINT;
	else if (func == (void *)_sctp_print)
		methodnum = TCPDUMP_HELPER_OP_SCTP_PRINT;
	else if (func == (void *)_forces_print)
		methodnum = TCPDUMP_HELPER_OP_FORCES_PRINT;
	else if (func == (void *)_mpls_print)
		methodnum = TCPDUMP_HELPER_OP_MPLS_PRINT;
	else if (func == (void *)_zephyr_print)
		methodnum = TCPDUMP_HELPER_OP_ZEPHYR_PRINT;
	else if (func == (void *)_zmtp1_print)
		methodnum = TCPDUMP_HELPER_OP_ZMTP1_PRINT;
	else if (func == (void *)_hsrp_print)
		methodnum = TCPDUMP_HELPER_OP_HSRP_PRINT;
	else if (func == (void *)_bfd_print)
		methodnum = TCPDUMP_HELPER_OP_BFD_PRINT;
	else if (func == (void *)_sip_print)
		methodnum = TCPDUMP_HELPER_OP_SIP_PRINT;
	else if (func == (void *)_syslog_print)
		methodnum = TCPDUMP_HELPER_OP_SYSLOG_PRINT;
	else if (func == (void *)_vxlan_print)
		methodnum = TCPDUMP_HELPER_OP_VXLAN_PRINT;
	else if (func == (void *)_otv_print)
		methodnum = TCPDUMP_HELPER_OP_OTV_PRINT;
	else if (func == (void *)_token_print)
		methodnum = TCPDUMP_HELPER_OP_TOKEN_PRINT;
	else if (func == (void *)_fr_print)
		methodnum = TCPDUMP_HELPER_OP_FR_PRINT;
	else if (func == (void *)_mfr_print)
		methodnum = TCPDUMP_HELPER_OP_MFR_PRINT;
	else if (func == (void *)_llap_print)
		methodnum = TCPDUMP_HELPER_OP_LLAP_PRINT;
	else if (func == (void *)_pppoe_print)
		methodnum = TCPDUMP_HELPER_OP_PPPOE_PRINT;
	else if (func == (void *)_ppp_print)
		methodnum = TCPDUMP_HELPER_OP_PPP_PRINT;
	else if (func == (void *)_chdlc_print)
		methodnum = TCPDUMP_HELPER_OP_CHDLC_PRINT;
	else if (func == (void *)_ripng_print)
		methodnum = TCPDUMP_HELPER_OP_RIPNG_PRINT;
	else if (func == (void *)_ospf6_print)
		methodnum = TCPDUMP_HELPER_OP_OSPF6_PRINT;
	else if (func == (void *)_dhcp6_print)
		methodnum = TCPDUMP_HELPER_OP_DHCP6_PRINT;
	else if (func == (void *)_babel_print)
		methodnum = TCPDUMP_HELPER_OP_BABEL_PRINT;
	else
		return (0);

	if (!CHERI_OBJECT_ISNULL(gnext_sandbox)) {
		if (0 != cheri_invoke(gnext_sandbox,
		    CHERI_INVOKE_METHOD_LEGACY_INVOKE,
		    methodnum,
		    length, arg2, arg3, arg4, arg5, 0, 0,
		    ndo, (void *)bp, (void *)bp2, carg1, carg2,
		    NULL, NULL, NULL)) {
			printf("failure in sandbox op=%d\n", (int)methodnum);
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
