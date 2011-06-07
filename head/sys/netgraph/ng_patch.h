/*-
 * Copyright (C) 2010 by Maxim Ignatenko <gelraen.ua@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef _NETGRAPH_NG_PATCH_H_
#define _NETGRAPH_NG_PATCH_H_

/* Node type name. */
#define	NG_PATCH_NODE_TYPE	"patch"

/* Node type cookie. */
#define	NGM_PATCH_COOKIE	1262445509

/* Hook names */
#define	NG_PATCH_HOOK_IN	"in"
#define	NG_PATCH_HOOK_OUT	"out"

/* Netgraph commands understood by this node type */
enum {
	NGM_PATCH_SETCONFIG = 1,
	NGM_PATCH_GETCONFIG,
	NGM_PATCH_GET_STATS,
	NGM_PATCH_CLR_STATS,
	NGM_PATCH_GETCLR_STATS
};

/* Patching modes */
enum {
	NG_PATCH_MODE_SET = 1,
	NG_PATCH_MODE_ADD = 2,
	NG_PATCH_MODE_SUB = 3,
	NG_PATCH_MODE_MUL = 4,
	NG_PATCH_MODE_DIV = 5,
	NG_PATCH_MODE_NEG = 6,
	NG_PATCH_MODE_AND = 7,
	NG_PATCH_MODE_OR = 8,
	NG_PATCH_MODE_XOR = 9,
	NG_PATCH_MODE_SHL = 10,
	NG_PATCH_MODE_SHR = 11
};

struct ng_patch_op {
	uint64_t	value;
	uint32_t	offset;
	uint16_t	length;	/* 1,2,4 or 8 (bytes) */
	uint16_t	mode;
};

#define	NG_PATCH_OP_TYPE_INFO	{	\
		{ "value",	&ng_parse_uint64_type	},	\
		{ "offset",	&ng_parse_uint32_type	},	\
		{ "length",	&ng_parse_uint16_type	},	\
		{ "mode",	&ng_parse_uint16_type	},	\
		{ NULL } \
}

struct ng_patch_config {
	uint32_t	count;
	uint32_t	csum_flags;
	struct ng_patch_op ops[];
};

#define	NG_PATCH_CONFIG_TYPE_INFO	{	\
		{ "count",	&ng_parse_uint32_type	},	\
		{ "csum_flags",	&ng_parse_uint32_type	},	\
		{ "ops",	&ng_patch_confarr_type	},	\
		{ NULL } \
}

struct ng_patch_stats {
	uint64_t	received;
	uint64_t	patched;
	uint64_t	dropped;
};

#define	NG_PATCH_STATS_TYPE_INFO {	\
		{ "received",	&ng_parse_uint64_type	},	\
		{ "patched",	&ng_parse_uint64_type	},	\
		{ "dropped",	&ng_parse_uint64_type	},	\
		{ NULL } \
}

#endif /* _NETGRAPH_NG_PATCH_H_ */
