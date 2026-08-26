/*
 * Copyright (c) 2026 Justin Hibbits
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	DPAA_FMAN_PARSER_H
#define	DPAA_FMAN_PARSER_H

#define	FMAN_PARSE_RESULT_OFF	32
#define	L3R_FIRST_IPV4		0x8000
#define	L3R_FIRST_IPV6		0x4000
#define	L3R_FIRST_IP_M		(L3R_FIRST_IPV4 | L3R_FIRST_IPV6)
#define	L3R_LAST_IPV4		0x8000
#define	L3R_LAST_IPV6		0x4000
#define	L3R_LAST_IP_M		(L3R_LAST_IPV4 | L3R_LAST_IPV6)
#define	L3R_FIRST_ERROR		0x2000
#define	L3R_LAST_ERROR		0x0080
#define	L4R_TYPE_M		0xe0
#define	L4R_TYPE_TCP		0x20
#define	L4R_TYPE_UDP		0x40
#define	L4R_TYPE_IPSEC		0x60
#define	L4R_TYPE_SCTP		0x80
#define	L4R_DCCP		0xa0
#define	L4R_ERR			0x10

#endif
