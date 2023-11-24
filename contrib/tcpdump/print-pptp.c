/*
 * Copyright (c) 1991, 1993, 1994, 1995, 1996, 1997
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
 * PPTP support contributed by Motonori Shindo (mshindo@mshindo.net)
 */

/* \summary: Point-to-Point Tunnelling Protocol (PPTP) printer */

/* specification: RFC 2637 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"


#define PPTP_MSG_TYPE_CTRL	1	/* Control Message */
#define PPTP_MSG_TYPE_MGMT	2	/* Management Message (currently not used */
#define PPTP_MAGIC_COOKIE	0x1a2b3c4d	/* for sanity check */

#define PPTP_CTRL_MSG_TYPE_SCCRQ	1
#define PPTP_CTRL_MSG_TYPE_SCCRP	2
#define PPTP_CTRL_MSG_TYPE_StopCCRQ	3
#define PPTP_CTRL_MSG_TYPE_StopCCRP	4
#define PPTP_CTRL_MSG_TYPE_ECHORQ	5
#define PPTP_CTRL_MSG_TYPE_ECHORP	6
#define PPTP_CTRL_MSG_TYPE_OCRQ		7
#define PPTP_CTRL_MSG_TYPE_OCRP		8
#define PPTP_CTRL_MSG_TYPE_ICRQ		9
#define PPTP_CTRL_MSG_TYPE_ICRP		10
#define PPTP_CTRL_MSG_TYPE_ICCN		11
#define PPTP_CTRL_MSG_TYPE_CCRQ		12
#define PPTP_CTRL_MSG_TYPE_CDN		13
#define PPTP_CTRL_MSG_TYPE_WEN		14
#define PPTP_CTRL_MSG_TYPE_SLI		15

#define PPTP_FRAMING_CAP_ASYNC_MASK	0x00000001      /* Asynchronous */
#define PPTP_FRAMING_CAP_SYNC_MASK	0x00000002      /* Synchronous */

#define PPTP_BEARER_CAP_ANALOG_MASK	0x00000001      /* Analog */
#define PPTP_BEARER_CAP_DIGITAL_MASK	0x00000002      /* Digital */

static const char *pptp_message_type_string[] = {
	"NOT_DEFINED",		/* 0  Not defined in the RFC2637 */
	"SCCRQ",		/* 1  Start-Control-Connection-Request */
	"SCCRP",		/* 2  Start-Control-Connection-Reply */
	"StopCCRQ",		/* 3  Stop-Control-Connection-Request */
	"StopCCRP",		/* 4  Stop-Control-Connection-Reply */
	"ECHORQ",		/* 5  Echo Request */
	"ECHORP",		/* 6  Echo Reply */

	"OCRQ",			/* 7  Outgoing-Call-Request */
	"OCRP",			/* 8  Outgoing-Call-Reply */
	"ICRQ",			/* 9  Incoming-Call-Request */
	"ICRP",			/* 10 Incoming-Call-Reply */
	"ICCN",			/* 11 Incoming-Call-Connected */
	"CCRQ",			/* 12 Call-Clear-Request */
	"CDN",			/* 13 Call-Disconnect-Notify */

	"WEN",			/* 14 WAN-Error-Notify */

	"SLI"			/* 15 Set-Link-Info */
#define PPTP_MAX_MSGTYPE_INDEX	16
};

/* common for all PPTP control messages */
struct pptp_hdr {
	nd_uint16_t length;
	nd_uint16_t msg_type;
	nd_uint32_t magic_cookie;
	nd_uint16_t ctrl_msg_type;
	nd_uint16_t reserved0;
};

struct pptp_msg_sccrq {
	nd_uint16_t proto_ver;
	nd_uint16_t reserved1;
	nd_uint32_t framing_cap;
	nd_uint32_t bearer_cap;
	nd_uint16_t max_channel;
	nd_uint16_t firm_rev;
	nd_byte     hostname[64];
	nd_byte     vendor[64];
};

struct pptp_msg_sccrp {
	nd_uint16_t proto_ver;
	nd_uint8_t  result_code;
	nd_uint8_t  err_code;
	nd_uint32_t framing_cap;
	nd_uint32_t bearer_cap;
	nd_uint16_t max_channel;
	nd_uint16_t firm_rev;
	nd_byte     hostname[64];
	nd_byte     vendor[64];
};

struct pptp_msg_stopccrq {
	nd_uint8_t  reason;
	nd_uint8_t  reserved1;
	nd_uint16_t reserved2;
};

struct pptp_msg_stopccrp {
	nd_uint8_t  result_code;
	nd_uint8_t  err_code;
	nd_uint16_t reserved1;
};

struct pptp_msg_echorq {
	nd_uint32_t id;
};

struct pptp_msg_echorp {
	nd_uint32_t id;
	nd_uint8_t  result_code;
	nd_uint8_t  err_code;
	nd_uint16_t reserved1;
};

struct pptp_msg_ocrq {
	nd_uint16_t call_id;
	nd_uint16_t call_ser;
	nd_uint32_t min_bps;
	nd_uint32_t max_bps;
	nd_uint32_t bearer_type;
	nd_uint32_t framing_type;
	nd_uint16_t recv_winsiz;
	nd_uint16_t pkt_proc_delay;
	nd_uint16_t phone_no_len;
	nd_uint16_t reserved1;
	nd_byte     phone_no[64];
	nd_byte     subaddr[64];
};

struct pptp_msg_ocrp {
	nd_uint16_t call_id;
	nd_uint16_t peer_call_id;
	nd_uint8_t  result_code;
	nd_uint8_t  err_code;
	nd_uint16_t cause_code;
	nd_uint32_t conn_speed;
	nd_uint16_t recv_winsiz;
	nd_uint16_t pkt_proc_delay;
	nd_uint32_t phy_chan_id;
};

struct pptp_msg_icrq {
	nd_uint16_t call_id;
	nd_uint16_t call_ser;
	nd_uint32_t bearer_type;
	nd_uint32_t phy_chan_id;
	nd_uint16_t dialed_no_len;
	nd_uint16_t dialing_no_len;
	nd_byte     dialed_no[64];		/* DNIS */
	nd_byte     dialing_no[64];		/* CLID */
	nd_byte     subaddr[64];
};

struct pptp_msg_icrp {
	nd_uint16_t call_id;
	nd_uint16_t peer_call_id;
	nd_uint8_t  result_code;
	nd_uint8_t  err_code;
	nd_uint16_t recv_winsiz;
	nd_uint16_t pkt_proc_delay;
	nd_uint16_t reserved1;
};

struct pptp_msg_iccn {
	nd_uint16_t peer_call_id;
	nd_uint16_t reserved1;
	nd_uint32_t conn_speed;
	nd_uint16_t recv_winsiz;
	nd_uint16_t pkt_proc_delay;
	nd_uint32_t framing_type;
};

struct pptp_msg_ccrq {
	nd_uint16_t call_id;
	nd_uint16_t reserved1;
};

struct pptp_msg_cdn {
	nd_uint16_t call_id;
	nd_uint8_t  result_code;
	nd_uint8_t  err_code;
	nd_uint16_t cause_code;
	nd_uint16_t reserved1;
	nd_byte     call_stats[128];
};

struct pptp_msg_wen {
	nd_uint16_t peer_call_id;
	nd_uint16_t reserved1;
	nd_uint32_t crc_err;
	nd_uint32_t framing_err;
	nd_uint32_t hardware_overrun;
	nd_uint32_t buffer_overrun;
	nd_uint32_t timeout_err;
	nd_uint32_t align_err;
};

struct pptp_msg_sli {
	nd_uint16_t peer_call_id;
	nd_uint16_t reserved1;
	nd_uint32_t send_accm;
	nd_uint32_t recv_accm;
};

/* attributes that appear more than once in above messages:

   Number of
   occurrence    attributes
  --------------------------------------
      2         uint32_t bearer_cap;
      2         uint32_t bearer_type;
      6         uint16_t call_id;
      2         uint16_t call_ser;
      2         uint16_t cause_code;
      2         uint32_t conn_speed;
      6         uint8_t err_code;
      2         uint16_t firm_rev;
      2         uint32_t framing_cap;
      2         uint32_t framing_type;
      2         u_char hostname[64];
      2         uint32_t id;
      2         uint16_t max_channel;
      5         uint16_t peer_call_id;
      2         uint32_t phy_chan_id;
      4         uint16_t pkt_proc_delay;
      2         uint16_t proto_ver;
      4         uint16_t recv_winsiz;
      2         uint8_t reserved1;
      9         uint16_t reserved1;
      6         uint8_t result_code;
      2         u_char subaddr[64];
      2         u_char vendor[64];

  so I will prepare print out functions for these attributes (except for
  reserved*).
*/

#define PRINT_RESERVED_IF_NOT_ZERO_1(reserved) \
        if (GET_U_1(reserved)) \
		ND_PRINT(" [ERROR: reserved=%u must be zero]", \
			 GET_U_1(reserved));

#define PRINT_RESERVED_IF_NOT_ZERO_2(reserved) \
        if (GET_BE_U_2(reserved)) \
		ND_PRINT(" [ERROR: reserved=%u must be zero]", \
			 GET_BE_U_2(reserved));

/******************************************/
/* Attribute-specific print out functions */
/******************************************/

static void
pptp_bearer_cap_print(netdissect_options *ndo,
                      const nd_uint32_t bearer_cap)
{
	ND_PRINT(" BEARER_CAP(%s%s)",
	          GET_BE_U_4(bearer_cap) & PPTP_BEARER_CAP_DIGITAL_MASK ? "D" : "",
	          GET_BE_U_4(bearer_cap) & PPTP_BEARER_CAP_ANALOG_MASK ? "A" : "");
}

static const struct tok pptp_btype_str[] = {
	{ 1, "A"   }, /* Analog */
	{ 2, "D"   }, /* Digital */
	{ 3, "Any" },
	{ 0, NULL }
};

static void
pptp_bearer_type_print(netdissect_options *ndo,
                       const nd_uint32_t bearer_type)
{
	ND_PRINT(" BEARER_TYPE(%s)",
	          tok2str(pptp_btype_str, "?", GET_BE_U_4(bearer_type)));
}

static void
pptp_call_id_print(netdissect_options *ndo,
                   const nd_uint16_t call_id)
{
	ND_PRINT(" CALL_ID(%u)", GET_BE_U_2(call_id));
}

static void
pptp_call_ser_print(netdissect_options *ndo,
                    const nd_uint16_t call_ser)
{
	ND_PRINT(" CALL_SER_NUM(%u)", GET_BE_U_2(call_ser));
}

static void
pptp_cause_code_print(netdissect_options *ndo,
                      const nd_uint16_t cause_code)
{
	ND_PRINT(" CAUSE_CODE(%u)", GET_BE_U_2(cause_code));
}

static void
pptp_conn_speed_print(netdissect_options *ndo,
                      const nd_uint32_t conn_speed)
{
	ND_PRINT(" CONN_SPEED(%u)", GET_BE_U_4(conn_speed));
}

static const struct tok pptp_errcode_str[] = {
	{ 0, "None"          },
	{ 1, "Not-Connected" },
	{ 2, "Bad-Format"    },
	{ 3, "Bad-Value"     },
	{ 4, "No-Resource"   },
	{ 5, "Bad-Call-ID"   },
	{ 6, "PAC-Error"     },
	{ 0, NULL }
};

static void
pptp_err_code_print(netdissect_options *ndo,
                    const nd_uint8_t err_code)
{
	ND_PRINT(" ERR_CODE(%u", GET_U_1(err_code));
	if (ndo->ndo_vflag) {
		ND_PRINT(":%s",
			 tok2str(pptp_errcode_str, "?", GET_U_1(err_code)));
	}
	ND_PRINT(")");
}

static void
pptp_firm_rev_print(netdissect_options *ndo,
                    const nd_uint16_t firm_rev)
{
	ND_PRINT(" FIRM_REV(%u)", GET_BE_U_2(firm_rev));
}

static void
pptp_framing_cap_print(netdissect_options *ndo,
                       const nd_uint32_t framing_cap)
{
	ND_PRINT(" FRAME_CAP(");
	if (GET_BE_U_4(framing_cap) & PPTP_FRAMING_CAP_ASYNC_MASK) {
                ND_PRINT("A");		/* Async */
        }
        if (GET_BE_U_4(framing_cap) & PPTP_FRAMING_CAP_SYNC_MASK) {
                ND_PRINT("S");		/* Sync */
        }
	ND_PRINT(")");
}

static const struct tok pptp_ftype_str[] = {
	{ 1, "A" }, /* Async */
	{ 2, "S" }, /* Sync */
	{ 3, "E" }, /* Either */
	{ 0, NULL }
};

static void
pptp_framing_type_print(netdissect_options *ndo,
                        const nd_uint32_t framing_type)
{
	ND_PRINT(" FRAME_TYPE(%s)",
	          tok2str(pptp_ftype_str, "?", GET_BE_U_4(framing_type)));
}

static void
pptp_hostname_print(netdissect_options *ndo,
                    const u_char *hostname)
{
	ND_PRINT(" HOSTNAME(");
	nd_printjnp(ndo, hostname, 64);
	ND_PRINT(")");
}

static void
pptp_id_print(netdissect_options *ndo,
              const nd_uint32_t id)
{
	ND_PRINT(" ID(%u)", GET_BE_U_4(id));
}

static void
pptp_max_channel_print(netdissect_options *ndo,
                       const nd_uint16_t max_channel)
{
	ND_PRINT(" MAX_CHAN(%u)", GET_BE_U_2(max_channel));
}

static void
pptp_peer_call_id_print(netdissect_options *ndo,
                        const nd_uint16_t peer_call_id)
{
	ND_PRINT(" PEER_CALL_ID(%u)", GET_BE_U_2(peer_call_id));
}

static void
pptp_phy_chan_id_print(netdissect_options *ndo,
                       const nd_uint32_t phy_chan_id)
{
	ND_PRINT(" PHY_CHAN_ID(%u)", GET_BE_U_4(phy_chan_id));
}

static void
pptp_pkt_proc_delay_print(netdissect_options *ndo,
                          const nd_uint16_t pkt_proc_delay)
{
	ND_PRINT(" PROC_DELAY(%u)", GET_BE_U_2(pkt_proc_delay));
}

static void
pptp_proto_ver_print(netdissect_options *ndo,
                     const nd_uint16_t proto_ver)
{
	ND_PRINT(" PROTO_VER(%u.%u)",	/* Version.Revision */
	       GET_BE_U_2(proto_ver) >> 8,
	       GET_BE_U_2(proto_ver) & 0xff);
}

static void
pptp_recv_winsiz_print(netdissect_options *ndo,
                       const nd_uint16_t recv_winsiz)
{
	ND_PRINT(" RECV_WIN(%u)", GET_BE_U_2(recv_winsiz));
}

static const struct tok pptp_scrrp_str[] = {
	{ 1, "Successful channel establishment"                           },
	{ 2, "General error"                                              },
	{ 3, "Command channel already exists"                             },
	{ 4, "Requester is not authorized to establish a command channel" },
	{ 5, "The protocol version of the requester is not supported"     },
	{ 0, NULL }
};

static const struct tok pptp_echorp_str[] = {
	{ 1, "OK" },
	{ 2, "General Error" },
	{ 0, NULL }
};

static const struct tok pptp_ocrp_str[] = {
	{ 1, "Connected"     },
	{ 2, "General Error" },
	{ 3, "No Carrier"    },
	{ 4, "Busy"          },
	{ 5, "No Dial Tone"  },
	{ 6, "Time-out"      },
	{ 7, "Do Not Accept" },
	{ 0, NULL }
};

static const struct tok pptp_icrp_str[] = {
	{ 1, "Connect"       },
	{ 2, "General Error" },
	{ 3, "Do Not Accept" },
	{ 0, NULL }
};

static const struct tok pptp_cdn_str[] = {
	{ 1, "Lost Carrier"   },
	{ 2, "General Error"  },
	{ 3, "Admin Shutdown" },
	{ 4, "Request"        },
	{ 0, NULL }
};

static void
pptp_result_code_print(netdissect_options *ndo,
                       const nd_uint8_t result_code, int ctrl_msg_type)
{
	ND_PRINT(" RESULT_CODE(%u", GET_U_1(result_code));
	if (ndo->ndo_vflag) {
		const struct tok *dict =
			ctrl_msg_type == PPTP_CTRL_MSG_TYPE_SCCRP    ? pptp_scrrp_str :
			ctrl_msg_type == PPTP_CTRL_MSG_TYPE_StopCCRP ? pptp_echorp_str :
			ctrl_msg_type == PPTP_CTRL_MSG_TYPE_ECHORP   ? pptp_echorp_str :
			ctrl_msg_type == PPTP_CTRL_MSG_TYPE_OCRP     ? pptp_ocrp_str :
			ctrl_msg_type == PPTP_CTRL_MSG_TYPE_ICRP     ? pptp_icrp_str :
			ctrl_msg_type == PPTP_CTRL_MSG_TYPE_CDN      ? pptp_cdn_str :
			NULL; /* assertion error */
		if (dict != NULL)
			ND_PRINT(":%s",
				 tok2str(dict, "?", GET_U_1(result_code)));
	}
	ND_PRINT(")");
}

static void
pptp_subaddr_print(netdissect_options *ndo,
                   const u_char *subaddr)
{
	ND_PRINT(" SUB_ADDR(");
	nd_printjnp(ndo, subaddr, 64);
	ND_PRINT(")");
}

static void
pptp_vendor_print(netdissect_options *ndo,
                  const u_char *vendor)
{
	ND_PRINT(" VENDOR(");
	nd_printjnp(ndo, vendor, 64);
	ND_PRINT(")");
}

/************************************/
/* PPTP message print out functions */
/************************************/
static void
pptp_sccrq_print(netdissect_options *ndo,
                 const u_char *dat)
{
	const struct pptp_msg_sccrq *ptr = (const struct pptp_msg_sccrq *)dat;

	pptp_proto_ver_print(ndo, ptr->proto_ver);
	PRINT_RESERVED_IF_NOT_ZERO_2(ptr->reserved1);
	pptp_framing_cap_print(ndo, ptr->framing_cap);
	pptp_bearer_cap_print(ndo, ptr->bearer_cap);
	pptp_max_channel_print(ndo, ptr->max_channel);
	pptp_firm_rev_print(ndo, ptr->firm_rev);
	pptp_hostname_print(ndo, ptr->hostname);
	pptp_vendor_print(ndo, ptr->vendor);
}

static void
pptp_sccrp_print(netdissect_options *ndo,
                 const u_char *dat)
{
	const struct pptp_msg_sccrp *ptr = (const struct pptp_msg_sccrp *)dat;

	pptp_proto_ver_print(ndo, ptr->proto_ver);
	pptp_result_code_print(ndo, ptr->result_code, PPTP_CTRL_MSG_TYPE_SCCRP);
	pptp_err_code_print(ndo, ptr->err_code);
	pptp_framing_cap_print(ndo, ptr->framing_cap);
	pptp_bearer_cap_print(ndo, ptr->bearer_cap);
	pptp_max_channel_print(ndo, ptr->max_channel);
	pptp_firm_rev_print(ndo, ptr->firm_rev);
	pptp_hostname_print(ndo, ptr->hostname);
	pptp_vendor_print(ndo, ptr->vendor);
}

static void
pptp_stopccrq_print(netdissect_options *ndo,
                    const u_char *dat)
{
	const struct pptp_msg_stopccrq *ptr = (const struct pptp_msg_stopccrq *)dat;

	ND_PRINT(" REASON(%u", GET_U_1(ptr->reason));
	if (ndo->ndo_vflag) {
		switch (GET_U_1(ptr->reason)) {
		case 1:
			ND_PRINT(":None");
			break;
		case 2:
			ND_PRINT(":Stop-Protocol");
			break;
		case 3:
			ND_PRINT(":Stop-Local-Shutdown");
			break;
		default:
			ND_PRINT(":?");
			break;
		}
	}
	ND_PRINT(")");
	PRINT_RESERVED_IF_NOT_ZERO_1(ptr->reserved1);
	PRINT_RESERVED_IF_NOT_ZERO_2(ptr->reserved2);
}

static void
pptp_stopccrp_print(netdissect_options *ndo,
                    const u_char *dat)
{
	const struct pptp_msg_stopccrp *ptr = (const struct pptp_msg_stopccrp *)dat;

	pptp_result_code_print(ndo, ptr->result_code, PPTP_CTRL_MSG_TYPE_StopCCRP);
	pptp_err_code_print(ndo, ptr->err_code);
	PRINT_RESERVED_IF_NOT_ZERO_2(ptr->reserved1);
}

static void
pptp_echorq_print(netdissect_options *ndo,
                  const u_char *dat)
{
	const struct pptp_msg_echorq *ptr = (const struct pptp_msg_echorq *)dat;

	pptp_id_print(ndo, ptr->id);
}

static void
pptp_echorp_print(netdissect_options *ndo,
                  const u_char *dat)
{
	const struct pptp_msg_echorp *ptr = (const struct pptp_msg_echorp *)dat;

	pptp_id_print(ndo, ptr->id);
	pptp_result_code_print(ndo, ptr->result_code, PPTP_CTRL_MSG_TYPE_ECHORP);
	pptp_err_code_print(ndo, ptr->err_code);
	PRINT_RESERVED_IF_NOT_ZERO_2(ptr->reserved1);
}

static void
pptp_ocrq_print(netdissect_options *ndo,
                const u_char *dat)
{
	const struct pptp_msg_ocrq *ptr = (const struct pptp_msg_ocrq *)dat;

	pptp_call_id_print(ndo, ptr->call_id);
	pptp_call_ser_print(ndo, ptr->call_ser);
	ND_PRINT(" MIN_BPS(%u)", GET_BE_U_4(ptr->min_bps));
	ND_PRINT(" MAX_BPS(%u)", GET_BE_U_4(ptr->max_bps));
	pptp_bearer_type_print(ndo, ptr->bearer_type);
	pptp_framing_type_print(ndo, ptr->framing_type);
	pptp_recv_winsiz_print(ndo, ptr->recv_winsiz);
	pptp_pkt_proc_delay_print(ndo, ptr->pkt_proc_delay);
	ND_PRINT(" PHONE_NO_LEN(%u)", GET_BE_U_2(ptr->phone_no_len));
	PRINT_RESERVED_IF_NOT_ZERO_2(ptr->reserved1);
	ND_PRINT(" PHONE_NO(");
	nd_printjnp(ndo, ptr->phone_no,
		    ND_MIN(64, GET_BE_U_2(ptr->phone_no_len)));
	ND_PRINT(")");
	pptp_subaddr_print(ndo, ptr->subaddr);
}

static void
pptp_ocrp_print(netdissect_options *ndo,
                const u_char *dat)
{
	const struct pptp_msg_ocrp *ptr = (const struct pptp_msg_ocrp *)dat;

	pptp_call_id_print(ndo, ptr->call_id);
	pptp_peer_call_id_print(ndo, ptr->peer_call_id);
	pptp_result_code_print(ndo, ptr->result_code, PPTP_CTRL_MSG_TYPE_OCRP);
	pptp_err_code_print(ndo, ptr->err_code);
	pptp_cause_code_print(ndo, ptr->cause_code);
	pptp_conn_speed_print(ndo, ptr->conn_speed);
	pptp_recv_winsiz_print(ndo, ptr->recv_winsiz);
	pptp_pkt_proc_delay_print(ndo, ptr->pkt_proc_delay);
	pptp_phy_chan_id_print(ndo, ptr->phy_chan_id);
}

static void
pptp_icrq_print(netdissect_options *ndo,
                const u_char *dat)
{
	const struct pptp_msg_icrq *ptr = (const struct pptp_msg_icrq *)dat;

	pptp_call_id_print(ndo, ptr->call_id);
	pptp_call_ser_print(ndo, ptr->call_ser);
	pptp_bearer_type_print(ndo, ptr->bearer_type);
	pptp_phy_chan_id_print(ndo, ptr->phy_chan_id);
	ND_PRINT(" DIALED_NO_LEN(%u)", GET_BE_U_2(ptr->dialed_no_len));
	ND_PRINT(" DIALING_NO_LEN(%u)", GET_BE_U_2(ptr->dialing_no_len));
	ND_PRINT(" DIALED_NO(");
	nd_printjnp(ndo, ptr->dialed_no,
		    ND_MIN(64, GET_BE_U_2(ptr->dialed_no_len)));
	ND_PRINT(")");
	ND_PRINT(" DIALING_NO(");
	nd_printjnp(ndo, ptr->dialing_no,
		    ND_MIN(64, GET_BE_U_2(ptr->dialing_no_len)));
	ND_PRINT(")");
	pptp_subaddr_print(ndo, ptr->subaddr);
}

static void
pptp_icrp_print(netdissect_options *ndo,
                const u_char *dat)
{
	const struct pptp_msg_icrp *ptr = (const struct pptp_msg_icrp *)dat;

	pptp_call_id_print(ndo, ptr->call_id);
	pptp_peer_call_id_print(ndo, ptr->peer_call_id);
	pptp_result_code_print(ndo, ptr->result_code, PPTP_CTRL_MSG_TYPE_ICRP);
	pptp_err_code_print(ndo, ptr->err_code);
	pptp_recv_winsiz_print(ndo, ptr->recv_winsiz);
	pptp_pkt_proc_delay_print(ndo, ptr->pkt_proc_delay);
	PRINT_RESERVED_IF_NOT_ZERO_2(ptr->reserved1);
}

static void
pptp_iccn_print(netdissect_options *ndo,
                const u_char *dat)
{
	const struct pptp_msg_iccn *ptr = (const struct pptp_msg_iccn *)dat;

	pptp_peer_call_id_print(ndo, ptr->peer_call_id);
	PRINT_RESERVED_IF_NOT_ZERO_2(ptr->reserved1);
	pptp_conn_speed_print(ndo, ptr->conn_speed);
	pptp_recv_winsiz_print(ndo, ptr->recv_winsiz);
	pptp_pkt_proc_delay_print(ndo, ptr->pkt_proc_delay);
	pptp_framing_type_print(ndo, ptr->framing_type);
}

static void
pptp_ccrq_print(netdissect_options *ndo,
                const u_char *dat)
{
	const struct pptp_msg_ccrq *ptr = (const struct pptp_msg_ccrq *)dat;

	pptp_call_id_print(ndo, ptr->call_id);
	PRINT_RESERVED_IF_NOT_ZERO_2(ptr->reserved1);
}

static void
pptp_cdn_print(netdissect_options *ndo,
               const u_char *dat)
{
	const struct pptp_msg_cdn *ptr = (const struct pptp_msg_cdn *)dat;

	pptp_call_id_print(ndo, ptr->call_id);
	pptp_result_code_print(ndo, ptr->result_code, PPTP_CTRL_MSG_TYPE_CDN);
	pptp_err_code_print(ndo, ptr->err_code);
	pptp_cause_code_print(ndo, ptr->cause_code);
	PRINT_RESERVED_IF_NOT_ZERO_2(ptr->reserved1);
	ND_PRINT(" CALL_STATS(");
	nd_printjnp(ndo, ptr->call_stats, 128);
	ND_PRINT(")");
}

static void
pptp_wen_print(netdissect_options *ndo,
               const u_char *dat)
{
	const struct pptp_msg_wen *ptr = (const struct pptp_msg_wen *)dat;

	pptp_peer_call_id_print(ndo, ptr->peer_call_id);
	PRINT_RESERVED_IF_NOT_ZERO_2(ptr->reserved1);
	ND_PRINT(" CRC_ERR(%u)", GET_BE_U_4(ptr->crc_err));
	ND_PRINT(" FRAMING_ERR(%u)", GET_BE_U_4(ptr->framing_err));
	ND_PRINT(" HARDWARE_OVERRUN(%u)", GET_BE_U_4(ptr->hardware_overrun));
	ND_PRINT(" BUFFER_OVERRUN(%u)", GET_BE_U_4(ptr->buffer_overrun));
	ND_PRINT(" TIMEOUT_ERR(%u)", GET_BE_U_4(ptr->timeout_err));
	ND_PRINT(" ALIGN_ERR(%u)", GET_BE_U_4(ptr->align_err));
}

static void
pptp_sli_print(netdissect_options *ndo,
               const u_char *dat)
{
	const struct pptp_msg_sli *ptr = (const struct pptp_msg_sli *)dat;

	pptp_peer_call_id_print(ndo, ptr->peer_call_id);
	PRINT_RESERVED_IF_NOT_ZERO_2(ptr->reserved1);
	ND_PRINT(" SEND_ACCM(0x%08x)", GET_BE_U_4(ptr->send_accm));
	ND_PRINT(" RECV_ACCM(0x%08x)", GET_BE_U_4(ptr->recv_accm));
}

void
pptp_print(netdissect_options *ndo,
           const u_char *dat)
{
	const struct pptp_hdr *hdr;
	uint32_t mc;
	uint16_t ctrl_msg_type;

	ndo->ndo_protocol = "pptp";
	ND_PRINT(": ");
	nd_print_protocol(ndo);

	hdr = (const struct pptp_hdr *)dat;

	if (ndo->ndo_vflag) {
		ND_PRINT(" Length=%u", GET_BE_U_2(hdr->length));
	}
	if (ndo->ndo_vflag) {
		switch(GET_BE_U_2(hdr->msg_type)) {
		case PPTP_MSG_TYPE_CTRL:
			ND_PRINT(" CTRL-MSG");
			break;
		case PPTP_MSG_TYPE_MGMT:
			ND_PRINT(" MGMT-MSG");
			break;
		default:
			ND_PRINT(" UNKNOWN-MSG-TYPE");
			break;
		}
	}

	mc = GET_BE_U_4(hdr->magic_cookie);
	if (mc != PPTP_MAGIC_COOKIE) {
		ND_PRINT(" UNEXPECTED Magic-Cookie!!(%08x)", mc);
	}
	if (ndo->ndo_vflag || mc != PPTP_MAGIC_COOKIE) {
		ND_PRINT(" Magic-Cookie=%08x", mc);
	}
	ctrl_msg_type = GET_BE_U_2(hdr->ctrl_msg_type);
	if (ctrl_msg_type < PPTP_MAX_MSGTYPE_INDEX) {
		ND_PRINT(" CTRL_MSGTYPE=%s",
		       pptp_message_type_string[ctrl_msg_type]);
	} else {
		ND_PRINT(" UNKNOWN_CTRL_MSGTYPE(%u)", ctrl_msg_type);
	}
	PRINT_RESERVED_IF_NOT_ZERO_2(hdr->reserved0);

	dat += 12;

	switch(ctrl_msg_type) {
	case PPTP_CTRL_MSG_TYPE_SCCRQ:
		pptp_sccrq_print(ndo, dat);
		break;
	case PPTP_CTRL_MSG_TYPE_SCCRP:
		pptp_sccrp_print(ndo, dat);
		break;
	case PPTP_CTRL_MSG_TYPE_StopCCRQ:
		pptp_stopccrq_print(ndo, dat);
		break;
	case PPTP_CTRL_MSG_TYPE_StopCCRP:
		pptp_stopccrp_print(ndo, dat);
		break;
	case PPTP_CTRL_MSG_TYPE_ECHORQ:
		pptp_echorq_print(ndo, dat);
		break;
	case PPTP_CTRL_MSG_TYPE_ECHORP:
		pptp_echorp_print(ndo, dat);
		break;
	case PPTP_CTRL_MSG_TYPE_OCRQ:
		pptp_ocrq_print(ndo, dat);
		break;
	case PPTP_CTRL_MSG_TYPE_OCRP:
		pptp_ocrp_print(ndo, dat);
		break;
	case PPTP_CTRL_MSG_TYPE_ICRQ:
		pptp_icrq_print(ndo, dat);
		break;
	case PPTP_CTRL_MSG_TYPE_ICRP:
		pptp_icrp_print(ndo, dat);
		break;
	case PPTP_CTRL_MSG_TYPE_ICCN:
		pptp_iccn_print(ndo, dat);
		break;
	case PPTP_CTRL_MSG_TYPE_CCRQ:
		pptp_ccrq_print(ndo, dat);
		break;
	case PPTP_CTRL_MSG_TYPE_CDN:
		pptp_cdn_print(ndo, dat);
		break;
	case PPTP_CTRL_MSG_TYPE_WEN:
		pptp_wen_print(ndo, dat);
		break;
	case PPTP_CTRL_MSG_TYPE_SLI:
		pptp_sli_print(ndo, dat);
		break;
	default:
		/* do nothing */
		break;
	}
}
