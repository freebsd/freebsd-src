/*      $NetBSD: ppc_reloc.c,v 1.10 2001/09/10 06:09:41 mycroft Exp $   */

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 1998   Tsubai Masanari
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/sysctl.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <machine/cpu.h>
#include <machine/md_var.h>

#include "debug.h"
#include "rtld.h"

#if !defined(_CALL_ELF) || _CALL_ELF == 1
struct funcdesc {
	Elf_Addr addr;
	Elf_Addr toc;
	Elf_Addr env;
};
#endif

/*
 * Process the R_PPC_COPY relocations
 */
int
do_copy_relocations(Obj_Entry *dstobj)
{
	const Elf_Rela *relalim;
	const Elf_Rela *rela;

	/*
	 * COPY relocs are invalid outside of the main program
	 */
	assert(dstobj->mainprog);

	relalim = (const Elf_Rela *)((const char *) dstobj->rela +
	    dstobj->relasize);
	for (rela = dstobj->rela;  rela < relalim;  rela++) {
		void *dstaddr;
		const Elf_Sym *dstsym;
		const char *name;
		size_t size;
		const void *srcaddr;
		const Elf_Sym *srcsym = NULL;
		const Obj_Entry *srcobj, *defobj;
		SymLook req;
		int res;

		if (ELF_R_TYPE(rela->r_info) != R_PPC_COPY) {
			continue;
		}

		dstaddr = (void *)(dstobj->relocbase + rela->r_offset);
		dstsym = dstobj->symtab + ELF_R_SYM(rela->r_info);
		name = dstobj->strtab + dstsym->st_name;
		size = dstsym->st_size;
		symlook_init(&req, name);
		req.ventry = fetch_ventry(dstobj, ELF_R_SYM(rela->r_info));
		req.flags = SYMLOOK_EARLY;

		for (srcobj = globallist_next(dstobj); srcobj != NULL;
		     srcobj = globallist_next(srcobj)) {
			res = symlook_obj(&req, srcobj);
			if (res == 0) {
				srcsym = req.sym_out;
				defobj = req.defobj_out;
				break;
			}
		}

		if (srcobj == NULL) {
			_rtld_error("Undefined symbol \"%s\" "
				    " referenced from COPY"
				    " relocation in %s", name, dstobj->path);
			return (-1);
		}

		srcaddr = (const void *)(defobj->relocbase+srcsym->st_value);
		memcpy(dstaddr, srcaddr, size);
		dbg("copy_reloc: src=%p,dst=%p,size=%zd\n",srcaddr,dstaddr,size);
	}

	return (0);
}


/*
 * Perform early relocation of the run-time linker image
 */
void
reloc_non_plt_self(Elf_Dyn *dynp, Elf_Addr relocbase)
{
	const Elf_Rela *rela = NULL, *relalim;
	Elf_Addr relasz = 0;
	Elf_Addr *where;

	/*
	 * Extract the rela/relasz values from the dynamic section
	 */
	for (; dynp->d_tag != DT_NULL; dynp++) {
		switch (dynp->d_tag) {
		case DT_RELA:
			rela = (const Elf_Rela *)(relocbase+dynp->d_un.d_ptr);
			break;
		case DT_RELASZ:
			relasz = dynp->d_un.d_val;
			break;
		}
	}

	/*
	 * Relocate these values
	 */
	relalim = (const Elf_Rela *)((const char *)rela + relasz);
	for (; rela < relalim; rela++) {
		where = (Elf_Addr *)(relocbase + rela->r_offset);
		*where = (Elf_Addr)(relocbase + rela->r_addend);
	}
}


/*
 * Relocate a non-PLT object with addend.
 */
static int
reloc_nonplt_object(Obj_Entry *obj_rtld __unused, Obj_Entry *obj,
    const Elf_Rela *rela, SymCache *cache, int flags, RtldLockState *lockstate)
{
	const Elf_Sym	*def = NULL;
	const Obj_Entry	*defobj;
	Elf_Addr	*where, symval = 0;

	/*
	 * First, resolve symbol for relocations which
	 * reference symbols.
	 */
	switch (ELF_R_TYPE(rela->r_info)) {

	case R_PPC64_UADDR64:    /* doubleword64 S + A */
	case R_PPC64_ADDR64:
	case R_PPC_GLOB_DAT:
	case R_PPC64_DTPMOD64:
	case R_PPC64_TPREL64:
	case R_PPC64_DTPREL64:
		def = find_symdef(ELF_R_SYM(rela->r_info), obj, &defobj,
		    flags, cache, lockstate);
		if (def == NULL) {
			return (-1);
		}
		/*
		 * If symbol is IFUNC, only perform relocation
		 * when caller allowed it by passing
		 * SYMLOOK_IFUNC flag.  Skip the relocations
		 * otherwise.
		 *
		 * Also error out in case IFUNC relocations
		 * are specified for TLS, which cannot be
		 * usefully interpreted.
		 */
		if (ELF_ST_TYPE(def->st_info) == STT_GNU_IFUNC) {
			switch (ELF_R_TYPE(rela->r_info)) {
			case R_PPC64_UADDR64:
			case R_PPC64_ADDR64:
			case R_PPC_GLOB_DAT:
				if ((flags & SYMLOOK_IFUNC) == 0) {
					dbg("Non-PLT reference to IFUNC found!");
					obj->non_plt_gnu_ifunc = true;
					return (0);
				}
				symval = (Elf_Addr)rtld_resolve_ifunc(
					defobj, def);
				break;
			default:
				_rtld_error("%s: IFUNC for TLS reloc",
					 obj->path);
				return (-1);
			}
		} else {
			if ((flags & SYMLOOK_IFUNC) != 0)
				return (0);
			symval = (Elf_Addr)defobj->relocbase +
				def->st_value;
		}
		break;
	default:
		if ((flags & SYMLOOK_IFUNC) != 0)
			return (0);
	}

	where = (Elf_Addr *)(obj->relocbase + rela->r_offset);

	switch (ELF_R_TYPE(rela->r_info)) {
	case R_PPC_NONE:
		break;
	case R_PPC64_UADDR64:
	case R_PPC64_ADDR64:
	case R_PPC_GLOB_DAT:
		/* Don't issue write if unnecessary; avoid COW page fault */
		if (*where != symval + rela->r_addend) {
			*where = symval + rela->r_addend;
		}
		break;
	case R_PPC64_DTPMOD64:
		*where = (Elf_Addr) defobj->tlsindex;
		break;
	case R_PPC64_TPREL64:
		/*
		 * We lazily allocate offsets for static TLS as we
		 * see the first relocation that references the
		 * TLS block. This allows us to support (small
		 * amounts of) static TLS in dynamically loaded
		 * modules. If we run out of space, we generate an
		 * error.
		 */
		if (!defobj->tls_static) {
			if (!allocate_tls_offset(
				    __DECONST(Obj_Entry *, defobj))) {
				_rtld_error("%s: No space available for static "
				    "Thread Local Storage", obj->path);
				return (-1);
			}
		}

		*(Elf_Addr **)where = *where * sizeof(Elf_Addr)
		    + (Elf_Addr *)(def->st_value + rela->r_addend
		    + defobj->tlsoffset - TLS_TP_OFFSET - TLS_TCB_SIZE);
		break;
	case R_PPC64_DTPREL64:
		*where += (Elf_Addr)(def->st_value + rela->r_addend
		    - TLS_DTV_OFFSET);
		break;
	case R_PPC_RELATIVE:  /* doubleword64 B + A */
		symval = (Elf_Addr)(obj->relocbase + rela->r_addend);

		/* As above, don't issue write unnecessarily */
		if (*where != symval) {
			*where = symval;
		}
		break;
	case R_PPC_COPY:
		/*
		 * These are deferred until all other relocations
		 * have been done.  All we do here is make sure
		 * that the COPY relocation is not in a shared
		 * library.  They are allowed only in executable
		 * files.
		 */
		if (!obj->mainprog) {
			_rtld_error("%s: Unexpected R_COPY "
				    " relocation in shared library",
				    obj->path);
			return (-1);
		}
		break;
	case R_PPC_IRELATIVE:
		/*
		 * These will be handled by reloc_iresolve().
		 */
		obj->irelative = true;
		break;
	case R_PPC_JMP_SLOT:
		/*
		 * These will be handled by the plt/jmpslot routines
		 */
		break;

	default:
		_rtld_error("%s: Unsupported relocation type %ld"
			    " in non-PLT relocations\n", obj->path,
			    ELF_R_TYPE(rela->r_info));
		return (-1);
	}
	return (0);
}


/*
 * Process non-PLT relocations
 */
int
reloc_non_plt(Obj_Entry *obj, Obj_Entry *obj_rtld, int flags,
    RtldLockState *lockstate)
{
	const Elf_Rela *relalim;
	const Elf_Rela *rela;
	const Elf_Phdr *phdr;
	SymCache *cache;
	int bytes = obj->dynsymcount * sizeof(SymCache);
	int r = -1;

	/*
	 * The dynamic loader may be called from a thread, we have
	 * limited amounts of stack available so we cannot use alloca().
	 */
	if (obj != obj_rtld) {
		cache = mmap(NULL, bytes, PROT_READ|PROT_WRITE, MAP_ANON,
		    -1, 0);
		if (cache == MAP_FAILED)
			cache = NULL;
	} else
		cache = NULL;

	/*
	 * From the SVR4 PPC ABI:
	 * "The PowerPC family uses only the Elf32_Rela relocation
	 *  entries with explicit addends."
	 */
	relalim = (const Elf_Rela *)((const char *)obj->rela + obj->relasize);
	for (rela = obj->rela; rela < relalim; rela++) {
		if (reloc_nonplt_object(obj_rtld, obj, rela, cache, flags,
		    lockstate) < 0)
			goto done;
	}
	r = 0;
done:
	if (cache)
		munmap(cache, bytes);

	/*
	 * Synchronize icache for executable segments in case we made
	 * any changes.
	 */
	for (phdr = obj->phdr;
	    (const char *)phdr < (const char *)obj->phdr + obj->phsize;
	    phdr++) {
		if (phdr->p_type == PT_LOAD && (phdr->p_flags & PF_X) != 0) {
			__syncicache(obj->relocbase + phdr->p_vaddr,
			    phdr->p_memsz);
		}
	}

	return (r);
}


/*
 * Initialise a PLT slot to the resolving trampoline
 */
static int
reloc_plt_object(Obj_Entry *obj, const Elf_Rela *rela)
{
	Elf_Addr *where = (Elf_Addr *)(obj->relocbase + rela->r_offset);
	long reloff;

	reloff = rela - obj->pltrela;

	dbg(" reloc_plt_object: where=%p,reloff=%lx,glink=%#lx", (void *)where,
	    reloff, obj->glink);

#if !defined(_CALL_ELF) || _CALL_ELF == 1
	/* Glink code is 3 instructions after the first 32k, 2 before */
	*where = (Elf_Addr)obj->glink + 32 + 
	    8*((reloff < 0x8000) ? reloff : 0x8000) + 
	    12*((reloff < 0x8000) ? 0 : (reloff - 0x8000));
#else
	/* 64-Bit ELF V2 ABI Specification, sec. 4.2.5.3. */
	*where = (Elf_Addr)obj->glink + 4*reloff + 32;
#endif

	return (0);
}

/*
 * Process the PLT relocations.
 */
int
reloc_plt(Obj_Entry *obj, int flags __unused, RtldLockState *lockstate __unused)
{
	const Elf_Rela *relalim;
	const Elf_Rela *rela;

	if (obj->pltrelasize != 0) {
		relalim = (const Elf_Rela *)((const char *)obj->pltrela +
		    obj->pltrelasize);
		for (rela = obj->pltrela;  rela < relalim;  rela++) {

#if defined(_CALL_ELF) && _CALL_ELF == 2
			if (ELF_R_TYPE(rela->r_info) == R_PPC_IRELATIVE) {
				dbg("ABI violation - found IRELATIVE in the PLT.");
				obj->irelative = true;
				continue;
			}
#endif
			/*
			 * PowerPC(64) .rela.plt is composed of an array of
			 * R_PPC_JMP_SLOT relocations. Unlike other platforms,
			 * this is the ONLY relocation type that is valid here.
			 */
			assert(ELF_R_TYPE(rela->r_info) == R_PPC_JMP_SLOT);

			if (reloc_plt_object(obj, rela) < 0) {
				return (-1);
			}
		}
	}

	return (0);
}

/*
 * LD_BIND_NOW was set - force relocation for all jump slots
 */
int
reloc_jmpslots(Obj_Entry *obj, int flags, RtldLockState *lockstate)
{
	const Obj_Entry *defobj;
	const Elf_Rela *relalim;
	const Elf_Rela *rela;
	const Elf_Sym *def;
	Elf_Addr *where;
	Elf_Addr target;

	relalim = (const Elf_Rela *)((const char *)obj->pltrela +
	    obj->pltrelasize);
	for (rela = obj->pltrela; rela < relalim; rela++) {
		/* This isn't actually a jump slot, ignore it. */
		if (ELF_R_TYPE(rela->r_info) == R_PPC_IRELATIVE)
			continue;
		assert(ELF_R_TYPE(rela->r_info) == R_PPC_JMP_SLOT);
		where = (Elf_Addr *)(obj->relocbase + rela->r_offset);
		def = find_symdef(ELF_R_SYM(rela->r_info), obj, &defobj,
		    SYMLOOK_IN_PLT | flags, NULL, lockstate);
		if (def == NULL) {
			dbg("reloc_jmpslots: sym not found");
			return (-1);
		}

		target = (Elf_Addr)(defobj->relocbase + def->st_value);

		if (def == &sym_zero) {
			/* Zero undefined weak symbols */
#if !defined(_CALL_ELF) || _CALL_ELF == 1
			bzero(where, sizeof(struct funcdesc));
#else
			*where = 0;
#endif
		} else {
			if (ELF_ST_TYPE(def->st_info) == STT_GNU_IFUNC) {
				/* LD_BIND_NOW, ifunc in shared lib.*/
				obj->gnu_ifunc = true;
				continue;
			}
			reloc_jmpslot(where, target, defobj, obj,
			    (const Elf_Rel *) rela);
		}
	}

	obj->jmpslots_done = true;

	return (0);
}


/*
 * Update the value of a PLT jump slot.
 */
Elf_Addr
reloc_jmpslot(Elf_Addr *wherep, Elf_Addr target, const Obj_Entry *defobj __unused,
    const Obj_Entry *obj __unused, const Elf_Rel *rel __unused)
{

	/*
	 * At the PLT entry pointed at by `wherep', construct
	 * a direct transfer to the now fully resolved function
	 * address.
	 */

#if !defined(_CALL_ELF) || _CALL_ELF == 1
	dbg(" reloc_jmpslot: where=%p, target=%p (%#lx + %#lx)",
	    (void *)wherep, (void *)target, *(Elf_Addr *)target,
	    (Elf_Addr)defobj->relocbase);

	if (ld_bind_not)
		goto out;

	/*
	 * For the trampoline, the second two elements of the function
	 * descriptor are unused, so we are fine replacing those at any time
	 * with the real ones with no thread safety implications. However, we
	 * need to make sure the main entry point pointer ([0]) is seen to be
	 * modified *after* the second two elements. This can't be done in
	 * general, since there are no barriers in the reading code, but put in
	 * some isyncs to at least make it a little better.
	 */
	memcpy(wherep, (void *)target, sizeof(struct funcdesc));
	wherep[2] = ((Elf_Addr *)target)[2];
	wherep[1] = ((Elf_Addr *)target)[1];
	__asm __volatile ("isync" : : : "memory");
	wherep[0] = ((Elf_Addr *)target)[0];
	__asm __volatile ("isync" : : : "memory");

	if (((struct funcdesc *)(wherep))->addr < (Elf_Addr)defobj->relocbase) {
		/*
		 * It is possible (LD_BIND_NOW) that the function
		 * descriptor we are copying has not yet been relocated.
		 * If this happens, fix it. Don't worry about threading in
		 * this case since LD_BIND_NOW makes it irrelevant.
		 */

		((struct funcdesc *)(wherep))->addr +=
		    (Elf_Addr)defobj->relocbase;
		((struct funcdesc *)(wherep))->toc +=
		    (Elf_Addr)defobj->relocbase;
	}
#else
	dbg(" reloc_jmpslot: where=%p, target=%p", (void *)wherep,
	    (void *)target);

	assert(target >= (Elf_Addr)defobj->relocbase);

	if (ld_bind_not)
		goto out;

	if (*wherep != target)
		*wherep = target;

#endif
out:

	return (target);
}

int
reloc_iresolve(Obj_Entry *obj,
    struct Struct_RtldLockState *lockstate)
{
	/*
	 * Since PLT slots on PowerPC64 are always R_PPC_JMP_SLOT,
	 * R_PPC_IRELATIVE is in RELA.
	 */
#if !defined(_CALL_ELF) || _CALL_ELF == 1
	(void)(obj);
	(void)(lockstate);
	/* XXX not implemented */
	return (0);
#else
	const Elf_Rela *relalim;
	const Elf_Rela *rela;
	Elf_Addr *where, target, *ptr;

	if (!obj->irelative)
		return (0);

	relalim = (const Elf_Rela *)((const char *)obj->rela + obj->relasize);
	for (rela = obj->rela;  rela < relalim;  rela++) {
		if (ELF_R_TYPE(rela->r_info) == R_PPC_IRELATIVE) {
			ptr = (Elf_Addr *)(obj->relocbase + rela->r_addend);
			where = (Elf_Addr *)(obj->relocbase + rela->r_offset);

			lock_release(rtld_bind_lock, lockstate);
			target = call_ifunc_resolver(ptr);
			wlock_acquire(rtld_bind_lock, lockstate);

			*where = target;
		}
	}
	/*
	 * XXX Remove me when lld is fixed!
	 * LLD currently makes illegal relocations in the PLT.
	 */
        relalim = (const Elf_Rela *)((const char *)obj->pltrela + obj->pltrelasize);
        for (rela = obj->pltrela;  rela < relalim;  rela++) {
                if (ELF_R_TYPE(rela->r_info) == R_PPC_IRELATIVE) {
                        ptr = (Elf_Addr *)(obj->relocbase + rela->r_addend);
                        where = (Elf_Addr *)(obj->relocbase + rela->r_offset);

                        lock_release(rtld_bind_lock, lockstate);
                        target = call_ifunc_resolver(ptr);
                        wlock_acquire(rtld_bind_lock, lockstate);

                        *where = target;
                }
        }

	obj->irelative = false;
	return (0);
#endif
}

int
reloc_gnu_ifunc(Obj_Entry *obj __unused, int flags __unused,
    struct Struct_RtldLockState *lockstate __unused)
{
#if !defined(_CALL_ELF) || _CALL_ELF == 1
	_rtld_error("reloc_gnu_ifunc(): Not implemented!");
	/* XXX not implemented */
	return (-1);
#else

	const Elf_Rela *relalim;
	const Elf_Rela *rela;
	Elf_Addr *where, target;
	const Elf_Sym *def;
	const Obj_Entry *defobj;

	if (!obj->gnu_ifunc)
		return (0);
	relalim = (const Elf_Rela *)((const char *)obj->pltrela + obj->pltrelasize);
	for (rela = obj->pltrela;  rela < relalim;  rela++) {
		if (ELF_R_TYPE(rela->r_info) == R_PPC_JMP_SLOT) {
			where = (Elf_Addr *)(obj->relocbase + rela->r_offset);
			def = find_symdef(ELF_R_SYM(rela->r_info), obj, &defobj,
			    SYMLOOK_IN_PLT | flags, NULL, lockstate);
			if (def == NULL)
				return (-1);
			if (ELF_ST_TYPE(def->st_info) != STT_GNU_IFUNC)
				continue;
			lock_release(rtld_bind_lock, lockstate);
			target = (Elf_Addr)rtld_resolve_ifunc(defobj, def);
			wlock_acquire(rtld_bind_lock, lockstate);
			reloc_jmpslot(where, target, defobj, obj,
			    (const Elf_Rel *)rela);
		}
	}
	obj->gnu_ifunc = false;
	return (0);
#endif
}

int
reloc_iresolve_nonplt(Obj_Entry *obj __unused,
    struct Struct_RtldLockState *lockstate __unused)
{
	return (0);
}

void
init_pltgot(Obj_Entry *obj)
{
	Elf_Addr *pltcall;

	pltcall = obj->pltgot;

	if (pltcall == NULL) {
		return;
	}

#if defined(_CALL_ELF) && _CALL_ELF == 2
	pltcall[0] = (Elf_Addr)&_rtld_bind_start; 
	pltcall[1] = (Elf_Addr)obj;
#else
	memcpy(pltcall, _rtld_bind_start, sizeof(struct funcdesc));
	pltcall[2] = (Elf_Addr)obj;
#endif
}

/*
 * Actual values are 32 bit.
 */
u_long cpu_features;
u_long cpu_features2;

void
powerpc64_abi_variant_hook(Elf_Auxinfo** aux_info)
{
	/*
	 * Since aux_info[] is easier to work with than aux, go ahead and
	 * initialize cpu_features / cpu_features2.
	 */
	cpu_features = -1UL;
	cpu_features2 = -1UL;
	if (aux_info[AT_HWCAP] != NULL)
		cpu_features = (uint32_t)aux_info[AT_HWCAP]->a_un.a_val;
	if (aux_info[AT_HWCAP2] != NULL)
		cpu_features2 = (uint32_t)aux_info[AT_HWCAP2]->a_un.a_val;
}

void
ifunc_init(Elf_Auxinfo aux_info[__min_size(AT_COUNT)] __unused)
{

}

void
allocate_initial_tls(Obj_Entry *list)
{

	/*
	* Fix the size of the static TLS block by using the maximum
	* offset allocated so far and adding a bit for dynamic modules to
	* use.
	*/

	tls_static_space = tls_last_offset + tls_last_size + RTLD_STATIC_TLS_EXTRA;

	_tcb_set(allocate_tls(list, NULL, TLS_TCB_SIZE, TLS_TCB_ALIGN));
}

void*
__tls_get_addr(tls_index* ti)
{
	uintptr_t **dtvp;
	char *p;

	dtvp = &_tcb_get()->tcb_dtv;
	p = tls_get_addr_common(dtvp, ti->ti_module, ti->ti_offset);

	return (p + TLS_DTV_OFFSET);
}
