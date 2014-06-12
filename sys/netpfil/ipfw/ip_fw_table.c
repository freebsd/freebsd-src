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
#include "opt_inet.h"
#ifndef INET
#error IPFIREWALL requires INET.
#endif /* INET */
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <net/if.h>	/* ip_fw.h requires IFNAMSIZ */
#include <net/radix.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/ip_var.h>	/* struct ipfw_rule_ref */
#include <netinet/ip_fw.h>

#include <netpfil/ipfw/ip_fw_private.h>

#ifdef MAC
#include <security/mac/mac_framework.h>
#endif

static MALLOC_DEFINE(M_IPFW_TBL, "ipfw_tbl", "IpFw tables");

struct table_entry {
	struct radix_node	rn[2];
	struct sockaddr_in	addr, mask;
	u_int32_t		value;
};

struct xaddr_iface {
	uint8_t		if_len;		/* length of this struct */
	uint8_t		pad[7];		/* Align name */
	char 		ifname[IF_NAMESIZE];	/* Interface name */
};

struct table_xentry {
	struct radix_node	rn[2];
	union {
#ifdef INET6
		struct sockaddr_in6	addr6;
#endif
		struct xaddr_iface	iface;
	} a;
	union {
#ifdef INET6
		struct sockaddr_in6	mask6;
#endif
		struct xaddr_iface	ifmask;
	} m;
	u_int32_t		value;
};

 /*
 * Table has the following `type` concepts:
 *
 * `type` represents lookup key type (cidr, ifp, uid, etc..)
 * `ftype` is pure userland field helping to properly format table data
 * `atype` represents exact lookup algorithm for given tabletype.
 *     For example, we can use more efficient search schemes if we plan
 *     to use some specific table for storing host-routes only.
 *
 */
struct table_config {
	struct named_object	no;
	uint8_t		ftype;		/* format table type */
	uint8_t		atype;		/* algorith type */
	uint8_t		linked;		/* 1 if already linked */
	uint8_t		spare0;
	uint32_t	count;		/* Number of records */
	char		tablename[64];	/* table name */
	void		*state;		/* Store some state if needed */
	void		*xstate;
};
#define	TABLE_SET(set)	((V_fw_tables_sets != 0) ? set : 0)

struct tables_config {
	struct namedobj_instance	*namehash;
};

static struct table_config *find_table(struct namedobj_instance *ni,
    struct tid_info *ti);
static struct table_config *alloc_table_config(struct namedobj_instance *ni,
    struct tid_info *ti);
static void free_table_config(struct namedobj_instance *ni,
    struct table_config *tc);
static void link_table(struct ip_fw_chain *chain, struct table_config *tc);
static void unlink_table(struct ip_fw_chain *chain, struct table_config *tc);
static int alloc_table_state(void **state, void **xstate, uint8_t type);
static void free_table_state(void **state, void **xstate, uint8_t type);


#define	CHAIN_TO_TCFG(chain)	((struct tables_config *)(chain)->tblcfg)
#define	CHAIN_TO_NI(chain)	(CHAIN_TO_TCFG(chain)->namehash)


/*
 * The radix code expects addr and mask to be array of bytes,
 * with the first byte being the length of the array. rn_inithead
 * is called with the offset in bits of the lookup key within the
 * array. If we use a sockaddr_in as the underlying type,
 * sin_len is conveniently located at offset 0, sin_addr is at
 * offset 4 and normally aligned.
 * But for portability, let's avoid assumption and make the code explicit
 */
#define KEY_LEN(v)	*((uint8_t *)&(v))
#define KEY_OFS		(8*offsetof(struct sockaddr_in, sin_addr))
/*
 * Do not require radix to compare more than actual IPv4/IPv6 address
 */
#define KEY_LEN_INET	(offsetof(struct sockaddr_in, sin_addr) + sizeof(in_addr_t))
#define KEY_LEN_INET6	(offsetof(struct sockaddr_in6, sin6_addr) + sizeof(struct in6_addr))
#define KEY_LEN_IFACE	(offsetof(struct xaddr_iface, ifname))

#define OFF_LEN_INET	(8 * offsetof(struct sockaddr_in, sin_addr))
#define OFF_LEN_INET6	(8 * offsetof(struct sockaddr_in6, sin6_addr))
#define OFF_LEN_IFACE	(8 * offsetof(struct xaddr_iface, ifname))


#ifdef INET6
static inline void
ipv6_writemask(struct in6_addr *addr6, uint8_t mask)
{
	uint32_t *cp;

	for (cp = (uint32_t *)addr6; mask >= 32; mask -= 32)
		*cp++ = 0xFFFFFFFF;
	*cp = htonl(mask ? ~((1 << (32 - mask)) - 1) : 0);
}
#endif

int
ipfw_add_table_entry(struct ip_fw_chain *ch, struct tid_info *ti,
    struct tentry_info *tei)
{
	struct radix_node_head *rnh;
	struct table_entry *ent;
	struct table_xentry *xent;
	struct radix_node *rn;
	in_addr_t addr;
	int offset;
	void *ent_ptr;
	struct sockaddr *addr_ptr, *mask_ptr;
	struct table_config *tc, *tc_new;
	struct namedobj_instance *ni;
	char c;
	uint8_t mlen;
	uint16_t kidx;

	if (ti->uidx >= V_fw_tables_max)
		return (EINVAL);

	mlen = tei->masklen;

	switch (ti->type) {
	case IPFW_TABLE_CIDR:
		if (tei->plen == sizeof(in_addr_t)) {
#ifdef INET
			/* IPv4 case */
			if (mlen > 32)
				return (EINVAL);
			ent = malloc(sizeof(*ent), M_IPFW_TBL, M_WAITOK | M_ZERO);
			ent->value = tei->value;
			/* Set 'total' structure length */
			KEY_LEN(ent->addr) = KEY_LEN_INET;
			KEY_LEN(ent->mask) = KEY_LEN_INET;
			/* Set offset of IPv4 address in bits */
			offset = OFF_LEN_INET;
			ent->mask.sin_addr.s_addr =
			    htonl(mlen ? ~((1 << (32 - mlen)) - 1) : 0);
			addr = *((in_addr_t *)tei->paddr);
			ent->addr.sin_addr.s_addr = addr & ent->mask.sin_addr.s_addr;
			/* Set pointers */
			ent_ptr = ent;
			addr_ptr = (struct sockaddr *)&ent->addr;
			mask_ptr = (struct sockaddr *)&ent->mask;
#endif
#ifdef INET6
		} else if (tei->plen == sizeof(struct in6_addr)) {
			/* IPv6 case */
			if (mlen > 128)
				return (EINVAL);
			xent = malloc(sizeof(*xent), M_IPFW_TBL, M_WAITOK | M_ZERO);
			xent->value = tei->value;
			/* Set 'total' structure length */
			KEY_LEN(xent->a.addr6) = KEY_LEN_INET6;
			KEY_LEN(xent->m.mask6) = KEY_LEN_INET6;
			/* Set offset of IPv6 address in bits */
			offset = OFF_LEN_INET6;
			ipv6_writemask(&xent->m.mask6.sin6_addr, mlen);
			memcpy(&xent->a.addr6.sin6_addr, tei->paddr,
			    sizeof(struct in6_addr));
			APPLY_MASK(&xent->a.addr6.sin6_addr, &xent->m.mask6.sin6_addr);
			/* Set pointers */
			ent_ptr = xent;
			addr_ptr = (struct sockaddr *)&xent->a.addr6;
			mask_ptr = (struct sockaddr *)&xent->m.mask6;
#endif
		} else {
			/* Unknown CIDR type */
			return (EINVAL);
		}
		break;
	
	case IPFW_TABLE_INTERFACE:
		/* Check if string is terminated */
		c = ((char *)tei->paddr)[IF_NAMESIZE - 1];
		((char *)tei->paddr)[IF_NAMESIZE - 1] = '\0';
		mlen = strlen((char *)tei->paddr);
		if ((mlen == IF_NAMESIZE - 1) && (c != '\0'))
			return (EINVAL);

		/* Include last \0 into comparison */
		mlen++;

		xent = malloc(sizeof(*xent), M_IPFW_TBL, M_WAITOK | M_ZERO);
		xent->value = tei->value;
		/* Set 'total' structure length */
		KEY_LEN(xent->a.iface) = KEY_LEN_IFACE + mlen;
		KEY_LEN(xent->m.ifmask) = KEY_LEN_IFACE + mlen;
		/* Set offset of interface name in bits */
		offset = OFF_LEN_IFACE;
		memcpy(xent->a.iface.ifname, tei->paddr, mlen);
		/* Assume direct match */
		/* TODO: Add interface pattern matching */
#if 0
		memset(xent->m.ifmask.ifname, 0xFF, IF_NAMESIZE);
		mask_ptr = (struct sockaddr *)&xent->m.ifmask;
#endif
		/* Set pointers */
		ent_ptr = xent;
		addr_ptr = (struct sockaddr *)&xent->a.iface;
		mask_ptr = NULL;
		break;

	default:
		return (EINVAL);
	}

	IPFW_UH_WLOCK(ch);

	ni = CHAIN_TO_NI(ch);

	tc_new = NULL;
	if ((tc = find_table(ni, ti)) == NULL) {
		/* Not found. We have to create new one */
		IPFW_UH_WUNLOCK(ch);

		tc_new = alloc_table_config(ni, ti);
		if (tc_new == NULL)
			return (ENOMEM);

		IPFW_UH_WLOCK(ch);

		/* Check if table has already allocated by other thread */
		if ((tc = find_table(ni, ti)) != NULL) {
			if (tc->no.type != ti->type) {
				IPFW_UH_WUNLOCK(ch);
				free_table_config(ni, tc);
				return (EINVAL);
			}
		} else {
			/*
			 * New table.
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
		/* We still have to check table type */
		if (tc->no.type != ti->type) {
			IPFW_UH_WUNLOCK(ch);
			return (EINVAL);
		}
	}
	kidx = tc->no.kidx;

	/* We've got valid table in @tc. Let's add data */
	IPFW_WLOCK(ch);

	if (tc->linked == 0) {
		link_table(ch, tc);
	}

	/* XXX: Temporary until splitting add/del to per-type functions */
	rnh = NULL;
	switch (ti->type) {
	case IPFW_TABLE_CIDR:
		if (tei->plen == sizeof(in_addr_t))
			rnh = ch->tables[kidx];
		else
			rnh = ch->xtables[kidx];
		break;
	case IPFW_TABLE_INTERFACE:
		rnh = ch->xtables[kidx];
		break;
	}

	rn = rnh->rnh_addaddr(addr_ptr, mask_ptr, rnh, ent_ptr);
	IPFW_WUNLOCK(ch);
	IPFW_UH_WUNLOCK(ch);

	if (tc_new != NULL)
		free_table_config(ni, tc);

	if (rn == NULL) {
		free(ent_ptr, M_IPFW_TBL);
		return (EEXIST);
	}

	return (0);
}

int
ipfw_del_table_entry(struct ip_fw_chain *ch, struct tid_info *ti,
    struct tentry_info *tei)
{
	struct radix_node_head *rnh;
	struct table_entry *ent;
	in_addr_t addr;
	struct sockaddr_in sa, mask;
	struct sockaddr *sa_ptr, *mask_ptr;
	struct table_config *tc;
	struct namedobj_instance *ni;
	char c;
	uint8_t mlen;
	uint16_t kidx;

	if (ti->uidx >= V_fw_tables_max)
		return (EINVAL);

	mlen = tei->masklen;

	switch (ti->type) {
	case IPFW_TABLE_CIDR:
		if (tei->plen == sizeof(in_addr_t)) {
			/* Set 'total' structure length */
			KEY_LEN(sa) = KEY_LEN_INET;
			KEY_LEN(mask) = KEY_LEN_INET;
			mask.sin_addr.s_addr = htonl(mlen ? ~((1 << (32 - mlen)) - 1) : 0);
			addr = *((in_addr_t *)tei->paddr);
			sa.sin_addr.s_addr = addr & mask.sin_addr.s_addr;
			sa_ptr = (struct sockaddr *)&sa;
			mask_ptr = (struct sockaddr *)&mask;
#ifdef INET6
		} else if (tei->plen == sizeof(struct in6_addr)) {
			/* IPv6 case */
			if (mlen > 128)
				return (EINVAL);
			struct sockaddr_in6 sa6, mask6;
			memset(&sa6, 0, sizeof(struct sockaddr_in6));
			memset(&mask6, 0, sizeof(struct sockaddr_in6));
			/* Set 'total' structure length */
			KEY_LEN(sa6) = KEY_LEN_INET6;
			KEY_LEN(mask6) = KEY_LEN_INET6;
			ipv6_writemask(&mask6.sin6_addr, mlen);
			memcpy(&sa6.sin6_addr, tei->paddr,
			    sizeof(struct in6_addr));
			APPLY_MASK(&sa6.sin6_addr, &mask6.sin6_addr);
			sa_ptr = (struct sockaddr *)&sa6;
			mask_ptr = (struct sockaddr *)&mask6;
#endif
		} else {
			/* Unknown CIDR type */
			return (EINVAL);
		}
		break;

	case IPFW_TABLE_INTERFACE:
		/* Check if string is terminated */
		c = ((char *)tei->paddr)[IF_NAMESIZE - 1];
		((char *)tei->paddr)[IF_NAMESIZE - 1] = '\0';
		mlen = strlen((char *)tei->paddr);
		if ((mlen == IF_NAMESIZE - 1) && (c != '\0'))
			return (EINVAL);

		struct xaddr_iface ifname, ifmask;
		memset(&ifname, 0, sizeof(ifname));

		/* Include last \0 into comparison */
		mlen++;

		/* Set 'total' structure length */
		KEY_LEN(ifname) = KEY_LEN_IFACE + mlen;
		KEY_LEN(ifmask) = KEY_LEN_IFACE + mlen;
		/* Assume direct match */
		/* FIXME: Add interface pattern matching */
#if 0
		memset(ifmask.ifname, 0xFF, IF_NAMESIZE);
		mask_ptr = (struct sockaddr *)&ifmask;
#endif
		mask_ptr = NULL;
		memcpy(ifname.ifname, tei->paddr, mlen);
		/* Set pointers */
		sa_ptr = (struct sockaddr *)&ifname;

		break;

	default:
		return (EINVAL);
	}

	IPFW_UH_RLOCK(ch);
	ni = CHAIN_TO_NI(ch);
	if ((tc = find_table(ni, ti)) == NULL) {
		IPFW_UH_RUNLOCK(ch);
		return (ESRCH);
	}

	if (tc->no.type != ti->type) {
		IPFW_UH_RUNLOCK(ch);
		return (EINVAL);
	}
	kidx = tc->no.kidx;

	IPFW_WLOCK(ch);

	rnh = NULL;
	switch (ti->type) {
	case IPFW_TABLE_CIDR:
		if (tei->plen == sizeof(in_addr_t))
			rnh = ch->tables[kidx];
		else
			rnh = ch->xtables[kidx];
		break;
	case IPFW_TABLE_INTERFACE:
		rnh = ch->xtables[kidx];
		break;
	}

	ent = (struct table_entry *)rnh->rnh_deladdr(sa_ptr, mask_ptr, rnh);
	IPFW_WUNLOCK(ch);

	IPFW_UH_RUNLOCK(ch);

	if (ent == NULL)
		return (ESRCH);

	free(ent, M_IPFW_TBL);
	return (0);
}

static int
flush_table_entry(struct radix_node *rn, void *arg)
{
	struct radix_node_head * const rnh = arg;
	struct table_entry *ent;

	ent = (struct table_entry *)
	    rnh->rnh_deladdr(rn->rn_key, rn->rn_mask, rnh);
	if (ent != NULL)
		free(ent, M_IPFW_TBL);
	return (0);
}

/*
 * Flushes all entries in given table minimizing hoding chain WLOCKs.
 *
 */
int
ipfw_flush_table(struct ip_fw_chain *ch, struct tid_info *ti)
{
	struct namedobj_instance *ni;
	struct table_config *tc;
	void *ostate, *oxstate;
	void *state, *xstate;
	int error;
	uint8_t type;
	uint16_t kidx;

	if (ti->uidx >= V_fw_tables_max)
		return (EINVAL);

	/*
	 * Stage 1: determine table type.
	 * Reference found table to ensure it won't disappear.
	 */
	IPFW_UH_WLOCK(ch);
	ni = CHAIN_TO_NI(ch);
	if ((tc = find_table(ni, ti)) == NULL) {
		IPFW_UH_WUNLOCK(ch);
		return (ESRCH);
	}
	type = tc->no.type;
	tc->no.refcnt++;
	IPFW_UH_WUNLOCK(ch);

	/*
	 * Stage 2: allocate new state for given type.
	 */
	if ((error = alloc_table_state(&state, &xstate, type)) != 0) {
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
	IPFW_WLOCK(ch);

	ni = CHAIN_TO_NI(ch);
	kidx = tc->no.kidx;

	ostate = ch->tables[kidx];
	ch->tables[kidx] = state;
	oxstate = ch->xtables[kidx];
	ch->xtables[kidx] = xstate;

	tc->no.refcnt--;

	IPFW_WUNLOCK(ch);
	IPFW_UH_WUNLOCK(ch);

	/*
	 * Stage 4: perform real flush.
	 */
	free_table_state(&ostate, &xstate, tc->no.type);

	return (0);
}

/*
 * Destroys given table @ti: flushes it,
 */
int
ipfw_destroy_table(struct ip_fw_chain *ch, struct tid_info *ti, int force)
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

	/* Do not permit destroying used tables */
	if (tc->no.refcnt > 0 && force == 0) {
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
	free(ch->tables, M_IPFW);
	free(ch->xtables, M_IPFW);

	ipfw_objhash_destroy(CHAIN_TO_NI(ch));
	free(CHAIN_TO_TCFG(ch), M_IPFW);
}

int
ipfw_init_tables(struct ip_fw_chain *ch)
{
	struct tables_config *tcfg;

	/* Allocate pointers */
	ch->tables = malloc(V_fw_tables_max * sizeof(void *), M_IPFW, M_WAITOK | M_ZERO);
	ch->xtables = malloc(V_fw_tables_max * sizeof(void *), M_IPFW, M_WAITOK | M_ZERO);

	tcfg = malloc(sizeof(struct tables_config), M_IPFW, M_WAITOK | M_ZERO);
	tcfg->namehash = ipfw_objhash_create(V_fw_tables_max);
	ch->tblcfg = tcfg;

	return (0);
}

int
ipfw_resize_tables(struct ip_fw_chain *ch, unsigned int ntables)
{
	struct radix_node_head **tables, **xtables, *rnh;
	struct radix_node_head **tables_old, **xtables_old;
	unsigned int ntables_old, tbl;
	struct namedobj_instance *ni;
	void *new_idx;
	int new_blocks;

	/* Check new value for validity */
	if (ntables > IPFW_TABLES_MAX)
		ntables = IPFW_TABLES_MAX;

	/* Allocate new pointers */
	tables = malloc(ntables * sizeof(void *), M_IPFW, M_WAITOK | M_ZERO);
	xtables = malloc(ntables * sizeof(void *), M_IPFW, M_WAITOK | M_ZERO);
	ipfw_objhash_bitmap_alloc(ntables, (void *)&new_idx, &new_blocks);

	IPFW_WLOCK(ch);

	tbl = (ntables >= V_fw_tables_max) ? V_fw_tables_max : ntables;
	ni = CHAIN_TO_NI(ch);

	/* Temportary restrict decreasing max_tables  */
	if (ipfw_objhash_bitmap_merge(ni, &new_idx, &new_blocks) != 0) {
		IPFW_WUNLOCK(ch);
		free(tables, M_IPFW);
		free(xtables, M_IPFW);
		ipfw_objhash_bitmap_free(new_idx, new_blocks);
		return (EINVAL);
	}

	/* Copy old table pointers */
	memcpy(tables, ch->tables, sizeof(void *) * tbl);
	memcpy(xtables, ch->xtables, sizeof(void *) * tbl);

	/* Change pointers and number of tables */
	tables_old = ch->tables;
	xtables_old = ch->xtables;
	ch->tables = tables;
	ch->xtables = xtables;

	ntables_old = V_fw_tables_max;
	V_fw_tables_max = ntables;

	IPFW_WUNLOCK(ch);

	/* Check if we need to destroy radix trees */
	if (ntables < ntables_old) {
		for (tbl = ntables; tbl < ntables_old; tbl++) {
			if ((rnh = tables_old[tbl]) != NULL) {
				rnh->rnh_walktree(rnh, flush_table_entry, rnh);
				rn_detachhead((void **)&rnh);
			}

			if ((rnh = xtables_old[tbl]) != NULL) {
				rnh->rnh_walktree(rnh, flush_table_entry, rnh);
				rn_detachhead((void **)&rnh);
			}
		}
	}

	/* Free old pointers */
	free(tables_old, M_IPFW);
	free(xtables_old, M_IPFW);
	ipfw_objhash_bitmap_free(new_idx, new_blocks);

	return (0);
}

int
ipfw_lookup_table(struct ip_fw_chain *ch, uint16_t tbl, in_addr_t addr,
    uint32_t *val)
{
	struct radix_node_head *rnh;
	struct table_entry *ent;
	struct sockaddr_in sa;

	if (tbl >= V_fw_tables_max)
		return (0);
	if ((rnh = ch->tables[tbl]) == NULL)
		return (0);
	KEY_LEN(sa) = KEY_LEN_INET;
	sa.sin_addr.s_addr = addr;
	ent = (struct table_entry *)(rnh->rnh_matchaddr(&sa, rnh));
	if (ent != NULL) {
		*val = ent->value;
		return (1);
	}
	return (0);
}

int
ipfw_lookup_table_extended(struct ip_fw_chain *ch, uint16_t tbl, void *paddr,
    uint32_t *val, int type)
{
	struct radix_node_head *rnh;
	struct table_xentry *xent;
	struct sockaddr_in6 sa6;
	struct xaddr_iface iface;

	if (tbl >= V_fw_tables_max)
		return (0);
	if ((rnh = ch->xtables[tbl]) == NULL)
		return (0);

	switch (type) {
	case IPFW_TABLE_CIDR:
		KEY_LEN(sa6) = KEY_LEN_INET6;
		memcpy(&sa6.sin6_addr, paddr, sizeof(struct in6_addr));
		xent = (struct table_xentry *)(rnh->rnh_matchaddr(&sa6, rnh));
		break;

	case IPFW_TABLE_INTERFACE:
		KEY_LEN(iface) = KEY_LEN_IFACE +
		    strlcpy(iface.ifname, (char *)paddr, IF_NAMESIZE) + 1;
		/* Assume direct match */
		/* FIXME: Add interface pattern matching */
		xent = (struct table_xentry *)(rnh->rnh_matchaddr(&iface, rnh));
		break;

	default:
		return (0);
	}

	if (xent != NULL) {
		*val = xent->value;
		return (1);
	}
	return (0);
}

static int
count_table_entry(struct radix_node *rn, void *arg)
{
	u_int32_t * const cnt = arg;

	(*cnt)++;
	return (0);
}

int
ipfw_count_table(struct ip_fw_chain *ch, struct tid_info *ti, uint32_t *cnt)
{
	struct radix_node_head *rnh;
	struct table_config *tc;

	if (ti->uidx >= V_fw_tables_max)
		return (EINVAL);
	if ((tc = find_table(CHAIN_TO_NI(ch), ti)) == NULL)
		return (ESRCH);
	*cnt = 0;
	if ((rnh = ch->tables[tc->no.kidx]) == NULL)
		return (0);
	rnh->rnh_walktree(rnh, count_table_entry, cnt);
	return (0);
}

static int
dump_table_entry(struct radix_node *rn, void *arg)
{
	struct table_entry * const n = (struct table_entry *)rn;
	ipfw_table * const tbl = arg;
	ipfw_table_entry *ent;

	if (tbl->cnt == tbl->size)
		return (1);
	ent = &tbl->ent[tbl->cnt];
	ent->tbl = tbl->tbl;
	if (in_nullhost(n->mask.sin_addr))
		ent->masklen = 0;
	else
		ent->masklen = 33 - ffs(ntohl(n->mask.sin_addr.s_addr));
	ent->addr = n->addr.sin_addr.s_addr;
	ent->value = n->value;
	tbl->cnt++;
	return (0);
}

int
ipfw_dump_table(struct ip_fw_chain *ch, struct tid_info *ti, ipfw_table *tbl)
{
	struct radix_node_head *rnh;
	struct table_config *tc;

	if (ti->uidx >= V_fw_tables_max)
		return (EINVAL);
	if ((tc = find_table(CHAIN_TO_NI(ch), ti)) == NULL)
		return (ESRCH);
	tbl->cnt = 0;
	if ((rnh = ch->tables[tc->no.kidx]) == NULL)
		return (0);
	rnh->rnh_walktree(rnh, dump_table_entry, tbl);
	return (0);
}

static int
count_table_xentry(struct radix_node *rn, void *arg)
{
	uint32_t * const cnt = arg;

	(*cnt) += sizeof(ipfw_table_xentry);
	return (0);
}

int
ipfw_count_xtable(struct ip_fw_chain *ch, struct tid_info *ti, uint32_t *cnt)
{
	struct radix_node_head *rnh;
	struct table_config *tc;

	if (ti->uidx >= V_fw_tables_max)
		return (EINVAL);
	*cnt = 0;
	if ((tc = find_table(CHAIN_TO_NI(ch), ti)) == NULL)
		return (0);	/* XXX: We should return ESRCH */
	if ((rnh = ch->tables[tc->no.kidx]) != NULL)
		rnh->rnh_walktree(rnh, count_table_xentry, cnt);
	if ((rnh = ch->xtables[tc->no.kidx]) != NULL)
		rnh->rnh_walktree(rnh, count_table_xentry, cnt);
	/* Return zero if table is empty */
	if (*cnt > 0)
		(*cnt) += sizeof(ipfw_xtable);
	return (0);
}


static int
dump_table_xentry_base(struct radix_node *rn, void *arg)
{
	struct table_entry * const n = (struct table_entry *)rn;
	ipfw_xtable * const tbl = arg;
	ipfw_table_xentry *xent;

	/* Out of memory, returning */
	if (tbl->cnt == tbl->size)
		return (1);
	xent = &tbl->xent[tbl->cnt];
	xent->len = sizeof(ipfw_table_xentry);
	xent->tbl = tbl->tbl;
	if (in_nullhost(n->mask.sin_addr))
		xent->masklen = 0;
	else
		xent->masklen = 33 - ffs(ntohl(n->mask.sin_addr.s_addr));
	/* Save IPv4 address as deprecated IPv6 compatible */
	xent->k.addr6.s6_addr32[3] = n->addr.sin_addr.s_addr;
	xent->flags = IPFW_TCF_INET;
	xent->value = n->value;
	tbl->cnt++;
	return (0);
}

static int
dump_table_xentry_extended(struct radix_node *rn, void *arg)
{
	struct table_xentry * const n = (struct table_xentry *)rn;
	ipfw_xtable * const tbl = arg;
	ipfw_table_xentry *xent;
#ifdef INET6
	int i;
	uint32_t *v;
#endif
	/* Out of memory, returning */
	if (tbl->cnt == tbl->size)
		return (1);
	xent = &tbl->xent[tbl->cnt];
	xent->len = sizeof(ipfw_table_xentry);
	xent->tbl = tbl->tbl;

	switch (tbl->type) {
#ifdef INET6
	case IPFW_TABLE_CIDR:
		/* Count IPv6 mask */
		v = (uint32_t *)&n->m.mask6.sin6_addr;
		for (i = 0; i < sizeof(struct in6_addr) / 4; i++, v++)
			xent->masklen += bitcount32(*v);
		memcpy(&xent->k, &n->a.addr6.sin6_addr, sizeof(struct in6_addr));
		break;
#endif
	case IPFW_TABLE_INTERFACE:
		/* Assume exact mask */
		xent->masklen = 8 * IF_NAMESIZE;
		memcpy(&xent->k, &n->a.iface.ifname, IF_NAMESIZE);
		break;
	
	default:
		/* unknown, skip entry */
		return (0);
	}

	xent->value = n->value;
	tbl->cnt++;
	return (0);
}

int
ipfw_dump_xtable(struct ip_fw_chain *ch, struct tid_info *ti, ipfw_xtable *tbl)
{
	struct radix_node_head *rnh;
	struct table_config *tc;

	if (tbl->tbl >= V_fw_tables_max)
		return (EINVAL);
	tbl->cnt = 0;

	if ((tc = find_table(CHAIN_TO_NI(ch), ti)) == NULL)
		return (0);	/* XXX: We should return ESRCH */
	tbl->type = tc->no.type;
	if ((rnh = ch->tables[tc->no.kidx]) != NULL)
		rnh->rnh_walktree(rnh, dump_table_xentry_base, tbl);
	if ((rnh = ch->xtables[tc->no.kidx]) != NULL)
		rnh->rnh_walktree(rnh, dump_table_xentry_extended, tbl);
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
	ipfw_xtable_ntlv *ntlv;
	uintptr_t pa, pe;
	int l;

	pa = (uintptr_t)tlvs;
	pe = pa + len;
	l = 0;
	for (; pa < pe; pa += l) {
		ntlv = (ipfw_xtable_ntlv *)pa;
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

static int
alloc_table_state(void **state, void **xstate, uint8_t type)
{

	switch (type) {
	case IPFW_TABLE_CIDR:
		if (!rn_inithead(state, OFF_LEN_INET))
			return (ENOMEM);
		if (!rn_inithead(xstate, OFF_LEN_INET6)) {
			rn_detachhead(state);
			return (ENOMEM);
		}
		break;
	case IPFW_TABLE_INTERFACE:
		*state = NULL;
		if (!rn_inithead(xstate, OFF_LEN_IFACE))
			return (ENOMEM);
		break;
	}
	
	return (0);
}


static struct table_config *
alloc_table_config(struct namedobj_instance *ni, struct tid_info *ti)
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
	strlcpy(tc->tablename, name, sizeof(tc->tablename));

	if (ti->tlvs == NULL) {
		tc->no.compat = 1;
		tc->no.uidx = ti->uidx;
	}

	/* Preallocate data structures for new tables */
	error = alloc_table_state(&tc->state, &tc->xstate, ti->type);
	if (error != 0) {
		free(tc, M_IPFW);
		return (NULL);
	}
	
	return (tc);
}

static void
free_table_state(void **state, void **xstate, uint8_t type)
{
	struct radix_node_head *rnh;

	switch (type) {
	case IPFW_TABLE_CIDR:
		rnh = (struct radix_node_head *)(*state);
		rnh->rnh_walktree(rnh, flush_table_entry, rnh);
		rn_detachhead(state);

		rnh = (struct radix_node_head *)(*xstate);
		rnh->rnh_walktree(rnh, flush_table_entry, rnh);
		rn_detachhead(xstate);
		break;
	case IPFW_TABLE_INTERFACE:
		rnh = (struct radix_node_head *)(*xstate);
		rnh->rnh_walktree(rnh, flush_table_entry, rnh);
		rn_detachhead(xstate);
		break;
	}
}

static void
free_table_config(struct namedobj_instance *ni, struct table_config *tc)
{

	if (tc->linked == 0)
		free_table_state(&tc->state, &tc->xstate, tc->no.type);

	free(tc, M_IPFW);
}

/*
 * Links @tc to @chain table named instance.
 * Sets appropriate type/states in @chain table info.
 */
static void
link_table(struct ip_fw_chain *chain, struct table_config *tc)
{
	struct namedobj_instance *ni;
	uint16_t kidx;

	IPFW_UH_WLOCK_ASSERT(chain);
	IPFW_WLOCK_ASSERT(chain);

	ni = CHAIN_TO_NI(chain);
	kidx = tc->no.kidx;

	ipfw_objhash_add(ni, &tc->no);
	chain->tables[kidx] = tc->state;
	chain->xtables[kidx] = tc->xstate;

	tc->linked = 1;
}

/*
 * Unlinks @tc from @chain table named instance.
 * Zeroes states in @chain and stores them in @tc.
 */
static void
unlink_table(struct ip_fw_chain *chain, struct table_config *tc)
{
	struct namedobj_instance *ni;
	uint16_t kidx;

	IPFW_UH_WLOCK_ASSERT(chain);
	IPFW_WLOCK_ASSERT(chain);

	ni = CHAIN_TO_NI(chain);
	kidx = tc->no.kidx;

	/* Clear state and save pointers for flush */
	ipfw_objhash_del(ni, &tc->no);
	tc->state = chain->tables[kidx];
	chain->tables[kidx] = NULL;
	tc->xstate = chain->xtables[kidx];
	chain->xtables[kidx] = NULL;

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

	tc = find_table(ni, ti);

	pidx->uidx = ti->uidx;
	pidx->type = ti->type;

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
		if ((error = bind_table(ni, ci, pidx, &ti)) != 0)
			break;

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

	if (ci->new_tables > 0) {
		/* Prepare queue to store configs */
		TAILQ_INIT(&nh);

		for (p = oib; p < pidx; p++) {
			if (p->new == 0)
				continue;

			/* TODO: get name from TLV */
			ti.uidx = p->uidx;
			ti.type = p->type;

			tc = alloc_table_config(ni, &ti);

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

		/*
		 * Stage 3: link & reference new table configs
		 */

		IPFW_UH_WLOCK(chain);

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
		 * Finally, attach tables and rewrite rule.
		 * We need to set table type for each new table,
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
	}

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
