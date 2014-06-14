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
 * Lookup table support for ipfw
 *
 * Lookup tables are implemented (at the moment) using the radix
 * tree used for routing tables. Tables store key-value entries, where
 * keys are network prefixes (addr/masklen), and values are integers.
 * As a degenerate case we can interpret keys as 32-bit integers
 * (with a /32 mask).
 *
 * The table is protected by the IPFW lock even for manipulation coming
 * from userland, because operations are typically fast.
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
 * `ftype` is pure userland field helping to properly format table data
 *     e.g. "value is IPv4 nexthop" or "key is port number"
 *
 */
struct table_config {
	struct named_object	no;
	uint8_t		ftype;		/* format table type */
	uint8_t		linked;		/* 1 if already linked */
	uint16_t	spare0;
	uint32_t	count;		/* Number of records */
	char		tablename[64];	/* table name */
	struct table_algo	*ta;	/* Callbacks for given algo */
	void		*astate;	/* algorithm state */
	struct table_info	ti;	/* data to put to table_info */
};
#define	TABLE_SET(set)	((V_fw_tables_sets != 0) ? set : 0)

struct tables_config {
	struct namedobj_instance	*namehash;
	int				algo_count;
	struct table_algo 		*algo[256];
};

static struct table_config *find_table(struct namedobj_instance *ni,
    struct tid_info *ti);
static struct table_config *alloc_table_config(struct namedobj_instance *ni,
    struct tid_info *ti, struct table_algo *ta);
static void free_table_config(struct namedobj_instance *ni,
    struct table_config *tc);
static void link_table(struct ip_fw_chain *chain, struct table_config *tc);
static void unlink_table(struct ip_fw_chain *chain, struct table_config *tc);
static void free_table_state(void **state, void **xstate, uint8_t type);
static int export_tables(struct ip_fw_chain *ch, ipfw_obj_lheader *olh);
static void export_table_info(struct table_config *tc, ipfw_xtable_info *i);
static int dump_table_xentry(void *e, void *arg);

static struct table_algo *find_table_algo(struct tables_config *tableconf,
    struct tid_info *ti);

#define	CHAIN_TO_TCFG(chain)	((struct tables_config *)(chain)->tblcfg)
#define	CHAIN_TO_NI(chain)	(CHAIN_TO_TCFG(chain)->namehash)
#define	KIDX_TO_TI(ch, k)	(&(((struct table_info *)(ch)->tablestate)[k]))



int
ipfw_add_table_entry(struct ip_fw_chain *ch, struct tid_info *ti,
    struct tentry_info *tei)
{
	struct table_config *tc, *tc_new;
	struct table_algo *ta;
	struct namedobj_instance *ni;
	uint16_t kidx;
	int error;
	char ta_buf[128];

#if 0
	if (ti->uidx >= V_fw_tables_max)
		return (EINVAL);
#endif

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

		/* Reference and unlock */
		tc->no.refcnt++;
		ta = tc->ta;
	}
	IPFW_UH_WUNLOCK(ch);

	tc_new = NULL;
	if (ta == NULL) {
		/* Table not found. We have to create new one */
		if ((ta = find_table_algo(CHAIN_TO_TCFG(ch), ti)) == NULL)
			return (ENOTSUP);

		tc_new = alloc_table_config(ni, ti, ta);
		if (tc_new == NULL)
			return (ENOMEM);
	}

	/* Prepare record (allocate memory) */
	memset(&ta_buf, 0, sizeof(ta_buf));
	error = ta->prepare_add(tei, &ta_buf);
	if (error != 0) {
		if (tc_new != NULL)
			free_table_config(ni, tc_new);
		return (error);
	}

	IPFW_UH_WLOCK(ch);

	ni = CHAIN_TO_NI(ch);

	if (tc == NULL) {
		/* Check if another table was allocated by other thread */
		if ((tc = find_table(ni, ti)) != NULL) {

			/*
			 * Check if algoritm is the same since we've
			 * already allocated state using @ta algoritm
			 * callbacks.
			 */
			if (tc->ta != ta) {
				IPFW_UH_WUNLOCK(ch);
				free_table_config(ni, tc);
				return (EINVAL);
			}
		} else {
			/*
			 * We're first to create this table.
			 * Set tc_new to zero not to free it afterwards.
			 */
			tc = tc_new;
			tc_new = NULL;

			/* Allocate table index. */
			if (ipfw_objhash_alloc_idx(ni, ti->set, &kidx) != 0) {
				/* Index full. */
				IPFW_UH_WUNLOCK(ch);
				printf("Unable to allocate index for table %s."
				    " Consider increasing "
				    "net.inet.ip.fw.tables_max",
				    tc->no.name);
				free_table_config(ni, tc);
				return (EBUSY);
			}
			/* Save kidx */
			tc->no.kidx = kidx;
		}
	} else {
		/* Drop reference we've used in first search */
		tc->no.refcnt--;
	}
	
	/* We've got valid table in @tc. Let's add data */
	kidx = tc->no.kidx;
	ta = tc->ta;

	IPFW_WLOCK(ch);

	if (tc->linked == 0) {
		link_table(ch, tc);
	}

	error = ta->add(tc->astate, KIDX_TO_TI(ch, kidx), tei, &ta_buf);

	IPFW_WUNLOCK(ch);

	if (error == 0)
		tc->count++;

	IPFW_UH_WUNLOCK(ch);

	if (tc_new != NULL)
		free_table_config(ni, tc);

	if (error != 0)
		ta->flush_entry(tei, &ta_buf);

	return (error);
}

int
ipfw_del_table_entry(struct ip_fw_chain *ch, struct tid_info *ti,
    struct tentry_info *tei)
{
	struct table_config *tc;
	struct table_algo *ta;
	struct namedobj_instance *ni;
	uint16_t kidx;
	int error;
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

	ta = tc->ta;

	memset(&ta_buf, 0, sizeof(ta_buf));
	if ((error = ta->prepare_del(tei, &ta_buf)) != 0) {
		IPFW_UH_WUNLOCK(ch);
		return (error);
	}

	kidx = tc->no.kidx;

	IPFW_WLOCK(ch);
	error = ta->del(tc->astate, KIDX_TO_TI(ch, kidx), tei, &ta_buf);
	IPFW_WUNLOCK(ch);

	if (error == 0)
		tc->count--;

	IPFW_UH_WUNLOCK(ch);

	if (error != 0)
		return (error);

	ta->flush_entry(tei, &ta_buf);
	return (0);
}

/*
 * Flushes all entries in given table.
 */
int
ipfw_flush_table(struct ip_fw_chain *ch, struct tid_info *ti)
{
	struct namedobj_instance *ni;
	struct table_config *tc;
	struct table_algo *ta;
	struct table_info ti_old, ti_new, *tablestate;
	void *astate_old, *astate_new;
	int error;
	uint16_t kidx;

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
	IPFW_UH_WUNLOCK(ch);

	/*
	 * Stage 2: allocate new table instance using same algo.
	 */
	memset(&ti_new, 0, sizeof(struct table_info));
	if ((error = ta->init(&astate_new, &ti_new)) != 0) {
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
 */
int
ipfw_destroy_table(struct ip_fw_chain *ch, struct tid_info *ti)
{
	struct namedobj_instance *ni;
	struct table_config *tc;

	ti->set = TABLE_SET(ti->set);

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
	if (ipfw_objhash_free_idx(ni, tc->no.set, tc->no.kidx) != 0)
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
	if (ipfw_objhash_free_idx(ni, no->set, no->kidx) != 0)
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
	int new_blocks;

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
 * High-level handlers for setsockopt
 */
int
ipfw_listsize_tables(struct ip_fw_chain *ch, struct sockopt *sopt,
    ip_fw3_opheader *op3, size_t valsize)
{
	struct _ipfw_obj_lheader *olh;

	olh = (struct _ipfw_obj_lheader *)op3;
	
	olh->size = sizeof(*olh); /* Make export_table store needed size */

	IPFW_UH_RLOCK(ch);
	export_tables(ch, olh);
	IPFW_UH_RUNLOCK(ch);

	sopt->sopt_valsize = sizeof(*olh);
	return (sooptcopyout(sopt, olh, sopt->sopt_valsize));
}


int
ipfw_list_tables(struct ip_fw_chain *ch, struct sockopt *sopt,
    ip_fw3_opheader *op3, size_t valsize)
{
	struct _ipfw_obj_lheader *olh;
	uint32_t sz, sz_min, sz_max;
	int error;

	olh = (struct _ipfw_obj_lheader *)op3;

	if (valsize != olh->size)
		return (EINVAL);

	/*
	 * Check if array size is "reasonable":
	 * Permit valsize between current size and
	 * 2x current size + 1
	 */
	IPFW_UH_RLOCK(ch);
	sz = ipfw_objhash_count(CHAIN_TO_NI(ch));
	IPFW_UH_RUNLOCK(ch);
	
	sz_min = sz * sizeof(ipfw_xtable_info) + sizeof(*olh);
	sz_max = sz_min + (sz + 1) * sizeof(ipfw_xtable_info);

	if (valsize < sz_min || valsize > sz_max)
		return (EINVAL);
	
	olh = malloc(valsize, M_TEMP, M_ZERO | M_WAITOK);
	/* Copy header to new storage */
	memcpy(olh, op3, sizeof(*olh));

	IPFW_UH_RLOCK(ch);
	error = export_tables(ch, olh);
	IPFW_UH_RUNLOCK(ch);

	if (error != 0) {
		free(olh, M_TEMP);
		return (error);
	}

	/* 
	 * Since we call sooptcopyin() with small buffer,
	 * sopt_valsize is decreased to reflect supplied
	 * buffer size. Set it back to original value.
	 */
	sopt->sopt_valsize = valsize;
	error = sooptcopyout(sopt, olh, olh->size);
	free(olh, M_TEMP);

	return (0);
}

struct dump_args {
	struct table_info *ti;
	struct table_config *tc;
	ipfw_table_entry *ent;
	ipfw_table_xentry *xent;
	uint32_t cnt;
	uint32_t size;
	uint16_t uidx;
};

int
ipfw_dump_table(struct ip_fw_chain *ch, struct sockopt *sopt,
    ip_fw3_opheader *op3, size_t valsize)
{
	struct _ipfw_obj_header *oh;
	ipfw_xtable_info *i;
	struct tid_info ti;
	struct table_config *tc;
	struct table_algo *ta;
	struct dump_args da;
	uint32_t sz, sz_min, sz_max;
	int error;

	oh = (struct _ipfw_obj_header *)op3;

	memset(&ti, 0, sizeof(ti));
	ti.set = oh->set;
	ti.uidx = oh->idx;
	ti.tlvs = &oh->ntlv;
	ti.tlen = oh->ntlv.head.length;

	IPFW_UH_RLOCK(ch);
	if ((tc = find_table(CHAIN_TO_NI(ch), &ti)) == NULL) {
		IPFW_UH_RUNLOCK(ch);
		return (ESRCH);
	}
	sz = tc->count;
	IPFW_UH_RUNLOCK(ch);

	/*
	 * Check if array size is "reasonable":
	 * Permit valsize between current size and
	 * 2x current size + 1
	 */
	
	sz_min = sz * sizeof(ipfw_table_xentry) + sizeof(ipfw_xtable_info) +
	    sizeof(*oh);

	sz_max = sz_min + (sz + 1) * sizeof(ipfw_table_xentry);

	if (valsize < sz_min || valsize > sz_max)
		return (EINVAL);
	
	oh = malloc(valsize, M_TEMP, M_ZERO | M_WAITOK);
	i = (ipfw_xtable_info *)(oh + 1);
	/* Copy header to new storage */
	memcpy(oh, op3, sizeof(*oh));

	IPFW_UH_RLOCK(ch);
	/* Find table and export info */
	if ((tc = find_table(CHAIN_TO_NI(ch), &ti)) == NULL) {
		IPFW_UH_RUNLOCK(ch);
		free(oh, M_TEMP);
		return (ESRCH);
	}

	export_table_info(tc, i);
	if (i->size > valsize) {
		IPFW_UH_RUNLOCK(ch);
		/* XXX: Should we pass size structure back ? */
		free(oh, M_TEMP);
		return (EINVAL);
	}

	/*
	 * Do the actual dump in eXtended format
	 */
	da.ti = KIDX_TO_TI(ch, tc->no.kidx);
	da.tc = tc;
	da.xent = (ipfw_table_xentry *)(i + 1);
	da.size = (valsize - sizeof(*oh) - sizeof(ipfw_xtable_info)) /
	    sizeof(ipfw_table_xentry);

	ta = tc->ta;

	ta->foreach(tc->astate, da.ti, dump_table_xentry, &da);
	IPFW_UH_RUNLOCK(ch);

	/* 
	 * Since we call sooptcopyin() with small buffer,
	 * sopt_valsize is decreased to reflect supplied
	 * buffer size. Set it back to original value.
	 */
	sopt->sopt_valsize = valsize;
	error = sooptcopyout(sopt, oh, i->size);
	free(oh, M_TEMP);

	return (0);
}

static void
export_table_info(struct table_config *tc, ipfw_xtable_info *i)
{
	
	i->type = tc->no.type;
	i->ftype = tc->ftype;
	i->atype = tc->ta->idx;
	i->set = tc->no.set;
	i->kidx = tc->no.kidx;
	i->refcnt = tc->no.refcnt;
	i->count = tc->count;
	i->size = tc->count * sizeof(ipfw_table_xentry);
	i->size += sizeof(ipfw_obj_header) + sizeof(ipfw_xtable_info);
	strlcpy(i->tablename, tc->tablename, sizeof(i->tablename));
}

int
ipfw_count_tables(struct ip_fw_chain *ch, ipfw_obj_lheader *olh)
{
	uint32_t count;

	count = ipfw_objhash_count(CHAIN_TO_NI(ch));

	olh->count = count;
	olh->size = count * sizeof(ipfw_xtable_info) + sizeof(ipfw_obj_lheader);
	olh->objsize = sizeof(ipfw_xtable_info);

	return (0);
}

int
ipfw_describe_table(struct ip_fw_chain *ch, struct tid_info *ti,
    ipfw_xtable_info *i)
{
	struct table_config *tc;

	if ((tc = find_table(CHAIN_TO_NI(ch), ti)) == NULL)
		return (ESRCH);

	export_table_info(tc, i);

	return (0);
}

static void
export_table_internal(struct namedobj_instance *ni, struct named_object *no,
    void *arg)
{
	ipfw_obj_lheader *olh;
	ipfw_xtable_info *i;

	olh = (ipfw_obj_lheader *)arg;
	i = (ipfw_xtable_info *)(caddr_t)(olh + 1) + olh->count;
	olh->count++;

	export_table_info((struct table_config *)no, i);
}

/*
 * Export all tables as ipfw_xtable_info structures to
 * storage provided by @olh.
 * Returns 0 on success.
 */
static int
export_tables(struct ip_fw_chain *ch, ipfw_obj_lheader *olh)
{
	uint32_t size;
	uint32_t count;

	count = ipfw_objhash_count(CHAIN_TO_NI(ch));
	size = count * sizeof(ipfw_xtable_info) + sizeof(ipfw_obj_lheader);
	if (size > olh->size) {
		/* Store new values anyway */
		olh->count = count;
		olh->size = size;
		olh->objsize = sizeof(ipfw_xtable_info);
		return (ENOMEM);
	}

	olh->count = 0;
	ipfw_objhash_foreach(CHAIN_TO_NI(ch), export_table_internal, olh);

	olh->count = count;
	olh->size = size;
	olh->objsize = sizeof(ipfw_xtable_info);

	return (0);
}

int
ipfw_count_table(struct ip_fw_chain *ch, struct tid_info *ti, uint32_t *cnt)
{
	struct table_config *tc;

	if ((tc = find_table(CHAIN_TO_NI(ch), ti)) == NULL)
		return (ESRCH);
	*cnt = tc->count;
	return (0);
}


int
ipfw_count_xtable(struct ip_fw_chain *ch, struct tid_info *ti, uint32_t *cnt)
{
	struct table_config *tc;

	if ((tc = find_table(CHAIN_TO_NI(ch), ti)) == NULL)
		return (0); /* 'table all list' requires success */
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

	da = (struct dump_args *)arg;

	tc = da->tc;
	ta = tc->ta;

	/* Out of memory, returning */
	if (da->cnt == da->size)
		return (1);
	ent = da->ent++;
	ent->tbl = da->uidx;
	da->cnt++;

	return (ta->dump_entry(tc->astate, da->ti, e, ent));
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

	if (ta->dump_entry == NULL)
		return (0);	/* Legacy dump support is not necessary */

	da.ti = KIDX_TO_TI(ch, tc->no.kidx);
	da.tc = tc;
	da.ent = &tbl->ent[0];
	da.cnt = 0;
	da.size = tbl->size;

	tbl->cnt = 0;
	ta->foreach(tc->astate, da.ti, dump_table_entry, &da);
	tbl->cnt = da.cnt;

	return (0);
}


static int
dump_table_xentry(void *e, void *arg)
{
	struct dump_args *da;
	struct table_config *tc;
	struct table_algo *ta;
	ipfw_table_xentry *xent;

	da = (struct dump_args *)arg;

	tc = da->tc;
	ta = tc->ta;

	/* Out of memory, returning */
	if (da->cnt == da->size)
		return (1);
	xent = da->xent++;
	xent->len = sizeof(ipfw_table_xentry);
	xent->tbl = da->uidx;
	da->cnt++;

	return (ta->dump_xentry(tc->astate, da->ti, e, xent));
}


int
ipfw_dump_xtable(struct ip_fw_chain *ch, struct tid_info *ti, ipfw_xtable *xtbl)
{
	struct table_config *tc;
	struct table_algo *ta;
	struct dump_args da;

	xtbl->cnt = 0;

	if ((tc = find_table(CHAIN_TO_NI(ch), ti)) == NULL)
		return (0);	/* XXX: We should return ESRCH */

	da.ti = KIDX_TO_TI(ch, tc->no.kidx);
	da.tc = tc;
	da.xent = &xtbl->xent[0];
	da.size = xtbl->size;
	da.cnt = 0;
	xtbl->type = tc->no.type;
	xtbl->tbl = ti->uidx;
	ta = tc->ta;

	ta->foreach(tc->astate, da.ti, dump_table_xentry, &da);
	xtbl->cnt = da.cnt;

	return (0);
}

/*
 * Table algorithms
 */ 

/*
 * Finds algoritm by index or table type
 */
static struct table_algo *
find_table_algo(struct tables_config *tcfg, struct tid_info *ti)
{

	/* Search by index */
	if (ti->atype != 0) {
		if (ti->atype > tcfg->algo_count)
			return (NULL);
		return (tcfg->algo[ti->atype]);
	}

	/* Search by type */
	switch (ti->type) {
	case IPFW_TABLE_CIDR:
		return (&radix_cidr);
	case IPFW_TABLE_INTERFACE:
		return (&radix_iface);
	}

	return (NULL);
}

void
ipfw_add_table_algo(struct ip_fw_chain *ch, struct table_algo *ta)
{
	struct tables_config *tcfg;

	tcfg = CHAIN_TO_TCFG(ch);

	KASSERT(tcfg->algo_count < 255, ("Increase algo array size"));

	tcfg->algo[++tcfg->algo_count] = ta;
	ta->idx = tcfg->algo_count;
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
				//type = IPFW_TABLE_U16;
				break;
			case 4:
				/* uid/gid */
				//type = IPFW_TABLE_U32;
			case 5:
				//type = IPFW_TABLE_U32;
				/* jid */
			case 6:
				//type = IPFW_TABLE_U16;
				/* dscp */
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
	}
}

static char *
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
		if (ntlv->head.type != IPFW_TLV_NAME)
			continue;
		if (ntlv->idx != uidx)
			continue;
		
		return (ntlv->name);
	}

	return (NULL);
}

static struct table_config *
find_table(struct namedobj_instance *ni, struct tid_info *ti)
{
	char *name, bname[16];
	struct named_object *no;

	if (ti->tlvs != NULL) {
		name = find_name_tlv(ti->tlvs, ti->tlen, ti->uidx);
		if (name == NULL)
			return (NULL);
	} else {
		snprintf(bname, sizeof(bname), "%d", ti->uidx);
		name = bname;
	}

	no = ipfw_objhash_lookup_name(ni, ti->set, name);

	return ((struct table_config *)no);
}

static struct table_config *
alloc_table_config(struct namedobj_instance *ni, struct tid_info *ti,
    struct table_algo *ta)
{
	char *name, bname[16];
	struct table_config *tc;
	int error;

	if (ti->tlvs != NULL) {
		name = find_name_tlv(ti->tlvs, ti->tlen, ti->uidx);
		if (name == NULL)
			return (NULL);
	} else {
		snprintf(bname, sizeof(bname), "%d", ti->uidx);
		name = bname;
	}

	tc = malloc(sizeof(struct table_config), M_IPFW, M_WAITOK | M_ZERO);
	tc->no.name = tc->tablename;
	tc->no.type = ti->type;
	tc->no.set = ti->set;
	tc->ta = ta;
	strlcpy(tc->tablename, name, sizeof(tc->tablename));

	if (ti->tlvs == NULL) {
		tc->no.compat = 1;
		tc->no.uidx = ti->uidx;
	}

	/* Preallocate data structures for new tables */
	error = ta->init(&tc->astate, &tc->ti);
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
		tc->ta->destroy(&tc->astate, &tc->ti);

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

	tc->linked = 1;
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
}

/*
 * Finds named object by @uidx number.
 * Refs found object, allocate new index for non-existing object.
 * Fills in @pidx with userland/kernel indexes.
 *
 * Returns 0 on success.
 */
static int
bind_table(struct namedobj_instance *ni, struct rule_check_info *ci,
    struct obj_idx *pidx, struct tid_info *ti)
{
	struct table_config *tc;

	pidx->uidx = ti->uidx;
	pidx->type = ti->type;

	tc = find_table(ni, ti);

	if (tc == NULL) {
		/* Try to acquire refcount */
		if (ipfw_objhash_alloc_idx(ni, ti->set, &pidx->kidx) != 0) {
			printf("Unable to allocate table index in set %u."
			    " Consider increasing net.inet.ip.fw.tables_max",
				    ti->set);
			return (EBUSY);
		}

		pidx->new = 1;
		ci->new_tables++;

		return (0);
	}

	/* Check if table type if valid first */
	if (tc->no.type != ti->type)
		return (EINVAL);

	tc->no.refcnt++;

	pidx->kidx = tc->no.kidx;

	return (0);
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
ipfw_rewrite_table_kidx(struct ip_fw_chain *chain, struct ip_fw *rule)
{
	int cmdlen, l;
	ipfw_insn *cmd;
	uint32_t set;
	uint16_t kidx;
	uint8_t type;
	struct named_object *no;
	struct namedobj_instance *ni;

	ni = CHAIN_TO_NI(chain);

	set = TABLE_SET(rule->set);
	
	l = rule->cmd_len;
	cmd = rule->cmd;
	cmdlen = 0;
	for ( ;	l > 0 ; l -= cmdlen, cmd += cmdlen) {
		cmdlen = F_LEN(cmd);

		if (classify_table_opcode(cmd, &kidx, &type) != 0)
			continue;

		if ((no = ipfw_objhash_lookup_idx(ni, set, kidx)) == NULL)
			return (1);

		if (no->compat == 0)
			return (2);

		update_table_opcode(cmd, no->uidx);
	}

	return (0);
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
	struct obj_idx *pidx, *p, *oib;
	struct namedobjects_head nh;
	struct tid_info ti;

	ni = CHAIN_TO_NI(chain);

	/*
	 * Prepare an array for storing opcode indices.
	 * Use stack allocation by default.
	 */
	if (ci->table_opcodes <= (sizeof(ci->obuf)/sizeof(ci->obuf[0]))) {
		/* Stack */
		pidx = ci->obuf;
	} else
		pidx = malloc(ci->table_opcodes * sizeof(struct obj_idx),
		    M_IPFW, M_WAITOK | M_ZERO);

	oib = pidx;
	error = 0;

	type = 0;
	ftype = 0;

	ci->tableset = TABLE_SET(ci->krule->set);

	memset(&ti, 0, sizeof(ti));
	ti.set = ci->tableset;
	ti.tlvs = ci->tlvs;
	ti.tlen = ci->tlen;

	/*
	 * Stage 1: reference existing tables and determine number
	 * of tables we need to allocate
	 */
	IPFW_UH_WLOCK(chain);

	l = ci->krule->cmd_len;
	cmd = ci->krule->cmd;
	cmdlen = 0;
	for ( ;	l > 0 ; l -= cmdlen, cmd += cmdlen) {
		cmdlen = F_LEN(cmd);

		if (classify_table_opcode(cmd, &ti.uidx, &ti.type) != 0)
			continue;

		/*
		 * Got table opcode with necessary info.
		 * Try to reference existing tables and allocate
		 * indices for non-existing one while holding write lock.
		 */
		if ((error = bind_table(ni, ci, pidx, &ti)) != 0) {
			printf("error!\n");
			break;
		}

		/*
		 * @pidx stores either existing ref'd table id or new one.
		 * Move to next index
		 */

		pidx++;
	}

	if (error != 0) {
		/* Unref everything we have already done */
		for (p = oib; p < pidx; p++) {
			if (p->new != 0) {
				ipfw_objhash_free_idx(ni, ci->tableset,p->kidx);
				continue;
			}

			/* Find & unref by existing idx */
			no = ipfw_objhash_lookup_idx(ni, ci->tableset, p->kidx);
			KASSERT(no!=NULL, ("Ref'd table %d disappeared",
			    p->kidx));

			no->refcnt--;
		}

		IPFW_UH_WUNLOCK(chain);

		if (oib != ci->obuf)
			free(oib, M_IPFW);

		return (error);
	}

	IPFW_UH_WUNLOCK(chain);

	/*
	 * Stage 2: allocate table configs for every non-existent table
	 */

	/* Prepare queue to store configs */
	TAILQ_INIT(&nh);
	if (ci->new_tables > 0) {
		for (p = oib; p < pidx; p++) {
			if (p->new == 0)
				continue;

			/* TODO: get name from TLV */
			ti.uidx = p->uidx;
			ti.type = p->type;
			ti.atype = 0;

			ta = find_table_algo(CHAIN_TO_TCFG(chain), &ti);
			if (ta == NULL) {
				error = ENOTSUP;
				goto free;
			}
			tc = alloc_table_config(ni, &ti, ta);

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
				if (strcmp(no->name, no_tmp->name) != 0)
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
			if (no_n != NULL) {
				/* Increase refcount for existing table */
				no_n->refcnt++;
				/* Keep oib array in sync: update kindx */
				for (p = oib; p < pidx; p++) {
					if (p->kidx == no->kidx) {
						p->kidx = no_n->kidx;
						break;
					}
				}

				continue;
			}

			/* New table. Attach to runtime hash */
			TAILQ_REMOVE(&nh, no, nn_next);

			link_table(chain, (struct table_config *)no);
		}
		IPFW_WUNLOCK(chain);
	}

	/* Perform rule rewrite */
	l = ci->krule->cmd_len;
	cmd = ci->krule->cmd;
	cmdlen = 0;
	pidx = oib;
	for ( ;	l > 0 ; l -= cmdlen, cmd += cmdlen) {
		cmdlen = F_LEN(cmd);

		if (classify_table_opcode(cmd, &uidx, &type) != 0)
			continue;
		update_table_opcode(cmd, pidx->kidx);
		pidx++;
	}

	IPFW_UH_WUNLOCK(chain);

	error = 0;

	/*
	 * Stage 4: free resources
	 */
free:
	TAILQ_FOREACH_SAFE(no, &nh, nn_next, no_tmp)
		free_table_config(ni, tc);

	if (oib != ci->obuf)
		free(oib, M_IPFW);

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
	uint32_t set;
	uint16_t kidx;
	uint8_t type;

	ni = CHAIN_TO_NI(chain);

	set = TABLE_SET(rule->set);

	l = rule->cmd_len;
	cmd = rule->cmd;
	cmdlen = 0;
	for ( ;	l > 0 ; l -= cmdlen, cmd += cmdlen) {
		cmdlen = F_LEN(cmd);

		if (classify_table_opcode(cmd, &kidx, &type) != 0)
			continue;

		no = ipfw_objhash_lookup_idx(ni, set, kidx); 

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
