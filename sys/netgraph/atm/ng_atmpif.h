/*-
 * Copyright (c) 2003 Harti Brandt.
 * Copyright (c) 2003 Vincent Jardin.
 * All rights reserved.
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
 * $FreeBSD$
 */

#ifndef _NETGRAPH_ATM_NG_ATMPIF_H_
#define _NETGRAPH_ATM_NG_ATMPIF_H_

/* Node type name and magic cookie */
#define NG_ATMPIF_NODE_TYPE		"atmpif"
#define NGM_ATMPIF_COOKIE		967239456

/*
 * Physical device name - used to configure HARP devices
 */
#ifndef VATMPIF_DEV_NAME
#define VATMPIF_DEV_NAME	"hva"	/* HARP Virtual ATM */
#endif

#define VATMPIF_MAX_VCI		65535
#define VATMPIF_MAX_VPI		255

/* Hook name */
#define NG_ATMPIF_HOOK_LINK	"link"	 /* virtual link hook */

/*
 * Node configuration structure
 */
struct ng_vatmpif_config {
	uint8_t		debug;		/* debug bit field (see below) */
	uint32_t	pcr;		/* peak cell rate */
	Mac_addr	macaddr;	/* Mac Address */
};
/* Keep this in sync with the above structure definition */
#define NG_ATMPIF_CONFIG_TYPE_INFO { 				\
	{ "debug",		&ng_parse_uint8_type	},	\
	{ "pcr",		&ng_parse_uint32_type	},	\
	{ "macaddr",		&ng_mac_addr_type	},	\
	{ NULL }						\
}

/*
 * Debug bit-fields
 */
#define VATMPIF_DEBUG_NONE	0x00
#define VATMPIF_DEBUG_PACKET	0x01 /* enable ng_atmpif debugging	 */

#define IS_VATMPIF_DEBUG_PACKET(a)  ( (a)	\
				&&  ((a)->conf.debug & VATMPIF_DEBUG_PACKET)  )

/*
 * Statistics
 */
struct hva_stats_ng {
	uint32_t	ng_errseq;	/* Duplicate or out of order */
	uint32_t	ng_lostpdu;	/* PDU lost detected */
	uint32_t	ng_badpdu;	/* Unknown PDU type */
	uint32_t	ng_rx_novcc;	/* Draining PDU on closed VCC */
	uint32_t	ng_rx_iqfull;	/* PDU drops, no room in atm_intrq */
	uint32_t	ng_tx_rawcell;	/* PDU raw cells transmitted */
	uint32_t	ng_rx_rawcell;	/* PDU raw cells received */
	uint64_t	ng_tx_pdu;	/* PDU transmitted */
	uint64_t	ng_rx_pdu;	/* PDU received */
};
typedef struct hva_stats_ng	Hva_Stats_ng;
/* Keep this in sync with the above structure definition */
#define HVA_STATS_NG_TYPE_INFO 		 			\
	{ "errSeqOrder",	&ng_parse_uint32_type	},	\
	{ "errLostPDU",		&ng_parse_uint32_type	},	\
	{ "recvBadPDU",		&ng_parse_uint32_type	},	\
	{ "ErrATMVC",		&ng_parse_uint32_type	},	\
	{ "ErrQfull",		&ng_parse_uint32_type	},	\
	{ "xmitRawCell",	&ng_parse_uint32_type	},	\
	{ "recvRawCell",	&ng_parse_uint32_type	},	\
	{ "xmitPDU",		&ng_parse_uint64_type	},	\
	{ "recvPDU",		&ng_parse_uint64_type	},
	

struct hva_stats_atm {
	uint64_t	atm_xmit;	/* Cells transmitted */
	uint64_t	atm_rcvd;	/* Cells received */
};
typedef struct hva_stats_atm	Hva_Stats_atm;
/* Keep this in sync with the above structure definition */
#define HVA_STATS_ATM_NG_TYPE_INFO	 			\
	{ "xmitATMCells",	&ng_parse_uint64_type	},	\
	{ "recvATMCells",	&ng_parse_uint64_type	},

struct hva_stats_aal5 {
	uint64_t	aal5_xmit;	/* Cells transmitted */
	uint64_t	aal5_rcvd;	/* Cells received */
	uint32_t	aal5_crc_len;	/* Cells with CRC/length errors */
	uint32_t	aal5_drops;	/* Cell drops */
	uint64_t	aal5_pdu_xmit;	/* CS PDUs transmitted */
	uint64_t	aal5_pdu_rcvd;	/* CS PDUs received */
	uint32_t	aal5_pdu_crc;	/* CS PDUs with CRC errors */
	uint32_t	aal5_pdu_errs;	/* CS layer protocol errors */
	uint32_t	aal5_pdu_drops;	/* CS PDUs dropped */
};
typedef struct hva_stats_aal5	Hva_Stats_aal5;
/* Keep this in sync with the above structure definition */
#define HVA_STATS_AAL5_NG_TYPE_INFO	 			\
	{ "xmitAAL5Cells",		&ng_parse_uint64_type	},	\
	{ "recvAAL5Cells",		&ng_parse_uint64_type	},	\
	{ "AAL5ErrCRCCells",		&ng_parse_uint32_type	},	\
	{ "AAL5DropsCells",		&ng_parse_uint32_type	},	\
	{ "xmitAAL5PDU",		&ng_parse_uint64_type	},	\
	{ "recvAAL5PDU",		&ng_parse_uint64_type	},	\
	{ "AAL5CRCPDU",			&ng_parse_uint32_type	},	\
	{ "AAL5ErrPDU",			&ng_parse_uint32_type	},	\
	{ "AAL5DropsPDU",		&ng_parse_uint32_type	},

struct vatmpif_stats {
	Hva_Stats_ng	hva_st_ng;	/* Netgraph layer stats */
	Hva_Stats_atm	hva_st_atm;	/* ATM layer stats */
	Hva_Stats_aal5	hva_st_aal5;	/* AAL5 layer stats */
};
typedef struct vatmpif_stats	Vatmpif_stats;
/* Keep this in sync with the above structure definition */
#define NG_ATMPIF_STATS_TYPE_INFO {	\
	HVA_STATS_NG_TYPE_INFO	 	\
	HVA_STATS_ATM_NG_TYPE_INFO	\
	HVA_STATS_AAL5_NG_TYPE_INFO	\
	{ NULL }			\
}

/* Structure returned by NGM_ATMPIF_GET_LINK_STATUS */
struct ng_atmpif_link_status {
	uint32_t	InSeq;		/* last received sequence number + 1 */
	uint32_t	OutSeq;		/* last sent sequence number */
	uint32_t	cur_pcr;	/* slot's reserved PCR */
};
/* Keep this in sync with the above structure definition */
#define NG_ATMPIF_LINK_STATUS_TYPE_INFO  {		\
	{ "InSeq",	&ng_parse_uint32_type },	\
	{ "OutSeq",	&ng_parse_uint32_type },	\
	{ "cur_pcr",	&ng_parse_uint32_type },	\
	{ NULL }					\
}

/* Netgraph control messages */
enum {
	NGM_ATMPIF_SET_CONFIG = 1,	/* set node configuration */
	NGM_ATMPIF_GET_CONFIG,		/* get node configuration */
	NGM_ATMPIF_GET_LINK_STATUS,	/* get link status */
	NGM_ATMPIF_GET_STATS,		/* get link stats */
	NGM_ATMPIF_CLR_STATS,		/* clear link stats */
	NGM_ATMPIF_GETCLR_STATS,	/* atomically get & clear link stats */
};

#endif /* _NETGRAPH_NG_ATMPIF_H_ */
