/*
 * util/data/packed_rrset.c - data storage for a set of resource records.
 *
 * Copyright (c) 2007, NLnet Labs. All rights reserved.
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
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This file contains the data storage for RRsets.
 */

#include "config.h"
#include <ldns/wire2host.h>
#include "util/data/packed_rrset.h"
#include "util/data/dname.h"
#include "util/storage/lookup3.h"
#include "util/log.h"
#include "util/alloc.h"
#include "util/regional.h"
#include "util/net_help.h"

void
ub_packed_rrset_parsedelete(struct ub_packed_rrset_key* pkey,
        struct alloc_cache* alloc)
{
	if(!pkey)
		return;
	if(pkey->entry.data)
		free(pkey->entry.data);
	pkey->entry.data = NULL;
	if(pkey->rk.dname)
		free(pkey->rk.dname);
	pkey->rk.dname = NULL;
	pkey->id = 0;
	alloc_special_release(alloc, pkey);
}

size_t 
ub_rrset_sizefunc(void* key, void* data)
{
	struct ub_packed_rrset_key* k = (struct ub_packed_rrset_key*)key;
	struct packed_rrset_data* d = (struct packed_rrset_data*)data;
	size_t s = sizeof(struct ub_packed_rrset_key) + k->rk.dname_len;
	s += packed_rrset_sizeof(d) + lock_get_mem(&k->entry.lock);
	return s;
}

size_t 
packed_rrset_sizeof(struct packed_rrset_data* d)
{
	size_t s;
	if(d->rrsig_count > 0) {
		s = ((uint8_t*)d->rr_data[d->count+d->rrsig_count-1] - 
			(uint8_t*)d) + d->rr_len[d->count+d->rrsig_count-1];
	} else {
		log_assert(d->count > 0);
		s = ((uint8_t*)d->rr_data[d->count-1] - (uint8_t*)d) + 
			d->rr_len[d->count-1];
	}
	return s;
}

int 
ub_rrset_compare(void* k1, void* k2)
{
	struct ub_packed_rrset_key* key1 = (struct ub_packed_rrset_key*)k1;
	struct ub_packed_rrset_key* key2 = (struct ub_packed_rrset_key*)k2;
	int c;
	if(key1 == key2)
		return 0;
	if(key1->rk.type != key2->rk.type) {
		if(key1->rk.type < key2->rk.type)
			return -1;
		return 1;
	}
	if(key1->rk.dname_len != key2->rk.dname_len) {
		if(key1->rk.dname_len < key2->rk.dname_len)
			return -1;
		return 1;
	}
	if((c=query_dname_compare(key1->rk.dname, key2->rk.dname)) != 0)
		return c;
	if(key1->rk.rrset_class != key2->rk.rrset_class) {
		if(key1->rk.rrset_class < key2->rk.rrset_class)
			return -1;
		return 1;
	}
	if(key1->rk.flags != key2->rk.flags) {
		if(key1->rk.flags < key2->rk.flags)
			return -1;
		return 1;
	}
	return 0;
}

void 
ub_rrset_key_delete(void* key, void* userdata)
{
	struct ub_packed_rrset_key* k = (struct ub_packed_rrset_key*)key;
	struct alloc_cache* a = (struct alloc_cache*)userdata;
	k->id = 0;
	free(k->rk.dname);
	k->rk.dname = NULL;
	alloc_special_release(a, k);
}

void 
rrset_data_delete(void* data, void* ATTR_UNUSED(userdata))
{
	struct packed_rrset_data* d = (struct packed_rrset_data*)data;
	free(d);
}

int 
rrsetdata_equal(struct packed_rrset_data* d1, struct packed_rrset_data* d2)
{
	size_t i;
	size_t total;
	if(d1->count != d2->count || d1->rrsig_count != d2->rrsig_count) 
		return 0;
	total = d1->count + d1->rrsig_count;
	for(i=0; i<total; i++) {
		if(d1->rr_len[i] != d2->rr_len[i])
			return 0;
		if(memcmp(d1->rr_data[i], d2->rr_data[i], d1->rr_len[i]) != 0)
			return 0;
	}
	return 1;
}

hashvalue_t 
rrset_key_hash(struct packed_rrset_key* key)
{
	/* type is hashed in host order */
	uint16_t t = ntohs(key->type);
	/* Note this MUST be identical to pkt_hash_rrset in msgparse.c */
	/* this routine does not have a compressed name */
	hashvalue_t h = 0xab;
	h = dname_query_hash(key->dname, h);
	h = hashlittle(&t, sizeof(t), h);
	h = hashlittle(&key->rrset_class, sizeof(uint16_t), h);
	h = hashlittle(&key->flags, sizeof(uint32_t), h);
	return h;
}

void 
packed_rrset_ptr_fixup(struct packed_rrset_data* data)
{
	size_t i;
	size_t total = data->count + data->rrsig_count;
	uint8_t* nextrdata;
	/* fixup pointers in packed rrset data */
	data->rr_len = (size_t*)((uint8_t*)data +
		sizeof(struct packed_rrset_data));
	data->rr_data = (uint8_t**)&(data->rr_len[total]);
	data->rr_ttl = (uint32_t*)&(data->rr_data[total]);
	nextrdata = (uint8_t*)&(data->rr_ttl[total]);
	for(i=0; i<total; i++) {
		data->rr_data[i] = nextrdata;
		nextrdata += data->rr_len[i];
	}
}

void 
get_cname_target(struct ub_packed_rrset_key* rrset, uint8_t** dname, 
	size_t* dname_len)
{
	struct packed_rrset_data* d;
	size_t len;
	if(ntohs(rrset->rk.type) != LDNS_RR_TYPE_CNAME && 
		ntohs(rrset->rk.type) != LDNS_RR_TYPE_DNAME)
		return;
	d = (struct packed_rrset_data*)rrset->entry.data;
	if(d->count < 1)
		return;
	if(d->rr_len[0] < 3) /* at least rdatalen + 0byte root label */
		return;
	len = ldns_read_uint16(d->rr_data[0]);
	if(len != d->rr_len[0] - sizeof(uint16_t))
		return;
	if(dname_valid(d->rr_data[0]+sizeof(uint16_t), len) != len)
		return;
	*dname = d->rr_data[0]+sizeof(uint16_t);
	*dname_len = len;
}

void 
packed_rrset_ttl_add(struct packed_rrset_data* data, uint32_t add)
{
	size_t i;
	size_t total = data->count + data->rrsig_count;
	data->ttl += add;
	for(i=0; i<total; i++)
		data->rr_ttl[i] += add;
}

const char* 
rrset_trust_to_string(enum rrset_trust s)
{
	switch(s) {
	case rrset_trust_none: 		return "rrset_trust_none";
	case rrset_trust_add_noAA: 	return "rrset_trust_add_noAA";
	case rrset_trust_auth_noAA: 	return "rrset_trust_auth_noAA";
	case rrset_trust_add_AA: 	return "rrset_trust_add_AA";
	case rrset_trust_nonauth_ans_AA:return "rrset_trust_nonauth_ans_AA";
	case rrset_trust_ans_noAA: 	return "rrset_trust_ans_noAA";
	case rrset_trust_glue: 		return "rrset_trust_glue";
	case rrset_trust_auth_AA: 	return "rrset_trust_auth_AA";
	case rrset_trust_ans_AA: 	return "rrset_trust_ans_AA";
	case rrset_trust_sec_noglue: 	return "rrset_trust_sec_noglue";
	case rrset_trust_prim_noglue: 	return "rrset_trust_prim_noglue";
	case rrset_trust_validated: 	return "rrset_trust_validated";
	case rrset_trust_ultimate: 	return "rrset_trust_ultimate";
	}
	return "unknown_rrset_trust_value";
}

const char* 
sec_status_to_string(enum sec_status s)
{
	switch(s) {
	case sec_status_unchecked: 	return "sec_status_unchecked";
	case sec_status_bogus: 		return "sec_status_bogus";
	case sec_status_indeterminate: 	return "sec_status_indeterminate";
	case sec_status_insecure: 	return "sec_status_insecure";
	case sec_status_secure: 	return "sec_status_secure";
	}
	return "unknown_sec_status_value";
}

void log_rrset_key(enum verbosity_value v, const char* str, 
	struct ub_packed_rrset_key* rrset)
{
	if(verbosity >= v)
		log_nametypeclass(v, str, rrset->rk.dname,
			ntohs(rrset->rk.type), ntohs(rrset->rk.rrset_class));
}

uint32_t 
ub_packed_rrset_ttl(struct ub_packed_rrset_key* key)
{
	struct packed_rrset_data* d = (struct packed_rrset_data*)key->
		entry.data;
	return d->ttl;
}

struct ub_packed_rrset_key*
packed_rrset_copy_region(struct ub_packed_rrset_key* key, 
	struct regional* region, uint32_t now)
{
	struct ub_packed_rrset_key* ck = regional_alloc(region, 
		sizeof(struct ub_packed_rrset_key));
	struct packed_rrset_data* d;
	struct packed_rrset_data* data = (struct packed_rrset_data*)
		key->entry.data;
	size_t dsize, i;
	if(!ck)
		return NULL;
	ck->id = key->id;
	memset(&ck->entry, 0, sizeof(ck->entry));
	ck->entry.hash = key->entry.hash;
	ck->entry.key = ck;
	ck->rk = key->rk;
	ck->rk.dname = regional_alloc_init(region, key->rk.dname, 
		key->rk.dname_len);
	if(!ck->rk.dname)
		return NULL;
	dsize = packed_rrset_sizeof(data);
	d = (struct packed_rrset_data*)regional_alloc_init(region, data, dsize);
	if(!d)
		return NULL;
	ck->entry.data = d;
	packed_rrset_ptr_fixup(d);
	/* make TTLs relative - once per rrset */
	for(i=0; i<d->count + d->rrsig_count; i++) {
		if(d->rr_ttl[i] < now)
			d->rr_ttl[i] = 0;
		else	d->rr_ttl[i] -= now;
	}
	if(d->ttl < now)
		d->ttl = 0;
	else	d->ttl -= now;
	return ck;
}

struct ub_packed_rrset_key* 
packed_rrset_copy_alloc(struct ub_packed_rrset_key* key, 
	struct alloc_cache* alloc, uint32_t now)
{
	struct packed_rrset_data* fd, *dd;
	struct ub_packed_rrset_key* dk = alloc_special_obtain(alloc);
	if(!dk) return NULL;
	fd = (struct packed_rrset_data*)key->entry.data;
	dk->entry.hash = key->entry.hash;
	dk->rk = key->rk;
	dk->rk.dname = (uint8_t*)memdup(key->rk.dname, key->rk.dname_len);
	if(!dk->rk.dname) {
		alloc_special_release(alloc, dk);
		return NULL;
	}
	dd = (struct packed_rrset_data*)memdup(fd, packed_rrset_sizeof(fd));
	if(!dd) {
		free(dk->rk.dname);
		alloc_special_release(alloc, dk);
		return NULL;
	}
	packed_rrset_ptr_fixup(dd);
	dk->entry.data = (void*)dd;
	packed_rrset_ttl_add(dd, now);
	return dk;
}

struct ub_packed_rrset_key* 
ub_packed_rrset_heap_key(ldns_rr_list* rrset)
{
	struct ub_packed_rrset_key* k;
	ldns_rr* rr;
	if(!rrset)
		return NULL;
	rr = ldns_rr_list_rr(rrset, 0);
	if(!rr)
		return NULL;
	k = (struct ub_packed_rrset_key*)calloc(1, sizeof(*k));
	if(!k)
		return NULL;
	k->rk.type = htons(ldns_rr_get_type(rr));
	k->rk.rrset_class = htons(ldns_rr_get_class(rr));
	k->rk.dname_len = ldns_rdf_size(ldns_rr_owner(rr));
	k->rk.dname = memdup(ldns_rdf_data(ldns_rr_owner(rr)),
		ldns_rdf_size(ldns_rr_owner(rr)));
	if(!k->rk.dname) {
		free(k);
		return NULL;
	}
	return k;
}

struct packed_rrset_data* 
packed_rrset_heap_data(ldns_rr_list* rrset)
{
	struct packed_rrset_data* data;
	size_t count=0, rrsig_count=0, len=0, i, j, total;
	uint8_t* nextrdata;
	if(!rrset || ldns_rr_list_rr_count(rrset)==0)
		return NULL;
	/* count sizes */
	for(i=0; i<ldns_rr_list_rr_count(rrset); i++) {
		ldns_rr* rr = ldns_rr_list_rr(rrset, i);
		if(ldns_rr_get_type(rr) == LDNS_RR_TYPE_RRSIG)
			rrsig_count++;
		else 	count++;
		for(j=0; j<ldns_rr_rd_count(rr); j++)
			len += ldns_rdf_size(ldns_rr_rdf(rr, j));
		len += 2; /* sizeof the rdlength */
	}

	/* allocate */
	total = count + rrsig_count;
	len += sizeof(*data) + total*(sizeof(size_t) + sizeof(uint32_t) + 
		sizeof(uint8_t*));
	data = (struct packed_rrset_data*)calloc(1, len);
	if(!data)
		return NULL;
	
	/* fill it */
	data->ttl = ldns_rr_ttl(ldns_rr_list_rr(rrset, 0));
	data->count = count;
	data->rrsig_count = rrsig_count;
	data->rr_len = (size_t*)((uint8_t*)data +
		sizeof(struct packed_rrset_data));
	data->rr_data = (uint8_t**)&(data->rr_len[total]);
	data->rr_ttl = (uint32_t*)&(data->rr_data[total]);
	nextrdata = (uint8_t*)&(data->rr_ttl[total]);

	/* fill out len, ttl, fields */
	for(i=0; i<total; i++) {
		ldns_rr* rr = ldns_rr_list_rr(rrset, i);
		data->rr_ttl[i] = ldns_rr_ttl(rr);
		if(data->rr_ttl[i] < data->ttl)
			data->ttl = data->rr_ttl[i];
		data->rr_len[i] = 2; /* the rdlength */
		for(j=0; j<ldns_rr_rd_count(rr); j++)
			data->rr_len[i] += ldns_rdf_size(ldns_rr_rdf(rr, j));
	}

	/* fixup rest of ptrs */
	for(i=0; i<total; i++) {
		data->rr_data[i] = nextrdata;
		nextrdata += data->rr_len[i];
	}

	/* copy data in there */
	for(i=0; i<total; i++) {
		ldns_rr* rr = ldns_rr_list_rr(rrset, i);
		uint16_t rdlen = htons(data->rr_len[i]-2);
		size_t p = sizeof(rdlen);
		memmove(data->rr_data[i], &rdlen, p);
		for(j=0; j<ldns_rr_rd_count(rr); j++) {
			ldns_rdf* rd = ldns_rr_rdf(rr, j);
			memmove(data->rr_data[i]+p, ldns_rdf_data(rd),
				ldns_rdf_size(rd));
			p += ldns_rdf_size(rd);
		}
	}

	if(data->rrsig_count && data->count == 0) {
		data->count = data->rrsig_count; /* rrset type is RRSIG */
		data->rrsig_count = 0;
	}
	return data;
}

/** convert i'th rr to ldns_rr */
static ldns_rr*
torr(struct ub_packed_rrset_key* k, ldns_buffer* buf, size_t i)
{
	struct packed_rrset_data* d = (struct packed_rrset_data*)k->entry.data;
	ldns_rr* rr = NULL;
	size_t pos = 0;
	ldns_status s;
	ldns_buffer_clear(buf);
	ldns_buffer_write(buf, k->rk.dname, k->rk.dname_len);
	if(i < d->count)
		ldns_buffer_write(buf, &k->rk.type, sizeof(uint16_t));
	else 	ldns_buffer_write_u16(buf, LDNS_RR_TYPE_RRSIG);
	ldns_buffer_write(buf, &k->rk.rrset_class, sizeof(uint16_t));
	ldns_buffer_write_u32(buf, d->rr_ttl[i]);
	ldns_buffer_write(buf, d->rr_data[i], d->rr_len[i]);
	ldns_buffer_flip(buf);
	s = ldns_wire2rr(&rr, ldns_buffer_begin(buf), ldns_buffer_limit(buf),
		&pos, LDNS_SECTION_ANSWER);
	if(s == LDNS_STATUS_OK)
		return rr;
	return NULL;
}

ldns_rr_list* 
packed_rrset_to_rr_list(struct ub_packed_rrset_key* k, ldns_buffer* buf)
{
	struct packed_rrset_data* d = (struct packed_rrset_data*)k->entry.data;
	ldns_rr_list* r = ldns_rr_list_new();
	size_t i;
	if(!r)
		return NULL;
	for(i=0; i<d->count+d->rrsig_count; i++) {
		ldns_rr* rr = torr(k, buf, i);
		if(!rr) {
			ldns_rr_list_deep_free(r);
			return NULL;
		}
		if(!ldns_rr_list_push_rr(r, rr)) {
			ldns_rr_free(rr);
			ldns_rr_list_deep_free(r);
			return NULL;
		}
	}
	return r;
}
