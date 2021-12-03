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

struct edns_strings* edns_strings_create(void)
{
	struct edns_strings* edns_strings = calloc(1,
		sizeof(struct edns_strings));
	if(!edns_strings)
		return NULL;
	if(!(edns_strings->region = regional_create())) {
		edns_strings_delete(edns_strings);
		return NULL;
	}
	return edns_strings;
}

void edns_strings_delete(struct edns_strings* edns_strings)
{
	if(!edns_strings)
		return;
	regional_destroy(edns_strings->region);
	free(edns_strings);
}

static int
edns_strings_client_insert(struct edns_strings* edns_strings,
	struct sockaddr_storage* addr, socklen_t addrlen, int net,
	const char* string)
{
	struct edns_string_addr* esa = regional_alloc_zero(edns_strings->region,
		sizeof(struct edns_string_addr));
	if(!esa)
		return 0;
	esa->string_len = strlen(string);
	esa->string = regional_alloc_init(edns_strings->region, string,
		esa->string_len);
	if(!esa->string)
		return 0;
	if(!addr_tree_insert(&edns_strings->client_strings, &esa->node, addr,
		addrlen, net)) {
		verbose(VERB_QUERY, "duplicate EDNS client string ignored.");
	}
	return 1;
}

int edns_strings_apply_cfg(struct edns_strings* edns_strings,
	struct config_file* config)
{
	struct config_str2list* c;
	regional_free_all(edns_strings->region);
	addr_tree_init(&edns_strings->client_strings);

	for(c=config->edns_client_strings; c; c=c->next) {
		struct sockaddr_storage addr;
		socklen_t addrlen;
		int net;
		log_assert(c->str && c->str2);

		if(!netblockstrtoaddr(c->str, UNBOUND_DNS_PORT, &addr, &addrlen,
			&net)) {
			log_err("cannot parse EDNS client string IP netblock: "
				"%s", c->str);
			return 0;
		}
		if(!edns_strings_client_insert(edns_strings, &addr, addrlen,
			net, c->str2)) {
			log_err("out of memory while adding EDNS strings");
			return 0;
		}
	}
	edns_strings->client_string_opcode = config->edns_client_string_opcode;

	addr_tree_init_parents(&edns_strings->client_strings);
	return 1;
}

struct edns_string_addr*
edns_string_addr_lookup(rbtree_type* tree, struct sockaddr_storage* addr,
	socklen_t addrlen)
{
	return (struct edns_string_addr*)addr_tree_lookup(tree, addr, addrlen);
}

