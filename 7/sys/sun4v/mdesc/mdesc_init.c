/*-
 * Copyright (c) 2006 Kip Macy
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/hypervisorvar.h>
#include <machine/hv_api.h>
#include <machine/cddl/mdesc.h>
#include <machine/cddl/mdesc_impl.h>


static void mdesc_postvm_init(void *);
/*
 * It doesn't really matter when this happens right now
 * it just needs to happen before device initialization 
 * and after VM initialization. Later on we will end up 
 * doing this statically from pmap_bootstrap so that we 
 * can kill off all calls to OBP. OBP removal is not in
 * the critical path for sun4v at this time.
 */
SYSINIT(mdesc_init, SI_SUB_CPU, SI_ORDER_SECOND, mdesc_postvm_init, NULL);


#define UNIMPLEMENTED panic("%s not implemented.", __FUNCTION__)

typedef struct mdesc_memops_ {
        void            *(*mm_buf_alloc)(size_t size, size_t align);
        void            (*mm_buf_free)(void *, size_t size);
        void            *(*mm_meta_alloc)(size_t size);
        void            (*mm_meta_free)(void *, size_t size);
} mdesc_memops_t;

typedef struct mdesc_ {
        uint64_t        md_gen;         /* md-generation# */
        struct mtx      md_lock;       
        void           *md_addr;        /* address of raw MD */
        uint64_t        md_size;        /* size of raw MD */
        uint64_t        md_buf_size;    /* size of buffer allocated for MD */
        int             md_refcount;    /* reference count */
        mdesc_memops_t *md_memops;      /* Memory operations for this MD */
} mdesc_t;



static mdesc_t        *curr_mdesc = NULL;
static struct mtx      curr_mdesc_lock;
static mdesc_memops_t *mdesc_memops;

static void *mdesc_boot_buf_alloc(size_t size, size_t align);
static void mdesc_boot_buf_free(void *buf, size_t align);
static void *mdesc_boot_meta_alloc(size_t size);
static void mdesc_boot_meta_free(void *buf, size_t size);

static void *mdesc_buf_alloc(size_t size, size_t align);
static void mdesc_buf_free(void *buf, size_t align);
static void *mdesc_meta_alloc(size_t size);
static void mdesc_meta_free(void *buf, size_t size);

static mdesc_memops_t mdesc_boot_memops = {
	mdesc_boot_buf_alloc,
	mdesc_boot_buf_free,
	mdesc_boot_meta_alloc,
	mdesc_boot_meta_free,
};

static mdesc_memops_t mdesc_generic_memops = {
	mdesc_buf_alloc,
	mdesc_buf_free,
	mdesc_meta_alloc,
	mdesc_meta_free,
};

static void *
mdesc_boot_buf_alloc(size_t size, size_t align)
{
	UNIMPLEMENTED;
}

static void 
mdesc_boot_buf_free(void *buf, size_t align)
{
	UNIMPLEMENTED;
}

static void *
mdesc_boot_meta_alloc(size_t size)
{
	UNIMPLEMENTED;
}

static void 
mdesc_boot_meta_free(void *buf, size_t size)
{
	UNIMPLEMENTED;
}

static void *
mdesc_buf_alloc(size_t size, size_t align)
{
	return pmap_alloc_zeroed_contig_pages(size/PAGE_SIZE, align);
}

static void 
mdesc_buf_free(void *buf, size_t align)
{
	contigfree(buf, PAGE_SIZE /*fix me*/, M_MDPROP);
}

static void *
mdesc_meta_alloc(size_t size)
{
	return malloc(size, M_MDPROP, M_NOWAIT);
}

static void 
mdesc_meta_free(void *buf, size_t size)
{
	free(buf, M_MDPROP);
}

void 
mdesc_init(void)
{

	mtx_init(&curr_mdesc_lock, "current machine description lock", NULL, MTX_DEF);
	mdesc_memops = &mdesc_boot_memops;
}

static void 
mdesc_postvm_init(void *unused)
{
	mdesc_memops = &mdesc_generic_memops;
}

static mdesc_t *
mdesc_alloc(void)
{
	mdesc_t *mdp;
	
	mdp = (mdesc_t *)(mdesc_memops->mm_meta_alloc)(sizeof(mdesc_t));
	if (mdp != NULL) {
		bzero(mdp, sizeof(*mdp));
		mdp->md_memops = mdesc_memops;
		mtx_init(&mdp->md_lock, "machine descriptor lock", NULL, MTX_DEF);
	}
	
	return (mdp);
}

int 
mdesc_update(void)
{
	uint64_t rv;
	uint64_t mdesc_size, mdesc_buf_size = 0;
	void    *buf = NULL;
#ifdef notyet
	uint64_t gen;
#endif
	do {
		if (buf != NULL)
			(mdesc_memops->mm_buf_free)(buf, mdesc_buf_size);

		mdesc_size = 0LL;
		do {
			rv = hv_mach_desc((uint64_t)0, &mdesc_size);
			if (rv != H_EOK && rv != H_EINVAL)
				printf("retrying to fetch mdesc size\n");
		} while (rv != H_EOK && rv != H_EINVAL); 

		mdesc_size = mdesc_buf_size = round_page(mdesc_size);
		
		if ((buf = (*mdesc_memops->mm_buf_alloc)(mdesc_buf_size, PAGE_SIZE)) == NULL) {
			rv = ENOMEM;
			goto done;
		}
		
		rv = hv_mach_desc(vtophys(buf), &mdesc_size);

		if (rv != H_EOK && rv != H_EINVAL) {
			goto done;
		}
	} while (mdesc_size > mdesc_buf_size);
	
	KASSERT(rv == H_EOK, ("unexpected return from hv_mach_desc"));
	
	/* XXX we ignore the generation count... not all versions may 
	 * support it  
	 */

	if (curr_mdesc->md_refcount == 0) {
		(*mdesc_memops->mm_buf_free) (curr_mdesc->md_addr, curr_mdesc->md_size);
	} else {
		panic("out of date machine description list not implemented");
	}

	mtx_lock(&curr_mdesc_lock);
#ifdef notyet
	curr_mdesc->md_gen = gen;
#endif
	curr_mdesc->md_addr = buf;
	curr_mdesc->md_size = mdesc_size;
	curr_mdesc->md_buf_size = mdesc_buf_size;
	mtx_unlock(&curr_mdesc_lock);
	
	return (0);

 done:
	if (buf != NULL)
		(*mdesc_memops->mm_buf_free)(buf, mdesc_buf_size);
	return (rv);
}

md_t *
md_get(void)
{
	md_t *mdp;
	int rc;
	
	/*
	 * XXX This should actually happen in init
	 */
	if (curr_mdesc == NULL) {
		if ((curr_mdesc = mdesc_alloc()) == NULL)
			panic("machine description allocation failed");
		if ((rc = mdesc_update()) != 0)
			panic("machine description update failed: %d", rc);
	}
		
	mtx_lock(&curr_mdesc_lock);
	curr_mdesc->md_refcount++;
	mdp = md_init_intern(curr_mdesc->md_addr, 
			     curr_mdesc->md_memops->mm_meta_alloc,
			     curr_mdesc->md_memops->mm_meta_free);
	mtx_unlock(&curr_mdesc_lock);

	return (mdp);
}

void
md_put(md_t *ptr)
{
	md_impl_t *mdp;
	
	mdp = (md_impl_t *)ptr;

#ifdef notyet
	mtx_lock(&curr_mdesc_lock)

	/* XXX check against generation */
	if (curr_mdesc->md_gen == mdp->md_gen) {
		curr_mdesc->md_refcount--;
		mtx_unlock(&curr_mdesc_lock)
		goto done;
	}
	mtx_unlock(&curr_mdesc_lock);
	/*
	 * MD is on the out of date list 
	 */

 done:
#endif
	/*
	 * We don't keep an out of date list currently
	 */
	mtx_lock(&curr_mdesc_lock);
	curr_mdesc->md_refcount--;
	mtx_unlock(&curr_mdesc_lock);
	mdp->freep(mdp, sizeof(md_impl_t));
}


