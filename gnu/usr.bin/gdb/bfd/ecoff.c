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
#include "bfdlink.h"
#include "libbfd.h"
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
static long ecoff_sec_to_styp_flags PARAMS ((const char *name,
					     flagword flags));
static boolean ecoff_slurp_symbolic_header PARAMS ((bfd *abfd));
static boolean ecoff_set_symbol_info PARAMS ((bfd *abfd, SYMR *ecoff_sym,
					   asymbol *asym, int ext,
					   asymbol **indirect_ptr_ptr));
static void ecoff_emit_aggregate PARAMS ((bfd *abfd, FDR *fdr,
					  char *string,
					  RNDXR *rndx, long isym,
					  const char *which));
static char *ecoff_type_to_string PARAMS ((bfd *abfd, FDR *fdr,
					   unsigned int indx));
static boolean ecoff_slurp_reloc_table PARAMS ((bfd *abfd, asection *section,
						asymbol **symbols));
static void ecoff_compute_section_file_positions PARAMS ((bfd *abfd));
static bfd_size_type ecoff_compute_reloc_file_positions PARAMS ((bfd *abfd));
static boolean ecoff_get_extr PARAMS ((asymbol *, EXTR *));
static void ecoff_set_index PARAMS ((asymbol *, bfd_size_type));
static unsigned int ecoff_armap_hash PARAMS ((CONST char *s,
					      unsigned int *rehash,
					      unsigned int size,
					      unsigned int hlog));

/* This stuff is somewhat copied from coffcode.h.  */

static asection bfd_debug_section = { "*DEBUG*" };

/* Create an ECOFF object.  */

boolean
_bfd_ecoff_mkobject (abfd)
     bfd *abfd;
{
  abfd->tdata.ecoff_obj_data = ((struct ecoff_tdata *)
				bfd_zalloc (abfd, sizeof (ecoff_data_type)));
  if (abfd->tdata.ecoff_obj_data == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }

  return true;
}

/* This is a hook called by coff_real_object_p to create any backend
   specific information.  */

PTR
_bfd_ecoff_mkobject_hook (abfd, filehdr, aouthdr)
     bfd *abfd;
     PTR filehdr;
     PTR aouthdr;
{
  struct internal_filehdr *internal_f = (struct internal_filehdr *) filehdr;
  struct internal_aouthdr *internal_a = (struct internal_aouthdr *) aouthdr;
  ecoff_data_type *ecoff;

  if (_bfd_ecoff_mkobject (abfd) == false)
    return NULL;

  ecoff = ecoff_data (abfd);
  ecoff->gp_size = 8;
  ecoff->sym_filepos = internal_f->f_symptr;

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

/*ARGSUSED*/
asection *
_bfd_ecoff_make_section_hook (abfd, name)
     bfd *abfd;
     char *name;
{
  return (asection *) NULL;
}

/* Initialize a new section.  */

boolean
_bfd_ecoff_new_section_hook (abfd, section)
     bfd *abfd;
     asection *section;
{
  /* For the .pdata section, which has a special meaning on the Alpha,
     we set the alignment to 8.  We correct this later in
     ecoff_compute_section_file_positions.  We do this hackery because
     we need to know the exact unaligned size of the .pdata section in
     order to set the lnnoptr field correctly.  */
  if (strcmp (section->name, _PDATA) == 0)
    section->alignment_power = 3;
  else
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
  else if (strcmp (section->name, _LIB) == 0)
    {
      /* An Irix 4 shared libary.  */
      section->flags |= SEC_COFF_SHARED_LIBRARY;
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
_bfd_ecoff_set_arch_mach_hook (abfd, filehdr)
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
   This is the inverse of _bfd_ecoff_set_arch_mach_hook, above.  */

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

static long
ecoff_sec_to_styp_flags (name, flags)
     const char *name;
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
  else if (strcmp (name, _LITA) == 0)
    styp = STYP_LITA;
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
  else if (strcmp (name, _PDATA) == 0)
    styp = STYP_PDATA;
  else if (strcmp (name, _XDATA) == 0)
    styp = STYP_XDATA;
  else if (strcmp (name, _LIB) == 0)
    styp = STYP_ECOFF_LIB;
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

/*ARGSUSED*/
flagword
_bfd_ecoff_styp_to_sec_flags (abfd, hdr)
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
	sec_flags |= SEC_CODE | SEC_COFF_SHARED_LIBRARY;
      else
	sec_flags |= SEC_CODE | SEC_LOAD | SEC_ALLOC;
    }
  else if ((styp_flags & STYP_DATA)
	   || (styp_flags & STYP_RDATA)
	   || (styp_flags & STYP_SDATA)
	   || styp_flags == STYP_PDATA
	   || styp_flags == STYP_XDATA)
    {
      if (sec_flags & SEC_NEVER_LOAD)
	sec_flags |= SEC_DATA | SEC_COFF_SHARED_LIBRARY;
      else
	sec_flags |= SEC_DATA | SEC_LOAD | SEC_ALLOC;
      if ((styp_flags & STYP_RDATA)
	  || styp_flags == STYP_PDATA)
	sec_flags |= SEC_READONLY;
    }
  else if ((styp_flags & STYP_BSS)
	   || (styp_flags & STYP_SBSS))
    {
      sec_flags |= SEC_ALLOC;
    }
  else if ((styp_flags & STYP_INFO) || styp_flags == STYP_COMMENT)
    {
      sec_flags |= SEC_NEVER_LOAD;
    }
  else if ((styp_flags & STYP_LITA)
	   || (styp_flags & STYP_LIT8)
	   || (styp_flags & STYP_LIT4))
    {
      sec_flags |= SEC_DATA | SEC_LOAD | SEC_ALLOC | SEC_READONLY;
    }
  else if (styp_flags & STYP_ECOFF_LIB)
    {
      sec_flags |= SEC_COFF_SHARED_LIBRARY;
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
_bfd_ecoff_swap_tir_in (bigend, ext_copy, intern)
     int bigend;
     const struct tir_ext *ext_copy;
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
_bfd_ecoff_swap_tir_out (bigend, intern_copy, ext)
     int bigend;
     const TIR *intern_copy;
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
_bfd_ecoff_swap_rndx_in (bigend, ext_copy, intern)
     int bigend;
     const struct rndx_ext *ext_copy;
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
		  | ((unsigned int) ext->r_bits[3]
		     << RNDX_BITS3_INDEX_SH_LEFT_LITTLE);
  }

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort();
#endif
}

/* Swap out a relative symbol record.  BIGEND says whether it is in
   big-endian or little-endian format.*/

void
_bfd_ecoff_swap_rndx_out (bigend, intern_copy, ext)
     int bigend;
     const RNDXR *intern_copy;
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

/* Read in the symbolic header for an ECOFF object file.  */

static boolean
ecoff_slurp_symbolic_header (abfd)
     bfd *abfd;
{
  const struct ecoff_backend_data * const backend = ecoff_backend (abfd);
  bfd_size_type external_hdr_size;
  PTR raw = NULL;
  HDRR *internal_symhdr;

  /* See if we've already read it in.  */
  if (ecoff_data (abfd)->debug_info.symbolic_header.magic == 
      backend->debug_swap.sym_magic)
    return true;

  /* See whether there is a symbolic header.  */
  if (ecoff_data (abfd)->sym_filepos == 0)
    {
      bfd_get_symcount (abfd) = 0;
      return true;
    }

  /* At this point bfd_get_symcount (abfd) holds the number of symbols
     as read from the file header, but on ECOFF this is always the
     size of the symbolic information header.  It would be cleaner to
     handle this when we first read the file in coffgen.c.  */
  external_hdr_size = backend->debug_swap.external_hdr_size;
  if (bfd_get_symcount (abfd) != external_hdr_size)
    {
      bfd_set_error (bfd_error_bad_value);
      return false;
    }

  /* Read the symbolic information header.  */
  raw = (PTR) malloc ((size_t) external_hdr_size);
  if (raw == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      goto error_return;
    }

  if (bfd_seek (abfd, ecoff_data (abfd)->sym_filepos, SEEK_SET) == -1
      || (bfd_read (raw, external_hdr_size, 1, abfd)
	  != external_hdr_size))
    goto error_return;
  internal_symhdr = &ecoff_data (abfd)->debug_info.symbolic_header;
  (*backend->debug_swap.swap_hdr_in) (abfd, raw, internal_symhdr);

  if (internal_symhdr->magic != backend->debug_swap.sym_magic)
    {
      bfd_set_error (bfd_error_bad_value);
      goto error_return;
    }

  /* Now we can get the correct number of symbols.  */
  bfd_get_symcount (abfd) = (internal_symhdr->isymMax
			     + internal_symhdr->iextMax);

  if (raw != NULL)
    free (raw);
  return true;
 error_return:
  if (raw != NULL)
    free (raw);
  return false;
}

/* Read in and swap the important symbolic information for an ECOFF
   object file.  This is called by gdb via the read_debug_info entry
   point in the backend structure.  */

/*ARGSUSED*/
boolean
_bfd_ecoff_slurp_symbolic_info (abfd, ignore, debug)
     bfd *abfd;
     asection *ignore;
     struct ecoff_debug_info *debug;
{
  const struct ecoff_backend_data * const backend = ecoff_backend (abfd);
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

  BFD_ASSERT (debug == &ecoff_data (abfd)->debug_info);

  /* Check whether we've already gotten it, and whether there's any to
     get.  */
  if (ecoff_data (abfd)->raw_syments != (PTR) NULL)
    return true;
  if (ecoff_data (abfd)->sym_filepos == 0)
    {
      bfd_get_symcount (abfd) = 0;
      return true;
    }

  if (! ecoff_slurp_symbolic_header (abfd))
    return false;

  internal_symhdr = &debug->symbolic_header;

  /* Read all the symbolic information at once.  */
  raw_base = (ecoff_data (abfd)->sym_filepos
	      + backend->debug_swap.external_hdr_size);

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
  UPDATE_RAW_END (cbDnOffset, idnMax, backend->debug_swap.external_dnr_size);
  UPDATE_RAW_END (cbPdOffset, ipdMax, backend->debug_swap.external_pdr_size);
  UPDATE_RAW_END (cbSymOffset, isymMax, backend->debug_swap.external_sym_size);
  UPDATE_RAW_END (cbOptOffset, ioptMax, backend->debug_swap.external_opt_size);
  UPDATE_RAW_END (cbAuxOffset, iauxMax, sizeof (union aux_ext));
  UPDATE_RAW_END (cbSsOffset, issMax, sizeof (char));
  UPDATE_RAW_END (cbSsExtOffset, issExtMax, sizeof (char));
  UPDATE_RAW_END (cbFdOffset, ifdMax, backend->debug_swap.external_fdr_size);
  UPDATE_RAW_END (cbRfdOffset, crfd, backend->debug_swap.external_rfd_size);
  UPDATE_RAW_END (cbExtOffset, iextMax, backend->debug_swap.external_ext_size);

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
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  if (bfd_seek (abfd,
		(ecoff_data (abfd)->sym_filepos
		 + backend->debug_swap.external_hdr_size),
		SEEK_SET) != 0
      || bfd_read (raw, raw_size, 1, abfd) != raw_size)
    {
      bfd_release (abfd, raw);
      return false;
    }

  ecoff_data (abfd)->raw_syments = raw;

  /* Get pointers for the numeric offsets in the HDRR structure.  */
#define FIX(off1, off2, type) \
  if (internal_symhdr->off1 == 0) \
    debug->off2 = (type) NULL; \
  else \
    debug->off2 = (type) ((char *) raw \
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
  debug->fdr = (struct fdr *) bfd_alloc (abfd,
					 (internal_symhdr->ifdMax *
					  sizeof (struct fdr)));
  if (debug->fdr == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  external_fdr_size = backend->debug_swap.external_fdr_size;
  fdr_ptr = debug->fdr;
  fraw_src = (char *) debug->external_fdr;
  fraw_end = fraw_src + internal_symhdr->ifdMax * external_fdr_size;
  for (; fraw_src < fraw_end; fraw_src += external_fdr_size, fdr_ptr++)
    (*backend->debug_swap.swap_fdr_in) (abfd, (PTR) fraw_src, fdr_ptr);

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
_bfd_ecoff_make_empty_symbol (abfd)
     bfd *abfd;
{
  ecoff_symbol_type *new;

  new = (ecoff_symbol_type *) bfd_alloc (abfd, sizeof (ecoff_symbol_type));
  if (new == (ecoff_symbol_type *) NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return (asymbol *) NULL;
    }
  memset ((PTR) new, 0, sizeof *new);
  new->symbol.section = (asection *) NULL;
  new->fdr = (FDR *) NULL;
  new->local = false;
  new->native = NULL;
  new->symbol.the_bfd = abfd;
  return &new->symbol;
}

/* Set the BFD flags and section for an ECOFF symbol.  */

static boolean
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
      asym->section = bfd_und_section_ptr;
      *indirect_ptr_ptr = NULL;
      return true;
    }

  if (ECOFF_IS_STAB (ecoff_sym)
      && (ECOFF_UNMARK_STAB (ecoff_sym->index) | N_EXT) == (N_INDR | N_EXT))
    {
      asym->flags = BSF_DEBUGGING | BSF_INDIRECT;
      asym->section = bfd_ind_section_ptr;
      /* Pass this symbol on to the next call to this function.  */
      *indirect_ptr_ptr = asym;
      return true;
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
	  return true;
	}
      break;
    default:
      asym->flags = BSF_DEBUGGING;
      return true;
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
      asym->section = bfd_abs_section_ptr;
      break;
    case scUndefined:
      asym->section = bfd_und_section_ptr;
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
	  asym->section = bfd_com_section_ptr;
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
      asym->section = bfd_und_section_ptr;
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
		if (!copy)
		  {
		    bfd_set_error (bfd_error_no_memory);
		    return false;
		  }
		strcpy (copy, name);
		section = bfd_make_section (abfd, copy);
	      }

	    /* Build a reloc pointing to this constructor.  */
	    reloc_chain =
	      (arelent_chain *) bfd_alloc (abfd, sizeof (arelent_chain));
	    if (!reloc_chain)
	      {
		bfd_set_error (bfd_error_no_memory);
		return false;
	      }
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
  return true;
}

/* Read an ECOFF symbol table.  */

boolean
_bfd_ecoff_slurp_symbol_table (abfd)
     bfd *abfd;
{
  const struct ecoff_backend_data * const backend = ecoff_backend (abfd);
  const bfd_size_type external_ext_size
    = backend->debug_swap.external_ext_size;
  const bfd_size_type external_sym_size
    = backend->debug_swap.external_sym_size;
  void (* const swap_ext_in) PARAMS ((bfd *, PTR, EXTR *))
    = backend->debug_swap.swap_ext_in;
  void (* const swap_sym_in) PARAMS ((bfd *, PTR, SYMR *))
    = backend->debug_swap.swap_sym_in;
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
  if (! _bfd_ecoff_slurp_symbolic_info (abfd, (asection *) NULL,
					&ecoff_data (abfd)->debug_info))
    return false;
  if (bfd_get_symcount (abfd) == 0)
    return true;

  internal_size = bfd_get_symcount (abfd) * sizeof (ecoff_symbol_type);
  internal = (ecoff_symbol_type *) bfd_alloc (abfd, internal_size);
  if (internal == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }

  internal_ptr = internal;
  indirect_ptr = NULL;
  eraw_src = (char *) ecoff_data (abfd)->debug_info.external_ext;
  eraw_end = (eraw_src
	      + (ecoff_data (abfd)->debug_info.symbolic_header.iextMax
		 * external_ext_size));
  for (; eraw_src < eraw_end; eraw_src += external_ext_size, internal_ptr++)
    {
      EXTR internal_esym;

      (*swap_ext_in) (abfd, (PTR) eraw_src, &internal_esym);
      internal_ptr->symbol.name = (ecoff_data (abfd)->debug_info.ssext
				   + internal_esym.asym.iss);
      if (!ecoff_set_symbol_info (abfd, &internal_esym.asym,
			     &internal_ptr->symbol, 1, &indirect_ptr))
	return false;
      /* The alpha uses a negative ifd field for section symbols.  */
      if (internal_esym.ifd >= 0)
	internal_ptr->fdr = (ecoff_data (abfd)->debug_info.fdr
			     + internal_esym.ifd);
      else
	internal_ptr->fdr = NULL;
      internal_ptr->local = false;
      internal_ptr->native = (PTR) eraw_src;
    }
  BFD_ASSERT (indirect_ptr == (asymbol *) NULL);

  /* The local symbols must be accessed via the fdr's, because the
     string and aux indices are relative to the fdr information.  */
  fdr_ptr = ecoff_data (abfd)->debug_info.fdr;
  fdr_end = fdr_ptr + ecoff_data (abfd)->debug_info.symbolic_header.ifdMax;
  for (; fdr_ptr < fdr_end; fdr_ptr++)
    {
      char *lraw_src;
      char *lraw_end;

      lraw_src = ((char *) ecoff_data (abfd)->debug_info.external_sym
		  + fdr_ptr->isymBase * external_sym_size);
      lraw_end = lraw_src + fdr_ptr->csym * external_sym_size;
      for (;
	   lraw_src < lraw_end;
	   lraw_src += external_sym_size, internal_ptr++)
	{
	  SYMR internal_sym;

	  (*swap_sym_in) (abfd, (PTR) lraw_src, &internal_sym);
	  internal_ptr->symbol.name = (ecoff_data (abfd)->debug_info.ss
				       + fdr_ptr->issBase
				       + internal_sym.iss);
	  if (!ecoff_set_symbol_info (abfd, &internal_sym,
				      &internal_ptr->symbol, 0, &indirect_ptr))
	    return false;
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

long
_bfd_ecoff_get_symtab_upper_bound (abfd)
     bfd *abfd;
{
  if (! _bfd_ecoff_slurp_symbolic_info (abfd, (asection *) NULL,
					&ecoff_data (abfd)->debug_info))
    return -1;

  if (bfd_get_symcount (abfd) == 0)
    return 0;

  return (bfd_get_symcount (abfd) + 1) * (sizeof (ecoff_symbol_type *));
}

/* Get the canonical symbols.  */

long
_bfd_ecoff_get_symtab (abfd, alocation)
     bfd *abfd;
     asymbol **alocation;
{
  unsigned int counter = 0;
  ecoff_symbol_type *symbase;
  ecoff_symbol_type **location = (ecoff_symbol_type **) alocation;

  if (_bfd_ecoff_slurp_symbol_table (abfd) == false)
    return -1;
  if (bfd_get_symcount (abfd) == 0)
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
ecoff_emit_aggregate (abfd, fdr, string, rndx, isym, which)
     bfd *abfd;
     FDR *fdr;
     char *string;
     RNDXR *rndx;
     long isym;
     const char *which;
{
  const struct ecoff_debug_swap * const debug_swap =
    &ecoff_backend (abfd)->debug_swap;
  struct ecoff_debug_info * const debug_info = &ecoff_data (abfd)->debug_info;
  unsigned int ifd = rndx->rfd;
  unsigned int indx = rndx->index;
  const char *name;
  
  if (ifd == 0xfff)
    ifd = isym;

  /* An ifd of -1 is an opaque type.  An escaped index of 0 is a
     struct return type of a procedure compiled without -g.  */
  if (ifd == 0xffffffff
      || (rndx->rfd == 0xfff && indx == 0))
    name = "<undefined>";
  else if (indx == indexNil)
    name = "<no name>";
  else
    {
      SYMR sym;

      if (debug_info->external_rfd == NULL)
	fdr = debug_info->fdr + ifd;
      else
	{
	  RFDT rfd;

	  (*debug_swap->swap_rfd_in) (abfd,
				      ((char *) debug_info->external_rfd
				       + ((fdr->rfdBase + ifd)
					  * debug_swap->external_rfd_size)),
				      &rfd);
	  fdr = debug_info->fdr + rfd;
	}

      indx += fdr->isymBase;

      (*debug_swap->swap_sym_in) (abfd,
				  ((char *) debug_info->external_sym
				   + indx * debug_swap->external_sym_size),
				  &sym);

      name = debug_info->ss + fdr->issBase + sym.iss;
    }

  sprintf (string,
	   "%s %s { ifd = %u, index = %lu }",
	   which, name, ifd,
	   ((long) indx
	    + debug_info->symbolic_header.iextMax));
}

/* Convert the type information to string format.  */

static char *
ecoff_type_to_string (abfd, fdr, indx)
     bfd *abfd;
     FDR *fdr;
     unsigned int indx;
{
  union aux_ext *aux_ptr;
  int bigendian;
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

  aux_ptr = ecoff_data (abfd)->debug_info.external_aux + fdr->iauxBase;
  bigendian = fdr->fBigendian;

  for (i = 0; i < 7; i++)
    {
      qualifiers[i].low_bound = 0;
      qualifiers[i].high_bound = 0;
      qualifiers[i].stride = 0;
    }

  if (AUX_GET_ISYM (bigendian, &aux_ptr[indx]) == -1)
    return "-1 (no type)";
  _bfd_ecoff_swap_tir_in (bigendian, &aux_ptr[indx++].a_ti, &u.ti);

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
      _bfd_ecoff_swap_rndx_in (bigendian, &aux_ptr[indx].a_rndx, &rndx);
      ecoff_emit_aggregate (abfd, fdr, p1, &rndx,
			    (long) AUX_GET_ISYM (bigendian, &aux_ptr[indx+1]),
			    "struct");
      indx++;			/* skip aux words */
      break;

      /* Unions add 1-2 aux words:
	 1st word is [ST_RFDESCAPE, offset] pointer to union def;
	 2nd word is file index if 1st word rfd is ST_RFDESCAPE.  */

    case btUnion:		/* Union */
      _bfd_ecoff_swap_rndx_in (bigendian, &aux_ptr[indx].a_rndx, &rndx);
      ecoff_emit_aggregate (abfd, fdr, p1, &rndx,
			    (long) AUX_GET_ISYM (bigendian, &aux_ptr[indx+1]),
			    "union");
      indx++;			/* skip aux words */
      break;

      /* Enumerations add 1-2 aux words:
	 1st word is [ST_RFDESCAPE, offset] pointer to enum def;
	 2nd word is file index if 1st word rfd is ST_RFDESCAPE.  */

    case btEnum:		/* Enumeration */
      _bfd_ecoff_swap_rndx_in (bigendian, &aux_ptr[indx].a_rndx, &rndx);
      ecoff_emit_aggregate (abfd, fdr, p1, &rndx,
			    (long) AUX_GET_ISYM (bigendian, &aux_ptr[indx+1]),
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

/*ARGSUSED*/
void
_bfd_ecoff_get_symbol_info (abfd, symbol, ret)
     bfd *abfd;			/* Ignored.  */
     asymbol *symbol;
     symbol_info *ret;
{
  bfd_symbol_info (symbol, ret);
}

/* Print information about an ECOFF symbol.  */

void
_bfd_ecoff_print_symbol (abfd, filep, symbol, how)
     bfd *abfd;
     PTR filep;
     asymbol *symbol;
     bfd_print_symbol_type how;
{
  const struct ecoff_debug_swap * const debug_swap
    = &ecoff_backend (abfd)->debug_swap;
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
	
	  (*debug_swap->swap_sym_in) (abfd, ecoffsymbol (symbol)->native,
				      &ecoff_sym);
	  fprintf (file, "ecoff local ");
	  fprintf_vma (file, (bfd_vma) ecoff_sym.value);
	  fprintf (file, " %x %x", (unsigned) ecoff_sym.st,
		   (unsigned) ecoff_sym.sc);
	}
      else
	{
	  EXTR ecoff_ext;

	  (*debug_swap->swap_ext_in) (abfd, ecoffsymbol (symbol)->native,
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
	    (*debug_swap->swap_sym_in) (abfd, ecoffsymbol (symbol)->native,
					&ecoff_ext.asym);
	    type = 'l';
	    pos = ((((char *) ecoffsymbol (symbol)->native
		     - (char *) ecoff_data (abfd)->debug_info.external_sym)
		    / debug_swap->external_sym_size)
		   + ecoff_data (abfd)->debug_info.symbolic_header.iextMax);
	    jmptbl = ' ';
	    cobol_main = ' ';
	    weakext = ' ';
	  }
	else
	  {
	    (*debug_swap->swap_ext_in) (abfd, ecoffsymbol (symbol)->native,
					&ecoff_ext);
	    type = 'e';
	    pos = (((char *) ecoffsymbol (symbol)->native
		    - (char *) ecoff_data (abfd)->debug_info.external_ext)
		   / debug_swap->external_ext_size);
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
	    FDR *fdr;
	    unsigned int indx;
	    int bigendian;
	    bfd_size_type sym_base;
	    union aux_ext *aux_base;

	    fdr = ecoffsymbol (symbol)->fdr;
	    indx = ecoff_ext.asym.index;

	    /* sym_base is used to map the fdr relative indices which
	       appear in the file to the position number which we are
	       using.  */
	    sym_base = fdr->isymBase;
	    if (ecoffsymbol (symbol)->local)
	      sym_base +=
		ecoff_data (abfd)->debug_info.symbolic_header.iextMax;

	    /* aux_base is the start of the aux entries for this file;
	       asym.index is an offset from this.  */
	    aux_base = (ecoff_data (abfd)->debug_info.external_aux
			+ fdr->iauxBase);

	    /* The aux entries are stored in host byte order; the
	       order is indicated by a bit in the fdr.  */
	    bigendian = fdr->fBigendian;

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
			   ((long)
			    (AUX_GET_ISYM (bigendian,
					   &aux_base[ecoff_ext.asym.index])
			     + sym_base)));
		break;

	      case stProc:
	      case stStaticProc:
		if (ECOFF_IS_STAB (&ecoff_ext.asym))
		  ;
		else if (ecoffsymbol (symbol)->local)
		  fprintf (file, "\n      End+1 symbol: %-7ld   Type:  %s",
			   ((long)
			    (AUX_GET_ISYM (bigendian,
					   &aux_base[ecoff_ext.asym.index])
			     + sym_base)),
			   ecoff_type_to_string (abfd, fdr, indx + 1));
		else
		  fprintf (file, "\n      Local symbol: %ld",
			   ((long) indx
			    + (long) sym_base
			    + (ecoff_data (abfd)
			       ->debug_info.symbolic_header.iextMax)));
		break;

	      case stStruct:
		fprintf (file, "\n      struct; End+1 symbol: %ld",
			 (long) (indx + sym_base));
		break;

	      case stUnion:
		fprintf (file, "\n      union; End+1 symbol: %ld",
			 (long) (indx + sym_base));
		break;

	      case stEnum:
		fprintf (file, "\n      enum; End+1 symbol: %ld",
			 (long) (indx + sym_base));
		break;

	      default:
		if (! ECOFF_IS_STAB (&ecoff_ext.asym))
		  fprintf (file, "\n      Type: %s",
			   ecoff_type_to_string (abfd, fdr, indx));
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

  if (_bfd_ecoff_slurp_symbol_table (abfd) == false)
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
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  if (bfd_seek (abfd, section->rel_filepos, SEEK_SET) != 0)
    return false;
  if (bfd_read (external_relocs, 1, external_relocs_size, abfd)
      != external_relocs_size)
    return false;

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
			  < (ecoff_data (abfd)
			     ->debug_info.symbolic_header.iextMax)));
	  rptr->sym_ptr_ptr = symbols + intern.r_symndx;
	  rptr->addend = 0;
	}
      else if (intern.r_symndx == RELOC_SECTION_NONE
	       || intern.r_symndx == RELOC_SECTION_ABS)
	{
	  rptr->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
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
	    case RELOC_SECTION_FINI:  sec_name = ".fini"; break;
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
      (*backend->adjust_reloc_in) (abfd, &intern, rptr);
    }

  bfd_release (abfd, external_relocs);

  section->relocation = internal_relocs;

  return true;
}

/* Get a canonical list of relocs.  */

long
_bfd_ecoff_canonicalize_reloc (abfd, section, relptr, symbols)
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
	return -1;

      tblptr = section->relocation;

      for (count = 0; count < section->reloc_count; count++)
	*relptr++ = tblptr++;
    }

  *relptr = (arelent *) NULL;

  return section->reloc_count;
}

/* Provided a BFD, a section and an offset into the section, calculate
   and return the name of the source file and the line nearest to the
   wanted location.  */

/*ARGSUSED*/
boolean
_bfd_ecoff_find_nearest_line (abfd, section, ignore_symbols, offset,
			      filename_ptr, functionname_ptr, retline_ptr)
     bfd *abfd;
     asection *section;
     asymbol **ignore_symbols;
     bfd_vma offset;
     CONST char **filename_ptr;
     CONST char **functionname_ptr;
     unsigned int *retline_ptr;
{
  const struct ecoff_debug_swap * const debug_swap
    = &ecoff_backend (abfd)->debug_swap;
  FDR *fdr_ptr;
  FDR *fdr_start;
  FDR *fdr_end;
  FDR *fdr_hold;
  bfd_size_type external_pdr_size;
  char *pdr_ptr;
  char *pdr_end;
  PDR pdr;
  bfd_vma first_off;
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
  if (! _bfd_ecoff_slurp_symbolic_info (abfd, (asection *) NULL,
					&ecoff_data (abfd)->debug_info)
      || bfd_get_symcount (abfd) == 0)
    return false;

  /* Each file descriptor (FDR) has a memory address.  Here we track
     down which FDR we want.  The FDR's are stored in increasing
     memory order.  If speed is ever important, this can become a
     binary search.  We must ignore FDR's with no PDR entries; they
     will have the adr of the FDR before or after them.  */
  fdr_start = ecoff_data (abfd)->debug_info.fdr;
  fdr_end = fdr_start + ecoff_data (abfd)->debug_info.symbolic_header.ifdMax;
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
  external_pdr_size = debug_swap->external_pdr_size;
  pdr_ptr = ((char *) ecoff_data (abfd)->debug_info.external_pdr
	     + fdr_ptr->ipdFirst * external_pdr_size);
  pdr_end = pdr_ptr + fdr_ptr->cpd * external_pdr_size;
  (*debug_swap->swap_pdr_in) (abfd, (PTR) pdr_ptr, &pdr);
  if (offset < pdr.adr)
    return false;

  /* The address of the first PDR is an offset which applies to the
     addresses of all the PDR's.  */
  first_off = pdr.adr;

  for (pdr_ptr += external_pdr_size;
       pdr_ptr < pdr_end;
       pdr_ptr += external_pdr_size)
    {
      (*debug_swap->swap_pdr_in) (abfd, (PTR) pdr_ptr, &pdr);
      if (offset < pdr.adr)
	break;
    }

  /* Now we can look for the actual line number.  The line numbers are
     stored in a very funky format, which I won't try to describe.
     Note that right here pdr_ptr and pdr hold the PDR *after* the one
     we want; we need this to compute line_end.  */
  line_end = ecoff_data (abfd)->debug_info.line;
  if (pdr_ptr == pdr_end)
    line_end += fdr_ptr->cbLineOffset + fdr_ptr->cbLine;
  else
    line_end += fdr_ptr->cbLineOffset + pdr.cbLineOffset;

  /* Now change pdr and pdr_ptr to the one we want.  */
  pdr_ptr -= external_pdr_size;
  (*debug_swap->swap_pdr_in) (abfd, (PTR) pdr_ptr, &pdr);

  offset -= pdr.adr - first_off;
  lineno = pdr.lnLow;
  line_ptr = (ecoff_data (abfd)->debug_info.line
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

	  (*debug_swap->swap_ext_in)
	    (abfd,
	     ((char *) ecoff_data (abfd)->debug_info.external_ext
	      + pdr.isym * debug_swap->external_ext_size),
	     &proc_ext);
	  *functionname_ptr = (ecoff_data (abfd)->debug_info.ssext
			       + proc_ext.asym.iss);
	}
    }
  else
    {
      SYMR proc_sym;

      *filename_ptr = (ecoff_data (abfd)->debug_info.ss
		       + fdr_ptr->issBase
		       + fdr_ptr->rss);
      (*debug_swap->swap_sym_in)
	(abfd,
	 ((char *) ecoff_data (abfd)->debug_info.external_sym
	  + (fdr_ptr->isymBase + pdr.isym) * debug_swap->external_sym_size),
	 &proc_sym);
      *functionname_ptr = (ecoff_data (abfd)->debug_info.ss
			   + fdr_ptr->issBase
			   + proc_sym.iss);
    }
  if (lineno == ilineNil)
    lineno = 0;
  *retline_ptr = lineno;
  return true;
}

/* Copy private BFD data.  This is called by objcopy and strip.  We
   use it to copy the ECOFF debugging information from one BFD to the
   other.  It would be theoretically possible to represent the ECOFF
   debugging information in the symbol table.  However, it would be a
   lot of work, and there would be little gain (gas, gdb, and ld
   already access the ECOFF debugging information via the
   ecoff_debug_info structure, and that structure would have to be
   retained in order to support ECOFF debugging in MIPS ELF).

   The debugging information for the ECOFF external symbols comes from
   the symbol table, so this function only handles the other debugging
   information.  */

boolean
_bfd_ecoff_bfd_copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  struct ecoff_debug_info *iinfo = &ecoff_data (ibfd)->debug_info;
  struct ecoff_debug_info *oinfo = &ecoff_data (obfd)->debug_info;
  register int i;
  asymbol **sym_ptr_ptr;
  size_t c;
  boolean local;

  /* This function is selected based on the input vector.  We only
     want to copy information over if the output BFD also uses ECOFF
     format.  */
  if (bfd_get_flavour (obfd) != bfd_target_ecoff_flavour)
    return true;

  /* Copy the GP value and the register masks.  */
  ecoff_data (obfd)->gp = ecoff_data (ibfd)->gp;
  ecoff_data (obfd)->gprmask = ecoff_data (ibfd)->gprmask;
  ecoff_data (obfd)->fprmask = ecoff_data (ibfd)->fprmask;
  for (i = 0; i < 3; i++)
    ecoff_data (obfd)->cprmask[i] = ecoff_data (ibfd)->cprmask[i];

  /* Copy the version stamp.  */
  oinfo->symbolic_header.vstamp = iinfo->symbolic_header.vstamp;

  /* If there are no symbols, don't copy any debugging information.  */
  c = bfd_get_symcount (obfd);
  sym_ptr_ptr = bfd_get_outsymbols (obfd);
  if (c == 0 || sym_ptr_ptr == (asymbol **) NULL)
    return true;

  /* See if there are any local symbols.  */
  local = false;
  for (; c > 0; c--, sym_ptr_ptr++)
    {
      if (ecoffsymbol (*sym_ptr_ptr)->local)
	{
	  local = true;
	  break;
	}
    }

  if (local)
    {
      /* There are some local symbols.  We just bring over all the
	 debugging information.  FIXME: This is not quite the right
	 thing to do.  If the user has asked us to discard all
	 debugging information, then we are probably going to wind up
	 keeping it because there will probably be some local symbol
	 which objcopy did not discard.  We should actually break
	 apart the debugging information and only keep that which
	 applies to the symbols we want to keep.  */
      oinfo->symbolic_header.ilineMax = iinfo->symbolic_header.ilineMax;
      oinfo->symbolic_header.cbLine = iinfo->symbolic_header.cbLine;
      oinfo->line = iinfo->line;

      oinfo->symbolic_header.idnMax = iinfo->symbolic_header.idnMax;
      oinfo->external_dnr = iinfo->external_dnr;

      oinfo->symbolic_header.ipdMax = iinfo->symbolic_header.ipdMax;
      oinfo->external_pdr = iinfo->external_pdr;

      oinfo->symbolic_header.isymMax = iinfo->symbolic_header.isymMax;
      oinfo->external_sym = iinfo->external_sym;

      oinfo->symbolic_header.ioptMax = iinfo->symbolic_header.ioptMax;
      oinfo->external_opt = iinfo->external_opt;

      oinfo->symbolic_header.iauxMax = iinfo->symbolic_header.iauxMax;
      oinfo->external_aux = iinfo->external_aux;

      oinfo->symbolic_header.issMax = iinfo->symbolic_header.issMax;
      oinfo->ss = iinfo->ss;

      oinfo->symbolic_header.ifdMax = iinfo->symbolic_header.ifdMax;
      oinfo->external_fdr = iinfo->external_fdr;

      oinfo->symbolic_header.crfd = iinfo->symbolic_header.crfd;
      oinfo->external_rfd = iinfo->external_rfd;
    }
  else
    {
      /* We are discarding all the local symbol information.  Look
	 through the external symbols and remove all references to FDR
	 or aux information.  */
      c = bfd_get_symcount (obfd);
      sym_ptr_ptr = bfd_get_outsymbols (obfd);
      for (; c > 0; c--, sym_ptr_ptr++)
	{
	  EXTR esym;

	  (*(ecoff_backend (obfd)->debug_swap.swap_ext_in))
	    (obfd, ecoffsymbol (*sym_ptr_ptr)->native, &esym);
	  esym.ifd = ifdNil;
	  esym.asym.index = indexNil;
	  (*(ecoff_backend (obfd)->debug_swap.swap_ext_out))
	    (obfd, &esym, ecoffsymbol (*sym_ptr_ptr)->native);
	}
    }

  return true;
}

/* Set the architecture.  The supported architecture is stored in the
   backend pointer.  We always set the architecture anyhow, since many
   callers ignore the return value.  */

boolean
_bfd_ecoff_set_arch_mach (abfd, arch, machine)
     bfd *abfd;
     enum bfd_architecture arch;
     unsigned long machine;
{
  bfd_default_set_arch_mach (abfd, arch, machine);
  return arch == ecoff_backend (abfd)->arch;
}

/* Get the size of the section headers.  */

/*ARGSUSED*/
int
_bfd_ecoff_sizeof_headers (abfd, reloc)
     bfd *abfd;
     boolean reloc;
{
  asection *current;
  int c;
  int ret;

  c = 0;
  for (current = abfd->sections;
       current != (asection *)NULL; 
       current = current->next) 
    ++c;

  ret = (bfd_coff_filhsz (abfd)
	 + bfd_coff_aoutsz (abfd)
	 + c * bfd_coff_scnhsz (abfd));
  return BFD_ALIGN (ret, 16);
}

/* Get the contents of a section.  */

boolean
_bfd_ecoff_get_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     asection *section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  return _bfd_generic_get_section_contents (abfd, section, location,
					    offset, count);
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

  sofar = _bfd_ecoff_sizeof_headers (abfd, false);

  first_data = true;
  for (current = abfd->sections;
       current != (asection *) NULL;
       current = current->next)
    {
      unsigned int alignment_power;

      /* Only deal with sections which have contents */
      if ((current->flags & (SEC_HAS_CONTENTS | SEC_LOAD)) == 0)
	continue;

      /* For the Alpha ECOFF .pdata section the lnnoptr field is
	 supposed to indicate the number of .pdata entries that are
	 really in the section.  Each entry is 8 bytes.  We store this
	 away in line_filepos before increasing the section size.  */
      if (strcmp (current->name, _PDATA) != 0)
	alignment_power = current->alignment_power;
      else
	{
	  current->line_filepos = current->_raw_size / 8;
	  alignment_power = 4;
	}

      /* On Ultrix, the data sections in an executable file must be
	 aligned to a page boundary within the file.  This does not
	 affect the section size, though.  FIXME: Does this work for
	 other platforms?  It requires some modification for the
	 Alpha, because .rdata on the Alpha goes with the text, not
	 the data.  */
      if ((abfd->flags & EXEC_P) != 0
	  && (abfd->flags & D_PAGED) != 0
	  && first_data != false
	  && (current->flags & SEC_CODE) == 0
	  && (! ecoff_backend (abfd)->rdata_in_text
	      || strcmp (current->name, _RDATA) != 0)
	  && strcmp (current->name, _PDATA) != 0)
	{
	  const bfd_vma round = ecoff_backend (abfd)->round;

	  sofar = (sofar + round - 1) &~ (round - 1);
	  first_data = false;
	}
      else if (strcmp (current->name, _LIB) == 0)
	{
	  const bfd_vma round = ecoff_backend (abfd)->round;
	  /* On Irix 4, the location of contents of the .lib section
	     from a shared library section is also rounded up to a
	     page boundary.  */

	  sofar = (sofar + round - 1) &~ (round - 1);
	}

      /* Align the sections in the file to the same boundary on
	 which they are aligned in virtual memory.  */
      old_sofar = sofar;
      sofar = BFD_ALIGN (sofar, 1 << alignment_power);

      current->filepos = sofar;

      sofar += current->_raw_size;

      /* make sure that this section is of the right size too */
      old_sofar = sofar;
      sofar = BFD_ALIGN (sofar, 1 << alignment_power);
      current->_raw_size += sofar - old_sofar;
    }

  ecoff_data (abfd)->reloc_filepos = sofar;
}

/* Determine the location of the relocs for all the sections in the
   output file, as well as the location of the symbolic debugging
   information.  */

static bfd_size_type
ecoff_compute_reloc_file_positions (abfd)
     bfd *abfd;
{
  const bfd_size_type external_reloc_size =
    ecoff_backend (abfd)->external_reloc_size;
  file_ptr reloc_base;
  bfd_size_type reloc_size;
  asection *current;
  file_ptr sym_base;

  if (! abfd->output_has_begun)
    {
      ecoff_compute_section_file_positions (abfd);
      abfd->output_has_begun = true;
    }
  
  reloc_base = ecoff_data (abfd)->reloc_filepos;

  reloc_size = 0;
  for (current = abfd->sections;
       current != (asection *)NULL; 
       current = current->next) 
    {
      if (current->reloc_count == 0)
	current->rel_filepos = 0;
      else
	{
	  bfd_size_type relsize;

	  current->rel_filepos = reloc_base;
	  relsize = current->reloc_count * external_reloc_size;
	  reloc_size += relsize;
	  reloc_base += relsize;
	}
    }

  sym_base = ecoff_data (abfd)->reloc_filepos + reloc_size;

  /* At least on Ultrix, the symbol table of an executable file must
     be aligned to a page boundary.  FIXME: Is this true on other
     platforms?  */
  if ((abfd->flags & EXEC_P) != 0
      && (abfd->flags & D_PAGED) != 0)
    sym_base = ((sym_base + ecoff_backend (abfd)->round - 1)
		&~ (ecoff_backend (abfd)->round - 1));

  ecoff_data (abfd)->sym_filepos = sym_base;

  return reloc_size;
}

/* Set the contents of a section.  */

boolean
_bfd_ecoff_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     asection *section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  /* This must be done first, because bfd_set_section_contents is
     going to set output_has_begun to true.  */
  if (abfd->output_has_begun == false)
    ecoff_compute_section_file_positions (abfd);

  /* If this is a .lib section, bump the vma address so that it winds
     up being the number of .lib sections output.  This is right for
     Irix 4.  Ian Taylor <ian@cygnus.com>.  */
  if (strcmp (section->name, _LIB) == 0)
    ++section->vma;

  if (count == 0)
    return true;

  if (bfd_seek (abfd, (file_ptr) (section->filepos + offset), SEEK_SET) != 0
      || bfd_write (location, 1, count, abfd) != count)
    return false;

  return true;
}

/* Get the GP value for an ECOFF file.  This is a hook used by
   nlmconv.  */

bfd_vma
bfd_ecoff_get_gp_value (abfd)
     bfd *abfd;
{
  if (bfd_get_flavour (abfd) != bfd_target_ecoff_flavour
      || bfd_get_format (abfd) != bfd_object)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return 0;
    }
  
  return ecoff_data (abfd)->gp;
}

/* Set the GP value for an ECOFF file.  This is a hook used by the
   assembler.  */

boolean
bfd_ecoff_set_gp_value (abfd, gp_value)
     bfd *abfd;
     bfd_vma gp_value;
{
  if (bfd_get_flavour (abfd) != bfd_target_ecoff_flavour
      || bfd_get_format (abfd) != bfd_object)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  ecoff_data (abfd)->gp = gp_value;

  return true;
}

/* Set the register masks for an ECOFF file.  This is a hook used by
   the assembler.  */

boolean
bfd_ecoff_set_regmasks (abfd, gprmask, fprmask, cprmask)
     bfd *abfd;
     unsigned long gprmask;
     unsigned long fprmask;
     unsigned long *cprmask;
{
  ecoff_data_type *tdata;

  if (bfd_get_flavour (abfd) != bfd_target_ecoff_flavour
      || bfd_get_format (abfd) != bfd_object)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  tdata = ecoff_data (abfd);
  tdata->gprmask = gprmask;
  tdata->fprmask = fprmask;
  if (cprmask != (unsigned long *) NULL)
    {
      register int i;

      for (i = 0; i < 3; i++)
	tdata->cprmask[i] = cprmask[i];
    }

  return true;
}

/* Get ECOFF EXTR information for an external symbol.  This function
   is passed to bfd_ecoff_debug_externals.  */

static boolean
ecoff_get_extr (sym, esym)
     asymbol *sym;
     EXTR *esym;
{
  ecoff_symbol_type *ecoff_sym_ptr;
  bfd *input_bfd;

  if (bfd_asymbol_flavour (sym) != bfd_target_ecoff_flavour
      || ecoffsymbol (sym)->native == NULL)
    {
      /* Don't include debugging, local, or section symbols.  */
      if ((sym->flags & BSF_DEBUGGING) != 0
	  || (sym->flags & BSF_LOCAL) != 0
	  || (sym->flags & BSF_SECTION_SYM) != 0)
	return false;

      esym->jmptbl = 0;
      esym->cobol_main = 0;
      esym->weakext = 0;
      esym->reserved = 0;
      esym->ifd = ifdNil;
      /* FIXME: we can do better than this for st and sc.  */
      esym->asym.st = stGlobal;
      esym->asym.sc = scAbs;
      esym->asym.reserved = 0;
      esym->asym.index = indexNil;
      return true;
    }

  ecoff_sym_ptr = ecoffsymbol (sym);

  if (ecoff_sym_ptr->local)
    return false;

  input_bfd = bfd_asymbol_bfd (sym);
  (*(ecoff_backend (input_bfd)->debug_swap.swap_ext_in))
    (input_bfd, ecoff_sym_ptr->native, esym);

  /* If the symbol was defined by the linker, then esym will be
     undefined but sym will not be.  Get a better class for such a
     symbol.  */
  if ((esym->asym.sc == scUndefined
       || esym->asym.sc == scSUndefined)
      && ! bfd_is_und_section (bfd_get_section (sym)))
    esym->asym.sc = scAbs;

  /* Adjust the FDR index for the symbol by that used for the input
     BFD.  */
  if (esym->ifd != -1)
    {
      struct ecoff_debug_info *input_debug;

      input_debug = &ecoff_data (input_bfd)->debug_info;
      BFD_ASSERT (esym->ifd < input_debug->symbolic_header.ifdMax);
      if (input_debug->ifdmap != (RFDT *) NULL)
	esym->ifd = input_debug->ifdmap[esym->ifd];
    }

  return true;
}

/* Set the external symbol index.  This routine is passed to
   bfd_ecoff_debug_externals.  */

static void
ecoff_set_index (sym, indx)
     asymbol *sym;
     bfd_size_type indx;
{
  ecoff_set_sym_index (sym, indx);
}

/* Write out an ECOFF file.  */

boolean
_bfd_ecoff_write_object_contents (abfd)
     bfd *abfd;
{
  const struct ecoff_backend_data * const backend = ecoff_backend (abfd);
  const bfd_vma round = backend->round;
  const bfd_size_type filhsz = bfd_coff_filhsz (abfd);
  const bfd_size_type aoutsz = bfd_coff_aoutsz (abfd);
  const bfd_size_type scnhsz = bfd_coff_scnhsz (abfd);
  const bfd_size_type external_hdr_size
    = backend->debug_swap.external_hdr_size;
  const bfd_size_type external_reloc_size = backend->external_reloc_size;
  void (* const adjust_reloc_out) PARAMS ((bfd *,
					   const arelent *,
					   struct internal_reloc *))
    = backend->adjust_reloc_out;
  void (* const swap_reloc_out) PARAMS ((bfd *,
					 const struct internal_reloc *,
					 PTR))
    = backend->swap_reloc_out;
  struct ecoff_debug_info * const debug = &ecoff_data (abfd)->debug_info;
  HDRR * const symhdr = &debug->symbolic_header;
  asection *current;
  unsigned int count;
  bfd_size_type reloc_size;
  bfd_size_type text_size;
  bfd_vma text_start;
  boolean set_text_start;
  bfd_size_type data_size;
  bfd_vma data_start;
  boolean set_data_start;
  bfd_size_type bss_size;
  PTR buff = NULL;
  PTR reloc_buff = NULL;
  struct internal_filehdr internal_f;
  struct internal_aouthdr internal_a;
  int i;

  /* Determine where the sections and relocs will go in the output
     file.  */
  reloc_size = ecoff_compute_reloc_file_positions (abfd);

  count = 1;
  for (current = abfd->sections;
       current != (asection *)NULL; 
       current = current->next) 
    {
      current->target_index = count;
      ++count;
    }

  if ((abfd->flags & D_PAGED) != 0)
    text_size = _bfd_ecoff_sizeof_headers (abfd, false);
  else
    text_size = 0;
  text_start = 0;
  set_text_start = false;
  data_size = 0;
  data_start = 0;
  set_data_start = false;
  bss_size = 0;

  /* Write section headers to the file.  */

  /* Allocate buff big enough to hold a section header,
     file header, or a.out header.  */
  {
    bfd_size_type siz;
    siz = scnhsz;
    if (siz < filhsz)
      siz = filhsz;
    if (siz < aoutsz)
      siz = aoutsz;
    buff = (PTR) malloc (siz);
    if (buff == NULL)
      {
	bfd_set_error (bfd_error_no_memory);
	goto error_return;
      }
  }

  internal_f.f_nscns = 0;
  if (bfd_seek (abfd, (file_ptr) (filhsz + aoutsz), SEEK_SET) != 0)
    goto error_return;
  for (current = abfd->sections;
       current != (asection *) NULL;
       current = current->next)
    {
      struct internal_scnhdr section;
      bfd_vma vma;

      ++internal_f.f_nscns;

      strncpy (section.s_name, current->name, sizeof section.s_name);

      /* This seems to be correct for Irix 4 shared libraries.  */
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
      if (strcmp (current->name, _PDATA) != 0)
	section.s_lnnoptr = 0;
      else
	{
	  /* The Alpha ECOFF .pdata section uses the lnnoptr field to
	     hold the number of entries in the section (each entry is
	     8 bytes).  We stored this in the line_filepos field in
	     ecoff_compute_section_file_positions.  */
	  section.s_lnnoptr = current->line_filepos;
	}

      section.s_nreloc = current->reloc_count;
      section.s_nlnno = 0;
      section.s_flags = ecoff_sec_to_styp_flags (current->name,
						 current->flags);

      bfd_coff_swap_scnhdr_out (abfd, (PTR) &section, buff);
      if (bfd_write (buff, 1, scnhsz, abfd) != scnhsz)
	goto error_return;

      if ((section.s_flags & STYP_TEXT) != 0
	  || ((section.s_flags & STYP_RDATA) != 0
	      && backend->rdata_in_text)
	  || strcmp (current->name, _PDATA) == 0)
	{
	  text_size += bfd_get_section_size_before_reloc (current);
	  if (! set_text_start || text_start > vma)
	    {
	      text_start = vma;
	      set_text_start = true;
	    }
	}
      else if ((section.s_flags & STYP_RDATA) != 0
	       || (section.s_flags & STYP_DATA) != 0
	       || (section.s_flags & STYP_LITA) != 0
	       || (section.s_flags & STYP_LIT8) != 0
	       || (section.s_flags & STYP_LIT4) != 0
	       || (section.s_flags & STYP_SDATA) != 0
	       || strcmp (current->name, _XDATA) == 0)
	{
	  data_size += bfd_get_section_size_before_reloc (current);
	  if (! set_data_start || data_start > vma)
	    {
	      data_start = vma;
	      set_data_start = true;
	    }
	}
      else if ((section.s_flags & STYP_BSS) != 0
	       || (section.s_flags & STYP_SBSS) != 0)
	bss_size += bfd_get_section_size_before_reloc (current);
      else if ((section.s_flags & STYP_ECOFF_LIB) != 0)
	/* Do nothing */ ;
      else
	abort ();
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
      internal_f.f_symptr = ecoff_data (abfd)->sym_filepos;
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

  /* FIXME: Is this really correct?  */
  internal_a.vstamp = symhdr->vstamp;

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
    goto error_return;

  bfd_coff_swap_filehdr_out (abfd, (PTR) &internal_f, buff);
  if (bfd_write (buff, 1, filhsz, abfd) != filhsz)
    goto error_return;

  bfd_coff_swap_aouthdr_out (abfd, (PTR) &internal_a, buff);
  if (bfd_write (buff, 1, aoutsz, abfd) != aoutsz)
    goto error_return;

  /* Build the external symbol information.  This must be done before
     writing out the relocs so that we know the symbol indices.  We
     don't do this if this BFD was created by the backend linker,
     since it will have already handled the symbols and relocs.  */
  if (! ecoff_data (abfd)->linker)
    {
      symhdr->iextMax = 0;
      symhdr->issExtMax = 0;
      debug->external_ext = debug->external_ext_end = NULL;
      debug->ssext = debug->ssext_end = NULL;
      if (bfd_ecoff_debug_externals (abfd, debug, &backend->debug_swap,
				     (((abfd->flags & EXEC_P) == 0)
				      ? true : false),
				     ecoff_get_extr, ecoff_set_index)
	  == false)
	goto error_return;

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

	  reloc_buff =
	    bfd_alloc (abfd, current->reloc_count * external_reloc_size);
	  if (reloc_buff == NULL)
	    {
	      bfd_set_error (bfd_error_no_memory);
	      goto error_return;
	    }

	  reloc_ptr_ptr = current->orelocation;
	  reloc_end = reloc_ptr_ptr + current->reloc_count;
	  out_ptr = (char *) reloc_buff;
	  for (;
	       reloc_ptr_ptr < reloc_end;
	       reloc_ptr_ptr++, out_ptr += external_reloc_size)
	    {
	      arelent *reloc;
	      asymbol *sym;
	      struct internal_reloc in;
	  
	      memset ((PTR) &in, 0, sizeof in);

	      reloc = *reloc_ptr_ptr;
	      sym = *reloc->sym_ptr_ptr;

	      in.r_vaddr = (reloc->address
			    + bfd_get_section_vma (abfd, current));
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
		  else if (strcmp (name, ".xdata") == 0)
		    in.r_symndx = RELOC_SECTION_XDATA;
		  else if (strcmp (name, ".pdata") == 0)
		    in.r_symndx = RELOC_SECTION_PDATA;
		  else if (strcmp (name, ".fini") == 0)
		    in.r_symndx = RELOC_SECTION_FINI;
		  else if (strcmp (name, ".lita") == 0)
		    in.r_symndx = RELOC_SECTION_LITA;
		  else if (strcmp (name, "*ABS*") == 0)
		    in.r_symndx = RELOC_SECTION_ABS;
		  else
		    abort ();
		  in.r_extern = 0;
		}

	      (*adjust_reloc_out) (abfd, reloc, &in);

	      (*swap_reloc_out) (abfd, &in, (PTR) out_ptr);
	    }

	  if (bfd_seek (abfd, current->rel_filepos, SEEK_SET) != 0)
	    goto error_return;
	  if (bfd_write (reloc_buff,
			 external_reloc_size, current->reloc_count, abfd)
	      != external_reloc_size * current->reloc_count)
	    goto error_return;
	  bfd_release (abfd, reloc_buff);
	  reloc_buff = NULL;
	}

      /* Write out the symbolic debugging information.  */
      if (bfd_get_symcount (abfd) > 0)
	{
	  /* Write out the debugging information.  */
	  if (bfd_ecoff_write_debug (abfd, debug, &backend->debug_swap,
				     ecoff_data (abfd)->sym_filepos)
	      == false)
	    goto error_return;
	}
    }

  /* The .bss section of a demand paged executable must receive an
     entire page.  If there are symbols, the symbols will start on the
     next page.  If there are no symbols, we must fill out the page by
     hand.  */
  if (bfd_get_symcount (abfd) == 0
      && (abfd->flags & EXEC_P) != 0
      && (abfd->flags & D_PAGED) != 0)
    {
      char c;

      if (bfd_seek (abfd, (file_ptr) ecoff_data (abfd)->sym_filepos - 1,
		    SEEK_SET) != 0)
	goto error_return;
      if (bfd_read (&c, 1, 1, abfd) == 0)
	c = 0;
      if (bfd_seek (abfd, (file_ptr) ecoff_data (abfd)->sym_filepos - 1,
		    SEEK_SET) != 0)
	goto error_return;
      if (bfd_write (&c, 1, 1, abfd) != 1)
	goto error_return;
    }

  if (reloc_buff != NULL)
    bfd_release (abfd, reloc_buff);
  if (buff != NULL)
    free (buff);
  return true;
 error_return:
  if (reloc_buff != NULL)
    bfd_release (abfd, reloc_buff);
  if (buff != NULL)
    free (buff);
  return false;
}

/* Archive handling.  ECOFF uses what appears to be a unique type of
   archive header (armap).  The byte ordering of the armap and the
   contents are encoded in the name of the armap itself.  At least for
   now, we only support archives with the same byte ordering in the
   armap and the contents.

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
_bfd_ecoff_slurp_armap (abfd)
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

  if (bfd_seek (abfd, (file_ptr) -16, SEEK_CUR) != 0)
    return false;

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
      bfd_set_error (bfd_error_wrong_format);
      return false;
    }

  /* Read in the armap.  */
  ardata = bfd_ardata (abfd);
  mapdata = _bfd_snarf_ar_hdr (abfd);
  if (mapdata == (struct areltdata *) NULL)
    return false;
  parsed_size = mapdata->parsed_size;
  bfd_release (abfd, (PTR) mapdata);
    
  raw_armap = (char *) bfd_alloc (abfd, parsed_size);
  if (raw_armap == (char *) NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
    
  if (bfd_read ((PTR) raw_armap, 1, parsed_size, abfd) != parsed_size)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_malformed_archive);
      bfd_release (abfd, (PTR) raw_armap);
      return false;
    }
    
  ardata->tdata = (PTR) raw_armap;

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
  if (!symdef_ptr)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }

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
_bfd_ecoff_write_armap (abfd, elength, map, orl_count, stridx)
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
  hdr.ar_fmag[1] = '\012';

  /* Turn all null bytes in the header into spaces.  */
  for (i = 0; i < sizeof (struct ar_hdr); i++)
   if (((char *)(&hdr))[i] == '\0')
     (((char *)(&hdr))[i]) = ' ';

  if (bfd_write ((PTR) &hdr, 1, sizeof (struct ar_hdr), abfd)
      != sizeof (struct ar_hdr))
    return false;

  bfd_h_put_32 (abfd, (bfd_vma) hashsize, temp);
  if (bfd_write ((PTR) temp, 1, 4, abfd) != 4)
    return false;
  
  hashtable = (bfd_byte *) bfd_zalloc (abfd, symdefsize);
  if (!hashtable)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }

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
	
      bfd_h_put_32 (abfd, (bfd_vma) map[i].namidx,
		    (PTR) (hashtable + hash * 8));
      bfd_h_put_32 (abfd, (bfd_vma) firstreal,
		    (PTR) (hashtable + hash * 8 + 4));
    }

  if (bfd_write ((PTR) hashtable, 1, symdefsize, abfd) != symdefsize)
    return false;

  bfd_release (abfd, hashtable);

  /* Now write the strings.  */
  bfd_h_put_32 (abfd, (bfd_vma) stringsize, temp);
  if (bfd_write ((PTR) temp, 1, 4, abfd) != 4)
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
      if (bfd_write ("", 1, 1, abfd) != 1)
	return false;
    }

  return true;
}

/* See whether this BFD is an archive.  If it is, read in the armap
   and the extended name table.  */

const bfd_target *
_bfd_ecoff_archive_p (abfd)
     bfd *abfd;
{
  char armag[SARMAG + 1];

  if (bfd_read ((PTR) armag, 1, SARMAG, abfd) != SARMAG
      || strncmp (armag, ARMAG, SARMAG) != 0)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return (const bfd_target *) NULL;
    }

  /* We are setting bfd_ardata(abfd) here, but since bfd_ardata
     involves a cast, we can't do it as the left operand of
     assignment.  */
  abfd->tdata.aout_ar_data =
    (struct artdata *) bfd_zalloc (abfd, sizeof (struct artdata));

  if (bfd_ardata (abfd) == (struct artdata *) NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return (const bfd_target *) NULL;
    }

  bfd_ardata (abfd)->first_file_filepos = SARMAG;
  bfd_ardata (abfd)->cache = NULL;
  bfd_ardata (abfd)->archive_head = NULL;
  bfd_ardata (abfd)->symdefs = NULL;
  bfd_ardata (abfd)->extended_names = NULL;
  bfd_ardata (abfd)->tdata = NULL;
  
  if (_bfd_ecoff_slurp_armap (abfd) == false
      || _bfd_ecoff_slurp_extended_name_table (abfd) == false)
    {
      bfd_release (abfd, bfd_ardata (abfd));
      abfd->tdata.aout_ar_data = (struct artdata *) NULL;
      return (const bfd_target *) NULL;
    }
  
  return abfd->xvec;
}

/* ECOFF linker code.  */

static struct bfd_hash_entry *ecoff_link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *entry,
	   struct bfd_hash_table *table,
	   const char *string));
static boolean ecoff_link_add_archive_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean ecoff_link_check_archive_element
  PARAMS ((bfd *, struct bfd_link_info *, boolean *pneeded));
static boolean ecoff_link_add_object_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean ecoff_link_add_externals
  PARAMS ((bfd *, struct bfd_link_info *, PTR, char *));

/* Routine to create an entry in an ECOFF link hash table.  */

static struct bfd_hash_entry *
ecoff_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct ecoff_link_hash_entry *ret = (struct ecoff_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct ecoff_link_hash_entry *) NULL)
    ret = ((struct ecoff_link_hash_entry *)
	   bfd_hash_allocate (table, sizeof (struct ecoff_link_hash_entry)));
  if (ret == (struct ecoff_link_hash_entry *) NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return NULL;
    }

  /* Call the allocation method of the superclass.  */
  ret = ((struct ecoff_link_hash_entry *)
	 _bfd_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				 table, string));

  if (ret)
    {
      /* Set local fields.  */
      ret->indx = -1;
      ret->abfd = NULL;
      ret->written = 0;
      ret->small = 0;
    }
  memset ((PTR) &ret->esym, 0, sizeof ret->esym);

  return (struct bfd_hash_entry *) ret;
}

/* Create an ECOFF link hash table.  */

struct bfd_link_hash_table *
_bfd_ecoff_bfd_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct ecoff_link_hash_table *ret;

  ret = ((struct ecoff_link_hash_table *)
	 malloc (sizeof (struct ecoff_link_hash_table)));
  if (!ret)
      {
	bfd_set_error (bfd_error_no_memory);
	return NULL;
      }
  if (! _bfd_link_hash_table_init (&ret->root, abfd,
				   ecoff_link_hash_newfunc))
    {
      free (ret);
      return (struct bfd_link_hash_table *) NULL;
    }
  return &ret->root;
}

/* Look up an entry in an ECOFF link hash table.  */

#define ecoff_link_hash_lookup(table, string, create, copy, follow) \
  ((struct ecoff_link_hash_entry *) \
   bfd_link_hash_lookup (&(table)->root, (string), (create), (copy), (follow)))

/* Traverse an ECOFF link hash table.  */

#define ecoff_link_hash_traverse(table, func, info)			\
  (bfd_link_hash_traverse						\
   (&(table)->root,							\
    (boolean (*) PARAMS ((struct bfd_link_hash_entry *, PTR))) (func),	\
    (info)))

/* Get the ECOFF link hash table from the info structure.  This is
   just a cast.  */

#define ecoff_hash_table(p) ((struct ecoff_link_hash_table *) ((p)->hash))

/* Given an ECOFF BFD, add symbols to the global hash table as
   appropriate.  */

boolean
_bfd_ecoff_bfd_link_add_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  switch (bfd_get_format (abfd))
    {
    case bfd_object:
      return ecoff_link_add_object_symbols (abfd, info);
    case bfd_archive:
      return ecoff_link_add_archive_symbols (abfd, info);
    default:
      bfd_set_error (bfd_error_wrong_format);
      return false;
    }
}

/* Add the symbols from an archive file to the global hash table.
   This looks through the undefined symbols, looks each one up in the
   archive hash table, and adds any associated object file.  We do not
   use _bfd_generic_link_add_archive_symbols because ECOFF archives
   already have a hash table, so there is no reason to construct
   another one.  */

static boolean
ecoff_link_add_archive_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  const bfd_byte *raw_armap;
  struct bfd_link_hash_entry **pundef;
  unsigned int armap_count;
  unsigned int armap_log;
  unsigned int i;
  const bfd_byte *hashtable;
  const char *stringbase;

  if (! bfd_has_map (abfd))
    {
      bfd_set_error (bfd_error_no_symbols);
      return false;
    }

  /* If we don't have any raw data for this archive, as can happen on
     Irix 4.0.5F, we call the generic routine.
     FIXME: We should be more clever about this, since someday tdata
     may get to something for a generic archive.  */
  raw_armap = (const bfd_byte *) bfd_ardata (abfd)->tdata;
  if (raw_armap == (bfd_byte *) NULL)
    return (_bfd_generic_link_add_archive_symbols
	    (abfd, info, ecoff_link_check_archive_element));

  armap_count = bfd_h_get_32 (abfd, raw_armap);

  armap_log = 0;
  for (i = 1; i < armap_count; i <<= 1)
    armap_log++;
  BFD_ASSERT (i == armap_count);

  hashtable = raw_armap + 4;
  stringbase = (const char *) raw_armap + armap_count * 8 + 8;

  /* Look through the list of undefined symbols.  */
  pundef = &info->hash->undefs;
  while (*pundef != (struct bfd_link_hash_entry *) NULL)
    {
      struct bfd_link_hash_entry *h;
      unsigned int hash, rehash;
      unsigned int file_offset;
      const char *name;
      bfd *element;

      h = *pundef;

      /* When a symbol is defined, it is not necessarily removed from
	 the list.  */
      if (h->type != bfd_link_hash_undefined
	  && h->type != bfd_link_hash_common)
	{
	  /* Remove this entry from the list, for general cleanliness
	     and because we are going to look through the list again
	     if we search any more libraries.  We can't remove the
	     entry if it is the tail, because that would lose any
	     entries we add to the list later on.  */
	  if (*pundef != info->hash->undefs_tail)
	    *pundef = (*pundef)->next;
	  else
	    pundef = &(*pundef)->next;
	  continue;
	}

      /* Native ECOFF linkers do not pull in archive elements merely
	 to satisfy common definitions, so neither do we.  We leave
	 them on the list, though, in case we are linking against some
	 other object format.  */
      if (h->type != bfd_link_hash_undefined)
	{
	  pundef = &(*pundef)->next;
	  continue;
	}

      /* Look for this symbol in the archive hash table.  */
      hash = ecoff_armap_hash (h->root.string, &rehash, armap_count,
			       armap_log);

      file_offset = bfd_h_get_32 (abfd, hashtable + (hash * 8) + 4);
      if (file_offset == 0)
	{
	  /* Nothing in this slot.  */
	  pundef = &(*pundef)->next;
	  continue;
	}

      name = stringbase + bfd_h_get_32 (abfd, hashtable + (hash * 8));
      if (name[0] != h->root.string[0]
	  || strcmp (name, h->root.string) != 0)
	{
	  unsigned int srch;
	  boolean found;

	  /* That was the wrong symbol.  Try rehashing.  */
	  found = false;
	  for (srch = (hash + rehash) & (armap_count - 1);
	       srch != hash;
	       srch = (srch + rehash) & (armap_count - 1))
	    {
	      file_offset = bfd_h_get_32 (abfd, hashtable + (srch * 8) + 4);
	      if (file_offset == 0)
		break;
	      name = stringbase + bfd_h_get_32 (abfd, hashtable + (srch * 8));
	      if (name[0] == h->root.string[0]
		  && strcmp (name, h->root.string) == 0)
		{
		  found = true;
		  break;
		}
	    }

	  if (! found)
	    {
	      pundef = &(*pundef)->next;
	      continue;
	    }

	  hash = srch;
	}

      element = _bfd_get_elt_at_filepos (abfd, file_offset);
      if (element == (bfd *) NULL)
	return false;

      if (! bfd_check_format (element, bfd_object))
	return false;

      /* Unlike the generic linker, we know that this element provides
	 a definition for an undefined symbol and we know that we want
	 to include it.  We don't need to check anything.  */
      if (! (*info->callbacks->add_archive_element) (info, element, name))
	return false;
      if (! ecoff_link_add_object_symbols (element, info))
	return false;

      pundef = &(*pundef)->next;
    }

  return true;
}

/* This is called if we used _bfd_generic_link_add_archive_symbols
   because we were not dealing with an ECOFF archive.  */

static boolean
ecoff_link_check_archive_element (abfd, info, pneeded)
     bfd *abfd;
     struct bfd_link_info *info;
     boolean *pneeded;
{
  const struct ecoff_backend_data * const backend = ecoff_backend (abfd);
  void (* const swap_ext_in) PARAMS ((bfd *, PTR, EXTR *))
    = backend->debug_swap.swap_ext_in;
  HDRR *symhdr;
  bfd_size_type external_ext_size;
  PTR external_ext = NULL;
  size_t esize;
  char *ssext = NULL;
  char *ext_ptr;
  char *ext_end;

  *pneeded = false;

  if (! ecoff_slurp_symbolic_header (abfd))
    goto error_return;

  /* If there are no symbols, we don't want it.  */
  if (bfd_get_symcount (abfd) == 0)
    goto successful_return;

  symhdr = &ecoff_data (abfd)->debug_info.symbolic_header;

  /* Read in the external symbols and external strings.  */
  external_ext_size = backend->debug_swap.external_ext_size;
  esize = symhdr->iextMax * external_ext_size;
  external_ext = (PTR) malloc (esize);
  if (external_ext == NULL && esize != 0)
    {
      bfd_set_error (bfd_error_no_memory);
      goto error_return;
    }

  if (bfd_seek (abfd, symhdr->cbExtOffset, SEEK_SET) != 0
      || bfd_read (external_ext, 1, esize, abfd) != esize)
    goto error_return;

  ssext = (char *) malloc (symhdr->issExtMax);
  if (ssext == NULL && symhdr->issExtMax != 0)
    {
      bfd_set_error (bfd_error_no_memory);
      goto error_return;
    }

  if (bfd_seek (abfd, symhdr->cbSsExtOffset, SEEK_SET) != 0
      || bfd_read (ssext, 1, symhdr->issExtMax, abfd) != symhdr->issExtMax)
    goto error_return;

  /* Look through the external symbols to see if they define some
     symbol that is currently undefined.  */
  ext_ptr = (char *) external_ext;
  ext_end = ext_ptr + esize;
  for (; ext_ptr < ext_end; ext_ptr += external_ext_size)
    {
      EXTR esym;
      boolean def;
      const char *name;
      struct bfd_link_hash_entry *h;

      (*swap_ext_in) (abfd, (PTR) ext_ptr, &esym);

      /* See if this symbol defines something.  */
      if (esym.asym.st != stGlobal
	  && esym.asym.st != stLabel
	  && esym.asym.st != stProc)
	continue;

      switch (esym.asym.sc)
	{
	case scText:
	case scData:
	case scBss:
	case scAbs:
	case scSData:
	case scSBss:
	case scRData:
	case scCommon:
	case scSCommon:
	case scInit:
	case scFini:
	  def = true;
	  break;
	default:
	  def = false;
	  break;
	}

      if (! def)
	continue;

      name = ssext + esym.asym.iss;
      h = bfd_link_hash_lookup (info->hash, name, false, false, true);

      /* Unlike the generic linker, we do not pull in elements because
	 of common symbols.  */
      if (h == (struct bfd_link_hash_entry *) NULL
	  || h->type != bfd_link_hash_undefined)
	continue;

      /* Include this element.  */
      if (! (*info->callbacks->add_archive_element) (info, abfd, name))
	goto error_return;
      if (! ecoff_link_add_externals (abfd, info, external_ext, ssext))
	goto error_return;

      *pneeded = true;
      goto successful_return;
    }

 successful_return:
  if (external_ext != NULL)
    free (external_ext);
  if (ssext != NULL)
    free (ssext);
  return true;
 error_return:
  if (external_ext != NULL)
    free (external_ext);
  if (ssext != NULL)
    free (ssext);
  return false;
}

/* Add symbols from an ECOFF object file to the global linker hash
   table.  */

static boolean
ecoff_link_add_object_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  HDRR *symhdr;
  bfd_size_type external_ext_size;
  PTR external_ext = NULL;
  size_t esize;
  char *ssext = NULL;
  boolean result;

  if (! ecoff_slurp_symbolic_header (abfd))
    return false;

  /* If there are no symbols, we don't want it.  */
  if (bfd_get_symcount (abfd) == 0)
    return true;

  symhdr = &ecoff_data (abfd)->debug_info.symbolic_header;

  /* Read in the external symbols and external strings.  */
  external_ext_size = ecoff_backend (abfd)->debug_swap.external_ext_size;
  esize = symhdr->iextMax * external_ext_size;
  external_ext = (PTR) malloc (esize);
  if (external_ext == NULL && esize != 0)
    {
      bfd_set_error (bfd_error_no_memory);
      goto error_return;
    }

  if (bfd_seek (abfd, symhdr->cbExtOffset, SEEK_SET) != 0
      || bfd_read (external_ext, 1, esize, abfd) != esize)
    goto error_return;

  ssext = (char *) malloc (symhdr->issExtMax);
  if (ssext == NULL && symhdr->issExtMax != 0)
    {
      bfd_set_error (bfd_error_no_memory);
      goto error_return;
    }

  if (bfd_seek (abfd, symhdr->cbSsExtOffset, SEEK_SET) != 0
      || bfd_read (ssext, 1, symhdr->issExtMax, abfd) != symhdr->issExtMax)
    goto error_return;

  result = ecoff_link_add_externals (abfd, info, external_ext, ssext);

  if (ssext != NULL)
    free (ssext);
  if (external_ext != NULL)
    free (external_ext);
  return result;

 error_return:
  if (ssext != NULL)
    free (ssext);
  if (external_ext != NULL)
    free (external_ext);
  return false;
}

/* Add the external symbols of an object file to the global linker
   hash table.  The external symbols and strings we are passed are
   just allocated on the stack, and will be discarded.  We must
   explicitly save any information we may need later on in the link.
   We do not want to read the external symbol information again.  */

static boolean
ecoff_link_add_externals (abfd, info, external_ext, ssext)
     bfd *abfd;
     struct bfd_link_info *info;
     PTR external_ext;
     char *ssext;
{
  const struct ecoff_backend_data * const backend = ecoff_backend (abfd);
  void (* const swap_ext_in) PARAMS ((bfd *, PTR, EXTR *))
    = backend->debug_swap.swap_ext_in;
  bfd_size_type external_ext_size = backend->debug_swap.external_ext_size;
  unsigned long ext_count;
  struct ecoff_link_hash_entry **sym_hash;
  char *ext_ptr;
  char *ext_end;

  ext_count = ecoff_data (abfd)->debug_info.symbolic_header.iextMax;

  sym_hash = ((struct ecoff_link_hash_entry **)
	      bfd_alloc (abfd,
			 ext_count * sizeof (struct bfd_link_hash_entry *)));
  if (!sym_hash)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  ecoff_data (abfd)->sym_hashes = sym_hash;

  ext_ptr = (char *) external_ext;
  ext_end = ext_ptr + ext_count * external_ext_size;
  for (; ext_ptr < ext_end; ext_ptr += external_ext_size, sym_hash++)
    {
      EXTR esym;
      boolean skip;
      bfd_vma value;
      asection *section;
      const char *name;
      struct ecoff_link_hash_entry *h;

      *sym_hash = NULL;

      (*swap_ext_in) (abfd, (PTR) ext_ptr, &esym);

      /* Skip debugging symbols.  */
      skip = false;
      switch (esym.asym.st)
	{
	case stGlobal:
	case stStatic:
	case stLabel:
	case stProc:
	case stStaticProc:
	  break;
	default:
	  skip = true;
	  break;
	}

      if (skip)
	continue;

      /* Get the information for this symbol.  */
      value = esym.asym.value;
      switch (esym.asym.sc)
	{
	default:
	case scNil:
	case scRegister:
	case scCdbLocal:
	case scBits:
	case scCdbSystem:
	case scRegImage:
	case scInfo:
	case scUserStruct:
	case scVar:
	case scVarRegister:
	case scVariant:
	case scBasedVar:
	case scXData:
	case scPData:
	  section = NULL;
	  break;
	case scText:
	  section = bfd_make_section_old_way (abfd, ".text");
	  value -= section->vma;
	  break;
	case scData:
	  section = bfd_make_section_old_way (abfd, ".data");
	  value -= section->vma;
	  break;
	case scBss:
	  section = bfd_make_section_old_way (abfd, ".bss");
	  value -= section->vma;
	  break;
	case scAbs:
	  section = bfd_abs_section_ptr;
	  break;
	case scUndefined:
	  section = bfd_und_section_ptr;
	  break;
	case scSData:
	  section = bfd_make_section_old_way (abfd, ".sdata");
	  value -= section->vma;
	  break;
	case scSBss:
	  section = bfd_make_section_old_way (abfd, ".sbss");
	  value -= section->vma;
	  break;
	case scRData:
	  section = bfd_make_section_old_way (abfd, ".rdata");
	  value -= section->vma;
	  break;
	case scCommon:
	  if (value > ecoff_data (abfd)->gp_size)
	    {
	      section = bfd_com_section_ptr;
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
	  section = &ecoff_scom_section;
	  break;
	case scSUndefined:
	  section = bfd_und_section_ptr;
	  break;
	case scInit:
	  section = bfd_make_section_old_way (abfd, ".init");
	  value -= section->vma;
	  break;
	case scFini:
	  section = bfd_make_section_old_way (abfd, ".fini");
	  value -= section->vma;
	  break;
	}

      if (section == (asection *) NULL)
	continue;

      name = ssext + esym.asym.iss;

      h = NULL;
      if (! (_bfd_generic_link_add_one_symbol
	     (info, abfd, name, BSF_GLOBAL, section, value,
	      (const char *) NULL, true, true,
	      (struct bfd_link_hash_entry **) &h)))
	return false;

      *sym_hash = h;

      /* If we are building an ECOFF hash table, save the external
	 symbol information.  */
      if (info->hash->creator->flavour == bfd_get_flavour (abfd))
	{
	  if (h->abfd == (bfd *) NULL
	      || (! bfd_is_und_section (section)
		  && (! bfd_is_com_section (section)
		      || h->root.type != bfd_link_hash_defined)))
	    {
	      h->abfd = abfd;
	      h->esym = esym;
	    }

	  /* Remember whether this symbol was small undefined.  */
	  if (esym.asym.sc == scSUndefined)
	    h->small = 1;

	  /* If this symbol was ever small undefined, it needs to wind
	     up in a GP relative section.  We can't control the
	     section of a defined symbol, but we can control the
	     section of a common symbol.  This case is actually needed
	     on Ultrix 4.2 to handle the symbol cred in -lckrb.  */
	  if (h->small
	      && h->root.type == bfd_link_hash_common
	      && strcmp (h->root.u.c.section->name, SCOMMON) != 0)
	    {
	      h->root.u.c.section = bfd_make_section_old_way (abfd, SCOMMON);
	      h->root.u.c.section->flags = SEC_ALLOC;
	      if (h->esym.asym.sc == scCommon)
		h->esym.asym.sc = scSCommon;
	    }
	}
    }

  return true;
}

/* ECOFF final link routines.  */

static boolean ecoff_final_link_debug_accumulate
  PARAMS ((bfd *output_bfd, bfd *input_bfd, struct bfd_link_info *,
	   PTR handle));
static boolean ecoff_link_write_external
  PARAMS ((struct ecoff_link_hash_entry *, PTR));
static boolean ecoff_indirect_link_order
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   struct bfd_link_order *));
static boolean ecoff_reloc_link_order
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   struct bfd_link_order *));

/* ECOFF final link routine.  This looks through all the input BFDs
   and gathers together all the debugging information, and then
   processes all the link order information.  This may cause it to
   close and reopen some input BFDs; I'll see how bad this is.  */

boolean
_bfd_ecoff_bfd_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  const struct ecoff_backend_data * const backend = ecoff_backend (abfd);
  struct ecoff_debug_info * const debug = &ecoff_data (abfd)->debug_info;
  HDRR *symhdr;
  PTR handle;
  register bfd *input_bfd;
  asection *o;
  struct bfd_link_order *p;

  /* We accumulate the debugging information counts in the symbolic
     header.  */
  symhdr = &debug->symbolic_header;
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

  /* We accumulate the debugging information itself in the debug_info
     structure.  */
  debug->line = NULL;
  debug->external_dnr = NULL;
  debug->external_pdr = NULL;
  debug->external_sym = NULL;
  debug->external_opt = NULL;
  debug->external_aux = NULL;
  debug->ss = NULL;
  debug->ssext = debug->ssext_end = NULL;
  debug->external_fdr = NULL;
  debug->external_rfd = NULL;
  debug->external_ext = debug->external_ext_end = NULL;

  handle = bfd_ecoff_debug_init (abfd, debug, &backend->debug_swap, info);
  if (handle == (PTR) NULL)
    return false;

  /* Accumulate the debugging symbols from each input BFD.  */
  for (input_bfd = info->input_bfds;
       input_bfd != (bfd *) NULL;
       input_bfd = input_bfd->link_next)
    {
      boolean ret;

      if (bfd_get_flavour (input_bfd) == bfd_target_ecoff_flavour)
	{
	  /* Abitrarily set the symbolic header vstamp to the vstamp
	     of the first object file in the link.  */
	  if (symhdr->vstamp == 0)
	    symhdr->vstamp
	      = ecoff_data (input_bfd)->debug_info.symbolic_header.vstamp;
	  ret = ecoff_final_link_debug_accumulate (abfd, input_bfd, info,
						   handle);
	}
      else
	ret = bfd_ecoff_debug_accumulate_other (handle, abfd,
						debug, &backend->debug_swap,
						input_bfd, info);
      if (! ret)
	return false;

      /* Combine the register masks.  */
      ecoff_data (abfd)->gprmask |= ecoff_data (input_bfd)->gprmask;
      ecoff_data (abfd)->fprmask |= ecoff_data (input_bfd)->fprmask;
      ecoff_data (abfd)->cprmask[0] |= ecoff_data (input_bfd)->cprmask[0];
      ecoff_data (abfd)->cprmask[1] |= ecoff_data (input_bfd)->cprmask[1];
      ecoff_data (abfd)->cprmask[2] |= ecoff_data (input_bfd)->cprmask[2];
      ecoff_data (abfd)->cprmask[3] |= ecoff_data (input_bfd)->cprmask[3];
    }

  /* Write out the external symbols.  */
  ecoff_link_hash_traverse (ecoff_hash_table (info),
			    ecoff_link_write_external,
			    (PTR) abfd);

  if (info->relocateable)
    {
      /* We need to make a pass over the link_orders to count up the
	 number of relocations we will need to output, so that we know
	 how much space they will take up.  */
      for (o = abfd->sections; o != (asection *) NULL; o = o->next)
	{
	  o->reloc_count = 0;
	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    if (p->type == bfd_indirect_link_order)
	      o->reloc_count += p->u.indirect.section->reloc_count;
	    else if (p->type == bfd_section_reloc_link_order
		     || p->type == bfd_symbol_reloc_link_order)
	      ++o->reloc_count;
	}
    }

  /* Compute the reloc and symbol file positions.  */
  ecoff_compute_reloc_file_positions (abfd);

  /* Write out the debugging information.  */
  if (! bfd_ecoff_write_accumulated_debug (handle, abfd, debug,
					   &backend->debug_swap, info,
					   ecoff_data (abfd)->sym_filepos))
    return false;

  bfd_ecoff_debug_free (handle, abfd, debug, &backend->debug_swap, info);

  if (info->relocateable)
    {
      /* Now reset the reloc_count field of the sections in the output
	 BFD to 0, so that we can use them to keep track of how many
	 relocs we have output thus far.  */
      for (o = abfd->sections; o != (asection *) NULL; o = o->next)
	o->reloc_count = 0;
    }

  /* Get a value for the GP register.  */
  if (ecoff_data (abfd)->gp == 0)
    {
      struct bfd_link_hash_entry *h;

      h = bfd_link_hash_lookup (info->hash, "_gp", false, false, true);
      if (h != (struct bfd_link_hash_entry *) NULL
	  && h->type == bfd_link_hash_defined)
	ecoff_data (abfd)->gp = (h->u.def.value
				 + h->u.def.section->output_section->vma
				 + h->u.def.section->output_offset);
      else if (info->relocateable)
	{
	  bfd_vma lo;

	  /* Make up a value.  */
	  lo = (bfd_vma) -1;
	  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
	    {
	      if (o->vma < lo
		  && (strcmp (o->name, _SBSS) == 0
		      || strcmp (o->name, _SDATA) == 0
		      || strcmp (o->name, _LIT4) == 0
		      || strcmp (o->name, _LIT8) == 0
		      || strcmp (o->name, _LITA) == 0))
		lo = o->vma;
	    }
	  ecoff_data (abfd)->gp = lo + 0x8000;
	}
      else
	{
	  /* If the relocate_section function needs to do a reloc
	     involving the GP value, it should make a reloc_dangerous
	     callback to warn that GP is not defined.  */
	}
    }

  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
    {
      for (p = o->link_order_head;
	   p != (struct bfd_link_order *) NULL;
	   p = p->next)
	{
	  if (p->type == bfd_indirect_link_order
	      && (bfd_get_flavour (p->u.indirect.section->owner)
		  == bfd_target_ecoff_flavour))
	    {
	      if (! ecoff_indirect_link_order (abfd, info, o, p))
		return false;
	    }
	  else if (p->type == bfd_section_reloc_link_order
		   || p->type == bfd_symbol_reloc_link_order)
	    {
	      if (! ecoff_reloc_link_order (abfd, info, o, p))
		return false;
	    }
	  else
	    {
	      if (! _bfd_default_link_order (abfd, info, o, p))
		return false;
	    }
	}
    }

  bfd_get_symcount (abfd) = symhdr->iextMax + symhdr->isymMax;

  ecoff_data (abfd)->linker = true;

  return true;
}

/* Accumulate the debugging information for an input BFD into the
   output BFD.  This must read in the symbolic information of the
   input BFD.  */

static boolean
ecoff_final_link_debug_accumulate (output_bfd, input_bfd, info, handle)
     bfd *output_bfd;
     bfd *input_bfd;
     struct bfd_link_info *info;
     PTR handle;
{
  struct ecoff_debug_info * const debug = &ecoff_data (input_bfd)->debug_info;
  const struct ecoff_debug_swap * const swap =
    &ecoff_backend (input_bfd)->debug_swap;
  HDRR *symhdr = &debug->symbolic_header;
  boolean ret;

#define READ(ptr, offset, count, size, type)				\
  if (symhdr->count == 0)						\
    debug->ptr = NULL;							\
  else									\
    {									\
      debug->ptr = (type) malloc (size * symhdr->count);		\
      if (debug->ptr == NULL)						\
	{								\
          bfd_set_error (bfd_error_no_memory);				\
          ret = false;							\
          goto return_something;					\
	}								\
      if ((bfd_seek (input_bfd, (file_ptr) symhdr->offset, SEEK_SET)	\
	   != 0)							\
	  || (bfd_read (debug->ptr, size, symhdr->count,		\
			input_bfd) != size * symhdr->count))		\
	{								\
          ret = false;							\
          goto return_something;					\
	}								\
    }

  /* If raw_syments is not NULL, then the data was already by read by
     _bfd_ecoff_slurp_symbolic_info.  */
  if (ecoff_data (input_bfd)->raw_syments == NULL)
    {
      READ (line, cbLineOffset, cbLine, sizeof (unsigned char),
	    unsigned char *);
      READ (external_dnr, cbDnOffset, idnMax, swap->external_dnr_size, PTR);
      READ (external_pdr, cbPdOffset, ipdMax, swap->external_pdr_size, PTR);
      READ (external_sym, cbSymOffset, isymMax, swap->external_sym_size, PTR);
      READ (external_opt, cbOptOffset, ioptMax, swap->external_opt_size, PTR);
      READ (external_aux, cbAuxOffset, iauxMax, sizeof (union aux_ext),
	    union aux_ext *);
      READ (ss, cbSsOffset, issMax, sizeof (char), char *);
      READ (external_fdr, cbFdOffset, ifdMax, swap->external_fdr_size, PTR);
      READ (external_rfd, cbRfdOffset, crfd, swap->external_rfd_size, PTR);
    }
#undef READ

  /* We do not read the external strings or the external symbols.  */

  ret = (bfd_ecoff_debug_accumulate
	 (handle, output_bfd, &ecoff_data (output_bfd)->debug_info,
	  &ecoff_backend (output_bfd)->debug_swap,
	  input_bfd, debug, swap, info));

 return_something:
  if (ecoff_data (input_bfd)->raw_syments == NULL)
    {
      if (debug->line != NULL)
	free (debug->line);
      if (debug->external_dnr != NULL)
	free (debug->external_dnr);
      if (debug->external_pdr != NULL)
	free (debug->external_pdr);
      if (debug->external_sym != NULL)
	free (debug->external_sym);
      if (debug->external_opt != NULL)
	free (debug->external_opt);
      if (debug->external_aux != NULL)
	free (debug->external_aux);
      if (debug->ss != NULL)
	free (debug->ss);
      if (debug->external_fdr != NULL)
	free (debug->external_fdr);
      if (debug->external_rfd != NULL)
	free (debug->external_rfd);

      /* Make sure we don't accidentally follow one of these pointers
	 into freed memory.  */
      debug->line = NULL;
      debug->external_dnr = NULL;
      debug->external_pdr = NULL;
      debug->external_sym = NULL;
      debug->external_opt = NULL;
      debug->external_aux = NULL;
      debug->ss = NULL;
      debug->external_fdr = NULL;
      debug->external_rfd = NULL;
    }

  return ret;
}

/* Put out information for an external symbol.  These come only from
   the hash table.  */

static boolean
ecoff_link_write_external (h, data)
     struct ecoff_link_hash_entry *h;
     PTR data;
{
  bfd *output_bfd = (bfd *) data;

  /* FIXME: We should check if this symbol is being stripped.  */

  if (h->written)
    return true;

  if (h->abfd == (bfd *) NULL)
    {
      h->esym.jmptbl = 0;
      h->esym.cobol_main = 0;
      h->esym.weakext = 0;
      h->esym.reserved = 0;
      h->esym.ifd = ifdNil;
      h->esym.asym.value = 0;
      h->esym.asym.st = stGlobal;

      if (h->root.type != bfd_link_hash_defined)
	h->esym.asym.sc = scAbs;
      else
	{
	  asection *output_section;
	  const char *name;

	  output_section = h->root.u.def.section->output_section;
	  name = bfd_section_name (output_section->owner, output_section);
	
	  if (strcmp (name, _TEXT) == 0)
	    h->esym.asym.sc = scText;
	  else if (strcmp (name, _DATA) == 0)
	    h->esym.asym.sc = scData;
	  else if (strcmp (name, _SDATA) == 0)
	    h->esym.asym.sc = scSData;
	  else if (strcmp (name, _RDATA) == 0)
	    h->esym.asym.sc = scRData;
	  else if (strcmp (name, _BSS) == 0)
	    h->esym.asym.sc = scBss;
	  else if (strcmp (name, _SBSS) == 0)
	    h->esym.asym.sc = scSBss;
	  else if (strcmp (name, _INIT) == 0)
	    h->esym.asym.sc = scInit;
	  else if (strcmp (name, _FINI) == 0)
	    h->esym.asym.sc = scFini;
	  else if (strcmp (name, _PDATA) == 0)
	    h->esym.asym.sc = scPData;
	  else if (strcmp (name, _XDATA) == 0)
	    h->esym.asym.sc = scXData;
	  else
	    h->esym.asym.sc = scAbs;
	}

      h->esym.asym.reserved = 0;
      h->esym.asym.index = indexNil;
    }
  else if (h->esym.ifd != -1)
    {
      struct ecoff_debug_info *debug;

      /* Adjust the FDR index for the symbol by that used for the
	 input BFD.  */
      debug = &ecoff_data (h->abfd)->debug_info;
      BFD_ASSERT (h->esym.ifd >= 0
		  && h->esym.ifd < debug->symbolic_header.ifdMax);
      h->esym.ifd = debug->ifdmap[h->esym.ifd];
    }

  switch (h->root.type)
    {
    default:
    case bfd_link_hash_new:
      abort ();
    case bfd_link_hash_undefined:
    case bfd_link_hash_weak:
      if (h->esym.asym.sc != scUndefined
	  && h->esym.asym.sc != scSUndefined)
	h->esym.asym.sc = scUndefined;
      break;
    case bfd_link_hash_defined:
      if (h->esym.asym.sc == scUndefined
	  || h->esym.asym.sc == scSUndefined)
	h->esym.asym.sc = scAbs;
      else if (h->esym.asym.sc == scCommon)
	h->esym.asym.sc = scBss;
      else if (h->esym.asym.sc == scSCommon)
	h->esym.asym.sc = scSBss;
      h->esym.asym.value = (h->root.u.def.value
			    + h->root.u.def.section->output_section->vma
			    + h->root.u.def.section->output_offset);
      break;
    case bfd_link_hash_common:
      if (h->esym.asym.sc != scCommon
	  && h->esym.asym.sc != scSCommon)
	h->esym.asym.sc = scCommon;
      h->esym.asym.value = h->root.u.c.size;
      break;
    case bfd_link_hash_indirect:
    case bfd_link_hash_warning:
      /* FIXME: Ignore these for now.  The circumstances under which
	 they should be written out are not clear to me.  */
      return true;
    }

  /* bfd_ecoff_debug_one_external uses iextMax to keep track of the
     symbol number.  */
  h->indx = ecoff_data (output_bfd)->debug_info.symbolic_header.iextMax;
  h->written = 1;

  return (bfd_ecoff_debug_one_external
	  (output_bfd, &ecoff_data (output_bfd)->debug_info,
	   &ecoff_backend (output_bfd)->debug_swap, h->root.root.string,
	   &h->esym));
}

/* Relocate and write an ECOFF section into an ECOFF output file.  */

static boolean
ecoff_indirect_link_order (output_bfd, info, output_section, link_order)
     bfd *output_bfd;
     struct bfd_link_info *info;
     asection *output_section;
     struct bfd_link_order *link_order;
{
  asection *input_section;
  bfd *input_bfd;
  struct ecoff_section_tdata *section_tdata;
  bfd_size_type raw_size;
  bfd_size_type cooked_size;
  bfd_byte *contents = NULL;
  bfd_size_type external_reloc_size;
  bfd_size_type external_relocs_size;
  PTR external_relocs = NULL;

  BFD_ASSERT ((output_section->flags & SEC_HAS_CONTENTS) != 0);

  if (link_order->size == 0)
    return true;

  input_section = link_order->u.indirect.section;
  input_bfd = input_section->owner;
  section_tdata = ecoff_section_data (input_bfd, input_section);

  raw_size = input_section->_raw_size;
  cooked_size = input_section->_cooked_size;
  if (cooked_size == 0)
    cooked_size = raw_size;

  BFD_ASSERT (input_section->output_section == output_section);
  BFD_ASSERT (input_section->output_offset == link_order->offset);
  BFD_ASSERT (cooked_size == link_order->size);

  /* Get the section contents.  We allocate memory for the larger of
     the size before relocating and the size after relocating.  */
  contents = (bfd_byte *) malloc (raw_size >= cooked_size
				  ? raw_size
				  : cooked_size);
  if (contents == NULL && raw_size != 0)
    {
      bfd_set_error (bfd_error_no_memory);
      goto error_return;
    }

  /* If we are relaxing, the contents may have already been read into
     memory, in which case we copy them into our new buffer.  We don't
     simply reuse the old buffer in case cooked_size > raw_size.  */
  if (section_tdata != (struct ecoff_section_tdata *) NULL
      && section_tdata->contents != (bfd_byte *) NULL)
    memcpy (contents, section_tdata->contents, raw_size);
  else
    {
      if (! bfd_get_section_contents (input_bfd, input_section,
				      (PTR) contents,
				      (file_ptr) 0, raw_size))
	goto error_return;
    }

  /* Get the relocs.  If we are relaxing MIPS code, they will already
     have been read in.  Otherwise, we read them in now.  */
  external_reloc_size = ecoff_backend (input_bfd)->external_reloc_size;
  external_relocs_size = external_reloc_size * input_section->reloc_count;

  if (section_tdata != (struct ecoff_section_tdata *) NULL)
    external_relocs = section_tdata->external_relocs;
  else
    {
      external_relocs = (PTR) malloc (external_relocs_size);
      if (external_relocs == NULL && external_relocs_size != 0)
	{
	  bfd_set_error (bfd_error_no_memory);
	  goto error_return;
	}

      if (bfd_seek (input_bfd, input_section->rel_filepos, SEEK_SET) != 0
	  || (bfd_read (external_relocs, 1, external_relocs_size, input_bfd)
	      != external_relocs_size))
	goto error_return;
    }

  /* Relocate the section contents.  */
  if (! ((*ecoff_backend (input_bfd)->relocate_section)
	 (output_bfd, info, input_bfd, input_section, contents,
	  external_relocs)))
    goto error_return;

  /* Write out the relocated section.  */
  if (! bfd_set_section_contents (output_bfd,
				  output_section,
				  (PTR) contents,
				  input_section->output_offset,
				  cooked_size))
    goto error_return;

  /* If we are producing relocateable output, the relocs were
     modified, and we write them out now.  We use the reloc_count
     field of output_section to keep track of the number of relocs we
     have output so far.  */
  if (info->relocateable)
    {
      if (bfd_seek (output_bfd,
		    (output_section->rel_filepos +
		     output_section->reloc_count * external_reloc_size),
		    SEEK_SET) != 0
	  || (bfd_write (external_relocs, 1, external_relocs_size, output_bfd)
	      != external_relocs_size))
	goto error_return;
      output_section->reloc_count += input_section->reloc_count;
    }

  if (contents != NULL)
    free (contents);
  if (external_relocs != NULL && section_tdata == NULL)
    free (external_relocs);
  return true;

 error_return:
  if (contents != NULL)
    free (contents);
  if (external_relocs != NULL && section_tdata == NULL)
    free (external_relocs);
  return false;
}

/* Generate a reloc when linking an ECOFF file.  This is a reloc
   requested by the linker, and does come from any input file.  This
   is used to build constructor and destructor tables when linking
   with -Ur.  */

static boolean
ecoff_reloc_link_order (output_bfd, info, output_section, link_order)
     bfd *output_bfd;
     struct bfd_link_info *info;
     asection *output_section;
     struct bfd_link_order *link_order;
{
  arelent rel;
  struct internal_reloc in;
  bfd_size_type external_reloc_size;
  bfd_byte *rbuf;
  boolean ok;

  /* We set up an arelent to pass to the backend adjust_reloc_out
     routine.  */
  rel.address = link_order->offset;

  rel.howto = bfd_reloc_type_lookup (output_bfd, link_order->u.reloc.p->reloc);
  if (rel.howto == (const reloc_howto_type *) NULL)
    {
      bfd_set_error (bfd_error_bad_value);
      return false;
    }

  if (link_order->type == bfd_section_reloc_link_order)
    rel.sym_ptr_ptr = link_order->u.reloc.p->u.section->symbol_ptr_ptr;
  else
    {
      /* We can't set up a reloc against a symbol correctly, because
	 we have no asymbol structure.  Currently no adjust_reloc_out
	 routine cases.  */
      rel.sym_ptr_ptr = (asymbol **) NULL;
    }

  /* All ECOFF relocs are in-place.  Put the addend into the object
     file.  */

  BFD_ASSERT (rel.howto->partial_inplace);
  if (link_order->u.reloc.p->addend != 0)
    {
      bfd_size_type size;
      bfd_reloc_status_type rstat;
      bfd_byte *buf;
      boolean ok;

      size = bfd_get_reloc_size (rel.howto);
      buf = (bfd_byte *) bfd_zmalloc (size);
      if (buf == (bfd_byte *) NULL)
	{
	  bfd_set_error (bfd_error_no_memory);
	  return false;
	}
      rstat = _bfd_relocate_contents (rel.howto, output_bfd,
				      link_order->u.reloc.p->addend, buf);
      switch (rstat)
	{
	case bfd_reloc_ok:
	  break;
	default:
	case bfd_reloc_outofrange:
	  abort ();
	case bfd_reloc_overflow:
	  if (! ((*info->callbacks->reloc_overflow)
		 (info,
		  (link_order->type == bfd_section_reloc_link_order
		   ? bfd_section_name (output_bfd,
				       link_order->u.reloc.p->u.section)
		   : link_order->u.reloc.p->u.name),
		  rel.howto->name, link_order->u.reloc.p->addend,
		  (bfd *) NULL, (asection *) NULL, (bfd_vma) 0)))
	    {
	      free (buf);
	      return false;
	    }
	  break;
	}
      ok = bfd_set_section_contents (output_bfd, output_section, (PTR) buf,
				     (file_ptr) link_order->offset, size);
      free (buf);
      if (! ok)
	return false;
    }

  rel.addend = 0;

  /* Move the information into a internal_reloc structure.  */
  in.r_vaddr = (rel.address
		+ bfd_get_section_vma (output_bfd, output_section));
  in.r_type = rel.howto->type;

  if (link_order->type == bfd_symbol_reloc_link_order)
    {
      struct ecoff_link_hash_entry *h;

      h = ecoff_link_hash_lookup (ecoff_hash_table (info),
				  link_order->u.reloc.p->u.name,
				  false, false, true);
      if (h != (struct ecoff_link_hash_entry *) NULL
	  && h->indx != -1)
	in.r_symndx = h->indx;
      else
	{
	  if (! ((*info->callbacks->unattached_reloc)
		 (info, link_order->u.reloc.p->u.name, (bfd *) NULL,
		  (asection *) NULL, (bfd_vma) 0)))
	    return false;
	  in.r_symndx = 0;
	}
      in.r_extern = 1;
    }
  else
    {
      CONST char *name;

      name = bfd_get_section_name (output_bfd,
				   link_order->u.reloc.p->u.section);
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
      else if (strcmp (name, ".xdata") == 0)
	in.r_symndx = RELOC_SECTION_XDATA;
      else if (strcmp (name, ".pdata") == 0)
	in.r_symndx = RELOC_SECTION_PDATA;
      else if (strcmp (name, ".fini") == 0)
	in.r_symndx = RELOC_SECTION_FINI;
      else if (strcmp (name, ".lita") == 0)
	in.r_symndx = RELOC_SECTION_LITA;
      else if (strcmp (name, "*ABS*") == 0)
	in.r_symndx = RELOC_SECTION_ABS;
      else
	abort ();
      in.r_extern = 0;
    }

  /* Let the BFD backend adjust the reloc.  */
  (*ecoff_backend (output_bfd)->adjust_reloc_out) (output_bfd, &rel, &in);

  /* Get some memory and swap out the reloc.  */
  external_reloc_size = ecoff_backend (output_bfd)->external_reloc_size;
  rbuf = (bfd_byte *) malloc (external_reloc_size);
  if (rbuf == (bfd_byte *) NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }

  (*ecoff_backend (output_bfd)->swap_reloc_out) (output_bfd, &in, (PTR) rbuf);

  ok = (bfd_seek (output_bfd,
		  (output_section->rel_filepos +
		   output_section->reloc_count * external_reloc_size),
		  SEEK_SET) == 0
	&& (bfd_write ((PTR) rbuf, 1, external_reloc_size, output_bfd)
	    == external_reloc_size));

  if (ok)
    ++output_section->reloc_count;

  free (rbuf);

  return ok;
}
