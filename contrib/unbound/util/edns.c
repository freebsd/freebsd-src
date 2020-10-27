/*
 * util/edns.c - handle base EDNS options.
 *
 * Copyright (c) 2018, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This file contains functions for base EDNS options.
 */

#include "config.h"
#include "util/edns.h"
#include "util/config_file.h"
#include "util/netevent.h"
#include "util/net_help.h"
#include "util/regional.h"
#include "util/data/msgparse.h"
#include "util/data/msgreply.h"

#if 0
/* XXX: remove me */
#include "edns.h"
#endif

struct edns_tags* edns_tags_create(void)
{
	struct edns_tags* edns_tags = calloc(1, sizeof(struct edns_tags));
	if(!edns_tags)
		return NULL;
	if(!(edns_tags->region = regional_create())) {
		edns_tags_delete(edns_tags);
		return NULL;
	}
	return edns_tags;
}

void edns_tags_delete(struct edns_tags* edns_tags)
{
	if(!edns_tags)
		return;
	regional_destroy(edns_tags->region);
	free(edns_tags);
}

static int
edns_tags_client_insert(struct edns_tags* edns_tags,
	struct sockaddr_storage* addr, socklen_t addrlen, int net,
	uint16_t tag_data)
{
	struct edns_tag_addr* eta = regional_alloc_zero(edns_tags->region,
		sizeof(struct edns_tag_addr));
	if(!eta)
		return 0;
	eta->tag_data = tag_data;
	if(!addr_tree_insert(&edns_tags->client_tags, &eta->node, addr, addrlen,
		net)) {
		verbose(VERB_QUERY, "duplicate EDNS client tag ignored.");
	}
	return 1;
}

int edns_tags_apply_cfg(struct edns_tags* edns_tags,
	struct config_file* config)
{
	struct config_str2list* c;
	regional_free_all(edns_tags->region);
	addr_tree_init(&edns_tags->client_tags);

	for(c=config->edns_client_tags; c; c=c->next) {
		struct sockaddr_storage addr;
		socklen_t addrlen;
		int net;
		uint16_t tag_data;
		log_assert(c->str && c->str2);

		if(!netblockstrtoaddr(c->str, UNBOUND_DNS_PORT, &addr, &addrlen,
			&net)) {
			log_err("cannot parse EDNS client tag IP netblock: %s",
				c->str);
			return 0;
		}
		tag_data = atoi(c->str2); /* validated in config parser */
		if(!edns_tags_client_insert(edns_tags, &addr, addrlen, net,
			tag_data)) {
			log_err("out of memory while adding EDNS tags");
			return 0;
		}
	}
	edns_tags->client_tag_opcode = config->edns_client_tag_opcode;

	addr_tree_init_parents(&edns_tags->client_tags);
	return 1;
}

struct edns_tag_addr*
edns_tag_addr_lookup(rbtree_type* tree, struct sockaddr_storage* addr,
	socklen_t addrlen)
{
	return (struct edns_tag_addr*)addr_tree_lookup(tree, addr, addrlen);
}

static int edns_keepalive(struct edns_data* edns_out, struct edns_data* edns_in,
		struct comm_point* c, struct regional* region)
{
	if(c->type == comm_udp)
		return 1;

	/* To respond with a Keepalive option, the client connection
	 * must have received one message with a TCP Keepalive EDNS option,
	 * and that option must have 0 length data. Subsequent messages
	 * sent on that connection will have a TCP Keepalive option.
	 */
	if(c->tcp_keepalive ||
		edns_opt_list_find(edns_in->opt_list, LDNS_EDNS_KEEPALIVE)) {
		int keepalive = c->tcp_timeout_msec / 100;
		uint8_t data[2];
		data[0] = (uint8_t)((keepalive >> 8) & 0xff);
		data[1] = (uint8_t)(keepalive & 0xff);
		if(!edns_opt_list_append(&edns_out->opt_list, LDNS_EDNS_KEEPALIVE,
			sizeof(data), data, region))
			return 0;
		c->tcp_keepalive = 1;
	}
	return 1;
}

int apply_edns_options(struct edns_data* edns_out, struct edns_data* edns_in,
	struct config_file* cfg, struct comm_point* c, struct regional* region)
{
	if(cfg->do_tcp_keepalive &&
		!edns_keepalive(edns_out, edns_in, c, region))
		return 0;

	return 1;
}
