/* Morpho Technologies MT specific support for 32-bit ELF
   Copyright 2001, 2002, 2003, 2004, 2005
   Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/mt.h"

/* Prototypes.  */
static reloc_howto_type * mt_reloc_type_lookup 
  (bfd *, bfd_reloc_code_real_type);

static void mt_info_to_howto_rela
  (bfd *, arelent *, Elf_Internal_Rela *);

static bfd_reloc_status_type mt_elf_relocate_hi16
  (bfd *, Elf_Internal_Rela *, bfd_byte *, bfd_vma);

static bfd_reloc_status_type mt_final_link_relocate
  (reloc_howto_type *, bfd *, asection *, bfd_byte *, 
   Elf_Internal_Rela *, bfd_vma);

static bfd_boolean mt_elf_relocate_section
  (bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *, 
   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **);

/* Relocation tables.  */
static reloc_howto_type mt_elf_howto_table [] =
{
  /* This reloc does nothing.  */
  HOWTO (R_MT_NONE,           /* type */
          0,                      /* rightshift */ 
          2,                      /* size (0 = byte, 1 = short, 2 = long) */ 
          32,                     /* bitsize */
          FALSE,                  /* pc_relative */ 
          0,                      /* bitpos */ 
          complain_overflow_dont, /* complain_on_overflow */ 
          bfd_elf_generic_reloc,  /* special_function */ 
          "R_MT_NONE",          /* name */ 
          FALSE,                  /* partial_inplace */ 
          0 ,                     /* src_mask */ 
          0,                      /* dst_mask */ 
          FALSE),                 /* pcrel_offset */

  /* A 16 bit absolute relocation.  */
  HOWTO (R_MT_16,             /* type */
          0,                      /* rightshift */ 
          2,                      /* size (0 = byte, 1 = short, 2 = long) */ 
          16,                     /* bitsize */
          FALSE,                  /* pc_relative */ 
          0,                      /* bitpos */ 
          complain_overflow_dont, /* complain_on_overflow */ 
          bfd_elf_generic_reloc,  /* special_function */ 
          "R_MT_16",            /* name */ 
          FALSE,                  /* partial_inplace */ 
          0 ,                     /* src_mask */ 
          0xffff,                 /* dst_mask */ 
          FALSE),                 /* pcrel_offset */

  /* A 32 bit absolute relocation.  */
  HOWTO (R_MT_32,             /* type */
          0,                      /* rightshift */ 
          2,                      /* size (0 = byte, 1 = short, 2 = long) */ 
          32,                     /* bitsize */
          FALSE,                  /* pc_relative */ 
          0,                      /* bitpos */ 
          complain_overflow_dont, /* complain_on_overflow */ 
          bfd_elf_generic_reloc,  /* special_function */ 
          "R_MT_32",            /* name */ 
          FALSE,                  /* partial_inplace */ 
          0 ,                     /* src_mask */ 
          0xffffffff,             /* dst_mask */ 
          FALSE),                 /* pcrel_offset */

  /* A 32 bit pc-relative relocation.  */
  HOWTO (R_MT_32_PCREL,       /* type */
          0,                      /* rightshift */ 
          2,                      /* size (0 = byte, 1 = short, 2 = long) */ 
          32,                     /* bitsize */
          TRUE,                   /* pc_relative */ 
          0,                      /* bitpos */ 
          complain_overflow_dont, /* complain_on_overflow */ 
          bfd_elf_generic_reloc,  /* special_function */ 
          "R_MT_32_PCREL",    /* name */ 
          FALSE,                  /* partial_inplace */ 
          0 ,                     /* src_mask */ 
          0xffffffff,             /* dst_mask */ 
          TRUE),                  /* pcrel_offset */

  /* A 16 bit pc-relative relocation.  */
  HOWTO (R_MT_PC16,           /* type */
          0,                      /* rightshift */ 
          2,                      /* size (0 = byte, 1 = short, 2 = long) */ 
          16,                     /* bitsize */
          TRUE,                   /* pc_relative */ 
          0,                      /* bitpos */ 
          complain_overflow_signed, /* complain_on_overflow */ 
          bfd_elf_generic_reloc,  /* special_function */ 
          "R_MT_PC16",          /* name */ 
          FALSE,                  /* partial_inplace */ 
          0,                      /* src_mask */ 
          0xffff,                 /* dst_mask */ 
          TRUE),                  /* pcrel_offset */

  /* high 16 bits of symbol value.  */
  HOWTO (R_MT_HI16,          /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         16,                    /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         bfd_elf_generic_reloc, /* special_function */
         "R_MT_HI16",        /* name */
         FALSE,                  /* partial_inplace */
         0xffff0000,            /* src_mask */
         0xffff0000,            /* dst_mask */
         FALSE),                /* pcrel_offset */

  /* Low 16 bits of symbol value.  */
  HOWTO (R_MT_LO16,          /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         16,                    /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         bfd_elf_generic_reloc, /* special_function */
         "R_MT_LO16",        /* name */
         FALSE,                  /* partial_inplace */
         0xffff,                /* src_mask */
         0xffff,                /* dst_mask */
         FALSE),                /* pcrel_offset */
};

/* Map BFD reloc types to MT ELF reloc types.  */

static reloc_howto_type *
mt_reloc_type_lookup
    (bfd *                    abfd ATTRIBUTE_UNUSED,
     bfd_reloc_code_real_type code)
{
  /* Note that the mt_elf_howto_table is indxed by the R_
     constants.  Thus, the order that the howto records appear in the
     table *must* match the order of the relocation types defined in
     include/elf/mt.h.  */

  switch (code)
    {
    case BFD_RELOC_NONE:
      return &mt_elf_howto_table[ (int) R_MT_NONE];
    case BFD_RELOC_16:
      return &mt_elf_howto_table[ (int) R_MT_16];
    case BFD_RELOC_32:
      return &mt_elf_howto_table[ (int) R_MT_32];
    case BFD_RELOC_32_PCREL:
      return &mt_elf_howto_table[ (int) R_MT_32_PCREL];
    case BFD_RELOC_16_PCREL:
      return &mt_elf_howto_table[ (int) R_MT_PC16];
    case BFD_RELOC_HI16:
      return &mt_elf_howto_table[ (int) R_MT_HI16];
    case BFD_RELOC_LO16:
      return &mt_elf_howto_table[ (int) R_MT_LO16];

    default:
      /* Pacify gcc -Wall.  */
      return NULL;
    }
  return NULL;
}

bfd_reloc_status_type
mt_elf_relocate_hi16
    (bfd *               input_bfd,
     Elf_Internal_Rela * relhi,
     bfd_byte *          contents,
     bfd_vma             value)
{
  bfd_vma insn;

  insn = bfd_get_32 (input_bfd, contents + relhi->r_offset);

  value += relhi->r_addend;
  value >>= 16;
  insn = ((insn & ~0xFFFF) | value);

  bfd_put_32 (input_bfd, insn, contents + relhi->r_offset);
  return bfd_reloc_ok;
}

/* XXX: The following code is the result of a cut&paste.  This unfortunate
   practice is very widespread in the various target back-end files.  */

/* Set the howto pointer for a MT ELF reloc.  */

static void
mt_info_to_howto_rela
    (bfd *               abfd ATTRIBUTE_UNUSED,
     arelent *           cache_ptr,
     Elf_Internal_Rela * dst)
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  cache_ptr->howto = & mt_elf_howto_table [r_type];
}

/* Perform a single relocation.  By default we use the standard BFD
   routines.  */

static bfd_reloc_status_type
mt_final_link_relocate
    (reloc_howto_type *  howto,
     bfd *               input_bfd,
     asection *          input_section,
     bfd_byte *          contents,
     Elf_Internal_Rela * rel,
     bfd_vma             relocation)
{
  return _bfd_final_link_relocate (howto, input_bfd, input_section,
				   contents, rel->r_offset,
				   relocation, rel->r_addend);
}

/* Relocate a MT ELF section.
   There is some attempt to make this function usable for many architectures,
   both USE_REL and USE_RELA ['twould be nice if such a critter existed],
   if only to serve as a learning tool.

   The RELOCATE_SECTION function is called by the new ELF backend linker
   to handle the relocations for a section.

   The relocs are always passed as Rela structures; if the section
   actually uses Rel structures, the r_addend field will always be
   zero.

   This function is responsible for adjusting the section contents as
   necessary, and (if using Rela relocs and generating a relocatable
   output file) adjusting the reloc addend as necessary.

   This function does not have to worry about setting the reloc
   address or the reloc symbol index.

   LOCAL_SYMS is a pointer to the swapped in local symbols.

   LOCAL_SECTIONS is an array giving the section in the input file
   corresponding to the st_shndx field of each local symbol.

   The global hash table entry for the global symbols can be found
   via elf_sym_hashes (input_bfd).

   When generating relocatable output, this function must handle
   STB_LOCAL/STT_SECTION symbols specially.  The output symbol is
   going to be the section symbol corresponding to the output
   section, which means that the addend must be adjusted
   accordingly.  */

static bfd_boolean
mt_elf_relocate_section
    (bfd *                   output_bfd ATTRIBUTE_UNUSED,
     struct bfd_link_info *  info,
     bfd *                   input_bfd,
     asection *              input_section,
     bfd_byte *              contents,
     Elf_Internal_Rela *     relocs,
     Elf_Internal_Sym *      local_syms,
     asection **             local_sections)
{
  Elf_Internal_Shdr *           symtab_hdr;
  struct elf_link_hash_entry ** sym_hashes;
  Elf_Internal_Rela *           rel;
  Elf_Internal_Rela *           relend;

  symtab_hdr = & elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  relend     = relocs + input_section->reloc_count;

  for (rel = relocs; rel < relend; rel ++)
    {
      reloc_howto_type *           howto;
      unsigned long                r_symndx;
      Elf_Internal_Sym *           sym;
      asection *                   sec;
      struct elf_link_hash_entry * h;
      bfd_vma                      relocation;
      bfd_reloc_status_type        r;
      const char *                 name = NULL;
      int                          r_type;
      
      r_type = ELF32_R_TYPE (rel->r_info);

      r_symndx = ELF32_R_SYM (rel->r_info);

      /* This is a final link.  */
      howto  = mt_elf_howto_table + ELF32_R_TYPE (rel->r_info);
      h      = NULL;
      sym    = NULL;
      sec    = NULL;
      
      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections [r_symndx];
	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, &sec, rel);
	  
	  name = bfd_elf_string_from_elf_section
	    (input_bfd, symtab_hdr->sh_link, sym->st_name);
	  name = (name == NULL) ? bfd_section_name (input_bfd, sec) : name;
	}
      else
	{
	  bfd_boolean unresolved_reloc;
	  bfd_boolean warned;

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned);

	  name = h->root.root.string;
	}


      /* Finally, the sole MT-specific part.  */
      switch (r_type)
        {
        case R_MT_HI16:
          r = mt_elf_relocate_hi16 (input_bfd, rel, contents, relocation);
          break;
	default:
          r = mt_final_link_relocate (howto, input_bfd, input_section,
		        		  contents, rel, relocation);
          break;
        }


      if (r != bfd_reloc_ok)
	{
	  const char * msg = (const char *) NULL;

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      r = info->callbacks->reloc_overflow
		(info, (h ? &h->root : NULL), name, howto->name, (bfd_vma) 0,
		 input_bfd, input_section, rel->r_offset);
	      break;
	      
	    case bfd_reloc_undefined:
	      r = info->callbacks->undefined_symbol
		(info, name, input_bfd, input_section, rel->r_offset, TRUE);
	      break;
	      
	    case bfd_reloc_outofrange:
	      msg = _("internal error: out of range error");
	      break;

	    case bfd_reloc_dangerous:
	      msg = _("internal error: dangerous relocation");
	      break;

	    default:
	      msg = _("internal error: unknown error");
	      break;
	    }

	  if (msg)
	    r = info->callbacks->warning
	      (info, msg, name, input_bfd, input_section, rel->r_offset);

	  if (! r)
	    return FALSE;
	}
    }

  return TRUE;
}

/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
mt_elf_gc_mark_hook
    (asection *                   sec,
     struct bfd_link_info *       info ATTRIBUTE_UNUSED,
     Elf_Internal_Rela *          rel ATTRIBUTE_UNUSED,
     struct elf_link_hash_entry * h,
     Elf_Internal_Sym *           sym)
{
  if (h != NULL)
    {
      switch (h->root.type)
	{
	case bfd_link_hash_defined:
	case bfd_link_hash_defweak:
	  return h->root.u.def.section;

	case bfd_link_hash_common:
	  return h->root.u.c.p->section;

	default:
	  break;
	}
    }
  else
    {
      if (!(elf_bad_symtab (sec->owner)
	    && ELF_ST_BIND (sym->st_info) != STB_LOCAL)
	  && ! ((sym->st_shndx <= 0 || sym->st_shndx >= SHN_LORESERVE)
		&& sym->st_shndx != SHN_COMMON))
	return bfd_section_from_elf_index (sec->owner, sym->st_shndx);
    }

  return NULL;
}

/* Update the got entry reference counts for the section being
   removed.  */

static bfd_boolean
mt_elf_gc_sweep_hook
    (bfd *                     abfd ATTRIBUTE_UNUSED,
     struct bfd_link_info *    info ATTRIBUTE_UNUSED,
     asection *                sec ATTRIBUTE_UNUSED,
     const Elf_Internal_Rela * relocs ATTRIBUTE_UNUSED)
{
  return TRUE;
}

/* Look through the relocs for a section during the first phase.
   Since we don't do .gots or .plts, we just need to consider the
   virtual table relocs for gc.  */
 
static bfd_boolean
mt_elf_check_relocs
    (bfd *                     abfd,
     struct bfd_link_info *    info,
     asection *                sec,
     const Elf_Internal_Rela * relocs)
{
  Elf_Internal_Shdr *           symtab_hdr;
  struct elf_link_hash_entry ** sym_hashes;
  struct elf_link_hash_entry ** sym_hashes_end;
  const Elf_Internal_Rela *     rel;
  const Elf_Internal_Rela *     rel_end;
  
  if (info->relocatable)
    return TRUE;
  
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + symtab_hdr->sh_size / sizeof (Elf32_External_Sym);
  if (!elf_bad_symtab (abfd))
    sym_hashes_end -= symtab_hdr->sh_info;
  
  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      struct elf_link_hash_entry *h;
      unsigned long r_symndx;
      
      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
        h = NULL;
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	}
    }

  return TRUE;
}

/* Return the MACH for an e_flags value.  */

static int
elf32_mt_machine (bfd *abfd)
{
  switch (elf_elfheader (abfd)->e_flags & EF_MT_CPU_MASK)
    {
    case EF_MT_CPU_MRISC:	return bfd_mach_ms1;
    case EF_MT_CPU_MRISC2:	return bfd_mach_mrisc2;
    case EF_MT_CPU_MS2:		return bfd_mach_ms2;
    }

  return bfd_mach_ms1;
}

static bfd_boolean
mt_elf_object_p (bfd * abfd)
{
  bfd_default_set_arch_mach (abfd, bfd_arch_mt, elf32_mt_machine (abfd));

  return TRUE;
}

/* Function to set the ELF flag bits.  */

static bfd_boolean
mt_elf_set_private_flags (bfd *    abfd,
			   flagword flags)
{
  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = TRUE;
  return TRUE;
}

static bfd_boolean
mt_elf_copy_private_bfd_data (bfd * ibfd, bfd * obfd)
{
  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;
  
  BFD_ASSERT (!elf_flags_init (obfd)
	      || elf_elfheader (obfd)->e_flags == elf_elfheader (ibfd)->e_flags);

  elf_elfheader (obfd)->e_flags = elf_elfheader (ibfd)->e_flags;
  elf_flags_init (obfd) = TRUE;
  return TRUE;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */

static bfd_boolean
mt_elf_merge_private_bfd_data (bfd * ibfd, bfd * obfd)
{
  flagword     old_flags, new_flags;
  bfd_boolean  ok = TRUE;

  /* Check if we have the same endianess.  */
  if (_bfd_generic_verify_endian_match (ibfd, obfd) == FALSE)
    return FALSE;

  /* If they're not both mt, then merging is meaningless, so just
     don't do it.  */
  if (strcmp (ibfd->arch_info->arch_name, "mt") != 0)
    return TRUE;
  if (strcmp (obfd->arch_info->arch_name, "mt") != 0)
    return TRUE;

  new_flags = elf_elfheader (ibfd)->e_flags;
  old_flags = elf_elfheader (obfd)->e_flags;

#ifdef DEBUG
  _bfd_error_handler ("%B: old_flags = 0x%.8lx, new_flags = 0x%.8lx, init = %s",
		      ibfd, old_flags, new_flags, elf_flags_init (obfd) ? "yes" : "no");
#endif

  if (!elf_flags_init (obfd))
    {
      old_flags = new_flags;
      elf_flags_init (obfd) = TRUE;
    }
  else if ((new_flags & EF_MT_CPU_MASK) != (old_flags & EF_MT_CPU_MASK))
    {
      /* CPU has changed.  This is invalid, because MRISC, MRISC2 and
	 MS2 are not subsets of each other.   */
      ok = FALSE;
    }
  
  if (ok)
    {
      obfd->arch_info = ibfd->arch_info;
      elf_elfheader (obfd)->e_flags = old_flags;
    }

  return ok;
}

static bfd_boolean
mt_elf_print_private_bfd_data (bfd * abfd, void * ptr)
{
  FILE *   file = (FILE *) ptr;
  flagword flags;

  BFD_ASSERT (abfd != NULL && ptr != NULL);
  
  /* Print normal ELF private data.  */
  _bfd_elf_print_private_bfd_data (abfd, ptr);

  flags = elf_elfheader (abfd)->e_flags;
  fprintf (file, _("private flags = 0x%lx:"), (long)flags);

  switch (flags & EF_MT_CPU_MASK)
    {
    default:
    case EF_MT_CPU_MRISC:   fprintf (file, " ms1-16-002");	break;
    case EF_MT_CPU_MRISC2:  fprintf (file, " ms1-16-003");	break;
    case EF_MT_CPU_MS2:     fprintf (file, " ms2");	break;
    }

  fputc ('\n', file);

  return TRUE;
}


#define TARGET_BIG_SYM	 bfd_elf32_mt_vec
#define TARGET_BIG_NAME  "elf32-mt"

#define ELF_ARCH	 bfd_arch_mt
#define ELF_MACHINE_CODE EM_MT
#define ELF_MAXPAGESIZE  1 /* No pages on the MT.  */

#define elf_info_to_howto_rel			NULL
#define elf_info_to_howto			mt_info_to_howto_rela

#define elf_backend_relocate_section		mt_elf_relocate_section

#define bfd_elf32_bfd_reloc_type_lookup	        mt_reloc_type_lookup

#define elf_backend_gc_mark_hook		mt_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook		mt_elf_gc_sweep_hook
#define elf_backend_check_relocs                mt_elf_check_relocs
#define elf_backend_object_p		        mt_elf_object_p
#define elf_backend_rela_normal			1

#define elf_backend_can_gc_sections		1

#define bfd_elf32_bfd_set_private_flags		mt_elf_set_private_flags
#define bfd_elf32_bfd_copy_private_bfd_data	mt_elf_copy_private_bfd_data
#define bfd_elf32_bfd_merge_private_bfd_data	mt_elf_merge_private_bfd_data
#define bfd_elf32_bfd_print_private_bfd_data	mt_elf_print_private_bfd_data

#include "elf32-target.h"
