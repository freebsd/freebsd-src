/*
 * Marko Kiiskila carnil@cs.tut.fi
 *
 * Tampere University of Technology - Telecommunications Laboratory
 *
 * Permission to use, copy, modify and distribute this
 * software and its documentation is hereby granted,
 * provided that both the copyright notice and this
 * permission notice appear in all copies of the software,
 * derivative works or modified versions, and any portions
 * thereof, that both notices appear in supporting
 * documentation, and that the use of this software is
 * acknowledged in any publications resulting from using
 * the software.
 *
 * TUT ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION AND DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS
 * SOFTWARE.
 *
 */

/* \summary: ATM LANE printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"

struct lecdatahdr_8023 {
  nd_uint16_t le_header;
  nd_mac_addr h_dest;
  nd_mac_addr h_source;
  nd_uint16_t h_type;
};

struct lane_controlhdr {
  nd_uint16_t lec_header;
  nd_uint8_t  lec_proto;
  nd_uint8_t  lec_vers;
  nd_uint16_t lec_opcode;
};

static const struct tok lecop2str[] = {
	{ 0x0001,	"configure request" },
	{ 0x0101,	"configure response" },
	{ 0x0002,	"join request" },
	{ 0x0102,	"join response" },
	{ 0x0003,	"ready query" },
	{ 0x0103,	"ready indication" },
	{ 0x0004,	"register request" },
	{ 0x0104,	"register response" },
	{ 0x0005,	"unregister request" },
	{ 0x0105,	"unregister response" },
	{ 0x0006,	"ARP request" },
	{ 0x0106,	"ARP response" },
	{ 0x0007,	"flush request" },
	{ 0x0107,	"flush response" },
	{ 0x0008,	"NARP request" },
	{ 0x0009,	"topology request" },
	{ 0,		NULL }
};

static void
lane_hdr_print(netdissect_options *ndo, const u_char *bp)
{
	ND_PRINT("lecid:%x ", GET_BE_U_2(bp));
}

/*
 * This assumes 802.3, not 802.5, LAN emulation.
 */
void
lane_print(netdissect_options *ndo, const u_char *p, u_int length, u_int caplen)
{
	const struct lane_controlhdr *lec;

	ndo->ndo_protocol = "lane";

	lec = (const struct lane_controlhdr *)p;
	if (GET_BE_U_2(lec->lec_header) == 0xff00) {
		/*
		 * LE Control.
		 */
		ND_PRINT("lec: proto %x vers %x %s",
			 GET_U_1(lec->lec_proto),
			 GET_U_1(lec->lec_vers),
			 tok2str(lecop2str, "opcode-#%u", GET_BE_U_2(lec->lec_opcode)));
		return;
	}

	/*
	 * Go past the LE header.
	 */
	ND_TCHECK_2(p); /* Needed */
	length -= 2;
	caplen -= 2;
	p += 2;

	/*
	 * Now print the encapsulated frame, under the assumption
	 * that it's an Ethernet frame.
	 */
	ether_print(ndo, p, length, caplen, lane_hdr_print, p - 2);
}
