/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>


#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/systm.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <cxgb_include.h>
#include <sys/mvec.h>

extern int cxgb_use_16k_clusters;
int cxgb_pcpu_cache_enable = 1;

struct buf_stack {
	caddr_t            *bs_stack;
	volatile int        bs_head;
	int                 bs_size;
};

static __inline int
buf_stack_push(struct buf_stack *bs, caddr_t buf)
{
	if (bs->bs_head + 1 >= bs->bs_size)
		return (ENOSPC);

	bs->bs_stack[++(bs->bs_head)] = buf;
	return (0);
}

static __inline caddr_t
buf_stack_pop(struct buf_stack *bs)
{
	if (bs->bs_head < 0)
		return (NULL);

	return (bs->bs_stack[(bs->bs_head)--]);
}

/*
 * Stack is full
 *
 */
static __inline int
buf_stack_avail(struct buf_stack *bs)
{
	return (bs->bs_size - bs->bs_head - 1);
}

struct cxgb_cache_pcpu {
	struct buf_stack        ccp_jumbo_free;
	struct buf_stack        ccp_cluster_free;
	uma_zone_t              ccp_jumbo_zone;
};

struct cxgb_cache_system {
	struct cxgb_cache_pcpu ccs_array[0];
} *cxgb_caches;

static int
buf_stack_init(struct buf_stack *bs, int size)
{
	bs->bs_size = size;
	bs->bs_head = -1;
	if((bs->bs_stack = malloc(sizeof(caddr_t)*size, M_DEVBUF, M_NOWAIT)) == NULL)
		return (ENOMEM);

	return (0);
}

static void
buf_stack_deinit(struct buf_stack *bs)
{
	if (bs->bs_stack != NULL)
		free(bs->bs_stack, M_DEVBUF);
}

static int
cxgb_cache_pcpu_init(struct cxgb_cache_pcpu *ccp)
{
	int err;
	
	if ((err = buf_stack_init(&ccp->ccp_jumbo_free, (JUMBO_Q_SIZE >> 2))))
		return (err);
	
	if ((err = buf_stack_init(&ccp->ccp_cluster_free, (FL_Q_SIZE >> 2))))
		return (err);

#if __FreeBSD_version > 800000		
	if (cxgb_use_16k_clusters) 
		ccp->ccp_jumbo_zone = zone_jumbo16;
	else
		ccp->ccp_jumbo_zone = zone_jumbo9;
#else
		ccp->ccp_jumbo_zone = zone_jumbop;
#endif
	return (0);
}

static void
cxgb_cache_pcpu_deinit(struct cxgb_cache_pcpu *ccp)
{
	void *cl;

	while ((cl = buf_stack_pop(&ccp->ccp_jumbo_free)) != NULL)
		uma_zfree(ccp->ccp_jumbo_zone, cl);
	while ((cl = buf_stack_pop(&ccp->ccp_cluster_free)) != NULL)
		uma_zfree(zone_clust, cl);

	buf_stack_deinit(&ccp->ccp_jumbo_free);
	buf_stack_deinit(&ccp->ccp_cluster_free);
	
}

static int inited = 0;

int
cxgb_cache_init(void)
{
	int i, err;
	
	if (inited++ > 0)
		return (0);

	if ((cxgb_caches = malloc(sizeof(struct cxgb_cache_pcpu)*mp_ncpus, M_DEVBUF, M_WAITOK|M_ZERO)) == NULL)
		return (ENOMEM);
	
	for (i = 0; i < mp_ncpus; i++) 
		if ((err = cxgb_cache_pcpu_init(&cxgb_caches->ccs_array[i])))
			goto err;

	return (0);
err:
	cxgb_cache_flush();

	return (err);
}

void
cxgb_cache_flush(void)
{
	int i;
	
	if (--inited > 0) 
		return;

	if (cxgb_caches == NULL)
		return;
	
	for (i = 0; i < mp_ncpus; i++) 
		cxgb_cache_pcpu_deinit(&cxgb_caches->ccs_array[i]);

	free(cxgb_caches, M_DEVBUF);
	cxgb_caches = NULL;
}

caddr_t
cxgb_cache_get(uma_zone_t zone)
{
	caddr_t cl = NULL;
	struct cxgb_cache_pcpu *ccp;

	if (cxgb_pcpu_cache_enable) {
		critical_enter();
		ccp = &cxgb_caches->ccs_array[curcpu];
		if (zone == zone_clust) {
			cl = buf_stack_pop(&ccp->ccp_cluster_free);
		} else if (zone == ccp->ccp_jumbo_zone) {
			cl = buf_stack_pop(&ccp->ccp_jumbo_free);
		}
		critical_exit();
	}
	
	if (cl == NULL) 
		cl = uma_zalloc(zone, M_NOWAIT);
	else
		cxgb_cached_allocations++;
	
	return (cl);
}

void
cxgb_cache_put(uma_zone_t zone, void *cl)
{
	struct cxgb_cache_pcpu *ccp;
	int err = ENOSPC;

	if (cxgb_pcpu_cache_enable) {
		critical_enter();
		ccp = &cxgb_caches->ccs_array[curcpu];
		if (zone == zone_clust) {
			err = buf_stack_push(&ccp->ccp_cluster_free, cl);
		} else if (zone == ccp->ccp_jumbo_zone){
			err = buf_stack_push(&ccp->ccp_jumbo_free, cl);
		}
		critical_exit();
	}
	
	if (err)
		uma_zfree(zone, cl);
	else
		cxgb_cached++;
}

void
cxgb_cache_refill(void)
{
	struct cxgb_cache_pcpu *ccp;
	caddr_t vec[8];
	uma_zone_t zone;
	int i, count;


	return;
restart:
	critical_enter();
	ccp = &cxgb_caches->ccs_array[curcpu];
	zone = ccp->ccp_jumbo_zone;
	if (!buf_stack_avail(&ccp->ccp_jumbo_free) &&
	    !buf_stack_avail(&ccp->ccp_cluster_free)) {
		critical_exit();
		return;
	}
	critical_exit();


	
	for (i = 0; i < 8; i++)
		if ((vec[i] = uma_zalloc(zone, M_NOWAIT)) == NULL) 
			goto free;

	critical_enter();
	ccp = &cxgb_caches->ccs_array[curcpu];
	for (i = 0; i < 8 && buf_stack_avail(&ccp->ccp_jumbo_free); i++)
		if (buf_stack_push(&ccp->ccp_jumbo_free, vec[i]))
			break;
	critical_exit();

	for (; i < 8; i++)
		uma_zfree(zone, vec[i]);


	
	zone = zone_clust;
	for (i = 0; i < 8; i++)
		if ((vec[i] = uma_zalloc(zone, M_NOWAIT)) == NULL) 
			goto free;

	critical_enter();
	ccp = &cxgb_caches->ccs_array[curcpu];
	for (i = 0; i < 8 && buf_stack_avail(&ccp->ccp_cluster_free); i++)
		if (buf_stack_push(&ccp->ccp_cluster_free, vec[i]))
			break;
	critical_exit();
	
	for (; i < 8; i++)
		uma_zfree(zone, vec[i]);

	goto restart;


free:
	count = i;
	for (; i < count; i++)
		uma_zfree(zone, vec[i]);
}
	
struct buf_ring *
buf_ring_alloc(int count, int flags)
{
	struct buf_ring *br;

	KASSERT(powerof2(count), ("buf ring must be size power of 2"));
	
	br = malloc(sizeof(struct buf_ring), M_DEVBUF, flags|M_ZERO);
	if (br == NULL)
		return (NULL);
	
	br->br_ring = malloc(sizeof(caddr_t)*count, M_DEVBUF, flags|M_ZERO);
	if (br->br_ring == NULL) {
		free(br, M_DEVBUF);
		return (NULL);
	}
	
	mtx_init(&br->br_lock, "buf ring", NULL, MTX_DUPOK|MTX_DEF);
	br->br_size = count;
	br->br_prod = br->br_cons = 0;

	return (br);
}

void
buf_ring_free(struct buf_ring *br)
{
	free(br->br_ring, M_DEVBUF);
	free(br, M_DEVBUF);
}
