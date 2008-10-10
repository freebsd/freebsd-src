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
		obj->pltgot[0] = (Elf_Addr) &_rtld_bind_start;
		obj->pltgot[1] |= (Elf_Addr) obj;
	}
}

int             
do_copy_relocations(Obj_Entry *dstobj)
{
	/* Do nothing */
	return 0;			     
}

void _rtld_bind_start(void);
void _rtld_relocate_nonplt_self(Elf_Dyn *, Elf_Addr);

int open();
int _open();

static __inline Elf_Addr
load_ptr(void *where)
{
	Elf_Addr res;

	memcpy(&res, where, sizeof(res));

	return (res);
}

void
store_ptr(void *where, Elf_Addr val)
{

	memcpy(where, &val, sizeof(val));
}

void
_rtld_relocate_nonplt_self(Elf_Dyn *dynp, Elf_Addr relocbase)
{
	const Elf_Rel *rel = 0, *rellim;
	Elf_Addr relsz = 0;
	const Elf_Sym *symtab = NULL, *sym;
	Elf_Addr *where;
	Elf_Addr *got = NULL;
	Elf_Word local_gotno = 0, symtabno = 0, gotsym = 0;
	int i;

	for (; dynp->d_tag != DT_NULL; dynp++) {
		switch (dynp->d_tag) {
		case DT_REL:
			rel = (const Elf_Rel *)(relocbase + dynp->d_un.d_ptr);
			break;
		case DT_RELSZ:
			relsz = dynp->d_un.d_val;
			break;
		case DT_SYMTAB:
			symtab = (const Elf_Sym *)(relocbase + dynp->d_un.d_ptr);
			break;
		case DT_PLTGOT:
			got = (Elf_Addr *)(relocbase + dynp->d_un.d_ptr);
			break;
		case DT_MIPS_LOCAL_GOTNO:
			local_gotno = dynp->d_un.d_val;
			break;
		case DT_MIPS_SYMTABNO:
			symtabno = dynp->d_un.d_val;
			break;
		case DT_MIPS_GOTSYM:
			gotsym = dynp->d_un.d_val;
			break;
		}
	}

	i = (got[1] & 0x80000000) ? 2 : 1;
	/* Relocate the local GOT entries */
	got += i;
	for (; i < local_gotno; i++) {
	       *got++ += relocbase;
	}

	sym = symtab + gotsym;
	/* Now do the global GOT entries */
	for (i = gotsym; i < symtabno; i++) {
		*got = sym->st_value + relocbase;
		++sym;
		++got;
	}

	rellim = (const Elf_Rel *)((caddr_t)rel + relsz);
	for (; rel < rellim; rel++) {
		where = (void *)(relocbase + rel->r_offset);

		switch (ELF_R_TYPE(rel->r_info)) {
		case R_TYPE(NONE):
			break;

		case R_TYPE(REL32):
			assert(ELF_R_SYM(rel->r_info) < gotsym);
			sym = symtab + ELF_R_SYM(rel->r_info);
			assert(ELF_ST_BIND(sym->st_info) == STB_LOCAL);
			store_ptr(where, load_ptr(where) + relocbase);
			break;

		default:
			abort();
			break;
		}
	}
}

Elf_Addr
_mips_rtld_bind(Obj_Entry *obj, Elf_Size reloff)
{
        Elf_Addr *got = obj->pltgot;
        const Elf_Sym *def;
        const Obj_Entry *defobj;
        Elf_Addr target;

        def = find_symdef(reloff, obj, &defobj, SYMLOOK_IN_PLT, NULL);
        if (def == NULL)
		_rtld_error("bind failed no symbol");

        target = (Elf_Addr)(defobj->relocbase + def->st_value);
        dbg("bind now/fixup at %s sym # %d in %s --> was=%p new=%p",
	    obj->path,
	    reloff, defobj->strtab + def->st_name, 
	    (void *)got[obj->local_gotno + reloff - obj->gotsym],
	    (void *)target);
        got[obj->local_gotno + reloff - obj->gotsym] = target;
	return (Elf_Addr)target;
}

/*
 * It is possible for the compiler to emit relocations for unaligned data.
 * We handle this situation with these inlines.
 */
#define	RELOC_ALIGNED_P(x) \
	(((uintptr_t)(x) & (sizeof(void *) - 1)) == 0)

/*
 * Process non-PLT relocations
 */
int
reloc_non_plt(Obj_Entry *obj, Obj_Entry *obj_rtld)
{
	const Elf_Rel *rel;
	const Elf_Rel *rellim;
	Elf_Addr *got = obj->pltgot;
	const Elf_Sym *sym, *def;
	const Obj_Entry *defobj;
	int i;

	/* The relocation for the dynamic loader has already been done. */
	if (obj == obj_rtld)
		return (0);

	i = (got[1] & 0x80000000) ? 2 : 1;

	/* Relocate the local GOT entries */
	got += i;
	dbg("got:%p for %d entries adding %x",
	    got, obj->local_gotno, (uint32_t)obj->relocbase);
	for (; i < obj->local_gotno; i++) {
	            *got += (Elf_Addr)obj->relocbase;
		    got++;
	}
	sym = obj->symtab + obj->gotsym;


	dbg("got:%p for %d entries",
	    got, obj->symtabno);
	/* Now do the global GOT entries */
	for (i = obj->gotsym; i < obj->symtabno; i++) {
		if (ELF_ST_TYPE(sym->st_info) == STT_FUNC &&
		    sym->st_value != 0 && sym->st_shndx == SHN_UNDEF) {
			/*
			 * If there are non-PLT references to the function,
			 * st_value should be 0, forcing us to resolve the
			 * address immediately.
			 *
			 * XXX DANGER WILL ROBINSON!
			 * The linker is not outputting PLT slots for calls to
			 * functions that are defined in the same shared
			 * library.  This is a bug, because it can screw up
			 * link ordering rules if the symbol is defined in
			 * more than one module.  For now, if there is a
			 * definition, we fail the test above and force a full
			 * symbol lookup.  This means that all intra-module
			 * calls are bound immediately.  - mycroft, 2003/09/24
			 */
			*got = sym->st_value + (Elf_Addr)obj->relocbase;
			if ((Elf_Addr)(*got) == (Elf_Addr)obj->relocbase) {
			  dbg("Warning2, i:%d maps to relocbase address:%x",
			      i, (uint32_t)obj->relocbase);
			}

		} else if (sym->st_info == ELF_ST_INFO(STB_GLOBAL, STT_SECTION)) {
			/* Symbols with index SHN_ABS are not relocated. */
		        if (sym->st_shndx != SHN_ABS) {
				*got = sym->st_value +
				    (Elf_Addr)obj->relocbase;
				if ((Elf_Addr)(*got) == (Elf_Addr)obj->relocbase) {
				  dbg("Warning3, i:%d maps to relocbase address:%x",
				      i, (uint32_t)obj->relocbase);
				}
			}
		} else {
			/* TODO: add cache here */
			def = find_symdef(i, obj, &defobj, false, NULL);
			if (def == NULL) {
			  dbg("Warning4, cant find symbole %d", i);
				return -1;
			}
			*got = def->st_value + (Elf_Addr)defobj->relocbase;
			if ((Elf_Addr)(*got) == (Elf_Addr)obj->relocbase) {
			  dbg("Warning4, i:%d maps to relocbase address:%x",
			      i, (uint32_t)obj->relocbase);
			  dbg("via first obj symbol %s",
			      obj->strtab + obj->symtab[i].st_name);
			  dbg("found in obj %p:%s",
			      defobj, defobj->path);
			} 
		}
		++sym;
		++got;
	}
	got = obj->pltgot;
	rellim = (const Elf_Rel *)((caddr_t)obj->rel + obj->relsize);
	for (rel = obj->rel; rel < rellim; rel++) {
		void		*where;
		Elf_Addr	 tmp;
		unsigned long	 symnum;

		where = obj->relocbase + rel->r_offset;
		symnum = ELF_R_SYM(rel->r_info);
		switch (ELF_R_TYPE(rel->r_info)) {
		case R_TYPE(NONE):
			break;

		case R_TYPE(REL32):
			/* 32-bit PC-relative reference */
			def = obj->symtab + symnum;
			if (symnum >= obj->gotsym) {
				tmp = load_ptr(where);
				tmp += got[obj->local_gotno + symnum - obj->gotsym];
				store_ptr(where, tmp);
				break;
			} else {
				tmp = load_ptr(where);

				if (def->st_info ==
				    ELF_ST_INFO(STB_LOCAL, STT_SECTION)
				    )
					tmp += (Elf_Addr)def->st_value;

				tmp += (Elf_Addr)obj->relocbase;
				store_ptr(where, tmp);
			}
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
	}

	return 0;
}

/*
 *  Process the PLT relocations.
 */
int
reloc_plt(Obj_Entry *obj)
{
	const Elf_Rel *rellim;
	const Elf_Rel *rel;
		
	dbg("reloc_plt obj:%p pltrel:%p sz:%d", obj, obj->pltrel, (int)obj->pltrelsize);
	dbg("gottable %p num syms:%d", obj->pltgot, obj->symtabno );
	dbg("*****************************************************");
	rellim = (const Elf_Rel *)((char *)obj->pltrel +
	    obj->pltrelsize);
	for (rel = obj->pltrel;  rel < rellim;  rel++) {
		Elf_Addr *where;
		where = (Elf_Addr *)(obj->relocbase + rel->r_offset);
		*where += (Elf_Addr )obj->relocbase;
	}

	return (0);
}

/*
 * LD_BIND_NOW was set - force relocation for all jump slots
 */
int
reloc_jmpslots(Obj_Entry *obj)
{
	/* Do nothing */
	obj->jmpslots_done = true;
	
	return (0);
}

Elf_Addr
reloc_jmpslot(Elf_Addr *where, Elf_Addr target, const Obj_Entry *defobj,
    		const Obj_Entry *obj, const Elf_Rel *rel)
{

	/* Do nothing */

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
