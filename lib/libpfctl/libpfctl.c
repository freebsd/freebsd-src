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
 */

#include <sys/cdefs.h>

#include <sys/ioctl.h>
#include <sys/nv.h>
#include <sys/queue.h>
#include <sys/types.h>

#include <net/if.h>
#include <net/pfvar.h>
#include <netinet/in.h>

#include <netpfil/pf/pf_nl.h>
#include <netlink/netlink.h>
#include <netlink/netlink_generic.h>
#include <netlink/netlink_snl.h>
#include <netlink/netlink_snl_generic.h>
#include <netlink/netlink_snl_route.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "libpfctl.h"

struct pfctl_handle {
	int fd;
	struct snl_state ss;
};

const char* PFCTL_SYNCOOKIES_MODE_NAMES[] = {
	"never",
	"always",
	"adaptive"
};

static int	_pfctl_clear_states(int , const struct pfctl_kill *,
		    unsigned int *, uint64_t);

struct pfctl_handle *
pfctl_open(const char *pf_device)
{
	struct pfctl_handle *h;

	h = calloc(1, sizeof(struct pfctl_handle));
	h->fd = -1;

	h->fd = open(pf_device, O_RDWR);
	if (h->fd < 0)
		goto error;

	if (!snl_init(&h->ss, NETLINK_GENERIC))
		goto error;

	return (h);
error:
	close(h->fd);
	snl_free(&h->ss);
	free(h);

	return (NULL);
}

void
pfctl_close(struct pfctl_handle *h)
{
	close(h->fd);
	snl_free(&h->ss);
	free(h);
}

static int
pfctl_do_ioctl(int dev, uint cmd, size_t size, nvlist_t **nvl)
{
	struct pfioc_nv nv;
	void *data;
	size_t nvlen;
	int ret;

	data = nvlist_pack(*nvl, &nvlen);
	if (nvlen > size)
		size = nvlen;

retry:
	nv.data = malloc(size);
	if (nv.data == NULL) {
		ret = ENOMEM;
		goto out;
	}

	memcpy(nv.data, data, nvlen);

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
			ret = EIO;
			goto out;
		}
	} else {
		ret = errno;
	}

out:
	free(data);
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

	for (size_t i = 0; i < elems && i < maxelems; i++)
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

int
pfctl_startstop(struct pfctl_handle *h, int start)
{
	struct snl_errmsg_data e = {};
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	uint32_t seq_id;
	int family_id;

	family_id = snl_get_genl_family(&h->ss, PFNL_FAMILY_NAME);
	if (family_id == 0)
		return (ENOTSUP);

	snl_init_writer(&h->ss, &nw);
	hdr = snl_create_genl_msg_request(&nw, family_id,
	    start ? PFNL_CMD_START : PFNL_CMD_STOP);

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL)
		return (ENOMEM);
	seq_id = hdr->nlmsg_seq;

	snl_send_message(&h->ss, hdr);

	while ((hdr = snl_read_reply_multi(&h->ss, seq_id, &e)) != NULL) {
	}

	return (e.error);
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
		if (c == NULL)
			continue;

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
		nvlist_destroy(nvl);
		free(status);
		return (NULL);
	}

	status->running = nvlist_get_bool(nvl, "running");
	status->since = nvlist_get_number(nvl, "since");
	status->debug = nvlist_get_number(nvl, "debug");
	status->hostid = ntohl(nvlist_get_number(nvl, "hostid"));
	status->states = nvlist_get_number(nvl, "states");
	status->src_nodes = nvlist_get_number(nvl, "src_nodes");
	status->syncookies_active = nvlist_get_bool(nvl, "syncookies_active");
	status->reass = nvlist_get_number(nvl, "reass");

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

static uint64_t
_pfctl_status_counter(struct pfctl_status_counters *counters, uint64_t id)
{
	struct pfctl_status_counter *c;

	TAILQ_FOREACH(c, counters, entry) {
		if (c->id == id)
			return (c->counter);
	}

	return (0);
}

uint64_t
pfctl_status_counter(struct pfctl_status *status, int id)
{
	return (_pfctl_status_counter(&status->counters, id));
}

uint64_t
pfctl_status_lcounter(struct pfctl_status *status, int id)
{
	return (_pfctl_status_counter(&status->lcounters, id));
}

uint64_t
pfctl_status_fcounter(struct pfctl_status *status, int id)
{
	return (_pfctl_status_counter(&status->fcounters, id));
}

uint64_t
pfctl_status_scounter(struct pfctl_status *status, int id)
{
	return (_pfctl_status_counter(&status->scounters, id));
}

void
pfctl_free_status(struct pfctl_status *status)
{
	struct pfctl_status_counter *c, *tmp;

	if (status == NULL)
		return;

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
pf_nvrule_uid_to_rule_uid(const nvlist_t *nvl, struct pf_rule_uid *uid)
{
	pf_nvuint_32_array(nvl, "uid", 2, uid->uid, NULL);
	uid->op = nvlist_get_number(nvl, "op");
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
	const char *const *labels;
	size_t labelcount, i;

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

	labels = nvlist_get_string_array(nvl, "labels", &labelcount);
	assert(labelcount <= PF_RULE_MAX_LABEL_COUNT);
	for (i = 0; i < labelcount; i++)
		strlcpy(rule->label[i], labels[i], PF_RULE_LABEL_SIZE);
	rule->ridentifier = nvlist_get_number(nvl, "ridentifier");

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

	strlcpy(rule->bridge_to, nvlist_get_string(nvl, "bridge_to"),
	    IFNAMSIZ);

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
		goto out;

	ri->nr = nvlist_get_number(nvl, "nr");

out:
	nvlist_destroy(nvl);
	return (ret);
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
		goto out;

	ri->nr = nvlist_get_number(nvl, "nr");
	strlcpy(ri->path, nvlist_get_string(nvl, "path"), MAXPATHLEN);
	strlcpy(ri->name, nvlist_get_string(nvl, "name"),
	    PF_ANCHOR_NAME_SIZE);

out:
	nvlist_destroy(nvl);
	return (ret);
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
		goto out;

	rules->nr = nvlist_get_number(nvl, "nr");
	rules->ticket = nvlist_get_number(nvl, "ticket");

out:
	nvlist_destroy(nvl);
	return (ret);
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
		goto out;

	pfctl_nveth_rule_to_eth_rule(nvl, rule);

	if (anchor_call)
		strlcpy(anchor_call, nvlist_get_string(nvl, "anchor_call"),
		    MAXPATHLEN);

out:
	nvlist_destroy(nvl);
	return (ret);
}

int
pfctl_add_eth_rule(int dev, const struct pfctl_eth_rule *r, const char *anchor,
    const char *anchor_call, uint32_t ticket)
{
	struct pfioc_nv nv;
	nvlist_t *nvl, *addr;
	void *packed;
	int error = 0;
	size_t labelcount, size;

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

	labelcount = 0;
	while (labelcount < PF_RULE_MAX_LABEL_COUNT &&
	    r->label[labelcount][0] != 0) {
		nvlist_append_string_array(nvl, "labels",
		    r->label[labelcount]);
		labelcount++;
	}
	nvlist_add_number(nvl, "ridentifier", r->ridentifier);

	nvlist_add_string(nvl, "qname", r->qname);
	nvlist_add_string(nvl, "tagname", r->tagname);
	nvlist_add_number(nvl, "dnpipe", r->dnpipe);
	nvlist_add_number(nvl, "dnflags", r->dnflags);

	nvlist_add_string(nvl, "bridge_to", r->bridge_to);

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

static void
snl_add_msg_attr_addr_wrap(struct snl_writer *nw, uint32_t type, const struct pf_addr_wrap *addr)
{
	int off;

	off = snl_add_msg_attr_nested(nw, type);

	snl_add_msg_attr_ip6(nw, PF_AT_ADDR, &addr->v.a.addr.v6);
	snl_add_msg_attr_ip6(nw, PF_AT_MASK, &addr->v.a.mask.v6);

	if (addr->type == PF_ADDR_DYNIFTL)
		snl_add_msg_attr_string(nw, PF_AT_IFNAME, addr->v.ifname);
	if (addr->type == PF_ADDR_TABLE)
		snl_add_msg_attr_string(nw, PF_AT_TABLENAME, addr->v.tblname);
	snl_add_msg_attr_u8(nw, PF_AT_TYPE, addr->type);
	snl_add_msg_attr_u8(nw, PF_AT_IFLAGS, addr->iflags);

	snl_end_attr_nested(nw, off);
}

static void
snl_add_msg_attr_rule_addr(struct snl_writer *nw, uint32_t type, const struct pf_rule_addr *addr)
{
	int off;

	off = snl_add_msg_attr_nested(nw, type);

	snl_add_msg_attr_addr_wrap(nw, PF_RAT_ADDR, &addr->addr);
	snl_add_msg_attr_u16(nw, PF_RAT_SRC_PORT, addr->port[0]);
	snl_add_msg_attr_u16(nw, PF_RAT_DST_PORT, addr->port[1]);
	snl_add_msg_attr_u8(nw, PF_RAT_NEG, addr->neg);
	snl_add_msg_attr_u8(nw, PF_RAT_OP, addr->port_op);

	snl_end_attr_nested(nw, off);
}

static void
snl_add_msg_attr_rule_labels(struct snl_writer *nw, uint32_t type, const char labels[PF_RULE_MAX_LABEL_COUNT][PF_RULE_LABEL_SIZE])
{
	int off, i = 0;

	off = snl_add_msg_attr_nested(nw, type);

	while (i < PF_RULE_MAX_LABEL_COUNT &&
	    labels[i][0] != 0) {
		snl_add_msg_attr_string(nw, PF_LT_LABEL, labels[i]);
		i++;
	}

	snl_end_attr_nested(nw, off);
}

static void
snl_add_msg_attr_mape(struct snl_writer *nw, uint32_t type, const struct pf_mape_portset *me)
{
	int off;

	off = snl_add_msg_attr_nested(nw, type);

	snl_add_msg_attr_u8(nw, PF_MET_OFFSET, me->offset);
	snl_add_msg_attr_u8(nw, PF_MET_PSID_LEN, me->psidlen);
	snl_add_msg_attr_u16(nw, PF_MET_PSID, me->psid);

	snl_end_attr_nested(nw, off);
}

static void
snl_add_msg_attr_rpool(struct snl_writer *nw, uint32_t type, const struct pfctl_pool *pool)
{
	int off;

	off = snl_add_msg_attr_nested(nw, type);

	snl_add_msg_attr(nw, PF_PT_KEY, sizeof(pool->key), &pool->key);
	snl_add_msg_attr_ip6(nw, PF_PT_COUNTER, &pool->counter.v6);
	snl_add_msg_attr_u32(nw, PF_PT_TBLIDX, pool->tblidx);
	snl_add_msg_attr_u16(nw, PF_PT_PROXY_SRC_PORT, pool->proxy_port[0]);
	snl_add_msg_attr_u16(nw, PF_PT_PROXY_DST_PORT, pool->proxy_port[1]);
	snl_add_msg_attr_u8(nw, PF_PT_OPTS, pool->opts);
	snl_add_msg_attr_mape(nw, PF_PT_MAPE, &pool->mape);

	snl_end_attr_nested(nw, off);
}

static void
snl_add_msg_attr_timeouts(struct snl_writer *nw, uint32_t type, const uint32_t *timeouts)
{
	int off;

	off = snl_add_msg_attr_nested(nw, type);

	for (int i = 0; i < PFTM_MAX; i++)
		snl_add_msg_attr_u32(nw, PF_TT_TIMEOUT, timeouts[i]);

	snl_end_attr_nested(nw, off);
}

static void
snl_add_msg_attr_uid(struct snl_writer *nw, uint32_t type, const struct pf_rule_uid *uid)
{
	int off;

	off = snl_add_msg_attr_nested(nw, type);

	snl_add_msg_attr_u32(nw, PF_RUT_UID_LOW, uid->uid[0]);
	snl_add_msg_attr_u32(nw, PF_RUT_UID_HIGH, uid->uid[1]);
	snl_add_msg_attr_u8(nw, PF_RUT_OP, uid->op);

	snl_end_attr_nested(nw, off);
}

static void
snl_add_msg_attr_pf_rule(struct snl_writer *nw, uint32_t type, const struct pfctl_rule *r)
{
	int off;

	off = snl_add_msg_attr_nested(nw, type);

	snl_add_msg_attr_rule_addr(nw, PF_RT_SRC, &r->src);
	snl_add_msg_attr_rule_addr(nw, PF_RT_DST, &r->dst);
	snl_add_msg_attr_rule_labels(nw, PF_RT_LABELS, r->label);
	snl_add_msg_attr_u32(nw, PF_RT_RIDENTIFIER, r->ridentifier);
	snl_add_msg_attr_string(nw, PF_RT_IFNAME, r->ifname);
	snl_add_msg_attr_string(nw, PF_RT_QNAME, r->qname);
	snl_add_msg_attr_string(nw, PF_RT_PQNAME, r->pqname);
	snl_add_msg_attr_string(nw, PF_RT_TAGNAME, r->tagname);
	snl_add_msg_attr_string(nw, PF_RT_MATCH_TAGNAME, r->match_tagname);
	snl_add_msg_attr_string(nw, PF_RT_OVERLOAD_TBLNAME, r->overload_tblname);
	snl_add_msg_attr_rpool(nw, PF_RT_RPOOL, &r->rpool);
	snl_add_msg_attr_u32(nw, PF_RT_OS_FINGERPRINT, r->os_fingerprint);
	snl_add_msg_attr_u32(nw, PF_RT_RTABLEID, r->rtableid);
	snl_add_msg_attr_timeouts(nw, PF_RT_TIMEOUT, r->timeout);
	snl_add_msg_attr_u32(nw, PF_RT_MAX_STATES, r->max_states);
	snl_add_msg_attr_u32(nw, PF_RT_MAX_SRC_NODES, r->max_src_nodes);
	snl_add_msg_attr_u32(nw, PF_RT_MAX_SRC_STATES, r->max_src_states);
	snl_add_msg_attr_u32(nw, PF_RT_MAX_SRC_CONN_RATE_LIMIT, r->max_src_conn_rate.limit);
	snl_add_msg_attr_u32(nw, PF_RT_MAX_SRC_CONN_RATE_SECS, r->max_src_conn_rate.seconds);

	snl_add_msg_attr_u16(nw, PF_RT_DNPIPE, r->dnpipe);
	snl_add_msg_attr_u16(nw, PF_RT_DNRPIPE, r->dnrpipe);
	snl_add_msg_attr_u32(nw, PF_RT_DNFLAGS, r->free_flags);

	snl_add_msg_attr_u32(nw, PF_RT_NR, r->nr);
	snl_add_msg_attr_u32(nw, PF_RT_PROB, r->prob);
	snl_add_msg_attr_u32(nw, PF_RT_CUID, r->cuid);
	snl_add_msg_attr_u32(nw, PF_RT_CPID, r->cpid);

	snl_add_msg_attr_u16(nw, PF_RT_RETURN_ICMP, r->return_icmp);
	snl_add_msg_attr_u16(nw, PF_RT_RETURN_ICMP6, r->return_icmp6);
	snl_add_msg_attr_u16(nw, PF_RT_MAX_MSS, r->max_mss);
	snl_add_msg_attr_u16(nw, PF_RT_SCRUB_FLAGS, r->scrub_flags);

	snl_add_msg_attr_uid(nw, PF_RT_UID, &r->uid);
	snl_add_msg_attr_uid(nw, PF_RT_GID, (const struct pf_rule_uid *)&r->gid);

	snl_add_msg_attr_u32(nw, PF_RT_RULE_FLAG, r->rule_flag);
	snl_add_msg_attr_u8(nw, PF_RT_ACTION, r->action);
	snl_add_msg_attr_u8(nw, PF_RT_DIRECTION, r->direction);
	snl_add_msg_attr_u8(nw, PF_RT_LOG, r->log);
	snl_add_msg_attr_u8(nw, PF_RT_LOGIF, r->logif);
	snl_add_msg_attr_u8(nw, PF_RT_QUICK, r->quick);
	snl_add_msg_attr_u8(nw, PF_RT_IF_NOT, r->ifnot);
	snl_add_msg_attr_u8(nw, PF_RT_MATCH_TAG_NOT, r->match_tag_not);
	snl_add_msg_attr_u8(nw, PF_RT_NATPASS, r->natpass);
	snl_add_msg_attr_u8(nw, PF_RT_KEEP_STATE, r->keep_state);
	snl_add_msg_attr_u8(nw, PF_RT_AF, r->af);
	snl_add_msg_attr_u8(nw, PF_RT_PROTO, r->proto);
	snl_add_msg_attr_u8(nw, PF_RT_TYPE, r->type);
	snl_add_msg_attr_u8(nw, PF_RT_CODE, r->code);
	snl_add_msg_attr_u8(nw, PF_RT_FLAGS, r->flags);
	snl_add_msg_attr_u8(nw, PF_RT_FLAGSET, r->flagset);
	snl_add_msg_attr_u8(nw, PF_RT_MIN_TTL, r->min_ttl);
	snl_add_msg_attr_u8(nw, PF_RT_ALLOW_OPTS, r->allow_opts);
	snl_add_msg_attr_u8(nw, PF_RT_RT, r->rt);
	snl_add_msg_attr_u8(nw, PF_RT_RETURN_TTL, r->return_ttl);
	snl_add_msg_attr_u8(nw, PF_RT_TOS, r->tos);
	snl_add_msg_attr_u8(nw, PF_RT_SET_TOS, r->set_tos);

	snl_add_msg_attr_u8(nw, PF_RT_ANCHOR_RELATIVE, r->anchor_relative);
	snl_add_msg_attr_u8(nw, PF_RT_ANCHOR_WILDCARD, r->anchor_wildcard);
	snl_add_msg_attr_u8(nw, PF_RT_FLUSH, r->flush);
	snl_add_msg_attr_u8(nw, PF_RT_PRIO, r->prio);
	snl_add_msg_attr_u8(nw, PF_RT_SET_PRIO, r->set_prio[0]);
	snl_add_msg_attr_u8(nw, PF_RT_SET_PRIO_REPLY, r->set_prio[1]);

	snl_add_msg_attr_ip6(nw, PF_RT_DIVERT_ADDRESS, &r->divert.addr.v6);
	snl_add_msg_attr_u16(nw, PF_RT_DIVERT_PORT, r->divert.port);

	snl_end_attr_nested(nw, off);
}

int
pfctl_add_rule(int dev __unused, const struct pfctl_rule *r, const char *anchor,
    const char *anchor_call, uint32_t ticket, uint32_t pool_ticket)
{
	struct pfctl_handle *h;
	int ret;

	h = pfctl_open(PF_DEVICE);
	if (h == NULL)
		return (ENODEV);

	ret = pfctl_add_rule_h(h, r, anchor, anchor_call, ticket, pool_ticket);

	pfctl_close(h);

	return (ret);
}

int
pfctl_add_rule_h(struct pfctl_handle *h, const struct pfctl_rule *r,
	    const char *anchor, const char *anchor_call, uint32_t ticket,
	    uint32_t pool_ticket)
{
	struct snl_writer nw;
	struct snl_errmsg_data e = {};
	struct nlmsghdr *hdr;
	uint32_t seq_id;
	int family_id;

	family_id = snl_get_genl_family(&h->ss, PFNL_FAMILY_NAME);
	if (family_id == 0)
		return (ENOTSUP);

	snl_init_writer(&h->ss, &nw);
	hdr = snl_create_genl_msg_request(&nw, family_id, PFNL_CMD_ADDRULE);
	hdr->nlmsg_flags |= NLM_F_DUMP;
	snl_add_msg_attr_u32(&nw, PF_ART_TICKET, ticket);
	snl_add_msg_attr_u32(&nw, PF_ART_POOL_TICKET, pool_ticket);
	snl_add_msg_attr_string(&nw, PF_ART_ANCHOR, anchor);
	snl_add_msg_attr_string(&nw, PF_ART_ANCHOR_CALL, anchor_call);

	snl_add_msg_attr_pf_rule(&nw, PF_ART_RULE, r);

	if ((hdr = snl_finalize_msg(&nw)) == NULL)
		return (ENXIO);

	seq_id = hdr->nlmsg_seq;

	if (! snl_send_message(&h->ss, hdr))
		return (ENXIO);

	while ((hdr = snl_read_reply_multi(&h->ss, seq_id, &e)) != NULL) {
	}

	return (e.error);
}

#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct pfctl_rules_info, _field)
static struct snl_attr_parser ap_getrules[] = {
	{ .type = PF_GR_NR, .off = _OUT(nr), .cb = snl_attr_get_uint32 },
	{ .type = PF_GR_TICKET, .off = _OUT(ticket), .cb = snl_attr_get_uint32 },
};
static struct snl_field_parser fp_getrules[] = {
};
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER(getrules_parser, struct genlmsghdr, fp_getrules, ap_getrules);

int
pfctl_get_rules_info(int dev __unused, struct pfctl_rules_info *rules, uint32_t ruleset,
    const char *path)
{
	struct snl_state ss = {};
	struct snl_errmsg_data e = {};
	struct nlmsghdr *hdr;
	struct snl_writer nw;
	uint32_t seq_id;
	int family_id;

	snl_init(&ss, NETLINK_GENERIC);
	family_id = snl_get_genl_family(&ss, PFNL_FAMILY_NAME);
	if (family_id == 0)
		return (ENOTSUP);

	snl_init_writer(&ss, &nw);
	hdr = snl_create_genl_msg_request(&nw, family_id, PFNL_CMD_GETRULES);
	hdr->nlmsg_flags |= NLM_F_DUMP;

	snl_add_msg_attr_string(&nw, PF_GR_ANCHOR, path);
	snl_add_msg_attr_u8(&nw, PF_GR_ACTION, ruleset);

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL)
		return (ENOMEM);

	seq_id = hdr->nlmsg_seq;
	if (! snl_send_message(&ss, hdr))
		return (ENXIO);

	while ((hdr = snl_read_reply_multi(&ss, seq_id, &e)) != NULL) {
		if (! snl_parse_nlmsg(&ss, hdr, &getrules_parser, rules))
			continue;
	}

	return (e.error);
}

int
pfctl_get_rule(int dev, uint32_t nr, uint32_t ticket, const char *anchor,
    uint32_t ruleset, struct pfctl_rule *rule, char *anchor_call)
{
	return (pfctl_get_clear_rule(dev, nr, ticket, anchor, ruleset, rule,
	    anchor_call, false));
}

#define _OUT(_field)	offsetof(struct pf_addr_wrap, _field)
static const struct snl_attr_parser ap_addr_wrap[] = {
	{ .type = PF_AT_ADDR, .off = _OUT(v.a.addr), .cb = snl_attr_get_in6_addr },
	{ .type = PF_AT_MASK, .off = _OUT(v.a.mask), .cb = snl_attr_get_in6_addr },
	{ .type = PF_AT_IFNAME, .off = _OUT(v.ifname), .arg = (void *)IFNAMSIZ,.cb = snl_attr_copy_string },
	{ .type = PF_AT_TABLENAME, .off = _OUT(v.tblname), .arg = (void *)PF_TABLE_NAME_SIZE, .cb = snl_attr_copy_string },
	{ .type = PF_AT_TYPE, .off = _OUT(type), .cb = snl_attr_get_uint8 },
	{ .type = PF_AT_IFLAGS, .off = _OUT(iflags), .cb = snl_attr_get_uint8 },
	{ .type = PF_AT_TBLCNT, .off = _OUT(p.tblcnt), .cb = snl_attr_get_uint32 },
	{ .type = PF_AT_DYNCNT, .off = _OUT(p.dyncnt), .cb = snl_attr_get_uint32 },
};
SNL_DECLARE_ATTR_PARSER(addr_wrap_parser, ap_addr_wrap);
#undef _OUT

#define _OUT(_field)	offsetof(struct pf_rule_addr, _field)
static struct snl_attr_parser ap_rule_addr[] = {
	{ .type = PF_RAT_ADDR, .off = _OUT(addr), .arg = &addr_wrap_parser, .cb = snl_attr_get_nested },
	{ .type = PF_RAT_SRC_PORT, .off = _OUT(port[0]), .cb = snl_attr_get_uint16 },
	{ .type = PF_RAT_DST_PORT, .off = _OUT(port[1]), .cb = snl_attr_get_uint16 },
	{ .type = PF_RAT_NEG, .off = _OUT(neg), .cb = snl_attr_get_uint8 },
	{ .type = PF_RAT_OP, .off = _OUT(port_op), .cb = snl_attr_get_uint8 },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER(rule_addr_parser, ap_rule_addr);

struct snl_parsed_labels
{
	char		labels[PF_RULE_MAX_LABEL_COUNT][PF_RULE_LABEL_SIZE];
	uint32_t	i;
};

static bool
snl_attr_get_pf_rule_labels(struct snl_state *ss, struct nlattr *nla,
    const void *arg __unused, void *target)
{
	struct snl_parsed_labels *l = (struct snl_parsed_labels *)target;
	bool ret;

	if (l->i >= PF_RULE_MAX_LABEL_COUNT)
		return (E2BIG);

	ret = snl_attr_copy_string(ss, nla, (void *)PF_RULE_LABEL_SIZE,
	    l->labels[l->i]);
	if (ret)
		l->i++;

	return (ret);
}

#define _OUT(_field)	offsetof(struct nl_parsed_labels, _field)
static const struct snl_attr_parser ap_labels[] = {
	{ .type = PF_LT_LABEL, .off = 0, .cb = snl_attr_get_pf_rule_labels },
};
SNL_DECLARE_ATTR_PARSER(rule_labels_parser, ap_labels);
#undef _OUT

static bool
snl_attr_get_nested_pf_rule_labels(struct snl_state *ss, struct nlattr *nla,
    const void *arg __unused, void *target)
{
	struct snl_parsed_labels parsed_labels = { };
	bool error;

	/* Assumes target points to the beginning of the structure */
	error = snl_parse_header(ss, NLA_DATA(nla), NLA_DATA_LEN(nla), &rule_labels_parser, &parsed_labels);
	if (! error)
		return (error);

	memcpy(target, parsed_labels.labels, sizeof(parsed_labels));

	return (true);
}

#define _OUT(_field)	offsetof(struct pf_mape_portset, _field)
static const struct snl_attr_parser ap_mape_portset[] = {
	{ .type = PF_MET_OFFSET, .off = _OUT(offset), .cb = snl_attr_get_uint8 },
	{ .type = PF_MET_PSID_LEN, .off = _OUT(psidlen), .cb = snl_attr_get_uint8 },
	{. type = PF_MET_PSID, .off = _OUT(psid), .cb = snl_attr_get_uint16 },
};
SNL_DECLARE_ATTR_PARSER(mape_portset_parser, ap_mape_portset);
#undef _OUT

#define _OUT(_field)	offsetof(struct pfctl_pool, _field)
static const struct snl_attr_parser ap_pool[] = {
	{ .type = PF_PT_KEY, .off = _OUT(key), .arg = (void *)sizeof(struct pf_poolhashkey), .cb = snl_attr_get_bytes },
	{ .type = PF_PT_COUNTER, .off = _OUT(counter), .cb = snl_attr_get_in6_addr },
	{ .type = PF_PT_TBLIDX, .off = _OUT(tblidx), .cb = snl_attr_get_uint32 },
	{ .type = PF_PT_PROXY_SRC_PORT, .off = _OUT(proxy_port[0]), .cb = snl_attr_get_uint16 },
	{ .type = PF_PT_PROXY_DST_PORT, .off = _OUT(proxy_port[1]), .cb = snl_attr_get_uint16 },
	{ .type = PF_PT_OPTS, .off = _OUT(opts), .cb = snl_attr_get_uint8 },
	{ .type = PF_PT_MAPE, .off = _OUT(mape), .arg = &mape_portset_parser, .cb = snl_attr_get_nested },
};
SNL_DECLARE_ATTR_PARSER(pool_parser, ap_pool);
#undef _OUT

struct nl_parsed_timeouts
{
	uint32_t	timeouts[PFTM_MAX];
	uint32_t	i;
};

static bool
snl_attr_get_pf_timeout(struct snl_state *ss, struct nlattr *nla,
    const void *arg __unused, void *target)
{
	struct nl_parsed_timeouts *t = (struct nl_parsed_timeouts *)target;
	bool ret;

	if (t->i >= PFTM_MAX)
		return (E2BIG);

	ret = snl_attr_get_uint32(ss, nla, NULL, &t->timeouts[t->i]);
	if (ret)
		t->i++;

	return (ret);
}

#define _OUT(_field)	offsetof(struct nl_parsed_timeout, _field)
static const struct snl_attr_parser ap_timeouts[] = {
	{ .type = PF_TT_TIMEOUT, .off = 0, .cb = snl_attr_get_pf_timeout },
};
SNL_DECLARE_ATTR_PARSER(timeout_parser, ap_timeouts);
#undef _OUT

static bool
snl_attr_get_nested_timeouts(struct snl_state *ss, struct nlattr *nla,
    const void *arg __unused, void *target)
{
	struct nl_parsed_timeouts parsed_timeouts = { };
	bool error;

	/* Assumes target points to the beginning of the structure */
	error = snl_parse_header(ss, NLA_DATA(nla), NLA_DATA_LEN(nla), &timeout_parser, &parsed_timeouts);
	if (! error)
		return (error);

	memcpy(target, parsed_timeouts.timeouts, sizeof(parsed_timeouts.timeouts));

	return (true);
}

#define _OUT(_field)	offsetof(struct pf_rule_uid, _field)
static const struct snl_attr_parser ap_rule_uid[] = {
	{ .type = PF_RUT_UID_LOW, .off = _OUT(uid[0]), .cb = snl_attr_get_uint32 },
	{ .type = PF_RUT_UID_HIGH, .off = _OUT(uid[1]), .cb = snl_attr_get_uint32 },
	{ .type = PF_RUT_OP, .off = _OUT(op), .cb = snl_attr_get_uint8 },
};
SNL_DECLARE_ATTR_PARSER(rule_uid_parser, ap_rule_uid);
#undef _OUT

struct pfctl_nl_get_rule {
	struct pfctl_rule r;
	char anchor_call[MAXPATHLEN];
};
#define	_OUT(_field)	offsetof(struct pfctl_nl_get_rule, _field)
static struct snl_attr_parser ap_getrule[] = {
	{ .type = PF_RT_SRC, .off = _OUT(r.src), .arg = &rule_addr_parser,.cb = snl_attr_get_nested },
	{ .type = PF_RT_DST, .off = _OUT(r.dst), .arg = &rule_addr_parser,.cb = snl_attr_get_nested },
	{ .type = PF_RT_RIDENTIFIER, .off = _OUT(r.ridentifier), .cb = snl_attr_get_uint32 },
	{ .type = PF_RT_LABELS, .off = _OUT(r.label), .arg = &rule_labels_parser,.cb = snl_attr_get_nested_pf_rule_labels },
	{ .type = PF_RT_IFNAME, .off = _OUT(r.ifname), .arg = (void *)IFNAMSIZ, .cb = snl_attr_copy_string },
	{ .type = PF_RT_QNAME, .off = _OUT(r.qname), .arg = (void *)PF_QNAME_SIZE, .cb = snl_attr_copy_string },
	{ .type = PF_RT_PQNAME, .off = _OUT(r.pqname), .arg = (void *)PF_QNAME_SIZE, .cb = snl_attr_copy_string },
	{ .type = PF_RT_TAGNAME, .off = _OUT(r.tagname), .arg = (void *)PF_TAG_NAME_SIZE, .cb = snl_attr_copy_string },
	{ .type = PF_RT_MATCH_TAGNAME, .off = _OUT(r.match_tagname), .arg = (void *)PF_TAG_NAME_SIZE, .cb = snl_attr_copy_string },
	{ .type = PF_RT_OVERLOAD_TBLNAME, .off = _OUT(r.overload_tblname), .arg = (void *)PF_TABLE_NAME_SIZE, .cb = snl_attr_copy_string },
	{ .type = PF_RT_RPOOL, .off = _OUT(r.rpool), .arg = &pool_parser, .cb = snl_attr_get_nested },
	{ .type = PF_RT_OS_FINGERPRINT, .off = _OUT(r.os_fingerprint), .cb = snl_attr_get_uint32 },
	{ .type = PF_RT_RTABLEID, .off = _OUT(r.rtableid), .cb = snl_attr_get_uint32 },
	{ .type = PF_RT_TIMEOUT, .off = _OUT(r.timeout), .arg = &timeout_parser, .cb = snl_attr_get_nested_timeouts },
	{ .type = PF_RT_MAX_STATES, .off = _OUT(r.max_states), .cb = snl_attr_get_uint32 },
	{ .type = PF_RT_MAX_SRC_NODES, .off = _OUT(r.max_src_nodes), .cb = snl_attr_get_uint32 },
	{ .type = PF_RT_MAX_SRC_STATES, .off = _OUT(r.max_src_states), .cb = snl_attr_get_uint32 },
	{ .type = PF_RT_MAX_SRC_CONN_RATE_LIMIT, .off = _OUT(r.max_src_conn_rate.limit), .cb = snl_attr_get_uint32 },
	{ .type = PF_RT_MAX_SRC_CONN_RATE_SECS, .off = _OUT(r.max_src_conn_rate.seconds), .cb = snl_attr_get_uint32 },
	{ .type = PF_RT_DNPIPE, .off = _OUT(r.dnpipe), .cb = snl_attr_get_uint16 },
	{ .type = PF_RT_DNRPIPE, .off = _OUT(r.dnrpipe), .cb = snl_attr_get_uint16 },
	{ .type = PF_RT_DNFLAGS, .off = _OUT(r.free_flags), .cb = snl_attr_get_uint32 },
	{ .type = PF_RT_NR, .off = _OUT(r.nr), .cb = snl_attr_get_uint32 },
	{ .type = PF_RT_PROB, .off = _OUT(r.prob), .cb = snl_attr_get_uint32 },
	{ .type = PF_RT_CUID, .off = _OUT(r.cuid), .cb = snl_attr_get_uint32 },
	{. type = PF_RT_CPID, .off = _OUT(r.cpid), .cb = snl_attr_get_uint32 },
	{ .type = PF_RT_RETURN_ICMP, .off = _OUT(r.return_icmp), .cb = snl_attr_get_uint16 },
	{ .type = PF_RT_RETURN_ICMP6, .off = _OUT(r.return_icmp6), .cb = snl_attr_get_uint16 },
	{ .type = PF_RT_MAX_MSS, .off = _OUT(r.max_mss), .cb = snl_attr_get_uint16 },
	{ .type = PF_RT_SCRUB_FLAGS, .off = _OUT(r.scrub_flags), .cb = snl_attr_get_uint16 },
	{ .type = PF_RT_UID, .off = _OUT(r.uid), .arg = &rule_uid_parser, .cb = snl_attr_get_nested },
	{ .type = PF_RT_GID, .off = _OUT(r.gid), .arg = &rule_uid_parser, .cb = snl_attr_get_nested },
	{ .type = PF_RT_RULE_FLAG, .off = _OUT(r.rule_flag), .cb = snl_attr_get_uint32 },
	{ .type = PF_RT_ACTION, .off = _OUT(r.action), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_DIRECTION, .off = _OUT(r.direction), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_LOG, .off = _OUT(r.log), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_LOGIF, .off = _OUT(r.logif), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_QUICK, .off = _OUT(r.quick), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_IF_NOT, .off = _OUT(r.ifnot), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_MATCH_TAG_NOT, .off = _OUT(r.match_tag_not), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_NATPASS, .off = _OUT(r.natpass), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_KEEP_STATE, .off = _OUT(r.keep_state), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_AF, .off = _OUT(r.af), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_PROTO, .off = _OUT(r.proto), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_TYPE, .off = _OUT(r.type), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_CODE, .off = _OUT(r.code), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_FLAGS, .off = _OUT(r.flags), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_FLAGSET, .off = _OUT(r.flagset), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_MIN_TTL, .off = _OUT(r.min_ttl), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_ALLOW_OPTS, .off = _OUT(r.allow_opts), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_RT, .off = _OUT(r.rt), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_RETURN_TTL, .off = _OUT(r.return_ttl), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_TOS, .off = _OUT(r.tos), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_SET_TOS, .off = _OUT(r.set_tos), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_ANCHOR_RELATIVE, .off = _OUT(r.anchor_relative), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_ANCHOR_WILDCARD, .off = _OUT(r.anchor_wildcard), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_FLUSH, .off = _OUT(r.flush), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_PRIO, .off = _OUT(r.prio), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_SET_PRIO, .off = _OUT(r.set_prio[0]), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_SET_PRIO_REPLY, .off = _OUT(r.set_prio[1]), .cb = snl_attr_get_uint8 },
	{ .type = PF_RT_DIVERT_ADDRESS, .off = _OUT(r.divert.addr), .cb = snl_attr_get_in6_addr },
	{ .type = PF_RT_DIVERT_PORT, .off = _OUT(r.divert.port), .cb = snl_attr_get_uint16 },
	{ .type = PF_RT_PACKETS_IN, .off = _OUT(r.packets[0]), .cb = snl_attr_get_uint64 },
	{ .type = PF_RT_PACKETS_OUT, .off = _OUT(r.packets[1]), .cb = snl_attr_get_uint64 },
	{ .type = PF_RT_BYTES_IN, .off = _OUT(r.bytes[0]), .cb = snl_attr_get_uint64 },
	{ .type = PF_RT_BYTES_OUT, .off = _OUT(r.bytes[1]), .cb = snl_attr_get_uint64 },
	{ .type = PF_RT_EVALUATIONS, .off = _OUT(r.evaluations), .cb = snl_attr_get_uint64 },
	{ .type = PF_RT_TIMESTAMP, .off = _OUT(r.last_active_timestamp), .cb = snl_attr_get_uint64 },
	{ .type = PF_RT_STATES_CUR, .off = _OUT(r.states_cur), .cb = snl_attr_get_uint64 },
	{ .type = PF_RT_STATES_TOTAL, .off = _OUT(r.states_tot), .cb = snl_attr_get_uint64 },
	{ .type = PF_RT_SRC_NODES, .off = _OUT(r.src_nodes), .cb = snl_attr_get_uint64 },
	{ .type = PF_RT_ANCHOR_CALL, .off = _OUT(anchor_call), .arg = (void*)MAXPATHLEN, .cb = snl_attr_copy_string },
};
static struct snl_field_parser fp_getrule[] = {};
#undef _OUT
SNL_DECLARE_PARSER(getrule_parser, struct genlmsghdr, fp_getrule, ap_getrule);

int
pfctl_get_clear_rule_h(struct pfctl_handle *h, uint32_t nr, uint32_t ticket,
    const char *anchor, uint32_t ruleset, struct pfctl_rule *rule,
    char *anchor_call, bool clear)
{
	struct pfctl_nl_get_rule attrs = {};
	struct snl_errmsg_data e = {};
	struct nlmsghdr *hdr;
	struct snl_writer nw;
	uint32_t seq_id;
	int family_id;

	family_id = snl_get_genl_family(&h->ss, PFNL_FAMILY_NAME);
	if (family_id == 0)
		return (ENOTSUP);

	snl_init_writer(&h->ss, &nw);
	hdr = snl_create_genl_msg_request(&nw, family_id, PFNL_CMD_GETRULE);
	hdr->nlmsg_flags |= NLM_F_DUMP;

	snl_add_msg_attr_string(&nw, PF_GR_ANCHOR, anchor);
	snl_add_msg_attr_u8(&nw, PF_GR_ACTION, ruleset);
	snl_add_msg_attr_u32(&nw, PF_GR_NR, nr);
	snl_add_msg_attr_u32(&nw, PF_GR_TICKET, ticket);
	snl_add_msg_attr_u8(&nw, PF_GR_CLEAR, clear);

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL)
		return (ENOMEM);

	seq_id = hdr->nlmsg_seq;
	if (! snl_send_message(&h->ss, hdr))
		return (ENXIO);

	while ((hdr = snl_read_reply_multi(&h->ss, seq_id, &e)) != NULL) {
		if (! snl_parse_nlmsg(&h->ss, hdr, &getrule_parser, &attrs))
			continue;
	}

	memcpy(rule, &attrs.r, sizeof(attrs.r));
	strlcpy(anchor_call, attrs.anchor_call, MAXPATHLEN);

	return (e.error);
}

int
pfctl_get_clear_rule(int dev, uint32_t nr, uint32_t ticket,
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
		goto out;

	pf_nvrule_to_rule(nvlist_get_nvlist(nvl, "rule"), rule);

	if (anchor_call)
		strlcpy(anchor_call, nvlist_get_string(nvl, "anchor_call"),
		    MAXPATHLEN);

out:
	nvlist_destroy(nvl);
	return (ret);
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

struct pfctl_creator {
	uint32_t id;
};
#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct pfctl_creator, _field)
static struct snl_attr_parser ap_creators[] = {
	{ .type = PF_ST_CREATORID, .off = _OUT(id), .cb = snl_attr_get_uint32 },
};
static struct snl_field_parser fp_creators[] = {
};
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER(creator_parser, struct genlmsghdr, fp_creators, ap_creators);

static int
pfctl_get_creators_nl(struct snl_state *ss, uint32_t *creators, size_t *len)
{

	int family_id = snl_get_genl_family(ss, PFNL_FAMILY_NAME);
	size_t i = 0;

	struct nlmsghdr *hdr;
	struct snl_writer nw;

	if (family_id == 0)
		return (ENOTSUP);

	snl_init_writer(ss, &nw);
	hdr = snl_create_genl_msg_request(&nw, family_id, PFNL_CMD_GETCREATORS);
	hdr->nlmsg_flags |= NLM_F_DUMP;
	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL)
		return (ENOMEM);
	uint32_t seq_id = hdr->nlmsg_seq;

	snl_send_message(ss, hdr);

	struct snl_errmsg_data e = {};
	while ((hdr = snl_read_reply_multi(ss, seq_id, &e)) != NULL) {
		struct pfctl_creator c;
		bzero(&c, sizeof(c));

		if (!snl_parse_nlmsg(ss, hdr, &creator_parser, &c))
			continue;

		creators[i] = c.id;
		i++;
		if (i > *len)
			return (E2BIG);
	}

	*len = i;

	return (0);
}

int
pfctl_get_creatorids(struct pfctl_handle *h, uint32_t *creators, size_t *len)
{
	int error;

	error = pfctl_get_creators_nl(&h->ss, creators, len);

	return (error);
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

static inline bool
snl_attr_get_pfaddr(struct snl_state *ss __unused, struct nlattr *nla,
    const void *arg __unused, void *target)
{
	memcpy(target, NLA_DATA(nla), NLA_DATA_LEN(nla));
	return (true);
}

static inline bool
snl_attr_store_ifname(struct snl_state *ss __unused, struct nlattr *nla,
    const void *arg __unused, void *target)
{
	size_t maxlen = NLA_DATA_LEN(nla);

	if (strnlen((char *)NLA_DATA(nla), maxlen) < maxlen) {
		strlcpy(target, (char *)NLA_DATA(nla), maxlen);
		return (true);
	}
	return (false);
}

#define	_OUT(_field)	offsetof(struct pfctl_state_peer, _field)
static const struct snl_attr_parser nla_p_speer[] = {
	{ .type = PF_STP_SEQLO, .off = _OUT(seqlo), .cb = snl_attr_get_uint32 },
	{ .type = PF_STP_SEQHI, .off = _OUT(seqhi), .cb = snl_attr_get_uint32 },
	{ .type = PF_STP_SEQDIFF, .off = _OUT(seqdiff), .cb = snl_attr_get_uint32 },
	{ .type = PF_STP_STATE, .off = _OUT(state), .cb = snl_attr_get_uint8 },
	{ .type = PF_STP_WSCALE, .off = _OUT(wscale), .cb = snl_attr_get_uint8 },
};
SNL_DECLARE_ATTR_PARSER(speer_parser, nla_p_speer);
#undef _OUT

#define	_OUT(_field)	offsetof(struct pf_state_key_export, _field)
static const struct snl_attr_parser nla_p_skey[] = {
	{ .type = PF_STK_ADDR0, .off = _OUT(addr[0]), .cb = snl_attr_get_pfaddr },
	{ .type = PF_STK_ADDR1, .off = _OUT(addr[1]), .cb = snl_attr_get_pfaddr },
	{ .type = PF_STK_PORT0, .off = _OUT(port[0]), .cb = snl_attr_get_uint16 },
	{ .type = PF_STK_PORT1, .off = _OUT(port[1]), .cb = snl_attr_get_uint16 },
};
SNL_DECLARE_ATTR_PARSER(skey_parser, nla_p_skey);
#undef _OUT

#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct pfctl_state, _field)
static struct snl_attr_parser ap_state[] = {
	{ .type = PF_ST_ID, .off = _OUT(id), .cb = snl_attr_get_uint64 },
	{ .type = PF_ST_CREATORID, .off = _OUT(creatorid), .cb = snl_attr_get_uint32 },
	{ .type = PF_ST_IFNAME, .off = _OUT(ifname), .cb = snl_attr_store_ifname },
	{ .type = PF_ST_ORIG_IFNAME, .off = _OUT(orig_ifname), .cb = snl_attr_store_ifname },
	{ .type = PF_ST_KEY_WIRE, .off = _OUT(key[0]), .arg = &skey_parser, .cb = snl_attr_get_nested },
	{ .type = PF_ST_KEY_STACK, .off = _OUT(key[1]), .arg = &skey_parser, .cb = snl_attr_get_nested },
	{ .type = PF_ST_PEER_SRC, .off = _OUT(src), .arg = &speer_parser, .cb = snl_attr_get_nested },
	{ .type = PF_ST_PEER_DST, .off = _OUT(dst), .arg = &speer_parser, .cb = snl_attr_get_nested },
	{ .type = PF_ST_RT_ADDR, .off = _OUT(rt_addr), .cb = snl_attr_get_pfaddr },
	{ .type = PF_ST_RULE, .off = _OUT(rule), .cb = snl_attr_get_uint32 },
	{ .type = PF_ST_ANCHOR, .off = _OUT(anchor), .cb = snl_attr_get_uint32 },
	{ .type = PF_ST_NAT_RULE, .off = _OUT(nat_rule), .cb = snl_attr_get_uint32 },
	{ .type = PF_ST_CREATION, .off = _OUT(creation), .cb = snl_attr_get_uint32 },
	{ .type = PF_ST_EXPIRE, .off = _OUT(expire), .cb = snl_attr_get_uint32 },
	{ .type = PF_ST_PACKETS0, .off = _OUT(packets[0]), .cb = snl_attr_get_uint64 },
	{ .type = PF_ST_PACKETS1, .off = _OUT(packets[1]), .cb = snl_attr_get_uint64 },
	{ .type = PF_ST_BYTES0, .off = _OUT(bytes[0]), .cb = snl_attr_get_uint64 },
	{ .type = PF_ST_BYTES1, .off = _OUT(bytes[1]), .cb = snl_attr_get_uint64 },
	{ .type = PF_ST_AF, .off = _OUT(key[0].af), .cb = snl_attr_get_uint8 },
	{ .type = PF_ST_PROTO, .off = _OUT(key[0].proto), .cb = snl_attr_get_uint8 },
	{ .type = PF_ST_DIRECTION, .off = _OUT(direction), .cb = snl_attr_get_uint8 },
	{ .type = PF_ST_LOG, .off = _OUT(log), .cb = snl_attr_get_uint8 },
	{ .type = PF_ST_STATE_FLAGS, .off = _OUT(state_flags), .cb = snl_attr_get_uint16 },
	{ .type = PF_ST_SYNC_FLAGS, .off = _OUT(sync_flags), .cb = snl_attr_get_uint8 },
	{ .type = PF_ST_RTABLEID, .off = _OUT(rtableid), .cb = snl_attr_get_int32 },
	{ .type = PF_ST_MIN_TTL, .off = _OUT(min_ttl), .cb = snl_attr_get_uint8 },
	{ .type = PF_ST_MAX_MSS, .off = _OUT(max_mss), .cb = snl_attr_get_uint16 },
	{ .type = PF_ST_DNPIPE, .off = _OUT(dnpipe), .cb = snl_attr_get_uint16 },
	{ .type = PF_ST_DNRPIPE, .off = _OUT(dnrpipe), .cb = snl_attr_get_uint16 },
	{ .type = PF_ST_RT, .off = _OUT(rt), .cb = snl_attr_get_uint8 },
	{ .type = PF_ST_RT_IFNAME, .off = _OUT(rt_ifname), .cb = snl_attr_store_ifname },
};
static struct snl_field_parser fp_state[] = {
};
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER(state_parser, struct genlmsghdr, fp_state, ap_state);

static const struct snl_hdr_parser *all_parsers[] = {
	&state_parser, &skey_parser, &speer_parser,
	&creator_parser, &getrules_parser
};

static int
pfctl_get_states_nl(struct pfctl_state_filter *filter, struct snl_state *ss, pfctl_get_state_fn f, void *arg)
{
	SNL_VERIFY_PARSERS(all_parsers);
	int family_id = snl_get_genl_family(ss, PFNL_FAMILY_NAME);
	int ret;

	struct nlmsghdr *hdr;
	struct snl_writer nw;

	if (family_id == 0)
		return (ENOTSUP);

	snl_init_writer(ss, &nw);
	hdr = snl_create_genl_msg_request(&nw, family_id, PFNL_CMD_GETSTATES);
	hdr->nlmsg_flags |= NLM_F_DUMP;
	snl_add_msg_attr_string(&nw, PF_ST_IFNAME, filter->ifname);
	snl_add_msg_attr_u16(&nw, PF_ST_PROTO, filter->proto);
	snl_add_msg_attr_u8(&nw, PF_ST_AF, filter->af);
	snl_add_msg_attr_ip6(&nw, PF_ST_FILTER_ADDR, &filter->addr.v6);
	snl_add_msg_attr_ip6(&nw, PF_ST_FILTER_MASK, &filter->mask.v6);

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL)
		return (ENOMEM);

	uint32_t seq_id = hdr->nlmsg_seq;

	snl_send_message(ss, hdr);

	struct snl_errmsg_data e = {};
	while ((hdr = snl_read_reply_multi(ss, seq_id, &e)) != NULL) {
		struct pfctl_state s;
		bzero(&s, sizeof(s));
		if (!snl_parse_nlmsg(ss, hdr, &state_parser, &s))
			continue;

		s.key[1].af = s.key[0].af;
		s.key[1].proto = s.key[0].proto;

		ret = f(&s, arg);
		if (ret != 0)
			return (ret);
	}

	return (0);
}

int
pfctl_get_states_iter(pfctl_get_state_fn f, void *arg)
{
	struct pfctl_state_filter filter = {};
	return (pfctl_get_filtered_states_iter(&filter, f, arg));
}

int
pfctl_get_filtered_states_iter(struct pfctl_state_filter *filter, pfctl_get_state_fn f, void *arg)
{
	struct snl_state ss = {};
	int error;

	snl_init(&ss, NETLINK_GENERIC);
	error = pfctl_get_states_nl(filter, &ss, f, arg);
	snl_free(&ss);

	return (error);
}

static int
pfctl_append_states(struct pfctl_state *s, void *arg)
{
	struct pfctl_state *new;
	struct pfctl_states *states = (struct pfctl_states *)arg;

	new = malloc(sizeof(*s));
	if (new == NULL)
		return (ENOMEM);

	memcpy(new, s, sizeof(*s));

	TAILQ_INSERT_TAIL(&states->states, new, entry);

	return (0);
}

int
pfctl_get_states(int dev __unused, struct pfctl_states *states)
{
	int ret;

	bzero(states, sizeof(*states));
	TAILQ_INIT(&states->states);

	ret = pfctl_get_states_iter(pfctl_append_states, states);
	if (ret != 0) {
		pfctl_free_states(states);
		return (ret);
	}

	return (0);
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
	nvlist_add_bool(nvl, "nat", kill->nat);

	if ((ret = pfctl_do_ioctl(dev, ioctlval, 1024, &nvl)) != 0)
		goto out;

	if (killed)
		*killed = nvlist_get_number(nvl, "killed");

out:
	nvlist_destroy(nvl);
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
	uint64_t	 lim, hi, lo;

	ret = pfctl_get_limit(dev, PF_LIMIT_STATES, &state_limit);
	if (ret != 0)
		return (ret);

	lim = state_limit;
	hi = lim * s->highwater / 100;
	lo = lim * s->lowwater / 100;

	if (lo == hi)
		hi++;

	nvl = nvlist_create(0);

	nvlist_add_bool(nvl, "enabled", s->mode != PFCTL_SYNCOOKIES_NEVER);
	nvlist_add_bool(nvl, "adaptive", s->mode == PFCTL_SYNCOOKIES_ADAPTIVE);
	nvlist_add_number(nvl, "highwater", hi);
	nvlist_add_number(nvl, "lowwater", lo);

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

	if ((ret = pfctl_do_ioctl(dev, DIOCGETSYNCOOKIES, 256, &nvl)) != 0) {
		ret = errno;
		goto out;
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
	s->halfopen_states = nvlist_get_number(nvl, "halfopen_states");

out:
	nvlist_destroy(nvl);
	return (ret);
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
