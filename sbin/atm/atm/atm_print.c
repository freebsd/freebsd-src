/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD: src/sbin/atm/atm/atm_print.c,v 1.3.2.1 2000/07/01 06:02:14 ps Exp $
 *
 */

/*
 * User configuration and display program
 * --------------------------------------
 *
 * Print routines for "show" subcommand
 *
 */

#include <sys/param.h>  
#include <sys/socket.h> 
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netatm/port.h>
#include <netatm/atm.h>
#include <netatm/atm_if.h> 
#include <netatm/atm_sap.h>
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_vc.h>
#include <netatm/atm_ioctl.h>
#include <netatm/ipatm/ipatm_var.h>
#include <netatm/sigpvc/sigpvc_var.h>
#include <netatm/spans/spans_var.h>
#include <netatm/uni/uniip_var.h>
#include <netatm/uni/unisig_var.h>

#include <errno.h>
#include <libatm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atm.h"

#ifndef lint
__RCSID("@(#) $FreeBSD: src/sbin/atm/atm/atm_print.c,v 1.3.2.1 2000/07/01 06:02:14 ps Exp $");
#endif


#define ARP_HDR \
"Net Intf  Flags  Age  Origin\n"

#define ASRV_HDR \
"Net Intf  State     ATM Address\n"

#define CFG_HDR \
"Intf      Vendor    Model     Media           Bus   Serial No\n"

#define IP_VCC_HDR \
"Net Intf  VPI   VCI  State   Flags IP Address\n"

#define INTF_HDR \
"Interface  Sigmgr   State\n"

#define NETIF_HDR \
"Net Intf  Phy Intf  IP Address\n"

#define VCC_HDR \
"Interface  VPI   VCI  AAL   Type Dir    State    Encaps   Owner\n"

#define VCC_STATS_HDR \
"                        Input    Input  Input  Output   Output Output\n\
Interface  VPI   VCI     PDUs    Bytes   Errs    PDUs    Bytes   Errs\n"

#define VERSION_HDR \
"Version\n"

#define PHY_STATS_HDR \
"             Input    Input  Input  Output   Output Output    Cmd\n\
Interface     PDUs    Bytes   Errs    PDUs    Bytes   Errs   Errs\n"

/*
 * External references
 */
extern struct proto	protos[];
extern struct aal	aals[];
extern struct encaps	encaps[];

/*
 * Local variables
 */
static int	arp_hdr = 0;
static int	asrv_hdr = 0;
static int	cfg_hdr = 0;
static int	ip_vcc_hdr = 0;
static int	netif_hdr = 0;
static int	vcc_hdr = 0;
static int	vcc_stats_hdr = 0;
static int	phy_stats_hdr = 0;
static int	version_hdr = 0;

/*
 * SIGPVC state definitions
 */
struct state	sigpvc_states[] = {
	{ "ACTIVE",	SIGPVC_ACTIVE },
	{ "DETACH",	SIGPVC_DETACH },
	{ 0,		0 }
};

/*
 * SPANS state definitions
 */
struct state	spans_states[] = {
	{ "ACTIVE",	SPANS_ACTIVE },
	{ "DETACH",	SPANS_DETACH },
	{ "INIT",	SPANS_INIT },
	{ "PROBE",	SPANS_PROBE },
	{ 0,		0 }
};

/*
 * UNISIG state definitions
 */
struct state    unisig_states[] = {
	{ "NULL",	UNISIG_NULL },
	{ "ADR_WAIT",	UNISIG_ADDR_WAIT },
	{ "INIT",	UNISIG_INIT },
	{ "ACTIVE",	UNISIG_ACTIVE },
	{ "DETACH",	UNISIG_DETACH },
	{ 0,		0 }
};

/*
 * SIGPVC VCC state definitions
 */
struct state	sigpvc_vcc_states[] = {
	{ "NULL",	VCCS_NULL },
	{ "ACTIVE",	VCCS_ACTIVE },
	{ "FREE",	VCCS_FREE },
	{ 0,		0 }
};

/*
 * SPANS VCC state definitions
 */
struct state	spans_vcc_states[] = {
	{ "NULL",	SPANS_VC_NULL },
	{ "ACTIVE",	SPANS_VC_ACTIVE },
	{ "ACT_DOWN",	SPANS_VC_ACT_DOWN },
	{ "POPEN",	SPANS_VC_POPEN },
	{ "R_POPEN",	SPANS_VC_R_POPEN },
	{ "OPEN",	SPANS_VC_OPEN },
	{ "CLOSE",	SPANS_VC_CLOSE },
	{ "ABORT",	SPANS_VC_ABORT },
	{ "FREE",	SPANS_VC_FREE },
	{0,		0 }
};

/*
 * UNISIG VCC state definitions
 */
struct state	unisig_vcc_states[] = {
	{ "NULL",	UNI_NULL },
	{ "C_INIT",	UNI_CALL_INITIATED },
	{ "C_OUT_PR",	UNI_CALL_OUT_PROC },
	{ "C_DELIV",	UNI_CALL_DELIVERED },
	{ "C_PRES",	UNI_CALL_PRESENT },
	{ "C_REC",	UNI_CALL_RECEIVED },
	{ "CONN_REQ",	UNI_CONNECT_REQUEST },
	{ "C_IN_PR",	UNI_CALL_IN_PROC },
	{ "ACTIVE",	UNI_ACTIVE },
	{ "REL_REQ",	UNI_RELEASE_REQUEST },
	{ "REL_IND",	UNI_RELEASE_IND },
	{ "SSCF_REC",	UNI_SSCF_RECOV },
	{ "FREE",	UNI_FREE },
	{ "ACTIVE",	UNI_PVC_ACTIVE },
	{ "ACT_DOWN",	UNI_PVC_ACT_DOWN },
	{0,			0 }
};

/*
 * IP VCC state definitions
 */
struct state	ip_vcc_states[] = {
	{ "FREE",	IPVCC_FREE },
	{ "PMAP",	IPVCC_PMAP },
	{ "POPEN",	IPVCC_POPEN },
	{ "PACCEPT",	IPVCC_PACCEPT },
	{ "ACTPENT",	IPVCC_ACTPENT },
	{ "ACTIVE",	IPVCC_ACTIVE },
	{ "CLOSED",	IPVCC_CLOSED },
	{ 0,		0 }
};

/*
 * ARP server state definitions
 */
struct state	arpserver_states[] = {
	{ "NOT_CONF",	UIAS_NOTCONF },
	{ "SERVER",	UIAS_SERVER_ACTIVE },
	{ "PEND_ADR",	UIAS_CLIENT_PADDR },
	{ "POPEN",	UIAS_CLIENT_POPEN },
	{ "REGISTER",	UIAS_CLIENT_REGISTER },
	{ "ACTIVE",	UIAS_CLIENT_ACTIVE },
	{ 0,		0 }
};

/*
 * Supported signalling managers
 */
struct proto_state	proto_states[] = {
	{ "SIGPVC",  sigpvc_states, sigpvc_vcc_states, ATM_SIG_PVC },
	{ "SPANS",   spans_states,  spans_vcc_states,  ATM_SIG_SPANS },
	{ "UNI 3.0", unisig_states, unisig_vcc_states, ATM_SIG_UNI30 },
	{ "UNI 3.1", unisig_states, unisig_vcc_states, ATM_SIG_UNI31 },
	{ "UNI 4.0", unisig_states, unisig_vcc_states, ATM_SIG_UNI40 },
	{ 0,         0,             0,                 0 }
};

/*
 * ATMARP origin values
 */
struct state	arp_origins[] = {
	{ "LOCAL",	UAO_LOCAL },
	{ "PERM",	UAO_PERM },
	{ "REG",	UAO_REGISTER },
	{ "SCSP",	UAO_SCSP },
	{ "LOOKUP",	UAO_LOOKUP },
	{ "PEER_RSP",	UAO_PEER_RSP },
	{ "PEER_REQ",	UAO_PEER_REQ },
	{ 0,		0 }
};


/*
 * Print ARP table information
 * 
 * Arguments:
 *	ai	pointer to a struct air_arp_rsp
 *
 * Returns:
 *	none
 *
 */
void
print_arp_info(ai)
	struct air_arp_rsp	*ai;
{
	int	i;
	char	*atm_addr, *ip_addr, *origin;
	char	age[8], flags[32];
	struct sockaddr_in	*sin;

	/*
	 * Print a header if it hasn't been done yet.
	 */
	if (!arp_hdr) {
		printf(ARP_HDR);
		arp_hdr = 1;
	}

	/*
	 * Format the addresses
	 */
	atm_addr = format_atm_addr(&ai->aap_addr);
	sin = (struct sockaddr_in *)&ai->aap_arp_addr;
	ip_addr = format_ip_addr(&sin->sin_addr);

	/*
	 * Decode the flags
	 */
	UM_ZERO(flags, sizeof(flags));
	if (ai->aap_flags & ARPF_VALID) {
		strcat(flags, "V");
	}
	if (ai->aap_flags & ARPF_REFRESH) {
		strcat(flags, "R");
	}

	/*
	 * Format the origin
	 */
	for (i=0; arp_origins[i].s_name != NULL && 
			ai->aap_origin != arp_origins[i].s_id;
			i++);
	if (arp_origins[i].s_name) {
		origin = arp_origins[i].s_name;
	} else {
		origin = "-";
	}

	/*
	 * Format the age
	 */
	UM_ZERO(age, sizeof(age));
	if (!(ai->aap_flags & ARPF_VALID)) {
		strcpy(age, "-");
	} else {
		sprintf(age, "%d", ai->aap_age);
	}

	/*
	 * Print the ARP information
	 */
	printf("%-8s  %-5s  %3s  %s\n    ATM address = %s\n    IP address = %s\n",
			ai->aap_intf,
			flags,
			age,
			origin,
			atm_addr,
			ip_addr);
}


/*
 * Print ARP server information
 * 
 * Arguments:
 *	si	pointer to a struct air_asrv_rsp
 *
 * Returns:
 *	none
 *
 */
void
print_asrv_info(si)
	struct air_asrv_rsp	*si;
{
	int		i;
	char		*atm_addr, *state;
	struct in_addr	*addr;

	/*
	 * Print a header if it hasn't been done yet.
	 */
	if (!asrv_hdr) {
		printf(ASRV_HDR);
		asrv_hdr = 1;
	}

	/*
	 * Format the ATM address of the ARP server
	 */
	atm_addr = format_atm_addr(&si->asp_addr);

	/*
	 * Format the server state
	 */
	for (i=0; arpserver_states[i].s_name != NULL && 
			si->asp_state != arpserver_states[i].s_id;
			i++);
	if (arpserver_states[i].s_name) {
		state = arpserver_states[i].s_name;
	} else {
		state = "-";
	}

	/*
	 * Print the ARP server information
	 */
	printf("%-8s  %-8s  %s\n",
			si->asp_intf,
			state,
			atm_addr);

	/*
	 * Format and print the LIS prefixes
	 */
	if (si->asp_nprefix) {
		addr = (struct in_addr *)((u_long)si +
				sizeof(struct air_asrv_rsp));
		printf("    LIS = ");
		for (i = 0; i < si->asp_nprefix; i++) {
			printf("%s", inet_ntoa(*addr));
			addr++;
			printf("/0x%0lx", ntohl(addr->s_addr));
			addr++;
			if (i < si->asp_nprefix -1)
				printf(", ");
		}
		printf("\n");
	}
}


/*
 * Print adapter configuration information
 * 
 * Arguments:
 *	si	pointer to a struct air_cfg_rsp
 *
 * Returns:
 *	none
 *
 */
void
print_cfg_info(si)
	struct air_cfg_rsp	*si;
{
	char	*adapter, *bus, *media, *vendor;

	/*
	 * Print a header if it hasn't been done yet.
	 */
	if (!cfg_hdr) {
		printf(CFG_HDR);
		cfg_hdr = 1;
	}

	/*
	 * Format the vendor name and adapter type
	 */
	vendor = get_vendor(si->acp_vendor);
	adapter = get_adapter(si->acp_device);

	/*
	 * Format the communications medium
	 */
	media = get_media_type(si->acp_media);
	bus = get_bus_type(si->acp_bustype);

	/*
	 * Print the ARP server information
	 */
	printf("%-8s  %-8s  %-8s  %-14s  %-4s  %ld\n",
			si->acp_intf,
			vendor,
			adapter,
			media,
			bus,
			si->acp_serial);
	printf("    MAC address = %s\n",
			format_mac_addr(&si->acp_macaddr));
	printf("    Hardware version = %s\n", si->acp_hard_vers);
	printf("    Firmware version = %s\n", si->acp_firm_vers);
}


/*
 * Print interface information
 * 
 * Arguments:
 *	ni	pointer to a struct air_int_rsp
 *
 * Returns:
 *	none
 *
 */
void
print_intf_info(ni)
	struct air_int_rsp	*ni;
{
	int	i;
	char	nif_names[(IFNAMSIZ *2)+4];
	char	*atm_addr;
	char	*sigmgr = "-", *state_name = "-";
	struct state		*s_t;

	/*
	 * Print a header
	 */
	printf(INTF_HDR);

	/*
	 * Translate signalling manager name
	 */
	for (i=0; proto_states[i].p_state != NULL; i++)
		if (ni->anp_sig_proto == proto_states[i].p_id)
			break;
	if (proto_states[i].p_state != NULL)
		sigmgr = proto_states[i].p_name;

	/*
	 * Get the signalling manager state
	 */
	if (proto_states[i].p_state != NULL) {
		s_t = proto_states[i].p_state;
		for (i=0; s_t[i].s_name != NULL; i++)
			if (ni->anp_sig_state == s_t[i].s_id)
				break;
		if (s_t[i].s_name != NULL)
			state_name = s_t[i].s_name;
	}

	/*
	 * Format the ATM address
	 */
	atm_addr = format_atm_addr(&ni->anp_addr);

	/*
	 * Get the range of NIFs on the physical interface
	 */
	UM_ZERO(nif_names, sizeof(nif_names));
	if (strlen(ni->anp_nif_pref) == 0) {
		strcpy(nif_names, "-");
	} else {
		strcpy(nif_names, ni->anp_nif_pref);
		strcat(nif_names, "0");
		if (ni->anp_nif_cnt > 1) {
			strcat(nif_names, " - ");
			strcat(nif_names, ni->anp_nif_pref);
			sprintf(&nif_names[strlen(nif_names)], "%d",
					ni->anp_nif_cnt-1);
		}
	}
	

	/*
	 * Print the interface information
	 */
	printf("%-9s  %-7s  %s\n",
			ni->anp_intf,
			sigmgr,
			state_name);
	printf("    ATM address = %s\n", atm_addr);
	printf("    Network interfaces: %s\n", nif_names);
}


/*
 * Print IP address map information
 * 
 * Arguments:
 *	ai	pointer to a struct air_arp_rsp
 *
 * Returns:
 *	none
 *
 */
void
print_ip_vcc_info(ai)
	struct air_ip_vcc_rsp	*ai;
{
	int	i;
	char	*ip_addr, *state;
	char	flags[32], vpi_vci[16];
	struct sockaddr_in	*sin;

	/*
	 * Print a header if it hasn't been done yet.
	 */
	if (!ip_vcc_hdr) {
		printf(IP_VCC_HDR);
		ip_vcc_hdr = 1;
	}

	/*
	 * Format the IP address
	 */
	sin = (struct sockaddr_in *)&ai->aip_dst_addr;
	ip_addr = format_ip_addr(&sin->sin_addr);

	/*
	 * Format the VPI/VCI
	 */
	if (ai->aip_vpi == 0 && ai->aip_vci == 0) {
		strcpy(vpi_vci, "  -     -");
	} else {
		sprintf(vpi_vci, "%3d %5d", ai->aip_vpi, ai->aip_vci);
	}

	/*
	 * Decode VCC flags
	 */
	UM_ZERO(flags, sizeof(flags));
	if (ai->aip_flags & IVF_PVC) {
		strcat(flags, "P");
	}
	if (ai->aip_flags & IVF_SVC) {
		strcat(flags, "S");
	}
	if (ai->aip_flags & IVF_LLC) {
		strcat(flags, "L");
	}
	if (ai->aip_flags & IVF_MAPOK) {
		strcat(flags, "M");
	}
	if (ai->aip_flags & IVF_NOIDLE) {
		strcat(flags, "N");
	}

	/*
	 * Get the state of the VCC
	 */
	for (i=0; ip_vcc_states[i].s_name != NULL && 
			ai->aip_state != ip_vcc_states[i].s_id;
			i++);
	if (ip_vcc_states[i].s_name) {
		state = ip_vcc_states[i].s_name;
	} else {
		state = "-";
	}

	/*
	 * Print the IP VCC information
	 */
	printf("%-8s  %9s  %-7s %-5s %s\n",
			ai->aip_intf,
			vpi_vci,
			state,
			flags,
			ip_addr);
}


/*
 * Print network interface information
 * 
 * Arguments:
 *	ni	pointer to a struct air_int_rsp
 *
 * Returns:
 *	none
 *
 */
void
print_netif_info(ni)
	struct air_netif_rsp	*ni;
{
	char			*ip_addr;
	struct sockaddr_in	*sin;

	/*
	 * Print a header
	 */
	if (!netif_hdr) {
		netif_hdr++;
		printf(NETIF_HDR);
	}

	/*
	 * Format the protocol address
	 */
	sin = (struct sockaddr_in *)&ni->anp_proto_addr;
	ip_addr = format_ip_addr(&sin->sin_addr);

	/*
	 * Print the network interface information
	 */
	printf("%-8s  %-8s  %s\n",
			ni->anp_intf,
			ni->anp_phy_intf,
			ip_addr);
}


/*
 * Print physical interface statistics
 * 
 * Arguments:
 *	pi	pointer to a struct air_phy_stat_rsp
 *
 * Returns:
 *	none
 *
 */
void
print_intf_stats(pi)
	struct air_phy_stat_rsp	*pi;
{
	/*
	 * Print a header if it hasn't already been done
	 */
	if (!phy_stats_hdr) {
		printf(PHY_STATS_HDR);
		phy_stats_hdr = 1;
	}

	/*
	 * Print the interface statistics
	 */
	printf("%-9s  %7ld %8ld  %5ld %7ld %8ld  %5ld  %5ld\n",
			pi->app_intf,
			pi->app_ipdus,
			pi->app_ibytes,
			pi->app_ierrors,
			pi->app_opdus,
			pi->app_obytes,
			pi->app_oerrors,
			pi->app_cmderrors);
}


/*
 * Print VCC statistics
 * 
 * Arguments:
 *	vi	pointer to VCC statistics to print
 *
 * Returns:
 *	none
 *
 */
void
print_vcc_stats(vi)
	struct air_vcc_rsp	*vi;
{

	/*
	 * Print a header if it hasn't already been done
	 */
	if (!vcc_stats_hdr) {
		printf(VCC_STATS_HDR);
		vcc_stats_hdr = 1;
	}

	/*
	 * Print the VCC statistics
	 */
	printf("%-9s  %3d  %4d",
			vi->avp_intf,
			vi->avp_vpi,
			vi->avp_vci);
	if ( vi->avp_type & VCC_IN )
		printf ( "  %7ld %8ld  %5ld",
			vi->avp_ipdus,
			vi->avp_ibytes,
			vi->avp_ierrors);
	else
		printf ( "        -        -      -" );

	if ( vi->avp_type & VCC_OUT )
		printf ( " %7ld %8ld  %5ld\n",
			vi->avp_opdus,
			vi->avp_obytes,
			vi->avp_oerrors);
	else
		printf ( "       -        -      -\n" );
}


/*
 * Print VCC information
 * 
 * Arguments:
 *	vi	pointer to a struct air_vcc_rsp
 *
 * Returns:
 *	none
 *
 */
void
print_vcc_info(vi)
	struct air_vcc_rsp	*vi;
{
	int	i;
	char	*aal_name = "-" , *encaps_name = "-", *owner_name = "-";
	char	*state_name = "-", *type_name = "-";
	char	dir_name[10];
	struct state	*s_t;

	/*
	 * Print a header if it hasn't already been done
	 */
	if (!vcc_hdr) {
		printf(VCC_HDR);
		vcc_hdr = 1;
	}

	/*
	 * Translate AAL
	 */
	for (i=0; aals[i].a_name != NULL; i++)
		if (vi->avp_aal == aals[i].a_id)
			break;
	if (aals[i].a_name)
		aal_name = aals[i].a_name;

	/*
	 * Translate VCC type
	 */
	if (vi->avp_type & VCC_PVC)
		type_name = "PVC";
	else if (vi->avp_type & VCC_SVC)
		type_name = "SVC";
	/*
	 * Translate VCC direction
	 */
	UM_ZERO(dir_name, sizeof(dir_name));
	if (vi->avp_type & VCC_IN)
		strcat(dir_name, "In");
	if (vi->avp_type & VCC_OUT)
		strcat(dir_name, "Out");
	if (strlen(dir_name) == 0)
		strcpy(dir_name, "-");

	/*
	 * Translate state
	 */
	for (i=0; proto_states[i].p_state != NULL; i++)
		if (vi->avp_sig_proto == proto_states[i].p_id)
			break;
	if (proto_states[i].p_state) {
		s_t = proto_states[i].v_state;
		for (i=0; s_t[i].s_name != NULL; i++)
			if (vi->avp_state == s_t[i].s_id)
				break;
		if (s_t[i].s_name)
			state_name = s_t[i].s_name;
	}

	/*
	 * Translate encapsulation
	 */
	for (i=0; encaps[i].e_name != NULL; i++)
		if (vi->avp_encaps == encaps[i].e_id)
			break;
	if (encaps[i].e_name)
		encaps_name = encaps[i].e_name;

	/*
	 * Print the VCC information
	 */
	printf("%-9s  %3d %5d  %-4s  %-4s %-5s  %-8s %-8s ",
			vi->avp_intf,
			vi->avp_vpi,
			vi->avp_vci,
			aal_name,
			type_name,
			dir_name,
			state_name,
			encaps_name);

	/*
	 * Print VCC owners' names
	 */
	for (i = 0, owner_name = vi->avp_owners;
			i < O_CNT - 1 && strlen(owner_name);
			i++, owner_name += (T_ATM_APP_NAME_LEN + 1)) {
		if (i > 0)
			printf(", ");
		printf("%s", owner_name);
	}
	if (i == 0)
		printf("-");
	printf("\n");

	/*
	 * Print destination address if it's an SVC
	 */
	if (vi->avp_type & VCC_SVC) {
		printf("    Dest = %s\n",
				format_atm_addr(&vi->avp_daddr));
	}
}


/*
 * Print network interface information
 * 
 * Arguments:
 *	ni	pointer to a struct air_int_rsp
 *
 * Returns:
 *	none
 *
 */
void
print_version_info(vi)
	struct air_version_rsp	*vi;
{
	char			version_str[80];

	/*
	 * Print a header
	 */
	if (!version_hdr) {
		version_hdr++;
		printf(VERSION_HDR);
	}

	/*
	 * Print the interface information
	 */
	sprintf(version_str, "%d.%d",
			ATM_VERS_MAJ(vi->avp_version),
			ATM_VERS_MIN(vi->avp_version));
	printf("%7s\n", version_str);
}
