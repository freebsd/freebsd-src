/*-
 * Copyright (c) 2009 Jeffrey Roberson <jeff@freebsd.org>
 * Copyright (c) 2009 Robert N. M. Watson
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/linker_set.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/vimage.h>

#include <net/vnet.h>

/*-
 * This is the virtual network stack allocator, which provides storage for
 * virtualized global variables.  These variables are defined/declared using
 * the VNET_DEFINE()/VNET_DECLARE() macros, which place them in the
 * 'set_vnet' linker set.  The details of the implementation are somewhat
 * subtle, but allow the majority of most network subsystems to maintain
 * virtualization-agnostic.
 *
 * The virtual network stack allocator handles variables in the base kernel
 * vs. modules in similar but different ways.  In both cases, virtualized
 * global variables are marked as such by being declared to be part of the
 * vnet linker set.  These "master" copies of global variables serve two
 * functions:
 *
 * (1) They contain static initialization or "default" values for global
 *     variables which will be propagated to each virtual network stack
 *     instance when created.  As with normal global variables, they default
 *     to zero-filled.
 *
 * (2) They act as unique global names by which the variable can be referred
 *     to, regardless of network stack instance.  The single global symbol
 *     will be used to calculate the location of a per-virtual instance
 *     variable at run-time.
 *
 * Each virtual network stack instance has a complete copy of each
 * virtualized global variable, stored in a malloc'd block of memory
 * referred to by vnet->vnet_data_mem.  Critical to the design is that each
 * per-instance memory block is laid out identically to the master block so
 * that the offset of each global variable is the same across all blocks.  To
 * optimize run-time access, a precalculated 'base' address,
 * vnet->vnet_data_base, is stored in each vnet, and is the amount that can
 * be added to the address of a 'master' instance of a variable to get to the
 * per-vnet instance.
 *
 * Virtualized global variables are handled in a similar manner, but as each
 * module has its own 'set_vnet' linker set, and we want to keep all
 * virtualized globals togther, we reserve space in the kernel's linker set
 * for potential module variables using a per-vnet character array,
 * 'modspace'.  The virtual network stack allocator maintains a free list to
 * track what space in the array is free (all, initially) and as modules are
 * linked, allocates portions of the space to specific globals.  The kernel
 * module linker queries the virtual network stack allocator and will
 * bind references of the global to the location during linking.  It also
 * calls into the virtual network stack allocator, once the memory is
 * initialized, in order to propagate the new static initializations to all
 * existing virtual network stack instances so that the soon-to-be executing
 * module will find every network stack instance with proper default values.
 */

/*
 * Location of the kernel's 'set_vnet' linker set.
 */
extern uintptr_t	*__start_set_vnet;
extern uintptr_t	*__stop_set_vnet;

#define	VNET_START	(uintptr_t)&__start_set_vnet
#define	VNET_STOP	(uintptr_t)&__stop_set_vnet

/*
 * Number of bytes of data in the 'set_vnet' linker set, and hence the total
 * size of all kernel virtualized global variables, and the malloc(9) type
 * that will be used to allocate it.
 */
#define	VNET_BYTES	(VNET_STOP - VNET_START)

MALLOC_DEFINE(M_VNET_DATA, "vnet_data", "VNET data");

/*
 * VNET_MODMIN is the minimum number of bytes we will reserve for the sum of
 * global variables across all loaded modules.  As this actually sizes an
 * array declared as a virtualized global variable in the kernel itself, and
 * we want the virtualized global variable space to be page-sized, we may
 * have more space than that in practice.
 */
#define	VNET_MODMIN	8192
#define	VNET_SIZE	roundup2(VNET_BYTES, PAGE_SIZE)
#define	VNET_MODSIZE	(VNET_SIZE - (VNET_BYTES - VNET_MODMIN))

/*
 * Space to store virtualized global variables from loadable kernel modules,
 * and the free list to manage it.
 */
static VNET_DEFINE(char, modspace[VNET_MODMIN]);

struct vnet_data_free {
	uintptr_t	vnd_start;
	int		vnd_len;
	TAILQ_ENTRY(vnet_data_free) vnd_link;
};

MALLOC_DEFINE(M_VNET_DATA_FREE, "vnet_data_free", "VNET resource accounting");
static TAILQ_HEAD(, vnet_data_free) vnet_data_free_head =
	    TAILQ_HEAD_INITIALIZER(vnet_data_free_head);
static struct sx vnet_data_free_lock;

/*
 * Allocate storage for virtualized global variables in a new virtual network
 * stack instance, and copy in initial values from our 'master' copy.
 */
void
vnet_data_init(struct vnet *vnet)
{

	vnet->vnet_data_mem = malloc(VNET_SIZE, M_VNET_DATA, M_WAITOK);
	memcpy(vnet->vnet_data_mem, (void *)VNET_START, VNET_BYTES);

	/*
	 * All use of vnet-specific data will immediately subtract VNET_START
	 * from the base memory pointer, so pre-calculate that now to avoid
	 * it on each use.
	 */
	vnet->vnet_data_base = (uintptr_t)vnet->vnet_data_mem - VNET_START;
}

/*
 * Release storage for a virtual network stack instance.
 */
void
vnet_data_destroy(struct vnet *vnet)
{

	free(vnet->vnet_data_mem, M_VNET_DATA);
	vnet->vnet_data_mem = NULL;
	vnet->vnet_data_base = 0;
}

/*
 * Once on boot, initialize the modspace freelist to entirely cover modspace.
 */
static void
vnet_data_startup(void *dummy __unused)
{
	struct vnet_data_free *df;

	df = malloc(sizeof(*df), M_VNET_DATA_FREE, M_WAITOK | M_ZERO);
	df->vnd_start = (uintptr_t)&VNET_NAME(modspace);
	df->vnd_len = VNET_MODSIZE;
	TAILQ_INSERT_HEAD(&vnet_data_free_head, df, vnd_link);
	sx_init(&vnet_data_free_lock, "vnet_data alloc lock");
}
SYSINIT(vnet_data, SI_SUB_KLD, SI_ORDER_FIRST, vnet_data_startup, 0);

/*
 * When a module is loaded and requires storage for a virtualized global
 * variable, allocate space from the modspace free list.  This interface
 * should be used only by the kernel linker.
 */
void *
vnet_data_alloc(int size)
{
	struct vnet_data_free *df;
	void *s;

	s = NULL;
	size = roundup2(size, sizeof(void *));
	sx_xlock(&vnet_data_free_lock);
	TAILQ_FOREACH(df, &vnet_data_free_head, vnd_link) {
		if (df->vnd_len < size)
			continue;
		if (df->vnd_len == size) {
			s = (void *)df->vnd_start;
			TAILQ_REMOVE(&vnet_data_free_head, df, vnd_link);
			free(df, M_VNET_DATA_FREE);
			break;
		}
		s = (void *)df->vnd_start;
		df->vnd_len -= size;
		df->vnd_start = df->vnd_start + size;
		break;
	}
	sx_xunlock(&vnet_data_free_lock);

	return (s);
}

/*
 * Free space for a virtualized global variable on module unload.
 */
void
vnet_data_free(void *start_arg, int size)
{
	struct vnet_data_free *df;
	struct vnet_data_free *dn;
	uintptr_t start;
	uintptr_t end;

	size = roundup2(size, sizeof(void *));
	start = (uintptr_t)start_arg;
	end = start + size;
	/*
	 * Free a region of space and merge it with as many neighbors as
	 * possible.  Keeping the list sorted simplifies this operation.
	 */
	sx_xlock(&vnet_data_free_lock);
	TAILQ_FOREACH(df, &vnet_data_free_head, vnd_link) {
		if (df->vnd_start > end)
			break;
		/*
		 * If we expand at the end of an entry we may have to
		 * merge it with the one following it as well.
		 */
		if (df->vnd_start + df->vnd_len == start) {
			df->vnd_len += size;
			dn = TAILQ_NEXT(df, vnd_link);
			if (df->vnd_start + df->vnd_len == dn->vnd_start) {
				df->vnd_len += dn->vnd_len;
				TAILQ_REMOVE(&vnet_data_free_head, dn, vnd_link);
				free(dn, M_VNET_DATA_FREE);
			}
			sx_xunlock(&vnet_data_free_lock);
			return;
		}
		if (df->vnd_start == end) {
			df->vnd_start = start;
			df->vnd_len += size;
			sx_xunlock(&vnet_data_free_lock);
			return;
		}
	}
	dn = malloc(sizeof(*df), M_VNET_DATA_FREE, M_WAITOK | M_ZERO);
	dn->vnd_start = start;
	dn->vnd_len = size;
	if (df)
		TAILQ_INSERT_BEFORE(df, dn, vnd_link);
	else
		TAILQ_INSERT_TAIL(&vnet_data_free_head, dn, vnd_link);
	sx_xunlock(&vnet_data_free_lock);
}

struct vnet_data_copy_fn_arg {
	void	*start;
	int	 size;
};

static void
vnet_data_copy_fn(struct vnet *vnet, void *arg)
{
	struct vnet_data_copy_fn_arg *varg = arg;

	memcpy((void *)((uintptr_t)vnet->vnet_data_base +
	    (uintptr_t)varg->start), varg->start, varg->size);
}

/*
 * When a new virtualized global variable has been allocated, propagate its
 * initial value to each already-allocated virtual network stack instance.
 */
void
vnet_data_copy(void *start, int size)
{
	struct vnet_data_copy_fn_arg varg;

	varg.start = start;
	varg.size = size;
	vnet_foreach(vnet_data_copy_fn, &varg);
}

/*
 * Variants on sysctl_handle_foo that know how to handle virtualized global
 * variables: if 'arg1' is a pointer, then we transform it to the local vnet
 * offset.
 */
int
vnet_sysctl_handle_int(SYSCTL_HANDLER_ARGS)
{

	if (arg1 != NULL)
		arg1 = (void *)(curvnet->vnet_data_base + (uintptr_t)arg1);
	return (sysctl_handle_int(oidp, arg1, arg2, req));
}

int
vnet_sysctl_handle_opaque(SYSCTL_HANDLER_ARGS)
{

	if (arg1 != NULL)
		arg1 = (void *)(curvnet->vnet_data_base + (uintptr_t)arg1);
	return (sysctl_handle_opaque(oidp, arg1, arg2, req));
}

int
vnet_sysctl_handle_string(SYSCTL_HANDLER_ARGS)
{

	if (arg1 != NULL)
		arg1 = (void *)(curvnet->vnet_data_base + (uintptr_t)arg1);
	return (sysctl_handle_string(oidp, arg1, arg2, req));
}

int
vnet_sysctl_handle_uint(SYSCTL_HANDLER_ARGS)
{

	if (arg1 != NULL)
		arg1 = (void *)(curvnet->vnet_data_base + (uintptr_t)arg1);
	return (sysctl_handle_int(oidp, arg1, arg2, req));
}
