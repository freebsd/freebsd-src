/*-
 * Copyright (c) 1998 Matthew Dillon,
 * Copyright (c) 1994 John S. Dyson
 * Copyright (c) 1990 University of Utah.
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *				New Swap System
 *				Matthew Dillon
 *
 * Radix Bitmap 'blists'.
 *
 *	- The new swapper uses the new radix bitmap code.  This should scale
 *	  to arbitrarily small or arbitrarily large swap spaces and an almost
 *	  arbitrary degree of fragmentation.
 *
 * Features:
 *
 *	- on the fly reallocation of swap during putpages.  The new system
 *	  does not try to keep previously allocated swap blocks for dirty
 *	  pages.  
 *
 *	- on the fly deallocation of swap
 *
 *	- No more garbage collection required.  Unnecessarily allocated swap
 *	  blocks only exist for dirty vm_page_t's now and these are already
 *	  cycled (in a high-load system) by the pager.  We also do on-the-fly
 *	  removal of invalidated swap blocks when a page is destroyed
 *	  or renamed.
 *
 * from: Utah $Hdr: swap_pager.c 1.4 91/04/30$
 *
 *	@(#)swap_pager.c	8.9 (Berkeley) 3/21/94
 *	@(#)vm_swap.c	8.5 (Berkeley) 2/17/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_swap.h"
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/blist.h>
#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/vmmeter.h>

#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_pageout.h>
#include <vm/vm_param.h>
#include <vm/swap_pager.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

#include <geom/geom.h>

/*
 * SWB_NPAGES must be a power of 2.  It may be set to 1, 2, 4, 8, 16
 * or 32 pages per allocation.
 * The 32-page limit is due to the radix code (kern/subr_blist.c).
 */
#ifndef MAX_PAGEOUT_CLUSTER
#define MAX_PAGEOUT_CLUSTER 16
#endif

#if !defined(SWB_NPAGES)
#define SWB_NPAGES	MAX_PAGEOUT_CLUSTER
#endif

/*
 * The swblock structure maps an object and a small, fixed-size range
 * of page indices to disk addresses within a swap area.
 * The collection of these mappings is implemented as a hash table.
 * Unused disk addresses within a swap area are allocated and managed
 * using a blist.
 */
#define SWCORRECT(n) (sizeof(void *) * (n) / sizeof(daddr_t))
#define SWAP_META_PAGES		(SWB_NPAGES * 2)
#define SWAP_META_MASK		(SWAP_META_PAGES - 1)

struct swblock {
	struct swblock	*swb_hnext;
	vm_object_t	swb_object;
	vm_pindex_t	swb_index;
	int		swb_count;
	daddr_t		swb_pages[SWAP_META_PAGES];
};

static struct mtx sw_dev_mtx;
static TAILQ_HEAD(, swdevt) swtailq = TAILQ_HEAD_INITIALIZER(swtailq);
static struct swdevt *swdevhd;	/* Allocate from here next */
static int nswapdev;		/* Number of swap devices */
int swap_pager_avail;
static int swdev_syscall_active = 0; /* serialize swap(on|off) */

static vm_ooffset_t swap_total;
SYSCTL_QUAD(_vm, OID_AUTO, swap_total, CTLFLAG_RD, &swap_total, 0, "");
static vm_ooffset_t swap_reserved;
SYSCTL_QUAD(_vm, OID_AUTO, swap_reserved, CTLFLAG_RD, &swap_reserved, 0, "");
static int overcommit = 0;
SYSCTL_INT(_vm, OID_AUTO, overcommit, CTLFLAG_RW, &overcommit, 0, "");

/* bits from overcommit */
#define	SWAP_RESERVE_FORCE_ON		(1 << 0)
#define	SWAP_RESERVE_RLIMIT_ON		(1 << 1)
#define	SWAP_RESERVE_ALLOW_NONWIRED	(1 << 2)

int
swap_reserve(vm_ooffset_t incr)
{

	return (swap_reserve_by_uid(incr, curthread->td_ucred->cr_ruidinfo));
}

int
swap_reserve_by_uid(vm_ooffset_t incr, struct uidinfo *uip)
{
	vm_ooffset_t r, s;
	int res, error;
	static int curfail;
	static struct timeval lastfail;

	if (incr & PAGE_MASK)
		panic("swap_reserve: & PAGE_MASK");

	res = 0;
	mtx_lock(&sw_dev_mtx);
	r = swap_reserved + incr;
	if (overcommit & SWAP_RESERVE_ALLOW_NONWIRED) {
		s = cnt.v_page_count - cnt.v_free_reserved - cnt.v_wire_count;
		s *= PAGE_SIZE;
	} else
		s = 0;
	s += swap_total;
	if ((overcommit & SWAP_RESERVE_FORCE_ON) == 0 || r <= s ||
	    (error = priv_check(curthread, PRIV_VM_SWAP_NOQUOTA)) == 0) {
		res = 1;
		swap_reserved = r;
	}
	mtx_unlock(&sw_dev_mtx);

	if (res) {
		PROC_LOCK(curproc);
		UIDINFO_VMSIZE_LOCK(uip);
		if ((overcommit & SWAP_RESERVE_RLIMIT_ON) != 0 &&
		    uip->ui_vmsize + incr > lim_cur(curproc, RLIMIT_SWAP) &&
		    priv_check(curthread, PRIV_VM_SWAP_NORLIMIT))
			res = 0;
		else
			uip->ui_vmsize += incr;
		UIDINFO_VMSIZE_UNLOCK(uip);
		PROC_UNLOCK(curproc);
		if (!res) {
			mtx_lock(&sw_dev_mtx);
			swap_reserved -= incr;
			mtx_unlock(&sw_dev_mtx);
		}
	}
	if (!res && ppsratecheck(&lastfail, &curfail, 1)) {
		printf("uid %d, pid %d: swap reservation for %jd bytes failed\n",
		    curproc->p_pid, uip->ui_uid, incr);
	}

	return (res);
}

void
swap_reserve_force(vm_ooffset_t incr)
{
	struct uidinfo *uip;

	mtx_lock(&sw_dev_mtx);
	swap_reserved += incr;
	mtx_unlock(&sw_dev_mtx);

	uip = curthread->td_ucred->cr_ruidinfo;
	PROC_LOCK(curproc);
	UIDINFO_VMSIZE_LOCK(uip);
	uip->ui_vmsize += incr;
	UIDINFO_VMSIZE_UNLOCK(uip);
	PROC_UNLOCK(curproc);
}

void
swap_release(vm_ooffset_t decr)
{
	struct uidinfo *uip;

	PROC_LOCK(curproc);
	uip = curthread->td_ucred->cr_ruidinfo;
	swap_release_by_uid(decr, uip);
	PROC_UNLOCK(curproc);
}

void
swap_release_by_uid(vm_ooffset_t decr, struct uidinfo *uip)
{

	if (decr & PAGE_MASK)
		panic("swap_release: & PAGE_MASK");

	mtx_lock(&sw_dev_mtx);
	if (swap_reserved < decr)
		panic("swap_reserved < decr");
	swap_reserved -= decr;
	mtx_unlock(&sw_dev_mtx);

	UIDINFO_VMSIZE_LOCK(uip);
	if (uip->ui_vmsize < decr)
		printf("negative vmsize for uid = %d\n", uip->ui_uid);
	uip->ui_vmsize -= decr;
	UIDINFO_VMSIZE_UNLOCK(uip);
}

static void swapdev_strategy(struct buf *, struct swdevt *sw);

#define SWM_FREE	0x02	/* free, period			*/
#define SWM_POP		0x04	/* pop out			*/

int swap_pager_full = 2;	/* swap space exhaustion (task killing) */
static int swap_pager_almost_full = 1; /* swap space exhaustion (w/hysteresis)*/
static int nsw_rcount;		/* free read buffers			*/
static int nsw_wcount_sync;	/* limit write buffers / synchronous	*/
static int nsw_wcount_async;	/* limit write buffers / asynchronous	*/
static int nsw_wcount_async_max;/* assigned maximum			*/
static int nsw_cluster_max;	/* maximum VOP I/O allowed		*/

static struct swblock **swhash;
static int swhash_mask;
static struct mtx swhash_mtx;

static int swap_async_max = 4;	/* maximum in-progress async I/O's	*/
static struct sx sw_alloc_sx;


SYSCTL_INT(_vm, OID_AUTO, swap_async_max,
        CTLFLAG_RW, &swap_async_max, 0, "Maximum running async swap ops");

/*
 * "named" and "unnamed" anon region objects.  Try to reduce the overhead
 * of searching a named list by hashing it just a little.
 */

#define NOBJLISTS		8

#define NOBJLIST(handle)	\
	(&swap_pager_object_list[((int)(intptr_t)handle >> 4) & (NOBJLISTS-1)])

static struct mtx sw_alloc_mtx;	/* protect list manipulation */ 
static struct pagerlst	swap_pager_object_list[NOBJLISTS];
static uma_zone_t	swap_zone;
static struct vm_object	swap_zone_obj;

/*
 * pagerops for OBJT_SWAP - "swap pager".  Some ops are also global procedure
 * calls hooked from other parts of the VM system and do not appear here.
 * (see vm/swap_pager.h).
 */
static vm_object_t
		swap_pager_alloc(void *handle, vm_ooffset_t size,
		    vm_prot_t prot, vm_ooffset_t offset, struct ucred *);
static void	swap_pager_dealloc(vm_object_t object);
static int	swap_pager_getpages(vm_object_t, vm_page_t *, int, int);
static void	swap_pager_putpages(vm_object_t, vm_page_t *, int, boolean_t, int *);
static boolean_t
		swap_pager_haspage(vm_object_t object, vm_pindex_t pindex, int *before, int *after);
static void	swap_pager_init(void);
static void	swap_pager_unswapped(vm_page_t);
static void	swap_pager_swapoff(struct swdevt *sp);

struct pagerops swappagerops = {
	.pgo_init =	swap_pager_init,	/* early system initialization of pager	*/
	.pgo_alloc =	swap_pager_alloc,	/* allocate an OBJT_SWAP object		*/
	.pgo_dealloc =	swap_pager_dealloc,	/* deallocate an OBJT_SWAP object	*/
	.pgo_getpages =	swap_pager_getpages,	/* pagein				*/
	.pgo_putpages =	swap_pager_putpages,	/* pageout				*/
	.pgo_haspage =	swap_pager_haspage,	/* get backing store status for page	*/
	.pgo_pageunswapped = swap_pager_unswapped,	/* remove swap related to page		*/
};

/*
 * dmmax is in page-sized chunks with the new swap system.  It was
 * dev-bsized chunks in the old.  dmmax is always a power of 2.
 *
 * swap_*() routines are externally accessible.  swp_*() routines are
 * internal.
 */
static int dmmax;
static int nswap_lowat = 128;	/* in pages, swap_pager_almost_full warn */
static int nswap_hiwat = 512;	/* in pages, swap_pager_almost_full warn */

SYSCTL_INT(_vm, OID_AUTO, dmmax,
	CTLFLAG_RD, &dmmax, 0, "Maximum size of a swap block");

static void	swp_sizecheck(void);
static void	swp_pager_async_iodone(struct buf *bp);
static int	swapongeom(struct thread *, struct vnode *);
static int	swaponvp(struct thread *, struct vnode *, u_long);
static int	swapoff_one(struct swdevt *sp, struct ucred *cred);

/*
 * Swap bitmap functions
 */
static void	swp_pager_freeswapspace(daddr_t blk, int npages);
static daddr_t	swp_pager_getswapspace(int npages);

/*
 * Metadata functions
 */
static struct swblock **swp_pager_hash(vm_object_t object, vm_pindex_t index);
static void swp_pager_meta_build(vm_object_t, vm_pindex_t, daddr_t);
static void swp_pager_meta_free(vm_object_t, vm_pindex_t, daddr_t);
static void swp_pager_meta_free_all(vm_object_t);
static daddr_t swp_pager_meta_ctl(vm_object_t, vm_pindex_t, int);

static void
swp_pager_free_nrpage(vm_page_t m)
{

	if (m->wire_count == 0)
		vm_page_free(m);
}

/*
 * SWP_SIZECHECK() -	update swap_pager_full indication
 *	
 *	update the swap_pager_almost_full indication and warn when we are
 *	about to run out of swap space, using lowat/hiwat hysteresis.
 *
 *	Clear swap_pager_full ( task killing ) indication when lowat is met.
 *
 *	No restrictions on call
 *	This routine may not block.
 */
static void
swp_sizecheck(void)
{

	if (swap_pager_avail < nswap_lowat) {
		if (swap_pager_almost_full == 0) {
			printf("swap_pager: out of swap space\n");
			swap_pager_almost_full = 1;
		}
	} else {
		swap_pager_full = 0;
		if (swap_pager_avail > nswap_hiwat)
			swap_pager_almost_full = 0;
	}
}

/*
 * SWP_PAGER_HASH() -	hash swap meta data
 *
 *	This is an helper function which hashes the swapblk given
 *	the object and page index.  It returns a pointer to a pointer
 *	to the object, or a pointer to a NULL pointer if it could not
 *	find a swapblk.
 */
static struct swblock **
swp_pager_hash(vm_object_t object, vm_pindex_t index)
{
	struct swblock **pswap;
	struct swblock *swap;

	index &= ~(vm_pindex_t)SWAP_META_MASK;
	pswap = &swhash[(index ^ (int)(intptr_t)object) & swhash_mask];
	while ((swap = *pswap) != NULL) {
		if (swap->swb_object == object &&
		    swap->swb_index == index
		) {
			break;
		}
		pswap = &swap->swb_hnext;
	}
	return (pswap);
}

/*
 * SWAP_PAGER_INIT() -	initialize the swap pager!
 *
 *	Expected to be started from system init.  NOTE:  This code is run 
 *	before much else so be careful what you depend on.  Most of the VM
 *	system has yet to be initialized at this point.
 */
static void
swap_pager_init(void)
{
	/*
	 * Initialize object lists
	 */
	int i;

	for (i = 0; i < NOBJLISTS; ++i)
		TAILQ_INIT(&swap_pager_object_list[i]);
	mtx_init(&sw_alloc_mtx, "swap_pager list", NULL, MTX_DEF);
	mtx_init(&sw_dev_mtx, "swapdev", NULL, MTX_DEF);

	/*
	 * Device Stripe, in PAGE_SIZE'd blocks
	 */
	dmmax = SWB_NPAGES * 2;
}

/*
 * SWAP_PAGER_SWAP_INIT() - swap pager initialization from pageout process
 *
 *	Expected to be started from pageout process once, prior to entering
 *	its main loop.
 */
void
swap_pager_swap_init(void)
{
	int n, n2;

	/*
	 * Number of in-transit swap bp operations.  Don't
	 * exhaust the pbufs completely.  Make sure we
	 * initialize workable values (0 will work for hysteresis
	 * but it isn't very efficient).
	 *
	 * The nsw_cluster_max is constrained by the bp->b_pages[]
	 * array (MAXPHYS/PAGE_SIZE) and our locally defined
	 * MAX_PAGEOUT_CLUSTER.   Also be aware that swap ops are
	 * constrained by the swap device interleave stripe size.
	 *
	 * Currently we hardwire nsw_wcount_async to 4.  This limit is 
	 * designed to prevent other I/O from having high latencies due to
	 * our pageout I/O.  The value 4 works well for one or two active swap
	 * devices but is probably a little low if you have more.  Even so,
	 * a higher value would probably generate only a limited improvement
	 * with three or four active swap devices since the system does not
	 * typically have to pageout at extreme bandwidths.   We will want
	 * at least 2 per swap devices, and 4 is a pretty good value if you
	 * have one NFS swap device due to the command/ack latency over NFS.
	 * So it all works out pretty well.
	 */
	nsw_cluster_max = min((MAXPHYS/PAGE_SIZE), MAX_PAGEOUT_CLUSTER);

	mtx_lock(&pbuf_mtx);
	nsw_rcount = (nswbuf + 1) / 2;
	nsw_wcount_sync = (nswbuf + 3) / 4;
	nsw_wcount_async = 4;
	nsw_wcount_async_max = nsw_wcount_async;
	mtx_unlock(&pbuf_mtx);

	/*
	 * Initialize our zone.  Right now I'm just guessing on the number
	 * we need based on the number of pages in the system.  Each swblock
	 * can hold 16 pages, so this is probably overkill.  This reservation
	 * is typically limited to around 32MB by default.
	 */
	n = cnt.v_page_count / 2;
	if (maxswzone && n > maxswzone / sizeof(struct swblock))
		n = maxswzone / sizeof(struct swblock);
	n2 = n;
	swap_zone = uma_zcreate("SWAPMETA", sizeof(struct swblock), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE | UMA_ZONE_VM);
	if (swap_zone == NULL)
		panic("failed to create swap_zone.");
	do {
		if (uma_zone_set_obj(swap_zone, &swap_zone_obj, n))
			break;
		/*
		 * if the allocation failed, try a zone two thirds the
		 * size of the previous attempt.
		 */
		n -= ((n + 2) / 3);
	} while (n > 0);
	if (n2 != n)
		printf("Swap zone entries reduced from %d to %d.\n", n2, n);
	n2 = n;

	/*
	 * Initialize our meta-data hash table.  The swapper does not need to
	 * be quite as efficient as the VM system, so we do not use an 
	 * oversized hash table.
	 *
	 * 	n: 		size of hash table, must be power of 2
	 *	swhash_mask:	hash table index mask
	 */
	for (n = 1; n < n2 / 8; n *= 2)
		;
	swhash = malloc(sizeof(struct swblock *) * n, M_VMPGDATA, M_WAITOK | M_ZERO);
	swhash_mask = n - 1;
	mtx_init(&swhash_mtx, "swap_pager swhash", NULL, MTX_DEF);
}

/*
 * SWAP_PAGER_ALLOC() -	allocate a new OBJT_SWAP VM object and instantiate
 *			its metadata structures.
 *
 *	This routine is called from the mmap and fork code to create a new
 *	OBJT_SWAP object.  We do this by creating an OBJT_DEFAULT object
 *	and then converting it with swp_pager_meta_build().
 *
 *	This routine may block in vm_object_allocate() and create a named
 *	object lookup race, so we must interlock.
 *
 * MPSAFE
 */
static vm_object_t
swap_pager_alloc(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t offset, struct ucred *cred)
{
	vm_object_t object;
	vm_pindex_t pindex;
	struct uidinfo *uip;

	uip = NULL;
	pindex = OFF_TO_IDX(offset + PAGE_MASK + size);
	if (handle) {
		mtx_lock(&Giant);
		/*
		 * Reference existing named region or allocate new one.  There
		 * should not be a race here against swp_pager_meta_build()
		 * as called from vm_page_remove() in regards to the lookup
		 * of the handle.
		 */
		sx_xlock(&sw_alloc_sx);
		object = vm_pager_object_lookup(NOBJLIST(handle), handle);
		if (object == NULL) {
			if (cred != NULL) {
				uip = cred->cr_ruidinfo;
				if (!swap_reserve_by_uid(size, uip)) {
					sx_xunlock(&sw_alloc_sx);
					mtx_unlock(&Giant);
					return (NULL);
				}
				uihold(uip);
			}
			object = vm_object_allocate(OBJT_DEFAULT, pindex);
			VM_OBJECT_LOCK(object);
			object->handle = handle;
			if (cred != NULL) {
				object->uip = uip;
				object->charge = size;
			}
			swp_pager_meta_build(object, 0, SWAPBLK_NONE);
			VM_OBJECT_UNLOCK(object);
		}
		sx_xunlock(&sw_alloc_sx);
		mtx_unlock(&Giant);
	} else {
		if (cred != NULL) {
			uip = cred->cr_ruidinfo;
			if (!swap_reserve_by_uid(size, uip))
				return (NULL);
			uihold(uip);
		}
		object = vm_object_allocate(OBJT_DEFAULT, pindex);
		VM_OBJECT_LOCK(object);
		if (cred != NULL) {
			object->uip = uip;
			object->charge = size;
		}
		swp_pager_meta_build(object, 0, SWAPBLK_NONE);
		VM_OBJECT_UNLOCK(object);
	}
	return (object);
}

/*
 * SWAP_PAGER_DEALLOC() -	remove swap metadata from object
 *
 *	The swap backing for the object is destroyed.  The code is 
 *	designed such that we can reinstantiate it later, but this
 *	routine is typically called only when the entire object is
 *	about to be destroyed.
 *
 *	The object must be locked.
 */
static void
swap_pager_dealloc(vm_object_t object)
{

	/*
	 * Remove from list right away so lookups will fail if we block for
	 * pageout completion.
	 */
	if (object->handle != NULL) {
		mtx_lock(&sw_alloc_mtx);
		TAILQ_REMOVE(NOBJLIST(object->handle), object, pager_object_list);
		mtx_unlock(&sw_alloc_mtx);
	}

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	vm_object_pip_wait(object, "swpdea");

	/*
	 * Free all remaining metadata.  We only bother to free it from 
	 * the swap meta data.  We do not attempt to free swapblk's still
	 * associated with vm_page_t's for this object.  We do not care
	 * if paging is still in progress on some objects.
	 */
	swp_pager_meta_free_all(object);
}

/************************************************************************
 *			SWAP PAGER BITMAP ROUTINES			*
 ************************************************************************/

/*
 * SWP_PAGER_GETSWAPSPACE() -	allocate raw swap space
 *
 *	Allocate swap for the requested number of pages.  The starting
 *	swap block number (a page index) is returned or SWAPBLK_NONE
 *	if the allocation failed.
 *
 *	Also has the side effect of advising that somebody made a mistake
 *	when they configured swap and didn't configure enough.
 *
 *	This routine may not sleep.
 *
 *	We allocate in round-robin fashion from the configured devices.
 */
static daddr_t
swp_pager_getswapspace(int npages)
{
	daddr_t blk;
	struct swdevt *sp;
	int i;

	blk = SWAPBLK_NONE;
	mtx_lock(&sw_dev_mtx);
	sp = swdevhd;
	for (i = 0; i < nswapdev; i++) {
		if (sp == NULL)
			sp = TAILQ_FIRST(&swtailq);
		if (!(sp->sw_flags & SW_CLOSING)) {
			blk = blist_alloc(sp->sw_blist, npages);
			if (blk != SWAPBLK_NONE) {
				blk += sp->sw_first;
				sp->sw_used += npages;
				swap_pager_avail -= npages;
				swp_sizecheck();
				swdevhd = TAILQ_NEXT(sp, sw_list);
				goto done;
			}
		}
		sp = TAILQ_NEXT(sp, sw_list);
	}
	if (swap_pager_full != 2) {
		printf("swap_pager_getswapspace(%d): failed\n", npages);
		swap_pager_full = 2;
		swap_pager_almost_full = 1;
	}
	swdevhd = NULL;
done:
	mtx_unlock(&sw_dev_mtx);
	return (blk);
}

static int
swp_pager_isondev(daddr_t blk, struct swdevt *sp)
{

	return (blk >= sp->sw_first && blk < sp->sw_end);
}
	
static void
swp_pager_strategy(struct buf *bp)
{
	struct swdevt *sp;

	mtx_lock(&sw_dev_mtx);
	TAILQ_FOREACH(sp, &swtailq, sw_list) {
		if (bp->b_blkno >= sp->sw_first && bp->b_blkno < sp->sw_end) {
			mtx_unlock(&sw_dev_mtx);
			sp->sw_strategy(bp, sp);
			return;
		}
	}
	panic("Swapdev not found");
}
	

/*
 * SWP_PAGER_FREESWAPSPACE() -	free raw swap space 
 *
 *	This routine returns the specified swap blocks back to the bitmap.
 *
 *	This routine may not sleep.
 */
static void
swp_pager_freeswapspace(daddr_t blk, int npages)
{
	struct swdevt *sp;

	mtx_lock(&sw_dev_mtx);
	TAILQ_FOREACH(sp, &swtailq, sw_list) {
		if (blk >= sp->sw_first && blk < sp->sw_end) {
			sp->sw_used -= npages;
			/*
			 * If we are attempting to stop swapping on
			 * this device, we don't want to mark any
			 * blocks free lest they be reused.  
			 */
			if ((sp->sw_flags & SW_CLOSING) == 0) {
				blist_free(sp->sw_blist, blk - sp->sw_first,
				    npages);
				swap_pager_avail += npages;
				swp_sizecheck();
			}
			mtx_unlock(&sw_dev_mtx);
			return;
		}
	}
	panic("Swapdev not found");
}

/*
 * SWAP_PAGER_FREESPACE() -	frees swap blocks associated with a page
 *				range within an object.
 *
 *	This is a globally accessible routine.
 *
 *	This routine removes swapblk assignments from swap metadata.
 *
 *	The external callers of this routine typically have already destroyed 
 *	or renamed vm_page_t's associated with this range in the object so 
 *	we should be ok.
 */
void
swap_pager_freespace(vm_object_t object, vm_pindex_t start, vm_size_t size)
{

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	swp_pager_meta_free(object, start, size);
}

/*
 * SWAP_PAGER_RESERVE() - reserve swap blocks in object
 *
 *	Assigns swap blocks to the specified range within the object.  The 
 *	swap blocks are not zerod.  Any previous swap assignment is destroyed.
 *
 *	Returns 0 on success, -1 on failure.
 */
int
swap_pager_reserve(vm_object_t object, vm_pindex_t start, vm_size_t size)
{
	int n = 0;
	daddr_t blk = SWAPBLK_NONE;
	vm_pindex_t beg = start;	/* save start index */

	VM_OBJECT_LOCK(object);
	while (size) {
		if (n == 0) {
			n = BLIST_MAX_ALLOC;
			while ((blk = swp_pager_getswapspace(n)) == SWAPBLK_NONE) {
				n >>= 1;
				if (n == 0) {
					swp_pager_meta_free(object, beg, start - beg);
					VM_OBJECT_UNLOCK(object);
					return (-1);
				}
			}
		}
		swp_pager_meta_build(object, start, blk);
		--size;
		++start;
		++blk;
		--n;
	}
	swp_pager_meta_free(object, start, n);
	VM_OBJECT_UNLOCK(object);
	return (0);
}

/*
 * SWAP_PAGER_COPY() -  copy blocks from source pager to destination pager
 *			and destroy the source.
 *
 *	Copy any valid swapblks from the source to the destination.  In
 *	cases where both the source and destination have a valid swapblk,
 *	we keep the destination's.
 *
 *	This routine is allowed to sleep.  It may sleep allocating metadata
 *	indirectly through swp_pager_meta_build() or if paging is still in
 *	progress on the source. 
 *
 *	The source object contains no vm_page_t's (which is just as well)
 *
 *	The source object is of type OBJT_SWAP.
 *
 *	The source and destination objects must be locked.
 *	Both object locks may temporarily be released.
 */
void
swap_pager_copy(vm_object_t srcobject, vm_object_t dstobject,
    vm_pindex_t offset, int destroysource)
{
	vm_pindex_t i;

	VM_OBJECT_LOCK_ASSERT(srcobject, MA_OWNED);
	VM_OBJECT_LOCK_ASSERT(dstobject, MA_OWNED);

	/*
	 * If destroysource is set, we remove the source object from the 
	 * swap_pager internal queue now. 
	 */
	if (destroysource) {
		if (srcobject->handle != NULL) {
			mtx_lock(&sw_alloc_mtx);
			TAILQ_REMOVE(
			    NOBJLIST(srcobject->handle),
			    srcobject,
			    pager_object_list
			);
			mtx_unlock(&sw_alloc_mtx);
		}
	}

	/*
	 * transfer source to destination.
	 */
	for (i = 0; i < dstobject->size; ++i) {
		daddr_t dstaddr;

		/*
		 * Locate (without changing) the swapblk on the destination,
		 * unless it is invalid in which case free it silently, or
		 * if the destination is a resident page, in which case the
		 * source is thrown away.
		 */
		dstaddr = swp_pager_meta_ctl(dstobject, i, 0);

		if (dstaddr == SWAPBLK_NONE) {
			/*
			 * Destination has no swapblk and is not resident,
			 * copy source.
			 */
			daddr_t srcaddr;

			srcaddr = swp_pager_meta_ctl(
			    srcobject, 
			    i + offset,
			    SWM_POP
			);

			if (srcaddr != SWAPBLK_NONE) {
				/*
				 * swp_pager_meta_build() can sleep.
				 */
				vm_object_pip_add(srcobject, 1);
				VM_OBJECT_UNLOCK(srcobject);
				vm_object_pip_add(dstobject, 1);
				swp_pager_meta_build(dstobject, i, srcaddr);
				vm_object_pip_wakeup(dstobject);
				VM_OBJECT_LOCK(srcobject);
				vm_object_pip_wakeup(srcobject);
			}
		} else {
			/*
			 * Destination has valid swapblk or it is represented
			 * by a resident page.  We destroy the sourceblock.
			 */
			
			swp_pager_meta_ctl(srcobject, i + offset, SWM_FREE);
		}
	}

	/*
	 * Free left over swap blocks in source.
	 *
	 * We have to revert the type to OBJT_DEFAULT so we do not accidently
	 * double-remove the object from the swap queues.
	 */
	if (destroysource) {
		swp_pager_meta_free_all(srcobject);
		/*
		 * Reverting the type is not necessary, the caller is going
		 * to destroy srcobject directly, but I'm doing it here
		 * for consistency since we've removed the object from its
		 * queues.
		 */
		srcobject->type = OBJT_DEFAULT;
	}
}

/*
 * SWAP_PAGER_HASPAGE() -	determine if we have good backing store for
 *				the requested page.
 *
 *	We determine whether good backing store exists for the requested
 *	page and return TRUE if it does, FALSE if it doesn't.
 *
 *	If TRUE, we also try to determine how much valid, contiguous backing
 *	store exists before and after the requested page within a reasonable
 *	distance.  We do not try to restrict it to the swap device stripe
 *	(that is handled in getpages/putpages).  It probably isn't worth
 *	doing here.
 */
static boolean_t
swap_pager_haspage(vm_object_t object, vm_pindex_t pindex, int *before, int *after)
{
	daddr_t blk0;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	/*
	 * do we have good backing store at the requested index ?
	 */
	blk0 = swp_pager_meta_ctl(object, pindex, 0);

	if (blk0 == SWAPBLK_NONE) {
		if (before)
			*before = 0;
		if (after)
			*after = 0;
		return (FALSE);
	}

	/*
	 * find backwards-looking contiguous good backing store
	 */
	if (before != NULL) {
		int i;

		for (i = 1; i < (SWB_NPAGES/2); ++i) {
			daddr_t blk;

			if (i > pindex)
				break;
			blk = swp_pager_meta_ctl(object, pindex - i, 0);
			if (blk != blk0 - i)
				break;
		}
		*before = (i - 1);
	}

	/*
	 * find forward-looking contiguous good backing store
	 */
	if (after != NULL) {
		int i;

		for (i = 1; i < (SWB_NPAGES/2); ++i) {
			daddr_t blk;

			blk = swp_pager_meta_ctl(object, pindex + i, 0);
			if (blk != blk0 + i)
				break;
		}
		*after = (i - 1);
	}
	return (TRUE);
}

/*
 * SWAP_PAGER_PAGE_UNSWAPPED() - remove swap backing store related to page
 *
 *	This removes any associated swap backing store, whether valid or
 *	not, from the page.  
 *
 *	This routine is typically called when a page is made dirty, at
 *	which point any associated swap can be freed.  MADV_FREE also
 *	calls us in a special-case situation
 *
 *	NOTE!!!  If the page is clean and the swap was valid, the caller
 *	should make the page dirty before calling this routine.  This routine
 *	does NOT change the m->dirty status of the page.  Also: MADV_FREE
 *	depends on it.
 *
 *	This routine may not sleep.
 */
static void
swap_pager_unswapped(vm_page_t m)
{

	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	swp_pager_meta_ctl(m->object, m->pindex, SWM_FREE);
}

/*
 * SWAP_PAGER_GETPAGES() - bring pages in from swap
 *
 *	Attempt to retrieve (m, count) pages from backing store, but make
 *	sure we retrieve at least m[reqpage].  We try to load in as large
 *	a chunk surrounding m[reqpage] as is contiguous in swap and which
 *	belongs to the same object.
 *
 *	The code is designed for asynchronous operation and 
 *	immediate-notification of 'reqpage' but tends not to be
 *	used that way.  Please do not optimize-out this algorithmic
 *	feature, I intend to improve on it in the future.
 *
 *	The parent has a single vm_object_pip_add() reference prior to
 *	calling us and we should return with the same.
 *
 *	The parent has BUSY'd the pages.  We should return with 'm'
 *	left busy, but the others adjusted.
 */
static int
swap_pager_getpages(vm_object_t object, vm_page_t *m, int count, int reqpage)
{
	struct buf *bp;
	vm_page_t mreq;
	int i;
	int j;
	daddr_t blk;

	mreq = m[reqpage];

	KASSERT(mreq->object == object,
	    ("swap_pager_getpages: object mismatch %p/%p",
	    object, mreq->object));

	/*
	 * Calculate range to retrieve.  The pages have already been assigned
	 * their swapblks.  We require a *contiguous* range but we know it to
	 * not span devices.   If we do not supply it, bad things
	 * happen.  Note that blk, iblk & jblk can be SWAPBLK_NONE, but the 
	 * loops are set up such that the case(s) are handled implicitly.
	 *
	 * The swp_*() calls must be made with the object locked.
	 */
	blk = swp_pager_meta_ctl(mreq->object, mreq->pindex, 0);

	for (i = reqpage - 1; i >= 0; --i) {
		daddr_t iblk;

		iblk = swp_pager_meta_ctl(m[i]->object, m[i]->pindex, 0);
		if (blk != iblk + (reqpage - i))
			break;
	}
	++i;

	for (j = reqpage + 1; j < count; ++j) {
		daddr_t jblk;

		jblk = swp_pager_meta_ctl(m[j]->object, m[j]->pindex, 0);
		if (blk != jblk - (j - reqpage))
			break;
	}

	/*
	 * free pages outside our collection range.   Note: we never free
	 * mreq, it must remain busy throughout.
	 */
	if (0 < i || j < count) {
		int k;

		vm_page_lock_queues();
		for (k = 0; k < i; ++k)
			swp_pager_free_nrpage(m[k]);
		for (k = j; k < count; ++k)
			swp_pager_free_nrpage(m[k]);
		vm_page_unlock_queues();
	}

	/*
	 * Return VM_PAGER_FAIL if we have nothing to do.  Return mreq 
	 * still busy, but the others unbusied.
	 */
	if (blk == SWAPBLK_NONE)
		return (VM_PAGER_FAIL);

	/*
	 * Getpbuf() can sleep.
	 */
	VM_OBJECT_UNLOCK(object);
	/*
	 * Get a swap buffer header to perform the IO
	 */
	bp = getpbuf(&nsw_rcount);
	bp->b_flags |= B_PAGING;

	/*
	 * map our page(s) into kva for input
	 */
	pmap_qenter((vm_offset_t)bp->b_data, m + i, j - i);

	bp->b_iocmd = BIO_READ;
	bp->b_iodone = swp_pager_async_iodone;
	bp->b_rcred = crhold(thread0.td_ucred);
	bp->b_wcred = crhold(thread0.td_ucred);
	bp->b_blkno = blk - (reqpage - i);
	bp->b_bcount = PAGE_SIZE * (j - i);
	bp->b_bufsize = PAGE_SIZE * (j - i);
	bp->b_pager.pg_reqpage = reqpage - i;

	VM_OBJECT_LOCK(object);
	{
		int k;

		for (k = i; k < j; ++k) {
			bp->b_pages[k - i] = m[k];
			m[k]->oflags |= VPO_SWAPINPROG;
		}
	}
	bp->b_npages = j - i;

	PCPU_INC(cnt.v_swapin);
	PCPU_ADD(cnt.v_swappgsin, bp->b_npages);

	/*
	 * We still hold the lock on mreq, and our automatic completion routine
	 * does not remove it.
	 */
	vm_object_pip_add(object, bp->b_npages);
	VM_OBJECT_UNLOCK(object);

	/*
	 * perform the I/O.  NOTE!!!  bp cannot be considered valid after
	 * this point because we automatically release it on completion.
	 * Instead, we look at the one page we are interested in which we
	 * still hold a lock on even through the I/O completion.
	 *
	 * The other pages in our m[] array are also released on completion,
	 * so we cannot assume they are valid anymore either.
	 *
	 * NOTE: b_blkno is destroyed by the call to swapdev_strategy
	 */
	BUF_KERNPROC(bp);
	swp_pager_strategy(bp);

	/*
	 * wait for the page we want to complete.  VPO_SWAPINPROG is always
	 * cleared on completion.  If an I/O error occurs, SWAPBLK_NONE
	 * is set in the meta-data.
	 */
	VM_OBJECT_LOCK(object);
	while ((mreq->oflags & VPO_SWAPINPROG) != 0) {
		mreq->oflags |= VPO_WANTED;
		PCPU_INC(cnt.v_intrans);
		if (msleep(mreq, VM_OBJECT_MTX(object), PSWP, "swread", hz*20)) {
			printf(
"swap_pager: indefinite wait buffer: bufobj: %p, blkno: %jd, size: %ld\n",
			    bp->b_bufobj, (intmax_t)bp->b_blkno, bp->b_bcount);
		}
	}

	/*
	 * mreq is left busied after completion, but all the other pages
	 * are freed.  If we had an unrecoverable read error the page will
	 * not be valid.
	 */
	if (mreq->valid != VM_PAGE_BITS_ALL) {
		return (VM_PAGER_ERROR);
	} else {
		return (VM_PAGER_OK);
	}

	/*
	 * A final note: in a low swap situation, we cannot deallocate swap
	 * and mark a page dirty here because the caller is likely to mark
	 * the page clean when we return, causing the page to possibly revert 
	 * to all-zero's later.
	 */
}

/*
 *	swap_pager_putpages: 
 *
 *	Assign swap (if necessary) and initiate I/O on the specified pages.
 *
 *	We support both OBJT_DEFAULT and OBJT_SWAP objects.  DEFAULT objects
 *	are automatically converted to SWAP objects.
 *
 *	In a low memory situation we may block in VOP_STRATEGY(), but the new 
 *	vm_page reservation system coupled with properly written VFS devices 
 *	should ensure that no low-memory deadlock occurs.  This is an area
 *	which needs work.
 *
 *	The parent has N vm_object_pip_add() references prior to
 *	calling us and will remove references for rtvals[] that are
 *	not set to VM_PAGER_PEND.  We need to remove the rest on I/O
 *	completion.
 *
 *	The parent has soft-busy'd the pages it passes us and will unbusy
 *	those whos rtvals[] entry is not set to VM_PAGER_PEND on return.
 *	We need to unbusy the rest on I/O completion.
 */
void
swap_pager_putpages(vm_object_t object, vm_page_t *m, int count,
    boolean_t sync, int *rtvals)
{
	int i;
	int n = 0;

	if (count && m[0]->object != object) {
		panic("swap_pager_putpages: object mismatch %p/%p", 
		    object, 
		    m[0]->object
		);
	}

	/*
	 * Step 1
	 *
	 * Turn object into OBJT_SWAP
	 * check for bogus sysops
	 * force sync if not pageout process
	 */
	if (object->type != OBJT_SWAP)
		swp_pager_meta_build(object, 0, SWAPBLK_NONE);
	VM_OBJECT_UNLOCK(object);

	if (curproc != pageproc)
		sync = TRUE;

	/*
	 * Step 2
	 *
	 * Update nsw parameters from swap_async_max sysctl values.  
	 * Do not let the sysop crash the machine with bogus numbers.
	 */
	mtx_lock(&pbuf_mtx);
	if (swap_async_max != nsw_wcount_async_max) {
		int n;

		/*
		 * limit range
		 */
		if ((n = swap_async_max) > nswbuf / 2)
			n = nswbuf / 2;
		if (n < 1)
			n = 1;
		swap_async_max = n;

		/*
		 * Adjust difference ( if possible ).  If the current async
		 * count is too low, we may not be able to make the adjustment
		 * at this time.
		 */
		n -= nsw_wcount_async_max;
		if (nsw_wcount_async + n >= 0) {
			nsw_wcount_async += n;
			nsw_wcount_async_max += n;
			wakeup(&nsw_wcount_async);
		}
	}
	mtx_unlock(&pbuf_mtx);

	/*
	 * Step 3
	 *
	 * Assign swap blocks and issue I/O.  We reallocate swap on the fly.
	 * The page is left dirty until the pageout operation completes
	 * successfully.
	 */
	for (i = 0; i < count; i += n) {
		int j;
		struct buf *bp;
		daddr_t blk;

		/*
		 * Maximum I/O size is limited by a number of factors.
		 */
		n = min(BLIST_MAX_ALLOC, count - i);
		n = min(n, nsw_cluster_max);

		/*
		 * Get biggest block of swap we can.  If we fail, fall
		 * back and try to allocate a smaller block.  Don't go
		 * overboard trying to allocate space if it would overly
		 * fragment swap.
		 */
		while (
		    (blk = swp_pager_getswapspace(n)) == SWAPBLK_NONE &&
		    n > 4
		) {
			n >>= 1;
		}
		if (blk == SWAPBLK_NONE) {
			for (j = 0; j < n; ++j)
				rtvals[i+j] = VM_PAGER_FAIL;
			continue;
		}

		/*
		 * All I/O parameters have been satisfied, build the I/O
		 * request and assign the swap space.
		 */
		if (sync == TRUE) {
			bp = getpbuf(&nsw_wcount_sync);
		} else {
			bp = getpbuf(&nsw_wcount_async);
			bp->b_flags = B_ASYNC;
		}
		bp->b_flags |= B_PAGING;
		bp->b_iocmd = BIO_WRITE;

		pmap_qenter((vm_offset_t)bp->b_data, &m[i], n);

		bp->b_rcred = crhold(thread0.td_ucred);
		bp->b_wcred = crhold(thread0.td_ucred);
		bp->b_bcount = PAGE_SIZE * n;
		bp->b_bufsize = PAGE_SIZE * n;
		bp->b_blkno = blk;

		VM_OBJECT_LOCK(object);
		for (j = 0; j < n; ++j) {
			vm_page_t mreq = m[i+j];

			swp_pager_meta_build(
			    mreq->object, 
			    mreq->pindex,
			    blk + j
			);
			vm_page_dirty(mreq);
			rtvals[i+j] = VM_PAGER_OK;

			mreq->oflags |= VPO_SWAPINPROG;
			bp->b_pages[j] = mreq;
		}
		VM_OBJECT_UNLOCK(object);
		bp->b_npages = n;
		/*
		 * Must set dirty range for NFS to work.
		 */
		bp->b_dirtyoff = 0;
		bp->b_dirtyend = bp->b_bcount;

		PCPU_INC(cnt.v_swapout);
		PCPU_ADD(cnt.v_swappgsout, bp->b_npages);

		/*
		 * asynchronous
		 *
		 * NOTE: b_blkno is destroyed by the call to swapdev_strategy
		 */
		if (sync == FALSE) {
			bp->b_iodone = swp_pager_async_iodone;
			BUF_KERNPROC(bp);
			swp_pager_strategy(bp);

			for (j = 0; j < n; ++j)
				rtvals[i+j] = VM_PAGER_PEND;
			/* restart outter loop */
			continue;
		}

		/*
		 * synchronous
		 *
		 * NOTE: b_blkno is destroyed by the call to swapdev_strategy
		 */
		bp->b_iodone = bdone;
		swp_pager_strategy(bp);

		/*
		 * Wait for the sync I/O to complete, then update rtvals.
		 * We just set the rtvals[] to VM_PAGER_PEND so we can call
		 * our async completion routine at the end, thus avoiding a
		 * double-free.
		 */
		bwait(bp, PVM, "swwrt");
		for (j = 0; j < n; ++j)
			rtvals[i+j] = VM_PAGER_PEND;
		/*
		 * Now that we are through with the bp, we can call the
		 * normal async completion, which frees everything up.
		 */
		swp_pager_async_iodone(bp);
	}
	VM_OBJECT_LOCK(object);
}

/*
 *	swp_pager_async_iodone:
 *
 *	Completion routine for asynchronous reads and writes from/to swap.
 *	Also called manually by synchronous code to finish up a bp.
 *
 *	For READ operations, the pages are VPO_BUSY'd.  For WRITE operations, 
 *	the pages are vm_page_t->busy'd.  For READ operations, we VPO_BUSY 
 *	unbusy all pages except the 'main' request page.  For WRITE 
 *	operations, we vm_page_t->busy'd unbusy all pages ( we can do this 
 *	because we marked them all VM_PAGER_PEND on return from putpages ).
 *
 *	This routine may not sleep.
 */
static void
swp_pager_async_iodone(struct buf *bp)
{
	int i;
	vm_object_t object = NULL;

	/*
	 * report error
	 */
	if (bp->b_ioflags & BIO_ERROR) {
		printf(
		    "swap_pager: I/O error - %s failed; blkno %ld,"
			"size %ld, error %d\n",
		    ((bp->b_iocmd == BIO_READ) ? "pagein" : "pageout"),
		    (long)bp->b_blkno, 
		    (long)bp->b_bcount,
		    bp->b_error
		);
	}

	/*
	 * remove the mapping for kernel virtual
	 */
	pmap_qremove((vm_offset_t)bp->b_data, bp->b_npages);

	if (bp->b_npages) {
		object = bp->b_pages[0]->object;
		VM_OBJECT_LOCK(object);
	}
	vm_page_lock_queues();
	/*
	 * cleanup pages.  If an error occurs writing to swap, we are in
	 * very serious trouble.  If it happens to be a disk error, though,
	 * we may be able to recover by reassigning the swap later on.  So
	 * in this case we remove the m->swapblk assignment for the page 
	 * but do not free it in the rlist.  The errornous block(s) are thus
	 * never reallocated as swap.  Redirty the page and continue.
	 */
	for (i = 0; i < bp->b_npages; ++i) {
		vm_page_t m = bp->b_pages[i];

		m->oflags &= ~VPO_SWAPINPROG;

		if (bp->b_ioflags & BIO_ERROR) {
			/*
			 * If an error occurs I'd love to throw the swapblk
			 * away without freeing it back to swapspace, so it
			 * can never be used again.  But I can't from an 
			 * interrupt.
			 */
			if (bp->b_iocmd == BIO_READ) {
				/*
				 * When reading, reqpage needs to stay
				 * locked for the parent, but all other
				 * pages can be freed.  We still want to
				 * wakeup the parent waiting on the page,
				 * though.  ( also: pg_reqpage can be -1 and 
				 * not match anything ).
				 *
				 * We have to wake specifically requested pages
				 * up too because we cleared VPO_SWAPINPROG and
				 * someone may be waiting for that.
				 *
				 * NOTE: for reads, m->dirty will probably
				 * be overridden by the original caller of
				 * getpages so don't play cute tricks here.
				 */
				m->valid = 0;
				if (i != bp->b_pager.pg_reqpage)
					swp_pager_free_nrpage(m);
				else
					vm_page_flash(m);
				/*
				 * If i == bp->b_pager.pg_reqpage, do not wake 
				 * the page up.  The caller needs to.
				 */
			} else {
				/*
				 * If a write error occurs, reactivate page
				 * so it doesn't clog the inactive list,
				 * then finish the I/O.
				 */
				vm_page_dirty(m);
				vm_page_activate(m);
				vm_page_io_finish(m);
			}
		} else if (bp->b_iocmd == BIO_READ) {
			/*
			 * NOTE: for reads, m->dirty will probably be 
			 * overridden by the original caller of getpages so
			 * we cannot set them in order to free the underlying
			 * swap in a low-swap situation.  I don't think we'd
			 * want to do that anyway, but it was an optimization
			 * that existed in the old swapper for a time before
			 * it got ripped out due to precisely this problem.
			 *
			 * If not the requested page then deactivate it.
			 *
			 * Note that the requested page, reqpage, is left
			 * busied, but we still have to wake it up.  The
			 * other pages are released (unbusied) by 
			 * vm_page_wakeup().
			 */
			KASSERT(!pmap_page_is_mapped(m),
			    ("swp_pager_async_iodone: page %p is mapped", m));
			m->valid = VM_PAGE_BITS_ALL;
			KASSERT(m->dirty == 0,
			    ("swp_pager_async_iodone: page %p is dirty", m));

			/*
			 * We have to wake specifically requested pages
			 * up too because we cleared VPO_SWAPINPROG and
			 * could be waiting for it in getpages.  However,
			 * be sure to not unbusy getpages specifically
			 * requested page - getpages expects it to be 
			 * left busy.
			 */
			if (i != bp->b_pager.pg_reqpage) {
				vm_page_deactivate(m);
				vm_page_wakeup(m);
			} else {
				vm_page_flash(m);
			}
		} else {
			/*
			 * For write success, clear the dirty
			 * status, then finish the I/O ( which decrements the 
			 * busy count and possibly wakes waiter's up ).
			 */
			KASSERT((m->flags & PG_WRITEABLE) == 0,
			    ("swp_pager_async_iodone: page %p is not write"
			    " protected", m));
			vm_page_undirty(m);
			vm_page_io_finish(m);
			if (vm_page_count_severe())
				vm_page_try_to_cache(m);
		}
	}
	vm_page_unlock_queues();

	/*
	 * adjust pip.  NOTE: the original parent may still have its own
	 * pip refs on the object.
	 */
	if (object != NULL) {
		vm_object_pip_wakeupn(object, bp->b_npages);
		VM_OBJECT_UNLOCK(object);
	}

	/* 
	 * swapdev_strategy() manually sets b_vp and b_bufobj before calling 
	 * bstrategy(). Set them back to NULL now we're done with it, or we'll
	 * trigger a KASSERT in relpbuf().
	 */
	if (bp->b_vp) {
		    bp->b_vp = NULL;
		    bp->b_bufobj = NULL;
	}
	/*
	 * release the physical I/O buffer
	 */
	relpbuf(
	    bp, 
	    ((bp->b_iocmd == BIO_READ) ? &nsw_rcount : 
		((bp->b_flags & B_ASYNC) ? 
		    &nsw_wcount_async : 
		    &nsw_wcount_sync
		)
	    )
	);
}

/*
 *	swap_pager_isswapped:
 *
 *	Return 1 if at least one page in the given object is paged
 *	out to the given swap device.
 *
 *	This routine may not sleep.
 */
int
swap_pager_isswapped(vm_object_t object, struct swdevt *sp)
{
	daddr_t index = 0;
	int bcount;
	int i;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if (object->type != OBJT_SWAP)
		return (0);

	mtx_lock(&swhash_mtx);
	for (bcount = 0; bcount < object->un_pager.swp.swp_bcount; bcount++) {
		struct swblock *swap;

		if ((swap = *swp_pager_hash(object, index)) != NULL) {
			for (i = 0; i < SWAP_META_PAGES; ++i) {
				if (swp_pager_isondev(swap->swb_pages[i], sp)) {
					mtx_unlock(&swhash_mtx);
					return (1);
				}
			}
		}
		index += SWAP_META_PAGES;
	}
	mtx_unlock(&swhash_mtx);
	return (0);
}

/*
 * SWP_PAGER_FORCE_PAGEIN() - force a swap block to be paged in
 *
 *	This routine dissociates the page at the given index within a
 *	swap block from its backing store, paging it in if necessary.
 *	If the page is paged in, it is placed in the inactive queue,
 *	since it had its backing store ripped out from under it.
 *	We also attempt to swap in all other pages in the swap block,
 *	we only guarantee that the one at the specified index is
 *	paged in.
 *
 *	XXX - The code to page the whole block in doesn't work, so we
 *	      revert to the one-by-one behavior for now.  Sigh.
 */
static inline void
swp_pager_force_pagein(vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t m;

	vm_object_pip_add(object, 1);
	m = vm_page_grab(object, pindex, VM_ALLOC_NORMAL|VM_ALLOC_RETRY);
	if (m->valid == VM_PAGE_BITS_ALL) {
		vm_object_pip_subtract(object, 1);
		vm_page_lock_queues();
		vm_page_activate(m);
		vm_page_dirty(m);
		vm_page_unlock_queues();
		vm_page_wakeup(m);
		vm_pager_page_unswapped(m);
		return;
	}

	if (swap_pager_getpages(object, &m, 1, 0) != VM_PAGER_OK)
		panic("swap_pager_force_pagein: read from swap failed");/*XXX*/
	vm_object_pip_subtract(object, 1);
	vm_page_lock_queues();
	vm_page_dirty(m);
	vm_page_dontneed(m);
	vm_page_unlock_queues();
	vm_page_wakeup(m);
	vm_pager_page_unswapped(m);
}

/*
 *	swap_pager_swapoff:
 *
 *	Page in all of the pages that have been paged out to the
 *	given device.  The corresponding blocks in the bitmap must be
 *	marked as allocated and the device must be flagged SW_CLOSING.
 *	There may be no processes swapped out to the device.
 *
 *	This routine may block.
 */
static void
swap_pager_swapoff(struct swdevt *sp)
{
	struct swblock *swap;
	int i, j, retries;

	GIANT_REQUIRED;

	retries = 0;
full_rescan:
	mtx_lock(&swhash_mtx);
	for (i = 0; i <= swhash_mask; i++) { /* '<=' is correct here */
restart:
		for (swap = swhash[i]; swap != NULL; swap = swap->swb_hnext) {
			vm_object_t object = swap->swb_object;
			vm_pindex_t pindex = swap->swb_index;
                        for (j = 0; j < SWAP_META_PAGES; ++j) {
                                if (swp_pager_isondev(swap->swb_pages[j], sp)) {
					/* avoid deadlock */
					if (!VM_OBJECT_TRYLOCK(object)) {
						break;
					} else {
						mtx_unlock(&swhash_mtx);
						swp_pager_force_pagein(object,
						    pindex + j);
						VM_OBJECT_UNLOCK(object);
						mtx_lock(&swhash_mtx);
						goto restart;
					}
				}
                        }
		}
	}
	mtx_unlock(&swhash_mtx);
	if (sp->sw_used) {
		/*
		 * Objects may be locked or paging to the device being
		 * removed, so we will miss their pages and need to
		 * make another pass.  We have marked this device as
		 * SW_CLOSING, so the activity should finish soon.
		 */
		retries++;
		if (retries > 100) {
			panic("swapoff: failed to locate %d swap blocks",
			    sp->sw_used);
		}
		pause("swpoff", hz / 20);
		goto full_rescan;
	}
}

/************************************************************************
 *				SWAP META DATA 				*
 ************************************************************************
 *
 *	These routines manipulate the swap metadata stored in the 
 *	OBJT_SWAP object.
 *
 *	Swap metadata is implemented with a global hash and not directly
 *	linked into the object.  Instead the object simply contains
 *	appropriate tracking counters.
 */

/*
 * SWP_PAGER_META_BUILD() -	add swap block to swap meta data for object
 *
 *	We first convert the object to a swap object if it is a default
 *	object.
 *
 *	The specified swapblk is added to the object's swap metadata.  If
 *	the swapblk is not valid, it is freed instead.  Any previously
 *	assigned swapblk is freed.
 */
static void
swp_pager_meta_build(vm_object_t object, vm_pindex_t pindex, daddr_t swapblk)
{
	struct swblock *swap;
	struct swblock **pswap;
	int idx;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	/*
	 * Convert default object to swap object if necessary
	 */
	if (object->type != OBJT_SWAP) {
		object->type = OBJT_SWAP;
		object->un_pager.swp.swp_bcount = 0;

		if (object->handle != NULL) {
			mtx_lock(&sw_alloc_mtx);
			TAILQ_INSERT_TAIL(
			    NOBJLIST(object->handle),
			    object, 
			    pager_object_list
			);
			mtx_unlock(&sw_alloc_mtx);
		}
	}
	
	/*
	 * Locate hash entry.  If not found create, but if we aren't adding
	 * anything just return.  If we run out of space in the map we wait
	 * and, since the hash table may have changed, retry.
	 */
retry:
	mtx_lock(&swhash_mtx);
	pswap = swp_pager_hash(object, pindex);

	if ((swap = *pswap) == NULL) {
		int i;

		if (swapblk == SWAPBLK_NONE)
			goto done;

		swap = *pswap = uma_zalloc(swap_zone, M_NOWAIT);
		if (swap == NULL) {
			mtx_unlock(&swhash_mtx);
			VM_OBJECT_UNLOCK(object);
			if (uma_zone_exhausted(swap_zone)) {
				printf("swap zone exhausted, increase kern.maxswzone\n");
				vm_pageout_oom(VM_OOM_SWAPZ);
				pause("swzonex", 10);
			} else
				VM_WAIT;
			VM_OBJECT_LOCK(object);
			goto retry;
		}

		swap->swb_hnext = NULL;
		swap->swb_object = object;
		swap->swb_index = pindex & ~(vm_pindex_t)SWAP_META_MASK;
		swap->swb_count = 0;

		++object->un_pager.swp.swp_bcount;

		for (i = 0; i < SWAP_META_PAGES; ++i)
			swap->swb_pages[i] = SWAPBLK_NONE;
	}

	/*
	 * Delete prior contents of metadata
	 */
	idx = pindex & SWAP_META_MASK;

	if (swap->swb_pages[idx] != SWAPBLK_NONE) {
		swp_pager_freeswapspace(swap->swb_pages[idx], 1);
		--swap->swb_count;
	}

	/*
	 * Enter block into metadata
	 */
	swap->swb_pages[idx] = swapblk;
	if (swapblk != SWAPBLK_NONE)
		++swap->swb_count;
done:
	mtx_unlock(&swhash_mtx);
}

/*
 * SWP_PAGER_META_FREE() - free a range of blocks in the object's swap metadata
 *
 *	The requested range of blocks is freed, with any associated swap 
 *	returned to the swap bitmap.
 *
 *	This routine will free swap metadata structures as they are cleaned 
 *	out.  This routine does *NOT* operate on swap metadata associated
 *	with resident pages.
 */
static void
swp_pager_meta_free(vm_object_t object, vm_pindex_t index, daddr_t count)
{

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if (object->type != OBJT_SWAP)
		return;

	while (count > 0) {
		struct swblock **pswap;
		struct swblock *swap;

		mtx_lock(&swhash_mtx);
		pswap = swp_pager_hash(object, index);

		if ((swap = *pswap) != NULL) {
			daddr_t v = swap->swb_pages[index & SWAP_META_MASK];

			if (v != SWAPBLK_NONE) {
				swp_pager_freeswapspace(v, 1);
				swap->swb_pages[index & SWAP_META_MASK] =
					SWAPBLK_NONE;
				if (--swap->swb_count == 0) {
					*pswap = swap->swb_hnext;
					uma_zfree(swap_zone, swap);
					--object->un_pager.swp.swp_bcount;
				}
			}
			--count;
			++index;
		} else {
			int n = SWAP_META_PAGES - (index & SWAP_META_MASK);
			count -= n;
			index += n;
		}
		mtx_unlock(&swhash_mtx);
	}
}

/*
 * SWP_PAGER_META_FREE_ALL() - destroy all swap metadata associated with object
 *
 *	This routine locates and destroys all swap metadata associated with
 *	an object.
 */
static void
swp_pager_meta_free_all(vm_object_t object)
{
	daddr_t index = 0;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	if (object->type != OBJT_SWAP)
		return;

	while (object->un_pager.swp.swp_bcount) {
		struct swblock **pswap;
		struct swblock *swap;

		mtx_lock(&swhash_mtx);
		pswap = swp_pager_hash(object, index);
		if ((swap = *pswap) != NULL) {
			int i;

			for (i = 0; i < SWAP_META_PAGES; ++i) {
				daddr_t v = swap->swb_pages[i];
				if (v != SWAPBLK_NONE) {
					--swap->swb_count;
					swp_pager_freeswapspace(v, 1);
				}
			}
			if (swap->swb_count != 0)
				panic("swap_pager_meta_free_all: swb_count != 0");
			*pswap = swap->swb_hnext;
			uma_zfree(swap_zone, swap);
			--object->un_pager.swp.swp_bcount;
		}
		mtx_unlock(&swhash_mtx);
		index += SWAP_META_PAGES;
	}
}

/*
 * SWP_PAGER_METACTL() -  misc control of swap and vm_page_t meta data.
 *
 *	This routine is capable of looking up, popping, or freeing
 *	swapblk assignments in the swap meta data or in the vm_page_t.
 *	The routine typically returns the swapblk being looked-up, or popped,
 *	or SWAPBLK_NONE if the block was freed, or SWAPBLK_NONE if the block
 *	was invalid.  This routine will automatically free any invalid 
 *	meta-data swapblks.
 *
 *	It is not possible to store invalid swapblks in the swap meta data
 *	(other then a literal 'SWAPBLK_NONE'), so we don't bother checking.
 *
 *	When acting on a busy resident page and paging is in progress, we 
 *	have to wait until paging is complete but otherwise can act on the 
 *	busy page.
 *
 *	SWM_FREE	remove and free swap block from metadata
 *	SWM_POP		remove from meta data but do not free.. pop it out
 */
static daddr_t
swp_pager_meta_ctl(vm_object_t object, vm_pindex_t pindex, int flags)
{
	struct swblock **pswap;
	struct swblock *swap;
	daddr_t r1;
	int idx;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	/*
	 * The meta data only exists of the object is OBJT_SWAP 
	 * and even then might not be allocated yet.
	 */
	if (object->type != OBJT_SWAP)
		return (SWAPBLK_NONE);

	r1 = SWAPBLK_NONE;
	mtx_lock(&swhash_mtx);
	pswap = swp_pager_hash(object, pindex);

	if ((swap = *pswap) != NULL) {
		idx = pindex & SWAP_META_MASK;
		r1 = swap->swb_pages[idx];

		if (r1 != SWAPBLK_NONE) {
			if (flags & SWM_FREE) {
				swp_pager_freeswapspace(r1, 1);
				r1 = SWAPBLK_NONE;
			}
			if (flags & (SWM_FREE|SWM_POP)) {
				swap->swb_pages[idx] = SWAPBLK_NONE;
				if (--swap->swb_count == 0) {
					*pswap = swap->swb_hnext;
					uma_zfree(swap_zone, swap);
					--object->un_pager.swp.swp_bcount;
				}
			} 
		}
	}
	mtx_unlock(&swhash_mtx);
	return (r1);
}

/*
 * System call swapon(name) enables swapping on device name,
 * which must be in the swdevsw.  Return EBUSY
 * if already swapping on this device.
 */
#ifndef _SYS_SYSPROTO_H_
struct swapon_args {
	char *name;
};
#endif

/* 
 * MPSAFE
 */
/* ARGSUSED */
int
swapon(struct thread *td, struct swapon_args *uap)
{
	struct vattr attr;
	struct vnode *vp;
	struct nameidata nd;
	int error;

	error = priv_check(td, PRIV_SWAPON);
	if (error)
		return (error);

	mtx_lock(&Giant);
	while (swdev_syscall_active)
	    tsleep(&swdev_syscall_active, PUSER - 1, "swpon", 0);
	swdev_syscall_active = 1;

	/*
	 * Swap metadata may not fit in the KVM if we have physical
	 * memory of >1GB.
	 */
	if (swap_zone == NULL) {
		error = ENOMEM;
		goto done;
	}

	NDINIT(&nd, LOOKUP, ISOPEN | FOLLOW | AUDITVNODE1, UIO_USERSPACE,
	    uap->name, td);
	error = namei(&nd);
	if (error)
		goto done;

	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;

	if (vn_isdisk(vp, &error)) {
		error = swapongeom(td, vp);
	} else if (vp->v_type == VREG &&
	    (vp->v_mount->mnt_vfc->vfc_flags & VFCF_NETWORK) != 0 &&
	    (error = VOP_GETATTR(vp, &attr, td->td_ucred)) == 0) {
		/*
		 * Allow direct swapping to NFS regular files in the same
		 * way that nfs_mountroot() sets up diskless swapping.
		 */
		error = swaponvp(td, vp, attr.va_size / DEV_BSIZE);
	}

	if (error)
		vrele(vp);
done:
	swdev_syscall_active = 0;
	wakeup_one(&swdev_syscall_active);
	mtx_unlock(&Giant);
	return (error);
}

static void
swaponsomething(struct vnode *vp, void *id, u_long nblks, sw_strategy_t *strategy, sw_close_t *close, dev_t dev)
{
	struct swdevt *sp, *tsp;
	swblk_t dvbase;
	u_long mblocks;

	/*
	 * nblks is in DEV_BSIZE'd chunks, convert to PAGE_SIZE'd chunks.
	 * First chop nblks off to page-align it, then convert.
	 * 
	 * sw->sw_nblks is in page-sized chunks now too.
	 */
	nblks &= ~(ctodb(1) - 1);
	nblks = dbtoc(nblks);

	/*
	 * If we go beyond this, we get overflows in the radix
	 * tree bitmap code.
	 */
	mblocks = 0x40000000 / BLIST_META_RADIX;
	if (nblks > mblocks) {
		printf(
    "WARNING: reducing swap size to maximum of %luMB per unit\n",
		    mblocks / 1024 / 1024 * PAGE_SIZE);
		nblks = mblocks;
	}

	sp = malloc(sizeof *sp, M_VMPGDATA, M_WAITOK | M_ZERO);
	sp->sw_vp = vp;
	sp->sw_id = id;
	sp->sw_dev = dev;
	sp->sw_flags = 0;
	sp->sw_nblks = nblks;
	sp->sw_used = 0;
	sp->sw_strategy = strategy;
	sp->sw_close = close;

	sp->sw_blist = blist_create(nblks, M_WAITOK);
	/*
	 * Do not free the first two block in order to avoid overwriting
	 * any bsd label at the front of the partition
	 */
	blist_free(sp->sw_blist, 2, nblks - 2);

	dvbase = 0;
	mtx_lock(&sw_dev_mtx);
	TAILQ_FOREACH(tsp, &swtailq, sw_list) {
		if (tsp->sw_end >= dvbase) {
			/*
			 * We put one uncovered page between the devices
			 * in order to definitively prevent any cross-device
			 * I/O requests
			 */
			dvbase = tsp->sw_end + 1;
		}
	}
	sp->sw_first = dvbase;
	sp->sw_end = dvbase + nblks;
	TAILQ_INSERT_TAIL(&swtailq, sp, sw_list);
	nswapdev++;
	swap_pager_avail += nblks;
	swap_total += (vm_ooffset_t)nblks * PAGE_SIZE;
	swp_sizecheck();
	mtx_unlock(&sw_dev_mtx);
}

/*
 * SYSCALL: swapoff(devname)
 *
 * Disable swapping on the given device.
 *
 * XXX: Badly designed system call: it should use a device index
 * rather than filename as specification.  We keep sw_vp around
 * only to make this work.
 */
#ifndef _SYS_SYSPROTO_H_
struct swapoff_args {
	char *name;
};
#endif

/*
 * MPSAFE
 */
/* ARGSUSED */
int
swapoff(struct thread *td, struct swapoff_args *uap)
{
	struct vnode *vp;
	struct nameidata nd;
	struct swdevt *sp;
	int error;

	error = priv_check(td, PRIV_SWAPOFF);
	if (error)
		return (error);

	mtx_lock(&Giant);
	while (swdev_syscall_active)
	    tsleep(&swdev_syscall_active, PUSER - 1, "swpoff", 0);
	swdev_syscall_active = 1;

	NDINIT(&nd, LOOKUP, FOLLOW | AUDITVNODE1, UIO_USERSPACE, uap->name,
	    td);
	error = namei(&nd);
	if (error)
		goto done;
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;

	mtx_lock(&sw_dev_mtx);
	TAILQ_FOREACH(sp, &swtailq, sw_list) {
		if (sp->sw_vp == vp)
			break;
	}
	mtx_unlock(&sw_dev_mtx);
	if (sp == NULL) {
		error = EINVAL;
		goto done;
	}
	error = swapoff_one(sp, td->td_ucred);
done:
	swdev_syscall_active = 0;
	wakeup_one(&swdev_syscall_active);
	mtx_unlock(&Giant);
	return (error);
}

static int
swapoff_one(struct swdevt *sp, struct ucred *cred)
{
	u_long nblks, dvbase;
#ifdef MAC
	int error;
#endif

	mtx_assert(&Giant, MA_OWNED);
#ifdef MAC
	(void) vn_lock(sp->sw_vp, LK_EXCLUSIVE | LK_RETRY);
	error = mac_system_check_swapoff(cred, sp->sw_vp);
	(void) VOP_UNLOCK(sp->sw_vp, 0);
	if (error != 0)
		return (error);
#endif
	nblks = sp->sw_nblks;

	/*
	 * We can turn off this swap device safely only if the
	 * available virtual memory in the system will fit the amount
	 * of data we will have to page back in, plus an epsilon so
	 * the system doesn't become critically low on swap space.
	 */
	if (cnt.v_free_count + cnt.v_cache_count + swap_pager_avail <
	    nblks + nswap_lowat) {
		return (ENOMEM);
	}

	/*
	 * Prevent further allocations on this device.
	 */
	mtx_lock(&sw_dev_mtx);
	sp->sw_flags |= SW_CLOSING;
	for (dvbase = 0; dvbase < sp->sw_end; dvbase += dmmax) {
		swap_pager_avail -= blist_fill(sp->sw_blist,
		     dvbase, dmmax);
	}
	swap_total -= (vm_ooffset_t)nblks * PAGE_SIZE;
	mtx_unlock(&sw_dev_mtx);

	/*
	 * Page in the contents of the device and close it.
	 */
	swap_pager_swapoff(sp);

	sp->sw_close(curthread, sp);
	sp->sw_id = NULL;
	mtx_lock(&sw_dev_mtx);
	TAILQ_REMOVE(&swtailq, sp, sw_list);
	nswapdev--;
	if (nswapdev == 0) {
		swap_pager_full = 2;
		swap_pager_almost_full = 1;
	}
	if (swdevhd == sp)
		swdevhd = NULL;
	mtx_unlock(&sw_dev_mtx);
	blist_destroy(sp->sw_blist);
	free(sp, M_VMPGDATA);
	return (0);
}

void
swapoff_all(void)
{
	struct swdevt *sp, *spt;
	const char *devname;
	int error;
 
	mtx_lock(&Giant);
	while (swdev_syscall_active)
		tsleep(&swdev_syscall_active, PUSER - 1, "swpoff", 0);
	swdev_syscall_active = 1;
 
	mtx_lock(&sw_dev_mtx);
	TAILQ_FOREACH_SAFE(sp, &swtailq, sw_list, spt) {
		mtx_unlock(&sw_dev_mtx);
		if (vn_isdisk(sp->sw_vp, NULL))
			devname = sp->sw_vp->v_rdev->si_name;
		else
			devname = "[file]";
		error = swapoff_one(sp, thread0.td_ucred);
		if (error != 0) {
			printf("Cannot remove swap device %s (error=%d), "
			    "skipping.\n", devname, error);
		} else if (bootverbose) {
			printf("Swap device %s removed.\n", devname);
		}
		mtx_lock(&sw_dev_mtx);
	}
	mtx_unlock(&sw_dev_mtx);
 
	swdev_syscall_active = 0;
	wakeup_one(&swdev_syscall_active);
	mtx_unlock(&Giant);
}

void
swap_pager_status(int *total, int *used)
{
	struct swdevt *sp;

	*total = 0;
	*used = 0;
	mtx_lock(&sw_dev_mtx);
	TAILQ_FOREACH(sp, &swtailq, sw_list) {
		*total += sp->sw_nblks;
		*used += sp->sw_used;
	}
	mtx_unlock(&sw_dev_mtx);
}

int
swap_dev_info(int name, struct xswdev *xs, char *devname, size_t len)
{
	struct swdevt *sp;
	char *tmp_devname;
	int error, n;

	n = 0;
	error = ENOENT;
	mtx_lock(&sw_dev_mtx);
	TAILQ_FOREACH(sp, &swtailq, sw_list) {
		if (n != name) {
			n++;
			continue;
		}
		xs->xsw_version = XSWDEV_VERSION;
		xs->xsw_dev = sp->sw_dev;
		xs->xsw_flags = sp->sw_flags;
		xs->xsw_nblks = sp->sw_nblks;
		xs->xsw_used = sp->sw_used;
		if (devname != NULL) {
			if (vn_isdisk(sp->sw_vp, NULL))
				tmp_devname = sp->sw_vp->v_rdev->si_name;
			else
				tmp_devname = "[file]";
			strncpy(devname, tmp_devname, len);
		}
		error = 0;
		break;
	}
	mtx_unlock(&sw_dev_mtx);
	return (error);
}

static int
sysctl_vm_swap_info(SYSCTL_HANDLER_ARGS)
{
	struct xswdev xs;
	int error;

	if (arg2 != 1)			/* name length */
		return (EINVAL);
	error = swap_dev_info(*(int *)arg1, &xs, NULL, 0);
	if (error != 0)
		return (error);
	error = SYSCTL_OUT(req, &xs, sizeof(xs));
	return (error);
}

SYSCTL_INT(_vm, OID_AUTO, nswapdev, CTLFLAG_RD, &nswapdev, 0,
    "Number of swap devices");
SYSCTL_NODE(_vm, OID_AUTO, swap_info, CTLFLAG_RD, sysctl_vm_swap_info,
    "Swap statistics by device");

/*
 * vmspace_swap_count() - count the approximate swap usage in pages for a
 *			  vmspace.
 *
 *	The map must be locked.
 *
 *	Swap usage is determined by taking the proportional swap used by
 *	VM objects backing the VM map.  To make up for fractional losses,
 *	if the VM object has any swap use at all the associated map entries
 *	count for at least 1 swap page.
 */
long
vmspace_swap_count(struct vmspace *vmspace)
{
	vm_map_t map;
	vm_map_entry_t cur;
	vm_object_t object;
	long count, n;

	map = &vmspace->vm_map;
	count = 0;

	for (cur = map->header.next; cur != &map->header; cur = cur->next) {
		if ((cur->eflags & MAP_ENTRY_IS_SUB_MAP) == 0 &&
		    (object = cur->object.vm_object) != NULL) {
			VM_OBJECT_LOCK(object);
			if (object->type == OBJT_SWAP &&
			    object->un_pager.swp.swp_bcount != 0) {
				n = (cur->end - cur->start) / PAGE_SIZE;
				count += object->un_pager.swp.swp_bcount *
				    SWAP_META_PAGES * n / object->size + 1;
			}
			VM_OBJECT_UNLOCK(object);
		}
	}
	return (count);
}

/*
 * GEOM backend
 *
 * Swapping onto disk devices.
 *
 */

static g_orphan_t swapgeom_orphan;

static struct g_class g_swap_class = {
	.name = "SWAP",
	.version = G_VERSION,
	.orphan = swapgeom_orphan,
};

DECLARE_GEOM_CLASS(g_swap_class, g_class);


static void
swapgeom_done(struct bio *bp2)
{
	struct buf *bp;

	bp = bp2->bio_caller2;
	bp->b_ioflags = bp2->bio_flags;
	if (bp2->bio_error)
		bp->b_ioflags |= BIO_ERROR;
	bp->b_resid = bp->b_bcount - bp2->bio_completed;
	bp->b_error = bp2->bio_error;
	bufdone(bp);
	g_destroy_bio(bp2);
}

static void
swapgeom_strategy(struct buf *bp, struct swdevt *sp)
{
	struct bio *bio;
	struct g_consumer *cp;

	cp = sp->sw_id;
	if (cp == NULL) {
		bp->b_error = ENXIO;
		bp->b_ioflags |= BIO_ERROR;
		bufdone(bp);
		return;
	}
	if (bp->b_iocmd == BIO_WRITE)
		bio = g_new_bio();
	else
		bio = g_alloc_bio();
	if (bio == NULL) {
		bp->b_error = ENOMEM;
		bp->b_ioflags |= BIO_ERROR;
		bufdone(bp);
		return;
	}

	bio->bio_caller2 = bp;
	bio->bio_cmd = bp->b_iocmd;
	bio->bio_data = bp->b_data;
	bio->bio_offset = (bp->b_blkno - sp->sw_first) * PAGE_SIZE;
	bio->bio_length = bp->b_bcount;
	bio->bio_done = swapgeom_done;
	g_io_request(bio, cp);
	return;
}

static void
swapgeom_orphan(struct g_consumer *cp)
{
	struct swdevt *sp;

	mtx_lock(&sw_dev_mtx);
	TAILQ_FOREACH(sp, &swtailq, sw_list)
		if (sp->sw_id == cp)
			sp->sw_id = NULL;
	mtx_unlock(&sw_dev_mtx);
}

static void
swapgeom_close_ev(void *arg, int flags)
{
	struct g_consumer *cp;

	cp = arg;
	g_access(cp, -1, -1, 0);
	g_detach(cp);
	g_destroy_consumer(cp);
}

static void
swapgeom_close(struct thread *td, struct swdevt *sw)
{

	/* XXX: direct call when Giant untangled */
	g_waitfor_event(swapgeom_close_ev, sw->sw_id, M_WAITOK, NULL);
}


struct swh0h0 {
	struct cdev *dev;
	struct vnode *vp;
	int	error;
};

static void
swapongeom_ev(void *arg, int flags)
{
	struct swh0h0 *swh;
	struct g_provider *pp;
	struct g_consumer *cp;
	static struct g_geom *gp;
	struct swdevt *sp;
	u_long nblks;
	int error;

	swh = arg;
	swh->error = 0;
	pp = g_dev_getprovider(swh->dev);
	if (pp == NULL) {
		swh->error = ENODEV;
		return;
	}
	mtx_lock(&sw_dev_mtx);
	TAILQ_FOREACH(sp, &swtailq, sw_list) {
		cp = sp->sw_id;
		if (cp != NULL && cp->provider == pp) {
			mtx_unlock(&sw_dev_mtx);
			swh->error = EBUSY;
			return;
		}
	}
	mtx_unlock(&sw_dev_mtx);
	if (gp == NULL)
		gp = g_new_geomf(&g_swap_class, "swap", NULL);
	cp = g_new_consumer(gp);
	g_attach(cp, pp);
	/*
	 * XXX: Everytime you think you can improve the margin for
	 * footshooting, somebody depends on the ability to do so:
	 * savecore(8) wants to write to our swapdev so we cannot
	 * set an exclusive count :-(
	 */
	error = g_access(cp, 1, 1, 0);
	if (error) {
		g_detach(cp);
		g_destroy_consumer(cp);
		swh->error = error;
		return;
	}
	nblks = pp->mediasize / DEV_BSIZE;
	swaponsomething(swh->vp, cp, nblks, swapgeom_strategy,
	    swapgeom_close, dev2udev(swh->dev));
	swh->error = 0;
	return;
}

static int
swapongeom(struct thread *td, struct vnode *vp)
{
	int error;
	struct swh0h0 swh;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	swh.dev = vp->v_rdev;
	swh.vp = vp;
	swh.error = 0;
	/* XXX: direct call when Giant untangled */
	error = g_waitfor_event(swapongeom_ev, &swh, M_WAITOK, NULL);
	if (!error)
		error = swh.error;
	VOP_UNLOCK(vp, 0);
	return (error);
}

/*
 * VNODE backend
 *
 * This is used mainly for network filesystem (read: probably only tested
 * with NFS) swapfiles.
 *
 */

static void
swapdev_strategy(struct buf *bp, struct swdevt *sp)
{
	struct vnode *vp2;

	bp->b_blkno = ctodb(bp->b_blkno - sp->sw_first);

	vp2 = sp->sw_id;
	vhold(vp2);
	if (bp->b_iocmd == BIO_WRITE) {
		if (bp->b_bufobj)
			bufobj_wdrop(bp->b_bufobj);
		bufobj_wref(&vp2->v_bufobj);
	}
	if (bp->b_bufobj != &vp2->v_bufobj)
		bp->b_bufobj = &vp2->v_bufobj;
	bp->b_vp = vp2;
	bp->b_iooffset = dbtob(bp->b_blkno);
	bstrategy(bp);
	return;
}

static void
swapdev_close(struct thread *td, struct swdevt *sp)
{

	VOP_CLOSE(sp->sw_vp, FREAD | FWRITE, td->td_ucred, td);
	vrele(sp->sw_vp);
}


static int
swaponvp(struct thread *td, struct vnode *vp, u_long nblks)
{
	struct swdevt *sp;
	int error;

	if (nblks == 0)
		return (ENXIO);
	mtx_lock(&sw_dev_mtx);
	TAILQ_FOREACH(sp, &swtailq, sw_list) {
		if (sp->sw_id == vp) {
			mtx_unlock(&sw_dev_mtx);
			return (EBUSY);
		}
	}
	mtx_unlock(&sw_dev_mtx);
    
	(void) vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
#ifdef MAC
	error = mac_system_check_swapon(td->td_ucred, vp);
	if (error == 0)
#endif
		error = VOP_OPEN(vp, FREAD | FWRITE, td->td_ucred, td, NULL);
	(void) VOP_UNLOCK(vp, 0);
	if (error)
		return (error);

	swaponsomething(vp, vp, nblks, swapdev_strategy, swapdev_close,
	    NODEV);
	return (0);
}
