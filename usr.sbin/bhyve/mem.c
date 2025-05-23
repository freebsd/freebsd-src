/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Memory ranges are represented with an RB tree. On insertion, the range
 * is checked for overlaps. On lookup, the key has the same base and limit
 * so it can be searched within the range.
 */

#include <sys/types.h>
#define _WANT_KERNEL_ERRNO 1
#include <sys/errno.h>
#include <sys/tree.h>
#include <machine/vmm.h>
#include <machine/vmm_instruction_emul.h>

#include <assert.h>
#include <err.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <vmmapi.h>

#include "mem.h"

struct mmio_rb_range {
	RB_ENTRY(mmio_rb_range)	mr_link;	/* RB tree links */
	struct mem_range	mr_param;
	uint64_t                mr_base;
	uint64_t                mr_end;
};

struct mmio_rb_tree;
RB_PROTOTYPE(mmio_rb_tree, mmio_rb_range, mr_link, mmio_rb_range_compare);

static RB_HEAD(mmio_rb_tree, mmio_rb_range) mmio_rb_root, mmio_rb_fallback;

/*
 * Per-vCPU cache. Since most accesses from a vCPU will be to
 * consecutive addresses in a range, it makes sense to cache the
 * result of a lookup.
 */
static struct mmio_rb_range	**mmio_hint;
static int mmio_ncpu;

static pthread_rwlock_t mmio_rwlock;

static int
mmio_rb_range_compare(struct mmio_rb_range *a, struct mmio_rb_range *b)
{
	if (a->mr_end < b->mr_base)
		return (-1);
	else if (a->mr_base > b->mr_end)
		return (1);
	return (0);
}

static int
mmio_rb_lookup(struct mmio_rb_tree *rbt, uint64_t addr,
    struct mmio_rb_range **entry)
{
	struct mmio_rb_range find, *res;

	find.mr_base = find.mr_end = addr;

	res = RB_FIND(mmio_rb_tree, rbt, &find);

	if (res != NULL) {
		*entry = res;
		return (0);
	}

	return (ENOENT);
}

static int
mmio_rb_add(struct mmio_rb_tree *rbt, struct mmio_rb_range *new)
{
	struct mmio_rb_range *overlap;

	overlap = RB_INSERT(mmio_rb_tree, rbt, new);

	if (overlap != NULL) {
#ifdef RB_DEBUG
		printf("overlap detected: new %lx:%lx, tree %lx:%lx, '%s' "
		       "claims region already claimed for '%s'\n",
		       new->mr_base, new->mr_end,
		       overlap->mr_base, overlap->mr_end,
		       new->mr_param.name, overlap->mr_param.name);
#endif

		return (EEXIST);
	}

	return (0);
}

#if 0
static void
mmio_rb_dump(struct mmio_rb_tree *rbt)
{
	int perror;
	struct mmio_rb_range *np;

	pthread_rwlock_rdlock(&mmio_rwlock);
	RB_FOREACH(np, mmio_rb_tree, rbt) {
		printf(" %lx:%lx, %s\n", np->mr_base, np->mr_end,
		       np->mr_param.name);
	}
	perror = pthread_rwlock_unlock(&mmio_rwlock);
	assert(perror == 0);
}
#endif

RB_GENERATE(mmio_rb_tree, mmio_rb_range, mr_link, mmio_rb_range_compare);

typedef int (mem_cb_t)(struct vcpu *vcpu, uint64_t gpa, struct mem_range *mr,
    void *arg);

static int
mem_read(struct vcpu *vcpu, uint64_t gpa, uint64_t *rval, int size, void *arg)
{
	int error;
	struct mem_range *mr = arg;

	error = (*mr->handler)(vcpu, MEM_F_READ, gpa, size, rval, mr->arg1,
	    mr->arg2);
	return (error);
}

static int
mem_write(struct vcpu *vcpu, uint64_t gpa, uint64_t wval, int size, void *arg)
{
	int error;
	struct mem_range *mr = arg;

	error = (*mr->handler)(vcpu, MEM_F_WRITE, gpa, size, &wval, mr->arg1,
	    mr->arg2);
	return (error);
}

static int
access_memory(struct vcpu *vcpu, uint64_t paddr, mem_cb_t *cb, void *arg)
{
	struct mmio_rb_range *entry;
	struct mem_range *mr;
	int err, perror, immutable, vcpuid;

	vcpuid = vcpu_id(vcpu);
	mr = NULL;
	pthread_rwlock_rdlock(&mmio_rwlock);

	/*
	 * First check the per-vCPU cache
	 */
	if (mmio_hint[vcpuid] &&
	    paddr >= mmio_hint[vcpuid]->mr_base &&
	    paddr <= mmio_hint[vcpuid]->mr_end) {
		entry = mmio_hint[vcpuid];
	} else
		entry = NULL;

	if (entry == NULL) {
		if (mmio_rb_lookup(&mmio_rb_root, paddr, &entry) == 0) {
			/* Update the per-vCPU cache */
			mmio_hint[vcpuid] = entry;
		} else if (mmio_rb_lookup(&mmio_rb_fallback, paddr,
		    &entry) == 0) {
		} else {
			err = mmio_handle_non_backed_mem(vcpu, paddr, &mr);
			if (err != 0) {
				perror = pthread_rwlock_unlock(&mmio_rwlock);
				assert(perror == 0);
				return (err == EJUSTRETURN ? 0 : err);
			}
		}
	}

	if (mr == NULL) {
		assert(entry != NULL);
		mr = &entry->mr_param;
	}

	/*
	 * An 'immutable' memory range is guaranteed to be never removed
	 * so there is no need to hold 'mmio_rwlock' while calling the
	 * handler.
	 *
	 * XXX writes to the PCIR_COMMAND register can cause register_mem()
	 * to be called. If the guest is using PCI extended config space
	 * to modify the PCIR_COMMAND register then register_mem() can
	 * deadlock on 'mmio_rwlock'. However by registering the extended
	 * config space window as 'immutable' the deadlock can be avoided.
	 */
	immutable = (mr->flags & MEM_F_IMMUTABLE) != 0;
	if (immutable) {
		perror = pthread_rwlock_unlock(&mmio_rwlock);
		assert(perror == 0);
	}

	err = cb(vcpu, paddr, mr, arg);

	if (!immutable) {
		perror = pthread_rwlock_unlock(&mmio_rwlock);
		assert(perror == 0);
	}

	return (err);
}

struct emulate_mem_args {
	struct vie *vie;
	struct vm_guest_paging *paging;
};

static int
emulate_mem_cb(struct vcpu *vcpu, uint64_t paddr, struct mem_range *mr,
    void *arg)
{
	struct emulate_mem_args *ema;

	ema = arg;
	return (vmm_emulate_instruction(vcpu, paddr, ema->vie, ema->paging,
	    mem_read, mem_write, mr));
}

int
emulate_mem(struct vcpu *vcpu, uint64_t paddr, struct vie *vie,
    struct vm_guest_paging *paging)
{
	struct emulate_mem_args ema;

	ema.vie = vie;
	ema.paging = paging;
	return (access_memory(vcpu, paddr, emulate_mem_cb, &ema));
}

struct rw_mem_args {
	uint64_t *val;
	int size;
	int operation;
};

static int
rw_mem_cb(struct vcpu *vcpu, uint64_t paddr, struct mem_range *mr, void *arg)
{
	struct rw_mem_args *rma;

	rma = arg;
	return (mr->handler(vcpu, rma->operation, paddr, rma->size,
	    rma->val, mr->arg1, mr->arg2));
}

int
read_mem(struct vcpu *vcpu, uint64_t gpa, uint64_t *rval, int size)
{
	struct rw_mem_args rma;

	rma.val = rval;
	rma.size = size;
	rma.operation = MEM_F_READ;
	return (access_memory(vcpu, gpa, rw_mem_cb, &rma));
}

int
write_mem(struct vcpu *vcpu, uint64_t gpa, uint64_t wval, int size)
{
	struct rw_mem_args rma;

	rma.val = &wval;
	rma.size = size;
	rma.operation = MEM_F_WRITE;
	return (access_memory(vcpu, gpa, rw_mem_cb, &rma));
}

static int
register_mem_int(struct mmio_rb_tree *rbt, struct mem_range *memp)
{
	struct mmio_rb_range *entry, *mrp;
	int err, perror;

	err = 0;

	mrp = malloc(sizeof(struct mmio_rb_range));
	if (mrp == NULL) {
		warn("%s: couldn't allocate memory for mrp\n",
		     __func__);
		err = ENOMEM;
	} else {
		mrp->mr_param = *memp;
		mrp->mr_base = memp->base;
		mrp->mr_end = memp->base + memp->size - 1;
		pthread_rwlock_wrlock(&mmio_rwlock);
		if (mmio_rb_lookup(rbt, memp->base, &entry) != 0)
			err = mmio_rb_add(rbt, mrp);
		perror = pthread_rwlock_unlock(&mmio_rwlock);
		assert(perror == 0);
		if (err)
			free(mrp);
	}

	return (err);
}

int
register_mem(struct mem_range *memp)
{

	return (register_mem_int(&mmio_rb_root, memp));
}

int
register_mem_fallback(struct mem_range *memp)
{

	return (register_mem_int(&mmio_rb_fallback, memp));
}

int
unregister_mem(struct mem_range *memp)
{
	struct mem_range *mr;
	struct mmio_rb_range *entry = NULL;
	int err, perror, i;

	pthread_rwlock_wrlock(&mmio_rwlock);
	err = mmio_rb_lookup(&mmio_rb_root, memp->base, &entry);
	if (err == 0) {
		mr = &entry->mr_param;
		assert(mr->name == memp->name);
		assert(mr->base == memp->base && mr->size == memp->size);
		assert((mr->flags & MEM_F_IMMUTABLE) == 0);
		RB_REMOVE(mmio_rb_tree, &mmio_rb_root, entry);

		/* flush Per-vCPU cache */
		for (i = 0; i < mmio_ncpu; i++) {
			if (mmio_hint[i] == entry)
				mmio_hint[i] = NULL;
		}
	}
	perror = pthread_rwlock_unlock(&mmio_rwlock);
	assert(perror == 0);

	if (entry)
		free(entry);

	return (err);
}

void
init_mem(int ncpu)
{

	mmio_ncpu = ncpu;
	mmio_hint = calloc(ncpu, sizeof(*mmio_hint));
	RB_INIT(&mmio_rb_root);
	RB_INIT(&mmio_rb_fallback);
	pthread_rwlock_init(&mmio_rwlock, NULL);
}
