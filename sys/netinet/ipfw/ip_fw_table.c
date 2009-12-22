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

#if !defined(KLD_MODULE)
#include "opt_ipfw.h"
#include "opt_ipdivert.h"
#include "opt_ipdn.h"
#include "opt_inet.h"
#ifndef INET
#error IPFIREWALL requires INET.
#endif /* INET */
#endif
#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <net/if.h>	/* ip_fw.h requires IFNAMSIZ */
#include <net/radix.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <netinet/ipfw/ip_fw_private.h>

#ifdef MAC
#include <security/mac/mac_framework.h>
#endif

MALLOC_DEFINE(M_IPFW_TBL, "ipfw_tbl", "IpFw tables");

struct table_entry {
	struct radix_node	rn[2];
	struct sockaddr_in	addr, mask;
	u_int32_t		value;
};

int
ipfw_add_table_entry(struct ip_fw_chain *ch, uint16_t tbl, in_addr_t addr,
    uint8_t mlen, uint32_t value)
{
	struct radix_node_head *rnh;
	struct table_entry *ent;
	struct radix_node *rn;

	if (tbl >= IPFW_TABLES_MAX)
		return (EINVAL);
	rnh = ch->tables[tbl];
	ent = malloc(sizeof(*ent), M_IPFW_TBL, M_NOWAIT | M_ZERO);
	if (ent == NULL)
		return (ENOMEM);
	ent->value = value;
	ent->addr.sin_len = ent->mask.sin_len = 8;
	ent->mask.sin_addr.s_addr = htonl(mlen ? ~((1 << (32 - mlen)) - 1) : 0);
	ent->addr.sin_addr.s_addr = addr & ent->mask.sin_addr.s_addr;
	IPFW_WLOCK(ch);
	rn = rnh->rnh_addaddr(&ent->addr, &ent->mask, rnh, (void *)ent);
	if (rn == NULL) {
		IPFW_WUNLOCK(ch);
		free(ent, M_IPFW_TBL);
		return (EEXIST);
	}
	IPFW_WUNLOCK(ch);
	return (0);
}

int
ipfw_del_table_entry(struct ip_fw_chain *ch, uint16_t tbl, in_addr_t addr,
    uint8_t mlen)
{
	struct radix_node_head *rnh;
	struct table_entry *ent;
	struct sockaddr_in sa, mask;

	if (tbl >= IPFW_TABLES_MAX)
		return (EINVAL);
	rnh = ch->tables[tbl];
	sa.sin_len = mask.sin_len = 8;
	mask.sin_addr.s_addr = htonl(mlen ? ~((1 << (32 - mlen)) - 1) : 0);
	sa.sin_addr.s_addr = addr & mask.sin_addr.s_addr;
	IPFW_WLOCK(ch);
	ent = (struct table_entry *)rnh->rnh_deladdr(&sa, &mask, rnh);
	if (ent == NULL) {
		IPFW_WUNLOCK(ch);
		return (ESRCH);
	}
	IPFW_WUNLOCK(ch);
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
	struct radix_node_head *rnh;

	IPFW_WLOCK_ASSERT(ch);

	if (tbl >= IPFW_TABLES_MAX)
		return (EINVAL);
	rnh = ch->tables[tbl];
	KASSERT(rnh != NULL, ("NULL IPFW table"));
	rnh->rnh_walktree(rnh, flush_table_entry, rnh);
	return (0);
}

void
ipfw_flush_tables(struct ip_fw_chain *ch)
{
	uint16_t tbl;

	IPFW_WLOCK_ASSERT(ch);

	for (tbl = 0; tbl < IPFW_TABLES_MAX; tbl++)
		ipfw_flush_table(ch, tbl);
}

int
ipfw_init_tables(struct ip_fw_chain *ch)
{ 
	int i;
	uint16_t j;

	for (i = 0; i < IPFW_TABLES_MAX; i++) {
		if (!rn_inithead((void **)&ch->tables[i], 32)) {
			for (j = 0; j < i; j++) {
				(void) ipfw_flush_table(ch, j);
			}
			return (ENOMEM);
		}
	}
	return (0);
}

int
ipfw_lookup_table(struct ip_fw_chain *ch, uint16_t tbl, in_addr_t addr,
    uint32_t *val)
{
	struct radix_node_head *rnh;
	struct table_entry *ent;
	struct sockaddr_in sa;

	if (tbl >= IPFW_TABLES_MAX)
		return (0);
	rnh = ch->tables[tbl];
	sa.sin_len = 8;
	sa.sin_addr.s_addr = addr;
	ent = (struct table_entry *)(rnh->rnh_lookup(&sa, NULL, rnh));
	if (ent != NULL) {
		*val = ent->value;
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

	if (tbl >= IPFW_TABLES_MAX)
		return (EINVAL);
	rnh = ch->tables[tbl];
	*cnt = 0;
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

	if (tbl->tbl >= IPFW_TABLES_MAX)
		return (EINVAL);
	rnh = ch->tables[tbl->tbl];
	tbl->cnt = 0;
	rnh->rnh_walktree(rnh, dump_table_entry, tbl);
	return (0);
}
/* end of file */
