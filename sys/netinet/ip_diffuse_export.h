/*-
 * Copyright (c) 2010-2011
 * 	Swinburne University of Technology, Melbourne, Australia.
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Sebastian Zander, made
 * possible in part by a gift from The Cisco University Research Program Fund, a
 * corporate advised fund of Silicon Valley Community Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * The public header file for DIFFUSE export protocol stuff.
 */

#ifndef _NETINET_IP_DIFFUSE_EXPORT_H_
#define _NETINET_IP_DIFFUSE_EXPORT_H_

/* DIFFUSE protocol version. */
#define	DIP_VERSION 1

/* Used if querying MTU from routing table fails. */
#define	DIP_DEFAULT_MTU 1500

#define	DIP_SET_ID_OPTS_TPL	0
#define	DIP_SET_ID_FLOWRULE_TPL	1
#define	DIP_SET_ID_CMD_TPL	2
#define	DIP_SET_ID_DATA		256

enum dip_msg_types {
	DIP_MSG_ADD = 0,
	DIP_MSG_REMOVE,
	DIP_MSG_REQSTATE
};

enum dip_timeout_types {
	DIP_TIMEOUT_NONE = 0,
	DIP_TIMEOUT_RULE,
	DIP_TIMEOUT_FLOW
};

enum dip_info_element_types {
	DIP_IE_NOP = 0,
	DIP_IE_SRC_IPV4,
	DIP_IE_DST_IPV4,
	DIP_IE_SRC_PORT,
	DIP_IE_DST_PORT,
	DIP_IE_PROTO,
	DIP_IE_SRC_IPV6,
	DIP_IE_DST_IPV6,
	DIP_IE_IPV4_TOS,
	DIP_IE_IPV6_LABEL,
	DIP_IE_CLASS_LABEL,
	DIP_IE_MATCH_DIR,
	DIP_IE_MSG_TYPE,	/* Add or remove. */
	DIP_IE_TIMEOUT_TYPE,	/* Rule timeout vs flow timeout. */
	DIP_IE_TIMEOUT,		/* Timeout value. */
	DIP_IE_ACTION_FLAGS,	/* Bidir. */
	DIP_IE_PCKT_CNT,	/* Current number of packets. */
	DIP_IE_KBYTE_CNT,	/* Current number of bytes. */
	DIP_IE_ACTION,		/* Type of action. */
	DIP_IE_ACTION_PARAMS,	/* Opaque, passed on to packet filter. */
	DIP_IE_EXPORT_NAME,	/* Name of export. */
	DIP_IE_CLASSIFIER_NAME,	/* Name of classifier. */
	DIP_IE_CLASSES		/* Classifier names/classes. */
};

#define	DI_IS_FIXED_LEN(x)	(((x) & 0xC000) == 0x0)
#define	DI_IS_VARIABLE_LEN(x)	(((x) & 0xC000) == 0x8000)
#define	DI_IS_DYNAMIC_LEN(x)	(((x) & 0xC000) == 0xC000)

struct dip_info_element {
	uint16_t	idx;
	uint16_t	id;
	int16_t		len;
};

struct dip_info_descr {
	uint16_t	idx;
	uint16_t	id;
	int16_t		len; /* Length in bytes, 0/-1 = var/dynamic length. */
	char		*name;
};

struct dip_header {
	uint16_t	version;
	uint16_t	msg_len;
	uint32_t	seq_no;
	uint32_t	time;
};

struct dip_set_header {
	uint16_t	set_id;
	uint16_t	set_len;
};

struct dip_templ_header {
	uint16_t	templ_id;
	uint16_t	flags;
};

#if defined(WITH_DIP_INFO)
static struct dip_info_descr dip_info[] = {
	{DIP_IE_NOP,			0,	0,	"NOP"},
	{DIP_IE_SRC_IPV4,		1,	4,	"SrcIP"},
	{DIP_IE_DST_IPV4,		2,	4,	"DstIP"},
	{DIP_IE_SRC_PORT,		3,	2,	"SrcPort"},
	{DIP_IE_DST_PORT,		4,	2,	"DstPort"},
	{DIP_IE_PROTO,	 		5,	1,	"Proto"},
	{DIP_IE_SRC_IPV6,		6,	16,	"SrcIP6"},
	{DIP_IE_DST_IPV6,		7,	16,	"DstIP6"},
	{DIP_IE_IPV4_TOS,		8,	1,	"ToS"},
	{DIP_IE_IPV6_LABEL,		9,	3,	"IP6Label"},
	{DIP_IE_CLASS_LABEL,		10,	2,	"Class"},
	{DIP_IE_MATCH_DIR,		11,	1,	"MatchDir"},
	{DIP_IE_MSG_TYPE,		12,	1,	"MsgType"},
	{DIP_IE_TIMEOUT_TYPE,		13,	1,	"TimeoutType"},
	{DIP_IE_TIMEOUT,		14,	2,	"TimeoutValue"},
	{DIP_IE_ACTION_FLAGS,		15,	2,	"ActionFlags"},
	{DIP_IE_PCKT_CNT,		16,	4,	"Packets"},
	{DIP_IE_KBYTE_CNT,		17,	4,	"KBytes"},
	{DIP_IE_ACTION,			32768,	0,	"Action"},
	{DIP_IE_ACTION_PARAMS,		32769,	0,	"ActionParams"},
	{DIP_IE_EXPORT_NAME,		32770,	0,	"ExportName"},
	{DIP_IE_CLASSIFIER_NAME,	32771,	0,	"ClassName"},
	{DIP_IE_CLASSES,		49152,	-1,	"ClassNames"},
	{DIP_IE_NOP,			0,	0,	"Unknown"}
};

/* Default flow rule template. */
static uint16_t def_flowrule_template[15] = {
	DIP_IE_EXPORT_NAME,
	DIP_IE_MSG_TYPE,
	DIP_IE_SRC_IPV4,
	DIP_IE_DST_IPV4,
	DIP_IE_SRC_PORT,
	DIP_IE_DST_PORT,
	DIP_IE_PROTO,
	DIP_IE_PCKT_CNT,
	DIP_IE_KBYTE_CNT,
	DIP_IE_CLASSES,
	DIP_IE_TIMEOUT_TYPE,
	DIP_IE_TIMEOUT,
	DIP_IE_ACTION,
	DIP_IE_ACTION_FLAGS,
	DIP_IE_ACTION_PARAMS
};
#define	N_DEFAULT_FLOWRULE_TEMPLATE_ITEMS (sizeof(def_flowrule_template) / \
    sizeof(*def_flowrule_template))

#endif /* WITH_DIP_INFO */

#endif /* _NETINET_IP_DIFFUSE_EXPORT_H_ */
