/* Generic ECOFF swapping routines, for BFD.
   Copyright 1992, 1993, 1994, 1995, 1996, 2000 Free Software Foundation, Inc.
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

/* NOTE: This is a header file, but it contains executable routines.
   This is done this way because these routines are substantially
   similar, but are not identical, for all ECOFF targets.

   These are routines to swap the ECOFF symbolic information in and
   out.  The routines are defined statically.  You can set breakpoints
   on them in gdb by naming the including source file; e.g.,
   'coff-mips.c':ecoff_swap_hdr_in.

   Before including this header file, one of ECOFF_32, ECOFF_64,
   ECOFF_SIGNED_32 or ECOFF_SIGNED_64 must be defined.  These are
   checked when swapping information that depends upon the target
   size.  This code works for 32 bit and 64 bit ECOFF, but may need to
   be generalized in the future.

   Some header file which defines the external forms of these
   structures must also be included before including this header file.
   Currently this is either coff/mips.h or coff/alpha.h.

   If the symbol TEST is defined when this file is compiled, a
   comparison is made to ensure that, in fact, the output is
   bit-for-bit the same as the input.  Of course, this symbol should
   only be defined when deliberately testing the code on a machine
   with the proper byte sex and such.  */

#ifdef ECOFF_32
#define ecoff_get_off bfd_h_get_32
#define ecoff_put_off bfd_h_put_32
#endif
#ifdef ECOFF_64
#define ecoff_get_off bfd_h_get_64
#define ecoff_put_off bfd_h_put_64
#endif
#ifdef ECOFF_SIGNED_32
#define ecoff_get_off bfd_h_get_signed_32
#define ecoff_put_off bfd_h_put_signed_32
#endif
#ifdef ECOFF_SIGNED_64
#define ecoff_get_off bfd_h_get_signed_64
#define ecoff_put_off bfd_h_put_signed_64
#endif

/* ECOFF auxiliary information swapping routines.  These are the same
   for all ECOFF targets, so they are defined in ecofflink.c.  */

extern void _bfd_ecoff_swap_tir_in
  PARAMS ((int, const struct tir_ext *, TIR *));
extern void _bfd_ecoff_swap_tir_out
  PARAMS ((int, const TIR *, struct tir_ext *));
extern void _bfd_ecoff_swap_rndx_in
  PARAMS ((int, const struct rndx_ext *, RNDXR *));
extern void _bfd_ecoff_swap_rndx_out
  PARAMS ((int, const RNDXR *, struct rndx_ext *));

/* Prototypes for functions defined in this file.  */

static void ecoff_swap_hdr_in PARAMS ((bfd *, PTR, HDRR *));
static void ecoff_swap_hdr_out PARAMS ((bfd *, const HDRR *, PTR));
static void ecoff_swap_fdr_in PARAMS ((bfd *, PTR, FDR *));
static void ecoff_swap_fdr_out PARAMS ((bfd *, const FDR *, PTR));
static void ecoff_swap_pdr_in PARAMS ((bfd *, PTR, PDR *));
static void ecoff_swap_pdr_out PARAMS ((bfd *, const PDR *, PTR));
static void ecoff_swap_sym_in PARAMS ((bfd *, PTR, SYMR *));
static void ecoff_swap_sym_out PARAMS ((bfd *, const SYMR *, PTR));
static void ecoff_swap_ext_in PARAMS ((bfd *, PTR, EXTR *));
static void ecoff_swap_ext_out PARAMS ((bfd *, const EXTR *, PTR));
static void ecoff_swap_rfd_in PARAMS ((bfd *, PTR, RFDT *));
static void ecoff_swap_rfd_out PARAMS ((bfd *, const RFDT *, PTR));
static void ecoff_swap_opt_in PARAMS ((bfd *, PTR, OPTR *));
static void ecoff_swap_opt_out PARAMS ((bfd *, const OPTR *, PTR));
static void ecoff_swap_dnr_in PARAMS ((bfd *, PTR, DNR *));
static void ecoff_swap_dnr_out PARAMS ((bfd *, const DNR *, PTR));

/* Swap in the symbolic header.  */

static void
ecoff_swap_hdr_in (abfd, ext_copy, intern)
     bfd *abfd;
     PTR ext_copy;
     HDRR *intern;
{
  struct hdr_ext ext[1];

  *ext = *(struct hdr_ext *) ext_copy;

  intern->magic         = bfd_h_get_signed_16 (abfd, (bfd_byte *)ext->h_magic);
  intern->vstamp        = bfd_h_get_signed_16 (abfd, (bfd_byte *)ext->h_vstamp);
  intern->ilineMax      = bfd_h_get_32 (abfd, (bfd_byte *)ext->h_ilineMax);
  intern->cbLine        = ecoff_get_off (abfd, (bfd_byte *)ext->h_cbLine);
  intern->cbLineOffset  = ecoff_get_off (abfd, (bfd_byte *)ext->h_cbLineOffset);
  intern->idnMax        = bfd_h_get_32 (abfd, (bfd_byte *)ext->h_idnMax);
  intern->cbDnOffset    = ecoff_get_off (abfd, (bfd_byte *)ext->h_cbDnOffset);
  intern->ipdMax        = bfd_h_get_32 (abfd, (bfd_byte *)ext->h_ipdMax);
  intern->cbPdOffset    = ecoff_get_off (abfd, (bfd_byte *)ext->h_cbPdOffset);
  intern->isymMax       = bfd_h_get_32 (abfd, (bfd_byte *)ext->h_isymMax);
  intern->cbSymOffset   = ecoff_get_off (abfd, (bfd_byte *)ext->h_cbSymOffset);
  intern->ioptMax       = bfd_h_get_32 (abfd, (bfd_byte *)ext->h_ioptMax);
  intern->cbOptOffset   = ecoff_get_off (abfd, (bfd_byte *)ext->h_cbOptOffset);
  intern->iauxMax       = bfd_h_get_32 (abfd, (bfd_byte *)ext->h_iauxMax);
  intern->cbAuxOffset   = ecoff_get_off (abfd, (bfd_byte *)ext->h_cbAuxOffset);
  intern->issMax        = bfd_h_get_32 (abfd, (bfd_byte *)ext->h_issMax);
  intern->cbSsOffset    = ecoff_get_off (abfd, (bfd_byte *)ext->h_cbSsOffset);
  intern->issExtMax     = bfd_h_get_32 (abfd, (bfd_byte *)ext->h_issExtMax);
  intern->cbSsExtOffset = ecoff_get_off (abfd, (bfd_byte *)ext->h_cbSsExtOffset);
  intern->ifdMax        = bfd_h_get_32 (abfd, (bfd_byte *)ext->h_ifdMax);
  intern->cbFdOffset    = ecoff_get_off (abfd, (bfd_byte *)ext->h_cbFdOffset);
  intern->crfd          = bfd_h_get_32 (abfd, (bfd_byte *)ext->h_crfd);
  intern->cbRfdOffset   = ecoff_get_off (abfd, (bfd_byte *)ext->h_cbRfdOffset);
  intern->iextMax       = bfd_h_get_32 (abfd, (bfd_byte *)ext->h_iextMax);
  intern->cbExtOffset   = ecoff_get_off (abfd, (bfd_byte *)ext->h_cbExtOffset);

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}

/* Swap out the symbolic header.  */

static void
ecoff_swap_hdr_out (abfd, intern_copy, ext_ptr)
     bfd *abfd;
     const HDRR *intern_copy;
     PTR ext_ptr;
{
  struct hdr_ext *ext = (struct hdr_ext *) ext_ptr;
  HDRR intern[1];

  *intern = *intern_copy;

  bfd_h_put_signed_16 (abfd, intern->magic, (bfd_byte *)ext->h_magic);
  bfd_h_put_signed_16 (abfd, intern->vstamp, (bfd_byte *)ext->h_vstamp);
  bfd_h_put_32 (abfd, intern->ilineMax, (bfd_byte *)ext->h_ilineMax);
  ecoff_put_off (abfd, intern->cbLine, (bfd_byte *)ext->h_cbLine);
  ecoff_put_off (abfd, intern->cbLineOffset, (bfd_byte *)ext->h_cbLineOffset);
  bfd_h_put_32 (abfd, intern->idnMax, (bfd_byte *)ext->h_idnMax);
  ecoff_put_off (abfd, intern->cbDnOffset, (bfd_byte *)ext->h_cbDnOffset);
  bfd_h_put_32 (abfd, intern->ipdMax, (bfd_byte *)ext->h_ipdMax);
  ecoff_put_off (abfd, intern->cbPdOffset, (bfd_byte *)ext->h_cbPdOffset);
  bfd_h_put_32 (abfd, intern->isymMax, (bfd_byte *)ext->h_isymMax);
  ecoff_put_off (abfd, intern->cbSymOffset, (bfd_byte *)ext->h_cbSymOffset);
  bfd_h_put_32 (abfd, intern->ioptMax, (bfd_byte *)ext->h_ioptMax);
  ecoff_put_off (abfd, intern->cbOptOffset, (bfd_byte *)ext->h_cbOptOffset);
  bfd_h_put_32 (abfd, intern->iauxMax, (bfd_byte *)ext->h_iauxMax);
  ecoff_put_off (abfd, intern->cbAuxOffset, (bfd_byte *)ext->h_cbAuxOffset);
  bfd_h_put_32 (abfd, intern->issMax, (bfd_byte *)ext->h_issMax);
  ecoff_put_off (abfd, intern->cbSsOffset, (bfd_byte *)ext->h_cbSsOffset);
  bfd_h_put_32 (abfd, intern->issExtMax, (bfd_byte *)ext->h_issExtMax);
  ecoff_put_off (abfd, intern->cbSsExtOffset, (bfd_byte *)ext->h_cbSsExtOffset);
  bfd_h_put_32 (abfd, intern->ifdMax, (bfd_byte *)ext->h_ifdMax);
  ecoff_put_off (abfd, intern->cbFdOffset, (bfd_byte *)ext->h_cbFdOffset);
  bfd_h_put_32 (abfd, intern->crfd, (bfd_byte *)ext->h_crfd);
  ecoff_put_off (abfd, intern->cbRfdOffset, (bfd_byte *)ext->h_cbRfdOffset);
  bfd_h_put_32 (abfd, intern->iextMax, (bfd_byte *)ext->h_iextMax);
  ecoff_put_off (abfd, intern->cbExtOffset, (bfd_byte *)ext->h_cbExtOffset);

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}

/* Swap in the file descriptor record.  */

static void
ecoff_swap_fdr_in (abfd, ext_copy, intern)
     bfd *abfd;
     PTR ext_copy;
     FDR *intern;
{
  struct fdr_ext ext[1];

  *ext = *(struct fdr_ext *) ext_copy;

  intern->adr           = ecoff_get_off (abfd, (bfd_byte *)ext->f_adr);
  intern->rss           = bfd_h_get_32 (abfd, (bfd_byte *)ext->f_rss);
#if defined (ECOFF_64) || defined (ECOFF_SIGNED_64)
  if (intern->rss == 0xffffffff)
    intern->rss = -1;
#endif
  intern->issBase       = bfd_h_get_32 (abfd, (bfd_byte *)ext->f_issBase);
  intern->cbSs          = ecoff_get_off (abfd, (bfd_byte *)ext->f_cbSs);
  intern->isymBase      = bfd_h_get_32 (abfd, (bfd_byte *)ext->f_isymBase);
  intern->csym          = bfd_h_get_32 (abfd, (bfd_byte *)ext->f_csym);
  intern->ilineBase     = bfd_h_get_32 (abfd, (bfd_byte *)ext->f_ilineBase);
  intern->cline         = bfd_h_get_32 (abfd, (bfd_byte *)ext->f_cline);
  intern->ioptBase      = bfd_h_get_32 (abfd, (bfd_byte *)ext->f_ioptBase);
  intern->copt          = bfd_h_get_32 (abfd, (bfd_byte *)ext->f_copt);
#if defined (ECOFF_32) || defined (ECOFF_SIGNED_32)
  intern->ipdFirst      = bfd_h_get_16 (abfd, (bfd_byte *)ext->f_ipdFirst);
  intern->cpd           = bfd_h_get_16 (abfd, (bfd_byte *)ext->f_cpd);
#endif
#if defined (ECOFF_64) || defined (ECOFF_SIGNED_64)
  intern->ipdFirst      = bfd_h_get_32 (abfd, (bfd_byte *)ext->f_ipdFirst);
  intern->cpd           = bfd_h_get_32 (abfd, (bfd_byte *)ext->f_cpd);
#endif
  intern->iauxBase      = bfd_h_get_32 (abfd, (bfd_byte *)ext->f_iauxBase);
  intern->caux          = bfd_h_get_32 (abfd, (bfd_byte *)ext->f_caux);
  intern->rfdBase       = bfd_h_get_32 (abfd, (bfd_byte *)ext->f_rfdBase);
  intern->crfd          = bfd_h_get_32 (abfd, (bfd_byte *)ext->f_crfd);

  /* now the fun stuff...  */
  if (bfd_header_big_endian (abfd)) {
    intern->lang        = (ext->f_bits1[0] & FDR_BITS1_LANG_BIG)
					>> FDR_BITS1_LANG_SH_BIG;
    intern->fMerge      = 0 != (ext->f_bits1[0] & FDR_BITS1_FMERGE_BIG);
    intern->fReadin     = 0 != (ext->f_bits1[0] & FDR_BITS1_FREADIN_BIG);
    intern->fBigendian  = 0 != (ext->f_bits1[0] & FDR_BITS1_FBIGENDIAN_BIG);
    intern->glevel      = (ext->f_bits2[0] & FDR_BITS2_GLEVEL_BIG)
					>> FDR_BITS2_GLEVEL_SH_BIG;
  } else {
    intern->lang        = (ext->f_bits1[0] & FDR_BITS1_LANG_LITTLE)
					>> FDR_BITS1_LANG_SH_LITTLE;
    intern->fMerge      = 0 != (ext->f_bits1[0] & FDR_BITS1_FMERGE_LITTLE);
    intern->fReadin     = 0 != (ext->f_bits1[0] & FDR_BITS1_FREADIN_LITTLE);
    intern->fBigendian  = 0 != (ext->f_bits1[0] & FDR_BITS1_FBIGENDIAN_LITTLE);
    intern->glevel      = (ext->f_bits2[0] & FDR_BITS2_GLEVEL_LITTLE)
					>> FDR_BITS2_GLEVEL_SH_LITTLE;
  }
  intern->reserved = 0;

  intern->cbLineOffset  = ecoff_get_off (abfd, (bfd_byte *)ext->f_cbLineOffset);
  intern->cbLine        = ecoff_get_off (abfd, (bfd_byte *)ext->f_cbLine);

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}

/* Swap out the file descriptor record.  */

static void
ecoff_swap_fdr_out (abfd, intern_copy, ext_ptr)
     bfd *abfd;
     const FDR *intern_copy;
     PTR ext_ptr;
{
  struct fdr_ext *ext = (struct fdr_ext *) ext_ptr;
  FDR intern[1];

  *intern = *intern_copy;	/* Make it reasonable to do in-place.  */

  ecoff_put_off (abfd, intern->adr, (bfd_byte *)ext->f_adr);
  bfd_h_put_32 (abfd, intern->rss, (bfd_byte *)ext->f_rss);
  bfd_h_put_32 (abfd, intern->issBase, (bfd_byte *)ext->f_issBase);
  ecoff_put_off (abfd, intern->cbSs, (bfd_byte *)ext->f_cbSs);
  bfd_h_put_32 (abfd, intern->isymBase, (bfd_byte *)ext->f_isymBase);
  bfd_h_put_32 (abfd, intern->csym, (bfd_byte *)ext->f_csym);
  bfd_h_put_32 (abfd, intern->ilineBase, (bfd_byte *)ext->f_ilineBase);
  bfd_h_put_32 (abfd, intern->cline, (bfd_byte *)ext->f_cline);
  bfd_h_put_32 (abfd, intern->ioptBase, (bfd_byte *)ext->f_ioptBase);
  bfd_h_put_32 (abfd, intern->copt, (bfd_byte *)ext->f_copt);
#if defined (ECOFF_32) || defined (ECOFF_SIGNED_32)
  bfd_h_put_16 (abfd, intern->ipdFirst, (bfd_byte *)ext->f_ipdFirst);
  bfd_h_put_16 (abfd, intern->cpd, (bfd_byte *)ext->f_cpd);
#endif
#if defined (ECOFF_64) || defined (ECOFF_SIGNED_64)
  bfd_h_put_32 (abfd, intern->ipdFirst, (bfd_byte *)ext->f_ipdFirst);
  bfd_h_put_32 (abfd, intern->cpd, (bfd_byte *)ext->f_cpd);
#endif
  bfd_h_put_32 (abfd, intern->iauxBase, (bfd_byte *)ext->f_iauxBase);
  bfd_h_put_32 (abfd, intern->caux, (bfd_byte *)ext->f_caux);
  bfd_h_put_32 (abfd, intern->rfdBase, (bfd_byte *)ext->f_rfdBase);
  bfd_h_put_32 (abfd, intern->crfd, (bfd_byte *)ext->f_crfd);

  /* now the fun stuff...  */
  if (bfd_header_big_endian (abfd)) {
    ext->f_bits1[0] = (((intern->lang << FDR_BITS1_LANG_SH_BIG)
			& FDR_BITS1_LANG_BIG)
		       | (intern->fMerge ? FDR_BITS1_FMERGE_BIG : 0)
		       | (intern->fReadin ? FDR_BITS1_FREADIN_BIG : 0)
		       | (intern->fBigendian ? FDR_BITS1_FBIGENDIAN_BIG : 0));
    ext->f_bits2[0] = ((intern->glevel << FDR_BITS2_GLEVEL_SH_BIG)
		       & FDR_BITS2_GLEVEL_BIG);
    ext->f_bits2[1] = 0;
    ext->f_bits2[2] = 0;
  } else {
    ext->f_bits1[0] = (((intern->lang << FDR_BITS1_LANG_SH_LITTLE)
			& FDR_BITS1_LANG_LITTLE)
		       | (intern->fMerge ? FDR_BITS1_FMERGE_LITTLE : 0)
		       | (intern->fReadin ? FDR_BITS1_FREADIN_LITTLE : 0)
		       | (intern->fBigendian ? FDR_BITS1_FBIGENDIAN_LITTLE : 0));
    ext->f_bits2[0] = ((intern->glevel << FDR_BITS2_GLEVEL_SH_LITTLE)
		       & FDR_BITS2_GLEVEL_LITTLE);
    ext->f_bits2[1] = 0;
    ext->f_bits2[2] = 0;
  }

  ecoff_put_off (abfd, intern->cbLineOffset, (bfd_byte *)ext->f_cbLineOffset);
  ecoff_put_off (abfd, intern->cbLine, (bfd_byte *)ext->f_cbLine);

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}

#ifndef MPW_C

/* Swap in the procedure descriptor record.  */

static void
ecoff_swap_pdr_in (abfd, ext_copy, intern)
     bfd *abfd;
     PTR ext_copy;
     PDR *intern;
{
  struct pdr_ext ext[1];

  *ext = *(struct pdr_ext *) ext_copy;

  memset ((PTR) intern, 0, sizeof (*intern));

  intern->adr           = ecoff_get_off (abfd, (bfd_byte *)ext->p_adr);
  intern->isym          = bfd_h_get_32 (abfd, (bfd_byte *)ext->p_isym);
  intern->iline         = bfd_h_get_32 (abfd, (bfd_byte *)ext->p_iline);
  intern->regmask       = bfd_h_get_32 (abfd, (bfd_byte *)ext->p_regmask);
  intern->regoffset     = bfd_h_get_signed_32 (abfd,
					       (bfd_byte *)ext->p_regoffset);
  intern->iopt          = bfd_h_get_signed_32 (abfd, (bfd_byte *)ext->p_iopt);
  intern->fregmask      = bfd_h_get_32 (abfd, (bfd_byte *)ext->p_fregmask);
  intern->fregoffset    = bfd_h_get_signed_32 (abfd,
					       (bfd_byte *)ext->p_fregoffset);
  intern->frameoffset   = bfd_h_get_signed_32 (abfd,
					       (bfd_byte *)ext->p_frameoffset);
  intern->framereg      = bfd_h_get_16 (abfd, (bfd_byte *)ext->p_framereg);
  intern->pcreg         = bfd_h_get_16 (abfd, (bfd_byte *)ext->p_pcreg);
  intern->lnLow         = bfd_h_get_32 (abfd, (bfd_byte *)ext->p_lnLow);
  intern->lnHigh        = bfd_h_get_32 (abfd, (bfd_byte *)ext->p_lnHigh);
  intern->cbLineOffset  = ecoff_get_off (abfd, (bfd_byte *)ext->p_cbLineOffset);

#if defined (ECOFF_64) || defined (ECOFF_SIGNED_64)
  intern->gp_prologue = bfd_h_get_8 (abfd, (bfd_byte *) ext->p_gp_prologue);
  if (bfd_header_big_endian (abfd))
    {
      intern->gp_used = 0 != (ext->p_bits1[0] & PDR_BITS1_GP_USED_BIG);
      intern->reg_frame = 0 != (ext->p_bits1[0] & PDR_BITS1_REG_FRAME_BIG);
      intern->prof = 0 != (ext->p_bits1[0] & PDR_BITS1_PROF_BIG);
      intern->reserved = (((ext->p_bits1[0] & PDR_BITS1_RESERVED_BIG)
			   << PDR_BITS1_RESERVED_SH_LEFT_BIG)
			  | ((ext->p_bits2[0] & PDR_BITS2_RESERVED_BIG)
			     >> PDR_BITS2_RESERVED_SH_BIG));
    }
  else
    {
      intern->gp_used = 0 != (ext->p_bits1[0] & PDR_BITS1_GP_USED_LITTLE);
      intern->reg_frame = 0 != (ext->p_bits1[0] & PDR_BITS1_REG_FRAME_LITTLE);
      intern->prof = 0 != (ext->p_bits1[0] & PDR_BITS1_PROF_LITTLE);
      intern->reserved = (((ext->p_bits1[0] & PDR_BITS1_RESERVED_LITTLE)
			   >> PDR_BITS1_RESERVED_SH_LITTLE)
			  | ((ext->p_bits2[0] & PDR_BITS2_RESERVED_LITTLE)
			     << PDR_BITS2_RESERVED_SH_LEFT_LITTLE));
    }
  intern->localoff = bfd_h_get_8 (abfd, (bfd_byte *) ext->p_localoff);
#endif

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}

/* Swap out the procedure descriptor record.  */

static void
ecoff_swap_pdr_out (abfd, intern_copy, ext_ptr)
     bfd *abfd;
     const PDR *intern_copy;
     PTR ext_ptr;
{
  struct pdr_ext *ext = (struct pdr_ext *) ext_ptr;
  PDR intern[1];

  *intern = *intern_copy;	/* Make it reasonable to do in-place.  */

  ecoff_put_off (abfd, intern->adr, (bfd_byte *)ext->p_adr);
  bfd_h_put_32 (abfd, intern->isym, (bfd_byte *)ext->p_isym);
  bfd_h_put_32 (abfd, intern->iline, (bfd_byte *)ext->p_iline);
  bfd_h_put_32 (abfd, intern->regmask, (bfd_byte *)ext->p_regmask);
  bfd_h_put_32 (abfd, intern->regoffset, (bfd_byte *)ext->p_regoffset);
  bfd_h_put_32 (abfd, intern->iopt, (bfd_byte *)ext->p_iopt);
  bfd_h_put_32 (abfd, intern->fregmask, (bfd_byte *)ext->p_fregmask);
  bfd_h_put_32 (abfd, intern->fregoffset, (bfd_byte *)ext->p_fregoffset);
  bfd_h_put_32 (abfd, intern->frameoffset, (bfd_byte *)ext->p_frameoffset);
  bfd_h_put_16 (abfd, intern->framereg, (bfd_byte *)ext->p_framereg);
  bfd_h_put_16 (abfd, intern->pcreg, (bfd_byte *)ext->p_pcreg);
  bfd_h_put_32 (abfd, intern->lnLow, (bfd_byte *)ext->p_lnLow);
  bfd_h_put_32 (abfd, intern->lnHigh, (bfd_byte *)ext->p_lnHigh);
  ecoff_put_off (abfd, intern->cbLineOffset, (bfd_byte *)ext->p_cbLineOffset);

#if defined (ECOFF_64) || defined (ECOFF_SIGNED_64)
  bfd_h_put_8 (abfd, intern->gp_prologue, (bfd_byte *) ext->p_gp_prologue);
  if (bfd_header_big_endian (abfd))
    {
      ext->p_bits1[0] = ((intern->gp_used ? PDR_BITS1_GP_USED_BIG : 0)
			 | (intern->reg_frame ? PDR_BITS1_REG_FRAME_BIG : 0)
			 | (intern->prof ? PDR_BITS1_PROF_BIG : 0)
			 | ((intern->reserved
			     >> PDR_BITS1_RESERVED_SH_LEFT_BIG)
			    & PDR_BITS1_RESERVED_BIG));
      ext->p_bits2[0] = ((intern->reserved << PDR_BITS2_RESERVED_SH_BIG)
			 & PDR_BITS2_RESERVED_BIG);
    }
  else
    {
      ext->p_bits1[0] = ((intern->gp_used ? PDR_BITS1_GP_USED_LITTLE : 0)
			 | (intern->reg_frame ? PDR_BITS1_REG_FRAME_LITTLE : 0)
			 | (intern->prof ? PDR_BITS1_PROF_LITTLE : 0)
			 | ((intern->reserved << PDR_BITS1_RESERVED_SH_LITTLE)
			    & PDR_BITS1_RESERVED_LITTLE));
      ext->p_bits2[0] = ((intern->reserved >>
			  PDR_BITS2_RESERVED_SH_LEFT_LITTLE)
			 & PDR_BITS2_RESERVED_LITTLE);
    }
  bfd_h_put_8 (abfd, intern->localoff, (bfd_byte *) ext->p_localoff);
#endif

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}

#else /* MPW_C */
/* Same routines, but with ECOFF_64 code removed, so ^&%$#&! MPW C doesn't
   corrupt itself and then freak out.  */
/* Swap in the procedure descriptor record.  */

static void
ecoff_swap_pdr_in (abfd, ext_copy, intern)
     bfd *abfd;
     PTR ext_copy;
     PDR *intern;
{
  struct pdr_ext ext[1];

  *ext = *(struct pdr_ext *) ext_copy;

  intern->adr           = ecoff_get_off (abfd, (bfd_byte *)ext->p_adr);
  intern->isym          = bfd_h_get_32 (abfd, (bfd_byte *)ext->p_isym);
  intern->iline         = bfd_h_get_32 (abfd, (bfd_byte *)ext->p_iline);
  intern->regmask       = bfd_h_get_32 (abfd, (bfd_byte *)ext->p_regmask);
  intern->regoffset     = bfd_h_get_signed_32 (abfd,
					       (bfd_byte *)ext->p_regoffset);
  intern->iopt          = bfd_h_get_signed_32 (abfd, (bfd_byte *)ext->p_iopt);
  intern->fregmask      = bfd_h_get_32 (abfd, (bfd_byte *)ext->p_fregmask);
  intern->fregoffset    = bfd_h_get_signed_32 (abfd,
					       (bfd_byte *)ext->p_fregoffset);
  intern->frameoffset   = bfd_h_get_signed_32 (abfd,
					       (bfd_byte *)ext->p_frameoffset);
  intern->framereg      = bfd_h_get_16 (abfd, (bfd_byte *)ext->p_framereg);
  intern->pcreg         = bfd_h_get_16 (abfd, (bfd_byte *)ext->p_pcreg);
  intern->lnLow         = bfd_h_get_32 (abfd, (bfd_byte *)ext->p_lnLow);
  intern->lnHigh        = bfd_h_get_32 (abfd, (bfd_byte *)ext->p_lnHigh);
  intern->cbLineOffset  = ecoff_get_off (abfd, (bfd_byte *)ext->p_cbLineOffset);

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}

/* Swap out the procedure descriptor record.  */

static void
ecoff_swap_pdr_out (abfd, intern_copy, ext_ptr)
     bfd *abfd;
     const PDR *intern_copy;
     PTR ext_ptr;
{
  struct pdr_ext *ext = (struct pdr_ext *) ext_ptr;
  PDR intern[1];

  *intern = *intern_copy;	/* Make it reasonable to do in-place.  */

  ecoff_put_off (abfd, intern->adr, (bfd_byte *)ext->p_adr);
  bfd_h_put_32 (abfd, intern->isym, (bfd_byte *)ext->p_isym);
  bfd_h_put_32 (abfd, intern->iline, (bfd_byte *)ext->p_iline);
  bfd_h_put_32 (abfd, intern->regmask, (bfd_byte *)ext->p_regmask);
  bfd_h_put_32 (abfd, intern->regoffset, (bfd_byte *)ext->p_regoffset);
  bfd_h_put_32 (abfd, intern->iopt, (bfd_byte *)ext->p_iopt);
  bfd_h_put_32 (abfd, intern->fregmask, (bfd_byte *)ext->p_fregmask);
  bfd_h_put_32 (abfd, intern->fregoffset, (bfd_byte *)ext->p_fregoffset);
  bfd_h_put_32 (abfd, intern->frameoffset, (bfd_byte *)ext->p_frameoffset);
  bfd_h_put_16 (abfd, intern->framereg, (bfd_byte *)ext->p_framereg);
  bfd_h_put_16 (abfd, intern->pcreg, (bfd_byte *)ext->p_pcreg);
  bfd_h_put_32 (abfd, intern->lnLow, (bfd_byte *)ext->p_lnLow);
  bfd_h_put_32 (abfd, intern->lnHigh, (bfd_byte *)ext->p_lnHigh);
  ecoff_put_off (abfd, intern->cbLineOffset, (bfd_byte *)ext->p_cbLineOffset);

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}
#endif /* MPW_C */

/* Swap in a symbol record.  */

static void
ecoff_swap_sym_in (abfd, ext_copy, intern)
     bfd *abfd;
     PTR ext_copy;
     SYMR *intern;
{
  struct sym_ext ext[1];

  *ext = *(struct sym_ext *) ext_copy;

  intern->iss           = bfd_h_get_32 (abfd, (bfd_byte *)ext->s_iss);
  intern->value         = ecoff_get_off (abfd, (bfd_byte *)ext->s_value);

  /* now the fun stuff...  */
  if (bfd_header_big_endian (abfd)) {
    intern->st          =  (ext->s_bits1[0] & SYM_BITS1_ST_BIG)
					   >> SYM_BITS1_ST_SH_BIG;
    intern->sc          = ((ext->s_bits1[0] & SYM_BITS1_SC_BIG)
					   << SYM_BITS1_SC_SH_LEFT_BIG)
			| ((ext->s_bits2[0] & SYM_BITS2_SC_BIG)
					   >> SYM_BITS2_SC_SH_BIG);
    intern->reserved    = 0 != (ext->s_bits2[0] & SYM_BITS2_RESERVED_BIG);
    intern->index       = ((ext->s_bits2[0] & SYM_BITS2_INDEX_BIG)
					   << SYM_BITS2_INDEX_SH_LEFT_BIG)
			| (ext->s_bits3[0] << SYM_BITS3_INDEX_SH_LEFT_BIG)
			| (ext->s_bits4[0] << SYM_BITS4_INDEX_SH_LEFT_BIG);
  } else {
    intern->st          =  (ext->s_bits1[0] & SYM_BITS1_ST_LITTLE)
					   >> SYM_BITS1_ST_SH_LITTLE;
    intern->sc          = ((ext->s_bits1[0] & SYM_BITS1_SC_LITTLE)
					   >> SYM_BITS1_SC_SH_LITTLE)
			| ((ext->s_bits2[0] & SYM_BITS2_SC_LITTLE)
					   << SYM_BITS2_SC_SH_LEFT_LITTLE);
    intern->reserved    = 0 != (ext->s_bits2[0] & SYM_BITS2_RESERVED_LITTLE);
    intern->index       = ((ext->s_bits2[0] & SYM_BITS2_INDEX_LITTLE)
					   >> SYM_BITS2_INDEX_SH_LITTLE)
			| (ext->s_bits3[0] << SYM_BITS3_INDEX_SH_LEFT_LITTLE)
			| ((unsigned int) ext->s_bits4[0]
			   << SYM_BITS4_INDEX_SH_LEFT_LITTLE);
  }

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}

/* Swap out a symbol record.  */

static void
ecoff_swap_sym_out (abfd, intern_copy, ext_ptr)
     bfd *abfd;
     const SYMR *intern_copy;
     PTR ext_ptr;
{
  struct sym_ext *ext = (struct sym_ext *) ext_ptr;
  SYMR intern[1];

  *intern = *intern_copy;	/* Make it reasonable to do in-place.  */

  bfd_h_put_32 (abfd, intern->iss, (bfd_byte *)ext->s_iss);
  ecoff_put_off (abfd, intern->value, (bfd_byte *)ext->s_value);

  /* now the fun stuff...  */
  if (bfd_header_big_endian (abfd)) {
    ext->s_bits1[0] = (((intern->st << SYM_BITS1_ST_SH_BIG)
			& SYM_BITS1_ST_BIG)
		       | ((intern->sc >> SYM_BITS1_SC_SH_LEFT_BIG)
			  & SYM_BITS1_SC_BIG));
    ext->s_bits2[0] = (((intern->sc << SYM_BITS2_SC_SH_BIG)
			& SYM_BITS2_SC_BIG)
		       | (intern->reserved ? SYM_BITS2_RESERVED_BIG : 0)
		       | ((intern->index >> SYM_BITS2_INDEX_SH_LEFT_BIG)
			  & SYM_BITS2_INDEX_BIG));
    ext->s_bits3[0] = (intern->index >> SYM_BITS3_INDEX_SH_LEFT_BIG) & 0xff;
    ext->s_bits4[0] = (intern->index >> SYM_BITS4_INDEX_SH_LEFT_BIG) & 0xff;
  } else {
    ext->s_bits1[0] = (((intern->st << SYM_BITS1_ST_SH_LITTLE)
			& SYM_BITS1_ST_LITTLE)
		       | ((intern->sc << SYM_BITS1_SC_SH_LITTLE)
			  & SYM_BITS1_SC_LITTLE));
    ext->s_bits2[0] = (((intern->sc >> SYM_BITS2_SC_SH_LEFT_LITTLE)
			& SYM_BITS2_SC_LITTLE)
		       | (intern->reserved ? SYM_BITS2_RESERVED_LITTLE : 0)
		       | ((intern->index << SYM_BITS2_INDEX_SH_LITTLE)
			  & SYM_BITS2_INDEX_LITTLE));
    ext->s_bits3[0] = (intern->index >> SYM_BITS3_INDEX_SH_LEFT_LITTLE) & 0xff;
    ext->s_bits4[0] = (intern->index >> SYM_BITS4_INDEX_SH_LEFT_LITTLE) & 0xff;
  }

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}

/* Swap in an external symbol record.  */

static void
ecoff_swap_ext_in (abfd, ext_copy, intern)
     bfd *abfd;
     PTR ext_copy;
     EXTR *intern;
{
  struct ext_ext ext[1];

  *ext = *(struct ext_ext *) ext_copy;

  /* now the fun stuff...  */
  if (bfd_header_big_endian (abfd)) {
    intern->jmptbl      = 0 != (ext->es_bits1[0] & EXT_BITS1_JMPTBL_BIG);
    intern->cobol_main  = 0 != (ext->es_bits1[0] & EXT_BITS1_COBOL_MAIN_BIG);
    intern->weakext     = 0 != (ext->es_bits1[0] & EXT_BITS1_WEAKEXT_BIG);
  } else {
    intern->jmptbl      = 0 != (ext->es_bits1[0] & EXT_BITS1_JMPTBL_LITTLE);
    intern->cobol_main  = 0 != (ext->es_bits1[0] & EXT_BITS1_COBOL_MAIN_LITTLE);
    intern->weakext     = 0 != (ext->es_bits1[0] & EXT_BITS1_WEAKEXT_LITTLE);
  }
  intern->reserved = 0;

#if defined (ECOFF_32) || defined (ECOFF_SIGNED_32)
  intern->ifd           = bfd_h_get_signed_16 (abfd, (bfd_byte *)ext->es_ifd);
#endif
#if defined (ECOFF_64) || defined (ECOFF_SIGNED_64)
  intern->ifd           = bfd_h_get_signed_32 (abfd, (bfd_byte *)ext->es_ifd);
#endif

  ecoff_swap_sym_in (abfd, &ext->es_asym, &intern->asym);

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}

/* Swap out an external symbol record.  */

static void
ecoff_swap_ext_out (abfd, intern_copy, ext_ptr)
     bfd *abfd;
     const EXTR *intern_copy;
     PTR ext_ptr;
{
  struct ext_ext *ext = (struct ext_ext *) ext_ptr;
  EXTR intern[1];

  *intern = *intern_copy;	/* Make it reasonable to do in-place.  */

  /* now the fun stuff...  */
  if (bfd_header_big_endian (abfd)) {
    ext->es_bits1[0] = ((intern->jmptbl ? EXT_BITS1_JMPTBL_BIG : 0)
			| (intern->cobol_main ? EXT_BITS1_COBOL_MAIN_BIG : 0)
			| (intern->weakext ? EXT_BITS1_WEAKEXT_BIG : 0));
    ext->es_bits2[0] = 0;
#if defined (ECOFF_64) || defined (ECOFF_SIGNED_64)
    ext->es_bits2[1] = 0;
    ext->es_bits2[2] = 0;
#endif
  } else {
    ext->es_bits1[0] = ((intern->jmptbl ? EXT_BITS1_JMPTBL_LITTLE : 0)
			| (intern->cobol_main ? EXT_BITS1_COBOL_MAIN_LITTLE : 0)
			| (intern->weakext ? EXT_BITS1_WEAKEXT_LITTLE : 0));
    ext->es_bits2[0] = 0;
#if defined (ECOFF_64) || defined (ECOFF_SIGNED_64)
    ext->es_bits2[1] = 0;
    ext->es_bits2[2] = 0;
#endif
  }

#if defined (ECOFF_32) || defined (ECOFF_SIGNED_32)
  bfd_h_put_signed_16 (abfd, intern->ifd, (bfd_byte *)ext->es_ifd);
#endif
#if defined (ECOFF_64) || defined (ECOFF_SIGNED_64)
  bfd_h_put_signed_32 (abfd, intern->ifd, (bfd_byte *)ext->es_ifd);
#endif

  ecoff_swap_sym_out (abfd, &intern->asym, &ext->es_asym);

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}

/* Swap in a relative file descriptor.  */

static void
ecoff_swap_rfd_in (abfd, ext_ptr, intern)
     bfd *abfd;
     PTR ext_ptr;
     RFDT *intern;
{
  struct rfd_ext *ext = (struct rfd_ext *) ext_ptr;

  *intern = bfd_h_get_32 (abfd, (bfd_byte *)ext->rfd);

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}

/* Swap out a relative file descriptor.  */

static void
ecoff_swap_rfd_out (abfd, intern, ext_ptr)
     bfd *abfd;
     const RFDT *intern;
     PTR ext_ptr;
{
  struct rfd_ext *ext = (struct rfd_ext *) ext_ptr;

  bfd_h_put_32 (abfd, *intern, (bfd_byte *)ext->rfd);

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}

/* Swap in an optimization symbol.  */

static void
ecoff_swap_opt_in (abfd, ext_copy, intern)
     bfd *abfd;
     PTR ext_copy;
     OPTR *intern;
{
  struct opt_ext ext[1];

  *ext = *(struct opt_ext *) ext_copy;

  if (bfd_header_big_endian (abfd))
    {
      intern->ot = ext->o_bits1[0];
      intern->value = (((unsigned int) ext->o_bits2[0]
			<< OPT_BITS2_VALUE_SH_LEFT_BIG)
		       | ((unsigned int) ext->o_bits3[0]
			  << OPT_BITS2_VALUE_SH_LEFT_BIG)
		       | ((unsigned int) ext->o_bits4[0]
			  << OPT_BITS2_VALUE_SH_LEFT_BIG));
    }
  else
    {
      intern->ot = ext->o_bits1[0];
      intern->value = ((ext->o_bits2[0] << OPT_BITS2_VALUE_SH_LEFT_LITTLE)
		       | (ext->o_bits3[0] << OPT_BITS2_VALUE_SH_LEFT_LITTLE)
		       | (ext->o_bits4[0] << OPT_BITS2_VALUE_SH_LEFT_LITTLE));
    }

  _bfd_ecoff_swap_rndx_in (bfd_header_big_endian (abfd),
			   &ext->o_rndx, &intern->rndx);

  intern->offset = bfd_h_get_32 (abfd, (bfd_byte *) ext->o_offset);

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}

/* Swap out an optimization symbol.  */

static void
ecoff_swap_opt_out (abfd, intern_copy, ext_ptr)
     bfd *abfd;
     const OPTR *intern_copy;
     PTR ext_ptr;
{
  struct opt_ext *ext = (struct opt_ext *) ext_ptr;
  OPTR intern[1];

  *intern = *intern_copy;	/* Make it reasonable to do in-place.  */

  if (bfd_header_big_endian (abfd))
    {
      ext->o_bits1[0] = intern->ot;
      ext->o_bits2[0] = intern->value >> OPT_BITS2_VALUE_SH_LEFT_BIG;
      ext->o_bits3[0] = intern->value >> OPT_BITS3_VALUE_SH_LEFT_BIG;
      ext->o_bits4[0] = intern->value >> OPT_BITS4_VALUE_SH_LEFT_BIG;
    }
  else
    {
      ext->o_bits1[0] = intern->ot;
      ext->o_bits2[0] = intern->value >> OPT_BITS2_VALUE_SH_LEFT_LITTLE;
      ext->o_bits3[0] = intern->value >> OPT_BITS3_VALUE_SH_LEFT_LITTLE;
      ext->o_bits4[0] = intern->value >> OPT_BITS4_VALUE_SH_LEFT_LITTLE;
    }

  _bfd_ecoff_swap_rndx_out (bfd_header_big_endian (abfd),
			    &intern->rndx, &ext->o_rndx);

  bfd_h_put_32 (abfd, intern->value, (bfd_byte *) ext->o_offset);

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}

/* Swap in a dense number.  */

static void
ecoff_swap_dnr_in (abfd, ext_copy, intern)
     bfd *abfd;
     PTR ext_copy;
     DNR *intern;
{
  struct dnr_ext ext[1];

  *ext = *(struct dnr_ext *) ext_copy;

  intern->rfd = bfd_h_get_32 (abfd, (bfd_byte *) ext->d_rfd);
  intern->index = bfd_h_get_32 (abfd, (bfd_byte *) ext->d_index);

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}

/* Swap out a dense number.  */

static void
ecoff_swap_dnr_out (abfd, intern_copy, ext_ptr)
     bfd *abfd;
     const DNR *intern_copy;
     PTR ext_ptr;
{
  struct dnr_ext *ext = (struct dnr_ext *) ext_ptr;
  DNR intern[1];

  *intern = *intern_copy;	/* Make it reasonable to do in-place.  */

  bfd_h_put_32 (abfd, intern->rfd, (bfd_byte *) ext->d_rfd);
  bfd_h_put_32 (abfd, intern->index, (bfd_byte *) ext->d_index);

#ifdef TEST
  if (memcmp ((char *)ext, (char *)intern, sizeof (*intern)) != 0)
    abort ();
#endif
}
