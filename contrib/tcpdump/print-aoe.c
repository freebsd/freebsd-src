/*
 * Copyright (c) 2014 The TCPDUMP project
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* \summary: ATA over Ethernet (AoE) protocol printer */

/* specification:
 * https://web.archive.org/web/20161025044402/http://brantleycoilecompany.com/AoEr11.pdf
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"


#define AOE_V1 1
#define ATA_SECTOR_SIZE 512

#define AOEV1_CMD_ISSUE_ATA_COMMAND        0
#define AOEV1_CMD_QUERY_CONFIG_INFORMATION 1
#define AOEV1_CMD_MAC_MASK_LIST            2
#define AOEV1_CMD_RESERVE_RELEASE          3

static const struct tok cmdcode_str[] = {
	{ AOEV1_CMD_ISSUE_ATA_COMMAND,        "Issue ATA Command"        },
	{ AOEV1_CMD_QUERY_CONFIG_INFORMATION, "Query Config Information" },
	{ AOEV1_CMD_MAC_MASK_LIST,            "MAC Mask List"            },
	{ AOEV1_CMD_RESERVE_RELEASE,          "Reserve/Release"          },
	{ 0, NULL }
};

#define AOEV1_COMMON_HDR_LEN    10U /* up to but w/o Arg                */
#define AOEV1_ISSUE_ARG_LEN     12U /* up to but w/o Data               */
#define AOEV1_QUERY_ARG_LEN      8U /* up to but w/o Config String      */
#define AOEV1_MAC_ARG_LEN        4U /* up to but w/o Directive 0        */
#define AOEV1_RESERVE_ARG_LEN    2U /* up to but w/o Ethernet address 0 */
#define AOEV1_MAX_CONFSTR_LEN 1024U

#define AOEV1_FLAG_R 0x08
#define AOEV1_FLAG_E 0x04

static const struct tok aoev1_flag_str[] = {
	{ AOEV1_FLAG_R, "Response" },
	{ AOEV1_FLAG_E, "Error"    },
	{ 0x02,         "MBZ-1"    },
	{ 0x01,         "MBZ-0"    },
	{ 0, NULL }
};

static const struct tok aoev1_errcode_str[] = {
	{ 1, "Unrecognized command code" },
	{ 2, "Bad argument parameter"    },
	{ 3, "Device unavailable"        },
	{ 4, "Config string present"     },
	{ 5, "Unsupported version"       },
	{ 6, "Target is reserved"        },
	{ 0, NULL }
};

#define AOEV1_AFLAG_E 0x40
#define AOEV1_AFLAG_D 0x10
#define AOEV1_AFLAG_A 0x02
#define AOEV1_AFLAG_W 0x01

static const struct tok aoev1_aflag_bitmap_str[] = {
	{ 0x80,          "MBZ-7"    },
	{ AOEV1_AFLAG_E, "Ext48"    },
	{ 0x20,          "MBZ-5"    },
	{ AOEV1_AFLAG_D, "Device"   },
	{ 0x08,          "MBZ-3"    },
	{ 0x04,          "MBZ-2"    },
	{ AOEV1_AFLAG_A, "Async"    },
	{ AOEV1_AFLAG_W, "Write"    },
	{ 0, NULL }
};

static const struct tok aoev1_ccmd_str[] = {
	{ 0, "read config string"        },
	{ 1, "test config string"        },
	{ 2, "test config string prefix" },
	{ 3, "set config string"         },
	{ 4, "force set config string"   },
	{ 0, NULL }
};

static const struct tok aoev1_mcmd_str[] = {
	{ 0, "Read Mac Mask List" },
	{ 1, "Edit Mac Mask List" },
	{ 0, NULL }
};

static const struct tok aoev1_merror_str[] = {
	{ 1, "Unspecified Error"  },
	{ 2, "Bad DCmd directive" },
	{ 3, "Mask list full"     },
	{ 0, NULL }
};

static const struct tok aoev1_dcmd_str[] = {
	{ 0, "No Directive"                      },
	{ 1, "Add mac address to mask list"      },
	{ 2, "Delete mac address from mask list" },
	{ 0, NULL }
};

static const struct tok aoev1_rcmd_str[] = {
	{ 0, "Read reserve list"      },
	{ 1, "Set reserve list"       },
	{ 2, "Force set reserve list" },
	{ 0, NULL }
};

static void
aoev1_issue_print(netdissect_options *ndo,
                  const u_char *cp, u_int len)
{
	if (len < AOEV1_ISSUE_ARG_LEN)
		goto invalid;
	/* AFlags */
	ND_PRINT("\n\tAFlags: [%s]",
		 bittok2str(aoev1_aflag_bitmap_str, "none", GET_U_1(cp)));
	cp += 1;
	len -= 1;
	/* Err/Feature */
	ND_PRINT(", Err/Feature: %u", GET_U_1(cp));
	cp += 1;
	len -= 1;
	/* Sector Count (not correlated with the length) */
	ND_PRINT(", Sector Count: %u", GET_U_1(cp));
	cp += 1;
	len -= 1;
	/* Cmd/Status */
	ND_PRINT(", Cmd/Status: %u", GET_U_1(cp));
	cp += 1;
	len -= 1;
	/* lba0 */
	ND_PRINT("\n\tlba0: %u", GET_U_1(cp));
	cp += 1;
	len -= 1;
	/* lba1 */
	ND_PRINT(", lba1: %u", GET_U_1(cp));
	cp += 1;
	len -= 1;
	/* lba2 */
	ND_PRINT(", lba2: %u", GET_U_1(cp));
	cp += 1;
	len -= 1;
	/* lba3 */
	ND_PRINT(", lba3: %u", GET_U_1(cp));
	cp += 1;
	len -= 1;
	/* lba4 */
	ND_PRINT(", lba4: %u", GET_U_1(cp));
	cp += 1;
	len -= 1;
	/* lba5 */
	ND_PRINT(", lba5: %u", GET_U_1(cp));
	cp += 1;
	len -= 1;
	/* Reserved */
	ND_TCHECK_2(cp);
	cp += 2;
	len -= 2;
	/* Data */
	if (len)
		ND_PRINT("\n\tData: %u bytes", len);
	return;

invalid:
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

static void
aoev1_query_print(netdissect_options *ndo,
                  const u_char *cp, u_int len)
{
	uint16_t cslen;

	if (len < AOEV1_QUERY_ARG_LEN)
		goto invalid;
	/* Buffer Count */
	ND_PRINT("\n\tBuffer Count: %u", GET_BE_U_2(cp));
	cp += 2;
	len -= 2;
	/* Firmware Version */
	ND_PRINT(", Firmware Version: %u", GET_BE_U_2(cp));
	cp += 2;
	len -= 2;
	/* Sector Count */
	ND_PRINT(", Sector Count: %u", GET_U_1(cp));
	cp += 1;
	len -= 1;
	/* AoE/CCmd */
	ND_PRINT(", AoE: %u, CCmd: %s", (GET_U_1(cp) & 0xF0) >> 4,
	          tok2str(aoev1_ccmd_str, "Unknown (0x02x)", GET_U_1(cp) & 0x0F));
	cp += 1;
	len -= 1;
	/* Config String Length */
	cslen = GET_BE_U_2(cp);
	cp += 2;
	len -= 2;
	if (cslen > AOEV1_MAX_CONFSTR_LEN || cslen > len)
		goto invalid;
	/* Config String */
	if (cslen) {
		ND_PRINT("\n\tConfig String (length %u): ", cslen);
		(void)nd_printn(ndo, cp, cslen, NULL);
	}
	return;

invalid:
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

static void
aoev1_mac_print(netdissect_options *ndo,
                const u_char *cp, u_int len)
{
	uint8_t dircount, i;

	if (len < AOEV1_MAC_ARG_LEN)
		goto invalid;
	/* Reserved */
	cp += 1;
	len -= 1;
	/* MCmd */
	ND_PRINT("\n\tMCmd: %s",
		 tok2str(aoev1_mcmd_str, "Unknown (0x%02x)", GET_U_1(cp)));
	cp += 1;
	len -= 1;
	/* MError */
	ND_PRINT(", MError: %s",
		 tok2str(aoev1_merror_str, "Unknown (0x%02x)", GET_U_1(cp)));
	cp += 1;
	len -= 1;
	/* Dir Count */
	dircount = GET_U_1(cp);
	cp += 1;
	len -= 1;
	ND_PRINT(", Dir Count: %u", dircount);
	if (dircount * 8U > len)
		goto invalid;
	/* directives */
	for (i = 0; i < dircount; i++) {
		/* Reserved */
		cp += 1;
		len -= 1;
		/* DCmd */
		ND_PRINT("\n\t DCmd: %s",
			 tok2str(aoev1_dcmd_str, "Unknown (0x%02x)", GET_U_1(cp)));
		cp += 1;
		len -= 1;
		/* Ethernet Address */
		ND_PRINT(", Ethernet Address: %s", GET_ETHERADDR_STRING(cp));
		cp += MAC_ADDR_LEN;
		len -= MAC_ADDR_LEN;
	}
	return;

invalid:
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

static void
aoev1_reserve_print(netdissect_options *ndo,
                    const u_char *cp, u_int len)
{
	uint8_t nmacs, i;

	if (len < AOEV1_RESERVE_ARG_LEN || (len - AOEV1_RESERVE_ARG_LEN) % MAC_ADDR_LEN)
		goto invalid;
	/* RCmd */
	ND_PRINT("\n\tRCmd: %s",
		 tok2str(aoev1_rcmd_str, "Unknown (0x%02x)", GET_U_1(cp)));
	cp += 1;
	len -= 1;
	/* NMacs (correlated with the length) */
	nmacs = GET_U_1(cp);
	cp += 1;
	len -= 1;
	ND_PRINT(", NMacs: %u", nmacs);
	if (nmacs * MAC_ADDR_LEN != len)
		goto invalid;
	/* addresses */
	for (i = 0; i < nmacs; i++) {
		ND_PRINT("\n\tEthernet Address %u: %s", i, GET_ETHERADDR_STRING(cp));
		cp += MAC_ADDR_LEN;
		len -= MAC_ADDR_LEN;
	}
	return;

invalid:
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

/* cp points to the Ver/Flags octet */
static void
aoev1_print(netdissect_options *ndo,
            const u_char *cp, u_int len)
{
	uint8_t flags, command;
	void (*cmd_decoder)(netdissect_options *, const u_char *, u_int);

	if (len < AOEV1_COMMON_HDR_LEN)
		goto invalid;
	/* Flags */
	flags = GET_U_1(cp) & 0x0F;
	ND_PRINT(", Flags: [%s]", bittok2str(aoev1_flag_str, "none", flags));
	cp += 1;
	len -= 1;
	if (! ndo->ndo_vflag)
		return;
	/* Error */
	if (flags & AOEV1_FLAG_E)
		ND_PRINT("\n\tError: %s",
			 tok2str(aoev1_errcode_str, "Invalid (%u)", GET_U_1(cp)));
	cp += 1;
	len -= 1;
	/* Major */
	ND_PRINT("\n\tMajor: 0x%04x", GET_BE_U_2(cp));
	cp += 2;
	len -= 2;
	/* Minor */
	ND_PRINT(", Minor: 0x%02x", GET_U_1(cp));
	cp += 1;
	len -= 1;
	/* Command */
	command = GET_U_1(cp);
	cp += 1;
	len -= 1;
	ND_PRINT(", Command: %s", tok2str(cmdcode_str, "Unknown (0x%02x)", command));
	/* Tag */
	ND_PRINT(", Tag: 0x%08x", GET_BE_U_4(cp));
	cp += 4;
	len -= 4;
	/* Arg */
	cmd_decoder =
		command == AOEV1_CMD_ISSUE_ATA_COMMAND        ? aoev1_issue_print :
		command == AOEV1_CMD_QUERY_CONFIG_INFORMATION ? aoev1_query_print :
		command == AOEV1_CMD_MAC_MASK_LIST            ? aoev1_mac_print :
		command == AOEV1_CMD_RESERVE_RELEASE          ? aoev1_reserve_print :
		NULL;
	if (cmd_decoder != NULL)
		cmd_decoder(ndo, cp, len);
	return;

invalid:
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

void
aoe_print(netdissect_options *ndo,
          const u_char *cp, const u_int len)
{
	uint8_t ver;

	ndo->ndo_protocol = "aoe";
	ND_PRINT("AoE length %u", len);

	if (len < 1)
		goto invalid;
	/* Ver/Flags */
	ver = (GET_U_1(cp) & 0xF0) >> 4;
	/* Don't advance cp yet: low order 4 bits are version-specific. */
	ND_PRINT(", Ver %u", ver);

	switch (ver) {
		case AOE_V1:
			aoev1_print(ndo, cp, len);
			break;
	}
	return;

invalid:
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

