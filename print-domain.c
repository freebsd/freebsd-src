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

/* \summary: Domain Name System (DNS) printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include <string.h>

#include "netdissect.h"
#include "addrtoname.h"
#include "addrtostr.h"
#include "extract.h"

#include "nameser.h"

static const char *ns_ops[] = {
	"", " inv_q", " stat", " op3", " notify", " update", " op6", " op7",
	" op8", " updateA", " updateD", " updateDA",
	" updateM", " updateMA", " zoneInit", " zoneRef",
};

static const char *ns_resp[] = {
	"", " FormErr", " ServFail", " NXDomain",
	" NotImp", " Refused", " YXDomain", " YXRRSet",
	" NXRRSet", " NotAuth", " NotZone", " Resp11",
	" Resp12", " Resp13", " Resp14", " NoChange",
	" BadVers", "Resp17", " Resp18", " Resp19",
	" Resp20", "Resp21", " Resp22", " BadCookie",
};

static const char *
ns_rcode(u_int rcode) {
	static char buf[sizeof(" Resp4095")];

	if (rcode < sizeof(ns_resp)/sizeof(ns_resp[0])) {
		return (ns_resp[rcode]);
	}
	snprintf(buf, sizeof(buf), " Resp%u", rcode & 0xfff);
	return (buf);
}

/* skip over a domain name */
static const u_char *
ns_nskip(netdissect_options *ndo,
         const u_char *cp)
{
	u_char i;

	if (!ND_TTEST_1(cp))
		return (NULL);
	i = GET_U_1(cp);
	cp++;
	while (i) {
		switch (i & TYPE_MASK) {

		case TYPE_INDIR:
			return (cp + 1);

		case TYPE_EDNS0: {
			int bitlen, bytelen;

			if ((i & ~TYPE_MASK) != EDNS0_ELT_BITLABEL)
				return(NULL); /* unknown ELT */
			if (!ND_TTEST_1(cp))
				return (NULL);
			if ((bitlen = GET_U_1(cp)) == 0)
				bitlen = 256;
			cp++;
			bytelen = (bitlen + 7) / 8;
			cp += bytelen;
		}
		break;

		case TYPE_RESERVED:
			return (NULL);

		case TYPE_LABEL:
			cp += i;
			break;
		}
		if (!ND_TTEST_1(cp))
			return (NULL);
		i = GET_U_1(cp);
		cp++;
	}
	return (cp);
}

static const u_char *
blabel_print(netdissect_options *ndo,
             const u_char *cp)
{
	u_int bitlen, slen, b;
	const u_char *bitp, *lim;
	uint8_t tc;

	if (!ND_TTEST_1(cp))
		return(NULL);
	if ((bitlen = GET_U_1(cp)) == 0)
		bitlen = 256;
	slen = (bitlen + 3) / 4;
	lim = cp + 1 + slen;

	/* print the bit string as a hex string */
	ND_PRINT("\\[x");
	for (bitp = cp + 1, b = bitlen; bitp < lim && b > 7; b -= 8, bitp++) {
		ND_PRINT("%02x", GET_U_1(bitp));
	}
	if (b > 4) {
		tc = GET_U_1(bitp);
		bitp++;
		ND_PRINT("%02x", tc & (0xff << (8 - b)));
	} else if (b > 0) {
		tc = GET_U_1(bitp);
		bitp++;
		ND_PRINT("%1x", ((tc >> 4) & 0x0f) & (0x0f << (4 - b)));
	}
	ND_PRINT("/%u]", bitlen);
	return lim;
}

static int
labellen(netdissect_options *ndo,
         const u_char *cp)
{
	u_int i;

	if (!ND_TTEST_1(cp))
		return(-1);
	i = GET_U_1(cp);
	switch (i & TYPE_MASK) {

	case TYPE_EDNS0: {
		u_int bitlen, elt;
		if ((elt = (i & ~TYPE_MASK)) != EDNS0_ELT_BITLABEL) {
			ND_PRINT("<ELT %d>", elt);
			return(-1);
		}
		if (!ND_TTEST_1(cp + 1))
			return(-1);
		if ((bitlen = GET_U_1(cp + 1)) == 0)
			bitlen = 256;
		return(((bitlen + 7) / 8) + 1);
	}

	case TYPE_INDIR:
	case TYPE_LABEL:
		return(i);

	default:
		/*
		 * TYPE_RESERVED, but we use default to suppress compiler
		 * warnings about falling out of the switch statement.
		 */
		ND_PRINT("<BAD LABEL TYPE>");
		return(-1);
	}
}

/* print a <domain-name> */
const u_char *
fqdn_print(netdissect_options *ndo,
          const u_char *cp, const u_char *bp)
{
	u_int i, l;
	const u_char *rp = NULL;
	int compress = 0;
	u_int elt;
	u_int offset, max_offset;
	u_int name_chars = 0;

	if ((l = labellen(ndo, cp)) == (u_int)-1)
		return(NULL);
	if (!ND_TTEST_1(cp))
		return(NULL);
	max_offset = (u_int)(cp - bp);
	i = GET_U_1(cp);
	cp++;
	if ((i & TYPE_MASK) != TYPE_INDIR) {
		compress = 0;
		rp = cp + l;
	}

	if (i != 0) {
		while (i && cp < ndo->ndo_snapend) {
			switch (i & TYPE_MASK) {

			case TYPE_INDIR:
				if (!compress) {
					rp = cp + 1;
					compress = 1;
				}
				if (!ND_TTEST_1(cp))
					return(NULL);
				offset = (((i << 8) | GET_U_1(cp)) & 0x3fff);
				/*
				 * This must move backwards in the packet.
				 * No RFC explicitly says that, but BIND's
				 * name decompression code requires it,
				 * as a way of preventing infinite loops
				 * and other bad behavior, and it's probably
				 * what was intended (compress by pointing
				 * to domain name suffixes already seen in
				 * the packet).
				 */
				if (offset >= max_offset) {
					ND_PRINT("<BAD PTR>");
					return(NULL);
				}
				max_offset = offset;
				cp = bp + offset;
				if (!ND_TTEST_1(cp))
					return(NULL);
				i = GET_U_1(cp);
				if ((l = labellen(ndo, cp)) == (u_int)-1)
					return(NULL);
				cp++;
				continue;

			case TYPE_EDNS0:
				elt = (i & ~TYPE_MASK);
				switch(elt) {
				case EDNS0_ELT_BITLABEL:
					if (blabel_print(ndo, cp) == NULL)
						return (NULL);
					break;
				default:
					/* unknown ELT */
					ND_PRINT("<ELT %u>", elt);
					return(NULL);
				}
				break;

			case TYPE_RESERVED:
				ND_PRINT("<BAD LABEL TYPE>");
				return(NULL);

			case TYPE_LABEL:
				if (name_chars + l <= MAXCDNAME) {
					if (nd_printn(ndo, cp, l, ndo->ndo_snapend))
						return(NULL);
				} else if (name_chars < MAXCDNAME) {
					if (nd_printn(ndo, cp,
					    MAXCDNAME - name_chars, ndo->ndo_snapend))
						return(NULL);
				}
				name_chars += l;
				break;
			}

			cp += l;
			if (name_chars <= MAXCDNAME)
				ND_PRINT(".");
			name_chars++;
			if (!ND_TTEST_1(cp))
				return(NULL);
			i = GET_U_1(cp);
			if ((l = labellen(ndo, cp)) == (u_int)-1)
				return(NULL);
			cp++;
			if (!compress)
				rp += l + 1;
		}
		if (name_chars > MAXCDNAME)
			ND_PRINT("<DOMAIN NAME TOO LONG>");
	} else
		ND_PRINT(".");
	return (rp);
}

/* print a <character-string> */
static const u_char *
ns_cprint(netdissect_options *ndo,
          const u_char *cp)
{
	u_int i;

	if (!ND_TTEST_1(cp))
		return (NULL);
	i = GET_U_1(cp);
	cp++;
	if (nd_printn(ndo, cp, i, ndo->ndo_snapend))
		return (NULL);
	return (cp + i);
}

static void
print_eopt_ecs(netdissect_options *ndo, const u_char *cp,
               u_int data_len)
{
    u_int family, addr_bits, src_len, scope_len;

    u_char padded[32];
    char addr[INET6_ADDRSTRLEN];

    /* ecs option must at least contain family, src len, and scope len */
    if (data_len < 4) {
        nd_print_invalid(ndo);
        return;
    }

    family = GET_BE_U_2(cp);
    cp += 2;
    src_len = GET_U_1(cp);
    cp += 1;
    scope_len = GET_U_1(cp);
    cp += 1;

    if (family == 1)
        addr_bits = 32;
    else if (family == 2)
        addr_bits = 128;
    else {
        nd_print_invalid(ndo);
        return;
    }

    if (data_len - 4 > (addr_bits / 8)) {
        nd_print_invalid(ndo);
        return;
    }
    /* checks for invalid ecs scope or source length */
    if (src_len > addr_bits || scope_len > addr_bits || ((src_len + 7) / 8) != (data_len - 4)) {
        nd_print_invalid(ndo);
        return;
    }

    /* pad the truncated address from ecs with zeros */
    memset(padded, 0, sizeof(padded));
    memcpy(padded, cp, data_len - 4);


    if (family == 1)
        ND_PRINT("%s/%d/%d", addrtostr(padded, addr, INET_ADDRSTRLEN),
                src_len, scope_len);
    else
        ND_PRINT("%s/%d/%d", addrtostr6(padded, addr, INET6_ADDRSTRLEN),
                src_len, scope_len);

}

extern const struct tok edns_opt2str[];
extern const struct tok dau_alg2str[];
extern const struct tok dhu_alg2str[];
extern const struct tok n3u_alg2str[];


/* print an <EDNS-option> */
static const u_char *
eopt_print(netdissect_options *ndo,
          const u_char *cp)
{
    u_int opt, data_len, i;

    if (!ND_TTEST_2(cp))
        return (NULL);
    opt = GET_BE_U_2(cp);
    cp += 2;
    ND_PRINT("%s", tok2str(edns_opt2str, "Opt%u", opt));
    if (!ND_TTEST_2(cp))
        return (NULL);
    data_len = GET_BE_U_2(cp);
    cp += 2;

    ND_TCHECK_LEN(cp, data_len);

    if (data_len > 0) {
        ND_PRINT(" ");
        switch (opt) {

        case E_ECS:
            print_eopt_ecs(ndo, cp, data_len);
            break;
        case E_COOKIE:
            if (data_len < 8 || (data_len > 8 && data_len < 16) || data_len > 40)
                nd_print_invalid(ndo);
            else {
                for (i = 0; i < data_len; ++i) {
                    /* split client and server cookie */
                    if (i == 8)
                        ND_PRINT(" ");
                    ND_PRINT("%02x", GET_U_1(cp + i));
                }
            }
            break;
        case E_KEEPALIVE:
            if (data_len != 2)
                nd_print_invalid(ndo);
            else
                /* keepalive is in increments of 100ms. Convert to seconds */
                ND_PRINT("%0.1f sec", (GET_BE_U_2(cp) / 10.0));
            break;
        case E_EXPIRE:
            if (data_len != 4)
                nd_print_invalid(ndo);
            else
                ND_PRINT("%u sec", GET_BE_U_4(cp));
            break;
        case E_PADDING:
            /* ignore contents and just print length */
            ND_PRINT("(%u)", data_len);
            break;
        case E_KEYTAG:
            if (data_len % 2 != 0)
                nd_print_invalid(ndo);
            else
                for (i = 0; i < data_len; i += 2) {
                    if (i > 0)
                        ND_PRINT(" ");
                    ND_PRINT("%u", GET_BE_U_2(cp + i));
                }
            break;
        case E_DAU:
            for (i = 0; i < data_len; ++i) {
                if (i > 0)
                    ND_PRINT(" ");
                ND_PRINT("%s", tok2str(dau_alg2str, "Alg_%u", GET_U_1(cp + i)));
            }
            break;
        case E_DHU:
            for (i = 0; i < data_len; ++i) {
                if (i > 0)
                    ND_PRINT(" ");
                ND_PRINT("%s", tok2str(dhu_alg2str, "Alg_%u", GET_U_1(cp + i)));
            }
            break;
        case E_N3U:
            for (i = 0; i < data_len; ++i) {
                if (i > 0)
                    ND_PRINT(" ");
                ND_PRINT("%s", tok2str(n3u_alg2str, "Alg_%u", GET_U_1(cp + i)));
            }
            break;
        case E_CHAIN:
            fqdn_print(ndo, cp, cp + data_len);
            break;
        case E_NSID:
            /* intentional fall-through. NSID is an undefined byte string */
        default:
            for (i = 0; i < data_len; ++i)
                ND_PRINT("%02x", GET_U_1(cp + i));
            break;
        }
    }
    return (cp + data_len);

  trunc:
    return (NULL);

}



extern const struct tok ns_type2str[];

/* https://www.iana.org/assignments/dns-parameters */
const struct tok ns_type2str[] = {
	{ T_A,		"A" },			/* RFC 1035 */
	{ T_NS,		"NS" },			/* RFC 1035 */
	{ T_MD,		"MD" },			/* RFC 1035 */
	{ T_MF,		"MF" },			/* RFC 1035 */
	{ T_CNAME,	"CNAME" },		/* RFC 1035 */
	{ T_SOA,	"SOA" },		/* RFC 1035 */
	{ T_MB,		"MB" },			/* RFC 1035 */
	{ T_MG,		"MG" },			/* RFC 1035 */
	{ T_MR,		"MR" },			/* RFC 1035 */
	{ T_NULL,	"NULL" },		/* RFC 1035 */
	{ T_WKS,	"WKS" },		/* RFC 1035 */
	{ T_PTR,	"PTR" },		/* RFC 1035 */
	{ T_HINFO,	"HINFO" },		/* RFC 1035 */
	{ T_MINFO,	"MINFO" },		/* RFC 1035 */
	{ T_MX,		"MX" },			/* RFC 1035 */
	{ T_TXT,	"TXT" },		/* RFC 1035 */
	{ T_RP,		"RP" },			/* RFC 1183 */
	{ T_AFSDB,	"AFSDB" },		/* RFC 5864 */
	{ T_X25,	"X25" },		/* RFC 1183 */
	{ T_ISDN,	"ISDN" },		/* RFC 1183 */
	{ T_RT,		"RT" },			/* RFC 1183 */
	{ T_NSAP,	"NSAP" },		/* RFC 1706 */
	{ T_NSAP_PTR,	"NSAP_PTR" },		/* RFC 1706 */
	{ T_SIG,	"SIG" },		/* RFC 3008 */
	{ T_KEY,	"KEY" },		/* RFC 3110 */
	{ T_PX,		"PX" },			/* RFC 2163 */
	{ T_GPOS,	"GPOS" },		/* RFC 1712 */
	{ T_AAAA,	"AAAA" },		/* RFC 3596 */
	{ T_LOC,	"LOC" },		/* RFC 1876 */
	{ T_NXT,	"NXT" },		/* RFC 3755 */
	{ T_EID,	"EID" },		/* Nimrod */
	{ T_NIMLOC,	"NIMLOC" },		/* Nimrod */
	{ T_SRV,	"SRV" },		/* RFC 2782 */
	{ T_ATMA,	"ATMA" },		/* ATM Forum */
	{ T_NAPTR,	"NAPTR" },		/* RFC 3403 */
	{ T_KX,		"KX" },			/* RFC 2230 */
	{ T_CERT,	"CERT" },		/* RFC 4398 */
	{ T_A6,		"A6" },			/* RFC 6563 */
	{ T_DNAME,	"DNAME" },		/* RFC 6672 */
	{ T_SINK,	"SINK" },
	{ T_OPT,	"OPT" },		/* RFC 6891 */
	{ T_APL,	"APL" },		/* RFC 3123 */
	{ T_DS,		"DS" },			/* RFC 4034 */
	{ T_SSHFP,	"SSHFP" },		/* RFC 4255 */
	{ T_IPSECKEY,	"IPSECKEY" },		/* RFC 4025 */
	{ T_RRSIG,	"RRSIG" },		/* RFC 4034 */
	{ T_NSEC,	"NSEC" },		/* RFC 4034 */
	{ T_DNSKEY,	"DNSKEY" },		/* RFC 4034 */
	{ T_DHCID,	"DHCID" },		/* RFC 4071 */
	{ T_NSEC3,	"NSEC3" },		/* RFC 5155 */
	{ T_NSEC3PARAM,	"NSEC3PARAM" },		/* RFC 5155 */
	{ T_TLSA,	"TLSA" },		/* RFC 6698 */
	{ T_SMIMEA,	"SMIMEA" },		/* RFC 8162 */
	{ T_HIP,	"HIP" },		/* RFC 8005 */
	{ T_NINFO,	"NINFO" },
	{ T_RKEY,	"RKEY" },
	{ T_TALINK,	"TALINK" },
	{ T_CDS,	"CDS" },		/* RFC 7344 */
	{ T_CDNSKEY,	"CDNSKEY" },		/* RFC 7344 */
	{ T_OPENPGPKEY,	"OPENPGPKEY" },		/* RFC 7929 */
	{ T_CSYNC,	"CSYNC" },		/* RFC 7477 */
	{ T_ZONEMD,	"ZONEMD" },		/* RFC 8976 */
	{ T_SVCB,	"SVCB" },
	{ T_HTTPS,	"HTTPS" },
	{ T_SPF,	"SPF" },		/* RFC 7208 */
	{ T_UINFO,	"UINFO" },
	{ T_UID,	"UID" },
	{ T_GID,	"GID" },
	{ T_UNSPEC,	"UNSPEC" },
	{ T_NID,	"NID" },		/* RFC 6742 */
	{ T_L32,	"L32" },		/* RFC 6742 */
	{ T_L64,	"L64" },		/* RFC 6742 */
	{ T_LP,		"LP" },			/* RFC 6742 */
	{ T_EUI48,	"EUI48" },		/* RFC 7043 */
	{ T_EUI64,	"EUI64" },		/* RFC 7043 */
	{ T_TKEY,	"TKEY" },		/* RFC 2930 */
	{ T_TSIG,	"TSIG" },		/* RFC 8945 */
	{ T_IXFR,	"IXFR" },		/* RFC 1995 */
	{ T_AXFR,	"AXFR" },		/* RFC 5936 */
	{ T_MAILB,	"MAILB" },		/* RFC 1035 */
	{ T_MAILA,	"MAILA" },		/* RFC 1035 */
	{ T_ANY,	"ANY" },		/* RFC 8482 */
	{ T_URI,	"URI" },		/* RFC 7553 */
	{ T_CAA,	"CAA" },		/* RFC 8659 */
	{ T_AVC,	"AVC" },
	{ T_DOA,	"DOA" },
	{ T_AMTRELAY,	"AMTRELAY" },		/* RFC 8777 */
	{ T_TA,		"TA" },
	{ T_DLV,	"DLV" },		/* RFC 8749 */
	{ 0,		NULL }
};

extern const struct tok ns_class2str[];

const struct tok ns_class2str[] = {
	{ C_IN,		"IN" },		/* Not used */
	{ C_CHAOS,	"CHAOS" },
	{ C_HS,		"HS" },
	{ C_ANY,	"ANY" },
	{ 0,		NULL }
};

const struct tok edns_opt2str[] = {
    { E_LLQ,        "LLQ" },
    { E_UL,         "UL" },
    { E_NSID,       "NSID" },
    { E_DAU,        "DAU" },
    { E_DHU,        "DHU" },
    { E_N3U,        "N3U" },
    { E_ECS,        "ECS" },
    { E_EXPIRE,     "EXPIRE" },
    { E_COOKIE,     "COOKIE" },
    { E_KEEPALIVE,  "KEEPALIVE" },
    { E_PADDING,    "PADDING" },
    { E_CHAIN,      "CHAIN" },
    { E_KEYTAG,     "KEY-TAG" },
    { E_CLIENTTAG,  "CLIENT-TAG" },
    { E_SERVERTAG,  "SERVER-TAG" },
    { 0,            NULL }
};

const struct tok dau_alg2str[] = {
    { A_DELETE,             "DELETE" },
    { A_RSAMD5,             "RSAMD5" },
    { A_DH,                 "DH" },
    { A_DSA,                "DS" },
    { A_RSASHA1,            "RSASHA1" },
    { A_DSA_NSEC3_SHA1,     "DSA-NSEC3-SHA1" },
    { A_RSASHA1_NSEC3_SHA1, "RSASHA1-NSEC3-SHA1" },
    { A_RSASHA256,          "RSASHA256" },
    { A_RSASHA512,          "RSASHA512" },
    { A_ECC_GOST,           "ECC-GOST" },
    { A_ECDSAP256SHA256,    "ECDSAP256SHA256" },
    { A_ECDSAP384SHA384,    "ECDSAP384SHA384" },
    { A_ED25519,            "ED25519" },
    { A_ED448,              "ED448" },
    { A_INDIRECT,           "INDIRECT" },
    { A_PRIVATEDNS,         "PRIVATEDNS" },
    { A_PRIVATEOID,         "PRIVATEOID" },
    { 0,                NULL }
};

const struct tok dhu_alg2str[] = {
    { DS_SHA1,  "SHA-1" },
    { DS_SHA256,"SHA-256" },
    { DS_GOST,  "GOST_R_34.11-94" },
    { DS_SHA384,"SHA-384" },
    { 0,    NULL }
};

const struct tok n3u_alg2str[] = {
    { NSEC_SHA1,"SHA-1" },
    { 0,    NULL }
};

/* print a query */
static const u_char *
ns_qprint(netdissect_options *ndo,
          const u_char *cp, const u_char *bp, int is_mdns)
{
	const u_char *np = cp;
	u_int i, class;

	cp = ns_nskip(ndo, cp);

	if (cp == NULL || !ND_TTEST_4(cp))
		return(NULL);

	/* print the qtype */
	i = GET_BE_U_2(cp);
	cp += 2;
	ND_PRINT(" %s", tok2str(ns_type2str, "Type%u", i));
	/* print the qclass (if it's not IN) */
	i = GET_BE_U_2(cp);
	cp += 2;
	if (is_mdns)
		class = (i & ~C_QU);
	else
		class = i;
	if (class != C_IN)
		ND_PRINT(" %s", tok2str(ns_class2str, "(Class %u)", class));
	if (is_mdns) {
		ND_PRINT(i & C_QU ? " (QU)" : " (QM)");
	}

	ND_PRINT("? ");
	cp = fqdn_print(ndo, np, bp);
	return(cp ? cp + 4 : NULL);
}

/* print a reply */
static const u_char *
ns_rprint(netdissect_options *ndo,
          const u_char *cp, const u_char *bp, int is_mdns)
{
	u_int i, class, opt_flags = 0;
	u_short typ, len;
	const u_char *rp;

	if (ndo->ndo_vflag) {
		ND_PRINT(" ");
		if ((cp = fqdn_print(ndo, cp, bp)) == NULL)
			return NULL;
	} else
		cp = ns_nskip(ndo, cp);

	if (cp == NULL || !ND_TTEST_LEN(cp, 10))
		return (ndo->ndo_snapend);

	/* print the type/qtype */
	typ = GET_BE_U_2(cp);
	cp += 2;
	/* print the class (if it's not IN and the type isn't OPT) */
	i = GET_BE_U_2(cp);
	cp += 2;
	if (is_mdns)
		class = (i & ~C_CACHE_FLUSH);
	else
		class = i;
	if (class != C_IN && typ != T_OPT)
		ND_PRINT(" %s", tok2str(ns_class2str, "(Class %u)", class));
	if (is_mdns) {
		if (i & C_CACHE_FLUSH)
			ND_PRINT(" (Cache flush)");
	}

	if (typ == T_OPT) {
		/* get opt flags */
		cp += 2;
		opt_flags = GET_BE_U_2(cp);
		/* ignore rest of ttl field */
		cp += 2;
	} else if (ndo->ndo_vflag > 2) {
		/* print ttl */
		ND_PRINT(" [");
		unsigned_relts_print(ndo, GET_BE_U_4(cp));
		ND_PRINT("]");
		cp += 4;
	} else {
		/* ignore ttl */
		cp += 4;
	}

	len = GET_BE_U_2(cp);
	cp += 2;

	rp = cp + len;

	ND_PRINT(" %s", tok2str(ns_type2str, "Type%u", typ));
	if (rp > ndo->ndo_snapend)
		return(NULL);

	switch (typ) {
	case T_A:
		if (!ND_TTEST_LEN(cp, sizeof(nd_ipv4)))
			return(NULL);
		ND_PRINT(" %s", intoa(GET_IPV4_TO_NETWORK_ORDER(cp)));
		break;

	case T_NS:
	case T_CNAME:
	case T_PTR:
	case T_DNAME:
		ND_PRINT(" ");
		if (fqdn_print(ndo, cp, bp) == NULL)
			return(NULL);
		break;

	case T_SOA:
		if (!ndo->ndo_vflag)
			break;
		ND_PRINT(" ");
		if ((cp = fqdn_print(ndo, cp, bp)) == NULL)
			return(NULL);
		ND_PRINT(" ");
		if ((cp = fqdn_print(ndo, cp, bp)) == NULL)
			return(NULL);
		if (!ND_TTEST_LEN(cp, 5 * 4))
			return(NULL);
		ND_PRINT(" %u", GET_BE_U_4(cp));
		cp += 4;
		ND_PRINT(" %u", GET_BE_U_4(cp));
		cp += 4;
		ND_PRINT(" %u", GET_BE_U_4(cp));
		cp += 4;
		ND_PRINT(" %u", GET_BE_U_4(cp));
		cp += 4;
		ND_PRINT(" %u", GET_BE_U_4(cp));
		cp += 4;
		break;
	case T_MX:
		ND_PRINT(" ");
		if (!ND_TTEST_2(cp))
			return(NULL);
		if (fqdn_print(ndo, cp + 2, bp) == NULL)
			return(NULL);
		ND_PRINT(" %u", GET_BE_U_2(cp));
		break;

	case T_TXT:
		while (cp < rp) {
			ND_PRINT(" \"");
			cp = ns_cprint(ndo, cp);
			if (cp == NULL)
				return(NULL);
			ND_PRINT("\"");
		}
		break;

	case T_SRV:
		ND_PRINT(" ");
		if (!ND_TTEST_6(cp))
			return(NULL);
		if (fqdn_print(ndo, cp + 6, bp) == NULL)
			return(NULL);
		ND_PRINT(":%u %u %u", GET_BE_U_2(cp + 4),
			  GET_BE_U_2(cp), GET_BE_U_2(cp + 2));
		break;

	case T_AAAA:
	    {
		char ntop_buf[INET6_ADDRSTRLEN];

		if (!ND_TTEST_LEN(cp, sizeof(nd_ipv6)))
			return(NULL);
		ND_PRINT(" %s",
		    addrtostr6(cp, ntop_buf, sizeof(ntop_buf)));

		break;
	    }

	case T_A6:
	    {
		nd_ipv6 a;
		int pbit, pbyte;
		char ntop_buf[INET6_ADDRSTRLEN];

		if (!ND_TTEST_1(cp))
			return(NULL);
		pbit = GET_U_1(cp);
		pbyte = (pbit & ~7) / 8;
		if (pbit > 128) {
			ND_PRINT(" %u(bad plen)", pbit);
			break;
		} else if (pbit < 128) {
			memset(a, 0, sizeof(a));
			GET_CPY_BYTES(a + pbyte, cp + 1, sizeof(a) - pbyte);
			ND_PRINT(" %u %s", pbit,
			    addrtostr6(&a, ntop_buf, sizeof(ntop_buf)));
		}
		if (pbit > 0) {
			ND_PRINT(" ");
			if (fqdn_print(ndo, cp + 1 + sizeof(a) - pbyte, bp) == NULL)
				return(NULL);
		}
		break;
	    }

	case T_URI:
		if (!ND_TTEST_LEN(cp, len))
			return(NULL);
		ND_PRINT(" %u %u ", GET_BE_U_2(cp), GET_BE_U_2(cp + 2));
		if (nd_printn(ndo, cp + 4, len - 4, ndo->ndo_snapend))
			return(NULL);
		break;

	case T_OPT:
		ND_PRINT(" UDPsize=%u", class);
		if (opt_flags & 0x8000)
			ND_PRINT(" DO");
        if (cp < rp) {
            ND_PRINT(" [");
            while (cp < rp) {
                cp = eopt_print(ndo, cp);
                if (cp == NULL)
                    return(NULL);
                if (cp < rp)
                    ND_PRINT(",");
            }
            ND_PRINT("]");
        }
		break;

	case T_TSIG:
	    {
		if (cp + len > ndo->ndo_snapend)
			return(NULL);
		if (!ndo->ndo_vflag)
			break;
		ND_PRINT(" ");
		if ((cp = fqdn_print(ndo, cp, bp)) == NULL)
			return(NULL);
		cp += 6;
		if (!ND_TTEST_2(cp))
			return(NULL);
		ND_PRINT(" fudge=%u", GET_BE_U_2(cp));
		cp += 2;
		if (!ND_TTEST_2(cp))
			return(NULL);
		ND_PRINT(" maclen=%u", GET_BE_U_2(cp));
		cp += 2 + GET_BE_U_2(cp);
		if (!ND_TTEST_2(cp))
			return(NULL);
		ND_PRINT(" origid=%u", GET_BE_U_2(cp));
		cp += 2;
		if (!ND_TTEST_2(cp))
			return(NULL);
		ND_PRINT(" error=%u", GET_BE_U_2(cp));
		cp += 2;
		if (!ND_TTEST_2(cp))
			return(NULL);
		ND_PRINT(" otherlen=%u", GET_BE_U_2(cp));
		cp += 2;
	    }
	}
	return (rp);		/* XXX This isn't always right */
}

void
domain_print(netdissect_options *ndo,
             const u_char *bp, u_int length, int over_tcp, int is_mdns)
{
	const dns_header_t *np;
	uint16_t flags, rcode, rdlen, type;
	u_int qdcount, ancount, nscount, arcount;
	u_int i;
	const u_char *cp;
	uint16_t b2;

	ndo->ndo_protocol = "domain";

	if (over_tcp) {
		/*
		 * The message is prefixed with a two byte length field
		 * which gives the message length, excluding the two byte
		 * length field. (RFC 1035 - 4.2.2. TCP usage)
		 */
		if (length < 2) {
			ND_PRINT(" [DNS over TCP: length %u < 2]", length);
			nd_print_invalid(ndo);
			return;
		} else {
			length -= 2; /* excluding the two byte length field */
			if (GET_BE_U_2(bp) != length) {
				ND_PRINT(" [prefix length(%u) != length(%u)]",
					 GET_BE_U_2(bp), length);
				nd_print_invalid(ndo);
				return;
			} else {
				bp += 2;
				/* in over TCP case, we need to prepend a space
				 * (not needed in over UDP case)
				 */
				ND_PRINT(" ");
			}
		}
	}

	np = (const dns_header_t *)bp;

	if(length < sizeof(*np)) {
		nd_print_protocol(ndo);
		ND_PRINT(" [length %u < %zu]", length, sizeof(*np));
		nd_print_invalid(ndo);
		return;
	}

	ND_TCHECK_SIZE(np);
	flags = GET_BE_U_2(np->flags);
	/* get the byte-order right */
	qdcount = GET_BE_U_2(np->qdcount);
	ancount = GET_BE_U_2(np->ancount);
	nscount = GET_BE_U_2(np->nscount);
	arcount = GET_BE_U_2(np->arcount);

	/* find the opt record to extract extended rcode */
	cp = (const u_char *)(np + 1);
	rcode = DNS_RCODE(flags);
	for (i = 0; i < qdcount; i++) {
		if ((cp = ns_nskip(ndo, cp)) == NULL)
			goto print;
		cp += 4;	/* skip QTYPE and QCLASS */
		if (cp >= ndo->ndo_snapend)
			goto print;
	}
	for (i = 0; i < ancount + nscount; i++) {
		if ((cp = ns_nskip(ndo, cp)) == NULL)
			goto print;
		cp += 8;	/* skip TYPE, CLASS and TTL */
		if (cp + 2 > ndo->ndo_snapend)
			goto print;
		rdlen = GET_BE_U_2(cp);
		cp += 2 + rdlen;
		if (cp >= ndo->ndo_snapend)
			goto print;
	}
	for (i = 0; i < arcount; i++) {
		if ((cp = ns_nskip(ndo, cp)) == NULL)
			goto print;
		if (cp + 2 > ndo->ndo_snapend)
			goto print;
		type = GET_BE_U_2(cp);
		cp += 4;	/* skip TYPE and CLASS */
		if (cp + 1 > ndo->ndo_snapend)
			goto print;
		if (type == T_OPT) {
			rcode |= (GET_U_1(cp) << 4);
			goto print;
		}
		cp += 4;
		if (cp + 2 > ndo->ndo_snapend)
			goto print;
		rdlen = GET_BE_U_2(cp);
		cp += 2 + rdlen;
		if (cp >= ndo->ndo_snapend)
			goto print;
	}

 print:
	if (DNS_QR(flags)) {
		/* this is a response */
		ND_PRINT("%u%s%s%s%s%s%s",
			GET_BE_U_2(np->id),
			ns_ops[DNS_OPCODE(flags)],
			ns_rcode(rcode),
			DNS_AA(flags)? "*" : "",
			DNS_RA(flags)? "" : "-",
			DNS_TC(flags)? "|" : "",
			DNS_AD(flags)? "$" : "");

		if (qdcount != 1)
			ND_PRINT(" [%uq]", qdcount);
		/* Print QUESTION section on -vv */
		cp = (const u_char *)(np + 1);
		for (i = 0; i < qdcount; i++) {
			if (i != 0)
				ND_PRINT(",");
			if (ndo->ndo_vflag > 1) {
				ND_PRINT(" q:");
				if ((cp = ns_qprint(ndo, cp, bp, is_mdns)) == NULL)
					goto trunc;
			} else {
				if ((cp = ns_nskip(ndo, cp)) == NULL)
					goto trunc;
				cp += 4;	/* skip QTYPE and QCLASS */
			}
		}
		ND_PRINT(" %u/%u/%u", ancount, nscount, arcount);
		if (ancount) {
			if ((cp = ns_rprint(ndo, cp, bp, is_mdns)) == NULL)
				goto trunc;
			ancount--;
			while (cp < ndo->ndo_snapend && ancount) {
				ND_PRINT(",");
				if ((cp = ns_rprint(ndo, cp, bp, is_mdns)) == NULL)
					goto trunc;
				ancount--;
			}
		}
		if (ancount)
			goto trunc;
		/* Print NS and AR sections on -vv */
		if (ndo->ndo_vflag > 1) {
			if (cp < ndo->ndo_snapend && nscount) {
				ND_PRINT(" ns:");
				if ((cp = ns_rprint(ndo, cp, bp, is_mdns)) == NULL)
					goto trunc;
				nscount--;
				while (cp < ndo->ndo_snapend && nscount) {
					ND_PRINT(",");
					if ((cp = ns_rprint(ndo, cp, bp, is_mdns)) == NULL)
						goto trunc;
					nscount--;
				}
			}
			if (nscount)
				goto trunc;
			if (cp < ndo->ndo_snapend && arcount) {
				ND_PRINT(" ar:");
				if ((cp = ns_rprint(ndo, cp, bp, is_mdns)) == NULL)
					goto trunc;
				arcount--;
				while (cp < ndo->ndo_snapend && arcount) {
					ND_PRINT(",");
					if ((cp = ns_rprint(ndo, cp, bp, is_mdns)) == NULL)
						goto trunc;
					arcount--;
				}
			}
			if (arcount)
				goto trunc;
		}
	}
	else {
		/* this is a request */
		ND_PRINT("%u%s%s%s", GET_BE_U_2(np->id),
			  ns_ops[DNS_OPCODE(flags)],
			  DNS_RD(flags) ? "+" : "",
			  DNS_CD(flags) ? "%" : "");

		/* any weirdness? */
		b2 = GET_BE_U_2(((const u_short *)np) + 1);
		if (b2 & 0x6cf)
			ND_PRINT(" [b2&3=0x%x]", b2);

		if (DNS_OPCODE(flags) == IQUERY) {
			if (qdcount)
				ND_PRINT(" [%uq]", qdcount);
			if (ancount != 1)
				ND_PRINT(" [%ua]", ancount);
		}
		else {
			if (ancount)
				ND_PRINT(" [%ua]", ancount);
			if (qdcount != 1)
				ND_PRINT(" [%uq]", qdcount);
		}
		if (nscount)
			ND_PRINT(" [%un]", nscount);
		if (arcount)
			ND_PRINT(" [%uau]", arcount);

		cp = (const u_char *)(np + 1);
		if (qdcount) {
			cp = ns_qprint(ndo, cp, (const u_char *)np, is_mdns);
			if (!cp)
				goto trunc;
			qdcount--;
			while (cp < ndo->ndo_snapend && qdcount) {
				cp = ns_qprint(ndo, (const u_char *)cp,
					       (const u_char *)np,
					       is_mdns);
				if (!cp)
					goto trunc;
				qdcount--;
			}
		}
		if (qdcount)
			goto trunc;

		/* Print remaining sections on -vv */
		if (ndo->ndo_vflag > 1) {
			if (ancount) {
				if ((cp = ns_rprint(ndo, cp, bp, is_mdns)) == NULL)
					goto trunc;
				ancount--;
				while (cp < ndo->ndo_snapend && ancount) {
					ND_PRINT(",");
					if ((cp = ns_rprint(ndo, cp, bp, is_mdns)) == NULL)
						goto trunc;
					ancount--;
				}
			}
			if (ancount)
				goto trunc;
			if (cp < ndo->ndo_snapend && nscount) {
				ND_PRINT(" ns:");
				if ((cp = ns_rprint(ndo, cp, bp, is_mdns)) == NULL)
					goto trunc;
				nscount--;
				while (cp < ndo->ndo_snapend && nscount) {
					ND_PRINT(",");
					if ((cp = ns_rprint(ndo, cp, bp, is_mdns)) == NULL)
						goto trunc;
					nscount--;
				}
			}
			if (nscount > 0)
				goto trunc;
			if (cp < ndo->ndo_snapend && arcount) {
				ND_PRINT(" ar:");
				if ((cp = ns_rprint(ndo, cp, bp, is_mdns)) == NULL)
					goto trunc;
				arcount--;
				while (cp < ndo->ndo_snapend && arcount) {
					ND_PRINT(",");
					if ((cp = ns_rprint(ndo, cp, bp, is_mdns)) == NULL)
						goto trunc;
					arcount--;
				}
			}
			if (arcount)
				goto trunc;
		}
	}
	ND_PRINT(" (%u)", length);
	return;

  trunc:
	nd_print_trunc(ndo);
}
