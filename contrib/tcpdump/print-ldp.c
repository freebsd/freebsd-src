/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * Original code by Hannes Gredler (hannes@gredler.at)
 *  and Steinar Haug (sthaug@nethelp.no)
 */

/* \summary: Label Distribution Protocol (LDP) printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"

#include "l2vpn.h"
#include "af.h"


/*
 * ldp common header
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  Version                      |         PDU Length            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         LDP Identifier                        |
 * +                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */

struct ldp_common_header {
    nd_uint16_t version;
    nd_uint16_t pdu_length;
    nd_ipv4     lsr_id;
    nd_uint16_t label_space;
};

#define LDP_VERSION 1

/*
 * ldp message header
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |U|   Message Type              |      Message Length           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     Message ID                                |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +                                                               +
 * |                     Mandatory Parameters                      |
 * +                                                               +
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +                                                               +
 * |                     Optional Parameters                       |
 * +                                                               +
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

struct ldp_msg_header {
    nd_uint16_t type;
    nd_uint16_t length;
    nd_uint32_t id;
};

#define	LDP_MASK_MSG_TYPE(x)  ((x)&0x7fff)
#define	LDP_MASK_U_BIT(x)     ((x)&0x8000)

#define	LDP_MSG_NOTIF                0x0001
#define	LDP_MSG_HELLO                0x0100
#define	LDP_MSG_INIT                 0x0200
#define	LDP_MSG_KEEPALIVE            0x0201
#define	LDP_MSG_ADDRESS              0x0300
#define	LDP_MSG_ADDRESS_WITHDRAW     0x0301
#define	LDP_MSG_LABEL_MAPPING        0x0400
#define	LDP_MSG_LABEL_REQUEST        0x0401
#define	LDP_MSG_LABEL_WITHDRAW       0x0402
#define	LDP_MSG_LABEL_RELEASE        0x0403
#define	LDP_MSG_LABEL_ABORT_REQUEST  0x0404

#define	LDP_VENDOR_PRIVATE_MIN       0x3e00
#define	LDP_VENDOR_PRIVATE_MAX       0x3eff
#define	LDP_EXPERIMENTAL_MIN         0x3f00
#define	LDP_EXPERIMENTAL_MAX         0x3fff

static const struct tok ldp_msg_values[] = {
    { LDP_MSG_NOTIF,	             "Notification" },
    { LDP_MSG_HELLO,	             "Hello" },
    { LDP_MSG_INIT,	             "Initialization" },
    { LDP_MSG_KEEPALIVE,             "Keepalive" },
    { LDP_MSG_ADDRESS,	             "Address" },
    { LDP_MSG_ADDRESS_WITHDRAW,	     "Address Withdraw" },
    { LDP_MSG_LABEL_MAPPING,	     "Label Mapping" },
    { LDP_MSG_LABEL_REQUEST,	     "Label Request" },
    { LDP_MSG_LABEL_WITHDRAW,	     "Label Withdraw" },
    { LDP_MSG_LABEL_RELEASE,	     "Label Release" },
    { LDP_MSG_LABEL_ABORT_REQUEST,   "Label Abort Request" },
    { 0, NULL}
};

#define	LDP_MASK_TLV_TYPE(x)  ((x)&0x3fff)
#define	LDP_MASK_F_BIT(x) ((x)&0x4000)

#define	LDP_TLV_FEC                  0x0100
#define	LDP_TLV_ADDRESS_LIST         0x0101
#define LDP_TLV_ADDRESS_LIST_AFNUM_LEN 2
#define	LDP_TLV_HOP_COUNT            0x0103
#define	LDP_TLV_PATH_VECTOR          0x0104
#define	LDP_TLV_GENERIC_LABEL        0x0200
#define	LDP_TLV_ATM_LABEL            0x0201
#define	LDP_TLV_FR_LABEL             0x0202
#define	LDP_TLV_STATUS               0x0300
#define	LDP_TLV_EXTD_STATUS          0x0301
#define	LDP_TLV_RETURNED_PDU         0x0302
#define	LDP_TLV_RETURNED_MSG         0x0303
#define	LDP_TLV_COMMON_HELLO         0x0400
#define	LDP_TLV_IPV4_TRANSPORT_ADDR  0x0401
#define	LDP_TLV_CONFIG_SEQ_NUMBER    0x0402
#define	LDP_TLV_IPV6_TRANSPORT_ADDR  0x0403
#define	LDP_TLV_COMMON_SESSION       0x0500
#define	LDP_TLV_ATM_SESSION_PARM     0x0501
#define	LDP_TLV_FR_SESSION_PARM      0x0502
#define LDP_TLV_FT_SESSION	     0x0503
#define	LDP_TLV_LABEL_REQUEST_MSG_ID 0x0600
#define LDP_TLV_MTU                  0x0601 /* rfc 3988 */

static const struct tok ldp_tlv_values[] = {
    { LDP_TLV_FEC,	             "FEC" },
    { LDP_TLV_ADDRESS_LIST,          "Address List" },
    { LDP_TLV_HOP_COUNT,             "Hop Count" },
    { LDP_TLV_PATH_VECTOR,           "Path Vector" },
    { LDP_TLV_GENERIC_LABEL,         "Generic Label" },
    { LDP_TLV_ATM_LABEL,             "ATM Label" },
    { LDP_TLV_FR_LABEL,              "Frame-Relay Label" },
    { LDP_TLV_STATUS,                "Status" },
    { LDP_TLV_EXTD_STATUS,           "Extended Status" },
    { LDP_TLV_RETURNED_PDU,          "Returned PDU" },
    { LDP_TLV_RETURNED_MSG,          "Returned Message" },
    { LDP_TLV_COMMON_HELLO,          "Common Hello Parameters" },
    { LDP_TLV_IPV4_TRANSPORT_ADDR,   "IPv4 Transport Address" },
    { LDP_TLV_CONFIG_SEQ_NUMBER,     "Configuration Sequence Number" },
    { LDP_TLV_IPV6_TRANSPORT_ADDR,   "IPv6 Transport Address" },
    { LDP_TLV_COMMON_SESSION,        "Common Session Parameters" },
    { LDP_TLV_ATM_SESSION_PARM,      "ATM Session Parameters" },
    { LDP_TLV_FR_SESSION_PARM,       "Frame-Relay Session Parameters" },
    { LDP_TLV_FT_SESSION,            "Fault-Tolerant Session Parameters" },
    { LDP_TLV_LABEL_REQUEST_MSG_ID,  "Label Request Message ID" },
    { LDP_TLV_MTU,                   "MTU" },
    { 0, NULL}
};

#define LDP_FEC_WILDCARD	0x01
#define LDP_FEC_PREFIX		0x02
#define LDP_FEC_HOSTADDRESS	0x03
/* From RFC 4906; should probably be updated to RFC 4447 (e.g., VC -> PW) */
#define LDP_FEC_MARTINI_VC	0x80

static const struct tok ldp_fec_values[] = {
    { LDP_FEC_WILDCARD,		"Wildcard" },
    { LDP_FEC_PREFIX,		"Prefix" },
    { LDP_FEC_HOSTADDRESS,	"Host address" },
    { LDP_FEC_MARTINI_VC,	"Martini VC" },
    { 0, NULL}
};

#define LDP_FEC_MARTINI_IFPARM_MTU  0x01
#define LDP_FEC_MARTINI_IFPARM_DESC 0x03
#define LDP_FEC_MARTINI_IFPARM_VCCV 0x0c

static const struct tok ldp_fec_martini_ifparm_values[] = {
    { LDP_FEC_MARTINI_IFPARM_MTU, "MTU" },
    { LDP_FEC_MARTINI_IFPARM_DESC, "Description" },
    { LDP_FEC_MARTINI_IFPARM_VCCV, "VCCV" },
    { 0, NULL}
};

/* draft-ietf-pwe3-vccv-04.txt */
static const struct tok ldp_fec_martini_ifparm_vccv_cc_values[] = {
    { 0x01, "PWE3 control word" },
    { 0x02, "MPLS Router Alert Label" },
    { 0x04, "MPLS inner label TTL = 1" },
    { 0, NULL}
};

/* draft-ietf-pwe3-vccv-04.txt */
static const struct tok ldp_fec_martini_ifparm_vccv_cv_values[] = {
    { 0x01, "ICMP Ping" },
    { 0x02, "LSP Ping" },
    { 0x04, "BFD" },
    { 0, NULL}
};

static u_int ldp_pdu_print(netdissect_options *, const u_char *);

/*
 * ldp tlv header
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |U|F|        Type               |            Length             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * |                             Value                             |
 * ~                                                               ~
 * |                                                               |
 * |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

#define TLV_TCHECK(minlen) \
    if (tlv_tlen < minlen) { \
        ND_PRINT(" [tlv length %u < %u]", tlv_tlen, minlen); \
        nd_print_invalid(ndo); \
        goto invalid; \
    }

static u_int
ldp_tlv_print(netdissect_options *ndo,
              const u_char *tptr,
              u_int msg_tlen)
{
    struct ldp_tlv_header {
        nd_uint16_t type;
        nd_uint16_t length;
    };

    const struct ldp_tlv_header *ldp_tlv_header;
    u_short tlv_type,tlv_len,tlv_tlen,af,ft_flags;
    u_char fec_type;
    u_int ui,vc_info_len, vc_info_tlv_type, vc_info_tlv_len,idx;
    char buf[100];
    int i;

    ldp_tlv_header = (const struct ldp_tlv_header *)tptr;
    ND_TCHECK_SIZE(ldp_tlv_header);
    tlv_len=GET_BE_U_2(ldp_tlv_header->length);
    if (tlv_len + 4U > msg_tlen) {
        ND_PRINT("\n\t\t TLV contents go past end of message");
        return 0;
    }
    tlv_tlen=tlv_len;
    tlv_type=LDP_MASK_TLV_TYPE(GET_BE_U_2(ldp_tlv_header->type));

    /* FIXME vendor private / experimental check */
    ND_PRINT("\n\t    %s TLV (0x%04x), length: %u, Flags: [%s and %s forward if unknown]",
           tok2str(ldp_tlv_values,
                   "Unknown",
                   tlv_type),
           tlv_type,
           tlv_len,
           LDP_MASK_U_BIT(GET_BE_U_2(ldp_tlv_header->type)) ? "continue processing" : "ignore",
           LDP_MASK_F_BIT(GET_BE_U_2(ldp_tlv_header->type)) ? "do" : "don't");

    tptr+=sizeof(struct ldp_tlv_header);

    switch(tlv_type) {

    case LDP_TLV_COMMON_HELLO:
        TLV_TCHECK(4);
        ND_PRINT("\n\t      Hold Time: %us, Flags: [%s Hello%s]",
               GET_BE_U_2(tptr),
               (GET_BE_U_2(tptr + 2)&0x8000) ? "Targeted" : "Link",
               (GET_BE_U_2(tptr + 2)&0x4000) ? ", Request for targeted Hellos" : "");
        break;

    case LDP_TLV_IPV4_TRANSPORT_ADDR:
        TLV_TCHECK(4);
        ND_PRINT("\n\t      IPv4 Transport Address: %s", GET_IPADDR_STRING(tptr));
        break;
    case LDP_TLV_IPV6_TRANSPORT_ADDR:
        TLV_TCHECK(16);
        ND_PRINT("\n\t      IPv6 Transport Address: %s", GET_IP6ADDR_STRING(tptr));
        break;
    case LDP_TLV_CONFIG_SEQ_NUMBER:
        TLV_TCHECK(4);
        ND_PRINT("\n\t      Sequence Number: %u", GET_BE_U_4(tptr));
        break;

    case LDP_TLV_ADDRESS_LIST:
        TLV_TCHECK(LDP_TLV_ADDRESS_LIST_AFNUM_LEN);
	af = GET_BE_U_2(tptr);
	tptr+=LDP_TLV_ADDRESS_LIST_AFNUM_LEN;
        tlv_tlen -= LDP_TLV_ADDRESS_LIST_AFNUM_LEN;
	ND_PRINT("\n\t      Address Family: %s, addresses",
               tok2str(af_values, "Unknown (%u)", af));
        switch (af) {
        case AFNUM_INET:
	    while(tlv_tlen >= sizeof(nd_ipv4)) {
		ND_PRINT(" %s", GET_IPADDR_STRING(tptr));
		tlv_tlen-=sizeof(nd_ipv4);
		tptr+=sizeof(nd_ipv4);
	    }
            break;
        case AFNUM_INET6:
	    while(tlv_tlen >= sizeof(nd_ipv6)) {
		ND_PRINT(" %s", GET_IP6ADDR_STRING(tptr));
		tlv_tlen-=sizeof(nd_ipv6);
		tptr+=sizeof(nd_ipv6);
	    }
            break;
        default:
            /* unknown AF */
            break;
        }
	break;

    case LDP_TLV_COMMON_SESSION:
	TLV_TCHECK(14);
	ND_PRINT("\n\t      Version: %u, Keepalive: %us, Flags: [Downstream %s, Loop Detection %s]",
	       GET_BE_U_2(tptr), GET_BE_U_2(tptr + 2),
	       (GET_BE_U_2(tptr + 4)&0x8000) ? "On Demand" : "Unsolicited",
	       (GET_BE_U_2(tptr + 4)&0x4000) ? "Enabled" : "Disabled"
	       );
	ND_PRINT("\n\t      Path Vector Limit %u, Max-PDU length: %u, Receiver Label-Space-ID %s:%u",
	       GET_U_1(tptr+5),
	       GET_BE_U_2(tptr+6),
	       GET_IPADDR_STRING(tptr+8),
	       GET_BE_U_2(tptr+12)
	       );
	break;

    case LDP_TLV_FEC:
        TLV_TCHECK(1);
        fec_type = GET_U_1(tptr);
	ND_PRINT("\n\t      %s FEC (0x%02x)",
	       tok2str(ldp_fec_values, "Unknown", fec_type),
	       fec_type);

	tptr+=1;
	tlv_tlen-=1;
	switch(fec_type) {

	case LDP_FEC_WILDCARD:
	    break;
	case LDP_FEC_PREFIX:
	    TLV_TCHECK(2);
	    af = GET_BE_U_2(tptr);
	    tptr+=2;
	    tlv_tlen-=2;
	    if (af == AFNUM_INET) {
		i=decode_prefix4(ndo, tptr, tlv_tlen, buf, sizeof(buf));
		if (i == -2)
		    goto trunc;
		if (i == -3)
		    ND_PRINT(": IPv4 prefix (goes past end of TLV)");
		else if (i == -1)
		    ND_PRINT(": IPv4 prefix (invalid length)");
		else
		    ND_PRINT(": IPv4 prefix %s", buf);
	    } else if (af == AFNUM_INET6) {
		i=decode_prefix6(ndo, tptr, tlv_tlen, buf, sizeof(buf));
		if (i == -2)
		    goto trunc;
		if (i == -3)
		    ND_PRINT(": IPv4 prefix (goes past end of TLV)");
		else if (i == -1)
		    ND_PRINT(": IPv6 prefix (invalid length)");
		else
		    ND_PRINT(": IPv6 prefix %s", buf);
	    } else
		ND_PRINT(": Address family %u prefix", af);
	    break;
	case LDP_FEC_HOSTADDRESS:
	    break;
	case LDP_FEC_MARTINI_VC:
            /*
             * We assume the type was supposed to be one of the MPLS
             * Pseudowire Types.
             */
            TLV_TCHECK(7);
            vc_info_len = GET_U_1(tptr + 2);

            /*
	     * According to RFC 4908, the VC info Length field can be zero,
	     * in which case not only are there no interface parameters,
	     * there's no VC ID.
	     */
            if (vc_info_len == 0) {
                ND_PRINT(": %s, %scontrol word, group-ID %u, VC-info-length: %u",
                       tok2str(mpls_pw_types_values, "Unknown", GET_BE_U_2(tptr)&0x7fff),
                       GET_BE_U_2(tptr)&0x8000 ? "" : "no ",
                       GET_BE_U_4(tptr + 3),
                       vc_info_len);
                break;
            }

            /* Make sure we have the VC ID as well */
            TLV_TCHECK(11);
	    ND_PRINT(": %s, %scontrol word, group-ID %u, VC-ID %u, VC-info-length: %u",
		   tok2str(mpls_pw_types_values, "Unknown", GET_BE_U_2(tptr)&0x7fff),
		   GET_BE_U_2(tptr)&0x8000 ? "" : "no ",
		   GET_BE_U_4(tptr + 3),
		   GET_BE_U_4(tptr + 7),
		   vc_info_len);
            if (vc_info_len < 4) {
                /* minimum 4, for the VC ID */
                ND_PRINT(" (invalid, < 4");
                return(tlv_len+4); /* Type & Length fields not included */
	    }
            vc_info_len -= 4; /* subtract out the VC ID, giving the length of the interface parameters */

            /* Skip past the fixed information and the VC ID */
            tptr+=11;
            tlv_tlen-=11;
            TLV_TCHECK(vc_info_len);

            while (vc_info_len > 2) {
                vc_info_tlv_type = GET_U_1(tptr);
                vc_info_tlv_len = GET_U_1(tptr + 1);
                if (vc_info_tlv_len < 2)
                    break;
                if (vc_info_len < vc_info_tlv_len)
                    break;

                ND_PRINT("\n\t\tInterface Parameter: %s (0x%02x), len %u",
                       tok2str(ldp_fec_martini_ifparm_values,"Unknown",vc_info_tlv_type),
                       vc_info_tlv_type,
                       vc_info_tlv_len);

                switch(vc_info_tlv_type) {
                case LDP_FEC_MARTINI_IFPARM_MTU:
                    ND_PRINT(": %u", GET_BE_U_2(tptr + 2));
                    break;

                case LDP_FEC_MARTINI_IFPARM_DESC:
                    ND_PRINT(": ");
                    for (idx = 2; idx < vc_info_tlv_len; idx++)
                        fn_print_char(ndo, GET_U_1(tptr + idx));
                    break;

                case LDP_FEC_MARTINI_IFPARM_VCCV:
                    ND_PRINT("\n\t\t  Control Channels (0x%02x) = [%s]",
                           GET_U_1((tptr + 2)),
                           bittok2str(ldp_fec_martini_ifparm_vccv_cc_values, "none", GET_U_1((tptr + 2))));
                    ND_PRINT("\n\t\t  CV Types (0x%02x) = [%s]",
                           GET_U_1((tptr + 3)),
                           bittok2str(ldp_fec_martini_ifparm_vccv_cv_values, "none", GET_U_1((tptr + 3))));
                    break;

                default:
                    print_unknown_data(ndo, tptr+2, "\n\t\t  ", vc_info_tlv_len-2);
                    break;
                }

                vc_info_len -= vc_info_tlv_len;
                tptr += vc_info_tlv_len;
            }
	    break;
	}

	break;

    case LDP_TLV_GENERIC_LABEL:
	TLV_TCHECK(4);
	ND_PRINT("\n\t      Label: %u", GET_BE_U_4(tptr) & 0xfffff);
	break;

    case LDP_TLV_STATUS:
	TLV_TCHECK(8);
	ui = GET_BE_U_4(tptr);
	tptr+=4;
	ND_PRINT("\n\t      Status: 0x%02x, Flags: [%s and %s forward]",
	       ui&0x3fffffff,
	       ui&0x80000000 ? "Fatal error" : "Advisory Notification",
	       ui&0x40000000 ? "do" : "don't");
	ui = GET_BE_U_4(tptr);
	tptr+=4;
	if (ui)
	    ND_PRINT(", causing Message ID: 0x%08x", ui);
	break;

    case LDP_TLV_FT_SESSION:
	TLV_TCHECK(12);
	ft_flags = GET_BE_U_2(tptr);
	ND_PRINT("\n\t      Flags: [%sReconnect, %sSave State, %sAll-Label Protection, %s Checkpoint, %sRe-Learn State]",
	       ft_flags&0x8000 ? "" : "No ",
	       ft_flags&0x8 ? "" : "Don't ",
	       ft_flags&0x4 ? "" : "No ",
	       ft_flags&0x2 ? "Sequence Numbered Label" : "All Labels",
	       ft_flags&0x1 ? "" : "Don't ");
	/* 16 bits (FT Flags) + 16 bits (Reserved) */
	tptr+=4;
	ui = GET_BE_U_4(tptr);
	if (ui)
	    ND_PRINT(", Reconnect Timeout: %ums", ui);
	tptr+=4;
	ui = GET_BE_U_4(tptr);
	if (ui)
	    ND_PRINT(", Recovery Time: %ums", ui);
	break;

    case LDP_TLV_MTU:
	TLV_TCHECK(2);
	ND_PRINT("\n\t      MTU: %u", GET_BE_U_2(tptr));
	break;


    /*
     *  FIXME those are the defined TLVs that lack a decoder
     *  you are welcome to contribute code ;-)
     */

    case LDP_TLV_HOP_COUNT:
    case LDP_TLV_PATH_VECTOR:
    case LDP_TLV_ATM_LABEL:
    case LDP_TLV_FR_LABEL:
    case LDP_TLV_EXTD_STATUS:
    case LDP_TLV_RETURNED_PDU:
    case LDP_TLV_RETURNED_MSG:
    case LDP_TLV_ATM_SESSION_PARM:
    case LDP_TLV_FR_SESSION_PARM:
    case LDP_TLV_LABEL_REQUEST_MSG_ID:

    default:
        if (ndo->ndo_vflag <= 1)
            print_unknown_data(ndo, tptr, "\n\t      ", tlv_tlen);
        break;
    }
    return(tlv_len+4); /* Type & Length fields not included */

trunc:
    nd_trunc_longjmp(ndo);

invalid:
    return(tlv_len+4); /* Type & Length fields not included */
}

void
ldp_print(netdissect_options *ndo,
          const u_char *pptr, u_int len)
{
    u_int processed;

    ndo->ndo_protocol = "ldp";
    while (len > (sizeof(struct ldp_common_header) + sizeof(struct ldp_msg_header))) {
        processed = ldp_pdu_print(ndo, pptr);
        if (processed == 0)
            return;
        if (len < processed) {
            ND_PRINT(" [remaining length %u < %u]", len, processed);
            nd_print_invalid(ndo);
            break;
        }
        len -= processed;
        pptr += processed;
    }
}

static u_int
ldp_pdu_print(netdissect_options *ndo,
              const u_char *pptr)
{
    const struct ldp_common_header *ldp_com_header;
    const struct ldp_msg_header *ldp_msg_header;
    const u_char *tptr,*msg_tptr;
    u_short tlen;
    u_short pdu_len,msg_len,msg_type;
    u_int msg_tlen;
    int hexdump,processed;

    ldp_com_header = (const struct ldp_common_header *)pptr;
    ND_TCHECK_SIZE(ldp_com_header);

    /*
     * Sanity checking of the header.
     */
    if (GET_BE_U_2(ldp_com_header->version) != LDP_VERSION) {
	ND_PRINT("%sLDP version %u packet not supported",
               (ndo->ndo_vflag < 1) ? "" : "\n\t",
               GET_BE_U_2(ldp_com_header->version));
	return 0;
    }

    pdu_len = GET_BE_U_2(ldp_com_header->pdu_length);
    if (pdu_len < sizeof(struct ldp_common_header)-4) {
        /* length too short */
        ND_PRINT("%sLDP, pdu-length: %u (too short, < %zu)",
                 (ndo->ndo_vflag < 1) ? "" : "\n\t",
                 pdu_len,
                 sizeof(struct ldp_common_header)-4);
        return 0;
    }

    /* print the LSR-ID, label-space & length */
    ND_PRINT("%sLDP, Label-Space-ID: %s:%u, pdu-length: %u",
           (ndo->ndo_vflag < 1) ? "" : "\n\t",
           GET_IPADDR_STRING(ldp_com_header->lsr_id),
           GET_BE_U_2(ldp_com_header->label_space),
           pdu_len);

    /* bail out if non-verbose */
    if (ndo->ndo_vflag < 1)
        return 0;

    /* ok they seem to want to know everything - lets fully decode it */
    tptr = pptr + sizeof(struct ldp_common_header);
    tlen = pdu_len - (sizeof(struct ldp_common_header)-4);	/* Type & Length fields not included */

    while(tlen>0) {
        /* did we capture enough for fully decoding the msg header ? */
        ND_TCHECK_LEN(tptr, sizeof(struct ldp_msg_header));

        ldp_msg_header = (const struct ldp_msg_header *)tptr;
        msg_len=GET_BE_U_2(ldp_msg_header->length);
        msg_type=LDP_MASK_MSG_TYPE(GET_BE_U_2(ldp_msg_header->type));

        if (msg_len < sizeof(struct ldp_msg_header)-4) {
            /* length too short */
            /* FIXME vendor private / experimental check */
            ND_PRINT("\n\t  %s Message (0x%04x), length: %u (too short, < %zu)",
                     tok2str(ldp_msg_values,
                             "Unknown",
                             msg_type),
                     msg_type,
                     msg_len,
                     sizeof(struct ldp_msg_header)-4);
            return 0;
        }

        /* FIXME vendor private / experimental check */
        ND_PRINT("\n\t  %s Message (0x%04x), length: %u, Message ID: 0x%08x, Flags: [%s if unknown]",
               tok2str(ldp_msg_values,
                       "Unknown",
                       msg_type),
               msg_type,
               msg_len,
               GET_BE_U_4(ldp_msg_header->id),
               LDP_MASK_U_BIT(GET_BE_U_2(ldp_msg_header->type)) ? "continue processing" : "ignore");

        msg_tptr=tptr+sizeof(struct ldp_msg_header);
        msg_tlen=msg_len-(sizeof(struct ldp_msg_header)-4); /* Type & Length fields not included */

        /* did we capture enough for fully decoding the message ? */
        ND_TCHECK_LEN(tptr, msg_len);
        hexdump=FALSE;

        switch(msg_type) {

        case LDP_MSG_NOTIF:
        case LDP_MSG_HELLO:
        case LDP_MSG_INIT:
        case LDP_MSG_KEEPALIVE:
        case LDP_MSG_ADDRESS:
        case LDP_MSG_LABEL_MAPPING:
        case LDP_MSG_ADDRESS_WITHDRAW:
        case LDP_MSG_LABEL_WITHDRAW:
            while(msg_tlen >= 4) {
                processed = ldp_tlv_print(ndo, msg_tptr, msg_tlen);
                if (processed == 0)
                    break;
                msg_tlen-=processed;
                msg_tptr+=processed;
            }
            break;

        /*
         *  FIXME those are the defined messages that lack a decoder
         *  you are welcome to contribute code ;-)
         */

        case LDP_MSG_LABEL_REQUEST:
        case LDP_MSG_LABEL_RELEASE:
        case LDP_MSG_LABEL_ABORT_REQUEST:

        default:
            if (ndo->ndo_vflag <= 1)
                print_unknown_data(ndo, msg_tptr, "\n\t  ", msg_tlen);
            break;
        }
        /* do we want to see an additionally hexdump ? */
        if (ndo->ndo_vflag > 1 || hexdump==TRUE)
            print_unknown_data(ndo, tptr+sizeof(struct ldp_msg_header), "\n\t  ",
                               msg_len);

        tptr += msg_len+4;
        tlen -= msg_len+4;
    }
    return pdu_len+4;
trunc:
    nd_trunc_longjmp(ndo);
}
