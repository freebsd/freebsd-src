/*
 * Copyright (c) 1993 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *	$Id: rrs.c,v 1.7 1993/12/02 00:56:39 jkh Exp $
 */

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <ar.h>
#include <ranlib.h>
#include <a.out.h>
#include <stab.h>
#include <string.h>
#include <strings.h>

#include "ld.h"

static struct link_dynamic	rrs_dyn;		/* defined in link.h */
static struct ld_debug		rrs_ld_debug;		/* defined in link.h */
static struct link_dynamic_2	rrs_dyn2;		/* defined in link.h */
static got_t			*rrs_got;
static jmpslot_t		*rrs_plt;		/* defined in md.h */
static struct relocation_info	*rrs_reloc;
static struct nzlist		*rrs_symbols;		/* RRS symbol table */
static char			*rrs_strtab;		/* RRS strings */
static struct rrs_hash		*rrs_hashtab;		/* RT hash table */
static struct shobj		*rrs_shobjs;

static int	reserved_rrs_relocs;
static int	claimed_rrs_relocs;

static int	number_of_gotslots;
static int	number_of_jmpslots;
static int	number_of_rrs_hash_entries;
static int	number_of_rrs_symbols;
static int	rrs_strtab_size;
static int	rrs_symbol_size;

static int	current_jmpslot_offset;
static int	current_got_offset;
static int	current_reloc_offset;
static int	current_hash_index;
static int	number_of_shobjs;

struct shobj {
	struct shobj		*next;
	struct file_entry	*entry;
};

/*
RRS text segment:
		+-------------------+  <-- ld_rel (rrs_text_start)
		|                   |
		|    relocation     |
		|                   |
		+-------------------+  <-- <link_dynamic_2>.ld_hash
		|                   |
		|    hash buckets   |
		|                   |
		+-------------------+  <-- <link_dynamic_2>.ld_stab
		|                   |
		|     symbols       |
		|                   |
		+-------------------+  <-- <link_dynamic_2>.ld_strings
		|                   |
		|     strings       |
		|                   |
		+-------------------+  <-- <link_dynamic_2>.ld_need
		|                   |
		|     shobjs        |
		|                   |
		+-------------------+
		|                   |
		|  shobjs strings   |  <-- <shobj>.lo_name
		|                   |
		+-------------------+


RRS data segment:

		+-------------------+  <-- __DYNAMIC (rrs_data_start)
		|                   |
		|   link_dymamic    |
		|                   |
		+-------------------+  <-- __DYNAMIC.ldd
		|                   |
		|   ld_debug        |
		|                   |
		+-------------------+  <-- __DYNAMIC.ld_un.ld_2
		|                   |
		|   link_dymamic_2  |
		|                   |
		+-------------------+  <-- _GLOBAL_OFFSET_TABLE_ (ld_got)
		|                   |
		|      _GOT_        |
		|                   |
		+-------------------+  <-- ld_plt
		|                   |
		|       PLT         |
		|                   |
		+-------------------+
*/

/*
 * Initialize RRS
 */

void
init_rrs()
{
	reserved_rrs_relocs = 0;
	claimed_rrs_relocs = 0;

	number_of_rrs_symbols = 0;
	rrs_strtab_size = 0;

	/* First jmpslot reserved for run-time binder */
	current_jmpslot_offset = sizeof(jmpslot_t);
	number_of_jmpslots = 1;

	/* First gotslot reserved for __DYNAMIC */
	current_got_offset = sizeof(got_t);
	number_of_gotslots = 1;

	current_reloc_offset = 0;
}

/*
 * Add NAME to the list of needed run-time objects.
 * Return 1 if ENTRY was added to the list.
 */
int
rrs_add_shobj(entry)
struct file_entry	*entry;
{
	struct shobj	**p;

	for (p = &rrs_shobjs; *p != NULL; p = &(*p)->next)
		if (strcmp((*p)->entry->filename, entry->filename) == 0)
			return 0;
	*p = (struct shobj *)xmalloc(sizeof(struct shobj));
	(*p)->next = NULL;
	(*p)->entry = entry;

	number_of_shobjs++;
	return 1;
}

void
alloc_rrs_reloc(entry, sp)
struct file_entry	*entry;
symbol *sp;
{
#ifdef DEBUG
printf("alloc_rrs_reloc: %s in %s\n", sp->name, get_file_name(entry));
#endif
	reserved_rrs_relocs++;
}

void
alloc_rrs_segment_reloc(entry, r)
struct file_entry	*entry;
struct relocation_info	*r;
{
#ifdef DEBUG
printf("alloc_rrs_segment_reloc at %#x in %s\n",
	r->r_address, get_file_name(entry));
#endif
	reserved_rrs_relocs++;
}

void
alloc_rrs_jmpslot(entry, sp)
struct file_entry	*entry;
symbol *sp;
{
	if (sp->jmpslot_offset == -1) {
		sp->jmpslot_offset = current_jmpslot_offset;
		current_jmpslot_offset += sizeof(jmpslot_t);
		number_of_jmpslots++;
		if (!(link_mode & SYMBOLIC) || JMPSLOT_NEEDS_RELOC) {
			reserved_rrs_relocs++;
		}
	}
}

void
alloc_rrs_gotslot(entry, r, lsp)
struct file_entry	*entry;
struct relocation_info	*r;
struct localsymbol	*lsp;
{
	symbol	*sp = lsp->symbol;

	if (!RELOC_EXTERN_P(r)) {

		if (sp != NULL) {
			error("%s: relocation for internal symbol expected at %#x",
				get_file_name(entry), RELOC_ADDRESS(r));
			return;
		}

		if (!RELOC_STATICS_THROUGH_GOT_P(r))
			/* No need for a GOT slot */
			return;

		if (lsp->gotslot_offset == -1) {
			lsp->gotslot_offset = current_got_offset;
			current_got_offset += sizeof(got_t);
			number_of_gotslots++;
			/*
			 * Now, see if slot needs run-time fixing
			 * If the load address is known (entry_symbol), this
			 * slot will have its final value set by `claim_got'
			 */
			if ((link_mode & SHAREABLE) || (link_mode & SYMBOLIC))
				reserved_rrs_relocs++;
		}

	} else {

		if (sp == NULL) {
			error("%s: relocation must refer to global symbol at %#x",
				get_file_name(entry), RELOC_ADDRESS(r));
			return;
		}

		if (sp->alias)
			sp = sp->alias;

		if (sp->gotslot_offset != -1)
			return;

		/*
		 * External symbols always get a relocation entry
		 */
		sp->gotslot_offset = current_got_offset;
		reserved_rrs_relocs++;
		current_got_offset += sizeof(got_t);
		number_of_gotslots++;
	}

}

void
alloc_rrs_cpy_reloc(entry, sp)
struct file_entry	*entry;
symbol *sp;
{
	if (sp->cpyreloc_reserved)
		return;
#ifdef DEBUG
printf("alloc_rrs_copy: %s in %s\n", sp->name, get_file_name(entry));
#endif
	sp->cpyreloc_reserved = 1;
	reserved_rrs_relocs++;
}

static struct relocation_info *
rrs_next_reloc()
{
	struct relocation_info	*r;

	r = rrs_reloc + claimed_rrs_relocs++;
	if (claimed_rrs_relocs > reserved_rrs_relocs)
		fatal("internal error: RRS relocs exceed allocation %d",
			reserved_rrs_relocs);
	return r;
}

/*
 * Claim a RRS relocation as a result of a regular (ie. non-PIC)
 * relocation record in a rel file.
 *
 * Return 1 if the output file needs no further updating.
 * Return 0 if the relocation value pointed to by RELOCATION must
 * written to a.out.
 */
int
claim_rrs_reloc(entry, rp, sp, relocation)
struct file_entry	*entry;
struct relocation_info	*rp;
symbol	*sp;
long	*relocation;
{
	struct relocation_info	*r = rrs_next_reloc();

#ifdef DEBUG
	if (rp->r_address < text_start + text_size)
		error("%s: RRS text relocation at %#x for \"%s\"",
			get_file_name(entry), rp->r_address, sp->name);

printf("claim_rrs_reloc: %s in %s\n", sp->name, get_file_name(entry));
#endif
	r->r_address = rp->r_address;
	r->r_symbolnum = sp->rrs_symbolnum;

	if (link_mode & SYMBOLIC) {
		if (!sp->defined)
			error("Cannot reduce symbol \"%s\" in %s",
					sp->name, get_file_name(entry));
		RELOC_EXTERN_P(r) = 0;
		*relocation += sp->value;
		(void) md_make_reloc(rp, r, RELTYPE_RELATIVE);
		return 0;
	} else {
		RELOC_EXTERN_P(r) = 1;
		return md_make_reloc(rp, r, RELTYPE_EXTERN);
	}
}

/*
 * Claim a jmpslot. Setup RRS relocation if claimed for the first time.
 */
long
claim_rrs_jmpslot(entry, rp, sp, addend)
struct file_entry	*entry;
struct relocation_info	*rp;
symbol			*sp;
long			addend;
{
	struct relocation_info *r;

	if (sp->jmpslot_claimed)
		return rrs_dyn2.ld_plt + sp->jmpslot_offset;

#ifdef DEBUG
printf("claim_rrs_jmpslot: %s: %s(%d) -> offset %x (textreloc %#x)\n",
	get_file_name(entry),
	sp->name, sp->rrs_symbolnum, sp->jmpslot_offset, text_relocation);
#endif

	if (sp->jmpslot_offset == -1)
		fatal(
		"internal error: %s: claim_rrs_jmpslot: %s: jmpslot_offset == -1\n",
		get_file_name(entry),
		sp->name);

	if ((link_mode & SYMBOLIC) || rrs_section_type == RRS_PARTIAL) {
		if (!sp->defined)
			error("Cannot reduce symbol \"%s\" in %s",
				sp->name, get_file_name(entry));

		md_fix_jmpslot( rrs_plt + sp->jmpslot_offset/sizeof(jmpslot_t),
				rrs_dyn2.ld_plt + sp->jmpslot_offset,
				sp->value);
		if (!JMPSLOT_NEEDS_RELOC) {
			return rrs_dyn2.ld_plt + sp->jmpslot_offset;
		}
	} else {
		md_make_jmpslot( rrs_plt + sp->jmpslot_offset/sizeof(jmpslot_t),
				sp->jmpslot_offset,
				claimed_rrs_relocs);
	}

	if (rrs_section_type == RRS_PARTIAL)
		/* PLT is self-contained */
		return rrs_dyn2.ld_plt + sp->jmpslot_offset;

	/*
	 * Install a run-time relocation for this PLT entry.
	 */
	r = rrs_next_reloc();
	sp->jmpslot_claimed = 1;

	RELOC_SYMBOL(r) = sp->rrs_symbolnum;

	r->r_address = (long)rrs_dyn2.ld_plt + sp->jmpslot_offset;

	if (link_mode & SYMBOLIC) {
		RELOC_EXTERN_P(r) = 0;
		md_make_jmpreloc(rp, r, RELTYPE_RELATIVE);
	} else {
		RELOC_EXTERN_P(r) = 1;
		md_make_jmpreloc(rp, r, 0);
	}

	return rrs_dyn2.ld_plt + sp->jmpslot_offset;
}

/*
 * Claim GOT entry for a global symbol. If this is the first relocation
 * claiming the entry, setup a RRS relocation for it.
 * Return offset into the GOT allocated to this symbol.
 */
long
claim_rrs_gotslot(entry, rp, lsp, addend)
struct file_entry	*entry;
struct relocation_info	*rp;
struct localsymbol	*lsp;
long			addend;
{
	struct relocation_info	*r;
	symbol	*sp = lsp->symbol;
	int	reloc_type = 0;

	if (sp == NULL) {
		return 0;
	}

	if (sp->alias)
		sp = sp->alias;

#ifdef DEBUG
printf("claim_rrs_gotslot: %s(%d) slot offset %#x, addend %#x\n",
	 sp->name, sp->rrs_symbolnum, sp->gotslot_offset, addend);
#endif
	if (sp->gotslot_offset == -1)
		fatal(
		"internal error: %s: claim_rrs_gotslot: %s: gotslot_offset == -1\n",
			get_file_name(entry), sp->name);

	if (sp->gotslot_claimed)
		/* This symbol already passed here before. */
		return sp->gotslot_offset;

	if (sp->defined &&
		(!(link_mode & SHAREABLE) || (link_mode & SYMBOLIC))) {

		/*
		 * Reduce to just a base-relative translation.
		 */

		*(got_t *)((long)rrs_got + sp->gotslot_offset) =
						sp->value + addend;
		reloc_type = RELTYPE_RELATIVE;

	} else if ((link_mode & SYMBOLIC) || rrs_section_type == RRS_PARTIAL) {
		/*
		 * SYMBOLIC: all symbols must be known.
		 * RRS_PARTIAL: we don't link against shared objects,
		 * so again all symbols must be known.
		 */
		error("Cannot reduce symbol \"%s\" in %s",
				sp->name, get_file_name(entry));

	} else {

		/*
		 * This gotslot will be updated with symbol value at run-rime.
		 */

		*(got_t *)((long)rrs_got + sp->gotslot_offset) = addend;
	}

	if (rrs_section_type == RRS_PARTIAL) {
		/*
		 * Base address is known, gotslot should be fully
		 * relocated by now.
		 * NOTE: RRS_PARTIAL implies !SHAREABLE.
		 */
		if (!sp->defined)
			error("Cannot reduce symbol \"%s\" in %s",
					sp->name, get_file_name(entry));
		return sp->gotslot_offset;
	}

	/*
	 * Claim a relocation entry.
	 * If symbol is defined and in "main" (!SHAREABLE)
	 * we still put out a relocation as we cannot easily
	 * undo the allocation.
	 * `RELTYPE_RELATIVE' relocations have the external bit off
	 * as no symbol need be looked up at run-time.
	 */
	r = rrs_next_reloc();
	sp->gotslot_claimed = 1;
	r->r_address = rrs_dyn2.ld_got + sp->gotslot_offset;
	RELOC_SYMBOL(r) = sp->rrs_symbolnum;
	RELOC_EXTERN_P(r) = !(reloc_type == RELTYPE_RELATIVE);
	md_make_gotreloc(rp, r, reloc_type);

	return sp->gotslot_offset;
}

/*
 * Claim a GOT entry for a static symbol. Return offset of the
 * allocated GOT entry. If RELOC_STATICS_THROUGH_GOT_P is in effect
 * return the offset of the symbol with respect to the *location* of
 * the GOT.
 */
long
claim_rrs_internal_gotslot(entry, rp, lsp, addend)
struct file_entry	*entry;
struct relocation_info	*rp;
struct localsymbol	*lsp;
long			addend;
{
	struct relocation_info	*r;

	addend += lsp->nzlist.nz_value;

	if (!RELOC_STATICS_THROUGH_GOT_P(r))
		return addend - rrs_dyn2.ld_got;

#ifdef DEBUG
printf("claim_rrs_internal_gotslot: %s: slot offset %#x, addend = %#x\n",
	get_file_name(entry), lsp->gotslot_offset, addend);
#endif

	if (lsp->gotslot_offset == -1)
		fatal(
		"internal error: %s: claim_rrs_internal_gotslot at %#x: slot_offset == -1\n",
			get_file_name(entry), RELOC_ADDRESS(rp));

	if (lsp->gotslot_claimed)
		/* Already done */
		return lsp->gotslot_offset;

	*(long *)((long)rrs_got + lsp->gotslot_offset) = addend;

	if (!(link_mode & SHAREABLE))
		return lsp->gotslot_offset;

	/*
	 * Relocation entry needed for this static GOT entry.
	 */
	r = rrs_next_reloc();
	lsp->gotslot_claimed = 1;
	r->r_address = rrs_dyn2.ld_got + lsp->gotslot_offset;
	RELOC_EXTERN_P(r) = 0;
	md_make_gotreloc(rp, r, RELTYPE_RELATIVE);
	return lsp->gotslot_offset;
}

void
claim_rrs_cpy_reloc(entry, rp, sp)
struct file_entry	*entry;
struct relocation_info	*rp;
symbol *sp;
{
	struct relocation_info	*r;

	if (sp->cpyreloc_claimed)
		return;

	if (!sp->cpyreloc_reserved)
		fatal("internal error: %s: claim_cpy_reloc: %s: no reservation\n",
			get_file_name(entry), sp->name);

#ifdef DEBUG
printf("claim_rrs_copy: %s: %s -> %x\n",
	get_file_name(entry), sp->name, sp->so_defined);
#endif

	r = rrs_next_reloc();
	sp->cpyreloc_claimed = 1;
	r->r_address = rp->r_address;
	RELOC_SYMBOL(r) = sp->rrs_symbolnum;
	RELOC_EXTERN_P(r) = RELOC_EXTERN_P(rp);
	md_make_cpyreloc(rp, r);
}

void
claim_rrs_segment_reloc(entry, rp)
struct file_entry	*entry;
struct relocation_info	*rp;
{
	struct relocation_info	*r = rrs_next_reloc();

#ifdef DEBUG
printf("claim_rrs_segment_reloc: %s at %#x\n",
	get_file_name(entry), rp->r_address);
#endif
	r->r_address = rp->r_address;
	RELOC_TYPE(r) = RELOC_TYPE(rp);
	RELOC_EXTERN_P(r) = 0;
	md_make_reloc(rp, r, RELTYPE_RELATIVE);

}

/*
 * Fill the RRS hash table for the given symbol name.
 * NOTE: the hash value computation must match the one in rtld.
 */
void
rrs_insert_hash(cp, index)
char	*cp;
int	index;
{
	int		hashval = 0;
	struct rrs_hash	*hp;

	for (; *cp; cp++)
		hashval = (hashval << 1) + *cp;

	hashval = (hashval & 0x7fffffff) % rrs_dyn2.ld_buckets;

	/* Get to the bucket */
	hp = rrs_hashtab + hashval;
	if (hp->rh_symbolnum == -1) {
		/* Empty bucket, use it */
		hp->rh_symbolnum = index;
		hp->rh_next = 0;
		return;
	}

	while (hp->rh_next != 0)
		hp = rrs_hashtab + hp->rh_next;

	hp->rh_next = current_hash_index++;
	hp = rrs_hashtab + hp->rh_next;
	hp->rh_symbolnum = index;
	hp->rh_next = 0;
}

/*
 * There are two interesting cases to consider here.
 *
 * 1) No shared objects were loaded, but there were PIC input rel files.
 *    In this case we must output a _GLOBAL_OFFSET_TABLE_ but no other
 *    RRS data. Also, the entries in the GOT must be fully resolved.
 *
 * 2) It's a genuine dynamically linked program, so the whole RRS scoop
 *    goes into a.out.
 */
void
consider_rrs_section_lengths()
{
	int		n;
	struct shobj	*shp;
	int		symbolsize;

	/* First, determine what of the RRS we want */

	if (relocatable_output)
		rrs_section_type = RRS_NONE;
	else if (link_mode & SHAREABLE)
		rrs_section_type = RRS_FULL;
	else if (number_of_shobjs == 0 /*&& !(link_mode & DYNAMIC)*/) {
		/*
		 * First slots in both tables are reserved
		 * hence the "> 1" condition
		 */
		if (number_of_gotslots > 1 || number_of_jmpslots > 1)
			rrs_section_type = RRS_PARTIAL;
		else
			rrs_section_type = RRS_NONE;
	} else
		rrs_section_type = RRS_FULL;

	if (rrs_section_type == RRS_NONE) {
		got_symbol->defined = 0;
		return;
	}

	rrs_symbol_size = LD_VERSION_NZLIST_P(soversion) ?
			sizeof(struct nzlist) : sizeof(struct nlist);

	/*
	 * If there is an entry point, __DYNAMIC must be referenced (usually
	 * from crt0), as this is the method used to determine whether the
	 * run-time linker must be called.
	 */
	if (!(link_mode & SHAREABLE) && !dynamic_symbol->referenced)
		fatal("No reference to __DYNAMIC");

	dynamic_symbol->referenced = 1;

	if (number_of_gotslots > 1)
		got_symbol->referenced = 1;


	/* Next, allocate relocs, got and plt */
	n = reserved_rrs_relocs * sizeof(struct relocation_info);
	rrs_reloc = (struct relocation_info *)xmalloc(n);
	bzero(rrs_reloc, n);

	n = number_of_gotslots * sizeof(got_t);
	rrs_got = (got_t *)xmalloc(n);
	bzero(rrs_got, n);

	n = number_of_jmpslots * sizeof(jmpslot_t);
	rrs_plt = (jmpslot_t *)xmalloc(n);
	bzero(rrs_plt, n);

	/* Initialize first jmpslot */
	md_fix_jmpslot(rrs_plt, 0, 0);

	if (rrs_section_type == RRS_PARTIAL) {
		rrs_data_size = number_of_gotslots * sizeof(got_t);
		rrs_data_size += number_of_jmpslots * sizeof(jmpslot_t);
		return;
	}

	/*
	 * Walk the symbol table, assign RRS symbol numbers
	 * Assign number 0 to __DYNAMIC (!! Sun compatibility)
	 */
	dynamic_symbol->rrs_symbolnum = number_of_rrs_symbols++;
	FOR_EACH_SYMBOL(i ,sp) {
		if (sp->referenced) {
			rrs_strtab_size += 1 + strlen(sp->name);
			if (sp != dynamic_symbol)
				sp->rrs_symbolnum = number_of_rrs_symbols++;
		}
	} END_EACH_SYMBOL;

	/*
	 * Now that we know how many RRS symbols there are going to be,
	 * allocate and initialize the RRS symbol hash table.
	 */
	rrs_dyn2.ld_buckets = number_of_rrs_symbols/4;
	if (rrs_dyn2.ld_buckets < 4)
		rrs_dyn2.ld_buckets = 4;

	number_of_rrs_hash_entries = rrs_dyn2.ld_buckets + number_of_rrs_symbols;
	rrs_hashtab = (struct rrs_hash *)xmalloc(
			number_of_rrs_hash_entries * sizeof(struct rrs_hash));
	for (n = 0; n < rrs_dyn2.ld_buckets; n++)
		rrs_hashtab[n].rh_symbolnum = -1;
	current_hash_index = rrs_dyn2.ld_buckets;

	/*
	 * Get symbols into hash table now, so we can fine tune the size
	 * of the latter. We adjust the value of `number_of_rrs_hash_entries'
	 * to the number of hash link slots actually used.
	 */
	FOR_EACH_SYMBOL(i ,sp) {
		if (sp->referenced)
			rrs_insert_hash(sp->name, sp->rrs_symbolnum);
	} END_EACH_SYMBOL;
	number_of_rrs_hash_entries = current_hash_index;

	/*
	 * Calculate RRS section sizes.
	 */
	rrs_data_size = sizeof(struct link_dynamic);
	rrs_data_size += sizeof(struct ld_debug);
	rrs_data_size += sizeof(struct link_dynamic_2);
	rrs_data_size += number_of_gotslots * sizeof(got_t);
	rrs_data_size += number_of_jmpslots * sizeof(jmpslot_t);
	rrs_data_size = MALIGN(rrs_data_size);

	rrs_text_size = reserved_rrs_relocs * sizeof(struct relocation_info);
	rrs_text_size += number_of_rrs_hash_entries * sizeof(struct rrs_hash);
	rrs_text_size += number_of_rrs_symbols * rrs_symbol_size;

	/* Align strings size */
	rrs_strtab_size = MALIGN(rrs_strtab_size);
	rrs_text_size += rrs_strtab_size;

	/* Process needed shared objects */
	for (shp = rrs_shobjs; shp; shp = shp->next) {
		char	*name = shp->entry->local_sym_name;

		if (*name == '-' && *(name+1) == 'l')
			name += 2;

		rrs_text_size += sizeof(struct link_object);
		rrs_text_size += 1 + strlen(name);
	}

	/* Finally, align size */
	rrs_text_size = MALIGN(rrs_text_size);
}

void
relocate_rrs_addresses()
{

	dynamic_symbol->value = 0;

	if (rrs_section_type == RRS_NONE)
		return;

	if (rrs_section_type == RRS_PARTIAL) {
		got_symbol->value = rrs_dyn2.ld_got = rrs_data_start;
		rrs_dyn2.ld_plt = rrs_dyn2.ld_got +
					number_of_gotslots * sizeof(got_t);
		return;
	}

	/*
	 * RRS data relocations.
	 */
	rrs_dyn.ld_version = soversion;
	rrs_dyn.ldd = (struct ld_debug *)
			(rrs_data_start + sizeof(struct link_dynamic));
	rrs_dyn.ld_un.ld_2 = (struct link_dynamic_2 *)
				((long)rrs_dyn.ldd + sizeof(struct ld_debug));

	rrs_dyn2.ld_got = (long)rrs_dyn.ld_un.ld_2 +
					sizeof(struct link_dynamic_2);
	rrs_dyn2.ld_plt = rrs_dyn2.ld_got + number_of_gotslots*sizeof(got_t);

	/*
	 * RRS text relocations.
	 */
	rrs_dyn2.ld_rel = rrs_text_start;
	/*
	 * Sun BUG compatibility alert.
	 * Main program's RRS text values are relative to TXTADDR? WHY??
	 */
#ifdef SUN_COMPAT
	if (soversion == LD_VERSION_SUN && !(link_mode & SHAREABLE))
		rrs_dyn2.ld_rel -= N_TXTADDR(outheader);
#endif

	rrs_dyn2.ld_hash = rrs_dyn2.ld_rel +
			reserved_rrs_relocs * sizeof(struct relocation_info);
	rrs_dyn2.ld_symbols = rrs_dyn2.ld_hash +
			number_of_rrs_hash_entries * sizeof(struct rrs_hash);
	rrs_dyn2.ld_strings = rrs_dyn2.ld_symbols +
			number_of_rrs_symbols * rrs_symbol_size;
	rrs_dyn2.ld_str_sz = rrs_strtab_size;
	rrs_dyn2.ld_text_sz = text_size;
	rrs_dyn2.ld_plt_sz = number_of_jmpslots * sizeof(jmpslot_t);

	rrs_dyn2.ld_need = rrs_shobjs ? rrs_dyn2.ld_strings+rrs_strtab_size : 0;
	rrs_dyn2.ld_stab_hash = 0;
	rrs_dyn2.ld_rules = 0;

	/*
	 * Assign addresses to _GLOBAL_OFFSET_TABLE_ and __DYNAMIC
	 * &__DYNAMIC is also in the first GOT entry.
	 */
	got_symbol->value = rrs_dyn2.ld_got;

	*rrs_got = dynamic_symbol->value = rrs_data_start;

}

void
write_rrs_data()
{
	long	pos;

	if (rrs_section_type == RRS_NONE)
		return;

	pos = rrs_data_start + (N_DATOFF(outheader) - DATA_START(outheader));
	if (lseek(outdesc, pos, L_SET) != pos)
		fatal("write_rrs_data: cant position in output file");

	if (rrs_section_type == RRS_PARTIAL) {
		/*
		 * Only a GOT and PLT are needed.
		 */
		if (number_of_gotslots <= 1)
			fatal("write_rrs_data: # gotslots <= 1");

		md_swapout_got(rrs_got, number_of_gotslots);
		mywrite(rrs_got, number_of_gotslots,
					sizeof(got_t), outdesc);

		if (number_of_jmpslots <= 1)
			fatal("write_rrs_data: # jmpslots <= 1");

		md_swapout_jmpslot(rrs_plt, number_of_jmpslots);
		mywrite(rrs_plt, number_of_jmpslots,
					sizeof(jmpslot_t), outdesc);
		return;
	}

	md_swapout_link_dynamic(&rrs_dyn);
	mywrite(&rrs_dyn, 1, sizeof(struct link_dynamic), outdesc);

	md_swapout_ld_debug(&rrs_ld_debug);
	mywrite(&rrs_ld_debug, 1, sizeof(struct ld_debug), outdesc);

	md_swapout_link_dynamic_2(&rrs_dyn2);
	mywrite(&rrs_dyn2, 1, sizeof(struct link_dynamic_2), outdesc);

	md_swapout_got(rrs_got, number_of_gotslots);
	mywrite(rrs_got, number_of_gotslots, sizeof(got_t), outdesc);

	md_swapout_jmpslot(rrs_plt, number_of_jmpslots);
	mywrite(rrs_plt, number_of_jmpslots, sizeof(jmpslot_t), outdesc);
}

void
write_rrs_text()
{
	long			pos;
	int			i;
	int			symsize;
	struct nzlist		*nlp;
	int			offset = 0;
	struct shobj		*shp;
	struct link_object	*lo;

	if (rrs_section_type == RRS_PARTIAL)
		return;

	pos = rrs_text_start + (N_TXTOFF(outheader) - TEXT_START(outheader));
	if (lseek(outdesc, pos, L_SET) != pos)
		fatal("write_rrs_text: cant position in output file");

	/* Write relocation records */
	md_swapout_reloc(rrs_reloc, reserved_rrs_relocs);
	mywrite(rrs_reloc, reserved_rrs_relocs,
				sizeof(struct relocation_info), outdesc);

	/* Write the RRS symbol hash tables */
	md_swapout_rrs_hash(rrs_hashtab, number_of_rrs_hash_entries);
	mywrite(rrs_hashtab, number_of_rrs_hash_entries, sizeof(struct rrs_hash), outdesc);

	/*
	 * Determine size of an RRS symbol entry, allocate space
	 * to collect them in.
	 */
	symsize = number_of_rrs_symbols * rrs_symbol_size;
	nlp = rrs_symbols = (struct nzlist *)alloca(symsize);
	rrs_strtab = (char *)alloca(rrs_strtab_size);

#define INCR_NLP(p)	((p) = (struct nzlist *)((long)(p) + rrs_symbol_size))

	/* __DYNAMIC symbol *must* be first for Sun compatibility */
	nlp->nz_desc = nlp->nz_other = 0;
	if (LD_VERSION_NZLIST_P(soversion))
		nlp->nz_size = 0;
	nlp->nz_type = dynamic_symbol->defined;
	nlp->nz_value = dynamic_symbol->value;
	nlp->nz_value = dynamic_symbol->value;
	nlp->nz_strx = offset;
	strcpy(rrs_strtab + offset, dynamic_symbol->name);
	offset += 1 + strlen(dynamic_symbol->name);
	INCR_NLP(nlp);

	/*
	 * Now, for each global symbol, construct a nzlist element
	 * for inclusion in the RRS symbol table.
	 */
	FOR_EACH_SYMBOL(i, sp) {

		if (!sp->referenced || sp == dynamic_symbol)
			continue;

		if ((long)nlp - (long)rrs_symbols >=
					number_of_rrs_symbols * rrs_symbol_size)
			fatal(
			    "internal error: rrs symbols exceed allocation %d ",
				number_of_rrs_symbols);

		nlp->nz_desc = 0;
		nlp->nz_other = 0;
		if (LD_VERSION_NZLIST_P(soversion))
			nlp->nz_size = 0;

		if (sp->defined > 1) {
			/* defined with known type */
			if (sp->defined == N_SIZE) {
				/*
				 * Make sure this symbol isn't going
				 * to define anything.
				 */
				nlp->nz_type = N_UNDF;
				nlp->nz_value = 0;
			} else {
				nlp->nz_type = sp->defined;
				nlp->nz_value = sp->value;
			}
			if (LD_VERSION_NZLIST_P(soversion))
				nlp->nz_size = sp->size;
		} else if (sp->max_common_size) {
			/*
			 * a common definition
			 */
			nlp->nz_type = N_UNDF | N_EXT;
			nlp->nz_value = sp->max_common_size;
		} else if (!sp->defined) {
			/* undefined */
			nlp->nz_type = N_UNDF | N_EXT;
			nlp->nz_value = 0;
		} else
			fatal(
			      "internal error: %s defined in mysterious way",
			      sp->name);


		/* Handle auxialiary type qualifiers */
		switch (sp->aux) {
		case 0:
			break;
		case RRS_FUNC:
			if (sp->so_defined != (N_TEXT+N_EXT))
				fatal("internal error: %s: other but not text",
							      sp->name);
			if (sp->jmpslot_offset == -1)
				fatal(
				  "internal error: %s has no jmpslot but other",
				      sp->name);
			nlp->nz_other = sp->aux;
			nlp->nz_value =
				rrs_dyn2.ld_plt + sp->jmpslot_offset;
			break;
		default:
			fatal(
		      "internal error: %s: unsupported other value: %x",
				      sp->name, sp->aux);
			break;
		}

		/* Set symbol's name */
		nlp->nz_strx = offset;
		strcpy(rrs_strtab + offset, sp->name);
		offset += 1 + strlen(sp->name);

		INCR_NLP(nlp);

	} END_EACH_SYMBOL;

	if (MALIGN(offset) != rrs_strtab_size)
		fatal(
		"internal error: inconsistent RRS string table length: %d",
			offset);

	/* Write the symbol table */
	if (rrs_symbol_size == sizeof(struct nlist))
		md_swapout_symbols(rrs_symbols, number_of_rrs_symbols);
	else
		md_swapout_zsymbols(rrs_symbols, number_of_rrs_symbols);
	mywrite(rrs_symbols, symsize, 1, outdesc);

	/* Write the strings */
	mywrite(rrs_strtab, rrs_strtab_size, 1, outdesc);

	/*
	 * Write the names of the shared objects needed at run-time
	 */
	pos = rrs_dyn2.ld_need + number_of_shobjs * sizeof(struct link_object);
	lo = (struct link_object *)alloca(
				number_of_shobjs * sizeof(struct link_object));

	for (i = 0, shp = rrs_shobjs; shp; i++, shp = shp->next) {
		char	*name = shp->entry->local_sym_name;

		if (i >= number_of_shobjs)
			fatal("internal error: # of link objects exceeds %d",
				number_of_shobjs);

		lo[i].lo_name = pos;
		lo[i].lo_major = shp->entry->lib_major;
		lo[i].lo_minor = shp->entry->lib_minor;

		if (*name == '-' && *(name+1) == 'l') {
			name += 2;
			lo[i].lo_library = 1;
		} else
			lo[i].lo_library = 0;

		pos += 1 + strlen(name);
		lo[i].lo_next = (i == number_of_shobjs - 1) ? 0 :
			(rrs_dyn2.ld_need + (i+1)*sizeof(struct link_object));
	}

	if (i < number_of_shobjs)
		fatal("internal error: # of link objects less then expected %d",
				number_of_shobjs);

	md_swapout_link_object(lo, number_of_shobjs);
	mywrite(lo, number_of_shobjs, sizeof(struct link_object), outdesc);

	for (i = 0, shp = rrs_shobjs; shp; i++, shp = shp->next) {
		char	*name = shp->entry->local_sym_name;

		if (*name == '-' && *(name+1) == 'l') {
			name += 2;
		}

		mywrite(name, strlen(name) + 1, 1, outdesc);
	}
}

void
write_rrs()
{

	/*
	 * First, do some consistency checks on the RRS segment.
	 */
	if (rrs_section_type == RRS_NONE) {
		if (reserved_rrs_relocs > 1)
			fatal(
			"internal error: RRS relocs in static program: %d",
				reserved_rrs_relocs-1);
		return;
	}

#ifdef DEBUG
printf("rrs_relocs %d, gotslots %d, jmpslots %d\n",
	reserved_rrs_relocs, number_of_gotslots-1, number_of_jmpslots-1);
#endif

	if (claimed_rrs_relocs != reserved_rrs_relocs) {
/*
		fatal("internal error: reserved relocs(%d) != claimed(%d)",
			reserved_rrs_relocs, claimed_rrs_relocs);
*/
		printf("FIX:internal error: reserved relocs(%d) != claimed(%d)\n",
			reserved_rrs_relocs, claimed_rrs_relocs);
	}

	/* Write the RRS segments. */
	write_rrs_text ();
	write_rrs_data ();
}
