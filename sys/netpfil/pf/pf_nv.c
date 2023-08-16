/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Rubicon Communications, LLC (Netgate)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/limits.h>
#include <sys/queue.h>
#include <sys/systm.h>

#include <netpfil/pf/pf_nv.h>

#define	PF_NV_IMPL_UINT(fnname, type, max)					\
	int									\
	pf_nv ## fnname ## _opt(const nvlist_t *nvl, const char *name,		\
	    type *val, type dflt)						\
	{									\
		uint64_t raw;							\
		if (! nvlist_exists_number(nvl, name)) {			\
			*val = dflt;						\
			return (0);						\
		}								\
		raw = nvlist_get_number(nvl, name);				\
		if (raw > max)							\
			return (ERANGE);					\
		*val = (type)raw;						\
		return (0);							\
	}									\
	int									\
	pf_nv ## fnname(const nvlist_t *nvl, const char *name, type *val)	\
	{									\
		uint64_t raw;							\
		if (! nvlist_exists_number(nvl, name))				\
			return (EINVAL);					\
		raw = nvlist_get_number(nvl, name);				\
		if (raw > max)							\
			return (ERANGE);					\
		*val = (type)raw;						\
		return (0);							\
	}									\
	int									\
	pf_nv ## fnname ## _array(const nvlist_t *nvl, const char *name,	\
	    type *array, size_t maxelems, size_t *nelems)			\
	{									\
		const uint64_t *n;						\
		size_t nitems;							\
		bzero(array, sizeof(type) * maxelems);				\
		if (! nvlist_exists_number_array(nvl, name))			\
			return (EINVAL);					\
		n = nvlist_get_number_array(nvl, name, &nitems);		\
		if (nitems > maxelems)						\
			return (E2BIG);						\
		if (nelems != NULL)						\
			*nelems = nitems;					\
		for (size_t i = 0; i < nitems; i++) {				\
			if (n[i] > max)						\
				return (ERANGE);				\
			array[i] = (type)n[i];					\
		}								\
		return (0);							\
	}									\
	void									\
	pf_ ## fnname ## _array_nv(nvlist_t *nvl, const char *name,		\
	    const type *numbers, size_t count)					\
	{									\
		uint64_t tmp;							\
		for (size_t i = 0; i < count; i++) {				\
			tmp = numbers[i];					\
			nvlist_append_number_array(nvl, name, tmp);		\
		}								\
	}

int
pf_nvbool(const nvlist_t *nvl, const char *name, bool *val)
{
	if (! nvlist_exists_bool(nvl, name))
		return (EINVAL);

	*val = nvlist_get_bool(nvl, name);

	return (0);
}

int
pf_nvbinary(const nvlist_t *nvl, const char *name, void *data,
    size_t expected_size)
{
	const uint8_t *nvdata;
	size_t len;

	bzero(data, expected_size);

	if (! nvlist_exists_binary(nvl, name))
		return (EINVAL);

	nvdata = (const uint8_t *)nvlist_get_binary(nvl, name, &len);
	if (len > expected_size)
		return (EINVAL);

	memcpy(data, nvdata, len);

	return (0);
}

PF_NV_IMPL_UINT(uint8, uint8_t, UINT8_MAX);
PF_NV_IMPL_UINT(uint16, uint16_t, UINT16_MAX);
PF_NV_IMPL_UINT(uint32, uint32_t, UINT32_MAX);
PF_NV_IMPL_UINT(uint64, uint64_t, UINT64_MAX);

int
pf_nvint(const nvlist_t *nvl, const char *name, int *val)
{
	int64_t raw;

	if (! nvlist_exists_number(nvl, name))
		return (EINVAL);

	raw = nvlist_get_number(nvl, name);
	if (raw > INT_MAX || raw < INT_MIN)
		return (ERANGE);

	*val = (int)raw;

	return (0);
}

int
pf_nvstring(const nvlist_t *nvl, const char *name, char *str, size_t maxlen)
{
	int ret;

	if (! nvlist_exists_string(nvl, name))
		return (EINVAL);

	ret = strlcpy(str, nvlist_get_string(nvl, name), maxlen);
	if (ret >= maxlen)
		return (EINVAL);

	return (0);
}

static int
pf_nvaddr_to_addr(const nvlist_t *nvl, struct pf_addr *paddr)
{
	return (pf_nvbinary(nvl, "addr", paddr, sizeof(*paddr)));
}

static nvlist_t *
pf_addr_to_nvaddr(const struct pf_addr *paddr)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (NULL);

	nvlist_add_binary(nvl, "addr", paddr, sizeof(*paddr));

	return (nvl);
}

static int
pf_nvmape_to_mape(const nvlist_t *nvl, struct pf_mape_portset *mape)
{
	int error = 0;

	bzero(mape, sizeof(*mape));
	PFNV_CHK(pf_nvuint8(nvl, "offset", &mape->offset));
	PFNV_CHK(pf_nvuint8(nvl, "psidlen", &mape->psidlen));
	PFNV_CHK(pf_nvuint16(nvl, "psid", &mape->psid));

errout:
	return (error);
}

static nvlist_t *
pf_mape_to_nvmape(const struct pf_mape_portset *mape)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (NULL);

	nvlist_add_number(nvl, "offset", mape->offset);
	nvlist_add_number(nvl, "psidlen", mape->psidlen);
	nvlist_add_number(nvl, "psid", mape->psid);

	return (nvl);
}

static int
pf_nvpool_to_pool(const nvlist_t *nvl, struct pf_kpool *kpool)
{
	int error = 0;

	PFNV_CHK(pf_nvbinary(nvl, "key", &kpool->key, sizeof(kpool->key)));

	if (nvlist_exists_nvlist(nvl, "counter")) {
		PFNV_CHK(pf_nvaddr_to_addr(nvlist_get_nvlist(nvl, "counter"),
		    &kpool->counter));
	}

	PFNV_CHK(pf_nvint(nvl, "tblidx", &kpool->tblidx));
	PFNV_CHK(pf_nvuint16_array(nvl, "proxy_port", kpool->proxy_port, 2,
	    NULL));
	PFNV_CHK(pf_nvuint8(nvl, "opts", &kpool->opts));

	if (nvlist_exists_nvlist(nvl, "mape")) {
		PFNV_CHK(pf_nvmape_to_mape(nvlist_get_nvlist(nvl, "mape"),
		    &kpool->mape));
	}

errout:
	return (error);
}

static nvlist_t *
pf_pool_to_nvpool(const struct pf_kpool *pool)
{
	nvlist_t *nvl;
	nvlist_t *tmp;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (NULL);

	nvlist_add_binary(nvl, "key", &pool->key, sizeof(pool->key));
	tmp = pf_addr_to_nvaddr(&pool->counter);
	if (tmp == NULL)
		goto error;
	nvlist_add_nvlist(nvl, "counter", tmp);
	nvlist_destroy(tmp);

	nvlist_add_number(nvl, "tblidx", pool->tblidx);
	pf_uint16_array_nv(nvl, "proxy_port", pool->proxy_port, 2);
	nvlist_add_number(nvl, "opts", pool->opts);

	tmp = pf_mape_to_nvmape(&pool->mape);
	if (tmp == NULL)
		goto error;
	nvlist_add_nvlist(nvl, "mape", tmp);
	nvlist_destroy(tmp);

	return (nvl);

error:
	nvlist_destroy(nvl);
	return (NULL);
}

static int
pf_nvaddr_wrap_to_addr_wrap(const nvlist_t *nvl, struct pf_addr_wrap *addr)
{
	int error = 0;

	bzero(addr, sizeof(*addr));

	PFNV_CHK(pf_nvuint8(nvl, "type", &addr->type));
	PFNV_CHK(pf_nvuint8(nvl, "iflags", &addr->iflags));
	if (addr->type == PF_ADDR_DYNIFTL)
		PFNV_CHK(pf_nvstring(nvl, "ifname", addr->v.ifname,
		    sizeof(addr->v.ifname)));
	if (addr->type == PF_ADDR_TABLE)
		PFNV_CHK(pf_nvstring(nvl, "tblname", addr->v.tblname,
		    sizeof(addr->v.tblname)));

	if (! nvlist_exists_nvlist(nvl, "addr"))
		return (EINVAL);
	PFNV_CHK(pf_nvaddr_to_addr(nvlist_get_nvlist(nvl, "addr"),
	    &addr->v.a.addr));

	if (! nvlist_exists_nvlist(nvl, "mask"))
		return (EINVAL);
	PFNV_CHK(pf_nvaddr_to_addr(nvlist_get_nvlist(nvl, "mask"),
	    &addr->v.a.mask));

	switch (addr->type) {
	case PF_ADDR_DYNIFTL:
	case PF_ADDR_TABLE:
	case PF_ADDR_RANGE:
	case PF_ADDR_ADDRMASK:
	case PF_ADDR_NOROUTE:
	case PF_ADDR_URPFFAILED:
		break;
	default:
		return (EINVAL);
	}

errout:
	return (error);
}

static nvlist_t *
pf_addr_wrap_to_nvaddr_wrap(const struct pf_addr_wrap *addr)
{
	nvlist_t *nvl;
	nvlist_t *tmp;
	uint64_t num;
	struct pfr_ktable *kt;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (NULL);

	nvlist_add_number(nvl, "type", addr->type);
	nvlist_add_number(nvl, "iflags", addr->iflags);
	if (addr->type == PF_ADDR_DYNIFTL) {
		nvlist_add_string(nvl, "ifname", addr->v.ifname);
		num = 0;
		if (addr->p.dyn != NULL)
			num = addr->p.dyn->pfid_acnt4 +
			    addr->p.dyn->pfid_acnt6;
		nvlist_add_number(nvl, "dyncnt", num);
	}
	if (addr->type == PF_ADDR_TABLE) {
		nvlist_add_string(nvl, "tblname", addr->v.tblname);
		num = -1;
		kt = addr->p.tbl;
		if ((kt->pfrkt_flags & PFR_TFLAG_ACTIVE) &&
		    kt->pfrkt_root != NULL)
			kt = kt->pfrkt_root;
		if (kt->pfrkt_flags & PFR_TFLAG_ACTIVE)
			num = kt->pfrkt_cnt;
		nvlist_add_number(nvl, "tblcnt", num);
	}

	tmp = pf_addr_to_nvaddr(&addr->v.a.addr);
	if (tmp == NULL)
		goto error;
	nvlist_add_nvlist(nvl, "addr", tmp);
	nvlist_destroy(tmp);
	tmp = pf_addr_to_nvaddr(&addr->v.a.mask);
	if (tmp == NULL)
		goto error;
	nvlist_add_nvlist(nvl, "mask", tmp);
	nvlist_destroy(tmp);

	return (nvl);

error:
	nvlist_destroy(nvl);
	return (NULL);
}

static int
pf_validate_op(uint8_t op)
{
	switch (op) {
	case PF_OP_NONE:
	case PF_OP_IRG:
	case PF_OP_EQ:
	case PF_OP_NE:
	case PF_OP_LT:
	case PF_OP_LE:
	case PF_OP_GT:
	case PF_OP_GE:
	case PF_OP_XRG:
	case PF_OP_RRG:
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

static int
pf_nvrule_addr_to_rule_addr(const nvlist_t *nvl, struct pf_rule_addr *addr)
{
	int error = 0;

	if (! nvlist_exists_nvlist(nvl, "addr"))
		return (EINVAL);

	PFNV_CHK(pf_nvaddr_wrap_to_addr_wrap(nvlist_get_nvlist(nvl, "addr"),
	    &addr->addr));
	PFNV_CHK(pf_nvuint16_array(nvl, "port", addr->port, 2, NULL));
	PFNV_CHK(pf_nvuint8(nvl, "neg", &addr->neg));
	PFNV_CHK(pf_nvuint8(nvl, "port_op", &addr->port_op));

	PFNV_CHK(pf_validate_op(addr->port_op));

errout:
	return (error);
}

static nvlist_t *
pf_rule_addr_to_nvrule_addr(const struct pf_rule_addr *addr)
{
	nvlist_t *nvl;
	nvlist_t *tmp;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (NULL);

	tmp = pf_addr_wrap_to_nvaddr_wrap(&addr->addr);
	if (tmp == NULL)
		goto error;
	nvlist_add_nvlist(nvl, "addr", tmp);
	nvlist_destroy(tmp);
	pf_uint16_array_nv(nvl, "port", addr->port, 2);
	nvlist_add_number(nvl, "neg", addr->neg);
	nvlist_add_number(nvl, "port_op", addr->port_op);

	return (nvl);

error:
	nvlist_destroy(nvl);
	return (NULL);
}

static int
pf_nvrule_uid_to_rule_uid(const nvlist_t *nvl, struct pf_rule_uid *uid)
{
	int error = 0;

	bzero(uid, sizeof(*uid));

	PFNV_CHK(pf_nvuint32_array(nvl, "uid", uid->uid, 2, NULL));
	PFNV_CHK(pf_nvuint8(nvl, "op", &uid->op));

	PFNV_CHK(pf_validate_op(uid->op));

errout:
	return (error);
}

static nvlist_t *
pf_rule_uid_to_nvrule_uid(const struct pf_rule_uid *uid)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (NULL);

	pf_uint32_array_nv(nvl, "uid", uid->uid, 2);
	nvlist_add_number(nvl, "op", uid->op);

	return (nvl);
}

static int
pf_nvrule_gid_to_rule_gid(const nvlist_t *nvl, struct pf_rule_gid *gid)
{
	/* Cheat a little. These stucts are the same, other than the name of
	 * the first field. */
	return (pf_nvrule_uid_to_rule_uid(nvl, (struct pf_rule_uid *)gid));
}

int
pf_check_rule_addr(const struct pf_rule_addr *addr)
{

	switch (addr->addr.type) {
	case PF_ADDR_ADDRMASK:
	case PF_ADDR_NOROUTE:
	case PF_ADDR_DYNIFTL:
	case PF_ADDR_TABLE:
	case PF_ADDR_URPFFAILED:
	case PF_ADDR_RANGE:
		break;
	default:
		return (EINVAL);
	}

	if (addr->addr.p.dyn != NULL) {
		return (EINVAL);
	}

	return (0);
}


int
pf_nvrule_to_krule(const nvlist_t *nvl, struct pf_krule *rule)
{
	int error = 0;

#define	ERROUT(x)	ERROUT_FUNCTION(errout, x)

	PFNV_CHK(pf_nvuint32(nvl, "nr", &rule->nr));

	if (! nvlist_exists_nvlist(nvl, "src"))
		ERROUT(EINVAL);

	error = pf_nvrule_addr_to_rule_addr(nvlist_get_nvlist(nvl, "src"),
	    &rule->src);
	if (error != 0)
		ERROUT(error);

	if (! nvlist_exists_nvlist(nvl, "dst"))
		ERROUT(EINVAL);

	PFNV_CHK(pf_nvrule_addr_to_rule_addr(nvlist_get_nvlist(nvl, "dst"),
	    &rule->dst));

	if (nvlist_exists_string(nvl, "label")) {
		PFNV_CHK(pf_nvstring(nvl, "label", rule->label[0],
		    sizeof(rule->label[0])));
	} else if (nvlist_exists_string_array(nvl, "labels")) {
		const char *const *strs;
		size_t items;
		int ret;

		strs = nvlist_get_string_array(nvl, "labels", &items);
		if (items > PF_RULE_MAX_LABEL_COUNT)
			ERROUT(E2BIG);

		for (size_t i = 0; i < items; i++) {
			ret = strlcpy(rule->label[i], strs[i],
			    sizeof(rule->label[0]));
			if (ret >= sizeof(rule->label[0]))
				ERROUT(E2BIG);
		}
	}

	PFNV_CHK(pf_nvuint32_opt(nvl, "ridentifier", &rule->ridentifier, 0));
	PFNV_CHK(pf_nvstring(nvl, "ifname", rule->ifname,
	    sizeof(rule->ifname)));
	PFNV_CHK(pf_nvstring(nvl, "qname", rule->qname, sizeof(rule->qname)));
	PFNV_CHK(pf_nvstring(nvl, "pqname", rule->pqname,
	    sizeof(rule->pqname)));
	PFNV_CHK(pf_nvstring(nvl, "tagname", rule->tagname,
	    sizeof(rule->tagname)));
	PFNV_CHK(pf_nvuint16_opt(nvl, "dnpipe", &rule->dnpipe, 0));
	PFNV_CHK(pf_nvuint16_opt(nvl, "dnrpipe", &rule->dnrpipe, 0));
	PFNV_CHK(pf_nvuint32_opt(nvl, "dnflags", &rule->free_flags, 0));
	PFNV_CHK(pf_nvstring(nvl, "match_tagname", rule->match_tagname,
	    sizeof(rule->match_tagname)));
	PFNV_CHK(pf_nvstring(nvl, "overload_tblname", rule->overload_tblname,
	    sizeof(rule->overload_tblname)));

	if (! nvlist_exists_nvlist(nvl, "rpool"))
		ERROUT(EINVAL);
	PFNV_CHK(pf_nvpool_to_pool(nvlist_get_nvlist(nvl, "rpool"),
	    &rule->rpool));

	PFNV_CHK(pf_nvuint32(nvl, "os_fingerprint", &rule->os_fingerprint));

	PFNV_CHK(pf_nvint(nvl, "rtableid", &rule->rtableid));
	PFNV_CHK(pf_nvuint32_array(nvl, "timeout", rule->timeout, PFTM_MAX, NULL));
	PFNV_CHK(pf_nvuint32(nvl, "max_states", &rule->max_states));
	PFNV_CHK(pf_nvuint32(nvl, "max_src_nodes", &rule->max_src_nodes));
	PFNV_CHK(pf_nvuint32(nvl, "max_src_states", &rule->max_src_states));
	PFNV_CHK(pf_nvuint32(nvl, "max_src_conn", &rule->max_src_conn));
	PFNV_CHK(pf_nvuint32(nvl, "max_src_conn_rate.limit",
	    &rule->max_src_conn_rate.limit));
	PFNV_CHK(pf_nvuint32(nvl, "max_src_conn_rate.seconds",
	    &rule->max_src_conn_rate.seconds));
	PFNV_CHK(pf_nvuint32(nvl, "prob", &rule->prob));
	PFNV_CHK(pf_nvuint32(nvl, "cuid", &rule->cuid));
	PFNV_CHK(pf_nvuint32(nvl, "cpid", &rule->cpid));

	PFNV_CHK(pf_nvuint16(nvl, "return_icmp", &rule->return_icmp));
	PFNV_CHK(pf_nvuint16(nvl, "return_icmp6", &rule->return_icmp6));

	PFNV_CHK(pf_nvuint16(nvl, "max_mss", &rule->max_mss));
	PFNV_CHK(pf_nvuint16(nvl, "scrub_flags", &rule->scrub_flags));

	if (! nvlist_exists_nvlist(nvl, "uid"))
		ERROUT(EINVAL);
	PFNV_CHK(pf_nvrule_uid_to_rule_uid(nvlist_get_nvlist(nvl, "uid"),
	    &rule->uid));

	if (! nvlist_exists_nvlist(nvl, "gid"))
		ERROUT(EINVAL);
	PFNV_CHK(pf_nvrule_gid_to_rule_gid(nvlist_get_nvlist(nvl, "gid"),
	    &rule->gid));

	PFNV_CHK(pf_nvuint32(nvl, "rule_flag", &rule->rule_flag));
	PFNV_CHK(pf_nvuint8(nvl, "action", &rule->action));
	PFNV_CHK(pf_nvuint8(nvl, "direction", &rule->direction));
	PFNV_CHK(pf_nvuint8(nvl, "log", &rule->log));
	PFNV_CHK(pf_nvuint8(nvl, "logif", &rule->logif));
	PFNV_CHK(pf_nvuint8(nvl, "quick", &rule->quick));
	PFNV_CHK(pf_nvuint8(nvl, "ifnot", &rule->ifnot));
	PFNV_CHK(pf_nvuint8(nvl, "match_tag_not", &rule->match_tag_not));
	PFNV_CHK(pf_nvuint8(nvl, "natpass", &rule->natpass));

	PFNV_CHK(pf_nvuint8(nvl, "keep_state", &rule->keep_state));
	PFNV_CHK(pf_nvuint8(nvl, "af", &rule->af));
	PFNV_CHK(pf_nvuint8(nvl, "proto", &rule->proto));
	PFNV_CHK(pf_nvuint8(nvl, "type", &rule->type));
	PFNV_CHK(pf_nvuint8(nvl, "code", &rule->code));
	PFNV_CHK(pf_nvuint8(nvl, "flags", &rule->flags));
	PFNV_CHK(pf_nvuint8(nvl, "flagset", &rule->flagset));
	PFNV_CHK(pf_nvuint8(nvl, "min_ttl", &rule->min_ttl));
	PFNV_CHK(pf_nvuint8(nvl, "allow_opts", &rule->allow_opts));
	PFNV_CHK(pf_nvuint8(nvl, "rt", &rule->rt));
	PFNV_CHK(pf_nvuint8(nvl, "return_ttl", &rule->return_ttl));
	PFNV_CHK(pf_nvuint8(nvl, "tos", &rule->tos));
	PFNV_CHK(pf_nvuint8(nvl, "set_tos", &rule->set_tos));

	PFNV_CHK(pf_nvuint8(nvl, "flush", &rule->flush));
	PFNV_CHK(pf_nvuint8(nvl, "prio", &rule->prio));

	PFNV_CHK(pf_nvuint8_array(nvl, "set_prio", rule->set_prio, 2, NULL));

	if (nvlist_exists_nvlist(nvl, "divert")) {
		const nvlist_t *nvldivert = nvlist_get_nvlist(nvl, "divert");

		if (! nvlist_exists_nvlist(nvldivert, "addr"))
			ERROUT(EINVAL);
		PFNV_CHK(pf_nvaddr_to_addr(nvlist_get_nvlist(nvldivert, "addr"),
		    &rule->divert.addr));
		PFNV_CHK(pf_nvuint16(nvldivert, "port", &rule->divert.port));
	}

	/* Validation */
#ifndef INET
	if (rule->af == AF_INET)
		ERROUT(EAFNOSUPPORT);
#endif /* INET */
#ifndef INET6
	if (rule->af == AF_INET6)
		ERROUT(EAFNOSUPPORT);
#endif /* INET6 */

	PFNV_CHK(pf_check_rule_addr(&rule->src));
	PFNV_CHK(pf_check_rule_addr(&rule->dst));

	return (0);

#undef ERROUT
errout:
	return (error);
}

static nvlist_t *
pf_divert_to_nvdivert(const struct pf_krule *rule)
{
	nvlist_t *nvl;
	nvlist_t *tmp;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (NULL);

	tmp = pf_addr_to_nvaddr(&rule->divert.addr);
	if (tmp == NULL)
		goto error;
	nvlist_add_nvlist(nvl, "addr", tmp);
	nvlist_destroy(tmp);
	nvlist_add_number(nvl, "port", rule->divert.port);

	return (nvl);

error:
	nvlist_destroy(nvl);
	return (NULL);
}

nvlist_t *
pf_krule_to_nvrule(struct pf_krule *rule)
{
	nvlist_t *nvl, *tmp;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (nvl);

	nvlist_add_number(nvl, "nr", rule->nr);
	tmp = pf_rule_addr_to_nvrule_addr(&rule->src);
	if (tmp == NULL)
		goto error;
	nvlist_add_nvlist(nvl, "src", tmp);
	nvlist_destroy(tmp);
	tmp = pf_rule_addr_to_nvrule_addr(&rule->dst);
	if (tmp == NULL)
		goto error;
	nvlist_add_nvlist(nvl, "dst", tmp);
	nvlist_destroy(tmp);

	for (int i = 0; i < PF_SKIP_COUNT; i++) {
		nvlist_append_number_array(nvl, "skip",
		    rule->skip[i].ptr ? rule->skip[i].ptr->nr : -1);
	}

	for (int i = 0; i < PF_RULE_MAX_LABEL_COUNT; i++) {
		nvlist_append_string_array(nvl, "labels", rule->label[i]);
	}
	nvlist_add_string(nvl, "label", rule->label[0]);
	nvlist_add_number(nvl, "ridentifier", rule->ridentifier);
	nvlist_add_string(nvl, "ifname", rule->ifname);
	nvlist_add_string(nvl, "qname", rule->qname);
	nvlist_add_string(nvl, "pqname", rule->pqname);
	nvlist_add_number(nvl, "dnpipe", rule->dnpipe);
	nvlist_add_number(nvl, "dnrpipe", rule->dnrpipe);
	nvlist_add_number(nvl, "dnflags", rule->free_flags);
	nvlist_add_string(nvl, "tagname", rule->tagname);
	nvlist_add_string(nvl, "match_tagname", rule->match_tagname);
	nvlist_add_string(nvl, "overload_tblname", rule->overload_tblname);

	tmp = pf_pool_to_nvpool(&rule->rpool);
	if (tmp == NULL)
		goto error;
	nvlist_add_nvlist(nvl, "rpool", tmp);
	nvlist_destroy(tmp);

	nvlist_add_number(nvl, "evaluations",
	    pf_counter_u64_fetch(&rule->evaluations));
	for (int i = 0; i < 2; i++) {
		nvlist_append_number_array(nvl, "packets",
		    pf_counter_u64_fetch(&rule->packets[i]));
		nvlist_append_number_array(nvl, "bytes",
		    pf_counter_u64_fetch(&rule->bytes[i]));
	}
	nvlist_add_number(nvl, "timestamp", pf_get_timestamp(rule));

	nvlist_add_number(nvl, "os_fingerprint", rule->os_fingerprint);

	nvlist_add_number(nvl, "rtableid", rule->rtableid);
	pf_uint32_array_nv(nvl, "timeout", rule->timeout, PFTM_MAX);
	nvlist_add_number(nvl, "max_states", rule->max_states);
	nvlist_add_number(nvl, "max_src_nodes", rule->max_src_nodes);
	nvlist_add_number(nvl, "max_src_states", rule->max_src_states);
	nvlist_add_number(nvl, "max_src_conn", rule->max_src_conn);
	nvlist_add_number(nvl, "max_src_conn_rate.limit",
	    rule->max_src_conn_rate.limit);
	nvlist_add_number(nvl, "max_src_conn_rate.seconds",
	    rule->max_src_conn_rate.seconds);
	nvlist_add_number(nvl, "qid", rule->qid);
	nvlist_add_number(nvl, "pqid", rule->pqid);
	nvlist_add_number(nvl, "prob", rule->prob);
	nvlist_add_number(nvl, "cuid", rule->cuid);
	nvlist_add_number(nvl, "cpid", rule->cpid);

	nvlist_add_number(nvl, "states_cur",
	    counter_u64_fetch(rule->states_cur));
	nvlist_add_number(nvl, "states_tot",
	    counter_u64_fetch(rule->states_tot));
	nvlist_add_number(nvl, "src_nodes",
	    counter_u64_fetch(rule->src_nodes));

	nvlist_add_number(nvl, "return_icmp", rule->return_icmp);
	nvlist_add_number(nvl, "return_icmp6", rule->return_icmp6);

	nvlist_add_number(nvl, "max_mss", rule->max_mss);
	nvlist_add_number(nvl, "scrub_flags", rule->scrub_flags);

	tmp = pf_rule_uid_to_nvrule_uid(&rule->uid);
	if (tmp == NULL)
		goto error;
	nvlist_add_nvlist(nvl, "uid", tmp);
	nvlist_destroy(tmp);
	tmp = pf_rule_uid_to_nvrule_uid((const struct pf_rule_uid *)&rule->gid);
	if (tmp == NULL)
		goto error;
	nvlist_add_nvlist(nvl, "gid", tmp);
	nvlist_destroy(tmp);

	nvlist_add_number(nvl, "rule_flag", rule->rule_flag);
	nvlist_add_number(nvl, "action", rule->action);
	nvlist_add_number(nvl, "direction", rule->direction);
	nvlist_add_number(nvl, "log", rule->log);
	nvlist_add_number(nvl, "logif", rule->logif);
	nvlist_add_number(nvl, "quick", rule->quick);
	nvlist_add_number(nvl, "ifnot", rule->ifnot);
	nvlist_add_number(nvl, "match_tag_not", rule->match_tag_not);
	nvlist_add_number(nvl, "natpass", rule->natpass);

	nvlist_add_number(nvl, "keep_state", rule->keep_state);
	nvlist_add_number(nvl, "af", rule->af);
	nvlist_add_number(nvl, "proto", rule->proto);
	nvlist_add_number(nvl, "type", rule->type);
	nvlist_add_number(nvl, "code", rule->code);
	nvlist_add_number(nvl, "flags", rule->flags);
	nvlist_add_number(nvl, "flagset", rule->flagset);
	nvlist_add_number(nvl, "min_ttl", rule->min_ttl);
	nvlist_add_number(nvl, "allow_opts", rule->allow_opts);
	nvlist_add_number(nvl, "rt", rule->rt);
	nvlist_add_number(nvl, "return_ttl", rule->return_ttl);
	nvlist_add_number(nvl, "tos", rule->tos);
	nvlist_add_number(nvl, "set_tos", rule->set_tos);
	nvlist_add_number(nvl, "anchor_relative", rule->anchor_relative);
	nvlist_add_number(nvl, "anchor_wildcard", rule->anchor_wildcard);

	nvlist_add_number(nvl, "flush", rule->flush);
	nvlist_add_number(nvl, "prio", rule->prio);

	pf_uint8_array_nv(nvl, "set_prio", rule->set_prio, 2);

	tmp = pf_divert_to_nvdivert(rule);
	if (tmp == NULL)
		goto error;
	nvlist_add_nvlist(nvl, "divert", tmp);
	nvlist_destroy(tmp);

	return (nvl);

error:
	nvlist_destroy(nvl);
	return (NULL);
}

static int
pf_nvstate_cmp_to_state_cmp(const nvlist_t *nvl, struct pf_state_cmp *cmp)
{
	int error = 0;

	bzero(cmp, sizeof(*cmp));

	PFNV_CHK(pf_nvuint64(nvl, "id", &cmp->id));
	PFNV_CHK(pf_nvuint32(nvl, "creatorid", &cmp->creatorid));
	PFNV_CHK(pf_nvuint8(nvl, "direction", &cmp->direction));

errout:
	return (error);
}

int
pf_nvstate_kill_to_kstate_kill(const nvlist_t *nvl,
    struct pf_kstate_kill *kill)
{
	int error = 0;

	bzero(kill, sizeof(*kill));

	if (! nvlist_exists_nvlist(nvl, "cmp"))
		return (EINVAL);

	PFNV_CHK(pf_nvstate_cmp_to_state_cmp(nvlist_get_nvlist(nvl, "cmp"),
	    &kill->psk_pfcmp));
	PFNV_CHK(pf_nvuint8(nvl, "af", &kill->psk_af));
	PFNV_CHK(pf_nvint(nvl, "proto", &kill->psk_proto));

	if (! nvlist_exists_nvlist(nvl, "src"))
		return (EINVAL);
	PFNV_CHK(pf_nvrule_addr_to_rule_addr(nvlist_get_nvlist(nvl, "src"),
	    &kill->psk_src));
	if (! nvlist_exists_nvlist(nvl, "dst"))
		return (EINVAL);
	PFNV_CHK(pf_nvrule_addr_to_rule_addr(nvlist_get_nvlist(nvl, "dst"),
	    &kill->psk_dst));
	if (nvlist_exists_nvlist(nvl, "rt_addr")) {
		PFNV_CHK(pf_nvrule_addr_to_rule_addr(
		    nvlist_get_nvlist(nvl, "rt_addr"), &kill->psk_rt_addr));
	}

	PFNV_CHK(pf_nvstring(nvl, "ifname", kill->psk_ifname,
	    sizeof(kill->psk_ifname)));
	PFNV_CHK(pf_nvstring(nvl, "label", kill->psk_label,
	    sizeof(kill->psk_label)));
	PFNV_CHK(pf_nvbool(nvl, "kill_match", &kill->psk_kill_match));

errout:
	return (error);
}

static nvlist_t *
pf_state_key_to_nvstate_key(const struct pf_state_key *key)
{
	nvlist_t	*nvl, *tmp;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (NULL);

	for (int i = 0; i < 2; i++) {
		tmp = pf_addr_to_nvaddr(&key->addr[i]);
		if (tmp == NULL)
			goto errout;
		nvlist_append_nvlist_array(nvl, "addr", tmp);
		nvlist_destroy(tmp);
		nvlist_append_number_array(nvl, "port", key->port[i]);
	}
	nvlist_add_number(nvl, "af", key->af);
	nvlist_add_number(nvl, "proto", key->proto);

	return (nvl);

errout:
	nvlist_destroy(nvl);
	return (NULL);
}

static nvlist_t *
pf_state_peer_to_nvstate_peer(const struct pf_state_peer *peer)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (NULL);

	nvlist_add_number(nvl, "seqlo", peer->seqlo);
	nvlist_add_number(nvl, "seqhi", peer->seqhi);
	nvlist_add_number(nvl, "seqdiff", peer->seqdiff);
	nvlist_add_number(nvl, "state", peer->state);
	nvlist_add_number(nvl, "wscale", peer->wscale);

	return (nvl);
}

nvlist_t *
pf_state_to_nvstate(const struct pf_kstate *s)
{
	nvlist_t	*nvl, *tmp;
	uint32_t	 expire, flags = 0;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (NULL);

	nvlist_add_number(nvl, "id", s->id);
	nvlist_add_string(nvl, "ifname", s->kif->pfik_name);
	nvlist_add_string(nvl, "orig_ifname", s->orig_kif->pfik_name);

	tmp = pf_state_key_to_nvstate_key(s->key[PF_SK_STACK]);
	if (tmp == NULL)
		goto errout;
	nvlist_add_nvlist(nvl, "stack_key", tmp);
	nvlist_destroy(tmp);

	tmp = pf_state_key_to_nvstate_key(s->key[PF_SK_WIRE]);
	if (tmp == NULL)
		goto errout;
	nvlist_add_nvlist(nvl, "wire_key", tmp);
	nvlist_destroy(tmp);

	tmp = pf_state_peer_to_nvstate_peer(&s->src);
	if (tmp == NULL)
		goto errout;
	nvlist_add_nvlist(nvl, "src", tmp);
	nvlist_destroy(tmp);

	tmp = pf_state_peer_to_nvstate_peer(&s->dst);
	if (tmp == NULL)
		goto errout;
	nvlist_add_nvlist(nvl, "dst", tmp);
	nvlist_destroy(tmp);

	tmp = pf_addr_to_nvaddr(&s->rt_addr);
	if (tmp == NULL)
		goto errout;
	nvlist_add_nvlist(nvl, "rt_addr", tmp);
	nvlist_destroy(tmp);

	nvlist_add_number(nvl, "rule", s->rule.ptr ? s->rule.ptr->nr : -1);
	nvlist_add_number(nvl, "anchor",
	    s->anchor.ptr ? s->anchor.ptr->nr : -1);
	nvlist_add_number(nvl, "nat_rule",
	    s->nat_rule.ptr ? s->nat_rule.ptr->nr : -1);
	nvlist_add_number(nvl, "creation", s->creation);

	expire = pf_state_expires(s);
	if (expire <= time_uptime)
		expire = 0;
	else
		expire = expire - time_uptime;
	nvlist_add_number(nvl, "expire", expire);

	for (int i = 0; i < 2; i++) {
		nvlist_append_number_array(nvl, "packets",
		    s->packets[i]);
		nvlist_append_number_array(nvl, "bytes",
		    s->bytes[i]);
	}

	nvlist_add_number(nvl, "creatorid", s->creatorid);
	nvlist_add_number(nvl, "direction", s->direction);
	nvlist_add_number(nvl, "state_flags", s->state_flags);
	if (s->src_node)
		flags |= PFSYNC_FLAG_SRCNODE;
	if (s->nat_src_node)
		flags |= PFSYNC_FLAG_NATSRCNODE;
	nvlist_add_number(nvl, "sync_flags", flags);

	return (nvl);

errout:
	nvlist_destroy(nvl);
	return (NULL);
}

static int
pf_nveth_rule_addr_to_keth_rule_addr(const nvlist_t *nvl,
    struct pf_keth_rule_addr *krule)
{
	static const u_int8_t EMPTY_MAC[ETHER_ADDR_LEN] = { 0 };
	int error = 0;

	PFNV_CHK(pf_nvbinary(nvl, "addr", &krule->addr, sizeof(krule->addr)));
	PFNV_CHK(pf_nvbool(nvl, "neg", &krule->neg));
	if (nvlist_exists_binary(nvl, "mask"))
		PFNV_CHK(pf_nvbinary(nvl, "mask", &krule->mask,
		    sizeof(krule->mask)));

	/* To make checks for 'is this address set?' easier. */
	if (memcmp(krule->addr, EMPTY_MAC, ETHER_ADDR_LEN) != 0)
		krule->isset = 1;

errout:
	return (error);
}

static nvlist_t*
pf_keth_rule_addr_to_nveth_rule_addr(const struct pf_keth_rule_addr *krule)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (NULL);

	nvlist_add_binary(nvl, "addr", &krule->addr, sizeof(krule->addr));
	nvlist_add_binary(nvl, "mask", &krule->mask, sizeof(krule->mask));
	nvlist_add_bool(nvl, "neg", krule->neg);

	return (nvl);
}

nvlist_t*
pf_keth_rule_to_nveth_rule(const struct pf_keth_rule *krule)
{
	nvlist_t *nvl, *addr;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (NULL);

	for (int i = 0; i < PF_RULE_MAX_LABEL_COUNT; i++) {
		nvlist_append_string_array(nvl, "labels", krule->label[i]);
	}
	nvlist_add_number(nvl, "ridentifier", krule->ridentifier);

	nvlist_add_number(nvl, "nr", krule->nr);
	nvlist_add_bool(nvl, "quick", krule->quick);
	nvlist_add_string(nvl, "ifname", krule->ifname);
	nvlist_add_bool(nvl, "ifnot", krule->ifnot);
	nvlist_add_number(nvl, "direction", krule->direction);
	nvlist_add_number(nvl, "proto", krule->proto);
	nvlist_add_string(nvl, "match_tagname", krule->match_tagname);
	nvlist_add_number(nvl, "match_tag", krule->match_tag);
	nvlist_add_bool(nvl, "match_tag_not", krule->match_tag_not);

	addr = pf_keth_rule_addr_to_nveth_rule_addr(&krule->src);
	if (addr == NULL) {
		nvlist_destroy(nvl);
		return (NULL);
	}
	nvlist_add_nvlist(nvl, "src", addr);
	nvlist_destroy(addr);

	addr = pf_keth_rule_addr_to_nveth_rule_addr(&krule->dst);
	if (addr == NULL) {
		nvlist_destroy(nvl);
		return (NULL);
	}
	nvlist_add_nvlist(nvl, "dst", addr);
	nvlist_destroy(addr);

	addr = pf_rule_addr_to_nvrule_addr(&krule->ipsrc);
	if (addr == NULL) {
		nvlist_destroy(nvl);
		return (NULL);
	}
	nvlist_add_nvlist(nvl, "ipsrc", addr);
	nvlist_destroy(addr);

	addr = pf_rule_addr_to_nvrule_addr(&krule->ipdst);
	if (addr == NULL) {
		nvlist_destroy(nvl);
		return (NULL);
	}
	nvlist_add_nvlist(nvl, "ipdst", addr);
	nvlist_destroy(addr);

	nvlist_add_number(nvl, "evaluations",
	    counter_u64_fetch(krule->evaluations));
	nvlist_add_number(nvl, "packets-in",
	    counter_u64_fetch(krule->packets[0]));
	nvlist_add_number(nvl, "packets-out",
	    counter_u64_fetch(krule->packets[1]));
	nvlist_add_number(nvl, "bytes-in",
	    counter_u64_fetch(krule->bytes[0]));
	nvlist_add_number(nvl, "bytes-out",
	    counter_u64_fetch(krule->bytes[1]));

	nvlist_add_number(nvl, "timestamp", pf_get_timestamp(krule));
	nvlist_add_string(nvl, "qname", krule->qname);
	nvlist_add_string(nvl, "tagname", krule->tagname);

	nvlist_add_number(nvl, "dnpipe", krule->dnpipe);
	nvlist_add_number(nvl, "dnflags", krule->dnflags);

	nvlist_add_number(nvl, "anchor_relative", krule->anchor_relative);
	nvlist_add_number(nvl, "anchor_wildcard", krule->anchor_wildcard);

	nvlist_add_string(nvl, "bridge_to", krule->bridge_to_name);
	nvlist_add_number(nvl, "action", krule->action);

	return (nvl);
}

int
pf_nveth_rule_to_keth_rule(const nvlist_t *nvl,
    struct pf_keth_rule *krule)
{
	int error = 0;

#define ERROUT(x)	ERROUT_FUNCTION(errout, x)

	bzero(krule, sizeof(*krule));

	if (nvlist_exists_string_array(nvl, "labels")) {
		const char *const *strs;
		size_t items;
		int ret;

		strs = nvlist_get_string_array(nvl, "labels", &items);
		if (items > PF_RULE_MAX_LABEL_COUNT)
			ERROUT(E2BIG);

		for (size_t i = 0; i < items; i++) {
			ret = strlcpy(krule->label[i], strs[i],
			    sizeof(krule->label[0]));
			if (ret >= sizeof(krule->label[0]))
				ERROUT(E2BIG);
		}
	}

	PFNV_CHK(pf_nvuint32_opt(nvl, "ridentifier", &krule->ridentifier, 0));

	PFNV_CHK(pf_nvuint32(nvl, "nr", &krule->nr));
	PFNV_CHK(pf_nvbool(nvl, "quick", &krule->quick));
	PFNV_CHK(pf_nvstring(nvl, "ifname", krule->ifname,
	    sizeof(krule->ifname)));
	PFNV_CHK(pf_nvbool(nvl, "ifnot", &krule->ifnot));
	PFNV_CHK(pf_nvuint8(nvl, "direction", &krule->direction));
	PFNV_CHK(pf_nvuint16(nvl, "proto", &krule->proto));

	if (nvlist_exists_nvlist(nvl, "src")) {
		error = pf_nveth_rule_addr_to_keth_rule_addr(
		    nvlist_get_nvlist(nvl, "src"), &krule->src);
		if (error)
			return (error);
	}
	if (nvlist_exists_nvlist(nvl, "dst")) {
		error = pf_nveth_rule_addr_to_keth_rule_addr(
		    nvlist_get_nvlist(nvl, "dst"), &krule->dst);
		if (error)
			return (error);
	}

	if (nvlist_exists_nvlist(nvl, "ipsrc")) {
		error = pf_nvrule_addr_to_rule_addr(
		    nvlist_get_nvlist(nvl, "ipsrc"), &krule->ipsrc);
		if (error != 0)
			return (error);

		if (krule->ipsrc.addr.type != PF_ADDR_ADDRMASK &&
		    krule->ipsrc.addr.type != PF_ADDR_TABLE)
			return (EINVAL);
	}

	if (nvlist_exists_nvlist(nvl, "ipdst")) {
		error = pf_nvrule_addr_to_rule_addr(
		    nvlist_get_nvlist(nvl, "ipdst"), &krule->ipdst);
		if (error != 0)
			return (error);

		if (krule->ipdst.addr.type != PF_ADDR_ADDRMASK &&
		    krule->ipdst.addr.type != PF_ADDR_TABLE)
			return (EINVAL);
	}

	if (nvlist_exists_string(nvl, "match_tagname")) {
		PFNV_CHK(pf_nvstring(nvl, "match_tagname", krule->match_tagname,
		    sizeof(krule->match_tagname)));
		PFNV_CHK(pf_nvbool(nvl, "match_tag_not", &krule->match_tag_not));
	}

	PFNV_CHK(pf_nvstring(nvl, "qname", krule->qname, sizeof(krule->qname)));
	PFNV_CHK(pf_nvstring(nvl, "tagname", krule->tagname,
	    sizeof(krule->tagname)));

	PFNV_CHK(pf_nvuint16_opt(nvl, "dnpipe", &krule->dnpipe, 0));
	PFNV_CHK(pf_nvuint32_opt(nvl, "dnflags", &krule->dnflags, 0));
	PFNV_CHK(pf_nvstring(nvl, "bridge_to", krule->bridge_to_name,
	    sizeof(krule->bridge_to_name)));

	PFNV_CHK(pf_nvuint8(nvl, "action", &krule->action));

	if (krule->action != PF_PASS && krule->action != PF_DROP &&
	    krule->action != PF_MATCH)
		return (EBADMSG);

#undef ERROUT
errout:
	return (error);
}
