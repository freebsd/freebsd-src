/*
 * Copyright (c) 1990 University of Utah.
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)swap_pager.h	7.1 (Berkeley) 12/5/90
 * $FreeBSD$
 */

/*
 * Modifications to the block allocation data structure by John S. Dyson
 * 18 Dec 93.
 */

#ifndef	_VM_SWAP_PAGER_H_
#define	_VM_SWAP_PAGER_H_ 1

/*
 * Swap device table
 */
struct swdevt {
	udev_t	sw_dev;			/* For quasibogus swapdev reporting */
	int	sw_flags;
	int	sw_nblks;
	int     sw_used;
	struct	vnode *sw_vp;
	dev_t	sw_device;
};
#define	SW_FREED	0x01
#define	SW_SEQUENTIAL	0x02
#define	SW_CLOSING	0x04
#define	sw_freed	sw_flags	/* XXX compat */

#ifdef _KERNEL

/*
 * SWB_NPAGES must be a power of 2.  It may be set to 1, 2, 4, 8, or 16
 * pages per allocation.  We recommend you stick with the default of 8.
 * The 16-page limit is due to the radix code (kern/subr_blist.c).
 */
#ifndef MAX_PAGEOUT_CLUSTER
#define MAX_PAGEOUT_CLUSTER 16
#endif

#if !defined(SWB_NPAGES)
#define SWB_NPAGES	MAX_PAGEOUT_CLUSTER
#endif

/*
 * Piecemeal swap metadata structure.  Swap is stored in a radix tree.
 *
 * If SWB_NPAGES is 8 and sizeof(char *) == sizeof(daddr_t), our radix
 * is basically 8.  Assuming PAGE_SIZE == 4096, one tree level represents
 * 32K worth of data, two levels represent 256K, three levels represent
 * 2 MBytes.   This is acceptable.
 *
 * Overall memory utilization is about the same as the old swap structure.
 */
#define SWCORRECT(n) (sizeof(void *) * (n) / sizeof(daddr_t))
#define SWAP_META_PAGES		(SWB_NPAGES * 2)
#define SWAP_META_MASK		(SWAP_META_PAGES - 1)

extern int swap_pager_full;
extern struct blist *swapblist;
extern int vm_swap_size;

void swap_pager_putpages(vm_object_t, vm_page_t *, int, boolean_t, int *);
void swap_pager_copy(vm_object_t, vm_object_t, vm_pindex_t, int);
void swap_pager_freespace(vm_object_t, vm_pindex_t, vm_size_t);
void swap_pager_swap_init(void);
int swap_pager_isswapped(vm_object_t, int);
int swap_pager_reserve(vm_object_t, vm_pindex_t, vm_size_t);

#endif				/* _KERNEL */
#endif				/* _VM_SWAP_PAGER_H_ */
