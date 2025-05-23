/*-
 * Copyright (c) 2014-2015 The FreeBSD Foundation
 *
 * Portions of this software were developed by Andrew Turner
 * under sponsorship from the FreeBSD Foundation.
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

#include <sys/types.h>

#include <machine/sysarch.h>

#include <stdlib.h>

#include "debug.h"
#include "rtld.h"
#include "rtld_printf.h"

/*
 * It is possible for the compiler to emit relocations for unaligned data.
 * We handle this situation with these inlines.
 */
#define	RELOC_ALIGNED_P(x) \
	(((uintptr_t)(x) & (sizeof(void *) - 1)) == 0)

/*
 * This is not the correct prototype, but we only need it for
 * a function pointer to a simple asm function.
 */
void *_rtld_tlsdesc_static(void *);
void *_rtld_tlsdesc_undef(void *);
void *_rtld_tlsdesc_dynamic(void *);

void _exit(int);

bool
arch_digest_dynamic(struct Struct_Obj_Entry *obj, const Elf_Dyn *dynp)
{
	if (dynp->d_tag == DT_AARCH64_VARIANT_PCS) {
		obj->variant_pcs = true;
		return (true);
	}

	return (false);
}

bool
arch_digest_note(struct Struct_Obj_Entry *obj __unused, const Elf_Note *note)
{
	const char *note_name;
	const uint32_t *note_data;

	note_name = (const char *)(note + 1);
	/* Only handle GNU notes */
	if (note->n_namesz != sizeof(ELF_NOTE_GNU) ||
	    strncmp(note_name, ELF_NOTE_GNU, sizeof(ELF_NOTE_GNU)) != 0)
		return (false);

	/* Only handle GNU property notes */
	if (note->n_type != NT_GNU_PROPERTY_TYPE_0)
		return (false);

	/*
	 * note_data[0] - Type
	 * note_data[1] - Length
	 * note_data[2] - Data
	 * note_data[3] - Padding?
	 */
	note_data = (const uint32_t *)(note_name + note->n_namesz);

	/* Only handle AArch64 feature notes */
	if (note_data[0] != GNU_PROPERTY_AARCH64_FEATURE_1_AND)
		return (false);

	/* We expect at least 4 bytes of data */
	if (note_data[1] < 4)
		return (false);

	/* TODO: Only guard if HWCAP2_BTI is set */
	if ((note_data[2] & GNU_PROPERTY_AARCH64_FEATURE_1_BTI) != 0) {
		struct arm64_guard_page_args guard;

		guard.addr = (uintptr_t)obj->mapbase;
		guard.len = obj->mapsize;

		sysarch(ARM64_GUARD_PAGE, &guard);
	}

	return (true);
}

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
	const Obj_Entry *srcobj, *defobj;
	const Elf_Rela *relalim;
	const Elf_Rela *rela;
	const Elf_Sym *srcsym;
	const Elf_Sym *dstsym;
	const void *srcaddr;
	const char *name;
	void *dstaddr;
	SymLook req;
	size_t size;
	int res;

	/*
	 * COPY relocs are invalid outside of the main program
	 */
	assert(dstobj->mainprog);

	relalim = (const Elf_Rela *)((const char *)dstobj->rela +
	    dstobj->relasize);
	for (rela = dstobj->rela; rela < relalim; rela++) {
		if (ELF_R_TYPE(rela->r_info) != R_AARCH64_COPY)
			continue;

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
			_rtld_error("Undefined symbol \"%s\" referenced from "
			    "COPY relocation in %s", name, dstobj->path);
			return (-1);
		}

		srcaddr = (const void *)(defobj->relocbase + srcsym->st_value);
		memcpy(dstaddr, srcaddr, size);
	}

	return (0);
}

struct tls_data {
	Elf_Addr	dtv_gen;
	int		tls_index;
	Elf_Addr	tls_offs;
};

static struct tls_data *
reloc_tlsdesc_alloc(int tlsindex, Elf_Addr tlsoffs)
{
	struct tls_data *tlsdesc;

	tlsdesc = xmalloc(sizeof(struct tls_data));
	tlsdesc->dtv_gen = tls_dtv_generation;
	tlsdesc->tls_index = tlsindex;
	tlsdesc->tls_offs = tlsoffs;

	return (tlsdesc);
}

struct tlsdesc_entry {
	void	*(*func)(void *);
	union {
		Elf_Ssize	addend;
		Elf_Size	offset;
		struct tls_data	*data;
	};
};

static void
reloc_tlsdesc(const Obj_Entry *obj, const Elf_Rela *rela,
    struct tlsdesc_entry *where, int flags, RtldLockState *lockstate)
{
	const Elf_Sym *def;
	const Obj_Entry *defobj;
	Elf_Addr offs;

	offs = 0;
	if (ELF_R_SYM(rela->r_info) != 0) {
		def = find_symdef(ELF_R_SYM(rela->r_info), obj, &defobj, flags,
			    NULL, lockstate);
		if (def == NULL)
			rtld_die();
		offs = def->st_value;
		obj = defobj;
		if (def->st_shndx == SHN_UNDEF) {
			/* Weak undefined thread variable */
			where->func = _rtld_tlsdesc_undef;
			where->addend = rela->r_addend;
			return;
		}
	}
	offs += rela->r_addend;

	if (obj->tlsoffset != 0) {
		/* Variable is in initialy allocated TLS segment */
		where->func = _rtld_tlsdesc_static;
		where->offset = obj->tlsoffset + offs;
	} else {
		/* TLS offest is unknown at load time, use dynamic resolving */
		where->func = _rtld_tlsdesc_dynamic;
		where->data = reloc_tlsdesc_alloc(obj->tlsindex, offs);
	}
}

/*
 * Process the PLT relocations.
 */
int
reloc_plt(Obj_Entry *obj, int flags, RtldLockState *lockstate)
{
	const Obj_Entry *defobj;
	const Elf_Rela *relalim;
	const Elf_Rela *rela;
	const Elf_Sym *def, *sym;
	bool lazy;

	relalim = (const Elf_Rela *)((const char *)obj->pltrela +
	    obj->pltrelasize);
	for (rela = obj->pltrela; rela < relalim; rela++) {
		Elf_Addr *where, target;

		where = (Elf_Addr *)(obj->relocbase + rela->r_offset);

		switch(ELF_R_TYPE(rela->r_info)) {
		case R_AARCH64_JUMP_SLOT:
			lazy = true;
			if (obj->variant_pcs) {
				sym = &obj->symtab[ELF_R_SYM(rela->r_info)];
				/*
				 * Variant PCS functions don't follow the
				 * standard register convention. Because of
				 * this we can't use lazy relocation and
				 * need to set the target address.
				 */
				if ((sym->st_other & STO_AARCH64_VARIANT_PCS) !=
				    0)
					lazy = false;
			}
			if (lazy) {
				*where += (Elf_Addr)obj->relocbase;
			} else {
				def = find_symdef(ELF_R_SYM(rela->r_info), obj,
				    &defobj, SYMLOOK_IN_PLT | flags, NULL,
				    lockstate);
				if (def == NULL)
					return (-1);
				if (ELF_ST_TYPE(def->st_info) == STT_GNU_IFUNC){
					obj->gnu_ifunc = true;
					continue;
				}
				target = (Elf_Addr)(defobj->relocbase +
				    def->st_value);
				/*
				 * Ignore ld_bind_not as it requires lazy
				 * binding
				 */
				*where = target;
			}
			break;
		case R_AARCH64_TLSDESC:
			reloc_tlsdesc(obj, rela, (struct tlsdesc_entry *)where,
			    SYMLOOK_IN_PLT | flags, lockstate);
			break;
		case R_AARCH64_IRELATIVE:
			obj->irelative = true;
			break;
		case R_AARCH64_NONE:
			break;
		default:
			_rtld_error("Unknown relocation type %u in PLT",
			    (unsigned int)ELF_R_TYPE(rela->r_info));
			return (-1);
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

	if (obj->jmpslots_done)
		return (0);

	relalim = (const Elf_Rela *)((const char *)obj->pltrela +
	    obj->pltrelasize);
	for (rela = obj->pltrela; rela < relalim; rela++) {
		Elf_Addr *where, target;

		where = (Elf_Addr *)(obj->relocbase + rela->r_offset);
		switch(ELF_R_TYPE(rela->r_info)) {
		case R_AARCH64_JUMP_SLOT:
			def = find_symdef(ELF_R_SYM(rela->r_info), obj,
			    &defobj, SYMLOOK_IN_PLT | flags, NULL, lockstate);
			if (def == NULL)
				return (-1);
			if (ELF_ST_TYPE(def->st_info) == STT_GNU_IFUNC) {
				obj->gnu_ifunc = true;
				continue;
			}
			target = (Elf_Addr)(defobj->relocbase + def->st_value);
			reloc_jmpslot(where, target, defobj, obj,
			    (const Elf_Rel *)rela);
			break;
		}
	}
	obj->jmpslots_done = true;

	return (0);
}

static void
reloc_iresolve_one(Obj_Entry *obj, const Elf_Rela *rela,
    RtldLockState *lockstate)
{
	Elf_Addr *where, target, *ptr;

	ptr = (Elf_Addr *)(obj->relocbase + rela->r_addend);
	where = (Elf_Addr *)(obj->relocbase + rela->r_offset);
	lock_release(rtld_bind_lock, lockstate);
	target = call_ifunc_resolver(ptr);
	wlock_acquire(rtld_bind_lock, lockstate);
	*where = target;
}

int
reloc_iresolve(Obj_Entry *obj, struct Struct_RtldLockState *lockstate)
{
	const Elf_Rela *relalim;
	const Elf_Rela *rela;

	if (!obj->irelative)
		return (0);
	obj->irelative = false;
	relalim = (const Elf_Rela *)((const char *)obj->pltrela +
	    obj->pltrelasize);
	for (rela = obj->pltrela;  rela < relalim;  rela++) {
		if (ELF_R_TYPE(rela->r_info) == R_AARCH64_IRELATIVE)
			reloc_iresolve_one(obj, rela, lockstate);
	}
	return (0);
}

int
reloc_iresolve_nonplt(Obj_Entry *obj, struct Struct_RtldLockState *lockstate)
{
	const Elf_Rela *relalim;
	const Elf_Rela *rela;

	if (!obj->irelative_nonplt)
		return (0);
	obj->irelative_nonplt = false;
	relalim = (const Elf_Rela *)((const char *)obj->rela + obj->relasize);
	for (rela = obj->rela;  rela < relalim;  rela++) {
		if (ELF_R_TYPE(rela->r_info) == R_AARCH64_IRELATIVE)
			reloc_iresolve_one(obj, rela, lockstate);
	}
	return (0);
}

int
reloc_gnu_ifunc(Obj_Entry *obj, int flags,
   struct Struct_RtldLockState *lockstate)
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
		if (ELF_R_TYPE(rela->r_info) == R_AARCH64_JUMP_SLOT) {
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

Elf_Addr
reloc_jmpslot(Elf_Addr *where, Elf_Addr target,
    const Obj_Entry *defobj __unused, const Obj_Entry *obj __unused,
    const Elf_Rel *rel)
{

	assert(ELF_R_TYPE(rel->r_info) == R_AARCH64_JUMP_SLOT ||
	    ELF_R_TYPE(rel->r_info) == R_AARCH64_IRELATIVE);

	if (*where != target && !ld_bind_not)
		*where = target;
	return (target);
}

void
ifunc_init(Elf_Auxinfo *aux_info[__min_size(AT_COUNT)] __unused)
{

}

/*
 * Process non-PLT relocations
 */
int
reloc_non_plt(Obj_Entry *obj, Obj_Entry *obj_rtld, int flags,
    RtldLockState *lockstate)
{
	const Obj_Entry *defobj;
	const Elf_Rela *relalim;
	const Elf_Rela *rela;
	const Elf_Sym *def;
	SymCache *cache;
	Elf_Addr *where, symval;

	/*
	 * The dynamic loader may be called from a thread, we have
	 * limited amounts of stack available so we cannot use alloca().
	 */
	if (obj == obj_rtld)
		cache = NULL;
	else
		cache = calloc(obj->dynsymcount, sizeof(SymCache));
		/* No need to check for NULL here */

	relalim = (const Elf_Rela *)((const char *)obj->rela + obj->relasize);
	for (rela = obj->rela; rela < relalim; rela++) {
		/*
		 * First, resolve symbol for relocations which
		 * reference symbols.
		 */
		switch (ELF_R_TYPE(rela->r_info)) {
		case R_AARCH64_ABS64:
		case R_AARCH64_GLOB_DAT:
		case R_AARCH64_TLS_TPREL64:
		case R_AARCH64_TLS_DTPREL64:
		case R_AARCH64_TLS_DTPMOD64:
			def = find_symdef(ELF_R_SYM(rela->r_info), obj,
			    &defobj, flags, cache, lockstate);
			if (def == NULL)
				return (-1);
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
				case R_AARCH64_ABS64:
				case R_AARCH64_GLOB_DAT:
					if ((flags & SYMLOOK_IFUNC) == 0) {
						obj->non_plt_gnu_ifunc = true;
						continue;
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
					continue;
				symval = (Elf_Addr)defobj->relocbase +
				    def->st_value;
			}
			break;
		default:
			if ((flags & SYMLOOK_IFUNC) != 0)
				continue;
		}

		where = (Elf_Addr *)(obj->relocbase + rela->r_offset);

		switch (ELF_R_TYPE(rela->r_info)) {
		case R_AARCH64_ABS64:
		case R_AARCH64_GLOB_DAT:
			*where = symval + rela->r_addend;
			break;
		case R_AARCH64_COPY:
			/*
			 * These are deferred until all other relocations have
			 * been done. All we do here is make sure that the
			 * COPY relocation is not in a shared library. They
			 * are allowed only in executable files.
			 */
			if (!obj->mainprog) {
				_rtld_error("%s: Unexpected R_AARCH64_COPY "
				    "relocation in shared library", obj->path);
				return (-1);
			}
			break;
		case R_AARCH64_TLSDESC:
			reloc_tlsdesc(obj, rela, (struct tlsdesc_entry *)where,
			    flags, lockstate);
			break;
		case R_AARCH64_TLS_TPREL64:
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
					_rtld_error(
					    "%s: No space available for static "
					    "Thread Local Storage", obj->path);
					return (-1);
				}
			}
			*where = def->st_value + rela->r_addend +
			    defobj->tlsoffset;
			break;

		/*
		 * !!! BEWARE !!!
		 * ARM ELF ABI defines TLS_DTPMOD64 as 1029, and TLS_DTPREL64
		 * as 1028. But actual bfd linker and the glibc RTLD linker
		 * treats TLS_DTPMOD64 as 1028 and TLS_DTPREL64 1029.
		 */
		case R_AARCH64_TLS_DTPREL64: /* efectively is TLS_DTPMOD64 */
			*where += (Elf_Addr)defobj->tlsindex;
			break;
		case R_AARCH64_TLS_DTPMOD64: /* efectively is TLS_DTPREL64 */
			*where += (Elf_Addr)(def->st_value + rela->r_addend);
			break;
		case R_AARCH64_RELATIVE:
			*where = (Elf_Addr)(obj->relocbase + rela->r_addend);
			break;
		case R_AARCH64_NONE:
			break;
		case R_AARCH64_IRELATIVE:
			obj->irelative_nonplt = true;
			break;
		default:
			rtld_printf("%s: Unhandled relocation %lu\n",
			    obj->path, ELF_R_TYPE(rela->r_info));
			return (-1);
		}
	}

	return (0);
}

void
allocate_initial_tls(Obj_Entry *objs)
{

	/*
	* Fix the size of the static TLS block by using the maximum
	* offset allocated so far and adding a bit for dynamic modules to
	* use.
	*/
	tls_static_space = tls_last_offset + tls_last_size +
	    ld_static_tls_extra;

	_tcb_set(allocate_tls(objs, NULL, TLS_TCB_SIZE, TLS_TCB_ALIGN));
}

void *
__tls_get_addr(tls_index* ti)
{
	struct dtv **dtvp;

	dtvp = &_tcb_get()->tcb_dtv;
	return (tls_get_addr_common(dtvp, ti->ti_module, ti->ti_offset));
}
