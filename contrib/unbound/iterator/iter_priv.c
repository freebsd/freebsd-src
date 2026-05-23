/*
 * iterator/iter_priv.c - iterative resolver private address and domain store
 *
 * Copyright (c) 2008, NLnet Labs. All rights reserved.
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
 * This file contains functions to assist the iterator module.
 * Keep track of the private addresses and lookup fast.
 */

#include "config.h"
#include "iterator/iter_priv.h"
#include "util/regional.h"
#include "util/log.h"
#include "util/config_file.h"
#include "util/data/dname.h"
#include "util/data/msgparse.h"
#include "util/net_help.h"
#include "util/storage/dnstree.h"
#include "sldns/str2wire.h"
#include "sldns/sbuffer.h"

struct iter_priv* priv_create(void)
{
	struct iter_priv* priv = (struct iter_priv*)calloc(1, sizeof(*priv));
	if(!priv)
		return NULL;
	priv->region = regional_create();
	if(!priv->region) {
		priv_delete(priv);
		return NULL;
	}
	addr_tree_init(&priv->a);
	name_tree_init(&priv->n);
	return priv;
}

void priv_delete(struct iter_priv* priv)
{
	if(!priv) return;
	regional_destroy(priv->region);
	free(priv);
}

/** Read private-addr declarations from config */
static int read_addrs(struct iter_priv* priv, struct config_file* cfg)
{
	/* parse addresses, report errors, insert into tree */
	struct config_strlist* p;
	struct addr_tree_node* n;
	struct sockaddr_storage addr;
	int net;
	socklen_t addrlen;

	for(p = cfg->private_address; p; p = p->next) {
		log_assert(p->str);
		if(!netblockstrtoaddr(p->str, UNBOUND_DNS_PORT, &addr, 
			&addrlen, &net)) {
			log_err("cannot parse private-address: %s", p->str);
			return 0;
		}
		n = (struct addr_tree_node*)regional_alloc(priv->region,
			sizeof(*n));
		if(!n) {
			log_err("out of memory");
			return 0;
		}
		if(!addr_tree_insert(&priv->a, n, &addr, addrlen, net)) {
			verbose(VERB_QUERY, "ignoring duplicate "
				"private-address: %s", p->str);
		}
	}
	return 1;
}

/** Read private-domain declarations from config */
static int read_names(struct iter_priv* priv, struct config_file* cfg)
{
	/* parse names, report errors, insert into tree */
	struct config_strlist* p;
	struct name_tree_node* n;
	uint8_t* nm, *nmr;
	size_t nm_len;
	int nm_labs;

	for(p = cfg->private_domain; p; p = p->next) {
		log_assert(p->str);
		nm = sldns_str2wire_dname(p->str, &nm_len);
		if(!nm) {
			log_err("cannot parse private-domain: %s", p->str);
			return 0;
		}
		nm_labs = dname_count_size_labels(nm, &nm_len);
		nmr = (uint8_t*)regional_alloc_init(priv->region, nm, nm_len);
		free(nm);
		if(!nmr) {
			log_err("out of memory");
			return 0;
		}
		n = (struct name_tree_node*)regional_alloc(priv->region,
			sizeof(*n));
		if(!n) {
			log_err("out of memory");
			return 0;
		}
		if(!name_tree_insert(&priv->n, n, nmr, nm_len, nm_labs,
			LDNS_RR_CLASS_IN)) {
			verbose(VERB_QUERY, "ignoring duplicate "
				"private-domain: %s", p->str);
		}
	}
	return 1;
}

int priv_apply_cfg(struct iter_priv* priv, struct config_file* cfg)
{
	/* empty the current contents */
	regional_free_all(priv->region);
	addr_tree_init(&priv->a);
	name_tree_init(&priv->n);

	/* read new contents */
	if(!read_addrs(priv, cfg))
		return 0;
	if(!read_names(priv, cfg))
		return 0;

	/* prepare for lookups */
	addr_tree_init_parents(&priv->a);
	name_tree_init_parents(&priv->n);
	return 1;
}

/**
 * See if an address is blocked.
 * @param priv: structure for address storage.
 * @param addr: address to check
 * @param addrlen: length of addr.
 * @return: true if the address must not be queried. false if unlisted.
 */
static int 
priv_lookup_addr(struct iter_priv* priv, struct sockaddr_storage* addr,
	socklen_t addrlen)
{
	return addr_tree_lookup(&priv->a, addr, addrlen) != NULL;
}

/**
 * See if a name is whitelisted.
 * @param priv: structure for address storage.
 * @param pkt: the packet (for compression ptrs).
 * @param name: name to check.
 * @param name_len: uncompressed length of the name to check.
 * @param dclass: class to check.
 * @return: true if the name is OK. false if unlisted.
 */
static int 
priv_lookup_name(struct iter_priv* priv, sldns_buffer* pkt,
	uint8_t* name, size_t name_len, uint16_t dclass)
{
	size_t len;
	uint8_t decomp[256];
	int labs;
	if(name_len >= sizeof(decomp))
		return 0;
	dname_pkt_copy(pkt, decomp, name);
	labs = dname_count_size_labels(decomp, &len);
	log_assert(name_len == len);
	return name_tree_lookup(&priv->n, decomp, len, labs, dclass) != NULL;
}

size_t priv_get_mem(struct iter_priv* priv)
{
	if(!priv) return 0;
	return sizeof(*priv) + regional_get_mem(priv->region);
}

/**
 * Check if svcparam ipv4hint contains a private address.
 * @param priv: private address lookup struct.
 * @param d: the data bytes.
 * @param data_len: number of data bytes in the svcparam.
 * @param addr: address to return the private address to log in to.
 *	It has space for IPv4 and IPv6 addresses.
 * @param addrlen: length of the addr. Returns the correct size for the addr.
 * @return true if the rdata contains a private address.
 */
static int svcb_ipv4hint_contains_priv_addr(struct iter_priv* priv,
	uint8_t* d, uint16_t data_len, struct sockaddr_storage* addr,
	socklen_t* addrlen)
{
	struct sockaddr_in sa;
	*addrlen = (socklen_t)sizeof(struct sockaddr_in);
	memset(&sa, 0, sizeof(struct sockaddr_in));
	sa.sin_family = AF_INET;
	sa.sin_port = (in_port_t)htons(UNBOUND_DNS_PORT);

	while(data_len >= LDNS_IP4ADDRLEN) {
		memmove(&sa.sin_addr, d, LDNS_IP4ADDRLEN);
		memmove(addr, &sa, *addrlen);
		if(priv_lookup_addr(priv, addr, *addrlen))
			return 1;

		d += LDNS_IP4ADDRLEN;
		data_len -= LDNS_IP4ADDRLEN;
	}
	/* if data_len != 0 here, then the svcparam is malformed. */
	return 0;
}

/**
 * Check if svcparam ipv6hint contains a private address.
 * @param priv: private address lookup struct.
 * @param d: the data bytes.
 * @param data_len: number of data bytes in the svcparam.
 * @param addr: address to return the private address to log in to.
 *	It has space for IPv4 and IPv6 addresses.
 * @param addrlen: length of the addr. Returns the correct size for the addr.
 * @return true if the rdata contains a private address.
 */
static int svcb_ipv6hint_contains_priv_addr(struct iter_priv* priv,
	uint8_t* d, uint16_t data_len, struct sockaddr_storage* addr,
	socklen_t* addrlen)
{
	struct sockaddr_in6 sa;
	*addrlen = (socklen_t)sizeof(struct sockaddr_in6);
	memset(&sa, 0, sizeof(struct sockaddr_in6));
	sa.sin6_family = AF_INET6;
	sa.sin6_port = (in_port_t)htons(UNBOUND_DNS_PORT);

	while(data_len >= LDNS_IP6ADDRLEN) {
		memmove(&sa.sin6_addr, d, LDNS_IP6ADDRLEN);
		memmove(addr, &sa, *addrlen);
		if(priv_lookup_addr(priv, addr, *addrlen))
			return 1;

		d += LDNS_IP6ADDRLEN;
		data_len -= LDNS_IP6ADDRLEN;
	}
	/* if data_len != 0 here, then the svcparam is malformed. */
	return 0;
}

/**
 * Check if type SVCB and HTTPS rdata contains a private address.
 * @param priv: private address lookup struct.
 * @param pkt: the packet.
 * @param rr: the rr with rdata to check.
 * @param addr: address to return the private address to log in to.
 * @param addrlen: length of the addr. Initially the total size, on
 *	return the correct size for the addr.
 * @return true if the rdata contains a private address.
 */
static int svcb_rr_contains_priv_addr(struct iter_priv* priv,
	sldns_buffer* pkt, struct rr_parse* rr, struct sockaddr_storage* addr,
	socklen_t* addrlen)
{
	uint8_t* d = rr->ttl_data;
	uint16_t svcparamkey, data_len, rdatalen;
	size_t oldpos, dname_len, dname_start, dname_compr_len;
	d += 4; /* skip TTL */
	rdatalen = sldns_read_uint16(d); /* read rdata length */
	d += 2;

	if(rdatalen < 2 /* priority */ + 1 /* 1 length target */)
		return 0; /* malformed, too short */
	d += 2; /* skip priority */
	rdatalen -= 2;
	oldpos = sldns_buffer_position(pkt);
	sldns_buffer_set_position(pkt, (size_t)(d - sldns_buffer_begin(pkt)));
	dname_start = sldns_buffer_position(pkt);
	dname_len = pkt_dname_len(pkt);
	dname_compr_len = sldns_buffer_position(pkt) - dname_start;
	sldns_buffer_set_position(pkt, oldpos);
	if(dname_len == 0)
		return 0; /* dname malformed */
	if(dname_compr_len > rdatalen)
		return 0; /* malformed */
	d += dname_compr_len; /* skip target */
	rdatalen -= dname_compr_len;

	while(rdatalen >= 4) {
		svcparamkey = sldns_read_uint16(d);
		data_len = sldns_read_uint16(d+2);
		d += 4;
		rdatalen -= 4;

		/* verify that we have data_len data */
		if(data_len > rdatalen) {
			/* It is malformed, but if there are addresses
			 * in there it can be rejected. */
			data_len = rdatalen;
		}

		if(!data_len)
			continue; /* no data for the svcparamkey */

		if(svcparamkey == SVCB_KEY_IPV4HINT) {
			if(svcb_ipv4hint_contains_priv_addr(priv, d, data_len,
				addr, addrlen))
				return 1;
		} else if(svcparamkey == SVCB_KEY_IPV6HINT) {
			if(svcb_ipv6hint_contains_priv_addr(priv, d, data_len,
				addr, addrlen))
				return 1;
		}
		d += data_len;
		rdatalen -= data_len;
	}
	/* If rdatalen != 0 here, then the svcb rdata is malformed. */
	return 0;
}

/**
 * Check if the SVCB and HTTPS rrset is bad.
 * @param priv: private address lookup struct.
 * @param pkt: the packet.
 * @param rrset: the rrset to check.
 * @return 1 if the entire rrset has to be removed. 0 if not.
 * It removes RRs if they have private addresses, and log that.
 */
static int priv_svcb_rrset_bad(struct iter_priv* priv, sldns_buffer* pkt,
	struct rrset_parse* rrset)
{
	struct rr_parse* rr, *prev = NULL;
	struct sockaddr_storage addr;
	socklen_t addrlen = (socklen_t)sizeof(addr);
	for(rr = rrset->rr_first; rr; rr = rr->next) {
		if(svcb_rr_contains_priv_addr(priv, pkt, rr, &addr,
			&addrlen)) {
			if(msgparse_rrset_remove_rr("sanitize: removing public name with private address", pkt, rrset, prev, rr, &addr, addrlen))
				return 1;
			continue;
		}
		prev = rr;
	}
	return 0;
}

int priv_rrset_bad(struct iter_priv* priv, sldns_buffer* pkt,
	struct rrset_parse* rrset)
{
	if(priv->a.count == 0) 
		return 0; /* there are no blocked addresses */

	/* see if it is a private name, that is allowed to have any */
	if(priv_lookup_name(priv, pkt, rrset->dname, rrset->dname_len,
		ntohs(rrset->rrset_class))) {
		return 0;
	} else {
		/* so its a public name, check the address */
		socklen_t len;
		struct rr_parse* rr, *prev = NULL;
		if(rrset->type == LDNS_RR_TYPE_A) {
			struct sockaddr_storage addr;
			struct sockaddr_in sa;

			len = (socklen_t)sizeof(sa);
			memset(&sa, 0, len);
			sa.sin_family = AF_INET;
			sa.sin_port = (in_port_t)htons(UNBOUND_DNS_PORT);
			for(rr = rrset->rr_first; rr; rr = rr->next) {
				if(sldns_read_uint16(rr->ttl_data+4) 
					!= INET_SIZE) {
					prev = rr;
					continue;
				}
				memmove(&sa.sin_addr, rr->ttl_data+4+2, 
					INET_SIZE);
				memmove(&addr, &sa, len);
				if(priv_lookup_addr(priv, &addr, len)) {
					if(msgparse_rrset_remove_rr("sanitize: removing public name with private address", pkt, rrset, prev, rr, &addr, len))
						return 1;
					continue;
				}
				prev = rr;
			}
		} else if(rrset->type == LDNS_RR_TYPE_AAAA) {
			struct sockaddr_storage addr;
			struct sockaddr_in6 sa;
			len = (socklen_t)sizeof(sa);
			memset(&sa, 0, len);
			sa.sin6_family = AF_INET6;
			sa.sin6_port = (in_port_t)htons(UNBOUND_DNS_PORT);
			for(rr = rrset->rr_first; rr; rr = rr->next) {
				if(sldns_read_uint16(rr->ttl_data+4) 
					!= INET6_SIZE) {
					prev = rr;
					continue;
				}
				memmove(&sa.sin6_addr, rr->ttl_data+4+2, 
					INET6_SIZE);
				memmove(&addr, &sa, len);
				if(priv_lookup_addr(priv, &addr, len)) {
					if(msgparse_rrset_remove_rr("sanitize: removing public name with private address", pkt, rrset, prev, rr, &addr, len))
						return 1;
					continue;
				}
				prev = rr;
			}
		} else if(rrset->type == LDNS_RR_TYPE_SVCB ||
			rrset->type == LDNS_RR_TYPE_HTTPS) {
			if(priv_svcb_rrset_bad(priv, pkt, rrset))
				return 1;
		}
	}
	return 0;
}
