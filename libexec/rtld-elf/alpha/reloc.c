/*-
 * Copyright 1996-1998 John D. Polstra.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *      $Id: reloc.c,v 1.4 1999/04/09 00:28:43 jdp Exp $
 */

/*
 * Dynamic linker for ELF.
 *
 * John Polstra <jdp@polstra.com>.
 */

#include <sys/param.h>
#include <sys/mman.h>

#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "rtld.h"

/*
 * Debugging support.
 */

#define assert(cond)	((cond) ? (void) 0 :\
    (msg("oops: " __XSTRING(__LINE__) "\n"), abort()))
#define msg(s)		(write(1, s, strlen(s)))
#define trace()		msg("trace: " __XSTRING(__LINE__) "\n");

extern Elf_Dyn _DYNAMIC;

/* Relocate a non-PLT object with addend. */
static int
reloc_non_plt_obj(Obj_Entry *obj_rtld, const Obj_Entry *obj,
    const Elf_Rela *rela)
{
	Elf_Addr *where = (Elf_Addr *) (obj->relocbase + rela->r_offset);

	switch (ELF_R_TYPE(rela->r_info)) {

		case R_ALPHA_REFQUAD: {
			const Elf_Sym *def;
			const Obj_Entry *defobj;
			Elf_Addr tmp_value;

			def = find_symdef(ELF_R_SYM(rela->r_info), obj,
			    &defobj, false);
			if (def == NULL)
				return -1;

			tmp_value = (Elf_Addr) (defobj->relocbase +
			    def->st_value) + *where + rela->r_addend;
			if (*where != tmp_value)
				*where = tmp_value;
		}
		break;

		case R_ALPHA_GLOB_DAT: {
			const Elf_Sym *def;
			const Obj_Entry *defobj;

			def = find_symdef(ELF_R_SYM(rela->r_info), obj,
			    &defobj, false);
			if (def == NULL)
				return -1;

			if (*where != (Elf_Addr) (defobj->relocbase +
			    def->st_value + rela->r_addend))
				*where = (Elf_Addr) (defobj->relocbase +
				    def->st_value + rela->r_addend);
		}
		break;

		case R_ALPHA_RELATIVE: {
			if (obj != obj_rtld ||
			    (caddr_t)where < (caddr_t)_GLOBAL_OFFSET_TABLE_ ||
			    (caddr_t)where >= (caddr_t)&_DYNAMIC)
				*where += (Elf_Addr) obj->relocbase;
		}
		break;

		case R_ALPHA_COPY: {
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
				return -1;
			}
		}
		break;

		default:
			_rtld_error("%s: Unsupported relocation type %d"
			    " in non-PLT relocations\n", obj->path,
			    ELF_R_TYPE(rela->r_info));
			return -1;
	}
	return(0);
}

/* Process the non-PLT relocations. */
int
reloc_non_plt(Obj_Entry *obj, Obj_Entry *obj_rtld)
{
	const Elf_Rel *rellim;
	const Elf_Rel *rel;
	const Elf_Rela *relalim;
	const Elf_Rela *rela;

	/* Perform relocations without addend if there are any: */
	rellim = (const Elf_Rel *) ((caddr_t) obj->rel + obj->relsize);
	for (rel = obj->rel;  obj->rel != NULL && rel < rellim;  rel++) {
		Elf_Rela locrela;

		locrela.r_info = rel->r_info;
		locrela.r_offset = rel->r_offset;
		locrela.r_addend = 0;
		if (reloc_non_plt_obj(obj_rtld, obj, &locrela))
			return -1;
	}

	/* Perform relocations with addend if there are any: */
	relalim = (const Elf_Rela *) ((caddr_t) obj->rela + obj->relasize);
	for (rela = obj->rela;  obj->rela != NULL && rela < relalim;  rela++) {
		if (reloc_non_plt_obj(obj_rtld, obj, rela))
			return -1;
	}
    return 0;
}

/* Process the PLT relocations. */
int
reloc_plt(Obj_Entry *obj, bool bind_now)
{
	const Elf_Rel *rellim;
	const Elf_Rel *rel;
	const Elf_Rela *relalim;
	const Elf_Rela *rela;

	/* Process the PLT relocations without addend if there are any. */
	rellim = (const Elf_Rel *) ((caddr_t) obj->pltrel + obj->pltrelsize);
	if (bind_now) {
	    /* Fully resolve procedure addresses now */
	    for (rel = obj->pltrel;  obj->pltrel != NULL && rel < rellim;
		    rel++) {
		Elf_Addr *where = (Elf_Addr *) (obj->relocbase + rel->r_offset);
		const Elf_Sym *def;
		const Obj_Entry *defobj;

		assert(ELF_R_TYPE(rel->r_info) == R_ALPHA_JMP_SLOT);

		def = find_symdef(ELF_R_SYM(rel->r_info), obj, &defobj, true);
		if (def == NULL)
			return -1;

		*where = (Elf_Addr) (defobj->relocbase + def->st_value);
	    }
	} else {	/* Just relocate the GOT slots pointing into the PLT */
	    for (rel = obj->pltrel; obj->pltrel != NULL && rel < rellim;
		rel++) {
		Elf_Addr *where = (Elf_Addr *)
		  (obj->relocbase + rel->r_offset);
		*where += (Elf_Addr) obj->relocbase;
	    }
	}

	/* Process the PLT relocations with addend if there are any. */
	relalim = (const Elf_Rela *) ((caddr_t) obj->pltrela +
	    obj->pltrelasize);
	if (bind_now) {
	    /* Fully resolve procedure addresses now */
	    for (rela = obj->pltrela;  obj->pltrela != NULL && rela < relalim;
		rela++) {
		Elf_Addr *where = (Elf_Addr *) (obj->relocbase +
		    rela->r_offset);
		const Elf_Sym *def;
		const Obj_Entry *defobj;

		assert(ELF_R_TYPE(rela->r_info) == R_ALPHA_JMP_SLOT);

		def = find_symdef(ELF_R_SYM(rela->r_info), obj, &defobj, true);
		if (def == NULL)
			return -1;

		*where = (Elf_Addr) (defobj->relocbase + def->st_value);
	    }
	} else {	/* Just relocate the GOT slots pointing into the PLT */
	    for (rela = obj->pltrela;  obj->pltrela != NULL && rela < relalim;
		rela++) {
		Elf_Addr *where = (Elf_Addr *)
		  (obj->relocbase + rela->r_offset);
		*where += (Elf_Addr) obj->relocbase;
	    }
	}
    return 0;
}

/* Process an R_ALPHA_COPY relocation. */
static int
do_copy_relocation(Obj_Entry *dstobj, const Elf_Rela *rela)
{
	void *dstaddr;
	const Elf_Sym *dstsym;
	const char *name;
	unsigned long hash;
	size_t size;
	const void *srcaddr;
	const Elf_Sym *srcsym;
	Obj_Entry *srcobj;

	dstaddr = (void *) (dstobj->relocbase + rela->r_offset);
	dstsym = dstobj->symtab + ELF_R_SYM(rela->r_info);
	name = dstobj->strtab + dstsym->st_name;
	hash = elf_hash(name);
	size = dstsym->st_size;

	for (srcobj = dstobj->next;  srcobj != NULL;  srcobj = srcobj->next)
		if ((srcsym = symlook_obj(name, hash, srcobj, false)) != NULL)
			break;

	if (srcobj == NULL) {
		_rtld_error("Undefined symbol \"%s\" referenced from COPY"
		    " relocation in %s", name, dstobj->path);
		return -1;
	}

	srcaddr = (const void *) (srcobj->relocbase + srcsym->st_value);
	memcpy(dstaddr, srcaddr, size);
	return 0;
}

/*
 * Process the special R_ALPHA_COPY relocations in the main program.  These
 * copy data from a shared object into a region in the main program's BSS
 * segment.
 *
 * Returns 0 on success, -1 on failure.
 */
int
do_copy_relocations(Obj_Entry *dstobj)
{
	const Elf_Rel *rellim;
	const Elf_Rel *rel;
	const Elf_Rela *relalim;
	const Elf_Rela *rela;

	assert(dstobj->mainprog);	/* COPY relocations are invalid elsewhere */

	rellim = (const Elf_Rel *) ((caddr_t) dstobj->rel + dstobj->relsize);
	for (rel = dstobj->rel; dstobj->rel != NULL && rel < rellim;  rel++) {
		if (ELF_R_TYPE(rel->r_info) == R_ALPHA_COPY) {
			Elf_Rela locrela;

			locrela.r_info = rel->r_info;
			locrela.r_offset = rel->r_offset;
			locrela.r_addend = 0;
			if (do_copy_relocation(dstobj, &locrela))
				return -1;
		}
	}

	relalim = (const Elf_Rela *) ((caddr_t) dstobj->rela +
	    dstobj->relasize);
	for (rela = dstobj->rela; dstobj->rela != NULL && rela < relalim;
	    rela++) {
		if (ELF_R_TYPE(rela->r_info) == R_ALPHA_COPY) {
			if (do_copy_relocation(dstobj, rela))
				return -1;
		}
	}

	return 0;
}

/* Initialize the special PLT entries. */
void
init_pltgot(Obj_Entry *obj)
{
	if (obj->pltgot != NULL &&
	    (obj->pltrelsize != 0 || obj->pltrelasize != 0)) {
		/* This function will be called to perform the relocation.  */
		obj->pltgot[2] = (Elf_Addr) &_rtld_bind_start;
		/* Identify this shared object */
		obj->pltgot[3] = (Elf_Addr) obj;
	}
}
