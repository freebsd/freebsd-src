/*
 * Copyright (c) 2009
 *	Siemens AG, All rights reserved.
 *	Dmitry Eremin-Solenikov (dbaryshkov@gmail.com)
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

/* \summary: IEEE 802.15.4 printer */

#include <config.h>

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "addrtoname.h"

#include "extract.h"

#define CHECK_BIT(num,bit) (((num) >> (bit)) & 0x1)

#define BROKEN_6TISCH_PAN_ID_COMPRESSION 0

/* Frame types from Table 7-1 of 802.15.4-2015 */
static const char *ftypes[] = {
	"Beacon",			/* 0 */
	"Data",				/* 1 */
	"ACK",				/* 2 */
	"Command",			/* 3 */
	"Reserved",			/* 4 */
	"Multipurpose",			/* 5 */
	"Fragment",			/* 6 */
	"Extended"			/* 7 */
};

/* Element IDs for Header IEs from Table 7-7 of 802.15.4-2015 */
static const char *h_ie_names[] = {
	"Vendor Specific Header IE",			/* 0x00 */
	"Reserved 0x01",				/* 0x01 */
	"Reserved 0x02",				/* 0x02 */
	"Reserved 0x03",				/* 0x03 */
	"Reserved 0x04",				/* 0x04 */
	"Reserved 0x05",				/* 0x05 */
	"Reserved 0x06",				/* 0x06 */
	"Reserved 0x07",				/* 0x07 */
	"Reserved 0x08",				/* 0x08 */
	"Reserved 0x09",				/* 0x09 */
	"Reserved 0x0a",				/* 0x0a */
	"Reserved 0x0b",				/* 0x0b */
	"Reserved 0x0c",				/* 0x0c */
	"Reserved 0x0d",				/* 0x0d */
	"Reserved 0x0e",				/* 0x0e */
	"Reserved 0x0f",				/* 0x0f */
	"Reserved 0x10",				/* 0x10 */
	"Reserved 0x11",				/* 0x11 */
	"Reserved 0x12",				/* 0x12 */
	"Reserved 0x13",				/* 0x13 */
	"Reserved 0x14",				/* 0x14 */
	"Reserved 0x15",				/* 0x15 */
	"Reserved 0x16",				/* 0x16 */
	"Reserved 0x17",				/* 0x17 */
	"Reserved 0x18",				/* 0x18 */
	"Reserved 0x19",				/* 0x19 */
	"LE CSL IE",					/* 0x1a */
	"LE RIT IE",					/* 0x1b */
	"DSME PAN descriptor IE",			/* 0x1c */
	"Rendezvous Time IE",				/* 0x1d */
	"Time Correction IE",				/* 0x1e */
	"Reserved 0x1f",				/* 0x1f */
	"Reserved 0x20",				/* 0x20 */
	"Extended DSME PAN descriptor IE",		/* 0x21 */
	"Fragment Sequence Context Description IE",	/* 0x22 */
	"Simplified Superframe Specification IE",	/* 0x23 */
	"Simplified GTS Specification IE",		/* 0x24 */
	"LECIM Capabilities IE",			/* 0x25 */
	"TRLE Descriptor IE",				/* 0x26 */
	"RCC Capabilities IE",				/* 0x27 */
	"RCCN Descriptor IE",				/* 0x28 */
	"Global Time IE",				/* 0x29 */
	"Omnibus Header IE",				/* 0x2a */
	"DA IE",					/* 0x2b */
	"Reserved 0x2c",				/* 0x2c */
	"Reserved 0x2d",				/* 0x2d */
	"Reserved 0x2e",				/* 0x2e */
	"Reserved 0x2f",				/* 0x2f */
	"Reserved 0x30",				/* 0x30 */
	"Reserved 0x31",				/* 0x31 */
	"Reserved 0x32",				/* 0x32 */
	"Reserved 0x33",				/* 0x33 */
	"Reserved 0x34",				/* 0x34 */
	"Reserved 0x35",				/* 0x35 */
	"Reserved 0x36",				/* 0x36 */
	"Reserved 0x37",				/* 0x37 */
	"Reserved 0x38",				/* 0x38 */
	"Reserved 0x39",				/* 0x39 */
	"Reserved 0x3a",				/* 0x3a */
	"Reserved 0x3b",				/* 0x3b */
	"Reserved 0x3c",				/* 0x3c */
	"Reserved 0x3d",				/* 0x3d */
	"Reserved 0x3e",				/* 0x3e */
	"Reserved 0x3f",				/* 0x3f */
	"Reserved 0x40",				/* 0x40 */
	"Reserved 0x41",				/* 0x41 */
	"Reserved 0x42",				/* 0x42 */
	"Reserved 0x43",				/* 0x43 */
	"Reserved 0x44",				/* 0x44 */
	"Reserved 0x45",				/* 0x45 */
	"Reserved 0x46",				/* 0x46 */
	"Reserved 0x47",				/* 0x47 */
	"Reserved 0x48",				/* 0x48 */
	"Reserved 0x49",				/* 0x49 */
	"Reserved 0x4a",				/* 0x4a */
	"Reserved 0x4b",				/* 0x4b */
	"Reserved 0x4c",				/* 0x4c */
	"Reserved 0x4d",				/* 0x4d */
	"Reserved 0x4e",				/* 0x4e */
	"Reserved 0x4f",				/* 0x4f */
	"Reserved 0x50",				/* 0x50 */
	"Reserved 0x51",				/* 0x51 */
	"Reserved 0x52",				/* 0x52 */
	"Reserved 0x53",				/* 0x53 */
	"Reserved 0x54",				/* 0x54 */
	"Reserved 0x55",				/* 0x55 */
	"Reserved 0x56",				/* 0x56 */
	"Reserved 0x57",				/* 0x57 */
	"Reserved 0x58",				/* 0x58 */
	"Reserved 0x59",				/* 0x59 */
	"Reserved 0x5a",				/* 0x5a */
	"Reserved 0x5b",				/* 0x5b */
	"Reserved 0x5c",				/* 0x5c */
	"Reserved 0x5d",				/* 0x5d */
	"Reserved 0x5e",				/* 0x5e */
	"Reserved 0x5f",				/* 0x5f */
	"Reserved 0x60",				/* 0x60 */
	"Reserved 0x61",				/* 0x61 */
	"Reserved 0x62",				/* 0x62 */
	"Reserved 0x63",				/* 0x63 */
	"Reserved 0x64",				/* 0x64 */
	"Reserved 0x65",				/* 0x65 */
	"Reserved 0x66",				/* 0x66 */
	"Reserved 0x67",				/* 0x67 */
	"Reserved 0x68",				/* 0x68 */
	"Reserved 0x69",				/* 0x69 */
	"Reserved 0x6a",				/* 0x6a */
	"Reserved 0x6b",				/* 0x6b */
	"Reserved 0x6c",				/* 0x6c */
	"Reserved 0x6d",				/* 0x6d */
	"Reserved 0x6e",				/* 0x6e */
	"Reserved 0x6f",				/* 0x6f */
	"Reserved 0x70",				/* 0x70 */
	"Reserved 0x71",				/* 0x71 */
	"Reserved 0x72",				/* 0x72 */
	"Reserved 0x73",				/* 0x73 */
	"Reserved 0x74",				/* 0x74 */
	"Reserved 0x75",				/* 0x75 */
	"Reserved 0x76",				/* 0x76 */
	"Reserved 0x77",				/* 0x77 */
	"Reserved 0x78",				/* 0x78 */
	"Reserved 0x79",				/* 0x79 */
	"Reserved 0x7a",				/* 0x7a */
	"Reserved 0x7b",				/* 0x7b */
	"Reserved 0x7c",				/* 0x7c */
	"Reserved 0x7d",				/* 0x7d */
	"Header Termination 1 IE",			/* 0x7e */
	"Header Termination 2 IE"			/* 0x7f */
};

/* Payload IE Group IDs from Table 7-15 of 802.15.4-2015 */
static const char *p_ie_names[] = {
	"ESDU IE",			/* 0x00 */
	"MLME IE",			/* 0x01 */
	"Vendor Specific Nested IE",	/* 0x02 */
	"Multiplexed IE (802.15.9)",	/* 0x03 */
	"Omnibus Payload Group IE",	/* 0x04 */
	"IETF IE",			/* 0x05 */
	"Reserved 0x06",		/* 0x06 */
	"Reserved 0x07",		/* 0x07 */
	"Reserved 0x08",		/* 0x08 */
	"Reserved 0x09",		/* 0x09 */
	"Reserved 0x0a",		/* 0x0a */
	"Reserved 0x0b",		/* 0x0b */
	"Reserved 0x0c",		/* 0x0c */
	"Reserved 0x0d",		/* 0x0d */
	"Reserved 0x0e",		/* 0x0e */
	"List termination"		/* 0x0f */
};

/* Sub-ID for short format from Table 7-16 of 802.15.4-2015 */
static const char *p_mlme_short_names[] = {
	"Reserved for long format 0x0",			/* 0x00 */
	"Reserved for long format 0x1",			/* 0x01 */
	"Reserved for long format 0x2",			/* 0x02 */
	"Reserved for long format 0x3",			/* 0x03 */
	"Reserved for long format 0x4",			/* 0x04 */
	"Reserved for long format 0x5",			/* 0x05 */
	"Reserved for long format 0x6",			/* 0x06 */
	"Reserved for long format 0x7",			/* 0x07 */
	"Reserved for long format 0x8",			/* 0x08 */
	"Reserved for long format 0x9",			/* 0x09 */
	"Reserved for long format 0xa",			/* 0x0a */
	"Reserved for long format 0xb",			/* 0x0b */
	"Reserved for long format 0xc",			/* 0x0c */
	"Reserved for long format 0xd",			/* 0x0d */
	"Reserved for long format 0xe",			/* 0x0e */
	"Reserved for long format 0xf",			/* 0x0f */
	"Reserved 0x10",				/* 0x10 */
	"Reserved 0x11",				/* 0x11 */
	"Reserved 0x12",				/* 0x12 */
	"Reserved 0x13",				/* 0x13 */
	"Reserved 0x14",				/* 0x14 */
	"Reserved 0x15",				/* 0x15 */
	"Reserved 0x16",				/* 0x16 */
	"Reserved 0x17",				/* 0x17 */
	"Reserved 0x18",				/* 0x18 */
	"Reserved 0x19",				/* 0x19 */
	"TSCH Synchronization IE",			/* 0x1a */
	"TSCH Slotframe and Link IE",			/* 0x1b */
	"TSCH Timeslot IE",				/* 0x1c */
	"Hopping timing IE",				/* 0x1d */
	"Enhanced Beacon Filter IE",			/* 0x1e */
	"MAC Metrics IE",				/* 0x1f */
	"All MAC Metrics IE",				/* 0x20 */
	"Coexistence Specification IE",			/* 0x21 */
	"SUN Device Capabilities IE",			/* 0x22 */
	"SUN FSK Generic PHY IE",			/* 0x23 */
	"Mode Switch Parameter IE",			/* 0x24 */
	"PHY Parameter Change IE",			/* 0x25 */
	"O-QPSK PHY Mode IE",				/* 0x26 */
	"PCA Allocation IE",				/* 0x27 */
	"LECIM DSSS Operating Mode IE",			/* 0x28 */
	"LECIM FSK Operating Mode IE",			/* 0x29 */
	"Reserved 0x2a",				/* 0x2a */
	"TVWS PHY Operating Mode Description IE",	/* 0x2b */
	"TVWS Device Capabilities IE",			/* 0x2c */
	"TVWS Device Category IE",			/* 0x2d */
	"TVWS Device Identification IE",		/* 0x2e */
	"TVWS Device Location IE",			/* 0x2f */
	"TVWS Channel Information Query IE",		/* 0x30 */
	"TVWS Channel Information Source IE",		/* 0x31 */
	"CTM IE",					/* 0x32 */
	"Timestamp IE",					/* 0x33 */
	"Timestamp Difference IE",			/* 0x34 */
	"TMCTP Specification IE",			/* 0x35 */
	"RCC PHY Operating Mode IE",			/* 0x36 */
	"Reserved 0x37",				/* 0x37 */
	"Reserved 0x38",				/* 0x38 */
	"Reserved 0x39",				/* 0x39 */
	"Reserved 0x3a",				/* 0x3a */
	"Reserved 0x3b",				/* 0x3b */
	"Reserved 0x3c",				/* 0x3c */
	"Reserved 0x3d",				/* 0x3d */
	"Reserved 0x3e",				/* 0x3e */
	"Reserved 0x3f",				/* 0x3f */
	"Reserved 0x40",				/* 0x40 */
	"Reserved 0x41",				/* 0x41 */
	"Reserved 0x42",				/* 0x42 */
	"Reserved 0x43",				/* 0x43 */
	"Reserved 0x44",				/* 0x44 */
	"Reserved 0x45",				/* 0x45 */
	"Reserved 0x46",				/* 0x46 */
	"Reserved 0x47",				/* 0x47 */
	"Reserved 0x48",				/* 0x48 */
	"Reserved 0x49",				/* 0x49 */
	"Reserved 0x4a",				/* 0x4a */
	"Reserved 0x4b",				/* 0x4b */
	"Reserved 0x4c",				/* 0x4c */
	"Reserved 0x4d",				/* 0x4d */
	"Reserved 0x4e",				/* 0x4e */
	"Reserved 0x4f",				/* 0x4f */
	"Reserved 0x50",				/* 0x50 */
	"Reserved 0x51",				/* 0x51 */
	"Reserved 0x52",				/* 0x52 */
	"Reserved 0x53",				/* 0x53 */
	"Reserved 0x54",				/* 0x54 */
	"Reserved 0x55",				/* 0x55 */
	"Reserved 0x56",				/* 0x56 */
	"Reserved 0x57",				/* 0x57 */
	"Reserved 0x58",				/* 0x58 */
	"Reserved 0x59",				/* 0x59 */
	"Reserved 0x5a",				/* 0x5a */
	"Reserved 0x5b",				/* 0x5b */
	"Reserved 0x5c",				/* 0x5c */
	"Reserved 0x5d",				/* 0x5d */
	"Reserved 0x5e",				/* 0x5e */
	"Reserved 0x5f",				/* 0x5f */
	"Reserved 0x60",				/* 0x60 */
	"Reserved 0x61",				/* 0x61 */
	"Reserved 0x62",				/* 0x62 */
	"Reserved 0x63",				/* 0x63 */
	"Reserved 0x64",				/* 0x64 */
	"Reserved 0x65",				/* 0x65 */
	"Reserved 0x66",				/* 0x66 */
	"Reserved 0x67",				/* 0x67 */
	"Reserved 0x68",				/* 0x68 */
	"Reserved 0x69",				/* 0x69 */
	"Reserved 0x6a",				/* 0x6a */
	"Reserved 0x6b",				/* 0x6b */
	"Reserved 0x6c",				/* 0x6c */
	"Reserved 0x6d",				/* 0x6d */
	"Reserved 0x6e",				/* 0x6e */
	"Reserved 0x6f",				/* 0x6f */
	"Reserved 0x70",				/* 0x70 */
	"Reserved 0x71",				/* 0x71 */
	"Reserved 0x72",				/* 0x72 */
	"Reserved 0x73",				/* 0x73 */
	"Reserved 0x74",				/* 0x74 */
	"Reserved 0x75",				/* 0x75 */
	"Reserved 0x76",				/* 0x76 */
	"Reserved 0x77",				/* 0x77 */
	"Reserved 0x78",				/* 0x78 */
	"Reserved 0x79",				/* 0x79 */
	"Reserved 0x7a",				/* 0x7a */
	"Reserved 0x7b",				/* 0x7b */
	"Reserved 0x7c",				/* 0x7c */
	"Reserved 0x7d",				/* 0x7d */
	"Reserved 0x7e",				/* 0x7e */
	"Reserved 0x7f"					/* 0x7f */
};

/* Sub-ID for long format from Table 7-17 of 802.15.4-2015 */
static const char *p_mlme_long_names[] = {
	"Reserved 0x00",			/* 0x00 */
	"Reserved 0x01",			/* 0x01 */
	"Reserved 0x02",			/* 0x02 */
	"Reserved 0x03",			/* 0x03 */
	"Reserved 0x04",			/* 0x04 */
	"Reserved 0x05",			/* 0x05 */
	"Reserved 0x06",			/* 0x06 */
	"Reserved 0x07",			/* 0x07 */
	"Vendor Specific MLME Nested IE",	/* 0x08 */
	"Channel Hopping IE",			/* 0x09 */
	"Reserved 0x0a",			/* 0x0a */
	"Reserved 0x0b",			/* 0x0b */
	"Reserved 0x0c",			/* 0x0c */
	"Reserved 0x0d",			/* 0x0d */
	"Reserved 0x0e",			/* 0x0e */
	"Reserved 0x0f"				/* 0x0f */
};

/* MAC commands from Table 7-49 of 802.15.4-2015 */
static const char *mac_c_names[] = {
	"Reserved 0x00",				/* 0x00 */
	"Association Request command",			/* 0x01 */
	"Association Response command",			/* 0x02 */
	"Disassociation Notification command",		/* 0x03 */
	"Data Request command",				/* 0x04 */
	"PAN ID Conflict Notification command",		/* 0x05 */
	"Orphan Notification command",			/* 0x06 */
	"Beacon Request command",			/* 0x07 */
	"Coordinator realignment command",		/* 0x08 */
	"GTS request command",				/* 0x09 */
	"TRLE Management Request command",		/* 0x0a */
	"TRLE Management Response command",		/* 0x0b */
	"Reserved 0x0c",				/* 0x0c */
	"Reserved 0x0d",				/* 0x0d */
	"Reserved 0x0e",				/* 0x0e */
	"Reserved 0x0f",				/* 0x0f */
	"Reserved 0x10",				/* 0x10 */
	"Reserved 0x11",				/* 0x11 */
	"Reserved 0x12",				/* 0x12 */
	"DSME Association Request command",		/* 0x13 */
	"DSME Association Response command",		/* 0x14 */
	"DSME GTS Request command",			/* 0x15 */
	"DSME GTS Response command",			/* 0x16 */
	"DSME GTS Notify command",			/* 0x17 */
	"DSME Information Request command",		/* 0x18 */
	"DSME Information Response command",		/* 0x19 */
	"DSME Beacon Allocation Notification command",	/* 0x1a */
	"DSME Beacon Collision Notification command",	/* 0x1b */
	"DSME Link Report command",			/* 0x1c */
	"Reserved 0x1d",				/* 0x1d */
	"Reserved 0x1e",				/* 0x1e */
	"Reserved 0x1f",				/* 0x1f */
	"RIT Data Request command",			/* 0x20 */
	"DBS Request command",				/* 0x21 */
	"DBS Response command",				/* 0x22 */
	"RIT Data Response command",			/* 0x23 */
	"Vendor Specific command",			/* 0x24 */
	"Reserved 0x25",				/* 0x25 */
	"Reserved 0x26",				/* 0x26 */
	"Reserved 0x27",				/* 0x27 */
	"Reserved 0x28",				/* 0x28 */
	"Reserved 0x29",				/* 0x29 */
	"Reserved 0x2a",				/* 0x2a */
	"Reserved 0x2b",				/* 0x2b */
	"Reserved 0x2c",				/* 0x2c */
	"Reserved 0x2d",				/* 0x2d */
	"Reserved 0x2e",				/* 0x2e */
	"Reserved 0x2f"					/* 0x2f */
};

/*
 * Frame Control subfields.
 */
#define FC_FRAME_TYPE(fc)              ((fc) & 0x7)
#define FC_FRAME_VERSION(fc)           (((fc) >> 12) & 0x3)

#define FC_ADDRESSING_MODE_NONE         0x00
#define FC_ADDRESSING_MODE_RESERVED     0x01
#define FC_ADDRESSING_MODE_SHORT        0x02
#define FC_ADDRESSING_MODE_LONG         0x03

/*
 * IEEE 802.15.4 CRC 16 function. This is using the CCITT polynomial of 0x1021,
 * but the initial value is 0, and the bits are reversed for both in and out.
 * See section 7.2.10 of 802.15.4-2015 for more information.
 */
static uint16_t
ieee802_15_4_crc16(netdissect_options *ndo, const u_char *p,
		   u_int data_len)
{
	uint16_t crc;
	u_char x, y;

	crc = 0x0000; /* Note, initial value is 0x0000 not 0xffff. */

	while (data_len != 0){
		y = GET_U_1(p);
		p++;
		/* Reverse bits on input */
		y = (((y & 0xaa) >> 1) | ((y & 0x55) << 1));
		y = (((y & 0xcc) >> 2) | ((y & 0x33) << 2));
		y = (((y & 0xf0) >> 4) | ((y & 0x0f) << 4));
		/* Update CRC */
		x = crc >> 8 ^ y;
		x ^= x >> 4;
		crc = ((uint16_t)(crc << 8)) ^
			((uint16_t)(x << 12)) ^
			((uint16_t)(x << 5)) ^
			((uint16_t)x);
		data_len--;
	}
	/* Reverse bits on output */
	crc = (((crc & 0xaaaa) >> 1) | ((crc & 0x5555) << 1));
	crc = (((crc & 0xcccc) >> 2) | ((crc & 0x3333) << 2));
	crc = (((crc & 0xf0f0) >> 4) | ((crc & 0x0f0f) << 4));
	crc = (((crc & 0xff00) >> 8) | ((crc & 0x00ff) << 8));
	return crc;
}

/*
 * Reverses the bits of the 32-bit word.
 */
static uint32_t
ieee802_15_4_reverse32(uint32_t x)
{
	x = ((x & 0x55555555) <<  1) | ((x >>  1) & 0x55555555);
	x = ((x & 0x33333333) <<  2) | ((x >>  2) & 0x33333333);
	x = ((x & 0x0F0F0F0F) <<  4) | ((x >>  4) & 0x0F0F0F0F);
	x = (x << 24) | ((x & 0xFF00) << 8) |
		((x >> 8) & 0xFF00) | (x >> 24);
	return x;
}

/*
 * IEEE 802.15.4 CRC 32 function. This is using the ANSI X3.66-1979 polynomial of
 * 0x04C11DB7, but the initial value is 0, and the bits are reversed for both
 * in and out. See section 7.2.10 of 802.15.4-2015 for more information.
 */
static uint32_t
ieee802_15_4_crc32(netdissect_options *ndo, const u_char *p,
		   u_int data_len)
{
	uint32_t crc, byte;
	int b;

	crc = 0x00000000; /* Note, initial value is 0x00000000 not 0xffffffff */

	while (data_len != 0){
		byte = GET_U_1(p);
		p++;
		/* Reverse bits on input */
		byte = ieee802_15_4_reverse32(byte);
		/* Update CRC */
		for(b = 0; b <= 7; b++) {
		  if ((int) (crc ^ byte) < 0)
		    crc = (crc << 1) ^ 0x04C11DB7;
		  else
		    crc = crc << 1;
		  byte = byte << 1;
		}
		data_len--;
	}
	/* Reverse bits on output */
	crc = ieee802_15_4_reverse32(crc);
	return crc;
}

/*
 * Find out the address length based on the address type. See table 7-3 of
 * 802.15.4-2015. Returns the address length.
 */
static int
ieee802_15_4_addr_len(uint16_t addr_type)
{
	switch (addr_type) {
	case FC_ADDRESSING_MODE_NONE: /* None. */
		return 0;
		break;
	case FC_ADDRESSING_MODE_RESERVED: /* Reserved, there used to be 8-bit
					   * address type in one amendment, but
					   * that and the feature using it was
					   * removed during 802.15.4-2015
					   * maintenance process. */
		return -1;
		break;
	case FC_ADDRESSING_MODE_SHORT: /* Short. */
		return 2;
		break;
	case FC_ADDRESSING_MODE_LONG: /* Extended. */
		return 8;
		break;
	}
	return 0;
}

/*
 * Print out the ieee 802.15.4 address.
 */
static void
ieee802_15_4_print_addr(netdissect_options *ndo, const u_char *p,
			int dst_addr_len)
{
	switch (dst_addr_len) {
	case 0:
		ND_PRINT("none");
		break;
	case 2:
		ND_PRINT("%04x", GET_LE_U_2(p));
		break;
	case 8:
		ND_PRINT("%s", GET_LE64ADDR_STRING(p));
		break;
	}
}

/*
 * Beacon frame superframe specification structure. Used in the old Beacon
 * frames, and in the DSME PAN Descriptor IE. See section 7.3.1.3 of the
 * 802.15.4-2015.
 */
static void
ieee802_15_4_print_superframe_specification(netdissect_options *ndo,
					    uint16_t ss)
{
	if (ndo->ndo_vflag < 1) {
		return;
	}
	ND_PRINT("\n\tBeacon order = %d, Superframe order = %d, ",
		 (ss & 0xf), ((ss >> 4) & 0xf));
	ND_PRINT("Final CAP Slot = %d",
		 ((ss >> 8) & 0xf));
	if (CHECK_BIT(ss, 12)) { ND_PRINT(", BLE enabled"); }
	if (CHECK_BIT(ss, 14)) { ND_PRINT(", PAN Coordinator"); }
	if (CHECK_BIT(ss, 15)) { ND_PRINT(", Association Permit"); }
}

/*
 * Beacon frame gts info structure. Used in the old Beacon frames, and
 * in the DSME PAN Descriptor IE. See section 7.3.1.4 of 802.15.4-2015.
 *
 * Returns number of byts consumed from the packet or -1 in case of error.
 */
static int
ieee802_15_4_print_gts_info(netdissect_options *ndo,
			    const u_char *p,
			    u_int data_len)
{
	uint8_t gts_spec, gts_cnt;
	u_int len;
	int i;

	gts_spec = GET_U_1(p);
	gts_cnt = gts_spec & 0x7;

	if (gts_cnt == 0) {
		if (ndo->ndo_vflag > 0) {
			ND_PRINT("\n\tGTS Descriptor Count = %d, ", gts_cnt);
		}
		return 1;
	}
	len = 1 + 1 + gts_cnt * 3;

	if (data_len < len) {
		ND_PRINT(" [ERROR: Truncated GTS Info List]");
		return -1;
	}
	if (ndo->ndo_vflag < 2) {
		return len;
	}
	ND_PRINT("GTS Descriptor Count = %d, ", gts_cnt);
	ND_PRINT("GTS Directions Mask = %02x, [ ",
		 GET_U_1(p + 1) & 0x7f);

	for(i = 0; i < gts_cnt; i++) {
		ND_PRINT("[ ");
		ieee802_15_4_print_addr(ndo, p + 2 + i * 3, 2);
		ND_PRINT(", Start slot = %d, Length = %d ] ",
			 GET_U_1(p + 2 + i * 3 + 1) & 0x0f,
			 (GET_U_1(p + 2 + i * 3 + 1) >> 4) & 0x0f);
	}
	ND_PRINT("]");
	return len;
}

/*
 * Beacon frame pending address structure. Used in the old Beacon frames, and
 * in the DSME PAN Descriptor IE. See section 7.3.1.5 of 802.15.4-2015.
 *
 * Returns number of byts consumed from the packet or -1 in case of error.
 */
static int16_t
ieee802_15_4_print_pending_addresses(netdissect_options *ndo,
				     const u_char *p,
				     u_int data_len)
{
	uint8_t pas, s_cnt, e_cnt, len, i;

	pas = GET_U_1(p);
	s_cnt = pas & 0x7;
	e_cnt = (pas >> 4) & 0x7;
	len = 1 + s_cnt * 2 + e_cnt * 8;
	if (ndo->ndo_vflag > 0) {
		ND_PRINT("\n\tPending address list, "
			 "# short addresses = %d, # extended addresses = %d",
			 s_cnt, e_cnt);
	}
	if (data_len < len) {
		ND_PRINT(" [ERROR: Pending address list truncated]");
		return -1;
	}
	if (ndo->ndo_vflag < 2) {
		return len;
	}
	if (s_cnt != 0) {
		ND_PRINT(", Short address list = [ ");
		for(i = 0; i < s_cnt; i++) {
			ieee802_15_4_print_addr(ndo, p + 1 + i * 2, 2);
			ND_PRINT(" ");
		}
		ND_PRINT("]");
	}
	if (e_cnt != 0) {
		ND_PRINT(", Extended address list = [ ");
		for(i = 0; i < e_cnt; i++) {
			ieee802_15_4_print_addr(ndo, p + 1 + s_cnt * 2 +
						i * 8, 8);
			ND_PRINT(" ");
		}
		ND_PRINT("]");
	}
	return len;
}

/*
 * Print header ie content.
 */
static void
ieee802_15_4_print_header_ie(netdissect_options *ndo,
			     const u_char *p,
			     uint16_t ie_len,
			     int element_id)
{
	int i;

	switch (element_id) {
	case 0x00: /* Vendor Specific Header IE */
		if (ie_len < 3) {
			ND_PRINT("[ERROR: Vendor OUI missing]");
		} else {
			ND_PRINT("OUI = 0x%02x%02x%02x, ", GET_U_1(p),
				 GET_U_1(p + 1), GET_U_1(p + 2));
			ND_PRINT("Data = ");
			for(i = 3; i < ie_len; i++) {
				ND_PRINT("%02x ", GET_U_1(p + i));
			}
		}
		break;
	case 0x1a: /* LE CSL IE */
		if (ie_len < 4) {
			ND_PRINT("[ERROR: Truncated CSL IE]");
		} else {
			ND_PRINT("CSL Phase = %d, CSL Period = %d",
				 GET_LE_U_2(p), GET_LE_U_2(p + 2));
			if (ie_len >= 6) {
				ND_PRINT(", Rendezvous time = %d",
					 GET_LE_U_2(p + 4));
			}
			if (ie_len != 4 && ie_len != 6) {
				ND_PRINT(" [ERROR: CSL IE length wrong]");
			}
		}
		break;
	case 0x1b: /* LE RIT IE */
		if (ie_len < 4) {
			ND_PRINT("[ERROR: Truncated RIT IE]");
		} else {
			ND_PRINT("Time to First Listen = %d, # of Repeat Listen = %d, Repeat Listen Interval = %d",
				 GET_U_1(p),
				 GET_U_1(p + 1),
				 GET_LE_U_2(p + 2));
		}
		break;
	case 0x1c: /* DSME PAN Descriptor IE */
		/*FALLTHROUGH*/
	case 0x21: /* Extended DSME PAN descriptor IE */
		if (ie_len < 2) {
			ND_PRINT("[ERROR: Truncated DSME PAN IE]");
		} else {
			uint16_t ss, ptr, ulen;
			int16_t len;
			int hopping_present;

			hopping_present = 0;

			ss = GET_LE_U_2(p);
			ieee802_15_4_print_superframe_specification(ndo, ss);
			if (ie_len < 3) {
				ND_PRINT("[ERROR: Truncated before pending addresses field]");
				break;
			}
			ptr = 2;
			len = ieee802_15_4_print_pending_addresses(ndo,
								   p + ptr,
								   ie_len -
								   ptr);
			if (len < 0) {
				break;
			}
			ptr += len;

			if (element_id == 0x21) {
				/* Extended version. */
				if (ie_len < ptr + 2) {
					ND_PRINT("[ERROR: Truncated before DSME Superframe Specification]");
					break;
				}
				ss = GET_LE_U_2(p + ptr);
				ptr += 2;
				ND_PRINT("Multi-superframe Order = %d", ss & 0xff);
				ND_PRINT(", %s", ((ss & 0x100) ?
						  "Channel hopping mode" :
						  "Channel adaptation mode"));
				if (ss & 0x400) {
					ND_PRINT(", CAP reduction enabled");
				}
				if (ss & 0x800) {
					ND_PRINT(", Deferred beacon enabled");
				}
				if (ss & 0x1000) {
					ND_PRINT(", Hopping Sequence Present");
					hopping_present = 1;
				}
			} else {
				if (ie_len < ptr + 1) {
					ND_PRINT("[ERROR: Truncated before DSME Superframe Specification]");
					break;
				}
				ss = GET_U_1(p + ptr);
				ptr++;
				ND_PRINT("Multi-superframe Order = %d",
					 ss & 0x0f);
				ND_PRINT(", %s", ((ss & 0x10) ?
						  "Channel hopping mode" :
						  "Channel adaptation mode"));
				if (ss & 0x40) {
					ND_PRINT(", CAP reduction enabled");
				}
				if (ss & 0x80) {
					ND_PRINT(", Deferred beacon enabled");
				}
			}
			if (ie_len < ptr + 8) {
				ND_PRINT(" [ERROR: Truncated before Time synchronization specification]");
				break;
			}
			ND_PRINT("Beacon timestamp = %" PRIu64 ", offset = %d",
				 GET_LE_U_6(p + ptr),
				 GET_LE_U_2(p + ptr + 6));
			ptr += 8;
			if (ie_len < ptr + 4) {
				ND_PRINT(" [ERROR: Truncated before Beacon Bitmap]");
				break;
			}

			ulen = GET_LE_U_2(p + ptr + 2);
			ND_PRINT("SD Index = %d, Bitmap len = %d, ",
				 GET_LE_U_2(p + ptr), ulen);
			ptr += 4;
			if (ie_len < ptr + ulen) {
				ND_PRINT(" [ERROR: Truncated in SD bitmap]");
				break;
			}
			ND_PRINT(" SD Bitmap = ");
			for(i = 0; i < ulen; i++) {
				ND_PRINT("%02x ", GET_U_1(p + ptr + i));
			}
			ptr += ulen;

			if (ie_len < ptr + 5) {
				ND_PRINT(" [ERROR: Truncated before Channel hopping specification]");
				break;
			}

			ulen = GET_LE_U_2(p + ptr + 4);
			ND_PRINT("Hopping Seq ID = %d, PAN Coordinator BSN = %d, "
				 "Channel offset = %d, Bitmap length = %d, ",
				 GET_U_1(p + ptr),
				 GET_U_1(p + ptr + 1),
				 GET_LE_U_2(p + ptr + 2),
				 ulen);
			ptr += 5;
			if (ie_len < ptr + ulen) {
				ND_PRINT(" [ERROR: Truncated in Channel offset bitmap]");
				break;
			}
			ND_PRINT(" Channel offset bitmap = ");
			for(i = 0; i < ulen; i++) {
				ND_PRINT("%02x ", GET_U_1(p + ptr + i));
			}
			ptr += ulen;
			if (hopping_present) {
				if (ie_len < ptr + 1) {
					ND_PRINT(" [ERROR: Truncated in Hopping Sequence length]");
					break;
				}
				ulen = GET_U_1(p + ptr);
				ptr++;
				ND_PRINT("Hopping Seq length = %d [ ", ulen);

				/* The specification is not clear how the
				   hopping sequence is encoded, I assume two
				   octet unsigned integers for each channel. */

				if (ie_len < ptr + ulen * 2) {
					ND_PRINT(" [ERROR: Truncated in Channel offset bitmap]");
					break;
				}
				for(i = 0; i < ulen; i++) {
					ND_PRINT("%02x ",
						 GET_LE_U_2(p + ptr + i * 2));
				}
				ND_PRINT("]");
				ptr += ulen * 2;
			}
		}
		break;
	case 0x1d: /* Rendezvous Tome IE */
		if (ie_len != 4) {
			ND_PRINT("[ERROR: Length != 2]");
		} else {
			uint16_t r_time, w_u_interval;
			r_time = GET_LE_U_2(p);
			w_u_interval = GET_LE_U_2(p + 2);

			ND_PRINT("Rendezvous time = %d, Wake-up Interval = %d",
				 r_time, w_u_interval);
		}
		break;
	case 0x1e: /* Time correction IE */
		if (ie_len != 2) {
			ND_PRINT("[ERROR: Length != 2]");
		} else {
			uint16_t val;
			int16_t timecorr;

			val = GET_LE_U_2(p);
			if (val & 0x8000) { ND_PRINT("Negative "); }
			val &= 0xfff;
			val <<= 4;
			timecorr = val;
			timecorr >>= 4;

			ND_PRINT("Ack time correction = %d, ", timecorr);
		}
		break;
	case 0x22: /* Fragment Sequence Content Description IE */
		/* XXX Not implemented */
	case 0x23: /* Simplified Superframe Specification IE */
		/* XXX Not implemented */
	case 0x24: /* Simplified GTS Specification IE */
		/* XXX Not implemented */
	case 0x25: /* LECIM Capabilities IE */
		/* XXX Not implemented */
	case 0x26: /* TRLE Descriptor IE */
		/* XXX Not implemented */
	case 0x27: /* RCC Capabilities IE */
		/* XXX Not implemented */
	case 0x28: /* RCCN Descriptor IE */
		/* XXX Not implemented */
	case 0x29: /* Global Time IE */
		/* XXX Not implemented */
	case 0x2b: /* DA IE */
		/* XXX Not implemented */
	default:
		ND_PRINT("IE Data = ");
		for(i = 0; i < ie_len; i++) {
			ND_PRINT("%02x ", GET_U_1(p + i));
		}
		break;
	}
}

/*
 * Parse and print Header IE list. See 7.4.2 of 802.15.4-2015 for
 * more information.
 *
 * Returns number of byts consumed from the packet or -1 in case of error.
 */
static int
ieee802_15_4_print_header_ie_list(netdissect_options *ndo,
				  const u_char *p,
				  u_int caplen,
				  int *payload_ie_present)
{
	int len, ie, element_id, i;
	uint16_t ie_len;

	*payload_ie_present = 0;
	len = 0;
	do {
		if (caplen < 2) {
			ND_PRINT("[ERROR: Truncated header IE]");
			return -1;
		}
		/* Extract IE Header */
		ie = GET_LE_U_2(p);
		if (CHECK_BIT(ie, 15)) {
			ND_PRINT("[ERROR: Header IE with type 1] ");
		}
		/* Get length and Element ID */
		ie_len = ie & 0x7f;
		element_id = (ie >> 7) & 0xff;
		if (element_id > 127) {
			ND_PRINT("Reserved Element ID %02x, length = %d ",
				 element_id, ie_len);
		} else {
			if (ie_len == 0) {
				ND_PRINT("\n\t%s [", h_ie_names[element_id]);
			} else {
				ND_PRINT("\n\t%s [ length = %d, ",
					 h_ie_names[element_id], ie_len);
			}
		}
		if (caplen < 2U + ie_len) {
			ND_PRINT("[ERROR: Truncated IE data]");
			return -1;
		}
		/* Skip header */
		p += 2;

		/* Parse and print content. */
		if (ndo->ndo_vflag > 3 && ie_len != 0) {
			ieee802_15_4_print_header_ie(ndo, p,
						     ie_len, element_id);
		} else {
			if (ie_len != 0) {
				ND_PRINT("IE Data = ");
				for(i = 0; i < ie_len; i++) {
					ND_PRINT("%02x ", GET_U_1(p + i));
				}
			}
		}
		ND_PRINT("] ");
		len += 2 + ie_len;
		p += ie_len;
		caplen -= 2 + ie_len;
		if (element_id == 0x7e) {
			*payload_ie_present = 1;
			break;
		}
		if (element_id == 0x7f) {
			break;
		}
	} while (caplen != 0);
	return len;
}

/*
 * Print MLME ie content.
 */
static void
ieee802_15_4_print_mlme_ie(netdissect_options *ndo,
			   const u_char *p,
			   uint16_t sub_ie_len,
			   int sub_id)
{
	int i, j;
	uint16_t len;

	/* Note, as there is no overlap with the long and short
	   MLME sub IDs, we can just use one switch here. */
	switch (sub_id) {
	case 0x08: /* Vendor Specific Nested IE */
		if (sub_ie_len < 3) {
			ND_PRINT("[ERROR: Vendor OUI missing]");
		} else {
			ND_PRINT("OUI = 0x%02x%02x%02x, ",
				 GET_U_1(p),
				 GET_U_1(p + 1),
				 GET_U_1(p + 2));
			ND_PRINT("Data = ");
			for(i = 3; i < sub_ie_len; i++) {
				ND_PRINT("%02x ", GET_U_1(p + i));
			}
		}
		break;
	case 0x09: /* Channel Hopping IE */
		if (sub_ie_len < 1) {
			ND_PRINT("[ERROR: Hopping sequence ID missing]");
		} else if (sub_ie_len == 1) {
			ND_PRINT("Hopping Sequence ID = %d", GET_U_1(p));
			p++;
			sub_ie_len--;
		} else {
			uint16_t channel_page, number_of_channels;

			ND_PRINT("Hopping Sequence ID = %d", GET_U_1(p));
			p++;
			sub_ie_len--;
			if (sub_ie_len < 7) {
				ND_PRINT("[ERROR: IE truncated]");
				break;
			}
			channel_page = GET_U_1(p);
			number_of_channels = GET_LE_U_2(p + 1);
			ND_PRINT("Channel Page = %d, Number of Channels = %d, ",
				 channel_page, number_of_channels);
			ND_PRINT("Phy Configuration = 0x%08x, ",
				 GET_LE_U_4(p + 3));
			p += 7;
			sub_ie_len -= 7;
			if (channel_page == 9 || channel_page == 10) {
				len = (number_of_channels + 7) / 8;
				if (sub_ie_len < len) {
					ND_PRINT("[ERROR: IE truncated]");
					break;
				}
				ND_PRINT("Extended bitmap = 0x");
				for(i = 0; i < len; i++) {
					ND_PRINT("%02x", GET_U_1(p + i));
				}
				ND_PRINT(", ");
				p += len;
				sub_ie_len -= len;
			}
			if (sub_ie_len < 2) {
				ND_PRINT("[ERROR: IE truncated]");
				break;
			}
			len = GET_LE_U_2(p);
			p += 2;
			sub_ie_len -= 2;
			ND_PRINT("Hopping Seq length = %d [ ", len);

			if (sub_ie_len < len * 2) {
				ND_PRINT(" [ERROR: IE truncated]");
				break;
			}
			for(i = 0; i < len; i++) {
				ND_PRINT("%02x ", GET_LE_U_2(p + i * 2));
			}
			ND_PRINT("]");
			p += len * 2;
			sub_ie_len -= len * 2;
			if (sub_ie_len < 2) {
				ND_PRINT("[ERROR: IE truncated]");
				break;
			}
			ND_PRINT("Current hop = %d", GET_LE_U_2(p));
		}

		break;
	case 0x1a: /* TSCH Synchronization IE. */
		if (sub_ie_len < 6) {
			ND_PRINT("[ERROR: Length != 6]");
		}
		ND_PRINT("ASN = %010" PRIx64 ", Join Metric = %d ",
			 GET_LE_U_5(p), GET_U_1(p + 5));
		break;
	case 0x1b: /* TSCH Slotframe and Link IE. */
		{
			int sf_num, off, links, opts;

			if (sub_ie_len < 1) {
				ND_PRINT("[ERROR: Truncated IE]");
				break;
			}
			sf_num = GET_U_1(p);
			ND_PRINT("Slotframes = %d ", sf_num);
			off = 1;
			for(i = 0; i < sf_num; i++) {
				if (sub_ie_len < off + 4) {
					ND_PRINT("[ERROR: Truncated IE before slotframes]");
					break;
				}
				links = GET_U_1(p + off + 3);
				ND_PRINT("\n\t\t\t[ Handle %d, size = %d, links = %d ",
					 GET_U_1(p + off),
					 GET_LE_U_2(p + off + 1),
					 links);
				off += 4;
				for(j = 0; j < links; j++) {
					if (sub_ie_len < off + 5) {
						ND_PRINT("[ERROR: Truncated IE links]");
						break;
					}
					opts = GET_U_1(p + off + 4);
					ND_PRINT("\n\t\t\t\t[ Timeslot =  %d, Offset = %d, Options = ",
						 GET_LE_U_2(p + off),
						 GET_LE_U_2(p + off + 2));
					if (opts & 0x1) { ND_PRINT("TX "); }
					if (opts & 0x2) { ND_PRINT("RX "); }
					if (opts & 0x4) { ND_PRINT("Shared "); }
					if (opts & 0x8) {
						ND_PRINT("Timekeeping ");
					}
					if (opts & 0x10) {
						ND_PRINT("Priority ");
					}
					off += 5;
					ND_PRINT("] ");
				}
				ND_PRINT("] ");
			}
		}
		break;
	case 0x1c: /* TSCH Timeslot IE. */
		if (sub_ie_len == 1) {
			ND_PRINT("Time slot ID = %d ", GET_U_1(p));
		} else if (sub_ie_len == 25) {
			ND_PRINT("Time slot ID = %d, CCA Offset = %d, CCA = %d, TX Offset = %d, RX Offset = %d, RX Ack Delay = %d, TX Ack Delay = %d, RX Wait = %d, Ack Wait = %d, RX TX = %d, Max Ack = %d, Max TX = %d, Time slot Length = %d ",
				 GET_U_1(p),
				 GET_LE_U_2(p + 1),
				 GET_LE_U_2(p + 3),
				 GET_LE_U_2(p + 5),
				 GET_LE_U_2(p + 7),
				 GET_LE_U_2(p + 9),
				 GET_LE_U_2(p + 11),
				 GET_LE_U_2(p + 13),
				 GET_LE_U_2(p + 15),
				 GET_LE_U_2(p + 17),
				 GET_LE_U_2(p + 19),
				 GET_LE_U_2(p + 21),
				 GET_LE_U_2(p + 23));
		} else if (sub_ie_len == 27) {
			ND_PRINT("Time slot ID = %d, CCA Offset = %d, CCA = %d, TX Offset = %d, RX Offset = %d, RX Ack Delay = %d, TX Ack Delay = %d, RX Wait = %d, Ack Wait = %d, RX TX = %d, Max Ack = %d, Max TX = %d, Time slot Length = %d ",
				 GET_U_1(p),
				 GET_LE_U_2(p + 1),
				 GET_LE_U_2(p + 3),
				 GET_LE_U_2(p + 5),
				 GET_LE_U_2(p + 7),
				 GET_LE_U_2(p + 9),
				 GET_LE_U_2(p + 11),
				 GET_LE_U_2(p + 13),
				 GET_LE_U_2(p + 15),
				 GET_LE_U_2(p + 17),
				 GET_LE_U_2(p + 19),
				 GET_LE_U_3(p + 21),
				 GET_LE_U_3(p + 24));
		} else {
			ND_PRINT("[ERROR: Length not 1, 25, or 27]");
			ND_PRINT("\n\t\t\tIE Data = ");
			for(i = 0; i < sub_ie_len; i++) {
				ND_PRINT("%02x ", GET_U_1(p + i));
			}
		}
		break;
	case 0x1d: /* Hopping timing IE */
		/* XXX Not implemented */
	case 0x1e: /* Enhanced Beacon Filter IE */
		/* XXX Not implemented */
	case 0x1f: /* MAC Metrics IE */
		/* XXX Not implemented */
	case 0x20: /* All MAC Metrics IE */
		/* XXX Not implemented */
	case 0x21: /* Coexistence Specification IE */
		/* XXX Not implemented */
	case 0x22: /* SUN Device Capabilities IE */
		/* XXX Not implemented */
	case 0x23: /* SUN FSK Generic PHY IE */
		/* XXX Not implemented */
	case 0x24: /* Mode Switch Parameter IE */
		/* XXX Not implemented */
	case 0x25: /* PHY Parameter Change IE */
		/* XXX Not implemented */
	case 0x26: /* O-QPSK PHY Mode IE */
		/* XXX Not implemented */
	case 0x27: /* PCA Allocation IE */
		/* XXX Not implemented */
	case 0x28: /* LECIM DSSS Operating Mode IE */
		/* XXX Not implemented */
	case 0x29: /* LECIM FSK Operating Mode IE */
		/* XXX Not implemented */
	case 0x2b: /* TVWS PHY Operating Mode Description IE */
		/* XXX Not implemented */
	case 0x2c: /* TVWS Device Capabilities IE */
		/* XXX Not implemented */
	case 0x2d: /* TVWS Device Category IE */
		/* XXX Not implemented */
	case 0x2e: /* TVWS Device Identification IE */
		/* XXX Not implemented */
	case 0x2f: /* TVWS Device Location IE */
		/* XXX Not implemented */
	case 0x30: /* TVWS Channel Information Query IE */
		/* XXX Not implemented */
	case 0x31: /* TVWS Channel Information Source IE */
		/* XXX Not implemented */
	case 0x32: /* CTM IE */
		/* XXX Not implemented */
	case 0x33: /* Timestamp IE */
		/* XXX Not implemented */
	case 0x34: /* Timestamp Difference IE */
		/* XXX Not implemented */
	case 0x35: /* TMCTP Specification IE */
		/* XXX Not implemented */
	case 0x36: /* TCC PHY Operating Mode IE */
		/* XXX Not implemented */
	default:
		ND_PRINT("IE Data = ");
		for(i = 0; i < sub_ie_len; i++) {
			ND_PRINT("%02x ", GET_U_1(p + i));
		}
		break;
	}
}

/*
 * MLME IE list parsing and printing. See 7.4.3.2 of 802.15.4-2015
 * for more information.
 */
static void
ieee802_15_4_print_mlme_ie_list(netdissect_options *ndo,
				const u_char *p,
				uint16_t ie_len)
{
	int ie, sub_id, i, type;
	uint16_t sub_ie_len;

	do {
		if (ie_len < 2) {
			ND_PRINT("[ERROR: Truncated MLME IE]");
			return;
		}
		/* Extract IE header */
		ie = GET_LE_U_2(p);
		type = CHECK_BIT(ie, 15);
		if (type) {
			/* Long type */
			sub_ie_len = ie & 0x3ff;
			sub_id = (ie >> 11) & 0x0f;
		} else {
			sub_ie_len = ie & 0xff;
			sub_id = (ie >> 8) & 0x7f;
		}

		/* Skip the IE header */
		p += 2;

		if (type == 0) {
			ND_PRINT("\n\t\t%s [ length = %d, ",
				 p_mlme_short_names[sub_id], sub_ie_len);
		} else {
			ND_PRINT("\n\t\t%s [ length = %d, ",
				 p_mlme_long_names[sub_id], sub_ie_len);
		}

		if (ie_len < 2 + sub_ie_len) {
			ND_PRINT("[ERROR: Truncated IE data]");
			return;
		}
		if (sub_ie_len != 0) {
			if (ndo->ndo_vflag > 3) {
				ieee802_15_4_print_mlme_ie(ndo, p, sub_ie_len, sub_id);
			} else if (ndo->ndo_vflag > 2) {
				ND_PRINT("IE Data = ");
				for(i = 0; i < sub_ie_len; i++) {
					ND_PRINT("%02x ", GET_U_1(p + i));
				}
			}
		}
		ND_PRINT("] ");
		p += sub_ie_len;
		ie_len -= 2 + sub_ie_len;
	} while (ie_len != 0);
}

/*
 * Multiplexed IE (802.15.9) parsing and printing.
 *
 * Returns number of bytes consumed from packet or -1 in case of error.
 */
static void
ieee802_15_4_print_mpx_ie(netdissect_options *ndo,
			  const u_char *p,
			  uint16_t ie_len)
{
	int transfer_type, tid;
	int fragment_number, data_start;
	int i;

	data_start = 0;
	if (ie_len < 1) {
		ND_PRINT("[ERROR: Transaction control byte missing]");
		return;
	}

	transfer_type = GET_U_1(p) & 0x7;
	tid = GET_U_1(p) >> 3;
	switch (transfer_type) {
	case 0x00: /* Full upper layer frame. */
	case 0x01: /* Full upper layer frame with small Multiplex ID. */
		ND_PRINT("Type = Full upper layer fragment%s, ",
			 (transfer_type == 0x01 ?
			  " with small Multiplex ID" : ""));
		if (transfer_type == 0x00) {
			if (ie_len < 3) {
				ND_PRINT("[ERROR: Multiplex ID missing]");
				return;
			}
			data_start = 3;
			ND_PRINT("tid = 0x%02x, Multiplex ID = 0x%04x, ",
				 tid, GET_LE_U_2(p + 1));
		} else {
			data_start = 1;
			ND_PRINT("Multiplex ID = 0x%04x, ", tid);
		}
		break;
	case 0x02: /* First, or middle, Fragments */
	case 0x04: /* Last fragment */
		if (ie_len < 2) {
			ND_PRINT("[ERROR: fragment number missing]");
			return;
		}

		fragment_number = GET_U_1(p + 1);
		ND_PRINT("Type = %s, tid = 0x%02x, fragment = 0x%02x, ",
			 (transfer_type == 0x02 ?
			  (fragment_number == 0 ?
			   "First fragment" : "Middle fragment") :
			  "Last fragment"), tid,
			 fragment_number);
		data_start = 2;
		if (fragment_number == 0) {
			int total_size, multiplex_id;

			if (ie_len < 6) {
				ND_PRINT("[ERROR: Total upper layer size or multiplex ID missing]");
				return;
			}
			total_size = GET_LE_U_2(p + 2);
			multiplex_id = GET_LE_U_2(p + 4);
			ND_PRINT("Total upper layer size = 0x%04x, Multiplex ID = 0x%04x, ",
				 total_size, multiplex_id);
			data_start = 6;
		}
		break;
	case 0x06: /* Abort code */
		if (ie_len == 1) {
			ND_PRINT("Type = Abort, tid = 0x%02x, no max size given",
				 tid);
		} else if (ie_len == 3) {
			ND_PRINT("Type = Abort, tid = 0x%02x, max size = 0x%04x",
				 tid, GET_LE_U_2(p + 1));
		} else {
			ND_PRINT("Type = Abort, tid = 0x%02x, invalid length = %d (not 1 or 3)",
				 tid, ie_len);
			ND_PRINT("Abort data = ");
			for(i = 1; i < ie_len; i++) {
				ND_PRINT("%02x ", GET_U_1(p + i));
			}
		}
		return;
		/* NOTREACHED */
		break;
	case 0x03: /* Reserved */
	case 0x05: /* Reserved */
	case 0x07: /* Reserved */
		ND_PRINT("Type = %d (Reserved), tid = 0x%02x, ",
			 transfer_type, tid);
		data_start = 1;
		break;
	}

	ND_PRINT("Upper layer data = ");
	for(i = data_start; i < ie_len; i++) {
		ND_PRINT("%02x ", GET_U_1(p + i));
	}
}

/*
 * Payload IE list parsing and printing. See 7.4.3 of 802.15.4-2015
 * for more information.
 *
 * Returns number of byts consumed from the packet or -1 in case of error.
 */
static int
ieee802_15_4_print_payload_ie_list(netdissect_options *ndo,
				   const u_char *p,
				   u_int caplen)
{
	int len, ie, group_id, i;
	uint16_t ie_len;

	len = 0;
	do {
		if (caplen < 2) {
			ND_PRINT("[ERROR: Truncated header IE]");
			return -1;
		}
		/* Extract IE header */
		ie = GET_LE_U_2(p);
		if ((CHECK_BIT(ie, 15)) == 0) {
			ND_PRINT("[ERROR: Payload IE with type 0] ");
		}
		ie_len = ie & 0x3ff;
		group_id = (ie >> 11) & 0x0f;

		/* Skip the IE header */
		p += 2;
		if (ie_len == 0) {
			ND_PRINT("\n\t%s [", p_ie_names[group_id]);
		} else {
			ND_PRINT("\n\t%s [ length = %d, ",
				 p_ie_names[group_id], ie_len);
		}
		if (caplen < 2U + ie_len) {
			ND_PRINT("[ERROR: Truncated IE data]");
			return -1;
		}
		if (ndo->ndo_vflag > 3 && ie_len != 0) {
			switch (group_id) {
			case 0x1: /* MLME IE */
				ieee802_15_4_print_mlme_ie_list(ndo, p, ie_len);
				break;
			case 0x2: /* Vendor Specific Nested IE */
				if (ie_len < 3) {
					ND_PRINT("[ERROR: Vendor OUI missing]");
				} else {
					ND_PRINT("OUI = 0x%02x%02x%02x, ",
						 GET_U_1(p),
						 GET_U_1(p + 1),
						 GET_U_1(p + 2));
					ND_PRINT("Data = ");
					for(i = 3; i < ie_len; i++) {
						ND_PRINT("%02x ",
							 GET_U_1(p + i));
					}
				}
				break;
			case 0x3: /* Multiplexed IE (802.15.9) */
				ieee802_15_4_print_mpx_ie(ndo, p, ie_len);
				break;
			case 0x5: /* IETF IE */
				if (ie_len < 1) {
					ND_PRINT("[ERROR: Subtype ID missing]");
				} else {
					ND_PRINT("Subtype ID = 0x%02x, Subtype content = ",
						 GET_U_1(p));
					for(i = 1; i < ie_len; i++) {
						ND_PRINT("%02x ",
							 GET_U_1(p + i));
					}
				}
				break;
			default:
				ND_PRINT("IE Data = ");
				for(i = 0; i < ie_len; i++) {
					ND_PRINT("%02x ", GET_U_1(p + i));
				}
				break;
			}
		} else {
			if (ie_len != 0) {
				ND_PRINT("IE Data = ");
				for(i = 0; i < ie_len; i++) {
					ND_PRINT("%02x ", GET_U_1(p + i));
				}
			}
		}
		ND_PRINT("]\n\t");
		len += 2 + ie_len;
		p += ie_len;
		caplen -= 2 + ie_len;
		if (group_id == 0xf) {
			break;
		}
	} while (caplen != 0);
	return len;
}

/*
 * Parse and print auxiliary security header.
 *
 * Returns number of byts consumed from the packet or -1 in case of error.
 */
static int
ieee802_15_4_print_aux_sec_header(netdissect_options *ndo,
				  const u_char *p,
				  u_int caplen,
				  int *security_level)
{
	int sc, key_id_mode, len;

	if (caplen < 1) {
		ND_PRINT("[ERROR: Truncated before Aux Security Header]");
		return -1;
	}
	sc = GET_U_1(p);
	len = 1;
	*security_level = sc & 0x7;
	key_id_mode = (sc >> 3) & 0x3;

	caplen -= 1;
	p += 1;

	if (ndo->ndo_vflag > 0) {
		ND_PRINT("\n\tSecurity Level %d, Key Id Mode %d, ",
			 *security_level, key_id_mode);
	}
	if ((CHECK_BIT(sc, 5)) == 0) {
		if (caplen < 4) {
			ND_PRINT("[ERROR: Truncated before Frame Counter]");
			return -1;
		}
		if (ndo->ndo_vflag > 1) {
			ND_PRINT("Frame Counter 0x%08x ",
				 GET_LE_U_4(p));
		}
		p += 4;
		caplen -= 4;
		len += 4;
	}
	switch (key_id_mode) {
	case 0x00: /* Implicit. */
		if (ndo->ndo_vflag > 1) {
			ND_PRINT("Implicit");
		}
		return len;
		break;
	case 0x01: /* Key Index, nothing to print here. */
		break;
	case 0x02: /* PAN and Short address Key Source, and Key Index. */
		if (caplen < 4) {
			ND_PRINT("[ERROR: Truncated before Key Source]");
			return -1;
		}
		if (ndo->ndo_vflag > 1) {
			ND_PRINT("KeySource 0x%04x:%0x4x, ",
				 GET_LE_U_2(p), GET_LE_U_2(p + 2));
		}
		p += 4;
		caplen -= 4;
		len += 4;
		break;
	case 0x03: /* Extended address and Key Index. */
		if (caplen < 8) {
			ND_PRINT("[ERROR: Truncated before Key Source]");
			return -1;
		}
		if (ndo->ndo_vflag > 1) {
			ND_PRINT("KeySource %s, ", GET_LE64ADDR_STRING(p));
		}
		p += 4;
		caplen -= 4;
		len += 4;
		break;
	}
	if (caplen < 1) {
		ND_PRINT("[ERROR: Truncated before Key Index]");
		return -1;
	}
	if (ndo->ndo_vflag > 1) {
		ND_PRINT("KeyIndex 0x%02x, ", GET_U_1(p));
	}
	caplen -= 1;
	p += 1;
	len += 1;
	return len;
}

/*
 * Print command data.
 *
 * Returns number of byts consumed from the packet or -1 in case of error.
 */
static int
ieee802_15_4_print_command_data(netdissect_options *ndo,
				uint8_t command_id,
				const u_char *p,
				u_int caplen)
{
	u_int i;

	switch (command_id) {
	case 0x01: /* Association Request */
		if (caplen != 1) {
			ND_PRINT("Invalid Association request command length");
			return -1;
		} else {
			uint8_t cap_info;
			cap_info = GET_U_1(p);
			ND_PRINT("%s%s%s%s%s%s",
				 ((cap_info & 0x02) ?
				  "FFD, " : "RFD, "),
				 ((cap_info & 0x04) ?
				  "AC powered, " : ""),
				 ((cap_info & 0x08) ?
				  "Receiver on when idle, " : ""),
				 ((cap_info & 0x10) ?
				  "Fast association, " : ""),
				 ((cap_info & 0x40) ?
				  "Security supported, " : ""),
				 ((cap_info & 0x80) ?
				  "Allocate address, " : ""));
			return caplen;
		}
		break;
	case 0x02: /* Association Response */
		if (caplen != 3) {
			ND_PRINT("Invalid Association response command length");
			return -1;
		} else {
			ND_PRINT("Short address = ");
			ieee802_15_4_print_addr(ndo, p, 2);
			switch (GET_U_1(p + 2)) {
			case 0x00:
				ND_PRINT(", Association successful");
				break;
			case 0x01:
				ND_PRINT(", PAN at capacity");
				break;
			case 0x02:
				ND_PRINT(", PAN access denied");
				break;
			case 0x03:
				ND_PRINT(", Hooping sequence offset duplication");
				break;
			case 0x80:
				ND_PRINT(", Fast association successful");
				break;
			default:
				ND_PRINT(", Status = 0x%02x",
					 GET_U_1(p + 2));
				break;
			}
			return caplen;
		}
		break;
	case 0x03: /* Disassociation Notification command */
		if (caplen != 1) {
			ND_PRINT("Invalid Disassociation Notification command length");
			return -1;
		} else {
			switch (GET_U_1(p)) {
			case 0x00:
				ND_PRINT("Reserved");
				break;
			case 0x01:
				ND_PRINT("Reason = The coordinator wishes the device to leave PAN");
				break;
			case 0x02:
				ND_PRINT("Reason = The device wishes to leave the PAN");
				break;
			default:
				ND_PRINT("Reason = 0x%02x", GET_U_1(p + 2));
				break;
			}
			return caplen;
		}

		/* Following ones do not have any data. */
	case 0x04: /* Data Request command */
	case 0x05: /* PAN ID Conflict Notification command */
	case 0x06: /* Orphan Notification command */
	case 0x07: /* Beacon Request command */
		/* Should not have any data. */
		return 0;
	case 0x08: /* Coordinator Realignment command */
		if (caplen < 7 || caplen > 8) {
			ND_PRINT("Invalid Coordinator Realignment command length");
			return -1;
		} else {
			uint16_t channel, page;

			ND_PRINT("Pan ID = 0x%04x, Coordinator short address = ",
				 GET_LE_U_2(p));
			ieee802_15_4_print_addr(ndo, p + 2, 2);
			channel = GET_U_1(p + 4);

			if (caplen == 8) {
				page = GET_U_1(p + 7);
			} else {
				page = 0x80;
			}
			if (CHECK_BIT(page, 7)) {
				/* No page present, instead we have msb of
				   channel in the page. */
				channel |= (page & 0x7f) << 8;
				ND_PRINT(", Channel Number = %d", channel);
			} else {
				ND_PRINT(", Channel Number = %d, page = %d",
					 channel, page);
			}
			ND_PRINT(", Short address = ");
			ieee802_15_4_print_addr(ndo, p + 5, 2);
			return caplen;
		}
		break;
	case 0x09: /* GTS Request command */
		if (caplen != 1) {
			ND_PRINT("Invalid GTS Request command length");
			return -1;
		} else {
			uint8_t gts;

			gts = GET_U_1(p);
			ND_PRINT("GTS Length = %d, %s, %s",
				 gts & 0xf,
				 (CHECK_BIT(gts, 4) ?
				  "Receive-only GTS" : "Transmit-only GTS"),
				 (CHECK_BIT(gts, 5) ?
				  "GTS allocation" : "GTS deallocations"));
			return caplen;
		}
		break;
	case 0x13: /* DSME Association Request command */
		/* XXX Not implemented */
	case 0x14: /* DSME Association Response command */
		/* XXX Not implemented */
	case 0x15: /* DSME GTS Request command */
		/* XXX Not implemented */
	case 0x16: /* DSME GTS Response command */
		/* XXX Not implemented */
	case 0x17: /* DSME GTS Notify command */
		/* XXX Not implemented */
	case 0x18: /* DSME Information Request command */
		/* XXX Not implemented */
	case 0x19: /* DSME Information Response command */
		/* XXX Not implemented */
	case 0x1a: /* DSME Beacon Allocation Notification command */
		/* XXX Not implemented */
	case 0x1b: /* DSME Beacon Collision Notification command */
		/* XXX Not implemented */
	case 0x1c: /* DSME Link Report command */
		/* XXX Not implemented */
	case 0x20: /* RIT Data Request command */
		/* XXX Not implemented */
	case 0x21: /* DBS Request command */
		/* XXX Not implemented */
	case 0x22: /* DBS Response command */
		/* XXX Not implemented */
	case 0x23: /* RIT Data Response command */
		/* XXX Not implemented */
	case 0x24: /* Vendor Specific command */
		/* XXX Not implemented */
	case 0x0a: /* TRLE Management Request command */
		/* XXX Not implemented */
	case 0x0b: /* TRLE Management Response command */
		/* XXX Not implemented */
	default:
		ND_PRINT("Command Data = ");
		for(i = 0; i < caplen; i++) {
			ND_PRINT("%02x ", GET_U_1(p + i));
		}
		break;
	}
	return 0;
}

/*
 * Parse and print frames following standard format.
 *
 * Returns FALSE in case of error.
 */
static u_int
ieee802_15_4_std_frames(netdissect_options *ndo,
			const u_char *p, u_int caplen,
			uint16_t fc)
{
	int len, frame_version, pan_id_comp;
	int frame_type;
	int src_pan, dst_pan, src_addr_len, dst_addr_len;
	int security_level;
	u_int miclen = 0;
	int payload_ie_present;
	uint8_t seq;
	uint32_t fcs, crc_check;
	const u_char *mic_start = NULL;

	payload_ie_present = 0;

	crc_check = 0;
	/* Assume 2 octet FCS, the FCS length depends on the PHY, and we do not
	   know about that. */
	if (caplen < 4) {
		/* Cannot have FCS, assume no FCS. */
		fcs = 0;
	} else {
		/* Test for 4 octet FCS. */
		fcs = GET_LE_U_4(p + caplen - 4);
		crc_check = ieee802_15_4_crc32(ndo, p, caplen - 4);
		if (crc_check == fcs) {
			/* Remove FCS */
			caplen -= 4;
		} else {
			/* Test for 2 octet FCS. */
			fcs = GET_LE_U_2(p + caplen - 2);
			crc_check = ieee802_15_4_crc16(ndo, p, caplen - 2);
			if (crc_check == fcs) {
				/* Remove FCS */
				caplen -= 2;
			} else {
				/* Wrong FCS, FCS might not be included in the
				   captured frame, do not remove it. */
			}
		}
	}

	/* Frame version. */
	frame_version = FC_FRAME_VERSION(fc);
	frame_type = FC_FRAME_TYPE(fc);
	ND_PRINT("v%d ", frame_version);

	if (ndo->ndo_vflag > 2) {
		if (CHECK_BIT(fc, 3)) { ND_PRINT("Security Enabled, "); }
		if (CHECK_BIT(fc, 4)) { ND_PRINT("Frame Pending, "); }
		if (CHECK_BIT(fc, 5)) { ND_PRINT("AR, "); }
		if (CHECK_BIT(fc, 6)) { ND_PRINT("PAN ID Compression, "); }
		if (CHECK_BIT(fc, 8)) { ND_PRINT("Sequence Number Suppression, "); }
		if (CHECK_BIT(fc, 9)) { ND_PRINT("IE present, "); }
	}

	/* Check for the sequence number suppression. */
	if (CHECK_BIT(fc, 8)) {
		/* Sequence number is suppressed. */
		if (frame_version < 2) {
			/* Sequence number can only be suppressed for frame
			   version 2 or higher, this is invalid frame. */
			ND_PRINT("[ERROR: Sequence number suppressed on frames where version < 2]");
		}
		if (ndo->ndo_vflag)
			ND_PRINT("seq suppressed ");
		if (caplen < 2) {
			nd_print_trunc(ndo);
			return 0;
		}
		p += 2;
		caplen -= 2;
	} else {
		seq = GET_U_1(p + 2);
		if (ndo->ndo_vflag)
			ND_PRINT("seq %02x ", seq);
		if (caplen < 3) {
			nd_print_trunc(ndo);
			return 0;
		}
		p += 3;
		caplen -= 3;
	}

	/* See which parts of addresses we have. */
	dst_addr_len = ieee802_15_4_addr_len((fc >> 10) & 0x3);
	src_addr_len = ieee802_15_4_addr_len((fc >> 14) & 0x3);
	if (src_addr_len < 0) {
		ND_PRINT("[ERROR: Invalid src address mode]");
		return 0;
	}
	if (dst_addr_len < 0) {
		ND_PRINT("[ERROR: Invalid dst address mode]");
		return 0;
	}
	src_pan = 0;
	dst_pan = 0;
	pan_id_comp = CHECK_BIT(fc, 6);

	/* The PAN ID Compression rules are complicated. */

	/* First check old versions, where the rules are simple. */
	if (frame_version < 2) {
		if (pan_id_comp) {
			src_pan = 0;
			dst_pan = 1;
			if (dst_addr_len <= 0 || src_addr_len <= 0) {
				/* Invalid frame, PAN ID Compression must be 0
				   if only one address in the frame. */
				ND_PRINT("[ERROR: PAN ID Compression != 0, and only one address with frame version < 2]");
			}
		} else {
			src_pan = 1;
			dst_pan = 1;
		}
		if (dst_addr_len <= 0) {
			dst_pan = 0;
		}
		if (src_addr_len <= 0) {
			src_pan = 0;
		}
	} else {
		/* Frame version 2 rules are more complicated, and they depend
		   on the address modes of the frame, generic rules are same,
		   but then there are some special cases. */
		if (pan_id_comp) {
			src_pan = 0;
			dst_pan = 1;
		} else {
			src_pan = 1;
			dst_pan = 1;
		}
		if (dst_addr_len <= 0) {
			dst_pan = 0;
		}
		if (src_addr_len <= 0) {
			src_pan = 0;
		}
		if (pan_id_comp) {
			if (src_addr_len == 0 &&
			    dst_addr_len == 0) {
				/* Both addresses are missing, but PAN ID
				   compression set, special case we have
				   destination PAN but no addresses. */
				dst_pan = 1;
			} else if ((src_addr_len == 0 &&
				    dst_addr_len > 0) ||
				   (src_addr_len > 0 &&
				    dst_addr_len == 0)) {
				/* Only one address present, and PAN ID
				   compression is set, we do not have PAN id at
				   all. */
				dst_pan = 0;
				src_pan = 0;
			} else if (src_addr_len == 8 &&
				   dst_addr_len == 8) {
				/* Both addresses are Extended, and PAN ID
				   compression set, we do not have PAN ID at
				   all. */
				dst_pan = 0;
				src_pan = 0;
			}
		} else {
			/* Special cases where PAN ID Compression is not set. */
			if (src_addr_len == 8 &&
			    dst_addr_len == 8) {
				/* Both addresses are Extended, and PAN ID
				   compression not set, we do have only one PAN
				   ID (destination). */
				dst_pan = 1;
				src_pan = 0;
			}
#ifdef BROKEN_6TISCH_PAN_ID_COMPRESSION
			if (src_addr_len == 8 &&
			    dst_addr_len == 2) {
				/* Special case for the broken 6tisch
				   implementations. */
				src_pan = 0;
			}
#endif /* BROKEN_6TISCH_PAN_ID_COMPRESSION */
		}
	}

	/* Print dst PAN and address. */
	if (dst_pan) {
		if (caplen < 2) {
			ND_PRINT("[ERROR: Truncated before dst_pan]");
			return 0;
		}
		ND_PRINT("%04x:", GET_LE_U_2(p));
		p += 2;
		caplen -= 2;
	} else {
		ND_PRINT("-:");
	}
	if (caplen < (u_int) dst_addr_len) {
		ND_PRINT("[ERROR: Truncated before dst_addr]");
		return 0;
	}
	ieee802_15_4_print_addr(ndo, p, dst_addr_len);
	p += dst_addr_len;
	caplen -= dst_addr_len;

	ND_PRINT(" < ");

	/* Print src PAN and address. */
	if (src_pan) {
		if (caplen < 2) {
			ND_PRINT("[ERROR: Truncated before dst_pan]");
			return 0;
		}
		ND_PRINT("%04x:", GET_LE_U_2(p));
		p += 2;
		caplen -= 2;
	} else {
		ND_PRINT("-:");
	}
	if (caplen < (u_int) src_addr_len) {
		ND_PRINT("[ERROR: Truncated before dst_addr]");
		return 0;
	}
	ieee802_15_4_print_addr(ndo, p, src_addr_len);
	ND_PRINT(" ");
	p += src_addr_len;
	caplen -= src_addr_len;
	if (CHECK_BIT(fc, 3)) {
		/*
		 * XXX - if frame_version is 0, this is the 2003
		 * spec, and you don't have the auxiliary security
		 * header, you have a frame counter and key index
		 * for the AES-CTR and AES-CCM security suites but
		 * not for the AES-CBC-MAC security suite.
		 */
		len = ieee802_15_4_print_aux_sec_header(ndo, p, caplen,
							&security_level);
		if (len < 0) {
			return 0;
		}
		ND_TCHECK_LEN(p, len);
		p += len;
		caplen -= len;
	} else {
		security_level = 0;
	}

	switch (security_level) {
	case 0: /*FALLTHROUGH */
	case 4:
		miclen = 0;
		break;
	case 1: /*FALLTHROUGH */
	case 5:
		miclen = 4;
		break;
	case 2: /*FALLTHROUGH */
	case 6:
		miclen = 8;
		break;
	case 3: /*FALLTHROUGH */
	case 7:
		miclen = 16;
		break;
	}

	/* Remove MIC */
	if (miclen != 0) {
		if (caplen < miclen) {
			ND_PRINT("[ERROR: Truncated before MIC]");
			return 0;
		}
		caplen -= miclen;
		mic_start = p + caplen;
	}

	/* Parse Information elements if present */
	if (CHECK_BIT(fc, 9)) {
		/* Yes we have those. */
		len = ieee802_15_4_print_header_ie_list(ndo, p, caplen,
							&payload_ie_present);
		if (len < 0) {
			return 0;
		}
		p += len;
		caplen -= len;
	}

	if (payload_ie_present) {
		if (security_level >= 4) {
			ND_PRINT("Payload IEs present, but encrypted, cannot print ");
		} else {
			len = ieee802_15_4_print_payload_ie_list(ndo, p, caplen);
			if (len < 0) {
				return 0;
			}
			p += len;
			caplen -= len;
		}
	}

	/* Print MIC */
	if (ndo->ndo_vflag > 2 && miclen != 0) {
		ND_PRINT("\n\tMIC ");

		for (u_int micoffset = 0; micoffset < miclen; micoffset++) {
			ND_PRINT("%02x", GET_U_1(mic_start + micoffset));
		}
		ND_PRINT(" ");
	}

	/* Print FCS */
	if (ndo->ndo_vflag > 2) {
		if (crc_check == fcs) {
			ND_PRINT("FCS %x ", fcs);
		} else {
			ND_PRINT("wrong FCS %x vs %x (assume no FCS stored) ",
				 fcs, crc_check);
		}
	}

	/* Payload print */
	switch (frame_type) {
	case 0x00: /* Beacon */
		if (frame_version < 2) {
			if (caplen < 2) {
				ND_PRINT("[ERROR: Truncated before beacon information]");
				break;
			} else {
				uint16_t ss;

				ss = GET_LE_U_2(p);
				ieee802_15_4_print_superframe_specification(ndo, ss);
				p += 2;
				caplen -= 2;

				/* GTS */
				if (caplen < 1) {
					ND_PRINT("[ERROR: Truncated before GTS info]");
					break;
				}

				len = ieee802_15_4_print_gts_info(ndo, p, caplen);
				if (len < 0) {
					break;
				}

				p += len;
				caplen -= len;

				/* Pending Addresses */
				if (caplen < 1) {
					ND_PRINT("[ERROR: Truncated before pending addresses]");
					break;
				}
				len = ieee802_15_4_print_pending_addresses(ndo, p, caplen);
				if (len < 0) {
					break;
				}
				ND_TCHECK_LEN(p, len);
				p += len;
				caplen -= len;
			}
		}
		if (!ndo->ndo_suppress_default_print)
			ND_DEFAULTPRINT(p, caplen);

		break;
	case 0x01: /* Data */
	case 0x02: /* Acknowledgement */
		if (!ndo->ndo_suppress_default_print)
			ND_DEFAULTPRINT(p, caplen);
		break;
	case 0x03: /* MAC Command */
		if (caplen < 1) {
			ND_PRINT("[ERROR: Truncated before Command ID]");
		} else {
			uint8_t command_id;

			command_id = GET_U_1(p);
			if (command_id >= 0x30) {
				ND_PRINT("Command ID = Reserved 0x%02x ",
					 command_id);
			} else {
				ND_PRINT("Command ID = %s ",
					 mac_c_names[command_id]);
			}
			p++;
			caplen--;
			if (caplen != 0) {
				len = ieee802_15_4_print_command_data(ndo, command_id, p, caplen);
				if (len >= 0) {
					p += len;
					caplen -= len;
				}
			}
		}
		if (!ndo->ndo_suppress_default_print)
			ND_DEFAULTPRINT(p, caplen);
		break;
	}
	return 1;
}

/*
 * Print and parse Multipurpose frames.
 *
 * Returns FALSE in case of error.
 */
static u_int
ieee802_15_4_mp_frame(netdissect_options *ndo,
		      const u_char *p, u_int caplen,
		      uint16_t fc)
{
	int len, frame_version, pan_id_present;
	int src_addr_len, dst_addr_len;
	int security_level;
	u_int miclen = 0;
	int ie_present, payload_ie_present, security_enabled;
	uint8_t seq;
	uint32_t fcs, crc_check;
	const u_char *mic_start = NULL;

	pan_id_present = 0;
	ie_present = 0;
	payload_ie_present = 0;
	security_enabled = 0;
	crc_check = 0;

	/* Assume 2 octet FCS, the FCS length depends on the PHY, and we do not
	   know about that. */
	if (caplen < 3) {
		/* Cannot have FCS, assume no FCS. */
		fcs = 0;
	} else {
		if (caplen > 4) {
			/* Test for 4 octet FCS. */
			fcs = GET_LE_U_4(p + caplen - 4);
			crc_check = ieee802_15_4_crc32(ndo, p, caplen - 4);
			if (crc_check == fcs) {
				/* Remove FCS */
				caplen -= 4;
			} else {
				fcs = GET_LE_U_2(p + caplen - 2);
				crc_check = ieee802_15_4_crc16(ndo, p, caplen - 2);
				if (crc_check == fcs) {
					/* Remove FCS */
					caplen -= 2;
				}
			}
		} else {
			fcs = GET_LE_U_2(p + caplen - 2);
			crc_check = ieee802_15_4_crc16(ndo, p, caplen - 2);
			if (crc_check == fcs) {
				/* Remove FCS */
				caplen -= 2;
			}
		}
	}

	if (CHECK_BIT(fc, 3)) {
		/* Long Frame Control */

		/* Frame version. */
		frame_version = FC_FRAME_VERSION(fc);
		ND_PRINT("v%d ", frame_version);

		pan_id_present = CHECK_BIT(fc, 8);
		ie_present = CHECK_BIT(fc, 15);
		security_enabled = CHECK_BIT(fc, 9);

		if (ndo->ndo_vflag > 2) {
			if (security_enabled) { ND_PRINT("Security Enabled, "); }
			if (CHECK_BIT(fc, 11)) { ND_PRINT("Frame Pending, "); }
			if (CHECK_BIT(fc, 14)) { ND_PRINT("AR, "); }
			if (pan_id_present) { ND_PRINT("PAN ID Present, "); }
			if (CHECK_BIT(fc, 10)) {
				ND_PRINT("Sequence Number Suppression, ");
			}
			if (ie_present) { ND_PRINT("IE present, "); }
		}

		/* Check for the sequence number suppression. */
		if (CHECK_BIT(fc, 10)) {
			/* Sequence number is suppressed, but long version. */
			if (caplen < 2) {
				nd_print_trunc(ndo);
				return 0;
			}
			p += 2;
			caplen -= 2;
		} else {
			seq = GET_U_1(p + 2);
			if (ndo->ndo_vflag)
				ND_PRINT("seq %02x ", seq);
			if (caplen < 3) {
				nd_print_trunc(ndo);
				return 0;
			}
			p += 3;
			caplen -= 3;
		}
	} else {
		/* Short format of header, but with seq no */
		seq = GET_U_1(p + 1);
		p += 2;
		caplen -= 2;
		if (ndo->ndo_vflag)
			ND_PRINT("seq %02x ", seq);
	}

	/* See which parts of addresses we have. */
	dst_addr_len = ieee802_15_4_addr_len((fc >> 4) & 0x3);
	src_addr_len = ieee802_15_4_addr_len((fc >> 6) & 0x3);
	if (src_addr_len < 0) {
		ND_PRINT("[ERROR: Invalid src address mode]");
		return 0;
	}
	if (dst_addr_len < 0) {
		ND_PRINT("[ERROR: Invalid dst address mode]");
		return 0;
	}

	/* Print dst PAN and address. */
	if (pan_id_present) {
		if (caplen < 2) {
			ND_PRINT("[ERROR: Truncated before dst_pan]");
			return 0;
		}
		ND_PRINT("%04x:", GET_LE_U_2(p));
		p += 2;
		caplen -= 2;
	} else {
		ND_PRINT("-:");
	}
	if (caplen < (u_int) dst_addr_len) {
		ND_PRINT("[ERROR: Truncated before dst_addr]");
		return 0;
	}
	ieee802_15_4_print_addr(ndo, p, dst_addr_len);
	p += dst_addr_len;
	caplen -= dst_addr_len;

	ND_PRINT(" < ");

	/* Print src PAN and address. */
	ND_PRINT(" -:");
	if (caplen < (u_int) src_addr_len) {
		ND_PRINT("[ERROR: Truncated before dst_addr]");
		return 0;
	}
	ieee802_15_4_print_addr(ndo, p, src_addr_len);
	ND_PRINT(" ");
	p += src_addr_len;
	caplen -= src_addr_len;

	if (security_enabled) {
		len = ieee802_15_4_print_aux_sec_header(ndo, p, caplen,
							&security_level);
		if (len < 0) {
			return 0;
		}
		ND_TCHECK_LEN(p, len);
		p += len;
		caplen -= len;
	} else {
		security_level = 0;
	}

	switch (security_level) {
	case 0: /*FALLTHROUGH */
	case 4:
		miclen = 0;
		break;
	case 1: /*FALLTHROUGH */
	case 5:
		miclen = 4;
		break;
	case 2: /*FALLTHROUGH */
	case 6:
		miclen = 8;
		break;
	case 3: /*FALLTHROUGH */
	case 7:
		miclen = 16;
		break;
	}

	/* Remove MIC */
	if (miclen != 0) {
		if (caplen < miclen) {
			ND_PRINT("[ERROR: Truncated before MIC]");
			return 0;
		}
		caplen -= miclen;
		mic_start = p + caplen;
	}

	/* Parse Information elements if present */
	if (ie_present) {
		/* Yes we have those. */
		len = ieee802_15_4_print_header_ie_list(ndo, p, caplen,
							&payload_ie_present);
		if (len < 0) {
			return 0;
		}
		p += len;
		caplen -= len;
	}

	if (payload_ie_present) {
		if (security_level >= 4) {
			ND_PRINT("Payload IEs present, but encrypted, cannot print ");
		} else {
			len = ieee802_15_4_print_payload_ie_list(ndo, p,
								 caplen);
			if (len < 0) {
				return 0;
			}
			p += len;
			caplen -= len;
		}
	}

	/* Print MIC */
	if (ndo->ndo_vflag > 2 && miclen != 0) {
		ND_PRINT("\n\tMIC ");

		for (u_int micoffset = 0; micoffset < miclen; micoffset++) {
			ND_PRINT("%02x", GET_U_1(mic_start + micoffset));
		}
		ND_PRINT(" ");
	}


	/* Print FCS */
	if (ndo->ndo_vflag > 2) {
		if (crc_check == fcs) {
			ND_PRINT("FCS %x ", fcs);
		} else {
			ND_PRINT("wrong FCS %x vs %x (assume no FCS stored) ",
				 fcs, crc_check);
		}
	}

	if (!ndo->ndo_suppress_default_print)
		ND_DEFAULTPRINT(p, caplen);

	return 1;
}

/*
 * Print frag frame.
 *
 * Returns FALSE in case of error.
 */
static u_int
ieee802_15_4_frag_frame(netdissect_options *ndo _U_,
			const u_char *p _U_,
			u_int caplen _U_,
			uint16_t fc _U_)
{
	/* Not implement yet, might be bit hard to implement, as the
	 * information to set up the fragment is coming in the previous frame
	 * in the Fragment Sequence Context Description IE, thus we need to
	 * store information from there, so we can use it here. */
	return 0;
}

/*
 * Internal call to dissector taking packet + len instead of pcap_pkthdr.
 *
 * Returns FALSE in case of error.
 */
u_int
ieee802_15_4_print(netdissect_options *ndo,
		   const u_char *p, u_int caplen)
{
	int frame_type;
	uint16_t fc;

	ndo->ndo_protocol = "802.15.4";

	if (caplen < 2) {
		nd_print_trunc(ndo);
		return caplen;
	}

	fc = GET_LE_U_2(p);

	/* First we need to check the frame type to know how to parse the rest
	   of the FC. Frame type is the first 3 bit of the frame control field.
	*/

	frame_type = FC_FRAME_TYPE(fc);
	ND_PRINT("IEEE 802.15.4 %s packet ", ftypes[frame_type]);

	switch (frame_type) {
	case 0x00: /* Beacon */
	case 0x01: /* Data */
	case 0x02: /* Acknowledgement */
	case 0x03: /* MAC Command */
		return ieee802_15_4_std_frames(ndo, p, caplen, fc);
		break;
	case 0x04: /* Reserved */
		return 0;
		break;
	case 0x05: /* Multipurpose */
		return ieee802_15_4_mp_frame(ndo, p, caplen, fc);
		break;
	case 0x06: /* Fragment or Frak */
		return ieee802_15_4_frag_frame(ndo, p, caplen, fc);
		break;
	case 0x07: /* Extended */
		return 0;
		break;
	}
	return 0;
}

/*
 * Main function to print packets.
 */

void
ieee802_15_4_if_print(netdissect_options *ndo,
                      const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	ndo->ndo_protocol = "802.15.4";
	ndo->ndo_ll_hdr_len += ieee802_15_4_print(ndo, p, caplen);
}

/* For DLT_IEEE802_15_4_TAP */
/* https://github.com/jkcko/ieee802.15.4-tap */
void
ieee802_15_4_tap_if_print(netdissect_options *ndo,
                          const struct pcap_pkthdr *h, const u_char *p)
{
	uint8_t version;
	uint16_t length;

	ndo->ndo_protocol = "802.15.4_tap";
	if (h->caplen < 4) {
		nd_print_trunc(ndo);
		ndo->ndo_ll_hdr_len += h->caplen;
		return;
	}

	version = GET_U_1(p);
	length = GET_LE_U_2(p + 2);
	if (version != 0 || length < 4) {
		nd_print_invalid(ndo);
		return;
	}

	if (h->caplen < length) {
		nd_print_trunc(ndo);
		ndo->ndo_ll_hdr_len += h->caplen;
		return;
	}

	ndo->ndo_ll_hdr_len += ieee802_15_4_print(ndo, p+length, h->caplen-length) + length;
}
