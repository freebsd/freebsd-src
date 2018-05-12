/*
 * edns-subnet/subnet-whitelist.h - Hosts we actively try to send subnet option
 * to.
 *
 * Copyright (c) 2013, NLnet Labs. All rights reserved.
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
 * Keep track of the white listed servers for subnet option. Based
 * on acl_list.c|h
 */

#ifndef EDNSSUBNET_WHITELIST_H
#define EDNSSUBNET_WHITELIST_H
#include "util/storage/dnstree.h"

struct config_file;
struct regional;

/**
 * ednssubnet_upstream structure
 */
struct ednssubnet_upstream {
	/** regional for allocation */
	struct regional* region;
	/** 
	 * Tree of the address spans that are whitelisted.
	 * contents of type addr_tree_node. Each node is an address span 
	 * Unbound will append subnet option for.
	 */
	rbtree_type tree;
};

/**
 * Create ednssubnet_upstream structure 
 * @return new structure or NULL on error.
 */
struct ednssubnet_upstream* upstream_create(void);

/**
 * Delete ednssubnet_upstream structure.
 * @param upstream: to delete.
 */
void upstream_delete(struct ednssubnet_upstream* upstream);

/**
 * Process ednssubnet_upstream config.
 * @param upstream: where to store.
 * @param cfg: config options.
 * @return 0 on error.
 */
int upstream_apply_cfg(struct ednssubnet_upstream* upstream,
	struct config_file* cfg);

/**
 * See if an address is whitelisted.
 * @param upstream: structure for address storage.
 * @param addr: address to check
 * @param addrlen: length of addr.
 * @return: true if the address is whitelisted for subnet option. 
 */
int upstream_is_whitelisted(struct ednssubnet_upstream* upstream,
	struct sockaddr_storage* addr, socklen_t addrlen);

/**
 * Get memory used by ednssubnet_upstream structure.
 * @param upstream: structure for address storage.
 * @return bytes in use.
 */
size_t upstream_get_mem(struct ednssubnet_upstream* upstream);

#endif /* EDNSSUBNET_WHITELIST_H */
