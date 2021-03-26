/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Rubicon Communications, LLC (Netgate)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>

#include <sys/ioctl.h>
#include <sys/nv.h>
#include <sys/queue.h>
#include <sys/types.h>

#include <net/if.h>
#include <net/pfvar.h>
#include <netinet/in.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "pfctl_ioctl.h"

static void
pf_nvuint_8_array(const nvlist_t *nvl, const char *name, size_t maxelems,
    u_int8_t *numbers, size_t *nelems)
{
	const uint64_t *tmp;
	size_t elems;

	tmp = nvlist_get_number_array(nvl, name, &elems);
	assert(elems <= maxelems);

	for (size_t i = 0; i < elems; i++)
		numbers[i] = tmp[i];

	if (nelems)
		*nelems = elems;
}

static void
pf_nvuint_16_array(const nvlist_t *nvl, const char *name, size_t maxelems,
    u_int16_t *numbers, size_t *nelems)
{
	const uint64_t *tmp;
	size_t elems;

	tmp = nvlist_get_number_array(nvl, name, &elems);
	assert(elems <= maxelems);

	for (size_t i = 0; i < elems; i++)
		numbers[i] = tmp[i];

	if (nelems)
		*nelems = elems;
}

static void
pf_nvuint_32_array(const nvlist_t *nvl, const char *name, size_t maxelems,
    u_int32_t *numbers, size_t *nelems)
{
	const uint64_t *tmp;
	size_t elems;

	tmp = nvlist_get_number_array(nvl, name, &elems);
	assert(elems <= maxelems);

	for (size_t i = 0; i < elems; i++)
		numbers[i] = tmp[i];

	if (nelems)
		*nelems = elems;
}

static void
pf_nvuint_64_array(const nvlist_t *nvl, const char *name, size_t maxelems,
    u_int64_t *numbers, size_t *nelems)
{
	const uint64_t *tmp;
	size_t elems;

	tmp = nvlist_get_number_array(nvl, name, &elems);
	assert(elems <= maxelems);

	for (size_t i = 0; i < elems; i++)
		numbers[i] = tmp[i];

	if (nelems)
		*nelems = elems;
}

static void
pf_nvaddr_to_addr(const nvlist_t *nvl, struct pf_addr *addr)
{
	size_t len;
	const void *data;

	data = nvlist_get_binary(nvl, "addr", &len);
	assert(len == sizeof(struct pf_addr));
	memcpy(addr, data, len);
}

static void
pf_nvaddr_wrap_to_addr_wrap(const nvlist_t *nvl, struct pf_addr_wrap *addr)
{
	addr->type = nvlist_get_number(nvl, "type");
	addr->iflags = nvlist_get_number(nvl, "iflags");
	strlcpy(addr->v.ifname, nvlist_get_string(nvl, "ifname"), IFNAMSIZ);
	strlcpy(addr->v.tblname, nvlist_get_string(nvl, "tblname"),
	    PF_TABLE_NAME_SIZE);

	pf_nvaddr_to_addr(nvlist_get_nvlist(nvl, "addr"), &addr->v.a.addr);
	pf_nvaddr_to_addr(nvlist_get_nvlist(nvl, "mask"), &addr->v.a.mask);
}

static void
pf_nvrule_addr_to_rule_addr(const nvlist_t *nvl, struct pf_rule_addr *addr)
{
	pf_nvaddr_wrap_to_addr_wrap(nvlist_get_nvlist(nvl, "addr"), &addr->addr);

	pf_nvuint_16_array(nvl, "port", 2, addr->port, NULL);
	addr->neg = nvlist_get_number(nvl, "neg");
	addr->port_op = nvlist_get_number(nvl, "port_op");
}

static void
pf_nvpool_to_pool(const nvlist_t *nvl, struct pf_pool *pool)
{
	size_t len;
	const void *data;

	data = nvlist_get_binary(nvl, "key", &len);
	assert(len == sizeof(pool->key));
	memcpy(&pool->key, data, len);

	pf_nvaddr_to_addr(nvlist_get_nvlist(nvl, "counter"), &pool->counter);

	pool->tblidx = nvlist_get_number(nvl, "tblidx");
	pf_nvuint_16_array(nvl, "proxy_port", 2, pool->proxy_port, NULL);
	pool->opts = nvlist_get_number(nvl, "opts");
}

static void
pf_nvrule_uid_to_rule_uid(const nvlist_t *nvl, struct pf_rule_uid *uid)
{
	pf_nvuint_32_array(nvl, "uid", 2, uid->uid, NULL);
	uid->op = nvlist_get_number(nvl, "op");
}

static void
pf_nvdivert_to_divert(const nvlist_t *nvl, struct pf_rule *rule)
{
	pf_nvaddr_to_addr(nvlist_get_nvlist(nvl, "addr"), &rule->divert.addr);
	rule->divert.port = nvlist_get_number(nvl, "port");
}

static void
pf_nvrule_to_rule(const nvlist_t *nvl, struct pf_rule *rule)
{
	const uint64_t *skip;
	size_t skipcount;

	rule->nr = nvlist_get_number(nvl, "nr");

	pf_nvrule_addr_to_rule_addr(nvlist_get_nvlist(nvl, "src"), &rule->src);
	pf_nvrule_addr_to_rule_addr(nvlist_get_nvlist(nvl, "dst"), &rule->dst);

	skip = nvlist_get_number_array(nvl, "skip", &skipcount);
	assert(skip);
	assert(skipcount == PF_SKIP_COUNT);
	for (int i = 0; i < PF_SKIP_COUNT; i++)
		rule->skip[i].nr = skip[i];

	strlcpy(rule->label, nvlist_get_string(nvl, "label"), PF_RULE_LABEL_SIZE);
	strlcpy(rule->ifname, nvlist_get_string(nvl, "ifname"), IFNAMSIZ);
	strlcpy(rule->qname, nvlist_get_string(nvl, "qname"), PF_QNAME_SIZE);
	strlcpy(rule->pqname, nvlist_get_string(nvl, "pqname"), PF_QNAME_SIZE);
	strlcpy(rule->tagname, nvlist_get_string(nvl, "tagname"),
	    PF_TAG_NAME_SIZE);
	strlcpy(rule->match_tagname, nvlist_get_string(nvl, "match_tagname"),
	    PF_TAG_NAME_SIZE);

	strlcpy(rule->overload_tblname, nvlist_get_string(nvl, "overload_tblname"),
	    PF_TABLE_NAME_SIZE);

	pf_nvpool_to_pool(nvlist_get_nvlist(nvl, "rpool"), &rule->rpool);

	rule->evaluations = nvlist_get_number(nvl, "evaluations");
	pf_nvuint_64_array(nvl, "packets", 2, rule->packets, NULL);
	pf_nvuint_64_array(nvl, "bytes", 2, rule->bytes, NULL);

	rule->os_fingerprint = nvlist_get_number(nvl, "os_fingerprint");

	rule->rtableid = nvlist_get_number(nvl, "rtableid");
	pf_nvuint_32_array(nvl, "timeout", PFTM_MAX, rule->timeout, NULL);
	rule->max_states = nvlist_get_number(nvl, "max_states");
	rule->max_src_nodes = nvlist_get_number(nvl, "max_src_nodes");
	rule->max_src_states = nvlist_get_number(nvl, "max_src_states");
	rule->max_src_conn = nvlist_get_number(nvl, "max_src_conn");
	rule->max_src_conn_rate.limit =
	    nvlist_get_number(nvl, "max_src_conn_rate.limit");
	rule->max_src_conn_rate.seconds =
	    nvlist_get_number(nvl, "max_src_conn_rate.seconds");
	rule->qid = nvlist_get_number(nvl, "qid");
	rule->pqid = nvlist_get_number(nvl, "pqid");
	rule->prob = nvlist_get_number(nvl, "prob");
	rule->cuid = nvlist_get_number(nvl, "cuid");
	rule->cpid = nvlist_get_number(nvl, "cpid");

	rule->return_icmp = nvlist_get_number(nvl, "return_icmp");
	rule->return_icmp6 = nvlist_get_number(nvl, "return_icmp6");
	rule->max_mss = nvlist_get_number(nvl, "max_mss");
	rule->scrub_flags = nvlist_get_number(nvl, "scrub_flags");

	pf_nvrule_uid_to_rule_uid(nvlist_get_nvlist(nvl, "uid"), &rule->uid);
	pf_nvrule_uid_to_rule_uid(nvlist_get_nvlist(nvl, "gid"),
	    (struct pf_rule_uid *)&rule->gid);

	rule->rule_flag = nvlist_get_number(nvl, "rule_flag");
	rule->action = nvlist_get_number(nvl, "action");
	rule->direction = nvlist_get_number(nvl, "direction");
	rule->log = nvlist_get_number(nvl, "log");
	rule->logif = nvlist_get_number(nvl, "logif");
	rule->quick = nvlist_get_number(nvl, "quick");
	rule->ifnot = nvlist_get_number(nvl, "ifnot");
	rule->match_tag_not = nvlist_get_number(nvl, "match_tag_not");
	rule->natpass = nvlist_get_number(nvl, "natpass");

	rule->keep_state = nvlist_get_number(nvl, "keep_state");
	rule->af = nvlist_get_number(nvl, "af");
	rule->proto = nvlist_get_number(nvl, "proto");
	rule->type = nvlist_get_number(nvl, "type");
	rule->code = nvlist_get_number(nvl, "code");
	rule->flags = nvlist_get_number(nvl, "flags");
	rule->flagset = nvlist_get_number(nvl, "flagset");
	rule->min_ttl = nvlist_get_number(nvl, "min_ttl");
	rule->allow_opts = nvlist_get_number(nvl, "allow_opts");
	rule->rt = nvlist_get_number(nvl, "rt");
	rule->return_ttl  = nvlist_get_number(nvl, "return_ttl");
	rule->tos = nvlist_get_number(nvl, "tos");
	rule->set_tos = nvlist_get_number(nvl, "set_tos");
	rule->anchor_relative = nvlist_get_number(nvl, "anchor_relative");
	rule->anchor_wildcard = nvlist_get_number(nvl, "anchor_wildcard");

	rule->flush = nvlist_get_number(nvl, "flush");
	rule->prio = nvlist_get_number(nvl, "prio");
	pf_nvuint_8_array(nvl, "set_prio", 2, rule->set_prio, NULL);

	pf_nvdivert_to_divert(nvlist_get_nvlist(nvl, "divert"), rule);

	rule->u_states_cur = nvlist_get_number(nvl, "states_cur");
	rule->u_states_tot = nvlist_get_number(nvl, "states_tot");
	rule->u_src_nodes = nvlist_get_number(nvl, "src_nodes");
}


int
pfctl_get_rule(int dev, u_int32_t nr, u_int32_t ticket, const char *anchor,
    u_int32_t ruleset, struct pf_rule *rule, char *anchor_call)
{
	struct pfioc_nv nv;
	nvlist_t *nvl;
	void *nvlpacked;
	int ret;

	nvl = nvlist_create(0);
	if (nvl == 0)
		return (ENOMEM);

	nvlist_add_number(nvl, "nr", nr);
	nvlist_add_number(nvl, "ticket", ticket);
	nvlist_add_string(nvl, "anchor", anchor);
	nvlist_add_number(nvl, "ruleset", ruleset);

	nvlpacked = nvlist_pack(nvl, &nv.len);
	if (nvlpacked == NULL) {
		nvlist_destroy(nvl);
		return (ENOMEM);
	}
	nv.data = malloc(8182);
	nv.size = 8192;
	assert(nv.len <= nv.size);
	memcpy(nv.data, nvlpacked, nv.len);
	nvlist_destroy(nvl);
	nvl = NULL;
	free(nvlpacked);

	ret = ioctl(dev, DIOCGETRULENV, &nv);
	if (ret != 0) {
		free(nv.data);
		return (ret);
	}

	nvl = nvlist_unpack(nv.data, nv.len, 0);
	if (nvl == NULL) {
		free(nv.data);
		return (EIO);
	}

	pf_nvrule_to_rule(nvlist_get_nvlist(nvl, "rule"), rule);

	if (anchor_call)
		strlcpy(anchor_call, nvlist_get_string(nvl, "anchor_call"),
		    MAXPATHLEN);

	free(nv.data);
	nvlist_destroy(nvl);

	return (0);
}
