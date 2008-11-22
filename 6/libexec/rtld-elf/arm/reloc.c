/*	$NetBSD: mdreloc.c,v 1.23 2003/07/26 15:04:38 mrg Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/mman.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "debug.h"
#include "rtld.h"

void
init_pltgot(Obj_Entry *obj)
{       
	if (obj->pltgot != NULL) {
		obj->pltgot[1] = (Elf_Addr) obj;
		obj->pltgot[2] = (Elf_Addr) &_rtld_bind_start;
	}
}

int             
do_copy_relocations(Obj_Entry *dstobj)
{
	const Elf_Rel *rellim;
	const Elf_Rel *rel;

	assert(dstobj->mainprog);	/* COPY relocations are invalid elsewhere */

   	rellim = (const Elf_Rel *) ((caddr_t) dstobj->rel + dstobj->relsize);
	for (rel = dstobj->rel;  rel < rellim;  rel++) {
		if (ELF_R_TYPE(rel->r_info) == R_ARM_COPY) {
	    		void *dstaddr;
			const Elf_Sym *dstsym;
			const char *name;
			unsigned long hash;
			size_t size;
			const void *srcaddr;
			const Elf_Sym *srcsym;
			Obj_Entry *srcobj;
			
			dstaddr = (void *) (dstobj->relocbase + rel->r_offset);
			dstsym = dstobj->symtab + ELF_R_SYM(rel->r_info);
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
		}
	}
	return 0;			     
}

void _rtld_bind_start(void);
void _rtld_relocate_nonplt_self(Elf_Dyn *, Elf_Addr);

int open();
int _open();
void
_rtld_relocate_nonplt_self(Elf_Dyn *dynp, Elf_Addr relocbase)
{
	const Elf_Rel *rel = 0, *rellim;
	Elf_Addr relsz = 0;
	Elf_Addr *where;
	uint32_t size;

	for (; dynp->d_tag != DT_NULL; dynp++) {
		switch (dynp->d_tag) {
		case DT_REL:
			rel = (const Elf_Rel *)(relocbase + dynp->d_un.d_ptr);
			break;
		case DT_RELSZ:
			relsz = dynp->d_un.d_val;
			break;
		}
	}
	rellim = (const Elf_Rel *)((caddr_t)rel + relsz);
	size = (rellim - 1)->r_offset - rel->r_offset;
	for (; rel < rellim; rel++) {
		where = (Elf_Addr *)(relocbase + rel->r_offset);
		
		*where += (Elf_Addr)relocbase;
	}
}
/*
 * It is possible for the compiler to emit relocations for unaligned data.
 * We handle this situation with these inlines.
 */
#define	RELOC_ALIGNED_P(x) \
	(((uintptr_t)(x) & (sizeof(void *) - 1)) == 0)

static __inline Elf_Addr
load_ptr(void *where)
{
	Elf_Addr res;

	memcpy(&res, where, sizeof(res));

	return (res);
}

static __inline void
store_ptr(void *where, Elf_Addr val)
{

	memcpy(where, &val, sizeof(val));
}

static int
reloc_nonplt_object(Obj_Entry *obj, const Elf_Rel *rel, SymCache *cache)
{
	Elf_Addr        *where;
	const Elf_Sym   *def;
	const Obj_Entry *defobj;
	Elf_Addr         tmp;
	unsigned long	 symnum;

	where = (Elf_Addr *)(obj->relocbase + rel->r_offset);
	symnum = ELF_R_SYM(rel->r_info);

	switch (ELF_R_TYPE(rel->r_info)) {
	case R_ARM_NONE:
		break;
		
#if 1 /* XXX should not occur */
	case R_ARM_PC24: {	/* word32 S - P + A */
		Elf32_Sword addend;
		
		/*
		 * Extract addend and sign-extend if needed.
		 */
		addend = *where;
		if (addend & 0x00800000)
			addend |= 0xff000000;
		
		def = find_symdef(symnum, obj, &defobj, false, cache);
		if (def == NULL)
				return -1;
			tmp = (Elf_Addr)obj->relocbase + def->st_value
			    - (Elf_Addr)where + (addend << 2);
			if ((tmp & 0xfe000000) != 0xfe000000 &&
			    (tmp & 0xfe000000) != 0) {
				_rtld_error(
				"%s: R_ARM_PC24 relocation @ %p to %s failed "
				"(displacement %ld (%#lx) out of range)",
				    obj->path, where,
				    obj->strtab + obj->symtab[symnum].st_name,
				    (long) tmp, (long) tmp);
				return -1;
			}
			tmp >>= 2;
			*where = (*where & 0xff000000) | (tmp & 0x00ffffff);
			dbg("PC24 %s in %s --> %p @ %p in %s",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)*where, where, defobj->path);
			break;
		}
#endif

		case R_ARM_ABS32:	/* word32 B + S + A */
		case R_ARM_GLOB_DAT:	/* word32 B + S */
			def = find_symdef(symnum, obj, &defobj, false, cache);
			if (def == NULL)
				return -1;
			if (__predict_true(RELOC_ALIGNED_P(where))) {
				tmp =  *where + (Elf_Addr)defobj->relocbase +
				    def->st_value;
				*where = tmp;
			} else {
				tmp = load_ptr(where) +
				    (Elf_Addr)defobj->relocbase +
				    def->st_value;
				store_ptr(where, tmp);
			}
			dbg("ABS32/GLOB_DAT %s in %s --> %p @ %p in %s",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)tmp, where, defobj->path);
			break;

		case R_ARM_RELATIVE:	/* word32 B + A */
			if (__predict_true(RELOC_ALIGNED_P(where))) {
				tmp = *where + (Elf_Addr)obj->relocbase;
				*where = tmp;
			} else {
				tmp = load_ptr(where) +
				    (Elf_Addr)obj->relocbase;
				store_ptr(where, tmp);
			}
			dbg("RELATIVE in %s --> %p", obj->path,
			    (void *)tmp);
			break;

		case R_ARM_COPY:
			/*
			 * These are deferred until all other relocations have
			 * been done.  All we do here is make sure that the
			 * COPY relocation is not in a shared library.  They
			 * are allowed only in executable files.
			 */
			if (!obj->mainprog) {
				_rtld_error(
			"%s: Unexpected R_COPY relocation in shared library",
				    obj->path);
				return -1;
			}
			dbg("COPY (avoid in main)");
			break;

		default:
			dbg("sym = %lu, type = %lu, offset = %p, "
			    "contents = %p, symbol = %s",
			    symnum, (u_long)ELF_R_TYPE(rel->r_info),
			    (void *)rel->r_offset, (void *)load_ptr(where),
			    obj->strtab + obj->symtab[symnum].st_name);
			_rtld_error("%s: Unsupported relocation type %ld "
			    "in non-PLT relocations\n",
			    obj->path, (u_long) ELF_R_TYPE(rel->r_info));
			return -1;
	}
	return 0;
}

/*
 *  * Process non-PLT relocations
 *   */
int
reloc_non_plt(Obj_Entry *obj, Obj_Entry *obj_rtld)
{
	const Elf_Rel *rellim;
	const Elf_Rel *rel;
	SymCache *cache;
	int bytes = obj->nchains * sizeof(SymCache);
	int r = -1;
	
	/* The relocation for the dynamic loader has already been done. */
	if (obj == obj_rtld)
		return (0);
	/*
 	 * The dynamic loader may be called from a thread, we have
	 * limited amounts of stack available so we cannot use alloca().
	 */
	cache = mmap(NULL, bytes, PROT_READ|PROT_WRITE, MAP_ANON, -1, 0);
	if (cache == MAP_FAILED)
		cache = NULL;
	
	rellim = (const Elf_Rel *)((caddr_t)obj->rel + obj->relsize);
	for (rel = obj->rel; rel < rellim; rel++) {
		if (reloc_nonplt_object(obj, rel, cache) < 0)
			goto done;
	}
	r = 0;
done:
	if (cache) {
		munmap(cache, bytes);
	}
	return (r);
}

/*
 *  * Process the PLT relocations.
 *   */
int
reloc_plt(Obj_Entry *obj)
{
	const Elf_Rel *rellim;
	const Elf_Rel *rel;
		
	rellim = (const Elf_Rel *)((char *)obj->pltrel +
	    obj->pltrelsize);
	for (rel = obj->pltrel;  rel < rellim;  rel++) {
		Elf_Addr *where;

		assert(ELF_R_TYPE(rel->r_info) == R_ARM_JUMP_SLOT);
		
		where = (Elf_Addr *)(obj->relocbase + rel->r_offset);
		*where += (Elf_Addr )obj->relocbase;
	}
	
	return (0);
}

/*
 *  * LD_BIND_NOW was set - force relocation for all jump slots
 *   */
int
reloc_jmpslots(Obj_Entry *obj)
{
	const Obj_Entry *defobj;
	const Elf_Rel *rellim;
	const Elf_Rel *rel;
	const Elf_Sym *def;
	Elf_Addr *where;
	Elf_Addr target;
	
	rellim = (const Elf_Rel *)((char *)obj->pltrel + obj->pltrelsize);
	for (rel = obj->pltrel; rel < rellim; rel++) {
		assert(ELF_R_TYPE(rel->r_info) == R_ARM_JUMP_SLOT);
		where = (Elf_Addr *)(obj->relocbase + rel->r_offset);
		def = find_symdef(ELF_R_SYM(rel->r_info), obj, &defobj,
		    true, NULL);
		if (def == NULL) {
			dbg("reloc_jmpslots: sym not found");
			return (-1);
		}
		
		target = (Elf_Addr)(defobj->relocbase + def->st_value);		
		reloc_jmpslot(where, target, defobj, obj,
		    (const Elf_Rel *) rel);
	}
	
	obj->jmpslots_done = true;
	
	return (0);
}

Elf_Addr
reloc_jmpslot(Elf_Addr *where, Elf_Addr target, const Obj_Entry *defobj,
    		const Obj_Entry *obj, const Elf_Rel *rel)
{

	assert(ELF_R_TYPE(rel->r_info) == R_ARM_JUMP_SLOT);

	if (*where != target)
		*where = target;

	return target;
}

void
allocate_initial_tls(Obj_Entry *objs)
{
	
}

void *
__tls_get_addr(tls_index* ti)
{
	return (NULL);
}
