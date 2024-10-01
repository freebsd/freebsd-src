/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
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

/* \summary: Address Resolution Protocol (ARP) printer */

#include <config.h>

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "addrtoname.h"
#include "ethertype.h"
#include "extract.h"


/*
 * Address Resolution Protocol.
 *
 * See RFC 826 for protocol description.  ARP packets are variable
 * in size; the arphdr structure defines the fixed-length portion.
 * Protocol type values are the same as those for 10 Mb/s Ethernet.
 * It is followed by the variable-sized fields ar_sha, arp_spa,
 * arp_tha and arp_tpa in that order, according to the lengths
 * specified.  Field names used correspond to RFC 826.
 */
struct  arp_pkthdr {
        nd_uint16_t ar_hrd;     /* format of hardware address */
#define ARPHRD_ETHER    1       /* ethernet hardware format */
#define ARPHRD_IEEE802  6       /* token-ring hardware format */
#define ARPHRD_ARCNET   7       /* arcnet hardware format */
#define ARPHRD_FRELAY   15      /* frame relay hardware format */
#define ARPHRD_ATM2225  19      /* ATM (RFC 2225) */
#define ARPHRD_STRIP    23      /* Ricochet Starmode Radio hardware format */
#define ARPHRD_IEEE1394 24      /* IEEE 1394 (FireWire) hardware format */
#define ARPHRD_INFINIBAND 32    /* InfiniBand RFC 4391 */
        nd_uint16_t ar_pro;     /* format of protocol address */
        nd_uint8_t  ar_hln;     /* length of hardware address */
        nd_uint8_t  ar_pln;     /* length of protocol address */
        nd_uint16_t ar_op;      /* one of: */
#define ARPOP_REQUEST   1       /* request to resolve address */
#define ARPOP_REPLY     2       /* response to previous request */
#define ARPOP_REVREQUEST 3      /* request protocol address given hardware */
#define ARPOP_REVREPLY  4       /* response giving protocol address */
#define ARPOP_INVREQUEST 8      /* request to identify peer */
#define ARPOP_INVREPLY  9       /* response identifying peer */
#define ARPOP_NAK       10      /* NAK - only valid for ATM ARP */

/*
 * The remaining fields are variable in size,
 * according to the sizes above.
 */
#ifdef COMMENT_ONLY
	nd_byte		ar_sha[];	/* sender hardware address */
	nd_byte		ar_spa[];	/* sender protocol address */
	nd_byte		ar_tha[];	/* target hardware address */
	nd_byte		ar_tpa[];	/* target protocol address */
#endif
#define ar_sha(ap)	(((const u_char *)((ap)+1))+  0)
#define ar_spa(ap)	(((const u_char *)((ap)+1))+  GET_U_1((ap)->ar_hln))
#define ar_tha(ap)	(((const u_char *)((ap)+1))+  GET_U_1((ap)->ar_hln)+GET_U_1((ap)->ar_pln))
#define ar_tpa(ap)	(((const u_char *)((ap)+1))+2*GET_U_1((ap)->ar_hln)+GET_U_1((ap)->ar_pln))
};

#define ARP_HDRLEN	8

#define HRD(ap) GET_BE_U_2((ap)->ar_hrd)
#define HRD_LEN(ap) GET_U_1((ap)->ar_hln)
#define PROTO_LEN(ap) GET_U_1((ap)->ar_pln)
#define OP(ap)  GET_BE_U_2((ap)->ar_op)
#define PRO(ap) GET_BE_U_2((ap)->ar_pro)
#define SHA(ap) (ar_sha(ap))
#define SPA(ap) (ar_spa(ap))
#define THA(ap) (ar_tha(ap))
#define TPA(ap) (ar_tpa(ap))


static const struct tok arpop_values[] = {
    { ARPOP_REQUEST, "Request" },
    { ARPOP_REPLY, "Reply" },
    { ARPOP_REVREQUEST, "Reverse Request" },
    { ARPOP_REVREPLY, "Reverse Reply" },
    { ARPOP_INVREQUEST, "Inverse Request" },
    { ARPOP_INVREPLY, "Inverse Reply" },
    { ARPOP_NAK, "NACK Reply" },
    { 0, NULL }
};

static const struct tok arphrd_values[] = {
    { ARPHRD_ETHER, "Ethernet" },
    { ARPHRD_IEEE802, "TokenRing" },
    { ARPHRD_ARCNET, "ArcNet" },
    { ARPHRD_FRELAY, "FrameRelay" },
    { ARPHRD_STRIP, "Strip" },
    { ARPHRD_IEEE1394, "IEEE 1394" },
    { ARPHRD_ATM2225, "ATM" },
    { ARPHRD_INFINIBAND, "InfiniBand" },
    { 0, NULL }
};

/*
 * ATM Address Resolution Protocol.
 *
 * See RFC 2225 for protocol description.  ATMARP packets are similar
 * to ARP packets, except that there are no length fields for the
 * protocol address - instead, there are type/length fields for
 * the ATM number and subaddress - and the hardware addresses consist
 * of an ATM number and an ATM subaddress.
 */
struct  atmarp_pkthdr {
        nd_uint16_t aar_hrd;    /* format of hardware address */
        nd_uint16_t aar_pro;    /* format of protocol address */
        nd_uint8_t  aar_shtl;   /* length of source ATM number */
        nd_uint8_t  aar_sstl;   /* length of source ATM subaddress */
#define ATMARP_IS_E164  0x40    /* bit in type/length for E.164 format */
#define ATMARP_LEN_MASK 0x3F    /* length of {sub}address in type/length */
        nd_uint16_t aar_op;     /* same as regular ARP */
        nd_uint8_t  aar_spln;   /* length of source protocol address */
        nd_uint8_t  aar_thtl;   /* length of target ATM number */
        nd_uint8_t  aar_tstl;   /* length of target ATM subaddress */
        nd_uint8_t  aar_tpln;   /* length of target protocol address */
/*
 * The remaining fields are variable in size,
 * according to the sizes above.
 */
#ifdef COMMENT_ONLY
	nd_byte		aar_sha[];	/* source ATM number */
	nd_byte		aar_ssa[];	/* source ATM subaddress */
	nd_byte		aar_spa[];	/* sender protocol address */
	nd_byte		aar_tha[];	/* target ATM number */
	nd_byte		aar_tsa[];	/* target ATM subaddress */
	nd_byte		aar_tpa[];	/* target protocol address */
#endif

#define ATMHRD(ap)  GET_BE_U_2((ap)->aar_hrd)
#define ATMSHRD_LEN(ap) (GET_U_1((ap)->aar_shtl) & ATMARP_LEN_MASK)
#define ATMSSLN(ap) (GET_U_1((ap)->aar_sstl) & ATMARP_LEN_MASK)
#define ATMSPROTO_LEN(ap) GET_U_1((ap)->aar_spln)
#define ATMOP(ap)   GET_BE_U_2((ap)->aar_op)
#define ATMPRO(ap)  GET_BE_U_2((ap)->aar_pro)
#define ATMTHRD_LEN(ap) (GET_U_1((ap)->aar_thtl) & ATMARP_LEN_MASK)
#define ATMTSLN(ap) (GET_U_1((ap)->aar_tstl) & ATMARP_LEN_MASK)
#define ATMTPROTO_LEN(ap) GET_U_1((ap)->aar_tpln)
#define aar_sha(ap)	((const u_char *)((ap)+1))
#define aar_ssa(ap)	(aar_sha(ap) + ATMSHRD_LEN(ap))
#define aar_spa(ap)	(aar_ssa(ap) + ATMSSLN(ap))
#define aar_tha(ap)	(aar_spa(ap) + ATMSPROTO_LEN(ap))
#define aar_tsa(ap)	(aar_tha(ap) + ATMTHRD_LEN(ap))
#define aar_tpa(ap)	(aar_tsa(ap) + ATMTSLN(ap))
};

#define ATMSHA(ap) (aar_sha(ap))
#define ATMSSA(ap) (aar_ssa(ap))
#define ATMSPA(ap) (aar_spa(ap))
#define ATMTHA(ap) (aar_tha(ap))
#define ATMTSA(ap) (aar_tsa(ap))
#define ATMTPA(ap) (aar_tpa(ap))

static int
isnonzero(netdissect_options *ndo, const u_char *a, size_t len)
{
	while (len > 0) {
		if (GET_U_1(a) != 0)
			return (1);
		a++;
		len--;
	}
	return (0);
}

static void
tpaddr_print_ip(netdissect_options *ndo,
	        const struct arp_pkthdr *ap, u_short pro)
{
	if (pro != ETHERTYPE_IP && pro != ETHERTYPE_TRAIL)
		ND_PRINT("<wrong proto type>");
	else if (PROTO_LEN(ap) != 4)
		ND_PRINT("<wrong len>");
	else
		ND_PRINT("%s", GET_IPADDR_STRING(TPA(ap)));
}

static void
spaddr_print_ip(netdissect_options *ndo,
	        const struct arp_pkthdr *ap, u_short pro)
{
	if (pro != ETHERTYPE_IP && pro != ETHERTYPE_TRAIL)
		ND_PRINT("<wrong proto type>");
	else if (PROTO_LEN(ap) != 4)
		ND_PRINT("<wrong len>");
	else
		ND_PRINT("%s", GET_IPADDR_STRING(SPA(ap)));
}

static void
atmarp_addr_print(netdissect_options *ndo,
		  const u_char *ha, u_int ha_len, const u_char *srca,
    u_int srca_len)
{
	if (ha_len == 0)
		ND_PRINT("<No address>");
	else {
		ND_PRINT("%s", GET_LINKADDR_STRING(ha, LINKADDR_ATM, ha_len));
		if (srca_len != 0)
			ND_PRINT(",%s",
				  GET_LINKADDR_STRING(srca, LINKADDR_ATM, srca_len));
	}
}

static void
atmarp_tpaddr_print(netdissect_options *ndo,
		    const struct atmarp_pkthdr *ap, u_short pro)
{
	if (pro != ETHERTYPE_IP && pro != ETHERTYPE_TRAIL)
		ND_PRINT("<wrong proto type>");
	else if (ATMTPROTO_LEN(ap) != 4)
		ND_PRINT("<wrong tplen>");
	else
		ND_PRINT("%s", GET_IPADDR_STRING(ATMTPA(ap)));
}

static void
atmarp_spaddr_print(netdissect_options *ndo,
		    const struct atmarp_pkthdr *ap, u_short pro)
{
	if (pro != ETHERTYPE_IP && pro != ETHERTYPE_TRAIL)
		ND_PRINT("<wrong proto type>");
	else if (ATMSPROTO_LEN(ap) != 4)
		ND_PRINT("<wrong splen>");
	else
		ND_PRINT("%s", GET_IPADDR_STRING(ATMSPA(ap)));
}

static void
atmarp_print(netdissect_options *ndo,
	     const u_char *bp, u_int length, u_int caplen)
{
	const struct atmarp_pkthdr *ap;
	u_short pro, hrd, op;

	ap = (const struct atmarp_pkthdr *)bp;
	ND_TCHECK_SIZE(ap);

	hrd = ATMHRD(ap);
	pro = ATMPRO(ap);
	op = ATMOP(ap);

	ND_TCHECK_LEN(ATMTPA(ap), ATMTPROTO_LEN(ap));

        if (!ndo->ndo_eflag) {
            ND_PRINT("ARP, ");
        }

	if ((pro != ETHERTYPE_IP && pro != ETHERTYPE_TRAIL) ||
	    ATMSPROTO_LEN(ap) != 4 ||
            ATMTPROTO_LEN(ap) != 4 ||
            ndo->ndo_vflag) {
                ND_PRINT("%s, %s (len %u/%u)",
                          tok2str(arphrd_values, "Unknown Hardware (%u)", hrd),
                          tok2str(ethertype_values, "Unknown Protocol (0x%04x)", pro),
                          ATMSPROTO_LEN(ap),
                          ATMTPROTO_LEN(ap));

                /* don't know about the address formats */
                if (!ndo->ndo_vflag) {
                    goto out;
                }
	}

        /* print operation */
        ND_PRINT("%s%s ",
               ndo->ndo_vflag ? ", " : "",
               tok2str(arpop_values, "Unknown (%u)", op));

	switch (op) {

	case ARPOP_REQUEST:
		ND_PRINT("who-has ");
		atmarp_tpaddr_print(ndo, ap, pro);
		if (ATMTHRD_LEN(ap) != 0) {
			ND_PRINT(" (");
			atmarp_addr_print(ndo, ATMTHA(ap), ATMTHRD_LEN(ap),
			    ATMTSA(ap), ATMTSLN(ap));
			ND_PRINT(")");
		}
		ND_PRINT(" tell ");
		atmarp_spaddr_print(ndo, ap, pro);
		break;

	case ARPOP_REPLY:
		atmarp_spaddr_print(ndo, ap, pro);
		ND_PRINT(" is-at ");
		atmarp_addr_print(ndo, ATMSHA(ap), ATMSHRD_LEN(ap), ATMSSA(ap),
                                  ATMSSLN(ap));
		break;

	case ARPOP_INVREQUEST:
		ND_PRINT("who-is ");
		atmarp_addr_print(ndo, ATMTHA(ap), ATMTHRD_LEN(ap), ATMTSA(ap),
		    ATMTSLN(ap));
		ND_PRINT(" tell ");
		atmarp_addr_print(ndo, ATMSHA(ap), ATMSHRD_LEN(ap), ATMSSA(ap),
		    ATMSSLN(ap));
		break;

	case ARPOP_INVREPLY:
		atmarp_addr_print(ndo, ATMSHA(ap), ATMSHRD_LEN(ap), ATMSSA(ap),
		    ATMSSLN(ap));
		ND_PRINT("at ");
		atmarp_spaddr_print(ndo, ap, pro);
		break;

	case ARPOP_NAK:
		ND_PRINT("for ");
		atmarp_spaddr_print(ndo, ap, pro);
		break;

	default:
		ND_DEFAULTPRINT((const u_char *)ap, caplen);
		return;
	}

 out:
        ND_PRINT(", length %u", length);
}

void
arp_print(netdissect_options *ndo,
	  const u_char *bp, u_int length, u_int caplen)
{
	const struct arp_pkthdr *ap;
	u_short pro, hrd, op, linkaddr;

	ndo->ndo_protocol = "arp";
	ap = (const struct arp_pkthdr *)bp;
	ND_TCHECK_SIZE(ap);

	hrd = HRD(ap);
	pro = PRO(ap);
	op = OP(ap);


        /* if its ATM then call the ATM ARP printer
           for Frame-relay ARP most of the fields
           are similar to Ethernet so overload the Ethernet Printer
           and set the linkaddr type for GET_LINKADDR_STRING() accordingly */

        switch(hrd) {
        case ARPHRD_ATM2225:
            atmarp_print(ndo, bp, length, caplen);
            return;
        case ARPHRD_FRELAY:
            linkaddr = LINKADDR_FRELAY;
            break;
        default:
            linkaddr = LINKADDR_ETHER;
            break;
	}

	ND_TCHECK_LEN(TPA(ap), PROTO_LEN(ap));

        if (!ndo->ndo_eflag) {
            ND_PRINT("ARP, ");
        }

        /* print hardware type/len and proto type/len */
        if ((pro != ETHERTYPE_IP && pro != ETHERTYPE_TRAIL) ||
	    PROTO_LEN(ap) != 4 ||
            HRD_LEN(ap) == 0 ||
            ndo->ndo_vflag) {
            ND_PRINT("%s (len %u), %s (len %u)",
                      tok2str(arphrd_values, "Unknown Hardware (%u)", hrd),
                      HRD_LEN(ap),
                      tok2str(ethertype_values, "Unknown Protocol (0x%04x)", pro),
                      PROTO_LEN(ap));

            /* don't know about the address formats */
            if (!ndo->ndo_vflag) {
                goto out;
            }
	}

        /* print operation */
        ND_PRINT("%s%s ",
               ndo->ndo_vflag ? ", " : "",
               tok2str(arpop_values, "Unknown (%u)", op));

	switch (op) {

	case ARPOP_REQUEST:
		ND_PRINT("who-has ");
		tpaddr_print_ip(ndo, ap, pro);
		if (isnonzero(ndo, (const u_char *)THA(ap), HRD_LEN(ap)))
			ND_PRINT(" (%s)",
				  GET_LINKADDR_STRING(THA(ap), linkaddr, HRD_LEN(ap)));
		ND_PRINT(" tell ");
		spaddr_print_ip(ndo, ap, pro);
		break;

	case ARPOP_REPLY:
		spaddr_print_ip(ndo, ap, pro);
		ND_PRINT(" is-at %s",
                          GET_LINKADDR_STRING(SHA(ap), linkaddr, HRD_LEN(ap)));
		break;

	case ARPOP_REVREQUEST:
		/*
		 * XXX - GET_LINKADDR_STRING() may return a pointer to
		 * a static buffer, so we only have one call to it per
		 * ND_PRINT() call.
		 *
		 * This should be done in a cleaner fashion.
		 */
		ND_PRINT("who-is %s",
			  GET_LINKADDR_STRING(THA(ap), linkaddr, HRD_LEN(ap)));
		ND_PRINT(" tell %s",
			  GET_LINKADDR_STRING(SHA(ap), linkaddr, HRD_LEN(ap)));
		break;

	case ARPOP_REVREPLY:
		ND_PRINT("%s at ",
			  GET_LINKADDR_STRING(THA(ap), linkaddr, HRD_LEN(ap)));
		tpaddr_print_ip(ndo, ap, pro);
		break;

	case ARPOP_INVREQUEST:
		/*
		 * XXX - GET_LINKADDR_STRING() may return a pointer to
		 * a static buffer, so we only have one call to it per
		 * ND_PRINT() call.
		 *
		 * This should be done in a cleaner fashion.
		 */
		ND_PRINT("who-is %s",
			  GET_LINKADDR_STRING(THA(ap), linkaddr, HRD_LEN(ap)));
		ND_PRINT(" tell %s",
			  GET_LINKADDR_STRING(SHA(ap), linkaddr, HRD_LEN(ap)));
		break;

	case ARPOP_INVREPLY:
		ND_PRINT("%s at ",
			  GET_LINKADDR_STRING(SHA(ap), linkaddr, HRD_LEN(ap)));
		spaddr_print_ip(ndo, ap, pro);
		break;

	default:
		ND_DEFAULTPRINT((const u_char *)ap, caplen);
		return;
	}

 out:
        ND_PRINT(", length %u", length);
}
