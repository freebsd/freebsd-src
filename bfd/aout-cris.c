/* BFD backend for CRIS a.out binaries.
   Copyright 2000, 2001, 2002, 2003 Free Software Foundation, Inc.
   Contributed by Axis Communications AB.
   Written by Hans-Peter Nilsson.

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

/* See info in the file PORTING for documentation of these macros and
   functions.  Beware; some of the information there is outdated.  */

#define N_HEADER_IN_TEXT(x) 0
#define N_TXTOFF(x) 32
#define ENTRY_CAN_BE_ZERO
#define TEXT_START_ADDR 0

/* Without reading symbols to get the text start symbol, there is no way
   to know where the text segment starts in an a.out file.  Defaulting to
   anything as constant as TEXT_START_ADDR is bad.  But we can guess from
   the entry point, which is usually within the first 64k of the text
   segment.  We also assume here that the text segment is 64k-aligned.
   FIXME: It is also wrong to assume that data and bss follow immediately
   after text, but with those, we don't have any choice besides reading
   symbol info, and luckily there's no pressing need for correctness for
   those vma:s at this time.  */
#define N_TXTADDR(x) ((x).a_entry & ~(bfd_vma) 0xffff)

/* If you change this to 4, you can not link to an address N*4+2.  */
#define SEGMENT_SIZE 2

/* For some reason, if the a.out file has Z_MAGIC, then
   adata(abfd).exec_bytes_size is not used, but rather
   adata(abfd).zmagic_disk_block_size, even though the exec_header is
   *not* included in the text segment.  A simple workaround is to
   #define ZMAGIC_DISK_BLOCK_SIZE, which is used if defined; otherwise
   TARGET_PAGE_SIZE is used.  */
#define ZMAGIC_DISK_BLOCK_SIZE N_TXTOFF (0)

/* It seems odd at first to set a page-size this low, but gives greater
   freedom in where things can be linked.  The drawback is that you have
   to set alignment and padding in linker scripts.  */
#define TARGET_PAGE_SIZE SEGMENT_SIZE
#define TARGETNAME "a.out-cris"

/* N_SHARED_LIB gets this reasonable default as of 1999-07-12, but we
   have to work with 2.9.1.  Note that N_SHARED_LIB is used in a
   SUN-specific context, not applicable to CRIS.  */
#define N_SHARED_LIB(x) 0

/* The definition here seems not used; just provided as a convention.  */
#define DEFAULT_ARCH bfd_arch_cris

/* Do not "beautify" the CONCAT* macro args.  Traditional C will not
   remove whitespace added here, and thus will fail to concatenate
   the tokens.  */
#define MY(OP) CONCAT2 (cris_aout_,OP)
#define NAME(x, y) CONCAT3 (cris_aout,_32_,y)

#include "bfd.h"

/* Version 1 of the header.  */
#define MY_exec_hdr_flags 1

#define MY_write_object_contents MY(write_object_contents)
static bfd_boolean MY(write_object_contents) PARAMS ((bfd *));

/* Forward this, so we can use a pointer to it in PARAMS.  */
struct reloc_ext_external;

#define MY_swap_ext_reloc_out MY(swap_ext_reloc_out)
static void MY(swap_ext_reloc_out) PARAMS ((bfd *, arelent *,
					    struct reloc_ext_external *));

#define MY_swap_ext_reloc_in MY(swap_ext_reloc_in)
static void MY(swap_ext_reloc_in) PARAMS ((bfd *, struct
					   reloc_ext_external *,
					   arelent *, asymbol **,
					   bfd_size_type));

#define MY_set_sizes MY(set_sizes)
static bfd_boolean MY(set_sizes) PARAMS ((bfd *));

/* To set back reloc_size to ext, we make MY(set_sizes) be called
   through this construct.  Note that MY_set_arch_mach is only called
   through SET_ARCH_MACH.  The default bfd_default_set_arch_mach will
   not call set_sizes.  */

#define MY_set_arch_mach NAME (aout, set_arch_mach)
#define SET_ARCH_MACH(BFD, EXEC) \
 MY_set_arch_mach (BFD, DEFAULT_ARCH, N_MACHTYPE (EXEC))

/* These macros describe the binary layout of the reloc information we
   use in a file.  */
#define RELOC_EXT_BITS_EXTERN_LITTLE 0x80
#define RELOC_EXT_BITS_TYPE_LITTLE 3
#define RELOC_EXT_BITS_TYPE_SH_LITTLE 0

#ifndef MY_get_section_contents
#define MY_get_section_contents aout_32_get_section_contents
#endif

#define MACHTYPE_OK(mtype) ((mtype) == M_CRIS)

/* Include generic functions (some are overridden above).  */
#include "aout32.c"
#include "aout-target.h"

/* We need our own version to set header flags.  */

static bfd_boolean
MY(write_object_contents) (abfd)
     bfd *abfd;
{
  struct external_exec exec_bytes;
  struct internal_exec *execp = exec_hdr (abfd);

  /* We set the reloc type to RELOC_EXT_SIZE, although setting it at all
     seems unnecessary when inspecting as and ld behavior (not an
     exhaustive inspection).  The default write_object_contents
     definition sets RELOC_EXT_SIZE, so we follow suite and set it too.  */
  obj_reloc_entry_size (abfd) = RELOC_EXT_SIZE;

  /* Setting N_SET_MACHTYPE and using N_SET_FLAGS is not performed by
     the default definition.  */
  if (bfd_get_arch(abfd) == bfd_arch_cris)
    N_SET_MACHTYPE(*execp, M_CRIS);

  N_SET_FLAGS (*execp, aout_backend_info (abfd)->exec_hdr_flags);

  WRITE_HEADERS (abfd, execp);

  return TRUE;
}

/* We need our own for these reasons:
   - Assert that a normal 8, 16 or 32 reloc is output.
   - Fix what seems to be a weak-bug (perhaps there for valid reasons).  */

static void
MY(swap_ext_reloc_out) (abfd, g, natptr)
     bfd *abfd;
     arelent *g;
     struct reloc_ext_external *natptr;
{
  int r_index;
  int r_extern;
  unsigned int r_type;
  bfd_vma r_addend;
  asymbol *sym = *(g->sym_ptr_ptr);
  asection *output_section = sym->section->output_section;

  PUT_WORD (abfd, g->address, natptr->r_address);

  r_type = (unsigned int) g->howto->type;

  r_addend = g->addend;
  if ((sym->flags & BSF_SECTION_SYM) != 0)
    r_addend += (*(g->sym_ptr_ptr))->section->output_section->vma;

  /* If this relocation is relative to a symbol then set the
     r_index to the symbols index, and the r_extern bit.

     Absolute symbols can come in in two ways, either as an offset
     from the abs section, or as a symbol which has an abs value.
     check for that here.  */

  if (bfd_is_abs_section (bfd_get_section (sym)))
    {
      r_extern = 0;
      r_index = N_ABS;
    }
  else if ((sym->flags & BSF_SECTION_SYM) == 0)
    {
      if (bfd_is_und_section (bfd_get_section (sym))
	  /* Remember to check for weak symbols; they count as global.  */
	  || (sym->flags & (BSF_GLOBAL | BSF_WEAK)) != 0)
	r_extern = 1;
      else
	r_extern = 0;
      r_index = (*(g->sym_ptr_ptr))->KEEPIT;
    }
  else
    {
      /* Just an ordinary section.  */
      r_extern = 0;
      r_index = output_section->target_index;
    }

  /* The relocation type is the same as the canonical ones, but only
     the first 3 are used: RELOC_8, RELOC_16, RELOC_32.
     We may change this later, but assert this for the moment.  */
  if (r_type > 2)
    {
      (*_bfd_error_handler) (_("%s: Invalid relocation type exported: %d"),
			     bfd_get_filename (abfd), r_type);

      bfd_set_error (bfd_error_wrong_format);
    }

  /* Now the fun stuff.  */
  natptr->r_index[2] = r_index >> 16;
  natptr->r_index[1] = r_index >> 8;
  natptr->r_index[0] = r_index;
  natptr->r_type[0] =
     (r_extern ? RELOC_EXT_BITS_EXTERN_LITTLE : 0)
      | (r_type << RELOC_EXT_BITS_TYPE_SH_LITTLE);

  PUT_WORD (abfd, r_addend, natptr->r_addend);
}

/* We need our own to assert that a normal 8, 16 or 32 reloc is input.  */

static void
MY(swap_ext_reloc_in) (abfd, bytes, cache_ptr, symbols, symcount)
     bfd *abfd;
     struct reloc_ext_external *bytes;
     arelent *cache_ptr;
     asymbol **symbols;
     bfd_size_type symcount;
{
  unsigned int r_index;
  int r_extern;
  unsigned int r_type;
  struct aoutdata *su = &(abfd->tdata.aout_data->a);

  cache_ptr->address = (GET_SWORD (abfd, bytes->r_address));

  /* Now the fun stuff.  */
  r_index =  (bytes->r_index[2] << 16)
    | (bytes->r_index[1] << 8)
    |  bytes->r_index[0];
  r_extern = (0 != (bytes->r_type[0] & RELOC_EXT_BITS_EXTERN_LITTLE));
  r_type = ((bytes->r_type[0]) >> RELOC_EXT_BITS_TYPE_SH_LITTLE)
    & RELOC_EXT_BITS_TYPE_LITTLE;

  if (r_type > 2)
    {
      (*_bfd_error_handler) (_("%s: Invalid relocation type imported: %d"),
			     bfd_archive_filename (abfd), r_type);

      bfd_set_error(bfd_error_wrong_format);
    }

  cache_ptr->howto =  howto_table_ext + r_type;

  if (r_extern && r_index > symcount)
    {
      (*_bfd_error_handler)
        (_("%s: Bad relocation record imported: %d"),
         bfd_archive_filename (abfd), r_index);

      bfd_set_error (bfd_error_wrong_format);

      /* We continue, so we can catch further errors.  */
      r_extern = 0;
      r_index = N_ABS;
    }

  /* Magically uses r_extern, symbols etc.  Ugly, but it's what's in the
     default.  */
  MOVE_ADDRESS (GET_SWORD (abfd, bytes->r_addend));
}

/* We use the same as the default, except that we also set
   "obj_reloc_entry_size (abfd) = RELOC_EXT_SIZE;", to avoid changing
   NAME (aout, set_arch_mach) in aoutx.  */

static bfd_boolean
MY(set_sizes) (abfd)
     bfd *abfd;
{
  /* Just as the default in aout-target.h (with some #ifdefs folded)...  */

  adata(abfd).page_size = TARGET_PAGE_SIZE;
  adata(abfd).segment_size = SEGMENT_SIZE;
  adata(abfd).zmagic_disk_block_size = ZMAGIC_DISK_BLOCK_SIZE;
  adata(abfd).exec_bytes_size = EXEC_BYTES_SIZE;

  /* ... except for that we have the extended reloc.  The alternative
     would be to add a check on bfd_arch_cris in NAME (aout,
     set_arch_mach) in aoutx.h, but I don't want to do that since
     target-specific things should not be added there.  */

  obj_reloc_entry_size (abfd) = RELOC_EXT_SIZE;

  return TRUE;
}

/*
 * Local variables:
 * eval: (c-set-style "gnu")
 * indent-tabs-mode: t
 * End:
 */
