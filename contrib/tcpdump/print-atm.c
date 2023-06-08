/*
 * Copyright (c) 1994, 1995, 1996, 1997
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
 */

/* \summary: Asynchronous Transfer Mode (ATM) printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"
#include "atm.h"
#include "llc.h"

/* start of the original atmuni31.h */

/*
 * Copyright (c) 1997 Yen Yen Lim and North Dakota State University
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Yen Yen Lim and
        North Dakota State University
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Based on UNI3.1 standard by ATM Forum */

/* ATM traffic types based on VPI=0 and (the following VCI */
#define VCI_PPC			0x05	/* Point-to-point signal msg */
#define VCI_BCC			0x02	/* Broadcast signal msg */
#define VCI_OAMF4SC		0x03	/* Segment OAM F4 flow cell */
#define VCI_OAMF4EC		0x04	/* End-to-end OAM F4 flow cell */
#define VCI_METAC		0x01	/* Meta signal msg */
#define VCI_ILMIC		0x10	/* ILMI msg */

/* Q.2931 signalling messages */
#define CALL_PROCEED		0x02	/* call proceeding */
#define CONNECT			0x07	/* connect */
#define CONNECT_ACK		0x0f	/* connect_ack */
#define SETUP			0x05	/* setup */
#define RELEASE			0x4d	/* release */
#define RELEASE_DONE		0x5a	/* release_done */
#define RESTART			0x46	/* restart */
#define RESTART_ACK		0x4e	/* restart ack */
#define STATUS			0x7d	/* status */
#define STATUS_ENQ		0x75	/* status ack */
#define ADD_PARTY		0x80	/* add party */
#define ADD_PARTY_ACK		0x81	/* add party ack */
#define ADD_PARTY_REJ		0x82	/* add party rej */
#define DROP_PARTY		0x83	/* drop party */
#define DROP_PARTY_ACK		0x84	/* drop party ack */

/* Information Element Parameters in the signalling messages */
#define CAUSE			0x08	/* cause */
#define ENDPT_REF		0x54	/* endpoint reference */
#define AAL_PARA		0x58	/* ATM adaptation layer parameters */
#define TRAFF_DESCRIP		0x59	/* atm traffic descriptors */
#define CONNECT_ID		0x5a	/* connection identifier */
#define QOS_PARA		0x5c	/* quality of service parameters */
#define B_HIGHER		0x5d	/* broadband higher layer information */
#define B_BEARER		0x5e	/* broadband bearer capability */
#define B_LOWER			0x5f	/* broadband lower information */
#define CALLING_PARTY		0x6c	/* calling party number */
#define CALLED_PARTY		0x70	/* called party nmber */

#define Q2931			0x09

/* Q.2931 signalling general messages format */
#define PROTO_POS       0	/* offset of protocol discriminator */
#define CALL_REF_POS    2	/* offset of call reference value */
#define MSG_TYPE_POS    5	/* offset of message type */
#if 0
#define MSG_LEN_POS     7	/* offset of message length */
#define IE_BEGIN_POS    9	/* offset of first information element */

/* format of signalling messages */
#define TYPE_POS	0
#define LEN_POS		2
#define FIELD_BEGIN_POS 4
#endif

/* end of the original atmuni31.h */


#define OAM_CRC10_MASK 0x3ff
#define OAM_PAYLOAD_LEN 48
#define OAM_FUNCTION_SPECIFIC_LEN 45 /* this excludes crc10 and cell-type/function-type */
#define OAM_CELLTYPE_FUNCTYPE_LEN 1

static const struct tok oam_f_values[] = {
    { VCI_OAMF4SC, "OAM F4 (segment)" },
    { VCI_OAMF4EC, "OAM F4 (end)" },
    { 0, NULL }
};

static const struct tok atm_pty_values[] = {
    { 0x0, "user data, uncongested, SDU 0" },
    { 0x1, "user data, uncongested, SDU 1" },
    { 0x2, "user data, congested, SDU 0" },
    { 0x3, "user data, congested, SDU 1" },
    { 0x4, "VCC OAM F5 flow segment" },
    { 0x5, "VCC OAM F5 flow end-to-end" },
    { 0x6, "Traffic Control and resource Mgmt" },
    { 0, NULL }
};

#define OAM_CELLTYPE_FM 0x1
#define OAM_CELLTYPE_PM 0x2
#define OAM_CELLTYPE_AD 0x8
#define OAM_CELLTYPE_SM 0xf

static const struct tok oam_celltype_values[] = {
    { OAM_CELLTYPE_FM, "Fault Management" },
    { OAM_CELLTYPE_PM, "Performance Management" },
    { OAM_CELLTYPE_AD, "activate/deactivate" },
    { OAM_CELLTYPE_SM, "System Management" },
    { 0, NULL }
};

#define OAM_FM_FUNCTYPE_AIS       0x0
#define OAM_FM_FUNCTYPE_RDI       0x1
#define OAM_FM_FUNCTYPE_CONTCHECK 0x4
#define OAM_FM_FUNCTYPE_LOOPBACK  0x8

static const struct tok oam_fm_functype_values[] = {
    { OAM_FM_FUNCTYPE_AIS, "AIS" },
    { OAM_FM_FUNCTYPE_RDI, "RDI" },
    { OAM_FM_FUNCTYPE_CONTCHECK, "Continuity Check" },
    { OAM_FM_FUNCTYPE_LOOPBACK, "Loopback" },
    { 0, NULL }
};

static const struct tok oam_pm_functype_values[] = {
    { 0x0, "Forward Monitoring" },
    { 0x1, "Backward Reporting" },
    { 0x2, "Monitoring and Reporting" },
    { 0, NULL }
};

static const struct tok oam_ad_functype_values[] = {
    { 0x0, "Performance Monitoring" },
    { 0x1, "Continuity Check" },
    { 0, NULL }
};

#define OAM_FM_LOOPBACK_INDICATOR_MASK 0x1

static const struct tok oam_fm_loopback_indicator_values[] = {
    { 0x0, "Reply" },
    { 0x1, "Request" },
    { 0, NULL }
};

static const struct uint_tokary oam_celltype2tokary[] = {
	{ OAM_CELLTYPE_FM, oam_fm_functype_values },
	{ OAM_CELLTYPE_PM, oam_pm_functype_values },
	{ OAM_CELLTYPE_AD, oam_ad_functype_values },
	/* uint2tokary() does not use array termination. */
};

/*
 * Print an RFC 1483 LLC-encapsulated ATM frame.
 */
static u_int
atm_llc_print(netdissect_options *ndo,
              const u_char *p, int length, int caplen)
{
	int llc_hdrlen;

	llc_hdrlen = llc_print(ndo, p, length, caplen, NULL, NULL);
	if (llc_hdrlen < 0) {
		/* packet not known, print raw packet */
		if (!ndo->ndo_suppress_default_print)
			ND_DEFAULTPRINT(p, caplen);
		llc_hdrlen = -llc_hdrlen;
	}
	return (llc_hdrlen);
}

/*
 * Given a SAP value, generate the LLC header value for a UI packet
 * with that SAP as the source and destination SAP.
 */
#define LLC_UI_HDR(sap)	((sap)<<16 | (sap<<8) | 0x03)

/*
 * This is the top level routine of the printer.  'p' points
 * to the LLC/SNAP header of the packet, 'h->ts' is the timestamp,
 * 'h->len' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
void
atm_if_print(netdissect_options *ndo,
             const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;
	uint32_t llchdr;
	u_int hdrlen = 0;

	ndo->ndo_protocol = "atm";

        /* Cisco Style NLPID ? */
        if (GET_U_1(p) == LLC_UI) {
            if (ndo->ndo_eflag)
                ND_PRINT("CNLPID ");
            ndo->ndo_ll_hdr_len += 1;
            isoclns_print(ndo, p + 1, length - 1);
            return;
        }

	/*
	 * Must have at least a DSAP, an SSAP, and the first byte of the
	 * control field.
	 */

	/*
	 * Extract the presumed LLC header into a variable, for quick
	 * testing.
	 * Then check for a header that's neither a header for a SNAP
	 * packet nor an RFC 2684 routed NLPID-formatted PDU nor
	 * an 802.2-but-no-SNAP IP packet.
	 */
	llchdr = GET_BE_U_3(p);
	if (llchdr != LLC_UI_HDR(LLCSAP_SNAP) &&
	    llchdr != LLC_UI_HDR(LLCSAP_ISONS) &&
	    llchdr != LLC_UI_HDR(LLCSAP_IP)) {
		/*
		 * XXX - assume 802.6 MAC header from Fore driver.
		 *
		 * Unfortunately, the above list doesn't check for
		 * all known SAPs, doesn't check for headers where
		 * the source and destination SAP aren't the same,
		 * and doesn't check for non-UI frames.  It also
		 * runs the risk of an 802.6 MAC header that happens
		 * to begin with one of those values being
		 * incorrectly treated as an 802.2 header.
		 *
		 * So is that Fore driver still around?  And, if so,
		 * is it still putting 802.6 MAC headers on ATM
		 * packets?  If so, could it be changed to use a
		 * new DLT_IEEE802_6 value if we added it?
		 */
		if (ndo->ndo_eflag)
			ND_PRINT("%08x%08x %08x%08x ",
			       GET_BE_U_4(p),
			       GET_BE_U_4(p + 4),
			       GET_BE_U_4(p + 8),
			       GET_BE_U_4(p + 12));
		/* Always cover the full header. */
		ND_TCHECK_LEN(p, 20);
		p += 20;
		length -= 20;
		caplen -= 20;
		hdrlen += 20;
	}
	ndo->ndo_ll_hdr_len += hdrlen;
	ndo->ndo_ll_hdr_len += atm_llc_print(ndo, p, length, caplen);
}

/*
 * ATM signalling.
 */
static const struct tok msgtype2str[] = {
	{ CALL_PROCEED,		"Call_proceeding" },
	{ CONNECT,		"Connect" },
	{ CONNECT_ACK,		"Connect_ack" },
	{ SETUP,		"Setup" },
	{ RELEASE,		"Release" },
	{ RELEASE_DONE,		"Release_complete" },
	{ RESTART,		"Restart" },
	{ RESTART_ACK,		"Restart_ack" },
	{ STATUS,		"Status" },
	{ STATUS_ENQ,		"Status_enquiry" },
	{ ADD_PARTY,		"Add_party" },
	{ ADD_PARTY_ACK,	"Add_party_ack" },
	{ ADD_PARTY_REJ,	"Add_party_reject" },
	{ DROP_PARTY,		"Drop_party" },
	{ DROP_PARTY_ACK,	"Drop_party_ack" },
	{ 0,			NULL }
};

static void
sig_print(netdissect_options *ndo,
          const u_char *p)
{
	uint32_t call_ref;

	if (GET_U_1(p + PROTO_POS) == Q2931) {
		/*
		 * protocol:Q.2931 for User to Network Interface
		 * (UNI 3.1) signalling
		 */
		ND_PRINT("Q.2931");
		ND_PRINT(":%s ",
		    tok2str(msgtype2str, "msgtype#%u", GET_U_1(p + MSG_TYPE_POS)));

		/*
		 * The call reference comes before the message type,
		 * so if we know we have the message type, which we
		 * do from the caplen test above, we also know we have
		 * the call reference.
		 */
		call_ref = GET_BE_U_3(p + CALL_REF_POS);
		ND_PRINT("CALL_REF:0x%06x", call_ref);
	} else {
		/* SSCOP with some unknown protocol atop it */
		ND_PRINT("SSCOP, proto %u ", GET_U_1(p + PROTO_POS));
	}
}

/*
 * Print an ATM PDU (such as an AAL5 PDU).
 */
void
atm_print(netdissect_options *ndo,
          u_int vpi, u_int vci, u_int traftype, const u_char *p, u_int length,
          u_int caplen)
{
	ndo->ndo_protocol = "atm";
	if (ndo->ndo_eflag)
		ND_PRINT("VPI:%u VCI:%u ", vpi, vci);

	if (vpi == 0) {
		switch (vci) {

		case VCI_PPC:
			sig_print(ndo, p);
			return;

		case VCI_BCC:
			ND_PRINT("broadcast sig: ");
			return;

		case VCI_OAMF4SC: /* fall through */
		case VCI_OAMF4EC:
			oam_print(ndo, p, length, ATM_OAM_HEC);
			return;

		case VCI_METAC:
			ND_PRINT("meta: ");
			return;

		case VCI_ILMIC:
			ND_PRINT("ilmi: ");
			snmp_print(ndo, p, length);
			return;
		}
	}

	switch (traftype) {

	case ATM_LLC:
	default:
		/*
		 * Assumes traffic is LLC if unknown.
		 */
		atm_llc_print(ndo, p, length, caplen);
		break;

	case ATM_LANE:
		lane_print(ndo, p, length, caplen);
		break;
	}
}

struct oam_fm_loopback_t {
    nd_uint8_t  loopback_indicator;
    nd_uint32_t correlation_tag;
    nd_byte     loopback_id[12];
    nd_byte     source_id[12];
    nd_byte     unused[16];
};

struct oam_fm_ais_rdi_t {
    nd_uint8_t  failure_type;
    nd_byte     failure_location[16];
    nd_byte     unused[28];
};

void
oam_print(netdissect_options *ndo,
          const u_char *p, u_int length, u_int hec)
{
    uint32_t cell_header;
    uint16_t vpi, vci, cksum, cksum_shouldbe, idx;
    uint8_t  cell_type, func_type, payload, clp;
    const struct tok *oam_functype_str;

    union {
        const struct oam_fm_loopback_t *oam_fm_loopback;
        const struct oam_fm_ais_rdi_t *oam_fm_ais_rdi;
    } oam_ptr;

    ndo->ndo_protocol = "oam";
    cell_header = GET_BE_U_4(p + hec);
    cell_type = (GET_U_1((p + ATM_HDR_LEN_NOHEC + hec)) >> 4) & 0x0f;
    func_type = GET_U_1((p + ATM_HDR_LEN_NOHEC + hec)) & 0x0f;

    vpi = (cell_header>>20)&0xff;
    vci = (cell_header>>4)&0xffff;
    payload = (cell_header>>1)&0x7;
    clp = cell_header&0x1;

    ND_PRINT("%s, vpi %u, vci %u, payload [ %s ], clp %u, length %u",
           tok2str(oam_f_values, "OAM F5", vci),
           vpi, vci,
           tok2str(atm_pty_values, "Unknown", payload),
           clp, length);

    if (!ndo->ndo_vflag) {
        return;
    }

    ND_PRINT("\n\tcell-type %s (%u)",
           tok2str(oam_celltype_values, "unknown", cell_type),
           cell_type);

    oam_functype_str = uint2tokary(oam_celltype2tokary, cell_type);
    if (oam_functype_str == NULL)
        ND_PRINT(", func-type unknown (%u)", func_type);
    else
        ND_PRINT(", func-type %s (%u)",
               tok2str(oam_functype_str, "none", func_type),
               func_type);

    p += ATM_HDR_LEN_NOHEC + hec;

    switch (cell_type << 4 | func_type) {
    case (OAM_CELLTYPE_FM << 4 | OAM_FM_FUNCTYPE_LOOPBACK):
        oam_ptr.oam_fm_loopback = (const struct oam_fm_loopback_t *)(p + OAM_CELLTYPE_FUNCTYPE_LEN);
        ND_TCHECK_SIZE(oam_ptr.oam_fm_loopback);
        ND_PRINT("\n\tLoopback-Indicator %s, Correlation-Tag 0x%08x",
               tok2str(oam_fm_loopback_indicator_values,
                       "Unknown",
                       GET_U_1(oam_ptr.oam_fm_loopback->loopback_indicator) & OAM_FM_LOOPBACK_INDICATOR_MASK),
               GET_BE_U_4(oam_ptr.oam_fm_loopback->correlation_tag));
        ND_PRINT("\n\tLocation-ID ");
        for (idx = 0; idx < sizeof(oam_ptr.oam_fm_loopback->loopback_id); idx++) {
            if (idx % 2) {
                ND_PRINT("%04x ",
                         GET_BE_U_2(&oam_ptr.oam_fm_loopback->loopback_id[idx]));
            }
        }
        ND_PRINT("\n\tSource-ID   ");
        for (idx = 0; idx < sizeof(oam_ptr.oam_fm_loopback->source_id); idx++) {
            if (idx % 2) {
                ND_PRINT("%04x ",
                         GET_BE_U_2(&oam_ptr.oam_fm_loopback->source_id[idx]));
            }
        }
        break;

    case (OAM_CELLTYPE_FM << 4 | OAM_FM_FUNCTYPE_AIS):
    case (OAM_CELLTYPE_FM << 4 | OAM_FM_FUNCTYPE_RDI):
        oam_ptr.oam_fm_ais_rdi = (const struct oam_fm_ais_rdi_t *)(p + OAM_CELLTYPE_FUNCTYPE_LEN);
        ND_TCHECK_SIZE(oam_ptr.oam_fm_ais_rdi);
        ND_PRINT("\n\tFailure-type 0x%02x",
                 GET_U_1(oam_ptr.oam_fm_ais_rdi->failure_type));
        ND_PRINT("\n\tLocation-ID ");
        for (idx = 0; idx < sizeof(oam_ptr.oam_fm_ais_rdi->failure_location); idx++) {
            if (idx % 2) {
                ND_PRINT("%04x ",
                         GET_BE_U_2(&oam_ptr.oam_fm_ais_rdi->failure_location[idx]));
            }
        }
        break;

    case (OAM_CELLTYPE_FM << 4 | OAM_FM_FUNCTYPE_CONTCHECK):
        /* FIXME */
        break;

    default:
        break;
    }

    /* crc10 checksum verification */
    cksum = GET_BE_U_2(p + OAM_CELLTYPE_FUNCTYPE_LEN + OAM_FUNCTION_SPECIFIC_LEN)
        & OAM_CRC10_MASK;
    cksum_shouldbe = verify_crc10_cksum(0, p, OAM_PAYLOAD_LEN);

    ND_PRINT("\n\tcksum 0x%03x (%scorrect)",
           cksum,
           cksum_shouldbe == 0 ? "" : "in");
}
