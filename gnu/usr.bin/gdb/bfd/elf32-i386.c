/* Intel 80386/80486-specific support for 32-bit ELF
   Copyright 1993 Free Software Foundation, Inc.

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "libelf.h"

static CONST struct reloc_howto_struct *elf_i386_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void elf_i386_info_to_howto
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rela *));
static void elf_i386_info_to_howto_rel
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rel *));
static boolean elf_i386_create_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean elf_i386_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));
static boolean elf_i386_allocate_dynamic_section
  PARAMS ((bfd *, const char *));
static boolean elf_i386_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean elf_i386_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **, char *));
static boolean elf_i386_finish_dynamic_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	   Elf_Internal_Sym *));
static boolean elf_i386_finish_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));

#define USE_REL	1		/* 386 uses REL relocations instead of RELA */

enum reloc_type
  {
    R_386_NONE = 0,
    R_386_32,
    R_386_PC32,
    R_386_GOT32,
    R_386_PLT32,
    R_386_COPY,
    R_386_GLOB_DAT,
    R_386_JUMP_SLOT,
    R_386_RELATIVE,
    R_386_GOTOFF,
    R_386_GOTPC,
    R_386_max
  };

#if 0
static CONST char *CONST reloc_type_names[] =
{
  "R_386_NONE",
  "R_386_32",
  "R_386_PC32",
  "R_386_GOT32",
  "R_386_PLT32",
  "R_386_COPY",
  "R_386_GLOB_DAT",
  "R_386_JUMP_SLOT",
  "R_386_RELATIVE",
  "R_386_GOTOFF",
  "R_386_GOTPC",
};
#endif

static reloc_howto_type elf_howto_table[]=
{
  HOWTO(R_386_NONE,	 0,0, 0,false,0,complain_overflow_bitfield, bfd_elf_generic_reloc,"R_386_NONE",	    true,0x00000000,0x00000000,false),
  HOWTO(R_386_32,	 0,2,32,false,0,complain_overflow_bitfield, bfd_elf_generic_reloc,"R_386_32",	    true,0xffffffff,0xffffffff,false),
  HOWTO(R_386_PC32,	 0,2,32,true, 0,complain_overflow_bitfield, bfd_elf_generic_reloc,"R_386_PC32",	    true,0xffffffff,0xffffffff,true),
  HOWTO(R_386_GOT32,	 0,2,32,false,0,complain_overflow_bitfield, bfd_elf_generic_reloc,"R_386_GOT32",    true,0xffffffff,0xffffffff,false),
  HOWTO(R_386_PLT32,	 0,2,32,false,0,complain_overflow_bitfield, bfd_elf_generic_reloc,"R_386_PLT32",    true,0xffffffff,0xffffffff,false),
  HOWTO(R_386_COPY,      0,2,32,false,0,complain_overflow_bitfield, bfd_elf_generic_reloc,"R_386_COPY",	    true,0xffffffff,0xffffffff,false),
  HOWTO(R_386_GLOB_DAT,  0,2,32,false,0,complain_overflow_bitfield, bfd_elf_generic_reloc,"R_386_GLOB_DAT", true,0xffffffff,0xffffffff,false),
  HOWTO(R_386_JUMP_SLOT, 0,2,32,false,0,complain_overflow_bitfield, bfd_elf_generic_reloc,"R_386_JUMP_SLOT",true,0xffffffff,0xffffffff,false),
  HOWTO(R_386_RELATIVE,  0,2,32,false,0,complain_overflow_bitfield, bfd_elf_generic_reloc,"R_386_RELATIVE", true,0xffffffff,0xffffffff,false),
  HOWTO(R_386_GOTOFF,    0,2,32,false,0,complain_overflow_bitfield, bfd_elf_generic_reloc,"R_386_GOTOFF",   true,0xffffffff,0xffffffff,false),
  HOWTO(R_386_GOTPC,     0,2,32,false,0,complain_overflow_bitfield, bfd_elf_generic_reloc,"R_386_GOTPC",    true,0xffffffff,0xffffffff,false),
};

#ifdef DEBUG_GEN_RELOC
#define TRACE(str) fprintf (stderr, "i386 bfd reloc lookup %d (%s)\n", code, str)
#else
#define TRACE(str)
#endif

static CONST struct reloc_howto_struct *
elf_i386_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
    case BFD_RELOC_NONE:
      TRACE ("BFD_RELOC_NONE");
      return &elf_howto_table[ (int)R_386_NONE ];

    case BFD_RELOC_32:
      TRACE ("BFD_RELOC_32");
      return &elf_howto_table[ (int)R_386_32 ];

    case BFD_RELOC_32_PCREL:
      TRACE ("BFD_RELOC_PC32");
      return &elf_howto_table[ (int)R_386_PC32 ];

    case BFD_RELOC_386_GOT32:
      TRACE ("BFD_RELOC_386_GOT32");
      return &elf_howto_table[ (int)R_386_GOT32 ];

    case BFD_RELOC_386_PLT32:
      TRACE ("BFD_RELOC_386_PLT32");
      return &elf_howto_table[ (int)R_386_PLT32 ];

    case BFD_RELOC_386_COPY:
      TRACE ("BFD_RELOC_386_COPY");
      return &elf_howto_table[ (int)R_386_COPY ];

    case BFD_RELOC_386_GLOB_DAT:
      TRACE ("BFD_RELOC_386_GLOB_DAT");
      return &elf_howto_table[ (int)R_386_GLOB_DAT ];

    case BFD_RELOC_386_JUMP_SLOT:
      TRACE ("BFD_RELOC_386_JUMP_SLOT");
      return &elf_howto_table[ (int)R_386_JUMP_SLOT ];

    case BFD_RELOC_386_RELATIVE:
      TRACE ("BFD_RELOC_386_RELATIVE");
      return &elf_howto_table[ (int)R_386_RELATIVE ];

    case BFD_RELOC_386_GOTOFF:
      TRACE ("BFD_RELOC_386_GOTOFF");
      return &elf_howto_table[ (int)R_386_GOTOFF ];

    case BFD_RELOC_386_GOTPC:
      TRACE ("BFD_RELOC_386_GOTPC");
      return &elf_howto_table[ (int)R_386_GOTPC ];

    default:
      break;
    }

  TRACE ("Unknown");
  return 0;
}

static void
elf_i386_info_to_howto (abfd, cache_ptr, dst)
     bfd		*abfd;
     arelent		*cache_ptr;
     Elf32_Internal_Rela *dst;
{
  BFD_ASSERT (ELF32_R_TYPE(dst->r_info) < (unsigned int) R_386_max);

  cache_ptr->howto = &elf_howto_table[ELF32_R_TYPE(dst->r_info)];
}

static void
elf_i386_info_to_howto_rel (abfd, cache_ptr, dst)
     bfd		*abfd;
     arelent		*cache_ptr;
     Elf32_Internal_Rel *dst;
{
  BFD_ASSERT (ELF32_R_TYPE(dst->r_info) < (unsigned int) R_386_max);

  cache_ptr->howto = &elf_howto_table[ELF32_R_TYPE(dst->r_info)];
}

/* Functions for the i386 ELF linker.  */

/* The name of the dynamic interpreter.  This is put in the .interp
   section.  */

#define ELF_DYNAMIC_INTERPRETER "/usr/lib/libc.so.1"

/* The size in bytes of an entry in the procedure linkage table.  */

#define PLT_ENTRY_SIZE 16

/* The first entry in an absolute procedure linkage table looks like
   this.  See the SVR4 ABI i386 supplement to see how this works.  */

static bfd_byte elf_i386_plt0_entry[PLT_ENTRY_SIZE] =
{
  0xff, 0x35,	/* pushl contents of address */
  0, 0, 0, 0,	/* replaced with address of .got + 4.  */
  0xff, 0x25,	/* jmp indirect */
  0, 0, 0, 0,	/* replaced with address of .got + 8.  */
  0, 0, 0, 0	/* pad out to 16 bytes.  */
};

/* Subsequent entries in an absolute procedure linkage table look like
   this.  */

static bfd_byte elf_i386_plt_entry[PLT_ENTRY_SIZE] =
{
  0xff, 0x25,	/* jmp indirect */
  0, 0, 0, 0,	/* replaced with address of this symbol in .got.  */
  0x68,		/* pushl immediate */
  0, 0, 0, 0,	/* replaced with offset into relocation table.  */
  0xe9,		/* jmp relative */
  0, 0, 0, 0	/* replaced with offset to start of .plt.  */
};

/* Create dynamic sections when linking against a dynamic object.  */

static boolean
elf_i386_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  flagword flags;
  register asection *s;
  struct elf_link_hash_entry *h;

  /* We need to create .plt, .rel.plt, .got, .dynbss, and .rel.bss
     sections.  */

  flags = SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY;

  s = bfd_make_section (abfd, ".plt");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY | SEC_CODE)
      || ! bfd_set_section_alignment (abfd, s, 2))
    return false;

  s = bfd_make_section (abfd, ".rel.plt");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, 2))
    return false;

  s = bfd_make_section (abfd, ".got");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags)
      || ! bfd_set_section_alignment (abfd, s, 2))
    return false;

  /* Define the symbol _GLOBAL_OFFSET_TABLE_ at the start of the .got
     section.  We don't do this in the linker script because we don't
     want to define the symbol if we are not creating a global offset
     table.  */
  h = NULL;
  if (! (_bfd_generic_link_add_one_symbol
	 (info, abfd, "_GLOBAL_OFFSET_TABLE_", BSF_GLOBAL, s, (bfd_vma) 0,
	  (const char *) NULL, false, get_elf_backend_data (abfd)->collect,
	  (struct bfd_link_hash_entry **) &h)))
    return false;
  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;

  /* The first three global offset table entries are reserved.  */
  s->_raw_size += 3 * 4;

  /* The .dynbss section is a place to put symbols which are defined
     by dynamic objects, are referenced by regular objects, and are
     not functions.  We must allocate space for them in the process
     image and use a R_386_COPY reloc to tell the dynamic linker to
     initialize them at run time.  The linker script puts the .dynbss
     section into the .bss section of the final image.  */
  s = bfd_make_section (abfd, ".dynbss");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, SEC_ALLOC))
    return false;

  /* The .rel.bss section holds copy relocs.  This section is not
     normally needed.  We need to create it here, though, so that the
     linker will map it to an output section.  If it turns out not to
     be needed, we can discard it later.  */
  s = bfd_make_section (abfd, ".rel.bss");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, 2))
    return false;

  return true;
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

static boolean
elf_i386_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  bfd *dynobj;
  asection *s;
  unsigned int power_of_two;
  size_t align;

  dynobj = elf_hash_table (info)->dynobj;

  /* Make sure we know what is going on here.  */
  BFD_ASSERT (dynobj != NULL
	      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
	      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) != 0
	      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0
	      && h->root.type == bfd_link_hash_defined
	      && (bfd_get_flavour (h->root.u.def.section->owner)
		  == bfd_target_elf_flavour)
	      && (elf_elfheader (h->root.u.def.section->owner)->e_type
		  == ET_DYN)
	      && h->root.u.def.section->output_section == NULL);

  /* If this is a function, put it in the procedure linkage table.  We
     will fill in the contents of the procedure linkage table later,
     when we know the address of the .got section.  */
  if (h->type == STT_FUNC)
    {
      s = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (s != NULL);

      /* If this is the first .plt entry, make room for the special
	 first entry.  */
      if (s->_raw_size == 0)
	s->_raw_size += PLT_ENTRY_SIZE;

      /* Set the symbol to this location in the .plt.  */
      h->root.u.def.section = s;
      h->root.u.def.value = s->_raw_size;

      /* Make room for this entry.  */
      s->_raw_size += PLT_ENTRY_SIZE;

      /* We also need to make an entry in the .got section.  */

      s = bfd_get_section_by_name (dynobj, ".got");
      BFD_ASSERT (s != NULL);
      s->_raw_size += 4;

      /* We also need to make an entry in the .rel.plt section.  */

      s = bfd_get_section_by_name (dynobj, ".rel.plt");
      BFD_ASSERT (s != NULL);
      s->_raw_size += sizeof (Elf32_External_Rel);

      return true;
    }

  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->weakdef != NULL)
    {
      BFD_ASSERT (h->weakdef->root.type == bfd_link_hash_defined);
      h->root.u.def.section = h->weakdef->root.u.def.section;
      h->root.u.def.value = h->weakdef->root.u.def.value;
      h->align = (bfd_size_type) -1;
      return true;
    }

  /* This is a reference to a symbol defined by a dynamic object which
     is not a function.  We must allocate it in our .dynbss section,
     which will become part of the .bss section of the executable.
     There will be an entry for this symbol in the .dynsym section.
     The dynamic object will contain position independent code, so all
     references from the dynamic object to this symbol will go through
     the global offset table.  The dynamic linker will use the .dynsym
     entry to determine the address it must put in the global offset
     table, so both the dynamic object and the regular object will
     refer to the same memory location for the variable.  */

  s = bfd_get_section_by_name (dynobj, ".dynbss");
  BFD_ASSERT (s != NULL);

  /* If the symbol is currently defined in the .bss section of the
     dynamic object, then it is OK to simply initialize it to zero.
     If the symbol is in some other section, we must generate a
     R_386_COPY reloc to tell the dynamic linker to copy the initial
     value out of the dynamic object and into the runtime process
     image.  We need to remember the offset into the .rel.bss section
     we are going to use, and we coopt the align field for this
     purpose (the align field is only used for common symbols, and
     these symbols are always defined).  It would be cleaner to use a
     new field, but that would waste memory.  */
  if ((h->root.u.def.section->flags & SEC_LOAD) == 0)
    h->align = (bfd_size_type) -1;
  else
    {
      asection *srel;

      srel = bfd_get_section_by_name (dynobj, ".rel.bss");
      BFD_ASSERT (srel != NULL);
      h->align = srel->_raw_size;
      srel->_raw_size += sizeof (Elf32_External_Rel);
    }

  /* We need to figure out the alignment required for this symbol.  I
     have no idea how ELF linkers handle this.  */
  switch (h->size)
    {
    case 0:
    case 1:
      power_of_two = 0;
      align = 1;
      break;
    case 2:
      power_of_two = 1;
      align = 2;
      break;
    case 3:
    case 4:
      power_of_two = 2;
      align = 4;
      break;
    case 5:
    case 6:
    case 7:
    case 8:
      power_of_two = 3;
      align = 8;
      break;
    default:
      power_of_two = 4;
      align = 16;
      break;
    }

  /* Apply the required alignment.  */
  s->_raw_size = BFD_ALIGN (s->_raw_size, align);
  if (power_of_two > bfd_get_section_alignment (dynobj, s))
    {
      if (! bfd_set_section_alignment (dynobj, s, power_of_two))
	return false;
    }

  /* Define the symbol as being at this point in the section.  */
  h->root.u.def.section = s;
  h->root.u.def.value = s->_raw_size;

  /* Increment the section size to make room for the symbol.  */
  s->_raw_size += h->size;

  return true;
}

/* Allocate contents for a section.  */

static INLINE boolean
elf_i386_allocate_dynamic_section (dynobj, name)
     bfd *dynobj;
     const char *name;
{
  register asection *s;

  s = bfd_get_section_by_name (dynobj, name);
  BFD_ASSERT (s != NULL);
  s->contents = (bfd_byte *) bfd_alloc (dynobj, s->_raw_size);
  if (s->contents == NULL && s->_raw_size != 0)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  return true;
}

/* Set the sizes of the dynamic sections.  */

static boolean
elf_i386_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *s;

  dynobj = elf_hash_table (info)->dynobj;
  BFD_ASSERT (dynobj != NULL);

  /* Set the contents of the .interp section to the interpreter.  */
  if (! info->shared)
    {
      s = bfd_get_section_by_name (dynobj, ".interp");
      BFD_ASSERT (s != NULL);
      s->_raw_size = sizeof ELF_DYNAMIC_INTERPRETER;
      s->contents = (unsigned char *) ELF_DYNAMIC_INTERPRETER;
    }

  /* The adjust_dynamic_symbol entry point has determined the sizes of
     the various dynamic sections.  Allocate some memory for them to
     hold contents.  */
  if (! elf_i386_allocate_dynamic_section (dynobj, ".plt")
      || ! elf_i386_allocate_dynamic_section (dynobj, ".rel.plt")
      || ! elf_i386_allocate_dynamic_section (dynobj, ".got")
      || ! elf_i386_allocate_dynamic_section (dynobj, ".rel.bss"))
    return false;

  /* Add some entries to the .dynamic section.  We fill in the values
     later, in elf_i386_finish_dynamic_sections, but we must add the
     entries now so that we get the correct size for the .dynamic
     section.  The DT_DEBUG entry is filled in by the dynamic linker
     and used by the debugger.  */
  if (! bfd_elf32_add_dynamic_entry (info, DT_DEBUG, 0)
      || ! bfd_elf32_add_dynamic_entry (info, DT_PLTGOT, 0))
    return false;

  s = bfd_get_section_by_name (dynobj, ".plt");
  BFD_ASSERT (s != NULL);
  if (s->_raw_size != 0)
    {
      if (! bfd_elf32_add_dynamic_entry (info, DT_PLTRELSZ, 0)
	  || ! bfd_elf32_add_dynamic_entry (info, DT_PLTREL, DT_REL)
	  || ! bfd_elf32_add_dynamic_entry (info, DT_JMPREL, 0))
	return false;
    }

  /* If we didn't need the .rel.bss section, then discard it from the
     output file.  This is a hack.  We don't bother to do it for the
     other sections because they normally are needed.  */
  s = bfd_get_section_by_name (dynobj, ".rel.bss");
  BFD_ASSERT (s != NULL);
  if (s->_raw_size == 0)
    {
      asection **spp;

      for (spp = &s->output_section->owner->sections;
	   *spp != s->output_section;
	   spp = &(*spp)->next)
	;
      *spp = s->output_section->next;
      --s->output_section->owner->section_count;
    }
  else
    {
      if (! bfd_elf32_add_dynamic_entry (info, DT_REL, 0)
	  || ! bfd_elf32_add_dynamic_entry (info, DT_RELSZ, 0)
	  || ! bfd_elf32_add_dynamic_entry (info, DT_RELENT,
					    sizeof (Elf32_External_Rel)))
	return false;
    }

  return true;
}

/* Relocate an i386 ELF section.  */

static boolean
elf_i386_relocate_section (output_bfd, info, input_bfd, input_section,
			   contents, relocs, local_syms, local_sections,
			   output_names)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     Elf_Internal_Rela *relocs;
     Elf_Internal_Sym *local_syms;
     asection **local_sections;
     char *output_names;
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      const reloc_howto_type *howto;
      long r_symndx;
      struct elf_link_hash_entry *h;
      Elf_Internal_Sym *sym;
      asection *sec;
      bfd_vma relocation;
      bfd_reloc_status_type r;

      r_type = ELF32_R_TYPE (rel->r_info);
      if (r_type < 0 || r_type >= (int) R_386_max)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
      howto = elf_howto_table + r_type;

      r_symndx = ELF32_R_SYM (rel->r_info);

      if (info->relocateable)
	{
	  /* This is a relocateable link.  We don't have to change
	     anything, unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      sym = local_syms + r_symndx;
	      if (ELF_ST_TYPE (sym->st_info) == STT_SECTION)
		{
		  bfd_vma val;

		  sec = local_sections[r_symndx];
		  val = bfd_get_32 (input_bfd, contents + rel->r_offset);
		  val += sec->output_offset + sym->st_value;
		  bfd_put_32 (input_bfd, val, contents + rel->r_offset);
		}
	    }

	  continue;
	}

      /* This is a final link.  */
      h = NULL;
      sym = NULL;
      sec = NULL;
      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  relocation = (sec->output_section->vma
			+ sec->output_offset
			+ sym->st_value);
	}
      else
	{
	  long indx;

	  indx = r_symndx - symtab_hdr->sh_info;
	  h = elf_sym_hashes (input_bfd)[indx];
	  if (h->root.type == bfd_link_hash_defined)
	    {
	      sec = h->root.u.def.section;
	      relocation = (h->root.u.def.value
			    + sec->output_section->vma
			    + sec->output_offset);
	    }
	  else if (h->root.type == bfd_link_hash_weak)
	    relocation = 0;
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd,
		      input_section, rel->r_offset)))
		return false;
	      relocation = 0;
	    }
	}

      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
				    contents, rel->r_offset,
				    relocation, (bfd_vma) 0);

      if (r != bfd_reloc_ok)
	{
	  switch (r)
	    {
	    default:
	    case bfd_reloc_outofrange:
	      abort ();
	    case bfd_reloc_overflow:
	      {
		const char *name;

		if (h != NULL)
		  name = h->root.root.string;
		else
		  {
		    name = output_names + sym->st_name;
		    if (name == NULL)
		      return false;
		    if (*name == '\0')
		      name = bfd_section_name (input_bfd, sec);
		  }
		if (! ((*info->callbacks->reloc_overflow)
		       (info, name, howto->name, (bfd_vma) 0,
			input_bfd, input_section, rel->r_offset)))
		  return false;
	      }
	      break;
	    }
	}
    }

  return true;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static boolean
elf_i386_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  /* If this symbol is not defined by a dynamic object, or is not
     referenced by a regular object, ignore it.  */
  if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0
      || (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) == 0
      || (h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0)
    {
      /* Mark _DYNAMIC and _GLOBAL_OFFSET_TABLE_ as absolute.  */
      if (strcmp (h->root.root.string, "_DYNAMIC") == 0
	  || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
	sym->st_shndx = SHN_ABS;
      return true;
    }

  BFD_ASSERT (h->root.type == bfd_link_hash_defined);
  BFD_ASSERT (h->dynindx != -1);

  if (h->type == STT_FUNC)
    {
      asection *splt;
      asection *sgot;
      asection *srel;
      bfd_vma plt_index;
      bfd_vma got_offset;
      Elf_Internal_Rel rel;

      splt = h->root.u.def.section;
      BFD_ASSERT (strcmp (bfd_get_section_name (splt->owner, splt), ".plt")
		  == 0);
      sgot = bfd_get_section_by_name (splt->owner, ".got");
      BFD_ASSERT (sgot != NULL);
      srel = bfd_get_section_by_name (splt->owner, ".rel.plt");
      BFD_ASSERT (srel != NULL);

      /* FIXME: This only handles an absolute procedure linkage table.
	 When producing a dynamic object, we need to generate a
	 position independent procedure linkage table.  */

      /* Get the index in the procedure linkage table which
	 corresponds to this symbol.  This is the index of this symbol
	 in all the symbols for which we are making plt entries.  The
	 first entry in the procedure linkage table is reserved.  */
      plt_index = h->root.u.def.value / PLT_ENTRY_SIZE - 1;

      /* Get the offset into the .got table of the entry that
	 corresponds to this function.  Each .got entry is 4 bytes.
	 The first three are reserved.  */
      got_offset = (plt_index + 3) * 4;

      /* Fill in the entry in the procedure linkage table.  */
      memcpy (splt->contents + h->root.u.def.value, elf_i386_plt_entry,
	      PLT_ENTRY_SIZE);
      bfd_put_32 (output_bfd,
		  (sgot->output_section->vma
		   + sgot->output_offset
		   + got_offset),
		  splt->contents + h->root.u.def.value + 2);
      bfd_put_32 (output_bfd, plt_index * sizeof (Elf32_External_Rel),
		  splt->contents + h->root.u.def.value + 7);
      bfd_put_32 (output_bfd, - (h->root.u.def.value + PLT_ENTRY_SIZE),
		  splt->contents + h->root.u.def.value + 12);

      /* Fill in the entry in the global offset table.  */
      bfd_put_32 (output_bfd,
		  (splt->output_section->vma
		   + splt->output_offset
		   + h->root.u.def.value
		   + 6),
		  sgot->contents + got_offset);

      /* Fill in the entry in the .rel.plt section.  */
      rel.r_offset = (sgot->output_section->vma
		      + sgot->output_offset
		      + got_offset);
      rel.r_info = ELF32_R_INFO (h->dynindx, R_386_JUMP_SLOT);
      bfd_elf32_swap_reloc_out (output_bfd, &rel,
				((Elf32_External_Rel *) srel->contents
				 + plt_index));

      /* Mark the symbol as undefined, rather than as defined in the
	 .plt section.  Leave the value alone.  */
      sym->st_shndx = SHN_UNDEF;
    }
  else
    {
      /* This is not a function.  We have already allocated memory for
	 it in the .bss section (via .dynbss).  All we have to do here
	 is create a COPY reloc if required.  */
      if (h->align != (bfd_size_type) -1)
	{
	  asection *s;
	  Elf_Internal_Rel rel;

	  s = bfd_get_section_by_name (h->root.u.def.section->owner,
				       ".rel.bss");
	  BFD_ASSERT (s != NULL);

	  rel.r_offset = (h->root.u.def.value
			  + h->root.u.def.section->output_section->vma
			  + h->root.u.def.section->output_offset);
	  rel.r_info = ELF32_R_INFO (h->dynindx, R_386_COPY);
	  bfd_elf32_swap_reloc_out (output_bfd, &rel,
				    ((Elf32_External_Rel *)
				     (s->contents + h->align)));
	}
    }

  return true;
}

/* Finish up the dynamic sections.  */

static boolean
elf_i386_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  asection *splt;
  asection *sgot;
  asection *sdyn;
  Elf32_External_Dyn *dyncon, *dynconend;

  splt = bfd_get_section_by_name (elf_hash_table (info)->dynobj, ".plt");
  sgot = bfd_get_section_by_name (elf_hash_table (info)->dynobj, ".got");
  sdyn = bfd_get_section_by_name (elf_hash_table (info)->dynobj, ".dynamic");
  BFD_ASSERT (splt != NULL && sgot != NULL && sdyn != NULL);

  dyncon = (Elf32_External_Dyn *) sdyn->contents;
  dynconend = (Elf32_External_Dyn *) (sdyn->contents + sdyn->_raw_size);
  for (; dyncon < dynconend; dyncon++)
    {
      Elf_Internal_Dyn dyn;
      const char *name;
      boolean size;

      bfd_elf32_swap_dyn_in (elf_hash_table (info)->dynobj, dyncon, &dyn);

      /* My reading of the SVR4 ABI indicates that the procedure
	 linkage table relocs (DT_JMPREL) should be included in the
	 overall relocs (DT_REL).  This is what Solaris does.
	 However, UnixWare can not handle that case.  Therefore, we
	 override the DT_REL and DT_RELSZ entries here to make them
	 not include the JMPREL relocs.  */

      switch (dyn.d_tag)
	{
	case DT_PLTGOT:   name = ".got"; size = false; break;
	case DT_PLTRELSZ: name = ".rel.plt"; size = true; break;
	case DT_JMPREL:   name = ".rel.plt"; size = false; break;
	case DT_REL:	  name = ".rel.bss"; size = false; break;
	case DT_RELSZ:	  name = ".rel.bss"; size = true; break;
	default:	  name = NULL; size = false; break;
	}

      if (name != NULL)
	{
	  asection *s;

	  s = bfd_get_section_by_name (output_bfd, name);
	  BFD_ASSERT (s != NULL);
	  if (! size)
	    dyn.d_un.d_ptr = s->vma;
	  else
	    {
	      if (s->_cooked_size != 0)
		dyn.d_un.d_val = s->_cooked_size;
	      else
		dyn.d_un.d_val = s->_raw_size;
	    }
	  bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	}
    }

  /* Fill in the first entry in the procedure linkage table.  */
  if (splt->_raw_size > 0)
    {
      memcpy (splt->contents, elf_i386_plt0_entry, PLT_ENTRY_SIZE);
      bfd_put_32 (output_bfd,
		  sgot->output_section->vma + sgot->output_offset + 4,
		  splt->contents + 2);
      bfd_put_32 (output_bfd,
		  sgot->output_section->vma + sgot->output_offset + 8,
		  splt->contents + 8);
    }

  /* Fill in the first three entries in the global offset table.  */
  if (sgot->_raw_size > 0)
    {
      bfd_put_32 (output_bfd,
		  sdyn->output_section->vma + sdyn->output_offset,
		  sgot->contents);
      bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents + 4);
      bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents + 8);
    }

  elf_section_data (sgot->output_section)->this_hdr.sh_entsize = 4;

  /* UnixWare sets the entsize of .plt to 4, although that doesn't
     really seem like the right value.  */
  elf_section_data (splt->output_section)->this_hdr.sh_entsize = 4;

  return true;
}

#define TARGET_LITTLE_SYM		bfd_elf32_i386_vec
#define TARGET_LITTLE_NAME		"elf32-i386"
#define ELF_ARCH			bfd_arch_i386
#define ELF_MACHINE_CODE		EM_386
#define elf_info_to_howto		elf_i386_info_to_howto
#define elf_info_to_howto_rel		elf_i386_info_to_howto_rel
#define bfd_elf32_bfd_reloc_type_lookup	elf_i386_reloc_type_lookup
#define ELF_MAXPAGESIZE			0x1000
#define elf_backend_create_dynamic_sections \
					elf_i386_create_dynamic_sections
#define elf_backend_adjust_dynamic_symbol \
					elf_i386_adjust_dynamic_symbol
#define elf_backend_size_dynamic_sections \
					elf_i386_size_dynamic_sections
#define elf_backend_relocate_section	elf_i386_relocate_section
#define elf_backend_finish_dynamic_symbol \
					elf_i386_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
					elf_i386_finish_dynamic_sections

#include "elf32-target.h"
