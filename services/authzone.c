/*
 * services/authzone.c - authoritative zone that is locally hosted.
 *
 * Copyright (c) 2017, NLnet Labs. All rights reserved.
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
 * This file contains the functions for an authority zone.  This zone
 * is queried by the iterator, just like a stub or forward zone, but then
 * the data is locally held.
 */

#include "config.h"
#include "services/authzone.h"
#include "util/data/dname.h"
#include "util/data/msgreply.h"
#include "util/data/packed_rrset.h"
#include "util/regional.h"
#include "util/net_help.h"
#include "util/config_file.h"
#include "util/log.h"
#include "services/cache/dns.h"
#include "sldns/rrdef.h"
#include "sldns/pkthdr.h"
#include "sldns/sbuffer.h"
#include "sldns/str2wire.h"
#include "sldns/wire2str.h"
#include "sldns/parseutil.h"
#include "validator/val_nsec3.h"
#include "validator/val_secalgo.h"

/** bytes to use for NSEC3 hash buffer. 20 for sha1 */
#define N3HASHBUFLEN 32
/** max number of CNAMEs we are willing to follow (in one answer) */
#define MAX_CNAME_CHAIN 8

/** create new dns_msg */
static struct dns_msg*
msg_create(struct regional* region, struct query_info* qinfo)
{
	struct dns_msg* msg = (struct dns_msg*)regional_alloc(region,
		sizeof(struct dns_msg));
	if(!msg)
		return NULL;
	msg->qinfo.qname = regional_alloc_init(region, qinfo->qname,
		qinfo->qname_len);
	if(!msg->qinfo.qname)
		return NULL;
	msg->qinfo.qname_len = qinfo->qname_len;
	msg->qinfo.qtype = qinfo->qtype;
	msg->qinfo.qclass = qinfo->qclass;
	msg->qinfo.local_alias = NULL;
	/* non-packed reply_info, because it needs to grow the array */
	msg->rep = (struct reply_info*)regional_alloc_zero(region,
		sizeof(struct reply_info)-sizeof(struct rrset_ref));
	if(!msg->rep)
		return NULL;
	msg->rep->flags = (uint16_t)(BIT_QR | BIT_AA);
	msg->rep->authoritative = 1;
	msg->rep->qdcount = 1;
	/* rrsets is NULL, no rrsets yet */
	return msg;
}

/** grow rrset array by one in msg */
static int
msg_grow_array(struct regional* region, struct dns_msg* msg)
{
	if(msg->rep->rrsets == NULL) {
		msg->rep->rrsets = regional_alloc_zero(region,
			sizeof(struct ub_packed_rrset_key*)*(msg->rep->rrset_count+1));
		if(!msg->rep->rrsets)
			return 0;
	} else {
		struct ub_packed_rrset_key** rrsets_old = msg->rep->rrsets;
		msg->rep->rrsets = regional_alloc_zero(region,
			sizeof(struct ub_packed_rrset_key*)*(msg->rep->rrset_count+1));
		if(!msg->rep->rrsets)
			return 0;
		memmove(msg->rep->rrsets, rrsets_old,
			sizeof(struct ub_packed_rrset_key*)*msg->rep->rrset_count);
	}
	return 1;
}

/** get ttl of rrset */
static time_t
get_rrset_ttl(struct ub_packed_rrset_key* k)
{
	struct packed_rrset_data* d = (struct packed_rrset_data*)
		k->entry.data;
	return d->ttl;
}

/** Copy rrset into region from domain-datanode and packet rrset */
static struct ub_packed_rrset_key*
auth_packed_rrset_copy_region(struct auth_zone* z, struct auth_data* node,
	struct auth_rrset* rrset, struct regional* region, time_t adjust)
{
	struct ub_packed_rrset_key key;
	memset(&key, 0, sizeof(key));
	key.entry.key = &key;
	key.entry.data = rrset->data;
	key.rk.dname = node->name;
	key.rk.dname_len = node->namelen;
	key.rk.type = htons(rrset->type);
	key.rk.rrset_class = htons(z->dclass);
	key.entry.hash = rrset_key_hash(&key.rk);
	return packed_rrset_copy_region(&key, region, adjust);
}

/** fix up msg->rep TTL and prefetch ttl */
static void
msg_ttl(struct dns_msg* msg)
{
	if(msg->rep->rrset_count == 0) return;
	if(msg->rep->rrset_count == 1) {
		msg->rep->ttl = get_rrset_ttl(msg->rep->rrsets[0]);
		msg->rep->prefetch_ttl = PREFETCH_TTL_CALC(msg->rep->ttl);
	} else if(get_rrset_ttl(msg->rep->rrsets[msg->rep->rrset_count-1]) <
		msg->rep->ttl) {
		msg->rep->ttl = get_rrset_ttl(msg->rep->rrsets[
			msg->rep->rrset_count-1]);
		msg->rep->prefetch_ttl = PREFETCH_TTL_CALC(msg->rep->ttl);
	}
}

/** see if rrset is a duplicate in the answer message */
static int
msg_rrset_duplicate(struct dns_msg* msg, uint8_t* nm, size_t nmlen,
	uint16_t type, uint16_t dclass)
{
	size_t i;
	for(i=0; i<msg->rep->rrset_count; i++) {
		struct ub_packed_rrset_key* k = msg->rep->rrsets[i];
		if(ntohs(k->rk.type) == type && k->rk.dname_len == nmlen &&
			ntohs(k->rk.rrset_class) == dclass &&
			query_dname_compare(k->rk.dname, nm) == 0)
			return 1;
	}
	return 0;
}

/** add rrset to answer section (no auth, add rrsets yet) */
static int
msg_add_rrset_an(struct auth_zone* z, struct regional* region,
	struct dns_msg* msg, struct auth_data* node, struct auth_rrset* rrset)
{
	log_assert(msg->rep->ns_numrrsets == 0);
	log_assert(msg->rep->ar_numrrsets == 0);
	if(!rrset)
		return 1;
	if(msg_rrset_duplicate(msg, node->name, node->namelen, rrset->type,
		z->dclass))
		return 1;
	/* grow array */
	if(!msg_grow_array(region, msg))
		return 0;
	/* copy it */
	if(!(msg->rep->rrsets[msg->rep->rrset_count] =
		auth_packed_rrset_copy_region(z, node, rrset, region, 0)))
		return 0;
	msg->rep->rrset_count++;
	msg->rep->an_numrrsets++;
	msg_ttl(msg);
	return 1;
}

/** add rrset to authority section (no additonal section rrsets yet) */
static int
msg_add_rrset_ns(struct auth_zone* z, struct regional* region,
	struct dns_msg* msg, struct auth_data* node, struct auth_rrset* rrset)
{
	log_assert(msg->rep->ar_numrrsets == 0);
	if(!rrset)
		return 1;
	if(msg_rrset_duplicate(msg, node->name, node->namelen, rrset->type,
		z->dclass))
		return 1;
	/* grow array */
	if(!msg_grow_array(region, msg))
		return 0;
	/* copy it */
	if(!(msg->rep->rrsets[msg->rep->rrset_count] =
		auth_packed_rrset_copy_region(z, node, rrset, region, 0)))
		return 0;
	msg->rep->rrset_count++;
	msg->rep->ns_numrrsets++;
	msg_ttl(msg);
	return 1;
}

/** add rrset to additional section */
static int
msg_add_rrset_ar(struct auth_zone* z, struct regional* region,
	struct dns_msg* msg, struct auth_data* node, struct auth_rrset* rrset)
{
	if(!rrset)
		return 1;
	if(msg_rrset_duplicate(msg, node->name, node->namelen, rrset->type,
		z->dclass))
		return 1;
	/* grow array */
	if(!msg_grow_array(region, msg))
		return 0;
	/* copy it */
	if(!(msg->rep->rrsets[msg->rep->rrset_count] =
		auth_packed_rrset_copy_region(z, node, rrset, region, 0)))
		return 0;
	msg->rep->rrset_count++;
	msg->rep->ar_numrrsets++;
	msg_ttl(msg);
	return 1;
}

struct auth_zones* auth_zones_create(void)
{
	struct auth_zones* az = (struct auth_zones*)calloc(1, sizeof(*az));
	if(!az) {
		log_err("out of memory");
		return NULL;
	}
	rbtree_init(&az->ztree, &auth_zone_cmp);
	lock_rw_init(&az->lock);
	lock_protect(&az->lock, &az->ztree, sizeof(az->ztree));
	/* also lock protects the rbnode's in struct auth_zone */
	return az;
}

int auth_zone_cmp(const void* z1, const void* z2)
{
	/* first sort on class, so that hierarchy can be maintained within
	 * a class */
	struct auth_zone* a = (struct auth_zone*)z1;
	struct auth_zone* b = (struct auth_zone*)z2;
	int m;
	if(a->dclass != b->dclass) {
		if(a->dclass < b->dclass)
			return -1;
		return 1;
	}
	/* sorted such that higher zones sort before lower zones (their
	 * contents) */
	return dname_lab_cmp(a->name, a->namelabs, b->name, b->namelabs, &m);
}

int auth_data_cmp(const void* z1, const void* z2)
{
	struct auth_data* a = (struct auth_data*)z1;
	struct auth_data* b = (struct auth_data*)z2;
	int m;
	/* canonical sort, because DNSSEC needs that */
	return dname_canon_lab_cmp(a->name, a->namelabs, b->name,
		b->namelabs, &m);
}

/** delete auth rrset node */
static void
auth_rrset_delete(struct auth_rrset* rrset)
{
	if(!rrset) return;
	free(rrset->data);
	free(rrset);
}

/** delete auth data domain node */
static void
auth_data_delete(struct auth_data* n)
{
	struct auth_rrset* p, *np;
	if(!n) return;
	p = n->rrsets;
	while(p) {
		np = p->next;
		auth_rrset_delete(p);
		p = np;
	}
	free(n->name);
	free(n);
}

/** helper traverse to delete zones */
static void
auth_data_del(rbnode_type* n, void* ATTR_UNUSED(arg))
{
	struct auth_data* z = (struct auth_data*)n->key;
	auth_data_delete(z);
}

/** delete an auth zone structure (tree remove must be done elsewhere) */
static void
auth_zone_delete(struct auth_zone* z)
{
	if(!z) return;
	lock_rw_destroy(&z->lock);
	traverse_postorder(&z->data, auth_data_del, NULL);
	free(z->name);
	free(z->zonefile);
	free(z);
}

struct auth_zone*
auth_zone_create(struct auth_zones* az, uint8_t* nm, size_t nmlen,
	uint16_t dclass)
{
	struct auth_zone* z = (struct auth_zone*)calloc(1, sizeof(*z));
	if(!z) {
		return NULL;
	}
	z->node.key = z;
	z->dclass = dclass;
	z->namelen = nmlen;
	z->namelabs = dname_count_labels(nm);
	z->name = memdup(nm, nmlen);
	if(!z->name) {
		free(z);
		return NULL;
	}
	rbtree_init(&z->data, &auth_data_cmp);
	lock_rw_init(&z->lock);
	lock_protect(&z->lock, &z->name, sizeof(*z)-sizeof(rbnode_type));
	lock_rw_wrlock(&z->lock);
	/* z lock protects all, except rbtree itself, which is az->lock */
	if(!rbtree_insert(&az->ztree, &z->node)) {
		lock_rw_unlock(&z->lock);
		auth_zone_delete(z);
		log_warn("duplicate auth zone");
		return NULL;
	}
	return z;
}

struct auth_zone*
auth_zone_find(struct auth_zones* az, uint8_t* nm, size_t nmlen,
	uint16_t dclass)
{
	struct auth_zone key;
	key.node.key = &key;
	key.dclass = dclass;
	key.name = nm;
	key.namelen = nmlen;
	key.namelabs = dname_count_labels(nm);
	return (struct auth_zone*)rbtree_search(&az->ztree, &key);
}

/** find an auth zone or sorted less-or-equal, return true if exact */
static int
auth_zone_find_less_equal(struct auth_zones* az, uint8_t* nm, size_t nmlen,
	uint16_t dclass, struct auth_zone** z)
{
	struct auth_zone key;
	key.node.key = &key;
	key.dclass = dclass;
	key.name = nm;
	key.namelen = nmlen;
	key.namelabs = dname_count_labels(nm);
	return rbtree_find_less_equal(&az->ztree, &key, (rbnode_type**)z);
}

/** find the auth zone that is above the given qname */
struct auth_zone*
auth_zones_find_zone(struct auth_zones* az, struct query_info* qinfo)
{
	uint8_t* nm = qinfo->qname;
	size_t nmlen = qinfo->qname_len;
	struct auth_zone* z;
	if(auth_zone_find_less_equal(az, nm, nmlen, qinfo->qclass, &z)) {
		/* exact match */
		return z;
	} else {
		/* less-or-nothing */
		if(!z) return NULL; /* nothing smaller, nothing above it */
		/* we found smaller name; smaller may be above the qname,
		 * but not below it. */
		nm = dname_get_shared_topdomain(z->name, qinfo->qname);
		dname_count_size_labels(nm, &nmlen);
	}
	/* search up */
	while(!z && !dname_is_root(nm)) {
		dname_remove_label(&nm, &nmlen);
		z = auth_zone_find(az, nm, nmlen, qinfo->qclass);
	}
	return z;
}

/** find or create zone with name str. caller must have lock on az. 
 * returns a wrlocked zone */
static struct auth_zone*
auth_zones_find_or_add_zone(struct auth_zones* az, char* name)
{
	uint8_t nm[LDNS_MAX_DOMAINLEN+1];
	size_t nmlen = sizeof(nm);
	struct auth_zone* z;

	if(sldns_str2wire_dname_buf(name, nm, &nmlen) != 0) {
		log_err("cannot parse auth zone name: %s", name);
		return 0;
	}
	z = auth_zone_find(az, nm, nmlen, LDNS_RR_CLASS_IN);
	if(!z) {
		/* not found, create the zone */
		z = auth_zone_create(az, nm, nmlen, LDNS_RR_CLASS_IN);
	} else {
		lock_rw_wrlock(&z->lock);
	}
	return z;
}

int
auth_zone_set_zonefile(struct auth_zone* z, char* zonefile)
{
	if(z->zonefile) free(z->zonefile);
	if(zonefile == NULL) {
		z->zonefile = NULL;
	} else {
		z->zonefile = strdup(zonefile);
		if(!z->zonefile) {
			log_err("malloc failure");
			return 0;
		}
	}
	return 1;
}

/** set auth zone fallback. caller must have lock on zone */
int
auth_zone_set_fallback(struct auth_zone* z, char* fallbackstr)
{
	if(strcmp(fallbackstr, "yes") != 0 && strcmp(fallbackstr, "no") != 0){
		log_err("auth zone fallback, expected yes or no, got %s",
			fallbackstr);
		return 0;
	}
	z->fallback_enabled = (strcmp(fallbackstr, "yes")==0);
	return 1;
}

/** create domain with the given name */
static struct auth_data*
az_domain_create(struct auth_zone* z, uint8_t* nm, size_t nmlen)
{
	struct auth_data* n = (struct auth_data*)malloc(sizeof(*n));
	if(!n) return NULL;
	memset(n, 0, sizeof(*n));
	n->node.key = n;
	n->name = memdup(nm, nmlen);
	if(!n->name) {
		free(n);
		return NULL;
	}
	n->namelen = nmlen;
	n->namelabs = dname_count_labels(nm);
	if(!rbtree_insert(&z->data, &n->node)) {
		log_warn("duplicate auth domain name");
		free(n->name);
		free(n);
		return NULL;
	}
	return n;
}

/** find domain with exactly the given name */
static struct auth_data*
az_find_name(struct auth_zone* z, uint8_t* nm, size_t nmlen)
{
	struct auth_zone key;
	key.node.key = &key;
	key.name = nm;
	key.namelen = nmlen;
	key.namelabs = dname_count_labels(nm);
	return (struct auth_data*)rbtree_search(&z->data, &key);
}

/** Find domain name (or closest match) */
static void
az_find_domain(struct auth_zone* z, struct query_info* qinfo, int* node_exact,
	struct auth_data** node)
{
	struct auth_zone key;
	key.node.key = &key;
	key.name = qinfo->qname;
	key.namelen = qinfo->qname_len;
	key.namelabs = dname_count_labels(key.name);
	*node_exact = rbtree_find_less_equal(&z->data, &key,
		(rbnode_type**)node);
}

/** find or create domain with name in zone */
static struct auth_data*
az_domain_find_or_create(struct auth_zone* z, uint8_t* dname,
	size_t dname_len)
{
	struct auth_data* n = az_find_name(z, dname, dname_len);
	if(!n) {
		n = az_domain_create(z, dname, dname_len);
	}
	return n;
}

/** find rrset of given type in the domain */
static struct auth_rrset*
az_domain_rrset(struct auth_data* n, uint16_t t)
{
	struct auth_rrset* rrset;
	if(!n) return NULL;
	rrset = n->rrsets;
	while(rrset) {
		if(rrset->type == t)
			return rrset;
		rrset = rrset->next;
	}
	return NULL;
}

/** remove rrset of this type from domain */
static void
domain_remove_rrset(struct auth_data* node, uint16_t rr_type)
{
	struct auth_rrset* rrset, *prev;
	if(!node) return;
	prev = NULL;
	rrset = node->rrsets;
	while(rrset) {
		if(rrset->type == rr_type) {
			/* found it, now delete it */
			if(prev) prev->next = rrset->next;
			else	node->rrsets = rrset->next;
			auth_rrset_delete(rrset);
			return;
		}
		prev = rrset;
		rrset = rrset->next;
	}
}

/** see if rdata is duplicate */
static int
rdata_duplicate(struct packed_rrset_data* d, uint8_t* rdata, size_t len)
{
	size_t i;
	for(i=0; i<d->count + d->rrsig_count; i++) {
		if(d->rr_len[i] != len)
			continue;
		if(memcmp(d->rr_data[i], rdata, len) == 0)
			return 1;
	}
	return 0;
}

/** get rrsig type covered from rdata.
 * @param rdata: rdata in wireformat, starting with 16bit rdlength.
 * @param rdatalen: length of rdata buffer.
 * @return type covered (or 0).
 */
static uint16_t
rrsig_rdata_get_type_covered(uint8_t* rdata, size_t rdatalen)
{
	if(rdatalen < 4)
		return 0;
	return sldns_read_uint16(rdata+2);
}

/** add RR to existing RRset. If insert_sig is true, add to rrsigs. 
 * This reallocates the packed rrset for a new one */
static int
rrset_add_rr(struct auth_rrset* rrset, uint32_t rr_ttl, uint8_t* rdata,
	size_t rdatalen, int insert_sig)
{
	struct packed_rrset_data* d, *old = rrset->data;
	size_t total, old_total;

	d = (struct packed_rrset_data*)calloc(1, packed_rrset_sizeof(old)
		+ sizeof(size_t) + sizeof(uint8_t*) + sizeof(time_t)
		+ rdatalen);
	if(!d) {
		log_err("out of memory");
		return 0;
	}
	/* copy base values */
	memcpy(d, old, sizeof(struct packed_rrset_data));
	if(!insert_sig) {
		d->count++;
	} else {
		d->rrsig_count++;
	}
	old_total = old->count + old->rrsig_count;
	total = d->count + d->rrsig_count;
	/* set rr_len, needed for ptr_fixup */
	d->rr_len = (size_t*)((uint8_t*)d +
		sizeof(struct packed_rrset_data));
	if(old->count != 0)
		memmove(d->rr_len, old->rr_len, old->count*sizeof(size_t));
	if(old->rrsig_count != 0)
		memmove(d->rr_len+d->count, old->rr_len+old->count,
			old->rrsig_count*sizeof(size_t));
	if(!insert_sig)
		d->rr_len[d->count-1] = rdatalen;
	else	d->rr_len[total-1] = rdatalen;
	packed_rrset_ptr_fixup(d);
	if((time_t)rr_ttl < d->ttl)
		d->ttl = rr_ttl;

	/* copy old values into new array */
	if(old->count != 0) {
		memmove(d->rr_ttl, old->rr_ttl, old->count*sizeof(time_t));
		/* all the old rr pieces are allocated sequential, so we
		 * can copy them in one go */
		memmove(d->rr_data[0], old->rr_data[0],
			(old->rr_data[old->count-1] - old->rr_data[0]) +
			old->rr_len[old->count-1]);
	}
	if(old->rrsig_count != 0) {
		memmove(d->rr_ttl+d->count, old->rr_ttl+old->count,
			old->rrsig_count*sizeof(time_t));
		memmove(d->rr_data[d->count], old->rr_data[old->count],
			(old->rr_data[old_total-1] - old->rr_data[old->count]) +
			old->rr_len[old_total-1]);
	}

	/* insert new value */
	if(!insert_sig) {
		d->rr_ttl[d->count-1] = rr_ttl;
		memmove(d->rr_data[d->count-1], rdata, rdatalen);
	} else {
		d->rr_ttl[total-1] = rr_ttl;
		memmove(d->rr_data[total-1], rdata, rdatalen);
	}

	rrset->data = d;
	free(old);
	return 1;
}

/** Create new rrset for node with packed rrset with one RR element */
static struct auth_rrset*
rrset_create(struct auth_data* node, uint16_t rr_type, uint32_t rr_ttl,
	uint8_t* rdata, size_t rdatalen)
{
	struct auth_rrset* rrset = (struct auth_rrset*)calloc(1,
		sizeof(*rrset));
	struct auth_rrset* p, *prev;
	struct packed_rrset_data* d;
	if(!rrset) {
		log_err("out of memory");
		return NULL;
	}
	rrset->type = rr_type;

	/* the rrset data structure, with one RR */
	d = (struct packed_rrset_data*)calloc(1,
		sizeof(struct packed_rrset_data) + sizeof(size_t) +
		sizeof(uint8_t*) + sizeof(time_t) + rdatalen);
	if(!d) {
		free(rrset);
		log_err("out of memory");
		return NULL;
	}
	rrset->data = d;
	d->ttl = rr_ttl;
	d->trust = rrset_trust_prim_noglue;
	d->rr_len = (size_t*)((uint8_t*)d + sizeof(struct packed_rrset_data));
	d->rr_data = (uint8_t**)&(d->rr_len[1]);
	d->rr_ttl = (time_t*)&(d->rr_data[1]);
	d->rr_data[0] = (uint8_t*)&(d->rr_ttl[1]);

	/* insert the RR */
	d->rr_len[0] = rdatalen;
	d->rr_ttl[0] = rr_ttl;
	memmove(d->rr_data[0], rdata, rdatalen);
	d->count++;

	/* insert rrset into linked list for domain */
	/* find sorted place to link the rrset into the list */
	prev = NULL;
	p = node->rrsets;
	while(p && p->type<=rr_type) {
		prev = p;
		p = p->next;
	}
	/* so, prev is smaller, and p is larger than rr_type */
	rrset->next = p;
	if(prev) prev->next = rrset;
	else node->rrsets = rrset;
	return rrset;
}

/** count number (and size) of rrsigs that cover a type */
static size_t
rrsig_num_that_cover(struct auth_rrset* rrsig, uint16_t rr_type, size_t* sigsz)
{
	struct packed_rrset_data* d = rrsig->data;
	size_t i, num = 0;
	*sigsz = 0;
	log_assert(d && rrsig->type == LDNS_RR_TYPE_RRSIG);
	for(i=0; i<d->count+d->rrsig_count; i++) {
		if(rrsig_rdata_get_type_covered(d->rr_data[i],
			d->rr_len[i]) == rr_type) {
			num++;
			(*sigsz) += d->rr_len[i];
		}
	}
	return num;
}

/** See if rrsig set has covered sigs for rrset and move them over */
static int
rrset_moveover_rrsigs(struct auth_data* node, uint16_t rr_type,
	struct auth_rrset* rrset, struct auth_rrset* rrsig)
{
	size_t sigs, sigsz, i, j, total;
	struct packed_rrset_data* sigold = rrsig->data;
	struct packed_rrset_data* old = rrset->data;
	struct packed_rrset_data* d, *sigd;

	log_assert(rrset->type == rr_type);
	log_assert(rrsig->type == LDNS_RR_TYPE_RRSIG);
	sigs = rrsig_num_that_cover(rrsig, rr_type, &sigsz);
	if(sigs == 0) {
		/* 0 rrsigs to move over, done */
		return 1;
	}
	log_info("moveover %d sigs size %d", (int)sigs, (int)sigsz);

	/* allocate rrset sigsz larger for extra sigs elements, and
	 * allocate rrsig sigsz smaller for less sigs elements. */
	d = (struct packed_rrset_data*)calloc(1, packed_rrset_sizeof(old)
		+ sigs*(sizeof(size_t) + sizeof(uint8_t*) + sizeof(time_t))
		+ sigsz);
	if(!d) {
		log_err("out of memory");
		return 0;
	}
	/* copy base values */
	total = old->count + old->rrsig_count;
	memcpy(d, old, sizeof(struct packed_rrset_data));
	d->rrsig_count += sigs;
	/* setup rr_len */
	d->rr_len = (size_t*)((uint8_t*)d +
		sizeof(struct packed_rrset_data));
	if(total != 0)
		memmove(d->rr_len, old->rr_len, total*sizeof(size_t));
	j = d->count+d->rrsig_count-sigs;
	for(i=0; i<sigold->count+sigold->rrsig_count; i++) {
		if(rrsig_rdata_get_type_covered(sigold->rr_data[i],
			sigold->rr_len[i]) == rr_type) {
			d->rr_len[j] = sigold->rr_len[i];
			j++;
		}
	}
	packed_rrset_ptr_fixup(d);

	/* copy old values into new array */
	if(total != 0) {
		memmove(d->rr_ttl, old->rr_ttl, total*sizeof(time_t));
		/* all the old rr pieces are allocated sequential, so we
		 * can copy them in one go */
		memmove(d->rr_data[0], old->rr_data[0],
			(old->rr_data[total-1] - old->rr_data[0]) +
			old->rr_len[total-1]);
	}

	/* move over the rrsigs to the larger rrset*/
	j = d->count+d->rrsig_count-sigs;
	for(i=0; i<sigold->count+sigold->rrsig_count; i++) {
		if(rrsig_rdata_get_type_covered(sigold->rr_data[i],
			sigold->rr_len[i]) == rr_type) {
			/* move this one over to location j */
			d->rr_ttl[j] = sigold->rr_ttl[i];
			memmove(d->rr_data[j], sigold->rr_data[i],
				sigold->rr_len[i]);
			if(d->rr_ttl[j] < d->ttl)
				d->ttl = d->rr_ttl[j];
			j++;
		}
	}

	/* put it in and deallocate the old rrset */
	rrset->data = d;
	free(old);

	/* now make rrsig set smaller */
	if(sigold->count+sigold->rrsig_count == sigs) {
		/* remove all sigs from rrsig, remove it entirely */
		domain_remove_rrset(node, LDNS_RR_TYPE_RRSIG);
		return 1;
	}
	log_assert(packed_rrset_sizeof(sigold) > sigs*(sizeof(size_t) +
		sizeof(uint8_t*) + sizeof(time_t)) + sigsz);
	sigd = (struct packed_rrset_data*)calloc(1, packed_rrset_sizeof(sigold)
		- sigs*(sizeof(size_t) + sizeof(uint8_t*) + sizeof(time_t))
		- sigsz);
	if(!sigd) {
		/* no need to free up d, it has already been placed in the
		 * node->rrset structure */
		log_err("out of memory");
		return 0;
	}
	/* copy base values */
	memcpy(sigd, sigold, sizeof(struct packed_rrset_data));
	sigd->rrsig_count -= sigs;
	/* setup rr_len */
	sigd->rr_len = (size_t*)((uint8_t*)sigd +
		sizeof(struct packed_rrset_data));
	j = 0;
	for(i=0; i<sigold->count+sigold->rrsig_count; i++) {
		if(rrsig_rdata_get_type_covered(sigold->rr_data[i],
			sigold->rr_len[i]) != rr_type) {
			sigd->rr_len[j] = sigold->rr_len[i];
			j++;
		}
	}
	packed_rrset_ptr_fixup(sigd);

	/* copy old values into new rrsig array */
	j = 0;
	for(i=0; i<sigold->count+sigold->rrsig_count; i++) {
		if(rrsig_rdata_get_type_covered(sigold->rr_data[i],
			sigold->rr_len[i]) != rr_type) {
			/* move this one over to location j */
			sigd->rr_ttl[j] = sigold->rr_ttl[i];
			memmove(sigd->rr_data[j], sigold->rr_data[i],
				sigold->rr_len[i]);
			if(j==0) sigd->ttl = sigd->rr_ttl[j];
			else {
				if(sigd->rr_ttl[j] < sigd->ttl)
					sigd->ttl = sigd->rr_ttl[j];
			}
			j++;
		}
	}

	/* put it in and deallocate the old rrset */
	rrsig->data = sigd;
	free(sigold);

	return 1;
}

/** Add rr to node, ignores duplicate RRs,
 * rdata points to buffer with rdatalen octets, starts with 2bytelength. */
static int
az_domain_add_rr(struct auth_data* node, uint16_t rr_type, uint32_t rr_ttl,
	uint8_t* rdata, size_t rdatalen)
{
	struct auth_rrset* rrset;
	/* packed rrsets have their rrsigs along with them, sort them out */
	if(rr_type == LDNS_RR_TYPE_RRSIG) {
		uint16_t ctype = rrsig_rdata_get_type_covered(rdata, rdatalen);
		if((rrset=az_domain_rrset(node, ctype))!= NULL) {
			/* a node of the correct type exists, add the RRSIG
			 * to the rrset of the covered data type */
			if(rdata_duplicate(rrset->data, rdata, rdatalen))
				return 1;
			if(!rrset_add_rr(rrset, rr_ttl, rdata, rdatalen, 1))
				return 0;
		} else if((rrset=az_domain_rrset(node, rr_type))!= NULL) {
			/* add RRSIG to rrset of type RRSIG */
			if(rdata_duplicate(rrset->data, rdata, rdatalen))
				return 1;
			if(!rrset_add_rr(rrset, rr_ttl, rdata, rdatalen, 0))
				return 0;
		} else {
			/* create rrset of type RRSIG */
			if(!rrset_create(node, rr_type, rr_ttl, rdata,
				rdatalen))
				return 0;
		}
	} else {
		/* normal RR type */
		if((rrset=az_domain_rrset(node, rr_type))!= NULL) {
			/* add data to existing node with data type */
			if(rdata_duplicate(rrset->data, rdata, rdatalen))
				return 1;
			if(!rrset_add_rr(rrset, rr_ttl, rdata, rdatalen, 0))
				return 0;
		} else {
			struct auth_rrset* rrsig;
			/* create new node with data type */
			if(!(rrset=rrset_create(node, rr_type, rr_ttl, rdata,
				rdatalen)))
				return 0;

			/* see if node of type RRSIG has signatures that
			 * cover the data type, and move them over */
			/* and then make the RRSIG type smaller */
			if((rrsig=az_domain_rrset(node, LDNS_RR_TYPE_RRSIG))
				!= NULL) {
				if(!rrset_moveover_rrsigs(node, rr_type,
					rrset, rrsig))
					return 0;
			}
		}
	}
	return 1;
}

/** insert RR into zone, ignore duplicates */
static int
az_insert_rr(struct auth_zone* z, uint8_t* rr, size_t rr_len,
	size_t dname_len)
{
	struct auth_data* node;
	uint8_t* dname = rr;
	uint16_t rr_type = sldns_wirerr_get_type(rr, rr_len, dname_len);
	uint16_t rr_class = sldns_wirerr_get_class(rr, rr_len, dname_len);
	uint32_t rr_ttl = sldns_wirerr_get_ttl(rr, rr_len, dname_len);
	size_t rdatalen = ((size_t)sldns_wirerr_get_rdatalen(rr, rr_len,
		dname_len))+2;
	/* rdata points to rdata prefixed with uint16 rdatalength */
	uint8_t* rdata = sldns_wirerr_get_rdatawl(rr, rr_len, dname_len);

	if(rr_class != z->dclass) {
		log_err("wrong class for RR");
		return 0;
	}
	if(!(node=az_domain_find_or_create(z, dname, dname_len))) {
		log_err("cannot create domain");
		return 0;
	}
	if(!az_domain_add_rr(node, rr_type, rr_ttl, rdata, rdatalen)) {
		log_err("cannot add RR to domain");
		return 0;
	}
	return 1;
}

/** 
 * Parse zonefile
 * @param z: zone to read in.
 * @param in: file to read from (just opened).
 * @param rr: buffer to use for RRs, 64k.
 *	passed so that recursive includes can use the same buffer and do
 *	not grow the stack too much.
 * @param rrbuflen: sizeof rr buffer.
 * @param state: parse state with $ORIGIN, $TTL and 'prev-dname' and so on,
 *	that is kept between includes.
 *	The lineno is set at 1 and then increased by the function.
 * returns false on failure, has printed an error message
 */
static int
az_parse_file(struct auth_zone* z, FILE* in, uint8_t* rr, size_t rrbuflen,
	struct sldns_file_parse_state* state)
{
	size_t rr_len, dname_len;
	int status;
	state->lineno = 1;

	while(!feof(in)) {
		rr_len = rrbuflen;
		dname_len = 0;
		status = sldns_fp2wire_rr_buf(in, rr, &rr_len, &dname_len,
			state);
		if(status == LDNS_WIREPARSE_ERR_INCLUDE && rr_len == 0) {
			/* we have $INCLUDE or $something */
			if(strncmp((char*)rr, "$INCLUDE ", 9) == 0 ||
			   strncmp((char*)rr, "$INCLUDE\t", 9) == 0) {
				FILE* inc;
				int lineno_orig = state->lineno;
				char* incfile = (char*)rr + 8;
				/* skip spaces */
				while(*incfile == ' ' || *incfile == '\t')
					incfile++;
				verbose(VERB_ALGO, "opening $INCLUDE %s",
					incfile);
				inc = fopen(incfile, "r");
				if(!inc) {
					log_err("%s:%d cannot open include "
						"file %s: %s", z->zonefile,
						lineno_orig, incfile,
						strerror(errno));
					return 0;
				}
				/* recurse read that file now */
				if(!az_parse_file(z, inc, rr, rrbuflen,
					state)) {
					log_err("%s:%d cannot parse include "
						"file %s", z->zonefile,
						lineno_orig, incfile);
					fclose(inc);
					return 0;
				}
				fclose(inc);
				verbose(VERB_ALGO, "done with $INCLUDE %s",
					incfile);
				state->lineno = lineno_orig;
			}
			continue;
		}
		if(status != 0) {
			log_err("parse error %s %d:%d: %s", z->zonefile,
				state->lineno, LDNS_WIREPARSE_OFFSET(status),
				sldns_get_errorstr_parse(status));
			return 0;
		}
		if(rr_len == 0) {
			/* EMPTY line, TTL or ORIGIN */
			continue;
		}
		/* insert wirerr in rrbuf */
		if(!az_insert_rr(z, rr, rr_len, dname_len)) {
			char buf[17];
			sldns_wire2str_type_buf(sldns_wirerr_get_type(rr,
				rr_len, dname_len), buf, sizeof(buf));
			log_err("%s:%d cannot insert RR of type %s",
				z->zonefile, state->lineno, buf);
			return 0;
		}
	}
	return 1;
}

int
auth_zone_read_zonefile(struct auth_zone* z)
{
	uint8_t rr[LDNS_RR_BUF_SIZE];
	struct sldns_file_parse_state state;
	FILE* in;
	if(!z || !z->zonefile || z->zonefile[0]==0)
		return 1; /* no file, or "", nothing to read */
	verbose(VERB_ALGO, "read zonefile %s", z->zonefile);
	in = fopen(z->zonefile, "r");
	if(!in) {
		char* n = sldns_wire2str_dname(z->name, z->namelen);
		log_err("cannot open zonefile %s for %s: %s",
			z->zonefile, n?n:"error", strerror(errno));
		free(n);
		return 0;
	}
	memset(&state, 0, sizeof(state));
	/* default TTL to 3600 */
	state.default_ttl = 3600;
	/* set $ORIGIN to the zone name */
	if(z->namelen <= sizeof(state.origin)) {
		memcpy(state.origin, z->name, z->namelen);
		state.origin_len = z->namelen;
	}
	/* parse the (toplevel) file */
	if(!az_parse_file(z, in, rr, sizeof(rr), &state)) {
		char* n = sldns_wire2str_dname(z->name, z->namelen);
		log_err("error parsing zonefile %s for %s",
			z->zonefile, n?n:"error");
		free(n);
		fclose(in);
		return 0;
	}
	fclose(in);
	return 1;
}

/** write buffer to file and check return codes */
static int
write_out(FILE* out, const char* str)
{
	size_t r, len = strlen(str);
	if(len == 0)
		return 1;
	r = fwrite(str, 1, len, out);
	if(r == 0) {
		log_err("write failed: %s", strerror(errno));
		return 0;
	} else if(r < len) {
		log_err("write failed: too short (disk full?)");
		return 0;
	}
	return 1;
}

/** write rrset to file */
static int
auth_zone_write_rrset(struct auth_zone* z, struct auth_data* node,
	struct auth_rrset* r, FILE* out)
{
	size_t i, count = r->data->count + r->data->rrsig_count;
	char buf[LDNS_RR_BUF_SIZE];
	for(i=0; i<count; i++) {
		struct ub_packed_rrset_key key;
		memset(&key, 0, sizeof(key));
		key.entry.key = &key;
		key.entry.data = r->data;
		key.rk.dname = node->name;
		key.rk.dname_len = node->namelen;
		key.rk.type = htons(r->type);
		key.rk.rrset_class = htons(z->dclass);
		if(!packed_rr_to_string(&key, i, 0, buf, sizeof(buf))) {
			verbose(VERB_ALGO, "failed to rr2str rr %d", (int)i);
			continue;
		}
		if(!write_out(out, buf))
			return 0;
	}
	return 1;
}

/** write domain to file */
static int
auth_zone_write_domain(struct auth_zone* z, struct auth_data* n, FILE* out)
{
	struct auth_rrset* r;
	/* if this is zone apex, write SOA first */
	if(z->namelen == n->namelen) {
		struct auth_rrset* soa = az_domain_rrset(n, LDNS_RR_TYPE_SOA);
		if(soa) {
			if(!auth_zone_write_rrset(z, n, soa, out))
				return 0;
		}
	}
	/* write all the RRsets for this domain */
	for(r = n->rrsets; r; r = r->next) {
		if(z->namelen == n->namelen &&
			r->type == LDNS_RR_TYPE_SOA)
			continue; /* skip SOA here */
		if(!auth_zone_write_rrset(z, n, r, out))
			return 0;
	}
	return 1;
}

int auth_zone_write_file(struct auth_zone* z, const char* fname)
{
	FILE* out;
	struct auth_data* n;
	out = fopen(fname, "w");
	if(!out) {
		log_err("could not open %s: %s", fname, strerror(errno));
		return 0;
	}
	RBTREE_FOR(n, struct auth_data*, &z->data) {
		if(!auth_zone_write_domain(z, n, out)) {
			log_err("could not write domain to %s", fname);
			fclose(out);
			return 0;
		}
	}
	fclose(out);
	return 1;
}

/** read all auth zones from file (if they have) */
static int
auth_zones_read_zones(struct auth_zones* az)
{
	struct auth_zone* z;
	lock_rw_wrlock(&az->lock);
	RBTREE_FOR(z, struct auth_zone*, &az->ztree) {
		lock_rw_wrlock(&z->lock);
		if(!auth_zone_read_zonefile(z)) {
			lock_rw_unlock(&z->lock);
			lock_rw_unlock(&az->lock);
			return 0;
		}
		lock_rw_unlock(&z->lock);
	}
	lock_rw_unlock(&az->lock);
	return 1;
}

/** set str2list with (zonename, zonefile) config items and create zones */
static int
auth_zones_cfg_zonefile(struct auth_zones* az, struct config_str2list* zlist)
{
	struct auth_zone* z;
	while(zlist) {
		lock_rw_wrlock(&az->lock);
		if(!(z=auth_zones_find_or_add_zone(az, zlist->str))) {
			lock_rw_unlock(&az->lock);
			return 0;
		}
		lock_rw_unlock(&az->lock);
		if(!auth_zone_set_zonefile(z, zlist->str2)) {
			lock_rw_unlock(&z->lock);
			return 0;
		}
		lock_rw_unlock(&z->lock);
		zlist = zlist->next;
	}
	return 1;
}

/** set str2list with (zonename, fallback) config items and create zones */
static int
auth_zones_cfg_fallback(struct auth_zones* az, struct config_str2list* zlist)
{
	struct auth_zone* z;
	while(zlist) {
		lock_rw_wrlock(&az->lock);
		if(!(z=auth_zones_find_or_add_zone(az, zlist->str))) {
			lock_rw_unlock(&az->lock);
			return 0;
		}
		lock_rw_unlock(&az->lock);
		if(!auth_zone_set_fallback(z, zlist->str2)) {
			lock_rw_unlock(&z->lock);
			return 0;
		}
		lock_rw_unlock(&z->lock);
		zlist = zlist->next;
	}
	return 1;
}

int auth_zones_apply_config(struct auth_zones* az, struct config_file* cfg)
{
	(void)cfg;
	/* TODO cfg str2lists */
	/* create config items for
	 * auth-zone:	name: "example.com"
	 * 		zonefile: "zones/example.com"
	 * 		fallback: yes
	 */
	if(!auth_zones_cfg_zonefile(az, NULL /*cfg->auth_zones*/))
		return 0;
	if(!auth_zones_cfg_fallback(az, NULL /*cfg->auth_zones*/))
		return 0;
	if(!auth_zones_read_zones(az))
		return 0;
	return 1;
}

/** helper traverse to delete zones */
static void
auth_zone_del(rbnode_type* n, void* ATTR_UNUSED(arg))
{
	struct auth_zone* z = (struct auth_zone*)n->key;
	auth_zone_delete(z);
}

void auth_zones_delete(struct auth_zones* az)
{
	if(!az) return;
	lock_rw_destroy(&az->lock);
	traverse_postorder(&az->ztree, auth_zone_del, NULL);
	free(az);
}

/** true if domain has only nsec3 */
static int
domain_has_only_nsec3(struct auth_data* n)
{
	struct auth_rrset* rrset = n->rrsets;
	int nsec3_seen = 0;
	while(rrset) {
		if(rrset->type == LDNS_RR_TYPE_NSEC3) {
			nsec3_seen = 1;
		} else if(rrset->type != LDNS_RR_TYPE_RRSIG) {
			return 0;
		}
		rrset = rrset->next;
	}
	return nsec3_seen;
}

/** see if the domain has a wildcard child '*.domain' */
static struct auth_data*
az_find_wildcard_domain(struct auth_zone* z, uint8_t* nm, size_t nmlen)
{
	uint8_t wc[LDNS_MAX_DOMAINLEN];
	if(nmlen+2 > sizeof(wc))
		return NULL; /* result would be too long */
	wc[0] = 1; /* length of wildcard label */
	wc[1] = (uint8_t)'*'; /* wildcard label */
	memmove(wc+2, nm, nmlen);
	return az_find_name(z, wc, nmlen+2);
}

/** find wildcard between qname and cename */
static struct auth_data*
az_find_wildcard(struct auth_zone* z, struct query_info* qinfo,
	struct auth_data* ce)
{
	uint8_t* nm = qinfo->qname;
	size_t nmlen = qinfo->qname_len;
	struct auth_data* node;
	if(!dname_subdomain_c(nm, z->name))
		return NULL; /* out of zone */
	while((node=az_find_wildcard_domain(z, nm, nmlen))==NULL) {
		/* see if we can go up to find the wildcard */
		if(nmlen == z->namelen)
			return NULL; /* top of zone reached */
		if(ce && nmlen == ce->namelen)
			return NULL; /* ce reached */
		if(dname_is_root(nm))
			return NULL; /* cannot go up */
		dname_remove_label(&nm, &nmlen);
	}
	return node;
}

/** domain is not exact, find first candidate ce (name that matches
 * a part of qname) in tree */
static struct auth_data*
az_find_candidate_ce(struct auth_zone* z, struct query_info* qinfo,
	struct auth_data* n)
{
	uint8_t* nm;
	size_t nmlen;
	if(n) {
		nm = dname_get_shared_topdomain(qinfo->qname, n->name);
	} else {
		nm = qinfo->qname;
	}
	dname_count_size_labels(nm, &nmlen);
	n = az_find_name(z, nm, nmlen);
	/* delete labels and go up on name */
	while(!n) {
		if(dname_is_root(nm))
			return NULL; /* cannot go up */
		dname_remove_label(&nm, &nmlen);
		n = az_find_name(z, nm, nmlen);
	}
	return n;
}

/** go up the auth tree to next existing name. */
static struct auth_data*
az_domain_go_up(struct auth_zone* z, struct auth_data* n)
{
	uint8_t* nm = n->name;
	size_t nmlen = n->namelen;
	while(!dname_is_root(nm)) {
		dname_remove_label(&nm, &nmlen);
		if((n=az_find_name(z, nm, nmlen)) != NULL)
			return n;
	}
	return NULL;
}

/** Find the closest encloser, an name that exists and is above the
 * qname.
 * return true if the node (param node) is existing, nonobscured and
 * 	can be used to generate answers from.  It is then also node_exact.
 * returns false if the node is not good enough (or it wasn't node_exact)
 *      in this case the ce can be filled.
 *      if ce is NULL, no ce exists, and likely the zone is completely empty,
 *      not even with a zone apex.
 *	if ce is nonNULL it is the closest enclosing upper name (that exists
 *	itself for answer purposes).  That name may have DNAME, NS or wildcard
 *	rrset is the closest DNAME or NS rrset that was found.
 */
static int
az_find_ce(struct auth_zone* z, struct query_info* qinfo,
	struct auth_data* node, int node_exact, struct auth_data** ce,
	struct auth_rrset** rrset)
{
	struct auth_data* n = node;
	*ce = NULL;
	*rrset = NULL;
	if(!node_exact) {
		/* if not exact, lookup closest exact match */
		n = az_find_candidate_ce(z, qinfo, n);
	} else {
		/* if exact, the node itself is the first candidate ce */
		*ce = n;
	}

	/* no direct answer from nsec3-only domains */
	if(n && domain_has_only_nsec3(n)) {
		node_exact = 0;
		*ce = NULL;
	}

	/* with exact matches, walk up the labels until we find the
	 * delegation, or DNAME or zone end */
	while(n) {
		/* see if the current candidate has issues */
		/* not zone apex and has type NS */
		if(n->namelen != z->namelen &&
			(*rrset=az_domain_rrset(n, LDNS_RR_TYPE_NS)) &&
			/* delegate here, but DS at exact the dp has notype */
			(qinfo->qtype != LDNS_RR_TYPE_DS || 
			n->namelen != qinfo->qname_len)) {
			/* referral */
			/* this is ce and the lowernode is nonexisting */
			*ce = n;
			return 0;
		}
		/* not equal to qname and has type DNAME */
		if(n->namelen != qinfo->qname_len &&
			(*rrset=az_domain_rrset(n, LDNS_RR_TYPE_DNAME))) {
			/* this is ce and the lowernode is nonexisting */
			*ce = n;
			return 0;
		}

		if(*ce == NULL && !domain_has_only_nsec3(n)) {
			/* if not found yet, this exact name must be
			 * our lowest match (but not nsec3onlydomain) */
			*ce = n;
		}

		/* walk up the tree by removing labels from name and lookup */
		n = az_domain_go_up(z, n);
	}
	/* found no problems, if it was an exact node, it is fine to use */
	return node_exact;
}

/** add additional A/AAAA from domain names in rrset rdata (+offset)
 * offset is number of bytes in rdata where the dname is located. */
static int
az_add_additionals_from(struct auth_zone* z, struct regional* region,
	struct dns_msg* msg, struct auth_rrset* rrset, size_t offset)
{
	struct packed_rrset_data* d = rrset->data;
	size_t i;
	if(!d) return 0;
	for(i=0; i<d->count; i++) {
		size_t dlen;
		struct auth_data* domain;
		struct auth_rrset* ref;
		if(d->rr_len[i] < 2+offset)
			continue; /* too short */
		if(!(dlen = dname_valid(d->rr_data[i]+2+offset,
			d->rr_len[i]-2-offset)))
			continue; /* malformed */
		domain = az_find_name(z, d->rr_data[i]+2+offset, dlen);
		if(!domain)
			continue;
		if((ref=az_domain_rrset(domain, LDNS_RR_TYPE_A)) != NULL) {
			if(!msg_add_rrset_ar(z, region, msg, domain, ref))
				return 0;
		}
		if((ref=az_domain_rrset(domain, LDNS_RR_TYPE_AAAA)) != NULL) {
			if(!msg_add_rrset_ar(z, region, msg, domain, ref))
				return 0;
		}
	}
	return 1;
}

/** add negative SOA record (with negative TTL) */
static int
az_add_negative_soa(struct auth_zone* z, struct regional* region,
	struct dns_msg* msg)
{
	uint32_t minimum;
	struct packed_rrset_data* d;
	struct auth_rrset* soa;
	struct auth_data* apex = az_find_name(z, z->name, z->namelen);
	if(!apex) return 0;
	soa = az_domain_rrset(apex, LDNS_RR_TYPE_SOA);
	if(!soa) return 0;
	/* must be first to put in message; we want to fix the TTL with
	 * one RRset here, otherwise we'd need to loop over the RRs to get
	 * the resulting lower TTL */
	log_assert(msg->rep->rrset_count == 0);
	if(!msg_add_rrset_ns(z, region, msg, apex, soa)) return 0;
	/* fixup TTL */
	d = (struct packed_rrset_data*)msg->rep->rrsets[msg->rep->rrset_count-1]->entry.data;
	/* last 4 bytes are minimum ttl in network format */
	if(d->count == 0) return 0;
	if(d->rr_len[0] < 2+4) return 0;
	minimum = sldns_read_uint32(d->rr_data[0]+(d->rr_len[0]-4));
	d->ttl = (time_t)minimum;
	d->rr_ttl[0] = (time_t)minimum;
	msg->rep->ttl = get_rrset_ttl(msg->rep->rrsets[0]);
	msg->rep->prefetch_ttl = PREFETCH_TTL_CALC(msg->rep->ttl);
	return 1;
}

/** See if the query goes to empty nonterminal (that has no auth_data,
 * but there are nodes underneath.  We already checked that there are
 * not NS, or DNAME above, so that we only need to check if some node
 * exists below (with nonempty rr list), return true if emptynonterminal */
static int
az_empty_nonterminal(struct auth_zone* z, struct query_info* qinfo,
	struct auth_data* node)
{
	struct auth_data* next;
	if(!node) {
		/* no smaller was found, use first (smallest) node as the
		 * next one */
		next = (struct auth_data*)rbtree_first(&z->data);
	} else {
		next = (struct auth_data*)rbtree_next(&node->node);
	}
	while(next && (rbnode_type*)next != RBTREE_NULL && next->rrsets == NULL) {
		/* the next name has empty rrsets, is an empty nonterminal
		 * itself, see if there exists something below it */
		next = (struct auth_data*)rbtree_next(&node->node);
	}
	if((rbnode_type*)next == RBTREE_NULL || !next) {
		/* there is no next node, so something below it cannot
		 * exist */
		return 0;
	}
	/* a next node exists, if there was something below the query,
	 * this node has to be it.  See if it is below the query name */
	if(dname_strict_subdomain_c(next->name, qinfo->qname))
		return 1;
	return 0;
}

/** create synth cname target name in buffer, or fail if too long */
static size_t
synth_cname_buf(uint8_t* qname, size_t qname_len, size_t dname_len,
	uint8_t* dtarg, size_t dtarglen, uint8_t* buf, size_t buflen)
{
	size_t newlen = qname_len + dtarglen - dname_len;
	if(newlen > buflen) {
		/* YXDOMAIN error */
		return 0;
	}
	/* new name is concatenation of qname front (without DNAME owner)
	 * and DNAME target name */
	memcpy(buf, qname, qname_len-dname_len);
	memmove(buf+(qname_len-dname_len), dtarg, dtarglen);
	return newlen;
}

/** create synthetic CNAME rrset for in a DNAME answer in region,
 * false on alloc failure, cname==NULL when name too long. */
static int
create_synth_cname(uint8_t* qname, size_t qname_len, struct regional* region,
	struct auth_data* node, struct auth_rrset* dname, uint16_t dclass,
	struct ub_packed_rrset_key** cname)
{
	uint8_t buf[LDNS_MAX_DOMAINLEN];
	uint8_t* dtarg;
	size_t dtarglen, newlen;
	struct packed_rrset_data* d;

	/* get DNAME target name */
	if(dname->data->count < 1) return 0;
	if(dname->data->rr_len[0] < 3) return 0; /* at least rdatalen +1 */
	dtarg = dname->data->rr_data[0]+2;
	dtarglen = dname->data->rr_len[0]-2;
	if(sldns_read_uint16(dname->data->rr_data[0]) != dtarglen)
		return 0; /* rdatalen in DNAME rdata is malformed */
	if(dname_valid(dtarg, dtarglen) != dtarglen)
		return 0; /* DNAME RR has malformed rdata */

	/* synthesize a CNAME */
	newlen = synth_cname_buf(qname, qname_len, node->namelen,
		dtarg, dtarglen, buf, sizeof(buf));
	if(newlen == 0) {
		/* YXDOMAIN error */
		*cname = NULL;
		return 1;
	}
	*cname = (struct ub_packed_rrset_key*)regional_alloc(region,
		sizeof(struct ub_packed_rrset_key));
	if(!*cname)
		return 0; /* out of memory */
	memset(&(*cname)->entry, 0, sizeof((*cname)->entry));
	(*cname)->entry.key = (*cname);
	(*cname)->rk.type = htons(LDNS_RR_TYPE_CNAME);
	(*cname)->rk.rrset_class = htons(dclass);
	(*cname)->rk.flags = 0;
	(*cname)->rk.dname = regional_alloc_init(region, qname, qname_len);
	if(!(*cname)->rk.dname)
		return 0; /* out of memory */
	(*cname)->rk.dname_len = qname_len;
	(*cname)->entry.hash = rrset_key_hash(&(*cname)->rk);
	d = (struct packed_rrset_data*)regional_alloc_zero(region,
		sizeof(struct packed_rrset_data) + sizeof(size_t) +
		sizeof(uint8_t*) + sizeof(time_t) + sizeof(uint16_t)
		+ newlen);
	if(!d)
		return 0; /* out of memory */
	(*cname)->entry.data = d;
	d->ttl = 0; /* 0 for synthesized CNAME TTL */
	d->count = 1;
	d->rrsig_count = 0;
	d->trust = rrset_trust_ans_noAA;
	d->rr_len = (size_t*)((uint8_t*)d +
		sizeof(struct packed_rrset_data));
	d->rr_len[0] = newlen + sizeof(uint16_t);
	packed_rrset_ptr_fixup(d);
	d->rr_ttl[0] = d->ttl;
	sldns_write_uint16(d->rr_data[0], newlen);
	memmove(d->rr_data[0] + sizeof(uint16_t), buf, newlen);
	return 1;
}

/** add a synthesized CNAME to the answer section */
static int
add_synth_cname(struct auth_zone* z, uint8_t* qname, size_t qname_len,
	struct regional* region, struct dns_msg* msg, struct auth_data* dname,
	struct auth_rrset* rrset)
{
	struct ub_packed_rrset_key* cname;
	/* synthesize a CNAME */
	if(!create_synth_cname(qname, qname_len, region, dname, rrset,
		z->dclass, &cname)) {
		/* out of memory */
		return 0;
	}
	if(!cname) {
		/* cname cannot be create because of YXDOMAIN */
		msg->rep->flags |= LDNS_RCODE_YXDOMAIN;
		return 1;
	}
	/* add cname to message */
	if(!msg_grow_array(region, msg))
		return 0;
	msg->rep->rrsets[msg->rep->rrset_count] = cname;
	msg->rep->rrset_count++;
	msg->rep->an_numrrsets++;
	msg_ttl(msg);
	return 1;
}

/** Change a dname to a different one, for wildcard namechange */
static void
az_change_dnames(struct dns_msg* msg, uint8_t* oldname, uint8_t* newname,
	size_t newlen, int an_only)
{
	size_t i;
	size_t start = 0, end = msg->rep->rrset_count;
	if(!an_only) start = msg->rep->an_numrrsets;
	if(an_only) end = msg->rep->an_numrrsets;
	for(i=start; i<end; i++) {
		/* allocated in region so we can change the ptrs */
		if(query_dname_compare(msg->rep->rrsets[i]->rk.dname, oldname)
			== 0) {
			msg->rep->rrsets[i]->rk.dname = newname;
			msg->rep->rrsets[i]->rk.dname_len = newlen;
		}
	}
}

/** find NSEC record covering the query */
static struct auth_rrset*
az_find_nsec_cover(struct auth_zone* z, struct auth_data** node)
{
	uint8_t* nm = (*node)->name;
	size_t nmlen = (*node)->namelen;
	struct auth_rrset* rrset;
	/* find the NSEC for the smallest-or-equal node */
	/* if node == NULL, we did not find a smaller name.  But the zone
	 * name is the smallest name and should have an NSEC. So there is
	 * no NSEC to return (for a properly signed zone) */
	/* for empty nonterminals, the auth-data node should not exist,
	 * and thus we don't need to go rbtree_previous here to find
	 * a domain with an NSEC record */
	/* but there could be glue, and if this is node, then it has no NSEC.
	 * Go up to find nonglue (previous) NSEC-holding nodes */
	while((rrset=az_domain_rrset(*node, LDNS_RR_TYPE_NSEC)) == NULL) {
		if(dname_is_root(nm)) return NULL;
		if(nmlen == z->namelen) return NULL;
		dname_remove_label(&nm, &nmlen);
		/* adjust *node for the nsec rrset to find in */
		*node = az_find_name(z, nm, nmlen);
	}
	return rrset;
}

/** Find NSEC and add for wildcard denial */
static int
az_nsec_wildcard_denial(struct auth_zone* z, struct regional* region,
	struct dns_msg* msg, uint8_t* cenm, size_t cenmlen)
{
	struct query_info qinfo;
	int node_exact;
	struct auth_data* node;
	struct auth_rrset* nsec;
	uint8_t wc[LDNS_MAX_DOMAINLEN];
	if(cenmlen+2 > sizeof(wc))
		return 0; /* result would be too long */
	wc[0] = 1; /* length of wildcard label */
	wc[1] = (uint8_t)'*'; /* wildcard label */
	memmove(wc+2, cenm, cenmlen);

	/* we have '*.ce' in wc wildcard name buffer */
	/* get nsec cover for that */
	qinfo.qname = wc;
	qinfo.qname_len = cenmlen+2;
	qinfo.qtype = 0;
	qinfo.qclass = 0;
	az_find_domain(z, &qinfo, &node_exact, &node);
	if((nsec=az_find_nsec_cover(z, &node)) != NULL) {
		if(!msg_add_rrset_ns(z, region, msg, node, nsec)) return 0;
	}
	return 1;
}

/** Find the NSEC3PARAM rrset (if any) and if true you have the parameters */
static int
az_nsec3_param(struct auth_zone* z, int* algo, size_t* iter, uint8_t** salt,
	size_t* saltlen)
{
	struct auth_data* apex;
	struct auth_rrset* param;
	size_t i;
	apex = az_find_name(z, z->name, z->namelen);
	if(!apex) return 0;
	param = az_domain_rrset(apex, LDNS_RR_TYPE_NSEC3PARAM);
	if(!param || param->data->count==0)
		return 0; /* no RRset or no RRs in rrset */
	/* find out which NSEC3PARAM RR has supported parameters */
	/* skip unknown flags (dynamic signer is recalculating nsec3 chain) */
	for(i=0; i<param->data->count; i++) {
		uint8_t* rdata = param->data->rr_data[i]+2;
		size_t rdatalen = param->data->rr_len[i];
		if(rdatalen < 2+5)
			continue; /* too short */
		if(!nsec3_hash_algo_size_supported((int)(rdata[0])))
			continue; /* unsupported algo */
		if(rdatalen < (size_t)(2+5+(size_t)rdata[4]))
			continue; /* salt missing */
		if((rdata[1]&NSEC3_UNKNOWN_FLAGS)!=0)
			continue; /* unknown flags */
		*algo = (int)(rdata[0]);
		*iter = sldns_read_uint16(rdata+2);
		*saltlen = rdata[4];
		if(*saltlen == 0)
			*salt = NULL;
		else	*salt = rdata+5;
		return 1;
	}
	/* no supported params */
	return 0;
}

/** Hash a name with nsec3param into buffer, it has zone name appended.
 * return length of hash */
static size_t
az_nsec3_hash(uint8_t* buf, size_t buflen, uint8_t* nm, size_t nmlen,
	int algo, size_t iter, uint8_t* salt, size_t saltlen)
{
	size_t hlen = nsec3_hash_algo_size_supported(algo);
	/* buffer has domain name, nsec3hash, and 256 is for max saltlen
	 * (salt has 0-255 length) */
	unsigned char p[LDNS_MAX_DOMAINLEN+1+N3HASHBUFLEN+256];
	size_t i;
	if(nmlen+saltlen > sizeof(p) || hlen+saltlen > sizeof(p))
		return 0;
	if(hlen > buflen)
		return 0; /* somehow too large for destination buffer */
	/* hashfunc(name, salt) */
	memmove(p, nm, nmlen);
	query_dname_tolower(p);
	memmove(p+nmlen, salt, saltlen);
	(void)secalgo_nsec3_hash(algo, p, nmlen+saltlen, (unsigned char*)buf);
	for(i=0; i<iter; i++) {
		/* hashfunc(hash, salt) */
		memmove(p, buf, hlen);
		memmove(p+hlen, salt, saltlen);
		(void)secalgo_nsec3_hash(algo, p, hlen+saltlen,
			(unsigned char*)buf);
	}
	return hlen;
}

/** Hash name and return b32encoded hashname for lookup, zone name appended */
static int
az_nsec3_hashname(struct auth_zone* z, uint8_t* hashname, size_t* hashnmlen,
	uint8_t* nm, size_t nmlen, int algo, size_t iter, uint8_t* salt,
	size_t saltlen)
{
	uint8_t hash[N3HASHBUFLEN];
	size_t hlen;
	int ret;
	hlen = az_nsec3_hash(hash, sizeof(hash), nm, nmlen, algo, iter,
		salt, saltlen);
	if(!hlen) return 0;
	/* b32 encode */
	if(*hashnmlen < hlen*2+1+z->namelen) /* approx b32 as hexb16 */
		return 0;
	ret = sldns_b32_ntop_extended_hex(hash, hlen, (char*)(hashname+1),
		(*hashnmlen)-1);
	if(ret<1)
		return 0;
	hashname[0] = (uint8_t)ret;
	ret++;
	if((*hashnmlen) - ret < z->namelen)
		return 0;
	memmove(hashname+ret, z->name, z->namelen);
	*hashnmlen = z->namelen+(size_t)ret;
	return 1;
}

/** Find the datanode that covers the nsec3hash-name */
struct auth_data*
az_nsec3_findnode(struct auth_zone* z, uint8_t* hashnm, size_t hashnmlen)
{
	struct query_info qinfo;
	struct auth_data* node;
	int node_exact;
	qinfo.qclass = 0;
	qinfo.qtype = 0;
	qinfo.qname = hashnm;
	qinfo.qname_len = hashnmlen;
	/* because canonical ordering and b32 nsec3 ordering are the same.
	 * this is a good lookup to find the nsec3 name. */
	az_find_domain(z, &qinfo, &node_exact, &node);
	/* but we may have to skip non-nsec3 nodes */
	/* this may be a lot, the way to speed that up is to have a
	 * separate nsec3 tree with nsec3 nodes */
	while(node && (rbnode_type*)node != RBTREE_NULL &&
		!az_domain_rrset(node, LDNS_RR_TYPE_NSEC3)) {
		node = (struct auth_data*)rbtree_previous(&node->node);
	}
	if((rbnode_type*)node == RBTREE_NULL)
		node = NULL;
	return node;
}

/** Find cover for hashed(nm, nmlen) (or NULL) */
static struct auth_data*
az_nsec3_find_cover(struct auth_zone* z, uint8_t* nm, size_t nmlen,
	int algo, size_t iter, uint8_t* salt, size_t saltlen)
{
	struct auth_data* node;
	uint8_t hname[LDNS_MAX_DOMAINLEN];
	size_t hlen = sizeof(hname);
	if(!az_nsec3_hashname(z, hname, &hlen, nm, nmlen, algo, iter,
		salt, saltlen))
		return NULL;
	node = az_nsec3_findnode(z, hname, hlen);
	if(node)
		return node;
	/* we did not find any, perhaps because the NSEC3 hash is before
	 * the first hash, we have to find the 'last hash' in the zone */
	node = (struct auth_data*)rbtree_last(&z->data);
	while(node && (rbnode_type*)node != RBTREE_NULL &&
		!az_domain_rrset(node, LDNS_RR_TYPE_NSEC3)) {
		node = (struct auth_data*)rbtree_previous(&node->node);
	}
        if((rbnode_type*)node == RBTREE_NULL)
		node = NULL;
	return node;
}

/** Find exact match for hashed(nm, nmlen) NSEC3 record or NULL */
static struct auth_data*
az_nsec3_find_exact(struct auth_zone* z, uint8_t* nm, size_t nmlen,
	int algo, size_t iter, uint8_t* salt, size_t saltlen)
{
	struct auth_data* node;
	uint8_t hname[LDNS_MAX_DOMAINLEN];
	size_t hlen = sizeof(hname);
	if(!az_nsec3_hashname(z, hname, &hlen, nm, nmlen, algo, iter,
		salt, saltlen))
		return NULL;
	node = az_find_name(z, hname, hlen);
	if(az_domain_rrset(node, LDNS_RR_TYPE_NSEC3))
		return node;
	return NULL;
}

/** Return nextcloser name (as a ref into the qname).  This is one label
 * more than the cenm (cename must be a suffix of qname) */
static void
az_nsec3_get_nextcloser(uint8_t* cenm, uint8_t* qname, size_t qname_len,
	uint8_t** nx, size_t* nxlen)
{
	int celabs = dname_count_labels(cenm);
	int qlabs = dname_count_labels(qname);
	int strip = qlabs - celabs -1;
	log_assert(dname_strict_subdomain(qname, qlabs, cenm, celabs));
	*nx = qname;
	*nxlen = qname_len;
	if(strip>0)
		dname_remove_labels(nx, nxlen, strip);
}

/** Find the closest encloser that has exact NSEC3.
 * updated cenm to the new name. If it went up no-exact-ce is true. */
static struct auth_data*
az_nsec3_find_ce(struct auth_zone* z, uint8_t** cenm, size_t* cenmlen,
	int* no_exact_ce, int algo, size_t iter, uint8_t* salt, size_t saltlen)
{
	struct auth_data* node;
	while((node = az_nsec3_find_exact(z, *cenm, *cenmlen,
		algo, iter, salt, saltlen)) == NULL) {
		if(*cenmlen == z->namelen) {
			/* next step up would take us out of the zone. fail */
			return NULL;
		}
		*no_exact_ce = 1;
		dname_remove_label(cenm, cenmlen);
	}
	return node;
}

/* Insert NSEC3 record in authority section, if NULL does nothing */
static int
az_nsec3_insert(struct auth_zone* z, struct regional* region,
	struct dns_msg* msg, struct auth_data* node)
{
	struct auth_rrset* nsec3;
	if(!node) return 1; /* no node, skip this */
	nsec3 = az_domain_rrset(node, LDNS_RR_TYPE_NSEC3);
	if(!nsec3) return 1; /* if no nsec3 RR, skip it */
	if(!msg_add_rrset_ns(z, region, msg, node, nsec3)) return 0;
	return 1;
}

/** add NSEC3 records to the zone for the nsec3 proof.
 * Specify with the flags with parts of the proof are required.
 * the ce is the exact matching name (for notype) but also delegation points.
 * qname is the one where the nextcloser name can be derived from.
 * If NSEC3 is not properly there (in the zone) nothing is added.
 * always enabled: include nsec3 proving about the Closest Encloser.
 * 	that is an exact match that should exist for it.
 * 	If that does not exist, a higher exact match + nxproof is enabled
 * 	(for some sort of opt-out empty nonterminal cases).
 * nxproof: include denial of the qname.
 * wcproof: include denial of wildcard (wildcard.ce).
 */
static int
az_add_nsec3_proof(struct auth_zone* z, struct regional* region,
	struct dns_msg* msg, uint8_t* cenm, size_t cenmlen, uint8_t* qname,
	size_t qname_len, int nxproof, int wcproof)
{
	int algo;
	size_t iter, saltlen;
	uint8_t* salt;
	int no_exact_ce = 0;
	struct auth_data* node;

	/* find parameters of nsec3 proof */
	if(!az_nsec3_param(z, &algo, &iter, &salt, &saltlen))
		return 1; /* no nsec3 */
	/* find ce that has an NSEC3 */
	node = az_nsec3_find_ce(z, &cenm, &cenmlen, &no_exact_ce,
		algo, iter, salt, saltlen);
	if(no_exact_ce) nxproof = 1;
	if(!az_nsec3_insert(z, region, msg, node))
		return 0;

	if(nxproof) {
		uint8_t* nx;
		size_t nxlen;
		/* create nextcloser domain name */
		az_nsec3_get_nextcloser(cenm, qname, qname_len, &nx, &nxlen);
		/* find nsec3 that matches or covers it */
		node = az_nsec3_find_cover(z, nx, nxlen, algo, iter, salt,
			saltlen);
		if(!az_nsec3_insert(z, region, msg, node))
			return 0;
	}
	if(wcproof) {
		/* create wildcard name *.ce */
		uint8_t wc[LDNS_MAX_DOMAINLEN];
		size_t wclen;
		if(cenmlen+2 > sizeof(wc))
			return 0; /* result would be too long */
		wc[0] = 1; /* length of wildcard label */
		wc[1] = (uint8_t)'*'; /* wildcard label */
		memmove(wc+2, cenm, cenmlen);
		wclen = cenmlen+2;
		/* find nsec3 that matches or covers it */
		node = az_nsec3_find_cover(z, wc, wclen, algo, iter, salt,
			saltlen);
		if(!az_nsec3_insert(z, region, msg, node))
			return 0;
	}
	return 1;
}

/** generate answer for positive answer */
static int
az_generate_positive_answer(struct auth_zone* z, struct regional* region,
	struct dns_msg* msg, struct auth_data* node, struct auth_rrset* rrset)
{
	if(!msg_add_rrset_an(z, region, msg, node, rrset)) return 0;
	/* see if we want additional rrs */
	if(rrset->type == LDNS_RR_TYPE_MX) {
		if(!az_add_additionals_from(z, region, msg, rrset, 2))
			return 0;
	} else if(rrset->type == LDNS_RR_TYPE_SRV) {
		if(!az_add_additionals_from(z, region, msg, rrset, 6))
			return 0;
	} else if(rrset->type == LDNS_RR_TYPE_NS) {
		if(!az_add_additionals_from(z, region, msg, rrset, 0))
			return 0;
	}
	return 1;
}

/** generate answer for type ANY answer */
static int
az_generate_any_answer(struct auth_zone* z, struct regional* region,
	struct dns_msg* msg, struct auth_data* node)
{
	struct auth_rrset* rrset;
	int added = 0;
	/* add a couple (at least one) RRs */
	if((rrset=az_domain_rrset(node, LDNS_RR_TYPE_SOA)) != NULL) {
		if(!msg_add_rrset_an(z, region, msg, node, rrset)) return 0;
		added++;
	}
	if((rrset=az_domain_rrset(node, LDNS_RR_TYPE_MX)) != NULL) {
		if(!msg_add_rrset_an(z, region, msg, node, rrset)) return 0;
		added++;
	}
	if((rrset=az_domain_rrset(node, LDNS_RR_TYPE_A)) != NULL) {
		if(!msg_add_rrset_an(z, region, msg, node, rrset)) return 0;
		added++;
	}
	if((rrset=az_domain_rrset(node, LDNS_RR_TYPE_AAAA)) != NULL) {
		if(!msg_add_rrset_an(z, region, msg, node, rrset)) return 0;
		added++;
	}
	if(added == 0 && node->rrsets) {
		if(!msg_add_rrset_an(z, region, msg, node,
			node->rrsets)) return 0;
	}
	return 1;
}

/** follow cname chain and add more data to the answer section */
static int
follow_cname_chain(struct auth_zone* z, uint16_t qtype,
	struct regional* region, struct dns_msg* msg,
	struct packed_rrset_data* d)
{
	int maxchain = 0;
	/* see if we can add the target of the CNAME into the answer */
	while(maxchain++ < MAX_CNAME_CHAIN) {
		struct auth_data* node;
		struct auth_rrset* rrset;
		size_t clen;
		/* d has cname rdata */
		if(d->count == 0) break; /* no CNAME */
		if(d->rr_len[0] < 2+1) break; /* too small */
		if((clen=dname_valid(d->rr_data[0]+2, d->rr_len[0]-2))==0)
			break; /* malformed */
		if(!dname_subdomain_c(d->rr_data[0]+2, z->name))
			break; /* target out of zone */
		if((node = az_find_name(z, d->rr_data[0]+2, clen))==NULL)
			break; /* no such target name */
		if((rrset=az_domain_rrset(node, qtype))!=NULL) {
			/* done we found the target */
			if(!msg_add_rrset_an(z, region, msg, node, rrset))
				return 0;
			break;
		}
		if((rrset=az_domain_rrset(node, LDNS_RR_TYPE_CNAME))==NULL)
			break; /* no further CNAME chain, notype */
		if(!msg_add_rrset_an(z, region, msg, node, rrset)) return 0;
		d = rrset->data;
	}
	return 1;
}

/** generate answer for cname answer */
static int
az_generate_cname_answer(struct auth_zone* z, struct query_info* qinfo,
	struct regional* region, struct dns_msg* msg,
	struct auth_data* node, struct auth_rrset* rrset)
{
	if(!msg_add_rrset_an(z, region, msg, node, rrset)) return 0;
	if(!rrset) return 1;
	if(!follow_cname_chain(z, qinfo->qtype, region, msg, rrset->data))
		return 0;
	return 1;
}

/** generate answer for notype answer */
static int
az_generate_notype_answer(struct auth_zone* z, struct regional* region,
	struct dns_msg* msg, struct auth_data* node)
{
	struct auth_rrset* rrset;
	if(!az_add_negative_soa(z, region, msg)) return 0;
	/* DNSSEC denial NSEC */
	if((rrset=az_domain_rrset(node, LDNS_RR_TYPE_NSEC))!=NULL) {
		if(!msg_add_rrset_ns(z, region, msg, node, rrset)) return 0;
	} else if(node) {
		/* DNSSEC denial NSEC3 */
		if(!az_add_nsec3_proof(z, region, msg, node->name,
			node->namelen, msg->qinfo.qname,
			msg->qinfo.qname_len, 0, 0))
			return 0;
	}
	return 1;
}

/** generate answer for referral answer */
static int
az_generate_referral_answer(struct auth_zone* z, struct regional* region,
	struct dns_msg* msg, struct auth_data* ce, struct auth_rrset* rrset)
{
	struct auth_rrset* ds, *nsec;
	/* turn off AA flag, referral is nonAA because it leaves the zone */
	log_assert(ce);
	msg->rep->flags &= ~BIT_AA;
	if(!msg_add_rrset_ns(z, region, msg, ce, rrset)) return 0;
	/* add DS or deny it */
	if((ds=az_domain_rrset(ce, LDNS_RR_TYPE_DS))!=NULL) {
		if(!msg_add_rrset_ns(z, region, msg, ce, ds)) return 0;
	} else {
		/* deny the DS */
		if((nsec=az_domain_rrset(ce, LDNS_RR_TYPE_NSEC))!=NULL) {
			if(!msg_add_rrset_ns(z, region, msg, ce, nsec))
				return 0;
		} else {
			if(!az_add_nsec3_proof(z, region, msg, ce->name,
				ce->namelen, msg->qinfo.qname,
				msg->qinfo.qname_len, 0, 0))
				return 0;
		}
	}
	/* add additional rrs for type NS */
	if(!az_add_additionals_from(z, region, msg, rrset, 0)) return 0;
	return 1;
}

/** generate answer for DNAME answer */
static int
az_generate_dname_answer(struct auth_zone* z, struct query_info* qinfo,
	struct regional* region, struct dns_msg* msg, struct auth_data* ce,
	struct auth_rrset* rrset)
{
	log_assert(ce);
	/* add the DNAME and then a CNAME */
	if(!msg_add_rrset_an(z, region, msg, ce, rrset)) return 0;
	if(!add_synth_cname(z, qinfo->qname, qinfo->qname_len, region,
		msg, ce, rrset)) return 0;
	if(FLAGS_GET_RCODE(msg->rep->flags) == LDNS_RCODE_YXDOMAIN)
		return 1;
	if(msg->rep->rrset_count == 0 ||
		!msg->rep->rrsets[msg->rep->rrset_count-1])
		return 0;
	if(!follow_cname_chain(z, qinfo->qtype, region, msg, 
		(struct packed_rrset_data*)msg->rep->rrsets[
		msg->rep->rrset_count-1]->entry.data))
		return 0;
	return 1;
}

/** generate answer for wildcard answer */
static int
az_generate_wildcard_answer(struct auth_zone* z, struct query_info* qinfo,
	struct regional* region, struct dns_msg* msg, struct auth_data* ce,
	struct auth_data* wildcard, struct auth_data* node)
{
	struct auth_rrset* rrset, *nsec;
	if(verbosity>=VERB_ALGO) {
		char wcname[256];
		sldns_wire2str_dname_buf(wildcard->name, wildcard->namelen,
			wcname, sizeof(wcname));
		log_info("wildcard %s", wcname);
	}
	if((rrset=az_domain_rrset(wildcard, qinfo->qtype)) != NULL) {
		/* wildcard has type, add it */
		if(!msg_add_rrset_an(z, region, msg, wildcard, rrset))
			return 0;
		az_change_dnames(msg, wildcard->name, msg->qinfo.qname,
			msg->qinfo.qname_len, 1);
	} else if((rrset=az_domain_rrset(wildcard, LDNS_RR_TYPE_CNAME))!=NULL) {
		/* wildcard has cname instead, do that */
		if(!msg_add_rrset_an(z, region, msg, wildcard, rrset))
			return 0;
		az_change_dnames(msg, wildcard->name, msg->qinfo.qname,
			msg->qinfo.qname_len, 1);
		if(!follow_cname_chain(z, qinfo->qtype, region, msg,
			rrset->data))
			return 0;
	} else if(qinfo->qtype == LDNS_RR_TYPE_ANY && wildcard->rrsets) {
		/* add ANY rrsets from wildcard node */
		if(!az_generate_any_answer(z, region, msg, wildcard))
			return 0;
		az_change_dnames(msg, wildcard->name, msg->qinfo.qname,
			msg->qinfo.qname_len, 1);
	} else {
		/* wildcard has nodata, notype answer */
		/* call other notype routine for dnssec notype denials */
		if(!az_generate_notype_answer(z, region, msg, wildcard))
			return 0;
	}

	/* ce and node for dnssec denial of wildcard original name */
	if((nsec=az_find_nsec_cover(z, &node)) != NULL) {
		if(!msg_add_rrset_ns(z, region, msg, node, nsec)) return 0;
	} else if(ce) {
		if(!az_add_nsec3_proof(z, region, msg, ce->name,
			ce->namelen, msg->qinfo.qname,
			msg->qinfo.qname_len, 1, 0))
			return 0;
	}

	/* fixup name of wildcard from *.zone to qname, use already allocated
	 * pointer to msg qname */
	az_change_dnames(msg, wildcard->name, msg->qinfo.qname,
		msg->qinfo.qname_len, 0);
	return 1;
}

/** generate answer for nxdomain answer */
static int
az_generate_nxdomain_answer(struct auth_zone* z, struct regional* region,
	struct dns_msg* msg, struct auth_data* ce, struct auth_data* node)
{
	struct auth_rrset* nsec;
	msg->rep->flags |= LDNS_RCODE_NXDOMAIN;
	if(!az_add_negative_soa(z, region, msg)) return 0;
	if((nsec=az_find_nsec_cover(z, &node)) != NULL) {
		if(!msg_add_rrset_ns(z, region, msg, node, nsec)) return 0;
		if(ce && !az_nsec_wildcard_denial(z, region, msg, ce->name,
			ce->namelen)) return 0;
	} else if(ce) {
		if(!az_add_nsec3_proof(z, region, msg, ce->name,
			ce->namelen, msg->qinfo.qname,
			msg->qinfo.qname_len, 1, 1))
			return 0;
	}
	return 1;
}

/** Create answers when an exact match exists for the domain name */
static int
az_generate_answer_with_node(struct auth_zone* z, struct query_info* qinfo,
	struct regional* region, struct dns_msg* msg, struct auth_data* node)
{
	struct auth_rrset* rrset;
	/* positive answer, rrset we are looking for exists */
	if((rrset=az_domain_rrset(node, qinfo->qtype)) != NULL) {
		return az_generate_positive_answer(z, region, msg, node, rrset);
	}
	/* CNAME? */
	if((rrset=az_domain_rrset(node, LDNS_RR_TYPE_CNAME)) != NULL) {
		return az_generate_cname_answer(z, qinfo, region, msg,
			node, rrset);
	}
	/* type ANY ? */
	if(qinfo->qtype == LDNS_RR_TYPE_ANY) {
		return az_generate_any_answer(z, region, msg, node);
	}
	/* NOERROR/NODATA (no such type at domain name) */
	return az_generate_notype_answer(z, region, msg, node);
}

/** Generate answer without an existing-node that we can use.
 * So it'll be a referral, DNAME or nxdomain */
static int
az_generate_answer_nonexistnode(struct auth_zone* z, struct query_info* qinfo,
	struct regional* region, struct dns_msg* msg, struct auth_data* ce,
	struct auth_rrset* rrset, struct auth_data* node)
{
	struct auth_data* wildcard;

	/* we do not have an exact matching name (that exists) */
	/* see if we have a NS or DNAME in the ce */
	if(ce && rrset && rrset->type == LDNS_RR_TYPE_NS) {
		return az_generate_referral_answer(z, region, msg, ce, rrset);
	}
	if(ce && rrset && rrset->type == LDNS_RR_TYPE_DNAME) {
		return az_generate_dname_answer(z, qinfo, region, msg, ce,
			rrset);
	}
	/* if there is an empty nonterminal, wildcard and nxdomain don't
	 * happen, it is a notype answer */
	if(az_empty_nonterminal(z, qinfo, node)) {
		return az_generate_notype_answer(z, region, msg, node);
	}
	/* see if we have a wildcard under the ce */
	if((wildcard=az_find_wildcard(z, qinfo, ce)) != NULL) {
		return az_generate_wildcard_answer(z, qinfo, region, msg,
			ce, wildcard, node);
	}
	/* generate nxdomain answer */
	return az_generate_nxdomain_answer(z, region, msg, ce, node);
}

/** Lookup answer in a zone. */
static int
auth_zone_generate_answer(struct auth_zone* z, struct query_info* qinfo,
	struct regional* region, struct dns_msg** msg, int* fallback)
{
	struct auth_data* node, *ce;
	struct auth_rrset* rrset;
	int node_exact, node_exists;
	/* does the zone want fallback in case of failure? */
	*fallback = z->fallback_enabled;
	if(!(*msg=msg_create(region, qinfo))) return 0;

	/* lookup if there is a matching domain name for the query */
	az_find_domain(z, qinfo, &node_exact, &node);

	/* see if node exists for generating answers from (i.e. not glue and
	 * obscured by NS or DNAME or NSEC3-only), and also return the
	 * closest-encloser from that, closest node that should be used
	 * to generate answers from that is above the query */
	node_exists = az_find_ce(z, qinfo, node, node_exact, &ce, &rrset);

	if(verbosity >= VERB_ALGO) {
		char zname[256], qname[256], nname[256], cename[256],
			tpstr[32], rrstr[32];
		sldns_wire2str_dname_buf(qinfo->qname, qinfo->qname_len, qname,
			sizeof(qname));
		sldns_wire2str_type_buf(qinfo->qtype, tpstr, sizeof(tpstr));
		sldns_wire2str_dname_buf(z->name, z->namelen, zname,
			sizeof(zname));
		if(node)
			sldns_wire2str_dname_buf(node->name, node->namelen,
				nname, sizeof(nname));
		else	snprintf(nname, sizeof(nname), "NULL");
		if(ce)
			sldns_wire2str_dname_buf(ce->name, ce->namelen,
				cename, sizeof(cename));
		else	snprintf(cename, sizeof(cename), "NULL");
		if(rrset) sldns_wire2str_type_buf(rrset->type, rrstr,
			sizeof(rrstr));
		else	snprintf(rrstr, sizeof(rrstr), "NULL");
		log_info("auth_zone %s query %s %s, domain %s %s %s, "
			"ce %s, rrset %s", zname, qname, tpstr, nname,
			(node_exact?"exact":"notexact"),
			(node_exists?"exist":"notexist"), cename, rrstr);
	}

	if(node_exists) {
		/* the node is fine, generate answer from node */
		return az_generate_answer_with_node(z, qinfo, region, *msg,
			node);
	}
	return az_generate_answer_nonexistnode(z, qinfo, region, *msg,
		ce, rrset, node);
}

int auth_zones_lookup(struct auth_zones* az, struct query_info* qinfo,
	struct regional* region, struct dns_msg** msg, int* fallback,
	uint8_t* dp_nm, size_t dp_nmlen)
{
	int r;
	struct auth_zone* z;

	/* find the zone that should contain the answer. */
	lock_rw_rdlock(&az->lock);
	z = auth_zone_find(az, dp_nm, dp_nmlen, qinfo->qclass);
	if(!z) {
		lock_rw_unlock(&az->lock);
		verbose(VERB_ALGO, "no auth zone for query, fallback");
		/* no auth zone, fallback to internet */
		*fallback = 1;
		return 0;
	}
	lock_rw_rdlock(&z->lock);
	lock_rw_unlock(&az->lock);

	/* see what answer that zone would generate */
	r = auth_zone_generate_answer(z, qinfo, region, msg, fallback);
	lock_rw_unlock(&z->lock);
	return r;
}
