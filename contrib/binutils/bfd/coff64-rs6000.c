/* BFD back-end for IBM RS/6000 "XCOFF64" files.
   Copyright 2000, 2001
   Free Software Foundation, Inc.
   Written Clinton Popetz.
   Contributed by Cygnus Support.

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
#include "bfdlink.h"
#include "libbfd.h"
#include "coff/internal.h"
#include "coff/xcoff.h"
#include "coff/rs6k64.h"
#include "libcoff.h"
#include "libxcoff.h"

#define GET_FILEHDR_SYMPTR H_GET_64
#define PUT_FILEHDR_SYMPTR H_PUT_64
#define GET_AOUTHDR_DATA_START H_GET_64
#define PUT_AOUTHDR_DATA_START H_PUT_64
#define GET_AOUTHDR_TEXT_START H_GET_64
#define PUT_AOUTHDR_TEXT_START H_PUT_64
#define GET_AOUTHDR_TSIZE H_GET_64
#define PUT_AOUTHDR_TSIZE H_PUT_64
#define GET_AOUTHDR_DSIZE H_GET_64
#define PUT_AOUTHDR_DSIZE H_PUT_64
#define GET_AOUTHDR_BSIZE H_GET_64
#define PUT_AOUTHDR_BSIZE H_PUT_64
#define GET_AOUTHDR_ENTRY H_GET_64
#define PUT_AOUTHDR_ENTRY H_PUT_64
#define GET_SCNHDR_PADDR H_GET_64
#define PUT_SCNHDR_PADDR H_PUT_64
#define GET_SCNHDR_VADDR H_GET_64
#define PUT_SCNHDR_VADDR H_PUT_64
#define GET_SCNHDR_SIZE H_GET_64
#define PUT_SCNHDR_SIZE H_PUT_64
#define GET_SCNHDR_SCNPTR H_GET_64
#define PUT_SCNHDR_SCNPTR H_PUT_64
#define GET_SCNHDR_RELPTR H_GET_64
#define PUT_SCNHDR_RELPTR H_PUT_64
#define GET_SCNHDR_LNNOPTR H_GET_64
#define PUT_SCNHDR_LNNOPTR H_PUT_64
#define GET_SCNHDR_NRELOC H_GET_32
#define MAX_SCNHDR_NRELOC 0xffffffff
#define PUT_SCNHDR_NRELOC H_PUT_32
#define GET_SCNHDR_NLNNO H_GET_32
#define MAX_SCNHDR_NLNNO 0xffffffff
#define PUT_SCNHDR_NLNNO H_PUT_32
#define GET_RELOC_VADDR H_GET_64
#define PUT_RELOC_VADDR H_PUT_64

#define COFF_FORCE_SYMBOLS_IN_STRINGS
#define COFF_DEBUG_STRING_WIDE_PREFIX


#define COFF_ADJUST_SCNHDR_OUT_POST(ABFD, INT, EXT)			\
  do									\
    {									\
      memset (((SCNHDR *) EXT)->s_pad, 0,				\
	      sizeof (((SCNHDR *) EXT)->s_pad));			\
    }									\
  while (0)

#define NO_COFF_LINENOS

#define coff_SWAP_lineno_in _bfd_xcoff64_swap_lineno_in
#define coff_SWAP_lineno_out _bfd_xcoff64_swap_lineno_out

static void _bfd_xcoff64_swap_lineno_in PARAMS ((bfd *, PTR, PTR));
static unsigned int _bfd_xcoff64_swap_lineno_out PARAMS ((bfd *, PTR, PTR));
static boolean _bfd_xcoff64_put_ldsymbol_name
  PARAMS ((bfd *, struct xcoff_loader_info *, struct internal_ldsym *,
	   const char *));
static void _bfd_xcoff64_swap_sym_in PARAMS ((bfd *, PTR, PTR));
static unsigned int _bfd_xcoff64_swap_sym_out PARAMS ((bfd *, PTR, PTR));
static void _bfd_xcoff64_swap_aux_in
  PARAMS ((bfd *, PTR, int, int, int, int, PTR));
static unsigned int _bfd_xcoff64_swap_aux_out
  PARAMS ((bfd *, PTR, int, int, int, int, PTR));
extern boolean _bfd_xcoff_mkobject PARAMS ((bfd *));
extern boolean _bfd_xcoff_copy_private_bfd_data PARAMS ((bfd *, bfd *));
extern boolean _bfd_xcoff_is_local_label_name PARAMS ((bfd *, const char *));
extern void xcoff64_rtype2howto
  PARAMS ((arelent *, struct internal_reloc *));
extern reloc_howto_type * xcoff64_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
extern boolean _bfd_xcoff_slurp_armap PARAMS ((bfd *));
extern PTR _bfd_xcoff_read_ar_hdr PARAMS ((bfd *));
extern bfd *_bfd_xcoff_openr_next_archived_file PARAMS ((bfd *, bfd *));
extern int _bfd_xcoff_stat_arch_elt PARAMS ((bfd *, struct stat *));
extern boolean _bfd_xcoff_write_armap
  PARAMS ((bfd *, unsigned int, struct orl *, unsigned int, int));
extern boolean _bfd_xcoff_write_archive_contents PARAMS ((bfd *));
extern int _bfd_xcoff_sizeof_headers PARAMS ((bfd *, boolean));
extern void _bfd_xcoff_swap_sym_in PARAMS ((bfd *, PTR, PTR));
extern unsigned int _bfd_xcoff_swap_sym_out PARAMS ((bfd *, PTR, PTR));
extern void _bfd_xcoff_swap_aux_in
  PARAMS ((bfd *, PTR, int, int, int, int, PTR));
extern unsigned int _bfd_xcoff_swap_aux_out
  PARAMS ((bfd *, PTR, int, int, int, int, PTR));
static void xcoff64_swap_ldhdr_in
  PARAMS ((bfd *, const PTR, struct internal_ldhdr *));
static void xcoff64_swap_ldhdr_out
  PARAMS ((bfd *, const struct internal_ldhdr *, PTR d));
static void xcoff64_swap_ldsym_in
  PARAMS ((bfd *, const PTR, struct internal_ldsym *));
static void xcoff64_swap_ldsym_out
  PARAMS ((bfd *, const struct internal_ldsym *, PTR d));
static void xcoff64_swap_ldrel_in
  PARAMS ((bfd *, const PTR, struct internal_ldrel *));
static void xcoff64_swap_ldrel_out
  PARAMS ((bfd *, const struct internal_ldrel *, PTR d));
static boolean xcoff64_write_object_contents PARAMS ((bfd *));
static boolean xcoff64_ppc_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	 struct internal_reloc *, struct internal_syment *,
	 asection **sections));
static boolean xcoff64_slurp_armap PARAMS ((bfd *));
static const bfd_target *xcoff64_archive_p PARAMS ((bfd *));
static bfd *xcoff64_openr_next_archived_file PARAMS ((bfd *, bfd *));
static int xcoff64_sizeof_headers PARAMS ((bfd *, boolean));
static asection *xcoff64_create_csect_from_smclas
  PARAMS ((bfd *, union internal_auxent *, const char *));
static boolean xcoff64_is_lineno_count_overflow PARAMS ((bfd *, bfd_vma));
static boolean xcoff64_is_reloc_count_overflow PARAMS ((bfd *, bfd_vma));
static bfd_vma xcoff64_loader_symbol_offset
  PARAMS ((bfd *, struct internal_ldhdr *));
static bfd_vma xcoff64_loader_reloc_offset
  PARAMS ((bfd *, struct internal_ldhdr *));
static boolean xcoff64_generate_rtinit 
  PARAMS((bfd *, const char *, const char *));


/* coffcode.h needs these to be defined */
/* Internalcoff.h and coffcode.h modify themselves based on these flags.  */
#define XCOFF64
#define RS6000COFF_C 1

#define SELECT_RELOC(internal, howto)					\
  {									\
    internal.r_type = howto->type;					\
    internal.r_size =							\
      ((howto->complain_on_overflow == complain_overflow_signed		\
	? 0x80								\
	: 0)								\
       | (howto->bitsize - 1));						\
  }

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (3)
#define COFF_LONG_FILENAMES
#define NO_COFF_SYMBOLS
#define RTYPE2HOWTO(cache_ptr, dst) xcoff64_rtype2howto (cache_ptr, dst)
#define coff_mkobject _bfd_xcoff_mkobject
#define coff_bfd_copy_private_bfd_data _bfd_xcoff_copy_private_bfd_data
#define coff_bfd_is_local_label_name _bfd_xcoff_is_local_label_name
#define coff_bfd_reloc_type_lookup xcoff64_reloc_type_lookup
#ifdef AIX_CORE
extern const bfd_target * rs6000coff_core_p ();
extern boolean rs6000coff_core_file_matches_executable_p ();
extern char *rs6000coff_core_file_failing_command PARAMS ((bfd *abfd));
extern int rs6000coff_core_file_failing_signal PARAMS ((bfd *abfd));
#define CORE_FILE_P rs6000coff_core_p
#define coff_core_file_failing_command \
  rs6000coff_core_file_failing_command
#define coff_core_file_failing_signal \
  rs6000coff_core_file_failing_signal
#define coff_core_file_matches_executable_p \
  rs6000coff_core_file_matches_executable_p
#else
#define CORE_FILE_P _bfd_dummy_target
#define coff_core_file_failing_command \
  _bfd_nocore_core_file_failing_command
#define coff_core_file_failing_signal \
  _bfd_nocore_core_file_failing_signal
#define coff_core_file_matches_executable_p \
  _bfd_nocore_core_file_matches_executable_p
#endif
#define coff_SWAP_sym_in _bfd_xcoff64_swap_sym_in
#define coff_SWAP_sym_out _bfd_xcoff64_swap_sym_out
#define coff_SWAP_aux_in _bfd_xcoff64_swap_aux_in
#define coff_SWAP_aux_out _bfd_xcoff64_swap_aux_out


#include "coffcode.h"

/* For XCOFF64, the effective width of symndx changes depending on
   whether we are the first entry.  Sigh.  */
static void
_bfd_xcoff64_swap_lineno_in (abfd, ext1, in1)
     bfd *abfd;
     PTR ext1;
     PTR in1;
{
  LINENO *ext = (LINENO *) ext1;
  struct internal_lineno *in = (struct internal_lineno *) in1;

  in->l_lnno = H_GET_32 (abfd, (ext->l_lnno));
  if (in->l_lnno == 0)
    in->l_addr.l_symndx = H_GET_32 (abfd, ext->l_addr.l_symndx);
  else
    in->l_addr.l_paddr = H_GET_64 (abfd, ext->l_addr.l_paddr);
}

static unsigned int
_bfd_xcoff64_swap_lineno_out (abfd, inp, outp)
     bfd *abfd;
     PTR inp;
     PTR outp;
{
  struct internal_lineno *in = (struct internal_lineno *) inp;
  struct external_lineno *ext = (struct external_lineno *) outp;

  H_PUT_32 (abfd, in->l_addr.l_symndx, ext->l_addr.l_symndx);
  H_PUT_32 (abfd, in->l_lnno, (ext->l_lnno));

  if (in->l_lnno == 0)
    H_PUT_32 (abfd, in->l_addr.l_symndx, ext->l_addr.l_symndx);
  else
    H_PUT_64 (abfd, in->l_addr.l_paddr, ext->l_addr.l_paddr);

  return bfd_coff_linesz (abfd);
}

static void
_bfd_xcoff64_swap_sym_in (abfd, ext1, in1)
     bfd *abfd;
     PTR ext1;
     PTR in1;
{
  struct external_syment *ext = (struct external_syment *) ext1;
  struct internal_syment *in = (struct internal_syment *) in1;

  in->_n._n_n._n_zeroes = 0;
  in->_n._n_n._n_offset = H_GET_32 (abfd, ext->e_offset);
  in->n_value = H_GET_64 (abfd, ext->e_value);
  in->n_scnum = H_GET_16 (abfd, ext->e_scnum);
  in->n_type = H_GET_16 (abfd, ext->e_type);
  in->n_sclass = H_GET_8 (abfd, ext->e_sclass);
  in->n_numaux = H_GET_8 (abfd, ext->e_numaux);
}

static unsigned int
_bfd_xcoff64_swap_sym_out (abfd, inp, extp)
     bfd *abfd;
     PTR inp;
     PTR extp;
{
  struct internal_syment *in = (struct internal_syment *) inp;
  struct external_syment *ext = (struct external_syment *) extp;

  H_PUT_32 (abfd, in->_n._n_n._n_offset, ext->e_offset);
  H_PUT_64 (abfd, in->n_value, ext->e_value);
  H_PUT_16 (abfd, in->n_scnum, ext->e_scnum);
  H_PUT_16 (abfd, in->n_type, ext->e_type);
  H_PUT_8 (abfd, in->n_sclass, ext->e_sclass);
  H_PUT_8 (abfd, in->n_numaux, ext->e_numaux);
  return bfd_coff_symesz (abfd);
}

static void
_bfd_xcoff64_swap_aux_in (abfd, ext1, type, class, indx, numaux, in1)
     bfd *abfd;
     PTR ext1;
     int type;
     int class;
     int indx;
     int numaux;
     PTR in1;
{
  union external_auxent *ext = (union external_auxent *) ext1;
  union internal_auxent *in = (union internal_auxent *) in1;

  switch (class)
    {
    case C_FILE:
      if (ext->x_file.x_n.x_zeroes == 0)
	{
	  in->x_file.x_n.x_zeroes = 0;
	  in->x_file.x_n.x_offset = H_GET_32 (abfd, ext->x_file.x_n.x_offset);
	}
      else
	{
	  memcpy (in->x_file.x_fname, ext->x_file.x_fname, FILNMLEN);
	}
      goto end;

      /* RS/6000 "csect" auxents */
    case C_EXT:
    case C_HIDEXT:
      if (indx + 1 == numaux)
	{
	  bfd_signed_vma h = 0;
	  bfd_vma l = 0;

	  h = H_GET_S32 (abfd, ext->x_csect.x_scnlen_hi);
	  l = H_GET_32 (abfd, ext->x_csect.x_scnlen_lo);

	  in->x_csect.x_scnlen.l = h << 32 | (l & 0xffffffff);

	  in->x_csect.x_parmhash = H_GET_32 (abfd, ext->x_csect.x_parmhash);
	  in->x_csect.x_snhash = H_GET_16 (abfd, ext->x_csect.x_snhash);
	  /* We don't have to hack bitfields in x_smtyp because it's
	     defined by shifts-and-ands, which are equivalent on all
	     byte orders.  */
	  in->x_csect.x_smtyp = H_GET_8 (abfd, ext->x_csect.x_smtyp);
	  in->x_csect.x_smclas = H_GET_8 (abfd, ext->x_csect.x_smclas);
	  goto end;
	}
      break;

    case C_STAT:
    case C_LEAFSTAT:
    case C_HIDDEN:
      if (type == T_NULL)
	{
	  /* PE defines some extra fields; we zero them out for
	     safety.  */
	  in->x_scn.x_checksum = 0;
	  in->x_scn.x_associated = 0;
	  in->x_scn.x_comdat = 0;

	  goto end;
	}
      break;
    }

  if (class == C_BLOCK || class == C_FCN || ISFCN (type) || ISTAG (class))
    {
      in->x_sym.x_fcnary.x_fcn.x_lnnoptr
	= H_GET_64 (abfd, ext->x_sym.x_fcnary.x_fcn.x_lnnoptr);
      in->x_sym.x_fcnary.x_fcn.x_endndx.l
	= H_GET_32 (abfd, ext->x_sym.x_fcnary.x_fcn.x_endndx);
    }
  if (ISFCN (type))
    {
      in->x_sym.x_misc.x_fsize
	= H_GET_32 (abfd, ext->x_sym.x_fcnary.x_fcn.x_fsize);
    }
  else
    {
      in->x_sym.x_misc.x_lnsz.x_lnno
	= H_GET_32 (abfd, ext->x_sym.x_fcnary.x_lnsz.x_lnno);
      in->x_sym.x_misc.x_lnsz.x_size
	= H_GET_16 (abfd, ext->x_sym.x_fcnary.x_lnsz.x_size);
    }

 end: ;
}

static unsigned int
_bfd_xcoff64_swap_aux_out (abfd, inp, type, class, indx, numaux, extp)
     bfd *abfd;
     PTR inp;
     int type;
     int class;
     int indx ATTRIBUTE_UNUSED;
     int numaux ATTRIBUTE_UNUSED;
     PTR extp;
{
  union internal_auxent *in = (union internal_auxent *) inp;
  union external_auxent *ext = (union external_auxent *) extp;

  memset ((PTR) ext, 0, bfd_coff_auxesz (abfd));
  switch (class)
    {
    case C_FILE:
      if (ext->x_file.x_n.x_zeroes == 0)
	{
	  H_PUT_32 (abfd, 0, ext->x_file.x_n.x_zeroes);
	  H_PUT_32 (abfd, in->x_file.x_n.x_offset, ext->x_file.x_n.x_offset);
	}
      else
	{
	  memcpy (ext->x_file.x_fname, in->x_file.x_fname, FILNMLEN);
	}
      H_PUT_8 (abfd, _AUX_FILE, ext->x_auxtype.x_auxtype);
      goto end;

      /* RS/6000 "csect" auxents */
    case C_EXT:
    case C_HIDEXT:
      if (indx + 1 == numaux)
	{
	  bfd_vma temp;

	  temp = in->x_csect.x_scnlen.l & 0xffffffff;
	  H_PUT_32 (abfd, temp, ext->x_csect.x_scnlen_lo);
	  temp = in->x_csect.x_scnlen.l >> 32;
	  H_PUT_32 (abfd, temp, ext->x_csect.x_scnlen_hi);
	  H_PUT_32 (abfd, in->x_csect.x_parmhash, ext->x_csect.x_parmhash);
	  H_PUT_16 (abfd, in->x_csect.x_snhash, ext->x_csect.x_snhash);
	  /* We don't have to hack bitfields in x_smtyp because it's
	     defined by shifts-and-ands, which are equivalent on all
	     byte orders.  */
	  H_PUT_8 (abfd, in->x_csect.x_smtyp, ext->x_csect.x_smtyp);
	  H_PUT_8 (abfd, in->x_csect.x_smclas, ext->x_csect.x_smclas);
	  H_PUT_8 (abfd, _AUX_CSECT, ext->x_auxtype.x_auxtype);
	  goto end;
	}
      break;

    case C_STAT:
    case C_LEAFSTAT:
    case C_HIDDEN:
      if (type == T_NULL)
	{
	  goto end;
	}
      break;
    }

  if (class == C_BLOCK || class == C_FCN || ISFCN (type) || ISTAG (class))
    {
      H_PUT_64 (abfd, in->x_sym.x_fcnary.x_fcn.x_lnnoptr,
	       ext->x_sym.x_fcnary.x_fcn.x_lnnoptr);
      H_PUT_8 (abfd, _AUX_FCN,
	       ext->x_auxtype.x_auxtype);
      H_PUT_32 (abfd, in->x_sym.x_fcnary.x_fcn.x_endndx.l,
	       ext->x_sym.x_fcnary.x_fcn.x_endndx);
    }
  if (ISFCN (type))
    {
      H_PUT_32 (abfd, in->x_sym.x_misc.x_fsize,
	       ext->x_sym.x_fcnary.x_fcn.x_fsize);
    }
  else
    {
      H_PUT_32 (abfd, in->x_sym.x_misc.x_lnsz.x_lnno,
	       ext->x_sym.x_fcnary.x_lnsz.x_lnno);
      H_PUT_16 (abfd, in->x_sym.x_misc.x_lnsz.x_size,
	       ext->x_sym.x_fcnary.x_lnsz.x_size);
    }

 end:

  return bfd_coff_auxesz (abfd);
}

static boolean
_bfd_xcoff64_put_symbol_name (bfd *abfd, struct bfd_strtab_hash *strtab,
			      struct internal_syment *sym,
			      const char *name) {
  boolean hash;
  bfd_size_type indx;

  hash = true;

  if ((abfd->flags & BFD_TRADITIONAL_FORMAT) != 0)
    hash = false;

  indx = _bfd_stringtab_add (strtab, name, hash, false);

  if (indx == (bfd_size_type) -1)
    return false;

  sym->_n._n_n._n_zeroes = 0;
  sym->_n._n_n._n_offset = STRING_SIZE_SIZE + indx;

  return true;
}

static boolean
_bfd_xcoff64_put_ldsymbol_name (abfd, ldinfo, ldsym, name)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct xcoff_loader_info *ldinfo;
     struct internal_ldsym *ldsym;
     const char *name;
{

  size_t len;
  len = strlen (name);

  if (ldinfo->string_size + len + 3 > ldinfo->string_alc)
    {
      bfd_size_type newalc;
      bfd_byte *newstrings;

      newalc = ldinfo->string_alc * 2;
      if (newalc == 0)
	newalc = 32;
      while (ldinfo->string_size + len + 3 > newalc)
	newalc *= 2;

      newstrings = ((bfd_byte *)
		    bfd_realloc ((PTR) ldinfo->strings, newalc));
      if (newstrings == NULL)
	{
	  ldinfo->failed = true;
	  return false;
	}
      ldinfo->string_alc = newalc;
      ldinfo->strings = newstrings;
    }

  bfd_put_16 (ldinfo->output_bfd, (bfd_vma) (len + 1),
	      ldinfo->strings + ldinfo->string_size);
  strcpy (ldinfo->strings + ldinfo->string_size + 2, name);
  ldsym->_l._l_l._l_zeroes = 0;
  ldsym->_l._l_l._l_offset = ldinfo->string_size + 2;
  ldinfo->string_size += len + 3;

  return true;
}

/* Routines to swap information in the XCOFF .loader section.  If we
   ever need to write an XCOFF loader, this stuff will need to be
   moved to another file shared by the linker (which XCOFF calls the
   ``binder'') and the loader.  */

/* Swap in the ldhdr structure.  */

static void
xcoff64_swap_ldhdr_in (abfd, s, dst)
     bfd *abfd;
     const PTR s;
     struct internal_ldhdr *dst;
{
  const struct external_ldhdr *src = (const struct external_ldhdr *) s;

  dst->l_version = bfd_get_32 (abfd, src->l_version);
  dst->l_nsyms = bfd_get_32 (abfd, src->l_nsyms);
  dst->l_nreloc = bfd_get_32 (abfd, src->l_nreloc);
  dst->l_istlen = bfd_get_32 (abfd, src->l_istlen);
  dst->l_nimpid = bfd_get_32 (abfd, src->l_nimpid);
  dst->l_stlen = bfd_get_32 (abfd, src->l_stlen);
  dst->l_impoff = bfd_get_64 (abfd, src->l_impoff);
  dst->l_stoff = bfd_get_64 (abfd, src->l_stoff);
  dst->l_symoff = bfd_get_64 (abfd, src->l_symoff);
  dst->l_rldoff = bfd_get_64 (abfd, src->l_rldoff);
}

/* Swap out the ldhdr structure.  */

static void
xcoff64_swap_ldhdr_out (abfd, src, d)
     bfd *abfd;
     const struct internal_ldhdr *src;
     PTR d;
{
  struct external_ldhdr *dst = (struct external_ldhdr *) d;

  bfd_put_32 (abfd, (bfd_vma) src->l_version, dst->l_version);
  bfd_put_32 (abfd, src->l_nsyms, dst->l_nsyms);
  bfd_put_32 (abfd, src->l_nreloc, dst->l_nreloc);
  bfd_put_32 (abfd, src->l_istlen, dst->l_istlen);
  bfd_put_32 (abfd, src->l_nimpid, dst->l_nimpid);
  bfd_put_32 (abfd, src->l_stlen, dst->l_stlen);
  bfd_put_64 (abfd, src->l_impoff, dst->l_impoff);
  bfd_put_64 (abfd, src->l_stoff, dst->l_stoff);
  bfd_put_64 (abfd, src->l_symoff, dst->l_symoff);
  bfd_put_64 (abfd, src->l_rldoff, dst->l_rldoff);
}

/* Swap in the ldsym structure.  */

static void
xcoff64_swap_ldsym_in (abfd, s, dst)
     bfd *abfd;
     const PTR s;
     struct internal_ldsym *dst;
{
  const struct external_ldsym *src = (const struct external_ldsym *) s;
  /* XCOFF64 does not use l_zeroes like XCOFF32
     Set the internal l_zeroes to 0 so the common 32/64 code uses l_value
     as an offset into the loader symbol table.  */
  dst->_l._l_l._l_zeroes = 0;
  dst->_l._l_l._l_offset = bfd_get_32 (abfd, src->l_offset);
  dst->l_value = bfd_get_64 (abfd, src->l_value);
  dst->l_scnum = bfd_get_16 (abfd, src->l_scnum);
  dst->l_smtype = bfd_get_8 (abfd, src->l_smtype);
  dst->l_smclas = bfd_get_8 (abfd, src->l_smclas);
  dst->l_ifile = bfd_get_32 (abfd, src->l_ifile);
  dst->l_parm = bfd_get_32 (abfd, src->l_parm);
}

/* Swap out the ldsym structure.  */

static void
xcoff64_swap_ldsym_out (abfd, src, d)
     bfd *abfd;
     const struct internal_ldsym *src;
     PTR d;
{
  struct external_ldsym *dst = (struct external_ldsym *) d;

  bfd_put_64 (abfd, src->l_value, dst->l_value);
  bfd_put_32 (abfd, (bfd_vma) src->_l._l_l._l_offset, dst->l_offset);
  bfd_put_16 (abfd, (bfd_vma) src->l_scnum, dst->l_scnum);
  bfd_put_8 (abfd, src->l_smtype, dst->l_smtype);
  bfd_put_8 (abfd, src->l_smclas, dst->l_smclas);
  bfd_put_32 (abfd, src->l_ifile, dst->l_ifile);
  bfd_put_32 (abfd, src->l_parm, dst->l_parm);
}

/* Swap in the ldrel structure.  */

static void
xcoff64_swap_ldrel_in (abfd, s, dst)
     bfd *abfd;
     const PTR s;
     struct internal_ldrel *dst;
{
  const struct external_ldrel *src = (const struct external_ldrel *) s;

  dst->l_vaddr = bfd_get_64 (abfd, src->l_vaddr);
  dst->l_symndx = bfd_get_32 (abfd, src->l_symndx);
  dst->l_rtype = bfd_get_16 (abfd, src->l_rtype);
  dst->l_rsecnm = bfd_get_16 (abfd, src->l_rsecnm);
}

/* Swap out the ldrel structure.  */

static void
xcoff64_swap_ldrel_out (abfd, src, d)
     bfd *abfd;
     const struct internal_ldrel *src;
     PTR d;
{
  struct external_ldrel *dst = (struct external_ldrel *) d;

  bfd_put_64 (abfd, src->l_vaddr, dst->l_vaddr);
  bfd_put_16 (abfd, (bfd_vma) src->l_rtype, dst->l_rtype);
  bfd_put_16 (abfd, (bfd_vma) src->l_rsecnm, dst->l_rsecnm);
  bfd_put_32 (abfd, src->l_symndx, dst->l_symndx);
}

static boolean
xcoff64_write_object_contents (abfd)
     bfd * abfd;
{
  asection *current;
  boolean hasrelocs = false;
  boolean haslinno = false;
  file_ptr scn_base;
  file_ptr reloc_base;
  file_ptr lineno_base;
  file_ptr sym_base;
  unsigned long reloc_size = 0;
  unsigned long lnno_size = 0;
  boolean long_section_names;
  asection *text_sec = ((void *) 0);
  asection *data_sec = ((void *) 0);
  asection *bss_sec = ((void *) 0);
  struct internal_filehdr internal_f;
  struct internal_aouthdr internal_a;

  bfd_set_error (bfd_error_system_call);

  if (abfd->output_has_begun == false)
    {
      if (! bfd_coff_compute_section_file_positions (abfd))
	return false;
    }

  /* Work out the size of the reloc and linno areas */
  reloc_base = obj_relocbase (abfd);

  for (current = abfd->sections; current != NULL; current = current->next)
    reloc_size += current->reloc_count * bfd_coff_relsz (abfd);

  lineno_base = reloc_base + reloc_size;

  /* Make a pass through the symbol table to count line number entries and
     put them into the correct asections */
  lnno_size = coff_count_linenumbers (abfd) * bfd_coff_linesz (abfd);

  sym_base = lineno_base + lnno_size;

  /* Indicate in each section->line_filepos its actual file address */
  for (current = abfd->sections; current != NULL; current =  current->next)
    {
      if (current->lineno_count)
	{
	  current->line_filepos = lineno_base;
	  current->moving_line_filepos = lineno_base;
	  lineno_base += current->lineno_count * bfd_coff_linesz (abfd);
	}
      else
	{
	  current->line_filepos = 0;
	}

      if (current->reloc_count)
	{
	  current->rel_filepos = reloc_base;
	  reloc_base += current->reloc_count * bfd_coff_relsz (abfd);
	}
      else
	{
	  current->rel_filepos = 0;
	}
    }

  if ((abfd->flags & EXEC_P) != 0)
    {
      scn_base = bfd_coff_filhsz (abfd) + bfd_coff_aoutsz (abfd);
      internal_f.f_opthdr = bfd_coff_aoutsz (abfd);
    }
  else
    {
      scn_base = bfd_coff_filhsz (abfd);
      internal_f.f_opthdr = 0;
    }

  internal_f.f_nscns = 0;

  if (bfd_seek (abfd, scn_base, SEEK_SET) != 0)
    return false;

  long_section_names = false;
  for (current = abfd->sections; current != NULL; current = current->next)
    {
      struct internal_scnhdr section;
      struct external_scnhdr buff;
      bfd_size_type amount;

      internal_f.f_nscns++;

      strncpy (section.s_name, current->name, SCNNMLEN);

      section.s_vaddr = current->vma;
      section.s_paddr = current->lma;
      section.s_size =  current->_raw_size;

      /* If this section has no size or is unloadable then the scnptr
	 will be 0 too.  */
      if (current->_raw_size == 0
	  || (current->flags & (SEC_LOAD | SEC_HAS_CONTENTS)) == 0)
	{
	  section.s_scnptr = 0;
	}
      else
	{
	  section.s_scnptr = current->filepos;
	}

      section.s_relptr = current->rel_filepos;
      section.s_lnnoptr = current->line_filepos;
      section.s_nreloc = current->reloc_count;

      section.s_nlnno = current->lineno_count;
      if (current->reloc_count != 0)
	hasrelocs = true;
      if (current->lineno_count != 0)
	haslinno = true;

      section.s_flags = sec_to_styp_flags (current->name, current->flags);

      if (!strcmp (current->name, _TEXT))
	{
	  text_sec = current;
	}
      else if (!strcmp (current->name, _DATA))
	{
	  data_sec = current;
	}
      else if (!strcmp (current->name, _BSS))
	{
	  bss_sec = current;
	}

      amount = bfd_coff_scnhsz (abfd);
      if (bfd_coff_swap_scnhdr_out (abfd, &section, &buff) == 0
	  || bfd_bwrite ((PTR) (&buff), amount, abfd) != amount)
	return false;
    }

  internal_f.f_timdat = 0;

  internal_f.f_flags = 0;

  if (!hasrelocs)
    internal_f.f_flags |= F_RELFLG;
  if (!haslinno)
    internal_f.f_flags |= F_LNNO;
  if (abfd->flags & EXEC_P)
    internal_f.f_flags |= F_EXEC;

  /* FIXME: this is wrong for PPC_PE! */
  if (bfd_little_endian (abfd))
    internal_f.f_flags |= F_AR32WR;
  else
    internal_f.f_flags |= F_AR32W;

  if ((abfd->flags & DYNAMIC) != 0)
    internal_f.f_flags |= F_SHROBJ;
  if (bfd_get_section_by_name (abfd, _LOADER) != NULL)
    internal_f.f_flags |= F_DYNLOAD;

  memset (&internal_a, 0, sizeof internal_a);


  /* This can only be called from the xcoff64 backend so the magic #
     must be for xcoff64.  */
  internal_f.f_magic = 0757;

  internal_a.magic = (abfd->flags & D_PAGED) ? RS6K_AOUTHDR_ZMAGIC :
    (abfd->flags & WP_TEXT) ? RS6K_AOUTHDR_NMAGIC :
    RS6K_AOUTHDR_OMAGIC;

  /* FIXME: Does anybody ever set this to another value?  */
  internal_a.vstamp = 0;

  /* Now should write relocs, strings, syms */
  obj_sym_filepos (abfd) = sym_base;

  internal_f.f_symptr = 0;
  internal_f.f_nsyms = 0;

  /* If bfd_get_symcount (abfd) != 0, then we are not using the COFF
     backend linker, and obj_raw_syment_count is not valid until after
     coff_write_symbols is called.  */
  if (bfd_get_symcount (abfd) != 0)
    {
      int firstundef;

      if (!coff_renumber_symbols (abfd, &firstundef))
	return false;
      coff_mangle_symbols (abfd);
      if (! coff_write_symbols (abfd))
	return false;
      if (! coff_write_linenumbers (abfd))
	return false;
      if (! coff_write_relocs (abfd, firstundef))
	return false;

      internal_f.f_symptr = sym_base;
      internal_f.f_nsyms = bfd_get_symcount (abfd);
    }
  else if (obj_raw_syment_count (abfd) != 0)
    {
      internal_f.f_symptr = sym_base;

      /* AIX appears to require that F_RELFLG not be set if there are
	 local symbols but no relocations.  */
      internal_f.f_flags &=~ F_RELFLG;
    }
  else
    {
      internal_f.f_flags |= F_LSYMS;
    }

  if (text_sec)
    {
      internal_a.tsize = bfd_get_section_size_before_reloc (text_sec);
      internal_a.text_start = internal_a.tsize ? text_sec->vma : 0;
    }

  if (data_sec)
    {
      internal_a.dsize = bfd_get_section_size_before_reloc (data_sec);
      internal_a.data_start = internal_a.dsize ? data_sec->vma : 0;
    }

  if (bss_sec)
    {
      internal_a.bsize = bfd_get_section_size_before_reloc (bss_sec);
      if (internal_a.bsize && bss_sec->vma < internal_a.data_start)
	internal_a.data_start = bss_sec->vma;
    }

  internal_a.entry = bfd_get_start_address (abfd);
  internal_f.f_nsyms = obj_raw_syment_count (abfd);

  if (xcoff_data (abfd)->full_aouthdr)
    {
      bfd_vma toc;
      asection *loader_sec;

      internal_a.vstamp = 1;

      internal_a.o_snentry = xcoff_data (abfd)->snentry;
      if (internal_a.o_snentry == 0)
	internal_a.entry = (bfd_vma) -1;

      if (text_sec != NULL)
	{
	  internal_a.o_sntext = text_sec->target_index;
	  internal_a.o_algntext = bfd_get_section_alignment (abfd, text_sec);
	}
      else
	{
	  internal_a.o_sntext = 0;
	  internal_a.o_algntext = 0;
	}

      if (data_sec != NULL)
	{
	  internal_a.o_sndata = data_sec->target_index;
	  internal_a.o_algndata = bfd_get_section_alignment (abfd, data_sec);
	}
      else
	{
	  internal_a.o_sndata = 0;
	  internal_a.o_algndata = 0;
	}

      loader_sec = bfd_get_section_by_name (abfd, ".loader");
      if (loader_sec != NULL)
	internal_a.o_snloader = loader_sec->target_index;
      else
	internal_a.o_snloader = 0;
      if (bss_sec != NULL)
	internal_a.o_snbss = bss_sec->target_index;
      else
	internal_a.o_snbss = 0;

      toc = xcoff_data (abfd)->toc;
      internal_a.o_toc = toc;
      internal_a.o_sntoc = xcoff_data (abfd)->sntoc;

      internal_a.o_modtype = xcoff_data (abfd)->modtype;
      if (xcoff_data (abfd)->cputype != -1)
	internal_a.o_cputype = xcoff_data (abfd)->cputype;
      else
	{
	  switch (bfd_get_arch (abfd))
	    {
	    case bfd_arch_rs6000:
	      internal_a.o_cputype = 4;
	      break;
	    case bfd_arch_powerpc:
	      if (bfd_get_mach (abfd) == 0)
		internal_a.o_cputype = 3;
	      else
		internal_a.o_cputype = 1;
	      break;
	    default:
	      abort ();
	    }
	}
      internal_a.o_maxstack = xcoff_data (abfd)->maxstack;
      internal_a.o_maxdata = xcoff_data (abfd)->maxdata;
    }

  if (bfd_seek (abfd, (file_ptr) 0, 0) != 0)
    return false;

  {
    char * buff;
    bfd_size_type amount = bfd_coff_filhsz (abfd);

    buff = bfd_malloc (amount);
    if (buff == NULL)
      return false;

    bfd_coff_swap_filehdr_out (abfd, (PTR) &internal_f, (PTR) buff);
    amount = bfd_bwrite ((PTR) buff, amount, abfd);

    free (buff);

    if (amount != bfd_coff_filhsz (abfd))
      return false;
  }

  if (abfd->flags & EXEC_P)
    {
      char * buff;
      bfd_size_type amount = bfd_coff_aoutsz (abfd);

      buff = bfd_malloc (amount);
      if (buff == NULL)
	return false;

      bfd_coff_swap_aouthdr_out (abfd, (PTR) & internal_a, (PTR) buff);
      amount = bfd_bwrite ((PTR) buff, amount, abfd);

      free (buff);

      if (amount != bfd_coff_aoutsz (abfd))
	return false;
    }

  return true;
}

/* This is the relocation function for the RS/6000/POWER/PowerPC.
   This is currently the only processor which uses XCOFF; I hope that
   will never change.  */

static boolean
xcoff64_ppc_relocate_section (output_bfd, info, input_bfd,
			      input_section, contents, relocs, syms,
			      sections)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     struct internal_reloc *relocs;
     struct internal_syment *syms;
     asection **sections;
{
  struct internal_reloc *rel;
  struct internal_reloc *relend;

  rel = relocs;
  relend = rel + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      long symndx;
      struct xcoff_link_hash_entry *h;
      struct internal_syment *sym;
      bfd_vma addend;
      bfd_vma val;
      struct reloc_howto_struct howto;
      bfd_reloc_status_type rstat;

      /* Relocation type R_REF is a special relocation type which is
         merely used to prevent garbage collection from occurring for
         the csect including the symbol which it references.  */
      if (rel->r_type == R_REF)
	continue;

      symndx = rel->r_symndx;

      if (symndx == -1)
	{
	  h = NULL;
	  sym = NULL;
	  addend = 0;
	}
      else
	{
	  h = obj_xcoff_sym_hashes (input_bfd)[symndx];
	  sym = syms + symndx;
	  addend = - sym->n_value;
	}

      /* We build the howto information on the fly.  */

      howto.type = rel->r_type;
      howto.rightshift = 0;
      howto.size = 4;
      howto.bitsize = (rel->r_size & 0x3f) + 1;
      howto.pc_relative = false;
      howto.bitpos = 0;
      if ((rel->r_size & 0x80) != 0)
	howto.complain_on_overflow = complain_overflow_signed;
      else
	howto.complain_on_overflow = complain_overflow_bitfield;
      howto.special_function = NULL;
      howto.name = "internal";
      howto.partial_inplace = true;

      if (howto.bitsize == 64)
	{
	  howto.src_mask = howto.dst_mask = MINUS_ONE;
	}
      else if (howto.bitsize == 32)
	{
	  howto.src_mask = howto.dst_mask = 0xffffffff;
	}
      else
	{
	  howto.src_mask = howto.dst_mask = (1 << howto.bitsize) - 1;
	  if (howto.bitsize == 16)
	    howto.size = 1;
	}
      howto.pcrel_offset = false;

      val = 0;

      if (h == NULL)
	{
	  asection *sec;

	  if (symndx == -1)
	    {
	      sec = bfd_abs_section_ptr;
	      val = 0;
	    }
	  else
	    {
	      sec = sections[symndx];
	      /* Hack to make sure we use the right TOC anchor value
		 if this reloc is against the TOC anchor.  */
	      if (sec->name[3] == '0'
		  && strcmp (sec->name, ".tc0") == 0)
		val = xcoff_data (output_bfd)->toc;
	      else
		val = (sec->output_section->vma
		       + sec->output_offset
		       + sym->n_value
		       - sec->vma);
	    }

	}
      else
	{

	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      asection *sec;

	      sec = h->root.u.def.section;
	      val = (h->root.u.def.value
		     + sec->output_section->vma
		     + sec->output_offset);

	    }
	  else if (h->root.type == bfd_link_hash_common)
	    {
	      asection *sec;

	      sec = h->root.u.c.p->section;
	      val = (sec->output_section->vma
		     + sec->output_offset);
	    }
	  else if ((h->flags & XCOFF_DEF_DYNAMIC) != 0
		   || (h->flags & XCOFF_IMPORT) != 0)
	    {
	      /* Every symbol in a shared object is defined somewhere.  */
	      val = 0;
	    }
	  else if (! info->relocateable)
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd, input_section,
		      rel->r_vaddr - input_section->vma, true)))
		return false;

	      /* Don't try to process the reloc.  It can't help, and
		 it may generate another error.  */
	      continue;
	    }
	}

      /* I took the relocation type definitions from two documents:
	 the PowerPC AIX Version 4 Application Binary Interface, First
	 Edition (April 1992), and the PowerOpen ABI, Big-Endian
	 32-Bit Hardware Implementation (June 30, 1994).  Differences
	 between the documents are noted below.  */

      switch (rel->r_type)
	{
	case R_RTB:
	case R_RRTBI:
	case R_RRTBA:
	  /* These relocs are defined by the PowerPC ABI to be
	     relative branches which use half of the difference
	     between the symbol and the program counter.  I can't
	     quite figure out when this is useful.  These relocs are
	     not defined by the PowerOpen ABI.  */
	default:
	  (*_bfd_error_handler)
	    (_("%s: unsupported relocation type 0x%02x"),
	     bfd_archive_filename (input_bfd), (unsigned int) rel->r_type);
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	case R_POS:
	  /* Simple positive relocation.  */
	  break;
	case R_NEG:
	  /* Simple negative relocation.  */
	  val = - val;
	  break;
	case R_REL:
	  /* Simple PC relative relocation.  */
	  howto.pc_relative = true;
	  break;
	case R_TOC:
	  /* TOC relative relocation.  The value in the instruction in
	     the input file is the offset from the input file TOC to
	     the desired location.  We want the offset from the final
	     TOC to the desired location.  We have:
	     isym = iTOC + in
	     iinsn = in + o
	     osym = oTOC + on
	     oinsn = on + o
	     so we must change insn by on - in.
	  */
	case R_GL:
	  /* Global linkage relocation.  The value of this relocation
	     is the address of the entry in the TOC section.  */
	case R_TCL:
	  /* Local object TOC address.  I can't figure out the
	     difference between this and case R_GL.  */
	case R_TRL:
	  /* TOC relative relocation.  A TOC relative load instruction
	     which may be changed to a load address instruction.
	     FIXME: We don't currently implement this optimization.  */
	case R_TRLA:
	  /* TOC relative relocation.  This is a TOC relative load
	     address instruction which may be changed to a load
	     instruction.  FIXME: I don't know if this is the correct
	     implementation.  */
	  if (h != NULL && h->smclas != XMC_TD)
	    {
	      if (h->toc_section == NULL)
		{
		  (*_bfd_error_handler)
		    (_("%s: TOC reloc at 0x%x to symbol `%s' with no TOC entry"),
		     bfd_archive_filename (input_bfd), rel->r_vaddr,
		     h->root.root.string);
		  bfd_set_error (bfd_error_bad_value);
		  return false;
		}

	      BFD_ASSERT ((h->flags & XCOFF_SET_TOC) == 0);
	      val = (h->toc_section->output_section->vma
		     + h->toc_section->output_offset);
	    }

	  val = ((val - xcoff_data (output_bfd)->toc)
		 - (sym->n_value - xcoff_data (input_bfd)->toc));

	  addend = 0;
	  break;
	case R_BA:
	  /* Absolute branch.  We don't want to mess with the lower
	     two bits of the instruction.  */
	case R_CAI:
	  /* The PowerPC ABI defines this as an absolute call which
	     may be modified to become a relative call.  The PowerOpen
	     ABI does not define this relocation type.  */
	case R_RBA:
	  /* Absolute branch which may be modified to become a
	     relative branch.  */
	case R_RBAC:
	  /* The PowerPC ABI defines this as an absolute branch to a
	     fixed address which may be modified to an absolute branch
	     to a symbol.  The PowerOpen ABI does not define this
	     relocation type.  */
	case R_RBRC:
	  /* The PowerPC ABI defines this as an absolute branch to a
	     fixed address which may be modified to a relative branch.
	     The PowerOpen ABI does not define this relocation type.  */
	  howto.src_mask &= ~3;
	  howto.dst_mask = howto.src_mask;
	  break;
	case R_BR:
	  /* Relative branch.  We don't want to mess with the lower
	     two bits of the instruction.  */
	case R_CREL:
	  /* The PowerPC ABI defines this as a relative call which may
	     be modified to become an absolute call.  The PowerOpen
	     ABI does not define this relocation type.  */
	case R_RBR:
	  /* A relative branch which may be modified to become an
	     absolute branch.  FIXME: We don't implement this,
	     although we should for symbols of storage mapping class
	     XMC_XO.  */
	  howto.pc_relative = true;
	  howto.src_mask &= ~3;
	  howto.dst_mask = howto.src_mask;
	  howto.size = 2;
	  howto.complain_on_overflow = complain_overflow_bitfield;
	  break;
	case R_RL:
	  /* The PowerPC AIX ABI describes this as a load which may be
	     changed to a load address.  The PowerOpen ABI says this
	     is the same as case R_POS.  */
	  break;
	case R_RLA:
	  /* The PowerPC AIX ABI describes this as a load address
	     which may be changed to a load.  The PowerOpen ABI says
	     this is the same as R_POS.  */
	  break;
	}

      /* If we see an R_BR or R_RBR reloc which is jumping to global
         linkage code, and it is followed by an appropriate cror nop
         instruction, we replace the cror with ld r2,40(r1).  This
         restores the TOC after the glink code.  Contrariwise, if the
         call is followed by a ld r2,40(r1), but the call is not
         going to global linkage code, we can replace the load with a
         cror.  */
      if ((rel->r_type == R_BR || rel->r_type == R_RBR)
	  && h != NULL
	  && h->root.type == bfd_link_hash_defined
	  && (rel->r_vaddr - input_section->vma + 8
	      <= input_section->_cooked_size))
	{
	  bfd_byte *pnext;
	  unsigned long next;

	  pnext = contents + (rel->r_vaddr - input_section->vma) + 4;
	  next = bfd_get_32 (input_bfd, pnext);


	  /* The _ptrgl function is magic.  It is used by the AIX
	   * compiler to call a function through a pointer.
	   *
	   * special case XMC_GL, global linkage
	   */
	  if (h->smclas == XMC_GL
	      || strcmp (h->root.root.string, "._ptrgl") == 0)
	    {
	      if (next == 0x4def7b82 /* cror 15,15,15 */
		  || next == 0x4ffffb82 /* cror 31,31,31 */
		  || next == 0x60000000 /* ori r0,r0,0 */)
		bfd_put_32 (input_bfd, (bfd_vma) 0xe8410028 /* ld r2,40(r1) */,
			    pnext);
	    }
	  else
	    {
	      if (next == 0xe8410028 /* ld r2,40(r1) */)
		bfd_put_32 (input_bfd, (bfd_vma) 0x60000000 /* ori r0,r0,0 */,
			    pnext);
	    }
	}

      /* A PC relative reloc includes the section address.  */
      if (howto.pc_relative)
	addend += input_section->vma;

      rstat = _bfd_final_link_relocate (&howto, input_bfd, input_section,
					contents,
					rel->r_vaddr - input_section->vma,
					val, addend);

      switch (rstat)
	{
	default:
	  abort ();
	case bfd_reloc_ok:
	  break;
	case bfd_reloc_overflow:
	  {
	    const char *name;
	    char buf[SYMNMLEN + 1];
	    char howto_name[10];

	    if (symndx == -1)
	      name = "*ABS*";
	    else if (h != NULL)
	      name = h->root.root.string;
	    else
	      {
		name = _bfd_coff_internal_syment_name (input_bfd, sym, buf);
		if (name == NULL)
		  return false;
	      }
	    sprintf (howto_name, "0x%02x", rel->r_type);

	    if (! ((*info->callbacks->reloc_overflow)
		   (info, name, howto_name, (bfd_vma) 0, input_bfd,
		    input_section, rel->r_vaddr - input_section->vma)))
	      return false;
	  }
	}
    }

  return true;
}



/* The XCOFF reloc table.  Actually, XCOFF relocations specify the
   bitsize and whether they are signed or not, along with a
   conventional type.  This table is for the types, which are used for
   different algorithms for putting in the reloc.  Many of these
   relocs need special_function entries, which I have not written.  */


reloc_howto_type xcoff64_howto_table[] =
{
  /* Standard 64 bit relocation.  */
  HOWTO (0,			/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_POS",		/* name */
	 true,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit relocation, but store negative value.  */
  HOWTO (1,			/* type */
	 0,			/* rightshift */
	 -4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_NEG",		/* name */
	 true,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit PC relative relocation.  */
  HOWTO (2,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_REL",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit TOC relative relocation.  */
  HOWTO (3,			/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_TOC",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* I don't really know what this is.	*/
  HOWTO (4,			/* type */
	 1,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_RTB",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* External TOC relative symbol.  */
  HOWTO (5,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_GL",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Local TOC relative symbol.	 */
  HOWTO (6,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_TCL",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  EMPTY_HOWTO (7),

  /* Non modifiable absolute branch.  */
  HOWTO (8,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_BA",		/* name */
	 true,			/* partial_inplace */
	 0x3fffffc,		/* src_mask */
	 0x3fffffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  EMPTY_HOWTO (9),

  /* Non modifiable relative branch.  */
  HOWTO (0xa,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_BR",		/* name */
	 true,			/* partial_inplace */
	 0x3fffffc,		/* src_mask */
	 0x3fffffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  EMPTY_HOWTO (0xb),

  /* Indirect load.  */
  HOWTO (0xc,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_RL",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Load address.  */
  HOWTO (0xd,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_RLA",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  EMPTY_HOWTO (0xe),

  /* Non-relocating reference.	*/
  HOWTO (0xf,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_REF",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  EMPTY_HOWTO (0x10),
  EMPTY_HOWTO (0x11),

  /* TOC relative indirect load.  */
  HOWTO (0x12,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_TRL",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* TOC relative load address.	 */
  HOWTO (0x13,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_TRLA",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Modifiable relative branch.  */
  HOWTO (0x14,			/* type */
	 1,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_RRTBI",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Modifiable absolute branch.  */
  HOWTO (0x15,			/* type */
	 1,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_RRTBA",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Modifiable call absolute indirect.	 */
  HOWTO (0x16,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_CAI",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Modifiable call relative.	*/
  HOWTO (0x17,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_CREL",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Modifiable branch absolute.  */
  HOWTO (0x18,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_RBA",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Modifiable branch absolute.  */
  HOWTO (0x19,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_RBAC",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Modifiable branch relative.  */
  HOWTO (0x1a,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_RBR",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Modifiable branch absolute.  */
  HOWTO (0x1b,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_RBRC",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (0,			/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "R_POS",		/* name */
	 true,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false)			/* pcrel_offset */
};

void
xcoff64_rtype2howto (relent, internal)
     arelent *relent;
     struct internal_reloc *internal;
{
  relent->howto = xcoff64_howto_table + internal->r_type;

  /* Check for relocs we don't know of.  */
  if (internal->r_type
      >= sizeof (xcoff64_howto_table) / sizeof (xcoff64_howto_table[0]))
    abort ();
  if (internal->r_type != relent->howto->type)
    abort ();

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
xcoff64_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
    case BFD_RELOC_PPC_B26:
      return &xcoff64_howto_table[0xa];
    case BFD_RELOC_PPC_BA26:
      return &xcoff64_howto_table[8];
    case BFD_RELOC_PPC_TOC16:
      return &xcoff64_howto_table[3];
    case BFD_RELOC_32:
    case BFD_RELOC_CTOR:
      return &xcoff64_howto_table[0];
    case BFD_RELOC_64:
      return &xcoff64_howto_table[0x1c];
    default:
      return NULL;
    }
}



/* Read in the armap of an XCOFF archive.  */

static boolean
xcoff64_slurp_armap (abfd)
     bfd *abfd;
{
  file_ptr off;
  size_t namlen;
  bfd_size_type sz, amt;
  bfd_byte *contents, *cend;
  bfd_vma c, i;
  carsym *arsym;
  bfd_byte *p;
  file_ptr pos;

  /* This is for the new format.  */
  struct xcoff_ar_hdr_big hdr;

  if (xcoff_ardata (abfd) == NULL)
    {
      bfd_has_map (abfd) = false;
      return true;
    }

  off = strtol (xcoff_ardata_big (abfd)->symoff64, (char **) NULL, 10);
  if (off == 0)
    {
      bfd_has_map (abfd) = false;
      return true;
    }

  if (bfd_seek (abfd, off, SEEK_SET) != 0)
    return false;

  /* The symbol table starts with a normal archive header.  */
  if (bfd_bread ((PTR) &hdr, (bfd_size_type) SIZEOF_AR_HDR_BIG, abfd)
      != SIZEOF_AR_HDR_BIG)
    return false;

  /* Skip the name (normally empty).  */
  namlen = strtol (hdr.namlen, (char **) NULL, 10);
  pos = ((namlen + 1) & ~(size_t) 1) + SXCOFFARFMAG;
  if (bfd_seek (abfd, pos, SEEK_CUR) != 0)
    return false;

  /* XXX This actually has to be a call to strtoll (at least on 32-bit
     machines) since the field width is 20 and there numbers with more
     than 32 bits can be represented.  */
  sz = strtol (hdr.size, (char **) NULL, 10);

  /* Read in the entire symbol table.  */
  contents = (bfd_byte *) bfd_alloc (abfd, sz);
  if (contents == NULL)
    return false;
  if (bfd_bread ((PTR) contents, sz, abfd) != sz)
    return false;

  /* The symbol table starts with an eight byte count.  */
  c = H_GET_64 (abfd, contents);

  if (c * 8 >= sz)
    {
      bfd_set_error (bfd_error_bad_value);
      return false;
    }
  amt = c;
  amt *= sizeof (carsym);
  bfd_ardata (abfd)->symdefs = (carsym *) bfd_alloc (abfd, amt);
  if (bfd_ardata (abfd)->symdefs == NULL)
    return false;

  /* After the count comes a list of eight byte file offsets.  */
  for (i = 0, arsym = bfd_ardata (abfd)->symdefs, p = contents + 8;
       i < c;
       ++i, ++arsym, p += 8)
    arsym->file_offset = H_GET_64 (abfd, p);

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


/* See if this is an NEW XCOFF archive.  */

static const bfd_target *
xcoff64_archive_p (abfd)
     bfd *abfd;
{
  char magic[SXCOFFARMAG];
  /* This is the new format.  */
  struct xcoff_ar_file_hdr_big hdr;
  bfd_size_type amt = SXCOFFARMAG;

  if (bfd_bread ((PTR) magic, amt, abfd) != amt)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (strncmp (magic, XCOFFARMAGBIG, SXCOFFARMAG) != 0)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* We are setting bfd_ardata(abfd) here, but since bfd_ardata
     involves a cast, we can't do it as the left operand of
     assignment.  */
  amt = sizeof (struct artdata);
  abfd->tdata.aout_ar_data = (struct artdata *) bfd_zalloc (abfd, amt);

  if (bfd_ardata (abfd) == (struct artdata *) NULL)
    return NULL;

  bfd_ardata (abfd)->cache = NULL;
  bfd_ardata (abfd)->archive_head = NULL;
  bfd_ardata (abfd)->symdefs = NULL;
  bfd_ardata (abfd)->extended_names = NULL;

  /* Copy over the magic string.  */
  memcpy (hdr.magic, magic, SXCOFFARMAG);

  /* Now read the rest of the file header.  */
  if (bfd_bread ((PTR) &hdr.memoff,
		(bfd_size_type) (SIZEOF_AR_FILE_HDR_BIG - SXCOFFARMAG),
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

  amt = SIZEOF_AR_FILE_HDR_BIG;
  bfd_ardata (abfd)->tdata = bfd_zalloc (abfd, amt);
  if (bfd_ardata (abfd)->tdata == NULL)
    return NULL;

  memcpy (bfd_ardata (abfd)->tdata, &hdr, SIZEOF_AR_FILE_HDR_BIG);

  if (! xcoff64_slurp_armap (abfd))
    {
      bfd_release (abfd, bfd_ardata (abfd));
      abfd->tdata.aout_ar_data = (struct artdata *) NULL;
      return NULL;
    }

  return abfd->xvec;
}


/* Open the next element in an XCOFF archive.  */

static bfd *
xcoff64_openr_next_archived_file (archive, last_file)
     bfd *archive;
     bfd *last_file;
{
  file_ptr filestart;

  if ((xcoff_ardata (archive) == NULL)
      || ! xcoff_big_format_p (archive))
    {
      bfd_set_error (bfd_error_invalid_operation);
      return NULL;
    }

  if (last_file == NULL)
    {
      filestart = bfd_ardata (archive)->first_file_filepos;
    }
  else
    {
      /* XXX These actually have to be a calls to strtoll (at least
	 on 32-bit machines) since the fields's width is 20 and
	 there numbers with more than 32 bits can be represented.  */
      filestart = strtol (arch_xhdr_big (last_file)->nextoff, (char **) NULL,
			  10);
    }
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

  return _bfd_get_elt_at_filepos (archive, filestart);
}

/* We can't use the usual coff_sizeof_headers routine, because AIX
   always uses an a.out header.  */

/*ARGSUSED*/
static int
xcoff64_sizeof_headers (abfd, reloc)
     bfd *abfd;
     boolean reloc ATTRIBUTE_UNUSED;
{
  int size;

  size = bfd_coff_filhsz (abfd);

  /* Don't think the small aout header can be used since some of the
     old elements have been reordered past the end of the old coff
     small aout size.  */

  if (xcoff_data (abfd)->full_aouthdr)
    size += bfd_coff_aoutsz (abfd);

  size += abfd->section_count * bfd_coff_scnhsz (abfd);
  return size;
}



static asection *
xcoff64_create_csect_from_smclas (abfd, aux, symbol_name)
     bfd *abfd;
     union internal_auxent *aux;
     const char *symbol_name;
{
  asection *return_value = NULL;

  /* Changes from 32 :
     .sv == 8, is only for 32 bit programs
     .ti == 12 and .tb == 13 are now reserved.  */
  static const char *names[19] =
  {
    ".pr", ".ro", ".db", ".tc", ".ua", ".rw", ".gl", ".xo",
    NULL, ".bs", ".ds", ".uc", NULL,  NULL,  NULL,  ".tc0",
    ".td", ".sv64", ".sv3264"
  };

  if ((19 >= aux->x_csect.x_smclas)
      && (NULL != names[aux->x_csect.x_smclas]))
    {

      return_value = bfd_make_section_anyway
	(abfd, names[aux->x_csect.x_smclas]);

    }
  else
    {
      (*_bfd_error_handler)
	(_("%s: symbol `%s' has unrecognized smclas %d"),
	 bfd_archive_filename (abfd), symbol_name, aux->x_csect.x_smclas);
      bfd_set_error (bfd_error_bad_value);
    }

  return return_value;
}

static boolean
xcoff64_is_lineno_count_overflow (abfd, value)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_vma value ATTRIBUTE_UNUSED;
{
  return false;
}

static boolean
xcoff64_is_reloc_count_overflow (abfd, value)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_vma value ATTRIBUTE_UNUSED;
{
  return false;
}

static bfd_vma
xcoff64_loader_symbol_offset (abfd, ldhdr)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct internal_ldhdr *ldhdr;
{
  return (ldhdr->l_symoff);
}

static bfd_vma
xcoff64_loader_reloc_offset (abfd, ldhdr)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct internal_ldhdr *ldhdr;
{
  return (ldhdr->l_rldoff);
}

static boolean 
xcoff64_generate_rtinit  (abfd, init, fini)
     bfd *abfd;
     const char *init;
     const char *fini;
{
  bfd_byte filehdr_ext[FILHSZ];
  bfd_byte scnhdr_ext[SCNHSZ];
  bfd_byte syment_ext[SYMESZ * 8];
  bfd_byte reloc_ext[RELSZ * 2];
  bfd_byte *data_buffer;
  bfd_size_type data_buffer_size;
  bfd_byte *string_table, *st_tmp;
  bfd_size_type string_table_size;
  bfd_vma val;
  size_t initsz, finisz;
  struct internal_filehdr filehdr;
  struct internal_scnhdr scnhdr;
  struct internal_syment syment;
  union internal_auxent auxent;
  struct internal_reloc reloc;
  
  char *data_name = ".data";
  char *rtinit_name = "__rtinit";
  
  if (! bfd_xcoff_rtinit_size (abfd) 
      || (init == NULL && fini == NULL))
    return false;

  initsz = (init == NULL ? 0 : 1 + strlen (init));
  finisz = (fini == NULL ? 0 : 1 + strlen (fini));

  /* file header */
  memset (filehdr_ext, 0, FILHSZ);
  memset (&filehdr, 0, sizeof (struct internal_filehdr));
  filehdr.f_magic = bfd_xcoff_magic_number (abfd);
  filehdr.f_nscns = 1; 
  filehdr.f_timdat = 0;
  filehdr.f_nsyms = 0;  /* at least 6, no more than 8 */
  filehdr.f_symptr = 0; /* set below */
  filehdr.f_opthdr = 0;
  filehdr.f_flags = 0;

  /* section header */
  memset (scnhdr_ext, 0, SCNHSZ);
  memset (&scnhdr, 0, sizeof (struct internal_scnhdr));
  memcpy (scnhdr.s_name, data_name, strlen (data_name));
  scnhdr.s_paddr = 0;
  scnhdr.s_vaddr = 0;
  scnhdr.s_size = 0;    /* set below */
  scnhdr.s_scnptr = FILHSZ + SCNHSZ;
  scnhdr.s_relptr = 0;  /* set below */
  scnhdr.s_lnnoptr = 0;
  scnhdr.s_nreloc = 0;  /* either 1 or 2 */
  scnhdr.s_nlnno = 0;
  scnhdr.s_flags = STYP_DATA;

  /* .data 
     0x0000           0x00000000 : rtl
     0x0004           0x00000000 :
     0x0008           0x00000018 : offset to init, or 0
     0x000C           0x00000038 : offset to fini, or 0
     0x0010           0x00000010 : size of descriptor 
     0x0014           0x00000000 : pad
     0x0018           0x00000000 : init, needs a reloc
     0x001C           0x00000000 :
     0x0020           0x00000058 : offset to init name
     0x0024           0x00000000 : flags, padded to a word
     0x0028           0x00000000 : empty init
     0x002C           0x00000000 :
     0x0030           0x00000000 : 
     0x0034           0x00000000 : 
     0x0038           0x00000000 : fini, needs a reloc
     0x003C           0x00000000 :
     0x0040           0x00000??? : offset to fini name
     0x0044           0x00000000 : flags, padded to a word
     0x0048           0x00000000 : empty fini
     0x004C           0x00000000 :
     0x0050           0x00000000 : 
     0x0054           0x00000000 : 
     0x0058           init name
     0x0058 + initsz  fini name */

  data_buffer_size = 0x0058 + initsz + finisz;
  data_buffer_size += (data_buffer_size & 7) ? 8 - (data_buffer_size & 7) : 0;
  data_buffer = NULL;
  data_buffer = (bfd_byte *)bfd_malloc (data_buffer_size);
  if (data_buffer == NULL)
    return false;
  
  memset (data_buffer, 0, data_buffer_size);

  if (initsz) 
    {
      val = 0x18;
      bfd_put_32 (abfd, val, &data_buffer[0x08]);
      val = 0x58;
      bfd_put_32 (abfd, val, &data_buffer[0x20]);
      memcpy (&data_buffer[val], init, initsz);
    }

  if (finisz) 
    {
      val = 0x38;
      bfd_put_32 (abfd, val, &data_buffer[0x0C]);
      val = 0x58 + initsz;
      bfd_put_32 (abfd, val, &data_buffer[0x40]);
      memcpy (&data_buffer[val], fini, finisz);
    }

  val = 0x10;
  bfd_put_32 (abfd, val, &data_buffer[0x10]);
  scnhdr.s_size = data_buffer_size;

  /* string table */
  string_table_size = 4;
  string_table_size += strlen (data_name) + 1;
  string_table_size += strlen (rtinit_name) + 1;
  string_table_size += initsz;
  string_table_size += finisz;

  string_table = (bfd_byte *)bfd_malloc (string_table_size);
  memset (string_table, 0, string_table_size);
  val = string_table_size;
  bfd_put_32 (abfd, val, &string_table[0]);
  st_tmp = string_table + 4;
  
  /* symbols 
     0. .data csect
     2. __rtinit
     4. init function 
     6. fini function */
  memset (syment_ext, 0, 8 * SYMESZ);
  memset (reloc_ext, 0, 2 * RELSZ);

  /* .data csect */
  memset (&syment, 0, sizeof (struct internal_syment));
  memset (&auxent, 0, sizeof (union internal_auxent));

  syment._n._n_n._n_offset = st_tmp - string_table;
  memcpy (st_tmp, data_name, strlen (data_name));
  st_tmp += strlen (data_name) + 1;

  syment.n_scnum = 1;
  syment.n_sclass = C_HIDEXT;
  syment.n_numaux = 1;
  auxent.x_csect.x_scnlen.l = data_buffer_size;
  auxent.x_csect.x_smtyp = 3 << 3 | XTY_SD;
  auxent.x_csect.x_smclas = XMC_RW;
  bfd_coff_swap_sym_out (abfd, &syment, 
			 &syment_ext[filehdr.f_nsyms * SYMESZ]);
  bfd_coff_swap_aux_out (abfd, &auxent, syment.n_type, syment.n_sclass, 0, 
			 syment.n_numaux, 
			 &syment_ext[(filehdr.f_nsyms + 1) * SYMESZ]);
  filehdr.f_nsyms += 2;

  /* __rtinit */
  memset (&syment, 0, sizeof (struct internal_syment));
  memset (&auxent, 0, sizeof (union internal_auxent));
  syment._n._n_n._n_offset = st_tmp - string_table;
  memcpy (st_tmp, rtinit_name, strlen (rtinit_name));
  st_tmp += strlen (rtinit_name) + 1;
  
  syment.n_scnum = 1;
  syment.n_sclass = C_EXT;
  syment.n_numaux = 1;
  auxent.x_csect.x_smtyp = XTY_LD;
  auxent.x_csect.x_smclas = XMC_RW;
  bfd_coff_swap_sym_out (abfd, &syment, 
			 &syment_ext[filehdr.f_nsyms * SYMESZ]);
  bfd_coff_swap_aux_out (abfd, &auxent, syment.n_type, syment.n_sclass, 0, 
			 syment.n_numaux, 
			 &syment_ext[(filehdr.f_nsyms + 1) * SYMESZ]);
  filehdr.f_nsyms += 2;

  /* init */
  if (initsz) 
    {
      memset (&syment, 0, sizeof (struct internal_syment));
      memset (&auxent, 0, sizeof (union internal_auxent));

      syment._n._n_n._n_offset = st_tmp - string_table;
      memcpy (st_tmp, init, initsz);
      st_tmp += initsz;

      syment.n_sclass = C_EXT;
      syment.n_numaux = 1;
      bfd_coff_swap_sym_out (abfd, &syment, 
			     &syment_ext[filehdr.f_nsyms * SYMESZ]);
      bfd_coff_swap_aux_out (abfd, &auxent, syment.n_type, syment.n_sclass, 0, 
			     syment.n_numaux, 
			     &syment_ext[(filehdr.f_nsyms + 1) * SYMESZ]);
      /* reloc */
      memset (&reloc, 0, sizeof (struct internal_reloc));
      reloc.r_vaddr = 0x0018;
      reloc.r_symndx = filehdr.f_nsyms;
      reloc.r_type = R_POS;
      reloc.r_size = 63;
      bfd_coff_swap_reloc_out (abfd, &reloc, &reloc_ext[0]);

      filehdr.f_nsyms += 2;
      scnhdr.s_nreloc += 1;
    }

  /* finit */
  if (finisz) 
    {
      memset (&syment, 0, sizeof (struct internal_syment));
      memset (&auxent, 0, sizeof (union internal_auxent));

      syment._n._n_n._n_offset = st_tmp - string_table;
      memcpy (st_tmp, fini, finisz);
      st_tmp += finisz;

      syment.n_sclass = C_EXT;
      syment.n_numaux = 1;
      bfd_coff_swap_sym_out (abfd, &syment, 
			     &syment_ext[filehdr.f_nsyms * SYMESZ]);
      bfd_coff_swap_aux_out (abfd, &auxent, syment.n_type, syment.n_sclass, 0, 
			     syment.n_numaux, 
			     &syment_ext[(filehdr.f_nsyms + 1) * SYMESZ]);

      /* reloc */
      memset (&reloc, 0, sizeof (struct internal_reloc));
      reloc.r_vaddr = 0x0038;
      reloc.r_symndx = filehdr.f_nsyms;
      reloc.r_type = R_POS;
      reloc.r_size = 63;
      bfd_coff_swap_reloc_out (abfd, &reloc, 
			       &reloc_ext[scnhdr.s_nreloc * RELSZ]);

      filehdr.f_nsyms += 2;
      scnhdr.s_nreloc += 1;
    }

  scnhdr.s_relptr = scnhdr.s_scnptr + data_buffer_size;
  filehdr.f_symptr = scnhdr.s_relptr + scnhdr.s_nreloc * RELSZ;

  bfd_coff_swap_filehdr_out (abfd, &filehdr, filehdr_ext);
  bfd_bwrite (filehdr_ext, FILHSZ, abfd);
  bfd_coff_swap_scnhdr_out (abfd, &scnhdr, scnhdr_ext);
  bfd_bwrite (scnhdr_ext, SCNHSZ, abfd);
  bfd_bwrite (data_buffer, data_buffer_size, abfd);
  bfd_bwrite (reloc_ext, scnhdr.s_nreloc * RELSZ, abfd);
  bfd_bwrite (syment_ext, filehdr.f_nsyms * SYMESZ, abfd);
  bfd_bwrite (string_table, string_table_size, abfd);

  free (data_buffer);
  data_buffer = NULL;

  return true;
}

/* The typical dynamic reloc.  */

static reloc_howto_type xcoff64_dynamic_reloc =
HOWTO (0,			/* type */
       0,			/* rightshift */
       4,			/* size (0 = byte, 1 = short, 2 = long) */
       64,			/* bitsize */
       false,			/* pc_relative */
       0,			/* bitpos */
       complain_overflow_bitfield, /* complain_on_overflow */
       0,			/* special_function */
       "R_POS",			/* name */
       true,			/* partial_inplace */
       MINUS_ONE,		/* src_mask */
       MINUS_ONE,		/* dst_mask */
       false);			/* pcrel_offset */

static unsigned long xcoff64_glink_code[10] =
{
  0xe9820000,   /* ld r12,0(r2) */
  0xf8410028,   /* std r2,40(r1) */
  0xe80c0000,   /* ld r0,0(r12) */
  0xe84c0008,   /* ld r0,8(r12) */
  0x7c0903a6,   /* mtctr r0 */
  0x4e800420,   /* bctr */
  0x00000000,   /* start of traceback table */
  0x000ca000,   /* traceback table */
  0x00000000,   /* traceback table */
  0x00000018,   /* ??? */
};

static const struct xcoff_backend_data_rec bfd_xcoff_backend_data =
{
  { /* COFF backend, defined in libcoff.h */
    _bfd_xcoff64_swap_aux_in,         /* _bfd_coff_swap_aux_in */
    _bfd_xcoff64_swap_sym_in,         /* _bfd_coff_swap_sym_in */
    _bfd_xcoff64_swap_lineno_in,      /* _bfd_coff_swap_lineno_in */
    _bfd_xcoff64_swap_aux_out,        /* _bfd_swap_aux_out */
    _bfd_xcoff64_swap_sym_out,        /* _bfd_swap_sym_out */
    _bfd_xcoff64_swap_lineno_out,     /* _bfd_swap_lineno_out */
    coff_swap_reloc_out,              /* _bfd_swap_reloc_out */
    coff_swap_filehdr_out,            /* _bfd_swap_filehdr_out */
    coff_swap_aouthdr_out,            /* _bfd_swap_aouthdr_out */
    coff_swap_scnhdr_out,             /* _bfd_swap_scnhdr_out */
    FILHSZ,                           /* _bfd_filhsz */
    AOUTSZ,                           /* _bfd_aoutsz */
    SCNHSZ,                           /* _bfd_scnhsz */
    SYMESZ,                           /* _bfd_symesz */
    AUXESZ,                           /* _bfd_auxesz */
    RELSZ,                            /* _bfd_relsz */
    LINESZ,                           /* _bfd_linesz */
    FILNMLEN,                         /* _bfd_filnmlen */
    true,                             /* _bfd_coff_long_filenames */
    false,                            /* _bfd_coff_long_section_names */
    (3),                        /* _bfd_coff_default_section_alignment_power */
    true,                            /* _bfd_coff_force_symnames_in_strings */
    4,                               /* _bfd_coff_debug_string_prefix_length */
    coff_swap_filehdr_in,             /* _bfd_coff_swap_filehdr_in */
    coff_swap_aouthdr_in,             /* _bfd_swap_aouthdr_in */
    coff_swap_scnhdr_in,              /* _bfd_swap_scnhdr_in */
    coff_swap_reloc_in,               /* _bfd_reloc_in */
    coff_bad_format_hook,             /* _bfd_bad_format_hook */
    coff_set_arch_mach_hook,          /* _bfd_set_arch_mach_hook */
    coff_mkobject_hook,               /* _bfd_mkobject_hook */
    styp_to_sec_flags,                /* _bfd_syp_to_sec_flags */
    coff_set_alignment_hook,          /* _bfd_set_alignment_hook */
    coff_slurp_symbol_table,     /* _bfd_coff_slurp_symbol_table */
    symname_in_debug_hook,            /* _coff_symname_in_debug_hook */
    coff_pointerize_aux_hook,         /* _bfd_coff_pointerize_aux_hook */
    coff_print_aux,                   /* bfd_coff_print_aux */
    dummy_reloc16_extra_cases,        /* _bfd_coff_reloc16_extra_cases */
    dummy_reloc16_estimate,           /* _bfd_coff_reloc16_estimate */
    NULL,                             /* bfd_coff_sym_is_global */
    /* _bfd_coff_compute_section_file_positions */
    coff_compute_section_file_positions,
    NULL ,                            /* _bfd_coff_start_final_link */
    xcoff64_ppc_relocate_section,  /* _bfd_coff_relocate_section */
    coff_rtype_to_howto,              /* _bfd_coff_rtype_to_howto */
    NULL ,                            /* _bfd_coff_addust_symndx */
    _bfd_generic_link_add_one_symbol, /* _bfd_coff_add_one_symbol */
    coff_link_output_has_begun,       /* _bfd_coff_link_output_has_begun */
    coff_final_link_postscript        /* _bfd_coff_final_link_postscript */
  },

  0x01EF,                             /* magic number */
  bfd_arch_powerpc,                   /* architecture */
  bfd_mach_ppc_620,                   /* machine */

  /* function pointers to xcoff specific swap routines */
  xcoff64_swap_ldhdr_in,                /* _xcoff_swap_ldhdr_in */
  xcoff64_swap_ldhdr_out,               /* _xcoff_swap_ldhdr_out */
  xcoff64_swap_ldsym_in,                /* _xcoff_swap_ldsym_in */
  xcoff64_swap_ldsym_out,               /* _xcoff_swap_ldsym_out */
  xcoff64_swap_ldrel_in,                /* _xcoff_swap_ldrel_in */
  xcoff64_swap_ldrel_out,               /* _xcoff_swap_ldrel_out */

  /* sizes */
  LDHDRSZ,                              /* _xcoff_ldhdrsz */
  LDSYMSZ,                              /* _xcoff_ldsymsz */
  LDRELSZ,                              /* _xcoff_ldrelsz */
  24,                                   /* _xcoff_function_descriptor_size */
  0,                                    /* _xcoff_small_aout_header_size */
  /* versions */
  2,                                    /* _xcoff_ldhdr_version */

  /* xcoff vs xcoff64 putting symbol names */
  _bfd_xcoff64_put_symbol_name,          /* _xcoff_put_symbol_name */
  _bfd_xcoff64_put_ldsymbol_name,        /* _xcoff_put_ldsymbol_name */

  /* dynamic reloc howto */
  &xcoff64_dynamic_reloc,

  xcoff64_create_csect_from_smclas,

  /* lineno and reloc count overflow */
  xcoff64_is_lineno_count_overflow,
  xcoff64_is_reloc_count_overflow,

  xcoff64_loader_symbol_offset,
  xcoff64_loader_reloc_offset,

  /* glink */
  &xcoff64_glink_code[0],
  40,           /* _xcoff_glink_size */

  /* rtinit */
  88,           /* _xcoff_rtinit_size */
  xcoff64_generate_rtinit,  /* _xcoff_generate_rtinit */

};

/* The transfer vector that leads the outside world to all of the above. */
const bfd_target rs6000coff64_vec =
{
  "aixcoff64-rs6000",
  bfd_target_xcoff_flavour,
  BFD_ENDIAN_BIG,		/* data byte order is big */
  BFD_ENDIAN_BIG,		/* header byte order is big */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG | DYNAMIC |
   HAS_SYMS | HAS_LOCALS | WP_TEXT),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
  0,				/* leading char */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen??? FIXMEmgo */

  /* data */
  bfd_getb64,         /* bfd_getx64 */
  bfd_getb_signed_64, /* bfd_getx_signed_64 */
  bfd_putb64,         /* bfd_putx64 */
  bfd_getb32,         /* bfd_getx32 */
  bfd_getb_signed_32, /* bfd_getx_signed_32 */
  bfd_putb32,         /* bfd_putx32 */
  bfd_getb16,         /* bfd_getx16 */
  bfd_getb_signed_16, /* bfd_getx_signed_16 */
  bfd_putb16,         /* bfd_putx16 */

  /* hdrs */
  bfd_getb64,         /* bfd_h_getx64 */
  bfd_getb_signed_64, /* bfd_h_getx_signed_64 */
  bfd_putb64,         /* bfd_h_putx64 */
  bfd_getb32,         /* bfd_h_getx32 */
  bfd_getb_signed_32, /* bfd_h_getx_signed_32 */
  bfd_putb32,         /* bfd_h_putx32 */
  bfd_getb16,         /* bfd_h_getx16 */
  bfd_getb_signed_16, /* bfd_h_getx_signed_16 */
  bfd_putb16,         /* bfd_h_putx16 */

  { /* bfd_check_format */
    _bfd_dummy_target,
    coff_object_p,
    xcoff64_archive_p,
    CORE_FILE_P
  },

  { /* bfd_set_format */
    bfd_false,
    coff_mkobject,
    _bfd_generic_mkarchive,
    bfd_false
  },

  {/* bfd_write_contents */
    bfd_false,
    xcoff64_write_object_contents,
    _bfd_xcoff_write_archive_contents,
    bfd_false
  },

  /* Generic */
  bfd_true,                          /* _close_and_cleanup */
  bfd_true,                          /* _bfd_free_cached_info */
  coff_new_section_hook,             /* _new_section_hook */
  _bfd_generic_get_section_contents, /* _bfd_get_section_contents */
  /* _bfd_get_section_contents_in_window */
  _bfd_generic_get_section_contents_in_window,

  /* Copy */
  _bfd_xcoff_copy_private_bfd_data, /* _bfd_copy_private_bfd */
  /* _bfd_merge_private_bfd_data */
  ((boolean (*) (bfd *, bfd *)) bfd_true),
  /* _bfd_copy_pivate_section_data */
  ((boolean (*) (bfd *, asection *, bfd *, asection *)) bfd_true),
  /* _bfd_copy_private_symbol_data */
  ((boolean (*) (bfd *, asymbol *, bfd *, asymbol *)) bfd_true),
  ((boolean (*) (bfd *, flagword)) bfd_true), /* _bfd_set_private_flags */
  ((boolean (*) (bfd *, void * )) bfd_true),  /* _bfd_print_private_bfd_data */

  /* Core */
  coff_core_file_failing_command,    /* _core_file_failing_command */
  coff_core_file_failing_signal,     /* _core_file_failing_signal */
  /* _core_file_matches_executable_p */
  coff_core_file_matches_executable_p,

  /* Archive */
  xcoff64_slurp_armap,                  /* _slurp_armap */
  /* XCOFF archives do not have
     anything which corresponds to
     an extended name table.  */
  bfd_false,                               /* _slurp_extended_name_table */
  /* _construct_extended_name_table */
  ((boolean (*) (bfd *, char **, bfd_size_type *, const char **)) bfd_false),
  bfd_dont_truncate_arname,                /* _truncate_arname */
  _bfd_xcoff_write_armap,                  /* _write_armap */
  _bfd_xcoff_read_ar_hdr,                  /* _read_ar_hdr */
  xcoff64_openr_next_archived_file,        /* _openr_next_archived_file */
  _bfd_generic_get_elt_at_index,           /* _get_elt_at_index */
  _bfd_xcoff_stat_arch_elt,                /* _generic_stat_arch_elt */
  /* XCOFF archives do not have
     a timestamp.  */
  bfd_true,                                /* _update_armap_timestamp */

  /* Symbols */
  coff_get_symtab_upper_bound,             /* _get_symtab_upper_bound */
  coff_get_symtab,                         /* _get_symtab */
  coff_make_empty_symbol,                  /* _make_empty_symbol */
  coff_print_symbol,                       /* _print_symbol */
  coff_get_symbol_info,                    /* _get_symbol_info */
  _bfd_xcoff_is_local_label_name,          /* _bfd_is_local_label_name */
  coff_get_lineno,                         /* _get_lineno */
  coff_find_nearest_line,                  /* _find_nearest_line */
  coff_bfd_make_debug_symbol,              /* _bfd_make_debug_symbol */
  _bfd_generic_read_minisymbols,           /* _read_minisymbols */
  _bfd_generic_minisymbol_to_symbol,       /* _minsymbol_to_symbol */

  /* Reloc */
  coff_get_reloc_upper_bound,              /* _get_reloc_upper_bound */
  coff_canonicalize_reloc,           /* _cononicalize_reloc */
  xcoff64_reloc_type_lookup,               /* _bfd_reloc_type_lookup */

  /* Write */
  coff_set_arch_mach,                      /* _set_arch_mach */
  coff_set_section_contents,               /* _set_section_contents */

  /* Link */
  xcoff64_sizeof_headers,               /* _sizeof_headers */
  /* _bfd_get_relocated_section_contents */
  bfd_generic_get_relocated_section_contents,
  bfd_generic_relax_section,               /* _bfd_relax_section */
  _bfd_xcoff_bfd_link_hash_table_create,   /* _bfd_link_hash_table_create */
  _bfd_xcoff_bfd_link_add_symbols,         /* _bfd_link_add_symbols */
  _bfd_xcoff_bfd_final_link,               /* _bfd_filnal_link */
  _bfd_generic_link_split_section,         /* _bfd_link_split_section */
  bfd_generic_gc_sections,                 /* _bfd_gc_sections */
  bfd_generic_merge_sections,               /* _bfd_merge_sections */

  /* Dynamic */
  /* _get_dynamic_symtab_upper_bound */
  _bfd_xcoff_get_dynamic_symtab_upper_bound,
  _bfd_xcoff_canonicalize_dynamic_symtab,  /* _cononicalize_dynamic_symtab */
  _bfd_xcoff_get_dynamic_reloc_upper_bound,/* _get_dynamic_reloc_upper_bound */
  _bfd_xcoff_canonicalize_dynamic_reloc,   /* _cononicalize_dynamic_reloc */

  /* Opposite endian version, none exists */
  NULL,

  /* back end data */
  (void *) &bfd_xcoff_backend_data,
};
