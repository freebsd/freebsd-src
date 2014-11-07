/*-
 * Copyright (c) 2012-2014 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#if !__has_feature(capabilities)
#error "This code requires a CHERI-aware compiler"
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libgen.h>
#include <sandbox_stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cheri_class.h"
#include "cheri_invoke.h"
#include "cheri_system.h"
#include "cheri_type.h"
#include "libcheri_stat.h"
#include "sandbox.h"
#include "sandbox_internal.h"
#include "sandboxasm.h"

#define	roundup2(x, y)	(((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */

#define	GUARD_PAGE_SIZE	0x1000
#define	METADATA_SIZE	0x1000
#define	STACK_SIZE	(32*PAGE_SIZE)

CHERI_CLASS_DECL(libcheri_system);

int
sandbox_object_load(struct sandbox_class *sbcp, struct sandbox_object *sbop)
{
	__capability void *codecap, *datacap, *typecap;
	struct sandbox_metadata *sbm;
	size_t length;
	int saved_errno;
	uint8_t *base;

	/*
	 * Perform an initial reservation of space for the sandbox, but using
	 * anonymous memory that is neither readable nor writable.  This
	 * ensures there is space for all the various segments we will be
	 * installing later.
	 *
	 * The rough sandbox memory map is as follows:
	 *
	 * K + 0x1000 [stack]
	 * K          [guard page]
	 * J + 0x1000 [heap]
	 * J          [guard page]
	 * 0x8000     [memory mapped binary] (SANDBOX_ENTRY)
	 * 0x2000     [guard page]
	 * 0x1000     [read-only sandbox metadata page]
	 * 0x0000     [guard page]
	 *
	 * Address constants in sandbox.h must be synchronised with the layout
	 * implemented here.  Location and contents of sandbox metadata is
	 * part of the ABI.
	 */
	length = sbcp->sbc_sandboxlen;
	base = sbop->sbo_mem = mmap(NULL, length, 0, MAP_ANON, -1, 0);
	if (sbop->sbo_mem == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap region", __func__);
		goto error;
	}

	/*
	 * Skip guard page(s) to the base of the metadata structure.
	 */
	base += SANDBOX_METADATA_BASE;
	length -= SANDBOX_METADATA_BASE;

	/*
	 * Map metadata structure -- but can't fill it out until we have
	 * calculated all the other addresses involved.
	 */
	if ((sbm = mmap(base, METADATA_SIZE, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_FIXED, -1, 0)) == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap metadata", __func__);
		goto error;
	}

	/*
	 * Skip forward to the mapping location for the binary -- in case we
	 * add more metadata in the future.  Assert that we didn't bump into
	 * the sandbox entry address.  This address is hard to change as it is
	 * the address used in static linking for sandboxed code.
	 */
	assert((register_t)base - (register_t)sbop->sbo_mem < SANDBOX_ENTRY);
	base = (void *)((register_t)sbop->sbo_mem + SANDBOX_ENTRY);
	length = sbcp->sbc_sandboxlen - SANDBOX_ENTRY;

	/*
	 * Map program binary.
	 */
	if (mmap(base, sbcp->sbc_stat.st_size, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_FIXED | MAP_PREFAULT_READ, sbcp->sbc_fd, 0) ==
	    MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap %s", __func__, sbcp->sbc_path);
		goto error;
	}
	base += roundup2(sbcp->sbc_stat.st_size, PAGE_SIZE);
	length -= roundup2(sbcp->sbc_stat.st_size, PAGE_SIZE);

	/*
	 * Skip guard page.
	 */
	base += GUARD_PAGE_SIZE;
	length -= GUARD_PAGE_SIZE;

	/*
	 * Heap.
	 */
	sbop->sbo_heapbase = (register_t)base - (register_t)sbop->sbo_mem;
	sbop->sbo_heaplen = length - (GUARD_PAGE_SIZE + STACK_SIZE);
	if (mmap(base, sbop->sbo_heaplen, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_FIXED, -1, 0) == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap heap", __func__);
		goto error;
	}
	memset(base, 0, sbop->sbo_heaplen);
	base += sbop->sbo_heaplen;
	length -= sbop->sbo_heaplen;

	/*
	 * Skip guard page.
	 */
	base += GUARD_PAGE_SIZE;
	length -= GUARD_PAGE_SIZE;

	/*
	 * Stack.
	 */
	if (mmap(base, length, PROT_READ | PROT_WRITE, MAP_ANON | MAP_FIXED,
	    -1, 0) == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap stack", __func__);
		goto error;
	}
	memset(base, 0, length);
	base += STACK_SIZE;
	length -= STACK_SIZE;

	/*
	 * There should not be too much, nor too little space remaining.  0
	 * is our Goldilocks number.
	 */
	assert(length == 0);

	/*
	 * Now that addresses are known, write out metadata for in-sandbox
	 * use; then mprotect() so that it can't be modified by the sandbox.
	 */
	sbm->sbm_heapbase = sbop->sbo_heapbase;
	sbm->sbm_heaplen = sbop->sbo_heaplen;
	if (mprotect(base, METADATA_SIZE, PROT_READ) < 0) {
		saved_errno = errno;
		warn("%s: mprotect metadata", __func__);
		goto error;
	}

	if (sbcp->sbc_sandbox_class_statp != NULL) {
		(void)sandbox_stat_object_register(
		    &sbop->sbo_sandbox_object_statp,
		    sbcp->sbc_sandbox_class_statp,
		    SANDBOX_OBJECT_TYPE_POINTER, (uintptr_t)sbop->sbo_mem);
		SANDBOX_CLASS_ALLOC(sbcp->sbc_sandbox_class_statp);
	}

	typecap = cheri_type_alloc();

	/*
	 * Construct code capability.
	 *
	 * XXXRW: Do we really need CHERI_PERM_LOAD?
	 */
	codecap = cheri_ptrperm(sbop->sbo_mem, sbcp->sbc_sandboxlen,
	    CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | CHERI_PERM_EXECUTE);
	codecap = cheri_setoffset(codecap, SANDBOX_ENTRY);
	sbop->sbo_cheri_object.co_codecap = cheri_seal(codecap, typecap);

	/*
	 * Construct data capability.  For now, allow storing local
	 * capabilities ... but we will stop doing this once the stack
	 * capability stops being derived from the data capability on entry.
	 */
	datacap = cheri_ptrperm(sbop->sbo_mem, sbcp->sbc_sandboxlen,
	    CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP |
	    CHERI_PERM_STORE | CHERI_PERM_STORE_CAP |
	    CHERI_PERM_STORE_LOCAL_CAP);
	sbop->sbo_cheri_object.co_datacap = cheri_seal(datacap, typecap);

	/*
	 * Construct an object capability for the system-class instance that
	 * will be passed into the sandbox.
	 */
	typecap = cheri_system_type;

	/*
	 * The code capability will simply be our $pcc.
	 *
	 * XXXRW: For now, we will populate $c0 with $pcc on invocation, so we
	 * need to leave a full set of permissions on it.  Eventually, we
	 * would prefer to limit this to LOAD and EXECUTE.
	 */
	codecap = cheri_getpcc();
	codecap = cheri_setoffset(codecap,
	    (register_t)CHERI_CLASS_ENTRY(libcheri_system));
	sbop->sbo_cheri_system_object.co_codecap = cheri_seal(codecap,
	    typecap);

	/*
	 * Construct a data capability describing the sandbox structure
	 * itself, which allows the system class to identify the sandbox a
	 * request is being issued from.
	 */
	datacap = cheri_ptrperm(sbop, sizeof(*sbop), CHERI_PERM_GLOBAL |
	    CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP | CHERI_PERM_STORE |
	    CHERI_PERM_STORE_CAP | CHERI_PERM_STORE_LOCAL_CAP);
	sbop->sbo_cheri_system_object.co_datacap = cheri_seal(datacap,
	    typecap);
	return (0);

error:
	if (sbop->sbo_mem != NULL)
		munmap(sbop->sbo_mem, sbcp->sbc_sandboxlen);
	errno = saved_errno;
	return (-1);
}

void
sandbox_object_unload(struct sandbox_class *sbcp, struct sandbox_object *sbop)
{

	munmap(sbop->sbo_mem, sbcp->sbc_sandboxlen);
}

void *
sandbox_object_getbase(struct sandbox_object *sbop)
{

	return (sbop->sbo_mem);
}

size_t
sandbox_class_getlength(struct sandbox_class *sbcp) 
{

	return (sbcp->sbc_sandboxlen);
}
