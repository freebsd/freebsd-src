/* Generic COFF swapping routines, for BFD.
   Copyright 1990, 91, 92, 93, 94, 95, 96, 1997 Free Software Foundation, Inc.
   Written by Cygnus Support.

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

/* This file contains routines used to swap COFF data.  It is a header
   file because the details of swapping depend on the details of the
   structures used by each COFF implementation.  This is included by
   coffcode.h, as well as by the ECOFF backend.

   Any file which uses this must first include "coff/internal.h" and
   "coff/CPU.h".  The functions will then be correct for that CPU.  */

#ifndef IMAGE_BASE
#define IMAGE_BASE 0
#endif

#define PUTWORD bfd_h_put_32
#define PUTHALF bfd_h_put_16
#define	PUTBYTE bfd_h_put_8

#ifndef GET_FCN_LNNOPTR
#define GET_FCN_LNNOPTR(abfd, ext)  bfd_h_get_32(abfd, (bfd_byte *) ext->x_sym.x_fcnary.x_fcn.x_lnnoptr)
#endif

#ifndef GET_FCN_ENDNDX
#define GET_FCN_ENDNDX(abfd, ext)  bfd_h_get_32(abfd, (bfd_byte *) ext->x_sym.x_fcnary.x_fcn.x_endndx)
#endif

#ifndef PUT_FCN_LNNOPTR
#define PUT_FCN_LNNOPTR(abfd, in, ext)  PUTWORD(abfd,  in, (bfd_byte *) ext->x_sym.x_fcnary.x_fcn.x_lnnoptr)
#endif
#ifndef PUT_FCN_ENDNDX
#define PUT_FCN_ENDNDX(abfd, in, ext) PUTWORD(abfd, in, (bfd_byte *) ext->x_sym.x_fcnary.x_fcn.x_endndx)
#endif
#ifndef GET_LNSZ_LNNO
#define GET_LNSZ_LNNO(abfd, ext) bfd_h_get_16(abfd, (bfd_byte *) ext->x_sym.x_misc.x_lnsz.x_lnno)
#endif
#ifndef GET_LNSZ_SIZE
#define GET_LNSZ_SIZE(abfd, ext) bfd_h_get_16(abfd, (bfd_byte *) ext->x_sym.x_misc.x_lnsz.x_size)
#endif
#ifndef PUT_LNSZ_LNNO
#define PUT_LNSZ_LNNO(abfd, in, ext) bfd_h_put_16(abfd, in, (bfd_byte *)ext->x_sym.x_misc.x_lnsz.x_lnno)
#endif
#ifndef PUT_LNSZ_SIZE
#define PUT_LNSZ_SIZE(abfd, in, ext) bfd_h_put_16(abfd, in, (bfd_byte*) ext->x_sym.x_misc.x_lnsz.x_size)
#endif
#ifndef GET_SCN_SCNLEN
#define GET_SCN_SCNLEN(abfd,  ext) bfd_h_get_32(abfd, (bfd_byte *) ext->x_scn.x_scnlen)
#endif
#ifndef GET_SCN_NRELOC
#define GET_SCN_NRELOC(abfd,  ext) bfd_h_get_16(abfd, (bfd_byte *)ext->x_scn.x_nreloc)
#endif
#ifndef GET_SCN_NLINNO
#define GET_SCN_NLINNO(abfd, ext)  bfd_h_get_16(abfd, (bfd_byte *)ext->x_scn.x_nlinno)
#endif
#ifndef PUT_SCN_SCNLEN
#define PUT_SCN_SCNLEN(abfd,in, ext) bfd_h_put_32(abfd, in, (bfd_byte *) ext->x_scn.x_scnlen)
#endif
#ifndef PUT_SCN_NRELOC
#define PUT_SCN_NRELOC(abfd,in, ext) bfd_h_put_16(abfd, in, (bfd_byte *)ext->x_scn.x_nreloc)
#endif
#ifndef PUT_SCN_NLINNO
#define PUT_SCN_NLINNO(abfd,in, ext)  bfd_h_put_16(abfd,in, (bfd_byte  *) ext->x_scn.x_nlinno)
#endif
#ifndef GET_LINENO_LNNO
#define GET_LINENO_LNNO(abfd, ext) bfd_h_get_16(abfd, (bfd_byte *) (ext->l_lnno));
#endif
#ifndef PUT_LINENO_LNNO
#define PUT_LINENO_LNNO(abfd,val, ext) bfd_h_put_16(abfd,val,  (bfd_byte *) (ext->l_lnno));
#endif

/* The f_symptr field in the filehdr is sometimes 64 bits.  */
#ifndef GET_FILEHDR_SYMPTR
#define GET_FILEHDR_SYMPTR bfd_h_get_32
#endif
#ifndef PUT_FILEHDR_SYMPTR
#define PUT_FILEHDR_SYMPTR bfd_h_put_32
#endif

/* Some fields in the aouthdr are sometimes 64 bits.  */
#ifndef GET_AOUTHDR_TSIZE
#define GET_AOUTHDR_TSIZE bfd_h_get_32
#endif
#ifndef PUT_AOUTHDR_TSIZE
#define PUT_AOUTHDR_TSIZE bfd_h_put_32
#endif
#ifndef GET_AOUTHDR_DSIZE
#define GET_AOUTHDR_DSIZE bfd_h_get_32
#endif
#ifndef PUT_AOUTHDR_DSIZE
#define PUT_AOUTHDR_DSIZE bfd_h_put_32
#endif
#ifndef GET_AOUTHDR_BSIZE
#define GET_AOUTHDR_BSIZE bfd_h_get_32
#endif
#ifndef PUT_AOUTHDR_BSIZE
#define PUT_AOUTHDR_BSIZE bfd_h_put_32
#endif
#ifndef GET_AOUTHDR_ENTRY
#define GET_AOUTHDR_ENTRY bfd_h_get_32
#endif
#ifndef PUT_AOUTHDR_ENTRY
#define PUT_AOUTHDR_ENTRY bfd_h_put_32
#endif
#ifndef GET_AOUTHDR_TEXT_START
#define GET_AOUTHDR_TEXT_START bfd_h_get_32
#endif
#ifndef PUT_AOUTHDR_TEXT_START
#define PUT_AOUTHDR_TEXT_START bfd_h_put_32
#endif
#ifndef GET_AOUTHDR_DATA_START
#define GET_AOUTHDR_DATA_START bfd_h_get_32
#endif
#ifndef PUT_AOUTHDR_DATA_START
#define PUT_AOUTHDR_DATA_START bfd_h_put_32
#endif

/* Some fields in the scnhdr are sometimes 64 bits.  */
#ifndef GET_SCNHDR_PADDR
#define GET_SCNHDR_PADDR bfd_h_get_32
#endif
#ifndef PUT_SCNHDR_PADDR
#define PUT_SCNHDR_PADDR bfd_h_put_32
#endif
#ifndef GET_SCNHDR_VADDR
#define GET_SCNHDR_VADDR bfd_h_get_32
#endif
#ifndef PUT_SCNHDR_VADDR
#define PUT_SCNHDR_VADDR bfd_h_put_32
#endif
#ifndef GET_SCNHDR_SIZE
#define GET_SCNHDR_SIZE bfd_h_get_32
#endif
#ifndef PUT_SCNHDR_SIZE
#define PUT_SCNHDR_SIZE bfd_h_put_32
#endif
#ifndef GET_SCNHDR_SCNPTR
#define GET_SCNHDR_SCNPTR bfd_h_get_32
#endif
#ifndef PUT_SCNHDR_SCNPTR
#define PUT_SCNHDR_SCNPTR bfd_h_put_32
#endif
#ifndef GET_SCNHDR_RELPTR
#define GET_SCNHDR_RELPTR bfd_h_get_32
#endif
#ifndef PUT_SCNHDR_RELPTR
#define PUT_SCNHDR_RELPTR bfd_h_put_32
#endif
#ifndef GET_SCNHDR_LNNOPTR
#define GET_SCNHDR_LNNOPTR bfd_h_get_32
#endif
#ifndef PUT_SCNHDR_LNNOPTR
#define PUT_SCNHDR_LNNOPTR bfd_h_put_32
#endif
#ifndef GET_SCNHDR_NRELOC
#define GET_SCNHDR_NRELOC bfd_h_get_16
#endif
#ifndef PUT_SCNHDR_NRELOC
#define PUT_SCNHDR_NRELOC bfd_h_put_16
#endif
#ifndef GET_SCNHDR_NLNNO
#define GET_SCNHDR_NLNNO bfd_h_get_16
#endif
#ifndef PUT_SCNHDR_NLNNO
#define PUT_SCNHDR_NLNNO bfd_h_put_16
#endif
#ifndef GET_SCNHDR_FLAGS
#define GET_SCNHDR_FLAGS bfd_h_get_32
#endif
#ifndef PUT_SCNHDR_FLAGS
#define PUT_SCNHDR_FLAGS bfd_h_put_32
#endif


static void coff_swap_aouthdr_in PARAMS ((bfd *, PTR, PTR));
static unsigned int coff_swap_aouthdr_out PARAMS ((bfd *, PTR, PTR));
static void coff_swap_scnhdr_in PARAMS ((bfd *, PTR, PTR));
static unsigned int coff_swap_scnhdr_out PARAMS ((bfd *, PTR, PTR));
static void coff_swap_filehdr_in PARAMS ((bfd *, PTR, PTR));
static unsigned int coff_swap_filehdr_out PARAMS ((bfd *, PTR, PTR));
#ifndef NO_COFF_RELOCS
static void coff_swap_reloc_in PARAMS ((bfd *, PTR, PTR));
static unsigned int coff_swap_reloc_out PARAMS ((bfd *, PTR, PTR));
#endif /* NO_COFF_RELOCS */
#ifndef NO_COFF_SYMBOLS
static void coff_swap_sym_in PARAMS ((bfd *, PTR, PTR));
static unsigned int coff_swap_sym_out PARAMS ((bfd *, PTR, PTR));
static void coff_swap_aux_in PARAMS ((bfd *, PTR, int, int, int, int, PTR));
static unsigned int coff_swap_aux_out PARAMS ((bfd *, PTR, int, int, int, int, PTR));
#endif /* NO_COFF_SYMBOLS */
#ifndef NO_COFF_LINENOS
static void coff_swap_lineno_in PARAMS ((bfd *, PTR, PTR));
static unsigned int coff_swap_lineno_out PARAMS ((bfd *, PTR, PTR));
#endif /* NO_COFF_LINENOS */

#ifndef NO_COFF_RELOCS

static void
coff_swap_reloc_in (abfd, src, dst)
     bfd *abfd;
     PTR src;
     PTR dst;
{
  RELOC *reloc_src = (RELOC *) src;
  struct internal_reloc *reloc_dst = (struct internal_reloc *) dst;

  reloc_dst->r_vaddr = bfd_h_get_32(abfd, (bfd_byte *)reloc_src->r_vaddr);
  reloc_dst->r_symndx = bfd_h_get_signed_32(abfd, (bfd_byte *) reloc_src->r_symndx);

#ifdef RS6000COFF_C
  reloc_dst->r_type = bfd_h_get_8(abfd, reloc_src->r_type);
  reloc_dst->r_size = bfd_h_get_8(abfd, reloc_src->r_size);
#else
  reloc_dst->r_type = bfd_h_get_16(abfd, (bfd_byte *) reloc_src->r_type);
#endif

#ifdef SWAP_IN_RELOC_OFFSET
  reloc_dst->r_offset = SWAP_IN_RELOC_OFFSET(abfd,
					     (bfd_byte *) reloc_src->r_offset);
#endif
}

static unsigned int
coff_swap_reloc_out (abfd, src, dst)
     bfd       *abfd;
     PTR	src;
     PTR	dst;
{
  struct internal_reloc *reloc_src = (struct internal_reloc *)src;
  struct external_reloc *reloc_dst = (struct external_reloc *)dst;
  bfd_h_put_32(abfd, reloc_src->r_vaddr, (bfd_byte *) reloc_dst->r_vaddr);
  bfd_h_put_32(abfd, reloc_src->r_symndx, (bfd_byte *) reloc_dst->r_symndx);

#ifdef RS6000COFF_C
  bfd_h_put_8 (abfd, reloc_src->r_type, (bfd_byte *) reloc_dst->r_type);
  bfd_h_put_8 (abfd, reloc_src->r_size, (bfd_byte *) reloc_dst->r_size);
#else
  bfd_h_put_16(abfd, reloc_src->r_type, (bfd_byte *)
	       reloc_dst->r_type);
#endif

#ifdef SWAP_OUT_RELOC_OFFSET
  SWAP_OUT_RELOC_OFFSET(abfd,
			reloc_src->r_offset,
			(bfd_byte *) reloc_dst->r_offset);
#endif
#ifdef SWAP_OUT_RELOC_EXTRA
  SWAP_OUT_RELOC_EXTRA(abfd,reloc_src, reloc_dst);
#endif

  return RELSZ;
}

#endif /* NO_COFF_RELOCS */

static void
coff_swap_filehdr_in (abfd, src, dst)
     bfd            *abfd;
     PTR	     src;
     PTR	     dst;
{
  FILHDR *filehdr_src = (FILHDR *) src;
  struct internal_filehdr *filehdr_dst = (struct internal_filehdr *) dst;
#ifdef COFF_ADJUST_FILEHDR_IN_PRE
  COFF_ADJUST_FILEHDR_IN_PRE (abfd, src, dst);
#endif
  filehdr_dst->f_magic = bfd_h_get_16(abfd, (bfd_byte *) filehdr_src->f_magic);
  filehdr_dst->f_nscns = bfd_h_get_16(abfd, (bfd_byte *)filehdr_src-> f_nscns);
  filehdr_dst->f_timdat = bfd_h_get_32(abfd, (bfd_byte *)filehdr_src-> f_timdat);
  filehdr_dst->f_symptr =
    GET_FILEHDR_SYMPTR (abfd, (bfd_byte *) filehdr_src->f_symptr);
  filehdr_dst->f_nsyms = bfd_h_get_32(abfd, (bfd_byte *)filehdr_src-> f_nsyms);
  filehdr_dst->f_opthdr = bfd_h_get_16(abfd, (bfd_byte *)filehdr_src-> f_opthdr);
  filehdr_dst->f_flags = bfd_h_get_16(abfd, (bfd_byte *)filehdr_src-> f_flags);

#ifdef COFF_ADJUST_FILEHDR_IN_POST
  COFF_ADJUST_FILEHDR_IN_POST (abfd, src, dst);
#endif
}

static  unsigned int
coff_swap_filehdr_out (abfd, in, out)
     bfd       *abfd;
     PTR	in;
     PTR	out;
{
  struct internal_filehdr *filehdr_in = (struct internal_filehdr *)in;
  FILHDR *filehdr_out = (FILHDR *)out;

#ifdef COFF_ADJUST_FILEHDR_OUT_PRE
  COFF_ADJUST_FILEHDR_OUT_PRE (abfd, in, out);
#endif
  bfd_h_put_16(abfd, filehdr_in->f_magic, (bfd_byte *) filehdr_out->f_magic);
  bfd_h_put_16(abfd, filehdr_in->f_nscns, (bfd_byte *) filehdr_out->f_nscns);
  bfd_h_put_32(abfd, filehdr_in->f_timdat, (bfd_byte *) filehdr_out->f_timdat);
  PUT_FILEHDR_SYMPTR (abfd, (bfd_vma) filehdr_in->f_symptr,
		      (bfd_byte *) filehdr_out->f_symptr);
  bfd_h_put_32(abfd, filehdr_in->f_nsyms, (bfd_byte *) filehdr_out->f_nsyms);
  bfd_h_put_16(abfd, filehdr_in->f_opthdr, (bfd_byte *) filehdr_out->f_opthdr);
  bfd_h_put_16(abfd, filehdr_in->f_flags, (bfd_byte *) filehdr_out->f_flags);

#ifdef COFF_ADJUST_FILEHDR_OUT_POST
  COFF_ADJUST_FILEHDR_OUT_POST (abfd, in, out);
#endif
  return FILHSZ;
}


#ifndef NO_COFF_SYMBOLS

static void
coff_swap_sym_in (abfd, ext1, in1)
     bfd            *abfd;
     PTR ext1;
     PTR in1;
{
  SYMENT *ext = (SYMENT *)ext1;
  struct internal_syment      *in = (struct internal_syment *)in1;

  if( ext->e.e_name[0] == 0) {
    in->_n._n_n._n_zeroes = 0;
    in->_n._n_n._n_offset = bfd_h_get_32(abfd, (bfd_byte *) ext->e.e.e_offset);
  }
  else {
#if SYMNMLEN != E_SYMNMLEN
   -> Error, we need to cope with truncating or extending SYMNMLEN!;
#else
    memcpy(in->_n._n_name, ext->e.e_name, SYMNMLEN);
#endif
  }
  in->n_value = bfd_h_get_32(abfd, (bfd_byte *) ext->e_value); 
  in->n_scnum = bfd_h_get_16(abfd, (bfd_byte *) ext->e_scnum);
  if (sizeof(ext->e_type) == 2){
    in->n_type = bfd_h_get_16(abfd, (bfd_byte *) ext->e_type);
  }
  else {
    in->n_type = bfd_h_get_32(abfd, (bfd_byte *) ext->e_type);
  }
  in->n_sclass = bfd_h_get_8(abfd, ext->e_sclass);
  in->n_numaux = bfd_h_get_8(abfd, ext->e_numaux);
}

static unsigned int
coff_swap_sym_out (abfd, inp, extp)
     bfd       *abfd;
     PTR	inp;
     PTR	extp;
{
  struct internal_syment *in = (struct internal_syment *)inp;
  SYMENT *ext =(SYMENT *)extp;
  if(in->_n._n_name[0] == 0) {
    bfd_h_put_32(abfd, 0, (bfd_byte *) ext->e.e.e_zeroes);
    bfd_h_put_32(abfd, in->_n._n_n._n_offset, (bfd_byte *)  ext->e.e.e_offset);
  }
  else {
#if SYMNMLEN != E_SYMNMLEN
    -> Error, we need to cope with truncating or extending SYMNMLEN!;
#else
    memcpy(ext->e.e_name, in->_n._n_name, SYMNMLEN);
#endif
  }
  bfd_h_put_32(abfd,  in->n_value , (bfd_byte *) ext->e_value);
  bfd_h_put_16(abfd,  in->n_scnum , (bfd_byte *) ext->e_scnum);
  if (sizeof(ext->e_type) == 2)
      {
	bfd_h_put_16(abfd,  in->n_type , (bfd_byte *) ext->e_type);
      }
  else
      {
	bfd_h_put_32(abfd,  in->n_type , (bfd_byte *) ext->e_type);
      }
  bfd_h_put_8(abfd,  in->n_sclass , ext->e_sclass);
  bfd_h_put_8(abfd,  in->n_numaux , ext->e_numaux);
  return SYMESZ;
}

static void
coff_swap_aux_in (abfd, ext1, type, class, indx, numaux, in1)
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

#ifdef COFF_ADJUST_AUX_IN_PRE
  COFF_ADJUST_AUX_IN_PRE (abfd, ext1, type, class, indx, numaux, in1);
#endif
  switch (class) {
    case C_FILE:
      if (ext->x_file.x_fname[0] == 0) {
	  in->x_file.x_n.x_zeroes = 0;
	  in->x_file.x_n.x_offset = 
	   bfd_h_get_32(abfd, (bfd_byte *) ext->x_file.x_n.x_offset);
	} else {
#if FILNMLEN != E_FILNMLEN
	    -> Error, we need to cope with truncating or extending FILNMLEN!;
#else
	    memcpy (in->x_file.x_fname, ext->x_file.x_fname, FILNMLEN);
#endif
	  }
      goto end;

      /* RS/6000 "csect" auxents */
#ifdef RS6000COFF_C
    case C_EXT:
    case C_HIDEXT:
      if (indx + 1 == numaux)
	{
	  in->x_csect.x_scnlen.l = bfd_h_get_32 (abfd, ext->x_csect.x_scnlen);
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
#endif

    case C_STAT:
#ifdef C_LEAFSTAT
    case C_LEAFSTAT:
#endif
    case C_HIDDEN:
      if (type == T_NULL) {
	  in->x_scn.x_scnlen = GET_SCN_SCNLEN(abfd, ext);
	  in->x_scn.x_nreloc = GET_SCN_NRELOC(abfd, ext);
	  in->x_scn.x_nlinno = GET_SCN_NLINNO(abfd, ext);

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
#ifndef NO_TVNDX
  in->x_sym.x_tvndx = bfd_h_get_16(abfd, (bfd_byte *) ext->x_sym.x_tvndx);
#endif

  if (class == C_BLOCK || class == C_FCN || ISFCN (type) || ISTAG (class))
    {
      in->x_sym.x_fcnary.x_fcn.x_lnnoptr = GET_FCN_LNNOPTR (abfd, ext);
      in->x_sym.x_fcnary.x_fcn.x_endndx.l = GET_FCN_ENDNDX (abfd, ext);
    }
  else
    {
#if DIMNUM != E_DIMNUM
 #error we need to cope with truncating or extending DIMNUM
#endif
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
    in->x_sym.x_misc.x_lnsz.x_lnno = GET_LNSZ_LNNO(abfd, ext);
    in->x_sym.x_misc.x_lnsz.x_size = GET_LNSZ_SIZE(abfd, ext);
  }

end: ;
  /* the semicolon is because MSVC doesn't like labels at
     end of block. */

#ifdef COFF_ADJUST_AUX_IN_POST
  COFF_ADJUST_AUX_IN_POST (abfd, ext1, type, class, indx, numaux, in1);
#endif
}

static unsigned int
coff_swap_aux_out (abfd, inp, type, class, indx, numaux, extp)
     bfd   *abfd;
     PTR 	inp;
     int   type;
     int   class;
     int   indx;
     int   numaux;
     PTR	extp;
{
  union internal_auxent *in = (union internal_auxent *)inp;
  AUXENT *ext = (AUXENT *)extp;

#ifdef COFF_ADJUST_AUX_OUT_PRE
  COFF_ADJUST_AUX_OUT_PRE (abfd, inp, type, class, indx, numaux, extp);
#endif
  memset((PTR)ext, 0, AUXESZ);
  switch (class) {
  case C_FILE:
    if (in->x_file.x_fname[0] == 0) {
      PUTWORD(abfd, 0, (bfd_byte *) ext->x_file.x_n.x_zeroes);
      PUTWORD(abfd,
	      in->x_file.x_n.x_offset,
	      (bfd_byte *) ext->x_file.x_n.x_offset);
    }
    else {
#if FILNMLEN != E_FILNMLEN
      -> Error, we need to cope with truncating or extending FILNMLEN!;
#else
      memcpy (ext->x_file.x_fname, in->x_file.x_fname, FILNMLEN);
#endif
    }
    goto end;

#ifdef RS6000COFF_C
  /* RS/6000 "csect" auxents */
  case C_EXT:
  case C_HIDEXT:
    if (indx + 1 == numaux)
      {
	PUTWORD (abfd, in->x_csect.x_scnlen.l,	ext->x_csect.x_scnlen);
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
#endif

  case C_STAT:
#ifdef C_LEAFSTAT
  case C_LEAFSTAT:
#endif
  case C_HIDDEN:
    if (type == T_NULL) {
      PUT_SCN_SCNLEN(abfd, in->x_scn.x_scnlen, ext);
      PUT_SCN_NRELOC(abfd, in->x_scn.x_nreloc, ext);
      PUT_SCN_NLINNO(abfd, in->x_scn.x_nlinno, ext);
      goto end;
    }
    break;
  }

  PUTWORD(abfd, in->x_sym.x_tagndx.l, (bfd_byte *) ext->x_sym.x_tagndx);
#ifndef NO_TVNDX
  bfd_h_put_16(abfd, in->x_sym.x_tvndx , (bfd_byte *) ext->x_sym.x_tvndx);
#endif

  if (class == C_BLOCK || class == C_FCN || ISFCN (type) || ISTAG (class))
    {
      PUT_FCN_LNNOPTR(abfd,  in->x_sym.x_fcnary.x_fcn.x_lnnoptr, ext);
      PUT_FCN_ENDNDX(abfd,  in->x_sym.x_fcnary.x_fcn.x_endndx.l, ext);
    }
  else
    {
#if DIMNUM != E_DIMNUM
 #error we need to cope with truncating or extending DIMNUM
#endif
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
      PUT_LNSZ_LNNO (abfd, in->x_sym.x_misc.x_lnsz.x_lnno, ext);
      PUT_LNSZ_SIZE (abfd, in->x_sym.x_misc.x_lnsz.x_size, ext);
    }

end:
#ifdef COFF_ADJUST_AUX_OUT_POST
  COFF_ADJUST_AUX_OUT_POST (abfd, inp, type, class, indx, numaux, extp);
#endif
  return AUXESZ;
}

#endif /* NO_COFF_SYMBOLS */

#ifndef NO_COFF_LINENOS

static void
coff_swap_lineno_in (abfd, ext1, in1)
     bfd            *abfd;
     PTR ext1;
     PTR in1;
{
  LINENO *ext = (LINENO *)ext1;
  struct internal_lineno      *in = (struct internal_lineno *)in1;

  in->l_addr.l_symndx = bfd_h_get_32(abfd, (bfd_byte *) ext->l_addr.l_symndx);
  in->l_lnno = GET_LINENO_LNNO(abfd, ext);
}

static unsigned int
coff_swap_lineno_out (abfd, inp, outp)
     bfd       *abfd;
     PTR	inp;
     PTR	outp;
{
  struct internal_lineno *in = (struct internal_lineno *)inp;
  struct external_lineno *ext = (struct external_lineno *)outp;
  PUTWORD(abfd, in->l_addr.l_symndx, (bfd_byte *)
	  ext->l_addr.l_symndx);

  PUT_LINENO_LNNO (abfd, in->l_lnno, ext);
  return LINESZ;
}

#endif /* NO_COFF_LINENOS */

static void
coff_swap_aouthdr_in (abfd, aouthdr_ext1, aouthdr_int1)
     bfd            *abfd;
     PTR aouthdr_ext1;
     PTR aouthdr_int1;
{
  AOUTHDR        *aouthdr_ext = (AOUTHDR *) aouthdr_ext1;
  struct internal_aouthdr *aouthdr_int = (struct internal_aouthdr *)aouthdr_int1;

  aouthdr_int->magic = bfd_h_get_16(abfd, (bfd_byte *) aouthdr_ext->magic);
  aouthdr_int->vstamp = bfd_h_get_16(abfd, (bfd_byte *) aouthdr_ext->vstamp);
  aouthdr_int->tsize =
    GET_AOUTHDR_TSIZE (abfd, (bfd_byte *) aouthdr_ext->tsize);
  aouthdr_int->dsize =
    GET_AOUTHDR_DSIZE (abfd, (bfd_byte *) aouthdr_ext->dsize);
  aouthdr_int->bsize =
    GET_AOUTHDR_BSIZE (abfd, (bfd_byte *) aouthdr_ext->bsize);
  aouthdr_int->entry =
    GET_AOUTHDR_ENTRY (abfd, (bfd_byte *) aouthdr_ext->entry);
  aouthdr_int->text_start =
    GET_AOUTHDR_TEXT_START (abfd, (bfd_byte *) aouthdr_ext->text_start);
  aouthdr_int->data_start =
    GET_AOUTHDR_DATA_START (abfd, (bfd_byte *) aouthdr_ext->data_start);

#ifdef I960
  aouthdr_int->tagentries = bfd_h_get_32(abfd, (bfd_byte *) aouthdr_ext->tagentries);
#endif

#ifdef APOLLO_M68
  bfd_h_put_32(abfd, aouthdr_int->o_inlib, (bfd_byte *) aouthdr_ext->o_inlib);
  bfd_h_put_32(abfd, aouthdr_int->o_sri, (bfd_byte *) aouthdr_ext->o_sri);
  bfd_h_put_32(abfd, aouthdr_int->vid[0], (bfd_byte *) aouthdr_ext->vid);
  bfd_h_put_32(abfd, aouthdr_int->vid[1], (bfd_byte *) aouthdr_ext->vid + 4);
#endif


#ifdef RS6000COFF_C
  aouthdr_int->o_toc = bfd_h_get_32(abfd, aouthdr_ext->o_toc);
  aouthdr_int->o_snentry = bfd_h_get_16(abfd, aouthdr_ext->o_snentry);
  aouthdr_int->o_sntext = bfd_h_get_16(abfd, aouthdr_ext->o_sntext);
  aouthdr_int->o_sndata = bfd_h_get_16(abfd, aouthdr_ext->o_sndata);
  aouthdr_int->o_sntoc = bfd_h_get_16(abfd, aouthdr_ext->o_sntoc);
  aouthdr_int->o_snloader = bfd_h_get_16(abfd, aouthdr_ext->o_snloader);
  aouthdr_int->o_snbss = bfd_h_get_16(abfd, aouthdr_ext->o_snbss);
  aouthdr_int->o_algntext = bfd_h_get_16(abfd, aouthdr_ext->o_algntext);
  aouthdr_int->o_algndata = bfd_h_get_16(abfd, aouthdr_ext->o_algndata);
  aouthdr_int->o_modtype = bfd_h_get_16(abfd, aouthdr_ext->o_modtype);
  aouthdr_int->o_cputype = bfd_h_get_16(abfd, aouthdr_ext->o_cputype);
  aouthdr_int->o_maxstack = bfd_h_get_32(abfd, aouthdr_ext->o_maxstack);
  aouthdr_int->o_maxdata = bfd_h_get_32(abfd, aouthdr_ext->o_maxdata);
#endif

#ifdef MIPSECOFF
  aouthdr_int->bss_start = bfd_h_get_32(abfd, aouthdr_ext->bss_start);
  aouthdr_int->gp_value = bfd_h_get_32(abfd, aouthdr_ext->gp_value);
  aouthdr_int->gprmask = bfd_h_get_32(abfd, aouthdr_ext->gprmask);
  aouthdr_int->cprmask[0] = bfd_h_get_32(abfd, aouthdr_ext->cprmask[0]);
  aouthdr_int->cprmask[1] = bfd_h_get_32(abfd, aouthdr_ext->cprmask[1]);
  aouthdr_int->cprmask[2] = bfd_h_get_32(abfd, aouthdr_ext->cprmask[2]);
  aouthdr_int->cprmask[3] = bfd_h_get_32(abfd, aouthdr_ext->cprmask[3]);
#endif

#ifdef ALPHAECOFF
  aouthdr_int->bss_start = bfd_h_get_64(abfd, aouthdr_ext->bss_start);
  aouthdr_int->gp_value = bfd_h_get_64(abfd, aouthdr_ext->gp_value);
  aouthdr_int->gprmask = bfd_h_get_32(abfd, aouthdr_ext->gprmask);
  aouthdr_int->fprmask = bfd_h_get_32(abfd, aouthdr_ext->fprmask);
#endif
}

static unsigned int
coff_swap_aouthdr_out (abfd, in, out)
     bfd       *abfd;
     PTR	in;
     PTR	out;
{
  struct internal_aouthdr *aouthdr_in = (struct internal_aouthdr *)in;
  AOUTHDR *aouthdr_out = (AOUTHDR *)out;

  bfd_h_put_16(abfd, aouthdr_in->magic, (bfd_byte *) aouthdr_out->magic);
  bfd_h_put_16(abfd, aouthdr_in->vstamp, (bfd_byte *) aouthdr_out->vstamp);
  PUT_AOUTHDR_TSIZE (abfd, aouthdr_in->tsize, (bfd_byte *) aouthdr_out->tsize);
  PUT_AOUTHDR_DSIZE (abfd, aouthdr_in->dsize, (bfd_byte *) aouthdr_out->dsize);
  PUT_AOUTHDR_BSIZE (abfd, aouthdr_in->bsize, (bfd_byte *) aouthdr_out->bsize);
  PUT_AOUTHDR_ENTRY (abfd, aouthdr_in->entry, (bfd_byte *) aouthdr_out->entry);
  PUT_AOUTHDR_TEXT_START (abfd, aouthdr_in->text_start,
			  (bfd_byte *) aouthdr_out->text_start);
  PUT_AOUTHDR_DATA_START (abfd, aouthdr_in->data_start,
			  (bfd_byte *) aouthdr_out->data_start);

#ifdef I960
  bfd_h_put_32(abfd, aouthdr_in->tagentries, (bfd_byte *) aouthdr_out->tagentries);
#endif

#ifdef RS6000COFF_C
  bfd_h_put_32 (abfd, aouthdr_in->o_toc, aouthdr_out->o_toc);
  bfd_h_put_16 (abfd, aouthdr_in->o_snentry, aouthdr_out->o_snentry);
  bfd_h_put_16 (abfd, aouthdr_in->o_sntext, aouthdr_out->o_sntext);
  bfd_h_put_16 (abfd, aouthdr_in->o_sndata, aouthdr_out->o_sndata);
  bfd_h_put_16 (abfd, aouthdr_in->o_sntoc, aouthdr_out->o_sntoc);
  bfd_h_put_16 (abfd, aouthdr_in->o_snloader, aouthdr_out->o_snloader);
  bfd_h_put_16 (abfd, aouthdr_in->o_snbss, aouthdr_out->o_snbss);
  bfd_h_put_16 (abfd, aouthdr_in->o_algntext, aouthdr_out->o_algntext);
  bfd_h_put_16 (abfd, aouthdr_in->o_algndata, aouthdr_out->o_algndata);
  bfd_h_put_16 (abfd, aouthdr_in->o_modtype, aouthdr_out->o_modtype);
  bfd_h_put_16 (abfd, aouthdr_in->o_cputype, aouthdr_out->o_cputype);
  bfd_h_put_32 (abfd, aouthdr_in->o_maxstack, aouthdr_out->o_maxstack);
  bfd_h_put_32 (abfd, aouthdr_in->o_maxdata, aouthdr_out->o_maxdata);
  memset (aouthdr_out->o_resv2, 0, sizeof aouthdr_out->o_resv2);
#endif

#ifdef MIPSECOFF
  bfd_h_put_32(abfd, aouthdr_in->bss_start, (bfd_byte *) aouthdr_out->bss_start);
  bfd_h_put_32(abfd, aouthdr_in->gp_value, (bfd_byte *) aouthdr_out->gp_value);
  bfd_h_put_32(abfd, aouthdr_in->gprmask, (bfd_byte *) aouthdr_out->gprmask);
  bfd_h_put_32(abfd, aouthdr_in->cprmask[0], (bfd_byte *) aouthdr_out->cprmask[0]);
  bfd_h_put_32(abfd, aouthdr_in->cprmask[1], (bfd_byte *) aouthdr_out->cprmask[1]);
  bfd_h_put_32(abfd, aouthdr_in->cprmask[2], (bfd_byte *) aouthdr_out->cprmask[2]);
  bfd_h_put_32(abfd, aouthdr_in->cprmask[3], (bfd_byte *) aouthdr_out->cprmask[3]);
#endif

#ifdef ALPHAECOFF
  /* FIXME: What does bldrev mean?  */
  bfd_h_put_16(abfd, (bfd_vma) 2, (bfd_byte *) aouthdr_out->bldrev);
  bfd_h_put_16(abfd, (bfd_vma) 0, (bfd_byte *) aouthdr_out->padding);
  bfd_h_put_64(abfd, aouthdr_in->bss_start, (bfd_byte *) aouthdr_out->bss_start);
  bfd_h_put_64(abfd, aouthdr_in->gp_value, (bfd_byte *) aouthdr_out->gp_value);
  bfd_h_put_32(abfd, aouthdr_in->gprmask, (bfd_byte *) aouthdr_out->gprmask);
  bfd_h_put_32(abfd, aouthdr_in->fprmask, (bfd_byte *) aouthdr_out->fprmask);
#endif

  return AOUTSZ;
}

static void
coff_swap_scnhdr_in (abfd, ext, in)
     bfd            *abfd;
     PTR	     ext;
     PTR	     in;
{
  SCNHDR *scnhdr_ext = (SCNHDR *) ext;
  struct internal_scnhdr *scnhdr_int = (struct internal_scnhdr *) in;

#ifdef COFF_ADJUST_SCNHDR_IN_PRE
  COFF_ADJUST_SCNHDR_IN_PRE (abfd, ext, in);
#endif
  memcpy(scnhdr_int->s_name, scnhdr_ext->s_name, sizeof(scnhdr_int->s_name));
  scnhdr_int->s_vaddr =
    GET_SCNHDR_VADDR (abfd, (bfd_byte *) scnhdr_ext->s_vaddr);
  scnhdr_int->s_paddr =
    GET_SCNHDR_PADDR (abfd, (bfd_byte *) scnhdr_ext->s_paddr);
  scnhdr_int->s_size =
    GET_SCNHDR_SIZE (abfd, (bfd_byte *) scnhdr_ext->s_size);

  scnhdr_int->s_scnptr =
    GET_SCNHDR_SCNPTR (abfd, (bfd_byte *) scnhdr_ext->s_scnptr);
  scnhdr_int->s_relptr =
    GET_SCNHDR_RELPTR (abfd, (bfd_byte *) scnhdr_ext->s_relptr);
  scnhdr_int->s_lnnoptr =
    GET_SCNHDR_LNNOPTR (abfd, (bfd_byte *) scnhdr_ext->s_lnnoptr);
  scnhdr_int->s_flags =
    GET_SCNHDR_FLAGS (abfd, (bfd_byte *) scnhdr_ext->s_flags);
  scnhdr_int->s_nreloc =
    GET_SCNHDR_NRELOC (abfd, (bfd_byte *) scnhdr_ext->s_nreloc);
  scnhdr_int->s_nlnno =
    GET_SCNHDR_NLNNO (abfd, (bfd_byte *) scnhdr_ext->s_nlnno);
#ifdef I960
  scnhdr_int->s_align =
    GET_SCNHDR_ALIGN (abfd, (bfd_byte *) scnhdr_ext->s_align);
#endif
#ifdef COFF_ADJUST_SCNHDR_IN_POST
  COFF_ADJUST_SCNHDR_IN_POST (abfd, ext, in);
#endif
}

static unsigned int
coff_swap_scnhdr_out (abfd, in, out)
     bfd       *abfd;
     PTR	in;
     PTR	out;
{
  struct internal_scnhdr *scnhdr_int = (struct internal_scnhdr *)in;
  SCNHDR *scnhdr_ext = (SCNHDR *)out;
  unsigned int ret = SCNHSZ;

#ifdef COFF_ADJUST_SCNHDR_OUT_PRE
  COFF_ADJUST_SCNHDR_OUT_PRE (abfd, in, out);
#endif
  memcpy(scnhdr_ext->s_name, scnhdr_int->s_name, sizeof(scnhdr_int->s_name));

  PUT_SCNHDR_VADDR (abfd, scnhdr_int->s_vaddr,
		    (bfd_byte *) scnhdr_ext->s_vaddr);


  PUT_SCNHDR_PADDR (abfd, scnhdr_int->s_paddr,
		    (bfd_byte *) scnhdr_ext->s_paddr);
  PUT_SCNHDR_SIZE (abfd, scnhdr_int->s_size,
		   (bfd_byte *) scnhdr_ext->s_size);

  PUT_SCNHDR_SCNPTR (abfd, scnhdr_int->s_scnptr,
		     (bfd_byte *) scnhdr_ext->s_scnptr);
  PUT_SCNHDR_RELPTR (abfd, scnhdr_int->s_relptr,
		     (bfd_byte *) scnhdr_ext->s_relptr);
  PUT_SCNHDR_LNNOPTR (abfd, scnhdr_int->s_lnnoptr,
		      (bfd_byte *) scnhdr_ext->s_lnnoptr);
  PUT_SCNHDR_FLAGS (abfd, scnhdr_int->s_flags,
		    (bfd_byte *) scnhdr_ext->s_flags);
#if defined(M88)
  PUTWORD(abfd, scnhdr_int->s_nlnno, (bfd_byte *) scnhdr_ext->s_nlnno);
  PUTWORD(abfd, scnhdr_int->s_nreloc, (bfd_byte *) scnhdr_ext->s_nreloc);
#else
  if (scnhdr_int->s_nlnno <= 0xffff)
    PUTHALF(abfd, scnhdr_int->s_nlnno, (bfd_byte *) scnhdr_ext->s_nlnno);
  else
    {
      char buf[sizeof (scnhdr_int->s_name) + 1];

      memcpy (buf, scnhdr_int->s_name, sizeof (scnhdr_int->s_name));
      buf[sizeof (scnhdr_int->s_name)] = '\0';
      (*_bfd_error_handler)
	("%s: warning: %s: line number overflow: 0x%lx > 0xffff",
	 bfd_get_filename (abfd),
	 buf, scnhdr_int->s_nlnno);
      PUTHALF (abfd, 0xffff, (bfd_byte *) scnhdr_ext->s_nlnno);
    }
  if (scnhdr_int->s_nreloc <= 0xffff)
    PUTHALF(abfd, scnhdr_int->s_nreloc, (bfd_byte *) scnhdr_ext->s_nreloc);
  else
    {
      char buf[sizeof (scnhdr_int->s_name) + 1];

      memcpy (buf, scnhdr_int->s_name, sizeof (scnhdr_int->s_name));
      buf[sizeof (scnhdr_int->s_name)] = '\0';
      (*_bfd_error_handler) ("%s: %s: reloc overflow: 0x%lx > 0xffff",
			     bfd_get_filename (abfd),
			     buf, scnhdr_int->s_nreloc);
      bfd_set_error (bfd_error_file_truncated);
      PUTHALF (abfd, 0xffff, (bfd_byte *) scnhdr_ext->s_nreloc);
      ret = 0;
    }
#endif

#ifdef I960
  PUT_SCNHDR_ALIGN (abfd, scnhdr_int->s_align, (bfd_byte *) scnhdr_ext->s_align);
#endif
#ifdef COFF_ADJUST_SCNHDR_OUT_POST
  COFF_ADJUST_SCNHDR_OUT_POST (abfd, in, out);
#endif
  return ret;
}
