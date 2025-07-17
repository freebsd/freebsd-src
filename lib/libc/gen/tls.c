/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2004 Doug Rabson
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

/*
 * Define stubs for TLS internals so that programs and libraries can
 * link. These functions will be replaced by functional versions at
 * runtime from ld-elf.so.1.
 */

#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>
#include <unistd.h>

#include "rtld.h"
#include "libc_private.h"

#define	tls_assert(cond)	((cond) ? (void) 0 :			\
    (tls_msg(#cond ": assert failed: " __FILE__ ":"			\
      __XSTRING(__LINE__) "\n"), abort()))
#define	tls_msg(s)		write(STDOUT_FILENO, s, strlen(s))

/* Provided by jemalloc to avoid bootstrapping issues. */
void	*__je_bootstrap_malloc(size_t size);
void	*__je_bootstrap_calloc(size_t num, size_t size);
void	__je_bootstrap_free(void *ptr);

__weak_reference(__libc_allocate_tls, _rtld_allocate_tls);
__weak_reference(__libc_free_tls, _rtld_free_tls);

#ifdef __i386__

__weak_reference(___libc_tls_get_addr, ___tls_get_addr);
__attribute__((__regparm__(1))) void * ___libc_tls_get_addr(void *);

#endif

void * __libc_tls_get_addr(void *);
__weak_reference(__libc_tls_get_addr, __tls_get_addr);

void *_rtld_allocate_tls(void *oldtls, size_t tcbsize, size_t tcbalign);
void _rtld_free_tls(void *tls, size_t tcbsize, size_t tcbalign);
void *__libc_allocate_tls(void *oldtls, size_t tcbsize, size_t tcbalign);
void __libc_free_tls(void *tls, size_t tcbsize, size_t tcbalign);

#ifndef PIC

static size_t libc_tls_static_space;
static size_t libc_tls_init_size;
static size_t libc_tls_init_align;
static void *libc_tls_init;
#endif

void *
__libc_tls_get_addr(void *vti)
{
	struct dtv *dtv;
	tls_index *ti;

	dtv = _tcb_get()->tcb_dtv;
	ti = vti;
	return (dtv->dtv_slots[ti->ti_module - 1].dtvs_tls +
	    (ti->ti_offset + TLS_DTV_OFFSET));
}

#ifdef __i386__

/* GNU ABI */

__attribute__((__regparm__(1)))
void *
___libc_tls_get_addr(void *vti)
{
	return (__libc_tls_get_addr(vti));
}

#endif

#ifndef PIC

static void *
libc_malloc_aligned(size_t size, size_t align)
{
	void *mem, *res;

	if (align < sizeof(void *))
		align = sizeof(void *);

	mem = __je_bootstrap_malloc(size + sizeof(void *) + align - 1);
	if (mem == NULL)
		return (NULL);
	res = (void *)roundup2((uintptr_t)mem + sizeof(void *), align);
	*(void **)((uintptr_t)res - sizeof(void *)) = mem;
	return (res);
}

static void
libc_free_aligned(void *ptr)
{
	void *mem;
	uintptr_t x;

	if (ptr == NULL)
		return;

	x = (uintptr_t)ptr;
	x -= sizeof(void *);
	mem = *(void **)x;
	__je_bootstrap_free(mem);
}

#ifdef TLS_VARIANT_I

/*
 * There are two versions of variant I of TLS
 *
 * - ARM and aarch64 uses original variant I as is described in [1] and [2],
 *   where TP points to start of TCB followed by aligned TLS segment.
 *   Both TCB and TLS must be aligned to alignment of TLS section. The TCB[0]
 *   points to DTV vector and DTV values are real addresses (without bias).
 *   Note: for Local Exec TLS Model, the offsets from TP (TCB in this case) to
 *   TLS variables are computed by linker, so we cannot overalign TLS section.
 *
 * - PowerPC and RISC-V use modified version of variant I, described in [3]
 *   where TP points (with bias) to TLS and TCB immediately precedes TLS without
 *   any alignment gap[4]. Only TLS should be aligned.  The TCB[0] points to DTV
 *   vector and DTV values are biased by constant value (TLS_DTV_OFFSET) from
 *   real addresses. However, like RTLD, we don't actually bias the DTV values,
 *   instead we compensate in __tls_get_addr for ti_offset's bias.
 *
 * [1] Ulrich Drepper: ELF Handling for Thread-Local Storage
 *     www.akkadia.org/drepper/tls.pdf
 *
 * [2] ARM IHI 0045E: Addenda to, and Errata in, the ABI for the ARM(r)
 *     Architecture
 *   infocenter.arm.com/help/topic/com.arm.doc.ihi0045e/IHI0045E_ABI_addenda.pdf
 *
 * [3] OpenPOWER: Power Architecture 64-Bit ELF V2 ABI Specification
 *     https://members.openpowerfoundation.org/document/dl/576
 *
 * [4] Its unclear if "without any alignment gap" is hard ABI requirement,
 *     but we must follow this rule due to suboptimal _tcb_set()
 *     (aka <ARCH>_SET_TP) implementation. This function doesn't expect TP but
 *     TCB as argument.
 */

/*
 * Return pointer to allocated TLS block
 */
static void *
get_tls_block_ptr(void *tcb, size_t tcbsize)
{
	size_t extra_size, post_size, pre_size, tls_block_size;

	/* Compute fragments sizes. */
	extra_size = tcbsize - TLS_TCB_SIZE;
#if defined(__aarch64__) || defined(__arm__)
	post_size =  roundup2(TLS_TCB_SIZE, libc_tls_init_align) - TLS_TCB_SIZE;
#else
	post_size = 0;
#endif
	tls_block_size = tcbsize + post_size;
	pre_size = roundup2(tls_block_size, libc_tls_init_align) -
	    tls_block_size;

	return ((char *)tcb - pre_size - extra_size);
}

/*
 * Free Static TLS using the Variant I method. The tcbsize
 * and tcbalign parameters must be the same as those used to allocate
 * the block.
 */
void
__libc_free_tls(void *tcb, size_t tcbsize, size_t tcbalign __unused)
{
	struct dtv *dtv;

	dtv = ((struct tcb *)tcb)->tcb_dtv;
	__je_bootstrap_free(dtv);
	libc_free_aligned(get_tls_block_ptr(tcb, tcbsize));
}

/*
 * Allocate Static TLS using the Variant I method.
 *
 * To handle all above requirements, we setup the following layout for 
 * TLS block:
 * (whole memory block is aligned with MAX(TLS_TCB_ALIGN, tls_init_align))
 *
 * +----------+--------------+--------------+-----------+------------------+
 * | pre gap  | extended TCB |     TCB      | post gap  |    TLS segment   |
 * | pre_size |  extra_size  | TLS_TCB_SIZE | post_size | tls_static_space |
 * +----------+--------------+--------------+-----------+------------------+
 *
 * where:
 *  extra_size is tcbsize - TLS_TCB_SIZE
 *  post_size is used to adjust TCB to TLS alignment for first version of TLS
 *            layout and is always 0 for second version.
 *  pre_size  is used to adjust TCB alignment for first version and to adjust
 *            TLS alignment for second version.
 *
 */
void *
__libc_allocate_tls(void *oldtcb, size_t tcbsize, size_t tcbalign)
{
	struct dtv *dtv;
	struct tcb *tcb;
	char *tls_block, *tls;
	size_t extra_size, maxalign, post_size, pre_size, tls_block_size;

	if (oldtcb != NULL && tcbsize == TLS_TCB_SIZE)
		return (oldtcb);

	tls_assert(tcbalign >= TLS_TCB_ALIGN);
	maxalign = MAX(tcbalign, libc_tls_init_align);

	/* Compute fragmets sizes. */
	extra_size = tcbsize - TLS_TCB_SIZE;
#if defined(__aarch64__) || defined(__arm__)
	post_size = roundup2(TLS_TCB_SIZE, libc_tls_init_align) - TLS_TCB_SIZE;
#else
	post_size = 0;
#endif
	tls_block_size = tcbsize + post_size;
	pre_size = roundup2(tls_block_size, libc_tls_init_align) -
	    tls_block_size;
	tls_block_size += pre_size + libc_tls_static_space;

	/* Allocate whole TLS block */
	tls_block = libc_malloc_aligned(tls_block_size, maxalign);
	if (tls_block == NULL) {
		tls_msg("__libc_allocate_tls: Out of memory.\n");
		abort();
	}
	memset(tls_block, 0, tls_block_size);
	tcb = (struct tcb *)(tls_block + pre_size + extra_size);
	tls = (char *)tcb + TLS_TCB_SIZE + post_size;

	if (oldtcb != NULL) {
		memcpy(tls_block, get_tls_block_ptr(oldtcb, tcbsize),
		    tls_block_size);
		libc_free_aligned(oldtcb);

		/* Adjust the DTV. */
		dtv = tcb->tcb_dtv;
		dtv->dtv_slots[0].dtvs_tls = tls;
	} else {
		dtv = __je_bootstrap_malloc(sizeof(struct dtv) +
		    sizeof(struct dtv_slot));
		if (dtv == NULL) {
			tls_msg("__libc_allocate_tls: Out of memory.\n");
			abort();
		}
		/* Build the DTV. */
		tcb->tcb_dtv = dtv;
		dtv->dtv_gen = 1;		/* Generation. */
		dtv->dtv_size = 1;		/* Segments count. */
		dtv->dtv_slots[0].dtvs_tls = tls;

		if (libc_tls_init_size > 0)
			memcpy(tls, libc_tls_init, libc_tls_init_size);
	}

	return (tcb);
}

#endif

#ifdef TLS_VARIANT_II

/*
 * Free Static TLS using the Variant II method.
 */
void
__libc_free_tls(void *tcb, size_t tcbsize __unused, size_t tcbalign)
{
	size_t size;
	struct dtv *dtv;
	uintptr_t tlsstart, tlsend;

	/*
	 * Figure out the size of the initial TLS block so that we can
	 * find stuff which ___tls_get_addr() allocated dynamically.
	 */
	tcbalign = MAX(tcbalign, libc_tls_init_align);
	size = roundup2(libc_tls_static_space, tcbalign);

	dtv = ((struct tcb *)tcb)->tcb_dtv;
	tlsend = (uintptr_t)tcb;
	tlsstart = tlsend - size;
	libc_free_aligned((void*)tlsstart);
	__je_bootstrap_free(dtv);
}

/*
 * Allocate Static TLS using the Variant II method.
 */
void *
__libc_allocate_tls(void *oldtcb, size_t tcbsize, size_t tcbalign)
{
	size_t size;
	char *tls_block, *tls;
	struct dtv *dtv;
	struct tcb *tcb;

	tcbalign = MAX(tcbalign, libc_tls_init_align);
	size = roundup2(libc_tls_static_space, tcbalign);

	if (tcbsize < 2 * sizeof(uintptr_t))
		tcbsize = 2 * sizeof(uintptr_t);
	tls_block = libc_malloc_aligned(size + tcbsize, tcbalign);
	if (tls_block == NULL) {
		tls_msg("__libc_allocate_tls: Out of memory.\n");
		abort();
	}
	memset(tls_block, 0, size + tcbsize);
	dtv = __je_bootstrap_malloc(sizeof(struct dtv) +
	    sizeof(struct dtv_slot));
	if (dtv == NULL) {
		tls_msg("__libc_allocate_tls: Out of memory.\n");
		abort();
	}

	tcb = (struct tcb *)(tls_block + size);
	tls = (char *)tcb - libc_tls_static_space;
	tcb->tcb_self = tcb;
	tcb->tcb_dtv = dtv;

	dtv->dtv_gen = 1;
	dtv->dtv_size = 1;
	dtv->dtv_slots[0].dtvs_tls = tls;

	if (oldtcb != NULL) {
		/*
		 * Copy the static TLS block over whole.
		 */
		memcpy(tls, (const char *)oldtcb - libc_tls_static_space,
		    libc_tls_static_space);

		/*
		 * We assume that this block was the one we created with
		 * allocate_initial_tls().
		 */
		_rtld_free_tls(oldtcb, 2 * sizeof(uintptr_t),
		    sizeof(uintptr_t));
	} else {
		memcpy(tls, libc_tls_init, libc_tls_init_size);
		memset(tls + libc_tls_init_size, 0,
		    libc_tls_static_space - libc_tls_init_size);
	}

	return (tcb);
}

#endif /* TLS_VARIANT_II */

#else

void *
__libc_allocate_tls(void *oldtcb __unused, size_t tcbsize __unused,
	size_t tcbalign __unused)
{
	return (0);
}

void
__libc_free_tls(void *tcb __unused, size_t tcbsize __unused,
	size_t tcbalign __unused)
{
}

#endif /* PIC */

void
_init_tls(void)
{
#ifndef PIC
	Elf_Addr *sp;
	Elf_Auxinfo *aux, *auxp;
	Elf_Phdr *phdr;
	size_t phent, phnum;
	int i;
	void *tls;

	sp = (Elf_Addr *) environ;
	while (*sp++ != 0)
		;
	aux = (Elf_Auxinfo *) sp;
	phdr = NULL;
	phent = phnum = 0;
	for (auxp = aux; auxp->a_type != AT_NULL; auxp++) {
		switch (auxp->a_type) {
		case AT_PHDR:
			phdr = auxp->a_un.a_ptr;
			break;

		case AT_PHENT:
			phent = auxp->a_un.a_val;
			break;

		case AT_PHNUM:
			phnum = auxp->a_un.a_val;
			break;
		}
	}
	if (phdr == NULL || phent != sizeof(Elf_Phdr) || phnum == 0)
		return;

	for (i = 0; (unsigned) i < phnum; i++) {
		if (phdr[i].p_type == PT_TLS) {
			libc_tls_static_space = roundup2(phdr[i].p_memsz,
			    phdr[i].p_align);
			libc_tls_init_size = phdr[i].p_filesz;
			libc_tls_init_align = phdr[i].p_align;
			libc_tls_init = (void *)phdr[i].p_vaddr;
			break;
		}
	}
	tls = _rtld_allocate_tls(NULL, TLS_TCB_SIZE, TLS_TCB_ALIGN);

	_tcb_set(tls);
#endif
}
