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
 *	@(#) $FreeBSD$
 *
 */

/*
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * Interface to server clients of SCSP
 *
 */

#ifndef _SCSP_SCSP_IF_H
#define _SCSP_SCSP_IF_H


/*
 * SCSP configuration message
 */
struct scsp_cfg_msg {
	char	atmarp_netif[IFNAMSIZ];
};
typedef	struct scsp_cfg_msg	Scsp_cfg_msg;


/*
 * SCSP cache summary
 */
struct scsp_sum_msg {
	u_short		ss_hops;	/* Hop count */
	u_char		ss_null;	/* Null flag */
	long		ss_seq;		/* CSA seq. no. */
	Scsp_ckey	ss_key;		/* Cache key */
	Scsp_id		ss_oid;		/* Originator ID */
};
typedef	struct scsp_sum_msg	Scsp_sum_msg;


/*
 * SCSP constants for ATMARP
 */
#define	SCSP_ATMARP_PROTO	1
#define	SCSP_ATMARP_SIDL	4
#define	SCSP_ATMARP_RIDL	4
#define	SCSP_ATMARP_CKL		4
#define	SCSP_ATMARP_OIDL	4


/*
 * SCSP ATMARP message
 */
struct scsp_atmarp_msg {
	u_char		sa_state;	/* Cache entry state (below) */
	struct in_addr	sa_cpa;		/* Cached protocol address */
	Atm_addr	sa_cha;		/* Cached ATM address */
	Atm_addr	sa_csa;		/* Cached ATM subaddress */
	Scsp_ckey	sa_key;		/* Cache key for entry */
	Scsp_id		sa_oid;		/* Originator ID */
	long		sa_seq;		/* Sequence no. */
};
typedef	struct scsp_atmarp_msg	Scsp_atmarp_msg;

#define	SCSP_ASTATE_NEW	0	/* ATMARP new server registration */
#define	SCSP_ASTATE_UPD	1	/* ATMARP server refreshed */
#define	SCSP_ASTATE_DEL	2	/* ATMARP server data deleted */


/*
 * SCSP constants for NHRP
 */
#define	SCSP_NHRP_PROTO		2
#define	SCSP_NHRP_SIDL		4
#define	SCSP_NHRP_RIDL		4
#define	SCSP_NHRP_CKL		4
#define	SCSP_NHRP_OIDL		4


/*
 * SCSP NHRP message
 */
struct scsp_nhrp_msg {
	u_short	sn_af;		/* Address family */
	u_short	sn_proto;	/* NHRP protocol type */
	u_char	sn_snap[5];	/* SNAP */
	u_char	sn_ver;		/* NHRP version number */
	u_short	sn_flags;	/* Flags */
	u_long	sn_rid;		/* Request ID */
	u_char	sn_state;	/* State */
	u_char	sn_prel;	/* Prefix length */
	u_short	sn_mtu;		/* Maximum transmission unit */
	u_short sn_hold;	/* Holding time */
	Atm_addr	sn_addr;	/* Server network address */
	Atm_addr	sn_saddr;	/* Server network subaddress */
	struct in_addr	sn_paddr;	/* Server protocol address */
	Scsp_ckey sn_key;	/* Cache key for entry */
	Scsp_id	sn_oid;		/* Originator ID */
};
typedef	struct scsp_nhrp_msg	Scsp_nhrp_msg;

#define	SCSP_NSTATE_NEW	0	/* New NHRP server */
#define	SCSP_NSTATE_UPD	1	/* NHRP server re-registered */
#define	SCSP_NSTATE_DEL	2	/* NHRP server data purged */
#define	SCSP_NSTATE_NSD	3	/* NHRP no such data in server */


/*
 * SCSP/server message header
 */
struct scsp_if_msg_hdr {
	u_char	sh_type;	/* Message type */
	u_char	sh_rc;		/* Response code */
	u_short	sh_proto;	/* SCSP protocol ID */
	int	sh_len;		/* Length of message */
	u_long	sh_tok;		/* Token from SCSP daemon */
};
typedef	struct scsp_if_msg_hdr	Scsp_if_msg_hdr;


/*
 * SCSP-server message
 */
struct scsp_if_msg {
	Scsp_if_msg_hdr	si_hdr;	/* Header fields */
	union {
		Scsp_cfg_msg	siu_cfg;	/* Config data */
		Scsp_sum_msg	siu_sum;	/* Cache summary */
		Scsp_atmarp_msg	siu_atmarp;	/* ATMARP update */
		Scsp_nhrp_msg	siu_nhrp;	/* NHRP update */
	} si_u;
};
typedef	struct scsp_if_msg	Scsp_if_msg;

#define	si_type		si_hdr.sh_type
#define	si_rc		si_hdr.sh_rc
#define	si_proto	si_hdr.sh_proto
#define	si_len		si_hdr.sh_len
#define	si_tok		si_hdr.sh_tok

#define	si_cfg		si_u.siu_cfg
#define	si_sum		si_u.siu_sum
#define	si_atmarp	si_u.siu_atmarp
#define	si_nhrp		si_u.siu_nhrp


/*
 * Message types
 */
#define	SCSP_NOP_REQ		1
#define	SCSP_CFG_REQ		2
#define	SCSP_CFG_RSP		3
#define	SCSP_CACHE_IND		4
#define	SCSP_CACHE_RSP		5
#define	SCSP_SOLICIT_IND	6
#define	SCSP_SOLICIT_RSP	7
#define	SCSP_UPDATE_IND		8
#define	SCSP_UPDATE_REQ		9
#define	SCSP_UPDATE_RSP		10


/*
 * Response codes
 */
#define	SCSP_RSP_OK		0
#define	SCSP_RSP_ERR		1
#define	SCSP_RSP_REJ		2
#define	SCSP_RSP_NOT_FOUND	3


#endif	/* _SCSP_SCSP_IF_H */
