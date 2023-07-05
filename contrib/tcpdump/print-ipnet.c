/* \summary: Solaris DLT_IPNET printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"


typedef struct ipnet_hdr {
	nd_uint8_t	iph_version;
	nd_uint8_t	iph_family;
	nd_uint16_t	iph_htype;
	nd_uint32_t	iph_pktlen;
	nd_uint32_t	iph_ifindex;
	nd_uint32_t	iph_grifindex;
	nd_uint32_t	iph_zsrc;
	nd_uint32_t	iph_zdst;
} ipnet_hdr_t;

#define	IPH_AF_INET	2		/* Matches Solaris's AF_INET */
#define	IPH_AF_INET6	26		/* Matches Solaris's AF_INET6 */

#ifdef DLT_IPNET

static const struct tok ipnet_values[] = {
	{ IPH_AF_INET,		"IPv4" },
	{ IPH_AF_INET6,		"IPv6" },
	{ 0,			NULL }
};

static void
ipnet_hdr_print(netdissect_options *ndo, const u_char *bp, u_int length)
{
	const ipnet_hdr_t *hdr;
	hdr = (const ipnet_hdr_t *)bp;

	ND_PRINT("%u > %u", GET_BE_U_4(hdr->iph_zsrc),
		  GET_BE_U_4(hdr->iph_zdst));

	if (!ndo->ndo_qflag) {
		ND_PRINT(", family %s (%u)",
                          tok2str(ipnet_values, "Unknown",
                                  GET_U_1(hdr->iph_family)),
                          GET_U_1(hdr->iph_family));
        } else {
		ND_PRINT(", %s",
                          tok2str(ipnet_values,
                                  "Unknown Ethertype (0x%04x)",
				  GET_U_1(hdr->iph_family)));
        }

	ND_PRINT(", length %u: ", length);
}

static void
ipnet_print(netdissect_options *ndo, const u_char *p, u_int length, u_int caplen)
{
	const ipnet_hdr_t *hdr;

	ND_TCHECK_LEN(p, sizeof(ipnet_hdr_t));
	ndo->ndo_ll_hdr_len += sizeof(ipnet_hdr_t);

	if (ndo->ndo_eflag)
		ipnet_hdr_print(ndo, p, length);

	length -= sizeof(ipnet_hdr_t);
	caplen -= sizeof(ipnet_hdr_t);
	hdr = (const ipnet_hdr_t *)p;
	p += sizeof(ipnet_hdr_t);

	switch (GET_U_1(hdr->iph_family)) {

	case IPH_AF_INET:
	        ip_print(ndo, p, length);
		break;

	case IPH_AF_INET6:
		ip6_print(ndo, p, length);
		break;

	default:
		if (!ndo->ndo_eflag)
			ipnet_hdr_print(ndo, (const u_char *)hdr,
					length + sizeof(ipnet_hdr_t));

		if (!ndo->ndo_suppress_default_print)
			ND_DEFAULTPRINT(p, caplen);
		break;
	}
}

/*
 * This is the top level routine of the printer.  'p' points
 * to the ether header of the packet, 'h->ts' is the timestamp,
 * 'h->len' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
void
ipnet_if_print(netdissect_options *ndo,
               const struct pcap_pkthdr *h, const u_char *p)
{
	ndo->ndo_protocol = "ipnet";
	ipnet_print(ndo, p, h->len, h->caplen);
}
#endif /* DLT_IPNET */
