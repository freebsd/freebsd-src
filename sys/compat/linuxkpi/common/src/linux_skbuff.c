/*-
 * Copyright (c) 2020-2025 The FreeBSD Foundation
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
 */

/*
 * NOTE: this socket buffer compatibility code is highly EXPERIMENTAL.
 *       Do not rely on the internals of this implementation.  They are highly
 *       likely to change as we will improve the integration to FreeBSD mbufs.
 */

#include <sys/cdefs.h>
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>

#include <vm/uma.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#ifdef __LP64__
#include <linux/log2.h>
#endif

SYSCTL_DECL(_compat_linuxkpi);
SYSCTL_NODE(_compat_linuxkpi, OID_AUTO, skb, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "LinuxKPI skbuff");

#ifdef SKB_DEBUG
int linuxkpi_debug_skb;
SYSCTL_INT(_compat_linuxkpi_skb, OID_AUTO, debug, CTLFLAG_RWTUN,
    &linuxkpi_debug_skb, 0, "SKB debug level");
#endif

static uma_zone_t skbzone;

#define	SKB_DMA32_MALLOC
#ifdef	SKB_DMA32_MALLOC
/*
 * Realtek wireless drivers (e.g., rtw88) require 32bit DMA in a single segment.
 * busdma(9) has a hard time providing this currently for 3-ish pages at large
 * quantities (see lkpi_pci_nseg1_fail in linux_pci.c).
 * Work around this for now by allowing a tunable to enforce physical addresses
 * allocation limits using "old-school" contigmalloc(9) to avoid bouncing.
 * Note: with the malloc/contigmalloc + kmalloc changes also providing physical
 * contiguous memory, and the nseg=1 limit for bouncing we should in theory be
 * fine now and not need any of this anymore, however busdma still has troubles
 * boncing three contiguous pages so for now this stays.
 */
static int linuxkpi_skb_memlimit;
SYSCTL_INT(_compat_linuxkpi_skb, OID_AUTO, mem_limit, CTLFLAG_RDTUN,
    &linuxkpi_skb_memlimit, 0, "SKB memory limit: 0=no limit, "
    "1=32bit, 2=36bit, other=undef (currently 32bit)");

static MALLOC_DEFINE(M_LKPISKB, "lkpiskb", "Linux KPI skbuff compat");
#endif

struct sk_buff *
linuxkpi_alloc_skb(size_t size, gfp_t gfp)
{
	struct sk_buff *skb;
	void *p;
	size_t len;

	skb = uma_zalloc(skbzone, linux_check_m_flags(gfp) | M_ZERO);
	if (skb == NULL)
		return (NULL);

	skb->prev = skb->next = skb;
	skb->truesize = size;
	skb->shinfo = (struct skb_shared_info *)(skb + 1);

	if (size == 0)
		return (skb);

	len = size;
#ifdef	SKB_DMA32_MALLOC
	/*
	 * Using our own type here not backing my kmalloc.
	 * We assume no one calls kfree directly on the skb.
	 */
	if (__predict_false(linuxkpi_skb_memlimit != 0)) {
		vm_paddr_t high;

		switch (linuxkpi_skb_memlimit) {
#ifdef __LP64__
		case 2:
			high = (0xfffffffff);	/* 1<<36 really. */
			break;
#endif
		case 1:
		default:
			high = (0xffffffff);	/* 1<<32 really. */
			break;
		}
		len = roundup_pow_of_two(len);
		p = contigmalloc(len, M_LKPISKB,
		    linux_check_m_flags(gfp) | M_ZERO, 0, high, PAGE_SIZE, 0);
	} else
#endif
	p = __kmalloc(len, linux_check_m_flags(gfp) | M_ZERO);
	if (p == NULL) {
		uma_zfree(skbzone, skb);
		return (NULL);
	}

	skb->head = skb->data = (uint8_t *)p;
	skb_reset_tail_pointer(skb);
	skb->end = skb->head + size;

	SKB_TRACE_FMT(skb, "data %p size %zu", (skb) ? skb->data : NULL, size);
	return (skb);
}

struct sk_buff *
linuxkpi_dev_alloc_skb(size_t size, gfp_t gfp)
{
	struct sk_buff *skb;
	size_t len;

	len = size + NET_SKB_PAD;
	skb = linuxkpi_alloc_skb(len, gfp);

	if (skb != NULL)
		skb_reserve(skb, NET_SKB_PAD);

	SKB_TRACE_FMT(skb, "data %p size %zu len %zu",
	    (skb) ? skb->data : NULL, size, len);
	return (skb);
}

struct sk_buff *
linuxkpi_build_skb(void *data, size_t fragsz)
{
	struct sk_buff *skb;

	if (data == NULL || fragsz == 0)
		return (NULL);

	/* Just allocate a skb without data area. */
	skb = linuxkpi_alloc_skb(0, GFP_KERNEL);
	if (skb == NULL)
		return (NULL);

	skb->_flags |= _SKB_FLAGS_SKBEXTFRAG;
	skb->truesize = fragsz;
	skb->head = skb->data = data;
	skb_reset_tail_pointer(skb);
	skb->end = skb->head + fragsz;

	return (skb);
}

struct sk_buff *
linuxkpi_skb_copy(const struct sk_buff *skb, gfp_t gfp)
{
	struct sk_buff *new;
	struct skb_shared_info *shinfo;
	size_t len;
	unsigned int headroom;

	/* Full buffer size + any fragments. */
	len = skb->end - skb->head + skb->data_len;

	new = linuxkpi_alloc_skb(len, gfp);
	if (new == NULL)
		return (NULL);

	headroom = skb_headroom(skb);
	/* Fixup head and end. */
	skb_reserve(new, headroom);	/* data and tail move headroom forward. */
	skb_put(new, skb->len);		/* tail and len get adjusted */

	/* Copy data. */
	memcpy(new->head, skb->data - headroom, headroom + skb->len);

	/* Deal with fragments. */
	shinfo = skb->shinfo;
	if (shinfo->nr_frags > 0) {
		printf("%s:%d: NOT YET SUPPORTED; missing %d frags\n",
		    __func__, __LINE__, shinfo->nr_frags);
		SKB_TODO();
	}

	/* Deal with header fields. */
	memcpy(new->cb, skb->cb, sizeof(skb->cb));
	SKB_IMPROVE("more header fields to copy?");

	return (new);
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

	if ((skb->_flags & _SKB_FLAGS_SKBEXTFRAG) != 0) {
		void *p;

		p = skb->head;
		skb_free_frag(p);
		skb->head = NULL;
	}

#ifdef	SKB_DMA32_MALLOC
	if (__predict_false(linuxkpi_skb_memlimit != 0))
		free(skb->head, M_LKPISKB);
	else
#endif
	kfree(skb->head);
	uma_zfree(skbzone, skb);
}

static void
lkpi_skbuff_init(void *arg __unused)
{
	skbzone = uma_zcreate("skbuff",
	    sizeof(struct sk_buff) + sizeof(struct skb_shared_info),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	/* Do we need to apply limits? */
}
SYSINIT(linuxkpi_skbuff, SI_SUB_DRIVERS, SI_ORDER_FIRST, lkpi_skbuff_init, NULL);

static void
lkpi_skbuff_destroy(void *arg __unused)
{
	uma_zdestroy(skbzone);
}
SYSUNINIT(linuxkpi_skbuff, SI_SUB_DRIVERS, SI_ORDER_SECOND, lkpi_skbuff_destroy, NULL);

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
	db_printf("\tlist %p\n", &skb->list);
	db_printf("\tlen %u data_len %u truesize %u mac_len %u\n",
	    skb->len, skb->data_len, skb->truesize, skb->mac_len);
	db_printf("\tcsum %#06x l3hdroff %u l4hdroff %u priority %u qmap %u\n",
	    skb->csum, skb->l3hdroff, skb->l4hdroff, skb->priority, skb->qmap);
	db_printf("\tpkt_type %d dev %p sk %p\n",
	    skb->pkt_type, skb->dev, skb->sk);
	db_printf("\tcsum_offset %d csum_start %d ip_summed %d protocol %d\n",
	    skb->csum_offset, skb->csum_start, skb->ip_summed, skb->protocol);
	db_printf("\t_flags %#06x\n", skb->_flags);		/* XXX-BZ print names? */
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

	db_printf("\t__scratch[0] %p\n", skb->__scratch);
};
#endif
