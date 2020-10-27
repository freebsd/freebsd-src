/*
 * services/rpz.c - rpz service
 *
 * Copyright (c) 2019, NLnet Labs. All rights reserved.
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
 * This file contains functions to enable RPZ service.
 */

#include "config.h"
#include "services/rpz.h"
#include "util/config_file.h"
#include "sldns/wire2str.h"
#include "sldns/str2wire.h"
#include "util/data/dname.h"
#include "util/net_help.h"
#include "util/log.h"
#include "util/data/dname.h"
#include "util/locks.h"
#include "util/regional.h"

/** string for RPZ action enum */
const char*
rpz_action_to_string(enum rpz_action a)
{
	switch(a) {
	case RPZ_NXDOMAIN_ACTION:	return "nxdomain";
	case RPZ_NODATA_ACTION:		return "nodata";
	case RPZ_PASSTHRU_ACTION:	return "passthru";
	case RPZ_DROP_ACTION:		return "drop";
	case RPZ_TCP_ONLY_ACTION:	return "tcp_only";
	case RPZ_INVALID_ACTION:	return "invalid";
	case RPZ_LOCAL_DATA_ACTION:	return "local_data";
	case RPZ_DISABLED_ACTION:	return "disabled";
	case RPZ_CNAME_OVERRIDE_ACTION:	return "cname_override";
	case RPZ_NO_OVERRIDE_ACTION:	return "no_override";
	}
	return "unknown";
}

/** RPZ action enum for config string */
static enum rpz_action
rpz_config_to_action(char* a)
{
	if(strcmp(a, "nxdomain") == 0)
		return RPZ_NXDOMAIN_ACTION;
	else if(strcmp(a, "nodata") == 0)
		return RPZ_NODATA_ACTION;
	else if(strcmp(a, "passthru") == 0)
		return RPZ_PASSTHRU_ACTION;
	else if(strcmp(a, "drop") == 0)
		return RPZ_DROP_ACTION;
	else if(strcmp(a, "tcp_only") == 0)
		return RPZ_TCP_ONLY_ACTION;
	else if(strcmp(a, "cname") == 0)
		return RPZ_CNAME_OVERRIDE_ACTION;
	else if(strcmp(a, "disabled") == 0)
		return RPZ_DISABLED_ACTION;
	return RPZ_INVALID_ACTION;
}

/** string for RPZ trigger enum */
static const char*
rpz_trigger_to_string(enum rpz_trigger r)
{
	switch(r) {
	case RPZ_QNAME_TRIGGER:		return "qname";
	case RPZ_CLIENT_IP_TRIGGER:	return "client_ip";
	case RPZ_RESPONSE_IP_TRIGGER:	return "response_ip";
	case RPZ_NSDNAME_TRIGGER:	return "nsdname";
	case RPZ_NSIP_TRIGGER:		return "nsip";
	case RPZ_INVALID_TRIGGER:	return "invalid";
	}
	return "unknown";
}

/**
 * Get the label that is just before the root label.
 * @param dname: dname to work on
 * @param maxdnamelen: maximum length of the dname
 * @return: pointer to TLD label, NULL if not found or invalid dname
 */
static uint8_t*
get_tld_label(uint8_t* dname, size_t maxdnamelen)
{
	uint8_t* prevlab = dname;
	size_t dnamelen = 0;

	/* one byte needed for label length */
	if(dnamelen+1 > maxdnamelen)
		return NULL;

	/* only root label */
	if(*dname == 0)
		return NULL;

	while(*dname) {
		dnamelen += ((size_t)*dname)+1;
		if(dnamelen+1 > maxdnamelen)
			return NULL;
		dname = dname+((size_t)*dname)+1;
		if(*dname != 0)
			prevlab = dname;
	}
	return prevlab;
}

/**
 * Classify RPZ action for RR type/rdata
 * @param rr_type: the RR type
 * @param rdatawl: RDATA with 2 bytes length
 * @param rdatalen: the length of rdatawl (including its 2 bytes length)
 * @return: the RPZ action
 */
static enum rpz_action
rpz_rr_to_action(uint16_t rr_type, uint8_t* rdatawl, size_t rdatalen)
{
	char* endptr;
	uint8_t* rdata;
	int rdatalabs;
	uint8_t* tldlab = NULL;

	switch(rr_type) {
		case LDNS_RR_TYPE_SOA:
		case LDNS_RR_TYPE_NS:
		case LDNS_RR_TYPE_DNAME:
		/* all DNSSEC-related RRs must be ignored */
		case LDNS_RR_TYPE_DNSKEY:
		case LDNS_RR_TYPE_DS:
		case LDNS_RR_TYPE_RRSIG:
		case LDNS_RR_TYPE_NSEC:
		case LDNS_RR_TYPE_NSEC3:
			return RPZ_INVALID_ACTION;
		case LDNS_RR_TYPE_CNAME:
			break;
		default:
			return RPZ_LOCAL_DATA_ACTION;
	}

	/* use CNAME target to determine RPZ action */
	log_assert(rr_type == LDNS_RR_TYPE_CNAME);
	if(rdatalen < 3)
		return RPZ_INVALID_ACTION;

	rdata = rdatawl + 2; /* 2 bytes of rdata length */
	if(dname_valid(rdata, rdatalen-2) != rdatalen-2)
		return RPZ_INVALID_ACTION;

	rdatalabs = dname_count_labels(rdata);
	if(rdatalabs == 1)
		return RPZ_NXDOMAIN_ACTION;
	else if(rdatalabs == 2) {
		if(dname_subdomain_c(rdata, (uint8_t*)&"\001*\000"))
			return RPZ_NODATA_ACTION;
		else if(dname_subdomain_c(rdata,
			(uint8_t*)&"\014rpz-passthru\000"))
			return RPZ_PASSTHRU_ACTION;
		else if(dname_subdomain_c(rdata, (uint8_t*)&"\010rpz-drop\000"))
			return RPZ_DROP_ACTION;
		else if(dname_subdomain_c(rdata,
			(uint8_t*)&"\014rpz-tcp-only\000"))
			return RPZ_TCP_ONLY_ACTION;
	}

	/* all other TLDs starting with "rpz-" are invalid */
	tldlab = get_tld_label(rdata, rdatalen-2);
	if(tldlab && dname_lab_startswith(tldlab, "rpz-", &endptr))
		return RPZ_INVALID_ACTION;

	/* no special label found */
	return RPZ_LOCAL_DATA_ACTION;
}

static enum localzone_type 
rpz_action_to_localzone_type(enum rpz_action a)
{
	switch(a) {
	case RPZ_NXDOMAIN_ACTION:	return local_zone_always_nxdomain;
	case RPZ_NODATA_ACTION:		return local_zone_always_nodata;
	case RPZ_DROP_ACTION:		return local_zone_always_deny;
	case RPZ_PASSTHRU_ACTION:	return local_zone_always_transparent;
	case RPZ_LOCAL_DATA_ACTION:	/* fallthrough */
	case RPZ_CNAME_OVERRIDE_ACTION: return local_zone_redirect;
	case RPZ_INVALID_ACTION: 	/* fallthrough */
	case RPZ_TCP_ONLY_ACTION:	/* fallthrough */
	default:			return local_zone_invalid;
	}
}

enum respip_action
rpz_action_to_respip_action(enum rpz_action a)
{
	switch(a) {
	case RPZ_NXDOMAIN_ACTION:	return respip_always_nxdomain;
	case RPZ_NODATA_ACTION:		return respip_always_nodata;
	case RPZ_DROP_ACTION:		return respip_always_deny;
	case RPZ_PASSTHRU_ACTION:	return respip_always_transparent;
	case RPZ_LOCAL_DATA_ACTION:	/* fallthrough */
	case RPZ_CNAME_OVERRIDE_ACTION: return respip_redirect;
	case RPZ_INVALID_ACTION:	/* fallthrough */
	case RPZ_TCP_ONLY_ACTION:	/* fallthrough */
	default:			return respip_invalid;
	}
}

static enum rpz_action
localzone_type_to_rpz_action(enum localzone_type lzt)
{
	switch(lzt) {
	case local_zone_always_nxdomain:	return RPZ_NXDOMAIN_ACTION;
	case local_zone_always_nodata:		return RPZ_NODATA_ACTION;
	case local_zone_always_deny:		return RPZ_DROP_ACTION;
	case local_zone_always_transparent:	return RPZ_PASSTHRU_ACTION;
	case local_zone_redirect:		return RPZ_LOCAL_DATA_ACTION;
	case local_zone_invalid:
	default:
		return RPZ_INVALID_ACTION;
	}
}

enum rpz_action
respip_action_to_rpz_action(enum respip_action a)
{
	switch(a) {
	case respip_always_nxdomain:	return RPZ_NXDOMAIN_ACTION;
	case respip_always_nodata:	return RPZ_NODATA_ACTION;
	case respip_always_deny:	return RPZ_DROP_ACTION;
	case respip_always_transparent:	return RPZ_PASSTHRU_ACTION;
	case respip_redirect:		return RPZ_LOCAL_DATA_ACTION;
	case respip_invalid:
	default:
		return RPZ_INVALID_ACTION;
	}
}

/**
 * Get RPZ trigger for dname
 * @param dname: dname containing RPZ trigger
 * @param dname_len: length of the dname
 * @return: RPZ trigger enum
 */
static enum rpz_trigger
rpz_dname_to_trigger(uint8_t* dname, size_t dname_len)
{
	uint8_t* tldlab;
	char* endptr;

	if(dname_valid(dname, dname_len) != dname_len)
		return RPZ_INVALID_TRIGGER;

	tldlab = get_tld_label(dname, dname_len);
	if(!tldlab || !dname_lab_startswith(tldlab, "rpz-", &endptr))
		return RPZ_QNAME_TRIGGER;

	if(dname_subdomain_c(tldlab,
		(uint8_t*)&"\015rpz-client-ip\000"))
		return RPZ_CLIENT_IP_TRIGGER;
	else if(dname_subdomain_c(tldlab, (uint8_t*)&"\006rpz-ip\000"))
		return RPZ_RESPONSE_IP_TRIGGER;
	else if(dname_subdomain_c(tldlab, (uint8_t*)&"\013rpz-nsdname\000"))
		return RPZ_NSDNAME_TRIGGER;
	else if(dname_subdomain_c(tldlab, (uint8_t*)&"\010rpz-nsip\000"))
		return RPZ_NSIP_TRIGGER;

	return RPZ_QNAME_TRIGGER;
}

void rpz_delete(struct rpz* r)
{
	if(!r)
		return;
	local_zones_delete(r->local_zones);
	respip_set_delete(r->respip_set);
	regional_destroy(r->region);
	free(r->taglist);
	free(r->log_name);
	free(r);
}

int
rpz_clear(struct rpz* r)
{
	/* must hold write lock on auth_zone */
	local_zones_delete(r->local_zones);
	respip_set_delete(r->respip_set);
	if(!(r->local_zones = local_zones_create())){
		return 0;
	}
	if(!(r->respip_set = respip_set_create())) {
		return 0;
	}
	return 1;
}

void
rpz_finish_config(struct rpz* r)
{
	lock_rw_wrlock(&r->respip_set->lock);
	addr_tree_init_parents(&r->respip_set->ip_tree);
	lock_rw_unlock(&r->respip_set->lock);
}

/** new rrset containing CNAME override, does not yet contain a dname */
static struct ub_packed_rrset_key*
new_cname_override(struct regional* region, uint8_t* ct, size_t ctlen)
{
	struct ub_packed_rrset_key* rrset;
	struct packed_rrset_data* pd;
	uint16_t rdlength = htons(ctlen);
	rrset = (struct ub_packed_rrset_key*)regional_alloc_zero(region,
		sizeof(*rrset));
	if(!rrset) {
		log_err("out of memory");
		return NULL;
	}
	rrset->entry.key = rrset;
	pd = (struct packed_rrset_data*)regional_alloc_zero(region, sizeof(*pd));
	if(!pd) {
		log_err("out of memory");
		return NULL;
	}
	pd->trust = rrset_trust_prim_noglue;
	pd->security = sec_status_insecure;

	pd->count = 1;
	pd->rr_len = regional_alloc_zero(region, sizeof(*pd->rr_len));
	pd->rr_ttl = regional_alloc_zero(region, sizeof(*pd->rr_ttl));
	pd->rr_data = regional_alloc_zero(region, sizeof(*pd->rr_data));
	if(!pd->rr_len || !pd->rr_ttl || !pd->rr_data) {
		log_err("out of memory");
		return NULL;
	}
	pd->rr_len[0] = ctlen+2;
	pd->rr_ttl[0] = 3600;
	pd->rr_data[0] = regional_alloc_zero(region, 2 /* rdlength */ + ctlen);
	if(!pd->rr_data[0]) {
		log_err("out of memory");
		return NULL;
	}
	memmove(pd->rr_data[0], &rdlength, 2);
	memmove(pd->rr_data[0]+2, ct, ctlen);

	rrset->entry.data = pd;
	rrset->rk.type = htons(LDNS_RR_TYPE_CNAME);
	rrset->rk.rrset_class = htons(LDNS_RR_CLASS_IN);
	return rrset;
}

struct rpz*
rpz_create(struct config_auth* p)
{
	struct rpz* r = calloc(1, sizeof(*r));
	if(!r)
		goto err;

	r->region = regional_create_custom(sizeof(struct regional));
	if(!r->region) {
		goto err;
	}

	if(!(r->local_zones = local_zones_create())){
		goto err;
	}
	if(!(r->respip_set = respip_set_create())) {
		goto err;
	}
	r->taglistlen = p->rpz_taglistlen;
	r->taglist = memdup(p->rpz_taglist, r->taglistlen);
	if(p->rpz_action_override) {
		r->action_override = rpz_config_to_action(p->rpz_action_override);
	}
	else
		r->action_override = RPZ_NO_OVERRIDE_ACTION;

	if(r->action_override == RPZ_CNAME_OVERRIDE_ACTION) {
		uint8_t nm[LDNS_MAX_DOMAINLEN+1];
		size_t nmlen = sizeof(nm);

		if(!p->rpz_cname) {
			log_err("RPZ override with cname action found, but no "
				"rpz-cname-override configured");
			goto err;
		}

		if(sldns_str2wire_dname_buf(p->rpz_cname, nm, &nmlen) != 0) {
			log_err("cannot parse RPZ cname override: %s",
				p->rpz_cname);
			goto err;
		}
		r->cname_override = new_cname_override(r->region, nm, nmlen);
		if(!r->cname_override) {
			goto err;
		}
	}
	r->log = p->rpz_log;
	if(p->rpz_log_name) {
		if(!(r->log_name = strdup(p->rpz_log_name))) {
			log_err("malloc failure on RPZ log_name strdup");
			goto err;
		}
	}
	return r;
err:
	if(r) {
		if(r->local_zones)
			local_zones_delete(r->local_zones);
		if(r->respip_set)
			respip_set_delete(r->respip_set);
		if(r->taglist)
			free(r->taglist);
		free(r);
	}
	return NULL;
}

/**
 * Remove RPZ zone name from dname
 * Copy dname to newdname, without the originlen number of trailing bytes
 */
static size_t
strip_dname_origin(uint8_t* dname, size_t dnamelen, size_t originlen,
	uint8_t* newdname, size_t maxnewdnamelen)
{
	size_t newdnamelen;
	if(dnamelen < originlen)
		return 0;
	newdnamelen = dnamelen - originlen;
	if(newdnamelen+1 > maxnewdnamelen)
		return 0;
	memmove(newdname, dname, newdnamelen);
	newdname[newdnamelen] = 0;
	return newdnamelen + 1;	/* + 1 for root label */
}

/** Insert RR into RPZ's local-zone */
static void
rpz_insert_qname_trigger(struct rpz* r, uint8_t* dname, size_t dnamelen,
	enum rpz_action a, uint16_t rrtype, uint16_t rrclass, uint32_t ttl,
	uint8_t* rdata, size_t rdata_len, uint8_t* rr, size_t rr_len)
{
	struct local_zone* z;
	enum localzone_type tp = local_zone_always_transparent;
	int dnamelabs = dname_count_labels(dname);
	char* rrstr;
	int newzone = 0;

	if(a == RPZ_TCP_ONLY_ACTION || a == RPZ_INVALID_ACTION) {
		verbose(VERB_ALGO, "RPZ: skipping unsupported action: %s",
			rpz_action_to_string(a));
		free(dname);
		return;
	}

	lock_rw_wrlock(&r->local_zones->lock);
	/* exact match */
	z = local_zones_find(r->local_zones, dname, dnamelen, dnamelabs,
		LDNS_RR_CLASS_IN);
	if(z && a != RPZ_LOCAL_DATA_ACTION) {
		rrstr = sldns_wire2str_rr(rr, rr_len);
		if(!rrstr) {
			log_err("malloc error while inserting RPZ qname "
				"trigger");
			free(dname);
			lock_rw_unlock(&r->local_zones->lock);
			return;
		}
		verbose(VERB_ALGO, "RPZ: skipping duplicate record: '%s'",
			rrstr);
		free(rrstr);
		free(dname);
		lock_rw_unlock(&r->local_zones->lock);
		return;
	}
	if(!z) {
		tp = rpz_action_to_localzone_type(a);
		if(!(z = local_zones_add_zone(r->local_zones, dname, dnamelen,
			dnamelabs, rrclass, tp))) {
			log_warn("RPZ create failed");
			lock_rw_unlock(&r->local_zones->lock);
			/* dname will be free'd in failed local_zone_create() */
			return;
		}
		newzone = 1;
	}
	if(a == RPZ_LOCAL_DATA_ACTION) {
		rrstr = sldns_wire2str_rr(rr, rr_len);
		if(!rrstr) {
			log_err("malloc error while inserting RPZ qname "
				"trigger");
			free(dname);
			lock_rw_unlock(&r->local_zones->lock);
			return;
		}
		lock_rw_wrlock(&z->lock);
		local_zone_enter_rr(z, dname, dnamelen, dnamelabs,
			rrtype, rrclass, ttl, rdata, rdata_len, rrstr);
		lock_rw_unlock(&z->lock);
		free(rrstr);
	}
	if(!newzone)
		free(dname);
	lock_rw_unlock(&r->local_zones->lock);
	return;
}

/** Insert RR into RPZ's respip_set */
static int
rpz_insert_response_ip_trigger(struct rpz* r, uint8_t* dname, size_t dnamelen,
	enum rpz_action a, uint16_t rrtype, uint16_t rrclass, uint32_t ttl,
	uint8_t* rdata, size_t rdata_len, uint8_t* rr, size_t rr_len)
{
	struct resp_addr* node;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int net, af;
	char* rrstr;
	enum respip_action respa = rpz_action_to_respip_action(a);

	if(a == RPZ_TCP_ONLY_ACTION || a == RPZ_INVALID_ACTION ||
		respa == respip_invalid) {
		verbose(VERB_ALGO, "RPZ: skipping unsupported action: %s",
			rpz_action_to_string(a));
		return 0;
	}

	if(!netblockdnametoaddr(dname, dnamelen, &addr, &addrlen, &net, &af))
		return 0;

	lock_rw_wrlock(&r->respip_set->lock);
	rrstr = sldns_wire2str_rr(rr, rr_len);
	if(!rrstr) {
		log_err("malloc error while inserting RPZ respip trigger");
		lock_rw_unlock(&r->respip_set->lock);
		return 0;
	}
	if(!(node=respip_sockaddr_find_or_create(r->respip_set, &addr, addrlen,
		net, 1, rrstr))) {
		lock_rw_unlock(&r->respip_set->lock);
		free(rrstr);
		return 0;
	}

	lock_rw_wrlock(&node->lock);
	lock_rw_unlock(&r->respip_set->lock);
	node->action = respa;

	if(a == RPZ_LOCAL_DATA_ACTION) {
		respip_enter_rr(r->respip_set->region, node, rrtype,
			rrclass, ttl, rdata, rdata_len, rrstr, "");
	}
	lock_rw_unlock(&node->lock);
	free(rrstr);
	return 1;
}

int
rpz_insert_rr(struct rpz* r, uint8_t* azname, size_t aznamelen, uint8_t* dname,
	size_t dnamelen, uint16_t rr_type, uint16_t rr_class, uint32_t rr_ttl,
	uint8_t* rdatawl, size_t rdatalen, uint8_t* rr, size_t rr_len)
{
	size_t policydnamelen;
	/* name is free'd in local_zone delete */
	enum rpz_trigger t;
	enum rpz_action a;
	uint8_t* policydname;

	if(!dname_subdomain_c(dname, azname)) {
		char* dname_str = sldns_wire2str_dname(dname, dnamelen);
		char* azname_str = sldns_wire2str_dname(azname, aznamelen);
		if(dname_str && azname_str) {
			log_err("RPZ: name of record (%s) to insert into RPZ is not a "
				"subdomain of the configured name of the RPZ zone (%s)",
				dname_str, azname_str);
		} else {
			log_err("RPZ: name of record to insert into RPZ is not a "
				"subdomain of the configured name of the RPZ zone");
		}
		free(dname_str);
		free(azname_str);
		return 0;
	}

	log_assert(dnamelen >= aznamelen);
	if(!(policydname = calloc(1, (dnamelen-aznamelen)+1))) {
		log_err("malloc error while inserting RPZ RR");
		return 0;
	}

	a = rpz_rr_to_action(rr_type, rdatawl, rdatalen);
	if(!(policydnamelen = strip_dname_origin(dname, dnamelen, aznamelen,
		policydname, (dnamelen-aznamelen)+1))) {
		free(policydname);
		return 0;
	}
	t = rpz_dname_to_trigger(policydname, policydnamelen);
	if(t == RPZ_INVALID_TRIGGER) {
		free(policydname);
		verbose(VERB_ALGO, "RPZ: skipping invalid trigger");
		return 1;
	}
	if(t == RPZ_QNAME_TRIGGER) {
		rpz_insert_qname_trigger(r, policydname, policydnamelen,
			a, rr_type, rr_class, rr_ttl, rdatawl, rdatalen, rr,
			rr_len);
	}
	else if(t == RPZ_RESPONSE_IP_TRIGGER) {
		rpz_insert_response_ip_trigger(r, policydname, policydnamelen,
			a, rr_type, rr_class, rr_ttl, rdatawl, rdatalen, rr,
			rr_len);
		free(policydname);
	}
	else {
		free(policydname);
		verbose(VERB_ALGO, "RPZ: skipping unsupported trigger: %s",
			rpz_trigger_to_string(t));
	}
	return 1;
}

/**
 * Find RPZ local-zone by qname.
 * @param r: rpz containing local-zone tree
 * @param qname: qname
 * @param qname_len: length of qname
 * @param qclass: qclass
 * @param only_exact: if 1 only excact (non wildcard) matches are returned
 * @param wr: get write lock for local-zone if 1, read lock if 0
 * @param zones_keep_lock: if set do not release the r->local_zones lock, this
 * 	  makes the caller of this function responsible for releasing the lock.
 * @return: NULL or local-zone holding rd or wr lock
 */
static struct local_zone*
rpz_find_zone(struct rpz* r, uint8_t* qname, size_t qname_len, uint16_t qclass,
	int only_exact, int wr, int zones_keep_lock)
{
	uint8_t* ce;
	size_t ce_len, ce_labs;
	uint8_t wc[LDNS_MAX_DOMAINLEN+1];
	int exact;
	struct local_zone* z = NULL;
	if(wr) {
		lock_rw_wrlock(&r->local_zones->lock);
	} else {
		lock_rw_rdlock(&r->local_zones->lock);
	}
	z = local_zones_find_le(r->local_zones, qname, qname_len,
		dname_count_labels(qname),
		LDNS_RR_CLASS_IN, &exact);
	if(!z || (only_exact && !exact)) {
		lock_rw_unlock(&r->local_zones->lock);
		return NULL;
	}
	if(wr) {
		lock_rw_wrlock(&z->lock);
	} else {
		lock_rw_rdlock(&z->lock);
	}
	if(!zones_keep_lock) {
		lock_rw_unlock(&r->local_zones->lock);
	}

	if(exact)
		return z;

	/* No exact match found, lookup wildcard. closest encloser must
	 * be the shared parent between the qname and the best local
	 * zone match, append '*' to that and do another lookup. */

	ce = dname_get_shared_topdomain(z->name, qname);
	if(!ce /* should not happen */ || !*ce /* root */) {
		lock_rw_unlock(&z->lock);
		if(zones_keep_lock) {
			lock_rw_unlock(&r->local_zones->lock);
		}
		return NULL;
	}
	ce_labs = dname_count_size_labels(ce, &ce_len);
	if(ce_len+2 > sizeof(wc)) {
		lock_rw_unlock(&z->lock);
		if(zones_keep_lock) {
			lock_rw_unlock(&r->local_zones->lock);
		}
		return NULL;
	}
	wc[0] = 1; /* length of wildcard label */
	wc[1] = (uint8_t)'*'; /* wildcard label */
	memmove(wc+2, ce, ce_len);
	lock_rw_unlock(&z->lock);

	if(!zones_keep_lock) {
		if(wr) {
			lock_rw_wrlock(&r->local_zones->lock);
		} else {
			lock_rw_rdlock(&r->local_zones->lock);
		}
	}
	z = local_zones_find_le(r->local_zones, wc,
		ce_len+2, ce_labs+1, qclass, &exact);
	if(!z || !exact) {
		lock_rw_unlock(&r->local_zones->lock);
		return NULL;
	}
	if(wr) {
		lock_rw_wrlock(&z->lock);
	} else {
		lock_rw_rdlock(&z->lock);
	}
	if(!zones_keep_lock) {
		lock_rw_unlock(&r->local_zones->lock);
	}
	return z;
}

/**
 * Remove RR from RPZ's local-data
 * @param z: local-zone for RPZ, holding write lock
 * @param policydname: dname of RR to remove
 * @param policydnamelen: lenth of policydname
 * @param rr_type: RR type of RR to remove
 * @param rdata: rdata of RR to remove
 * @param rdatalen: length of rdata
 * @return: 1 if zone must be removed after RR deletion
 */
static int
rpz_data_delete_rr(struct local_zone* z, uint8_t* policydname,
	size_t policydnamelen, uint16_t rr_type, uint8_t* rdata,
	size_t rdatalen)
{
	struct local_data* ld;
	struct packed_rrset_data* d;
	size_t index;
	ld = local_zone_find_data(z, policydname, policydnamelen,
		dname_count_labels(policydname));
	if(ld) {
		struct local_rrset* prev=NULL, *p=ld->rrsets;
		while(p && ntohs(p->rrset->rk.type) != rr_type) {
			prev = p;
			p = p->next;
		}
		if(!p)
			return 0;
		d = (struct packed_rrset_data*)p->rrset->entry.data;
		if(packed_rrset_find_rr(d, rdata, rdatalen, &index)) {
			if(d->count == 1) {
				/* no memory recycling for zone deletions ... */
				if(prev) prev->next = p->next;
				else ld->rrsets = p->next;
			}
			if(d->count > 1) {
				if(!local_rrset_remove_rr(d, index))
					return 0;
			}
		}
	}
	if(ld && ld->rrsets)
		return 0;
	return 1;
}

/**
 * Remove RR from RPZ's respip set
 * @param raddr: respip node
 * @param rr_type: RR type of RR to remove
 * @param rdata: rdata of RR to remove
 * @param rdatalen: length of rdata
 * @return: 1 if zone must be removed after RR deletion
 */
static int
rpz_rrset_delete_rr(struct resp_addr* raddr, uint16_t rr_type, uint8_t* rdata,
	size_t rdatalen)
{
	size_t index;
	struct packed_rrset_data* d;
	if(!raddr->data)
		return 1;
	d = raddr->data->entry.data;
	if(ntohs(raddr->data->rk.type) != rr_type) {
		return 0;
	}
	if(packed_rrset_find_rr(d, rdata, rdatalen, &index)) {
		if(d->count == 1) {
			/* regional alloc'd */
			raddr->data->entry.data = NULL; 
			raddr->data = NULL;
			return 1;
		}
		if(d->count > 1) {
			if(!local_rrset_remove_rr(d, index))
				return 0;
		}
	}
	return 0;

}

/** Remove RR from RPZ's local-zone */
static void
rpz_remove_qname_trigger(struct rpz* r, uint8_t* dname, size_t dnamelen,
	enum rpz_action a, uint16_t rr_type, uint16_t rr_class,
	uint8_t* rdatawl, size_t rdatalen)
{
	struct local_zone* z;
	int delete_zone = 1;
	z = rpz_find_zone(r, dname, dnamelen, rr_class,
		1 /* only exact */, 1 /* wr lock */, 1 /* keep lock*/);
	if(!z) {
		verbose(VERB_ALGO, "RPZ: cannot remove RR from IXFR, "
			"RPZ domain not found");
		return;
	}
	if(a == RPZ_LOCAL_DATA_ACTION)
		delete_zone = rpz_data_delete_rr(z, dname,
			dnamelen, rr_type, rdatawl, rdatalen);
	else if(a != localzone_type_to_rpz_action(z->type)) {
		lock_rw_unlock(&z->lock);
		lock_rw_unlock(&r->local_zones->lock);
		return;
	}
	lock_rw_unlock(&z->lock); 
	if(delete_zone) {
		local_zones_del_zone(r->local_zones, z);
	}
	lock_rw_unlock(&r->local_zones->lock); 
	return;
}

static void
rpz_remove_response_ip_trigger(struct rpz* r, uint8_t* dname, size_t dnamelen,
	enum rpz_action a, uint16_t rr_type, uint8_t* rdatawl, size_t rdatalen)
{
	struct resp_addr* node;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int net, af;
	int delete_respip = 1;

	if(!netblockdnametoaddr(dname, dnamelen, &addr, &addrlen, &net, &af))
		return;

	lock_rw_wrlock(&r->respip_set->lock);
	if(!(node = (struct resp_addr*)addr_tree_find(
		&r->respip_set->ip_tree, &addr, addrlen, net))) {
		verbose(VERB_ALGO, "RPZ: cannot remove RR from IXFR, "
			"RPZ domain not found");
		lock_rw_unlock(&r->respip_set->lock);
		return;
	}

	lock_rw_wrlock(&node->lock);
	if(a == RPZ_LOCAL_DATA_ACTION) {
		/* remove RR, signal whether RR can be removed */
		delete_respip = rpz_rrset_delete_rr(node, rr_type, rdatawl, 
			rdatalen);
	}
	lock_rw_unlock(&node->lock);
	if(delete_respip)
		respip_sockaddr_delete(r->respip_set, node);
	lock_rw_unlock(&r->respip_set->lock);
}

void
rpz_remove_rr(struct rpz* r, size_t aznamelen, uint8_t* dname, size_t dnamelen,
	uint16_t rr_type, uint16_t rr_class, uint8_t* rdatawl, size_t rdatalen)
{
	size_t policydnamelen;
	enum rpz_trigger t;
	enum rpz_action a;
	uint8_t* policydname;

	if(!(policydname = calloc(1, LDNS_MAX_DOMAINLEN + 1)))
		return;

	a = rpz_rr_to_action(rr_type, rdatawl, rdatalen);
	if(a == RPZ_INVALID_ACTION) {
		free(policydname);
		return;
	}
	if(!(policydnamelen = strip_dname_origin(dname, dnamelen, aznamelen,
		policydname, LDNS_MAX_DOMAINLEN + 1))) {
		free(policydname);
		return;
	}
	t = rpz_dname_to_trigger(policydname, policydnamelen);
	if(t == RPZ_QNAME_TRIGGER) {
		rpz_remove_qname_trigger(r, policydname, policydnamelen, a,
			rr_type, rr_class, rdatawl, rdatalen);
	} else if(t == RPZ_RESPONSE_IP_TRIGGER) {
		rpz_remove_response_ip_trigger(r, policydname, policydnamelen,
			a, rr_type, rdatawl, rdatalen);
	}
	free(policydname);
}

/** print log information for an applied RPZ policy. Based on local-zone's
 * lz_inform_print().
 */
static void
log_rpz_apply(uint8_t* dname, enum rpz_action a, struct query_info* qinfo,
	struct comm_reply* repinfo, char* log_name)
{
	char ip[128], txt[512];
	char dnamestr[LDNS_MAX_DOMAINLEN+1];
	uint16_t port = ntohs(((struct sockaddr_in*)&repinfo->addr)->sin_port);
	dname_str(dname, dnamestr);
	addr_to_str(&repinfo->addr, repinfo->addrlen, ip, sizeof(ip));
	if(log_name)
		snprintf(txt, sizeof(txt), "RPZ applied [%s] %s %s %s@%u",
			log_name, dnamestr, rpz_action_to_string(a), ip,
			(unsigned)port);
	else
		snprintf(txt, sizeof(txt), "RPZ applied %s %s %s@%u",
			dnamestr, rpz_action_to_string(a), ip, (unsigned)port);
	log_nametypeclass(0, txt, qinfo->qname, qinfo->qtype, qinfo->qclass);
}

int
rpz_apply_qname_trigger(struct auth_zones* az, struct module_env* env,
	struct query_info* qinfo, struct edns_data* edns, sldns_buffer* buf,
	struct regional* temp, struct comm_reply* repinfo,
	uint8_t* taglist, size_t taglen, struct ub_server_stats* stats)
{
	struct rpz* r = NULL;
	struct auth_zone* a;
	int ret;
	enum localzone_type lzt;
	struct local_zone* z = NULL;
	struct local_data* ld = NULL;
	lock_rw_rdlock(&az->rpz_lock);
	for(a = az->rpz_first; a; a = a->rpz_az_next) {
		lock_rw_rdlock(&a->lock);
		r = a->rpz;
		if(!r->taglist || taglist_intersect(r->taglist, 
			r->taglistlen, taglist, taglen)) {
			z = rpz_find_zone(r, qinfo->qname, qinfo->qname_len,
				qinfo->qclass, 0, 0, 0);
			if(z && r->action_override == RPZ_DISABLED_ACTION) {
				if(r->log)
					log_rpz_apply(z->name,
						r->action_override,
						qinfo, repinfo, r->log_name);
				/* TODO only register stats when stats_extended?
				 * */
				stats->rpz_action[r->action_override]++;
				lock_rw_unlock(&z->lock);
				z = NULL;
			}
			if(z)
				break;
		}
		lock_rw_unlock(&a->lock); /* not found in this auth_zone */
	}
	lock_rw_unlock(&az->rpz_lock);
	if(!z)
		return 0; /* not holding auth_zone.lock anymore */

	log_assert(r);
	if(r->action_override == RPZ_NO_OVERRIDE_ACTION)
		lzt = z->type;
	else
		lzt = rpz_action_to_localzone_type(r->action_override);

	if(r->action_override == RPZ_CNAME_OVERRIDE_ACTION) {
		qinfo->local_alias =
			regional_alloc_zero(temp, sizeof(struct local_rrset));
		if(!qinfo->local_alias) {
			lock_rw_unlock(&z->lock);
			lock_rw_unlock(&a->lock);
			return 0; /* out of memory */
		}
		qinfo->local_alias->rrset =
			regional_alloc_init(temp, r->cname_override,
				sizeof(*r->cname_override));
		if(!qinfo->local_alias->rrset) {
			lock_rw_unlock(&z->lock);
			lock_rw_unlock(&a->lock);
			return 0; /* out of memory */
		}
		qinfo->local_alias->rrset->rk.dname = qinfo->qname;
		qinfo->local_alias->rrset->rk.dname_len = qinfo->qname_len;
		if(r->log)
			log_rpz_apply(z->name, RPZ_CNAME_OVERRIDE_ACTION, 
				qinfo, repinfo, r->log_name);
		stats->rpz_action[RPZ_CNAME_OVERRIDE_ACTION]++;
		lock_rw_unlock(&z->lock);
		lock_rw_unlock(&a->lock);
		return 0;
	}

	if(lzt == local_zone_redirect && local_data_answer(z, env, qinfo,
		edns, repinfo, buf, temp, dname_count_labels(qinfo->qname),
		&ld, lzt, -1, NULL, 0, NULL, 0)) {
		if(r->log)
			log_rpz_apply(z->name,
				localzone_type_to_rpz_action(lzt), qinfo,
				repinfo, r->log_name);
		stats->rpz_action[localzone_type_to_rpz_action(lzt)]++;
		lock_rw_unlock(&z->lock);
		lock_rw_unlock(&a->lock);
		return !qinfo->local_alias;
	}

	ret = local_zones_zone_answer(z, env, qinfo, edns, repinfo, buf, temp,
		0 /* no local data used */, lzt);
	if(r->log)
		log_rpz_apply(z->name, localzone_type_to_rpz_action(lzt),
			qinfo, repinfo, r->log_name);
	stats->rpz_action[localzone_type_to_rpz_action(lzt)]++;
	lock_rw_unlock(&z->lock);
	lock_rw_unlock(&a->lock);

	return ret;
}
