/*
 * Copyright (c) 1991, 1992, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Lawrence Berkeley Laboratory,
 * Berkeley, CA.  The name of the University may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Initial contribution from Jeff Honig (jch@MITCHELL.CIT.CORNELL.EDU).
 */

/* \summary: Exterior Gateway Protocol (EGP) printer */

/* specification: RFC 827 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

struct egp_packet {
	nd_uint8_t  egp_version;
#define	EGP_VERSION	2
	nd_uint8_t  egp_type;
#define  EGPT_ACQUIRE	3
#define  EGPT_REACH	5
#define  EGPT_POLL	2
#define  EGPT_UPDATE	1
#define  EGPT_ERROR	8
	nd_uint8_t  egp_code;
#define  EGPC_REQUEST	0
#define  EGPC_CONFIRM	1
#define  EGPC_REFUSE	2
#define  EGPC_CEASE	3
#define  EGPC_CEASEACK	4
#define  EGPC_HELLO	0
#define  EGPC_HEARDU	1
	nd_uint8_t  egp_status;
#define  EGPS_UNSPEC	0
#define  EGPS_ACTIVE	1
#define  EGPS_PASSIVE	2
#define  EGPS_NORES	3
#define  EGPS_ADMIN	4
#define  EGPS_GODOWN	5
#define  EGPS_PARAM	6
#define  EGPS_PROTO	7
#define  EGPS_INDET	0
#define  EGPS_UP	1
#define  EGPS_DOWN	2
#define  EGPS_UNSOL	0x80
	nd_uint16_t  egp_checksum;
	nd_uint16_t  egp_as;
	nd_uint16_t  egp_sequence;
	union {
		nd_uint16_t egpu_hello;
		nd_uint8_t  egpu_gws[2];
		nd_uint16_t egpu_reason;
#define  EGPR_UNSPEC	0
#define  EGPR_BADHEAD	1
#define  EGPR_BADDATA	2
#define  EGPR_NOREACH	3
#define  EGPR_XSPOLL	4
#define  EGPR_NORESP	5
#define  EGPR_UVERSION	6
	} egp_handg;
#define  egp_hello  egp_handg.egpu_hello
#define  egp_intgw  egp_handg.egpu_gws[0]
#define  egp_extgw  egp_handg.egpu_gws[1]
#define  egp_reason  egp_handg.egpu_reason
	union {
		nd_uint16_t egpu_poll;
		nd_ipv4 egpu_sourcenet;
	} egp_pands;
#define  egp_poll  egp_pands.egpu_poll
#define  egp_sourcenet  egp_pands.egpu_sourcenet
};

static const char *egp_acquire_codes[] = {
	"request",
	"confirm",
	"refuse",
	"cease",
	"cease_ack"
};

static const char *egp_acquire_status[] = {
	"unspecified",
	"active_mode",
	"passive_mode",
	"insufficient_resources",
	"administratively_prohibited",
	"going_down",
	"parameter_violation",
	"protocol_violation"
};

static const char *egp_reach_codes[] = {
	"hello",
	"i-h-u"
};

static const char *egp_status_updown[] = {
	"indeterminate",
	"up",
	"down"
};

static const char *egp_reasons[] = {
	"unspecified",
	"bad_EGP_header_format",
	"bad_EGP_data_field_format",
	"reachability_info_unavailable",
	"excessive_polling_rate",
	"no_response",
	"unsupported_version"
};

static void
egpnr_print(netdissect_options *ndo,
           const struct egp_packet *egp, u_int length)
{
	const uint8_t *cp;
	uint32_t addr;
	uint32_t net;
	u_int netlen;
	u_int gateways, distances, networks;
	u_int intgw, extgw, t_gateways;
	const char *comma;

	addr = GET_IPV4_TO_NETWORK_ORDER(egp->egp_sourcenet);
	if (IN_CLASSA(addr)) {
		net = addr & IN_CLASSA_NET;
		netlen = 1;
	} else if (IN_CLASSB(addr)) {
		net = addr & IN_CLASSB_NET;
		netlen = 2;
	} else if (IN_CLASSC(addr)) {
		net = addr & IN_CLASSC_NET;
		netlen = 3;
	} else {
		net = 0;
		netlen = 0;
	}
	cp = (const uint8_t *)(egp + 1);
	length -= sizeof(*egp);

	intgw = GET_U_1(egp->egp_intgw);
	extgw = GET_U_1(egp->egp_extgw);
	t_gateways = intgw + extgw;
	for (gateways = 0; gateways < t_gateways; ++gateways) {
		/* Pickup host part of gateway address */
		addr = 0;
		if (length < 4 - netlen)
			goto trunc;
		ND_TCHECK_LEN(cp, 4 - netlen);
		switch (netlen) {

		case 1:
			addr = GET_U_1(cp);
			cp++;
			/* fall through */
		case 2:
			addr = (addr << 8) | GET_U_1(cp);
			cp++;
			/* fall through */
		case 3:
			addr = (addr << 8) | GET_U_1(cp);
			cp++;
			break;
		}
		addr |= net;
		length -= 4 - netlen;
		if (length < 1)
			goto trunc;
		distances = GET_U_1(cp);
		cp++;
		length--;
		ND_PRINT(" %s %s ",
		       gateways < intgw ? "int" : "ext",
		       ipaddr_string(ndo, (const u_char *)&addr)); /* local buffer, not packet data; don't use GET_IPADDR_STRING() */

		comma = "";
		ND_PRINT("(");
		while (distances != 0) {
			if (length < 2)
				goto trunc;
			ND_PRINT("%sd%u:", comma, GET_U_1(cp));
			cp++;
			comma = ", ";
			networks = GET_U_1(cp);
			cp++;
			length -= 2;
			while (networks != 0) {
				/* Pickup network number */
				if (length < 1)
					goto trunc;
				addr = ((uint32_t) GET_U_1(cp)) << 24;
				cp++;
				length--;
				if (IN_CLASSB(addr)) {
					if (length < 1)
						goto trunc;
					addr |= ((uint32_t) GET_U_1(cp)) << 16;
					cp++;
					length--;
				} else if (!IN_CLASSA(addr)) {
					if (length < 2)
						goto trunc;
					addr |= ((uint32_t) GET_U_1(cp)) << 16;
					cp++;
					addr |= ((uint32_t) GET_U_1(cp)) << 8;
					cp++;
					length -= 2;
				}
				ND_PRINT(" %s", ipaddr_string(ndo, (const u_char *)&addr)); /* local buffer, not packet data; don't use GET_IPADDR_STRING() */
				networks--;
			}
			distances--;
		}
		ND_PRINT(")");
	}
	return;
trunc:
	nd_print_trunc(ndo);
}

void
egp_print(netdissect_options *ndo,
          const uint8_t *bp, u_int length)
{
	const struct egp_packet *egp;
	u_int version;
	u_int type;
	u_int code;
	u_int status;

	ndo->ndo_protocol = "egp";
	egp = (const struct egp_packet *)bp;
	if (length < sizeof(*egp) || !ND_TTEST_SIZE(egp)) {
		nd_print_trunc(ndo);
		return;
	}

	version = GET_U_1(egp->egp_version);
        if (!ndo->ndo_vflag) {
            ND_PRINT("EGPv%u, AS %u, seq %u, length %u",
                   version,
                   GET_BE_U_2(egp->egp_as),
                   GET_BE_U_2(egp->egp_sequence),
                   length);
            return;
        } else
            ND_PRINT("EGPv%u, length %u",
                   version,
                   length);

	if (version != EGP_VERSION) {
		ND_PRINT("[version %u]", version);
		return;
	}

	type = GET_U_1(egp->egp_type);
	code = GET_U_1(egp->egp_code);
	status = GET_U_1(egp->egp_status);

	switch (type) {
	case EGPT_ACQUIRE:
		ND_PRINT(" acquire");
		switch (code) {
		case EGPC_REQUEST:
		case EGPC_CONFIRM:
			ND_PRINT(" %s", egp_acquire_codes[code]);
			switch (status) {
			case EGPS_UNSPEC:
			case EGPS_ACTIVE:
			case EGPS_PASSIVE:
				ND_PRINT(" %s", egp_acquire_status[status]);
				break;

			default:
				ND_PRINT(" [status %u]", status);
				break;
			}
			ND_PRINT(" hello:%u poll:%u",
			       GET_BE_U_2(egp->egp_hello),
			       GET_BE_U_2(egp->egp_poll));
			break;

		case EGPC_REFUSE:
		case EGPC_CEASE:
		case EGPC_CEASEACK:
			ND_PRINT(" %s", egp_acquire_codes[code]);
			switch (status ) {
			case EGPS_UNSPEC:
			case EGPS_NORES:
			case EGPS_ADMIN:
			case EGPS_GODOWN:
			case EGPS_PARAM:
			case EGPS_PROTO:
				ND_PRINT(" %s", egp_acquire_status[status]);
				break;

			default:
				ND_PRINT("[status %u]", status);
				break;
			}
			break;

		default:
			ND_PRINT("[code %u]", code);
			break;
		}
		break;

	case EGPT_REACH:
		switch (code) {

		case EGPC_HELLO:
		case EGPC_HEARDU:
			ND_PRINT(" %s", egp_reach_codes[code]);
			if (status <= EGPS_DOWN)
				ND_PRINT(" state:%s", egp_status_updown[status]);
			else
				ND_PRINT(" [status %u]", status);
			break;

		default:
			ND_PRINT("[reach code %u]", code);
			break;
		}
		break;

	case EGPT_POLL:
		ND_PRINT(" poll");
		if (status <= EGPS_DOWN)
			ND_PRINT(" state:%s", egp_status_updown[status]);
		else
			ND_PRINT(" [status %u]", status);
		ND_PRINT(" net:%s", GET_IPADDR_STRING(egp->egp_sourcenet));
		break;

	case EGPT_UPDATE:
		ND_PRINT(" update");
		if (status & EGPS_UNSOL) {
			status &= ~EGPS_UNSOL;
			ND_PRINT(" unsolicited");
		}
		if (status <= EGPS_DOWN)
			ND_PRINT(" state:%s", egp_status_updown[status]);
		else
			ND_PRINT(" [status %u]", status);
		ND_PRINT(" %s int %u ext %u",
		       GET_IPADDR_STRING(egp->egp_sourcenet),
		       GET_U_1(egp->egp_intgw),
		       GET_U_1(egp->egp_extgw));
		if (ndo->ndo_vflag)
			egpnr_print(ndo, egp, length);
		break;

	case EGPT_ERROR:
		ND_PRINT(" error");
		if (status <= EGPS_DOWN)
			ND_PRINT(" state:%s", egp_status_updown[status]);
		else
			ND_PRINT(" [status %u]", status);

		if (GET_BE_U_2(egp->egp_reason) <= EGPR_UVERSION)
			ND_PRINT(" %s",
				 egp_reasons[GET_BE_U_2(egp->egp_reason)]);
		else
			ND_PRINT(" [reason %u]", GET_BE_U_2(egp->egp_reason));
		break;

	default:
		ND_PRINT("[type %u]", type);
		break;
	}
}
