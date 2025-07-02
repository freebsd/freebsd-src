/*-
 * Copyright (c) 2018 Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
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

#ifndef	_LINUXKPI_LINUX_SWAP_H_
#define	_LINUXKPI_LINUX_SWAP_H_

#include <sys/param.h>
#include <sys/domainset.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/pcpu.h>

#include <vm/vm.h>
#include <vm/swap_pager.h>
#include <vm/vm_pageout.h>

#include <linux/pagemap.h>
#include <linux/page-flags.h>

static inline long
get_nr_swap_pages(void)
{
	int i, j;

	/* NB: This could be done cheaply by obtaining swap_total directly */
	swap_pager_status(&i, &j);
	return i - j;
}

static inline int
current_is_kswapd(void)
{

	return (curproc == pageproc);
}

static inline void
folio_mark_accessed(struct folio *folio)
{
	mark_page_accessed(&folio->page);
}

static inline void
check_move_unevictable_folios(struct folio_batch *fbatch)
{
}

#endif
