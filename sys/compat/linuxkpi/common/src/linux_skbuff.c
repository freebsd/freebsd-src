/*-
 * Copyright (c) 2020-2021 The FreeBSD Foundation
 * Copyright (c) 2021 Bjoern A. Zeeb
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <linux/skbuff.h>
#include <linux/slab.h>

static MALLOC_DEFINE(M_LKPISKB, "lkpiskb", "Linux KPI skbuff compat");

struct sk_buff *
linuxkpi_alloc_skb(size_t size, gfp_t gfp)
{
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*skb) + size + sizeof(struct skb_shared_info);
	/*
	 * Using or own type here not backing my kmalloc.
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
	uint16_t fragno;

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
	for (fragno = 0; fragno < nitems(shinfo->frags); fragno++) {

		if (shinfo->frags[fragno].page != NULL)
			__free_page(shinfo->frags[fragno].page);
	}

	free(skb, M_LKPISKB);
}
