/*      $NetBSD: ppc_reloc.c,v 1.10 2001/09/10 06:09:41 mycroft Exp $   */

/*-
 * SPDX-License-Identifier: BSD-2-Clause-NetBSD
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <machine/cpu.h>
#include <machine/atomic.h>
#include <machine/md_var.h>

#include "debug.h"
#include "rtld.h"

#define _ppc_ha(x) ((((u_int32_t)(x) & 0x8000) ? \
                        ((u_int32_t)(x) + 0x10000) : (u_int32_t)(x)) >> 16)
#define _ppc_la(x) ((u_int32_t)(x) & 0xffff)

#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))

#define PLT_EXTENDED_BEGIN	(1 << 13)
#define JMPTAB_BASE(N)		(18 + N*2 + ((N > PLT_EXTENDED_BEGIN) ? \
				    (N - PLT_EXTENDED_BEGIN)*2 : 0))

void _rtld_bind_secureplt_start(void);

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
		dbg("copy_reloc: src=%p,dst=%p,size=%d\n",srcaddr,dstaddr,size);
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

	case R_PPC_UADDR32:    /* word32 S + A */
	case R_PPC_ADDR32:
	case R_PPC_GLOB_DAT:  /* word32 S + A */
	case R_PPC_DTPMOD32:
	case R_PPC_TPREL32:
	case R_PPC_DTPREL32:
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
			case R_PPC_UADDR32:
			case R_PPC_ADDR32:
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
	case R_PPC_UADDR32:
	case R_PPC_ADDR32:
	case R_PPC_GLOB_DAT:
		/* Don't issue write if unnecessary; avoid COW page fault */
		if (*where != symval + rela->r_addend) {
			*where = symval + rela->r_addend;
		}
		break;
	case R_PPC_DTPMOD32:
		*where = (Elf_Addr) defobj->tlsindex;
		break;
	case R_PPC_TPREL32:
		/*
		 * We lazily allocate offsets for static TLS as we
		 * see the first relocation that references the
		 * TLS block. This allows us to support (small
		 * amounts of) static TLS in dynamically loaded
		 * modules. If we run out of space, we generate an
		 * error.
		 */
		if (!defobj->tls_done) {
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
	case R_PPC_DTPREL32:
		*where += (Elf_Addr)(def->st_value + rela->r_addend 
		    - TLS_DTV_OFFSET);
		break;
	case R_PPC_RELATIVE: /* word32 B + A */
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
		_rtld_error("%s: Unsupported relocation type %d"
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
	int r = -1;

	/*
	 * The dynamic loader may be called from a thread, we have
	 * limited amounts of stack available so we cannot use alloca().
	 */
	if (obj != obj_rtld) {
		cache = calloc(obj->dynsymcount, sizeof(SymCache));
		/* No need to check for NULL here */
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
	if (cache != NULL)
		free(cache);

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
	Elf_Word *where = (Elf_Word *)(obj->relocbase + rela->r_offset);
	Elf_Addr *pltresolve, *pltlongresolve, *jmptab;
	Elf_Addr distance;
	int N = obj->pltrelasize / sizeof(Elf_Rela);
	int reloff;

	reloff = rela - obj->pltrela;

	if (reloff < 0)
		return (-1);

	if (obj->gotptr != NULL) {
		*where += (Elf_Addr)obj->relocbase;
		return (0);
	}

	pltlongresolve = obj->pltgot + 5;
	pltresolve = pltlongresolve + 5;

	distance = (Elf_Addr)pltresolve - (Elf_Addr)(where + 1);

	dbg(" reloc_plt_object: where=%p,pltres=%p,reloff=%x,distance=%x",
	    (void *)where, (void *)pltresolve, reloff, distance);

	if (reloff < PLT_EXTENDED_BEGIN) {
		/* li   r11,reloff  */
		/* b    pltresolve  */
		where[0] = 0x39600000 | reloff;
		where[1] = 0x48000000 | (distance & 0x03fffffc);
	} else {
		jmptab = obj->pltgot + JMPTAB_BASE(N);
		jmptab[reloff] = (u_int)pltlongresolve;

		/* lis	r11,jmptab[reloff]@ha */
		/* lwzu	r12,jmptab[reloff]@l(r11) */
		/* mtctr r12 */
		/* bctr */
		where[0] = 0x3d600000 | _ppc_ha(&jmptab[reloff]);
		where[1] = 0x858b0000 | _ppc_la(&jmptab[reloff]);
		where[2] = 0x7d8903a6;
		where[3] = 0x4e800420;
	}
		

	/*
	 * The icache will be sync'd in reloc_plt, which is called
	 * after all the slots have been updated
	 */

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
	int N = obj->pltrelasize / sizeof(Elf_Rela);

	if (obj->pltrelasize != 0) {

		relalim = (const Elf_Rela *)((const char *)obj->pltrela +
		    obj->pltrelasize);
		for (rela = obj->pltrela;  rela < relalim;  rela++) {
			if (ELF_R_TYPE(rela->r_info) == R_PPC_IRELATIVE) {
				dbg("ABI violation - found IRELATIVE in the PLT.");
				obj->irelative = true;
				continue;
			}

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

	/*
	 * Sync the icache for the byte range represented by the
	 * trampoline routines and call slots.
	 */
	if (obj->pltgot != NULL && obj->gotptr == NULL)
		__syncicache(obj->pltgot, JMPTAB_BASE(N)*4);

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
			*where = 0;
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
reloc_jmpslot(Elf_Addr *wherep, Elf_Addr target,
    const Obj_Entry *defobj __unused, const Obj_Entry *obj, const Elf_Rel *rel)
{
	Elf_Addr offset;
	const Elf_Rela *rela = (const Elf_Rela *) rel;

	dbg(" reloc_jmpslot: where=%p, target=%p",
	    (void *)wherep, (void *)target);

	if (ld_bind_not)
		goto out;


	/*
	 * Process Secure-PLT.
	 */
	if (obj->gotptr != NULL) {
		assert(wherep >= (Elf_Word *)obj->pltgot);
		assert(wherep <
		    (Elf_Word *)obj->pltgot + obj->pltrelasize);
		if (*wherep != target)
			*wherep = target;
		goto out;
	}

	/*
	 * BSS-PLT optimization:
	 * Branch directly to the target if it is within +/- 32Mb,
	 * otherwise go indirectly via the pltcall trampoline call and
	 * jump table.
	 */
	offset = target - (Elf_Addr)wherep;
	if (abs((int)offset) < 32*1024*1024) {     /* inside 32MB? */
		/*
		 * At the PLT entry pointed at by `wherep', construct
		 * a direct transfer to the now fully resolved function
		 * address.
		 */
		/* b    value   # branch directly */
		*wherep = 0x48000000 | (offset & 0x03fffffc);
		__syncicache(wherep, 4);
	} else {
		Elf_Addr *pltcall, *jmptab;
		int distance;
		int N = obj->pltrelasize / sizeof(Elf_Rela);
		int reloff = rela - obj->pltrela;

		if (reloff < 0)
			return (-1);

		pltcall = obj->pltgot;

		dbg(" reloc_jmpslot: indir, reloff=%x, N=%x\n",
		    reloff, N);

		jmptab = obj->pltgot + JMPTAB_BASE(N);
		jmptab[reloff] = target;
		mb(); /* Order jmptab update before next changes */

		if (reloff < PLT_EXTENDED_BEGIN) {
			/* for extended PLT entries, we keep the old code */

			distance = (Elf_Addr)pltcall - (Elf_Addr)(wherep + 1);

			/* li   r11,reloff */
			/* b    pltcall  # use indirect pltcall routine */

			/* first instruction same as before */
			wherep[1] = 0x48000000 | (distance & 0x03fffffc);
			__syncicache(wherep, 8);
		}
	}

out:
	return (target);
}

int
reloc_iresolve(Obj_Entry *obj,
    struct Struct_RtldLockState *lockstate)
{
	/*
	 * Since PLT slots on PowerPC are always R_PPC_JMP_SLOT,
	 * R_PPC_IRELATIVE is in RELA.
	 */
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
}

int
reloc_iresolve_nonplt(Obj_Entry *obj __unused,
    struct Struct_RtldLockState *lockstate __unused)
{
	return (0);
}

int
reloc_gnu_ifunc(Obj_Entry *obj __unused, int flags __unused,
    struct Struct_RtldLockState *lockstate __unused)
{
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
}

/*
 * Setup the plt glue routines.
 */
#define PLTCALL_SIZE	   	20
#define PLTLONGRESOLVE_SIZE	20
#define PLTRESOLVE_SIZE		24

void
init_pltgot(Obj_Entry *obj)
{
	Elf_Word *pltcall, *pltresolve, *pltlongresolve;
	Elf_Word *jmptab;
	int N = obj->pltrelasize / sizeof(Elf_Rela);

	pltcall = obj->pltgot;

	if (pltcall == NULL) {
		return;
	}

	/* Handle Secure-PLT first, if applicable. */
	if (obj->gotptr != NULL) {
		obj->gotptr[1] = (Elf_Addr)_rtld_bind_secureplt_start;
		obj->gotptr[2] = (Elf_Addr)obj;
		dbg("obj %s secure-plt gotptr=%p start=%p obj=%p",
		    obj->path, obj->gotptr,
		    (void *)obj->gotptr[1], (void *)obj->gotptr[2]);
		return;
	}

	/*
	 * From the SVR4 PPC ABI:
	 *
	 * 'The first 18 words (72 bytes) of the PLT are reserved for
	 * use by the dynamic linker.
	 *   ...
	 * 'If the executable or shared object requires N procedure
	 *  linkage table entries, the link editor shall reserve 3*N
	 *  words (12*N bytes) following the 18 reserved words. The
	 *  first 2*N of these words are the procedure linkage table
	 *  entries themselves. The static linker directs calls to bytes
	 *  (72 + (i-1)*8), for i between 1 and N inclusive. The remaining
	 *  N words (4*N bytes) are reserved for use by the dynamic linker.'
	 */

	/*
	 * Copy the absolute-call assembler stub into the first part of
	 * the reserved PLT area.
	 */
	memcpy(pltcall, _rtld_powerpc_pltcall, PLTCALL_SIZE);

	/*
	 * Determine the address of the jumptable, which is the dyn-linker
	 * reserved area after the call cells. Write the absolute address
	 * of the jumptable into the absolute-call assembler code so it
	 * can determine this address.
	 */
	jmptab = obj->pltgot + JMPTAB_BASE(N);
	pltcall[1] |= _ppc_ha(jmptab);	   /* addis 11,11,jmptab@ha */
	pltcall[2] |= _ppc_la(jmptab);     /* lwz   11,jmptab@l(11) */

	/*
	 * Skip down 20 bytes into the initial reserved area and copy
	 * in the standard resolving assembler call. Into this assembler,
	 * insert the absolute address of the _rtld_bind_start routine
	 * and the address of the relocation object.
	 *
	 * We place pltlongresolve first, so it can fix up its arguments
	 * and then fall through to the regular PLT resolver.
	 */
	pltlongresolve = obj->pltgot + 5;

	memcpy(pltlongresolve, _rtld_powerpc_pltlongresolve,
	    PLTLONGRESOLVE_SIZE);
	pltlongresolve[0] |= _ppc_ha(jmptab);	/* lis	12,jmptab@ha	*/
	pltlongresolve[1] |= _ppc_la(jmptab);	/* addi	12,12,jmptab@l	*/

	pltresolve = pltlongresolve + PLTLONGRESOLVE_SIZE/sizeof(uint32_t);
	memcpy(pltresolve, _rtld_powerpc_pltresolve, PLTRESOLVE_SIZE);
	pltresolve[0] |= _ppc_ha(_rtld_bind_start);
	pltresolve[1] |= _ppc_la(_rtld_bind_start);
	pltresolve[3] |= _ppc_ha(obj);
	pltresolve[4] |= _ppc_la(obj);

	/*
	 * The icache will be sync'd in reloc_plt, which is called
	 * after all the slots have been updated
	 */
}

/*
 * 32 bit cpu feature flag fields.
 */
u_long cpu_features;
u_long cpu_features2;

void
powerpc_abi_variant_hook(Elf_Auxinfo** aux_info)
{
	/*
	 * Since aux_info[] is easier to work with than aux, go ahead and
	 * initialize cpu_features / cpu_features2.
	 */
	cpu_features = -1UL;
	cpu_features2 = -1UL;
	if (aux_info[AT_HWCAP] != NULL)
		cpu_features = aux_info[AT_HWCAP]->a_un.a_val;
	if (aux_info[AT_HWCAP2] != NULL)
		cpu_features2 = aux_info[AT_HWCAP2]->a_un.a_val;
}

void
ifunc_init(Elf_Auxinfo aux_info[__min_size(AT_COUNT)] __unused)
{

}

void
allocate_initial_tls(Obj_Entry *list)
{
	Elf_Addr **tp;

	/*
	* Fix the size of the static TLS block by using the maximum
	* offset allocated so far and adding a bit for dynamic modules to
	* use.
	*/

	tls_static_space = tls_last_offset + tls_last_size + RTLD_STATIC_TLS_EXTRA;

	tp = (Elf_Addr **)((char *) allocate_tls(list, NULL, TLS_TCB_SIZE, 8)
	    + TLS_TP_OFFSET + TLS_TCB_SIZE);

	/*
	 * XXX gcc seems to ignore 'tp = _tp;' 
	 */
	 
	__asm __volatile("mr 2,%0" :: "r"(tp));
}

void*
__tls_get_addr(tls_index* ti)
{
	register Elf_Addr **tp;
	char *p;

	__asm __volatile("mr %0,2" : "=r"(tp));
	p = tls_get_addr_common((Elf_Addr**)((Elf_Addr)tp - TLS_TP_OFFSET 
	    - TLS_TCB_SIZE), ti->ti_module, ti->ti_offset);

	return (p + TLS_DTV_OFFSET);
}
