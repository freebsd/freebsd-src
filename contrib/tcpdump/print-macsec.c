/* Copyright (c) 2017, Sabrina Dubroca <sd@queasysnail.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *   3. The names of the authors may not be used to endorse or promote
 *      products derived from this software without specific prior
 *      written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* \summary: MACsec printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "ethertype.h"
#include "extract.h"

#define MACSEC_DEFAULT_ICV_LEN 16

/* Header format (SecTAG), following an Ethernet header
 * IEEE 802.1AE-2006 9.3
 *
 * +---------------------------------+----------------+----------------+
 * |        (MACsec ethertype)       |     TCI_AN     |       SL       |
 * +---------------------------------+----------------+----------------+
 * |                           Packet Number                           |
 * +-------------------------------------------------------------------+
 * |                     Secure Channel Identifier                     |
 * |                            (optional)                             |
 * +-------------------------------------------------------------------+
 *
 * MACsec ethertype = 0x88e5
 * TCI: Tag Control Information, set of flags
 * AN: association number, 2 bits
 * SL (short length): 6-bit length of the protected payload, if < 48
 * Packet Number: 32-bits packet identifier
 * Secure Channel Identifier: 64-bit unique identifier, usually
 *     composed of a MAC address + 16-bit port number
 */
struct macsec_sectag {
	nd_uint8_t  tci_an;
	nd_uint8_t  short_length;
	nd_uint32_t packet_number;
	nd_uint8_t  secure_channel_id[8]; /* optional */
};

/* IEEE 802.1AE-2006 9.5 */
#define MACSEC_TCI_VERSION 0x80
#define MACSEC_TCI_ES      0x40 /* end station */
#define MACSEC_TCI_SC      0x20 /* SCI present */
#define MACSEC_TCI_SCB     0x10 /* epon */
#define MACSEC_TCI_E       0x08 /* encryption */
#define MACSEC_TCI_C       0x04 /* changed text */
#define MACSEC_AN_MASK     0x03 /* association number */
#define MACSEC_TCI_FLAGS   (MACSEC_TCI_ES | MACSEC_TCI_SC | MACSEC_TCI_SCB | MACSEC_TCI_E | MACSEC_TCI_C)
#define MACSEC_TCI_CONFID  (MACSEC_TCI_E | MACSEC_TCI_C)
#define MACSEC_SL_MASK     0x3F /* short length */

#define MACSEC_SECTAG_LEN_NOSCI 6  /* length of MACsec header without SCI */
#define MACSEC_SECTAG_LEN_SCI   14 /* length of MACsec header with SCI */

#define SCI_FMT "%016" PRIx64

static const struct tok macsec_flag_values[] = {
	{ MACSEC_TCI_E,   "E" },
	{ MACSEC_TCI_C,   "C" },
	{ MACSEC_TCI_ES,  "S" },
	{ MACSEC_TCI_SCB, "B" },
	{ MACSEC_TCI_SC,  "I" },
	{ 0, NULL }
};

static void macsec_print_header(netdissect_options *ndo,
				const struct macsec_sectag *sectag,
				u_int short_length)
{
	ND_PRINT("an %u, pn %u, flags %s",
		 GET_U_1(sectag->tci_an) & MACSEC_AN_MASK,
		 GET_BE_U_4(sectag->packet_number),
		 bittok2str_nosep(macsec_flag_values, "none",
				  GET_U_1(sectag->tci_an) & MACSEC_TCI_FLAGS));

	if (short_length != 0)
		ND_PRINT(", sl %u", short_length);

	if (GET_U_1(sectag->tci_an) & MACSEC_TCI_SC)
		ND_PRINT(", sci " SCI_FMT, GET_BE_U_8(sectag->secure_channel_id));

	ND_PRINT(", ");
}

/* returns < 0 iff the packet can be decoded completely */
int macsec_print(netdissect_options *ndo, const u_char **bp,
		 u_int *lengthp, u_int *caplenp, u_int *hdrlenp,
		 const struct lladdr_info *src, const struct lladdr_info *dst)
{
	const char *save_protocol;
	const u_char *p = *bp;
	u_int length = *lengthp;
	u_int caplen = *caplenp;
	u_int hdrlen = *hdrlenp;
	const struct macsec_sectag *sectag = (const struct macsec_sectag *)p;
	u_int sectag_len;
	u_int short_length;

	save_protocol = ndo->ndo_protocol;
	ndo->ndo_protocol = "macsec";

	/* we need the full MACsec header in the capture */
	if (caplen < MACSEC_SECTAG_LEN_NOSCI) {
		nd_print_trunc(ndo);
		ndo->ndo_protocol = save_protocol;
		return hdrlen + caplen;
	}
	if (length < MACSEC_SECTAG_LEN_NOSCI) {
		nd_print_trunc(ndo);
		ndo->ndo_protocol = save_protocol;
		return hdrlen + caplen;
	}

	if (GET_U_1(sectag->tci_an) & MACSEC_TCI_SC) {
		sectag_len = MACSEC_SECTAG_LEN_SCI;
		if (caplen < MACSEC_SECTAG_LEN_SCI) {
			nd_print_trunc(ndo);
			ndo->ndo_protocol = save_protocol;
			return hdrlen + caplen;
		}
		if (length < MACSEC_SECTAG_LEN_SCI) {
			nd_print_trunc(ndo);
			ndo->ndo_protocol = save_protocol;
			return hdrlen + caplen;
		}
	} else
		sectag_len = MACSEC_SECTAG_LEN_NOSCI;

	if ((GET_U_1(sectag->short_length) & ~MACSEC_SL_MASK) != 0 ||
	    GET_U_1(sectag->tci_an) & MACSEC_TCI_VERSION) {
		nd_print_invalid(ndo);
		ndo->ndo_protocol = save_protocol;
		return hdrlen + caplen;
	}

	short_length = GET_U_1(sectag->short_length) & MACSEC_SL_MASK;
	if (ndo->ndo_eflag)
		macsec_print_header(ndo, sectag, short_length);

	/* Skip the MACsec header. */
	*bp += sectag_len;
	*hdrlenp += sectag_len;

	/* Remove it from the lengths, as it's been processed. */
	*lengthp -= sectag_len;
	*caplenp -= sectag_len;

	if ((GET_U_1(sectag->tci_an) & MACSEC_TCI_CONFID)) {
		/*
		 * The payload is encrypted.  Print link-layer
		 * information, if it hasn't already been printed.
		 */
		if (!ndo->ndo_eflag) {
			/*
			 * Nobody printed the link-layer addresses,
			 * so print them, if we have any.
			 */
			if (src != NULL && dst != NULL) {
				ND_PRINT("%s > %s ",
					(src->addr_string)(ndo, src->addr),
					(dst->addr_string)(ndo, dst->addr));
			}

			ND_PRINT("802.1AE MACsec, ");

			/*
			 * Print the MACsec header.
			 */
			macsec_print_header(ndo, sectag, short_length);
		}

		/*
		 * Tell our caller it can't be dissected.
		 */
		ndo->ndo_protocol = save_protocol;
		return 0;
	}

	/*
	 * The payload isn't encrypted; remove the
	 * ICV length from the lengths, so our caller
	 * doesn't treat it as payload.
	 */
	if (*lengthp < MACSEC_DEFAULT_ICV_LEN) {
		nd_print_trunc(ndo);
		ndo->ndo_protocol = save_protocol;
		return hdrlen + caplen;
	}
	if (*caplenp < MACSEC_DEFAULT_ICV_LEN) {
		nd_print_trunc(ndo);
		ndo->ndo_protocol = save_protocol;
		return hdrlen + caplen;
	}
	*lengthp -= MACSEC_DEFAULT_ICV_LEN;
	*caplenp -= MACSEC_DEFAULT_ICV_LEN;
	/*
	 * Update the snapend thus the ICV field is not in the payload for
	 * the caller.
	 * The ICV (Integrity Check Value) is at the end of the frame, after
	 * the secure data.
	 */
	ndo->ndo_snapend -= MACSEC_DEFAULT_ICV_LEN;

	/*
	 * If the SL field is non-zero, then it's the length of the
	 * Secure Data; otherwise, the Secure Data is what's left
	 * ver after the MACsec header and ICV are removed.
	 */
	if (short_length != 0) {
		/*
		 * If the short length is more than we *have*,
		 * that's an error.
		 */
		if (short_length > *lengthp) {
			nd_print_trunc(ndo);
			ndo->ndo_protocol = save_protocol;
			return hdrlen + caplen;
		}
		if (short_length > *caplenp) {
			nd_print_trunc(ndo);
			ndo->ndo_protocol = save_protocol;
			return hdrlen + caplen;
		}
		if (*lengthp > short_length)
			*lengthp = short_length;
		if (*caplenp > short_length)
			*caplenp = short_length;
	}

	ndo->ndo_protocol = save_protocol;
	return -1;
}
