// Copyright (c) 2018 Arista Networks, Inc.  All rights reserved.

/* \summary: EtherType protocol for Arista Networks printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"

/*

From Bill Fenner:

The Arista timestamp header consists of the following fields:
1. The Arista ethertype (0xd28b)
2. A 2-byte subtype field; 0x01 indicates the timestamp header
3. A 2-byte version field, described below.
4. A 48-bit or 64-bit timestamp field, depending on the contents of the version field

This header is then followed by the original ethertype and the remainder of the original packet.

 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            dst mac                            |
+                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                               |                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
|                            src mac                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|        ethertype 0xd28b       |          subtype 0x1          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|            version            |                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
|                          timestamp...                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

The two-byte version value is split into 3 fields:
1. The timescale in use.  Currently assigned values include:
    0 = TAI
    1 = UTC
2. The timestamp format and length.  Currently assigned values include:
    1 = 64-bit timestamp
    2 = 48-bit timestamp
3. The hardware info
    0 = R/R2 series
    1 = R3 series

 0                   1
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   timescale   | format|hw info|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


See also: https://www.arista.com/assets/data/pdf/Whitepapers/Overview_Arista_Timestamps.pdf

*/

#define ARISTA_SUBTYPE_TIMESTAMP 0x0001
static const struct tok subtype_str[] = {
	{ ARISTA_SUBTYPE_TIMESTAMP, "Timestamp" },
	{ 0, NULL }
};

static const struct tok ts_timescale_str[] = {
	{ 0, "TAI" },
	{ 1, "UTC" },
	{ 0, NULL }
};

#define FORMAT_64BIT 0x1
#define FORMAT_48BIT 0x2
static const struct tok ts_format_str[] = {
	{ FORMAT_64BIT, "64-bit" },
	{ FORMAT_48BIT, "48-bit" },
	{ 0, NULL }
};

static const struct tok hw_info_str[] = {
	{ 0, "R/R2" },
	{ 1, "R3" },
	{ 0, NULL }
};

static inline void
arista_print_date_hms_time(netdissect_options *ndo, uint32_t seconds,
		uint32_t nanoseconds)
{
	time_t ts;
	char buf[sizeof("-yyyyyyyyyy-mm-dd hh:mm:ss")];

	ts = seconds + (nanoseconds / 1000000000);
	nanoseconds %= 1000000000;
	ND_PRINT("%s.%09u",
	    nd_format_time(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S",
	       gmtime(&ts)), nanoseconds);
}

int
arista_ethertype_print(netdissect_options *ndo, const u_char *bp, u_int len _U_)
{
	uint16_t subTypeId;
	u_short bytesConsumed = 0;

	ndo->ndo_protocol = "arista";

	subTypeId = GET_BE_U_2(bp);
	bp += 2;
	bytesConsumed += 2;

	ND_PRINT("SubType %s (0x%04x), ",
	         tok2str(subtype_str, "Unknown", subTypeId),
	         subTypeId);

	// TapAgg Header Timestamping
	if (subTypeId == ARISTA_SUBTYPE_TIMESTAMP) {
		uint64_t seconds;
		uint32_t nanoseconds;
		uint8_t ts_timescale = GET_U_1(bp);
		bp += 1;
		bytesConsumed += 1;
		ND_PRINT("Timescale %s (%u), ",
		         tok2str(ts_timescale_str, "Unknown", ts_timescale),
		         ts_timescale);

		uint8_t ts_format = GET_U_1(bp) >> 4;
		uint8_t hw_info = GET_U_1(bp) & 0x0f;
		bp += 1;
		bytesConsumed += 1;

		// Timestamp has 32-bit lsb in nanosec and remaining msb in sec
		ND_PRINT("Format %s (%u), HwInfo %s (%u), Timestamp ",
		         tok2str(ts_format_str, "Unknown", ts_format),
		         ts_format,
		         tok2str(hw_info_str, "Unknown", hw_info),
		         hw_info);
		switch (ts_format) {
		case FORMAT_64BIT:
			seconds = GET_BE_U_4(bp);
			nanoseconds = GET_BE_U_4(bp + 4);
			arista_print_date_hms_time(ndo, seconds, nanoseconds);
			bytesConsumed += 8;
			break;
		case FORMAT_48BIT:
			seconds = GET_BE_U_2(bp);
			nanoseconds = GET_BE_U_4(bp + 2);
			seconds += nanoseconds / 1000000000;
			nanoseconds %= 1000000000;
			ND_PRINT("%" PRIu64 ".%09u", seconds, nanoseconds);
			bytesConsumed += 6;
			break;
		default:
			return -1;
		}
	} else {
		return -1;
	}
	ND_PRINT(": ");
	return bytesConsumed;
}
