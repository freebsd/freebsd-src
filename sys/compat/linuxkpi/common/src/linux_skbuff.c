/*-
 * Copyright (c) 2020-2022 The FreeBSD Foundation
 * Copyright (c) 2021-2022 Bjoern A. Zeeb
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
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
 * $FreeBSD$
 */

/*
 * NOTE: this socket buffer compatibility code is highly EXPERIMENTAL.
 *       Do not rely on the internals of this implementation.  They are highly
 *       likely to change as we will improve the integration to FreeBSD mbufs.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/gfp.h>

#ifdef SKB_DEBUG
SYSCTL_DECL(_compat_linuxkpi);
SYSCTL_NODE(_compat_linuxkpi, OID_AUTO, skb, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "LinuxKPI skbuff");

int linuxkpi_debug_skb;
SYSCTL_INT(_compat_linuxkpi_skb, OID_AUTO, debug, CTLFLAG_RWTUN,
    &linuxkpi_debug_skb, 0, "SKB debug level");
#endif

static MALLOC_DEFINE(M_LKPISKB, "lkpiskb", "Linux KPI skbuff compat");

struct sk_buff *
linuxkpi_alloc_skb(size_t size, gfp_t gfp)
{
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*skb) + size + sizeof(struct skb_shared_info);
	/*
	 * Using our own type here not backing my kmalloc.
	 * We assume no one calls kfree directly on the skb.
	 */
	skb = malloc(len, M_LKPISKB, linux_check_m_flags(gfp) | M_ZERO);
	if (skb == NULL)
		return (skb);
	skb->_alloc_len = size;
	skb->truesize = size;

	skb->head = skb->data = skb->tail = (uint8_t *)(skb+1);
	skb->end = skb->head + size;

	skb->shinfo = (struct skb_shared_info *)(skb->end);

	SKB_TRACE_FMT(skb, "data %p size %zu", skb->data, size);
	return (skb);
}

void
linuxkpi_kfree_skb(struct sk_buff *skb)
{
	struct skb_shared_info *shinfo;
	uint16_t fragno, count;

	SKB_TRACE(skb);
	if (skb == NULL)
		return;

	/*
	 * XXX TODO this will go away once we have skb backed by mbuf.
	 * currently we allow the mbuf to stay around and use a private
	 * free function to allow secondary resources to be freed along.
	 */
	if (skb->m != NULL) {
		void *m;

		m = skb->m;
		skb->m = NULL;

		KASSERT(skb->m_free_func != NULL, ("%s: skb %p has m %p but no "
		    "m_free_func %p\n", __func__, skb, m, skb->m_free_func));
		skb->m_free_func(m);
	}
	KASSERT(skb->m == NULL,
	    ("%s: skb %p m %p != NULL\n", __func__, skb, skb->m));

	shinfo = skb->shinfo;
	for (count = fragno = 0;
	    count < shinfo->nr_frags && fragno < nitems(shinfo->frags);
	    fragno++) {

		if (shinfo->frags[fragno].page != NULL) {
			struct page *p;

			p = shinfo->frags[fragno].page;
			shinfo->frags[fragno].size = 0;
			shinfo->frags[fragno].offset = 0;
			shinfo->frags[fragno].page = NULL;
			__free_page(p);
			count++;
		}
	}

	free(skb, M_LKPISKB);
}

#ifdef DDB
DB_SHOW_COMMAND(skb, db_show_skb)
{
	struct sk_buff *skb;
	int i;

	if (!have_addr) {
		db_printf("usage: show skb <addr>\n");
			return;
	}

	skb = (struct sk_buff *)addr;

	db_printf("skb %p\n", skb);
	db_printf("\tnext %p prev %p\n", skb->next, skb->prev);
	db_printf("\tlist %d\n", skb->list);
	db_printf("\t_alloc_len %u len %u data_len %u truesize %u mac_len %u\n",
	    skb->_alloc_len, skb->len, skb->data_len, skb->truesize,
	    skb->mac_len);
	db_printf("\tcsum %#06x l3hdroff %u l4hdroff %u priority %u qmap %u\n",
	    skb->csum, skb->l3hdroff, skb->l4hdroff, skb->priority, skb->qmap);
	db_printf("\tpkt_type %d dev %p sk %p\n",
	    skb->pkt_type, skb->dev, skb->sk);
	db_printf("\tcsum_offset %d csum_start %d ip_summed %d protocol %d\n",
	    skb->csum_offset, skb->csum_start, skb->ip_summed, skb->protocol);
	db_printf("\thead %p data %p tail %p end %p\n",
	    skb->head, skb->data, skb->tail, skb->end);
	db_printf("\tshinfo %p m %p m_free_func %p\n",
	    skb->shinfo, skb->m, skb->m_free_func);

	if (skb->shinfo != NULL) {
		struct skb_shared_info *shinfo;

		shinfo = skb->shinfo;
		db_printf("\t\tgso_type %d gso_size %u nr_frags %u\n",
		    shinfo->gso_type, shinfo->gso_size, shinfo->nr_frags);
		for (i = 0; i < nitems(shinfo->frags); i++) {
			struct skb_frag *frag;

			frag = &shinfo->frags[i];
			if (frag == NULL || frag->page == NULL)
				continue;
			db_printf("\t\t\tfrag %p fragno %d page %p %p "
			    "offset %ju size %zu\n",
			    frag, i, frag->page, linux_page_address(frag->page),
			    (uintmax_t)frag->offset, frag->size);
		}
	}
	db_printf("\tcb[] %p {", skb->cb);
	for (i = 0; i < nitems(skb->cb); i++) {
		db_printf("%#04x%s",
		    skb->cb[i], (i < (nitems(skb->cb)-1)) ? ", " : "");
	}
	db_printf("}\n");

	db_printf("\t_spareu16_0 %#06x __scratch[0] %p\n",
	    skb->_spareu16_0, skb->__scratch);
};
#endif
