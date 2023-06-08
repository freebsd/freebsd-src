/*
 * Oracle
 */

/* \summary: Per-Packet Information (DLT_PPI) printer */

/* Specification:
 * Per-Packet Information Header Specification - Version 1.0.7
 * https://web.archive.org/web/20160328114748/http://www.cacetech.com/documents/PPI%20Header%20format%201.0.7.pdf
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"


typedef struct ppi_header {
	nd_uint8_t	ppi_ver;	/* Version.  Currently 0 */
	nd_uint8_t	ppi_flags;	/* Flags. */
	nd_uint16_t	ppi_len;	/* Length of entire message, including
					 * this header and TLV payload. */
	nd_uint32_t	ppi_dlt;	/* Data Link Type of the captured
					 * packet data. */
} ppi_header_t;

#define	PPI_HDRLEN	8

#ifdef DLT_PPI

static void
ppi_header_print(netdissect_options *ndo, const u_char *bp, u_int length)
{
	const ppi_header_t *hdr;
	uint16_t len;
	uint32_t dlt;
	const char *dltname;

	hdr = (const ppi_header_t *)bp;

	len = GET_LE_U_2(hdr->ppi_len);
	dlt = GET_LE_U_4(hdr->ppi_dlt);
	dltname = pcap_datalink_val_to_name(dlt);

	if (!ndo->ndo_qflag) {
		ND_PRINT("V.%u DLT %s (%u) len %u", GET_U_1(hdr->ppi_ver),
			  (dltname != NULL ? dltname : "UNKNOWN"), dlt,
                          len);
        } else {
		ND_PRINT("%s", (dltname != NULL ? dltname : "UNKNOWN"));
        }

	ND_PRINT(", length %u: ", length);
}

/*
 * This is the top level routine of the printer.  'p' points
 * to the ether header of the packet, 'h->ts' is the timestamp,
 * 'h->len' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
void
ppi_if_print(netdissect_options *ndo,
	     const struct pcap_pkthdr *h, const u_char *p)
{
	if_printer printer;
	const ppi_header_t *hdr;
	u_int caplen = h->caplen;
	u_int length = h->len;
	uint16_t len;
	uint32_t dlt;
	uint32_t hdrlen;
	struct pcap_pkthdr nhdr;

	ndo->ndo_protocol = "ppi";
	if (caplen < sizeof(ppi_header_t)) {
		nd_print_trunc(ndo);
		ndo->ndo_ll_hdr_len += caplen;
		return;
	}

	hdr = (const ppi_header_t *)p;
	len = GET_LE_U_2(hdr->ppi_len);
	if (len < sizeof(ppi_header_t) || len > 65532) {
		/* It MUST be between 8 and 65,532 inclusive (spec 3.1.3) */
		ND_PRINT(" [length %u < %zu or > 65532]", len,
			 sizeof(ppi_header_t));
		nd_print_invalid(ndo);
		ndo->ndo_ll_hdr_len += caplen;
		return;
	}
	if (caplen < len) {
		/*
		 * If we don't have the entire PPI header, don't
		 * bother.
		 */
		nd_print_trunc(ndo);
		ndo->ndo_ll_hdr_len += caplen;
		return;
	}
	dlt = GET_LE_U_4(hdr->ppi_dlt);

	if (ndo->ndo_eflag)
		ppi_header_print(ndo, p, length);

	length -= len;
	caplen -= len;
	p += len;

	printer = lookup_printer(dlt);
	if (printer != NULL) {
		nhdr = *h;
		nhdr.caplen = caplen;
		nhdr.len = length;
		printer(ndo, &nhdr, p);
		hdrlen = ndo->ndo_ll_hdr_len;
	} else {
		if (!ndo->ndo_eflag)
			ppi_header_print(ndo, (const u_char *)hdr, length + len);

		if (!ndo->ndo_suppress_default_print)
			ND_DEFAULTPRINT(p, caplen);
		hdrlen = 0;
	}
	ndo->ndo_ll_hdr_len += len + hdrlen;
}
#endif /* DLT_PPI */
