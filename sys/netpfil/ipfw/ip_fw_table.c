/*-
 * Copyright (c) 2004 Ruslan Ermilov and Vsevolod Lobko.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Lookup table support for ipfw.
 *
 * This file contains handlers for all generic tables' operations:
 * add/del/flush entries, list/dump tables etc..
 *
 * Table data modification is protected by both UH and runtimg lock
 * while reading configuration/data is protected by UH lock.
 *
 * Lookup algorithms for all table types are located in ip_fw_table_algo.c
 */

#include "opt_ipfw.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/queue.h>
#include <net/if.h>	/* ip_fw.h requires IFNAMSIZ */
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/ip_var.h>	/* struct ipfw_rule_ref */
#include <netinet/ip_fw.h>

#include <netpfil/ipfw/ip_fw_private.h>
#include <netpfil/ipfw/ip_fw_table.h>


 /*
 * Table has the following `type` concepts:
 *
 * `no.type` represents lookup key type (cidr, ifp, uid, etc..)
 * `ta->atype` represents exact lookup algorithm.
 *     For example, we can use more efficient search schemes if we plan
 *     to use some specific table for storing host-routes only.
 * `ftype` (at the moment )is pure userland field helping to properly
 *     format value data e.g. "value is IPv4 nexthop" or "value is DSCP"
 *     or "value is port".
 *
 */
struct table_config {
	struct named_object	no;
	uint8_t		vtype;		/* format table type */
	uint8_t		linked;		/* 1 if already linked */
	uint8_t		tflags;		/* type flags */
	uint8_t		spare;
	uint32_t	count;		/* Number of records */
	uint32_t	limit;		/* Max number of records */
	uint64_t	flags;		/* state flags */
	char		tablename[64];	/* table name */
	struct table_algo	*ta;	/* Callbacks for given algo */
	void		*astate;	/* algorithm state */
	struct table_info	ti;	/* data to put to table_info */
};

struct tables_config {
	struct namedobj_instance	*namehash;
	int				algo_count;
	struct table_algo 		*algo[256];
	struct table_algo		*def_algo[IPFW_TABLE_MAXTYPE + 1];
};

static struct table_config *find_table(struct namedobj_instance *ni,
    struct tid_info *ti);
static struct table_config *alloc_table_config(struct ip_fw_chain *ch,
    struct tid_info *ti, struct table_algo *ta, char *adata, uint8_t tflags,
    uint8_t vtype);
static void free_table_config(struct namedobj_instance *ni,
    struct table_config *tc);
static int create_table_internal(struct ip_fw_chain *ch, struct tid_info *ti,
    char *aname, ipfw_xtable_info *i);
static void link_table(struct ip_fw_chain *chain, struct table_config *tc);
static void unlink_table(struct ip_fw_chain *chain, struct table_config *tc);
static void free_table_state(void **state, void **xstate, uint8_t type);
static int export_tables(struct ip_fw_chain *ch, ipfw_obj_lheader *olh,
    struct sockopt_data *sd);
static void export_table_info(struct ip_fw_chain *ch, struct table_config *tc,
    ipfw_xtable_info *i);
static int dump_table_tentry(void *e, void *arg);
static int dump_table_xentry(void *e, void *arg);

static int ipfw_dump_table_v0(struct ip_fw_chain *ch, struct sockopt_data *sd);
static int ipfw_dump_table_v1(struct ip_fw_chain *ch, struct sockopt_data *sd);
static int ipfw_manage_table_ent_v0(struct ip_fw_chain *ch, ip_fw3_opheader *op3,
    struct sockopt_data *sd);
static int ipfw_manage_table_ent_v1(struct ip_fw_chain *ch, ip_fw3_opheader *op3,
    struct sockopt_data *sd);

static int modify_table(struct ip_fw_chain *ch, struct table_config *tc,
    struct table_algo *ta, void *ta_buf, uint64_t pflags);
static int destroy_table(struct ip_fw_chain *ch, struct tid_info *ti);

static struct table_algo *find_table_algo(struct tables_config *tableconf,
    struct tid_info *ti, char *name);

#define	CHAIN_TO_TCFG(chain)	((struct tables_config *)(chain)->tblcfg)
#define	CHAIN_TO_NI(chain)	(CHAIN_TO_TCFG(chain)->namehash)
#define	KIDX_TO_TI(ch, k)	(&(((struct table_info *)(ch)->tablestate)[k]))


int
add_table_entry(struct ip_fw_chain *ch, struct tid_info *ti,
    struct tentry_info *tei)
{
	struct table_config *tc;
	struct table_algo *ta;
	struct namedobj_instance *ni;
	uint16_t kidx;
	int error;
	uint32_t num;
	uint64_t aflags;
	ipfw_xtable_info xi;
	char ta_buf[128];

	IPFW_UH_WLOCK(ch);
	ni = CHAIN_TO_NI(ch);

	/*
	 * Find and reference existing table.
	 */
	ta = NULL;
	if ((tc = find_table(ni, ti)) != NULL) {
		/* check table type */
		if (tc->no.type != ti->type) {
			IPFW_UH_WUNLOCK(ch);
			return (EINVAL);
		}

		/* Try to exit early on limit hit */
		if (tc->limit != 0 && tc->count == tc->limit &&
		    (tei->flags & TEI_FLAGS_UPDATE) == 0) {
				IPFW_UH_WUNLOCK(ch);
				return (EFBIG);
		}

		/* Reference and unlock */
		tc->no.refcnt++;
		ta = tc->ta;
		aflags = tc->flags;
	}
	IPFW_UH_WUNLOCK(ch);

	if (tc == NULL) {
		/* Compability mode: create new table for old clients */
		if ((tei->flags & TEI_FLAGS_COMPAT) == 0)
			return (ESRCH);

		memset(&xi, 0, sizeof(xi));
		xi.vtype = IPFW_VTYPE_U32;

		error = create_table_internal(ch, ti, NULL, &xi);

		if (error != 0)
			return (error);

		/* Let's try to find & reference another time */
		IPFW_UH_WLOCK(ch);
		if ((tc = find_table(ni, ti)) == NULL) {
			IPFW_UH_WUNLOCK(ch);
			return (EINVAL);
		}

		if (tc->no.type != ti->type) {
			IPFW_UH_WUNLOCK(ch);
			return (EINVAL);
		}

		/* Reference and unlock */
		tc->no.refcnt++;
		ta = tc->ta;
		aflags = tc->flags;

		IPFW_UH_WUNLOCK(ch);
	}

	if (aflags != 0) {

		/*
		 * Previous add/delete call returned non-zero state.
		 * Run appropriate handler.
		 */
		error = modify_table(ch, tc, ta, &ta_buf, aflags);
		if (error != 0)
			return (error);
	}

	/* Prepare record (allocate memory) */
	memset(&ta_buf, 0, sizeof(ta_buf));
	error = ta->prepare_add(ch, tei, &ta_buf);
	if (error != 0)
		return (error);

	IPFW_UH_WLOCK(ch);

	ni = CHAIN_TO_NI(ch);

	/* Drop reference we've used in first search */
	tc->no.refcnt--;
	/* Update aflags since it can be changed after previous read */
	aflags = tc->flags;
	
	/* Check limit before adding */
	if (tc->limit != 0 && tc->count == tc->limit) {
		if ((tei->flags & TEI_FLAGS_UPDATE) == 0) {
			IPFW_UH_WUNLOCK(ch);
			return (EFBIG);
		}

		/*
		 * We have UPDATE flag set.
		 * Permit updating record (if found),
		 * but restrict adding new one since we've
		 * already hit the limit.
		 */
		tei->flags |= TEI_FLAGS_DONTADD;
	}

	/* We've got valid table in @tc. Let's add data */
	kidx = tc->no.kidx;
	ta = tc->ta;
	num = 0;

	IPFW_WLOCK(ch);
	error = ta->add(tc->astate, KIDX_TO_TI(ch, kidx), tei, &ta_buf,
	    &aflags, &num);
	IPFW_WUNLOCK(ch);

	/* Update number of records. */
	if (error == 0)
		tc->count += num;

	tc->flags = aflags;

	IPFW_UH_WUNLOCK(ch);

	/* Run cleaning callback anyway */
	ta->flush_entry(ch, tei, &ta_buf);

	return (error);
}

int
del_table_entry(struct ip_fw_chain *ch, struct tid_info *ti,
    struct tentry_info *tei)
{
	struct table_config *tc;
	struct table_algo *ta;
	struct namedobj_instance *ni;
	uint16_t kidx;
	int error;
	uint32_t num;
	uint64_t aflags;
	char ta_buf[128];

	IPFW_UH_WLOCK(ch);
	ni = CHAIN_TO_NI(ch);
	if ((tc = find_table(ni, ti)) == NULL) {
		IPFW_UH_WUNLOCK(ch);
		return (ESRCH);
	}

	if (tc->no.type != ti->type) {
		IPFW_UH_WUNLOCK(ch);
		return (EINVAL);
	}

	aflags = tc->flags;
	ta = tc->ta;

	if (aflags != 0) {

		/*
		 * Give the chance to algo to shrink its state.
		 */
		tc->no.refcnt++;
		IPFW_UH_WUNLOCK(ch);
		memset(&ta_buf, 0, sizeof(ta_buf));

		error = modify_table(ch, tc, ta, &ta_buf, aflags);

		IPFW_UH_WLOCK(ch);
		tc->no.refcnt--;
		aflags = tc->flags;

		if (error != 0) {
			IPFW_UH_WUNLOCK(ch);
			return (error);
		}
	}

	/*
	 * We assume ta_buf size is enough for storing
	 * prepare_del() key, so we're running under UH_LOCK here.
	 */
	memset(&ta_buf, 0, sizeof(ta_buf));
	if ((error = ta->prepare_del(ch, tei, &ta_buf)) != 0) {
		IPFW_UH_WUNLOCK(ch);
		return (error);
	}

	kidx = tc->no.kidx;
	num = 0;

	IPFW_WLOCK(ch);
	error = ta->del(tc->astate, KIDX_TO_TI(ch, kidx), tei, &ta_buf,
	    &aflags, &num);
	IPFW_WUNLOCK(ch);

	if (error == 0)
		tc->count -= num;
	tc->flags = aflags;

	IPFW_UH_WUNLOCK(ch);

	ta->flush_entry(ch, tei, &ta_buf);

	return (error);
}

/*
 * Runs callbacks to modify algo state (typically, table resize).
 *
 * Callbacks order:
 * 1) alloc_modify (no locks, M_WAITOK) - alloc new state based on @pflags.
 * 2) prepare_modifyt (UH_WLOCK) - copy old data into new storage
 * 3) modify (UH_WLOCK + WLOCK) - switch pointers
 * 4) flush_modify (no locks) - free state, if needed
 */
static int
modify_table(struct ip_fw_chain *ch, struct table_config *tc,
    struct table_algo *ta, void *ta_buf, uint64_t pflags)
{
	struct table_info *ti;
	int error;

	error = ta->prepare_mod(ta_buf, &pflags);
	if (error != 0)
		return (error);

	IPFW_UH_WLOCK(ch);
	ti = KIDX_TO_TI(ch, tc->no.kidx);

	error = ta->fill_mod(tc->astate, ti, ta_buf, &pflags);

	/*
	 * prepare_mofify may return zero in @pflags to
	 * indicate that modifications are not unnesessary.
	 */

	if (error == 0 && pflags != 0) {
		/* Do actual modification */
		IPFW_WLOCK(ch);
		ta->modify(tc->astate, ti, ta_buf, pflags);
		IPFW_WUNLOCK(ch);
	}

	IPFW_UH_WUNLOCK(ch);

	ta->flush_mod(ta_buf);

	return (error);
}

int
ipfw_manage_table_ent(struct ip_fw_chain *ch, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	int error;

	switch (op3->version) {
	case 0:
		error = ipfw_manage_table_ent_v0(ch, op3, sd);
		break;
	case 1:
		error = ipfw_manage_table_ent_v1(ch, op3, sd);
		break;
	default:
		error = ENOTSUP;
	}

	return (error);
}

/*
 * Adds or deletes record in table.
 * Data layout (v0):
 * Request: [ ip_fw3_opheader ipfw_table_xentry ]
 *
 * Returns 0 on success
 */
static int
ipfw_manage_table_ent_v0(struct ip_fw_chain *ch, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	ipfw_table_xentry *xent;
	struct tentry_info tei;
	struct tid_info ti;
	int error, hdrlen, read;

	hdrlen = offsetof(ipfw_table_xentry, k);

	/* Check minimum header size */
	if (sd->valsize < (sizeof(*op3) + hdrlen))
		return (EINVAL);

	read = sizeof(ip_fw3_opheader);

	/* Check if xentry len field is valid */
	xent = (ipfw_table_xentry *)(op3 + 1);
	if (xent->len < hdrlen || xent->len + read > sd->valsize)
		return (EINVAL);
	
	memset(&tei, 0, sizeof(tei));
	tei.paddr = &xent->k;
	tei.masklen = xent->masklen;
	tei.value = xent->value;
	/* Old requests compability */
	tei.flags = TEI_FLAGS_COMPAT;
	if (xent->type == IPFW_TABLE_CIDR) {
		if (xent->len - hdrlen == sizeof(in_addr_t))
			tei.subtype = AF_INET;
		else
			tei.subtype = AF_INET6;
	}

	memset(&ti, 0, sizeof(ti));
	ti.uidx = xent->tbl;
	ti.type = xent->type;

	error = (op3->opcode == IP_FW_TABLE_XADD) ?
	    add_table_entry(ch, &ti, &tei) :
	    del_table_entry(ch, &ti, &tei);

	return (error);
}

/*
 * Adds or deletes record in table.
 * Data layout (v1)(current):
 * Request: [ ipfw_obj_header
 *   ipfw_obj_ctlv(IPFW_TLV_TBLENT_LIST) [ ipfw_obj_tentry x N ]
 * ]
 *
 * Returns 0 on success
 */
static int
ipfw_manage_table_ent_v1(struct ip_fw_chain *ch, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	ipfw_obj_tentry *tent;
	ipfw_obj_ctlv *ctlv;
	ipfw_obj_header *oh;
	struct tentry_info tei;
	struct tid_info ti;
	int error, read;

	/* Check minimum header size */
	if (sd->valsize < (sizeof(*oh) + sizeof(*ctlv)))
		return (EINVAL);

	/* Check if passed data is too long */
	if (sd->valsize != sd->kavail)
		return (EINVAL);

	oh = (ipfw_obj_header *)sd->kbuf;

	/* Basic length checks for TLVs */
	if (oh->ntlv.head.length != sizeof(oh->ntlv))
		return (EINVAL);

	read = sizeof(*oh);

	ctlv = (ipfw_obj_ctlv *)(oh + 1);
	if (ctlv->head.length + read != sd->valsize)
		return (EINVAL);

	/*
	 * TODO: permit adding multiple entries for given table
	 * at once
	 */
	if (ctlv->count != 1)
		return (EOPNOTSUPP);

	read += sizeof(*ctlv);

	/* Assume tentry may grow to support larger keys */
	tent = (ipfw_obj_tentry *)(ctlv + 1);
	if (tent->head.length < sizeof(*tent) ||
	    tent->head.length + read > sd->valsize)
		return (EINVAL);

	/* Convert data into kernel request objects */
	memset(&tei, 0, sizeof(tei));
	tei.paddr = &tent->k;
	tei.subtype = tent->subtype;
	tei.masklen = tent->masklen;
	if (tent->head.flags & IPFW_TF_UPDATE)
		tei.flags |= TEI_FLAGS_UPDATE;
	tei.value = tent->value;

	objheader_to_ti(oh, &ti);
	ti.type = oh->ntlv.type;
	ti.uidx = tent->idx;

	error = (oh->opheader.opcode == IP_FW_TABLE_XADD) ?
	    add_table_entry(ch, &ti, &tei) :
	    del_table_entry(ch, &ti, &tei);

	return (error);
}

/*
 * Looks up an entry in given table.
 * Data layout (v0)(current):
 * Request: [ ipfw_obj_header ipfw_obj_tentry ]
 * Reply: [ ipfw_obj_header ipfw_obj_tentry ]
 *
 * Returns 0 on success
 */
int
ipfw_find_table_entry(struct ip_fw_chain *ch, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	ipfw_obj_tentry *tent;
	ipfw_obj_header *oh;
	struct tid_info ti;
	struct table_config *tc;
	struct table_algo *ta;
	struct table_info *kti;
	struct namedobj_instance *ni;
	int error;
	size_t sz;

	/* Check minimum header size */
	sz = sizeof(*oh) + sizeof(*tent);
	if (sd->valsize != sz)
		return (EINVAL);

	oh = (struct _ipfw_obj_header *)ipfw_get_sopt_header(sd, sz);
	tent = (ipfw_obj_tentry *)(oh + 1);

	/* Basic length checks for TLVs */
	if (oh->ntlv.head.length != sizeof(oh->ntlv))
		return (EINVAL);

	objheader_to_ti(oh, &ti);
	ti.type = oh->ntlv.type;
	ti.uidx = tent->idx;

	IPFW_UH_RLOCK(ch);
	ni = CHAIN_TO_NI(ch);

	/*
	 * Find existing table and check its type .
	 */
	ta = NULL;
	if ((tc = find_table(ni, &ti)) == NULL) {
		IPFW_UH_RUNLOCK(ch);
		return (ESRCH);
	}

	/* check table type */
	if (tc->no.type != ti.type) {
		IPFW_UH_RUNLOCK(ch);
		return (EINVAL);
	}

	kti = KIDX_TO_TI(ch, tc->no.kidx);
	ta = tc->ta;

	if (ta->find_tentry == NULL)
		return (ENOTSUP);

	error = ta->find_tentry(tc->astate, kti, tent);

	IPFW_UH_RUNLOCK(ch);

	return (error);
}

int
ipfw_flush_table(struct ip_fw_chain *ch, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	int error;
	struct _ipfw_obj_header *oh;
	struct tid_info ti;

	if (sd->valsize != sizeof(*oh))
		return (EINVAL);

	oh = (struct _ipfw_obj_header *)op3;
	objheader_to_ti(oh, &ti);

	if (op3->opcode == IP_FW_TABLE_XDESTROY)
		error = destroy_table(ch, &ti);
	else if (op3->opcode == IP_FW_TABLE_XFLUSH)
		error = flush_table(ch, &ti);
	else
		return (ENOTSUP);

	return (error);
}

/*
 * Flushes all entries in given table.
 * Data layout (v0)(current):
 * Request: [ ip_fw3_opheader ]
 *
 * Returns 0 on success
 */
int
flush_table(struct ip_fw_chain *ch, struct tid_info *ti)
{
	struct namedobj_instance *ni;
	struct table_config *tc;
	struct table_algo *ta;
	struct table_info ti_old, ti_new, *tablestate;
	void *astate_old, *astate_new;
	char algostate[64], *pstate;
	int error;
	uint16_t kidx;
	uint8_t tflags;

	/*
	 * Stage 1: save table algoritm.
	 * Reference found table to ensure it won't disappear.
	 */
	IPFW_UH_WLOCK(ch);
	ni = CHAIN_TO_NI(ch);
	if ((tc = find_table(ni, ti)) == NULL) {
		IPFW_UH_WUNLOCK(ch);
		return (ESRCH);
	}
	ta = tc->ta;
	tc->no.refcnt++;
	/* Save statup algo parameters */
	if (ta->print_config != NULL) {
		ta->print_config(tc->astate, KIDX_TO_TI(ch, tc->no.kidx),
		    algostate, sizeof(algostate));
		pstate = algostate;
	} else
		pstate = NULL;
	tflags = tc->tflags;
	IPFW_UH_WUNLOCK(ch);

	/*
	 * Stage 2: allocate new table instance using same algo.
	 */
	memset(&ti_new, 0, sizeof(struct table_info));
	if ((error = ta->init(ch, &astate_new, &ti_new, pstate, tflags)) != 0) {
		IPFW_UH_WLOCK(ch);
		tc->no.refcnt--;
		IPFW_UH_WUNLOCK(ch);
		return (error);
	}

	/*
	 * Stage 3: swap old state pointers with newly-allocated ones.
	 * Decrease refcount.
	 */
	IPFW_UH_WLOCK(ch);

	ni = CHAIN_TO_NI(ch);
	kidx = tc->no.kidx;
	tablestate = (struct table_info *)ch->tablestate;

	IPFW_WLOCK(ch);
	ti_old = tablestate[kidx];
	tablestate[kidx] = ti_new;
	IPFW_WUNLOCK(ch);

	astate_old = tc->astate;
	tc->astate = astate_new;
	tc->ti = ti_new;
	tc->count = 0;
	tc->no.refcnt--;

	IPFW_UH_WUNLOCK(ch);

	/*
	 * Stage 4: perform real flush.
	 */
	ta->destroy(astate_old, &ti_old);

	return (0);
}

/*
 * Destroys table specified by @ti.
 * Data layout (v0)(current):
 * Request: [ ip_fw3_opheader ]
 *
 * Returns 0 on success
 */
static int
destroy_table(struct ip_fw_chain *ch, struct tid_info *ti)
{
	struct namedobj_instance *ni;
	struct table_config *tc;

	IPFW_UH_WLOCK(ch);

	ni = CHAIN_TO_NI(ch);
	if ((tc = find_table(ni, ti)) == NULL) {
		IPFW_UH_WUNLOCK(ch);
		return (ESRCH);
	}

	/* Do not permit destroying referenced tables */
	if (tc->no.refcnt > 0) {
		IPFW_UH_WUNLOCK(ch);
		return (EBUSY);
	}

	IPFW_WLOCK(ch);
	unlink_table(ch, tc);
	IPFW_WUNLOCK(ch);

	/* Free obj index */
	if (ipfw_objhash_free_idx(ni, tc->no.kidx) != 0)
		printf("Error unlinking kidx %d from table %s\n",
		    tc->no.kidx, tc->tablename);

	IPFW_UH_WUNLOCK(ch);

	free_table_config(ni, tc);

	return (0);
}

static void
destroy_table_locked(struct namedobj_instance *ni, struct named_object *no,
    void *arg)
{

	unlink_table((struct ip_fw_chain *)arg, (struct table_config *)no);
	if (ipfw_objhash_free_idx(ni, no->kidx) != 0)
		printf("Error unlinking kidx %d from table %s\n",
		    no->kidx, no->name);
	free_table_config(ni, (struct table_config *)no);
}

void
ipfw_destroy_tables(struct ip_fw_chain *ch)
{

	/* Remove all tables from working set */
	IPFW_UH_WLOCK(ch);
	IPFW_WLOCK(ch);
	ipfw_objhash_foreach(CHAIN_TO_NI(ch), destroy_table_locked, ch);
	IPFW_WUNLOCK(ch);
	IPFW_UH_WUNLOCK(ch);

	/* Free pointers itself */
	free(ch->tablestate, M_IPFW);

	ipfw_table_algo_destroy(ch);

	ipfw_objhash_destroy(CHAIN_TO_NI(ch));
	free(CHAIN_TO_TCFG(ch), M_IPFW);
}

int
ipfw_init_tables(struct ip_fw_chain *ch)
{
	struct tables_config *tcfg;

	/* Allocate pointers */
	ch->tablestate = malloc(V_fw_tables_max * sizeof(struct table_info),
	    M_IPFW, M_WAITOK | M_ZERO);

	tcfg = malloc(sizeof(struct tables_config), M_IPFW, M_WAITOK | M_ZERO);
	tcfg->namehash = ipfw_objhash_create(V_fw_tables_max);
	ch->tblcfg = tcfg;

	ipfw_table_algo_init(ch);

	return (0);
}

int
ipfw_resize_tables(struct ip_fw_chain *ch, unsigned int ntables)
{
	unsigned int ntables_old, tbl;
	struct namedobj_instance *ni;
	void *new_idx, *old_tablestate, *tablestate;
	struct table_info *ti;
	struct table_config *tc;
	int i, new_blocks;

	/* Check new value for validity */
	if (ntables > IPFW_TABLES_MAX)
		ntables = IPFW_TABLES_MAX;

	/* Allocate new pointers */
	tablestate = malloc(ntables * sizeof(struct table_info),
	    M_IPFW, M_WAITOK | M_ZERO);

	ipfw_objhash_bitmap_alloc(ntables, (void *)&new_idx, &new_blocks);

	IPFW_UH_WLOCK(ch);

	tbl = (ntables >= V_fw_tables_max) ? V_fw_tables_max : ntables;
	ni = CHAIN_TO_NI(ch);

	/* Temporary restrict decreasing max_tables */
	if (ntables < V_fw_tables_max) {

		/*
		 * FIXME: Check if we really can shrink
		 */
		IPFW_UH_WUNLOCK(ch);
		return (EINVAL);
	}

	/* Copy table info/indices */
	memcpy(tablestate, ch->tablestate, sizeof(struct table_info) * tbl);
	ipfw_objhash_bitmap_merge(ni, &new_idx, &new_blocks);

	IPFW_WLOCK(ch);

	/* Change pointers */
	old_tablestate = ch->tablestate;
	ch->tablestate = tablestate;
	ipfw_objhash_bitmap_swap(ni, &new_idx, &new_blocks);

	ntables_old = V_fw_tables_max;
	V_fw_tables_max = ntables;

	IPFW_WUNLOCK(ch);

	/* Notify all consumers that their @ti pointer has changed */
	ti = (struct table_info *)ch->tablestate;
	for (i = 0; i < tbl; i++, ti++) {
		if (ti->lookup == NULL)
			continue;
		tc = (struct table_config *)ipfw_objhash_lookup_kidx(ni, i);
		if (tc == NULL || tc->ta->change_ti == NULL)
			continue;

		tc->ta->change_ti(tc->astate, ti);
	}

	IPFW_UH_WUNLOCK(ch);

	/* Free old pointers */
	free(old_tablestate, M_IPFW);
	ipfw_objhash_bitmap_free(new_idx, new_blocks);

	return (0);
}

int
ipfw_lookup_table(struct ip_fw_chain *ch, uint16_t tbl, in_addr_t addr,
    uint32_t *val)
{
	struct table_info *ti;

	ti = &(((struct table_info *)ch->tablestate)[tbl]);

	return (ti->lookup(ti, &addr, sizeof(in_addr_t), val));
}

int
ipfw_lookup_table_extended(struct ip_fw_chain *ch, uint16_t tbl, uint16_t plen,
    void *paddr, uint32_t *val)
{
	struct table_info *ti;

	ti = &(((struct table_info *)ch->tablestate)[tbl]);

	return (ti->lookup(ti, paddr, plen, val));
}

/*
 * Info/List/dump support for tables.
 *
 */

/*
 * High-level 'get' cmds sysctl handlers
 */

/*
 * Get buffer size needed to list info for all tables.
 * Data layout (v0)(current):
 * Request: [ empty ], size = sizeof(ipfw_obj_lheader)
 * Reply: [ ipfw_obj_lheader ]
 *
 * Returns 0 on success
 */
int
ipfw_listsize_tables(struct ip_fw_chain *ch, struct sockopt_data *sd)
{
	struct _ipfw_obj_lheader *olh;

	olh = (struct _ipfw_obj_lheader *)ipfw_get_sopt_header(sd,sizeof(*olh));
	if (olh == NULL)
		return (EINVAL);

	olh->size = sizeof(*olh); /* Make export_table store needed size */

	IPFW_UH_RLOCK(ch);
	export_tables(ch, olh, sd);
	IPFW_UH_RUNLOCK(ch);

	return (0);
}

/*
 * Lists all tables currently available in kernel.
 * Data layout (v0)(current):
 * Request: [ ipfw_obj_lheader ], size = ipfw_obj_lheader.size
 * Reply: [ ipfw_obj_lheader ipfw_xtable_info x N ]
 *
 * Returns 0 on success
 */
int
ipfw_list_tables(struct ip_fw_chain *ch, struct sockopt_data *sd)
{
	struct _ipfw_obj_lheader *olh;
	int error;

	olh = (struct _ipfw_obj_lheader *)ipfw_get_sopt_header(sd,sizeof(*olh));
	if (olh == NULL)
		return (EINVAL);
	if (sd->valsize < olh->size)
		return (EINVAL);

	IPFW_UH_RLOCK(ch);
	error = export_tables(ch, olh, sd);
	IPFW_UH_RUNLOCK(ch);

	return (error);
}

/*
 * Store table info to buffer provided by @sd.
 * Data layout (v0)(current):
 * Request: [ ipfw_obj_header ipfw_xtable_info(empty)]
 * Reply: [ ipfw_obj_header ipfw_xtable_info ]
 *
 * Returns 0 on success.
 */
int
ipfw_describe_table(struct ip_fw_chain *ch, struct sockopt_data *sd)
{
	struct _ipfw_obj_header *oh;
	struct table_config *tc;
	struct tid_info ti;
	size_t sz;

	sz = sizeof(*oh) + sizeof(ipfw_xtable_info);
	oh = (struct _ipfw_obj_header *)ipfw_get_sopt_header(sd, sz);
	if (oh == NULL)
		return (EINVAL);

	objheader_to_ti(oh, &ti);

	IPFW_UH_RLOCK(ch);
	if ((tc = find_table(CHAIN_TO_NI(ch), &ti)) == NULL) {
		IPFW_UH_RUNLOCK(ch);
		return (ESRCH);
	}

	export_table_info(ch, tc, (ipfw_xtable_info *)(oh + 1));
	IPFW_UH_RUNLOCK(ch);

	return (0);
}

struct dump_args {
	struct table_info *ti;
	struct table_config *tc;
	struct sockopt_data *sd;
	uint32_t cnt;
	uint16_t uidx;
	int error;
	ipfw_table_entry *ent;
	uint32_t size;
	ipfw_obj_tentry tent;
};

int
ipfw_dump_table(struct ip_fw_chain *ch, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	int error;

	switch (op3->version) {
	case 0:
		error = ipfw_dump_table_v0(ch, sd);
		break;
	case 1:
		error = ipfw_dump_table_v1(ch, sd);
		break;
	default:
		error = ENOTSUP;
	}

	return (error);
}

/*
 * Dumps all table data
 * Data layout (v1)(current):
 * Request: [ ipfw_obj_header ], size = ipfw_xtable_info.size
 * Reply: [ ipfw_obj_header ipfw_xtable_info ipfw_obj_tentry x N ]
 *
 * Returns 0 on success
 */
static int
ipfw_dump_table_v1(struct ip_fw_chain *ch, struct sockopt_data *sd)
{
	struct _ipfw_obj_header *oh;
	ipfw_xtable_info *i;
	struct tid_info ti;
	struct table_config *tc;
	struct table_algo *ta;
	struct dump_args da;
	uint32_t sz;

	sz = sizeof(ipfw_obj_header) + sizeof(ipfw_xtable_info);
	oh = (struct _ipfw_obj_header *)ipfw_get_sopt_header(sd, sz);
	if (oh == NULL)
		return (EINVAL);

	i = (ipfw_xtable_info *)(oh + 1);
	objheader_to_ti(oh, &ti);

	IPFW_UH_RLOCK(ch);
	if ((tc = find_table(CHAIN_TO_NI(ch), &ti)) == NULL) {
		IPFW_UH_RUNLOCK(ch);
		return (ESRCH);
	}
	export_table_info(ch, tc, i);
	sz = tc->count;

	if (sd->valsize < sz + tc->count * sizeof(ipfw_obj_tentry)) {

		/*
		 * Submitted buffer size is not enough.
		 * WE've already filled in @i structure with
		 * relevant table info including size, so we
		 * can return. Buffer will be flushed automatically.
		 */
		IPFW_UH_RUNLOCK(ch);
		return (ENOMEM);
	}

	/*
	 * Do the actual dump in eXtended format
	 */
	memset(&da, 0, sizeof(da));
	da.ti = KIDX_TO_TI(ch, tc->no.kidx);
	da.tc = tc;
	da.sd = sd;

	ta = tc->ta;

	ta->foreach(tc->astate, da.ti, dump_table_tentry, &da);
	IPFW_UH_RUNLOCK(ch);

	return (da.error);
}

/*
 * Dumps all table data
 * Data layout (version 0)(legacy):
 * Request: [ ipfw_xtable ], size = IP_FW_TABLE_XGETSIZE()
 * Reply: [ ipfw_xtable ipfw_table_xentry x N ]
 *
 * Returns 0 on success
 */
static int
ipfw_dump_table_v0(struct ip_fw_chain *ch, struct sockopt_data *sd)
{
	ipfw_xtable *xtbl;
	struct tid_info ti;
	struct table_config *tc;
	struct table_algo *ta;
	struct dump_args da;
	size_t sz;

	xtbl = (ipfw_xtable *)ipfw_get_sopt_header(sd, sizeof(ipfw_xtable));
	if (xtbl == NULL)
		return (EINVAL);

	memset(&ti, 0, sizeof(ti));
	ti.uidx = xtbl->tbl;
	
	IPFW_UH_RLOCK(ch);
	if ((tc = find_table(CHAIN_TO_NI(ch), &ti)) == NULL) {
		IPFW_UH_RUNLOCK(ch);
		return (0);
	}
	sz = tc->count * sizeof(ipfw_table_xentry) + sizeof(ipfw_xtable);

	xtbl->cnt = tc->count;
	xtbl->size = sz;
	xtbl->type = tc->no.type;
	xtbl->tbl = ti.uidx;

	if (sd->valsize < sz) {

		/*
		 * Submitted buffer size is not enough.
		 * WE've already filled in @i structure with
		 * relevant table info including size, so we
		 * can return. Buffer will be flushed automatically.
		 */
		IPFW_UH_RUNLOCK(ch);
		return (ENOMEM);
	}

	/* Do the actual dump in eXtended format */
	memset(&da, 0, sizeof(da));
	da.ti = KIDX_TO_TI(ch, tc->no.kidx);
	da.tc = tc;
	da.sd = sd;

	ta = tc->ta;

	ta->foreach(tc->astate, da.ti, dump_table_xentry, &da);
	IPFW_UH_RUNLOCK(ch);

	return (0);
}

/*
 * Creates new table.
 * Data layout (v0)(current):
 * Request: [ ipfw_obj_header ipfw_xtable_info ]
 *
 * Returns 0 on success
 */
int
ipfw_create_table(struct ip_fw_chain *ch, ip_fw3_opheader *op3,
    struct sockopt_data *sd)
{
	struct _ipfw_obj_header *oh;
	ipfw_xtable_info *i;
	char *tname, *aname;
	struct tid_info ti;
	struct namedobj_instance *ni;
	struct table_config *tc;

	if (sd->valsize != sizeof(*oh) + sizeof(ipfw_xtable_info))
		return (EINVAL);

	oh = (struct _ipfw_obj_header *)sd->kbuf;
	i = (ipfw_xtable_info *)(oh + 1);

	/*
	 * Verify user-supplied strings.
	 * Check for null-terminated/zero-length strings/
	 */
	tname = oh->ntlv.name;
	aname = i->algoname;
	if (ipfw_check_table_name(tname) != 0 ||
	    strnlen(aname, sizeof(i->algoname)) == sizeof(i->algoname))
		return (EINVAL);

	if (aname[0] == '\0') {
		/* Use default algorithm */
		aname = NULL;
	}

	objheader_to_ti(oh, &ti);
	ti.type = i->type;

	ni = CHAIN_TO_NI(ch);

	IPFW_UH_RLOCK(ch);
	if ((tc = find_table(ni, &ti)) != NULL) {
		IPFW_UH_RUNLOCK(ch);
		return (EEXIST);
	}
	IPFW_UH_RUNLOCK(ch);

	return (create_table_internal(ch, &ti, aname, i));
}

/*
 * Creates new table based on @ti and @aname.
 *
 * Relies on table name checking inside find_name_tlv()
 * Assume @aname to be checked and valid.
 *
 * Returns 0 on success.
 */
static int
create_table_internal(struct ip_fw_chain *ch, struct tid_info *ti,
    char *aname, ipfw_xtable_info *i)
{
	struct namedobj_instance *ni;
	struct table_config *tc;
	struct table_algo *ta;
	uint16_t kidx;

	ni = CHAIN_TO_NI(ch);

	ta = find_table_algo(CHAIN_TO_TCFG(ch), ti, aname);
	if (ta == NULL)
		return (ENOTSUP);

	tc = alloc_table_config(ch, ti, ta, aname, i->tflags, i->vtype);
	if (tc == NULL)
		return (ENOMEM);

	tc->limit = i->limit;

	IPFW_UH_WLOCK(ch);

	/* Check if table has been already created */
	if (find_table(ni, ti) != NULL) {
		IPFW_UH_WUNLOCK(ch);
		free_table_config(ni, tc);
		return (EEXIST);
	}

	if (ipfw_objhash_alloc_idx(ni, &kidx) != 0) {
		IPFW_UH_WUNLOCK(ch);
		printf("Unable to allocate table index."
		    " Consider increasing net.inet.ip.fw.tables_max");
		free_table_config(ni, tc);
		return (EBUSY);
	}

	tc->no.kidx = kidx;

	IPFW_WLOCK(ch);
	link_table(ch, tc);
	IPFW_WUNLOCK(ch);

	IPFW_UH_WUNLOCK(ch);

	return (0);
}

void
objheader_to_ti(struct _ipfw_obj_header *oh, struct tid_info *ti)
{

	memset(ti, 0, sizeof(struct tid_info));
	ti->set = oh->ntlv.set;
	ti->uidx = oh->idx;
	ti->tlvs = &oh->ntlv;
	ti->tlen = oh->ntlv.head.length;
}

int
ipfw_export_table_ntlv(struct ip_fw_chain *ch, uint16_t kidx,
    struct sockopt_data *sd)
{
	struct namedobj_instance *ni;
	struct named_object *no;
	ipfw_obj_ntlv *ntlv;

	ni = CHAIN_TO_NI(ch);

	no = ipfw_objhash_lookup_kidx(ni, kidx);
	KASSERT(no != NULL, ("invalid table kidx passed"));

	ntlv = (ipfw_obj_ntlv *)ipfw_get_sopt_space(sd, sizeof(*ntlv));
	if (ntlv == NULL)
		return (ENOMEM);

	ntlv->head.type = IPFW_TLV_TBL_NAME;
	ntlv->head.length = sizeof(*ntlv);
	ntlv->idx = no->kidx;
	strlcpy(ntlv->name, no->name, sizeof(ntlv->name));

	return (0);
}

static void
export_table_info(struct ip_fw_chain *ch, struct table_config *tc,
    ipfw_xtable_info *i)
{
	struct table_info *ti;
	
	i->type = tc->no.type;
	i->tflags = tc->tflags;
	i->vtype = tc->vtype;
	i->set = tc->no.set;
	i->kidx = tc->no.kidx;
	i->refcnt = tc->no.refcnt;
	i->count = tc->count;
	i->limit = tc->limit;
	i->size = tc->count * sizeof(ipfw_obj_tentry);
	i->size += sizeof(ipfw_obj_header) + sizeof(ipfw_xtable_info);
	strlcpy(i->tablename, tc->tablename, sizeof(i->tablename));
	if (tc->ta->print_config != NULL) {
		/* Use algo function to print table config to string */
		ti = KIDX_TO_TI(ch, tc->no.kidx);
		tc->ta->print_config(tc->astate, ti, i->algoname,
		    sizeof(i->algoname));
	} else
		strlcpy(i->algoname, tc->ta->name, sizeof(i->algoname));
}

struct dump_table_args {
	struct ip_fw_chain *ch;
	struct sockopt_data *sd;
};

static void
export_table_internal(struct namedobj_instance *ni, struct named_object *no,
    void *arg)
{
	ipfw_xtable_info *i;
	struct dump_table_args *dta;

	dta = (struct dump_table_args *)arg;

	i = (ipfw_xtable_info *)ipfw_get_sopt_space(dta->sd, sizeof(*i));
	KASSERT(i != 0, ("previously checked buffer is not enough"));

	export_table_info(dta->ch, (struct table_config *)no, i);
}

/*
 * Export all tables as ipfw_xtable_info structures to
 * storage provided by @sd.
 * Returns 0 on success.
 */
static int
export_tables(struct ip_fw_chain *ch, ipfw_obj_lheader *olh,
    struct sockopt_data *sd)
{
	uint32_t size;
	uint32_t count;
	struct dump_table_args dta;

	count = ipfw_objhash_count(CHAIN_TO_NI(ch));
	size = count * sizeof(ipfw_xtable_info) + sizeof(ipfw_obj_lheader);

	/* Fill in header regadless of buffer size */
	olh->count = count;
	olh->objsize = sizeof(ipfw_xtable_info);

	if (size > olh->size) {
		olh->size = size;
		return (ENOMEM);
	}

	olh->size = size;

	dta.ch = ch;
	dta.sd = sd;

	ipfw_objhash_foreach(CHAIN_TO_NI(ch), export_table_internal, &dta);

	return (0);
}

/*
 * Legacy IP_FW_TABLE_GETSIZE handler
 */
int
ipfw_count_table(struct ip_fw_chain *ch, struct tid_info *ti, uint32_t *cnt)
{
	struct table_config *tc;

	if ((tc = find_table(CHAIN_TO_NI(ch), ti)) == NULL)
		return (ESRCH);
	*cnt = tc->count;
	return (0);
}


/*
 * Legacy IP_FW_TABLE_XGETSIZE handler
 */
int
ipfw_count_xtable(struct ip_fw_chain *ch, struct tid_info *ti, uint32_t *cnt)
{
	struct table_config *tc;

	if ((tc = find_table(CHAIN_TO_NI(ch), ti)) == NULL) {
		*cnt = 0;
		return (0); /* 'table all list' requires success */
	}
	*cnt = tc->count * sizeof(ipfw_table_xentry);
	if (tc->count > 0)
		*cnt += sizeof(ipfw_xtable);
	return (0);
}

static int
dump_table_entry(void *e, void *arg)
{
	struct dump_args *da;
	struct table_config *tc;
	struct table_algo *ta;
	ipfw_table_entry *ent;
	int error;

	da = (struct dump_args *)arg;

	tc = da->tc;
	ta = tc->ta;

	/* Out of memory, returning */
	if (da->cnt == da->size)
		return (1);
	ent = da->ent++;
	ent->tbl = da->uidx;
	da->cnt++;

	error = ta->dump_tentry(tc->astate, da->ti, e, &da->tent);
	if (error != 0)
		return (error);

	ent->addr = da->tent.k.addr.s_addr;
	ent->masklen = da->tent.masklen;
	ent->value = da->tent.value;

	return (0);
}

/*
 * Dumps table in pre-8.1 legacy format.
 */
int
ipfw_dump_table_legacy(struct ip_fw_chain *ch, struct tid_info *ti,
    ipfw_table *tbl)
{
	struct table_config *tc;
	struct table_algo *ta;
	struct dump_args da;

	tbl->cnt = 0;

	if ((tc = find_table(CHAIN_TO_NI(ch), ti)) == NULL)
		return (0);	/* XXX: We should return ESRCH */

	ta = tc->ta;

	/* This dump format supports IPv4 only */
	if (tc->no.type != IPFW_TABLE_CIDR)
		return (0);

	memset(&da, 0, sizeof(da));
	da.ti = KIDX_TO_TI(ch, tc->no.kidx);
	da.tc = tc;
	da.ent = &tbl->ent[0];
	da.size = tbl->size;

	tbl->cnt = 0;
	ta->foreach(tc->astate, da.ti, dump_table_entry, &da);
	tbl->cnt = da.cnt;

	return (0);
}

/*
 * Dumps table entry in eXtended format (v1)(current).
 */
static int
dump_table_tentry(void *e, void *arg)
{
	struct dump_args *da;
	struct table_config *tc;
	struct table_algo *ta;
	ipfw_obj_tentry *tent;

	da = (struct dump_args *)arg;

	tc = da->tc;
	ta = tc->ta;

	tent = (ipfw_obj_tentry *)ipfw_get_sopt_space(da->sd, sizeof(*tent));
	/* Out of memory, returning */
	if (tent == NULL) {
		da->error = ENOMEM;
		return (1);
	}
	tent->head.length = sizeof(ipfw_obj_tentry);
	tent->idx = da->uidx;

	return (ta->dump_tentry(tc->astate, da->ti, e, tent));
}

/*
 * Dumps table entry in eXtended format (v0).
 */
static int
dump_table_xentry(void *e, void *arg)
{
	struct dump_args *da;
	struct table_config *tc;
	struct table_algo *ta;
	ipfw_table_xentry *xent;
	ipfw_obj_tentry *tent;
	int error;

	da = (struct dump_args *)arg;

	tc = da->tc;
	ta = tc->ta;

	xent = (ipfw_table_xentry *)ipfw_get_sopt_space(da->sd, sizeof(*xent));
	/* Out of memory, returning */
	if (xent == NULL)
		return (1);
	xent->len = sizeof(ipfw_table_xentry);
	xent->tbl = da->uidx;

	memset(&da->tent, 0, sizeof(da->tent));
	tent = &da->tent;
	error = ta->dump_tentry(tc->astate, da->ti, e, tent);
	if (error != 0)
		return (error);

	/* Convert current format to previous one */
	xent->masklen = tent->masklen;
	xent->value = tent->value;
	/* Apply some hacks */
	if (tc->no.type == IPFW_TABLE_CIDR && tent->subtype == AF_INET) {
		xent->k.addr6.s6_addr32[3] = tent->k.addr.s_addr;
		xent->flags = IPFW_TCF_INET;
	} else
		memcpy(&xent->k, &tent->k, sizeof(xent->k));

	return (0);
}

/*
 * Table algorithms
 */ 

/*
 * Finds algoritm by index, table type or supplied name
 */
static struct table_algo *
find_table_algo(struct tables_config *tcfg, struct tid_info *ti, char *name)
{
	int i, l;
	struct table_algo *ta;

	if (ti->type > IPFW_TABLE_MAXTYPE)
		return (NULL);

	/* Search by index */
	if (ti->atype != 0) {
		if (ti->atype > tcfg->algo_count)
			return (NULL);
		return (tcfg->algo[ti->atype]);
	}

	/* Search by name if supplied */
	if (name != NULL) {
		/* TODO: better search */
		for (i = 1; i <= tcfg->algo_count; i++) {
			ta = tcfg->algo[i];

			/*
			 * One can supply additional algorithm
			 * parameters so we compare only the first word
			 * of supplied name:
			 * 'hash_cidr hsize=32'
			 * '^^^^^^^^^'
			 *
			 */
			l = strlen(ta->name);
			if (strncmp(name, ta->name, l) == 0) {
				if (name[l] == '\0' || name[l] == ' ')
					return (ta);
			}
		}

		return (NULL);
	}

	/* Return default algorithm for given type if set */
	return (tcfg->def_algo[ti->type]);
}

int
ipfw_add_table_algo(struct ip_fw_chain *ch, struct table_algo *ta, size_t size,
    int *idx)
{
	struct tables_config *tcfg;
	struct table_algo *ta_new;

	if (size > sizeof(struct table_algo))
		return (EINVAL);

	KASSERT(ta->type >= IPFW_TABLE_MAXTYPE,("Increase IPFW_TABLE_MAXTYPE"));

	ta_new = malloc(sizeof(struct table_algo), M_IPFW, M_WAITOK | M_ZERO);
	memcpy(ta_new, ta, size);

	tcfg = CHAIN_TO_TCFG(ch);

	KASSERT(tcfg->algo_count < 255, ("Increase algo array size"));

	tcfg->algo[++tcfg->algo_count] = ta_new;
	ta_new->idx = tcfg->algo_count;

	/* Set algorithm as default one for given type */
	if ((ta_new->flags & TA_FLAG_DEFAULT) != 0 &&
	    tcfg->def_algo[ta_new->type] == NULL)
		tcfg->def_algo[ta_new->type] = ta_new;

	*idx = ta_new->idx;
	
	return (0);
}

void
ipfw_del_table_algo(struct ip_fw_chain *ch, int idx)
{
	struct tables_config *tcfg;
	struct table_algo *ta;

	tcfg = CHAIN_TO_TCFG(ch);

	KASSERT(idx <= tcfg->algo_count, ("algo idx %d out of rage 1..%d", idx, 
	    tcfg->algo_count));

	ta = tcfg->algo[idx];
	KASSERT(ta != NULL, ("algo idx %d is NULL", idx));

	if (tcfg->def_algo[ta->type] == ta)
		tcfg->def_algo[ta->type] = NULL;

	free(ta, M_IPFW);
}

/*
 * Lists all table algorithms currently available.
 * Data layout (v0)(current):
 * Request: [ ipfw_obj_lheader ], size = ipfw_obj_lheader.size
 * Reply: [ ipfw_obj_lheader ipfw_ta_info x N ]
 *
 * Returns 0 on success
 */
int
ipfw_list_table_algo(struct ip_fw_chain *ch, struct sockopt_data *sd)
{
	struct _ipfw_obj_lheader *olh;
	struct tables_config *tcfg;
	ipfw_ta_info *i;
	struct table_algo *ta;
	uint32_t count, n, size;

	olh = (struct _ipfw_obj_lheader *)ipfw_get_sopt_header(sd,sizeof(*olh));
	if (olh == NULL)
		return (EINVAL);
	if (sd->valsize < olh->size)
		return (EINVAL);

	IPFW_UH_RLOCK(ch);
	tcfg = CHAIN_TO_TCFG(ch);
	count = tcfg->algo_count;
	size = count * sizeof(ipfw_ta_info) + sizeof(ipfw_obj_lheader);

	/* Fill in header regadless of buffer size */
	olh->count = count;
	olh->objsize = sizeof(ipfw_ta_info);

	if (size > olh->size) {
		olh->size = size;
		IPFW_UH_RUNLOCK(ch);
		return (ENOMEM);
	}
	olh->size = size;

	for (n = 1; n <= count; n++) {
		i = (ipfw_ta_info *)ipfw_get_sopt_space(sd, sizeof(*i));
		KASSERT(i != 0, ("previously checked buffer is not enough"));
		ta = tcfg->algo[n];
		strlcpy(i->algoname, ta->name, sizeof(i->algoname));
		i->type = ta->type;
		i->refcnt = ta->refcnt;
	}

	IPFW_UH_RUNLOCK(ch);

	return (0);
}


/*
 * Tables rewriting code 
 *
 */

/*
 * Determine table number and lookup type for @cmd.
 * Fill @tbl and @type with appropriate values.
 * Returns 0 for relevant opcodes, 1 otherwise.
 */
static int
classify_table_opcode(ipfw_insn *cmd, uint16_t *puidx, uint8_t *ptype)
{
	ipfw_insn_if *cmdif;
	int skip;
	uint16_t v;

	skip = 1;

	switch (cmd->opcode) {
	case O_IP_SRC_LOOKUP:
	case O_IP_DST_LOOKUP:
		/* Basic IPv4/IPv6 or u32 lookups */
		*puidx = cmd->arg1;
		/* Assume CIDR by default */
		*ptype = IPFW_TABLE_CIDR;
		skip = 0;
		
		if (F_LEN(cmd) > F_INSN_SIZE(ipfw_insn_u32)) {
			/*
			 * generic lookup. The key must be
			 * in 32bit big-endian format.
			 */
			v = ((ipfw_insn_u32 *)cmd)->d[1];
			switch (v) {
			case 0:
			case 1:
				/* IPv4 src/dst */
				break;
			case 2:
			case 3:
				/* src/dst port */
				*ptype = IPFW_TABLE_NUMBER;
				break;
			case 4:
				/* uid/gid */
				*ptype = IPFW_TABLE_NUMBER;
				break;
			case 5:
				/* jid */
				*ptype = IPFW_TABLE_NUMBER;
				break;
			case 6:
				/* dscp */
				*ptype = IPFW_TABLE_NUMBER;
				break;
			}
		}
		break;
	case O_XMIT:
	case O_RECV:
	case O_VIA:
		/* Interface table, possibly */
		cmdif = (ipfw_insn_if *)cmd;
		if (cmdif->name[0] != '\1')
			break;

		*ptype = IPFW_TABLE_INTERFACE;
		*puidx = cmdif->p.glob;
		skip = 0;
		break;
	case O_IP_FLOW_LOOKUP:
		*puidx = cmd->arg1;
		*ptype = IPFW_TABLE_FLOW;
		skip = 0;
		break;
	}

	return (skip);
}

/*
 * Sets new table value for given opcode.
 * Assume the same opcodes as classify_table_opcode()
 */
static void
update_table_opcode(ipfw_insn *cmd, uint16_t idx)
{
	ipfw_insn_if *cmdif;

	switch (cmd->opcode) {
	case O_IP_SRC_LOOKUP:
	case O_IP_DST_LOOKUP:
		/* Basic IPv4/IPv6 or u32 lookups */
		cmd->arg1 = idx;
		break;
	case O_XMIT:
	case O_RECV:
	case O_VIA:
		/* Interface table, possibly */
		cmdif = (ipfw_insn_if *)cmd;
		cmdif->p.glob = idx;
		break;
	case O_IP_FLOW_LOOKUP:
		cmd->arg1 = idx;
		break;
	}
}

/*
 * Checks table name for validity.
 * Enforce basic length checks, the rest
 * should be done in userland.
 *
 * Returns 0 if name is considered valid.
 */
int
ipfw_check_table_name(char *name)
{
	int nsize;
	ipfw_obj_ntlv *ntlv = NULL;

	nsize = sizeof(ntlv->name);

	if (strnlen(name, nsize) == nsize)
		return (EINVAL);

	if (name[0] == '\0')
		return (EINVAL);

	/*
	 * TODO: do some more complicated checks
	 */

	return (0);
}

/*
 * Find tablename TLV by @uid.
 * Check @tlvs for valid data inside.
 *
 * Returns pointer to found TLV or NULL.
 */
static ipfw_obj_ntlv *
find_name_tlv(void *tlvs, int len, uint16_t uidx)
{
	ipfw_obj_ntlv *ntlv;
	uintptr_t pa, pe;
	int l;

	pa = (uintptr_t)tlvs;
	pe = pa + len;
	l = 0;
	for (; pa < pe; pa += l) {
		ntlv = (ipfw_obj_ntlv *)pa;
		l = ntlv->head.length;

		if (l != sizeof(*ntlv))
			return (NULL);

		if (ntlv->head.type != IPFW_TLV_TBL_NAME)
			continue;

		if (ntlv->idx != uidx)
			continue;

		if (ipfw_check_table_name(ntlv->name) != 0)
			return (NULL);
		
		return (ntlv);
	}

	return (NULL);
}

/*
 * Finds table config based on either legacy index
 * or name in ntlv.
 * Note @ti structure contains unchecked data from userland.
 *
 * Returns pointer to table_config or NULL.
 */
static struct table_config *
find_table(struct namedobj_instance *ni, struct tid_info *ti)
{
	char *name, bname[16];
	struct named_object *no;
	ipfw_obj_ntlv *ntlv;
	uint32_t set;

	if (ti->tlvs != NULL) {
		ntlv = find_name_tlv(ti->tlvs, ti->tlen, ti->uidx);
		if (ntlv == NULL)
			return (NULL);
		name = ntlv->name;
		set = ntlv->set;
	} else {
		snprintf(bname, sizeof(bname), "%d", ti->uidx);
		name = bname;
		set = 0;
	}

	no = ipfw_objhash_lookup_name(ni, set, name);

	return ((struct table_config *)no);
}

static struct table_config *
alloc_table_config(struct ip_fw_chain *ch, struct tid_info *ti,
    struct table_algo *ta, char *aname, uint8_t tflags, uint8_t vtype)
{
	char *name, bname[16];
	struct table_config *tc;
	int error;
	ipfw_obj_ntlv *ntlv;
	uint32_t set;

	if (ti->tlvs != NULL) {
		ntlv = find_name_tlv(ti->tlvs, ti->tlen, ti->uidx);
		if (ntlv == NULL)
			return (NULL);
		name = ntlv->name;
		set = ntlv->set;
	} else {
		snprintf(bname, sizeof(bname), "%d", ti->uidx);
		name = bname;
		set = 0;
	}

	tc = malloc(sizeof(struct table_config), M_IPFW, M_WAITOK | M_ZERO);
	tc->no.name = tc->tablename;
	tc->no.type = ti->type;
	tc->no.set = set;
	tc->tflags = tflags;
	tc->ta = ta;
	strlcpy(tc->tablename, name, sizeof(tc->tablename));
	/* Set default value type to u32 for compability reasons */
	if (vtype == 0)
		tc->vtype = IPFW_VTYPE_U32;
	else
		tc->vtype = vtype;

	if (ti->tlvs == NULL) {
		tc->no.compat = 1;
		tc->no.uidx = ti->uidx;
	}

	/* Preallocate data structures for new tables */
	error = ta->init(ch, &tc->astate, &tc->ti, aname, tflags);
	if (error != 0) {
		free(tc, M_IPFW);
		return (NULL);
	}
	
	return (tc);
}

static void
free_table_config(struct namedobj_instance *ni, struct table_config *tc)
{

	if (tc->linked == 0)
		tc->ta->destroy(tc->astate, &tc->ti);

	free(tc, M_IPFW);
}

/*
 * Links @tc to @chain table named instance.
 * Sets appropriate type/states in @chain table info.
 */
static void
link_table(struct ip_fw_chain *ch, struct table_config *tc)
{
	struct namedobj_instance *ni;
	struct table_info *ti;
	uint16_t kidx;

	IPFW_UH_WLOCK_ASSERT(ch);
	IPFW_WLOCK_ASSERT(ch);

	ni = CHAIN_TO_NI(ch);
	kidx = tc->no.kidx;

	ipfw_objhash_add(ni, &tc->no);

	ti = KIDX_TO_TI(ch, kidx);
	*ti = tc->ti;

	/* Notify algo on real @ti address */
	if (tc->ta->change_ti != NULL)
		tc->ta->change_ti(tc->astate, ti);

	tc->linked = 1;
	tc->ta->refcnt++;
}

/*
 * Unlinks @tc from @chain table named instance.
 * Zeroes states in @chain and stores them in @tc.
 */
static void
unlink_table(struct ip_fw_chain *ch, struct table_config *tc)
{
	struct namedobj_instance *ni;
	struct table_info *ti;
	uint16_t kidx;

	IPFW_UH_WLOCK_ASSERT(ch);
	IPFW_WLOCK_ASSERT(ch);

	ni = CHAIN_TO_NI(ch);
	kidx = tc->no.kidx;

	/* Clear state. @ti copy is already saved inside @tc */
	ipfw_objhash_del(ni, &tc->no);
	ti = KIDX_TO_TI(ch, kidx);
	memset(ti, 0, sizeof(struct table_info));
	tc->linked = 0;
	tc->ta->refcnt--;

	/* Notify algo on real @ti address */
	if (tc->ta->change_ti != NULL)
		tc->ta->change_ti(tc->astate, NULL);
}

/*
 * Finds named object by @uidx number.
 * Refs found object, allocate new index for non-existing object.
 * Fills in @oib with userland/kernel indexes.
 * First free oidx pointer is saved back in @oib.
 *
 * Returns 0 on success.
 */
static int
bind_table_rule(struct ip_fw_chain *ch, struct ip_fw *rule,
    struct rule_check_info *ci, struct obj_idx **oib, struct tid_info *ti)
{
	struct table_config *tc;
	struct namedobj_instance *ni;
	struct named_object *no;
	int error, l, cmdlen;
	ipfw_insn *cmd;
	struct obj_idx *pidx, *p;

	pidx = *oib;
	l = rule->cmd_len;
	cmd = rule->cmd;
	cmdlen = 0;
	error = 0;

	IPFW_UH_WLOCK(ch);
	ni = CHAIN_TO_NI(ch);

	for ( ;	l > 0 ; l -= cmdlen, cmd += cmdlen) {
		cmdlen = F_LEN(cmd);

		if (classify_table_opcode(cmd, &ti->uidx, &ti->type) != 0)
			continue;

		pidx->uidx = ti->uidx;
		pidx->type = ti->type;

		if ((tc = find_table(ni, ti)) != NULL) {
			if (tc->no.type != ti->type) {
				/* Incompatible types */
				error = EINVAL;
				break;
			}

			/* Reference found table and save kidx */
			tc->no.refcnt++;
			pidx->kidx = tc->no.kidx;
			pidx++;
			continue;
		}

		/* Table not found. Allocate new index and save for later */
		if (ipfw_objhash_alloc_idx(ni, &pidx->kidx) != 0) {
			printf("Unable to allocate table %s index in set %u."
			    " Consider increasing net.inet.ip.fw.tables_max",
			    "", ti->set);
			error = EBUSY;
			break;
		}

		ci->new_tables++;
		pidx->new = 1;
		pidx++;
	}

	if (error != 0) {
		/* Unref everything we have already done */
		for (p = *oib; p < pidx; p++) {
			if (p->new != 0) {
				ipfw_objhash_free_idx(ni, p->kidx);
				continue;
			}

			/* Find & unref by existing idx */
			no = ipfw_objhash_lookup_kidx(ni, p->kidx);
			KASSERT(no != NULL, ("Ref'd table %d disappeared",
			    p->kidx));

			no->refcnt--;
		}
	}
	IPFW_UH_WUNLOCK(ch);

	*oib = pidx;

	return (error);
}

/*
 * Compatibility function for old ipfw(8) binaries.
 * Rewrites table kernel indices with userland ones.
 * Works for \d+ talbes only (e.g. for tables, converted
 * from old numbered system calls).
 *
 * Returns 0 on success.
 * Raises error on any other tables.
 */
int
ipfw_rewrite_table_kidx(struct ip_fw_chain *chain, struct ip_fw_rule0 *rule)
{
	int cmdlen, error, l;
	ipfw_insn *cmd;
	uint16_t kidx, uidx;
	uint8_t type;
	struct named_object *no;
	struct namedobj_instance *ni;

	ni = CHAIN_TO_NI(chain);
	error = 0;

	l = rule->cmd_len;
	cmd = rule->cmd;
	cmdlen = 0;
	for ( ;	l > 0 ; l -= cmdlen, cmd += cmdlen) {
		cmdlen = F_LEN(cmd);

		if (classify_table_opcode(cmd, &kidx, &type) != 0)
			continue;

		if ((no = ipfw_objhash_lookup_kidx(ni, kidx)) == NULL)
			return (1);

		uidx = no->uidx;
		if (no->compat == 0) {

			/*
			 * We are called via legacy opcode.
			 * Save error and show table as fake number
			 * not to make ipfw(8) hang.
			 */
			uidx = 65535;
			error = 2;
		}

		update_table_opcode(cmd, uidx);
	}

	return (error);
}

/*
 * Sets every table kidx in @bmask which is used in rule @rule.
 * 
 * Returns number of newly-referenced tables.
 */
int
ipfw_mark_table_kidx(struct ip_fw_chain *chain, struct ip_fw *rule,
    uint32_t *bmask)
{
	int cmdlen, l, count;
	ipfw_insn *cmd;
	uint16_t kidx;
	uint8_t type;

	l = rule->cmd_len;
	cmd = rule->cmd;
	cmdlen = 0;
	count = 0;
	for ( ;	l > 0 ; l -= cmdlen, cmd += cmdlen) {
		cmdlen = F_LEN(cmd);

		if (classify_table_opcode(cmd, &kidx, &type) != 0)
			continue;

		if ((bmask[kidx / 32] & (1 << (kidx % 32))) == 0)
			count++;

		bmask[kidx / 32] |= 1 << (kidx % 32);
	}

	return (count);
}



/*
 * Checks is opcode is referencing table of appropriate type.
 * Adds reference count for found table if true.
 * Rewrites user-supplied opcode values with kernel ones.
 *
 * Returns 0 on success and appropriate error code otherwise.
 */
int
ipfw_rewrite_table_uidx(struct ip_fw_chain *chain,
    struct rule_check_info *ci)
{
	int cmdlen, error, ftype, l;
	ipfw_insn *cmd;
	uint16_t uidx;
	uint8_t type;
	struct table_config *tc;
	struct table_algo *ta;
	struct namedobj_instance *ni;
	struct named_object *no, *no_n, *no_tmp;
	struct obj_idx *p, *pidx_first, *pidx_last;
	struct namedobjects_head nh;
	struct tid_info ti;

	ni = CHAIN_TO_NI(chain);

	/* Prepare queue to store configs */
	TAILQ_INIT(&nh);

	/*
	 * Prepare an array for storing opcode indices.
	 * Use stack allocation by default.
	 */
	if (ci->table_opcodes <= (sizeof(ci->obuf)/sizeof(ci->obuf[0]))) {
		/* Stack */
		pidx_first = ci->obuf;
	} else
		pidx_first = malloc(ci->table_opcodes * sizeof(struct obj_idx),
		    M_IPFW, M_WAITOK | M_ZERO);

	pidx_last = pidx_first;
	error = 0;

	type = 0;
	ftype = 0;

	memset(&ti, 0, sizeof(ti));

	/*
	 * Use default set for looking up tables (old way) or
	 * use set rule is assigned to (new way).
	 */
	ti.set = (V_fw_tables_sets != 0) ? ci->krule->set : 0;
	if (ci->ctlv != NULL) {
		ti.tlvs = (void *)(ci->ctlv + 1);
		ti.tlen = ci->ctlv->head.length - sizeof(ipfw_obj_ctlv);
	}

	/*
	 * Stage 1: reference existing tables, determine number
	 * of tables we need to allocate and allocate indexes for each.
	 */
	error = bind_table_rule(chain, ci->krule, ci, &pidx_last, &ti);

	if (error != 0) {
		if (pidx_first != ci->obuf)
			free(pidx_first, M_IPFW);

		return (error);
	}

	/*
	 * Stage 2: allocate table configs for every non-existent table
	 */

	if (ci->new_tables > 0) {
		for (p = pidx_first; p < pidx_last; p++) {
			if (p->new == 0)
				continue;

			ti.uidx = p->uidx;
			ti.type = p->type;
			ti.atype = 0;

			ta = find_table_algo(CHAIN_TO_TCFG(chain), &ti, NULL);
			if (ta == NULL) {
				error = ENOTSUP;
				goto free;
			}
			tc = alloc_table_config(chain, &ti, ta, NULL, 0,
			    IPFW_VTYPE_U32);

			if (tc == NULL) {
				error = ENOMEM;
				goto free;
			}

			tc->no.kidx = p->kidx;
			tc->no.refcnt = 1;

			/* Add to list */
			TAILQ_INSERT_TAIL(&nh, &tc->no, nn_next);
		}

		/*
		 * Stage 2.1: Check if we're going to create 2 tables
		 * with the same name, but different table types.
		 */
		TAILQ_FOREACH(no, &nh, nn_next) {
			TAILQ_FOREACH(no_tmp, &nh, nn_next) {
				if (ipfw_objhash_same_name(ni, no, no_tmp) == 0)
					continue;
				if (no->type != no_tmp->type) {
					error = EINVAL;
					goto free;
				}
			}
		}
	}

	IPFW_UH_WLOCK(chain);

	if (ci->new_tables > 0) {
		/*
		 * Stage 3: link & reference new table configs
		 */


		/*
		 * Step 3.1: Check if some tables we need to create have been
		 * already created with different table type.
		 */

		error = 0;
		TAILQ_FOREACH_SAFE(no, &nh, nn_next, no_tmp) {
			no_n = ipfw_objhash_lookup_name(ni, no->set, no->name);
			if (no_n == NULL)
				continue;

			if (no_n->type != no->type) {
				error = EINVAL;
				break;
			}

		}

		if (error != 0) {
			/*
			 * Someone has allocated table with different table type.
			 * We have to rollback everything.
			 */
			IPFW_UH_WUNLOCK(chain);
			goto free;
		}

		/*
		 * Attach new tables.
		 * We need to set table pointers for each new table,
		 * so we have to acquire main WLOCK.
		 */
		IPFW_WLOCK(chain);
		TAILQ_FOREACH_SAFE(no, &nh, nn_next, no_tmp) {
			no_n = ipfw_objhash_lookup_name(ni, no->set, no->name);

			if (no_n == NULL) {
				/* New table. Attach to runtime hash */
				TAILQ_REMOVE(&nh, no, nn_next);
				link_table(chain, (struct table_config *)no);
				continue;
			}

			/*
			 * Newly-allocated table with the same type.
			 * Reference it and update out @pidx array
			 * rewrite info.
			 */
			no_n->refcnt++;
			/* Keep oib array in sync: update kidx */
			for (p = pidx_first; p < pidx_last; p++) {
				if (p->kidx != no->kidx)
					continue;
				/* Update kidx */
				p->kidx = no_n->kidx;
				break;
			}
		}
		IPFW_WUNLOCK(chain);
	}

	/* Perform rule rewrite */
	l = ci->krule->cmd_len;
	cmd = ci->krule->cmd;
	cmdlen = 0;
	p = pidx_first;
	for ( ;	l > 0 ; l -= cmdlen, cmd += cmdlen) {
		cmdlen = F_LEN(cmd);

		if (classify_table_opcode(cmd, &uidx, &type) != 0)
			continue;
		update_table_opcode(cmd, p->kidx);
		p++;
	}

	IPFW_UH_WUNLOCK(chain);

	error = 0;

	/*
	 * Stage 4: free resources
	 */
free:
	if (!TAILQ_EMPTY(&nh)) {
		/* Free indexes first */
		IPFW_UH_WLOCK(chain);
		TAILQ_FOREACH_SAFE(no, &nh, nn_next, no_tmp) {
			ipfw_objhash_free_idx(ni, no->kidx);
		}
		IPFW_UH_WUNLOCK(chain);
		/* Free configs */
		TAILQ_FOREACH_SAFE(no, &nh, nn_next, no_tmp)
			free_table_config(ni, tc);
	}

	if (pidx_first != ci->obuf)
		free(pidx_first, M_IPFW);

	return (error);
}

/*
 * Remove references from every table used in @rule.
 */
void
ipfw_unbind_table_rule(struct ip_fw_chain *chain, struct ip_fw *rule)
{
	int cmdlen, l;
	ipfw_insn *cmd;
	struct namedobj_instance *ni;
	struct named_object *no;
	uint16_t kidx;
	uint8_t type;

	ni = CHAIN_TO_NI(chain);

	l = rule->cmd_len;
	cmd = rule->cmd;
	cmdlen = 0;
	for ( ;	l > 0 ; l -= cmdlen, cmd += cmdlen) {
		cmdlen = F_LEN(cmd);

		if (classify_table_opcode(cmd, &kidx, &type) != 0)
			continue;

		no = ipfw_objhash_lookup_kidx(ni, kidx); 

		KASSERT(no != NULL, ("table id %d not found", kidx));
		KASSERT(no->type == type, ("wrong type %d (%d) for table id %d",
		    no->type, type, kidx));
		KASSERT(no->refcnt > 0, ("refcount for table %d is %d",
		    kidx, no->refcnt));

		no->refcnt--;
	}
}


/*
 * Removes table bindings for every rule in rule chain @head.
 */
void
ipfw_unbind_table_list(struct ip_fw_chain *chain, struct ip_fw *head)
{
	struct ip_fw *rule;

	while ((rule = head) != NULL) {
		head = head->x_next;
		ipfw_unbind_table_rule(chain, rule);
	}
}


/* end of file */
