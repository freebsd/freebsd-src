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
 *	@(#) $FreeBSD: src/sys/netatm/atm_ioctl.h,v 1.3 1999/08/28 00:48:36 peter Exp $
 *
 */

/*
 * Core ATM Services
 * -----------------
 *
 * PF_ATM socket ioctl definitions
 *
 */

#ifndef _NETATM_ATM_IOCTL_H
#define _NETATM_ATM_IOCTL_H


/*
 * Structure for PF_ATM configure (AIOCCFG) socket ioctls
 */
struct atmcfgreq {
	int		acr_opcode;		/* Sub-operation */
	union {
		struct {
			/* Configure - attach */
			char	acru_att_intf[IFNAMSIZ];/* Interface name */
			u_char	acru_att_proto;	/* Signalling protocol */
		} acru_att;
		struct {
			/* Configure - detach */
			char	acru_det_intf[IFNAMSIZ];/* Interface name */
		} acru_det;
	} acr_u;
};
#define	acr_att_intf	acr_u.acru_att.acru_att_intf
#define	acr_att_proto	acr_u.acru_att.acru_att_proto
#define	acr_det_intf	acr_u.acru_det.acru_det_intf


/*
 * Structure for PF_ATM set (AIOCSET) socket ioctls
 */
struct atmsetreq {
	int		asr_opcode;		/* Sub-operation */
	union {
		/* ARP server */
		struct {
			char	asru_arp_intf[IFNAMSIZ];/* Interface name */
			Atm_addr	asru_arp_addr;	/* ARP srvr address */
			Atm_addr	asru_arp_subaddr;/* ARP srvr subaddr */
			caddr_t		asru_arp_pbuf;	/* Prefix buffer addr */
			int		asru_arp_plen;	/* Prefix buffer len */
		} asru_asrvr;
		/* MAC address */
		struct {
			char	asru_mac_intf[IFNAMSIZ];/* Interface name */
			struct mac_addr	asru_mac_addr;	/* MAC address */
		} asru_mac;
		/* Network interface */
		struct {
			char	asru_nif_intf[IFNAMSIZ];/* Interface name */
			char	asru_nif_pref[IFNAMSIZ];/* I/f prefix name */
			int	asru_nif_cnt;		/* Number of i/fs */
		} asru_nif;
		/* NSAP prefix */
		struct {
			char	asru_prf_intf[IFNAMSIZ];/* Interface name */
			u_char	asru_prf_pref[13];	/* NSAP prefix */
		} asru_prf;
	} asr_u;
};
#define	asr_arp_intf	asr_u.asru_asrvr.asru_arp_intf
#define	asr_arp_addr	asr_u.asru_asrvr.asru_arp_addr
#define	asr_arp_pbuf	asr_u.asru_asrvr.asru_arp_pbuf
#define	asr_arp_plen	asr_u.asru_asrvr.asru_arp_plen
#define	asr_arp_subaddr	asr_u.asru_asrvr.asru_arp_subaddr
#define	asr_mac_intf	asr_u.asru_mac.asru_mac_intf
#define	asr_mac_addr	asr_u.asru_mac.asru_mac_addr
#define	asr_nif_intf	asr_u.asru_nif.asru_nif_intf
#define	asr_nif_pref	asr_u.asru_nif.asru_nif_pref
#define	asr_nif_cnt	asr_u.asru_nif.asru_nif_cnt
#define	asr_prf_intf	asr_u.asru_prf.asru_prf_intf
#define	asr_prf_pref	asr_u.asru_prf.asru_prf_pref


/*
 * Structure for PF_ATM add (AIOCADD) socket ioctls
 */
struct atmaddreq {
	int		aar_opcode;		/* Sub-operation */
	union {
		/* Add PVC */
		struct {
			char	aaru_pvc_intf[IFNAMSIZ];/* Interface name */
			u_short	aaru_pvc_vpi;		/* VPI value */
			u_short	aaru_pvc_vci;		/* VCI value */
			struct sockaddr	aaru_pvc_dst;	/* Destination addr */
			Sap_t	aaru_pvc_sap;		/* Endpoint SAP */
			Aal_t	aaru_pvc_aal;		/* AAL */
			Encaps_t aaru_pvc_encaps;	/* Encapsulation */
			u_char	aaru_pvc_flags;		/* Flags (see below) */
		} aaru_add_pvc;

		/* Add ARP table entry */
		struct {
			char	aaru_arp_intf[IFNAMSIZ];/* Interface name */
			struct sockaddr	aaru_arp_dst;	/* IP addr */
			Atm_addr	aaru_arp_addr;	/* ATM addr */
			u_char	aaru_arp_origin;	/* Entry origin */
		} aaru_add_arp;
	} aar_u;
};
#define	aar_pvc_intf	aar_u.aaru_add_pvc.aaru_pvc_intf
#define	aar_pvc_vpi	aar_u.aaru_add_pvc.aaru_pvc_vpi
#define	aar_pvc_vci	aar_u.aaru_add_pvc.aaru_pvc_vci
#define	aar_pvc_dst	aar_u.aaru_add_pvc.aaru_pvc_dst
#define	aar_pvc_sap	aar_u.aaru_add_pvc.aaru_pvc_sap
#define	aar_pvc_aal	aar_u.aaru_add_pvc.aaru_pvc_aal
#define	aar_pvc_encaps	aar_u.aaru_add_pvc.aaru_pvc_encaps
#define	aar_pvc_flags	aar_u.aaru_add_pvc.aaru_pvc_flags
#define	aar_arp_intf	aar_u.aaru_add_arp.aaru_arp_intf
#define	aar_arp_dst	aar_u.aaru_add_arp.aaru_arp_dst
#define	aar_arp_addr	aar_u.aaru_add_arp.aaru_arp_addr
#define	aar_arp_origin	aar_u.aaru_add_arp.aaru_arp_origin

/* PVC flags */
#define	PVC_DYN		0x01			/* Dest addr is dynamic */


/*
 * Structure for PF_ATM delete (AIOCDEL) socket ioctls
 */
struct atmdelreq {
	int		adr_opcode;		/* Sub-operation */
	union {
		/* Delete PVC */
		struct {
			char	adru_pvc_intf[IFNAMSIZ];/* Interface name */
			u_short	adru_pvc_vpi;		/* VPI value */
			u_short	adru_pvc_vci;		/* VCI value */
		} adru_del_pvc;

		/* Delete SVC */
		struct {
			char	adru_svc_intf[IFNAMSIZ];/* Interface name */
			u_short	adru_svc_vpi;		/* VPI value */
			u_short	adru_svc_vci;		/* VCI value */
		} adru_del_svc;

		/* Delete ARP table entry */
		struct {
			char	adru_arp_intf[IFNAMSIZ];/* Interface name */
			struct sockaddr	adru_arp_dst;	/* IP addr */
		} adru_del_arp;
	} adr_u;
};
#define	adr_pvc_intf	adr_u.adru_del_pvc.adru_pvc_intf
#define	adr_pvc_vpi	adr_u.adru_del_pvc.adru_pvc_vpi
#define	adr_pvc_vci	adr_u.adru_del_pvc.adru_pvc_vci
#define	adr_svc_intf	adr_u.adru_del_svc.adru_svc_intf
#define	adr_svc_vpi	adr_u.adru_del_svc.adru_svc_vpi
#define	adr_svc_vci	adr_u.adru_del_svc.adru_svc_vci
#define	adr_arp_intf	adr_u.adru_del_arp.adru_arp_intf
#define	adr_arp_dst	adr_u.adru_del_arp.adru_arp_dst


/*
 * Structure for PF_ATM information (AIOCINFO) socket ioctls
 */
struct atminfreq {
	int		air_opcode;		/* Sub-operation */
	caddr_t		air_buf_addr;		/* Buffer for returned info */
	int		air_buf_len;		/* Buffer length */
	union {
		/* Vendor info */
		char		airu_vinfo_intf[IFNAMSIZ];/* Interface name */
		/* IP VCC */
		struct sockaddr	airu_ip_addr;		/* Destination host */
		/* ARP table */
		struct {
			struct sockaddr	airu_arp_addr;	/* Destination host */
			u_char		airu_arp_flags;	/* Flags (see below) */
		} airu_arp;
		/* ARP server */
		char		airu_asrv_intf[IFNAMSIZ];/* Interface name */
		/* Interface */
		char		airu_int_intf[IFNAMSIZ];/* Interface name */
		/* VCC */
		char		airu_vcc_intf[IFNAMSIZ];/* Interface name */
		/* Configuration */
		char		airu_cfg_intf[IFNAMSIZ];/* Interface name */
		/* Network interface */
		char		airu_netif_intf[IFNAMSIZ];/* Interface name */
		/* Physical interface statistics */
		char		airu_physt_intf[IFNAMSIZ];/* Interface name */
	} air_u;
};
#define	air_vinfo_intf		air_u.airu_vinfo_intf
#define	air_ip_addr		air_u.airu_ip_addr
#define	air_arp_addr		air_u.airu_arp.airu_arp_addr
#define	air_arp_flags		air_u.airu_arp.airu_arp_flags
#define	air_asrv_intf		air_u.airu_asrv_intf
#define	air_int_intf		air_u.airu_int_intf
#define	air_vcc_intf		air_u.airu_vcc_intf
#define	air_cfg_intf		air_u.airu_cfg_intf
#define	air_netif_intf		air_u.airu_netif_intf
#define	air_physt_intf		air_u.airu_physt_intf

/* ARP table info flags */
#define	ARP_RESET_REF	0x01			/* Reset refresh status */


/*
 * Structures returned by information requests
 */

/*
 * Vendor-specific interface information
 */
struct air_vinfo_rsp {
	char		avsp_intf[IFNAMSIZ];	/* Interface name */
	int		avsp_len;		/* Length of returned
							Vendor Info block */
						/* Vendor info ... */
};


/*
 * ARP table information
 */
struct air_arp_rsp {
	struct sockaddr	aap_arp_addr;		/* Destination host */
	char		aap_intf[IFNAMSIZ];	/* Interface name */
	u_char		aap_flags;		/* Flags (see below) */
	u_char		aap_origin;		/* Entry origin (see below) */
	u_char		aap_age;		/* Aging timeout (minutes) */
	Atm_addr	aap_addr;		/* ATM address */
	Atm_addr	aap_subaddr;		/* ATM subaddress */
};

/*
 * ARP entry flags
 */
#define	ARPF_VALID	0x01			/* Entry is valid */
#define	ARPF_REFRESH	0x02			/* Entry has been refreshed */

/*
 * ARP entry origin
 */
#define	ARP_ORIG_PERM	50			/* Permanent entry */

/*
 * IP VCC information
 */
struct air_ip_vcc_rsp {
	struct sockaddr	aip_dst_addr;		/* Destination host */
	char		aip_intf[IFNAMSIZ];	/* Interface name */
	u_short		aip_vpi;		/* VPI value */
	u_short		aip_vci;		/* VCI value */
	u_char		aip_sig_proto;		/* Signalling protocol */
	u_char		aip_flags;		/* Flags (IVF_*) */
	u_char		aip_state;		/* IP VCC state */
};

/*
 * ARP server information
 */
struct air_asrv_rsp {
	char		asp_intf[IFNAMSIZ];	/* Interface name */
	Atm_addr	asp_addr;		/* Server ATM address */
	Atm_addr	asp_subaddr;		/* Server ATM subaddress */
	int		asp_state;		/* Server state */
	int		asp_nprefix;		/* Number of prefix entries */
};

/*
 * Interface information
 */
struct air_int_rsp {
	char		anp_intf[IFNAMSIZ];	/* Interface name */
	Atm_addr	anp_addr;		/* ATM address */
	Atm_addr	anp_subaddr;		/* ATM subaddress */
	u_char		anp_sig_proto;		/* Signalling protocol */
	u_char		anp_sig_state;		/* Signalling protocol state */
	char		anp_nif_pref[IFNAMSIZ]; /* Netif prefix */
	int		anp_nif_cnt;		/* No. of netifs */
};

/*
 * Network interface information
 */
struct air_netif_rsp {
	char		anp_intf[IFNAMSIZ];	/* Interface name */
	struct sockaddr	anp_proto_addr;		/* Protocol address */
	char		anp_phy_intf[IFNAMSIZ];	/* Interface name */
};

/*
 * VCC information
 */
#define	O_CNT		8
struct air_vcc_rsp {
	char		avp_intf[IFNAMSIZ];	/* Interface name */
	u_short		avp_vpi;		/* VPI value */
	u_short		avp_vci;		/* VCI value */
	u_char		avp_type;		/* Type (SVC or PVC) */
	u_char		avp_aal;		/* AAL */
	u_char		avp_sig_proto;		/* Signalling protocol */
	Encaps_t	avp_encaps;		/* Encapsulation */
	u_char		avp_state;		/* State (sig mgr specific) */
	char		avp_owners[(T_ATM_APP_NAME_LEN+1)*O_CNT];/* VCC users */
	Atm_addr	avp_daddr;		/* Address of far end */
	Atm_addr	avp_dsubaddr;		/* Subaddress of far end */
	long		avp_ipdus;		/* PDUs received from VCC */
	long		avp_opdus;		/* PDUs sent to VCC */
	long		avp_ibytes;		/* Bytes received from VCC */
	long		avp_obytes;		/* Bytes sent to VCC */
	long		avp_ierrors;		/* Errors receiving from VCC */
	long		avp_oerrors;		/* Errors sending to VCC */
	time_t		avp_tstamp;		/* State transition timestamp */
};

/*
 * Adapter configuration information
 */
struct air_cfg_rsp {
	char		acp_intf[IFNAMSIZ];	/* Interface name */
	Atm_config	acp_cfg;		/* Config info */
};
#define	acp_vendor	acp_cfg.ac_vendor
#define	acp_vendapi	acp_cfg.ac_vendapi
#define	acp_device	acp_cfg.ac_device
#define	acp_media	acp_cfg.ac_media
#define	acp_serial	acp_cfg.ac_serial
#define	acp_bustype	acp_cfg.ac_bustype
#define	acp_busslot	acp_cfg.ac_busslot
#define	acp_ram		acp_cfg.ac_ram
#define	acp_ramsize	acp_cfg.ac_ramsize
#define	acp_macaddr	acp_cfg.ac_macaddr
#define	acp_hard_vers	acp_cfg.ac_hard_vers
#define	acp_firm_vers	acp_cfg.ac_firm_vers

/*
 * Version information
 */
struct air_version_rsp {
	int		avp_version;		/* Software version */
};

/*
 * Physical interface statistics
 */
struct air_phy_stat_rsp {
	char		app_intf[IFNAMSIZ];	/* Interface name */
	long		app_ipdus;		/* PDUs received from I/F */
	long		app_opdus;		/* PDUs sent to I/F */
	long		app_ibytes;		/* Bytes received from I/F */
	long		app_obytes;		/* Bytes sent to I/F */
	long		app_ierrors;		/* Errors receiving from I/F */
	long		app_oerrors;		/* Errors sending to I/F */
	long		app_cmderrors;		/* I/F command errors */
};


/*
 * PF_ATM sub-operation codes
 */
#define	AIOCS_CFG_ATT	1
#define	AIOCS_CFG_DET	2
#define	AIOCS_ADD_PVC	32
#define	AIOCS_ADD_ARP	33
#define	AIOCS_DEL_PVC	64
#define	AIOCS_DEL_SVC	65
#define	AIOCS_DEL_ARP	66
#define	AIOCS_SET_ASV	96
#define	AIOCS_SET_NIF	97
#define	AIOCS_SET_PRF 	98
#define	AIOCS_SET_MAC 	99
#define	AIOCS_INF_VST	160
#define	AIOCS_INF_IPM	161
#define	AIOCS_INF_ARP	162
#define	AIOCS_INF_ASV	163
#define	AIOCS_INF_INT	164
#define	AIOCS_INF_VCC	165
#define	AIOCS_INF_CFG	166
#define	AIOCS_INF_NIF	167
#define	AIOCS_INF_PIS	168
#define	AIOCS_INF_VER	169


/*
 * PF_ATM ioctls
 */
#if defined(sun) && !defined(__GNUC__)
#define	AIOCCFG		_IOW(A, 128, struct atmcfgreq)	/* Configure i/f */
#define	AIOCADD		_IOW(A, 129, struct atmaddreq)	/* Add (e.g. PVC) */
#define	AIOCDEL		_IOW(A, 130, struct atmdelreq)	/* Delete */
#define	AIOCSET		_IOW(A, 132, struct atmsetreq)	/* Set (e.g. net i/f) */
#define	AIOCINFO	_IOWR(A, 133, struct atminfreq)	/* Show kernel info */
#else
#define	AIOCCFG		_IOW('A', 128, struct atmcfgreq)/* Configure i/f */
#define	AIOCADD		_IOW('A', 129, struct atmaddreq)/* Add (e.g. PVC) */
#define	AIOCDEL		_IOW('A', 130, struct atmdelreq)/* Delete */
#define	AIOCSET		_IOW('A', 132, struct atmsetreq)/* Set (e.g. net i/f) */
#define	AIOCINFO	_IOWR('A', 133, struct atminfreq)/* Show kernel info */
#endif

#endif	/* _NETATM_ATM_IOCTL_H */
