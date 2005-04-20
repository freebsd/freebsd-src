/*-
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
 *
 *	$FreeBSD$
 */

/*
 * Define stubs for TLS internals so that programs and libraries can
 * link. These functions will be replaced by functional versions at
 * runtime from ld-elf.so.1.
 */

#include <stdlib.h>
#include <string.h>
#include <elf.h>
#include <assert.h>
#include "libc_private.h"

/* XXX not sure what variants to use for arm. */

#if defined(__ia64__) || defined(__alpha__) || defined(__powerpc__)
#define TLS_VARIANT_I
#endif
#if defined(__i386__) || defined(__amd64__) || defined(__sparc64__)
#define TLS_VARIANT_II
#endif

#ifndef PIC

#define round(size, align) \
	(((size) + (align) - 1) & ~((align) - 1))

static size_t tls_static_space;
static size_t tls_init_size;
#ifdef TLS_VARIANT_I
static size_t tls_init_offset;
#endif
static void *tls_init;

void _rtld_free_tls(void *tcb, size_t tcbsize, size_t tcbalign);
#endif

#ifdef __i386__

extern void *___tls_get_addr(void *ti) __attribute__((__regparm__(1)));

#pragma weak ___tls_get_addr
__attribute__((__regparm__(1)))
void *
___tls_get_addr(void *ti)
{
	return (0);
}

#endif

#pragma weak __tls_get_addr
void *
__tls_get_addr()
{
	return (0);
}

#ifdef TLS_VARIANT_I

#pragma weak _rtld_free_tls
/*
 * Free Static TLS using the Variant I method.
 */
void
_rtld_free_tls(void *tls, size_t tcbsize, size_t tcbalign)
{
#ifndef PIC
	Elf_Addr* dtv;

	dtv = ((Elf_Addr**)tls)[0];
	free(tls);
	free(dtv);
#endif
}

#pragma weak _rtld_allocate_tls
/*
 * Allocate Static TLS using the Variant I method.
 */
void *
_rtld_allocate_tls(void *oldtls, size_t tcbsize, size_t tcbalign)
{
#ifndef PIC
	size_t size;
	char *tls;
	Elf_Addr *dtv;

	size = tls_static_space;
	if (size < tcbsize)
		size = tcbsize;

	tls = malloc(size);
	dtv = malloc(3 * sizeof(Elf_Addr));

	*(Elf_Addr**) tls = dtv;

	dtv[0] = 1;
	dtv[1] = 1;
	dtv[2] = (Elf_Addr)(tls + tls_init_offset);
	if (oldtls) {
		/*
		 * Copy the static TLS block over whole.
		 */
		memcpy(tls + tls_init_offset,
		    (char*) oldtls + tls_init_offset,
		    tls_static_space - tls_init_offset);

		/*
		 * We assume that this block was the one we created with
		 * allocate_initial_tls().
		 */
		_rtld_free_tls(oldtls, 2*sizeof(Elf_Addr), sizeof(Elf_Addr));
	} else {
		memcpy(tls + tls_init_offset, tls_init, tls_init_size);
		memset(tls + tls_init_offset + tls_init_size,
		    0, tls_static_space - tls_init_size);
	}

	return tls;
#else
	return (0);
#endif
}

#endif

#ifdef TLS_VARIANT_II

/*
 * Free Static TLS using the Variant II method.
 */
#pragma weak _rtld_free_tls
void
_rtld_free_tls(void *tcb, size_t tcbsize, size_t tcbalign)
{
#ifndef PIC
	size_t size;
	Elf_Addr* dtv;
	Elf_Addr tlsstart, tlsend;

	/*
	 * Figure out the size of the initial TLS block so that we can
	 * find stuff which ___tls_get_addr() allocated dynamically.
	 */
	size = round(tls_static_space, tcbalign);

	dtv = ((Elf_Addr**)tcb)[1];
	tlsend = (Elf_Addr) tcb;
	tlsstart = tlsend - size;
	free((void*) tlsstart);
	free(dtv);
#endif
}

#pragma weak _rtld_allocate_tls
/*
 * Allocate Static TLS using the Variant II method.
 */
void *
_rtld_allocate_tls(void *oldtls, size_t tcbsize, size_t tcbalign)
{
#ifndef PIC
	size_t size;
	char *tls;
	Elf_Addr *dtv;
	Elf_Addr segbase, oldsegbase;

	size = round(tls_static_space, tcbalign);

	assert(tcbsize >= 2*sizeof(Elf_Addr));
	tls = malloc(size + tcbsize);
	dtv = malloc(3 * sizeof(Elf_Addr));

	segbase = (Elf_Addr)(tls + size);
	((Elf_Addr*)segbase)[0] = segbase;
	((Elf_Addr*)segbase)[1] = (Elf_Addr) dtv;

	dtv[0] = 1;
	dtv[1] = 1;
	dtv[2] = segbase - tls_static_space;

	if (oldtls) {
		/*
		 * Copy the static TLS block over whole.
		 */
		oldsegbase = (Elf_Addr) oldtls;
		memcpy((void *)(segbase - tls_static_space),
		    (const void *)(oldsegbase - tls_static_space),
		    tls_static_space);

		/*
		 * We assume that this block was the one we created with
		 * allocate_initial_tls().
		 */
		_rtld_free_tls(oldtls, 2*sizeof(Elf_Addr), sizeof(Elf_Addr));
	} else {
		memcpy((void *)(segbase - tls_static_space),
		    tls_init, tls_init_size);
		memset((void *)(segbase - tls_static_space + tls_init_size),
		    0, tls_static_space - tls_init_size);
	}

	return (void*) segbase;
#else
	return (0);
#endif
}

#endif

void
_init_tls()
{
#ifndef PIC
	extern char **environ;
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
	phdr = 0;
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
	if (phdr == 0 || phent != sizeof(Elf_Phdr) || phnum == 0)
		return;

	for (i = 0; i < phnum; i++) {
		if (phdr[i].p_type == PT_TLS) {
#ifdef TLS_VARIANT_I
			tls_static_space = round(2*sizeof(Elf_Addr),
			    phdr[i].p_align) + phdr[i].p_memsz;
			tls_init_offset = round(2*sizeof(Elf_Addr),
			    phdr[i].p_align);
#else			    
			tls_static_space = round(phdr[i].p_memsz,
			    phdr[i].p_align);
#endif
			tls_init_size = phdr[i].p_filesz;
			tls_init = (void*) phdr[i].p_vaddr;
		}
	}

	tls = _rtld_allocate_tls(NULL, 2*sizeof(Elf_Addr),
	    sizeof(Elf_Addr));

	_set_tp(tls);
#endif
}
