/* BFD back-end for IBM RS/6000 "XCOFF" files.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001
   Free Software Foundation, Inc.
   FIXME: Can someone provide a transliteration of this name into ASCII?
   Using the following chars caused a compiler warning on HIUX (so I replaced
   them with octal escapes), and isn't useful without an understanding of what
   character set it is.
   Written by Metin G. Ozisik, Mimi Ph\373\364ng-Th\345o V\365,
     and John Gilmore.
   Archive support from Damon A. Permezel.
   Contributed by IBM Corporation and Cygnus Support.

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
#include "coff/internal.h"
#include "coff/rs6000.h"
#include "libcoff.h"
#define TARGET_NAME "aixcoff-rs6000"
#define TARGET_SYM rs6000coff_vec
#include "xcoff-target.h"

/* The main body of code is in coffcode.h.  */

static const char *normalize_filename PARAMS ((bfd *));

/* We use our own tdata type.  Its first field is the COFF tdata type,
   so the COFF routines are compatible.  */

boolean
_bfd_xcoff_mkobject (abfd)
     bfd *abfd;
{
  coff_data_type *coff;

  abfd->tdata.xcoff_obj_data =
    ((struct xcoff_tdata *)
     bfd_zalloc (abfd, sizeof (struct xcoff_tdata)));
  if (abfd->tdata.xcoff_obj_data == NULL)
    return false;
  coff = coff_data (abfd);
  coff->symbols = (coff_symbol_type *) NULL;
  coff->conversion_table = (unsigned int *) NULL;
  coff->raw_syments = (struct coff_ptr_struct *) NULL;
  coff->relocbase = 0;

  xcoff_data (abfd)->modtype = ('1' << 8) | 'L';

  /* We set cputype to -1 to indicate that it has not been
     initialized.  */
  xcoff_data (abfd)->cputype = -1;

  xcoff_data (abfd)->csects = NULL;
  xcoff_data (abfd)->debug_indices = NULL;

  return true;
}

/* Copy XCOFF data from one BFD to another.  */

boolean
_bfd_xcoff_copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  struct xcoff_tdata *ix, *ox;
  asection *sec;

  if (ibfd->xvec != obfd->xvec)
    return true;
  ix = xcoff_data (ibfd);
  ox = xcoff_data (obfd);
  ox->full_aouthdr = ix->full_aouthdr;
  ox->toc = ix->toc;
  if (ix->sntoc == 0)
    ox->sntoc = 0;
  else
    {
      sec = coff_section_from_bfd_index (ibfd, ix->sntoc);
      if (sec == NULL)
	ox->sntoc = 0;
      else
	ox->sntoc = sec->output_section->target_index;
    }
  if (ix->snentry == 0)
    ox->snentry = 0;
  else
    {
      sec = coff_section_from_bfd_index (ibfd, ix->snentry);
      if (sec == NULL)
	ox->snentry = 0;
      else
	ox->snentry = sec->output_section->target_index;
    }
  ox->text_align_power = ix->text_align_power;
  ox->data_align_power = ix->data_align_power;
  ox->modtype = ix->modtype;
  ox->cputype = ix->cputype;
  ox->maxdata = ix->maxdata;
  ox->maxstack = ix->maxstack;
  return true;
}

/* I don't think XCOFF really has a notion of local labels based on
   name.  This will mean that ld -X doesn't actually strip anything.
   The AIX native linker does not have a -X option, and it ignores the
   -x option.  */

boolean
_bfd_xcoff_is_local_label_name (abfd, name)
     bfd *abfd ATTRIBUTE_UNUSED;
     const char *name ATTRIBUTE_UNUSED;
{
  return false;
}

void
_bfd_xcoff_swap_sym_in (abfd, ext1, in1)
     bfd            *abfd;
     PTR ext1;
     PTR in1;
{
  SYMENT *ext = (SYMENT *)ext1;
  struct internal_syment      *in = (struct internal_syment *)in1;

  if (ext->e.e_name[0] != 0)
    {
      memcpy(in->_n._n_name, ext->e.e_name, SYMNMLEN);
    }
  else
    {
      in->_n._n_n._n_zeroes = 0;
      in->_n._n_n._n_offset =
	  bfd_h_get_32(abfd, (bfd_byte *) ext->e.e.e_offset);
    }

  in->n_value = bfd_h_get_32(abfd, (bfd_byte *) ext->e_value);
  in->n_scnum = bfd_h_get_16(abfd, (bfd_byte *) ext->e_scnum);
  in->n_type = bfd_h_get_16(abfd, (bfd_byte *) ext->e_type);
  in->n_sclass = bfd_h_get_8(abfd, ext->e_sclass);
  in->n_numaux = bfd_h_get_8(abfd, ext->e_numaux);
}

unsigned int
_bfd_xcoff_swap_sym_out (abfd, inp, extp)
     bfd       *abfd;
     PTR	inp;
     PTR	extp;
{
  struct internal_syment *in = (struct internal_syment *)inp;
  SYMENT *ext =(SYMENT *)extp;

  if (in->_n._n_name[0] != 0)
    {
      memcpy(ext->e.e_name, in->_n._n_name, SYMNMLEN);
    }
  else
    {
      bfd_h_put_32(abfd, 0, (bfd_byte *) ext->e.e.e_zeroes);
      bfd_h_put_32(abfd, in->_n._n_n._n_offset,
	      (bfd_byte *)  ext->e.e.e_offset);
    }

  bfd_h_put_32(abfd,  in->n_value , (bfd_byte *) ext->e_value);
  bfd_h_put_16(abfd,  in->n_scnum , (bfd_byte *) ext->e_scnum);
  bfd_h_put_16(abfd,  in->n_type , (bfd_byte *) ext->e_type);
  bfd_h_put_8(abfd,  in->n_sclass , ext->e_sclass);
  bfd_h_put_8(abfd,  in->n_numaux , ext->e_numaux);
  return bfd_coff_symesz (abfd);
}

#define PUTWORD bfd_h_put_32
#define PUTHALF bfd_h_put_16
#define PUTBYTE bfd_h_put_8
#define GETWORD bfd_h_get_32
#define GETHALF bfd_h_get_16
#define GETBYTE bfd_h_get_8

void
_bfd_xcoff_swap_aux_in (abfd, ext1, type, class, indx, numaux, in1)
     bfd            *abfd;
     PTR 	      ext1;
     int             type;
     int             class;
     int	      indx;
     int	      numaux;
     PTR 	      in1;
{
  AUXENT    *ext = (AUXENT *)ext1;
  union internal_auxent *in = (union internal_auxent *)in1;

  switch (class) {
    case C_FILE:
      if (ext->x_file.x_fname[0] == 0) {
	  in->x_file.x_n.x_zeroes = 0;
	  in->x_file.x_n.x_offset =
	   bfd_h_get_32(abfd, (bfd_byte *) ext->x_file.x_n.x_offset);
	} else {
	    if (numaux > 1)
	      {
		if (indx == 0)
		  memcpy (in->x_file.x_fname, ext->x_file.x_fname,
			  numaux * sizeof (AUXENT));
	      }
	    else
	      {
		memcpy (in->x_file.x_fname, ext->x_file.x_fname, FILNMLEN);
	      }
	  }
      goto end;

      /* RS/6000 "csect" auxents */
    case C_EXT:
    case C_HIDEXT:
      if (indx + 1 == numaux)
	{
	  in->x_csect.x_scnlen.l =
	      bfd_h_get_32 (abfd, ext->x_csect.x_scnlen);
	  in->x_csect.x_parmhash = bfd_h_get_32 (abfd,
						 ext->x_csect.x_parmhash);
	  in->x_csect.x_snhash   = bfd_h_get_16 (abfd, ext->x_csect.x_snhash);
	  /* We don't have to hack bitfields in x_smtyp because it's
	     defined by shifts-and-ands, which are equivalent on all
	     byte orders.  */
	  in->x_csect.x_smtyp    = bfd_h_get_8  (abfd, ext->x_csect.x_smtyp);
	  in->x_csect.x_smclas   = bfd_h_get_8  (abfd, ext->x_csect.x_smclas);
	  in->x_csect.x_stab     = bfd_h_get_32 (abfd, ext->x_csect.x_stab);
	  in->x_csect.x_snstab   = bfd_h_get_16 (abfd, ext->x_csect.x_snstab);
	  goto end;
	}
      break;

    case C_STAT:
    case C_LEAFSTAT:
    case C_HIDDEN:
      if (type == T_NULL) {
	  in->x_scn.x_scnlen = bfd_h_get_32(abfd,
		  (bfd_byte *) ext->x_scn.x_scnlen);
	  in->x_scn.x_nreloc = bfd_h_get_16(abfd,
		  (bfd_byte *) ext->x_scn.x_nreloc);
	  in->x_scn.x_nlinno = bfd_h_get_16(abfd,
		  (bfd_byte *) ext->x_scn.x_nlinno);
	  /* PE defines some extra fields; we zero them out for
             safety.  */
	  in->x_scn.x_checksum = 0;
	  in->x_scn.x_associated = 0;
	  in->x_scn.x_comdat = 0;

	  goto end;
	}
      break;
    }

  in->x_sym.x_tagndx.l = bfd_h_get_32(abfd, (bfd_byte *) ext->x_sym.x_tagndx);
  in->x_sym.x_tvndx = bfd_h_get_16(abfd, (bfd_byte *) ext->x_sym.x_tvndx);

  if (class == C_BLOCK || class == C_FCN || ISFCN (type) || ISTAG (class))
    {
      in->x_sym.x_fcnary.x_fcn.x_lnnoptr = bfd_h_get_32(abfd, (bfd_byte *)
	      ext->x_sym.x_fcnary.x_fcn.x_lnnoptr);
      in->x_sym.x_fcnary.x_fcn.x_endndx.l = bfd_h_get_32(abfd, (bfd_byte *)
	      ext->x_sym.x_fcnary.x_fcn.x_endndx);
    }
  else
    {
      in->x_sym.x_fcnary.x_ary.x_dimen[0] =
	bfd_h_get_16 (abfd, (bfd_byte *) ext->x_sym.x_fcnary.x_ary.x_dimen[0]);
      in->x_sym.x_fcnary.x_ary.x_dimen[1] =
	bfd_h_get_16 (abfd, (bfd_byte *) ext->x_sym.x_fcnary.x_ary.x_dimen[1]);
      in->x_sym.x_fcnary.x_ary.x_dimen[2] =
	bfd_h_get_16 (abfd, (bfd_byte *) ext->x_sym.x_fcnary.x_ary.x_dimen[2]);
      in->x_sym.x_fcnary.x_ary.x_dimen[3] =
	bfd_h_get_16 (abfd, (bfd_byte *) ext->x_sym.x_fcnary.x_ary.x_dimen[3]);
    }
  if (ISFCN(type)) {
    in->x_sym.x_misc.x_fsize = bfd_h_get_32(abfd, (bfd_byte *) ext->x_sym.x_misc.x_fsize);
  }
  else {
    in->x_sym.x_misc.x_lnsz.x_lnno = bfd_h_get_16(abfd, (bfd_byte *)
	    ext->x_sym.x_misc.x_lnsz.x_lnno);
    in->x_sym.x_misc.x_lnsz.x_size = bfd_h_get_16(abfd, (bfd_byte *)
	    ext->x_sym.x_misc.x_lnsz.x_size);
  }

end: ;
  /* the semicolon is because MSVC doesn't like labels at
     end of block.  */

}

unsigned int
_bfd_xcoff_swap_aux_out (abfd, inp, type, class, indx, numaux, extp)
     bfd   *abfd;
     PTR 	inp;
     int   type;
     int   class;
     int   indx ATTRIBUTE_UNUSED;
     int   numaux ATTRIBUTE_UNUSED;
     PTR	extp;
{
  union internal_auxent *in = (union internal_auxent *)inp;
  AUXENT *ext = (AUXENT *)extp;

  memset((PTR)ext, 0, bfd_coff_auxesz (abfd));
  switch (class)
    {
  case C_FILE:
    if (in->x_file.x_fname[0] == 0)
      {
      PUTWORD(abfd, 0, (bfd_byte *) ext->x_file.x_n.x_zeroes);
      PUTWORD(abfd,
	      in->x_file.x_n.x_offset,
	      (bfd_byte *) ext->x_file.x_n.x_offset);
    }
    else
      {
      memcpy (ext->x_file.x_fname, in->x_file.x_fname, FILNMLEN);
      }
    goto end;

  /* RS/6000 "csect" auxents */
  case C_EXT:
  case C_HIDEXT:
    if (indx + 1 == numaux)
      {
	PUTWORD (abfd, in->x_csect.x_scnlen.l,ext->x_csect.x_scnlen);
	PUTWORD (abfd, in->x_csect.x_parmhash,	ext->x_csect.x_parmhash);
	PUTHALF (abfd, in->x_csect.x_snhash,	ext->x_csect.x_snhash);
	/* We don't have to hack bitfields in x_smtyp because it's
	   defined by shifts-and-ands, which are equivalent on all
	   byte orders.  */
	PUTBYTE (abfd, in->x_csect.x_smtyp,	ext->x_csect.x_smtyp);
	PUTBYTE (abfd, in->x_csect.x_smclas,	ext->x_csect.x_smclas);
	PUTWORD (abfd, in->x_csect.x_stab,	ext->x_csect.x_stab);
	PUTHALF (abfd, in->x_csect.x_snstab,	ext->x_csect.x_snstab);
	goto end;
      }
    break;

  case C_STAT:
  case C_LEAFSTAT:
  case C_HIDDEN:
    if (type == T_NULL) {
      bfd_h_put_32(abfd, in->x_scn.x_scnlen, (bfd_byte *) ext->x_scn.x_scnlen);
      bfd_h_put_16(abfd, in->x_scn.x_nreloc, (bfd_byte *) ext->x_scn.x_nreloc);
      bfd_h_put_16(abfd, in->x_scn.x_nlinno, (bfd_byte *) ext->x_scn.x_nlinno);
      goto end;
    }
    break;
  }

  PUTWORD(abfd, in->x_sym.x_tagndx.l, (bfd_byte *) ext->x_sym.x_tagndx);
  bfd_h_put_16 (abfd, in->x_sym.x_tvndx , (bfd_byte *) ext->x_sym.x_tvndx);

  if (class == C_BLOCK || class == C_FCN || ISFCN (type) || ISTAG (class))
    {
      bfd_h_put_32(abfd,  in->x_sym.x_fcnary.x_fcn.x_lnnoptr,
	      (bfd_byte *) ext->x_sym.x_fcnary.x_fcn.x_lnnoptr);
      PUTWORD(abfd,  in->x_sym.x_fcnary.x_fcn.x_endndx.l,
	      (bfd_byte *) ext->x_sym.x_fcnary.x_fcn.x_endndx);
    }
  else
    {
      bfd_h_put_16 (abfd, in->x_sym.x_fcnary.x_ary.x_dimen[0],
		    (bfd_byte *) ext->x_sym.x_fcnary.x_ary.x_dimen[0]);
      bfd_h_put_16 (abfd, in->x_sym.x_fcnary.x_ary.x_dimen[1],
		    (bfd_byte *) ext->x_sym.x_fcnary.x_ary.x_dimen[1]);
      bfd_h_put_16 (abfd, in->x_sym.x_fcnary.x_ary.x_dimen[2],
		    (bfd_byte *) ext->x_sym.x_fcnary.x_ary.x_dimen[2]);
      bfd_h_put_16 (abfd, in->x_sym.x_fcnary.x_ary.x_dimen[3],
		    (bfd_byte *) ext->x_sym.x_fcnary.x_ary.x_dimen[3]);
    }

  if (ISFCN (type))
    PUTWORD (abfd, in->x_sym.x_misc.x_fsize,
	     (bfd_byte *)  ext->x_sym.x_misc.x_fsize);
  else
    {
      bfd_h_put_16(abfd, in->x_sym.x_misc.x_lnsz.x_lnno,
	      (bfd_byte *)ext->x_sym.x_misc.x_lnsz.x_lnno);
      bfd_h_put_16(abfd, in->x_sym.x_misc.x_lnsz.x_size,
	      (bfd_byte *)ext->x_sym.x_misc.x_lnsz.x_size);
    }

end:
  return bfd_coff_auxesz (abfd);
}

/* The XCOFF reloc table.  Actually, XCOFF relocations specify the
   bitsize and whether they are signed or not, along with a
   conventional type.  This table is for the types, which are used for
   different algorithms for putting in the reloc.  Many of these
   relocs need special_function entries, which I have not written.  */

/* In case we're on a 32-bit machine, construct a 64-bit "-1" value
   from smaller values.  Start with zero, widen, *then* decrement.  */
#define MINUS_ONE       (((bfd_vma)0) - 1)

reloc_howto_type xcoff_howto_table[] =
{
  /* Standard 32 bit relocation.  */
  HOWTO (0,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_POS",               /* name */
	 true,	                /* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,            /* dst_mask */
	 false),                /* pcrel_offset */

  /* 32 bit relocation, but store negative value.  */
  HOWTO (1,	                /* type */
	 0,	                /* rightshift */
	 -2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_NEG",               /* name */
	 true,	                /* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,            /* dst_mask */
	 false),                /* pcrel_offset */

  /* 32 bit PC relative relocation.  */
  HOWTO (2,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 true,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_REL",               /* name */
	 true,	                /* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,            /* dst_mask */
	 false),                /* pcrel_offset */

  /* 16 bit TOC relative relocation.  */
  HOWTO (3,	                /* type */
	 0,	                /* rightshift */
	 1,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_TOC",               /* name */
	 true,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 false),                /* pcrel_offset */

  /* I don't really know what this is.  */
  HOWTO (4,	                /* type */
	 1,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_RTB",               /* name */
	 true,	                /* partial_inplace */
	 0xffffffff,	        /* src_mask */
	 0xffffffff,        	/* dst_mask */
	 false),                /* pcrel_offset */

  /* External TOC relative symbol.  */
  HOWTO (5,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_GL",                /* name */
	 true,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 false),                /* pcrel_offset */

  /* Local TOC relative symbol.  */
  HOWTO (6,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_TCL",               /* name */
	 true,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 false),                /* pcrel_offset */

  EMPTY_HOWTO (7),

  /* Non modifiable absolute branch.  */
  HOWTO (8,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 26,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_BA",                /* name */
	 true,	                /* partial_inplace */
	 0x3fffffc,	        /* src_mask */
	 0x3fffffc,        	/* dst_mask */
	 false),                /* pcrel_offset */

  EMPTY_HOWTO (9),

  /* Non modifiable relative branch.  */
  HOWTO (0xa,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 26,	                /* bitsize */
	 true,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_BR",                /* name */
	 true,	                /* partial_inplace */
	 0x3fffffc,	        /* src_mask */
	 0x3fffffc,        	/* dst_mask */
	 false),                /* pcrel_offset */

  EMPTY_HOWTO (0xb),

  /* Indirect load.  */
  HOWTO (0xc,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_RL",                /* name */
	 true,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 false),                /* pcrel_offset */

  /* Load address.  */
  HOWTO (0xd,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_RLA",               /* name */
	 true,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 false),                /* pcrel_offset */

  EMPTY_HOWTO (0xe),

  /* Non-relocating reference.  */
  HOWTO (0xf,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_REF",               /* name */
	 false,	                /* partial_inplace */
	 0,		        /* src_mask */
	 0,     	   	/* dst_mask */
	 false),                /* pcrel_offset */

  EMPTY_HOWTO (0x10),
  EMPTY_HOWTO (0x11),

  /* TOC relative indirect load.  */
  HOWTO (0x12,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_TRL",               /* name */
	 true,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 false),                /* pcrel_offset */

  /* TOC relative load address.  */
  HOWTO (0x13,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_TRLA",              /* name */
	 true,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 false),                /* pcrel_offset */

  /* Modifiable relative branch.  */
  HOWTO (0x14,	                /* type */
	 1,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_RRTBI",             /* name */
	 true,	                /* partial_inplace */
	 0xffffffff,	        /* src_mask */
	 0xffffffff,        	/* dst_mask */
	 false),                /* pcrel_offset */

  /* Modifiable absolute branch.  */
  HOWTO (0x15,	                /* type */
	 1,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_RRTBA",             /* name */
	 true,	                /* partial_inplace */
	 0xffffffff,	        /* src_mask */
	 0xffffffff,        	/* dst_mask */
	 false),                /* pcrel_offset */

  /* Modifiable call absolute indirect.  */
  HOWTO (0x16,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_CAI",               /* name */
	 true,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 false),                /* pcrel_offset */

  /* Modifiable call relative.  */
  HOWTO (0x17,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_CREL",              /* name */
	 true,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 false),                /* pcrel_offset */

  /* Modifiable branch absolute.  */
  HOWTO (0x18,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 26,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_RBA",               /* name */
	 true,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 false),                /* pcrel_offset */

  /* Modifiable branch absolute.  */
  HOWTO (0x19,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_RBAC",              /* name */
	 true,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 false),                /* pcrel_offset */

  /* Modifiable branch relative.  */
  HOWTO (0x1a,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 26,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_RBR",               /* name */
	 true,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 false),                /* pcrel_offset */

  /* Modifiable branch absolute.  */
  HOWTO (0x1b,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_RBRC",              /* name */
	 true,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 false),                /* pcrel_offset */
  HOWTO (0,                     /* type */
         0,                     /* rightshift */
         4,                     /* size (0 = byte, 1 = short, 2 = long) */
         64,                    /* bitsize */
         false,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_bitfield, /* complain_on_overflow */
         0,                     /* special_function */
         "R_POS",               /* name */
         true,                  /* partial_inplace */
         MINUS_ONE,             /* src_mask */
	 MINUS_ONE,             /* dst_mask */
	 false)                 /* pcrel_offset */

};

/* These are the first two like the above but for 16-bit relocs.  */
static reloc_howto_type xcoff_howto_table_16[] =
{
  /* Standard 16 bit relocation.  */
  HOWTO (0,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_POS_16",            /* name */
	 true,	                /* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,            /* dst_mask */
	 false),                /* pcrel_offset */

  /* 16 bit relocation, but store negative value.  */
  HOWTO (1,	                /* type */
	 0,	                /* rightshift */
	 -2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_NEG_16",            /* name */
	 true,	                /* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,            /* dst_mask */
	 false),                /* pcrel_offset */

  /* 16 bit PC relative relocation.  */
  HOWTO (2,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 true,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_REL_16",            /* name */
	 true,	                /* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,            /* dst_mask */
	 false)                /* pcrel_offset */
  };

void
_bfd_xcoff_rtype2howto (relent, internal)
     arelent *relent;
     struct internal_reloc *internal;
{
  relent->howto = xcoff_howto_table + internal->r_type;

  if (relent->howto->bitsize != ((unsigned int) internal->r_size & 0x1f) + 1
      && (internal->r_type
	  < sizeof (xcoff_howto_table_16)/sizeof (xcoff_howto_table_16[0])))
    relent->howto = xcoff_howto_table_16 + internal->r_type;

  /* The r_size field of an XCOFF reloc encodes the bitsize of the
     relocation, as well as indicating whether it is signed or not.
     Doublecheck that the relocation information gathered from the
     type matches this information.  The bitsize is not significant
     for R_REF relocs.  */
  if (relent->howto->dst_mask != 0
      && (relent->howto->bitsize
	  != ((unsigned int) internal->r_size & 0x3f) + 1))
    abort ();
#if 0
  if ((internal->r_size & 0x80) != 0
      ? (relent->howto->complain_on_overflow != complain_overflow_signed)
      : (relent->howto->complain_on_overflow != complain_overflow_bitfield))
    abort ();
#endif
}

reloc_howto_type *
_bfd_xcoff_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
    case BFD_RELOC_PPC_B26:
      return &xcoff_howto_table[0xa];
    case BFD_RELOC_PPC_BA26:
      return &xcoff_howto_table[8];
    case BFD_RELOC_PPC_TOC16:
      return &xcoff_howto_table[3];
    case BFD_RELOC_32:
    case BFD_RELOC_CTOR:
      return &xcoff_howto_table[0];
    case BFD_RELOC_64:
      return &xcoff_howto_table[0x1c];
    default:
      return NULL;
    }
}

/* XCOFF archive support.  The original version of this code was by
   Damon A. Permezel.  It was enhanced to permit cross support, and
   writing archive files, by Ian Lance Taylor, Cygnus Support.

   XCOFF uses its own archive format.  Everything is hooked together
   with file offset links, so it is possible to rapidly update an
   archive in place.  Of course, we don't do that.  An XCOFF archive
   has a real file header, not just an ARMAG string.  The structure of
   the file header and of each archive header appear below.

   An XCOFF archive also has a member table, which is a list of
   elements in the archive (you can get that by looking through the
   linked list, but you have to read a lot more of the file).  The
   member table has a normal archive header with an empty name.  It is
   normally (and perhaps must be) the second to last entry in the
   archive.  The member table data is almost printable ASCII.  It
   starts with a 12 character decimal string which is the number of
   entries in the table.  For each entry it has a 12 character decimal
   string which is the offset in the archive of that member.  These
   entries are followed by a series of null terminated strings which
   are the member names for each entry.

   Finally, an XCOFF archive has a global symbol table, which is what
   we call the armap.  The global symbol table has a normal archive
   header with an empty name.  It is normally (and perhaps must be)
   the last entry in the archive.  The contents start with a four byte
   binary number which is the number of entries.  This is followed by
   a that many four byte binary numbers; each is the file offset of an
   entry in the archive.  These numbers are followed by a series of
   null terminated strings, which are symbol names.

   AIX 4.3 introduced a new archive format which can handle larger
   files and also 32- and 64-bit objects in the same archive.  The
   things said above remain true except that there is now more than
   one global symbol table.  The one is used to index 32-bit objects,
   the other for 64-bit objects.

   The new archives (recognizable by the new ARMAG string) has larger
   field lengths so that we cannot really share any code.  Also we have
   to take care that we are not generating the new form of archives
   on AIX 4.2 or earlier systems.  */

/* XCOFF archives use this as a magic string.  Note that both strings
   have the same length.  */

#define XCOFFARMAG    "<aiaff>\012"
#define XCOFFARMAGBIG "<bigaf>\012"
#define SXCOFFARMAG   8

/* This terminates an XCOFF archive member name.  */

#define XCOFFARFMAG "`\012"
#define SXCOFFARFMAG 2

/* XCOFF archives start with this (printable) structure.  */

struct xcoff_ar_file_hdr
{
  /* Magic string.  */
  char magic[SXCOFFARMAG];

  /* Offset of the member table (decimal ASCII string).  */
  char memoff[12];

  /* Offset of the global symbol table (decimal ASCII string).  */
  char symoff[12];

  /* Offset of the first member in the archive (decimal ASCII string).  */
  char firstmemoff[12];

  /* Offset of the last member in the archive (decimal ASCII string).  */
  char lastmemoff[12];

  /* Offset of the first member on the free list (decimal ASCII
     string).  */
  char freeoff[12];
};

#define SIZEOF_AR_FILE_HDR (5 * 12 + SXCOFFARMAG)

/* This is the equivalent data structure for the big archive format.  */

struct xcoff_ar_file_hdr_big
{
  /* Magic string.  */
  char magic[SXCOFFARMAG];

  /* Offset of the member table (decimal ASCII string).  */
  char memoff[20];

  /* Offset of the global symbol table for 32-bit objects (decimal ASCII
     string).  */
  char symoff[20];

  /* Offset of the global symbol table for 64-bit objects (decimal ASCII
     string).  */
  char symoff64[20];

  /* Offset of the first member in the archive (decimal ASCII string).  */
  char firstmemoff[20];

  /* Offset of the last member in the archive (decimal ASCII string).  */
  char lastmemoff[20];

  /* Offset of the first member on the free list (decimal ASCII
     string).  */
  char freeoff[20];
};

#define SIZEOF_AR_FILE_HDR_BIG (6 * 20 + SXCOFFARMAG)

/* Each XCOFF archive member starts with this (printable) structure.  */

struct xcoff_ar_hdr
{
  /* File size not including the header (decimal ASCII string).  */
  char size[12];

  /* File offset of next archive member (decimal ASCII string).  */
  char nextoff[12];

  /* File offset of previous archive member (decimal ASCII string).  */
  char prevoff[12];

  /* File mtime (decimal ASCII string).  */
  char date[12];

  /* File UID (decimal ASCII string).  */
  char uid[12];

  /* File GID (decimal ASCII string).  */
  char gid[12];

  /* File mode (octal ASCII string).  */
  char mode[12];

  /* Length of file name (decimal ASCII string).  */
  char namlen[4];

  /* This structure is followed by the file name.  The length of the
     name is given in the namlen field.  If the length of the name is
     odd, the name is followed by a null byte.  The name and optional
     null byte are followed by XCOFFARFMAG, which is not included in
     namlen.  The contents of the archive member follow; the number of
     bytes is given in the size field.  */
};

#define SIZEOF_AR_HDR (7 * 12 + 4)

/* The equivalent for the big archive format.  */

struct xcoff_ar_hdr_big
{
  /* File size not including the header (decimal ASCII string).  */
  char size[20];

  /* File offset of next archive member (decimal ASCII string).  */
  char nextoff[20];

  /* File offset of previous archive member (decimal ASCII string).  */
  char prevoff[20];

  /* File mtime (decimal ASCII string).  */
  char date[12];

  /* File UID (decimal ASCII string).  */
  char uid[12];

  /* File GID (decimal ASCII string).  */
  char gid[12];

  /* File mode (octal ASCII string).  */
  char mode[12];

  /* Length of file name (decimal ASCII string).  */
  char namlen[4];

  /* This structure is followed by the file name.  The length of the
     name is given in the namlen field.  If the length of the name is
     odd, the name is followed by a null byte.  The name and optional
     null byte are followed by XCOFFARFMAG, which is not included in
     namlen.  The contents of the archive member follow; the number of
     bytes is given in the size field.  */
};

#define SIZEOF_AR_HDR_BIG (3 * 20 + 4 * 12 + 4)

/* We often have to distinguish between the old and big file format.
   Make it a bit cleaner.  We can use `xcoff_ardata' here because the
   `hdr' member has the same size and position in both formats.  */
#define xcoff_big_format_p(abfd) \
  (xcoff_ardata (abfd)->magic[1] == 'b')

/* We store a copy of the xcoff_ar_file_hdr in the tdata field of the
   artdata structure.  Similar for the big archive.  */
#define xcoff_ardata(abfd) \
  ((struct xcoff_ar_file_hdr *) bfd_ardata (abfd)->tdata)
#define xcoff_ardata_big(abfd) \
  ((struct xcoff_ar_file_hdr_big *) bfd_ardata (abfd)->tdata)

/* We store a copy of the xcoff_ar_hdr in the arelt_data field of an
   archive element.  Similar for the big archive.  */
#define arch_eltdata(bfd) ((struct areltdata *) ((bfd)->arelt_data))
#define arch_xhdr(bfd) \
  ((struct xcoff_ar_hdr *) arch_eltdata (bfd)->arch_header)
#define arch_xhdr_big(bfd) \
  ((struct xcoff_ar_hdr_big *) arch_eltdata (bfd)->arch_header)

/* Read in the armap of an XCOFF archive.  */

boolean
_bfd_xcoff_slurp_armap (abfd)
     bfd *abfd;
{
  file_ptr off;
  size_t namlen;
  bfd_size_type sz;
  bfd_byte *contents, *cend;
  bfd_vma c, i;
  carsym *arsym;
  bfd_byte *p;

  if (xcoff_ardata (abfd) == NULL)
    {
      bfd_has_map (abfd) = false;
      return true;
    }

  if (! xcoff_big_format_p (abfd))
    {
      /* This is for the old format.  */
      struct xcoff_ar_hdr hdr;

      off = strtol (xcoff_ardata (abfd)->symoff, (char **) NULL, 10);
      if (off == 0)
	{
	  bfd_has_map (abfd) = false;
	  return true;
	}

      if (bfd_seek (abfd, off, SEEK_SET) != 0)
	return false;

      /* The symbol table starts with a normal archive header.  */
      if (bfd_read ((PTR) &hdr, SIZEOF_AR_HDR, 1, abfd) != SIZEOF_AR_HDR)
	return false;

      /* Skip the name (normally empty).  */
      namlen = strtol (hdr.namlen, (char **) NULL, 10);
      if (bfd_seek (abfd, ((namlen + 1) & ~1) + SXCOFFARFMAG, SEEK_CUR) != 0)
	return false;

      sz = strtol (hdr.size, (char **) NULL, 10);

      /* Read in the entire symbol table.  */
      contents = (bfd_byte *) bfd_alloc (abfd, sz);
      if (contents == NULL)
	return false;
      if (bfd_read ((PTR) contents, 1, sz, abfd) != sz)
	return false;

      /* The symbol table starts with a four byte count.  */
      c = bfd_h_get_32 (abfd, contents);

      if (c * 4 >= sz)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}

      bfd_ardata (abfd)->symdefs = ((carsym *)
				    bfd_alloc (abfd, c * sizeof (carsym)));
      if (bfd_ardata (abfd)->symdefs == NULL)
	return false;

      /* After the count comes a list of four byte file offsets.  */
      for (i = 0, arsym = bfd_ardata (abfd)->symdefs, p = contents + 4;
	   i < c;
	   ++i, ++arsym, p += 4)
	arsym->file_offset = bfd_h_get_32 (abfd, p);
    }
  else
    {
      /* This is for the new format.  */
      struct xcoff_ar_hdr_big hdr;

      off = strtol (xcoff_ardata_big (abfd)->symoff, (char **) NULL, 10);
      if (off == 0)
	{
	  bfd_has_map (abfd) = false;
	  return true;
	}

      if (bfd_seek (abfd, off, SEEK_SET) != 0)
	return false;

      /* The symbol table starts with a normal archive header.  */
      if (bfd_read ((PTR) &hdr, SIZEOF_AR_HDR_BIG, 1, abfd)
	  != SIZEOF_AR_HDR_BIG)
	return false;

      /* Skip the name (normally empty).  */
      namlen = strtol (hdr.namlen, (char **) NULL, 10);
      if (bfd_seek (abfd, ((namlen + 1) & ~1) + SXCOFFARFMAG, SEEK_CUR) != 0)
	return false;

      /* XXX This actually has to be a call to strtoll (at least on 32-bit
	 machines) since the field width is 20 and there numbers with more
	 than 32 bits can be represented.  */
      sz = strtol (hdr.size, (char **) NULL, 10);

      /* Read in the entire symbol table.  */
      contents = (bfd_byte *) bfd_alloc (abfd, sz);
      if (contents == NULL)
	return false;
      if (bfd_read ((PTR) contents, 1, sz, abfd) != sz)
	return false;

      /* The symbol table starts with an eight byte count.  */
      c = bfd_h_get_64 (abfd, contents);

      if (c * 8 >= sz)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}

      bfd_ardata (abfd)->symdefs = ((carsym *)
				    bfd_alloc (abfd, c * sizeof (carsym)));
      if (bfd_ardata (abfd)->symdefs == NULL)
	return false;

      /* After the count comes a list of eight byte file offsets.  */
      for (i = 0, arsym = bfd_ardata (abfd)->symdefs, p = contents + 8;
	   i < c;
	   ++i, ++arsym, p += 8)
	arsym->file_offset = bfd_h_get_64 (abfd, p);
    }

  /* After the file offsets come null terminated symbol names.  */
  cend = contents + sz;
  for (i = 0, arsym = bfd_ardata (abfd)->symdefs;
       i < c;
       ++i, ++arsym, p += strlen ((char *) p) + 1)
    {
      if (p >= cend)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
      arsym->name = (char *) p;
    }

  bfd_ardata (abfd)->symdef_count = c;
  bfd_has_map (abfd) = true;

  return true;
}

/* See if this is an XCOFF archive.  */

const bfd_target *
_bfd_xcoff_archive_p (abfd)
     bfd *abfd;
{
  char magic[SXCOFFARMAG];

  if (bfd_read ((PTR) magic, SXCOFFARMAG, 1, abfd) != SXCOFFARMAG)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (strncmp (magic, XCOFFARMAG, SXCOFFARMAG) != 0
      && strncmp (magic, XCOFFARMAGBIG, SXCOFFARMAG) != 0)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* We are setting bfd_ardata(abfd) here, but since bfd_ardata
     involves a cast, we can't do it as the left operand of
     assignment.  */
  abfd->tdata.aout_ar_data =
    (struct artdata *) bfd_zalloc (abfd, sizeof (struct artdata));

  if (bfd_ardata (abfd) == (struct artdata *) NULL)
    return NULL;

  bfd_ardata (abfd)->cache = NULL;
  bfd_ardata (abfd)->archive_head = NULL;
  bfd_ardata (abfd)->symdefs = NULL;
  bfd_ardata (abfd)->extended_names = NULL;

  /* Now handle the two formats.  */
  if (magic[1] != 'b')
    {
      /* This is the old format.  */
      struct xcoff_ar_file_hdr hdr;

      /* Copy over the magic string.  */
      memcpy (hdr.magic, magic, SXCOFFARMAG);

      /* Now read the rest of the file header.  */
      if (bfd_read ((PTR) &hdr.memoff, SIZEOF_AR_FILE_HDR - SXCOFFARMAG, 1,
		    abfd) != SIZEOF_AR_FILE_HDR - SXCOFFARMAG)
	{
	  if (bfd_get_error () != bfd_error_system_call)
	    bfd_set_error (bfd_error_wrong_format);
	  return NULL;
	}

      bfd_ardata (abfd)->first_file_filepos = strtol (hdr.firstmemoff,
						      (char **) NULL, 10);

      bfd_ardata (abfd)->tdata = bfd_zalloc (abfd, SIZEOF_AR_FILE_HDR);
      if (bfd_ardata (abfd)->tdata == NULL)
	return NULL;

      memcpy (bfd_ardata (abfd)->tdata, &hdr, SIZEOF_AR_FILE_HDR);
    }
  else
    {
      /* This is the new format.  */
      struct xcoff_ar_file_hdr_big hdr;

      /* Copy over the magic string.  */
      memcpy (hdr.magic, magic, SXCOFFARMAG);

      /* Now read the rest of the file header.  */
      if (bfd_read ((PTR) &hdr.memoff, SIZEOF_AR_FILE_HDR_BIG - SXCOFFARMAG, 1,
		    abfd) != SIZEOF_AR_FILE_HDR_BIG - SXCOFFARMAG)
	{
	  if (bfd_get_error () != bfd_error_system_call)
	    bfd_set_error (bfd_error_wrong_format);
	  return NULL;
	}

      /* XXX This actually has to be a call to strtoll (at least on 32-bit
	 machines) since the field width is 20 and there numbers with more
	 than 32 bits can be represented.  */
      bfd_ardata (abfd)->first_file_filepos = strtol (hdr.firstmemoff,
						      (char **) NULL, 10);

      bfd_ardata (abfd)->tdata = bfd_zalloc (abfd, SIZEOF_AR_FILE_HDR_BIG);
      if (bfd_ardata (abfd)->tdata == NULL)
	return NULL;

      memcpy (bfd_ardata (abfd)->tdata, &hdr, SIZEOF_AR_FILE_HDR_BIG);
    }

  if (! _bfd_xcoff_slurp_armap (abfd))
    {
      bfd_release (abfd, bfd_ardata (abfd));
      abfd->tdata.aout_ar_data = (struct artdata *) NULL;
      return NULL;
    }

  return abfd->xvec;
}

/* Read the archive header in an XCOFF archive.  */

PTR
_bfd_xcoff_read_ar_hdr (abfd)
     bfd *abfd;
{
  size_t namlen;
  struct areltdata *ret;

  ret = (struct areltdata *) bfd_alloc (abfd, sizeof (struct areltdata));
  if (ret == NULL)
    return NULL;

  if (! xcoff_big_format_p (abfd))
    {
      struct xcoff_ar_hdr hdr;
      struct xcoff_ar_hdr *hdrp;

      if (bfd_read ((PTR) &hdr, SIZEOF_AR_HDR, 1, abfd) != SIZEOF_AR_HDR)
	{
	  free (ret);
	  return NULL;
	}

      namlen = strtol (hdr.namlen, (char **) NULL, 10);
      hdrp = (struct xcoff_ar_hdr *) bfd_alloc (abfd,
						SIZEOF_AR_HDR + namlen + 1);
      if (hdrp == NULL)
	{
	  free (ret);
	  return NULL;
	}
      memcpy (hdrp, &hdr, SIZEOF_AR_HDR);
      if (bfd_read ((char *) hdrp + SIZEOF_AR_HDR, 1, namlen, abfd) != namlen)
	{
	  free (ret);
	  return NULL;
	}
      ((char *) hdrp)[SIZEOF_AR_HDR + namlen] = '\0';

      ret->arch_header = (char *) hdrp;
      ret->parsed_size = strtol (hdr.size, (char **) NULL, 10);
      ret->filename = (char *) hdrp + SIZEOF_AR_HDR;
    }
  else
    {
      struct xcoff_ar_hdr_big hdr;
      struct xcoff_ar_hdr_big *hdrp;

      if (bfd_read ((PTR) &hdr, SIZEOF_AR_HDR_BIG, 1, abfd)
	  != SIZEOF_AR_HDR_BIG)
	{
	  free (ret);
	  return NULL;
	}

      namlen = strtol (hdr.namlen, (char **) NULL, 10);
      hdrp = (struct xcoff_ar_hdr_big *) bfd_alloc (abfd,
						    SIZEOF_AR_HDR_BIG
						    + namlen + 1);
      if (hdrp == NULL)
	{
	  free (ret);
	  return NULL;
	}
      memcpy (hdrp, &hdr, SIZEOF_AR_HDR_BIG);
      if (bfd_read ((char *) hdrp + SIZEOF_AR_HDR_BIG, 1, namlen, abfd) != namlen)
	{
	  free (ret);
	  return NULL;
	}
      ((char *) hdrp)[SIZEOF_AR_HDR_BIG + namlen] = '\0';

      ret->arch_header = (char *) hdrp;
      /* XXX This actually has to be a call to strtoll (at least on 32-bit
	 machines) since the field width is 20 and there numbers with more
	 than 32 bits can be represented.  */
      ret->parsed_size = strtol (hdr.size, (char **) NULL, 10);
      ret->filename = (char *) hdrp + SIZEOF_AR_HDR_BIG;
    }

  /* Skip over the XCOFFARFMAG at the end of the file name.  */
  if (bfd_seek (abfd, (namlen & 1) + SXCOFFARFMAG, SEEK_CUR) != 0)
    return NULL;

  return (PTR) ret;
}

/* Open the next element in an XCOFF archive.  */

bfd *
_bfd_xcoff_openr_next_archived_file (archive, last_file)
     bfd *archive;
     bfd *last_file;
{
  file_ptr filestart;

  if (xcoff_ardata (archive) == NULL)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return NULL;
    }

  if (! xcoff_big_format_p (archive))
    {
      if (last_file == NULL)
	filestart = bfd_ardata (archive)->first_file_filepos;
      else
	filestart = strtol (arch_xhdr (last_file)->nextoff, (char **) NULL,
			    10);

      if (filestart == 0
	  || filestart == strtol (xcoff_ardata (archive)->memoff,
				  (char **) NULL, 10)
	  || filestart == strtol (xcoff_ardata (archive)->symoff,
				  (char **) NULL, 10))
	{
	  bfd_set_error (bfd_error_no_more_archived_files);
	  return NULL;
	}
    }
  else
    {
      if (last_file == NULL)
	filestart = bfd_ardata (archive)->first_file_filepos;
      else
	/* XXX These actually have to be a calls to strtoll (at least
	   on 32-bit machines) since the fields's width is 20 and
	   there numbers with more than 32 bits can be represented.  */
	filestart = strtol (arch_xhdr_big (last_file)->nextoff, (char **) NULL,
			    10);

      /* XXX These actually have to be calls to strtoll (at least on 32-bit
	 machines) since the fields's width is 20 and there numbers with more
	 than 32 bits can be represented.  */
      if (filestart == 0
	  || filestart == strtol (xcoff_ardata_big (archive)->memoff,
				  (char **) NULL, 10)
	  || filestart == strtol (xcoff_ardata_big (archive)->symoff,
				  (char **) NULL, 10))
	{
	  bfd_set_error (bfd_error_no_more_archived_files);
	  return NULL;
	}
    }

  return _bfd_get_elt_at_filepos (archive, filestart);
}

/* Stat an element in an XCOFF archive.  */

int
_bfd_xcoff_generic_stat_arch_elt (abfd, s)
     bfd *abfd;
     struct stat *s;
{
  if (abfd->arelt_data == NULL)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }

  if (! xcoff_big_format_p (abfd))
    {
      struct xcoff_ar_hdr *hdrp = arch_xhdr (abfd);

      s->st_mtime = strtol (hdrp->date, (char **) NULL, 10);
      s->st_uid = strtol (hdrp->uid, (char **) NULL, 10);
      s->st_gid = strtol (hdrp->gid, (char **) NULL, 10);
      s->st_mode = strtol (hdrp->mode, (char **) NULL, 8);
      s->st_size = arch_eltdata (abfd)->parsed_size;
    }
  else
    {
      struct xcoff_ar_hdr_big *hdrp = arch_xhdr_big (abfd);

      s->st_mtime = strtol (hdrp->date, (char **) NULL, 10);
      s->st_uid = strtol (hdrp->uid, (char **) NULL, 10);
      s->st_gid = strtol (hdrp->gid, (char **) NULL, 10);
      s->st_mode = strtol (hdrp->mode, (char **) NULL, 8);
      s->st_size = arch_eltdata (abfd)->parsed_size;
    }

  return 0;
}

/* Normalize a file name for inclusion in an archive.  */

static const char *
normalize_filename (abfd)
     bfd *abfd;
{
  const char *file;
  const char *filename;

  file = bfd_get_filename (abfd);
  filename = strrchr (file, '/');
  if (filename != NULL)
    filename++;
  else
    filename = file;
  return filename;
}

/* Write out an XCOFF armap.  */

static boolean
xcoff_write_armap_old (abfd, elength, map, orl_count, stridx)
     bfd *abfd;
     unsigned int elength ATTRIBUTE_UNUSED;
     struct orl *map;
     unsigned int orl_count;
     int stridx;
{
  struct xcoff_ar_hdr hdr;
  char *p;
  unsigned char buf[4];
  bfd *sub;
  file_ptr fileoff;
  unsigned int i;

  memset (&hdr, 0, sizeof hdr);
  sprintf (hdr.size, "%ld", (long) (4 + orl_count * 4 + stridx));
  sprintf (hdr.nextoff, "%d", 0);
  memcpy (hdr.prevoff, xcoff_ardata (abfd)->memoff, 12);
  sprintf (hdr.date, "%d", 0);
  sprintf (hdr.uid, "%d", 0);
  sprintf (hdr.gid, "%d", 0);
  sprintf (hdr.mode, "%d", 0);
  sprintf (hdr.namlen, "%d", 0);

  /* We need spaces, not null bytes, in the header.  */
  for (p = (char *) &hdr; p < (char *) &hdr + SIZEOF_AR_HDR; p++)
    if (*p == '\0')
      *p = ' ';

  if (bfd_write ((PTR) &hdr, SIZEOF_AR_HDR, 1, abfd) != SIZEOF_AR_HDR
      || bfd_write (XCOFFARFMAG, 1, SXCOFFARFMAG, abfd) != SXCOFFARFMAG)
    return false;

  bfd_h_put_32 (abfd, orl_count, buf);
  if (bfd_write (buf, 1, 4, abfd) != 4)
    return false;

  sub = abfd->archive_head;
  fileoff = SIZEOF_AR_FILE_HDR;
  i = 0;
  while (sub != NULL && i < orl_count)
    {
      size_t namlen;

      while (((bfd *) (map[i]).pos) == sub)
	{
	  bfd_h_put_32 (abfd, fileoff, buf);
	  if (bfd_write (buf, 1, 4, abfd) != 4)
	    return false;
	  ++i;
	}
      namlen = strlen (normalize_filename (sub));
      namlen = (namlen + 1) &~ 1;
      fileoff += (SIZEOF_AR_HDR
		  + namlen
		  + SXCOFFARFMAG
		  + arelt_size (sub));
      fileoff = (fileoff + 1) &~ 1;
      sub = sub->next;
    }

  for (i = 0; i < orl_count; i++)
    {
      const char *name;
      size_t namlen;

      name = *map[i].name;
      namlen = strlen (name);
      if (bfd_write (name, 1, namlen + 1, abfd) != namlen + 1)
	return false;
    }

  if ((stridx & 1) != 0)
    {
      char b;

      b = '\0';
      if (bfd_write (&b, 1, 1, abfd) != 1)
	return false;
    }

  return true;
}

/* Write a single armap in the big format.  */
static boolean
xcoff_write_one_armap_big (abfd, map, orl_count, orl_ccount, stridx, bits64,
			   prevoff, nextoff)
     bfd *abfd;
     struct orl *map;
     unsigned int orl_count;
     unsigned int orl_ccount;
     unsigned int stridx;
     int bits64;
     const char *prevoff;
     char *nextoff;
{
  struct xcoff_ar_hdr_big hdr;
  char *p;
  unsigned char buf[4];
  const bfd_arch_info_type *arch_info = NULL;
  bfd *sub;
  file_ptr fileoff;
  bfd *object_bfd;
  unsigned int i;

  memset (&hdr, 0, sizeof hdr);
  /* XXX This call actually should use %lld (at least on 32-bit
     machines) since the fields's width is 20 and there numbers with
     more than 32 bits can be represented.  */
  sprintf (hdr.size, "%ld", (long) (4 + orl_ccount * 4 + stridx));
  if (bits64)
    sprintf (hdr.nextoff, "%d", 0);
  else
    sprintf (hdr.nextoff, "%ld", (strtol (prevoff, (char **) NULL, 10)
				 + 4 + orl_ccount * 4 + stridx));
  memcpy (hdr.prevoff, prevoff, sizeof (hdr.prevoff));
  sprintf (hdr.date, "%d", 0);
  sprintf (hdr.uid, "%d", 0);
  sprintf (hdr.gid, "%d", 0);
  sprintf (hdr.mode, "%d", 0);
  sprintf (hdr.namlen, "%d", 0);

  /* We need spaces, not null bytes, in the header.  */
  for (p = (char *) &hdr; p < (char *) &hdr + SIZEOF_AR_HDR_BIG; p++)
    if (*p == '\0')
      *p = ' ';

  memcpy (nextoff, hdr.nextoff, sizeof (hdr.nextoff));

  if (bfd_write ((PTR) &hdr, SIZEOF_AR_HDR_BIG, 1, abfd) != SIZEOF_AR_HDR_BIG
      || bfd_write (XCOFFARFMAG, 1, SXCOFFARFMAG, abfd) != SXCOFFARFMAG)
    return false;

  bfd_h_put_32 (abfd, orl_ccount, buf);
  if (bfd_write (buf, 1, 4, abfd) != 4)
    return false;

  sub = abfd->archive_head;
  fileoff = SIZEOF_AR_FILE_HDR_BIG;
  i = 0;
  while (sub != NULL && i < orl_count)
    {
      size_t namlen;

      if ((bfd_arch_bits_per_address ((bfd *) map[i].pos) == 64) == bits64)
	while (((bfd *) (map[i]).pos) == sub)
	  {
	    bfd_h_put_32 (abfd, fileoff, buf);
	    if (bfd_write (buf, 1, 4, abfd) != 4)
	      return false;
	    i++;
	  }
      else
	while (((bfd *) (map[i]).pos) == sub)
	  i++;

      namlen = strlen (normalize_filename (sub));
      namlen = (namlen + 1) &~ 1;
      fileoff += (SIZEOF_AR_HDR_BIG
		  + namlen
		  + SXCOFFARFMAG
		  + arelt_size (sub));
      fileoff = (fileoff + 1) &~ 1;
      sub = sub->next;
    }

  object_bfd = NULL;
  for (i = 0; i < orl_count; i++)
    {
      const char *name;
      size_t namlen;
      bfd *ob = (bfd *)map[i].pos;

      if (ob != object_bfd)
	arch_info = bfd_get_arch_info (ob);

      if (arch_info && (arch_info->bits_per_address == 64) != bits64)
	continue;

      name = *map[i].name;
      namlen = strlen (name);
      if (bfd_write (name, 1, namlen + 1, abfd) != namlen + 1)
	return false;
    }

  if ((stridx & 1) != 0)
    {
      char b;

      b = '\0';
      if (bfd_write (&b, 1, 1, abfd) != 1)
	return false;
    }

  return true;
}

static boolean
xcoff_write_armap_big (abfd, elength, map, orl_count, stridx)
     bfd *abfd;
     unsigned int elength ATTRIBUTE_UNUSED;
     struct orl *map;
     unsigned int orl_count;
     int stridx;
{
  unsigned int i;
  unsigned int orl_count_32, orl_count_64;
  unsigned int stridx_32, stridx_64;
  const bfd_arch_info_type *arch_info = NULL;
  bfd *object_bfd;

  /* First, we look through the symbols and work out which are
     from 32-bit objects and which from 64-bit ones.  */
  orl_count_32 = 0;
  orl_count_64 = 0;
  stridx_32 = 0;
  stridx_64 = 0;
  object_bfd = NULL;
  for (i = 0; i < orl_count; i++)
    {
      bfd *ob = (bfd *)map[i].pos;
      unsigned int len;
      if (ob != object_bfd)
	arch_info = bfd_get_arch_info (ob);
      len = strlen (*map[i].name) + 1;
      if (arch_info && arch_info->bits_per_address == 64)
	{
	  orl_count_64++;
	  stridx_64 += len;
	}
      else
	{
	  orl_count_32++;
	  stridx_32 += len;
	}
      object_bfd = ob;
    }
  /* A quick sanity check...  */
  BFD_ASSERT (orl_count_64 + orl_count_32 == orl_count);
  BFD_ASSERT (stridx_64 + stridx_32 == stridx);

  /* Now write out each map.  */
  if (! xcoff_write_one_armap_big (abfd, map, orl_count, orl_count_32,
				   stridx_32, false,
				   xcoff_ardata_big (abfd)->memoff,
				   xcoff_ardata_big (abfd)->symoff))
    return false;
  if (! xcoff_write_one_armap_big (abfd, map, orl_count, orl_count_64,
				   stridx_64, true,
				   xcoff_ardata_big (abfd)->symoff,
				   xcoff_ardata_big (abfd)->symoff64))
    return false;

  return true;
}

boolean
_bfd_xcoff_write_armap (abfd, elength, map, orl_count, stridx)
     bfd *abfd;
     unsigned int elength ATTRIBUTE_UNUSED;
     struct orl *map;
     unsigned int orl_count;
     int stridx;
{
  if (! xcoff_big_format_p (abfd))
    return xcoff_write_armap_old (abfd, elength, map, orl_count, stridx);
  else
    return xcoff_write_armap_big (abfd, elength, map, orl_count, stridx);
}

/* Write out an XCOFF archive.  We always write an entire archive,
   rather than fussing with the freelist and so forth.  */

static boolean
xcoff_write_archive_contents_old (abfd)
     bfd *abfd;
{
  struct xcoff_ar_file_hdr fhdr;
  size_t count;
  size_t total_namlen;
  file_ptr *offsets;
  boolean makemap;
  boolean hasobjects;
  file_ptr prevoff, nextoff;
  bfd *sub;
  unsigned int i;
  struct xcoff_ar_hdr ahdr;
  bfd_size_type size;
  char *p;
  char decbuf[13];

  memset (&fhdr, 0, sizeof fhdr);
  strncpy (fhdr.magic, XCOFFARMAG, SXCOFFARMAG);
  sprintf (fhdr.firstmemoff, "%d", SIZEOF_AR_FILE_HDR);
  sprintf (fhdr.freeoff, "%d", 0);

  count = 0;
  total_namlen = 0;
  for (sub = abfd->archive_head; sub != NULL; sub = sub->next)
    {
      ++count;
      total_namlen += strlen (normalize_filename (sub)) + 1;
    }
  offsets = (file_ptr *) bfd_alloc (abfd, count * sizeof (file_ptr));
  if (offsets == NULL)
    return false;

  if (bfd_seek (abfd, SIZEOF_AR_FILE_HDR, SEEK_SET) != 0)
    return false;

  makemap = bfd_has_map (abfd);
  hasobjects = false;
  prevoff = 0;
  nextoff = SIZEOF_AR_FILE_HDR;
  for (sub = abfd->archive_head, i = 0; sub != NULL; sub = sub->next, i++)
    {
      const char *name;
      size_t namlen;
      struct xcoff_ar_hdr *ahdrp;
      bfd_size_type remaining;

      if (makemap && ! hasobjects)
	{
	  if (bfd_check_format (sub, bfd_object))
	    hasobjects = true;
	}

      name = normalize_filename (sub);
      namlen = strlen (name);

      if (sub->arelt_data != NULL)
	ahdrp = arch_xhdr (sub);
      else
	ahdrp = NULL;

      if (ahdrp == NULL)
	{
	  struct stat s;

	  memset (&ahdr, 0, sizeof ahdr);
	  ahdrp = &ahdr;
	  if (stat (bfd_get_filename (sub), &s) != 0)
	    {
	      bfd_set_error (bfd_error_system_call);
	      return false;
	    }

	  sprintf (ahdrp->size, "%ld", (long) s.st_size);
	  sprintf (ahdrp->date, "%ld", (long) s.st_mtime);
	  sprintf (ahdrp->uid, "%ld", (long) s.st_uid);
	  sprintf (ahdrp->gid, "%ld", (long) s.st_gid);
	  sprintf (ahdrp->mode, "%o", (unsigned int) s.st_mode);

	  if (sub->arelt_data == NULL)
	    {
	      sub->arelt_data = bfd_alloc (sub, sizeof (struct areltdata));
	      if (sub->arelt_data == NULL)
		return false;
	    }

	  arch_eltdata (sub)->parsed_size = s.st_size;
	}

      sprintf (ahdrp->prevoff, "%ld", (long) prevoff);
      sprintf (ahdrp->namlen, "%ld", (long) namlen);

      /* If the length of the name is odd, we write out the null byte
         after the name as well.  */
      namlen = (namlen + 1) &~ 1;

      remaining = arelt_size (sub);
      size = (SIZEOF_AR_HDR
	      + namlen
	      + SXCOFFARFMAG
	      + remaining);

      BFD_ASSERT (nextoff == bfd_tell (abfd));

      offsets[i] = nextoff;

      prevoff = nextoff;
      nextoff += size + (size & 1);

      sprintf (ahdrp->nextoff, "%ld", (long) nextoff);

      /* We need spaces, not null bytes, in the header.  */
      for (p = (char *) ahdrp; p < (char *) ahdrp + SIZEOF_AR_HDR; p++)
	if (*p == '\0')
	  *p = ' ';

      if (bfd_write ((PTR) ahdrp, 1, SIZEOF_AR_HDR, abfd) != SIZEOF_AR_HDR
	  || bfd_write ((PTR) name, 1, namlen, abfd) != namlen
	  || (bfd_write ((PTR) XCOFFARFMAG, 1, SXCOFFARFMAG, abfd)
	      != SXCOFFARFMAG))
	return false;

      if (bfd_seek (sub, (file_ptr) 0, SEEK_SET) != 0)
	return false;
      while (remaining != 0)
	{
	  bfd_size_type amt;
	  bfd_byte buffer[DEFAULT_BUFFERSIZE];

	  amt = sizeof buffer;
	  if (amt > remaining)
	    amt = remaining;
	  if (bfd_read (buffer, 1, amt, sub) != amt
	      || bfd_write (buffer, 1, amt, abfd) != amt)
	    return false;
	  remaining -= amt;
	}

      if ((size & 1) != 0)
	{
	  bfd_byte b;

	  b = '\0';
	  if (bfd_write (&b, 1, 1, abfd) != 1)
	    return false;
	}
    }

  sprintf (fhdr.lastmemoff, "%ld", (long) prevoff);

  /* Write out the member table.  */

  BFD_ASSERT (nextoff == bfd_tell (abfd));
  sprintf (fhdr.memoff, "%ld", (long) nextoff);

  memset (&ahdr, 0, sizeof ahdr);
  sprintf (ahdr.size, "%ld", (long) (12 + count * 12 + total_namlen));
  sprintf (ahdr.prevoff, "%ld", (long) prevoff);
  sprintf (ahdr.date, "%d", 0);
  sprintf (ahdr.uid, "%d", 0);
  sprintf (ahdr.gid, "%d", 0);
  sprintf (ahdr.mode, "%d", 0);
  sprintf (ahdr.namlen, "%d", 0);

  size = (SIZEOF_AR_HDR
	  + 12
	  + count * 12
	  + total_namlen
	  + SXCOFFARFMAG);

  prevoff = nextoff;
  nextoff += size + (size & 1);

  if (makemap && hasobjects)
    sprintf (ahdr.nextoff, "%ld", (long) nextoff);
  else
    sprintf (ahdr.nextoff, "%d", 0);

  /* We need spaces, not null bytes, in the header.  */
  for (p = (char *) &ahdr; p < (char *) &ahdr + SIZEOF_AR_HDR; p++)
    if (*p == '\0')
      *p = ' ';

  if (bfd_write ((PTR) &ahdr, 1, SIZEOF_AR_HDR, abfd) != SIZEOF_AR_HDR
      || (bfd_write ((PTR) XCOFFARFMAG, 1, SXCOFFARFMAG, abfd)
	  != SXCOFFARFMAG))
    return false;

  sprintf (decbuf, "%-12ld", (long) count);
  if (bfd_write ((PTR) decbuf, 1, 12, abfd) != 12)
    return false;
  for (i = 0; i < count; i++)
    {
      sprintf (decbuf, "%-12ld", (long) offsets[i]);
      if (bfd_write ((PTR) decbuf, 1, 12, abfd) != 12)
	return false;
    }
  for (sub = abfd->archive_head; sub != NULL; sub = sub->next)
    {
      const char *name;
      size_t namlen;

      name = normalize_filename (sub);
      namlen = strlen (name);
      if (bfd_write ((PTR) name, 1, namlen + 1, abfd) != namlen + 1)
	return false;
    }
  if ((size & 1) != 0)
    {
      bfd_byte b;

      b = '\0';
      if (bfd_write ((PTR) &b, 1, 1, abfd) != 1)
	return false;
    }

  /* Write out the armap, if appropriate.  */

  if (! makemap || ! hasobjects)
    sprintf (fhdr.symoff, "%d", 0);
  else
    {
      BFD_ASSERT (nextoff == bfd_tell (abfd));
      sprintf (fhdr.symoff, "%ld", (long) nextoff);
      bfd_ardata (abfd)->tdata = (PTR) &fhdr;
      if (! _bfd_compute_and_write_armap (abfd, 0))
	return false;
    }

  /* Write out the archive file header.  */

  /* We need spaces, not null bytes, in the header.  */
  for (p = (char *) &fhdr; p < (char *) &fhdr + SIZEOF_AR_FILE_HDR; p++)
    if (*p == '\0')
      *p = ' ';

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0
      || (bfd_write ((PTR) &fhdr, SIZEOF_AR_FILE_HDR, 1, abfd) !=
	  SIZEOF_AR_FILE_HDR))
    return false;

  return true;
}

static boolean
xcoff_write_archive_contents_big (abfd)
     bfd *abfd;
{
  struct xcoff_ar_file_hdr_big fhdr;
  size_t count;
  size_t total_namlen;
  file_ptr *offsets;
  boolean makemap;
  boolean hasobjects;
  file_ptr prevoff, nextoff;
  bfd *sub;
  unsigned int i;
  struct xcoff_ar_hdr_big ahdr;
  bfd_size_type size;
  char *p;
  char decbuf[13];

  memset (&fhdr, 0, sizeof fhdr);
  strncpy (fhdr.magic, XCOFFARMAGBIG, SXCOFFARMAG);
  sprintf (fhdr.firstmemoff, "%d", SIZEOF_AR_FILE_HDR_BIG);
  sprintf (fhdr.freeoff, "%d", 0);

  count = 0;
  total_namlen = 0;
  for (sub = abfd->archive_head; sub != NULL; sub = sub->next)
    {
      ++count;
      total_namlen += strlen (normalize_filename (sub)) + 1;
    }
  offsets = (file_ptr *) bfd_alloc (abfd, count * sizeof (file_ptr));
  if (offsets == NULL)
    return false;

  if (bfd_seek (abfd, SIZEOF_AR_FILE_HDR_BIG, SEEK_SET) != 0)
    return false;

  makemap = bfd_has_map (abfd);
  hasobjects = false;
  prevoff = 0;
  nextoff = SIZEOF_AR_FILE_HDR_BIG;
  for (sub = abfd->archive_head, i = 0; sub != NULL; sub = sub->next, i++)
    {
      const char *name;
      size_t namlen;
      struct xcoff_ar_hdr_big *ahdrp;
      bfd_size_type remaining;

      if (makemap && ! hasobjects)
	{
	  if (bfd_check_format (sub, bfd_object))
	    hasobjects = true;
	}

      name = normalize_filename (sub);
      namlen = strlen (name);

      if (sub->arelt_data != NULL)
	ahdrp = arch_xhdr_big (sub);
      else
	ahdrp = NULL;

      if (ahdrp == NULL)
	{
	  struct stat s;

	  memset (&ahdr, 0, sizeof ahdr);
	  ahdrp = &ahdr;
	  /* XXX This should actually be a call to stat64 (at least on
	     32-bit machines).  */
	  if (stat (bfd_get_filename (sub), &s) != 0)
	    {
	      bfd_set_error (bfd_error_system_call);
	      return false;
	    }

	  /* XXX This call actually should use %lld (at least on 32-bit
	     machines) since the fields's width is 20 and there numbers with
	     more than 32 bits can be represented.  */
	  sprintf (ahdrp->size, "%ld", (long) s.st_size);
	  sprintf (ahdrp->date, "%ld", (long) s.st_mtime);
	  sprintf (ahdrp->uid, "%ld", (long) s.st_uid);
	  sprintf (ahdrp->gid, "%ld", (long) s.st_gid);
	  sprintf (ahdrp->mode, "%o", (unsigned int) s.st_mode);

	  if (sub->arelt_data == NULL)
	    {
	      sub->arelt_data = bfd_alloc (sub, sizeof (struct areltdata));
	      if (sub->arelt_data == NULL)
		return false;
	    }

	  arch_eltdata (sub)->parsed_size = s.st_size;
	}

      /* XXX These calls actually should use %lld (at least on 32-bit
	 machines) since the fields's width is 20 and there numbers with
	 more than 32 bits can be represented.  */
      sprintf (ahdrp->prevoff, "%ld", (long) prevoff);
      sprintf (ahdrp->namlen, "%ld", (long) namlen);

      /* If the length of the name is odd, we write out the null byte
         after the name as well.  */
      namlen = (namlen + 1) &~ 1;

      remaining = arelt_size (sub);
      size = (SIZEOF_AR_HDR_BIG
	      + namlen
	      + SXCOFFARFMAG
	      + remaining);

      BFD_ASSERT (nextoff == bfd_tell (abfd));

      offsets[i] = nextoff;

      prevoff = nextoff;
      nextoff += size + (size & 1);

      sprintf (ahdrp->nextoff, "%ld", (long) nextoff);

      /* We need spaces, not null bytes, in the header.  */
      for (p = (char *) ahdrp; p < (char *) ahdrp + SIZEOF_AR_HDR_BIG; p++)
	if (*p == '\0')
	  *p = ' ';

      if (bfd_write ((PTR) ahdrp, 1, SIZEOF_AR_HDR_BIG, abfd)
	  != SIZEOF_AR_HDR_BIG
	  || bfd_write ((PTR) name, 1, namlen, abfd) != namlen
	  || (bfd_write ((PTR) XCOFFARFMAG, 1, SXCOFFARFMAG, abfd)
	      != SXCOFFARFMAG))
	return false;

      if (bfd_seek (sub, (file_ptr) 0, SEEK_SET) != 0)
	return false;
      while (remaining != 0)
	{
	  bfd_size_type amt;
	  bfd_byte buffer[DEFAULT_BUFFERSIZE];

	  amt = sizeof buffer;
	  if (amt > remaining)
	    amt = remaining;
	  if (bfd_read (buffer, 1, amt, sub) != amt
	      || bfd_write (buffer, 1, amt, abfd) != amt)
	    return false;
	  remaining -= amt;
	}

      if ((size & 1) != 0)
	{
	  bfd_byte b;

	  b = '\0';
	  if (bfd_write (&b, 1, 1, abfd) != 1)
	    return false;
	}
    }

  /* XXX This call actually should use %lld (at least on 32-bit
     machines) since the fields's width is 20 and there numbers with
     more than 32 bits can be represented.  */
  sprintf (fhdr.lastmemoff, "%ld", (long) prevoff);

  /* Write out the member table.  */

  BFD_ASSERT (nextoff == bfd_tell (abfd));
  /* XXX This call actually should use %lld (at least on 32-bit
     machines) since the fields's width is 20 and there numbers with
     more than 32 bits can be represented.  */
  sprintf (fhdr.memoff, "%ld", (long) nextoff);

  memset (&ahdr, 0, sizeof ahdr);
  /* XXX The next two calls actually should use %lld (at least on 32-bit
     machines) since the fields's width is 20 and there numbers with
     more than 32 bits can be represented.  */
  sprintf (ahdr.size, "%ld", (long) (12 + count * 12 + total_namlen));
  sprintf (ahdr.prevoff, "%ld", (long) prevoff);
  sprintf (ahdr.date, "%d", 0);
  sprintf (ahdr.uid, "%d", 0);
  sprintf (ahdr.gid, "%d", 0);
  sprintf (ahdr.mode, "%d", 0);
  sprintf (ahdr.namlen, "%d", 0);

  size = (SIZEOF_AR_HDR_BIG
	  + 12
	  + count * 12
	  + total_namlen
	  + SXCOFFARFMAG);

  prevoff = nextoff;
  nextoff += size + (size & 1);

  if (makemap && hasobjects)
    /* XXX This call actually should use %lld (at least on 32-bit
       machines) since the fields's width is 20 and there numbers with
       more than 32 bits can be represented.  */
    sprintf (ahdr.nextoff, "%ld", (long) nextoff);
  else
    sprintf (ahdr.nextoff, "%d", 0);

  /* We need spaces, not null bytes, in the header.  */
  for (p = (char *) &ahdr; p < (char *) &ahdr + SIZEOF_AR_HDR_BIG; p++)
    if (*p == '\0')
      *p = ' ';

  if (bfd_write ((PTR) &ahdr, 1, SIZEOF_AR_HDR_BIG, abfd) != SIZEOF_AR_HDR_BIG
      || (bfd_write ((PTR) XCOFFARFMAG, 1, SXCOFFARFMAG, abfd)
	  != SXCOFFARFMAG))
    return false;

  sprintf (decbuf, "%-12ld", (long) count);
  if (bfd_write ((PTR) decbuf, 1, 12, abfd) != 12)
    return false;
  for (i = 0; i < count; i++)
    {
      sprintf (decbuf, "%-12ld", (long) offsets[i]);
      if (bfd_write ((PTR) decbuf, 1, 12, abfd) != 12)
	return false;
    }
  for (sub = abfd->archive_head; sub != NULL; sub = sub->next)
    {
      const char *name;
      size_t namlen;

      name = normalize_filename (sub);
      namlen = strlen (name);
      if (bfd_write ((PTR) name, 1, namlen + 1, abfd) != namlen + 1)
	return false;
    }
  if ((size & 1) != 0)
    {
      bfd_byte b;

      b = '\0';
      if (bfd_write ((PTR) &b, 1, 1, abfd) != 1)
	return false;
    }

  /* Write out the armap, if appropriate.  */

  if (! makemap || ! hasobjects)
    sprintf (fhdr.symoff, "%d", 0);
  else
    {
      BFD_ASSERT (nextoff == bfd_tell (abfd));
      /* XXX This call actually should use %lld (at least on 32-bit
	 machines) since the fields's width is 20 and there numbers with
	 more than 32 bits can be represented.  */
      bfd_ardata (abfd)->tdata = (PTR) &fhdr;
      if (! _bfd_compute_and_write_armap (abfd, 0))
	return false;
    }

  /* Write out the archive file header.  */

  /* We need spaces, not null bytes, in the header.  */
  for (p = (char *) &fhdr; p < (char *) &fhdr + SIZEOF_AR_FILE_HDR_BIG; p++)
    if (*p == '\0')
      *p = ' ';

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0
      || (bfd_write ((PTR) &fhdr, SIZEOF_AR_FILE_HDR_BIG, 1, abfd) !=
	  SIZEOF_AR_FILE_HDR_BIG))
    return false;

  return true;
}

boolean
_bfd_xcoff_write_archive_contents (abfd)
     bfd *abfd;
{
  if (! xcoff_big_format_p (abfd))
    return xcoff_write_archive_contents_old (abfd);
  else
    return xcoff_write_archive_contents_big (abfd);
}

/* We can't use the usual coff_sizeof_headers routine, because AIX
   always uses an a.out header.  */

int
_bfd_xcoff_sizeof_headers (abfd, reloc)
     bfd *abfd;
     boolean reloc ATTRIBUTE_UNUSED;
{
  int size;

  size = FILHSZ;
  if (xcoff_data (abfd)->full_aouthdr)
    size += AOUTSZ;
  else
    size += SMALL_AOUTSZ;
  size += abfd->section_count * SCNHSZ;
  return size;
}
