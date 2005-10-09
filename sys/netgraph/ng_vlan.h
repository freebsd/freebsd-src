/*-
 * Copyright (c) 2003 IPNET Internet Communication Company
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
 * Author: Ruslan Ermilov <ru@FreeBSD.org>
 *
 * $FreeBSD$
 */

#ifndef _NETGRAPH_NG_VLAN_H_
#define	_NETGRAPH_NG_VLAN_H_

/* Node type name and magic cookie. */
#define	NG_VLAN_NODE_TYPE	"vlan"
#define	NGM_VLAN_COOKIE		1068486472

/* Hook names. */
#define	NG_VLAN_HOOK_DOWNSTREAM	"downstream"
#define	NG_VLAN_HOOK_NOMATCH	"nomatch"

/* Netgraph commands. */
enum {
	NGM_VLAN_ADD_FILTER = 1,
	NGM_VLAN_DEL_FILTER,
	NGM_VLAN_GET_TABLE
};

/* For NGM_VLAN_ADD_FILTER control message. */
struct ng_vlan_filter {
	char		hook[NG_HOOKSIZ];
	u_int16_t	vlan;
};	

/* Keep this in sync with the above structure definition.  */
#define	NG_VLAN_FILTER_FIELDS	{				\
	{ "hook",	&ng_parse_hookbuf_type  },		\
	{ "vlan",	&ng_parse_uint16_type   },		\
	{ NULL }						\
}

/* Structure returned by NGM_VLAN_GET_TABLE. */
struct ng_vlan_table {
	u_int32_t	n;
	struct ng_vlan_filter filter[0];
};

/* Keep this in sync with the above structure definition. */
#define	NG_VLAN_TABLE_FIELDS	{				\
	{ "n",		&ng_parse_uint32_type },		\
	{ "filter",	&ng_vlan_table_array_type },		\
	{ NULL }						\
}

#endif /* _NETGRAPH_NG_VLAN_H_ */
