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

#include "libpfctl.h"

const char* PFCTL_SYNCOOKIES_MODE_NAMES[] = {
	"never",
	"always",
	"adaptive"
};

static int	_pfctl_clear_states(int , const struct pfctl_kill *,
		    unsigned int *, uint64_t);

static void
pf_nvuint_8_array(const nvlist_t *nvl, const char *name, size_t maxelems,
    uint8_t *numbers, size_t *nelems)
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
    uint16_t *numbers, size_t *nelems)
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
    uint32_t *numbers, size_t *nelems)
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
    uint64_t *numbers, size_t *nelems)
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
_pfctl_get_status_counters(const nvlist_t *nvl,
    struct pfctl_status_counters *counters)
{
	const uint64_t		*ids, *counts;
	const char *const	*names;
	size_t id_len, counter_len, names_len;

	ids = nvlist_get_number_array(nvl, "ids", &id_len);
	counts = nvlist_get_number_array(nvl, "counters", &counter_len);
	names = nvlist_get_string_array(nvl, "names", &names_len);
	assert(id_len == counter_len);
	assert(counter_len == names_len);

	TAILQ_INIT(counters);

	for (size_t i = 0; i < id_len; i++) {
		struct pfctl_status_counter *c;

		c = malloc(sizeof(*c));

		c->id = ids[i];
		c->counter = counts[i];
		c->name = strdup(names[i]);

		TAILQ_INSERT_TAIL(counters, c, entry);
	}
}

struct pfctl_status *
pfctl_get_status(int dev)
{
	struct pfioc_nv	 nv;
	struct pfctl_status	*status;
	nvlist_t	*nvl;
	size_t		 len;
	const void	*chksum;

	status = calloc(1, sizeof(*status));
	if (status == NULL)
		return (NULL);

	nv.data = malloc(4096);
	nv.len = nv.size = 4096;

	if (ioctl(dev, DIOCGETSTATUSNV, &nv)) {
		free(nv.data);
		free(status);
		return (NULL);
	}

	nvl = nvlist_unpack(nv.data, nv.len, 0);
	free(nv.data);
	if (nvl == NULL) {
		free(status);
		return (NULL);
	}

	status->running = nvlist_get_bool(nvl, "running");
	status->since = nvlist_get_number(nvl, "since");
	status->debug = nvlist_get_number(nvl, "debug");
	status->hostid = ntohl(nvlist_get_number(nvl, "hostid"));
	status->states = nvlist_get_number(nvl, "states");
	status->src_nodes = nvlist_get_number(nvl, "src_nodes");

	strlcpy(status->ifname, nvlist_get_string(nvl, "ifname"),
	    IFNAMSIZ);
	chksum = nvlist_get_binary(nvl, "chksum", &len);
	assert(len == PF_MD5_DIGEST_LENGTH);
	memcpy(status->pf_chksum, chksum, len);

	_pfctl_get_status_counters(nvlist_get_nvlist(nvl, "counters"),
	    &status->counters);
	_pfctl_get_status_counters(nvlist_get_nvlist(nvl, "lcounters"),
	    &status->lcounters);
	_pfctl_get_status_counters(nvlist_get_nvlist(nvl, "fcounters"),
	    &status->fcounters);
	_pfctl_get_status_counters(nvlist_get_nvlist(nvl, "scounters"),
	    &status->scounters);

	pf_nvuint_64_array(nvl, "pcounters", 2 * 2 * 3,
	    (uint64_t *)status->pcounters, NULL);
	pf_nvuint_64_array(nvl, "bcounters", 2 * 2,
	    (uint64_t *)status->bcounters, NULL);

	nvlist_destroy(nvl);

	return (status);
}

void
pfctl_free_status(struct pfctl_status *status)
{
	struct pfctl_status_counter *c, *tmp;

	TAILQ_FOREACH_SAFE(c, &status->counters, entry, tmp) {
		free(c->name);
		free(c);
	}
	TAILQ_FOREACH_SAFE(c, &status->lcounters, entry, tmp) {
		free(c->name);
		free(c);
	}
	TAILQ_FOREACH_SAFE(c, &status->fcounters, entry, tmp) {
		free(c->name);
		free(c);
	}
	TAILQ_FOREACH_SAFE(c, &status->scounters, entry, tmp) {
		free(c->name);
		free(c);
	}

	free(status);
}

static void
pfctl_nv_add_addr(nvlist_t *nvparent, const char *name,
    const struct pf_addr *addr)
{
	nvlist_t *nvl = nvlist_create(0);

	nvlist_add_binary(nvl, "addr", addr, sizeof(*addr));

	nvlist_add_nvlist(nvparent, name, nvl);
	nvlist_destroy(nvl);
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
pfctl_nv_add_addr_wrap(nvlist_t *nvparent, const char *name,
    const struct pf_addr_wrap *addr)
{
	nvlist_t *nvl = nvlist_create(0);

	nvlist_add_number(nvl, "type", addr->type);
	nvlist_add_number(nvl, "iflags", addr->iflags);
	if (addr->type == PF_ADDR_DYNIFTL)
		nvlist_add_string(nvl, "ifname", addr->v.ifname);
	if (addr->type == PF_ADDR_TABLE)
		nvlist_add_string(nvl, "tblname", addr->v.tblname);
	pfctl_nv_add_addr(nvl, "addr", &addr->v.a.addr);
	pfctl_nv_add_addr(nvl, "mask", &addr->v.a.mask);

	nvlist_add_nvlist(nvparent, name, nvl);
	nvlist_destroy(nvl);
}

static void
pf_nvaddr_wrap_to_addr_wrap(const nvlist_t *nvl, struct pf_addr_wrap *addr)
{
	bzero(addr, sizeof(*addr));

	addr->type = nvlist_get_number(nvl, "type");
	addr->iflags = nvlist_get_number(nvl, "iflags");
	if (addr->type == PF_ADDR_DYNIFTL) {
		strlcpy(addr->v.ifname, nvlist_get_string(nvl, "ifname"),
		    IFNAMSIZ);
		addr->p.dyncnt = nvlist_get_number(nvl, "dyncnt");
	}
	if (addr->type == PF_ADDR_TABLE) {
		strlcpy(addr->v.tblname, nvlist_get_string(nvl, "tblname"),
		    PF_TABLE_NAME_SIZE);
		addr->p.tblcnt = nvlist_get_number(nvl, "tblcnt");
	}

	pf_nvaddr_to_addr(nvlist_get_nvlist(nvl, "addr"), &addr->v.a.addr);
	pf_nvaddr_to_addr(nvlist_get_nvlist(nvl, "mask"), &addr->v.a.mask);
}

static void
pfctl_nv_add_rule_addr(nvlist_t *nvparent, const char *name,
    const struct pf_rule_addr *addr)
{
	uint64_t ports[2];
	nvlist_t *nvl = nvlist_create(0);

	pfctl_nv_add_addr_wrap(nvl, "addr", &addr->addr);
	ports[0] = addr->port[0];
	ports[1] = addr->port[1];
	nvlist_add_number_array(nvl, "port", ports, 2);
	nvlist_add_number(nvl, "neg", addr->neg);
	nvlist_add_number(nvl, "port_op", addr->port_op);

	nvlist_add_nvlist(nvparent, name, nvl);
	nvlist_destroy(nvl);
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
pfctl_nv_add_mape(nvlist_t *nvparent, const char *name,
    const struct pf_mape_portset *mape)
{
	nvlist_t *nvl = nvlist_create(0);

	nvlist_add_number(nvl, "offset", mape->offset);
	nvlist_add_number(nvl, "psidlen", mape->psidlen);
	nvlist_add_number(nvl, "psid", mape->psid);
	nvlist_add_nvlist(nvparent, name, nvl);
	nvlist_destroy(nvl);
}

static void
pfctl_nv_add_pool(nvlist_t *nvparent, const char *name,
    const struct pfctl_pool *pool)
{
	uint64_t ports[2];
	nvlist_t *nvl = nvlist_create(0);

	nvlist_add_binary(nvl, "key", &pool->key, sizeof(pool->key));
	pfctl_nv_add_addr(nvl, "counter", &pool->counter);
	nvlist_add_number(nvl, "tblidx", pool->tblidx);

	ports[0] = pool->proxy_port[0];
	ports[1] = pool->proxy_port[1];
	nvlist_add_number_array(nvl, "proxy_port", ports, 2);
	nvlist_add_number(nvl, "opts", pool->opts);
	pfctl_nv_add_mape(nvl, "mape", &pool->mape);

	nvlist_add_nvlist(nvparent, name, nvl);
	nvlist_destroy(nvl);
}

static void
pf_nvmape_to_mape(const nvlist_t *nvl, struct pf_mape_portset *mape)
{
	mape->offset = nvlist_get_number(nvl, "offset");
	mape->psidlen = nvlist_get_number(nvl, "psidlen");
	mape->psid = nvlist_get_number(nvl, "psid");
}

static void
pf_nvpool_to_pool(const nvlist_t *nvl, struct pfctl_pool *pool)
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

	if (nvlist_exists_nvlist(nvl, "mape"))
		pf_nvmape_to_mape(nvlist_get_nvlist(nvl, "mape"), &pool->mape);
}

static void
pfctl_nv_add_uid(nvlist_t *nvparent, const char *name,
    const struct pf_rule_uid *uid)
{
	uint64_t uids[2];
	nvlist_t *nvl = nvlist_create(0);

	uids[0] = uid->uid[0];
	uids[1] = uid->uid[1];
	nvlist_add_number_array(nvl, "uid", uids, 2);
	nvlist_add_number(nvl, "op", uid->op);

	nvlist_add_nvlist(nvparent, name, nvl);
	nvlist_destroy(nvl);
}

static void
pf_nvrule_uid_to_rule_uid(const nvlist_t *nvl, struct pf_rule_uid *uid)
{
	pf_nvuint_32_array(nvl, "uid", 2, uid->uid, NULL);
	uid->op = nvlist_get_number(nvl, "op");
}

static void
pfctl_nv_add_divert(nvlist_t *nvparent, const char *name,
    const struct pfctl_rule *r)
{
	nvlist_t *nvl = nvlist_create(0);

	pfctl_nv_add_addr(nvl, "addr", &r->divert.addr);
	nvlist_add_number(nvl, "port", r->divert.port);

	nvlist_add_nvlist(nvparent, name, nvl);
	nvlist_destroy(nvl);
}

static void
pf_nvdivert_to_divert(const nvlist_t *nvl, struct pfctl_rule *rule)
{
	pf_nvaddr_to_addr(nvlist_get_nvlist(nvl, "addr"), &rule->divert.addr);
	rule->divert.port = nvlist_get_number(nvl, "port");
}

static void
pf_nvrule_to_rule(const nvlist_t *nvl, struct pfctl_rule *rule)
{
	const uint64_t *skip;
	const char *const *labels;
	size_t skipcount, labelcount;

	rule->nr = nvlist_get_number(nvl, "nr");

	pf_nvrule_addr_to_rule_addr(nvlist_get_nvlist(nvl, "src"), &rule->src);
	pf_nvrule_addr_to_rule_addr(nvlist_get_nvlist(nvl, "dst"), &rule->dst);

	skip = nvlist_get_number_array(nvl, "skip", &skipcount);
	assert(skip);
	assert(skipcount == PF_SKIP_COUNT);
	for (int i = 0; i < PF_SKIP_COUNT; i++)
		rule->skip[i].nr = skip[i];

	labels = nvlist_get_string_array(nvl, "labels", &labelcount);
	assert(labelcount <= PF_RULE_MAX_LABEL_COUNT);
	for (size_t i = 0; i < labelcount; i++)
		strlcpy(rule->label[i], labels[i], PF_RULE_LABEL_SIZE);
	rule->ridentifier = nvlist_get_number(nvl, "ridentifier");
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
	rule->dnpipe = nvlist_get_number(nvl, "dnpipe");
	rule->dnrpipe = nvlist_get_number(nvl, "dnrpipe");
	rule->free_flags = nvlist_get_number(nvl, "dnflags");
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

	rule->states_cur = nvlist_get_number(nvl, "states_cur");
	rule->states_tot = nvlist_get_number(nvl, "states_tot");
	rule->src_nodes = nvlist_get_number(nvl, "src_nodes");
}

int
pfctl_add_rule(int dev, const struct pfctl_rule *r, const char *anchor,
    const char *anchor_call, uint32_t ticket, uint32_t pool_ticket)
{
	struct pfioc_nv nv;
	uint64_t timeouts[PFTM_MAX];
	uint64_t set_prio[2];
	nvlist_t *nvl, *nvlr;
	size_t labelcount;
	int ret;

	nvl = nvlist_create(0);
	nvlr = nvlist_create(0);

	nvlist_add_number(nvl, "ticket", ticket);
	nvlist_add_number(nvl, "pool_ticket", pool_ticket);
	nvlist_add_string(nvl, "anchor", anchor);
	nvlist_add_string(nvl, "anchor_call", anchor_call);

	nvlist_add_number(nvlr, "nr", r->nr);
	pfctl_nv_add_rule_addr(nvlr, "src", &r->src);
	pfctl_nv_add_rule_addr(nvlr, "dst", &r->dst);

	labelcount = 0;
	while (r->label[labelcount][0] != 0 &&
	    labelcount < PF_RULE_MAX_LABEL_COUNT) {
		nvlist_append_string_array(nvlr, "labels",
		    r->label[labelcount]);
		labelcount++;
	}
	nvlist_add_number(nvlr, "ridentifier", r->ridentifier);

	nvlist_add_string(nvlr, "ifname", r->ifname);
	nvlist_add_string(nvlr, "qname", r->qname);
	nvlist_add_string(nvlr, "pqname", r->pqname);
	nvlist_add_string(nvlr, "tagname", r->tagname);
	nvlist_add_string(nvlr, "match_tagname", r->match_tagname);
	nvlist_add_string(nvlr, "overload_tblname", r->overload_tblname);

	pfctl_nv_add_pool(nvlr, "rpool", &r->rpool);

	nvlist_add_number(nvlr, "os_fingerprint", r->os_fingerprint);

	nvlist_add_number(nvlr, "rtableid", r->rtableid);
	for (int i = 0; i < PFTM_MAX; i++)
		timeouts[i] = r->timeout[i];
	nvlist_add_number_array(nvlr, "timeout", timeouts, PFTM_MAX);
	nvlist_add_number(nvlr, "max_states", r->max_states);
	nvlist_add_number(nvlr, "max_src_nodes", r->max_src_nodes);
	nvlist_add_number(nvlr, "max_src_states", r->max_src_states);
	nvlist_add_number(nvlr, "max_src_conn", r->max_src_conn);
	nvlist_add_number(nvlr, "max_src_conn_rate.limit",
	    r->max_src_conn_rate.limit);
	nvlist_add_number(nvlr, "max_src_conn_rate.seconds",
	    r->max_src_conn_rate.seconds);
	nvlist_add_number(nvlr, "dnpipe", r->dnpipe);
	nvlist_add_number(nvlr, "dnrpipe", r->dnrpipe);
	nvlist_add_number(nvlr, "dnflags", r->free_flags);
	nvlist_add_number(nvlr, "prob", r->prob);
	nvlist_add_number(nvlr, "cuid", r->cuid);
	nvlist_add_number(nvlr, "cpid", r->cpid);

	nvlist_add_number(nvlr, "return_icmp", r->return_icmp);
	nvlist_add_number(nvlr, "return_icmp6", r->return_icmp6);

	nvlist_add_number(nvlr, "max_mss", r->max_mss);
	nvlist_add_number(nvlr, "scrub_flags", r->scrub_flags);

	pfctl_nv_add_uid(nvlr, "uid", &r->uid);
	pfctl_nv_add_uid(nvlr, "gid", (const struct pf_rule_uid *)&r->gid);

	nvlist_add_number(nvlr, "rule_flag", r->rule_flag);
	nvlist_add_number(nvlr, "action", r->action);
	nvlist_add_number(nvlr, "direction", r->direction);
	nvlist_add_number(nvlr, "log", r->log);
	nvlist_add_number(nvlr, "logif", r->logif);
	nvlist_add_number(nvlr, "quick", r->quick);
	nvlist_add_number(nvlr, "ifnot", r->ifnot);
	nvlist_add_number(nvlr, "match_tag_not", r->match_tag_not);
	nvlist_add_number(nvlr, "natpass", r->natpass);

	nvlist_add_number(nvlr, "keep_state", r->keep_state);
	nvlist_add_number(nvlr, "af", r->af);
	nvlist_add_number(nvlr, "proto", r->proto);
	nvlist_add_number(nvlr, "type", r->type);
	nvlist_add_number(nvlr, "code", r->code);
	nvlist_add_number(nvlr, "flags", r->flags);
	nvlist_add_number(nvlr, "flagset", r->flagset);
	nvlist_add_number(nvlr, "min_ttl", r->min_ttl);
	nvlist_add_number(nvlr, "allow_opts", r->allow_opts);
	nvlist_add_number(nvlr, "rt", r->rt);
	nvlist_add_number(nvlr, "return_ttl", r->return_ttl);
	nvlist_add_number(nvlr, "tos", r->tos);
	nvlist_add_number(nvlr, "set_tos", r->set_tos);
	nvlist_add_number(nvlr, "anchor_relative", r->anchor_relative);
	nvlist_add_number(nvlr, "anchor_wildcard", r->anchor_wildcard);

	nvlist_add_number(nvlr, "flush", r->flush);

	nvlist_add_number(nvlr, "prio", r->prio);
	set_prio[0] = r->set_prio[0];
	set_prio[1] = r->set_prio[1];
	nvlist_add_number_array(nvlr, "set_prio", set_prio, 2);

	pfctl_nv_add_divert(nvlr, "divert", r);

	nvlist_add_nvlist(nvl, "rule", nvlr);
	nvlist_destroy(nvlr);

	/* Now do the call. */
	nv.data = nvlist_pack(nvl, &nv.len);
	nv.size = nv.len;

	ret = ioctl(dev, DIOCADDRULENV, &nv);

	free(nv.data);
	nvlist_destroy(nvl);

	return (ret);
}

int
pfctl_get_rule(int dev, uint32_t nr, uint32_t ticket, const char *anchor,
    uint32_t ruleset, struct pfctl_rule *rule, char *anchor_call)
{
	return (pfctl_get_clear_rule(dev, nr, ticket, anchor, ruleset, rule,
	    anchor_call, false));
}

int	pfctl_get_clear_rule(int dev, uint32_t nr, uint32_t ticket,
	    const char *anchor, uint32_t ruleset, struct pfctl_rule *rule,
	    char *anchor_call, bool clear)
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

	if (clear)
		nvlist_add_bool(nvl, "clear_counter", true);

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

int
pfctl_set_keepcounters(int dev, bool keep)
{
	struct pfioc_nv	 nv;
	nvlist_t	*nvl;
	int		 ret;

	nvl = nvlist_create(0);

	nvlist_add_bool(nvl, "keep_counters", keep);

	nv.data = nvlist_pack(nvl, &nv.len);
	nv.size = nv.len;

	nvlist_destroy(nvl);

	ret = ioctl(dev, DIOCKEEPCOUNTERS, &nv);

	free(nv.data);
	return (ret);
}

static void
pfctl_nv_add_state_cmp(nvlist_t *nvl, const char *name,
    const struct pfctl_state_cmp *cmp)
{
	nvlist_t	*nv;

	nv = nvlist_create(0);

	nvlist_add_number(nv, "id", cmp->id);
	nvlist_add_number(nv, "creatorid", cmp->creatorid);
	nvlist_add_number(nv, "direction", cmp->direction);

	nvlist_add_nvlist(nvl, name, nv);
	nvlist_destroy(nv);
}

static void
pf_state_key_export_to_state_key(struct pfctl_state_key *ps,
    const struct pf_state_key_export *s)
{
	bcopy(s->addr, ps->addr, sizeof(ps->addr[0]) * 2);
	ps->port[0] = s->port[0];
	ps->port[1] = s->port[1];
}

static void
pf_state_peer_export_to_state_peer(struct pfctl_state_peer *ps,
    const struct pf_state_peer_export *s)
{
	/* Ignore scrub. */
	ps->seqlo = s->seqlo;
	ps->seqhi = s->seqhi;
	ps->seqdiff = s->seqdiff;
	/* Ignore max_win & mss */
	ps->state = s->state;
	ps->wscale = s->wscale;
}

static void
pf_state_export_to_state(struct pfctl_state *ps, const struct pf_state_export *s)
{
	assert(s->version >= PF_STATE_VERSION);

	ps->id = s->id;
	strlcpy(ps->ifname, s->ifname, sizeof(ps->ifname));
	strlcpy(ps->orig_ifname, s->orig_ifname, sizeof(ps->orig_ifname));
	pf_state_key_export_to_state_key(&ps->key[0], &s->key[0]);
	pf_state_key_export_to_state_key(&ps->key[1], &s->key[1]);
	pf_state_peer_export_to_state_peer(&ps->src, &s->src);
	pf_state_peer_export_to_state_peer(&ps->dst, &s->dst);
	bcopy(&s->rt_addr, &ps->rt_addr, sizeof(ps->rt_addr));
	ps->rule = ntohl(s->rule);
	ps->anchor = ntohl(s->anchor);
	ps->nat_rule = ntohl(s->nat_rule);
	ps->creation = ntohl(s->creation);
	ps->expire = ntohl(s->expire);
	ps->packets[0] = s->packets[0];
	ps->packets[1] = s->packets[1];
	ps->bytes[0] = s->bytes[0];
	ps->bytes[1] = s->bytes[1];
	ps->creatorid = ntohl(s->creatorid);
	ps->key[0].proto = s->proto;
	ps->key[1].proto = s->proto;
	ps->key[0].af = s->af;
	ps->key[1].af = s->af;
	ps->direction = s->direction;
	ps->state_flags = s->state_flags;
	ps->sync_flags = s->sync_flags;
}

int
pfctl_get_states(int dev, struct pfctl_states *states)
{
	struct pfioc_states_v2 ps;
	struct pf_state_export *p;
	char *inbuf = NULL, *newinbuf = NULL;
	unsigned int len = 0;
	int i, error;

	bzero(&ps, sizeof(ps));
	ps.ps_req_version = PF_STATE_VERSION;

	bzero(states, sizeof(*states));
	TAILQ_INIT(&states->states);

	for (;;) {
		ps.ps_len = len;
		if (len) {
			newinbuf = realloc(inbuf, len);
			if (newinbuf == NULL)
				return (ENOMEM);
			ps.ps_buf = inbuf = newinbuf;
		}
		if ((error = ioctl(dev, DIOCGETSTATESV2, &ps)) < 0) {
			free(inbuf);
			return (error);
		}
		if (ps.ps_len + sizeof(struct pfioc_states_v2) < len)
			break;
		if (len == 0 && ps.ps_len == 0)
			goto out;
		if (len == 0 && ps.ps_len != 0)
			len = ps.ps_len;
		if (ps.ps_len == 0)
			goto out;      /* no states */
		len *= 2;
	}
	p = ps.ps_states;

	for (i = 0; i < ps.ps_len; i += sizeof(*p), p++) {
		struct pfctl_state *s = malloc(sizeof(*s));
		if (s == NULL) {
			pfctl_free_states(states);
			error = ENOMEM;
			goto out;
		}

		pf_state_export_to_state(s, p);
		TAILQ_INSERT_TAIL(&states->states, s, entry);
	}

out:
	free(inbuf);
	return (error);
}

void
pfctl_free_states(struct pfctl_states *states)
{
	struct pfctl_state *s, *tmp;

	TAILQ_FOREACH_SAFE(s, &states->states, entry, tmp) {
		free(s);
	}

	bzero(states, sizeof(*states));
}

static int
_pfctl_clear_states(int dev, const struct pfctl_kill *kill,
    unsigned int *killed, uint64_t ioctlval)
{
	struct pfioc_nv	 nv;
	nvlist_t	*nvl;
	int		 ret;

	nvl = nvlist_create(0);

	pfctl_nv_add_state_cmp(nvl, "cmp", &kill->cmp);
	nvlist_add_number(nvl, "af", kill->af);
	nvlist_add_number(nvl, "proto", kill->proto);
	pfctl_nv_add_rule_addr(nvl, "src", &kill->src);
	pfctl_nv_add_rule_addr(nvl, "dst", &kill->dst);
	pfctl_nv_add_rule_addr(nvl, "rt_addr", &kill->rt_addr);
	nvlist_add_string(nvl, "ifname", kill->ifname);
	nvlist_add_string(nvl, "label", kill->label);
	nvlist_add_bool(nvl, "kill_match", kill->kill_match);

	nv.data = nvlist_pack(nvl, &nv.len);
	nv.size = nv.len;
	nvlist_destroy(nvl);
	nvl = NULL;

	ret = ioctl(dev, ioctlval, &nv);
	if (ret != 0) {
		free(nv.data);
		return (ret);
	}

	nvl = nvlist_unpack(nv.data, nv.len, 0);
	if (nvl == NULL) {
		free(nv.data);
		return (EIO);
	}

	if (killed)
		*killed = nvlist_get_number(nvl, "killed");

	nvlist_destroy(nvl);
	free(nv.data);

	return (ret);
}

int
pfctl_clear_states(int dev, const struct pfctl_kill *kill,
    unsigned int *killed)
{
	return (_pfctl_clear_states(dev, kill, killed, DIOCCLRSTATESNV));
}

int
pfctl_kill_states(int dev, const struct pfctl_kill *kill, unsigned int *killed)
{
	return (_pfctl_clear_states(dev, kill, killed, DIOCKILLSTATESNV));
}

static int
pfctl_get_limit(int dev, const int index, uint *limit)
{
	struct pfioc_limit pl;

	bzero(&pl, sizeof(pl));
	pl.index = index;

	if (ioctl(dev, DIOCGETLIMIT, &pl) == -1)
		return (errno);

	*limit = pl.limit;

	return (0);
}

int
pfctl_set_syncookies(int dev, const struct pfctl_syncookies *s)
{
	struct pfioc_nv	 nv;
	nvlist_t	*nvl;
	int		 ret;
	uint		 state_limit;

	ret = pfctl_get_limit(dev, PF_LIMIT_STATES, &state_limit);
	if (ret != 0)
		return (ret);

	nvl = nvlist_create(0);

	nvlist_add_bool(nvl, "enabled", s->mode != PFCTL_SYNCOOKIES_NEVER);
	nvlist_add_bool(nvl, "adaptive", s->mode == PFCTL_SYNCOOKIES_ADAPTIVE);
	nvlist_add_number(nvl, "highwater", state_limit * s->highwater / 100);
	nvlist_add_number(nvl, "lowwater", state_limit * s->lowwater / 100);

	nv.data = nvlist_pack(nvl, &nv.len);
	nv.size = nv.len;
	nvlist_destroy(nvl);
	nvl = NULL;

	ret = ioctl(dev, DIOCSETSYNCOOKIES, &nv);

	free(nv.data);
	return (ret);
}

int
pfctl_get_syncookies(int dev, struct pfctl_syncookies *s)
{
	struct pfioc_nv	 nv;
	nvlist_t	*nvl;
	int		 ret;
	uint		 state_limit;
	bool		 enabled, adaptive;

	ret = pfctl_get_limit(dev, PF_LIMIT_STATES, &state_limit);
	if (ret != 0)
		return (ret);

	bzero(s, sizeof(*s));

	nv.data = malloc(256);
	nv.len = nv.size = 256;

	if (ioctl(dev, DIOCGETSYNCOOKIES, &nv)) {
		free(nv.data);
		return (errno);
	}

	nvl = nvlist_unpack(nv.data, nv.len, 0);
	free(nv.data);
	if (nvl == NULL) {
		return (EIO);
	}

	enabled = nvlist_get_bool(nvl, "enabled");
	adaptive = nvlist_get_bool(nvl, "adaptive");

	if (enabled) {
		if (adaptive)
			s->mode = PFCTL_SYNCOOKIES_ADAPTIVE;
		else
			s->mode = PFCTL_SYNCOOKIES_ALWAYS;
	} else {
		s->mode = PFCTL_SYNCOOKIES_NEVER;
	}

	s->highwater = nvlist_get_number(nvl, "highwater") * 100 / state_limit;
	s->lowwater = nvlist_get_number(nvl, "lowwater") * 100 / state_limit;

	nvlist_destroy(nvl);

	return (0);
}
