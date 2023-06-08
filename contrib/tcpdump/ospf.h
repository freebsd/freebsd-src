/*
 * Copyright (c) 1991, 1993, 1994, 1995, 1996, 1997
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
 * OSPF support contributed by Jeffrey Honig (jch@mitchell.cit.cornell.edu)
 */
#define	OSPF_TYPE_HELLO         1	/* Hello */
#define	OSPF_TYPE_DD            2	/* Database Description */
#define	OSPF_TYPE_LS_REQ        3	/* Link State Request */
#define	OSPF_TYPE_LS_UPDATE     4	/* Link State Update */
#define	OSPF_TYPE_LS_ACK        5	/* Link State Ack */

/* Options field
 *
 * +------------------------------------+
 * | DN | O | DC | L | N/P | MC | E | T |
 * +------------------------------------+
 *
 */

#define	OSPF_OPTION_MT	0x01	/* MT bit: multi-topology */
#define	OSPF_OPTION_E	0x02	/* E bit: External routes advertised */
#define	OSPF_OPTION_MC	0x04	/* MC bit: Multicast capable */
#define	OSPF_OPTION_NP	0x08	/* N/P bit: NSSA capable */
#define	OSPF_OPTION_L	0x10	/* L bit: Packet contains LLS data block */
#define	OSPF_OPTION_DC	0x20	/* DC bit: Demand circuit capable */
#define	OSPF_OPTION_O	0x40	/* O bit: Opaque LSA capable */
#define	OSPF_OPTION_DN	0x80	/* DN bit: Up/Down Bit capable - draft-ietf-ospf-2547-dnbit-04 */

/* ospf_authtype	*/
#define	OSPF_AUTH_NONE		0	/* No auth-data */
#define	OSPF_AUTH_SIMPLE	1	/* Simple password */
#define OSPF_AUTH_SIMPLE_LEN	8	/* max length of simple authentication */
#define OSPF_AUTH_MD5		2	/* MD5 authentication */
#define OSPF_AUTH_MD5_LEN	16	/* length of MD5 authentication */

/* db_flags	*/
#define	OSPF_DB_INIT		0x04
#define	OSPF_DB_MORE		0x02
#define	OSPF_DB_MASTER          0x01
#define OSPF_DB_RESYNC          0x08  /* RFC4811 */

/* ls_type	*/
#define	LS_TYPE_ROUTER		1   /* router link */
#define	LS_TYPE_NETWORK		2   /* network link */
#define	LS_TYPE_SUM_IP		3   /* summary link */
#define	LS_TYPE_SUM_ABR		4   /* summary area link */
#define	LS_TYPE_ASE		5   /* ASE  */
#define	LS_TYPE_GROUP		6   /* Group membership (multicast */
				    /* extensions 23 July 1991) */
#define	LS_TYPE_NSSA            7   /* rfc3101 - Not so Stubby Areas */
#define	LS_TYPE_OPAQUE_LL       9   /* rfc2370 - Opaque Link Local */
#define	LS_TYPE_OPAQUE_AL      10   /* rfc2370 - Opaque Link Local */
#define	LS_TYPE_OPAQUE_DW      11   /* rfc2370 - Opaque Domain Wide */

#define LS_OPAQUE_TYPE_TE       1   /* rfc3630 */
#define LS_OPAQUE_TYPE_GRACE    3   /* rfc3623 */
#define LS_OPAQUE_TYPE_RI       4   /* draft-ietf-ospf-cap-03 */

#define LS_OPAQUE_TE_TLV_ROUTER 1   /* rfc3630 */
#define LS_OPAQUE_TE_TLV_LINK   2   /* rfc3630 */

#define LS_OPAQUE_TE_LINK_SUBTLV_LINK_TYPE             1 /* rfc3630 */
#define LS_OPAQUE_TE_LINK_SUBTLV_LINK_ID               2 /* rfc3630 */
#define LS_OPAQUE_TE_LINK_SUBTLV_LOCAL_IP              3 /* rfc3630 */
#define LS_OPAQUE_TE_LINK_SUBTLV_REMOTE_IP             4 /* rfc3630 */
#define LS_OPAQUE_TE_LINK_SUBTLV_TE_METRIC             5 /* rfc3630 */
#define LS_OPAQUE_TE_LINK_SUBTLV_MAX_BW                6 /* rfc3630 */
#define LS_OPAQUE_TE_LINK_SUBTLV_MAX_RES_BW            7 /* rfc3630 */
#define LS_OPAQUE_TE_LINK_SUBTLV_UNRES_BW              8 /* rfc3630 */
#define LS_OPAQUE_TE_LINK_SUBTLV_ADMIN_GROUP           9 /* rfc3630 */
#define LS_OPAQUE_TE_LINK_SUBTLV_LINK_LOCAL_REMOTE_ID 11 /* rfc4203 */
#define LS_OPAQUE_TE_LINK_SUBTLV_LINK_PROTECTION_TYPE 14 /* rfc4203 */
#define LS_OPAQUE_TE_LINK_SUBTLV_INTF_SW_CAP_DESCR    15 /* rfc4203 */
#define LS_OPAQUE_TE_LINK_SUBTLV_SHARED_RISK_GROUP    16 /* rfc4203 */
#define LS_OPAQUE_TE_LINK_SUBTLV_BW_CONSTRAINTS       17 /* rfc4124 */

#define LS_OPAQUE_TE_LINK_SUBTLV_LINK_TYPE_PTP        1  /* rfc3630 */
#define LS_OPAQUE_TE_LINK_SUBTLV_LINK_TYPE_MA         2  /* rfc3630 */

#define LS_OPAQUE_GRACE_TLV_PERIOD       1 /* rfc3623 */
#define LS_OPAQUE_GRACE_TLV_REASON       2 /* rfc3623 */
#define LS_OPAQUE_GRACE_TLV_INT_ADDRESS  3 /* rfc3623 */

#define LS_OPAQUE_GRACE_TLV_REASON_UNKNOWN     0 /* rfc3623 */
#define LS_OPAQUE_GRACE_TLV_REASON_SW_RESTART  1 /* rfc3623 */
#define LS_OPAQUE_GRACE_TLV_REASON_SW_UPGRADE  2 /* rfc3623 */
#define LS_OPAQUE_GRACE_TLV_REASON_CP_SWITCH   3 /* rfc3623 */

#define LS_OPAQUE_RI_TLV_CAP             1 /* draft-ietf-ospf-cap-03 */


/* rla_link.link_type	*/
#define	RLA_TYPE_ROUTER		1   /* point-to-point to another router	*/
#define	RLA_TYPE_TRANSIT	2   /* connection to transit network	*/
#define	RLA_TYPE_STUB		3   /* connection to stub network	*/
#define RLA_TYPE_VIRTUAL	4   /* virtual link			*/

/* rla_flags	*/
#define	RLA_FLAG_B	0x01
#define	RLA_FLAG_E	0x02
#define	RLA_FLAG_V	0x04
#define	RLA_FLAG_W	0x08
#define	RLA_FLAG_NT	0x10 /* RFC3101 Appendix B */
#define	RLA_FLAG_H	0x80

/* sla_tosmetric breakdown	*/
#define	SLA_MASK_TOS		0x7f000000
#define	SLA_MASK_METRIC		0x00ffffff
#define SLA_SHIFT_TOS		24

/* asla_tosmetric breakdown	*/
#define	ASLA_FLAG_EXTERNAL	0x80000000
#define	ASLA_MASK_TOS		0x7f000000
#define	ASLA_SHIFT_TOS		24
#define	ASLA_MASK_METRIC	0x00ffffff

/* multicast vertex type */
#define	MCLA_VERTEX_ROUTER	1
#define	MCLA_VERTEX_NETWORK	2

/* Link-Local-Signaling */
#define OSPF_LLS_HDRLEN         4U /* RFC5613 Section 2.2 */

#define OSPF_LLS_EO             1  /* RFC4811, RFC4812 */
#define OSPF_LLS_MD5            2  /* RFC4813 */

#define OSPF_LLS_EO_LR		0x00000001		/* RFC4811 */
#define OSPF_LLS_EO_RS		0x00000002		/* RFC4812 */

/*
 * TOS metric struct (will be 0 or more in router links update)
 */
struct tos_metric {
    nd_uint8_t  tos_type;
    nd_uint8_t  reserved;
    nd_uint16_t tos_metric;
};
struct tos_link {
    nd_uint8_t  link_type;
    nd_uint8_t  link_tos_count;
    nd_uint16_t tos_metric;
};
union un_tos {
    struct tos_link link;
    struct tos_metric metrics;
};

/* link state advertisement header */
struct lsa_hdr {
    nd_uint16_t ls_age;
    nd_uint8_t  ls_options;
    nd_uint8_t  ls_type;
    union {
        nd_ipv4 lsa_id;
        struct { /* opaque LSAs change the LSA-ID field */
            nd_uint8_t  opaque_type;
            nd_uint24_t opaque_id;
	} opaque_field;
    } un_lsa_id;
    nd_ipv4     ls_router;
    nd_uint32_t ls_seq;
    nd_uint16_t ls_chksum;
    nd_uint16_t ls_length;
};

/* link state advertisement */
struct lsa {
    struct lsa_hdr ls_hdr;

    /* Link state types */
    union {
	/* Router links advertisements */
	struct {
	    nd_uint8_t  rla_flags;
	    nd_byte     rla_zero;
	    nd_uint16_t rla_count;
	    struct rlalink {
		nd_ipv4 link_id;
		nd_ipv4 link_data;
                union un_tos un_tos;
	    } rla_link[1];		/* may repeat	*/
	} un_rla;

	/* Network links advertisements */
	struct {
	    nd_ipv4 nla_mask;
	    nd_ipv4 nla_router[1];	/* may repeat	*/
	} un_nla;

	/* Summary links advertisements */
	struct {
	    nd_ipv4     sla_mask;
	    nd_uint32_t sla_tosmetric[1];	/* may repeat	*/
	} un_sla;

	/* AS external links advertisements */
	struct {
	    nd_ipv4 asla_mask;
	    struct aslametric {
		nd_uint32_t asla_tosmetric;
		nd_ipv4     asla_forward;
		nd_ipv4     asla_tag;
	    } asla_metric[1];		/* may repeat	*/
	} un_asla;

	/* Multicast group membership */
	struct mcla {
	    nd_uint32_t mcla_vtype;
	    nd_ipv4     mcla_vid;
	} un_mcla[1];

        /* Opaque TE LSA */
        struct {
	    nd_uint16_t type;
	    nd_uint16_t length;
	    nd_byte     data[1]; /* may repeat   */
	} un_te_lsa_tlv[1]; /* may repeat */

        /* Opaque Grace LSA */
        struct {
	    nd_uint16_t type;
	    nd_uint16_t length;
	    nd_byte     data[1]; /* may repeat   */
	} un_grace_tlv[1]; /* may repeat */

        /* Opaque Router information LSA */
        struct {
	    nd_uint16_t type;
	    nd_uint16_t length;
	    nd_byte     data[1]; /* may repeat   */
	} un_ri_tlv[1]; /* may repeat */

        /* Unknown LSA */
        struct unknown {
	    nd_byte data[1]; /* may repeat   */
	} un_unknown[1];

    } lsa_un;
};

#define	OSPF_AUTH_SIZE	8

/*
 * the main header
 */
struct ospfhdr {
    nd_uint8_t  ospf_version;
    nd_uint8_t  ospf_type;
    nd_uint16_t ospf_len;
    nd_ipv4     ospf_routerid;
    nd_ipv4     ospf_areaid;
    nd_uint16_t ospf_chksum;
    nd_uint16_t ospf_authtype;
    nd_byte     ospf_authdata[OSPF_AUTH_SIZE];
    union {

	/* Hello packet */
	struct {
	    nd_ipv4     hello_mask;
	    nd_uint16_t hello_helloint;
	    nd_uint8_t  hello_options;
	    nd_uint8_t  hello_priority;
	    nd_uint32_t hello_deadint;
	    nd_ipv4     hello_dr;
	    nd_ipv4     hello_bdr;
	    nd_ipv4     hello_neighbor[1]; /* may repeat	*/
	} un_hello;

	/* Database Description packet */
	struct {
	    nd_uint16_t db_ifmtu;
	    nd_uint8_t  db_options;
	    nd_uint8_t  db_flags;
	    nd_uint32_t db_seq;
	    struct lsa_hdr db_lshdr[1]; /* may repeat	*/
	} un_db;

	/* Link State Request */
	struct lsr {
	    nd_uint32_t ls_type;
            union {
                nd_ipv4 ls_stateid;
                struct { /* opaque LSAs change the LSA-ID field */
                    nd_uint8_t  opaque_type;
                    nd_uint24_t opaque_id;
                } opaque_field;
            } un_ls_stateid;
	    nd_ipv4 ls_router;
	} un_lsr[1];		/* may repeat	*/

	/* Link State Update */
	struct {
	    nd_uint32_t lsu_count;
	    struct lsa  lsu_lsa[1]; /* may repeat	*/
	} un_lsu;

	/* Link State Acknowledgement */
	struct {
	    struct lsa_hdr lsa_lshdr[1]; /* may repeat	*/
	} un_lsa ;
    } ospf_un ;
};

#define	ospf_hello	ospf_un.un_hello
#define	ospf_db		ospf_un.un_db
#define	ospf_lsr	ospf_un.un_lsr
#define	ospf_lsu	ospf_un.un_lsu
#define	ospf_lsa	ospf_un.un_lsa
