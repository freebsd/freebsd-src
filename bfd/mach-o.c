/* Mach-O support for BFD.
   Copyright 1999, 2000, 2001, 2002, 2003
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

#include "mach-o.h"
#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libiberty.h"
#include <ctype.h>

#ifndef BFD_IO_FUNCS
#define BFD_IO_FUNCS 0
#endif

#define bfd_mach_o_mkarchive _bfd_noarchive_mkarchive
#define bfd_mach_o_read_ar_hdr _bfd_noarchive_read_ar_hdr
#define bfd_mach_o_slurp_armap _bfd_noarchive_slurp_armap
#define bfd_mach_o_slurp_extended_name_table _bfd_noarchive_slurp_extended_name_table
#define bfd_mach_o_construct_extended_name_table _bfd_noarchive_construct_extended_name_table
#define bfd_mach_o_truncate_arname _bfd_noarchive_truncate_arname
#define bfd_mach_o_write_armap _bfd_noarchive_write_armap
#define bfd_mach_o_get_elt_at_index _bfd_noarchive_get_elt_at_index
#define bfd_mach_o_generic_stat_arch_elt _bfd_noarchive_generic_stat_arch_elt
#define bfd_mach_o_update_armap_timestamp _bfd_noarchive_update_armap_timestamp
#define	bfd_mach_o_close_and_cleanup _bfd_generic_close_and_cleanup
#define bfd_mach_o_bfd_free_cached_info _bfd_generic_bfd_free_cached_info
#define bfd_mach_o_new_section_hook _bfd_generic_new_section_hook
#define bfd_mach_o_get_section_contents_in_window _bfd_generic_get_section_contents_in_window
#define bfd_mach_o_bfd_is_local_label_name _bfd_nosymbols_bfd_is_local_label_name
#define bfd_mach_o_get_lineno _bfd_nosymbols_get_lineno
#define bfd_mach_o_find_nearest_line _bfd_nosymbols_find_nearest_line
#define bfd_mach_o_bfd_make_debug_symbol _bfd_nosymbols_bfd_make_debug_symbol
#define bfd_mach_o_read_minisymbols _bfd_generic_read_minisymbols
#define bfd_mach_o_minisymbol_to_symbol _bfd_generic_minisymbol_to_symbol
#define bfd_mach_o_get_reloc_upper_bound _bfd_norelocs_get_reloc_upper_bound
#define bfd_mach_o_canonicalize_reloc _bfd_norelocs_canonicalize_reloc
#define bfd_mach_o_bfd_reloc_type_lookup _bfd_norelocs_bfd_reloc_type_lookup
#define bfd_mach_o_bfd_get_relocated_section_contents bfd_generic_get_relocated_section_contents
#define bfd_mach_o_bfd_relax_section bfd_generic_relax_section
#define bfd_mach_o_bfd_link_hash_table_create _bfd_generic_link_hash_table_create
#define bfd_mach_o_bfd_link_hash_table_free _bfd_generic_link_hash_table_free
#define bfd_mach_o_bfd_link_add_symbols _bfd_generic_link_add_symbols
#define bfd_mach_o_bfd_link_just_syms _bfd_generic_link_just_syms
#define bfd_mach_o_bfd_final_link _bfd_generic_final_link
#define bfd_mach_o_bfd_link_split_section _bfd_generic_link_split_section
#define bfd_mach_o_set_arch_mach bfd_default_set_arch_mach
#define bfd_mach_o_bfd_merge_private_bfd_data _bfd_generic_bfd_merge_private_bfd_data
#define bfd_mach_o_bfd_set_private_flags _bfd_generic_bfd_set_private_flags
#define bfd_mach_o_bfd_print_private_bfd_data _bfd_generic_bfd_print_private_bfd_data
#define bfd_mach_o_get_section_contents _bfd_generic_get_section_contents
#define bfd_mach_o_set_section_contents _bfd_generic_set_section_contents
#define bfd_mach_o_bfd_gc_sections bfd_generic_gc_sections
#define bfd_mach_o_bfd_merge_sections bfd_generic_merge_sections
#define bfd_mach_o_bfd_discard_group bfd_generic_discard_group

static bfd_boolean bfd_mach_o_bfd_copy_private_symbol_data
  PARAMS ((bfd *, asymbol *, bfd *, asymbol *));
static bfd_boolean bfd_mach_o_bfd_copy_private_section_data
  PARAMS ((bfd *, asection *, bfd *, asection *));
static bfd_boolean bfd_mach_o_bfd_copy_private_bfd_data
  PARAMS ((bfd *, bfd *));
static long bfd_mach_o_count_symbols
  PARAMS ((bfd *));
static long bfd_mach_o_get_symtab_upper_bound
  PARAMS ((bfd *));
static long bfd_mach_o_canonicalize_symtab
  PARAMS ((bfd *, asymbol **));
static void bfd_mach_o_get_symbol_info
  PARAMS ((bfd *, asymbol *, symbol_info *));
static void bfd_mach_o_print_symbol
  PARAMS ((bfd *, PTR, asymbol *, bfd_print_symbol_type));
static void bfd_mach_o_convert_architecture
  PARAMS ((bfd_mach_o_cpu_type, bfd_mach_o_cpu_subtype,
	   enum bfd_architecture *, unsigned long *));
static bfd_boolean bfd_mach_o_write_contents
  PARAMS ((bfd *));
static int bfd_mach_o_sizeof_headers
  PARAMS ((bfd *, bfd_boolean));
static asymbol * bfd_mach_o_make_empty_symbol
  PARAMS ((bfd *));
static int bfd_mach_o_write_header
  PARAMS ((bfd *, bfd_mach_o_header *));
static int bfd_mach_o_read_header
  PARAMS ((bfd *, bfd_mach_o_header *));
static asection * bfd_mach_o_make_bfd_section
  PARAMS ((bfd *, bfd_mach_o_section *));
static int bfd_mach_o_scan_read_section
  PARAMS ((bfd *, bfd_mach_o_section *, bfd_vma));
static int bfd_mach_o_scan_write_section
  PARAMS ((bfd *, bfd_mach_o_section *, bfd_vma));
static int bfd_mach_o_scan_write_symtab_symbols
  PARAMS ((bfd *, bfd_mach_o_load_command *));
static int bfd_mach_o_scan_write_thread
  PARAMS ((bfd *, bfd_mach_o_load_command *));
static int bfd_mach_o_scan_read_dylinker
  PARAMS ((bfd *, bfd_mach_o_load_command *));
static int bfd_mach_o_scan_read_dylib
  PARAMS ((bfd *, bfd_mach_o_load_command *));
static int bfd_mach_o_scan_read_prebound_dylib
  PARAMS ((bfd *, bfd_mach_o_load_command *));
static int bfd_mach_o_scan_read_thread
  PARAMS ((bfd *, bfd_mach_o_load_command *));
static int bfd_mach_o_scan_write_symtab
  PARAMS ((bfd *, bfd_mach_o_load_command *));
static int bfd_mach_o_scan_read_dysymtab
  PARAMS ((bfd *, bfd_mach_o_load_command *));
static int bfd_mach_o_scan_read_symtab
  PARAMS ((bfd *, bfd_mach_o_load_command *));
static int bfd_mach_o_scan_read_segment
  PARAMS ((bfd *, bfd_mach_o_load_command *));
static int bfd_mach_o_scan_write_segment
  PARAMS ((bfd *, bfd_mach_o_load_command *));
static int bfd_mach_o_scan_read_command
  PARAMS ((bfd *, bfd_mach_o_load_command *));
static void bfd_mach_o_flatten_sections
  PARAMS ((bfd *));
static const char * bfd_mach_o_i386_flavour_string
  PARAMS ((unsigned int));
static const char * bfd_mach_o_ppc_flavour_string
  PARAMS ((unsigned int));

/* The flags field of a section structure is separated into two parts a section
   type and section attributes.  The section types are mutually exclusive (it
   can only have one type) but the section attributes are not (it may have more
   than one attribute).  */

#define SECTION_TYPE             0x000000ff     /* 256 section types.  */
#define SECTION_ATTRIBUTES       0xffffff00     /*  24 section attributes.  */

/* Constants for the section attributes part of the flags field of a section
   structure.  */

#define SECTION_ATTRIBUTES_USR   0xff000000     /* User-settable attributes.  */
#define S_ATTR_PURE_INSTRUCTIONS 0x80000000     /* Section contains only true machine instructions.  */
#define SECTION_ATTRIBUTES_SYS   0x00ffff00     /* System setable attributes.  */
#define S_ATTR_SOME_INSTRUCTIONS 0x00000400     /* Section contains some machine instructions.  */
#define S_ATTR_EXT_RELOC         0x00000200     /* Section has external relocation entries.  */
#define S_ATTR_LOC_RELOC         0x00000100     /* Section has local relocation entries.  */

#define N_STAB 0xe0
#define N_TYPE 0x1e
#define N_EXT  0x01
#define N_UNDF 0x0
#define N_ABS  0x2
#define N_SECT 0xe
#define N_INDR 0xa

bfd_boolean
bfd_mach_o_valid (abfd)
     bfd *abfd;
{
  if (abfd == NULL || abfd->xvec == NULL)
    return 0;

  if (! ((abfd->xvec == &mach_o_be_vec)
	 || (abfd->xvec == &mach_o_le_vec)
	 || (abfd->xvec == &mach_o_fat_vec)))
    return 0;

  if (abfd->tdata.mach_o_data == NULL)
    return 0;
  return 1;
}

/* Copy any private info we understand from the input symbol
   to the output symbol.  */

static bfd_boolean
bfd_mach_o_bfd_copy_private_symbol_data (ibfd, isymbol, obfd, osymbol)
     bfd *ibfd ATTRIBUTE_UNUSED;
     asymbol *isymbol ATTRIBUTE_UNUSED;
     bfd *obfd ATTRIBUTE_UNUSED;
     asymbol *osymbol ATTRIBUTE_UNUSED;
{
  return TRUE;
}

/* Copy any private info we understand from the input section
   to the output section.  */

static bfd_boolean
bfd_mach_o_bfd_copy_private_section_data (ibfd, isection, obfd, osection)
     bfd *ibfd ATTRIBUTE_UNUSED;
     asection *isection ATTRIBUTE_UNUSED;
     bfd *obfd ATTRIBUTE_UNUSED;
     asection *osection ATTRIBUTE_UNUSED;
{
  return TRUE;
}

/* Copy any private info we understand from the input bfd
   to the output bfd.  */

static bfd_boolean
bfd_mach_o_bfd_copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  BFD_ASSERT (bfd_mach_o_valid (ibfd));
  BFD_ASSERT (bfd_mach_o_valid (obfd));

  obfd->tdata.mach_o_data = ibfd->tdata.mach_o_data;
  obfd->tdata.mach_o_data->ibfd = ibfd;
  return TRUE;
}

static long
bfd_mach_o_count_symbols (abfd)
     bfd *abfd;
{
  bfd_mach_o_data_struct *mdata = NULL;
  long nsyms = 0;
  unsigned long i;

  BFD_ASSERT (bfd_mach_o_valid (abfd));
  mdata = abfd->tdata.mach_o_data;

  for (i = 0; i < mdata->header.ncmds; i++)
    if (mdata->commands[i].type == BFD_MACH_O_LC_SYMTAB)
      {
	bfd_mach_o_symtab_command *sym = &mdata->commands[i].command.symtab;
	nsyms += sym->nsyms;
      }

  return nsyms;
}

static long
bfd_mach_o_get_symtab_upper_bound (abfd)
     bfd *abfd;
{
  long nsyms = bfd_mach_o_count_symbols (abfd);

  if (nsyms < 0)
    return nsyms;

  return ((nsyms + 1) * sizeof (asymbol *));
}

static long
bfd_mach_o_canonicalize_symtab (abfd, alocation)
     bfd *abfd;
     asymbol **alocation;
{
  bfd_mach_o_data_struct *mdata = abfd->tdata.mach_o_data;
  long nsyms = bfd_mach_o_count_symbols (abfd);
  asymbol **csym = alocation;
  unsigned long i, j;

  if (nsyms < 0)
    return nsyms;

  for (i = 0; i < mdata->header.ncmds; i++)
    {
      if (mdata->commands[i].type == BFD_MACH_O_LC_SYMTAB)
	{
	  bfd_mach_o_symtab_command *sym = &mdata->commands[i].command.symtab;

	  if (bfd_mach_o_scan_read_symtab_symbols (abfd, &mdata->commands[i].command.symtab) != 0)
	    {
	      fprintf (stderr, "bfd_mach_o_canonicalize_symtab: unable to load symbols for section %lu\n", i);
	      return 0;
	    }

	  BFD_ASSERT (sym->symbols != NULL);

	  for (j = 0; j < sym->nsyms; j++)
	    {
	      BFD_ASSERT (csym < (alocation + nsyms));
	      *csym++ = &sym->symbols[j];
	    }
	}
    }

  *csym++ = NULL;

  return nsyms;
}

static void
bfd_mach_o_get_symbol_info (abfd, symbol, ret)
     bfd *abfd ATTRIBUTE_UNUSED;
     asymbol *symbol;
     symbol_info *ret;
{
  bfd_symbol_info (symbol, ret);
}

static void
bfd_mach_o_print_symbol (abfd, afile, symbol, how)
     bfd *abfd;
     PTR afile;
     asymbol *symbol;
     bfd_print_symbol_type how;
{
  FILE *file = (FILE *) afile;

  switch (how)
    {
    case bfd_print_symbol_name:
      fprintf (file, "%s", symbol->name);
      break;
    default:
      bfd_print_symbol_vandf (abfd, (PTR) file, symbol);
      fprintf (file, " %-5s %s", symbol->section->name, symbol->name);
    }
}

static void
bfd_mach_o_convert_architecture (mtype, msubtype, type, subtype)
     bfd_mach_o_cpu_type mtype;
     bfd_mach_o_cpu_subtype msubtype ATTRIBUTE_UNUSED;
     enum bfd_architecture *type;
     unsigned long *subtype;
{
  *subtype = bfd_arch_unknown;

  switch (mtype)
    {
    case BFD_MACH_O_CPU_TYPE_VAX: *type = bfd_arch_vax; break;
    case BFD_MACH_O_CPU_TYPE_MC680x0: *type = bfd_arch_m68k; break;
    case BFD_MACH_O_CPU_TYPE_I386: *type = bfd_arch_i386; break;
    case BFD_MACH_O_CPU_TYPE_MIPS: *type = bfd_arch_mips; break;
    case BFD_MACH_O_CPU_TYPE_MC98000: *type = bfd_arch_m98k; break;
    case BFD_MACH_O_CPU_TYPE_HPPA: *type = bfd_arch_hppa; break;
    case BFD_MACH_O_CPU_TYPE_ARM: *type = bfd_arch_arm; break;
    case BFD_MACH_O_CPU_TYPE_MC88000: *type = bfd_arch_m88k; break;
    case BFD_MACH_O_CPU_TYPE_SPARC: *type = bfd_arch_sparc; break;
    case BFD_MACH_O_CPU_TYPE_I860: *type = bfd_arch_i860; break;
    case BFD_MACH_O_CPU_TYPE_ALPHA: *type = bfd_arch_alpha; break;
    case BFD_MACH_O_CPU_TYPE_POWERPC: *type = bfd_arch_powerpc; break;
    default: *type = bfd_arch_unknown; break;
    }

  switch (*type)
    {
    case bfd_arch_i386: *subtype = bfd_mach_i386_i386; break;
    case bfd_arch_sparc: *subtype = bfd_mach_sparc; break;
    default:
      *subtype = bfd_arch_unknown;
    }
}

static bfd_boolean
bfd_mach_o_write_contents (abfd)
     bfd *abfd;
{
  unsigned int i;
  asection *s;

  bfd_mach_o_data_struct *mdata = abfd->tdata.mach_o_data;

  /* Write data sections first in case they overlap header data to be
     written later.  */

  for (s = abfd->sections; s != (asection *) NULL; s = s->next)
    ;

#if 0
  for (i = 0; i < mdata->header.ncmds; i++)
    {
      bfd_mach_o_load_command *cur = &mdata->commands[i];
      if (cur->type != BFD_MACH_O_LC_SEGMENT)
	break;

      {
	bfd_mach_o_segment_command *seg = &cur->command.segment;
	char buf[1024];
	bfd_vma nbytes = seg->filesize;
	bfd_vma curoff = seg->fileoff;

	while (nbytes > 0)
	  {
	    bfd_vma thisread = nbytes;

	    if (thisread > 1024)
	      thisread = 1024;

	    bfd_seek (abfd, curoff, SEEK_SET);
	    if (bfd_bread ((PTR) buf, thisread, abfd) != thisread)
	      return FALSE;

	    bfd_seek (abfd, curoff, SEEK_SET);
	    if (bfd_bwrite ((PTR) buf, thisread, abfd) != thisread)
	      return FALSE;

	    nbytes -= thisread;
	    curoff += thisread;
	  }
      }
  }
#endif

  /* Now write header information.  */
  if (bfd_mach_o_write_header (abfd, &mdata->header) != 0)
    return FALSE;

  for (i = 0; i < mdata->header.ncmds; i++)
    {
      unsigned char buf[8];
      bfd_mach_o_load_command *cur = &mdata->commands[i];
      unsigned long typeflag;

      typeflag = cur->type_required ? cur->type & BFD_MACH_O_LC_REQ_DYLD : cur->type;

      bfd_h_put_32 (abfd, typeflag, buf);
      bfd_h_put_32 (abfd, cur->len, buf + 4);

      bfd_seek (abfd, cur->offset, SEEK_SET);
      if (bfd_bwrite ((PTR) buf, 8, abfd) != 8)
	return FALSE;

      switch (cur->type)
	{
	case BFD_MACH_O_LC_SEGMENT:
	  if (bfd_mach_o_scan_write_segment (abfd, cur) != 0)
	    return FALSE;
	  break;
	case BFD_MACH_O_LC_SYMTAB:
	  if (bfd_mach_o_scan_write_symtab (abfd, cur) != 0)
	    return FALSE;
	  break;
	case BFD_MACH_O_LC_SYMSEG:
	  break;
	case BFD_MACH_O_LC_THREAD:
	case BFD_MACH_O_LC_UNIXTHREAD:
	  if (bfd_mach_o_scan_write_thread (abfd, cur) != 0)
	    return FALSE;
	  break;
	case BFD_MACH_O_LC_LOADFVMLIB:
	case BFD_MACH_O_LC_IDFVMLIB:
	case BFD_MACH_O_LC_IDENT:
	case BFD_MACH_O_LC_FVMFILE:
	case BFD_MACH_O_LC_PREPAGE:
	case BFD_MACH_O_LC_DYSYMTAB:
	case BFD_MACH_O_LC_LOAD_DYLIB:
	case BFD_MACH_O_LC_LOAD_WEAK_DYLIB:
	case BFD_MACH_O_LC_ID_DYLIB:
	case BFD_MACH_O_LC_LOAD_DYLINKER:
	case BFD_MACH_O_LC_ID_DYLINKER:
	case BFD_MACH_O_LC_PREBOUND_DYLIB:
	case BFD_MACH_O_LC_ROUTINES:
	case BFD_MACH_O_LC_SUB_FRAMEWORK:
	  break;
	default:
	  fprintf (stderr,
		   "unable to write unknown load command 0x%lx\n",
		   (long) cur->type);
	  return FALSE;
	}
    }

  return TRUE;
}

static int
bfd_mach_o_sizeof_headers (a, b)
     bfd *a ATTRIBUTE_UNUSED;
     bfd_boolean b ATTRIBUTE_UNUSED;
{
  return 0;
}

/* Make an empty symbol.  This is required only because
   bfd_make_section_anyway wants to create a symbol for the section.  */

static asymbol *
bfd_mach_o_make_empty_symbol (abfd)
     bfd *abfd;
{
  asymbol *new;

  new = (asymbol *) bfd_zalloc (abfd, sizeof (asymbol));
  if (new == NULL)
    return new;
  new->the_bfd = abfd;
  return new;
}

static int
bfd_mach_o_write_header (abfd, header)
     bfd *abfd;
     bfd_mach_o_header *header;
{
  unsigned char buf[28];

  bfd_h_put_32 (abfd, header->magic, buf + 0);
  bfd_h_put_32 (abfd, header->cputype, buf + 4);
  bfd_h_put_32 (abfd, header->cpusubtype, buf + 8);
  bfd_h_put_32 (abfd, header->filetype, buf + 12);
  bfd_h_put_32 (abfd, header->ncmds, buf + 16);
  bfd_h_put_32 (abfd, header->sizeofcmds, buf + 20);
  bfd_h_put_32 (abfd, header->flags, buf + 24);

  bfd_seek (abfd, 0, SEEK_SET);
  if (bfd_bwrite ((PTR) buf, 28, abfd) != 28)
    return -1;

  return 0;
}

static int
bfd_mach_o_read_header (abfd, header)
     bfd *abfd;
     bfd_mach_o_header *header;
{
  unsigned char buf[28];
  bfd_vma (*get32) (const void *) = NULL;

  bfd_seek (abfd, 0, SEEK_SET);

  if (bfd_bread ((PTR) buf, 28, abfd) != 28)
    return -1;

  if (bfd_getb32 (buf) == 0xfeedface)
    {
      header->byteorder = BFD_ENDIAN_BIG;
      header->magic = 0xfeedface;
      get32 = bfd_getb32;
    }
  else if (bfd_getl32 (buf) == 0xfeedface)
    {
      header->byteorder = BFD_ENDIAN_LITTLE;
      header->magic = 0xfeedface;
      get32 = bfd_getl32;
    }
  else
    {
      header->byteorder = BFD_ENDIAN_UNKNOWN;
      return -1;
    }

  header->cputype = (*get32) (buf + 4);
  header->cpusubtype = (*get32) (buf + 8);
  header->filetype = (*get32) (buf + 12);
  header->ncmds = (*get32) (buf + 16);
  header->sizeofcmds = (*get32) (buf + 20);
  header->flags = (*get32) (buf + 24);

  return 0;
}

static asection *
bfd_mach_o_make_bfd_section (abfd, section)
     bfd *abfd;
     bfd_mach_o_section *section;
{
  asection *bfdsec;
  char *sname;
  const char *prefix = "LC_SEGMENT";
  unsigned int snamelen;

  snamelen = strlen (prefix) + 1
    + strlen (section->segname) + 1
    + strlen (section->sectname) + 1;

  sname = (char *) bfd_alloc (abfd, snamelen);
  if (sname == NULL)
    return NULL;
  sprintf (sname, "%s.%s.%s", prefix, section->segname, section->sectname);

  bfdsec = bfd_make_section_anyway (abfd, sname);
  if (bfdsec == NULL)
    return NULL;

  bfdsec->vma = section->addr;
  bfdsec->lma = section->addr;
  bfdsec->_raw_size = section->size;
  bfdsec->filepos = section->offset;
  bfdsec->alignment_power = section->align;

  if (section->flags & BFD_MACH_O_S_ZEROFILL)
    bfdsec->flags = SEC_ALLOC;
  else
    bfdsec->flags = SEC_HAS_CONTENTS | SEC_LOAD | SEC_ALLOC | SEC_CODE;

  return bfdsec;
}

static int
bfd_mach_o_scan_read_section (abfd, section, offset)
     bfd *abfd;
     bfd_mach_o_section *section;
     bfd_vma offset;
{
  unsigned char buf[68];

  bfd_seek (abfd, offset, SEEK_SET);
  if (bfd_bread ((PTR) buf, 68, abfd) != 68)
    return -1;

  memcpy (section->sectname, buf, 16);
  section->sectname[16] = '\0';
  memcpy (section->segname, buf + 16, 16);
  section->segname[16] = '\0';
  section->addr = bfd_h_get_32 (abfd, buf + 32);
  section->size = bfd_h_get_32 (abfd, buf + 36);
  section->offset = bfd_h_get_32 (abfd, buf + 40);
  section->align = bfd_h_get_32 (abfd, buf + 44);
  section->reloff = bfd_h_get_32 (abfd, buf + 48);
  section->nreloc = bfd_h_get_32 (abfd, buf + 52);
  section->flags = bfd_h_get_32 (abfd, buf + 56);
  section->reserved1 = bfd_h_get_32 (abfd, buf + 60);
  section->reserved2 = bfd_h_get_32 (abfd, buf + 64);
  section->bfdsection = bfd_mach_o_make_bfd_section (abfd, section);

  if (section->bfdsection == NULL)
    return -1;

  return 0;
}

static int
bfd_mach_o_scan_write_section (abfd, section, offset)
     bfd *abfd;
     bfd_mach_o_section *section;
     bfd_vma offset;
{
  unsigned char buf[68];

  memcpy (buf, section->sectname, 16);
  memcpy (buf + 16, section->segname, 16);
  bfd_h_put_32 (abfd, section->addr, buf + 32);
  bfd_h_put_32 (abfd, section->size, buf + 36);
  bfd_h_put_32 (abfd, section->offset, buf + 40);
  bfd_h_put_32 (abfd, section->align, buf + 44);
  bfd_h_put_32 (abfd, section->reloff, buf + 48);
  bfd_h_put_32 (abfd, section->nreloc, buf + 52);
  bfd_h_put_32 (abfd, section->flags, buf + 56);
  /* bfd_h_put_32 (abfd, section->reserved1, buf + 60); */
  /* bfd_h_put_32 (abfd, section->reserved2, buf + 64); */

  bfd_seek (abfd, offset, SEEK_SET);
  if (bfd_bwrite ((PTR) buf, 68, abfd) != 68)
    return -1;

  return 0;
}

static int
bfd_mach_o_scan_write_symtab_symbols (abfd, command)
     bfd *abfd;
     bfd_mach_o_load_command *command;
{
  bfd_mach_o_symtab_command *sym = &command->command.symtab;
  asymbol *s = NULL;
  unsigned long i;

  for (i = 0; i < sym->nsyms; i++)
    {
      unsigned char buf[12];
      bfd_vma symoff = sym->symoff + (i * 12);
      unsigned char ntype = 0;
      unsigned char nsect = 0;
      short ndesc = 0;

      s = &sym->symbols[i];

      /* Don't set this from the symbol information; use stored values.  */
#if 0
      if (s->flags & BSF_GLOBAL)
	ntype |= N_EXT;
      if (s->flags & BSF_DEBUGGING)
	ntype |= N_STAB;

      if (s->section == bfd_und_section_ptr)
	ntype |= N_UNDF;
      else if (s->section == bfd_abs_section_ptr)
	ntype |= N_ABS;
      else
	ntype |= N_SECT;
#endif

      /* Instead just set from the stored values.  */
      ntype = (s->udata.i >> 24) & 0xff;
      nsect = (s->udata.i >> 16) & 0xff;
      ndesc = s->udata.i & 0xffff;

      bfd_h_put_32 (abfd, s->name - sym->strtab, buf);
      bfd_h_put_8 (abfd, ntype, buf + 4);
      bfd_h_put_8 (abfd, nsect, buf + 5);
      bfd_h_put_16 (abfd, ndesc, buf + 6);
      bfd_h_put_32 (abfd, s->section->vma + s->value, buf + 8);

      bfd_seek (abfd, symoff, SEEK_SET);
      if (bfd_bwrite ((PTR) buf, 12, abfd) != 12)
	{
	  fprintf (stderr, "bfd_mach_o_scan_write_symtab_symbols: unable to write %d bytes at %lu\n",
		   12, (unsigned long) symoff);
	  return -1;
	}
    }

  return 0;
}

int
bfd_mach_o_scan_read_symtab_symbol (abfd, sym, s, i)
     bfd *abfd;
     bfd_mach_o_symtab_command *sym;
     asymbol *s;
     unsigned long i;
{
  bfd_mach_o_data_struct *mdata = abfd->tdata.mach_o_data;
  bfd_vma symoff = sym->symoff + (i * 12);
  unsigned char buf[12];
  unsigned char type = -1;
  unsigned char section = -1;
  short desc = -1;
  unsigned long value = -1;
  unsigned long stroff = -1;
  unsigned int symtype = -1;

  BFD_ASSERT (sym->strtab != NULL);

  bfd_seek (abfd, symoff, SEEK_SET);
  if (bfd_bread ((PTR) buf, 12, abfd) != 12)
    {
      fprintf (stderr, "bfd_mach_o_scan_read_symtab_symbol: unable to read %d bytes at %lu\n",
	       12, (unsigned long) symoff);
      return -1;
    }

  stroff = bfd_h_get_32 (abfd, buf);
  type = bfd_h_get_8 (abfd, buf + 4);
  symtype = (type & 0x0e);
  section = bfd_h_get_8 (abfd, buf + 5) - 1;
  desc = bfd_h_get_16 (abfd, buf + 6);
  value = bfd_h_get_32 (abfd, buf + 8);

  if (stroff >= sym->strsize)
    {
      fprintf (stderr, "bfd_mach_o_scan_read_symtab_symbol: symbol name out of range (%lu >= %lu)\n",
	       (unsigned long) stroff, (unsigned long) sym->strsize);
      return -1;
    }

  s->the_bfd = abfd;
  s->name = sym->strtab + stroff;
  s->value = value;
  s->udata.i = (type << 24) | (section << 16) | desc;
  s->flags = 0x0;

  if (type & BFD_MACH_O_N_STAB)
    {
      s->flags |= BSF_DEBUGGING;
      s->section = bfd_und_section_ptr;
    }
  else
    {
      if (type & BFD_MACH_O_N_PEXT)
	{
	  type &= ~BFD_MACH_O_N_PEXT;
	  s->flags |= BSF_GLOBAL;
	}

      if (type & BFD_MACH_O_N_EXT)
	{
	  type &= ~BFD_MACH_O_N_EXT;
	  s->flags |= BSF_GLOBAL;
	}

      switch (symtype)
	{
	case BFD_MACH_O_N_UNDF:
	  s->section = bfd_und_section_ptr;
	  break;
	case BFD_MACH_O_N_PBUD:
	  s->section = bfd_und_section_ptr;
	  break;
	case BFD_MACH_O_N_ABS:
	  s->section = bfd_abs_section_ptr;
	  break;
	case BFD_MACH_O_N_SECT:
	  if ((section > 0) && (section <= mdata->nsects))
	    {
	      s->section = mdata->sections[section - 1]->bfdsection;
	      s->value = s->value - mdata->sections[section - 1]->addr;
	    }
	  else
	    {
	      /* Mach-O uses 0 to mean "no section"; not an error.  */
	      if (section != 0)
		{
		  fprintf (stderr, "bfd_mach_o_scan_read_symtab_symbol: "
			   "symbol \"%s\" specified invalid section %d (max %lu): setting to undefined\n",
			   s->name, section, mdata->nsects);
		}
	      s->section = bfd_und_section_ptr;
	    }
	  break;
	case BFD_MACH_O_N_INDR:
	  fprintf (stderr, "bfd_mach_o_scan_read_symtab_symbol: "
		   "symbol \"%s\" is unsupported 'indirect' reference: setting to undefined\n",
		   s->name);
	  s->section = bfd_und_section_ptr;
	  break;
	default:
	  fprintf (stderr, "bfd_mach_o_scan_read_symtab_symbol: "
		   "symbol \"%s\" specified invalid type field 0x%x: setting to undefined\n",
		   s->name, symtype);
	  s->section = bfd_und_section_ptr;
	  break;
	}
    }

  return 0;
}

int
bfd_mach_o_scan_read_symtab_strtab (abfd, sym)
     bfd *abfd;
     bfd_mach_o_symtab_command *sym;
{
  BFD_ASSERT (sym->strtab == NULL);

  if (abfd->flags & BFD_IN_MEMORY)
    {
      struct bfd_in_memory *b;

      b = (struct bfd_in_memory *) abfd->iostream;

      if ((sym->stroff + sym->strsize) > b->size)
	{
	  bfd_set_error (bfd_error_file_truncated);
	  return -1;
	}
      sym->strtab = b->buffer + sym->stroff;
      return 0;
    }

  sym->strtab = bfd_alloc (abfd, sym->strsize);
  if (sym->strtab == NULL)
    return -1;

  bfd_seek (abfd, sym->stroff, SEEK_SET);
  if (bfd_bread ((PTR) sym->strtab, sym->strsize, abfd) != sym->strsize)
    {
      fprintf (stderr, "bfd_mach_o_scan_read_symtab_strtab: unable to read %lu bytes at %lu\n",
	       sym->strsize, sym->stroff);
      return -1;
    }

  return 0;
}

int
bfd_mach_o_scan_read_symtab_symbols (abfd, sym)
     bfd *abfd;
     bfd_mach_o_symtab_command *sym;
{
  unsigned long i;
  int ret;

  BFD_ASSERT (sym->symbols == NULL);
  sym->symbols = bfd_alloc (abfd, sym->nsyms * sizeof (asymbol));

  if (sym->symbols == NULL)
    {
      fprintf (stderr, "bfd_mach_o_scan_read_symtab_symbols: unable to allocate memory for symbols\n");
      return -1;
    }

  ret = bfd_mach_o_scan_read_symtab_strtab (abfd, sym);
  if (ret != 0)
    return ret;

  for (i = 0; i < sym->nsyms; i++)
    {
      ret = bfd_mach_o_scan_read_symtab_symbol (abfd, sym, &sym->symbols[i], i);
      if (ret != 0)
	return ret;
    }

  return 0;
}

int
bfd_mach_o_scan_read_dysymtab_symbol (abfd, dysym, sym, s, i)
     bfd *abfd;
     bfd_mach_o_dysymtab_command *dysym;
     bfd_mach_o_symtab_command *sym;
     asymbol *s;
     unsigned long i;
{
  unsigned long isymoff = dysym->indirectsymoff + (i * 4);
  unsigned long symindex;
  unsigned char buf[4];

  BFD_ASSERT (i < dysym->nindirectsyms);

  bfd_seek (abfd, isymoff, SEEK_SET);
  if (bfd_bread ((PTR) buf, 4, abfd) != 4)
    {
      fprintf (stderr, "bfd_mach_o_scan_read_dysymtab_symbol: unable to read %lu bytes at %lu\n",
	       (unsigned long) 4, isymoff);
      return -1;
    }
  symindex = bfd_h_get_32 (abfd, buf);

  return bfd_mach_o_scan_read_symtab_symbol (abfd, sym, s, symindex);
}

static const char *
bfd_mach_o_i386_flavour_string (flavour)
     unsigned int flavour;
{
  switch ((int) flavour)
    {
    case BFD_MACH_O_i386_NEW_THREAD_STATE: return "i386_NEW_THREAD_STATE";
    case BFD_MACH_O_i386_FLOAT_STATE: return "i386_FLOAT_STATE";
    case BFD_MACH_O_i386_ISA_PORT_MAP_STATE: return "i386_ISA_PORT_MAP_STATE";
    case BFD_MACH_O_i386_V86_ASSIST_STATE: return "i386_V86_ASSIST_STATE";
    case BFD_MACH_O_i386_REGS_SEGS_STATE: return "i386_REGS_SEGS_STATE";
    case BFD_MACH_O_i386_THREAD_SYSCALL_STATE: return "i386_THREAD_SYSCALL_STATE";
    case BFD_MACH_O_i386_THREAD_STATE_NONE: return "i386_THREAD_STATE_NONE";
    case BFD_MACH_O_i386_SAVED_STATE: return "i386_SAVED_STATE";
    case BFD_MACH_O_i386_THREAD_STATE: return "i386_THREAD_STATE";
    case BFD_MACH_O_i386_THREAD_FPSTATE: return "i386_THREAD_FPSTATE";
    case BFD_MACH_O_i386_THREAD_EXCEPTSTATE: return "i386_THREAD_EXCEPTSTATE";
    case BFD_MACH_O_i386_THREAD_CTHREADSTATE: return "i386_THREAD_CTHREADSTATE";
    default: return "UNKNOWN";
    }
}

static const char *
bfd_mach_o_ppc_flavour_string (flavour)
     unsigned int flavour;
{
  switch ((int) flavour)
    {
    case BFD_MACH_O_PPC_THREAD_STATE: return "PPC_THREAD_STATE";
    case BFD_MACH_O_PPC_FLOAT_STATE: return "PPC_FLOAT_STATE";
    case BFD_MACH_O_PPC_EXCEPTION_STATE: return "PPC_EXCEPTION_STATE";
    case BFD_MACH_O_PPC_VECTOR_STATE: return "PPC_VECTOR_STATE";
    default: return "UNKNOWN";
    }
}

static int
bfd_mach_o_scan_write_thread (abfd, command)
     bfd *abfd;
     bfd_mach_o_load_command *command;
{
  bfd_mach_o_thread_command *cmd = &command->command.thread;
  unsigned int i;
  unsigned char buf[8];
  bfd_vma offset;
  unsigned int nflavours;

  BFD_ASSERT ((command->type == BFD_MACH_O_LC_THREAD)
	      || (command->type == BFD_MACH_O_LC_UNIXTHREAD));

  offset = 8;
  nflavours = 0;
  for (i = 0; i < cmd->nflavours; i++)
    {
      BFD_ASSERT ((cmd->flavours[i].size % 4) == 0);
      BFD_ASSERT (cmd->flavours[i].offset == (command->offset + offset + 8));

      bfd_h_put_32 (abfd, cmd->flavours[i].flavour, buf);
      bfd_h_put_32 (abfd, (cmd->flavours[i].size / 4), buf + 4);

      bfd_seek (abfd, command->offset + offset, SEEK_SET);
      if (bfd_bwrite ((PTR) buf, 8, abfd) != 8)
	return -1;

      offset += cmd->flavours[i].size + 8;
    }

  return 0;
}

static int
bfd_mach_o_scan_read_dylinker (abfd, command)
     bfd *abfd;
     bfd_mach_o_load_command *command;
{
  bfd_mach_o_dylinker_command *cmd = &command->command.dylinker;
  unsigned char buf[4];
  unsigned int nameoff;
  asection *bfdsec;
  char *sname;
  const char *prefix;

  BFD_ASSERT ((command->type == BFD_MACH_O_LC_ID_DYLINKER)
	      || (command->type == BFD_MACH_O_LC_LOAD_DYLINKER));

  bfd_seek (abfd, command->offset + 8, SEEK_SET);
  if (bfd_bread ((PTR) buf, 4, abfd) != 4)
    return -1;

  nameoff = bfd_h_get_32 (abfd, buf + 0);

  cmd->name_offset = command->offset + nameoff;
  cmd->name_len = command->len - nameoff;

  if (command->type == BFD_MACH_O_LC_LOAD_DYLINKER)
    prefix = "LC_LOAD_DYLINKER";
  else if (command->type == BFD_MACH_O_LC_ID_DYLINKER)
    prefix = "LC_ID_DYLINKER";
  else
    abort ();

  sname = (char *) bfd_alloc (abfd, strlen (prefix) + 1);
  if (sname == NULL)
    return -1;
  strcpy (sname, prefix);

  bfdsec = bfd_make_section_anyway (abfd, sname);
  if (bfdsec == NULL)
    return -1;

  bfdsec->vma = 0;
  bfdsec->lma = 0;
  bfdsec->_raw_size = command->len - 8;
  bfdsec->filepos = command->offset + 8;
  bfdsec->alignment_power = 0;
  bfdsec->flags = SEC_HAS_CONTENTS;

  cmd->section = bfdsec;

  return 0;
}

static int
bfd_mach_o_scan_read_dylib (abfd, command)
     bfd *abfd;
     bfd_mach_o_load_command *command;
{
  bfd_mach_o_dylib_command *cmd = &command->command.dylib;
  unsigned char buf[16];
  unsigned int nameoff;
  asection *bfdsec;
  char *sname;
  const char *prefix;

  BFD_ASSERT ((command->type == BFD_MACH_O_LC_ID_DYLIB)
	      || (command->type == BFD_MACH_O_LC_LOAD_DYLIB)
	      || (command->type == BFD_MACH_O_LC_LOAD_WEAK_DYLIB));

  bfd_seek (abfd, command->offset + 8, SEEK_SET);
  if (bfd_bread ((PTR) buf, 16, abfd) != 16)
    return -1;

  nameoff = bfd_h_get_32 (abfd, buf + 0);
  cmd->timestamp = bfd_h_get_32 (abfd, buf + 4);
  cmd->current_version = bfd_h_get_32 (abfd, buf + 8);
  cmd->compatibility_version = bfd_h_get_32 (abfd, buf + 12);

  cmd->name_offset = command->offset + nameoff;
  cmd->name_len = command->len - nameoff;

  if (command->type == BFD_MACH_O_LC_LOAD_DYLIB)
    prefix = "LC_LOAD_DYLIB";
  else if (command->type == BFD_MACH_O_LC_LOAD_WEAK_DYLIB)
    prefix = "LC_LOAD_WEAK_DYLIB";
  else if (command->type == BFD_MACH_O_LC_ID_DYLIB)
    prefix = "LC_ID_DYLIB";
  else
    abort ();

  sname = (char *) bfd_alloc (abfd, strlen (prefix) + 1);
  if (sname == NULL)
    return -1;
  strcpy (sname, prefix);

  bfdsec = bfd_make_section_anyway (abfd, sname);
  if (bfdsec == NULL)
    return -1;

  bfdsec->vma = 0;
  bfdsec->lma = 0;
  bfdsec->_raw_size = command->len - 8;
  bfdsec->filepos = command->offset + 8;
  bfdsec->alignment_power = 0;
  bfdsec->flags = SEC_HAS_CONTENTS;

  cmd->section = bfdsec;

  return 0;
}

static int
bfd_mach_o_scan_read_prebound_dylib (abfd, command)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_mach_o_load_command *command ATTRIBUTE_UNUSED;
{
  /* bfd_mach_o_prebound_dylib_command *cmd = &command->command.prebound_dylib; */

  BFD_ASSERT (command->type == BFD_MACH_O_LC_PREBOUND_DYLIB);
  return 0;
}

static int
bfd_mach_o_scan_read_thread (abfd, command)
     bfd *abfd;
     bfd_mach_o_load_command *command;
{
  bfd_mach_o_data_struct *mdata = NULL;
  bfd_mach_o_thread_command *cmd = &command->command.thread;
  unsigned char buf[8];
  bfd_vma offset;
  unsigned int nflavours;
  unsigned int i;

  BFD_ASSERT ((command->type == BFD_MACH_O_LC_THREAD)
	      || (command->type == BFD_MACH_O_LC_UNIXTHREAD));

  BFD_ASSERT (bfd_mach_o_valid (abfd));
  mdata = abfd->tdata.mach_o_data;

  offset = 8;
  nflavours = 0;
  while (offset != command->len)
    {
      if (offset >= command->len)
	return -1;

      bfd_seek (abfd, command->offset + offset, SEEK_SET);

      if (bfd_bread ((PTR) buf, 8, abfd) != 8)
	return -1;

      offset += 8 + bfd_h_get_32 (abfd, buf + 4) * 4;
      nflavours++;
    }

  cmd->flavours =
    ((bfd_mach_o_thread_flavour *)
     bfd_alloc (abfd, nflavours * sizeof (bfd_mach_o_thread_flavour)));
  if (cmd->flavours == NULL)
    return -1;
  cmd->nflavours = nflavours;

  offset = 8;
  nflavours = 0;
  while (offset != command->len)
    {
      if (offset >= command->len)
	return -1;

      if (nflavours >= cmd->nflavours)
	return -1;

      bfd_seek (abfd, command->offset + offset, SEEK_SET);

      if (bfd_bread ((PTR) buf, 8, abfd) != 8)
	return -1;

      cmd->flavours[nflavours].flavour = bfd_h_get_32 (abfd, buf);
      cmd->flavours[nflavours].offset = command->offset + offset + 8;
      cmd->flavours[nflavours].size = bfd_h_get_32 (abfd, buf + 4) * 4;
      offset += cmd->flavours[nflavours].size + 8;
      nflavours++;
    }

  for (i = 0; i < nflavours; i++)
    {
      asection *bfdsec;
      unsigned int snamelen;
      char *sname;
      const char *flavourstr;
      const char *prefix = "LC_THREAD";
      unsigned int j = 0;

      switch (mdata->header.cputype)
	{
	case BFD_MACH_O_CPU_TYPE_POWERPC:
	  flavourstr = bfd_mach_o_ppc_flavour_string (cmd->flavours[i].flavour);
	  break;
	case BFD_MACH_O_CPU_TYPE_I386:
	  flavourstr = bfd_mach_o_i386_flavour_string (cmd->flavours[i].flavour);
	  break;
	default:
	  flavourstr = "UNKNOWN_ARCHITECTURE";
	  break;
	}

      snamelen = strlen (prefix) + 1 + 20 + 1 + strlen (flavourstr) + 1;
      sname = (char *) bfd_alloc (abfd, snamelen);
      if (sname == NULL)
	return -1;

      for (;;)
	{
	  sprintf (sname, "%s.%s.%u", prefix, flavourstr, j);
	  if (bfd_get_section_by_name (abfd, sname) == NULL)
	    break;
	  j++;
	}

      bfdsec = bfd_make_section (abfd, sname);

      bfdsec->vma = 0;
      bfdsec->lma = 0;
      bfdsec->_raw_size = cmd->flavours[i].size;
      bfdsec->filepos = cmd->flavours[i].offset;
      bfdsec->alignment_power = 0x0;
      bfdsec->flags = SEC_HAS_CONTENTS;

      cmd->section = bfdsec;
    }

  return 0;
}

static int
bfd_mach_o_scan_write_symtab (abfd, command)
     bfd *abfd;
     bfd_mach_o_load_command *command;
{
  bfd_mach_o_symtab_command *seg = &command->command.symtab;
  unsigned char buf[16];

  BFD_ASSERT (command->type == BFD_MACH_O_LC_SYMTAB);

  bfd_h_put_32 (abfd, seg->symoff, buf);
  bfd_h_put_32 (abfd, seg->nsyms, buf + 4);
  bfd_h_put_32 (abfd, seg->stroff, buf + 8);
  bfd_h_put_32 (abfd, seg->strsize, buf + 12);

  bfd_seek (abfd, command->offset + 8, SEEK_SET);
  if (bfd_bwrite ((PTR) buf, 16, abfd) != 16)
    return -1;

  if (bfd_mach_o_scan_write_symtab_symbols (abfd, command) != 0)
    return -1;

  return 0;
}

static int
bfd_mach_o_scan_read_dysymtab (abfd, command)
     bfd *abfd;
     bfd_mach_o_load_command *command;
{
  bfd_mach_o_dysymtab_command *seg = &command->command.dysymtab;
  unsigned char buf[72];

  BFD_ASSERT (command->type == BFD_MACH_O_LC_DYSYMTAB);

  bfd_seek (abfd, command->offset + 8, SEEK_SET);
  if (bfd_bread ((PTR) buf, 72, abfd) != 72)
    return -1;

  seg->ilocalsym = bfd_h_get_32 (abfd, buf + 0);
  seg->nlocalsym = bfd_h_get_32 (abfd, buf + 4);
  seg->iextdefsym = bfd_h_get_32 (abfd, buf + 8);
  seg->nextdefsym = bfd_h_get_32 (abfd, buf + 12);
  seg->iundefsym = bfd_h_get_32 (abfd, buf + 16);
  seg->nundefsym = bfd_h_get_32 (abfd, buf + 20);
  seg->tocoff = bfd_h_get_32 (abfd, buf + 24);
  seg->ntoc = bfd_h_get_32 (abfd, buf + 28);
  seg->modtaboff = bfd_h_get_32 (abfd, buf + 32);
  seg->nmodtab = bfd_h_get_32 (abfd, buf + 36);
  seg->extrefsymoff = bfd_h_get_32 (abfd, buf + 40);
  seg->nextrefsyms = bfd_h_get_32 (abfd, buf + 44);
  seg->indirectsymoff = bfd_h_get_32 (abfd, buf + 48);
  seg->nindirectsyms = bfd_h_get_32 (abfd, buf + 52);
  seg->extreloff = bfd_h_get_32 (abfd, buf + 56);
  seg->nextrel = bfd_h_get_32 (abfd, buf + 60);
  seg->locreloff = bfd_h_get_32 (abfd, buf + 64);
  seg->nlocrel = bfd_h_get_32 (abfd, buf + 68);

  return 0;
}

static int
bfd_mach_o_scan_read_symtab (abfd, command)
     bfd *abfd;
     bfd_mach_o_load_command *command;
{
  bfd_mach_o_symtab_command *seg = &command->command.symtab;
  unsigned char buf[16];
  asection *bfdsec;
  char *sname;
  const char *prefix = "LC_SYMTAB.stabs";

  BFD_ASSERT (command->type == BFD_MACH_O_LC_SYMTAB);

  bfd_seek (abfd, command->offset + 8, SEEK_SET);
  if (bfd_bread ((PTR) buf, 16, abfd) != 16)
    return -1;

  seg->symoff = bfd_h_get_32 (abfd, buf);
  seg->nsyms = bfd_h_get_32 (abfd, buf + 4);
  seg->stroff = bfd_h_get_32 (abfd, buf + 8);
  seg->strsize = bfd_h_get_32 (abfd, buf + 12);
  seg->symbols = NULL;
  seg->strtab = NULL;

  sname = (char *) bfd_alloc (abfd, strlen (prefix) + 1);
  if (sname == NULL)
    return -1;
  strcpy (sname, prefix);

  bfdsec = bfd_make_section_anyway (abfd, sname);
  if (bfdsec == NULL)
    return -1;

  bfdsec->vma = 0;
  bfdsec->lma = 0;
  bfdsec->_raw_size = seg->nsyms * 12;
  bfdsec->filepos = seg->symoff;
  bfdsec->alignment_power = 0;
  bfdsec->flags = SEC_HAS_CONTENTS;

  seg->stabs_segment = bfdsec;

  prefix = "LC_SYMTAB.stabstr";
  sname = (char *) bfd_alloc (abfd, strlen (prefix) + 1);
  if (sname == NULL)
    return -1;
  strcpy (sname, prefix);

  bfdsec = bfd_make_section_anyway (abfd, sname);
  if (bfdsec == NULL)
    return -1;

  bfdsec->vma = 0;
  bfdsec->lma = 0;
  bfdsec->_raw_size = seg->strsize;
  bfdsec->filepos = seg->stroff;
  bfdsec->alignment_power = 0;
  bfdsec->flags = SEC_HAS_CONTENTS;

  seg->stabstr_segment = bfdsec;

  return 0;
}

static int
bfd_mach_o_scan_read_segment (abfd, command)
     bfd *abfd;
     bfd_mach_o_load_command *command;
{
  unsigned char buf[48];
  bfd_mach_o_segment_command *seg = &command->command.segment;
  unsigned long i;
  asection *bfdsec;
  char *sname;
  const char *prefix = "LC_SEGMENT";
  unsigned int snamelen;

  BFD_ASSERT (command->type == BFD_MACH_O_LC_SEGMENT);

  bfd_seek (abfd, command->offset + 8, SEEK_SET);
  if (bfd_bread ((PTR) buf, 48, abfd) != 48)
    return -1;

  memcpy (seg->segname, buf, 16);
  seg->vmaddr = bfd_h_get_32 (abfd, buf + 16);
  seg->vmsize = bfd_h_get_32 (abfd, buf + 20);
  seg->fileoff = bfd_h_get_32 (abfd, buf + 24);
  seg->filesize = bfd_h_get_32 (abfd, buf +  28);
  /* seg->maxprot = bfd_h_get_32 (abfd, buf + 32); */
  /* seg->initprot = bfd_h_get_32 (abfd, buf + 36); */
  seg->nsects = bfd_h_get_32 (abfd, buf + 40);
  seg->flags = bfd_h_get_32 (abfd, buf + 44);

  snamelen = strlen (prefix) + 1 + strlen (seg->segname) + 1;
  sname = (char *) bfd_alloc (abfd, snamelen);
  if (sname == NULL)
    return -1;
  sprintf (sname, "%s.%s", prefix, seg->segname);

  bfdsec = bfd_make_section_anyway (abfd, sname);
  if (bfdsec == NULL)
    return -1;

  bfdsec->vma = seg->vmaddr;
  bfdsec->lma = seg->vmaddr;
  bfdsec->_raw_size = seg->filesize;
  bfdsec->filepos = seg->fileoff;
  bfdsec->alignment_power = 0x0;
  bfdsec->flags = SEC_HAS_CONTENTS | SEC_LOAD | SEC_ALLOC | SEC_CODE;

  seg->segment = bfdsec;

  if (seg->nsects != 0)
    {
      seg->sections =
	((bfd_mach_o_section *)
	 bfd_alloc (abfd, seg->nsects * sizeof (bfd_mach_o_section)));
      if (seg->sections == NULL)
	return -1;

      for (i = 0; i < seg->nsects; i++)
	{
	  bfd_vma segoff = command->offset + 48 + 8 + (i * 68);

	  if (bfd_mach_o_scan_read_section (abfd, &seg->sections[i],
					    segoff) != 0)
	    return -1;
	}
    }

  return 0;
}

static int
bfd_mach_o_scan_write_segment (abfd, command)
     bfd *abfd;
     bfd_mach_o_load_command *command;
{
  unsigned char buf[48];
  bfd_mach_o_segment_command *seg = &command->command.segment;
  unsigned long i;

  BFD_ASSERT (command->type == BFD_MACH_O_LC_SEGMENT);

  memcpy (buf, seg->segname, 16);
  bfd_h_put_32 (abfd, seg->vmaddr, buf + 16);
  bfd_h_put_32 (abfd, seg->vmsize, buf + 20);
  bfd_h_put_32 (abfd, seg->fileoff, buf + 24);
  bfd_h_put_32 (abfd, seg->filesize, buf + 28);
  bfd_h_put_32 (abfd, 0 /* seg->maxprot */, buf + 32);
  bfd_h_put_32 (abfd, 0 /* seg->initprot */, buf + 36);
  bfd_h_put_32 (abfd, seg->nsects, buf + 40);
  bfd_h_put_32 (abfd, seg->flags, buf + 44);

  bfd_seek (abfd, command->offset + 8, SEEK_SET);
  if (bfd_bwrite ((PTR) buf, 48, abfd) != 48)
    return -1;

  {
    char buf[1024];
    bfd_vma nbytes = seg->filesize;
    bfd_vma curoff = seg->fileoff;

    while (nbytes > 0)
      {
	bfd_vma thisread = nbytes;

	if (thisread > 1024)
	  thisread = 1024;

	bfd_seek (abfd, curoff, SEEK_SET);
	if (bfd_bread ((PTR) buf, thisread, abfd) != thisread)
	  return -1;

	bfd_seek (abfd, curoff, SEEK_SET);
	if (bfd_bwrite ((PTR) buf, thisread, abfd) != thisread)
	  return -1;

	nbytes -= thisread;
	curoff += thisread;
      }
  }

  for (i = 0; i < seg->nsects; i++)
    {
      bfd_vma segoff = command->offset + 48 + 8 + (i * 68);

      if (bfd_mach_o_scan_write_section (abfd, &seg->sections[i], segoff) != 0)
	return -1;
    }

  return 0;
}

static int
bfd_mach_o_scan_read_command (abfd, command)
     bfd *abfd;
     bfd_mach_o_load_command *command;
{
  unsigned char buf[8];

  bfd_seek (abfd, command->offset, SEEK_SET);
  if (bfd_bread ((PTR) buf, 8, abfd) != 8)
    return -1;

  command->type = (bfd_h_get_32 (abfd, buf) & ~BFD_MACH_O_LC_REQ_DYLD);
  command->type_required = (bfd_h_get_32 (abfd, buf) & BFD_MACH_O_LC_REQ_DYLD
			    ? 1 : 0);
  command->len = bfd_h_get_32 (abfd, buf + 4);

  switch (command->type)
    {
    case BFD_MACH_O_LC_SEGMENT:
      if (bfd_mach_o_scan_read_segment (abfd, command) != 0)
	return -1;
      break;
    case BFD_MACH_O_LC_SYMTAB:
      if (bfd_mach_o_scan_read_symtab (abfd, command) != 0)
	return -1;
      break;
    case BFD_MACH_O_LC_SYMSEG:
      break;
    case BFD_MACH_O_LC_THREAD:
    case BFD_MACH_O_LC_UNIXTHREAD:
      if (bfd_mach_o_scan_read_thread (abfd, command) != 0)
	return -1;
      break;
    case BFD_MACH_O_LC_LOAD_DYLINKER:
    case BFD_MACH_O_LC_ID_DYLINKER:
      if (bfd_mach_o_scan_read_dylinker (abfd, command) != 0)
	return -1;
      break;
    case BFD_MACH_O_LC_LOAD_DYLIB:
    case BFD_MACH_O_LC_ID_DYLIB:
    case BFD_MACH_O_LC_LOAD_WEAK_DYLIB:
      if (bfd_mach_o_scan_read_dylib (abfd, command) != 0)
	return -1;
      break;
    case BFD_MACH_O_LC_PREBOUND_DYLIB:
      if (bfd_mach_o_scan_read_prebound_dylib (abfd, command) != 0)
	return -1;
      break;
    case BFD_MACH_O_LC_LOADFVMLIB:
    case BFD_MACH_O_LC_IDFVMLIB:
    case BFD_MACH_O_LC_IDENT:
    case BFD_MACH_O_LC_FVMFILE:
    case BFD_MACH_O_LC_PREPAGE:
    case BFD_MACH_O_LC_ROUTINES:
    case BFD_MACH_O_LC_SUB_FRAMEWORK:
      break;
    case BFD_MACH_O_LC_DYSYMTAB:
      if (bfd_mach_o_scan_read_dysymtab (abfd, command) != 0)
	return -1;
      break;
    case BFD_MACH_O_LC_SUB_UMBRELLA:
    case BFD_MACH_O_LC_SUB_CLIENT:
    case BFD_MACH_O_LC_SUB_LIBRARY:
    case BFD_MACH_O_LC_TWOLEVEL_HINTS:
    case BFD_MACH_O_LC_PREBIND_CKSUM:
      break;
    default:
      fprintf (stderr, "unable to read unknown load command 0x%lx\n",
	       (unsigned long) command->type);
      break;
    }

  return 0;
}

static void
bfd_mach_o_flatten_sections (abfd)
     bfd *abfd;
{
  bfd_mach_o_data_struct *mdata = abfd->tdata.mach_o_data;
  long csect = 0;
  unsigned long i, j;

  mdata->nsects = 0;

  for (i = 0; i < mdata->header.ncmds; i++)
    {
      if (mdata->commands[i].type == BFD_MACH_O_LC_SEGMENT)
	{
	  bfd_mach_o_segment_command *seg;

	  seg = &mdata->commands[i].command.segment;
	  mdata->nsects += seg->nsects;
	}
    }

  mdata->sections = bfd_alloc (abfd,
			       mdata->nsects * sizeof (bfd_mach_o_section *));
  csect = 0;

  for (i = 0; i < mdata->header.ncmds; i++)
    {
      if (mdata->commands[i].type == BFD_MACH_O_LC_SEGMENT)
	{
	  bfd_mach_o_segment_command *seg;

	  seg = &mdata->commands[i].command.segment;
	  BFD_ASSERT (csect + seg->nsects <= mdata->nsects);

	  for (j = 0; j < seg->nsects; j++)
	    mdata->sections[csect++] = &seg->sections[j];
	}
    }
}

int
bfd_mach_o_scan_start_address (abfd)
     bfd *abfd;
{
  bfd_mach_o_data_struct *mdata = abfd->tdata.mach_o_data;
  bfd_mach_o_thread_command *cmd = NULL;
  unsigned long i;

  for (i = 0; i < mdata->header.ncmds; i++)
    {
      if ((mdata->commands[i].type == BFD_MACH_O_LC_THREAD) ||
	  (mdata->commands[i].type == BFD_MACH_O_LC_UNIXTHREAD))
	{
	  if (cmd == NULL)
	    cmd = &mdata->commands[i].command.thread;
	  else
	    return 0;
	}
    }

  if (cmd == NULL)
    return 0;

  for (i = 0; i < cmd->nflavours; i++)
    {
      if ((mdata->header.cputype == BFD_MACH_O_CPU_TYPE_I386)
	  && (cmd->flavours[i].flavour
	      == (unsigned long) BFD_MACH_O_i386_THREAD_STATE))
	{
	  unsigned char buf[4];

	  bfd_seek (abfd, cmd->flavours[i].offset + 40, SEEK_SET);

	  if (bfd_bread (buf, 4, abfd) != 4)
	    return -1;

	  abfd->start_address = bfd_h_get_32 (abfd, buf);
	}
      else if ((mdata->header.cputype == BFD_MACH_O_CPU_TYPE_POWERPC)
	       && (cmd->flavours[i].flavour == BFD_MACH_O_PPC_THREAD_STATE))
	{
	  unsigned char buf[4];

	  bfd_seek (abfd, cmd->flavours[i].offset + 0, SEEK_SET);

	  if (bfd_bread (buf, 4, abfd) != 4)
	    return -1;

	  abfd->start_address = bfd_h_get_32 (abfd, buf);
	}
    }

  return 0;
}

int
bfd_mach_o_scan (abfd, header, mdata)
     bfd *abfd;
     bfd_mach_o_header *header;
     bfd_mach_o_data_struct *mdata;
{
  unsigned int i;
  enum bfd_architecture cputype;
  unsigned long cpusubtype;

  mdata->header = *header;
  mdata->symbols = NULL;

  abfd->flags = (abfd->xvec->object_flags
		 | (abfd->flags & (BFD_IN_MEMORY | BFD_IO_FUNCS)));
  abfd->tdata.mach_o_data = mdata;

  bfd_mach_o_convert_architecture (header->cputype, header->cpusubtype,
				   &cputype, &cpusubtype);
  if (cputype == bfd_arch_unknown)
    {
      fprintf (stderr, "bfd_mach_o_scan: unknown architecture 0x%lx/0x%lx\n",
	       header->cputype, header->cpusubtype);
      return -1;
    }

  bfd_set_arch_mach (abfd, cputype, cpusubtype);

  if (header->ncmds != 0)
    {
      mdata->commands =
	((bfd_mach_o_load_command *)
	 bfd_alloc (abfd, header->ncmds * sizeof (bfd_mach_o_load_command)));
      if (mdata->commands == NULL)
	return -1;

      for (i = 0; i < header->ncmds; i++)
	{
	  bfd_mach_o_load_command *cur = &mdata->commands[i];

	  if (i == 0)
	    cur->offset = 28;
	  else
	    {
	      bfd_mach_o_load_command *prev = &mdata->commands[i - 1];
	      cur->offset = prev->offset + prev->len;
	    }

	  if (bfd_mach_o_scan_read_command (abfd, cur) < 0)
	    return -1;
	}
    }

  if (bfd_mach_o_scan_start_address (abfd) < 0)
    {
#if 0
      fprintf (stderr, "bfd_mach_o_scan: unable to scan start address: %s\n",
	       bfd_errmsg (bfd_get_error ()));
      abfd->tdata.mach_o_data = NULL;
      return -1;
#endif
    }

  bfd_mach_o_flatten_sections (abfd);

  return 0;
}

bfd_boolean
bfd_mach_o_mkobject (abfd)
     bfd *abfd;
{
  bfd_mach_o_data_struct *mdata = NULL;

  mdata = ((bfd_mach_o_data_struct *)
	   bfd_alloc (abfd, sizeof (bfd_mach_o_data_struct)));
  if (mdata == NULL)
    return FALSE;
  abfd->tdata.mach_o_data = mdata;

  mdata->header.magic = 0;
  mdata->header.cputype = 0;
  mdata->header.cpusubtype = 0;
  mdata->header.filetype = 0;
  mdata->header.ncmds = 0;
  mdata->header.sizeofcmds = 0;
  mdata->header.flags = 0;
  mdata->header.byteorder = BFD_ENDIAN_UNKNOWN;
  mdata->commands = NULL;
  mdata->nsymbols = 0;
  mdata->symbols = NULL;
  mdata->nsects = 0;
  mdata->sections = NULL;
  mdata->ibfd = NULL;

  return TRUE;
}

const bfd_target *
bfd_mach_o_object_p (abfd)
     bfd *abfd;
{
  struct bfd_preserve preserve;
  bfd_mach_o_header header;

  preserve.marker = NULL;
  if (bfd_mach_o_read_header (abfd, &header) != 0)
    goto wrong;

  if (! (header.byteorder == BFD_ENDIAN_BIG
	 || header.byteorder == BFD_ENDIAN_LITTLE))
    {
      fprintf (stderr, "unknown header byte-order value 0x%lx\n",
	       (long) header.byteorder);
      goto wrong;
    }

  if (! ((header.byteorder == BFD_ENDIAN_BIG
	  && abfd->xvec->byteorder == BFD_ENDIAN_BIG
	  && abfd->xvec->header_byteorder == BFD_ENDIAN_BIG)
	 || (header.byteorder == BFD_ENDIAN_LITTLE
	     && abfd->xvec->byteorder == BFD_ENDIAN_LITTLE
	     && abfd->xvec->header_byteorder == BFD_ENDIAN_LITTLE)))
    goto wrong;

  preserve.marker = bfd_zalloc (abfd, sizeof (bfd_mach_o_data_struct));
  if (preserve.marker == NULL
      || !bfd_preserve_save (abfd, &preserve))
    goto fail;

  if (bfd_mach_o_scan (abfd, &header,
		       (bfd_mach_o_data_struct *) preserve.marker) != 0)
    goto wrong;

  bfd_preserve_finish (abfd, &preserve);
  return abfd->xvec;

 wrong:
  bfd_set_error (bfd_error_wrong_format);

 fail:
  if (preserve.marker != NULL)
    bfd_preserve_restore (abfd, &preserve);
  return NULL;
}

const bfd_target *
bfd_mach_o_core_p (abfd)
     bfd *abfd;
{
  struct bfd_preserve preserve;
  bfd_mach_o_header header;

  preserve.marker = NULL;
  if (bfd_mach_o_read_header (abfd, &header) != 0)
    goto wrong;

  if (! (header.byteorder == BFD_ENDIAN_BIG
	 || header.byteorder == BFD_ENDIAN_LITTLE))
    {
      fprintf (stderr, "unknown header byte-order value 0x%lx\n",
	       (long) header.byteorder);
      abort ();
    }

  if (! ((header.byteorder == BFD_ENDIAN_BIG
	  && abfd->xvec->byteorder == BFD_ENDIAN_BIG
	  && abfd->xvec->header_byteorder == BFD_ENDIAN_BIG)
	 || (header.byteorder == BFD_ENDIAN_LITTLE
	     && abfd->xvec->byteorder == BFD_ENDIAN_LITTLE
	     && abfd->xvec->header_byteorder == BFD_ENDIAN_LITTLE)))
    goto wrong;

  if (header.filetype != BFD_MACH_O_MH_CORE)
    goto wrong;

  preserve.marker = bfd_zalloc (abfd, sizeof (bfd_mach_o_data_struct));
  if (preserve.marker == NULL
      || !bfd_preserve_save (abfd, &preserve))
    goto fail;

  if (bfd_mach_o_scan (abfd, &header,
		       (bfd_mach_o_data_struct *) preserve.marker) != 0)
    goto wrong;

  bfd_preserve_finish (abfd, &preserve);
  return abfd->xvec;

 wrong:
  bfd_set_error (bfd_error_wrong_format);

 fail:
  if (preserve.marker != NULL)
    bfd_preserve_restore (abfd, &preserve);
  return NULL;
}

typedef struct mach_o_fat_archentry
{
  unsigned long cputype;
  unsigned long cpusubtype;
  unsigned long offset;
  unsigned long size;
  unsigned long align;
  bfd *abfd;
} mach_o_fat_archentry;

typedef struct mach_o_fat_data_struct
{
  unsigned long magic;
  unsigned long nfat_arch;
  mach_o_fat_archentry *archentries;
} mach_o_fat_data_struct;

const bfd_target *
bfd_mach_o_archive_p (abfd)
     bfd *abfd;
{
  mach_o_fat_data_struct *adata = NULL;
  unsigned char buf[20];
  unsigned long i;

  bfd_seek (abfd, 0, SEEK_SET);
  if (bfd_bread ((PTR) buf, 8, abfd) != 8)
    goto error;

  adata = (mach_o_fat_data_struct *)
    bfd_alloc (abfd, sizeof (mach_o_fat_data_struct));
  if (adata == NULL)
    goto error;

  adata->magic = bfd_getb32 (buf);
  adata->nfat_arch = bfd_getb32 (buf + 4);
  if (adata->magic != 0xcafebabe)
    goto error;

  adata->archentries = (mach_o_fat_archentry *)
    bfd_alloc (abfd, adata->nfat_arch * sizeof (mach_o_fat_archentry));
  if (adata->archentries == NULL)
    goto error;

  for (i = 0; i < adata->nfat_arch; i++)
    {
      bfd_seek (abfd, 8 + 20 * i, SEEK_SET);

      if (bfd_bread ((PTR) buf, 20, abfd) != 20)
	goto error;
      adata->archentries[i].cputype = bfd_getb32 (buf);
      adata->archentries[i].cpusubtype = bfd_getb32 (buf + 4);
      adata->archentries[i].offset = bfd_getb32 (buf + 8);
      adata->archentries[i].size = bfd_getb32 (buf + 12);
      adata->archentries[i].align = bfd_getb32 (buf + 16);
      adata->archentries[i].abfd = NULL;
    }

  abfd->tdata.mach_o_fat_data = adata;
  return abfd->xvec;

 error:
  if (adata != NULL)
    bfd_release (abfd, adata);
  bfd_set_error (bfd_error_wrong_format);
  return NULL;
}

bfd *
bfd_mach_o_openr_next_archived_file (archive, prev)
  bfd *archive;
  bfd *prev;
{
  mach_o_fat_data_struct *adata;
  mach_o_fat_archentry *entry = NULL;
  unsigned long i;

  adata = (mach_o_fat_data_struct *) archive->tdata.mach_o_fat_data;
  BFD_ASSERT (adata != NULL);

  /* Find index of previous entry.  */
  if (prev == NULL)
    i = 0;	/* Start at first one.  */
  else
    {
      for (i = 0; i < adata->nfat_arch; i++)
	{
	  if (adata->archentries[i].abfd == prev)
	    break;
	}

      if (i == adata->nfat_arch)
	{
	  /* Not found.  */
	  bfd_set_error (bfd_error_bad_value);
	  return NULL;
	}
    i++;	/* Get next entry.  */
  }

  if (i >= adata->nfat_arch)
    {
      bfd_set_error (bfd_error_no_more_archived_files);
      return NULL;
    }

  entry = &adata->archentries[i];
  if (entry->abfd == NULL)
    {
      bfd *nbfd = _bfd_new_bfd_contained_in (archive);
      char *s = NULL;

      if (nbfd == NULL)
	return NULL;

      nbfd->origin = entry->offset;
      s = bfd_malloc (strlen (archive->filename) + 1);
      if (s == NULL)
	return NULL;
      strcpy (s, archive->filename);
      nbfd->filename = s;
      nbfd->iostream = NULL;
      entry->abfd = nbfd;
    }

  return entry->abfd;
}

int
bfd_mach_o_lookup_section (abfd, section, mcommand, msection)
     bfd *abfd;
     asection *section;
     bfd_mach_o_load_command **mcommand;
     bfd_mach_o_section **msection;
{
  struct mach_o_data_struct *md = abfd->tdata.mach_o_data;
  unsigned int i, j, num;

  bfd_mach_o_load_command *ncmd = NULL;
  bfd_mach_o_section *nsect = NULL;

  BFD_ASSERT (mcommand != NULL);
  BFD_ASSERT (msection != NULL);

  num = 0;
  for (i = 0; i < md->header.ncmds; i++)
    {
      struct bfd_mach_o_load_command *cmd = &md->commands[i];
      struct bfd_mach_o_segment_command *seg = NULL;

      if (cmd->type != BFD_MACH_O_LC_SEGMENT)
	continue;
      seg = &cmd->command.segment;

      if (seg->segment == section)
	{
	  if (num == 0)
	    ncmd = cmd;
	  num++;
	}

      for (j = 0; j < seg->nsects; j++)
	{
	  struct bfd_mach_o_section *sect = &seg->sections[j];

	  if (sect->bfdsection == section)
	    {
	      if (num == 0)
		nsect = sect;
	      num++;
	    }
	}
    }

  *mcommand = ncmd;
  *msection = nsect;
  return num;
}

int
bfd_mach_o_lookup_command (abfd, type, mcommand)
     bfd *abfd;
     bfd_mach_o_load_command_type type;
     bfd_mach_o_load_command **mcommand;
{
  struct mach_o_data_struct *md = NULL;
  bfd_mach_o_load_command *ncmd = NULL;
  unsigned int i, num;

  md = abfd->tdata.mach_o_data;

  BFD_ASSERT (md != NULL);
  BFD_ASSERT (mcommand != NULL);

  num = 0;
  for (i = 0; i < md->header.ncmds; i++)
    {
      struct bfd_mach_o_load_command *cmd = &md->commands[i];

      if (cmd->type != type)
	continue;

      if (num == 0)
	ncmd = cmd;
      num++;
    }

  *mcommand = ncmd;
  return num;
}

unsigned long
bfd_mach_o_stack_addr (type)
     enum bfd_mach_o_cpu_type type;
{
  switch (type)
    {
    case BFD_MACH_O_CPU_TYPE_MC680x0:
      return 0x04000000;
    case BFD_MACH_O_CPU_TYPE_MC88000:
      return 0xffffe000;
    case BFD_MACH_O_CPU_TYPE_POWERPC:
      return 0xc0000000;
    case BFD_MACH_O_CPU_TYPE_I386:
      return 0xc0000000;
    case BFD_MACH_O_CPU_TYPE_SPARC:
      return 0xf0000000;
    case BFD_MACH_O_CPU_TYPE_I860:
      return 0;
    case BFD_MACH_O_CPU_TYPE_HPPA:
      return 0xc0000000 - 0x04000000;
    default:
      return 0;
    }
}

int
bfd_mach_o_core_fetch_environment (abfd, rbuf, rlen)
     bfd *abfd;
     unsigned char **rbuf;
     unsigned int *rlen;
{
  bfd_mach_o_data_struct *mdata = abfd->tdata.mach_o_data;
  unsigned long stackaddr = bfd_mach_o_stack_addr (mdata->header.cputype);
  unsigned int i = 0;

  for (i = 0; i < mdata->header.ncmds; i++)
    {
      bfd_mach_o_load_command *cur = &mdata->commands[i];
      bfd_mach_o_segment_command *seg = NULL;

      if (cur->type != BFD_MACH_O_LC_SEGMENT)
	continue;

      seg = &cur->command.segment;

      if ((seg->vmaddr + seg->vmsize) == stackaddr)
	{
	  unsigned long start = seg->fileoff;
	  unsigned long end = seg->fileoff + seg->filesize;
	  unsigned char *buf = bfd_malloc (1024);
	  unsigned long size = 1024;

	  for (;;)
	    {
	      bfd_size_type nread = 0;
	      unsigned long offset;
	      int found_nonnull = 0;

	      if (size > (end - start))
		size = (end - start);

	      buf = bfd_realloc (buf, size);

	      bfd_seek (abfd, end - size, SEEK_SET);
	      nread = bfd_bread (buf, size, abfd);

	      if (nread != size)
		return -1;

	      for (offset = 4; offset <= size; offset += 4)
		{
		  unsigned long val;

		  val = *((unsigned long *) (buf + size - offset));
		  if (! found_nonnull)
		    {
		      if (val != 0)
			found_nonnull = 1;
		    }
		  else if (val == 0x0)
		    {
		      unsigned long bottom;
		      unsigned long top;

		      bottom = seg->fileoff + seg->filesize - offset;
		      top = seg->fileoff + seg->filesize - 4;
		      *rbuf = bfd_malloc (top - bottom);
		      *rlen = top - bottom;

		      memcpy (*rbuf, buf + size - *rlen, *rlen);
		      return 0;
		    }
		}

	      if (size == (end - start))
		break;

	      size *= 2;
	    }
	}
    }

  return -1;
}

char *
bfd_mach_o_core_file_failing_command (abfd)
     bfd *abfd;
{
  unsigned char *buf = NULL;
  unsigned int len = 0;
  int ret = -1;

  ret = bfd_mach_o_core_fetch_environment (abfd, &buf, &len);
  if (ret < 0)
    return NULL;

  return buf;
}

int
bfd_mach_o_core_file_failing_signal (abfd)
     bfd *abfd ATTRIBUTE_UNUSED;
{
  return 0;
}

bfd_boolean
bfd_mach_o_core_file_matches_executable_p (core_bfd, exec_bfd)
     bfd *core_bfd ATTRIBUTE_UNUSED;
     bfd *exec_bfd ATTRIBUTE_UNUSED;
{
  return TRUE;
}

#define TARGET_NAME mach_o_be_vec
#define TARGET_STRING "mach-o-be"
#define TARGET_BIG_ENDIAN 1
#define TARGET_ARCHIVE 0

#include "mach-o-target.c"

#undef TARGET_NAME
#undef TARGET_STRING
#undef TARGET_BIG_ENDIAN
#undef TARGET_ARCHIVE

#define TARGET_NAME mach_o_le_vec
#define TARGET_STRING "mach-o-le"
#define TARGET_BIG_ENDIAN 0
#define TARGET_ARCHIVE 0

#include "mach-o-target.c"

#undef TARGET_NAME
#undef TARGET_STRING
#undef TARGET_BIG_ENDIAN
#undef TARGET_ARCHIVE

#define TARGET_NAME mach_o_fat_vec
#define TARGET_STRING "mach-o-fat"
#define TARGET_BIG_ENDIAN 1
#define TARGET_ARCHIVE 1

#include "mach-o-target.c"

#undef TARGET_NAME
#undef TARGET_STRING
#undef TARGET_BIG_ENDIAN
#undef TARGET_ARCHIVE
