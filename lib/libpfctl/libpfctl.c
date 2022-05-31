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

static int
pfctl_do_ioctl(int dev, uint cmd, size_t size, nvlist_t **nvl)
{
	struct pfioc_nv nv;
	void *data;
	size_t nvlen;
	int ret;

	data = nvlist_pack(*nvl, &nvlen);

retry:
	nv.data = malloc(size);
	memcpy(nv.data, data, nvlen);
	free(data);

	nv.len = nvlen;
	nv.size = size;

	ret = ioctl(dev, cmd, &nv);
	if (ret == -1 && errno == ENOSPC) {
		size *= 2;
		free(nv.data);
		goto retry;
	}

	nvlist_destroy(*nvl);
	*nvl = NULL;

	if (ret == 0) {
		*nvl = nvlist_unpack(nv.data, nv.len, 0);
		if (*nvl == NULL) {
			free(nv.data);
			return (EIO);
		}
	} else {
		ret = errno;
	}

	free(nv.data);

	return (ret);
}

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
	struct pfctl_status	*status;
	nvlist_t	*nvl;
	size_t		 len;
	const void	*chksum;

	status = calloc(1, sizeof(*status));
	if (status == NULL)
		return (NULL);

	nvl = nvlist_create(0);

	if (pfctl_do_ioctl(dev, DIOCGETSTATUSNV, 4096, &nvl)) {
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

	if (nvlist_exists_number(nvl, "timestamp")) {
		rule->last_active_timestamp = nvlist_get_number(nvl, "timestamp");
	}

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

static void
pfctl_nveth_addr_to_eth_addr(const nvlist_t *nvl, struct pfctl_eth_addr *addr)
{
	static const u_int8_t EMPTY_MAC[ETHER_ADDR_LEN] = { 0 };
	size_t len;
	const void *data;

	data = nvlist_get_binary(nvl, "addr", &len);
	assert(len == sizeof(addr->addr));
	memcpy(addr->addr, data, sizeof(addr->addr));

	data = nvlist_get_binary(nvl, "mask", &len);
	assert(len == sizeof(addr->mask));
	memcpy(addr->mask, data, sizeof(addr->mask));

	addr->neg = nvlist_get_bool(nvl, "neg");

	/* To make checks for 'is this address set?' easier. */
	addr->isset = memcmp(addr->addr, EMPTY_MAC, ETHER_ADDR_LEN) != 0;
}

static nvlist_t *
pfctl_eth_addr_to_nveth_addr(const struct pfctl_eth_addr *addr)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (NULL);

	nvlist_add_bool(nvl, "neg", addr->neg);
	nvlist_add_binary(nvl, "addr", &addr->addr, ETHER_ADDR_LEN);
	nvlist_add_binary(nvl, "mask", &addr->mask, ETHER_ADDR_LEN);

	return (nvl);
}

static void
pfctl_nveth_rule_to_eth_rule(const nvlist_t *nvl, struct pfctl_eth_rule *rule)
{
	rule->nr = nvlist_get_number(nvl, "nr");
	rule->quick = nvlist_get_bool(nvl, "quick");
	strlcpy(rule->ifname, nvlist_get_string(nvl, "ifname"), IFNAMSIZ);
	rule->ifnot = nvlist_get_bool(nvl, "ifnot");
	rule->direction = nvlist_get_number(nvl, "direction");
	rule->proto = nvlist_get_number(nvl, "proto");
	strlcpy(rule->match_tagname, nvlist_get_string(nvl, "match_tagname"),
	    PF_TAG_NAME_SIZE);
	rule->match_tag = nvlist_get_number(nvl, "match_tag");
	rule->match_tag_not = nvlist_get_bool(nvl, "match_tag_not");

	pfctl_nveth_addr_to_eth_addr(nvlist_get_nvlist(nvl, "src"),
	    &rule->src);
	pfctl_nveth_addr_to_eth_addr(nvlist_get_nvlist(nvl, "dst"),
	    &rule->dst);

	pf_nvrule_addr_to_rule_addr(nvlist_get_nvlist(nvl, "ipsrc"),
	    &rule->ipsrc);
	pf_nvrule_addr_to_rule_addr(nvlist_get_nvlist(nvl, "ipdst"),
	    &rule->ipdst);

	rule->evaluations = nvlist_get_number(nvl, "evaluations");
	rule->packets[0] = nvlist_get_number(nvl, "packets-in");
	rule->packets[1] = nvlist_get_number(nvl, "packets-out");
	rule->bytes[0] = nvlist_get_number(nvl, "bytes-in");
	rule->bytes[1] = nvlist_get_number(nvl, "bytes-out");

	if (nvlist_exists_number(nvl, "timestamp")) {
		rule->last_active_timestamp = nvlist_get_number(nvl, "timestamp");
	}

	strlcpy(rule->qname, nvlist_get_string(nvl, "qname"), PF_QNAME_SIZE);
	strlcpy(rule->tagname, nvlist_get_string(nvl, "tagname"),
	    PF_TAG_NAME_SIZE);

	rule->dnpipe = nvlist_get_number(nvl, "dnpipe");
	rule->dnflags = nvlist_get_number(nvl, "dnflags");

	rule->anchor_relative = nvlist_get_number(nvl, "anchor_relative");
	rule->anchor_wildcard = nvlist_get_number(nvl, "anchor_wildcard");

	rule->action = nvlist_get_number(nvl, "action");
}

int
pfctl_get_eth_rulesets_info(int dev, struct pfctl_eth_rulesets_info *ri,
    const char *path)
{
	nvlist_t *nvl;
	int ret;

	bzero(ri, sizeof(*ri));

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "path", path);

	if ((ret = pfctl_do_ioctl(dev, DIOCGETETHRULESETS, 256, &nvl)) != 0)
		return (ret);

	ri->nr = nvlist_get_number(nvl, "nr");

	nvlist_destroy(nvl);
	return (0);
}

int
pfctl_get_eth_ruleset(int dev, const char *path, int nr,
    struct pfctl_eth_ruleset_info *ri)
{
	nvlist_t *nvl;
	int ret;

	bzero(ri, sizeof(*ri));

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "path", path);
	nvlist_add_number(nvl, "nr", nr);

	if ((ret = pfctl_do_ioctl(dev, DIOCGETETHRULESET, 1024, &nvl)) != 0)
		return (ret);

	ri->nr = nvlist_get_number(nvl, "nr");
	strlcpy(ri->path, nvlist_get_string(nvl, "path"), MAXPATHLEN);
	strlcpy(ri->name, nvlist_get_string(nvl, "name"),
	    PF_ANCHOR_NAME_SIZE);

	return (0);
}

int
pfctl_get_eth_rules_info(int dev, struct pfctl_eth_rules_info *rules,
    const char *path)
{
	nvlist_t *nvl;
	int ret;

	bzero(rules, sizeof(*rules));

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "anchor", path);

	if ((ret = pfctl_do_ioctl(dev, DIOCGETETHRULES, 1024, &nvl)) != 0)
		return (ret);

	rules->nr = nvlist_get_number(nvl, "nr");
	rules->ticket = nvlist_get_number(nvl, "ticket");

	nvlist_destroy(nvl);
	return (0);
}

int
pfctl_get_eth_rule(int dev, uint32_t nr, uint32_t ticket,
    const char *path, struct pfctl_eth_rule *rule, bool clear,
    char *anchor_call)
{
	nvlist_t *nvl;
	int ret;

	nvl = nvlist_create(0);

	nvlist_add_string(nvl, "anchor", path);
	nvlist_add_number(nvl, "ticket", ticket);
	nvlist_add_number(nvl, "nr", nr);
	nvlist_add_bool(nvl, "clear", clear);

	if ((ret = pfctl_do_ioctl(dev, DIOCGETETHRULE, 4096, &nvl)) != 0)
		return (ret);

	pfctl_nveth_rule_to_eth_rule(nvl, rule);

	if (anchor_call)
		strlcpy(anchor_call, nvlist_get_string(nvl, "anchor_call"),
		    MAXPATHLEN);

	nvlist_destroy(nvl);
	return (0);
}

int
pfctl_add_eth_rule(int dev, const struct pfctl_eth_rule *r, const char *anchor,
    const char *anchor_call, uint32_t ticket)
{
	struct pfioc_nv nv;
	nvlist_t *nvl, *addr;
	void *packed;
	int error = 0;
	size_t size;

	nvl = nvlist_create(0);

	nvlist_add_number(nvl, "ticket", ticket);
	nvlist_add_string(nvl, "anchor", anchor);
	nvlist_add_string(nvl, "anchor_call", anchor_call);

	nvlist_add_number(nvl, "nr", r->nr);
	nvlist_add_bool(nvl, "quick", r->quick);
	nvlist_add_string(nvl, "ifname", r->ifname);
	nvlist_add_bool(nvl, "ifnot", r->ifnot);
	nvlist_add_number(nvl, "direction", r->direction);
	nvlist_add_number(nvl, "proto", r->proto);
	nvlist_add_string(nvl, "match_tagname", r->match_tagname);
	nvlist_add_bool(nvl, "match_tag_not", r->match_tag_not);

	addr = pfctl_eth_addr_to_nveth_addr(&r->src);
	if (addr == NULL) {
		nvlist_destroy(nvl);
		return (ENOMEM);
	}
	nvlist_add_nvlist(nvl, "src", addr);
	nvlist_destroy(addr);

	addr = pfctl_eth_addr_to_nveth_addr(&r->dst);
	if (addr == NULL) {
		nvlist_destroy(nvl);
		return (ENOMEM);
	}
	nvlist_add_nvlist(nvl, "dst", addr);
	nvlist_destroy(addr);

	pfctl_nv_add_rule_addr(nvl, "ipsrc", &r->ipsrc);
	pfctl_nv_add_rule_addr(nvl, "ipdst", &r->ipdst);

	nvlist_add_string(nvl, "qname", r->qname);
	nvlist_add_string(nvl, "tagname", r->tagname);
	nvlist_add_number(nvl, "dnpipe", r->dnpipe);
	nvlist_add_number(nvl, "dnflags", r->dnflags);

	nvlist_add_number(nvl, "action", r->action);

	packed = nvlist_pack(nvl, &size);
	if (packed == NULL) {
		nvlist_destroy(nvl);
		return (ENOMEM);
	}

	nv.len = size;
	nv.size = size;
	nv.data = packed;

	if (ioctl(dev, DIOCADDETHRULE, &nv) != 0)
		error = errno;

	free(packed);
	nvlist_destroy(nvl);

	return (error);
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
	if (ret == -1)
		ret = errno;

	free(nv.data);
	nvlist_destroy(nvl);

	return (ret);
}

int
pfctl_get_rules_info(int dev, struct pfctl_rules_info *rules, uint32_t ruleset,
    const char *path)
{
	struct pfioc_rule pr;
	int ret;

	bzero(&pr, sizeof(pr));
	if (strlcpy(pr.anchor, path, sizeof(pr.anchor)) >= sizeof(pr.anchor))
		return (E2BIG);

	pr.rule.action = ruleset;
	ret = ioctl(dev, DIOCGETRULES, &pr);
	if (ret != 0)
		return (ret);

	rules->nr = pr.nr;
	rules->ticket = pr.ticket;

	return (0);
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
	nvlist_t *nvl;
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

	if ((ret = pfctl_do_ioctl(dev, DIOCGETRULENV, 8192, &nvl)) != 0)
		return (ret);

	pf_nvrule_to_rule(nvlist_get_nvlist(nvl, "rule"), rule);

	if (anchor_call)
		strlcpy(anchor_call, nvlist_get_string(nvl, "anchor_call"),
		    MAXPATHLEN);

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
	nvlist_add_number(nv, "creatorid", htonl(cmp->creatorid));
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

	if ((ret = pfctl_do_ioctl(dev, ioctlval, 1024, &nvl)) != 0)
		return (ret);

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

int
pfctl_clear_rules(int dev, const char *anchorname)
{
	struct pfioc_trans trans;
	struct pfioc_trans_e transe[2];
	int ret;

	bzero(&trans, sizeof(trans));
	bzero(&transe, sizeof(transe));

	transe[0].rs_num = PF_RULESET_SCRUB;
	if (strlcpy(transe[0].anchor, anchorname, sizeof(transe[0].anchor))
	    >= sizeof(transe[0].anchor))
		return (E2BIG);

	transe[1].rs_num = PF_RULESET_FILTER;
	if (strlcpy(transe[1].anchor, anchorname, sizeof(transe[1].anchor))
	    >= sizeof(transe[1].anchor))
		return (E2BIG);

	trans.size = 2;
	trans.esize = sizeof(transe[0]);
	trans.array = transe;

	ret = ioctl(dev, DIOCXBEGIN, &trans);
	if (ret != 0)
		return (ret);
	return ioctl(dev, DIOCXCOMMIT, &trans);
}

int
pfctl_clear_nat(int dev, const char *anchorname)
{
	struct pfioc_trans trans;
	struct pfioc_trans_e transe[3];
	int ret;

	bzero(&trans, sizeof(trans));
	bzero(&transe, sizeof(transe));

	transe[0].rs_num = PF_RULESET_NAT;
	if (strlcpy(transe[0].anchor, anchorname, sizeof(transe[0].anchor))
	    >= sizeof(transe[0].anchor))
		return (E2BIG);

	transe[1].rs_num = PF_RULESET_BINAT;
	if (strlcpy(transe[1].anchor, anchorname, sizeof(transe[1].anchor))
	    >= sizeof(transe[0].anchor))
		return (E2BIG);

	transe[2].rs_num = PF_RULESET_RDR;
	if (strlcpy(transe[2].anchor, anchorname, sizeof(transe[2].anchor))
	    >= sizeof(transe[2].anchor))
		return (E2BIG);

	trans.size = 3;
	trans.esize = sizeof(transe[0]);
	trans.array = transe;

	ret = ioctl(dev, DIOCXBEGIN, &trans);
	if (ret != 0)
		return (ret);
	return ioctl(dev, DIOCXCOMMIT, &trans);
}
int
pfctl_clear_eth_rules(int dev, const char *anchorname)
{
	struct pfioc_trans trans;
	struct pfioc_trans_e transe;
	int ret;

	bzero(&trans, sizeof(trans));
	bzero(&transe, sizeof(transe));

	transe.rs_num = PF_RULESET_ETH;
	if (strlcpy(transe.anchor, anchorname, sizeof(transe.anchor))
	    >= sizeof(transe.anchor))
		return (E2BIG);

	trans.size = 1;
	trans.esize = sizeof(transe);
	trans.array = &transe;

	ret = ioctl(dev, DIOCXBEGIN, &trans);
	if (ret != 0)
		return (ret);
	return ioctl(dev, DIOCXCOMMIT, &trans);
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
	nvlist_t	*nvl;
	int		 ret;
	uint		 state_limit;
	bool		 enabled, adaptive;

	ret = pfctl_get_limit(dev, PF_LIMIT_STATES, &state_limit);
	if (ret != 0)
		return (ret);

	bzero(s, sizeof(*s));

	nvl = nvlist_create(0);

	if ((ret = pfctl_do_ioctl(dev, DIOCGETSYNCOOKIES, 256, &nvl)) != 0)
		return (errno);

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

int
pfctl_table_add_addrs(int dev, struct pfr_table *tbl, struct pfr_addr
    *addr, int size, int *nadd, int flags)
{
	struct pfioc_table io;

	if (tbl == NULL || size < 0 || (size && addr == NULL)) {
		return (EINVAL);
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_table = *tbl;
	io.pfrio_buffer = addr;
	io.pfrio_esize = sizeof(*addr);
	io.pfrio_size = size;

	if (ioctl(dev, DIOCRADDADDRS, &io))
		return (errno);
	if (nadd != NULL)
		*nadd = io.pfrio_nadd;
	return (0);
}

int
pfctl_table_del_addrs(int dev, struct pfr_table *tbl, struct pfr_addr
    *addr, int size, int *ndel, int flags)
{
	struct pfioc_table io;

	if (tbl == NULL || size < 0 || (size && addr == NULL)) {
		return (EINVAL);
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_table = *tbl;
	io.pfrio_buffer = addr;
	io.pfrio_esize = sizeof(*addr);
	io.pfrio_size = size;

	if (ioctl(dev, DIOCRDELADDRS, &io))
		return (errno);
	if (ndel != NULL)
		*ndel = io.pfrio_ndel;
	return (0);
}

int
pfctl_table_set_addrs(int dev, struct pfr_table *tbl, struct pfr_addr
    *addr, int size, int *size2, int *nadd, int *ndel, int *nchange, int flags)
{
	struct pfioc_table io;

	if (tbl == NULL || size < 0 || (size && addr == NULL)) {
		return (EINVAL);
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_table = *tbl;
	io.pfrio_buffer = addr;
	io.pfrio_esize = sizeof(*addr);
	io.pfrio_size = size;
	io.pfrio_size2 = (size2 != NULL) ? *size2 : 0;
	if (ioctl(dev, DIOCRSETADDRS, &io))
		return (-1);
	if (nadd != NULL)
		*nadd = io.pfrio_nadd;
	if (ndel != NULL)
		*ndel = io.pfrio_ndel;
	if (nchange != NULL)
		*nchange = io.pfrio_nchange;
	if (size2 != NULL)
		*size2 = io.pfrio_size2;
	return (0);
}

int pfctl_table_get_addrs(int dev, struct pfr_table *tbl, struct pfr_addr *addr,
    int *size, int flags)
{
	struct pfioc_table io;

	if (tbl == NULL || size == NULL || *size < 0 ||
	    (*size && addr == NULL)) {
		return (EINVAL);
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_table = *tbl;
	io.pfrio_buffer = addr;
	io.pfrio_esize = sizeof(*addr);
	io.pfrio_size = *size;
	if (ioctl(dev, DIOCRGETADDRS, &io))
		return (-1);
	*size = io.pfrio_size;
	return (0);
}
