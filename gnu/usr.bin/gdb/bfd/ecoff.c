/* Generic ECOFF (Extended-COFF) routines.
   Copyright 1990, 1991, 1992, 1993 Free Software Foundation, Inc.
   Original version by Per Bothner.
   Full support added by Ian Lance Taylor, ian@cygnus.com.

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
#include "libbfd.h"
#include "seclet.h"
#include "aout/ar.h"
#include "aout/ranlib.h"

/* FIXME: We need the definitions of N_SET[ADTB], but aout64.h defines
   some other stuff which we don't want and which conflicts with stuff
   we do want.  */
#include "libaout.h"
#include "aout/aout64.h"
#undef N_ABS
#undef exec_hdr
#undef obj_sym_filepos

#include "coff/internal.h"
#include "coff/sym.h"
#include "coff/symconst.h"
#include "coff/ecoff.h"
#include "libcoff.h"
#include "libecoff.h"

/* Prototypes for static functions.  */

static int ecoff_get_magic PARAMS ((bfd *abfd));
static void ecoff_set_symbol_info PARAMS ((bfd *abfd, SYMR *ecoff_sym,
					   asymbol *asym, int ext,
					   asymbol **indirect_ptr_ptr));
static void ecoff_emit_aggregate PARAMS ((bfd *abfd, char *string,
					  RNDXR *rndx, long isym,
					  CONST char *which));
static char *ecoff_type_to_string PARAMS ((bfd *abfd, union aux_ext *aux_ptr,
					   unsigned int indx, int bigendian));
static boolean ecoff_slurp_reloc_table PARAMS ((bfd *abfd, asection *section,
						asymbol **symbols));
static void ecoff_clear_output_flags PARAMS ((bfd *abfd));
static boolean ecoff_rel PARAMS ((bfd *output_bfd, bfd_seclet_type *seclet,
				  asection *output_section, PTR data,
				  boolean relocateable));
static boolean ecoff_dump_seclet PARAMS ((bfd *abfd, bfd_seclet_type *seclet,
					  asection *section, PTR data,
					  boolean relocateable));
static long ecoff_add_string PARAMS ((bfd *output_bfd, FDR *fdr,
				      CONST char *string, boolean external));
static boolean ecoff_get_debug PARAMS ((bfd *output_bfd,
					bfd_seclet_type *seclet,
					asection *section,
					boolean relocateable));
static void ecoff_compute_section_file_positions PARAMS ((bfd *abfd));
static unsigned int ecoff_armap_hash PARAMS ((CONST char *s,
					      unsigned int *rehash,
					      unsigned int size,
					      unsigned int hlog));

/* This stuff is somewhat copied from coffcode.h.  */

static asection bfd_debug_section = { "*DEBUG*" };

/* Create an ECOFF object.  */

boolean
ecoff_mkobject (abfd)
     bfd *abfd;
{
  abfd->tdata.ecoff_obj_data = ((struct ecoff_tdata *)
				bfd_zalloc (abfd, sizeof (ecoff_data_type)));
  if (abfd->tdata.ecoff_obj_data == NULL)
    {
      bfd_error = no_memory;
      return false;
    }

  /* Always create a .scommon section for every BFD.  This is a hack so
     that the linker has something to attach scSCommon symbols to.  */
  if (bfd_make_section (abfd, SCOMMON) == NULL)
    return false;

  return true;
}

/* This is a hook called by coff_real_object_p to create any backend
   specific information.  */

PTR
ecoff_mkobject_hook (abfd, filehdr, aouthdr)
     bfd *abfd;
     PTR filehdr;
     PTR aouthdr;
{
  struct internal_filehdr *internal_f = (struct internal_filehdr *) filehdr;
  struct internal_aouthdr *internal_a = (struct internal_aouthdr *) aouthdr;
  ecoff_data_type *ecoff;
  asection *regsec;

  if (ecoff_mkobject (abfd) == false)
    return NULL;

  ecoff = ecoff_data (abfd);
  ecoff->gp_size = 8;
  ecoff->sym_filepos = internal_f->f_symptr;

  /* Create the .reginfo section to give programs outside BFD a way to
     see the information stored in the a.out header.  See the comment
     in coff/ecoff.h.  */
  regsec = bfd_make_section (abfd, REGINFO);
  if (regsec == NULL)
    return NULL;

  if (internal_a != (struct internal_aouthdr *) NULL)
    {
      int i;

      ecoff->text_start = internal_a->text_start;
      ecoff->text_end = internal_a->text_start + internal_a->tsize;
      ecoff->gp = internal_a->gp_value;
      ecoff->gprmask = internal_a->gprmask;
      for (i = 0; i < 4; i++)
	ecoff->cprmask[i] = internal_a->cprmask[i];
      ecoff->fprmask = internal_a->fprmask;
      if (internal_a->magic == ECOFF_AOUT_ZMAGIC)
	abfd->flags |= D_PAGED;
    }

  /* It turns out that no special action is required by the MIPS or
     Alpha ECOFF backends.  They have different information in the
     a.out header, but we just copy it all (e.g., gprmask, cprmask and
     fprmask) and let the swapping routines ensure that only relevant
     information is written out.  */

  return (PTR) ecoff;
}

/* This is a hook needed by SCO COFF, but we have nothing to do.  */

asection *
ecoff_make_section_hook (abfd, name)
     bfd *abfd;
     char *name;
{
  return (asection *) NULL;
}

/* Initialize a new section.  */

boolean
ecoff_new_section_hook (abfd, section)
     bfd *abfd;
     asection *section;
{
  section->alignment_power = abfd->xvec->align_power_min;

  if (strcmp (section->name, _TEXT) == 0)
    section->flags |= SEC_CODE | SEC_LOAD | SEC_ALLOC;
  else if (strcmp (section->name, _DATA) == 0
	   || strcmp (section->name, _SDATA) == 0)
    section->flags |= SEC_DATA | SEC_LOAD | SEC_ALLOC;
  else if (strcmp (section->name, _RDATA) == 0
	   || strcmp (section->name, _LIT8) == 0
	   || strcmp (section->name, _LIT4) == 0)
    section->flags |= SEC_DATA | SEC_LOAD | SEC_ALLOC | SEC_READONLY;
  else if (strcmp (section->name, _BSS) == 0
	   || strcmp (section->name, _SBSS) == 0)
    section->flags |= SEC_ALLOC;
  else if (strcmp (section->name, REGINFO) == 0)
    {
      section->flags |= SEC_HAS_CONTENTS | SEC_NEVER_LOAD;
      section->_raw_size = sizeof (struct ecoff_reginfo);
    }

  /* Probably any other section name is SEC_NEVER_LOAD, but I'm
     uncertain about .init on some systems and I don't know how shared
     libraries work.  */

  return true;
}

/* Determine the machine architecture and type.  This is called from
   the generic COFF routines.  It is the inverse of ecoff_get_magic,
   below.  This could be an ECOFF backend routine, with one version
   for each target, but there aren't all that many ECOFF targets.  */

boolean
ecoff_set_arch_mach_hook (abfd, filehdr)
     bfd *abfd;
     PTR filehdr;
{
  struct internal_filehdr *internal_f = (struct internal_filehdr *) filehdr;
  enum bfd_architecture arch;
  unsigned long mach;

  switch (internal_f->f_magic)
    {
    case MIPS_MAGIC_1:
    case MIPS_MAGIC_LITTLE:
    case MIPS_MAGIC_BIG:
      arch = bfd_arch_mips;
      mach = 3000;
      break;

    case MIPS_MAGIC_LITTLE2:
    case MIPS_MAGIC_BIG2:
      /* MIPS ISA level 2: the r6000 */
      arch = bfd_arch_mips;
      mach = 6000;
      break;

    case MIPS_MAGIC_LITTLE3:
    case MIPS_MAGIC_BIG3:
      /* MIPS ISA level 3: the r4000 */
      arch = bfd_arch_mips;
      mach = 4000;
      break;

    case ALPHA_MAGIC:
      arch = bfd_arch_alpha;
      mach = 0;
      break;

    default:
      arch = bfd_arch_obscure;
      mach = 0;
      break;
    }

  return bfd_default_set_arch_mach (abfd, arch, mach);
}

/* Get the magic number to use based on the architecture and machine.
   This is the inverse of ecoff_set_arch_mach_hook, above.  */

static int
ecoff_get_magic (abfd)
     bfd *abfd;
{
  int big, little;

  switch (bfd_get_arch (abfd))
    {
    case bfd_arch_mips:
      switch (bfd_get_mach (abfd))
	{
	default:
	case 0:
	case 3000:
	  big = MIPS_MAGIC_BIG;
	  little = MIPS_MAGIC_LITTLE;
	  break;

	case 6000:
	  big = MIPS_MAGIC_BIG2;
	  little = MIPS_MAGIC_LITTLE2;
	  break;

	case 4000:
	  big = MIPS_MAGIC_BIG3;
	  little = MIPS_MAGIC_LITTLE3;
	  break;
	}

      return abfd->xvec->byteorder_big_p ? big : little;

    case bfd_arch_alpha:
      return ALPHA_MAGIC;

    default:
      abort ();
      return 0;
    }
}

/* Get the section s_flags to use for a section.  */

long
ecoff_sec_to_styp_flags (name, flags)
     CONST char *name;
     flagword flags;
{
  long styp;

  styp = 0;

  if (strcmp (name, _TEXT) == 0)
    styp = STYP_TEXT;
  else if (strcmp (name, _DATA) == 0)
    styp = STYP_DATA;
  else if (strcmp (name, _SDATA) == 0)
    styp = STYP_SDATA;
  else if (strcmp (name, _RDATA) == 0)
    styp = STYP_RDATA;
  else if (strcmp (name, _LIT8) == 0)
    styp = STYP_LIT8;
  else if (strcmp (name, _LIT4) == 0)
    styp = STYP_LIT4;
  else if (strcmp (name, _BSS) == 0)
    styp = STYP_BSS;
  else if (strcmp (name, _SBSS) == 0)
    styp = STYP_SBSS;
  else if (strcmp (name, _INIT) == 0)
    styp = STYP_ECOFF_INIT;
  else if (strcmp (name, _FINI) == 0)
    styp = STYP_ECOFF_FINI;
  else if (flags & SEC_CODE) 
    styp = STYP_TEXT;
  else if (flags & SEC_DATA) 
    styp = STYP_DATA;
  else if (flags & SEC_READONLY)
    styp = STYP_RDATA;
  else if (flags & SEC_LOAD)
    styp = STYP_REG;
  else
    styp = STYP_BSS;

  if (flags & SEC_NEVER_LOAD)
    styp |= STYP_NOLOAD;

  return styp;
}

/* Get the BFD flags to use for a section.  */

flagword
ecoff_styp_to_sec_flags (abfd, hdr)
     bfd *abfd;
     PTR hdr;
{
  struct internal_scnhdr *internal_s = (struct internal_scnhdr *) hdr;
  long styp_flags = internal_s->s_flags;
  flagword sec_flags=0;

  if (styp_flags & STYP_NOLOAD)
    sec_flags |= SEC_NEVER_LOAD;

  /* For 386 COFF, at least, an unloadable text or data section is
     actually a shared library section.  */
  if ((styp_flags & STYP_TEXT)
      || (styp_flags & STYP_ECOFF_INIT)
      || (styp_flags & STYP_ECOFF_FINI))
    {
      if (sec_flags & SEC_NEVER_LOAD)
	sec_flags |= SEC_CODE | SEC_SHARED_LIBRARY;
      else
	sec_flags |= SEC_CODE | SEC_LOAD | SEC_ALLOC;
    }
  else if ((styp_flags & STYP_DATA)
	   || (styp_flags & STYP_RDATA)
	   || (styp_flags & STYP_SDATA))
    {
      if (sec_flags & SEC_NEVER_LOAD)
	sec_flags |= SEC_DATA | SEC_SHARED_LIBRARY;
      else
	sec_flags |= SEC_DATA | SEC_LOAD | SEC_ALLOC;
      if (styp_flags & STYP_RDATA)
	sec_flags |= SEC_READONLY;
    }
  else if ((styp_flags & STYP_BSS)
	   || (styp_flags & STYP_SBSS))
    {
      sec_flags |= SEC_ALLOC;
    }
  else if (styp_flags & STYP_INFO) 
    {
      sec_flags |= SEC_NEVER_LOAD;
    }
  else if ((styp_flags & STYP_LIT8)
	   || (styp_flags & STYP_LIT4))
    {
      sec_flags |= SEC_DATA | SEC_LOAD | SEC_ALLOC | SEC_READONLY;
    }
  else
    {
      sec_flags |= SEC_ALLOC | SEC_LOAD;
    }

  return sec_flags;
}

/* Routines to swap auxiliary information in and out.  I am assuming
   that the auxiliary information format is always going to be target
   independent.  */

/* Swap in a type information record.
   BIGEND says whether AUX symbols are big-endian or little-endian; this
   info comes from the file header record (fh-fBigendian).  */

void
ecoff_swap_tir_in (bigend, ext_copy, intern)
     int bigend;
     struct tir_ext *ext_copy;
     TIR *intern;
{
  struct tir_ext ext[1];

  *ext = *ext_copy;		/* Make it reasonable to do in-place.  */
  
  /* now the fun stuff... */
  if (bigend) {
    intern->fBitfield   = 0 != (ext->t_bits1[0] & TIR_BITS1_FBITFIELD_BIG);
    intern->continued   = 0 != (ext->t_bits1[0] & TIR_BITS1_CONTINUED_BIG);
    intern->bt          = (ext->t_bits1[0] & TIR_BITS1_BT_BIG)
			>>		    TIR_BITS1_BT_SH_BIG;
    intern->tq4         = (ext->t_tq45[0] & TIR_BITS_TQ4_BIG)
			>>		    TIR_BITS_TQ4_SH_BIG;
    intern->tq5         = (ext->t_tq45[0] & TIR_BITS_TQ5_BIG)
			>>		    TIR_BITS_TQ5_SH_BIG;
    intern->tq0         = (ext->t_tq01[0] & TIR_BITS_TQ0_BIG)
			>>		    TIR_BITS_TQ0_SH_BIG;
    intern->tq1         = (ext->t_tq01[0] & TIR_BITS_TQ1_BIG)
			>>		    TIR_BITS_TQ1_SH_BIG;
    intern->tq2         = (ext->t_tq23[0] & TIR_BITS_TQ2_BIG)
			>>		    TIR_BITS_TQ2_SH_BIG;
    intern->tq3         = (ext->t_tq23[0] & TIR_BITS_TQ3_BIG)
			>>		    TIR_BITS_TQ3_SH_BIG;
  } else {
    intern->fBitfield   = 0 != (ext->t_bits1[0] & TIR_BITS1_FBITFIELD_LITTLE);
    intern->continued   = 0 != (ext->t_bits1[0] & TIR_BITS1_CONTINUED_LITTLE);
    intern->bt          = (ext->t_bits1[0] & TIR_BITS1_BT_LITTLE)
			>>		    TIR_BITS1_BT_SH_LITTLE;
    intern->tq4         = (ext->t_tq45[0] & TIR_BITS_TQ4_LITTLE)
			>>		    TIR_BITS_TQ4_SH_LITTLE;
    intern->tq5         = (ext->t_tq45[0] & TIR_BITS_TQ5_LITTLE)
			>>		    TIR_BITS_TQ5_SH_LITTLE;
    intern->tq0         = (ext->t_tq01[0] & TIR_BITS_TQ0_LITTLE)
			>>		    TIR_BITS_TQ0_SH_LITTLE;
    intern->tq1         = (ext->t_tq01[0] & TIR_BITS_TQ1_LITTLE)
			>>		    TIR_BITS_TQ1_SH_LITTLE;
    intern->tq2         = (ext->t_tq23[0] & TIR_BITS_TQ2_LITTLE)
			>>		    TIR_BITS_TQ2_SH_LITTLE;
    intern->tq3         = (ext->t_tq23[0] & TIR_BITS_TQ3_LITTLE)
			>>		    TIR_BITS_TQ3_SH_LITTLE;
  }

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort();
#endif
}

/* Swap out a type information record.
   BIGEND says whether AUX symbols are big-endian or little-endian; this
   info comes from the file header record (fh-fBigendian).  */

void
ecoff_swap_tir_out (bigend, intern_copy, ext)
     int bigend;
     TIR *intern_copy;
     struct tir_ext *ext;
{
  TIR intern[1];

  *intern = *intern_copy;	/* Make it reasonable to do in-place.  */
  
  /* now the fun stuff... */
  if (bigend) {
    ext->t_bits1[0] = ((intern->fBitfield ? TIR_BITS1_FBITFIELD_BIG : 0)
		       | (intern->continued ? TIR_BITS1_CONTINUED_BIG : 0)
		       | ((intern->bt << TIR_BITS1_BT_SH_BIG)
			  & TIR_BITS1_BT_BIG));
    ext->t_tq45[0] = (((intern->tq4 << TIR_BITS_TQ4_SH_BIG)
		       & TIR_BITS_TQ4_BIG)
		      | ((intern->tq5 << TIR_BITS_TQ5_SH_BIG)
			 & TIR_BITS_TQ5_BIG));
    ext->t_tq01[0] = (((intern->tq0 << TIR_BITS_TQ0_SH_BIG)
		       & TIR_BITS_TQ0_BIG)
		      | ((intern->tq1 << TIR_BITS_TQ1_SH_BIG)
			 & TIR_BITS_TQ1_BIG));
    ext->t_tq23[0] = (((intern->tq2 << TIR_BITS_TQ2_SH_BIG)
		       & TIR_BITS_TQ2_BIG)
		      | ((intern->tq3 << TIR_BITS_TQ3_SH_BIG)
			 & TIR_BITS_TQ3_BIG));
  } else {
    ext->t_bits1[0] = ((intern->fBitfield ? TIR_BITS1_FBITFIELD_LITTLE : 0)
		       | (intern->continued ? TIR_BITS1_CONTINUED_LITTLE : 0)
		       | ((intern->bt << TIR_BITS1_BT_SH_LITTLE)
			  & TIR_BITS1_BT_LITTLE));
    ext->t_tq45[0] = (((intern->tq4 << TIR_BITS_TQ4_SH_LITTLE)
		       & TIR_BITS_TQ4_LITTLE)
		      | ((intern->tq5 << TIR_BITS_TQ5_SH_LITTLE)
			 & TIR_BITS_TQ5_LITTLE));
    ext->t_tq01[0] = (((intern->tq0 << TIR_BITS_TQ0_SH_LITTLE)
		       & TIR_BITS_TQ0_LITTLE)
		      | ((intern->tq1 << TIR_BITS_TQ1_SH_LITTLE)
			 & TIR_BITS_TQ1_LITTLE));
    ext->t_tq23[0] = (((intern->tq2 << TIR_BITS_TQ2_SH_LITTLE)
		       & TIR_BITS_TQ2_LITTLE)
		      | ((intern->tq3 << TIR_BITS_TQ3_SH_LITTLE)
			 & TIR_BITS_TQ3_LITTLE));
  }

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort();
#endif
}

/* Swap in a relative symbol record.  BIGEND says whether it is in
   big-endian or little-endian format.*/

void
ecoff_swap_rndx_in (bigend, ext_copy, intern)
     int bigend;
     struct rndx_ext *ext_copy;
     RNDXR *intern;
{
  struct rndx_ext ext[1];

  *ext = *ext_copy;		/* Make it reasonable to do in-place.  */
  
  /* now the fun stuff... */
  if (bigend) {
    intern->rfd   = (ext->r_bits[0] << RNDX_BITS0_RFD_SH_LEFT_BIG)
		  | ((ext->r_bits[1] & RNDX_BITS1_RFD_BIG)
		    		    >> RNDX_BITS1_RFD_SH_BIG);
    intern->index = ((ext->r_bits[1] & RNDX_BITS1_INDEX_BIG)
		    		    << RNDX_BITS1_INDEX_SH_LEFT_BIG)
		  | (ext->r_bits[2] << RNDX_BITS2_INDEX_SH_LEFT_BIG)
		  | (ext->r_bits[3] << RNDX_BITS3_INDEX_SH_LEFT_BIG);
  } else {
    intern->rfd   = (ext->r_bits[0] << RNDX_BITS0_RFD_SH_LEFT_LITTLE)
		  | ((ext->r_bits[1] & RNDX_BITS1_RFD_LITTLE)
		    		    << RNDX_BITS1_RFD_SH_LEFT_LITTLE);
    intern->index = ((ext->r_bits[1] & RNDX_BITS1_INDEX_LITTLE)
		    		    >> RNDX_BITS1_INDEX_SH_LITTLE)
		  | (ext->r_bits[2] << RNDX_BITS2_INDEX_SH_LEFT_LITTLE)
		  | (ext->r_bits[3] << RNDX_BITS3_INDEX_SH_LEFT_LITTLE);
  }

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort();
#endif
}

/* Swap out a relative symbol record.  BIGEND says whether it is in
   big-endian or little-endian format.*/

void
ecoff_swap_rndx_out (bigend, intern_copy, ext)
     int bigend;
     RNDXR *intern_copy;
     struct rndx_ext *ext;
{
  RNDXR intern[1];

  *intern = *intern_copy;	/* Make it reasonable to do in-place.  */
  
  /* now the fun stuff... */
  if (bigend) {
    ext->r_bits[0] = intern->rfd >> RNDX_BITS0_RFD_SH_LEFT_BIG;
    ext->r_bits[1] = (((intern->rfd << RNDX_BITS1_RFD_SH_BIG)
		       & RNDX_BITS1_RFD_BIG)
		      | ((intern->index >> RNDX_BITS1_INDEX_SH_LEFT_BIG)
			 & RNDX_BITS1_INDEX_BIG));
    ext->r_bits[2] = intern->index >> RNDX_BITS2_INDEX_SH_LEFT_BIG;
    ext->r_bits[3] = intern->index >> RNDX_BITS3_INDEX_SH_LEFT_BIG;
  } else {
    ext->r_bits[0] = intern->rfd >> RNDX_BITS0_RFD_SH_LEFT_LITTLE;
    ext->r_bits[1] = (((intern->rfd >> RNDX_BITS1_RFD_SH_LEFT_LITTLE)
		       & RNDX_BITS1_RFD_LITTLE)
		      | ((intern->index << RNDX_BITS1_INDEX_SH_LITTLE)
			 & RNDX_BITS1_INDEX_LITTLE));
    ext->r_bits[2] = intern->index >> RNDX_BITS2_INDEX_SH_LEFT_LITTLE;
    ext->r_bits[3] = intern->index >> RNDX_BITS3_INDEX_SH_LEFT_LITTLE;
  }

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort();
#endif
}

/* Read in and swap the important symbolic information for an ECOFF
   object file.  This is called by gdb.  */

boolean
ecoff_slurp_symbolic_info (abfd)
     bfd *abfd;
{
  const struct ecoff_backend_data * const backend = ecoff_backend (abfd);
  bfd_size_type external_hdr_size;
  HDRR *internal_symhdr;
  bfd_size_type raw_base;
  bfd_size_type raw_size;
  PTR raw;
  bfd_size_type external_fdr_size;
  char *fraw_src;
  char *fraw_end;
  struct fdr *fdr_ptr;
  bfd_size_type raw_end;
  bfd_size_type cb_end;

  /* Check whether we've already gotten it, and whether there's any to
     get.  */
  if (ecoff_data (abfd)->raw_syments != (PTR) NULL)
    return true;
  if (ecoff_data (abfd)->sym_filepos == 0)
    {
      bfd_get_symcount (abfd) = 0;
      return true;
    }

  /* At this point bfd_get_symcount (abfd) holds the number of symbols
     as read from the file header, but on ECOFF this is always the
     size of the symbolic information header.  It would be cleaner to
     handle this when we first read the file in coffgen.c.  */
  external_hdr_size = backend->external_hdr_size;
  if (bfd_get_symcount (abfd) != external_hdr_size)
    {
      bfd_error = bad_value;
      return false;
    }

  /* Read the symbolic information header.  */
  raw = (PTR) alloca (external_hdr_size);
  if (bfd_seek (abfd, ecoff_data (abfd)->sym_filepos, SEEK_SET) == -1
      || (bfd_read (raw, external_hdr_size, 1, abfd)
	  != external_hdr_size))
    {
      bfd_error = system_call_error;
      return false;
    }
  internal_symhdr = &ecoff_data (abfd)->symbolic_header;
  (*backend->swap_hdr_in) (abfd, raw, internal_symhdr);

  if (internal_symhdr->magic != backend->sym_magic)
    {
      bfd_error = bad_value;
      return false;
    }

  /* Now we can get the correct number of symbols.  */
  bfd_get_symcount (abfd) = (internal_symhdr->isymMax
			     + internal_symhdr->iextMax);

  /* Read all the symbolic information at once.  */
  raw_base = ecoff_data (abfd)->sym_filepos + external_hdr_size;

  /* Alpha ecoff makes the determination of raw_size difficult. It has
     an undocumented debug data section between the symhdr and the first
     documented section. And the ordering of the sections varies between
     statically and dynamically linked executables.
     If bfd supports SEEK_END someday, this code could be simplified.  */

  raw_end = 0;

#define UPDATE_RAW_END(start, count, size) \
  cb_end = internal_symhdr->start + internal_symhdr->count * (size); \
  if (cb_end > raw_end) \
    raw_end = cb_end

  UPDATE_RAW_END (cbLineOffset, cbLine, sizeof (unsigned char));
  UPDATE_RAW_END (cbDnOffset, idnMax, backend->external_dnr_size);
  UPDATE_RAW_END (cbPdOffset, ipdMax, backend->external_pdr_size);
  UPDATE_RAW_END (cbSymOffset, isymMax, backend->external_sym_size);
  UPDATE_RAW_END (cbOptOffset, ioptMax, backend->external_opt_size);
  UPDATE_RAW_END (cbAuxOffset, iauxMax, sizeof (union aux_ext));
  UPDATE_RAW_END (cbSsOffset, issMax, sizeof (char));
  UPDATE_RAW_END (cbSsExtOffset, issExtMax, sizeof (char));
  UPDATE_RAW_END (cbFdOffset, ifdMax, backend->external_fdr_size);
  UPDATE_RAW_END (cbRfdOffset, crfd, backend->external_rfd_size);
  UPDATE_RAW_END (cbExtOffset, iextMax, backend->external_ext_size);

#undef UPDATE_RAW_END

  raw_size = raw_end - raw_base;
  if (raw_size == 0)
    {
      ecoff_data (abfd)->sym_filepos = 0;
      return true;
    }
  raw = (PTR) bfd_alloc (abfd, raw_size);
  if (raw == NULL)
    {
      bfd_error = no_memory;
      return false;
    }
  if (bfd_read (raw, raw_size, 1, abfd) != raw_size)
    {
      bfd_error = system_call_error;
      bfd_release (abfd, raw);
      return false;
    }

  ecoff_data (abfd)->raw_size = raw_size;
  ecoff_data (abfd)->raw_syments = raw;

  /* Get pointers for the numeric offsets in the HDRR structure.  */
#define FIX(off1, off2, type) \
  if (internal_symhdr->off1 == 0) \
    ecoff_data (abfd)->off2 = (type) NULL; \
  else \
    ecoff_data (abfd)->off2 = (type) ((char *) raw \
				      + internal_symhdr->off1 \
				      - raw_base)
  FIX (cbLineOffset, line, unsigned char *);
  FIX (cbDnOffset, external_dnr, PTR);
  FIX (cbPdOffset, external_pdr, PTR);
  FIX (cbSymOffset, external_sym, PTR);
  FIX (cbOptOffset, external_opt, PTR);
  FIX (cbAuxOffset, external_aux, union aux_ext *);
  FIX (cbSsOffset, ss, char *);
  FIX (cbSsExtOffset, ssext, char *);
  FIX (cbFdOffset, external_fdr, PTR);
  FIX (cbRfdOffset, external_rfd, PTR);
  FIX (cbExtOffset, external_ext, PTR);
#undef FIX

  /* I don't want to always swap all the data, because it will just
     waste time and most programs will never look at it.  The only
     time the linker needs most of the debugging information swapped
     is when linking big-endian and little-endian MIPS object files
     together, which is not a common occurrence.

     We need to look at the fdr to deal with a lot of information in
     the symbols, so we swap them here.  */
  ecoff_data (abfd)->fdr =
    (struct fdr *) bfd_alloc (abfd,
			      (internal_symhdr->ifdMax *
			       sizeof (struct fdr)));
  if (ecoff_data (abfd)->fdr == NULL)
    {
      bfd_error = no_memory;
      return false;
    }
  external_fdr_size = backend->external_fdr_size;
  fdr_ptr = ecoff_data (abfd)->fdr;
  fraw_src = (char *) ecoff_data (abfd)->external_fdr;
  fraw_end = fraw_src + internal_symhdr->ifdMax * external_fdr_size;
  for (; fraw_src < fraw_end; fraw_src += external_fdr_size, fdr_ptr++)
    (*backend->swap_fdr_in) (abfd, (PTR) fraw_src, fdr_ptr);

  return true;
}

/* ECOFF symbol table routines.  The ECOFF symbol table is described
   in gcc/mips-tfile.c.  */

/* ECOFF uses two common sections.  One is the usual one, and the
   other is for small objects.  All the small objects are kept
   together, and then referenced via the gp pointer, which yields
   faster assembler code.  This is what we use for the small common
   section.  */
static asection ecoff_scom_section;
static asymbol ecoff_scom_symbol;
static asymbol *ecoff_scom_symbol_ptr;

/* Create an empty symbol.  */

asymbol *
ecoff_make_empty_symbol (abfd)
     bfd *abfd;
{
  ecoff_symbol_type *new;

  new = (ecoff_symbol_type *) bfd_alloc (abfd, sizeof (ecoff_symbol_type));
  if (new == (ecoff_symbol_type *) NULL)
    {
      bfd_error = no_memory;
      return (asymbol *) NULL;
    }
  memset (new, 0, sizeof *new);
  new->symbol.section = (asection *) NULL;
  new->fdr = (FDR *) NULL;
  new->local = false;
  new->native = NULL;
  new->symbol.the_bfd = abfd;
  return &new->symbol;
}

/* Set the BFD flags and section for an ECOFF symbol.  */

static void
ecoff_set_symbol_info (abfd, ecoff_sym, asym, ext, indirect_ptr_ptr)
     bfd *abfd;
     SYMR *ecoff_sym;
     asymbol *asym;
     int ext;
     asymbol **indirect_ptr_ptr;
{
  asym->the_bfd = abfd;
  asym->value = ecoff_sym->value;
  asym->section = &bfd_debug_section;
  asym->udata = NULL;

  /* An indirect symbol requires two consecutive stabs symbols.  */
  if (*indirect_ptr_ptr != (asymbol *) NULL)
    {
      BFD_ASSERT (ECOFF_IS_STAB (ecoff_sym));

      /* @@ Stuffing pointers into integers is a no-no.
	 We can usually get away with it if the integer is
	 large enough though.  */
      if (sizeof (asym) > sizeof (bfd_vma))
	abort ();
      (*indirect_ptr_ptr)->value = (bfd_vma) asym;

      asym->flags = BSF_DEBUGGING;
      asym->section = &bfd_und_section;
      *indirect_ptr_ptr = NULL;
      return;
    }

  if (ECOFF_IS_STAB (ecoff_sym)
      && (ECOFF_UNMARK_STAB (ecoff_sym->index) | N_EXT) == (N_INDR | N_EXT))
    {
      asym->flags = BSF_DEBUGGING | BSF_INDIRECT;
      asym->section = &bfd_ind_section;
      /* Pass this symbol on to the next call to this function.  */
      *indirect_ptr_ptr = asym;
      return;
    }

  /* Most symbol types are just for debugging.  */
  switch (ecoff_sym->st)
    {
    case stGlobal:
    case stStatic:
    case stLabel:
    case stProc:
    case stStaticProc:
      break;
    case stNil:
      if (ECOFF_IS_STAB (ecoff_sym))
	{
	  asym->flags = BSF_DEBUGGING;
	  return;
	}
      break;
    default:
      asym->flags = BSF_DEBUGGING;
      return;
    }

  if (ext)
    asym->flags = BSF_EXPORT | BSF_GLOBAL;
  else
    asym->flags = BSF_LOCAL;
  switch (ecoff_sym->sc)
    {
    case scNil:
      /* Used for compiler generated labels.  Leave them in the
	 debugging section, and mark them as local.  If BSF_DEBUGGING
	 is set, then nm does not display them for some reason.  If no
	 flags are set then the linker whines about them.  */
      asym->flags = BSF_LOCAL;
      break;
    case scText:
      asym->section = bfd_make_section_old_way (abfd, ".text");
      asym->value -= asym->section->vma;
      break;
    case scData:
      asym->section = bfd_make_section_old_way (abfd, ".data");
      asym->value -= asym->section->vma;
      break;
    case scBss:
      asym->section = bfd_make_section_old_way (abfd, ".bss");
      asym->value -= asym->section->vma;
      break;
    case scRegister:
      asym->flags = BSF_DEBUGGING;
      break;
    case scAbs:
      asym->section = &bfd_abs_section;
      break;
    case scUndefined:
      asym->section = &bfd_und_section;
      asym->flags = 0;
      asym->value = 0;
      break;
    case scCdbLocal:
    case scBits:
    case scCdbSystem:
    case scRegImage:
    case scInfo:
    case scUserStruct:
      asym->flags = BSF_DEBUGGING;
      break;
    case scSData:
      asym->section = bfd_make_section_old_way (abfd, ".sdata");
      asym->value -= asym->section->vma;
      break;
    case scSBss:
      asym->section = bfd_make_section_old_way (abfd, ".sbss");
      asym->value -= asym->section->vma;
      break;
    case scRData:
      asym->section = bfd_make_section_old_way (abfd, ".rdata");
      asym->value -= asym->section->vma;
      break;
    case scVar:
      asym->flags = BSF_DEBUGGING;
      break;
    case scCommon:
      if (asym->value > ecoff_data (abfd)->gp_size)
	{
	  asym->section = &bfd_com_section;
	  asym->flags = 0;
	  break;
	}
      /* Fall through.  */
    case scSCommon:
      if (ecoff_scom_section.name == NULL)
	{
	  /* Initialize the small common section.  */
	  ecoff_scom_section.name = SCOMMON;
	  ecoff_scom_section.flags = SEC_IS_COMMON;
	  ecoff_scom_section.output_section = &ecoff_scom_section;
	  ecoff_scom_section.symbol = &ecoff_scom_symbol;
	  ecoff_scom_section.symbol_ptr_ptr = &ecoff_scom_symbol_ptr;
	  ecoff_scom_symbol.name = SCOMMON;
	  ecoff_scom_symbol.flags = BSF_SECTION_SYM;
	  ecoff_scom_symbol.section = &ecoff_scom_section;
	  ecoff_scom_symbol_ptr = &ecoff_scom_symbol;
	}
      asym->section = &ecoff_scom_section;
      asym->flags = 0;
      break;
    case scVarRegister:
    case scVariant:
      asym->flags = BSF_DEBUGGING;
      break;
    case scSUndefined:
      asym->section = &bfd_und_section;
      asym->flags = 0;
      asym->value = 0;
      break;
    case scInit:
      asym->section = bfd_make_section_old_way (abfd, ".init");
      asym->value -= asym->section->vma;
      break;
    case scBasedVar:
    case scXData:
    case scPData:
      asym->flags = BSF_DEBUGGING;
      break;
    case scFini:
      asym->section = bfd_make_section_old_way (abfd, ".fini");
      asym->value -= asym->section->vma;
      break;
    default:
      break;
    }

  /* Look for special constructors symbols and make relocation entries
     in a special construction section.  These are produced by the
     -fgnu-linker argument to g++.  */
  if (ECOFF_IS_STAB (ecoff_sym))
    {
      switch (ECOFF_UNMARK_STAB (ecoff_sym->index))
	{
	default:
	  break;

	case N_SETA:
	case N_SETT:
	case N_SETD:
	case N_SETB:
	  {
	    const char *name;
	    asection *section;
	    arelent_chain *reloc_chain;
	    unsigned int bitsize;

	    /* Get a section with the same name as the symbol (usually
	       __CTOR_LIST__ or __DTOR_LIST__).  FIXME: gcc uses the
	       name ___CTOR_LIST (three underscores).  We need
	       __CTOR_LIST (two underscores), since ECOFF doesn't use
	       a leading underscore.  This should be handled by gcc,
	       but instead we do it here.  Actually, this should all
	       be done differently anyhow.  */
	    name = bfd_asymbol_name (asym);
	    if (name[0] == '_' && name[1] == '_' && name[2] == '_')
	      {
		++name;
		asym->name = name;
	      }
	    section = bfd_get_section_by_name (abfd, name);
	    if (section == (asection *) NULL)
	      {
		char *copy;

		copy = (char *) bfd_alloc (abfd, strlen (name) + 1);
		strcpy (copy, name);
		section = bfd_make_section (abfd, copy);
	      }

	    /* Build a reloc pointing to this constructor.  */
	    reloc_chain =
	      (arelent_chain *) bfd_alloc (abfd, sizeof (arelent_chain));
	    reloc_chain->relent.sym_ptr_ptr =
	      bfd_get_section (asym)->symbol_ptr_ptr;
	    reloc_chain->relent.address = section->_raw_size;
	    reloc_chain->relent.addend = asym->value;
	    reloc_chain->relent.howto =
	      ecoff_backend (abfd)->constructor_reloc;

	    /* Set up the constructor section to hold the reloc.  */
	    section->flags = SEC_CONSTRUCTOR;
	    ++section->reloc_count;

	    /* Constructor sections must be rounded to a boundary
	       based on the bitsize.  These are not real sections--
	       they are handled specially by the linker--so the ECOFF
	       16 byte alignment restriction does not apply.  */
	    bitsize = ecoff_backend (abfd)->constructor_bitsize;
	    section->alignment_power = 1;
	    while ((1 << section->alignment_power) < bitsize / 8)
	      ++section->alignment_power;

	    reloc_chain->next = section->constructor_chain;
	    section->constructor_chain = reloc_chain;
	    section->_raw_size += bitsize / 8;

	    /* Mark the symbol as a constructor.  */
	    asym->flags |= BSF_CONSTRUCTOR;
	  }
	  break;
	}
    }
}

/* Read an ECOFF symbol table.  */

boolean
ecoff_slurp_symbol_table (abfd)
     bfd *abfd;
{
  const struct ecoff_backend_data * const backend = ecoff_backend (abfd);
  const bfd_size_type external_ext_size = backend->external_ext_size;
  const bfd_size_type external_sym_size = backend->external_sym_size;
  void (* const swap_ext_in) PARAMS ((bfd *, PTR, EXTR *))
    = backend->swap_ext_in;
  void (* const swap_sym_in) PARAMS ((bfd *, PTR, SYMR *))
    = backend->swap_sym_in;
  bfd_size_type internal_size;
  ecoff_symbol_type *internal;
  ecoff_symbol_type *internal_ptr;
  asymbol *indirect_ptr;
  char *eraw_src;
  char *eraw_end;
  FDR *fdr_ptr;
  FDR *fdr_end;

  /* If we've already read in the symbol table, do nothing.  */
  if (ecoff_data (abfd)->canonical_symbols != NULL)
    return true;

  /* Get the symbolic information.  */
  if (ecoff_slurp_symbolic_info (abfd) == false)
    return false;
  if (bfd_get_symcount (abfd) == 0)
    return true;

  internal_size = bfd_get_symcount (abfd) * sizeof (ecoff_symbol_type);
  internal = (ecoff_symbol_type *) bfd_alloc (abfd, internal_size);
  if (internal == NULL)
    {
      bfd_error = no_memory;
      return false;
    }

  internal_ptr = internal;
  indirect_ptr = NULL;
  eraw_src = (char *) ecoff_data (abfd)->external_ext;
  eraw_end = (eraw_src
	      + (ecoff_data (abfd)->symbolic_header.iextMax
		 * external_ext_size));
  for (; eraw_src < eraw_end; eraw_src += external_ext_size, internal_ptr++)
    {
      EXTR internal_esym;

      (*swap_ext_in) (abfd, (PTR) eraw_src, &internal_esym);
      internal_ptr->symbol.name = (ecoff_data (abfd)->ssext
				   + internal_esym.asym.iss);
      ecoff_set_symbol_info (abfd, &internal_esym.asym,
			     &internal_ptr->symbol, 1, &indirect_ptr);
      /* The alpha uses a negative ifd field for section symbols.  */
      if (internal_esym.ifd >= 0)
	internal_ptr->fdr = ecoff_data (abfd)->fdr + internal_esym.ifd;
      else
	internal_ptr->fdr = NULL;
      internal_ptr->local = false;
      internal_ptr->native = (PTR) eraw_src;
    }
  BFD_ASSERT (indirect_ptr == (asymbol *) NULL);

  /* The local symbols must be accessed via the fdr's, because the
     string and aux indices are relative to the fdr information.  */
  fdr_ptr = ecoff_data (abfd)->fdr;
  fdr_end = fdr_ptr + ecoff_data (abfd)->symbolic_header.ifdMax;
  for (; fdr_ptr < fdr_end; fdr_ptr++)
    {
      char *lraw_src;
      char *lraw_end;

      lraw_src = ((char *) ecoff_data (abfd)->external_sym
		  + fdr_ptr->isymBase * external_sym_size);
      lraw_end = lraw_src + fdr_ptr->csym * external_sym_size;
      for (;
	   lraw_src < lraw_end;
	   lraw_src += external_sym_size, internal_ptr++)
	{
	  SYMR internal_sym;

	  (*swap_sym_in) (abfd, (PTR) lraw_src, &internal_sym);
	  internal_ptr->symbol.name = (ecoff_data (abfd)->ss
				       + fdr_ptr->issBase
				       + internal_sym.iss);
	  ecoff_set_symbol_info (abfd, &internal_sym,
				 &internal_ptr->symbol, 0, &indirect_ptr);
	  internal_ptr->fdr = fdr_ptr;
	  internal_ptr->local = true;
	  internal_ptr->native = (PTR) lraw_src;
	}
    }
  BFD_ASSERT (indirect_ptr == (asymbol *) NULL);

  ecoff_data (abfd)->canonical_symbols = internal;

  return true;
}

/* Return the amount of space needed for the canonical symbols.  */

unsigned int
ecoff_get_symtab_upper_bound (abfd)
     bfd *abfd;
{
  if (ecoff_slurp_symbolic_info (abfd) == false
      || bfd_get_symcount (abfd) == 0)
    return 0;

  return (bfd_get_symcount (abfd) + 1) * (sizeof (ecoff_symbol_type *));
}

/* Get the canonicals symbols.  */

unsigned int
ecoff_get_symtab (abfd, alocation)
     bfd *abfd;
     asymbol **alocation;
{
  unsigned int counter = 0;
  ecoff_symbol_type *symbase;
  ecoff_symbol_type **location = (ecoff_symbol_type **) alocation;

  if (ecoff_slurp_symbol_table (abfd) == false
      || bfd_get_symcount (abfd) == 0)
    return 0;

  symbase = ecoff_data (abfd)->canonical_symbols;
  while (counter < bfd_get_symcount (abfd))
    {
      *(location++) = symbase++;
      counter++;
    }
  *location++ = (ecoff_symbol_type *) NULL;
  return bfd_get_symcount (abfd);
}

/* Turn ECOFF type information into a printable string.
   ecoff_emit_aggregate and ecoff_type_to_string are from
   gcc/mips-tdump.c, with swapping added and used_ptr removed.  */

/* Write aggregate information to a string.  */

static void
ecoff_emit_aggregate (abfd, string, rndx, isym, which)
     bfd *abfd;
     char *string;
     RNDXR *rndx;
     long isym;
     CONST char *which;
{
  int ifd = rndx->rfd;
  int indx = rndx->index;
  int sym_base, ss_base;
  CONST char *name;
  
  if (ifd == 0xfff)
    ifd = isym;

  sym_base = ecoff_data (abfd)->fdr[ifd].isymBase;
  ss_base  = ecoff_data (abfd)->fdr[ifd].issBase;
  
  if (indx == indexNil)
    name = "/* no name */";
  else
    {
      const struct ecoff_backend_data * const backend = ecoff_backend (abfd);
      SYMR sym;

      indx += sym_base;
      (*backend->swap_sym_in) (abfd,
			       ((char *) ecoff_data (abfd)->external_sym
				+ indx * backend->external_sym_size),
			       &sym);
      name = ecoff_data (abfd)->ss + ss_base + sym.iss;
    }

  sprintf (string,
	   "%s %s { ifd = %d, index = %d }",
	   which, name, ifd,
	   indx + ecoff_data (abfd)->symbolic_header.iextMax);
}

/* Convert the type information to string format.  */

static char *
ecoff_type_to_string (abfd, aux_ptr, indx, bigendian)
     bfd *abfd;
     union aux_ext *aux_ptr;
     unsigned int indx;
     int bigendian;
{
  AUXU u;
  struct qual {
    unsigned int  type;
    int  low_bound;
    int  high_bound;
    int  stride;
  } qualifiers[7];

  unsigned int basic_type;
  int i;
  static char buffer1[1024];
  static char buffer2[1024];
  char *p1 = buffer1;
  char *p2 = buffer2;
  RNDXR rndx;

  for (i = 0; i < 7; i++)
    {
      qualifiers[i].low_bound = 0;
      qualifiers[i].high_bound = 0;
      qualifiers[i].stride = 0;
    }

  if (AUX_GET_ISYM (bigendian, &aux_ptr[indx]) == -1)
    return "-1 (no type)";
  ecoff_swap_tir_in (bigendian, &aux_ptr[indx++].a_ti, &u.ti);

  basic_type = u.ti.bt;
  qualifiers[0].type = u.ti.tq0;
  qualifiers[1].type = u.ti.tq1;
  qualifiers[2].type = u.ti.tq2;
  qualifiers[3].type = u.ti.tq3;
  qualifiers[4].type = u.ti.tq4;
  qualifiers[5].type = u.ti.tq5;
  qualifiers[6].type = tqNil;

  /*
   * Go get the basic type.
   */
  switch (basic_type)
    {
    case btNil:			/* undefined */
      strcpy (p1, "nil");
      break;

    case btAdr:			/* address - integer same size as pointer */
      strcpy (p1, "address");
      break;

    case btChar:		/* character */
      strcpy (p1, "char");
      break;

    case btUChar:		/* unsigned character */
      strcpy (p1, "unsigned char");
      break;

    case btShort:		/* short */
      strcpy (p1, "short");
      break;

    case btUShort:		/* unsigned short */
      strcpy (p1, "unsigned short");
      break;

    case btInt:			/* int */
      strcpy (p1, "int");
      break;

    case btUInt:		/* unsigned int */
      strcpy (p1, "unsigned int");
      break;

    case btLong:		/* long */
      strcpy (p1, "long");
      break;

    case btULong:		/* unsigned long */
      strcpy (p1, "unsigned long");
      break;

    case btFloat:		/* float (real) */
      strcpy (p1, "float");
      break;

    case btDouble:		/* Double (real) */
      strcpy (p1, "double");
      break;

      /* Structures add 1-2 aux words:
	 1st word is [ST_RFDESCAPE, offset] pointer to struct def;
	 2nd word is file index if 1st word rfd is ST_RFDESCAPE.  */

    case btStruct:		/* Structure (Record) */
      ecoff_swap_rndx_in (bigendian, &aux_ptr[indx].a_rndx, &rndx);
      ecoff_emit_aggregate (abfd, p1, &rndx,
			    AUX_GET_ISYM (bigendian, &aux_ptr[indx+1]),
			    "struct");
      indx++;			/* skip aux words */
      break;

      /* Unions add 1-2 aux words:
	 1st word is [ST_RFDESCAPE, offset] pointer to union def;
	 2nd word is file index if 1st word rfd is ST_RFDESCAPE.  */

    case btUnion:		/* Union */
      ecoff_swap_rndx_in (bigendian, &aux_ptr[indx].a_rndx, &rndx);
      ecoff_emit_aggregate (abfd, p1, &rndx,
			    AUX_GET_ISYM (bigendian, &aux_ptr[indx+1]),
			    "union");
      indx++;			/* skip aux words */
      break;

      /* Enumerations add 1-2 aux words:
	 1st word is [ST_RFDESCAPE, offset] pointer to enum def;
	 2nd word is file index if 1st word rfd is ST_RFDESCAPE.  */

    case btEnum:		/* Enumeration */
      ecoff_swap_rndx_in (bigendian, &aux_ptr[indx].a_rndx, &rndx);
      ecoff_emit_aggregate (abfd, p1, &rndx,
			    AUX_GET_ISYM (bigendian, &aux_ptr[indx+1]),
			    "enum");
      indx++;			/* skip aux words */
      break;

    case btTypedef:		/* defined via a typedef, isymRef points */
      strcpy (p1, "typedef");
      break;

    case btRange:		/* subrange of int */
      strcpy (p1, "subrange");
      break;

    case btSet:			/* pascal sets */
      strcpy (p1, "set");
      break;

    case btComplex:		/* fortran complex */
      strcpy (p1, "complex");
      break;

    case btDComplex:		/* fortran double complex */
      strcpy (p1, "double complex");
      break;

    case btIndirect:		/* forward or unnamed typedef */
      strcpy (p1, "forward/unamed typedef");
      break;

    case btFixedDec:		/* Fixed Decimal */
      strcpy (p1, "fixed decimal");
      break;

    case btFloatDec:		/* Float Decimal */
      strcpy (p1, "float decimal");
      break;

    case btString:		/* Varying Length Character String */
      strcpy (p1, "string");
      break;

    case btBit:			/* Aligned Bit String */
      strcpy (p1, "bit");
      break;

    case btPicture:		/* Picture */
      strcpy (p1, "picture");
      break;

    case btVoid:		/* Void */
      strcpy (p1, "void");
      break;

    default:
      sprintf (p1, "Unknown basic type %d", (int) basic_type);
      break;
    }

  p1 += strlen (buffer1);

  /*
   * If this is a bitfield, get the bitsize.
   */
  if (u.ti.fBitfield)
    {
      int bitsize;

      bitsize = AUX_GET_WIDTH (bigendian, &aux_ptr[indx++]);
      sprintf (p1, " : %d", bitsize);
      p1 += strlen (buffer1);
    }


  /*
   * Deal with any qualifiers.
   */
  if (qualifiers[0].type != tqNil)
    {
      /*
       * Snarf up any array bounds in the correct order.  Arrays
       * store 5 successive words in the aux. table:
       *	word 0	RNDXR to type of the bounds (ie, int)
       *	word 1	Current file descriptor index
       *	word 2	low bound
       *	word 3	high bound (or -1 if [])
       *	word 4	stride size in bits
       */
      for (i = 0; i < 7; i++)
	{
	  if (qualifiers[i].type == tqArray)
	    {
	      qualifiers[i].low_bound =
		AUX_GET_DNLOW (bigendian, &aux_ptr[indx+2]);
	      qualifiers[i].high_bound =
		AUX_GET_DNHIGH (bigendian, &aux_ptr[indx+3]);
	      qualifiers[i].stride =
		AUX_GET_WIDTH (bigendian, &aux_ptr[indx+4]);
	      indx += 5;
	    }
	}

      /*
       * Now print out the qualifiers.
       */
      for (i = 0; i < 6; i++)
	{
	  switch (qualifiers[i].type)
	    {
	    case tqNil:
	    case tqMax:
	      break;

	    case tqPtr:
	      strcpy (p2, "ptr to ");
	      p2 += sizeof ("ptr to ")-1;
	      break;

	    case tqVol:
	      strcpy (p2, "volatile ");
	      p2 += sizeof ("volatile ")-1;
	      break;

	    case tqFar:
	      strcpy (p2, "far ");
	      p2 += sizeof ("far ")-1;
	      break;

	    case tqProc:
	      strcpy (p2, "func. ret. ");
	      p2 += sizeof ("func. ret. ");
	      break;

	    case tqArray:
	      {
		int first_array = i;
		int j;

		/* Print array bounds reversed (ie, in the order the C
		   programmer writes them).  C is such a fun language.... */

		while (i < 5 && qualifiers[i+1].type == tqArray)
		  i++;

		for (j = i; j >= first_array; j--)
		  {
		    strcpy (p2, "array [");
		    p2 += sizeof ("array [")-1;
		    if (qualifiers[j].low_bound != 0)
		      sprintf (p2,
			       "%ld:%ld {%ld bits}",
			       (long) qualifiers[j].low_bound,
			       (long) qualifiers[j].high_bound,
			       (long) qualifiers[j].stride);

		    else if (qualifiers[j].high_bound != -1)
		      sprintf (p2,
			       "%ld {%ld bits}",
			       (long) (qualifiers[j].high_bound + 1),
			       (long) (qualifiers[j].stride));

		    else
		      sprintf (p2, " {%ld bits}", (long) (qualifiers[j].stride));

		    p2 += strlen (p2);
		    strcpy (p2, "] of ");
		    p2 += sizeof ("] of ")-1;
		  }
	      }
	      break;
	    }
	}
    }

  strcpy (p2, buffer1);
  return buffer2;
}

/* Return information about ECOFF symbol SYMBOL in RET.  */

void
ecoff_get_symbol_info (abfd, symbol, ret)
     bfd *abfd;			/* Ignored.  */
     asymbol *symbol;
     symbol_info *ret;
{
  bfd_symbol_info (symbol, ret);
}

/* Print information about an ECOFF symbol.  */

void
ecoff_print_symbol (abfd, filep, symbol, how)
     bfd *abfd;
     PTR filep;
     asymbol *symbol;
     bfd_print_symbol_type how;
{
  const struct ecoff_backend_data * const backend = ecoff_backend (abfd);
  FILE *file = (FILE *)filep;

  switch (how)
    {
    case bfd_print_symbol_name:
      fprintf (file, "%s", symbol->name);
      break;
    case bfd_print_symbol_more:
      if (ecoffsymbol (symbol)->local)
	{
	  SYMR ecoff_sym;
	
	  (*backend->swap_sym_in) (abfd, ecoffsymbol (symbol)->native,
				   &ecoff_sym);
	  fprintf (file, "ecoff local ");
	  fprintf_vma (file, (bfd_vma) ecoff_sym.value);
	  fprintf (file, " %x %x", (unsigned) ecoff_sym.st,
		   (unsigned) ecoff_sym.sc);
	}
      else
	{
	  EXTR ecoff_ext;

	  (*backend->swap_ext_in) (abfd, ecoffsymbol (symbol)->native,
				   &ecoff_ext);
	  fprintf (file, "ecoff extern ");
	  fprintf_vma (file, (bfd_vma) ecoff_ext.asym.value);
	  fprintf (file, " %x %x", (unsigned) ecoff_ext.asym.st,
		   (unsigned) ecoff_ext.asym.sc);
	}
      break;
    case bfd_print_symbol_all:
      /* Print out the symbols in a reasonable way */
      {
	char type;
	int pos;
	EXTR ecoff_ext;
	char jmptbl;
	char cobol_main;
	char weakext;

	if (ecoffsymbol (symbol)->local)
	  {
	    (*backend->swap_sym_in) (abfd, ecoffsymbol (symbol)->native,
				     &ecoff_ext.asym);
	    type = 'l';
	    pos = ((((char *) ecoffsymbol (symbol)->native
		     - (char *) ecoff_data (abfd)->external_sym)
		    / backend->external_sym_size)
		   + ecoff_data (abfd)->symbolic_header.iextMax);
	    jmptbl = ' ';
	    cobol_main = ' ';
	    weakext = ' ';
	  }
	else
	  {
	    (*backend->swap_ext_in) (abfd, ecoffsymbol (symbol)->native,
				     &ecoff_ext);
	    type = 'e';
	    pos = (((char *) ecoffsymbol (symbol)->native
		    - (char *) ecoff_data (abfd)->external_ext)
		   / backend->external_ext_size);
	    jmptbl = ecoff_ext.jmptbl ? 'j' : ' ';
	    cobol_main = ecoff_ext.cobol_main ? 'c' : ' ';
	    weakext = ecoff_ext.weakext ? 'w' : ' ';
	  }

	fprintf (file, "[%3d] %c ",
		 pos, type);
	fprintf_vma (file, (bfd_vma) ecoff_ext.asym.value);
	fprintf (file, " st %x sc %x indx %x %c%c%c %s",
		 (unsigned) ecoff_ext.asym.st,
		 (unsigned) ecoff_ext.asym.sc,
		 (unsigned) ecoff_ext.asym.index,
		 jmptbl, cobol_main, weakext,
		 symbol->name);

	if (ecoffsymbol (symbol)->fdr != NULL
	    && ecoff_ext.asym.index != indexNil)
	  {
	    unsigned int indx;
	    int bigendian;
	    bfd_size_type sym_base;
	    union aux_ext *aux_base;

	    indx = ecoff_ext.asym.index;

	    /* sym_base is used to map the fdr relative indices which
	       appear in the file to the position number which we are
	       using.  */
	    sym_base = ecoffsymbol (symbol)->fdr->isymBase;
	    if (ecoffsymbol (symbol)->local)
	      sym_base += ecoff_data (abfd)->symbolic_header.iextMax;

	    /* aux_base is the start of the aux entries for this file;
	       asym.index is an offset from this.  */
	    aux_base = (ecoff_data (abfd)->external_aux
			+ ecoffsymbol (symbol)->fdr->iauxBase);

	    /* The aux entries are stored in host byte order; the
	       order is indicated by a bit in the fdr.  */
	    bigendian = ecoffsymbol (symbol)->fdr->fBigendian;

	    /* This switch is basically from gcc/mips-tdump.c  */
	    switch (ecoff_ext.asym.st)
	      {
	      case stNil:
	      case stLabel:
		break;

	      case stFile:
	      case stBlock:
		fprintf (file, "\n      End+1 symbol: %ld",
			 (long) (indx + sym_base));
		break;

	      case stEnd:
		if (ecoff_ext.asym.sc == scText
		    || ecoff_ext.asym.sc == scInfo)
		  fprintf (file, "\n      First symbol: %ld",
			   (long) (indx + sym_base));
		else
		  fprintf (file, "\n      First symbol: %ld", 
			   (long) (AUX_GET_ISYM (bigendian,
						 &aux_base[ecoff_ext.asym.index])
				   + sym_base));
		break;

	      case stProc:
	      case stStaticProc:
		if (ECOFF_IS_STAB (&ecoff_ext.asym))
		  ;
		else if (ecoffsymbol (symbol)->local)
		  fprintf (file, "\n      End+1 symbol: %-7ld   Type:  %s",
			   (long) (AUX_GET_ISYM (bigendian,
						 &aux_base[ecoff_ext.asym.index])
				   + sym_base),
			   ecoff_type_to_string (abfd, aux_base, indx + 1,
						 bigendian));
		else
		  fprintf (file, "\n      Local symbol: %d",
			   (indx
			    + sym_base
			    + ecoff_data (abfd)->symbolic_header.iextMax));
		break;

	      default:
		if (! ECOFF_IS_STAB (&ecoff_ext.asym))
		  fprintf (file, "\n      Type: %s",
			   ecoff_type_to_string (abfd, aux_base, indx,
						 bigendian));
		break;
	      }
	  }
      }
      break;
    }
}

/* Read in the relocs for a section.  */

static boolean
ecoff_slurp_reloc_table (abfd, section, symbols)
     bfd *abfd;
     asection *section;
     asymbol **symbols;
{
  const struct ecoff_backend_data * const backend = ecoff_backend (abfd);
  arelent *internal_relocs;
  bfd_size_type external_reloc_size;
  bfd_size_type external_relocs_size;
  char *external_relocs;
  arelent *rptr;
  unsigned int i;

  if (section->relocation != (arelent *) NULL
      || section->reloc_count == 0
      || (section->flags & SEC_CONSTRUCTOR) != 0)
    return true;

  if (ecoff_slurp_symbol_table (abfd) == false)
    return false;
  
  internal_relocs = (arelent *) bfd_alloc (abfd,
					   (sizeof (arelent)
					    * section->reloc_count));
  external_reloc_size = backend->external_reloc_size;
  external_relocs_size = external_reloc_size * section->reloc_count;
  external_relocs = (char *) bfd_alloc (abfd, external_relocs_size);
  if (internal_relocs == (arelent *) NULL
      || external_relocs == (char *) NULL)
    {
      bfd_error = no_memory;
      return false;
    }
  if (bfd_seek (abfd, section->rel_filepos, SEEK_SET) != 0)
    return false;
  if (bfd_read (external_relocs, 1, external_relocs_size, abfd)
      != external_relocs_size)
    {
      bfd_error = system_call_error;
      return false;
    }

  for (i = 0, rptr = internal_relocs; i < section->reloc_count; i++, rptr++)
    {
      struct internal_reloc intern;

      (*backend->swap_reloc_in) (abfd,
				 external_relocs + i * external_reloc_size,
				 &intern);

      if (intern.r_extern)
	{
	  /* r_symndx is an index into the external symbols.  */
	  BFD_ASSERT (intern.r_symndx >= 0
		      && (intern.r_symndx
			  < ecoff_data (abfd)->symbolic_header.iextMax));
	  rptr->sym_ptr_ptr = symbols + intern.r_symndx;
	  rptr->addend = 0;
	}
      else if (intern.r_symndx == RELOC_SECTION_NONE
	       || intern.r_symndx == RELOC_SECTION_ABS)
	{
	  rptr->sym_ptr_ptr = bfd_abs_section.symbol_ptr_ptr;
	  rptr->addend = 0;
	}
      else
	{
	  CONST char *sec_name;
	  asection *sec;

	  /* r_symndx is a section key.  */
	  switch (intern.r_symndx)
	    {
	    case RELOC_SECTION_TEXT:  sec_name = ".text";  break;
	    case RELOC_SECTION_RDATA: sec_name = ".rdata"; break;
	    case RELOC_SECTION_DATA:  sec_name = ".data";  break;
	    case RELOC_SECTION_SDATA: sec_name = ".sdata"; break;
	    case RELOC_SECTION_SBSS:  sec_name = ".sbss";  break;
	    case RELOC_SECTION_BSS:   sec_name = ".bss";   break;
	    case RELOC_SECTION_INIT:  sec_name = ".init";  break;
	    case RELOC_SECTION_LIT8:  sec_name = ".lit8";  break;
	    case RELOC_SECTION_LIT4:  sec_name = ".lit4";  break;
	    case RELOC_SECTION_XDATA: sec_name = ".xdata"; break;
	    case RELOC_SECTION_PDATA: sec_name = ".pdata"; break;
	    case RELOC_SECTION_LITA:  sec_name = ".lita";  break;
	    default: abort ();
	    }

	  sec = bfd_get_section_by_name (abfd, sec_name);
	  if (sec == (asection *) NULL)
	    abort ();
	  rptr->sym_ptr_ptr = sec->symbol_ptr_ptr;

	  rptr->addend = - bfd_get_section_vma (abfd, sec);
	}

      rptr->address = intern.r_vaddr - bfd_get_section_vma (abfd, section);

      /* Let the backend select the howto field and do any other
	 required processing.  */
      (*backend->finish_reloc) (abfd, &intern, rptr);
    }

  bfd_release (abfd, external_relocs);

  section->relocation = internal_relocs;

  return true;
}

/* Get a canonical list of relocs.  */

unsigned int
ecoff_canonicalize_reloc (abfd, section, relptr, symbols)
     bfd *abfd;
     asection *section;
     arelent **relptr;
     asymbol **symbols;
{
  unsigned int count;

  if (section->flags & SEC_CONSTRUCTOR) 
    {
      arelent_chain *chain;

      /* This section has relocs made up by us, not the file, so take
	 them out of their chain and place them into the data area
	 provided.  */
      for (count = 0, chain = section->constructor_chain;
	   count < section->reloc_count;
	   count++, chain = chain->next)
	*relptr++ = &chain->relent;
    }
  else
    { 
      arelent *tblptr;

      if (ecoff_slurp_reloc_table (abfd, section, symbols) == false)
	return 0;

      tblptr = section->relocation;
      if (tblptr == (arelent *) NULL)
	return 0;

      for (count = 0; count < section->reloc_count; count++)
	*relptr++ = tblptr++;
    }

  *relptr = (arelent *) NULL;

  return section->reloc_count;
}

/* Provided a BFD, a section and an offset into the section, calculate
   and return the name of the source file and the line nearest to the
   wanted location.  */

boolean
ecoff_find_nearest_line (abfd,
			 section,
			 ignore_symbols,
			 offset,
			 filename_ptr,
			 functionname_ptr,
			 retline_ptr)
     bfd *abfd;
     asection *section;
     asymbol **ignore_symbols;
     bfd_vma offset;
     CONST char **filename_ptr;
     CONST char **functionname_ptr;
     unsigned int *retline_ptr;
{
  const struct ecoff_backend_data * const backend = ecoff_backend (abfd);
  FDR *fdr_ptr;
  FDR *fdr_start;
  FDR *fdr_end;
  FDR *fdr_hold;
  bfd_size_type external_pdr_size;
  char *pdr_ptr;
  char *pdr_end;
  PDR pdr;
  unsigned char *line_ptr;
  unsigned char *line_end;
  int lineno;

  /* If we're not in the .text section, we don't have any line
     numbers.  */
  if (strcmp (section->name, _TEXT) != 0
      || offset < ecoff_data (abfd)->text_start
      || offset >= ecoff_data (abfd)->text_end)
    return false;

  /* Make sure we have the FDR's.  */
  if (ecoff_slurp_symbolic_info (abfd) == false
      || bfd_get_symcount (abfd) == 0)
    return false;

  /* Each file descriptor (FDR) has a memory address.  Here we track
     down which FDR we want.  The FDR's are stored in increasing
     memory order.  If speed is ever important, this can become a
     binary search.  We must ignore FDR's with no PDR entries; they
     will have the adr of the FDR before or after them.  */
  fdr_start = ecoff_data (abfd)->fdr;
  fdr_end = fdr_start + ecoff_data (abfd)->symbolic_header.ifdMax;
  fdr_hold = (FDR *) NULL;
  for (fdr_ptr = fdr_start; fdr_ptr < fdr_end; fdr_ptr++)
    {
      if (fdr_ptr->cpd == 0)
	continue;
      if (offset < fdr_ptr->adr)
	break;
      fdr_hold = fdr_ptr;
    }
  if (fdr_hold == (FDR *) NULL)
    return false;
  fdr_ptr = fdr_hold;

  /* Each FDR has a list of procedure descriptors (PDR).  PDR's also
     have an address, which is relative to the FDR address, and are
     also stored in increasing memory order.  */
  offset -= fdr_ptr->adr;
  external_pdr_size = backend->external_pdr_size;
  pdr_ptr = ((char *) ecoff_data (abfd)->external_pdr
	     + fdr_ptr->ipdFirst * external_pdr_size);
  pdr_end = pdr_ptr + fdr_ptr->cpd * external_pdr_size;
  (*backend->swap_pdr_in) (abfd, (PTR) pdr_ptr, &pdr);

  /* The address of the first PDR is an offset which applies to the
     addresses of all the PDR's.  */
  offset += pdr.adr;

  for (pdr_ptr += external_pdr_size;
       pdr_ptr < pdr_end;
       pdr_ptr += external_pdr_size)
    {
      (*backend->swap_pdr_in) (abfd, (PTR) pdr_ptr, &pdr);
      if (offset < pdr.adr)
	break;
    }

  /* Now we can look for the actual line number.  The line numbers are
     stored in a very funky format, which I won't try to describe.
     Note that right here pdr_ptr and pdr hold the PDR *after* the one
     we want; we need this to compute line_end.  */
  line_end = ecoff_data (abfd)->line;
  if (pdr_ptr == pdr_end)
    line_end += fdr_ptr->cbLineOffset + fdr_ptr->cbLine;
  else
    line_end += fdr_ptr->cbLineOffset + pdr.cbLineOffset;

  /* Now change pdr and pdr_ptr to the one we want.  */
  pdr_ptr -= external_pdr_size;
  (*backend->swap_pdr_in) (abfd, (PTR) pdr_ptr, &pdr);

  offset -= pdr.adr;
  lineno = pdr.lnLow;
  line_ptr = (ecoff_data (abfd)->line
	      + fdr_ptr->cbLineOffset
	      + pdr.cbLineOffset);
  while (line_ptr < line_end)
    {
      int delta;
      int count;

      delta = *line_ptr >> 4;
      if (delta >= 0x8)
	delta -= 0x10;
      count = (*line_ptr & 0xf) + 1;
      ++line_ptr;
      if (delta == -8)
	{
	  delta = (((line_ptr[0]) & 0xff) << 8) + ((line_ptr[1]) & 0xff);
	  if (delta >= 0x8000)
	    delta -= 0x10000;
	  line_ptr += 2;
	}
      lineno += delta;
      if (offset < count * 4)
	break;
      offset -= count * 4;
    }

  /* If fdr_ptr->rss is -1, then this file does not have full symbols,
     at least according to gdb/mipsread.c.  */
  if (fdr_ptr->rss == -1)
    {
      *filename_ptr = NULL;
      if (pdr.isym == -1)
	*functionname_ptr = NULL;
      else
	{
	  EXTR proc_ext;

	  (*backend->swap_ext_in) (abfd,
				   ((char *) ecoff_data (abfd)->external_ext
				    + pdr.isym * backend->external_ext_size),
				   &proc_ext);
	  *functionname_ptr = ecoff_data (abfd)->ssext + proc_ext.asym.iss;
	}
    }
  else
    {
      SYMR proc_sym;

      *filename_ptr = ecoff_data (abfd)->ss + fdr_ptr->issBase + fdr_ptr->rss;
      (*backend->swap_sym_in) (abfd,
			       ((char *) ecoff_data (abfd)->external_sym
				+ ((fdr_ptr->isymBase + pdr.isym)
				   * backend->external_sym_size)),
			       &proc_sym);
      *functionname_ptr = (ecoff_data (abfd)->ss
			   + fdr_ptr->issBase
			   + proc_sym.iss);
    }
  if (lineno == ilineNil)
    lineno = 0;
  *retline_ptr = lineno;
  return true;
}

/* We can't use the generic linking routines for ECOFF, because we
   have to handle all the debugging information.  The generic link
   routine just works out the section contents and attaches a list of
   symbols.

   We link by looping over all the seclets.  We make two passes.  On
   the first we set the actual section contents and determine the size
   of the debugging information.  On the second we accumulate the
   debugging information and write it out.

   This currently always accumulates the debugging information, which
   is incorrect, because it ignores the -s and -S options of the
   linker.  The linker needs to be modified to give us that
   information in a more useful format (currently it just provides a
   list of symbols which should appear in the output file).  */

/* Clear the output_has_begun flag for all the input BFD's.  We use it
   to avoid linking in the debugging information for a BFD more than
   once.  */

static void
ecoff_clear_output_flags (abfd)
     bfd *abfd;
{
  register asection *o;
  register bfd_seclet_type *p;

  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
    for (p = o->seclets_head;
	 p != (bfd_seclet_type *) NULL;
	 p = p->next)
      if (p->type == bfd_indirect_seclet)
	p->u.indirect.section->owner->output_has_begun = false;
}

/* Handle an indirect seclet on the first pass.  Set the contents of
   the output section, and accumulate the debugging information if
   any.  */

static boolean
ecoff_rel (output_bfd, seclet, output_section, data, relocateable)
     bfd *output_bfd;
     bfd_seclet_type *seclet;
     asection *output_section;
     PTR data;
     boolean relocateable;
{
  bfd *input_bfd;
  HDRR *output_symhdr;
  HDRR *input_symhdr;

  if ((output_section->flags & SEC_HAS_CONTENTS)
      && !(output_section->flags & SEC_NEVER_LOAD)
      && (output_section->flags & SEC_LOAD)
      && seclet->size)
    {
      data = (PTR) bfd_get_relocated_section_contents (output_bfd,
						       seclet,
						       data,
						       relocateable);
      if (bfd_set_section_contents (output_bfd,
				    output_section,
				    data,
				    seclet->offset,
				    seclet->size)
	  == false)
	{
	  abort();
	}
    }

  input_bfd = seclet->u.indirect.section->owner;

  /* We want to figure out how much space will be required to
     incorporate all the debugging information from input_bfd.  We use
     the output_has_begun field to avoid adding it in more than once.
     The actual incorporation is done in the second pass, in
     ecoff_get_debug.  The code has to parallel that code in its
     manipulations of output_symhdr.  */

  if (input_bfd->output_has_begun)
    return true;
  input_bfd->output_has_begun = true;

  output_symhdr = &ecoff_data (output_bfd)->symbolic_header;

  if (input_bfd->xvec->flavour != bfd_target_ecoff_flavour)
    {
      asymbol **symbols;
      asymbol **sym_ptr;
      asymbol **sym_end;

      /* We just accumulate local symbols from a non-ECOFF BFD.  The
	 external symbols are handled separately.  */

      symbols = (asymbol **) bfd_alloc (output_bfd,
					get_symtab_upper_bound (input_bfd));
      if (symbols == (asymbol **) NULL)
	{
	  bfd_error = no_memory;
	  return false;
	}
      sym_end = symbols + bfd_canonicalize_symtab (input_bfd, symbols);

      for (sym_ptr = symbols; sym_ptr < sym_end; sym_ptr++)
	{
	  size_t len;

	  len = strlen ((*sym_ptr)->name);
	  if (((*sym_ptr)->flags & BSF_EXPORT) == 0)
	    {
	      ++output_symhdr->isymMax;
	      output_symhdr->issMax += len + 1;
	    }
	}

      bfd_release (output_bfd, (PTR) symbols);

      ++output_symhdr->ifdMax;

      return true;
    }

  /* We simply add in the information from another ECOFF BFD.  First
     we make sure we have the symbolic information.  */
  if (ecoff_slurp_symbol_table (input_bfd) == false)
    return false;
  if (bfd_get_symcount (input_bfd) == 0)
    return true;

  input_symhdr = &ecoff_data (input_bfd)->symbolic_header;

  /* Figure out how much information we are going to be putting in.
     The external symbols are handled separately.  */
  output_symhdr->ilineMax += input_symhdr->ilineMax;
  output_symhdr->cbLine += input_symhdr->cbLine;
  output_symhdr->idnMax += input_symhdr->idnMax;
  output_symhdr->ipdMax += input_symhdr->ipdMax;
  output_symhdr->isymMax += input_symhdr->isymMax;
  output_symhdr->ioptMax += input_symhdr->ioptMax;
  output_symhdr->iauxMax += input_symhdr->iauxMax;
  output_symhdr->issMax += input_symhdr->issMax;
  output_symhdr->ifdMax += input_symhdr->ifdMax;

  /* The RFD's are special, since we create them if needed.  */
  if (input_symhdr->crfd > 0)
    output_symhdr->crfd += input_symhdr->crfd;
  else
    output_symhdr->crfd += input_symhdr->ifdMax;

  return true;
}

/* Handle an arbitrary seclet on the first pass.  */

static boolean
ecoff_dump_seclet (abfd, seclet, section, data, relocateable)
     bfd *abfd;
     bfd_seclet_type *seclet;
     asection *section;
     PTR data;
     boolean relocateable;
{
  switch (seclet->type) 
    {
    case bfd_indirect_seclet:
      /* The contents of this section come from another one somewhere
	 else.  */
      return ecoff_rel (abfd, seclet, section, data, relocateable);

    case bfd_fill_seclet:
      /* Fill in the section with fill.value.  This is used to pad out
	 sections, but we must avoid padding the .bss section.  */
      if ((section->flags & SEC_HAS_CONTENTS) == 0)
	{
	  if (seclet->u.fill.value != 0)
	    abort ();
	}
      else
	{
	  char *d = (char *) bfd_alloc (abfd, seclet->size);
	  unsigned int i;
	  boolean ret;

	  for (i = 0; i < seclet->size; i+=2)
	    d[i] = seclet->u.fill.value >> 8;
	  for (i = 1; i < seclet->size; i+=2)
	    d[i] = seclet->u.fill.value;
	  ret = bfd_set_section_contents (abfd, section, d, seclet->offset,
					  seclet->size);
	  bfd_release (abfd, (PTR) d);
	  return ret;
	}
      break;

    default:
      abort();
    }

  return true;
}

/* Add a string to the debugging information we are accumulating for a
   file.  Return the offset from the fdr string base or from the
   external string base.  */

static long
ecoff_add_string (output_bfd, fdr, string, external)
     bfd *output_bfd;
     FDR *fdr;
     CONST char *string;
     boolean external;
{
  HDRR *symhdr;
  size_t len;
  long ret;

  symhdr = &ecoff_data (output_bfd)->symbolic_header;
  len = strlen (string);
  if (external)
    {
      strcpy (ecoff_data (output_bfd)->ssext + symhdr->issExtMax, string);
      ret = symhdr->issExtMax;
      symhdr->issExtMax += len + 1;
    }
  else
    {
      strcpy (ecoff_data (output_bfd)->ss + symhdr->issMax, string);
      ret = fdr->cbSs;
      symhdr->issMax += len + 1;
      fdr->cbSs += len + 1;
    }
  return ret;
}

/* Accumulate the debugging information from an input section.  */

static boolean
ecoff_get_debug (output_bfd, seclet, section, relocateable)
     bfd *output_bfd;
     bfd_seclet_type *seclet;
     asection *section;
     boolean relocateable;
{
  const struct ecoff_backend_data * const backend = ecoff_backend (output_bfd);
  const bfd_size_type external_sym_size = backend->external_sym_size;
  const bfd_size_type external_pdr_size = backend->external_pdr_size;
  const bfd_size_type external_fdr_size = backend->external_fdr_size;
  const bfd_size_type external_rfd_size = backend->external_rfd_size;
  void (* const swap_sym_in) PARAMS ((bfd *, PTR, SYMR *))
    = backend->swap_sym_in;
  void (* const swap_sym_out) PARAMS ((bfd *, const SYMR *, PTR))
    = backend->swap_sym_out;
  void (* const swap_pdr_in) PARAMS ((bfd *, PTR, PDR *))
    = backend->swap_pdr_in;
  void (* const swap_fdr_out) PARAMS ((bfd *, const FDR *, PTR))
    = backend->swap_fdr_out;
  void (* const swap_rfd_out) PARAMS ((bfd *, const RFDT *, PTR))
    = backend->swap_rfd_out;
  bfd *input_bfd;
  HDRR *output_symhdr;
  HDRR *input_symhdr;
  ecoff_data_type *output_ecoff;
  ecoff_data_type *input_ecoff;
  unsigned int count;
  char *sym_out;
  ecoff_symbol_type *esym_ptr;
  ecoff_symbol_type *esym_end;
  FDR *fdr_ptr;
  FDR *fdr_end;
  char *fdr_out;

  input_bfd = seclet->u.indirect.section->owner;

  /* Don't get the information more than once. */
  if (input_bfd->output_has_begun)
    return true;
  input_bfd->output_has_begun = true;

  output_ecoff = ecoff_data (output_bfd);
  output_symhdr = &output_ecoff->symbolic_header;

  if (input_bfd->xvec->flavour != bfd_target_ecoff_flavour)
    {
      FDR fdr;
      asymbol **symbols;
      asymbol **sym_ptr;
      asymbol **sym_end;

      /* This is not an ECOFF BFD.  Just gather the symbols.  */

      memset (&fdr, 0, sizeof fdr);

      fdr.adr = bfd_get_section_vma (output_bfd, section) + seclet->offset;
      fdr.issBase = output_symhdr->issMax;
      fdr.cbSs = 0;
      fdr.rss = ecoff_add_string (output_bfd,
				  &fdr,
				  bfd_get_filename (input_bfd),
				  false);
      fdr.isymBase = output_symhdr->isymMax;

      /* Get the local symbols from the input BFD.  */
      symbols = (asymbol **) bfd_alloc (output_bfd,
					get_symtab_upper_bound (input_bfd));
      if (symbols == (asymbol **) NULL)
	{
	  bfd_error = no_memory;
	  return false;
	}
      sym_end = symbols + bfd_canonicalize_symtab (input_bfd, symbols);

      /* Handle the local symbols.  Any external symbols are handled
	 separately.  */
      fdr.csym = 0;
      for (sym_ptr = symbols; sym_ptr != sym_end; sym_ptr++)
	{
	  SYMR internal_sym;

	  if (((*sym_ptr)->flags & BSF_EXPORT) != 0)
	    continue;
	  memset (&internal_sym, 0, sizeof internal_sym);
	  internal_sym.iss = ecoff_add_string (output_bfd,
					       &fdr,
					       (*sym_ptr)->name,
					       false);

	  if (bfd_is_com_section ((*sym_ptr)->section)
	      || (*sym_ptr)->section == &bfd_und_section)
	    internal_sym.value = (*sym_ptr)->value;
	  else
	    internal_sym.value = ((*sym_ptr)->value
				  + (*sym_ptr)->section->output_offset
				  + (*sym_ptr)->section->output_section->vma);
	  internal_sym.st = stNil;
	  internal_sym.sc = scUndefined;
	  internal_sym.index = indexNil;
	  (*swap_sym_out) (output_bfd, &internal_sym,
			   ((char *) output_ecoff->external_sym
			    + output_symhdr->isymMax * external_sym_size));
	  ++fdr.csym;
	  ++output_symhdr->isymMax;
	}

      bfd_release (output_bfd, (PTR) symbols);

      /* Leave everything else in the FDR zeroed out.  This will cause
	 the lang field to be langC.  The fBigendian field will
	 indicate little endian format, but it doesn't matter because
	 it only applies to aux fields and there are none.  */

      (*swap_fdr_out) (output_bfd, &fdr,
		       ((char *) output_ecoff->external_fdr
			+ output_symhdr->ifdMax * external_fdr_size));
      ++output_symhdr->ifdMax;
      return true;
    }

  /* This is an ECOFF BFD.  We want to grab the information from
     input_bfd and attach it to output_bfd.  */
  count = bfd_get_symcount (input_bfd);
  if (count == 0)
    return true;
  input_ecoff = ecoff_data (input_bfd);
  input_symhdr = &input_ecoff->symbolic_header;

  /* I think that it is more efficient to simply copy the debugging
     information from the input BFD to the output BFD.  Because ECOFF
     uses relative pointers for most of the debugging information,
     only a little of it has to be changed at all.  */

  /* Swap in the local symbols, adjust their values, and swap them out
     again.  The external symbols are handled separately.  */
  sym_out = ((char *) output_ecoff->external_sym
	     + output_symhdr->isymMax * external_sym_size);

  esym_ptr = ecoff_data (input_bfd)->canonical_symbols;
  esym_end = esym_ptr + count;
  for (; esym_ptr < esym_end; esym_ptr++)
    {
      if (esym_ptr->local)
	{
	  SYMR sym;

	  (*swap_sym_in) (input_bfd, esym_ptr->native, &sym);

	  /* If we're producing an executable, move common symbols
	     into bss.  */
	  if (relocateable == false)
	    {
	      if (sym.sc == scCommon)
		sym.sc = scBss;
	      else if (sym.sc == scSCommon)
		sym.sc = scSBss;
	    }

	  if (! bfd_is_com_section (esym_ptr->symbol.section)
	      && (esym_ptr->symbol.flags & BSF_DEBUGGING) == 0
	      && esym_ptr->symbol.section != &bfd_und_section)
	    sym.value = (esym_ptr->symbol.value
			 + esym_ptr->symbol.section->output_offset
			 + esym_ptr->symbol.section->output_section->vma);
	  (*swap_sym_out) (output_bfd, &sym, sym_out);
	  sym_out += external_sym_size;
	}
    }

  /* That should have accounted for all the local symbols in
     input_bfd.  */

  /* Copy the information that does not need swapping.  */
  memcpy (output_ecoff->line + output_symhdr->cbLine,
	  input_ecoff->line,
	  input_symhdr->cbLine * sizeof (unsigned char));
  memcpy (output_ecoff->external_aux + output_symhdr->iauxMax,
	  input_ecoff->external_aux,
	  input_symhdr->iauxMax * sizeof (union aux_ext));
  memcpy (output_ecoff->ss + output_symhdr->issMax,
	  input_ecoff->ss,
	  input_symhdr->issMax * sizeof (char));

  /* Some of the information may need to be swapped.  */
  if (output_bfd->xvec->header_byteorder_big_p
      == input_bfd->xvec->header_byteorder_big_p)
    {
      /* The two BFD's have the same endianness, so memcpy will
	 suffice.  */
      if (input_symhdr->idnMax > 0)
	memcpy (((char *) output_ecoff->external_dnr
		 + output_symhdr->idnMax * backend->external_dnr_size),
		input_ecoff->external_dnr,
		input_symhdr->idnMax * backend->external_dnr_size);
      if (input_symhdr->ipdMax > 0)
	memcpy (((char *) output_ecoff->external_pdr
		 + output_symhdr->ipdMax * external_pdr_size),
		input_ecoff->external_pdr,
		input_symhdr->ipdMax * external_pdr_size);
      if (input_symhdr->ioptMax > 0)
	memcpy (((char *) output_ecoff->external_opt
		 + output_symhdr->ioptMax * backend->external_opt_size),
		input_ecoff->external_opt,
		input_symhdr->ioptMax * backend->external_opt_size);
    }
  else
    {
      bfd_size_type sz;
      char *in;
      char *end;
      char *out;

      /* The two BFD's have different endianness, so we must swap
	 everything in and out.  This code would always work, but it
	 would be slow in the normal case.  */
      sz = backend->external_dnr_size;
      in = (char *) input_ecoff->external_dnr;
      end = in + input_symhdr->idnMax * sz;
      out = (char *) output_ecoff->external_dnr + output_symhdr->idnMax * sz;
      for (; in < end; in += sz, out += sz)
	{
	  DNR dnr;

	  (*backend->swap_dnr_in) (input_bfd, in, &dnr);
	  (*backend->swap_dnr_out) (output_bfd, &dnr, out);
	}

      sz = external_pdr_size;
      in = (char *) input_ecoff->external_pdr;
      end = in + input_symhdr->ipdMax * sz;
      out = (char *) output_ecoff->external_pdr + output_symhdr->ipdMax * sz;
      for (; in < end; in += sz, out += sz)
	{
	  PDR pdr;

	  (*swap_pdr_in) (input_bfd, in, &pdr);
	  (*backend->swap_pdr_out) (output_bfd, &pdr, out);
	}

      sz = backend->external_opt_size;
      in = (char *) input_ecoff->external_opt;
      end = in + input_symhdr->ioptMax * sz;
      out = (char *) output_ecoff->external_opt + output_symhdr->ioptMax * sz;
      for (; in < end; in += sz, out += sz)
	{
	  OPTR opt;

	  (*backend->swap_opt_in) (input_bfd, in, &opt);
	  (*backend->swap_opt_out) (output_bfd, &opt, out);
	}
    }

  /* Set ifdbase so that the external symbols know how to adjust their
     ifd values.  */
  input_ecoff->ifdbase = output_symhdr->ifdMax;

  fdr_ptr = input_ecoff->fdr;
  fdr_end = fdr_ptr + input_symhdr->ifdMax;
  fdr_out = ((char *) output_ecoff->external_fdr
	     + output_symhdr->ifdMax * external_fdr_size);
  for (; fdr_ptr < fdr_end; fdr_ptr++, fdr_out += external_fdr_size)
    {
      FDR fdr;
      unsigned long pdr_off;

      fdr = *fdr_ptr;

      /* The memory address for this fdr is the address for the seclet
	 plus the offset to this fdr within input_bfd.  For some
	 reason the offset of the first procedure pointer is also
	 added in.  */
      if (fdr.cpd == 0)
	pdr_off = 0;
      else
	{
	  PDR pdr;

	  (*swap_pdr_in) (input_bfd,
			  ((char *) input_ecoff->external_pdr
			   + fdr.ipdFirst * external_pdr_size),
			  &pdr);
	  pdr_off = pdr.adr;
	}
      fdr.adr = (bfd_get_section_vma (output_bfd, section)
		 + seclet->offset
		 + (fdr_ptr->adr - input_ecoff->fdr->adr)
		 + pdr_off);

      fdr.issBase += output_symhdr->issMax;
      fdr.isymBase += output_symhdr->isymMax;
      fdr.ilineBase += output_symhdr->ilineMax;
      fdr.ioptBase += output_symhdr->ioptMax;
      fdr.ipdFirst += output_symhdr->ipdMax;
      fdr.iauxBase += output_symhdr->iauxMax;
      fdr.rfdBase += output_symhdr->crfd;

      /* If there are no RFD's, we are going to add some.  We don't
	 want to adjust irfd for this, so that all the FDR's can share
	 the RFD's.  */
      if (input_symhdr->crfd == 0)
	fdr.crfd = input_symhdr->ifdMax;

      if (fdr.cbLine != 0)
	fdr.cbLineOffset += output_symhdr->cbLine;

      (*swap_fdr_out) (output_bfd, &fdr, fdr_out);
    }

  if (input_symhdr->crfd > 0)
    {
      void (* const swap_rfd_in) PARAMS ((bfd *, PTR, RFDT *))
	= backend->swap_rfd_in;
      char *rfd_in;
      char *rfd_end;
      char *rfd_out;

      /* Swap and adjust the RFD's.  RFD's are only created by the
	 linker, so this will only be necessary if one of the input
	 files is the result of a partial link.  Presumably all
	 necessary RFD's are present.  */
      rfd_in = (char *) input_ecoff->external_rfd;
      rfd_end = rfd_in + input_symhdr->crfd * external_rfd_size;
      rfd_out = ((char *) output_ecoff->external_rfd
		 + output_symhdr->crfd * external_rfd_size);
      for (;
	   rfd_in < rfd_end;
	   rfd_in += external_rfd_size, rfd_out += external_rfd_size)
	{
	  RFDT rfd;

	  (*swap_rfd_in) (input_bfd, rfd_in, &rfd);
	  rfd += output_symhdr->ifdMax;
	  (*swap_rfd_out) (output_bfd, &rfd, rfd_out);
	}
      output_symhdr->crfd += input_symhdr->crfd;
    }
  else
    {
      char *rfd_out;
      char *rfd_end;
      RFDT rfd;

      /* Create RFD's.  Some of the debugging information includes
	 relative file indices.  These indices are taken as indices to
	 the RFD table if there is one, or to the global table if
	 there is not.  If we did not create RFD's, we would have to
	 parse and adjust all the debugging information which contains
	 file indices.  */
      rfd = output_symhdr->ifdMax;
      rfd_out = ((char *) output_ecoff->external_rfd
		 + output_symhdr->crfd * external_rfd_size);
      rfd_end = rfd_out + input_symhdr->ifdMax * external_rfd_size;
      for (; rfd_out < rfd_end; rfd_out += external_rfd_size, rfd++)
	(*swap_rfd_out) (output_bfd, &rfd, rfd_out);
      output_symhdr->crfd += input_symhdr->ifdMax;
    }

  /* Combine the register masks.  Not all of these are used on all
     targets, but that's OK because only the relevant ones will be
     swapped in and out.  */
  {
    int i;

    output_ecoff->gprmask |= input_ecoff->gprmask;
    output_ecoff->fprmask |= input_ecoff->fprmask;
    for (i = 0; i < 4; i++)
      output_ecoff->cprmask[i] |= input_ecoff->cprmask[i];
  }

  /* Update the counts.  */
  output_symhdr->ilineMax += input_symhdr->ilineMax;
  output_symhdr->cbLine += input_symhdr->cbLine;
  output_symhdr->idnMax += input_symhdr->idnMax;
  output_symhdr->ipdMax += input_symhdr->ipdMax;
  output_symhdr->isymMax += input_symhdr->isymMax;
  output_symhdr->ioptMax += input_symhdr->ioptMax;
  output_symhdr->iauxMax += input_symhdr->iauxMax;
  output_symhdr->issMax += input_symhdr->issMax;
  output_symhdr->ifdMax += input_symhdr->ifdMax;

  return true;
}

/* This is the actual link routine.  It makes two passes over all the
   seclets.  */

boolean
ecoff_bfd_seclet_link (abfd, data, relocateable)
     bfd *abfd;
     PTR data;
     boolean relocateable;
{
  const struct ecoff_backend_data * const backend = ecoff_backend (abfd);
  HDRR *symhdr;
  int ipass;
  register asection *o;
  register bfd_seclet_type *p;
  asymbol **sym_ptr_ptr;
  bfd_size_type debug_align;
  bfd_size_type size;
  char *raw;

  /* We accumulate the debugging information counts in the symbolic
     header.  */
  symhdr = &ecoff_data (abfd)->symbolic_header;
  symhdr->magic = backend->sym_magic;
  /* FIXME: What should the version stamp be?  */
  symhdr->vstamp = 0;
  symhdr->ilineMax = 0;
  symhdr->cbLine = 0;
  symhdr->idnMax = 0;
  symhdr->ipdMax = 0;
  symhdr->isymMax = 0;
  symhdr->ioptMax = 0;
  symhdr->iauxMax = 0;
  symhdr->issMax = 0;
  symhdr->issExtMax = 0;
  symhdr->ifdMax = 0;
  symhdr->crfd = 0;
  symhdr->iextMax = 0;

  /* We need to copy over the debugging symbols from each input BFD.
     When we do this copying, we have to adjust the text address in
     the FDR structures, so we have to know the text address used for
     the input BFD.  Since we only want to copy the symbols once per
     input BFD, but we are going to look at each input BFD multiple
     times (once for each section it provides), we arrange to always
     look at the text section first.  That means that when we copy the
     debugging information, we always know the text address.  So we
     actually do each pass in two sub passes; first the text sections,
     then the non-text sections.  We use the output_has_begun flag to
     determine whether we have copied over the debugging information
     yet.  */

  /* Do the first pass: set the output section contents and count the
     debugging information.  */
  ecoff_clear_output_flags (abfd);
  for (ipass = 0; ipass < 2; ipass++)
    {
      for (o = abfd->sections; o != (asection *) NULL; o = o->next)
	{
	  /* If this is a fake section, just forget it.  The register
	     information is handled in another way.  */
	  if (strcmp (o->name, SCOMMON) == 0
	      || strcmp (o->name, REGINFO) == 0)
	    continue;

	  /* For SEC_CODE sections, (flags & SEC_CODE) == 0 is false,
	     so they are done on pass 0.  For other sections the
	     expression is true, so they are done on pass 1.  */
	  if (((o->flags & SEC_CODE) == 0) != ipass)
	    continue;

	  for (p = o->seclets_head;
	       p != (bfd_seclet_type *) NULL;
	       p = p->next)
	    {
	      if (ecoff_dump_seclet (abfd, p, o, data, relocateable)
		  == false)
		return false;
	    }
	}
    }

  /* We handle the external symbols differently.  We use the ones
     attached to the output_bfd.  The linker will have already
     determined which symbols are to be attached.  Here we just
     determine how much space we will need for them.  */
  sym_ptr_ptr = bfd_get_outsymbols (abfd);
  if (sym_ptr_ptr != NULL)
    {
      asymbol **sym_end;

      sym_end = sym_ptr_ptr + bfd_get_symcount (abfd);
      for (; sym_ptr_ptr < sym_end; sym_ptr_ptr++)
	{
	  if (((*sym_ptr_ptr)->flags & BSF_DEBUGGING) == 0
	      && ((*sym_ptr_ptr)->flags & BSF_LOCAL) == 0)
	    {
	      ++symhdr->iextMax;
	      symhdr->issExtMax += strlen ((*sym_ptr_ptr)->name) + 1;
	    }
	}
    }

  /* Adjust the counts so that structures are longword aligned.  */
  debug_align = backend->debug_align;
  --debug_align;
  symhdr->cbLine = (symhdr->cbLine + debug_align) &~ debug_align;
  symhdr->issMax = (symhdr->issMax + debug_align) &~ debug_align;
  symhdr->issExtMax = (symhdr->issExtMax + debug_align) &~ debug_align;

  /* Now the counts in symhdr are the correct size for the debugging
     information.  We allocate the right amount of space, and reset
     the counts so that the second pass can use them as indices.  It
     would be possible to output the debugging information directly to
     the file in pass 2, rather than to build it in memory and then
     write it out.  Outputting to the file would require a lot of
     seeks and small writes, though, and I think this approach is
     faster.  */
  size = (symhdr->cbLine * sizeof (unsigned char)
	  + symhdr->idnMax * backend->external_dnr_size
	  + symhdr->ipdMax * backend->external_pdr_size
	  + symhdr->isymMax * backend->external_sym_size
	  + symhdr->ioptMax * backend->external_opt_size
	  + symhdr->iauxMax * sizeof (union aux_ext)
	  + symhdr->issMax * sizeof (char)
	  + symhdr->issExtMax * sizeof (char)
	  + symhdr->ifdMax * backend->external_fdr_size
	  + symhdr->crfd * backend->external_rfd_size
	  + symhdr->iextMax * backend->external_ext_size);
  raw = (char *) bfd_alloc (abfd, size);
  if (raw == (char *) NULL)
    {
      bfd_error = no_memory;
      return false;
    }
  ecoff_data (abfd)->raw_size = size;
  ecoff_data (abfd)->raw_syments = (PTR) raw;

  /* Initialize the raw pointers.  */
#define SET(field, count, type, size) \
  ecoff_data (abfd)->field = (type) raw; \
  raw += symhdr->count * size

  SET (line, cbLine, unsigned char *, sizeof (unsigned char));
  SET (external_dnr, idnMax, PTR, backend->external_dnr_size);
  SET (external_pdr, ipdMax, PTR, backend->external_pdr_size);
  SET (external_sym, isymMax, PTR, backend->external_sym_size);
  SET (external_opt, ioptMax, PTR, backend->external_opt_size);
  SET (external_aux, iauxMax, union aux_ext *, sizeof (union aux_ext));
  SET (ss, issMax, char *, sizeof (char));
  SET (ssext, issExtMax, char *, sizeof (char));
  SET (external_fdr, ifdMax, PTR, backend->external_fdr_size);
  SET (external_rfd, crfd, PTR, backend->external_rfd_size);
  SET (external_ext, iextMax, PTR, backend->external_ext_size);
#undef SET

  /* Reset the counts so the second pass can use them to know how far
     it has gotten.  */
  symhdr->ilineMax = 0;
  symhdr->cbLine = 0;
  symhdr->idnMax = 0;
  symhdr->ipdMax = 0;
  symhdr->isymMax = 0;
  symhdr->ioptMax = 0;
  symhdr->iauxMax = 0;
  symhdr->issMax = 0;
  symhdr->issExtMax = 0;
  symhdr->ifdMax = 0;
  symhdr->crfd = 0;
  symhdr->iextMax = 0;

  /* Do the second pass: accumulate the debugging information.  */
  ecoff_clear_output_flags (abfd);
  for (ipass = 0; ipass < 2; ipass++)
    {
      for (o = abfd->sections; o != (asection *) NULL; o = o->next)
	{
	  if (strcmp (o->name, SCOMMON) == 0
	      || strcmp (o->name, REGINFO) == 0)
	    continue;
	  if (((o->flags & SEC_CODE) == 0) != ipass)
	    continue;
	  for (p = o->seclets_head;
	       p != (bfd_seclet_type *) NULL;
	       p = p->next)
	    {
	      if (p->type == bfd_indirect_seclet)
		{
		  if (ecoff_get_debug (abfd, p, o, relocateable) == false)
		    return false;
		}
	    }
	}
    }

  /* Put in the external symbols.  */
  sym_ptr_ptr = bfd_get_outsymbols (abfd);
  if (sym_ptr_ptr != NULL)
    {
      const bfd_size_type external_ext_size = backend->external_ext_size;
      void (* const swap_ext_in) PARAMS ((bfd *, PTR, EXTR *))
	= backend->swap_ext_in;
      void (* const swap_ext_out) PARAMS ((bfd *, const EXTR *, PTR))
	= backend->swap_ext_out;
      char *ssext;
      char *external_ext;

      ssext = ecoff_data (abfd)->ssext;
      external_ext = (char *) ecoff_data (abfd)->external_ext;
      for (; *sym_ptr_ptr != NULL; sym_ptr_ptr++)
	{
	  asymbol *sym_ptr;
	  EXTR esym;

	  sym_ptr = *sym_ptr_ptr;

	  if ((sym_ptr->flags & BSF_DEBUGGING) != 0
	      || (sym_ptr->flags & BSF_LOCAL) != 0)
	    continue;

	  /* The native pointer can be NULL for a symbol created by
	     the linker via ecoff_make_empty_symbol.  */
	  if (bfd_asymbol_flavour (sym_ptr) != bfd_target_ecoff_flavour
	      || ecoffsymbol (sym_ptr)->native == NULL)
	    {
	      esym.jmptbl = 0;
	      esym.cobol_main = 0;
	      esym.weakext = 0;
	      esym.reserved = 0;
	      esym.ifd = ifdNil;
	      /* FIXME: we can do better than this for st and sc.  */
	      esym.asym.st = stGlobal;
	      esym.asym.sc = scAbs;
	      esym.asym.reserved = 0;
	      esym.asym.index = indexNil;
	    }
	  else
	    {
	      ecoff_symbol_type *ecoff_sym_ptr;

	      ecoff_sym_ptr = ecoffsymbol (sym_ptr);
	      if (ecoff_sym_ptr->local)
		abort ();
	      (*swap_ext_in) (abfd, ecoff_sym_ptr->native, &esym);

	      /* If we're producing an executable, move common symbols
		 into bss.  */
	      if (relocateable == false)
		{
		  if (esym.asym.sc == scCommon)
		    esym.asym.sc = scBss;
		  else if (esym.asym.sc == scSCommon)
		    esym.asym.sc = scSBss;
		}

	      /* Adjust the FDR index for the symbol by that used for
		 the input BFD.  */
	      esym.ifd += ecoff_data (bfd_asymbol_bfd (sym_ptr))->ifdbase;
	    }

	  esym.asym.iss = symhdr->issExtMax;

	  if (bfd_is_com_section (sym_ptr->section)
	      || sym_ptr->section == &bfd_und_section)
	    esym.asym.value = sym_ptr->value;
	  else
	    esym.asym.value = (sym_ptr->value
			       + sym_ptr->section->output_offset
			       + sym_ptr->section->output_section->vma);

	  (*swap_ext_out) (abfd, &esym, external_ext);

	  ecoff_set_sym_index (sym_ptr, symhdr->iextMax);

	  external_ext += external_ext_size;
	  ++symhdr->iextMax;

	  strcpy (ssext + symhdr->issExtMax, sym_ptr->name);
	  symhdr->issExtMax += strlen (sym_ptr->name) + 1;
	}
    }

  /* Adjust the counts so that structures are longword aligned.  */
  symhdr->cbLine = (symhdr->cbLine + debug_align) &~ debug_align;
  symhdr->issMax = (symhdr->issMax + debug_align) &~ debug_align;
  symhdr->issExtMax = (symhdr->issExtMax + debug_align) &~ debug_align;

  return true;
}

/* Set the architecture.  The supported architecture is stored in the
   backend pointer.  We always set the architecture anyhow, since many
   callers ignore the return value.  */

boolean
ecoff_set_arch_mach (abfd, arch, machine)
     bfd *abfd;
     enum bfd_architecture arch;
     unsigned long machine;
{
  bfd_default_set_arch_mach (abfd, arch, machine);
  return arch == ecoff_backend (abfd)->arch;
}

/* Get the size of the section headers.  We do not output the .scommon
   section which we created in ecoff_mkobject, nor do we output any
   .reginfo section.  */

int
ecoff_sizeof_headers (abfd, reloc)
     bfd *abfd;
     boolean reloc;
{
  asection *current;
  int c;

  c = 0;
  for (current = abfd->sections;
       current != (asection *)NULL; 
       current = current->next) 
    if (strcmp (current->name, SCOMMON) != 0
	&& strcmp (current->name, REGINFO) != 0)
      ++c;

  return (bfd_coff_filhsz (abfd)
	  + bfd_coff_aoutsz (abfd)
	  + c * bfd_coff_scnhsz (abfd));
}


/* Get the contents of a section.  This is where we handle reading the
   .reginfo section, which implicitly holds the contents of an
   ecoff_reginfo structure.  */

boolean
ecoff_get_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     asection *section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  ecoff_data_type *tdata = ecoff_data (abfd);
  struct ecoff_reginfo s;
  int i;

  if (strcmp (section->name, REGINFO) != 0)
    return bfd_generic_get_section_contents (abfd, section, location,
					     offset, count);

  s.gp_value = tdata->gp;
  s.gprmask = tdata->gprmask;
  for (i = 0; i < 4; i++)
    s.cprmask[i] = tdata->cprmask[i];
  s.fprmask = tdata->fprmask;

  /* bfd_get_section_contents has already checked that the offset and
     size is reasonable.  We don't have to worry about swapping or any
     such thing; the .reginfo section is defined such that the
     contents are an ecoff_reginfo structure as seen on the host.  */
  memcpy (location, ((char *) &s) + offset, count);
  return true;
}

/* Calculate the file position for each section, and set
   reloc_filepos.  */

static void
ecoff_compute_section_file_positions (abfd)
     bfd *abfd;
{
  asection *current;
  file_ptr sofar;
  file_ptr old_sofar;
  boolean first_data;

  if (bfd_get_start_address (abfd)) 
    abfd->flags |= EXEC_P;

  sofar = ecoff_sizeof_headers (abfd, false);

  first_data = true;
  for (current = abfd->sections;
       current != (asection *) NULL;
       current = current->next)
    {
      /* Only deal with sections which have contents */
      if ((current->flags & (SEC_HAS_CONTENTS | SEC_LOAD)) == 0
	  || strcmp (current->name, SCOMMON) == 0
	  || strcmp (current->name, REGINFO) == 0)
	continue;

      /* On Ultrix, the data sections in an executable file must be
	 aligned to a page boundary within the file.  This does not
	 affect the section size, though.  FIXME: Does this work for
	 other platforms?  */
      if ((abfd->flags & EXEC_P) != 0
	  && (abfd->flags & D_PAGED) != 0
	  && first_data != false
	  && (current->flags & SEC_CODE) == 0)
	{
	  const bfd_vma round = ecoff_backend (abfd)->round;

	  sofar = (sofar + round - 1) &~ (round - 1);
	  first_data = false;
	}

      /* Align the sections in the file to the same boundary on
	 which they are aligned in virtual memory.  */
      old_sofar = sofar;
      sofar = BFD_ALIGN (sofar, 1 << current->alignment_power);

      current->filepos = sofar;

      sofar += current->_raw_size;

      /* make sure that this section is of the right size too */
      old_sofar = sofar;
      sofar = BFD_ALIGN (sofar, 1 << current->alignment_power);
      current->_raw_size += sofar - old_sofar;
    }

  ecoff_data (abfd)->reloc_filepos = sofar;
}

/* Set the contents of a section.  This is where we handle setting the
   contents of the .reginfo section, which implicitly holds a
   ecoff_reginfo structure.  */

boolean
ecoff_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     asection *section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  if (abfd->output_has_begun == false)
    ecoff_compute_section_file_positions (abfd);

  if (strcmp (section->name, REGINFO) == 0)
    {
      ecoff_data_type *tdata = ecoff_data (abfd);
      struct ecoff_reginfo s;
      int i;

      /* If the caller is only changing part of the structure, we must
	 retrieve the current information before the memcpy.  */
      if (offset != 0 || count != sizeof (struct ecoff_reginfo))
	{
	  s.gp_value = tdata->gp;
	  s.gprmask = tdata->gprmask;
	  for (i = 0; i < 4; i++)
	    s.cprmask[i] = tdata->cprmask[i];
	  s.fprmask = tdata->fprmask;
	}

      /* bfd_set_section_contents has already checked that the offset
	 and size is reasonable.  We don't have to worry about
	 swapping or any such thing; the .reginfo section is defined
	 such that the contents are an ecoff_reginfo structure as seen
	 on the host.  */
      memcpy (((char *) &s) + offset, location, count);

      tdata->gp = s.gp_value;
      tdata->gprmask = s.gprmask;
      for (i = 0; i < 4; i++)
	tdata->cprmask[i] = s.cprmask[i];
      tdata->fprmask = s.fprmask;

      return true;

    }

  bfd_seek (abfd, (file_ptr) (section->filepos + offset), SEEK_SET);

  if (count != 0)
    return (bfd_write (location, 1, count, abfd) == count) ? true : false;

  return true;
}

/* Write out an ECOFF file.  */

boolean
ecoff_write_object_contents (abfd)
     bfd *abfd;
{
  const struct ecoff_backend_data * const backend = ecoff_backend (abfd);
  const bfd_vma round = backend->round;
  const bfd_size_type filhsz = bfd_coff_filhsz (abfd);
  const bfd_size_type aoutsz = bfd_coff_aoutsz (abfd);
  const bfd_size_type scnhsz = bfd_coff_scnhsz (abfd);
  const bfd_size_type external_hdr_size = backend->external_hdr_size;
  const bfd_size_type external_reloc_size = backend->external_reloc_size;
  void (* const swap_reloc_out) PARAMS ((bfd *,
					 const struct internal_reloc *,
					 PTR))
    = backend->swap_reloc_out;
  asection *current;
  unsigned int count;
  file_ptr scn_base;
  file_ptr reloc_base;
  file_ptr sym_base;
  unsigned long reloc_size;
  unsigned long text_size;
  unsigned long text_start;
  unsigned long data_size;
  unsigned long data_start;
  unsigned long bss_size;
  PTR buff;
  struct internal_filehdr internal_f;
  struct internal_aouthdr internal_a;
  int i;

  bfd_error = system_call_error;

  if(abfd->output_has_begun == false)
    ecoff_compute_section_file_positions(abfd);

  if (abfd->sections != (asection *) NULL)
    scn_base = abfd->sections->filepos;
  else
    scn_base = 0;
  reloc_base = ecoff_data (abfd)->reloc_filepos;

  count = 1;
  reloc_size = 0;
  for (current = abfd->sections;
       current != (asection *)NULL; 
       current = current->next) 
    {
      if (strcmp (current->name, SCOMMON) == 0
	  || strcmp (current->name, REGINFO) == 0)
	continue;
      current->target_index = count;
      ++count;
      if (current->reloc_count != 0)
	{
	  bfd_size_type relsize;

	  current->rel_filepos = reloc_base;
	  relsize = current->reloc_count * external_reloc_size;
	  reloc_size += relsize;
	  reloc_base += relsize;
	}
      else
	current->rel_filepos = 0;
    }

  sym_base = reloc_base + reloc_size;

  /* At least on Ultrix, the symbol table of an executable file must
     be aligned to a page boundary.  FIXME: Is this true on other
     platforms?  */
  if ((abfd->flags & EXEC_P) != 0
      && (abfd->flags & D_PAGED) != 0)
    sym_base = (sym_base + round - 1) &~ (round - 1);

  ecoff_data (abfd)->sym_filepos = sym_base;

  if ((abfd->flags & D_PAGED) != 0)
    text_size = ecoff_sizeof_headers (abfd, false);
  else
    text_size = 0;
  text_start = 0;
  data_size = 0;
  data_start = 0;
  bss_size = 0;

  /* Write section headers to the file.  */

  buff = (PTR) alloca (scnhsz);
  internal_f.f_nscns = 0;
  if (bfd_seek (abfd, (file_ptr) (filhsz + aoutsz), SEEK_SET) != 0)
    return false;
  for (current = abfd->sections;
       current != (asection *) NULL;
       current = current->next)
    {
      struct internal_scnhdr section;
      bfd_vma vma;

      if (strcmp (current->name, SCOMMON) == 0)
	{
	  BFD_ASSERT (bfd_get_section_size_before_reloc (current) == 0
		      && current->reloc_count == 0);
	  continue;
	}
      if (strcmp (current->name, REGINFO) == 0)
	{
	  BFD_ASSERT (current->reloc_count == 0);
	  continue;
	}

      ++internal_f.f_nscns;

      strncpy (section.s_name, current->name, sizeof section.s_name);

      /* FIXME: is this correct for shared libraries?  I think it is
	 but I have no platform to check.  Ian Lance Taylor.  */
      vma = bfd_get_section_vma (abfd, current);
      if (strcmp (current->name, _LIB) == 0)
	section.s_vaddr = 0;
      else
	section.s_vaddr = vma;

      section.s_paddr = vma;
      section.s_size = bfd_get_section_size_before_reloc (current);

      /* If this section is unloadable then the scnptr will be 0.  */
      if ((current->flags & (SEC_LOAD | SEC_HAS_CONTENTS)) == 0)
	section.s_scnptr = 0;
      else
	section.s_scnptr = current->filepos;
      section.s_relptr = current->rel_filepos;

      /* FIXME: the lnnoptr of the .sbss or .sdata section of an
	 object file produced by the assembler is supposed to point to
	 information about how much room is required by objects of
	 various different sizes.  I think this only matters if we
	 want the linker to compute the best size to use, or
	 something.  I don't know what happens if the information is
	 not present.  */
      section.s_lnnoptr = 0;

      section.s_nreloc = current->reloc_count;
      section.s_nlnno = 0;
      section.s_flags = ecoff_sec_to_styp_flags (current->name,
						 current->flags);

      bfd_coff_swap_scnhdr_out (abfd, (PTR) &section, buff);
      if (bfd_write (buff, 1, scnhsz, abfd) != scnhsz)
	return false;

      if ((section.s_flags & STYP_TEXT) != 0)
	{
	  text_size += bfd_get_section_size_before_reloc (current);
	  if (text_start == 0 || text_start > vma)
	    text_start = vma;
	}
      else if ((section.s_flags & STYP_RDATA) != 0
	       || (section.s_flags & STYP_DATA) != 0
	       || (section.s_flags & STYP_LIT8) != 0
	       || (section.s_flags & STYP_LIT4) != 0
	       || (section.s_flags & STYP_SDATA) != 0)
	{
	  data_size += bfd_get_section_size_before_reloc (current);
	  if (data_start == 0 || data_start > vma)
	    data_start = vma;
	}
      else if ((section.s_flags & STYP_BSS) != 0
	       || (section.s_flags & STYP_SBSS) != 0)
	bss_size += bfd_get_section_size_before_reloc (current);
    }	

  /* Set up the file header.  */

  internal_f.f_magic = ecoff_get_magic (abfd);

  /* We will NOT put a fucking timestamp in the header here. Every
     time you put it back, I will come in and take it out again.  I'm
     sorry.  This field does not belong here.  We fill it with a 0 so
     it compares the same but is not a reasonable time. --
     gnu@cygnus.com.  */
  internal_f.f_timdat = 0;

  if (bfd_get_symcount (abfd) != 0)
    {
      /* The ECOFF f_nsyms field is not actually the number of
	 symbols, it's the size of symbolic information header.  */
      internal_f.f_nsyms = external_hdr_size;
      internal_f.f_symptr = sym_base;
    }
  else
    {
      internal_f.f_nsyms = 0;
      internal_f.f_symptr = 0;
    }

  internal_f.f_opthdr = aoutsz;

  internal_f.f_flags = F_LNNO;
  if (reloc_size == 0)
    internal_f.f_flags |= F_RELFLG;
  if (bfd_get_symcount (abfd) == 0)
    internal_f.f_flags |= F_LSYMS;
  if (abfd->flags & EXEC_P)
    internal_f.f_flags |= F_EXEC;

  if (! abfd->xvec->byteorder_big_p)
    internal_f.f_flags |= F_AR32WR;
  else
    internal_f.f_flags |= F_AR32W;

  /* Set up the ``optional'' header.  */
  if ((abfd->flags & D_PAGED) != 0)
    internal_a.magic = ECOFF_AOUT_ZMAGIC;
  else
    internal_a.magic = ECOFF_AOUT_OMAGIC;

  /* FIXME: This is what Ultrix puts in, and it makes the Ultrix
     linker happy.  But, is it right?  */
  internal_a.vstamp = 0x20a;

  /* At least on Ultrix, these have to be rounded to page boundaries.
     FIXME: Is this true on other platforms?  */
  if ((abfd->flags & D_PAGED) != 0)
    {
      internal_a.tsize = (text_size + round - 1) &~ (round - 1);
      internal_a.text_start = text_start &~ (round - 1);
      internal_a.dsize = (data_size + round - 1) &~ (round - 1);
      internal_a.data_start = data_start &~ (round - 1);
    }
  else
    {
      internal_a.tsize = text_size;
      internal_a.text_start = text_start;
      internal_a.dsize = data_size;
      internal_a.data_start = data_start;
    }

  /* On Ultrix, the initial portions of the .sbss and .bss segments
     are at the end of the data section.  The bsize field in the
     optional header records how many bss bytes are required beyond
     those in the data section.  The value is not rounded to a page
     boundary.  */
  if (bss_size < internal_a.dsize - data_size)
    bss_size = 0;
  else
    bss_size -= internal_a.dsize - data_size;
  internal_a.bsize = bss_size;
  internal_a.bss_start = internal_a.data_start + internal_a.dsize;

  internal_a.entry = bfd_get_start_address (abfd);

  internal_a.gp_value = ecoff_data (abfd)->gp;

  internal_a.gprmask = ecoff_data (abfd)->gprmask;
  internal_a.fprmask = ecoff_data (abfd)->fprmask;
  for (i = 0; i < 4; i++)
    internal_a.cprmask[i] = ecoff_data (abfd)->cprmask[i];

  /* Write out the file header and the optional header.  */

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
    return false;

  buff = (PTR) alloca (filhsz);
  bfd_coff_swap_filehdr_out (abfd, (PTR) &internal_f, buff);
  if (bfd_write (buff, 1, filhsz, abfd) != filhsz)
    return false;

  buff = (PTR) alloca (aoutsz);
  bfd_coff_swap_aouthdr_out (abfd, (PTR) &internal_a, buff);
  if (bfd_write (buff, 1, aoutsz, abfd) != aoutsz)
    return false;

  /* Write out the relocs.  */
  for (current = abfd->sections;
       current != (asection *) NULL;
       current = current->next)
    {
      arelent **reloc_ptr_ptr;
      arelent **reloc_end;
      char *out_ptr;

      if (current->reloc_count == 0)
	continue;

      buff = bfd_alloc (abfd, current->reloc_count * external_reloc_size);
      if (buff == NULL)
	{
	  bfd_error = no_memory;
	  return false;
	}

      reloc_ptr_ptr = current->orelocation;
      reloc_end = reloc_ptr_ptr + current->reloc_count;
      out_ptr = (char *) buff;
      for (;
	   reloc_ptr_ptr < reloc_end;
	   reloc_ptr_ptr++, out_ptr += external_reloc_size)
	{
	  arelent *reloc;
	  asymbol *sym;
	  struct internal_reloc in;
	  
	  memset (&in, 0, sizeof in);

	  reloc = *reloc_ptr_ptr;
	  sym = *reloc->sym_ptr_ptr;

	  in.r_vaddr = reloc->address + bfd_get_section_vma (abfd, current);
	  in.r_type = reloc->howto->type;

	  if ((sym->flags & BSF_SECTION_SYM) == 0)
	    {
	      in.r_symndx = ecoff_get_sym_index (*reloc->sym_ptr_ptr);
	      in.r_extern = 1;
	    }
	  else
	    {
	      CONST char *name;

	      name = bfd_get_section_name (abfd, bfd_get_section (sym));
	      if (strcmp (name, ".text") == 0)
		in.r_symndx = RELOC_SECTION_TEXT;
	      else if (strcmp (name, ".rdata") == 0)
		in.r_symndx = RELOC_SECTION_RDATA;
	      else if (strcmp (name, ".data") == 0)
		in.r_symndx = RELOC_SECTION_DATA;
	      else if (strcmp (name, ".sdata") == 0)
		in.r_symndx = RELOC_SECTION_SDATA;
	      else if (strcmp (name, ".sbss") == 0)
		in.r_symndx = RELOC_SECTION_SBSS;
	      else if (strcmp (name, ".bss") == 0)
		in.r_symndx = RELOC_SECTION_BSS;
	      else if (strcmp (name, ".init") == 0)
		in.r_symndx = RELOC_SECTION_INIT;
	      else if (strcmp (name, ".lit8") == 0)
		in.r_symndx = RELOC_SECTION_LIT8;
	      else if (strcmp (name, ".lit4") == 0)
		in.r_symndx = RELOC_SECTION_LIT4;
	      else
		abort ();
	      in.r_extern = 0;
	    }

	  (*swap_reloc_out) (abfd, &in, (PTR) out_ptr);
	}

      if (bfd_seek (abfd, current->rel_filepos, SEEK_SET) != 0)
	return false;
      if (bfd_write (buff, external_reloc_size, current->reloc_count, abfd)
	  != external_reloc_size * current->reloc_count)
	return false;
      bfd_release (abfd, buff);
    }

  /* Write out the symbolic debugging information.  */
  if (bfd_get_symcount (abfd) > 0)
    {
      HDRR *symhdr;
      unsigned long sym_offset;

      /* Set up the offsets in the symbolic header.  */
      symhdr = &ecoff_data (abfd)->symbolic_header;
      sym_offset = ecoff_data (abfd)->sym_filepos + external_hdr_size;

#define SET(offset, size, ptr) \
  if (symhdr->size == 0) \
    symhdr->offset = 0; \
  else \
    symhdr->offset = (((char *) ecoff_data (abfd)->ptr \
		       - (char *) ecoff_data (abfd)->raw_syments) \
		      + sym_offset);

      SET (cbLineOffset, cbLine, line);
      SET (cbDnOffset, idnMax, external_dnr);
      SET (cbPdOffset, ipdMax, external_pdr);
      SET (cbSymOffset, isymMax, external_sym);
      SET (cbOptOffset, ioptMax, external_opt);
      SET (cbAuxOffset, iauxMax, external_aux);
      SET (cbSsOffset, issMax, ss);
      SET (cbSsExtOffset, issExtMax, ssext);
      SET (cbFdOffset, ifdMax, external_fdr);
      SET (cbRfdOffset, crfd, external_rfd);
      SET (cbExtOffset, iextMax, external_ext);
#undef SET

      if (bfd_seek (abfd, (file_ptr) ecoff_data (abfd)->sym_filepos,
		    SEEK_SET) != 0)
	return false;
      buff = (PTR) alloca (external_hdr_size);
      (*backend->swap_hdr_out) (abfd, &ecoff_data (abfd)->symbolic_header,
				buff);
      if (bfd_write (buff, 1, external_hdr_size, abfd) != external_hdr_size)
	return false;
      if (bfd_write ((PTR) ecoff_data (abfd)->raw_syments, 1,
		     ecoff_data (abfd)->raw_size, abfd)
	  != ecoff_data (abfd)->raw_size)
	return false;
    }
  else if ((abfd->flags & EXEC_P) != 0
	   && (abfd->flags & D_PAGED) != 0)
    {
      char c;

      /* A demand paged executable must occupy an even number of
	 pages.  */
      if (bfd_seek (abfd, (file_ptr) ecoff_data (abfd)->sym_filepos - 1,
		    SEEK_SET) != 0)
	return false;
      if (bfd_read (&c, 1, 1, abfd) == 0)
	c = 0;
      if (bfd_seek (abfd, (file_ptr) ecoff_data (abfd)->sym_filepos - 1,
		    SEEK_SET) != 0)
	return false;
      if (bfd_write (&c, 1, 1, abfd) != 1)
	return false;      
    }

  return true;
}

/* Archive handling.  ECOFF uses what appears to be a unique type of
   archive header (which I call an armap).  The byte ordering of the
   armap and the contents are encoded in the name of the armap itself.
   At least for now, we only support archives with the same byte
   ordering in the armap and the contents.

   The first four bytes in the armap are the number of symbol
   definitions.  This is always a power of two.

   This is followed by the symbol definitions.  Each symbol definition
   occupies 8 bytes.  The first four bytes are the offset from the
   start of the armap strings to the null-terminated string naming
   this symbol.  The second four bytes are the file offset to the
   archive member which defines this symbol.  If the second four bytes
   are 0, then this is not actually a symbol definition, and it should
   be ignored.

   The symbols are hashed into the armap with a closed hashing scheme.
   See the functions below for the details of the algorithm.

   We could use the hash table when looking up symbols in a library.
   This would require a new BFD target entry point to replace the
   bfd_get_next_mapent function used by the linker.

   After the symbol definitions comes four bytes holding the size of
   the string table, followed by the string table itself.  */

/* The name of an archive headers looks like this:
   __________E[BL]E[BL]_ (with a trailing space).
   The trailing space is changed to an X if the archive is changed to
   indicate that the armap is out of date.

   The Alpha seems to use ________64E[BL]E[BL]_.  */

#define ARMAP_BIG_ENDIAN 'B'
#define ARMAP_LITTLE_ENDIAN 'L'
#define ARMAP_MARKER 'E'
#define ARMAP_START_LENGTH 10
#define ARMAP_HEADER_MARKER_INDEX 10
#define ARMAP_HEADER_ENDIAN_INDEX 11
#define ARMAP_OBJECT_MARKER_INDEX 12
#define ARMAP_OBJECT_ENDIAN_INDEX 13
#define ARMAP_END_INDEX 14
#define ARMAP_END "_ "

/* This is a magic number used in the hashing algorithm.  */
#define ARMAP_HASH_MAGIC 0x9dd68ab5

/* This returns the hash value to use for a string.  It also sets
   *REHASH to the rehash adjustment if the first slot is taken.  SIZE
   is the number of entries in the hash table, and HLOG is the log
   base 2 of SIZE.  */

static unsigned int
ecoff_armap_hash (s, rehash, size, hlog)
     CONST char *s;
     unsigned int *rehash;
     unsigned int size;
     unsigned int hlog;
{
  unsigned int hash;

  hash = *s++;
  while (*s != '\0')
    hash = ((hash >> 27) | (hash << 5)) + *s++;
  hash *= ARMAP_HASH_MAGIC;
  *rehash = (hash & (size - 1)) | 1;
  return hash >> (32 - hlog);
}

/* Read in the armap.  */

boolean
ecoff_slurp_armap (abfd)
     bfd *abfd;
{
  char nextname[17];
  unsigned int i;
  struct areltdata *mapdata;
  bfd_size_type parsed_size;
  char *raw_armap;
  struct artdata *ardata;
  unsigned int count;
  char *raw_ptr;
  struct symdef *symdef_ptr;
  char *stringbase;
  
  /* Get the name of the first element.  */
  i = bfd_read ((PTR) nextname, 1, 16, abfd);
  if (i == 0)
      return true;
  if (i != 16)
      return false;

  bfd_seek (abfd, (file_ptr) -16, SEEK_CUR);

  /* Irix 4.0.5F apparently can use either an ECOFF armap or a
     standard COFF armap.  We could move the ECOFF armap stuff into
     bfd_slurp_armap, but that seems inappropriate since no other
     target uses this format.  Instead, we check directly for a COFF
     armap.  */
  if (strncmp (nextname, "/               ", 16) == 0)
    return bfd_slurp_armap (abfd);

  /* See if the first element is an armap.  */
  if (strncmp (nextname, ecoff_backend (abfd)->armap_start,
	       ARMAP_START_LENGTH) != 0
      || nextname[ARMAP_HEADER_MARKER_INDEX] != ARMAP_MARKER
      || (nextname[ARMAP_HEADER_ENDIAN_INDEX] != ARMAP_BIG_ENDIAN
	  && nextname[ARMAP_HEADER_ENDIAN_INDEX] != ARMAP_LITTLE_ENDIAN)
      || nextname[ARMAP_OBJECT_MARKER_INDEX] != ARMAP_MARKER
      || (nextname[ARMAP_OBJECT_ENDIAN_INDEX] != ARMAP_BIG_ENDIAN
	  && nextname[ARMAP_OBJECT_ENDIAN_INDEX] != ARMAP_LITTLE_ENDIAN)
      || strncmp (nextname + ARMAP_END_INDEX,
		  ARMAP_END, sizeof ARMAP_END - 1) != 0)
    {
      bfd_has_map (abfd) = false;
      return true;
    }

  /* Make sure we have the right byte ordering.  */
  if (((nextname[ARMAP_HEADER_ENDIAN_INDEX] == ARMAP_BIG_ENDIAN)
       ^ (abfd->xvec->header_byteorder_big_p != false))
      || ((nextname[ARMAP_OBJECT_ENDIAN_INDEX] == ARMAP_BIG_ENDIAN)
	  ^ (abfd->xvec->byteorder_big_p != false)))
    {
      bfd_error = wrong_format;
      return false;
    }

  /* Read in the armap.  */
  ardata = bfd_ardata (abfd);
  mapdata = snarf_ar_hdr (abfd);
  if (mapdata == (struct areltdata *) NULL)
    return false;
  parsed_size = mapdata->parsed_size;
  bfd_release (abfd, (PTR) mapdata);
    
  raw_armap = (char *) bfd_alloc (abfd, parsed_size);
  if (raw_armap == (char *) NULL)
    {
      bfd_error = no_memory;
      return false;
    }
    
  if (bfd_read ((PTR) raw_armap, 1, parsed_size, abfd) != parsed_size)
    {
      bfd_error = malformed_archive;
      bfd_release (abfd, (PTR) raw_armap);
      return false;
    }
    
  count = bfd_h_get_32 (abfd, (PTR) raw_armap);

  ardata->symdef_count = 0;
  ardata->cache = (struct ar_cache *) NULL;

  /* This code used to overlay the symdefs over the raw archive data,
     but that doesn't work on a 64 bit host.  */

  stringbase = raw_armap + count * 8 + 8;

#ifdef CHECK_ARMAP_HASH
  {
    unsigned int hlog;

    /* Double check that I have the hashing algorithm right by making
       sure that every symbol can be looked up successfully.  */
    hlog = 0;
    for (i = 1; i < count; i <<= 1)
      hlog++;
    BFD_ASSERT (i == count);

    raw_ptr = raw_armap + 4;
    for (i = 0; i < count; i++, raw_ptr += 8)
      {
	unsigned int name_offset, file_offset;
	unsigned int hash, rehash, srch;
      
	name_offset = bfd_h_get_32 (abfd, (PTR) raw_ptr);
	file_offset = bfd_h_get_32 (abfd, (PTR) (raw_ptr + 4));
	if (file_offset == 0)
	  continue;
	hash = ecoff_armap_hash (stringbase + name_offset, &rehash, count,
				 hlog);
	if (hash == i)
	  continue;

	/* See if we can rehash to this location.  */
	for (srch = (hash + rehash) & (count - 1);
	     srch != hash && srch != i;
	     srch = (srch + rehash) & (count - 1))
	  BFD_ASSERT (bfd_h_get_32 (abfd, (PTR) (raw_armap + 8 + srch * 8))
		      != 0);
	BFD_ASSERT (srch == i);
      }
  }

#endif /* CHECK_ARMAP_HASH */

  raw_ptr = raw_armap + 4;
  for (i = 0; i < count; i++, raw_ptr += 8)
    if (bfd_h_get_32 (abfd, (PTR) (raw_ptr + 4)) != 0)
      ++ardata->symdef_count;

  symdef_ptr = ((struct symdef *)
		bfd_alloc (abfd,
			   ardata->symdef_count * sizeof (struct symdef)));
  ardata->symdefs = (carsym *) symdef_ptr;

  raw_ptr = raw_armap + 4;
  for (i = 0; i < count; i++, raw_ptr += 8)
    {
      unsigned int name_offset, file_offset;

      file_offset = bfd_h_get_32 (abfd, (PTR) (raw_ptr + 4));
      if (file_offset == 0)
	continue;
      name_offset = bfd_h_get_32 (abfd, (PTR) raw_ptr);
      symdef_ptr->s.name = stringbase + name_offset;
      symdef_ptr->file_offset = file_offset;
      ++symdef_ptr;
    }

  ardata->first_file_filepos = bfd_tell (abfd);
  /* Pad to an even boundary.  */
  ardata->first_file_filepos += ardata->first_file_filepos % 2;

  bfd_has_map (abfd) = true;

  return true;
}

/* Write out an armap.  */

boolean
ecoff_write_armap (abfd, elength, map, orl_count, stridx)
     bfd *abfd;
     unsigned int elength;
     struct orl *map;
     unsigned int orl_count;
     int stridx;
{
  unsigned int hashsize, hashlog;
  unsigned int symdefsize;
  int padit;
  unsigned int stringsize;
  unsigned int mapsize;
  file_ptr firstreal;
  struct ar_hdr hdr;
  struct stat statbuf;
  unsigned int i;
  bfd_byte temp[4];
  bfd_byte *hashtable;
  bfd *current;
  bfd *last_elt;

  /* Ultrix appears to use as a hash table size the least power of two
     greater than twice the number of entries.  */
  for (hashlog = 0; (1 << hashlog) <= 2 * orl_count; hashlog++)
    ;
  hashsize = 1 << hashlog;

  symdefsize = hashsize * 8;
  padit = stridx % 2;
  stringsize = stridx + padit;

  /* Include 8 bytes to store symdefsize and stringsize in output. */
  mapsize = symdefsize + stringsize + 8;

  firstreal = SARMAG + sizeof (struct ar_hdr) + mapsize + elength;

  memset ((PTR) &hdr, 0, sizeof hdr);

  /* Work out the ECOFF armap name.  */
  strcpy (hdr.ar_name, ecoff_backend (abfd)->armap_start);
  hdr.ar_name[ARMAP_HEADER_MARKER_INDEX] = ARMAP_MARKER;
  hdr.ar_name[ARMAP_HEADER_ENDIAN_INDEX] =
    (abfd->xvec->header_byteorder_big_p
     ? ARMAP_BIG_ENDIAN
     : ARMAP_LITTLE_ENDIAN);
  hdr.ar_name[ARMAP_OBJECT_MARKER_INDEX] = ARMAP_MARKER;
  hdr.ar_name[ARMAP_OBJECT_ENDIAN_INDEX] =
    abfd->xvec->byteorder_big_p ? ARMAP_BIG_ENDIAN : ARMAP_LITTLE_ENDIAN;
  memcpy (hdr.ar_name + ARMAP_END_INDEX, ARMAP_END, sizeof ARMAP_END - 1);

  /* Write the timestamp of the archive header to be just a little bit
     later than the timestamp of the file, otherwise the linker will
     complain that the index is out of date.  Actually, the Ultrix
     linker just checks the archive name; the GNU linker may check the
     date.  */
  stat (abfd->filename, &statbuf);
  sprintf (hdr.ar_date, "%ld", (long) (statbuf.st_mtime + 60));

  /* The DECstation uses zeroes for the uid, gid and mode of the
     armap.  */
  hdr.ar_uid[0] = '0';
  hdr.ar_gid[0] = '0';
  hdr.ar_mode[0] = '0';

  sprintf (hdr.ar_size, "%-10d", (int) mapsize);

  hdr.ar_fmag[0] = '`';
  hdr.ar_fmag[1] = '\n';

  /* Turn all null bytes in the header into spaces.  */
  for (i = 0; i < sizeof (struct ar_hdr); i++)
   if (((char *)(&hdr))[i] == '\0')
     (((char *)(&hdr))[i]) = ' ';

  if (bfd_write ((PTR) &hdr, 1, sizeof (struct ar_hdr), abfd)
      != sizeof (struct ar_hdr))
    return false;

  bfd_h_put_32 (abfd, hashsize, temp);
  if (bfd_write (temp, 1, 4, abfd) != 4)
    return false;
  
  hashtable = (bfd_byte *) bfd_zalloc (abfd, symdefsize);

  current = abfd->archive_head;
  last_elt = current;
  for (i = 0; i < orl_count; i++)
    {
      unsigned int hash, rehash;

      /* Advance firstreal to the file position of this archive
	 element.  */
      if (((bfd *) map[i].pos) != last_elt)
	{
	  do
	    {
	      firstreal += arelt_size (current) + sizeof (struct ar_hdr);
	      firstreal += firstreal % 2;
	      current = current->next;
	    }
	  while (current != (bfd *) map[i].pos);
	}

      last_elt = current;

      hash = ecoff_armap_hash (*map[i].name, &rehash, hashsize, hashlog);
      if (bfd_h_get_32 (abfd, (PTR) (hashtable + (hash * 8) + 4)) != 0)
	{
	  unsigned int srch;

	  /* The desired slot is already taken.  */
	  for (srch = (hash + rehash) & (hashsize - 1);
	       srch != hash;
	       srch = (srch + rehash) & (hashsize - 1))
	    if (bfd_h_get_32 (abfd, (PTR) (hashtable + (srch * 8) + 4)) == 0)
	      break;

	  BFD_ASSERT (srch != hash);

	  hash = srch;
	}
	
      bfd_h_put_32 (abfd, map[i].namidx, (PTR) (hashtable + hash * 8));
      bfd_h_put_32 (abfd, firstreal, (PTR) (hashtable + hash * 8 + 4));
    }

  if (bfd_write (hashtable, 1, symdefsize, abfd) != symdefsize)
    return false;

  bfd_release (abfd, hashtable);

  /* Now write the strings.  */
  bfd_h_put_32 (abfd, stringsize, temp);
  if (bfd_write (temp, 1, 4, abfd) != 4)
    return false;
  for (i = 0; i < orl_count; i++)
    {
      bfd_size_type len;

      len = strlen (*map[i].name) + 1;
      if (bfd_write ((PTR) (*map[i].name), 1, len, abfd) != len)
	return false;
    }

  /* The spec sez this should be a newline.  But in order to be
     bug-compatible for DECstation ar we use a null.  */
  if (padit)
    {
      if (bfd_write ("\0", 1, 1, abfd) != 1)
	return false;
    }

  return true;
}

/* See whether this BFD is an archive.  If it is, read in the armap
   and the extended name table.  */

bfd_target *
ecoff_archive_p (abfd)
     bfd *abfd;
{
  char armag[SARMAG + 1];

  if (bfd_read ((PTR) armag, 1, SARMAG, abfd) != SARMAG
      || strncmp (armag, ARMAG, SARMAG) != 0)
    {
      bfd_error = wrong_format;
      return (bfd_target *) NULL;
    }

  /* We are setting bfd_ardata(abfd) here, but since bfd_ardata
     involves a cast, we can't do it as the left operand of
     assignment.  */
  abfd->tdata.aout_ar_data =
    (struct artdata *) bfd_zalloc (abfd, sizeof (struct artdata));

  if (bfd_ardata (abfd) == (struct artdata *) NULL)
    {
      bfd_error = no_memory;
      return (bfd_target *) NULL;
    }

  bfd_ardata (abfd)->first_file_filepos = SARMAG;
  
  if (ecoff_slurp_armap (abfd) == false
      || ecoff_slurp_extended_name_table (abfd) == false)
    {
      bfd_release (abfd, bfd_ardata (abfd));
      abfd->tdata.aout_ar_data = (struct artdata *) NULL;
      return (bfd_target *) NULL;
    }
  
  return abfd->xvec;
}
