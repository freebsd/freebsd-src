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
ipfw_add_table_entry(struct ip_fw_chain *ch, uint16_t tbl, void *paddr,
    uint8_t plen, uint8_t mlen, uint8_t type, uint32_t value)
{
	struct radix_node_head *rnh, **rnh_ptr;
	struct table_entry *ent;
	struct table_xentry *xent;
	struct radix_node *rn;
	in_addr_t addr;
	int offset;
	void *ent_ptr;
	struct sockaddr *addr_ptr, *mask_ptr;
	char c;

	if (tbl >= V_fw_tables_max)
		return (EINVAL);

	switch (type) {
	case IPFW_TABLE_CIDR:
		if (plen == sizeof(in_addr_t)) {
#ifdef INET
			/* IPv4 case */
			if (mlen > 32)
				return (EINVAL);
			ent = malloc(sizeof(*ent), M_IPFW_TBL, M_WAITOK | M_ZERO);
			ent->value = value;
			/* Set 'total' structure length */
			KEY_LEN(ent->addr) = KEY_LEN_INET;
			KEY_LEN(ent->mask) = KEY_LEN_INET;
			/* Set offset of IPv4 address in bits */
			offset = OFF_LEN_INET;
			ent->mask.sin_addr.s_addr = htonl(mlen ? ~((1 << (32 - mlen)) - 1) : 0);
			addr = *((in_addr_t *)paddr);
			ent->addr.sin_addr.s_addr = addr & ent->mask.sin_addr.s_addr;
			/* Set pointers */
			rnh_ptr = &ch->tables[tbl];
			ent_ptr = ent;
			addr_ptr = (struct sockaddr *)&ent->addr;
			mask_ptr = (struct sockaddr *)&ent->mask;
#endif
#ifdef INET6
		} else if (plen == sizeof(struct in6_addr)) {
			/* IPv6 case */
			if (mlen > 128)
				return (EINVAL);
			xent = malloc(sizeof(*xent), M_IPFW_TBL, M_WAITOK | M_ZERO);
			xent->value = value;
			/* Set 'total' structure length */
			KEY_LEN(xent->a.addr6) = KEY_LEN_INET6;
			KEY_LEN(xent->m.mask6) = KEY_LEN_INET6;
			/* Set offset of IPv6 address in bits */
			offset = OFF_LEN_INET6;
			ipv6_writemask(&xent->m.mask6.sin6_addr, mlen);
			memcpy(&xent->a.addr6.sin6_addr, paddr, sizeof(struct in6_addr));
			APPLY_MASK(&xent->a.addr6.sin6_addr, &xent->m.mask6.sin6_addr);
			/* Set pointers */
			rnh_ptr = &ch->xtables[tbl];
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
		c = ((char *)paddr)[IF_NAMESIZE - 1];
		((char *)paddr)[IF_NAMESIZE - 1] = '\0';
		if (((mlen = strlen((char *)paddr)) == IF_NAMESIZE - 1) && (c != '\0'))
			return (EINVAL);

		/* Include last \0 into comparison */
		mlen++;

		xent = malloc(sizeof(*xent), M_IPFW_TBL, M_WAITOK | M_ZERO);
		xent->value = value;
		/* Set 'total' structure length */
		KEY_LEN(xent->a.iface) = KEY_LEN_IFACE + mlen;
		KEY_LEN(xent->m.ifmask) = KEY_LEN_IFACE + mlen;
		/* Set offset of interface name in bits */
		offset = OFF_LEN_IFACE;
		memcpy(xent->a.iface.ifname, paddr, mlen);
		/* Assume direct match */
		/* TODO: Add interface pattern matching */
#if 0
		memset(xent->m.ifmask.ifname, 0xFF, IF_NAMESIZE);
		mask_ptr = (struct sockaddr *)&xent->m.ifmask;
#endif
		/* Set pointers */
		rnh_ptr = &ch->xtables[tbl];
		ent_ptr = xent;
		addr_ptr = (struct sockaddr *)&xent->a.iface;
		mask_ptr = NULL;
		break;

	default:
		return (EINVAL);
	}

	IPFW_WLOCK(ch);

	/* Check if tabletype is valid */
	if ((ch->tabletype[tbl] != 0) && (ch->tabletype[tbl] != type)) {
		IPFW_WUNLOCK(ch);
		free(ent_ptr, M_IPFW_TBL);
		return (EINVAL);
	}

	/* Check if radix tree exists */
	if ((rnh = *rnh_ptr) == NULL) {
		IPFW_WUNLOCK(ch);
		/* Create radix for a new table */
		if (!rn_inithead((void **)&rnh, offset)) {
			free(ent_ptr, M_IPFW_TBL);
			return (ENOMEM);
		}

		IPFW_WLOCK(ch);
		if (*rnh_ptr != NULL) {
			/* Tree is already attached by other thread */
			rn_detachhead((void **)&rnh);
			rnh = *rnh_ptr;
			/* Check table type another time */
			if (ch->tabletype[tbl] != type) {
				IPFW_WUNLOCK(ch);
				free(ent_ptr, M_IPFW_TBL);
				return (EINVAL);
			}
		} else {
			*rnh_ptr = rnh;
			/* 
			 * Set table type. It can be set already
			 * (if we have IPv6-only table) but setting
			 * it another time does not hurt
			 */
			ch->tabletype[tbl] = type;
		}
	}

	rn = rnh->rnh_addaddr(addr_ptr, mask_ptr, rnh, ent_ptr);
	IPFW_WUNLOCK(ch);

	if (rn == NULL) {
		free(ent_ptr, M_IPFW_TBL);
		return (EEXIST);
	}
	return (0);
}

int
ipfw_del_table_entry(struct ip_fw_chain *ch, uint16_t tbl, void *paddr,
    uint8_t plen, uint8_t mlen, uint8_t type)
{
	struct radix_node_head *rnh, **rnh_ptr;
	struct table_entry *ent;
	in_addr_t addr;
	struct sockaddr_in sa, mask;
	struct sockaddr *sa_ptr, *mask_ptr;
	char c;

	if (tbl >= V_fw_tables_max)
		return (EINVAL);

	switch (type) {
	case IPFW_TABLE_CIDR:
		if (plen == sizeof(in_addr_t)) {
			/* Set 'total' structure length */
			KEY_LEN(sa) = KEY_LEN_INET;
			KEY_LEN(mask) = KEY_LEN_INET;
			mask.sin_addr.s_addr = htonl(mlen ? ~((1 << (32 - mlen)) - 1) : 0);
			addr = *((in_addr_t *)paddr);
			sa.sin_addr.s_addr = addr & mask.sin_addr.s_addr;
			rnh_ptr = &ch->tables[tbl];
			sa_ptr = (struct sockaddr *)&sa;
			mask_ptr = (struct sockaddr *)&mask;
#ifdef INET6
		} else if (plen == sizeof(struct in6_addr)) {
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
			memcpy(&sa6.sin6_addr, paddr, sizeof(struct in6_addr));
			APPLY_MASK(&sa6.sin6_addr, &mask6.sin6_addr);
			rnh_ptr = &ch->xtables[tbl];
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
		c = ((char *)paddr)[IF_NAMESIZE - 1];
		((char *)paddr)[IF_NAMESIZE - 1] = '\0';
		if (((mlen = strlen((char *)paddr)) == IF_NAMESIZE - 1) && (c != '\0'))
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
		memcpy(ifname.ifname, paddr, mlen);
		/* Set pointers */
		rnh_ptr = &ch->xtables[tbl];
		sa_ptr = (struct sockaddr *)&ifname;

		break;

	default:
		return (EINVAL);
	}

	IPFW_WLOCK(ch);
	if ((rnh = *rnh_ptr) == NULL) {
		IPFW_WUNLOCK(ch);
		return (ESRCH);
	}

	if (ch->tabletype[tbl] != type) {
		IPFW_WUNLOCK(ch);
		return (EINVAL);
	}

	ent = (struct table_entry *)rnh->rnh_deladdr(sa_ptr, mask_ptr, rnh);
	IPFW_WUNLOCK(ch);

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

int
ipfw_flush_table(struct ip_fw_chain *ch, uint16_t tbl)
{
	struct radix_node_head *rnh, *xrnh;

	if (tbl >= V_fw_tables_max)
		return (EINVAL);

	/*
	 * We free both (IPv4 and extended) radix trees and
	 * clear table type here to permit table to be reused
	 * for different type without module reload
	 */

	IPFW_WLOCK(ch);
	/* Set IPv4 table pointer to zero */
	if ((rnh = ch->tables[tbl]) != NULL)
		ch->tables[tbl] = NULL;
	/* Set extended table pointer to zero */
	if ((xrnh = ch->xtables[tbl]) != NULL)
		ch->xtables[tbl] = NULL;
	/* Zero table type */
	ch->tabletype[tbl] = 0;
	IPFW_WUNLOCK(ch);

	if (rnh != NULL) {
		rnh->rnh_walktree(rnh, flush_table_entry, rnh);
		rn_detachhead((void **)&rnh);
	}

	if (xrnh != NULL) {
		xrnh->rnh_walktree(xrnh, flush_table_entry, xrnh);
		rn_detachhead((void **)&xrnh);
	}

	return (0);
}

void
ipfw_destroy_tables(struct ip_fw_chain *ch)
{
	uint16_t tbl;

	/* Flush all tables */
	for (tbl = 0; tbl < V_fw_tables_max; tbl++)
		ipfw_flush_table(ch, tbl);

	/* Free pointers itself */
	free(ch->tables, M_IPFW);
	free(ch->xtables, M_IPFW);
	free(ch->tabletype, M_IPFW);
}

int
ipfw_init_tables(struct ip_fw_chain *ch)
{
	/* Allocate pointers */
	ch->tables = malloc(V_fw_tables_max * sizeof(void *), M_IPFW, M_WAITOK | M_ZERO);
	ch->xtables = malloc(V_fw_tables_max * sizeof(void *), M_IPFW, M_WAITOK | M_ZERO);
	ch->tabletype = malloc(V_fw_tables_max * sizeof(uint8_t), M_IPFW, M_WAITOK | M_ZERO);
	return (0);
}

int
ipfw_resize_tables(struct ip_fw_chain *ch, unsigned int ntables)
{
	struct radix_node_head **tables, **xtables, *rnh;
	struct radix_node_head **tables_old, **xtables_old;
	uint8_t *tabletype, *tabletype_old;
	unsigned int ntables_old, tbl;

	/* Check new value for validity */
	if (ntables > IPFW_TABLES_MAX)
		ntables = IPFW_TABLES_MAX;

	/* Allocate new pointers */
	tables = malloc(ntables * sizeof(void *), M_IPFW, M_WAITOK | M_ZERO);
	xtables = malloc(ntables * sizeof(void *), M_IPFW, M_WAITOK | M_ZERO);
	tabletype = malloc(ntables * sizeof(uint8_t), M_IPFW, M_WAITOK | M_ZERO);

	IPFW_WLOCK(ch);

	tbl = (ntables >= V_fw_tables_max) ? V_fw_tables_max : ntables;

	/* Copy old table pointers */
	memcpy(tables, ch->tables, sizeof(void *) * tbl);
	memcpy(xtables, ch->xtables, sizeof(void *) * tbl);
	memcpy(tabletype, ch->tabletype, sizeof(uint8_t) * tbl);

	/* Change pointers and number of tables */
	tables_old = ch->tables;
	xtables_old = ch->xtables;
	tabletype_old = ch->tabletype;
	ch->tables = tables;
	ch->xtables = xtables;
	ch->tabletype = tabletype;

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
	free(tabletype_old, M_IPFW);

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
ipfw_count_table(struct ip_fw_chain *ch, uint32_t tbl, uint32_t *cnt)
{
	struct radix_node_head *rnh;

	if (tbl >= V_fw_tables_max)
		return (EINVAL);
	*cnt = 0;
	if ((rnh = ch->tables[tbl]) == NULL)
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
ipfw_dump_table(struct ip_fw_chain *ch, ipfw_table *tbl)
{
	struct radix_node_head *rnh;

	if (tbl->tbl >= V_fw_tables_max)
		return (EINVAL);
	tbl->cnt = 0;
	if ((rnh = ch->tables[tbl->tbl]) == NULL)
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
ipfw_count_xtable(struct ip_fw_chain *ch, uint32_t tbl, uint32_t *cnt)
{
	struct radix_node_head *rnh;

	if (tbl >= V_fw_tables_max)
		return (EINVAL);
	*cnt = 0;
	if ((rnh = ch->tables[tbl]) != NULL)
		rnh->rnh_walktree(rnh, count_table_xentry, cnt);
	if ((rnh = ch->xtables[tbl]) != NULL)
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
ipfw_dump_xtable(struct ip_fw_chain *ch, ipfw_xtable *tbl)
{
	struct radix_node_head *rnh;

	if (tbl->tbl >= V_fw_tables_max)
		return (EINVAL);
	tbl->cnt = 0;
	tbl->type = ch->tabletype[tbl->tbl];
	if ((rnh = ch->tables[tbl->tbl]) != NULL)
		rnh->rnh_walktree(rnh, dump_table_xentry_base, tbl);
	if ((rnh = ch->xtables[tbl->tbl]) != NULL)
		rnh->rnh_walktree(rnh, dump_table_xentry_extended, tbl);
	return (0);
}

/* end of file */
