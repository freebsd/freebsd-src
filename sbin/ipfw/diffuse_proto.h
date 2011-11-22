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
 * Description:
 * Functions for control protocol.
 */

#ifndef _SBIN_IPFW_DIFFUSE_PROTO_H_
#define _SBIN_IPFW_DIFFUSE_PROTO_H_

#define	DI_COLLECTOR_DEFAULT_LISTEN_PORT	3191
#define	DI_EXPORTER_DEFAULT_LISTEN_PORT		4377

/* Template list. */

struct di_template {
	uint16_t		id;		/* Template id. */
	struct dip_info_element	fields[64];	/* Fields. */
	int			fcnt;		/* Number of template fields. */
	RB_ENTRY(di_template)	node;
};

static inline int
template_compare(struct di_template *a, struct di_template *b)
{

	return ((a->id != b->id) ? (a->id < b->id ? -1 : 1) : 0);
}

RB_HEAD(di_template_head, di_template);
RB_PROTOTYPE(di_template_head, di_template, node, template_compare);

struct dip_info_descr diffuse_proto_get_info(uint16_t id);
void diffuse_proto_print_msg(char *buf, struct di_template_head *templ_list);

#endif /* _SBIN_IPFW_DIFFUSE_PROTO_H_ */
