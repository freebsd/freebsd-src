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
 * SCSP message formats
 *
 */

#ifndef _SCSP_SCSP_MSG_H
#define _SCSP_SCSP_MSG_H


/*
 * ATMARP constants
 */
#define	ARP_ATMFORUM	19
#define	ARP_TL_TMASK	0x40		/* Type mask */
#define	ARP_TL_NSAPA	0x00		/* Type = ATM Forum NSAPA */
#define	ARP_TL_E164	0x40		/* Type = E.164 */
#define	ARP_TL_LMASK	0x3f		/* Length mask */


/*
 * SCSP version number
 */
#define	SCSP_VER_1	1


/*
 * SCSP message types
 */
#define	SCSP_CA_MSG		1
#define	SCSP_CSU_REQ_MSG	2
#define	SCSP_CSU_REPLY_MSG	3
#define	SCSP_CSUS_MSG		4
#define	SCSP_HELLO_MSG		5


/*
 * SCSP Client Protocol IDs
 */
#define	SCSP_PROTO_ATMARP	1
#define	SCSP_PROTO_NHRP		2
#define	SCSP_PROTO_MARS		3
#define	SCSP_PROTO_DHCP		4
#define	SCSP_PROTO_LNNI		5


/*
 * Extension types
 */
#define	SCSP_EXT_END	0
#define	SCSP_EXT_AUTH	1
#define	SCSP_EXT_VENDOR	2

/*
 * Sequence number bounds
 */
#define	SCSP_CSA_SEQ_MIN	0x80000001
#define SCSP_CSA_SEQ_MAX	0x7FFFFFFF


/*
 * Sender, Receiver, or Originator ID lengths
 */
#define	SCSP_ATMARP_ID_LEN	4
#define	SCSP_NHRP_ID_LEN	4
#define	SCSP_MAX_ID_LEN		4


/*
 * Cache Key lengths
 */
#define	SCSP_ATMARP_KEY_LEN	4
#define	SCSP_NHRP_KEY_LEN	4
#define	SCSP_MAX_KEY_LEN	4


/*
 * Fixed header
 */
struct scsp_nhdr {
	u_char	sh_ver;			/* SCSP version */
	u_char	sh_type;		/* Message type */
	u_short	sh_len;			/* Message length */
	u_short	sh_checksum;		/* IP checksum over message */
	u_short	sh_ext_off;		/* Offset of first extension */
};


/*
 * Mandatory common part
 */
struct scsp_nmcp {
	u_short	sm_pid;			/* Protocol ID */
	u_short	sm_sgid;		/* Server group ID */
	u_short	sm_fill_0;		/* Unused */
	u_short	sm_flags;		/* Flags--see below */
	u_char	sm_sid_len;		/* Sender ID length */
	u_char	sm_rid_len;		/* Receiver ID length */
	u_short	sm_rec_cnt;		/* Number of records */
#ifdef	NOTDEF
	/* Variable length fields */
	u_char	sm_sid[];		/* Sender ID (variable) */
	u_char	sm_rid[];		/* Receiver ID (variable) */
#endif
};


/*
 * Extensions part
 */
struct scsp_next {
	u_short	se_type;		/* Extension type */
	u_short	se_len;			/* Length */
#ifdef	NOTDEF
	/* Variable length fields */
	u_char	se_value[];		/* Extension value */
#endif
};


/*
 * Cache State Advertisement record or
 *    Cache State Advertisement Summary record
 */
struct scsp_ncsa {
	u_short	scs_hop_cnt;		/* Hop count */
	u_short	scs_len;		/* Record length */
	u_char	scs_ck_len;		/* Cache key length */
	u_char	scs_oid_len;		/* Originator ID length */
	u_short	scs_nfill;		/* Null bit and filler */
	long	scs_seq;		/* Sequence number */
#ifdef NOTDEF
	/* Variable length fields */
	u_char	scs_ckey[];		/* Cache key */
	u_char	scs_oid[];		/* Originator ID */
	u_char	scs_proto[];		/* Protocol-specific (in CSA) */
#endif
};

#define	SCSP_CSAS_NULL	0x8000


/*
 * Cache Alignment message
 */
struct scsp_nca {
	long			sca_seq;	/* Sequence number */
	struct scsp_nmcp	sca_mcp;	/* Mandatory common */
#ifdef NOTDEF
	/* Variable length fields */
	struct scsp_ncsa	sca_rec[];	/* CSASs */
#endif
};

#define	SCSP_CA_M	0x8000		/* Master/Slave bit */
#define	SCSP_CA_I	0x4000		/* Initialization bit */
#define	SCSP_CA_O	0x2000		/* More bit */


/*
 * Cache State Update Request, Cache State Update Reply, or
 * Cache State Update Solicit message
 */
struct scsp_ncsu_msg {
	struct scsp_nmcp	scr_mcp;	/* Mandatory common */
#ifdef NOTDEF
	/* Variable length fields */
	struct scsp_ncsa	scr_rec[];	/* CSAs */
#endif
};


/*
 * Hello message
 */
struct scsp_nhello {
	u_short			sch_hi;		/* Hello interval */
	u_short			sch_df;		/* Dead factor */
	u_short			sch_fill_0;	/* Unused */
	u_short			sch_fid;	/* Family ID */
	struct scsp_nmcp	sch_mcp;	/* Mandatory common */
#ifdef NOTDEF
	/* Variable-length fields */
	struct scsp_nrid	sch_rid[];	/* Receiver IDs */
#endif
};


/*
 * ATMARP-specific Cache State Advertisement record
 */
struct scsp_atmarp_ncsa {
	u_short	sa_hrd;			/* Hardware type -- 0x0013 */
	u_short	sa_pro;			/* Protocol type -- 0x0800 */
	u_char	sa_shtl;		/* Src ATM addr type/len */
	u_char	sa_sstl;		/* Src ATM subaddr type/len */
	u_char	sa_state;		/* State */
	u_char	sa_fill1;		/* Unused */
	u_char	sa_spln;		/* Src proto addr type */
	u_char	sa_thtl;		/* Tgt ATM addr type/len */
	u_char	sa_tstl;		/* Tgt ATM subaddr type/len */
	u_char	sa_tpln;		/* Tgt proto addr len */
#ifdef NOTDEF
	/* Variable-length fields */
	u_char	sa_sha[];		/* Source ATM addr */
	u_char	sa_ssa[];		/* Source ATM subaddr */
	u_char	sa_spa[];		/* Source IP addr */
	u_char	sa_tha[];		/* Target ATM addr */
	u_char	sa_tsa[];		/* Target ATM subaddr */
	u_char	sa_tpa[];		/* Target IP addr */
#endif
};


/*
 * NHRP-specific Cache State Advertisement record
 */
struct scsp_nhrp_ncsa {
	u_short	sn_af;			/* Address family */
	u_short	sn_pro;			/* NHRP protocol type */
	u_char	sn_snap[5];		/* SNAP header */
	u_char	sn_ver;			/* NHRP version no. */
	u_short	sn_flags;		/* Flags */
	u_long	sn_rid;			/* Request ID */
	u_char	sn_state;		/* State */
	u_char	sn_pln;			/* Prefix length */
	u_short	sn_fill1;		/* Unused */
	u_short	sn_mtu;			/* MTU */
	u_short	sn_hold;		/* Holding time */
	u_char	sn_csatl;		/* Client addr type/len */
	u_char	sn_csstl;		/* Client subaddr type/len */
	u_char	sn_cpln;		/* Client proto addr len */
	u_char	sn_pref;		/* Preference for next hop */
#ifdef NOTDEF
	/* Variable-length fields */
	u_char	sn_csa[];		/* Client subnetwork addr */
	u_char	sn_css[];		/* Client subnetwork subaddr */
	u_char	sn_cpa[];		/* Client protocol addr */
#endif
};


/*
 * SCSP messages in internal format
 *
 *
 * Fixed message header
 */
struct scsp_hdr {
	u_char	msg_type;		/* Message type */
};
typedef	struct scsp_hdr Scsp_hdr;


/*
 * Sender or Receiver ID structure
 */
struct scsp_id {
	struct scsp_id	*next;			/* Next ID */
	u_char		id_len;			/* ID length */
	u_char		id[SCSP_MAX_ID_LEN];	/* ID */
};
typedef	struct scsp_id	Scsp_id;


/*
 * Cacke Key structure
 */
struct scsp_ckey {
	u_char		key_len;		/* Cache key length */
	u_char		key[SCSP_MAX_KEY_LEN];	/* Cache key */
};
typedef	struct scsp_ckey	Scsp_ckey;


/*
 * Mandatory  common part
 */
struct scsp_mcp {
	u_short		pid;		/* Protocol ID */
	u_short		sgid;		/* Server group ID */
	u_short		flags;		/* Flags */
	u_short		rec_cnt;	/* No. of records attached */
	Scsp_id		sid;		/* Sender ID */
	Scsp_id		rid;		/* Receiver ID */
};
typedef	struct scsp_mcp	Scsp_mcp;


/*
 * Extensions part
 */
struct scsp_ext {
	struct scsp_ext	*next;		/* Next extension */
	u_short		type;		/* Extension type */
	u_short		len;		/* Length */
#ifdef	NOTDEF
	/* Variable length fields */
	u_char		value[];	/* Extension value */
#endif
};
typedef	struct scsp_ext	Scsp_ext;


/*
 * Cache State Advertisement record or
 *    Cache State Advertisement Summary record
 */
struct scsp_csa {
	struct scsp_csa	*next;		/* Next CSAS record */
	u_short		hops;		/* Hop count */
	u_char		null;		/* Null flag */
	u_long		seq;		/* CSA seq. no. */
	Scsp_ckey	key;		/* Cache key */
	Scsp_id		oid;		/* Originator ID */
	int		trans_ct;	/* No. of times CSA sent */
	struct scsp_atmarp_csa	*atmarp_data;	/* ATMARP data */
#ifdef NOTDEF
	struct scsp_nhrp_csa	*nhrp_data;	/* NHRP data */
#endif
};
typedef	struct scsp_csa	Scsp_csa;

/*
 * Macro to free a CSA and any associated protocol-specific data
 */
#define	SCSP_FREE_CSA(c)					\
{								\
	if ((c)->atmarp_data) {					\
		UM_FREE((c)->atmarp_data);			\
	}							\
	UM_FREE((c));						\
}


/*
 * Cache Alignment message
 */
struct scsp_ca {
	long		ca_seq;		/* CA msg sequence no. */
	u_char		ca_m;		/* Master/slave bit */
	u_char		ca_i;		/* Initialization bit */
	u_char		ca_o;		/* More bit */
	Scsp_mcp	ca_mcp;		/* Mandatory common part */
	Scsp_csa	*ca_csa_rec;	/* Ptr. to CSAS records */
};
typedef	struct scsp_ca	Scsp_ca;


/*
 * Cache State Update Request, Cache State Update Reply, or
 * Cache State Update Solicit message
 */
struct scsp_csu_msg {
	Scsp_mcp	csu_mcp;	/* Mandatory common part */
	Scsp_csa	*csu_csa_rec;	/* Ptr. to CSA records */
};
typedef	struct scsp_csu_msg	Scsp_csu_msg;


/*
 * Hello message
 */
struct scsp_hello {
	u_short		hello_int;	/* Hello interval */
	u_short		dead_factor;	/* When is DCS dead? */
	u_short		family_id;	/* Family ID */
	Scsp_mcp	hello_mcp;	/* Mandatory common part */
};
typedef struct scsp_hello	Scsp_hello;


/*
 * NHRP-specific Cache State Advertisement record
 */
struct scsp_nhrp_csa {
	u_char	req_id;			/* Request ID */
	u_char	state;			/* State */
	u_char	pref_len;		/* Prefix length */
	u_short	flags;			/* See below */
	u_short	mtu;			/* Maximim transmission unit */
	u_short	hold_time;		/* Entry holding time */
	u_char	caddr_tlen;		/* Client addr type/length */
	u_char	csaddr_tlen;		/* Client subaddr type/length */
	u_char	cproto_len;		/* Client proto addr length */
	u_char	pref;			/* Preference */
	Atm_addr	caddr;		/* Client address */
	Atm_addr	csaddr;		/* Client subaddress */
	struct in_addr	cproto_addr;	/* Client protocol address */
};
typedef	struct scsp_nhrp	Scsp_nhrp;

#define	SCSP_NHRP_UNIQ	0x8000
#define	SCSP_NHRP_ARP	0x4000


/*
 * ATMARP-specific Cache State Advertisement record
 */
struct scsp_atmarp_csa {
	u_char		sa_state;	/* State */
	Atm_addr	sa_sha;		/* Source ATM addr */
	Atm_addr	sa_ssa;		/* Source ATM subaddr */
	struct in_addr	sa_spa;		/* Source IP addr */
	Atm_addr	sa_tha;		/* Target ATM addr */
	Atm_addr	sa_tsa;		/* Target ATM subaddr */
	struct in_addr	sa_tpa;		/* Target IP addr */
};
typedef	struct scsp_atmarp_csa	Scsp_atmarp_csa;


/*
 * SCSP message
 */
struct scsp_msg {
	Scsp_hdr	sc_hdr;
	union {
		Scsp_ca		*sc_u_ca;
		Scsp_csu_msg	*sc_u_csu_msg;
		Scsp_hello	*sc_u_hello;
	} sc_msg_u;
	Scsp_ext	*sc_ext;
};
typedef	struct scsp_msg	Scsp_msg;

#define	sc_msg_type	sc_hdr.msg_type
#define	sc_ca		sc_msg_u.sc_u_ca
#define	sc_csu_msg	sc_msg_u.sc_u_csu_msg
#define	sc_hello	sc_msg_u.sc_u_hello

#endif	/* _SCSP_SCSP_MSG_H */
