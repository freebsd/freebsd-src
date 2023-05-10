/*
 * Copyright (c) 2013 The TCPDUMP project
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

/* OpenFlow: protocol between controller and datapath. */

/* for netdissect_options */
#include "netdissect.h"

#define OF_FWD(n) { \
	cp += (n); \
	len -= (n); \
}

#define OF_CHK_FWD(n) { \
	ND_TCHECK_LEN(cp, (n)); \
	cp += (n); \
	len -= (n); \
}

#define OF_VER_1_0 0x01U
#define OF_VER_1_1 0x02U
#define OF_VER_1_2 0x03U
#define OF_VER_1_3 0x04U
#define OF_VER_1_4 0x05U
#define OF_VER_1_5 0x06U

#define OF_HEADER_FIXLEN 8U

#define ONF_EXP_ONF               0x4f4e4600
#define ONF_EXP_BUTE              0xff000001
#define ONF_EXP_NOVIFLOW          0xff000002
#define ONF_EXP_L3                0xff000003
#define ONF_EXP_L4L7              0xff000004
#define ONF_EXP_WMOB              0xff000005
#define ONF_EXP_FABS              0xff000006
#define ONF_EXP_OTRANS            0xff000007
#define ONF_EXP_NBLNCTU           0xff000008
#define ONF_EXP_MPCE              0xff000009
#define ONF_EXP_MPLSTPSPTN        0xff00000a
extern const struct tok onf_exp_str[];

extern const char * of_vendor_name(const uint32_t);
extern void of_bitmap_print(netdissect_options *ndo,
	const struct tok *, const uint32_t, const uint32_t);
extern void of_data_print(netdissect_options *ndo,
	const u_char *, const u_int);

/*
 * Routines to handle various versions of OpenFlow.
 */

struct of_msgtypeinfo {
	/* Should not be NULL. */
	const char *name;
	/* May be NULL to mean "message body printing is not implemented". */
	void (*decoder)(netdissect_options *ndo, const u_char *, const u_int);
	enum {
		REQ_NONE,   /* Message body length may be anything. */
		REQ_FIXLEN, /* Message body length must be == req_value. */
		REQ_MINLEN, /* Message body length must be >= req_value. */
	} req_what;
	uint16_t req_value;
};

extern const struct of_msgtypeinfo *of10_identify_msgtype(const uint8_t);
extern const struct of_msgtypeinfo *of13_identify_msgtype(const uint8_t);
