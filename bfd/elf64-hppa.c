/* Support for HPPA 64-bit ELF
   Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006
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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "alloca-conf.h"
#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/hppa.h"
#include "libhppa.h"
#include "elf64-hppa.h"
#define ARCH_SIZE	       64

#define PLT_ENTRY_SIZE 0x10
#define DLT_ENTRY_SIZE 0x8
#define OPD_ENTRY_SIZE 0x20

#define ELF_DYNAMIC_INTERPRETER "/usr/lib/pa20_64/dld.sl"

/* The stub is supposed to load the target address and target's DP
   value out of the PLT, then do an external branch to the target
   address.

   LDD PLTOFF(%r27),%r1
   BVE (%r1)
   LDD PLTOFF+8(%r27),%r27

   Note that we must use the LDD with a 14 bit displacement, not the one
   with a 5 bit displacement.  */
static char plt_stub[] = {0x53, 0x61, 0x00, 0x00, 0xe8, 0x20, 0xd0, 0x00,
			  0x53, 0x7b, 0x00, 0x00 };

struct elf64_hppa_dyn_hash_entry
{
  struct bfd_hash_entry root;

  /* Offsets for this symbol in various linker sections.  */
  bfd_vma dlt_offset;
  bfd_vma plt_offset;
  bfd_vma opd_offset;
  bfd_vma stub_offset;

  /* The symbol table entry, if any, that this was derived from.  */
  struct elf_link_hash_entry *h;

  /* The index of the (possibly local) symbol in the input bfd and its
     associated BFD.  Needed so that we can have relocs against local
     symbols in shared libraries.  */
  long sym_indx;
  bfd *owner;

  /* Dynamic symbols may need to have two different values.  One for
     the dynamic symbol table, one for the normal symbol table.

     In such cases we store the symbol's real value and section
     index here so we can restore the real value before we write
     the normal symbol table.  */
  bfd_vma st_value;
  int st_shndx;

  /* Used to count non-got, non-plt relocations for delayed sizing
     of relocation sections.  */
  struct elf64_hppa_dyn_reloc_entry
  {
    /* Next relocation in the chain.  */
    struct elf64_hppa_dyn_reloc_entry *next;

    /* The type of the relocation.  */
    int type;

    /* The input section of the relocation.  */
    asection *sec;

    /* The index of the section symbol for the input section of
       the relocation.  Only needed when building shared libraries.  */
    int sec_symndx;

    /* The offset within the input section of the relocation.  */
    bfd_vma offset;

    /* The addend for the relocation.  */
    bfd_vma addend;

  } *reloc_entries;

  /* Nonzero if this symbol needs an entry in one of the linker
     sections.  */
  unsigned want_dlt;
  unsigned want_plt;
  unsigned want_opd;
  unsigned want_stub;
};

struct elf64_hppa_dyn_hash_table
{
  struct bfd_hash_table root;
};

struct elf64_hppa_link_hash_table
{
  struct elf_link_hash_table root;

  /* Shortcuts to get to the various linker defined sections.  */
  asection *dlt_sec;
  asection *dlt_rel_sec;
  asection *plt_sec;
  asection *plt_rel_sec;
  asection *opd_sec;
  asection *opd_rel_sec;
  asection *other_rel_sec;

  /* Offset of __gp within .plt section.  When the PLT gets large we want
     to slide __gp into the PLT section so that we can continue to use
     single DP relative instructions to load values out of the PLT.  */
  bfd_vma gp_offset;

  /* Note this is not strictly correct.  We should create a stub section for
     each input section with calls.  The stub section should be placed before
     the section with the call.  */
  asection *stub_sec;

  bfd_vma text_segment_base;
  bfd_vma data_segment_base;

  struct elf64_hppa_dyn_hash_table dyn_hash_table;

  /* We build tables to map from an input section back to its
     symbol index.  This is the BFD for which we currently have
     a map.  */
  bfd *section_syms_bfd;

  /* Array of symbol numbers for each input section attached to the
     current BFD.  */
  int *section_syms;
};

#define elf64_hppa_hash_table(p) \
  ((struct elf64_hppa_link_hash_table *) ((p)->hash))

typedef struct bfd_hash_entry *(*new_hash_entry_func)
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));

static struct bfd_hash_entry *elf64_hppa_new_dyn_hash_entry
  PARAMS ((struct bfd_hash_entry *entry, struct bfd_hash_table *table,
	   const char *string));
static struct bfd_link_hash_table *elf64_hppa_hash_table_create
  PARAMS ((bfd *abfd));
static struct elf64_hppa_dyn_hash_entry *elf64_hppa_dyn_hash_lookup
  PARAMS ((struct elf64_hppa_dyn_hash_table *table, const char *string,
	   bfd_boolean create, bfd_boolean copy));
static void elf64_hppa_dyn_hash_traverse
  PARAMS ((struct elf64_hppa_dyn_hash_table *table,
	   bfd_boolean (*func) (struct elf64_hppa_dyn_hash_entry *, PTR),
	   PTR info));

static const char *get_dyn_name
  PARAMS ((bfd *, struct elf_link_hash_entry *,
	   const Elf_Internal_Rela *, char **, size_t *));

/* This must follow the definitions of the various derived linker
   hash tables and shared functions.  */
#include "elf-hppa.h"

static bfd_boolean elf64_hppa_object_p
  PARAMS ((bfd *));

static void elf64_hppa_post_process_headers
  PARAMS ((bfd *, struct bfd_link_info *));

static bfd_boolean elf64_hppa_create_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));

static bfd_boolean elf64_hppa_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));

static bfd_boolean elf64_hppa_mark_milli_and_exported_functions
  PARAMS ((struct elf_link_hash_entry *, PTR));

static bfd_boolean elf64_hppa_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));

static bfd_boolean elf64_hppa_link_output_symbol_hook
  PARAMS ((struct bfd_link_info *, const char *, Elf_Internal_Sym *,
	   asection *, struct elf_link_hash_entry *));

static bfd_boolean elf64_hppa_finish_dynamic_symbol
  PARAMS ((bfd *, struct bfd_link_info *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));

static int elf64_hppa_additional_program_headers
  PARAMS ((bfd *));

static bfd_boolean elf64_hppa_modify_segment_map
  PARAMS ((bfd *, struct bfd_link_info *));

static enum elf_reloc_type_class elf64_hppa_reloc_type_class
  PARAMS ((const Elf_Internal_Rela *));

static bfd_boolean elf64_hppa_finish_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));

static bfd_boolean elf64_hppa_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *,
	   asection *, const Elf_Internal_Rela *));

static bfd_boolean elf64_hppa_dynamic_symbol_p
  PARAMS ((struct elf_link_hash_entry *, struct bfd_link_info *));

static bfd_boolean elf64_hppa_mark_exported_functions
  PARAMS ((struct elf_link_hash_entry *, PTR));

static bfd_boolean elf64_hppa_finalize_opd
  PARAMS ((struct elf64_hppa_dyn_hash_entry *, PTR));

static bfd_boolean elf64_hppa_finalize_dlt
  PARAMS ((struct elf64_hppa_dyn_hash_entry *, PTR));

static bfd_boolean allocate_global_data_dlt
  PARAMS ((struct elf64_hppa_dyn_hash_entry *, PTR));

static bfd_boolean allocate_global_data_plt
  PARAMS ((struct elf64_hppa_dyn_hash_entry *, PTR));

static bfd_boolean allocate_global_data_stub
  PARAMS ((struct elf64_hppa_dyn_hash_entry *, PTR));

static bfd_boolean allocate_global_data_opd
  PARAMS ((struct elf64_hppa_dyn_hash_entry *, PTR));

static bfd_boolean get_reloc_section
  PARAMS ((bfd *, struct elf64_hppa_link_hash_table *, asection *));

static bfd_boolean count_dyn_reloc
  PARAMS ((bfd *, struct elf64_hppa_dyn_hash_entry *,
	   int, asection *, int, bfd_vma, bfd_vma));

static bfd_boolean allocate_dynrel_entries
  PARAMS ((struct elf64_hppa_dyn_hash_entry *, PTR));

static bfd_boolean elf64_hppa_finalize_dynreloc
  PARAMS ((struct elf64_hppa_dyn_hash_entry *, PTR));

static bfd_boolean get_opd
  PARAMS ((bfd *, struct bfd_link_info *, struct elf64_hppa_link_hash_table *));

static bfd_boolean get_plt
  PARAMS ((bfd *, struct bfd_link_info *, struct elf64_hppa_link_hash_table *));

static bfd_boolean get_dlt
  PARAMS ((bfd *, struct bfd_link_info *, struct elf64_hppa_link_hash_table *));

static bfd_boolean get_stub
  PARAMS ((bfd *, struct bfd_link_info *, struct elf64_hppa_link_hash_table *));

static int elf64_hppa_elf_get_symbol_type
  PARAMS ((Elf_Internal_Sym *, int));

static bfd_boolean
elf64_hppa_dyn_hash_table_init (struct elf64_hppa_dyn_hash_table *ht,
				bfd *abfd ATTRIBUTE_UNUSED,
				new_hash_entry_func new,
				unsigned int entsize)
{
  memset (ht, 0, sizeof (*ht));
  return bfd_hash_table_init (&ht->root, new, entsize);
}

static struct bfd_hash_entry*
elf64_hppa_new_dyn_hash_entry (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct elf64_hppa_dyn_hash_entry *ret;
  ret = (struct elf64_hppa_dyn_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (!ret)
    ret = bfd_hash_allocate (table, sizeof (*ret));

  if (!ret)
    return 0;

  /* Call the allocation method of the superclass.  */
  ret = ((struct elf64_hppa_dyn_hash_entry *)
	 bfd_hash_newfunc ((struct bfd_hash_entry *) ret, table, string));

  /* Initialize our local data.  All zeros.  */
  memset (&ret->dlt_offset, 0,
	  (sizeof (struct elf64_hppa_dyn_hash_entry)
	   - offsetof (struct elf64_hppa_dyn_hash_entry, dlt_offset)));

  return &ret->root;
}

/* Create the derived linker hash table.  The PA64 ELF port uses this
   derived hash table to keep information specific to the PA ElF
   linker (without using static variables).  */

static struct bfd_link_hash_table*
elf64_hppa_hash_table_create (abfd)
     bfd *abfd;
{
  struct elf64_hppa_link_hash_table *ret;

  ret = bfd_zalloc (abfd, (bfd_size_type) sizeof (*ret));
  if (!ret)
    return 0;
  if (!_bfd_elf_link_hash_table_init (&ret->root, abfd,
				      _bfd_elf_link_hash_newfunc,
				      sizeof (struct elf_link_hash_entry)))
    {
      bfd_release (abfd, ret);
      return 0;
    }

  if (!elf64_hppa_dyn_hash_table_init (&ret->dyn_hash_table, abfd,
				       elf64_hppa_new_dyn_hash_entry,
				       sizeof (struct elf64_hppa_dyn_hash_entry)))
    return 0;
  return &ret->root.root;
}

/* Look up an entry in a PA64 ELF linker hash table.  */

static struct elf64_hppa_dyn_hash_entry *
elf64_hppa_dyn_hash_lookup(table, string, create, copy)
     struct elf64_hppa_dyn_hash_table *table;
     const char *string;
     bfd_boolean create, copy;
{
  return ((struct elf64_hppa_dyn_hash_entry *)
	  bfd_hash_lookup (&table->root, string, create, copy));
}

/* Traverse a PA64 ELF linker hash table.  */

static void
elf64_hppa_dyn_hash_traverse (table, func, info)
     struct elf64_hppa_dyn_hash_table *table;
     bfd_boolean (*func) PARAMS ((struct elf64_hppa_dyn_hash_entry *, PTR));
     PTR info;
{
  (bfd_hash_traverse
   (&table->root,
    (bfd_boolean (*) PARAMS ((struct bfd_hash_entry *, PTR))) func,
    info));
}

/* Return nonzero if ABFD represents a PA2.0 ELF64 file.

   Additionally we set the default architecture and machine.  */
static bfd_boolean
elf64_hppa_object_p (abfd)
     bfd *abfd;
{
  Elf_Internal_Ehdr * i_ehdrp;
  unsigned int flags;

  i_ehdrp = elf_elfheader (abfd);
  if (strcmp (bfd_get_target (abfd), "elf64-hppa-linux") == 0)
    {
      /* GCC on hppa-linux produces binaries with OSABI=Linux,
	 but the kernel produces corefiles with OSABI=SysV.  */
      if (i_ehdrp->e_ident[EI_OSABI] != ELFOSABI_LINUX
	  && i_ehdrp->e_ident[EI_OSABI] != ELFOSABI_NONE) /* aka SYSV */
	return FALSE;
    }
  else
    {
      /* HPUX produces binaries with OSABI=HPUX,
	 but the kernel produces corefiles with OSABI=SysV.  */
      if (i_ehdrp->e_ident[EI_OSABI] != ELFOSABI_HPUX
	  && i_ehdrp->e_ident[EI_OSABI] != ELFOSABI_NONE) /* aka SYSV */
	return FALSE;
    }

  flags = i_ehdrp->e_flags;
  switch (flags & (EF_PARISC_ARCH | EF_PARISC_WIDE))
    {
    case EFA_PARISC_1_0:
      return bfd_default_set_arch_mach (abfd, bfd_arch_hppa, 10);
    case EFA_PARISC_1_1:
      return bfd_default_set_arch_mach (abfd, bfd_arch_hppa, 11);
    case EFA_PARISC_2_0:
      if (i_ehdrp->e_ident[EI_CLASS] == ELFCLASS64)
        return bfd_default_set_arch_mach (abfd, bfd_arch_hppa, 25);
      else
        return bfd_default_set_arch_mach (abfd, bfd_arch_hppa, 20);
    case EFA_PARISC_2_0 | EF_PARISC_WIDE:
      return bfd_default_set_arch_mach (abfd, bfd_arch_hppa, 25);
    }
  /* Don't be fussy.  */
  return TRUE;
}

/* Given section type (hdr->sh_type), return a boolean indicating
   whether or not the section is an elf64-hppa specific section.  */
static bfd_boolean
elf64_hppa_section_from_shdr (bfd *abfd,
			      Elf_Internal_Shdr *hdr,
			      const char *name,
			      int shindex)
{
  asection *newsect;

  switch (hdr->sh_type)
    {
    case SHT_PARISC_EXT:
      if (strcmp (name, ".PARISC.archext") != 0)
	return FALSE;
      break;
    case SHT_PARISC_UNWIND:
      if (strcmp (name, ".PARISC.unwind") != 0)
	return FALSE;
      break;
    case SHT_PARISC_DOC:
    case SHT_PARISC_ANNOT:
    default:
      return FALSE;
    }

  if (! _bfd_elf_make_section_from_shdr (abfd, hdr, name, shindex))
    return FALSE;
  newsect = hdr->bfd_section;

  return TRUE;
}

/* Construct a string for use in the elf64_hppa_dyn_hash_table.  The
   name describes what was once potentially anonymous memory.  We
   allocate memory as necessary, possibly reusing PBUF/PLEN.  */

static const char *
get_dyn_name (abfd, h, rel, pbuf, plen)
     bfd *abfd;
     struct elf_link_hash_entry *h;
     const Elf_Internal_Rela *rel;
     char **pbuf;
     size_t *plen;
{
  asection *sec = abfd->sections;
  size_t nlen, tlen;
  char *buf;
  size_t len;

  if (h && rel->r_addend == 0)
    return h->root.root.string;

  if (h)
    nlen = strlen (h->root.root.string);
  else
    nlen = 8 + 1 + sizeof (rel->r_info) * 2 - 8;
  tlen = nlen + 1 + sizeof (rel->r_addend) * 2 + 1;

  len = *plen;
  buf = *pbuf;
  if (len < tlen)
    {
      if (buf)
	free (buf);
      *pbuf = buf = malloc (tlen);
      *plen = len = tlen;
      if (!buf)
	return NULL;
    }

  if (h)
    {
      memcpy (buf, h->root.root.string, nlen);
      buf[nlen++] = '+';
      sprintf_vma (buf + nlen, rel->r_addend);
    }
  else
    {
      nlen = sprintf (buf, "%x:%lx",
		      sec->id & 0xffffffff,
		      (long) ELF64_R_SYM (rel->r_info));
      if (rel->r_addend)
	{
	  buf[nlen++] = '+';
	  sprintf_vma (buf + nlen, rel->r_addend);
	}
    }

  return buf;
}

/* SEC is a section containing relocs for an input BFD when linking; return
   a suitable section for holding relocs in the output BFD for a link.  */

static bfd_boolean
get_reloc_section (abfd, hppa_info, sec)
     bfd *abfd;
     struct elf64_hppa_link_hash_table *hppa_info;
     asection *sec;
{
  const char *srel_name;
  asection *srel;
  bfd *dynobj;

  srel_name = (bfd_elf_string_from_elf_section
	       (abfd, elf_elfheader(abfd)->e_shstrndx,
		elf_section_data(sec)->rel_hdr.sh_name));
  if (srel_name == NULL)
    return FALSE;

  BFD_ASSERT ((strncmp (srel_name, ".rela", 5) == 0
	       && strcmp (bfd_get_section_name (abfd, sec),
			  srel_name+5) == 0)
	      || (strncmp (srel_name, ".rel", 4) == 0
		  && strcmp (bfd_get_section_name (abfd, sec),
			     srel_name+4) == 0));

  dynobj = hppa_info->root.dynobj;
  if (!dynobj)
    hppa_info->root.dynobj = dynobj = abfd;

  srel = bfd_get_section_by_name (dynobj, srel_name);
  if (srel == NULL)
    {
      srel = bfd_make_section_with_flags (dynobj, srel_name,
					  (SEC_ALLOC
					   | SEC_LOAD
					   | SEC_HAS_CONTENTS
					   | SEC_IN_MEMORY
					   | SEC_LINKER_CREATED
					   | SEC_READONLY));
      if (srel == NULL
	  || !bfd_set_section_alignment (dynobj, srel, 3))
	return FALSE;
    }

  hppa_info->other_rel_sec = srel;
  return TRUE;
}

/* Add a new entry to the list of dynamic relocations against DYN_H.

   We use this to keep a record of all the FPTR relocations against a
   particular symbol so that we can create FPTR relocations in the
   output file.  */

static bfd_boolean
count_dyn_reloc (abfd, dyn_h, type, sec, sec_symndx, offset, addend)
     bfd *abfd;
     struct elf64_hppa_dyn_hash_entry *dyn_h;
     int type;
     asection *sec;
     int sec_symndx;
     bfd_vma offset;
     bfd_vma addend;
{
  struct elf64_hppa_dyn_reloc_entry *rent;

  rent = (struct elf64_hppa_dyn_reloc_entry *)
  bfd_alloc (abfd, (bfd_size_type) sizeof (*rent));
  if (!rent)
    return FALSE;

  rent->next = dyn_h->reloc_entries;
  rent->type = type;
  rent->sec = sec;
  rent->sec_symndx = sec_symndx;
  rent->offset = offset;
  rent->addend = addend;
  dyn_h->reloc_entries = rent;

  return TRUE;
}

/* Scan the RELOCS and record the type of dynamic entries that each
   referenced symbol needs.  */

static bfd_boolean
elf64_hppa_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  struct elf64_hppa_link_hash_table *hppa_info;
  const Elf_Internal_Rela *relend;
  Elf_Internal_Shdr *symtab_hdr;
  const Elf_Internal_Rela *rel;
  asection *dlt, *plt, *stubs;
  char *buf;
  size_t buf_len;
  int sec_symndx;

  if (info->relocatable)
    return TRUE;

  /* If this is the first dynamic object found in the link, create
     the special sections required for dynamic linking.  */
  if (! elf_hash_table (info)->dynamic_sections_created)
    {
      if (! _bfd_elf_link_create_dynamic_sections (abfd, info))
	return FALSE;
    }

  hppa_info = elf64_hppa_hash_table (info);
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  /* If necessary, build a new table holding section symbols indices
     for this BFD.  */

  if (info->shared && hppa_info->section_syms_bfd != abfd)
    {
      unsigned long i;
      unsigned int highest_shndx;
      Elf_Internal_Sym *local_syms = NULL;
      Elf_Internal_Sym *isym, *isymend;
      bfd_size_type amt;

      /* We're done with the old cache of section index to section symbol
	 index information.  Free it.

	 ?!? Note we leak the last section_syms array.  Presumably we
	 could free it in one of the later routines in this file.  */
      if (hppa_info->section_syms)
	free (hppa_info->section_syms);

      /* Read this BFD's local symbols.  */
      if (symtab_hdr->sh_info != 0)
	{
	  local_syms = (Elf_Internal_Sym *) symtab_hdr->contents;
	  if (local_syms == NULL)
	    local_syms = bfd_elf_get_elf_syms (abfd, symtab_hdr,
					       symtab_hdr->sh_info, 0,
					       NULL, NULL, NULL);
	  if (local_syms == NULL)
	    return FALSE;
	}

      /* Record the highest section index referenced by the local symbols.  */
      highest_shndx = 0;
      isymend = local_syms + symtab_hdr->sh_info;
      for (isym = local_syms; isym < isymend; isym++)
	{
	  if (isym->st_shndx > highest_shndx)
	    highest_shndx = isym->st_shndx;
	}

      /* Allocate an array to hold the section index to section symbol index
	 mapping.  Bump by one since we start counting at zero.  */
      highest_shndx++;
      amt = highest_shndx;
      amt *= sizeof (int);
      hppa_info->section_syms = (int *) bfd_malloc (amt);

      /* Now walk the local symbols again.  If we find a section symbol,
	 record the index of the symbol into the section_syms array.  */
      for (i = 0, isym = local_syms; isym < isymend; i++, isym++)
	{
	  if (ELF_ST_TYPE (isym->st_info) == STT_SECTION)
	    hppa_info->section_syms[isym->st_shndx] = i;
	}

      /* We are finished with the local symbols.  */
      if (local_syms != NULL
	  && symtab_hdr->contents != (unsigned char *) local_syms)
	{
	  if (! info->keep_memory)
	    free (local_syms);
	  else
	    {
	      /* Cache the symbols for elf_link_input_bfd.  */
	      symtab_hdr->contents = (unsigned char *) local_syms;
	    }
	}

      /* Record which BFD we built the section_syms mapping for.  */
      hppa_info->section_syms_bfd = abfd;
    }

  /* Record the symbol index for this input section.  We may need it for
     relocations when building shared libraries.  When not building shared
     libraries this value is never really used, but assign it to zero to
     prevent out of bounds memory accesses in other routines.  */
  if (info->shared)
    {
      sec_symndx = _bfd_elf_section_from_bfd_section (abfd, sec);

      /* If we did not find a section symbol for this section, then
	 something went terribly wrong above.  */
      if (sec_symndx == -1)
	return FALSE;

      sec_symndx = hppa_info->section_syms[sec_symndx];
    }
  else
    sec_symndx = 0;

  dlt = plt = stubs = NULL;
  buf = NULL;
  buf_len = 0;

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; ++rel)
    {
      enum
	{
	  NEED_DLT = 1,
	  NEED_PLT = 2,
	  NEED_STUB = 4,
	  NEED_OPD = 8,
	  NEED_DYNREL = 16,
	};

      struct elf_link_hash_entry *h = NULL;
      unsigned long r_symndx = ELF64_R_SYM (rel->r_info);
      struct elf64_hppa_dyn_hash_entry *dyn_h;
      int need_entry;
      const char *addr_name;
      bfd_boolean maybe_dynamic;
      int dynrel_type = R_PARISC_NONE;
      static reloc_howto_type *howto;

      if (r_symndx >= symtab_hdr->sh_info)
	{
	  /* We're dealing with a global symbol -- find its hash entry
	     and mark it as being referenced.  */
	  long indx = r_symndx - symtab_hdr->sh_info;
	  h = elf_sym_hashes (abfd)[indx];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  h->ref_regular = 1;
	}

      /* We can only get preliminary data on whether a symbol is
	 locally or externally defined, as not all of the input files
	 have yet been processed.  Do something with what we know, as
	 this may help reduce memory usage and processing time later.  */
      maybe_dynamic = FALSE;
      if (h && ((info->shared
		 && (!info->symbolic
		     || info->unresolved_syms_in_shared_libs == RM_IGNORE))
		|| !h->def_regular
		|| h->root.type == bfd_link_hash_defweak))
	maybe_dynamic = TRUE;

      howto = elf_hppa_howto_table + ELF64_R_TYPE (rel->r_info);
      need_entry = 0;
      switch (howto->type)
	{
	/* These are simple indirect references to symbols through the
	   DLT.  We need to create a DLT entry for any symbols which
	   appears in a DLTIND relocation.  */
	case R_PARISC_DLTIND21L:
	case R_PARISC_DLTIND14R:
	case R_PARISC_DLTIND14F:
	case R_PARISC_DLTIND14WR:
	case R_PARISC_DLTIND14DR:
	  need_entry = NEED_DLT;
	  break;

	/* ?!?  These need a DLT entry.  But I have no idea what to do with
	   the "link time TP value.  */
	case R_PARISC_LTOFF_TP21L:
	case R_PARISC_LTOFF_TP14R:
	case R_PARISC_LTOFF_TP14F:
	case R_PARISC_LTOFF_TP64:
	case R_PARISC_LTOFF_TP14WR:
	case R_PARISC_LTOFF_TP14DR:
	case R_PARISC_LTOFF_TP16F:
	case R_PARISC_LTOFF_TP16WF:
	case R_PARISC_LTOFF_TP16DF:
	  need_entry = NEED_DLT;
	  break;

	/* These are function calls.  Depending on their precise target we
	   may need to make a stub for them.  The stub uses the PLT, so we
	   need to create PLT entries for these symbols too.  */
	case R_PARISC_PCREL12F:
	case R_PARISC_PCREL17F:
	case R_PARISC_PCREL22F:
	case R_PARISC_PCREL32:
	case R_PARISC_PCREL64:
	case R_PARISC_PCREL21L:
	case R_PARISC_PCREL17R:
	case R_PARISC_PCREL17C:
	case R_PARISC_PCREL14R:
	case R_PARISC_PCREL14F:
	case R_PARISC_PCREL22C:
	case R_PARISC_PCREL14WR:
	case R_PARISC_PCREL14DR:
	case R_PARISC_PCREL16F:
	case R_PARISC_PCREL16WF:
	case R_PARISC_PCREL16DF:
	  need_entry = (NEED_PLT | NEED_STUB);
	  break;

	case R_PARISC_PLTOFF21L:
	case R_PARISC_PLTOFF14R:
	case R_PARISC_PLTOFF14F:
	case R_PARISC_PLTOFF14WR:
	case R_PARISC_PLTOFF14DR:
	case R_PARISC_PLTOFF16F:
	case R_PARISC_PLTOFF16WF:
	case R_PARISC_PLTOFF16DF:
	  need_entry = (NEED_PLT);
	  break;

	case R_PARISC_DIR64:
	  if (info->shared || maybe_dynamic)
	    need_entry = (NEED_DYNREL);
	  dynrel_type = R_PARISC_DIR64;
	  break;

	/* This is an indirect reference through the DLT to get the address
	   of a OPD descriptor.  Thus we need to make a DLT entry that points
	   to an OPD entry.  */
	case R_PARISC_LTOFF_FPTR21L:
	case R_PARISC_LTOFF_FPTR14R:
	case R_PARISC_LTOFF_FPTR14WR:
	case R_PARISC_LTOFF_FPTR14DR:
	case R_PARISC_LTOFF_FPTR32:
	case R_PARISC_LTOFF_FPTR64:
	case R_PARISC_LTOFF_FPTR16F:
	case R_PARISC_LTOFF_FPTR16WF:
	case R_PARISC_LTOFF_FPTR16DF:
	  if (info->shared || maybe_dynamic)
	    need_entry = (NEED_DLT | NEED_OPD);
	  else
	    need_entry = (NEED_DLT | NEED_OPD);
	  dynrel_type = R_PARISC_FPTR64;
	  break;

	/* This is a simple OPD entry.  */
	case R_PARISC_FPTR64:
	  if (info->shared || maybe_dynamic)
	    need_entry = (NEED_OPD | NEED_DYNREL);
	  else
	    need_entry = (NEED_OPD);
	  dynrel_type = R_PARISC_FPTR64;
	  break;

	/* Add more cases as needed.  */
	}

      if (!need_entry)
	continue;

      /* Collect a canonical name for this address.  */
      addr_name = get_dyn_name (abfd, h, rel, &buf, &buf_len);

      /* Collect the canonical entry data for this address.  */
      dyn_h = elf64_hppa_dyn_hash_lookup (&hppa_info->dyn_hash_table,
					  addr_name, TRUE, TRUE);
      BFD_ASSERT (dyn_h);

      /* Stash away enough information to be able to find this symbol
	 regardless of whether or not it is local or global.  */
      dyn_h->h = h;
      dyn_h->owner = abfd;
      dyn_h->sym_indx = r_symndx;

      /* ?!? We may need to do some error checking in here.  */
      /* Create what's needed.  */
      if (need_entry & NEED_DLT)
	{
	  if (! hppa_info->dlt_sec
	      && ! get_dlt (abfd, info, hppa_info))
	    goto err_out;
	  dyn_h->want_dlt = 1;
	}

      if (need_entry & NEED_PLT)
	{
	  if (! hppa_info->plt_sec
	      && ! get_plt (abfd, info, hppa_info))
	    goto err_out;
	  dyn_h->want_plt = 1;
	}

      if (need_entry & NEED_STUB)
	{
	  if (! hppa_info->stub_sec
	      && ! get_stub (abfd, info, hppa_info))
	    goto err_out;
	  dyn_h->want_stub = 1;
	}

      if (need_entry & NEED_OPD)
	{
	  if (! hppa_info->opd_sec
	      && ! get_opd (abfd, info, hppa_info))
	    goto err_out;

	  dyn_h->want_opd = 1;

	  /* FPTRs are not allocated by the dynamic linker for PA64, though
	     it is possible that will change in the future.  */

	  /* This could be a local function that had its address taken, in
	     which case H will be NULL.  */
	  if (h)
	    h->needs_plt = 1;
	}

      /* Add a new dynamic relocation to the chain of dynamic
	 relocations for this symbol.  */
      if ((need_entry & NEED_DYNREL) && (sec->flags & SEC_ALLOC))
	{
	  if (! hppa_info->other_rel_sec
	      && ! get_reloc_section (abfd, hppa_info, sec))
	    goto err_out;

	  if (!count_dyn_reloc (abfd, dyn_h, dynrel_type, sec,
				sec_symndx, rel->r_offset, rel->r_addend))
	    goto err_out;

	  /* If we are building a shared library and we just recorded
	     a dynamic R_PARISC_FPTR64 relocation, then make sure the
	     section symbol for this section ends up in the dynamic
	     symbol table.  */
	  if (info->shared && dynrel_type == R_PARISC_FPTR64
	      && ! (bfd_elf_link_record_local_dynamic_symbol
		    (info, abfd, sec_symndx)))
	    return FALSE;
	}
    }

  if (buf)
    free (buf);
  return TRUE;

 err_out:
  if (buf)
    free (buf);
  return FALSE;
}

struct elf64_hppa_allocate_data
{
  struct bfd_link_info *info;
  bfd_size_type ofs;
};

/* Should we do dynamic things to this symbol?  */

static bfd_boolean
elf64_hppa_dynamic_symbol_p (h, info)
     struct elf_link_hash_entry *h;
     struct bfd_link_info *info;
{
  /* ??? What, if anything, needs to happen wrt STV_PROTECTED symbols
     and relocations that retrieve a function descriptor?  Assume the
     worst for now.  */
  if (_bfd_elf_dynamic_symbol_p (h, info, 1))
    {
      /* ??? Why is this here and not elsewhere is_local_label_name.  */
      if (h->root.root.string[0] == '$' && h->root.root.string[1] == '$')
	return FALSE;

      return TRUE;
    }
  else
    return FALSE;
}

/* Mark all functions exported by this file so that we can later allocate
   entries in .opd for them.  */

static bfd_boolean
elf64_hppa_mark_exported_functions (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  struct bfd_link_info *info = (struct bfd_link_info *)data;
  struct elf64_hppa_link_hash_table *hppa_info;

  hppa_info = elf64_hppa_hash_table (info);

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h
      && (h->root.type == bfd_link_hash_defined
	  || h->root.type == bfd_link_hash_defweak)
      && h->root.u.def.section->output_section != NULL
      && h->type == STT_FUNC)
    {
       struct elf64_hppa_dyn_hash_entry *dyn_h;

      /* Add this symbol to the PA64 linker hash table.  */
      dyn_h = elf64_hppa_dyn_hash_lookup (&hppa_info->dyn_hash_table,
					  h->root.root.string, TRUE, TRUE);
      BFD_ASSERT (dyn_h);
      dyn_h->h = h;

      if (! hppa_info->opd_sec
	  && ! get_opd (hppa_info->root.dynobj, info, hppa_info))
	return FALSE;

      dyn_h->want_opd = 1;
      /* Put a flag here for output_symbol_hook.  */
      dyn_h->st_shndx = -1;
      h->needs_plt = 1;
    }

  return TRUE;
}

/* Allocate space for a DLT entry.  */

static bfd_boolean
allocate_global_data_dlt (dyn_h, data)
     struct elf64_hppa_dyn_hash_entry *dyn_h;
     PTR data;
{
  struct elf64_hppa_allocate_data *x = (struct elf64_hppa_allocate_data *)data;

  if (dyn_h->want_dlt)
    {
      struct elf_link_hash_entry *h = dyn_h->h;

      if (x->info->shared)
	{
	  /* Possibly add the symbol to the local dynamic symbol
	     table since we might need to create a dynamic relocation
	     against it.  */
	  if (! h
	      || (h->dynindx == -1 && h->type != STT_PARISC_MILLI))
	    {
	      bfd *owner;
	      owner = (h ? h->root.u.def.section->owner : dyn_h->owner);

	      if (! (bfd_elf_link_record_local_dynamic_symbol
		     (x->info, owner, dyn_h->sym_indx)))
		return FALSE;
	    }
	}

      dyn_h->dlt_offset = x->ofs;
      x->ofs += DLT_ENTRY_SIZE;
    }
  return TRUE;
}

/* Allocate space for a DLT.PLT entry.  */

static bfd_boolean
allocate_global_data_plt (dyn_h, data)
     struct elf64_hppa_dyn_hash_entry *dyn_h;
     PTR data;
{
  struct elf64_hppa_allocate_data *x = (struct elf64_hppa_allocate_data *)data;

  if (dyn_h->want_plt
      && elf64_hppa_dynamic_symbol_p (dyn_h->h, x->info)
      && !((dyn_h->h->root.type == bfd_link_hash_defined
	    || dyn_h->h->root.type == bfd_link_hash_defweak)
	   && dyn_h->h->root.u.def.section->output_section != NULL))
    {
      dyn_h->plt_offset = x->ofs;
      x->ofs += PLT_ENTRY_SIZE;
      if (dyn_h->plt_offset < 0x2000)
	elf64_hppa_hash_table (x->info)->gp_offset = dyn_h->plt_offset;
    }
  else
    dyn_h->want_plt = 0;

  return TRUE;
}

/* Allocate space for a STUB entry.  */

static bfd_boolean
allocate_global_data_stub (dyn_h, data)
     struct elf64_hppa_dyn_hash_entry *dyn_h;
     PTR data;
{
  struct elf64_hppa_allocate_data *x = (struct elf64_hppa_allocate_data *)data;

  if (dyn_h->want_stub
      && elf64_hppa_dynamic_symbol_p (dyn_h->h, x->info)
      && !((dyn_h->h->root.type == bfd_link_hash_defined
	    || dyn_h->h->root.type == bfd_link_hash_defweak)
	   && dyn_h->h->root.u.def.section->output_section != NULL))
    {
      dyn_h->stub_offset = x->ofs;
      x->ofs += sizeof (plt_stub);
    }
  else
    dyn_h->want_stub = 0;
  return TRUE;
}

/* Allocate space for a FPTR entry.  */

static bfd_boolean
allocate_global_data_opd (dyn_h, data)
     struct elf64_hppa_dyn_hash_entry *dyn_h;
     PTR data;
{
  struct elf64_hppa_allocate_data *x = (struct elf64_hppa_allocate_data *)data;

  if (dyn_h->want_opd)
    {
      struct elf_link_hash_entry *h = dyn_h->h;

      if (h)
	while (h->root.type == bfd_link_hash_indirect
	       || h->root.type == bfd_link_hash_warning)
	  h = (struct elf_link_hash_entry *) h->root.u.i.link;

      /* We never need an opd entry for a symbol which is not
	 defined by this output file.  */
      if (h && (h->root.type == bfd_link_hash_undefined
		|| h->root.type == bfd_link_hash_undefweak
		|| h->root.u.def.section->output_section == NULL))
	dyn_h->want_opd = 0;

      /* If we are creating a shared library, took the address of a local
	 function or might export this function from this object file, then
	 we have to create an opd descriptor.  */
      else if (x->info->shared
	       || h == NULL
	       || (h->dynindx == -1 && h->type != STT_PARISC_MILLI)
	       || (h->root.type == bfd_link_hash_defined
		   || h->root.type == bfd_link_hash_defweak))
	{
	  /* If we are creating a shared library, then we will have to
	     create a runtime relocation for the symbol to properly
	     initialize the .opd entry.  Make sure the symbol gets
	     added to the dynamic symbol table.  */
	  if (x->info->shared
	      && (h == NULL || (h->dynindx == -1)))
	    {
	      bfd *owner;
	      owner = (h ? h->root.u.def.section->owner : dyn_h->owner);

	      if (!bfd_elf_link_record_local_dynamic_symbol
		    (x->info, owner, dyn_h->sym_indx))
		return FALSE;
	    }

	  /* This may not be necessary or desirable anymore now that
	     we have some support for dealing with section symbols
	     in dynamic relocs.  But name munging does make the result
	     much easier to debug.  ie, the EPLT reloc will reference
	     a symbol like .foobar, instead of .text + offset.  */
	  if (x->info->shared && h)
	    {
	      char *new_name;
	      struct elf_link_hash_entry *nh;

	      new_name = alloca (strlen (h->root.root.string) + 2);
	      new_name[0] = '.';
	      strcpy (new_name + 1, h->root.root.string);

	      nh = elf_link_hash_lookup (elf_hash_table (x->info),
					 new_name, TRUE, TRUE, TRUE);

	      nh->root.type = h->root.type;
	      nh->root.u.def.value = h->root.u.def.value;
	      nh->root.u.def.section = h->root.u.def.section;

	      if (! bfd_elf_link_record_dynamic_symbol (x->info, nh))
		return FALSE;

	     }
	  dyn_h->opd_offset = x->ofs;
	  x->ofs += OPD_ENTRY_SIZE;
	}

      /* Otherwise we do not need an opd entry.  */
      else
	dyn_h->want_opd = 0;
    }
  return TRUE;
}

/* HP requires the EI_OSABI field to be filled in.  The assignment to
   EI_ABIVERSION may not be strictly necessary.  */

static void
elf64_hppa_post_process_headers (abfd, link_info)
     bfd * abfd;
     struct bfd_link_info * link_info ATTRIBUTE_UNUSED;
{
  Elf_Internal_Ehdr * i_ehdrp;

  i_ehdrp = elf_elfheader (abfd);

  if (strcmp (bfd_get_target (abfd), "elf64-hppa-linux") == 0)
    {
      i_ehdrp->e_ident[EI_OSABI] = ELFOSABI_LINUX;
    }
  else
    {
      i_ehdrp->e_ident[EI_OSABI] = ELFOSABI_HPUX;
      i_ehdrp->e_ident[EI_ABIVERSION] = 1;
    }
}

/* Create function descriptor section (.opd).  This section is called .opd
   because it contains "official procedure descriptors".  The "official"
   refers to the fact that these descriptors are used when taking the address
   of a procedure, thus ensuring a unique address for each procedure.  */

static bfd_boolean
get_opd (abfd, info, hppa_info)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     struct elf64_hppa_link_hash_table *hppa_info;
{
  asection *opd;
  bfd *dynobj;

  opd = hppa_info->opd_sec;
  if (!opd)
    {
      dynobj = hppa_info->root.dynobj;
      if (!dynobj)
	hppa_info->root.dynobj = dynobj = abfd;

      opd = bfd_make_section_with_flags (dynobj, ".opd",
					 (SEC_ALLOC
					  | SEC_LOAD
					  | SEC_HAS_CONTENTS
					  | SEC_IN_MEMORY
					  | SEC_LINKER_CREATED));
      if (!opd
	  || !bfd_set_section_alignment (abfd, opd, 3))
	{
	  BFD_ASSERT (0);
	  return FALSE;
	}

      hppa_info->opd_sec = opd;
    }

  return TRUE;
}

/* Create the PLT section.  */

static bfd_boolean
get_plt (abfd, info, hppa_info)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     struct elf64_hppa_link_hash_table *hppa_info;
{
  asection *plt;
  bfd *dynobj;

  plt = hppa_info->plt_sec;
  if (!plt)
    {
      dynobj = hppa_info->root.dynobj;
      if (!dynobj)
	hppa_info->root.dynobj = dynobj = abfd;

      plt = bfd_make_section_with_flags (dynobj, ".plt",
					 (SEC_ALLOC
					  | SEC_LOAD
					  | SEC_HAS_CONTENTS
					  | SEC_IN_MEMORY
					  | SEC_LINKER_CREATED));
      if (!plt
	  || !bfd_set_section_alignment (abfd, plt, 3))
	{
	  BFD_ASSERT (0);
	  return FALSE;
	}

      hppa_info->plt_sec = plt;
    }

  return TRUE;
}

/* Create the DLT section.  */

static bfd_boolean
get_dlt (abfd, info, hppa_info)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     struct elf64_hppa_link_hash_table *hppa_info;
{
  asection *dlt;
  bfd *dynobj;

  dlt = hppa_info->dlt_sec;
  if (!dlt)
    {
      dynobj = hppa_info->root.dynobj;
      if (!dynobj)
	hppa_info->root.dynobj = dynobj = abfd;

      dlt = bfd_make_section_with_flags (dynobj, ".dlt",
					 (SEC_ALLOC
					  | SEC_LOAD
					  | SEC_HAS_CONTENTS
					  | SEC_IN_MEMORY
					  | SEC_LINKER_CREATED));
      if (!dlt
	  || !bfd_set_section_alignment (abfd, dlt, 3))
	{
	  BFD_ASSERT (0);
	  return FALSE;
	}

      hppa_info->dlt_sec = dlt;
    }

  return TRUE;
}

/* Create the stubs section.  */

static bfd_boolean
get_stub (abfd, info, hppa_info)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     struct elf64_hppa_link_hash_table *hppa_info;
{
  asection *stub;
  bfd *dynobj;

  stub = hppa_info->stub_sec;
  if (!stub)
    {
      dynobj = hppa_info->root.dynobj;
      if (!dynobj)
	hppa_info->root.dynobj = dynobj = abfd;

      stub = bfd_make_section_with_flags (dynobj, ".stub",
					  (SEC_ALLOC | SEC_LOAD
					   | SEC_HAS_CONTENTS
					   | SEC_IN_MEMORY
					   | SEC_READONLY
					   | SEC_LINKER_CREATED));
      if (!stub
	  || !bfd_set_section_alignment (abfd, stub, 3))
	{
	  BFD_ASSERT (0);
	  return FALSE;
	}

      hppa_info->stub_sec = stub;
    }

  return TRUE;
}

/* Create sections necessary for dynamic linking.  This is only a rough
   cut and will likely change as we learn more about the somewhat
   unusual dynamic linking scheme HP uses.

   .stub:
	Contains code to implement cross-space calls.  The first time one
	of the stubs is used it will call into the dynamic linker, later
	calls will go straight to the target.

	The only stub we support right now looks like

	ldd OFFSET(%dp),%r1
	bve %r0(%r1)
	ldd OFFSET+8(%dp),%dp

	Other stubs may be needed in the future.  We may want the remove
	the break/nop instruction.  It is only used right now to keep the
	offset of a .plt entry and a .stub entry in sync.

   .dlt:
	This is what most people call the .got.  HP used a different name.
	Losers.

   .rela.dlt:
	Relocations for the DLT.

   .plt:
	Function pointers as address,gp pairs.

   .rela.plt:
	Should contain dynamic IPLT (and EPLT?) relocations.

   .opd:
	FPTRS

   .rela.opd:
	EPLT relocations for symbols exported from shared libraries.  */

static bfd_boolean
elf64_hppa_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  asection *s;

  if (! get_stub (abfd, info, elf64_hppa_hash_table (info)))
    return FALSE;

  if (! get_dlt (abfd, info, elf64_hppa_hash_table (info)))
    return FALSE;

  if (! get_plt (abfd, info, elf64_hppa_hash_table (info)))
    return FALSE;

  if (! get_opd (abfd, info, elf64_hppa_hash_table (info)))
    return FALSE;

  s = bfd_make_section_with_flags (abfd, ".rela.dlt",
				   (SEC_ALLOC | SEC_LOAD
				    | SEC_HAS_CONTENTS
				    | SEC_IN_MEMORY
				    | SEC_READONLY
				    | SEC_LINKER_CREATED));
  if (s == NULL
      || !bfd_set_section_alignment (abfd, s, 3))
    return FALSE;
  elf64_hppa_hash_table (info)->dlt_rel_sec = s;

  s = bfd_make_section_with_flags (abfd, ".rela.plt",
				   (SEC_ALLOC | SEC_LOAD
				    | SEC_HAS_CONTENTS
				    | SEC_IN_MEMORY
				    | SEC_READONLY
				    | SEC_LINKER_CREATED));
  if (s == NULL
      || !bfd_set_section_alignment (abfd, s, 3))
    return FALSE;
  elf64_hppa_hash_table (info)->plt_rel_sec = s;

  s = bfd_make_section_with_flags (abfd, ".rela.data",
				   (SEC_ALLOC | SEC_LOAD
				    | SEC_HAS_CONTENTS
				    | SEC_IN_MEMORY
				    | SEC_READONLY
				    | SEC_LINKER_CREATED));
  if (s == NULL
      || !bfd_set_section_alignment (abfd, s, 3))
    return FALSE;
  elf64_hppa_hash_table (info)->other_rel_sec = s;

  s = bfd_make_section_with_flags (abfd, ".rela.opd",
				   (SEC_ALLOC | SEC_LOAD
				    | SEC_HAS_CONTENTS
				    | SEC_IN_MEMORY
				    | SEC_READONLY
				    | SEC_LINKER_CREATED));
  if (s == NULL
      || !bfd_set_section_alignment (abfd, s, 3))
    return FALSE;
  elf64_hppa_hash_table (info)->opd_rel_sec = s;

  return TRUE;
}

/* Allocate dynamic relocations for those symbols that turned out
   to be dynamic.  */

static bfd_boolean
allocate_dynrel_entries (dyn_h, data)
     struct elf64_hppa_dyn_hash_entry *dyn_h;
     PTR data;
{
  struct elf64_hppa_allocate_data *x = (struct elf64_hppa_allocate_data *)data;
  struct elf64_hppa_link_hash_table *hppa_info;
  struct elf64_hppa_dyn_reloc_entry *rent;
  bfd_boolean dynamic_symbol, shared;

  hppa_info = elf64_hppa_hash_table (x->info);
  dynamic_symbol = elf64_hppa_dynamic_symbol_p (dyn_h->h, x->info);
  shared = x->info->shared;

  /* We may need to allocate relocations for a non-dynamic symbol
     when creating a shared library.  */
  if (!dynamic_symbol && !shared)
    return TRUE;

  /* Take care of the normal data relocations.  */

  for (rent = dyn_h->reloc_entries; rent; rent = rent->next)
    {
      /* Allocate one iff we are building a shared library, the relocation
	 isn't a R_PARISC_FPTR64, or we don't want an opd entry.  */
      if (!shared && rent->type == R_PARISC_FPTR64 && dyn_h->want_opd)
	continue;

      hppa_info->other_rel_sec->size += sizeof (Elf64_External_Rela);

      /* Make sure this symbol gets into the dynamic symbol table if it is
	 not already recorded.  ?!? This should not be in the loop since
	 the symbol need only be added once.  */
      if (dyn_h->h == 0
	  || (dyn_h->h->dynindx == -1 && dyn_h->h->type != STT_PARISC_MILLI))
	if (!bfd_elf_link_record_local_dynamic_symbol
	    (x->info, rent->sec->owner, dyn_h->sym_indx))
	  return FALSE;
    }

  /* Take care of the GOT and PLT relocations.  */

  if ((dynamic_symbol || shared) && dyn_h->want_dlt)
    hppa_info->dlt_rel_sec->size += sizeof (Elf64_External_Rela);

  /* If we are building a shared library, then every symbol that has an
     opd entry will need an EPLT relocation to relocate the symbol's address
     and __gp value based on the runtime load address.  */
  if (shared && dyn_h->want_opd)
    hppa_info->opd_rel_sec->size += sizeof (Elf64_External_Rela);

  if (dyn_h->want_plt && dynamic_symbol)
    {
      bfd_size_type t = 0;

      /* Dynamic symbols get one IPLT relocation.  Local symbols in
	 shared libraries get two REL relocations.  Local symbols in
	 main applications get nothing.  */
      if (dynamic_symbol)
	t = sizeof (Elf64_External_Rela);
      else if (shared)
	t = 2 * sizeof (Elf64_External_Rela);

      hppa_info->plt_rel_sec->size += t;
    }

  return TRUE;
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  */

static bfd_boolean
elf64_hppa_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     struct elf_link_hash_entry *h;
{
  /* ??? Undefined symbols with PLT entries should be re-defined
     to be the PLT entry.  */

  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->u.weakdef != NULL)
    {
      BFD_ASSERT (h->u.weakdef->root.type == bfd_link_hash_defined
		  || h->u.weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->u.weakdef->root.u.def.section;
      h->root.u.def.value = h->u.weakdef->root.u.def.value;
      return TRUE;
    }

  /* If this is a reference to a symbol defined by a dynamic object which
     is not a function, we might allocate the symbol in our .dynbss section
     and allocate a COPY dynamic relocation.

     But PA64 code is canonically PIC, so as a rule we can avoid this sort
     of hackery.  */

  return TRUE;
}

/* This function is called via elf_link_hash_traverse to mark millicode
   symbols with a dynindx of -1 and to remove the string table reference
   from the dynamic symbol table.  If the symbol is not a millicode symbol,
   elf64_hppa_mark_exported_functions is called.  */

static bfd_boolean
elf64_hppa_mark_milli_and_exported_functions (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  struct bfd_link_info *info = (struct bfd_link_info *)data;
  struct elf_link_hash_entry *elf = h;

  if (elf->root.type == bfd_link_hash_warning)
    elf = (struct elf_link_hash_entry *) elf->root.u.i.link;

  if (elf->type == STT_PARISC_MILLI)
    {
      if (elf->dynindx != -1)
	{
	  elf->dynindx = -1;
	  _bfd_elf_strtab_delref (elf_hash_table (info)->dynstr,
				  elf->dynstr_index);
	}
      return TRUE;
    }

  return elf64_hppa_mark_exported_functions (h, data);
}

/* Set the final sizes of the dynamic sections and allocate memory for
   the contents of our special sections.  */

static bfd_boolean
elf64_hppa_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *s;
  bfd_boolean plt;
  bfd_boolean relocs;
  bfd_boolean reltext;
  struct elf64_hppa_allocate_data data;
  struct elf64_hppa_link_hash_table *hppa_info;

  hppa_info = elf64_hppa_hash_table (info);

  dynobj = elf_hash_table (info)->dynobj;
  BFD_ASSERT (dynobj != NULL);

  /* Mark each function this program exports so that we will allocate
     space in the .opd section for each function's FPTR.  If we are
     creating dynamic sections, change the dynamic index of millicode
     symbols to -1 and remove them from the string table for .dynstr.

     We have to traverse the main linker hash table since we have to
     find functions which may not have been mentioned in any relocs.  */
  elf_link_hash_traverse (elf_hash_table (info),
			  (elf_hash_table (info)->dynamic_sections_created
			   ? elf64_hppa_mark_milli_and_exported_functions
			   : elf64_hppa_mark_exported_functions),
			  info);

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (info->executable)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  BFD_ASSERT (s != NULL);
	  s->size = sizeof ELF_DYNAMIC_INTERPRETER;
	  s->contents = (unsigned char *) ELF_DYNAMIC_INTERPRETER;
	}
    }
  else
    {
      /* We may have created entries in the .rela.got section.
	 However, if we are not creating the dynamic sections, we will
	 not actually use these entries.  Reset the size of .rela.dlt,
	 which will cause it to get stripped from the output file
	 below.  */
      s = bfd_get_section_by_name (dynobj, ".rela.dlt");
      if (s != NULL)
	s->size = 0;
    }

  /* Allocate the GOT entries.  */

  data.info = info;
  if (elf64_hppa_hash_table (info)->dlt_sec)
    {
      data.ofs = 0x0;
      elf64_hppa_dyn_hash_traverse (&hppa_info->dyn_hash_table,
				    allocate_global_data_dlt, &data);
      hppa_info->dlt_sec->size = data.ofs;

      data.ofs = 0x0;
      elf64_hppa_dyn_hash_traverse (&hppa_info->dyn_hash_table,
				    allocate_global_data_plt, &data);
      hppa_info->plt_sec->size = data.ofs;

      data.ofs = 0x0;
      elf64_hppa_dyn_hash_traverse (&hppa_info->dyn_hash_table,
				    allocate_global_data_stub, &data);
      hppa_info->stub_sec->size = data.ofs;
    }

  /* Allocate space for entries in the .opd section.  */
  if (elf64_hppa_hash_table (info)->opd_sec)
    {
      data.ofs = 0;
      elf64_hppa_dyn_hash_traverse (&hppa_info->dyn_hash_table,
				    allocate_global_data_opd, &data);
      hppa_info->opd_sec->size = data.ofs;
    }

  /* Now allocate space for dynamic relocations, if necessary.  */
  if (hppa_info->root.dynamic_sections_created)
    elf64_hppa_dyn_hash_traverse (&hppa_info->dyn_hash_table,
				  allocate_dynrel_entries, &data);

  /* The sizes of all the sections are set.  Allocate memory for them.  */
  plt = FALSE;
  relocs = FALSE;
  reltext = FALSE;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;

      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name (dynobj, s);

      if (strcmp (name, ".plt") == 0)
	{
	  /* Remember whether there is a PLT.  */
	  plt = s->size != 0;
	}
      else if (strcmp (name, ".opd") == 0
	       || strncmp (name, ".dlt", 4) == 0
	       || strcmp (name, ".stub") == 0
	       || strcmp (name, ".got") == 0)
	{
	  /* Strip this section if we don't need it; see the comment below.  */
	}
      else if (strncmp (name, ".rela", 5) == 0)
	{
	  if (s->size != 0)
	    {
	      asection *target;

	      /* Remember whether there are any reloc sections other
		 than .rela.plt.  */
	      if (strcmp (name, ".rela.plt") != 0)
		{
		  const char *outname;

		  relocs = TRUE;

		  /* If this relocation section applies to a read only
		     section, then we probably need a DT_TEXTREL
		     entry.  The entries in the .rela.plt section
		     really apply to the .got section, which we
		     created ourselves and so know is not readonly.  */
		  outname = bfd_get_section_name (output_bfd,
						  s->output_section);
		  target = bfd_get_section_by_name (output_bfd, outname + 4);
		  if (target != NULL
		      && (target->flags & SEC_READONLY) != 0
		      && (target->flags & SEC_ALLOC) != 0)
		    reltext = TRUE;
		}

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (s->size == 0)
	{
	  /* If we don't need this section, strip it from the
	     output file.  This is mostly to handle .rela.bss and
	     .rela.plt.  We must create both sections in
	     create_dynamic_sections, because they must be created
	     before the linker maps input sections to output
	     sections.  The linker does that before
	     adjust_dynamic_symbol is called, and it is that
	     function which decides whether anything needs to go
	     into these sections.  */
	  s->flags |= SEC_EXCLUDE;
	  continue;
	}

      if ((s->flags & SEC_HAS_CONTENTS) == 0)
	continue;

      /* Allocate memory for the section contents if it has not
	 been allocated already.  We use bfd_zalloc here in case
	 unused entries are not reclaimed before the section's
	 contents are written out.  This should not happen, but this
	 way if it does, we get a R_PARISC_NONE reloc instead of
	 garbage.  */
      if (s->contents == NULL)
	{
	  s->contents = (bfd_byte *) bfd_zalloc (dynobj, s->size);
	  if (s->contents == NULL)
	    return FALSE;
	}
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Always create a DT_PLTGOT.  It actually has nothing to do with
	 the PLT, it is how we communicate the __gp value of a load
	 module to the dynamic linker.  */
#define add_dynamic_entry(TAG, VAL) \
  _bfd_elf_add_dynamic_entry (info, TAG, VAL)

      if (!add_dynamic_entry (DT_HP_DLD_FLAGS, 0)
	  || !add_dynamic_entry (DT_PLTGOT, 0))
	return FALSE;

      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in elf64_hppa_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
      if (! info->shared)
	{
	  if (!add_dynamic_entry (DT_DEBUG, 0)
	      || !add_dynamic_entry (DT_HP_DLD_HOOK, 0)
	      || !add_dynamic_entry (DT_HP_LOAD_MAP, 0))
	    return FALSE;
	}

      /* Force DT_FLAGS to always be set.
	 Required by HPUX 11.00 patch PHSS_26559.  */
      if (!add_dynamic_entry (DT_FLAGS, (info)->flags))
	return FALSE;

      if (plt)
	{
	  if (!add_dynamic_entry (DT_PLTRELSZ, 0)
	      || !add_dynamic_entry (DT_PLTREL, DT_RELA)
	      || !add_dynamic_entry (DT_JMPREL, 0))
	    return FALSE;
	}

      if (relocs)
	{
	  if (!add_dynamic_entry (DT_RELA, 0)
	      || !add_dynamic_entry (DT_RELASZ, 0)
	      || !add_dynamic_entry (DT_RELAENT, sizeof (Elf64_External_Rela)))
	    return FALSE;
	}

      if (reltext)
	{
	  if (!add_dynamic_entry (DT_TEXTREL, 0))
	    return FALSE;
	  info->flags |= DF_TEXTREL;
	}
    }
#undef add_dynamic_entry

  return TRUE;
}

/* Called after we have output the symbol into the dynamic symbol
   table, but before we output the symbol into the normal symbol
   table.

   For some symbols we had to change their address when outputting
   the dynamic symbol table.  We undo that change here so that
   the symbols have their expected value in the normal symbol
   table.  Ick.  */

static bfd_boolean
elf64_hppa_link_output_symbol_hook (info, name, sym, input_sec, h)
     struct bfd_link_info *info;
     const char *name;
     Elf_Internal_Sym *sym;
     asection *input_sec ATTRIBUTE_UNUSED;
     struct elf_link_hash_entry *h;
{
  struct elf64_hppa_link_hash_table *hppa_info;
  struct elf64_hppa_dyn_hash_entry *dyn_h;

  /* We may be called with the file symbol or section symbols.
     They never need munging, so it is safe to ignore them.  */
  if (!name)
    return TRUE;

  /* Get the PA dyn_symbol (if any) associated with NAME.  */
  hppa_info = elf64_hppa_hash_table (info);
  dyn_h = elf64_hppa_dyn_hash_lookup (&hppa_info->dyn_hash_table,
				      name, FALSE, FALSE);
  if (!dyn_h || dyn_h->h != h)
    return TRUE;

  /* Function symbols for which we created .opd entries *may* have been
     munged by finish_dynamic_symbol and have to be un-munged here.

     Note that finish_dynamic_symbol sometimes turns dynamic symbols
     into non-dynamic ones, so we initialize st_shndx to -1 in
     mark_exported_functions and check to see if it was overwritten
     here instead of just checking dyn_h->h->dynindx.  */
  if (dyn_h->want_opd && dyn_h->st_shndx != -1)
    {
      /* Restore the saved value and section index.  */
      sym->st_value = dyn_h->st_value;
      sym->st_shndx = dyn_h->st_shndx;
    }

  return TRUE;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static bfd_boolean
elf64_hppa_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  asection *stub, *splt, *sdlt, *sopd, *spltrel, *sdltrel;
  struct elf64_hppa_link_hash_table *hppa_info;
  struct elf64_hppa_dyn_hash_entry *dyn_h;

  hppa_info = elf64_hppa_hash_table (info);
  dyn_h = elf64_hppa_dyn_hash_lookup (&hppa_info->dyn_hash_table,
				      h->root.root.string, FALSE, FALSE);

  stub = hppa_info->stub_sec;
  splt = hppa_info->plt_sec;
  sdlt = hppa_info->dlt_sec;
  sopd = hppa_info->opd_sec;
  spltrel = hppa_info->plt_rel_sec;
  sdltrel = hppa_info->dlt_rel_sec;

  /* Incredible.  It is actually necessary to NOT use the symbol's real
     value when building the dynamic symbol table for a shared library.
     At least for symbols that refer to functions.

     We will store a new value and section index into the symbol long
     enough to output it into the dynamic symbol table, then we restore
     the original values (in elf64_hppa_link_output_symbol_hook).  */
  if (dyn_h && dyn_h->want_opd)
    {
      BFD_ASSERT (sopd != NULL);

      /* Save away the original value and section index so that we
	 can restore them later.  */
      dyn_h->st_value = sym->st_value;
      dyn_h->st_shndx = sym->st_shndx;

      /* For the dynamic symbol table entry, we want the value to be
	 address of this symbol's entry within the .opd section.  */
      sym->st_value = (dyn_h->opd_offset
		       + sopd->output_offset
		       + sopd->output_section->vma);
      sym->st_shndx = _bfd_elf_section_from_bfd_section (output_bfd,
							 sopd->output_section);
    }

  /* Initialize a .plt entry if requested.  */
  if (dyn_h && dyn_h->want_plt
      && elf64_hppa_dynamic_symbol_p (dyn_h->h, info))
    {
      bfd_vma value;
      Elf_Internal_Rela rel;
      bfd_byte *loc;

      BFD_ASSERT (splt != NULL && spltrel != NULL);

      /* We do not actually care about the value in the PLT entry
	 if we are creating a shared library and the symbol is
	 still undefined, we create a dynamic relocation to fill
	 in the correct value.  */
      if (info->shared && h->root.type == bfd_link_hash_undefined)
	value = 0;
      else
	value = (h->root.u.def.value + h->root.u.def.section->vma);

      /* Fill in the entry in the procedure linkage table.

	 The format of a plt entry is
	 <funcaddr> <__gp>.

	 plt_offset is the offset within the PLT section at which to
	 install the PLT entry.

	 We are modifying the in-memory PLT contents here, so we do not add
	 in the output_offset of the PLT section.  */

      bfd_put_64 (splt->owner, value, splt->contents + dyn_h->plt_offset);
      value = _bfd_get_gp_value (splt->output_section->owner);
      bfd_put_64 (splt->owner, value, splt->contents + dyn_h->plt_offset + 0x8);

      /* Create a dynamic IPLT relocation for this entry.

	 We are creating a relocation in the output file's PLT section,
	 which is included within the DLT secton.  So we do need to include
	 the PLT's output_offset in the computation of the relocation's
	 address.  */
      rel.r_offset = (dyn_h->plt_offset + splt->output_offset
		      + splt->output_section->vma);
      rel.r_info = ELF64_R_INFO (h->dynindx, R_PARISC_IPLT);
      rel.r_addend = 0;

      loc = spltrel->contents;
      loc += spltrel->reloc_count++ * sizeof (Elf64_External_Rela);
      bfd_elf64_swap_reloca_out (splt->output_section->owner, &rel, loc);
    }

  /* Initialize an external call stub entry if requested.  */
  if (dyn_h && dyn_h->want_stub
      && elf64_hppa_dynamic_symbol_p (dyn_h->h, info))
    {
      bfd_vma value;
      int insn;
      unsigned int max_offset;

      BFD_ASSERT (stub != NULL);

      /* Install the generic stub template.

	 We are modifying the contents of the stub section, so we do not
	 need to include the stub section's output_offset here.  */
      memcpy (stub->contents + dyn_h->stub_offset, plt_stub, sizeof (plt_stub));

      /* Fix up the first ldd instruction.

	 We are modifying the contents of the STUB section in memory,
	 so we do not need to include its output offset in this computation.

	 Note the plt_offset value is the value of the PLT entry relative to
	 the start of the PLT section.  These instructions will reference
	 data relative to the value of __gp, which may not necessarily have
	 the same address as the start of the PLT section.

	 gp_offset contains the offset of __gp within the PLT section.  */
      value = dyn_h->plt_offset - hppa_info->gp_offset;

      insn = bfd_get_32 (stub->owner, stub->contents + dyn_h->stub_offset);
      if (output_bfd->arch_info->mach >= 25)
	{
	  /* Wide mode allows 16 bit offsets.  */
	  max_offset = 32768;
	  insn &= ~ 0xfff1;
	  insn |= re_assemble_16 ((int) value);
	}
      else
	{
	  max_offset = 8192;
	  insn &= ~ 0x3ff1;
	  insn |= re_assemble_14 ((int) value);
	}

      if ((value & 7) || value + max_offset >= 2*max_offset - 8)
	{
	  (*_bfd_error_handler) (_("stub entry for %s cannot load .plt, dp offset = %ld"),
				 dyn_h->root.string,
				 (long) value);
	  return FALSE;
	}

      bfd_put_32 (stub->owner, (bfd_vma) insn,
		  stub->contents + dyn_h->stub_offset);

      /* Fix up the second ldd instruction.  */
      value += 8;
      insn = bfd_get_32 (stub->owner, stub->contents + dyn_h->stub_offset + 8);
      if (output_bfd->arch_info->mach >= 25)
	{
	  insn &= ~ 0xfff1;
	  insn |= re_assemble_16 ((int) value);
	}
      else
	{
	  insn &= ~ 0x3ff1;
	  insn |= re_assemble_14 ((int) value);
	}
      bfd_put_32 (stub->owner, (bfd_vma) insn,
		  stub->contents + dyn_h->stub_offset + 8);
    }

  return TRUE;
}

/* The .opd section contains FPTRs for each function this file
   exports.  Initialize the FPTR entries.  */

static bfd_boolean
elf64_hppa_finalize_opd (dyn_h, data)
     struct elf64_hppa_dyn_hash_entry *dyn_h;
     PTR data;
{
  struct bfd_link_info *info = (struct bfd_link_info *)data;
  struct elf64_hppa_link_hash_table *hppa_info;
  struct elf_link_hash_entry *h = dyn_h ? dyn_h->h : NULL;
  asection *sopd;
  asection *sopdrel;

  hppa_info = elf64_hppa_hash_table (info);
  sopd = hppa_info->opd_sec;
  sopdrel = hppa_info->opd_rel_sec;

  if (h && dyn_h->want_opd)
    {
      bfd_vma value;

      /* The first two words of an .opd entry are zero.

	 We are modifying the contents of the OPD section in memory, so we
	 do not need to include its output offset in this computation.  */
      memset (sopd->contents + dyn_h->opd_offset, 0, 16);

      value = (h->root.u.def.value
	       + h->root.u.def.section->output_section->vma
	       + h->root.u.def.section->output_offset);

      /* The next word is the address of the function.  */
      bfd_put_64 (sopd->owner, value, sopd->contents + dyn_h->opd_offset + 16);

      /* The last word is our local __gp value.  */
      value = _bfd_get_gp_value (sopd->output_section->owner);
      bfd_put_64 (sopd->owner, value, sopd->contents + dyn_h->opd_offset + 24);
    }

  /* If we are generating a shared library, we must generate EPLT relocations
     for each entry in the .opd, even for static functions (they may have
     had their address taken).  */
  if (info->shared && dyn_h && dyn_h->want_opd)
    {
      Elf_Internal_Rela rel;
      bfd_byte *loc;
      int dynindx;

      /* We may need to do a relocation against a local symbol, in
	 which case we have to look up it's dynamic symbol index off
	 the local symbol hash table.  */
      if (h && h->dynindx != -1)
	dynindx = h->dynindx;
      else
	dynindx
	  = _bfd_elf_link_lookup_local_dynindx (info, dyn_h->owner,
						dyn_h->sym_indx);

      /* The offset of this relocation is the absolute address of the
	 .opd entry for this symbol.  */
      rel.r_offset = (dyn_h->opd_offset + sopd->output_offset
		      + sopd->output_section->vma);

      /* If H is non-null, then we have an external symbol.

	 It is imperative that we use a different dynamic symbol for the
	 EPLT relocation if the symbol has global scope.

	 In the dynamic symbol table, the function symbol will have a value
	 which is address of the function's .opd entry.

	 Thus, we can not use that dynamic symbol for the EPLT relocation
	 (if we did, the data in the .opd would reference itself rather
	 than the actual address of the function).  Instead we have to use
	 a new dynamic symbol which has the same value as the original global
	 function symbol.

	 We prefix the original symbol with a "." and use the new symbol in
	 the EPLT relocation.  This new symbol has already been recorded in
	 the symbol table, we just have to look it up and use it.

	 We do not have such problems with static functions because we do
	 not make their addresses in the dynamic symbol table point to
	 the .opd entry.  Ultimately this should be safe since a static
	 function can not be directly referenced outside of its shared
	 library.

	 We do have to play similar games for FPTR relocations in shared
	 libraries, including those for static symbols.  See the FPTR
	 handling in elf64_hppa_finalize_dynreloc.  */
      if (h)
	{
	  char *new_name;
	  struct elf_link_hash_entry *nh;

	  new_name = alloca (strlen (h->root.root.string) + 2);
	  new_name[0] = '.';
	  strcpy (new_name + 1, h->root.root.string);

	  nh = elf_link_hash_lookup (elf_hash_table (info),
				     new_name, FALSE, FALSE, FALSE);

	  /* All we really want from the new symbol is its dynamic
	     symbol index.  */
	  dynindx = nh->dynindx;
	}

      rel.r_addend = 0;
      rel.r_info = ELF64_R_INFO (dynindx, R_PARISC_EPLT);

      loc = sopdrel->contents;
      loc += sopdrel->reloc_count++ * sizeof (Elf64_External_Rela);
      bfd_elf64_swap_reloca_out (sopd->output_section->owner, &rel, loc);
    }
  return TRUE;
}

/* The .dlt section contains addresses for items referenced through the
   dlt.  Note that we can have a DLTIND relocation for a local symbol, thus
   we can not depend on finish_dynamic_symbol to initialize the .dlt.  */

static bfd_boolean
elf64_hppa_finalize_dlt (dyn_h, data)
     struct elf64_hppa_dyn_hash_entry *dyn_h;
     PTR data;
{
  struct bfd_link_info *info = (struct bfd_link_info *)data;
  struct elf64_hppa_link_hash_table *hppa_info;
  asection *sdlt, *sdltrel;
  struct elf_link_hash_entry *h = dyn_h ? dyn_h->h : NULL;

  hppa_info = elf64_hppa_hash_table (info);

  sdlt = hppa_info->dlt_sec;
  sdltrel = hppa_info->dlt_rel_sec;

  /* H/DYN_H may refer to a local variable and we know it's
     address, so there is no need to create a relocation.  Just install
     the proper value into the DLT, note this shortcut can not be
     skipped when building a shared library.  */
  if (! info->shared && h && dyn_h->want_dlt)
    {
      bfd_vma value;

      /* If we had an LTOFF_FPTR style relocation we want the DLT entry
	 to point to the FPTR entry in the .opd section.

	 We include the OPD's output offset in this computation as
	 we are referring to an absolute address in the resulting
	 object file.  */
      if (dyn_h->want_opd)
	{
	  value = (dyn_h->opd_offset
		   + hppa_info->opd_sec->output_offset
		   + hppa_info->opd_sec->output_section->vma);
	}
      else if ((h->root.type == bfd_link_hash_defined
		|| h->root.type == bfd_link_hash_defweak)
	       && h->root.u.def.section)
	{
	  value = h->root.u.def.value + h->root.u.def.section->output_offset;
	  if (h->root.u.def.section->output_section)
	    value += h->root.u.def.section->output_section->vma;
	  else
	    value += h->root.u.def.section->vma;
	}
      else
	/* We have an undefined function reference.  */
	value = 0;

      /* We do not need to include the output offset of the DLT section
	 here because we are modifying the in-memory contents.  */
      bfd_put_64 (sdlt->owner, value, sdlt->contents + dyn_h->dlt_offset);
    }

  /* Create a relocation for the DLT entry associated with this symbol.
     When building a shared library the symbol does not have to be dynamic.  */
  if (dyn_h->want_dlt
      && (elf64_hppa_dynamic_symbol_p (dyn_h->h, info) || info->shared))
    {
      Elf_Internal_Rela rel;
      bfd_byte *loc;
      int dynindx;

      /* We may need to do a relocation against a local symbol, in
	 which case we have to look up it's dynamic symbol index off
	 the local symbol hash table.  */
      if (h && h->dynindx != -1)
	dynindx = h->dynindx;
      else
	dynindx
	  = _bfd_elf_link_lookup_local_dynindx (info, dyn_h->owner,
						dyn_h->sym_indx);

      /* Create a dynamic relocation for this entry.  Do include the output
	 offset of the DLT entry since we need an absolute address in the
	 resulting object file.  */
      rel.r_offset = (dyn_h->dlt_offset + sdlt->output_offset
		      + sdlt->output_section->vma);
      if (h && h->type == STT_FUNC)
	  rel.r_info = ELF64_R_INFO (dynindx, R_PARISC_FPTR64);
      else
	  rel.r_info = ELF64_R_INFO (dynindx, R_PARISC_DIR64);
      rel.r_addend = 0;

      loc = sdltrel->contents;
      loc += sdltrel->reloc_count++ * sizeof (Elf64_External_Rela);
      bfd_elf64_swap_reloca_out (sdlt->output_section->owner, &rel, loc);
    }
  return TRUE;
}

/* Finalize the dynamic relocations.  Specifically the FPTR relocations
   for dynamic functions used to initialize static data.  */

static bfd_boolean
elf64_hppa_finalize_dynreloc (dyn_h, data)
     struct elf64_hppa_dyn_hash_entry *dyn_h;
     PTR data;
{
  struct bfd_link_info *info = (struct bfd_link_info *)data;
  struct elf64_hppa_link_hash_table *hppa_info;
  struct elf_link_hash_entry *h;
  int dynamic_symbol;

  dynamic_symbol = elf64_hppa_dynamic_symbol_p (dyn_h->h, info);

  if (!dynamic_symbol && !info->shared)
    return TRUE;

  if (dyn_h->reloc_entries)
    {
      struct elf64_hppa_dyn_reloc_entry *rent;
      int dynindx;

      hppa_info = elf64_hppa_hash_table (info);
      h = dyn_h->h;

      /* We may need to do a relocation against a local symbol, in
	 which case we have to look up it's dynamic symbol index off
	 the local symbol hash table.  */
      if (h && h->dynindx != -1)
	dynindx = h->dynindx;
      else
	dynindx
	  = _bfd_elf_link_lookup_local_dynindx (info, dyn_h->owner,
						dyn_h->sym_indx);

      for (rent = dyn_h->reloc_entries; rent; rent = rent->next)
	{
	  Elf_Internal_Rela rel;
	  bfd_byte *loc;

	  /* Allocate one iff we are building a shared library, the relocation
	     isn't a R_PARISC_FPTR64, or we don't want an opd entry.  */
	  if (!info->shared && rent->type == R_PARISC_FPTR64 && dyn_h->want_opd)
	    continue;

	  /* Create a dynamic relocation for this entry.

	     We need the output offset for the reloc's section because
	     we are creating an absolute address in the resulting object
	     file.  */
	  rel.r_offset = (rent->offset + rent->sec->output_offset
			  + rent->sec->output_section->vma);

	  /* An FPTR64 relocation implies that we took the address of
	     a function and that the function has an entry in the .opd
	     section.  We want the FPTR64 relocation to reference the
	     entry in .opd.

	     We could munge the symbol value in the dynamic symbol table
	     (in fact we already do for functions with global scope) to point
	     to the .opd entry.  Then we could use that dynamic symbol in
	     this relocation.

	     Or we could do something sensible, not munge the symbol's
	     address and instead just use a different symbol to reference
	     the .opd entry.  At least that seems sensible until you
	     realize there's no local dynamic symbols we can use for that
	     purpose.  Thus the hair in the check_relocs routine.

	     We use a section symbol recorded by check_relocs as the
	     base symbol for the relocation.  The addend is the difference
	     between the section symbol and the address of the .opd entry.  */
	  if (info->shared && rent->type == R_PARISC_FPTR64 && dyn_h->want_opd)
	    {
	      bfd_vma value, value2;

	      /* First compute the address of the opd entry for this symbol.  */
	      value = (dyn_h->opd_offset
		       + hppa_info->opd_sec->output_section->vma
		       + hppa_info->opd_sec->output_offset);

	      /* Compute the value of the start of the section with
		 the relocation.  */
	      value2 = (rent->sec->output_section->vma
			+ rent->sec->output_offset);

	      /* Compute the difference between the start of the section
		 with the relocation and the opd entry.  */
	      value -= value2;

	      /* The result becomes the addend of the relocation.  */
	      rel.r_addend = value;

	      /* The section symbol becomes the symbol for the dynamic
		 relocation.  */
	      dynindx
		= _bfd_elf_link_lookup_local_dynindx (info,
						      rent->sec->owner,
						      rent->sec_symndx);
	    }
	  else
	    rel.r_addend = rent->addend;

	  rel.r_info = ELF64_R_INFO (dynindx, rent->type);

	  loc = hppa_info->other_rel_sec->contents;
	  loc += (hppa_info->other_rel_sec->reloc_count++
		  * sizeof (Elf64_External_Rela));
	  bfd_elf64_swap_reloca_out (hppa_info->other_rel_sec->output_section->owner,
				     &rel, loc);
	}
    }

  return TRUE;
}

/* Used to decide how to sort relocs in an optimal manner for the
   dynamic linker, before writing them out.  */

static enum elf_reloc_type_class
elf64_hppa_reloc_type_class (rela)
     const Elf_Internal_Rela *rela;
{
  if (ELF64_R_SYM (rela->r_info) == 0)
    return reloc_class_relative;

  switch ((int) ELF64_R_TYPE (rela->r_info))
    {
    case R_PARISC_IPLT:
      return reloc_class_plt;
    case R_PARISC_COPY:
      return reloc_class_copy;
    default:
      return reloc_class_normal;
    }
}

/* Finish up the dynamic sections.  */

static bfd_boolean
elf64_hppa_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *sdyn;
  struct elf64_hppa_link_hash_table *hppa_info;

  hppa_info = elf64_hppa_hash_table (info);

  /* Finalize the contents of the .opd section.  */
  elf64_hppa_dyn_hash_traverse (&hppa_info->dyn_hash_table,
				elf64_hppa_finalize_opd,
				info);

  elf64_hppa_dyn_hash_traverse (&hppa_info->dyn_hash_table,
				elf64_hppa_finalize_dynreloc,
				info);

  /* Finalize the contents of the .dlt section.  */
  dynobj = elf_hash_table (info)->dynobj;
  /* Finalize the contents of the .dlt section.  */
  elf64_hppa_dyn_hash_traverse (&hppa_info->dyn_hash_table,
				elf64_hppa_finalize_dlt,
				info);

  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      Elf64_External_Dyn *dyncon, *dynconend;

      BFD_ASSERT (sdyn != NULL);

      dyncon = (Elf64_External_Dyn *) sdyn->contents;
      dynconend = (Elf64_External_Dyn *) (sdyn->contents + sdyn->size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  asection *s;

	  bfd_elf64_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    default:
	      break;

	    case DT_HP_LOAD_MAP:
	      /* Compute the absolute address of 16byte scratchpad area
		 for the dynamic linker.

		 By convention the linker script will allocate the scratchpad
		 area at the start of the .data section.  So all we have to
		 to is find the start of the .data section.  */
	      s = bfd_get_section_by_name (output_bfd, ".data");
	      dyn.d_un.d_ptr = s->vma;
	      bfd_elf64_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_PLTGOT:
	      /* HP's use PLTGOT to set the GOT register.  */
	      dyn.d_un.d_ptr = _bfd_get_gp_value (output_bfd);
	      bfd_elf64_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_JMPREL:
	      s = hppa_info->plt_rel_sec;
	      dyn.d_un.d_ptr = s->output_section->vma + s->output_offset;
	      bfd_elf64_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_PLTRELSZ:
	      s = hppa_info->plt_rel_sec;
	      dyn.d_un.d_val = s->size;
	      bfd_elf64_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_RELA:
	      s = hppa_info->other_rel_sec;
	      if (! s || ! s->size)
		s = hppa_info->dlt_rel_sec;
	      if (! s || ! s->size)
		s = hppa_info->opd_rel_sec;
	      dyn.d_un.d_ptr = s->output_section->vma + s->output_offset;
	      bfd_elf64_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_RELASZ:
	      s = hppa_info->other_rel_sec;
	      dyn.d_un.d_val = s->size;
	      s = hppa_info->dlt_rel_sec;
	      dyn.d_un.d_val += s->size;
	      s = hppa_info->opd_rel_sec;
	      dyn.d_un.d_val += s->size;
	      /* There is some question about whether or not the size of
		 the PLT relocs should be included here.  HP's tools do
		 it, so we'll emulate them.  */
	      s = hppa_info->plt_rel_sec;
	      dyn.d_un.d_val += s->size;
	      bfd_elf64_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    }
	}
    }

  return TRUE;
}

/* Support for core dump NOTE sections.  */

static bfd_boolean
elf64_hppa_grok_prstatus (bfd *abfd, Elf_Internal_Note *note)
{
  int offset;
  size_t size;

  switch (note->descsz)
    {
      default:
	return FALSE;

      case 760:		/* Linux/hppa */
	/* pr_cursig */
	elf_tdata (abfd)->core_signal = bfd_get_16 (abfd, note->descdata + 12);

	/* pr_pid */
	elf_tdata (abfd)->core_pid = bfd_get_32 (abfd, note->descdata + 32);

	/* pr_reg */
	offset = 112;
	size = 640;

	break;
    }

  /* Make a ".reg/999" section.  */
  return _bfd_elfcore_make_pseudosection (abfd, ".reg",
					  size, note->descpos + offset);
}

static bfd_boolean
elf64_hppa_grok_psinfo (bfd *abfd, Elf_Internal_Note *note)
{
  char * command;
  int n;

  switch (note->descsz)
    {
    default:
      return FALSE;

    case 136:		/* Linux/hppa elf_prpsinfo.  */
      elf_tdata (abfd)->core_program
	= _bfd_elfcore_strndup (abfd, note->descdata + 40, 16);
      elf_tdata (abfd)->core_command
	= _bfd_elfcore_strndup (abfd, note->descdata + 56, 80);
    }

  /* Note that for some reason, a spurious space is tacked
     onto the end of the args in some (at least one anyway)
     implementations, so strip it off if it exists.  */
  command = elf_tdata (abfd)->core_command;
  n = strlen (command);

  if (0 < n && command[n - 1] == ' ')
    command[n - 1] = '\0';

  return TRUE;
}

/* Return the number of additional phdrs we will need.

   The generic ELF code only creates PT_PHDRs for executables.  The HP
   dynamic linker requires PT_PHDRs for dynamic libraries too.

   This routine indicates that the backend needs one additional program
   header for that case.

   Note we do not have access to the link info structure here, so we have
   to guess whether or not we are building a shared library based on the
   existence of a .interp section.  */

static int
elf64_hppa_additional_program_headers (abfd)
     bfd *abfd;
{
  asection *s;

  /* If we are creating a shared library, then we have to create a
     PT_PHDR segment.  HP's dynamic linker chokes without it.  */
  s = bfd_get_section_by_name (abfd, ".interp");
  if (! s)
    return 1;
  return 0;
}

/* Allocate and initialize any program headers required by this
   specific backend.

   The generic ELF code only creates PT_PHDRs for executables.  The HP
   dynamic linker requires PT_PHDRs for dynamic libraries too.

   This allocates the PT_PHDR and initializes it in a manner suitable
   for the HP linker.

   Note we do not have access to the link info structure here, so we have
   to guess whether or not we are building a shared library based on the
   existence of a .interp section.  */

static bfd_boolean
elf64_hppa_modify_segment_map (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
{
  struct elf_segment_map *m;
  asection *s;

  s = bfd_get_section_by_name (abfd, ".interp");
  if (! s)
    {
      for (m = elf_tdata (abfd)->segment_map; m != NULL; m = m->next)
	if (m->p_type == PT_PHDR)
	  break;
      if (m == NULL)
	{
	  m = ((struct elf_segment_map *)
	       bfd_zalloc (abfd, (bfd_size_type) sizeof *m));
	  if (m == NULL)
	    return FALSE;

	  m->p_type = PT_PHDR;
	  m->p_flags = PF_R | PF_X;
	  m->p_flags_valid = 1;
	  m->p_paddr_valid = 1;
	  m->includes_phdrs = 1;

	  m->next = elf_tdata (abfd)->segment_map;
	  elf_tdata (abfd)->segment_map = m;
	}
    }

  for (m = elf_tdata (abfd)->segment_map; m != NULL; m = m->next)
    if (m->p_type == PT_LOAD)
      {
	unsigned int i;

	for (i = 0; i < m->count; i++)
	  {
	    /* The code "hint" is not really a hint.  It is a requirement
	       for certain versions of the HP dynamic linker.  Worse yet,
	       it must be set even if the shared library does not have
	       any code in its "text" segment (thus the check for .hash
	       to catch this situation).  */
	    if (m->sections[i]->flags & SEC_CODE
		|| (strcmp (m->sections[i]->name, ".hash") == 0))
	      m->p_flags |= (PF_X | PF_HP_CODE);
	  }
      }

  return TRUE;
}

/* Called when writing out an object file to decide the type of a
   symbol.  */
static int
elf64_hppa_elf_get_symbol_type (elf_sym, type)
     Elf_Internal_Sym *elf_sym;
     int type;
{
  if (ELF_ST_TYPE (elf_sym->st_info) == STT_PARISC_MILLI)
    return STT_PARISC_MILLI;
  else
    return type;
}

/* Support HP specific sections for core files.  */
static bfd_boolean
elf64_hppa_section_from_phdr (bfd *abfd, Elf_Internal_Phdr *hdr, int index,
			      const char *typename)
{
  if (hdr->p_type == PT_HP_CORE_KERNEL)
    {
      asection *sect;

      if (!_bfd_elf_make_section_from_phdr (abfd, hdr, index, typename))
	return FALSE;

      sect = bfd_make_section_anyway (abfd, ".kernel");
      if (sect == NULL)
	return FALSE;
      sect->size = hdr->p_filesz;
      sect->filepos = hdr->p_offset;
      sect->flags = SEC_HAS_CONTENTS | SEC_READONLY;
      return TRUE;
    }

  if (hdr->p_type == PT_HP_CORE_PROC)
    {
      int sig;

      if (bfd_seek (abfd, hdr->p_offset, SEEK_SET) != 0)
	return FALSE;
      if (bfd_bread (&sig, 4, abfd) != 4)
	return FALSE;

      elf_tdata (abfd)->core_signal = sig;

      if (!_bfd_elf_make_section_from_phdr (abfd, hdr, index, typename))
	return FALSE;

      /* GDB uses the ".reg" section to read register contents.  */
      return _bfd_elfcore_make_pseudosection (abfd, ".reg", hdr->p_filesz,
					      hdr->p_offset);
    }

  if (hdr->p_type == PT_HP_CORE_LOADABLE
      || hdr->p_type == PT_HP_CORE_STACK
      || hdr->p_type == PT_HP_CORE_MMF)
    hdr->p_type = PT_LOAD;

  return _bfd_elf_make_section_from_phdr (abfd, hdr, index, typename);
}

static const struct bfd_elf_special_section elf64_hppa_special_sections[] =
{
  { ".fini",   5, 0, SHT_PROGBITS, SHF_ALLOC + SHF_WRITE },
  { ".init",   5, 0, SHT_PROGBITS, SHF_ALLOC + SHF_WRITE },
  { ".plt",    4, 0, SHT_PROGBITS, SHF_ALLOC + SHF_WRITE + SHF_PARISC_SHORT },
  { ".dlt",    4, 0, SHT_PROGBITS, SHF_ALLOC + SHF_WRITE + SHF_PARISC_SHORT },
  { ".sdata",  6, 0, SHT_PROGBITS, SHF_ALLOC + SHF_WRITE + SHF_PARISC_SHORT },
  { ".sbss",   5, 0, SHT_NOBITS, SHF_ALLOC + SHF_WRITE + SHF_PARISC_SHORT },
  { ".tbss",   5, 0, SHT_NOBITS, SHF_ALLOC + SHF_WRITE + SHF_HP_TLS },
  { NULL,      0, 0, 0,            0 }
};

/* The hash bucket size is the standard one, namely 4.  */

const struct elf_size_info hppa64_elf_size_info =
{
  sizeof (Elf64_External_Ehdr),
  sizeof (Elf64_External_Phdr),
  sizeof (Elf64_External_Shdr),
  sizeof (Elf64_External_Rel),
  sizeof (Elf64_External_Rela),
  sizeof (Elf64_External_Sym),
  sizeof (Elf64_External_Dyn),
  sizeof (Elf_External_Note),
  4,
  1,
  64, 3,
  ELFCLASS64, EV_CURRENT,
  bfd_elf64_write_out_phdrs,
  bfd_elf64_write_shdrs_and_ehdr,
  bfd_elf64_write_relocs,
  bfd_elf64_swap_symbol_in,
  bfd_elf64_swap_symbol_out,
  bfd_elf64_slurp_reloc_table,
  bfd_elf64_slurp_symbol_table,
  bfd_elf64_swap_dyn_in,
  bfd_elf64_swap_dyn_out,
  bfd_elf64_swap_reloc_in,
  bfd_elf64_swap_reloc_out,
  bfd_elf64_swap_reloca_in,
  bfd_elf64_swap_reloca_out
};

#define TARGET_BIG_SYM			bfd_elf64_hppa_vec
#define TARGET_BIG_NAME			"elf64-hppa"
#define ELF_ARCH			bfd_arch_hppa
#define ELF_MACHINE_CODE		EM_PARISC
/* This is not strictly correct.  The maximum page size for PA2.0 is
   64M.  But everything still uses 4k.  */
#define ELF_MAXPAGESIZE			0x1000
#define bfd_elf64_bfd_reloc_type_lookup elf_hppa_reloc_type_lookup
#define bfd_elf64_bfd_is_local_label_name       elf_hppa_is_local_label_name
#define elf_info_to_howto		elf_hppa_info_to_howto
#define elf_info_to_howto_rel		elf_hppa_info_to_howto_rel

#define elf_backend_section_from_shdr	elf64_hppa_section_from_shdr
#define elf_backend_object_p		elf64_hppa_object_p
#define elf_backend_final_write_processing \
					elf_hppa_final_write_processing
#define elf_backend_fake_sections	elf_hppa_fake_sections
#define elf_backend_add_symbol_hook	elf_hppa_add_symbol_hook

#define elf_backend_relocate_section	elf_hppa_relocate_section

#define bfd_elf64_bfd_final_link	elf_hppa_final_link

#define elf_backend_create_dynamic_sections \
					elf64_hppa_create_dynamic_sections
#define elf_backend_post_process_headers	elf64_hppa_post_process_headers

#define elf_backend_adjust_dynamic_symbol \
					elf64_hppa_adjust_dynamic_symbol

#define elf_backend_size_dynamic_sections \
					elf64_hppa_size_dynamic_sections

#define elf_backend_finish_dynamic_symbol \
					elf64_hppa_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
					elf64_hppa_finish_dynamic_sections
#define elf_backend_grok_prstatus	elf64_hppa_grok_prstatus
#define elf_backend_grok_psinfo		elf64_hppa_grok_psinfo
 
/* Stuff for the BFD linker: */
#define bfd_elf64_bfd_link_hash_table_create \
	elf64_hppa_hash_table_create

#define elf_backend_check_relocs \
	elf64_hppa_check_relocs

#define elf_backend_size_info \
  hppa64_elf_size_info

#define elf_backend_additional_program_headers \
	elf64_hppa_additional_program_headers

#define elf_backend_modify_segment_map \
	elf64_hppa_modify_segment_map

#define elf_backend_link_output_symbol_hook \
	elf64_hppa_link_output_symbol_hook

#define elf_backend_want_got_plt	0
#define elf_backend_plt_readonly	0
#define elf_backend_want_plt_sym	0
#define elf_backend_got_header_size     0
#define elf_backend_type_change_ok	TRUE
#define elf_backend_get_symbol_type	elf64_hppa_elf_get_symbol_type
#define elf_backend_reloc_type_class	elf64_hppa_reloc_type_class
#define elf_backend_rela_normal		1
#define elf_backend_special_sections	elf64_hppa_special_sections
#define elf_backend_action_discarded	elf_hppa_action_discarded
#define elf_backend_section_from_phdr   elf64_hppa_section_from_phdr

#include "elf64-target.h"

#undef TARGET_BIG_SYM
#define TARGET_BIG_SYM			bfd_elf64_hppa_linux_vec
#undef TARGET_BIG_NAME
#define TARGET_BIG_NAME			"elf64-hppa-linux"

#define INCLUDED_TARGET_FILE 1
#include "elf64-target.h"
