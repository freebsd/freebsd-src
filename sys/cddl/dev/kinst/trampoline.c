/*
 * SPDX-License-Identifier: CDDL 1.0
 *
 * Copyright (c) 2022 Christos Margiolis <christos@FreeBSD.org>
 * Copyright (c) 2022 Mark Johnston <markj@FreeBSD.org>
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * Portions of this software were developed by Christos Margiolis
 * <christos@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
 */

#include <sys/param.h>
#include <sys/bitset.h>
#include <sys/cred.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sx.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>

#include <cddl/dev/dtrace/dtrace_cddl.h>

#include "kinst.h"
#include "kinst_isa.h"

#define KINST_TRAMP_FILL_PATTERN	((kinst_patchval_t []){KINST_PATCHVAL})
#define KINST_TRAMP_FILL_SIZE		sizeof(kinst_patchval_t)

#define KINST_TRAMPCHUNK_SIZE		PAGE_SIZE
#define KINST_TRAMPS_PER_CHUNK		(KINST_TRAMPCHUNK_SIZE / KINST_TRAMP_SIZE)

struct trampchunk {
	TAILQ_ENTRY(trampchunk) next;
	uint8_t *addr;
	/* 0 -> allocated, 1 -> free */
	BITSET_DEFINE(, KINST_TRAMPS_PER_CHUNK) free;
};

static TAILQ_HEAD(, trampchunk)	kinst_trampchunks =
    TAILQ_HEAD_INITIALIZER(kinst_trampchunks);
static struct sx		kinst_tramp_sx;
SX_SYSINIT(kinst_tramp_sx, &kinst_tramp_sx, "kinst tramp");
#ifdef __amd64__
static eventhandler_tag		kinst_thread_ctor_handler;
static eventhandler_tag		kinst_thread_dtor_handler;
#endif

/*
 * Fill the trampolines with KINST_TRAMP_FILL_PATTERN so that the kernel will
 * crash cleanly if things somehow go wrong.
 */
static void
kinst_trampoline_fill(uint8_t *addr, int size)
{
	int i;

	for (i = 0; i < size; i += KINST_TRAMP_FILL_SIZE) {
		memcpy(&addr[i], KINST_TRAMP_FILL_PATTERN,
		    KINST_TRAMP_FILL_SIZE);
	}
}

static struct trampchunk *
kinst_trampchunk_alloc(void)
{
	struct trampchunk *chunk;
	vm_offset_t trampaddr;
	int error __diagused;

	sx_assert(&kinst_tramp_sx, SX_XLOCKED);

#ifdef __amd64__
	/*
	 * To simplify population of trampolines, we follow the amd64 kernel's
	 * code model and allocate them above KERNBASE, i.e., in the top 2GB of
	 * the kernel's virtual address space (not the case for other
	 * platforms).
	 */
	trampaddr = KERNBASE;
#else
	trampaddr = VM_MIN_KERNEL_ADDRESS;
#endif
	/*
	 * Allocate virtual memory for the trampoline chunk. The returned
	 * address is saved in "trampaddr". Trampolines must be executable so
	 * max_prot must include VM_PROT_EXECUTE.
	 */
	error = vm_map_find(kernel_map, NULL, 0, &trampaddr,
	    KINST_TRAMPCHUNK_SIZE, 0, VMFS_ANY_SPACE, VM_PROT_ALL, VM_PROT_ALL,
	    0);
	if (error != KERN_SUCCESS) {
		KINST_LOG("trampoline chunk allocation failed: %d", error);
		return (NULL);
	}

	error = kmem_back(kernel_object, trampaddr, KINST_TRAMPCHUNK_SIZE,
	    M_WAITOK | M_EXEC);
	KASSERT(error == KERN_SUCCESS, ("kmem_back failed: %d", error));

	kinst_trampoline_fill((uint8_t *)trampaddr, KINST_TRAMPCHUNK_SIZE);

	/* Allocate a tracker for this chunk. */
	chunk = malloc(sizeof(*chunk), M_KINST, M_WAITOK);
	chunk->addr = (void *)trampaddr;
	BIT_FILL(KINST_TRAMPS_PER_CHUNK, &chunk->free);

	TAILQ_INSERT_HEAD(&kinst_trampchunks, chunk, next);

	return (chunk);
}

static void
kinst_trampchunk_free(struct trampchunk *chunk)
{
	sx_assert(&kinst_tramp_sx, SX_XLOCKED);

	TAILQ_REMOVE(&kinst_trampchunks, chunk, next);
	kmem_unback(kernel_object, (vm_offset_t)chunk->addr,
	    KINST_TRAMPCHUNK_SIZE);
	(void)vm_map_remove(kernel_map, (vm_offset_t)chunk->addr,
	    (vm_offset_t)(chunk->addr + KINST_TRAMPCHUNK_SIZE));
	free(chunk, M_KINST);
}

static uint8_t *
kinst_trampoline_alloc_locked(int how)
{
	struct trampchunk *chunk;
	uint8_t *tramp;
	int off;

	sx_assert(&kinst_tramp_sx, SX_XLOCKED);

	TAILQ_FOREACH(chunk, &kinst_trampchunks, next) {
		/* All trampolines from this chunk are already allocated. */
		if ((off = BIT_FFS(KINST_TRAMPS_PER_CHUNK, &chunk->free)) == 0)
			continue;
		/* BIT_FFS() returns indices starting at 1 instead of 0. */
		off--;
		break;
	}
	if (chunk == NULL) {
		if ((how & M_NOWAIT) != 0)
			return (NULL);

		if ((chunk = kinst_trampchunk_alloc()) == NULL) {
#ifdef __amd64__
			/*
			 * We didn't find any free trampoline in the current
			 * list, allocate a new one.  If that fails the
			 * provider will no longer be reliable, so try to warn
			 * the user.
			 */
			static bool once = true;

			if (once) {
				once = false;
				KINST_LOG(
				    "kinst: failed to allocate trampoline, "
				    "probes may not fire");
			}
#endif
			return (NULL);
		}
		off = 0;
	}
	BIT_CLR(KINST_TRAMPS_PER_CHUNK, off, &chunk->free);
	tramp = chunk->addr + off * KINST_TRAMP_SIZE;
	return (tramp);
}

uint8_t *
kinst_trampoline_alloc(int how)
{
	uint8_t *tramp;

	sx_xlock(&kinst_tramp_sx);
	tramp = kinst_trampoline_alloc_locked(how);
	sx_xunlock(&kinst_tramp_sx);
	return (tramp);
}

static void
kinst_trampoline_dealloc_locked(uint8_t *tramp, bool freechunks)
{
	struct trampchunk *chunk;
	int off;

	sx_assert(&kinst_tramp_sx, SX_XLOCKED);

	if (tramp == NULL)
		return;

	TAILQ_FOREACH(chunk, &kinst_trampchunks, next) {
		for (off = 0; off < KINST_TRAMPS_PER_CHUNK; off++) {
			if (chunk->addr + off * KINST_TRAMP_SIZE == tramp) {
				kinst_trampoline_fill(tramp, KINST_TRAMP_SIZE);
				BIT_SET(KINST_TRAMPS_PER_CHUNK, off,
				    &chunk->free);
				if (freechunks &&
				    BIT_ISFULLSET(KINST_TRAMPS_PER_CHUNK,
				    &chunk->free))
					kinst_trampchunk_free(chunk);
				return;
			}
		}
	}
	panic("%s: did not find trampoline chunk for %p", __func__, tramp);
}

void
kinst_trampoline_dealloc(uint8_t *tramp)
{
	sx_xlock(&kinst_tramp_sx);
	kinst_trampoline_dealloc_locked(tramp, true);
	sx_xunlock(&kinst_tramp_sx);
}

#ifdef __amd64__
static void
kinst_thread_ctor(void *arg __unused, struct thread *td)
{
	td->t_kinst_tramp = kinst_trampoline_alloc(M_WAITOK);
}

static void
kinst_thread_dtor(void *arg __unused, struct thread *td)
{
	void *tramp;

	tramp = td->t_kinst_tramp;
	td->t_kinst_tramp = NULL;

	/*
	 * This assumes that the thread_dtor event permits sleeping, which
	 * appears to be true for the time being.
	 */
	kinst_trampoline_dealloc(tramp);
}
#endif

int
kinst_trampoline_init(void)
{
#ifdef __amd64__
	struct proc *p;
	struct thread *td;
	void *tramp;
	int error;

	kinst_thread_ctor_handler = EVENTHANDLER_REGISTER(thread_ctor,
	    kinst_thread_ctor, NULL, EVENTHANDLER_PRI_ANY);
	kinst_thread_dtor_handler = EVENTHANDLER_REGISTER(thread_dtor,
	    kinst_thread_dtor, NULL, EVENTHANDLER_PRI_ANY);

	error = 0;
	tramp = NULL;

	sx_slock(&allproc_lock);
	sx_xlock(&kinst_tramp_sx);
	FOREACH_PROC_IN_SYSTEM(p) {
retry:
		PROC_LOCK(p);
		FOREACH_THREAD_IN_PROC(p, td) {
			if (td->t_kinst_tramp != NULL)
				continue;
			if (tramp == NULL) {
				/*
				 * Try to allocate a trampoline without dropping
				 * the process lock.  If all chunks are fully
				 * utilized, we must release the lock and try
				 * again.
				 */
				tramp = kinst_trampoline_alloc_locked(M_NOWAIT);
				if (tramp == NULL) {
					PROC_UNLOCK(p);
					tramp = kinst_trampoline_alloc_locked(
					    M_WAITOK);
					if (tramp == NULL) {
						/*
						 * Let the unload handler clean
						 * up.
						 */
						error = ENOMEM;
						goto out;
					} else
						goto retry;
				}
			}
			td->t_kinst_tramp = tramp;
			tramp = NULL;
		}
		PROC_UNLOCK(p);
	}
out:
	sx_xunlock(&kinst_tramp_sx);
	sx_sunlock(&allproc_lock);
#else
	int error = 0;

	sx_xlock(&kinst_tramp_sx);
	TAILQ_INIT(&kinst_trampchunks);
	sx_xunlock(&kinst_tramp_sx);
#endif

	return (error);
}

int
kinst_trampoline_deinit(void)
{
#ifdef __amd64__
	struct trampchunk *chunk, *tmp;
	struct proc *p;
	struct thread *td;

	EVENTHANDLER_DEREGISTER(thread_ctor, kinst_thread_ctor_handler);
	EVENTHANDLER_DEREGISTER(thread_dtor, kinst_thread_dtor_handler);

	sx_slock(&allproc_lock);
	sx_xlock(&kinst_tramp_sx);
	FOREACH_PROC_IN_SYSTEM(p) {
		PROC_LOCK(p);
		FOREACH_THREAD_IN_PROC(p, td) {
			kinst_trampoline_dealloc_locked(td->t_kinst_tramp,
			    false);
			td->t_kinst_tramp = NULL;
		}
		PROC_UNLOCK(p);
	}
	sx_sunlock(&allproc_lock);
	TAILQ_FOREACH_SAFE(chunk, &kinst_trampchunks, next, tmp)
		kinst_trampchunk_free(chunk);
	sx_xunlock(&kinst_tramp_sx);
#else
	struct trampchunk *chunk, *tmp;

	sx_xlock(&kinst_tramp_sx);
	TAILQ_FOREACH_SAFE(chunk, &kinst_trampchunks, next, tmp)
		kinst_trampchunk_free(chunk);
	sx_xunlock(&kinst_tramp_sx);
#endif

	return (0);
}
