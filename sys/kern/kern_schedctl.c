/*-
 * Copyright (c) 2013 Davide Italiano <davide@FreeBSD.org>
 * All rights reserved.
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/rwlock.h>
#include <sys/schedctl.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/uma.h>

#include <machine/cpufunc.h>

static uma_zone_t shpage_zone;		/* Zone from which allocate structures */
static size_t	avail_pagesize;		/* size of usable page on page */
static size_t	bitmap_len;		/* # of bits in allocation bitmap */

/*
 * Public prototypes.
 */
int	schedctl(struct thread *td, struct schedctl_args *uap);

static int
schedctl_alloc_page(struct proc *p, shpage_t **ret)
{
	vm_map_t map;
	vm_page_t m;
	shpage_t *spg;
	int error;

	spg = uma_zalloc(shpage_zone, M_ZERO | M_WAITOK);
	spg->bitmap = ~0;
	spg->available = bitmap_len;

	spg->shared_page_obj = vm_pager_allocate(OBJT_PHYS, 0, PAGE_SIZE,
	    VM_PROT_DEFAULT, 0, NULL);
	VM_OBJECT_WLOCK(spg->shared_page_obj);
	m = vm_page_grab(spg->shared_page_obj, 0, VM_ALLOC_RETRY |
	    VM_ALLOC_NOBUSY | VM_ALLOC_ZERO);
	m->valid = VM_PAGE_BITS_ALL;
	VM_OBJECT_WUNLOCK(spg->shared_page_obj);
	spg->pageaddr = kva_alloc(PAGE_SIZE);
	pmap_qenter(spg->pageaddr, &m, 1);

	/* Map in userspace */
	PROC_LOCK(p);
	SLIST_INSERT_HEAD(&(p->p_shpg), spg, pg_next);
	PROC_UNLOCK(p);
	map = &p->p_vmspace->vm_map;
	vm_object_reference(spg->shared_page_obj);

	/* XXX: is really VMFS_ANY_SPACE the right policy? */
	error = vm_map_find(map, spg->shared_page_obj, 0, &spg->usraddr,
	    PAGE_SIZE, VMFS_ANY_SPACE, VM_PROT_READ | VM_PROT_WRITE,
	    VM_PROT_READ | VM_PROT_WRITE, 0);
	if (error) {
		vm_object_deallocate(spg->shared_page_obj);
		return (error);
	}
	*ret = spg;
	return (0);
}

static int
schedctl_bmp_findfirst(shpage_t *shpg)
{
	int idx;

	/* Find first bit set in the bitmap */
	idx = ffsl(shpg->bitmap) - 1;
	if (idx > bitmap_len)
		return (-1);
	shpg->available--;
	clrbit(&(shpg->bitmap), idx);
	return (idx);
}

static int
schedctl_shared_alloc(struct thread *td, vm_offset_t *usroff,
    vm_offset_t *krnoff)
{
	struct proc *p;
	shpage_t *sh_pg;
	u_int idx;
	int error;

	sh_pg = NULL;
	p = curproc;
	KASSERT(p != NULL, ("proc should never be NULL"));
	if (!SLIST_EMPTY(&(p->p_shpg))) {

		/*
		 * Iterate over the set of shared pages allocated for the process.
		 * If this process hasn't pages allocated or they're all completely
		 * full, allocate a new one.
		 */
		SLIST_FOREACH(sh_pg, &(p->p_shpg), pg_next) {
			KASSERT(sh_pg->available >= 0, ("invalid shared page counter"));
			if (sh_pg->available > 0)
				break;
		}
	}
	if (sh_pg == NULL) {
		error = schedctl_alloc_page(p, &sh_pg);
		if (error != 0)
			return (ENOMEM);
	}

	/* Now were's (mostly) sure there's room for allocation. */
	KASSERT(sh_pg != NULL && sh_pg->available > 0,
	    ("schedctl_page_alloc: null shpg"));
	idx = schedctl_bmp_findfirst(sh_pg);
	KASSERT(idx != -1, ("schedctl_page_alloc: invalid bitmap index"));
	*usroff = sh_pg->usraddr + (sizeof(shstate_t) * idx);
	*krnoff = sh_pg->pageaddr + (sizeof(shstate_t) * idx);
	return (0);
}

static void
kern_schedctl(struct thread *td)
{
	vm_offset_t usroff, krnoff;

	if (td->td_schedctl == NULL) {
		if (schedctl_shared_alloc(td, &usroff, &krnoff) != 0)
			panic("schedctl: impossible set structure for thread");
		td->td_schedctl = (shstate_t *)krnoff;
		td->td_schedctl->sh_state = STATE_ONCPU;
		td->td_usrschedctl = (shstate_t *)usroff;
	}
}

int
sys_schedctl(struct thread *td, struct schedctl_args *uap)
{

	KASSERT(td != NULL, ("td should never be NULL"));
	kern_schedctl(td);
	td->td_retval[0] = (register_t)(td->td_usrschedctl);
	return (0);	
}

void
schedctl_init(void)
{

	avail_pagesize = PAGE_SIZE - (PAGE_SIZE % sizeof(shstate_t));
	bitmap_len = avail_pagesize / sizeof(shstate_t);
	shpage_zone = uma_zcreate("schedctl structures", sizeof(shpage_t), NULL,
	    NULL, NULL, NULL, 0, 0);
}

/*
 * XXX: SI_SUB_KMEM is the right place to call schedctl_init?
 */
SYSINIT(schedctl, SI_SUB_KMEM, SI_ORDER_ANY, schedctl_init, NULL);
