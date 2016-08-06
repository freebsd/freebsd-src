/*-
 * Copyright (c) 2012-2016 Robert N. M. Watson
 * Copyright (c) 2015 SRI International
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

#include <cheri/cheri.h>
#include <cheri/cheric.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cheri_class.h"
#include "cheri_invoke.h"
#include "cheri_system.h"
#include "cheri_type.h"
#include "sandbox.h"
#include "sandbox_elf.h"
#include "sandbox_internal.h"
#include "sandbox_methods.h"
#include "sandboxasm.h"

#define	roundup2(x, y)	(((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */

#define	GUARD_PAGE_SIZE	0x1000
#define	METADATA_SIZE	0x1000

CHERI_CLASS_DECL(libcheri_system);

int
sandbox_class_load(struct sandbox_class *sbcp)
{
	__capability void *codecap;
	int saved_errno;
	caddr_t base;

	/*
	 * Set up the code capability for a new sandbox class.  Very similar
	 * to object setup (i.e., guard pages for NULL, etc).
	 *
	 * Eventually, we will want to do something a bit different here -- in
	 * particular, set up the code and data capabilities quite differently.
	 */

	/*
	 * Ensure that we aren't going to map over NULL, guard pages, or
	 * metadata.  This check can probably be relaxed somewhat for
	 * the code capability, but for now it's right.
	 */
	if (sandbox_map_minoffset(sbcp->sbc_codemap) < SANDBOX_BINARY_BASE) {
		saved_errno = EINVAL;
		warnx("%s: sandbox wants to load below 0x%zx and 0x%zx",
		    __func__, (size_t)SANDBOX_BINARY_BASE,
		    sandbox_map_minoffset(sbcp->sbc_codemap));
		goto error;
	}

	sbcp->sbc_codelen = sandbox_map_maxoffset(sbcp->sbc_codemap);
	sbcp->sbc_codelen = roundup2(sbcp->sbc_codelen, PAGE_SIZE);
	base = sbcp->sbc_codemem = mmap(NULL, sbcp->sbc_codelen,
	    PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANON, -1, 0);
	if (sbcp->sbc_codemem == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap region", __func__);
		goto error;
	}
	if (mprotect(base, sbcp->sbc_codelen, PROT_NONE) == -1) {
		saved_errno = errno;
		warn("%s: mprotect region", __func__);
		goto error;
	}
	if (sandbox_map_load(base, sbcp->sbc_codemap) == -1) {
		saved_errno = EINVAL;
		warnx("%s: sandbox_map_load(sbc_codemap)\n", __func__);
		goto error;
	}

	/*
	 * Parse the sandbox ELF binary for CCall methods provided and
	 * required.
	 */
	if (sandbox_parse_ccall_methods(sbcp->sbc_fd,
	     &sbcp->sbc_provided_classes, &sbcp->sbc_required_methods) < 0) {
		saved_errno = EINVAL;
		warnx("%s: sandbox_parse_ccall_methods() failed for %s",
		    __func__, sbcp->sbc_path);
		goto error;
	}

	if (sandbox_map_protect(base, sbcp->sbc_codemap) == -1) {
		saved_errno = EINVAL;
		warnx("%s: sandbox_map_protect(sbc_codemap)\n", __func__);
		goto error;
	}

	/*
	 * Construct various class-related capabilities, such as the type,
	 * code capability for the run-time linker, and code capability for
	 * object-capability invocation.
	 */
	sbcp->sbc_typecap = cheri_type_alloc();

	/*
	 * Set bounds and mask permissions on code capabilities.
	 *
	 * XXXRW: In CheriABI, mmap(2) will return suitable bounds, so just
	 * mask permissions.  This is not well captured here.
	 *
	 * For both the MIPS ABI and CheriABI, we need to set suitable
	 * offsets.
	 *
	 * XXXRW: There are future questions to answer here about W^X and
	 * mmap(2) in CheriABI.
	 */
#ifdef __CHERI_PURE_CAPABILITY__
	codecap = cheri_andperm(sbcp->sbc_codemem,
	    CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | CHERI_PERM_EXECUTE);
#else
	codecap = cheri_codeptrperm(sbcp->sbc_codemem, sbcp->sbc_codelen,
	    CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | CHERI_PERM_EXECUTE);
#endif
	codecap = cheri_setoffset(codecap, SANDBOX_RTLD_VECTOR);
	sbcp->sbc_classcap_rtld = cheri_seal(codecap, sbcp->sbc_typecap);

#ifdef __CHERI_PURE_CAPABILITY__
	codecap = cheri_andperm(sbcp->sbc_codemem,
	    CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | CHERI_PERM_EXECUTE);
#else
	codecap = cheri_codeptrperm(sbcp->sbc_codemem, sbcp->sbc_codelen,
	    CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | CHERI_PERM_EXECUTE);
#endif
	codecap = cheri_setoffset(codecap, SANDBOX_INVOKE_VECTOR);
	sbcp->sbc_classcap_invoke = cheri_seal(codecap, sbcp->sbc_typecap);

	return (0);

error:
	if (sbcp->sbc_codemem != NULL)
		munmap(sbcp->sbc_codemem, sbcp->sbc_codelen);
	errno = saved_errno;
	return (-1);
}

void
sandbox_class_unload(struct sandbox_class *sbcp)
{

	munmap(sbcp->sbc_codemem, sbcp->sbc_codelen);
}

static struct cheri_object
cheri_system_object_for_instance(struct sandbox_object *sbop)
{
	struct cheri_object system_object;
	__capability void *codecap, *datacap;

	/*
	 * Construct an object capability for the system-class instance that
	 * will be passed into the sandbox.
	 *
	 * The code capability will simply be our $pcc.
	 *
	 * XXXRW: For now, we will populate $c0 with $pcc on invocation, so we
	 * need to leave a full set of permissions on it.  Eventually, we
	 * would prefer to limit this to LOAD and EXECUTE.
	 *
	 * XXXRW: We should do this once per class .. or even just once
	 * globally, rather than on every object creation.
	 *
	 * XXXRW: Use cheri_codeptr() here in the future?
	 */
	codecap = cheri_getpcc();
	codecap = cheri_setoffset(codecap,
	    (register_t)CHERI_CLASS_ENTRY(libcheri_system));
	system_object.co_codecap = cheri_seal(codecap, cheri_system_type);

	/*
	 * Construct a data capability describing the sandbox structure
	 * itself, which allows the system class to identify the sandbox a
	 * request is being issued from.  Embed saved $c0 as first field to
	 * allow the ambient MIPS environment to be installed.
	 */
	datacap = cheri_ptrperm(sbop, sizeof(*sbop), CHERI_PERM_GLOBAL |
	    CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP | CHERI_PERM_STORE |
	    CHERI_PERM_STORE_CAP | CHERI_PERM_STORE_LOCAL_CAP);
	assert(cheri_getoffset(datacap) == 0);
	assert(cheri_getlen(datacap) == sizeof(*sbop));
	system_object.co_datacap = cheri_seal(datacap, cheri_system_type);

	/*
	 * Return object capability.
	 *
	 * XXXRW: Possibly, this should be !CHERI_PERM_GLOBAL -- but we do not
	 * currently support invoking non-global objects.
	 */
	return (system_object);
}

int
sandbox_object_load(struct sandbox_class *sbcp, struct sandbox_object *sbop)
{
	__capability void *datacap;
	struct sandbox_metadata *sbmp;
	size_t length;
	size_t heaplen;
	size_t max_prog_offset;
#if CHERICAP_SIZE == 16
	ssize_t heaplen_adj;
#endif
	int saved_errno;
	caddr_t base;

	/*
	 * Perform an initial reservation of space for the sandbox, but using
	 * anonymous memory that is neither readable nor writable.  This
	 * ensures there is space for all the various segments we will be
	 * installing later.
	 *
	 * The rough sandbox memory map is as follows:
	 *
	 * J + 0x1000 [internal (non-shareable) heap]
	 * J          [guard page]
	 *  +0x600      Reserved vector
	 *  +0x400      Reserved vector
	 *  +0x200      Object-capability invocation vector
	 *  +0x0        Run-time linker vector
	 * 0x8000     [memory mapped binary]
	 * 0x2000     [guard page]
	 * 0x1000     [read-only sandbox metadata page]
	 * 0x0000     [guard page]
	 *
	 * Address constants in sandbox.h must be synchronised with the layout
	 * implemented here.  Location and contents of sandbox metadata is
	 * part of the ABI.
	 */

	/*
	 * Ensure that we aren't going to map over NULL, guard pages, or
	 * metadata.
	 */
	if (sandbox_map_minoffset(sbcp->sbc_datamap) < SANDBOX_BINARY_BASE) {
		saved_errno = EINVAL;
		warnx("%s: sandbox wants to load below 0x%zx and 0x%zx",
		    __func__, (size_t)SANDBOX_BINARY_BASE,
		    sandbox_map_minoffset(sbcp->sbc_datamap));
		goto error;
	}

	/* 0x0000 and metadata covered by maxoffset */
	length = roundup2(sandbox_map_maxoffset(sbcp->sbc_datamap), PAGE_SIZE)
	    + GUARD_PAGE_SIZE;

	/*
	 * Compartment data mappings are often quite large, so we may need to
	 * adjust up the effective heap size on 128-bit CHERI to shift the top
	 * of the data segment closer to a suitable alignment boundary for a
	 * sealed capability.
	 */
	heaplen = roundup2(sbop->sbo_heaplen, PAGE_SIZE);
#if CHERICAP_SIZE == 16
	heaplen_adj = length + heaplen;		/* Requested length. */
	heaplen_adj = roundup2(heaplen_adj, (1ULL << CHERI_SEAL_ALIGN_SHIFT(heaplen_adj))); /* Aligned len. */
	heaplen_adj -= (length + heaplen);	/* Calculate adjustment. */
	heaplen += heaplen_adj;			/* Apply adjustment. */
#endif
	length += heaplen;
	sbop->sbo_datalen = length;
	base = sbop->sbo_datamem = mmap(NULL, length, PROT_READ|PROT_WRITE,
	    MAP_ANON | MAP_ALIGNED_CHERI_SEAL, -1, 0);
	if (sbop->sbo_datamem == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap region", __func__);
		goto error;
	}
	if (mprotect(base, sbcp->sbc_codelen, PROT_NONE) == -1) {
		saved_errno = errno;
		warn("%s: mprotect region", __func__);
		goto error;
	}

	/*
	 * Assertions to make sure that we ended up with a well-aligned
	 * memory allocation as required for a precise set of bounds in the
	 * presence of compressed capabilities.  We don't check the
	 * pointer, but instead its base + offset, so as to be tolerant of
	 * both the MIPS ABI (where we are working from a $c0-based capability
	 * derived from a kernel-returned pointer) and CheriABI (where we are
	 * working from a capability directly returned by the kernel). All
	 * that matters here is the underlying virtual address.
	 */
#if CHERICAP_SIZE == 16
	assert(((cheri_getbase(base) + cheri_getoffset(base)) &
	    CHERI_SEAL_ALIGN_MASK(length)) == 0);
#endif

	/*
	 * Map and (eventually) link the program.
	 */
	if (sandbox_map_load(base, sbcp->sbc_datamap) == -1) {
		saved_errno = EINVAL;
		warnx("%s: sandbox_map_load(sbc_datamap)\n", __func__);
		goto error;
	}
	max_prog_offset = sandbox_map_maxoffset(sbcp->sbc_datamap);

	/*
	 * Skip guard page(s) to the base of the metadata structure.
	 */
	base += SANDBOX_METADATA_BASE;
	length -= SANDBOX_METADATA_BASE;

	/*
	 * Map metadata structure -- but can't fill it out until we have
	 * calculated all the other addresses involved.
	 */
	if ((sbmp = mmap(base, METADATA_SIZE, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_FIXED, -1, 0)) == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap metadata", __func__);
		goto error;
	}
	base += roundup2(METADATA_SIZE, PAGE_SIZE);
	length -= roundup2(METADATA_SIZE, PAGE_SIZE);

	/*
	 * Assert that we didn't bump into the sandbox entry address.  This
	 * address is hard to change as it is the address used in static
	 * linking for sandboxed code.
	 */
	assert((register_t)base - (register_t)sbop->sbo_datamem <
	    SANDBOX_BINARY_BASE);

	/*
	 * Skip already mapped binary.
	 */
	base = (caddr_t)sbop->sbo_datamem + roundup2(max_prog_offset, PAGE_SIZE);
	length = sbop->sbo_datalen - roundup2(max_prog_offset, PAGE_SIZE);

	/*
	 * Skip guard page.
	 */
	base += GUARD_PAGE_SIZE;
	length -= GUARD_PAGE_SIZE;

	/*
	 * Heap.
	 */
	sbop->sbo_heapbase = (register_t)base - (register_t)sbop->sbo_datamem;
	if (mmap(base, heaplen, PROT_READ | PROT_WRITE, MAP_ANON | MAP_FIXED,
	    -1, 0) == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap heap", __func__);
		goto error;
	}
	base += heaplen;
	length -= heaplen;

	/*
	 * There should not be too much, nor too little space remaining.  0
	 * is our Goldilocks number.
	 */
	assert(length == 0);

	/*
	 * Now that addresses are known, write out metadata for in-sandbox
	 * use.  The stack was configured by the higher-level object code, so
	 * all we do is install the capability here.
	 */
	sbmp->sbm_heapbase = sbop->sbo_heapbase;
	sbmp->sbm_heaplen = heaplen;
	sbmp->sbm_vtable = sandbox_make_vtable(sbop->sbo_datamem, NULL,
	    sbcp->sbc_provided_classes);
	sbmp->sbm_stackcap = sbop->sbo_stackcap;

	/*
	 * Construct data capability for run-time linker vector.
	 */
	datacap = cheri_ptrperm(sbop->sbo_datamem, sbop->sbo_datalen,
	    CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP |
	    CHERI_PERM_STORE | CHERI_PERM_STORE_CAP);
	assert(cheri_getoffset(datacap) == 0);
	assert(cheri_getlen(datacap) == sbop->sbo_datalen);
	sbop->sbo_cheri_object_rtld.co_codecap = sbcp->sbc_classcap_rtld;
	sbop->sbo_cheri_object_rtld.co_datacap = cheri_seal(datacap,
	    sbcp->sbc_typecap);

	/*
	 * Construct data capability for object-capability invocation vector.
	 */
	datacap = cheri_ptrperm(sbop->sbo_datamem, sbop->sbo_datalen,
	    CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP |
	    CHERI_PERM_STORE | CHERI_PERM_STORE_CAP);
	assert(cheri_getoffset(datacap) == 0);
	assert(cheri_getlen(datacap) == sbop->sbo_datalen);
	if (sandbox_set_required_method_variables(datacap,
	    sbcp->sbc_required_methods)
	    == -1) {
		saved_errno = EINVAL;
		warnx("%s: sandbox_set_ccaller_method_variables", __func__);
		goto error;
	}
	/*
	 * XXXBD: Ideally we would render the .CHERI_CCALLEE and
	 * .CHERI_CCALLER sections read-only at this point to avoid
	 * control flow attacks.
	 */
	sbop->sbo_cheri_object_invoke.co_codecap = sbcp->sbc_classcap_invoke;
	sbop->sbo_cheri_object_invoke.co_datacap = cheri_seal(datacap,
	    sbcp->sbc_typecap);

	/*
	 * XXXRW: At this point, it would be good to check the properties of
	 * all of the generated capabilities: seal bit, base, length,
	 * permissions, etc, for what is expected, and fail if not.
	 */

	/*
	 * Install a reference to the system object in the class.
	 */
	sbmp->sbm_system_object = sbop->sbo_cheri_object_system =
	    cheri_system_object_for_instance(sbop);

	/*
	 * Protect metadata now that we've written all values.
	 */
	if (mprotect(sbmp, METADATA_SIZE, PROT_READ) < 0) {
		saved_errno = errno;
		warn("%s: mprotect metadata", __func__);
		goto error;
	}
	return (0);

error:
	if (sbop->sbo_datamem != NULL)
		munmap(sbop->sbo_datamem, sbop->sbo_datalen);
	errno = saved_errno;
	return (-1);
}

int
sandbox_object_protect(struct sandbox_class *sbcp, struct sandbox_object *sbop)
{
	int saved_errno;

	if (sandbox_map_protect(sbop->sbo_datamem, sbcp->sbc_datamap) == -1) {
		saved_errno = EINVAL;
		warnx("%s: sandbox_map_protect(sbc_datamap)\n", __func__);
		errno = saved_errno;
		return (-1);
	}

	return (0);
}

/*
 * Reset the loader-managed address space to its start-time state.  Note that
 * this is not intended for use stand-alone, as sandbox_object_reset(), its
 * caller, is responsible for resetting the external stack(s).
 */
int
sandbox_object_reload(struct sandbox_object *sbop)
{
	int saved_errno;
	caddr_t base;
	size_t length;
	struct sandbox_class *sbcp;
	__capability void *datacap;

	assert(sbop != NULL);
	sbcp = sbop->sbo_sandbox_classp;
	assert(sbcp != NULL);

	if (sandbox_map_reload(sbop->sbo_datamem, sbcp->sbc_datamap) == -1) {
		saved_errno = EINVAL;
		warnx("%s:, sandbox_map_reset", __func__);
		goto error;
	}

	base = (caddr_t)sbop->sbo_datamem + sbop->sbo_heapbase;
	length = sbop->sbo_heaplen;
	if (mmap(base, length, PROT_READ | PROT_WRITE, MAP_ANON | MAP_FIXED,
	    -1, 0) == MAP_FAILED) {
		saved_errno = errno;
		warn("%s: mmap heap", __func__);
		goto error;
	}

	datacap = cheri_ptrperm(sbop->sbo_datamem, sbop->sbo_datalen,
	    CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP |
	    CHERI_PERM_STORE | CHERI_PERM_STORE_CAP |
	    CHERI_PERM_STORE_LOCAL_CAP);
	if (sandbox_set_required_method_variables(datacap,
	    sbcp->sbc_required_methods)
	    == -1) {
		saved_errno = EINVAL;
		warnx("%s: sandbox_set_ccaller_method_variables", __func__);
		goto error;
	}

	return (0);

error:
	if (sbop->sbo_datamem != NULL) {
		munmap(sbop->sbo_datamem, sbop->sbo_datalen);
		sbop->sbo_datamem = NULL;
	}
	errno = saved_errno;
	return (-1);
}

void
sandbox_object_unload(struct sandbox_object *sbop)
{
	struct sandbox_metadata *sbmp;

	sbmp = (void *)((char *)sbop->sbo_datamem +
	    SANDBOX_METADATA_BASE);
	free((void *)sbmp->sbm_vtable);

	munmap(sbop->sbo_datamem, sbop->sbo_datalen);
}
