/* Support for the generic parts of PE/PEI; the common executable parts.
   Copyright 1995, 1996, 1997, 1998, 1999, 2000, 2001
   Free Software Foundation, Inc.
   Written by Cygnus Solutions.

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

/* Most of this hacked by Steve Chamberlain <sac@cygnus.com>.

   PE/PEI rearrangement (and code added): Donn Terry
					  Softway Systems, Inc.
*/

/* Hey look, some documentation [and in a place you expect to find it]!

   The main reference for the pei format is "Microsoft Portable Executable
   and Common Object File Format Specification 4.1".  Get it if you need to
   do some serious hacking on this code.

   Another reference:
   "Peering Inside the PE: A Tour of the Win32 Portable Executable
   File Format", MSJ 1994, Volume 9.

   The *sole* difference between the pe format and the pei format is that the
   latter has an MSDOS 2.0 .exe header on the front that prints the message
   "This app must be run under Windows." (or some such).
   (FIXME: Whether that statement is *really* true or not is unknown.
   Are there more subtle differences between pe and pei formats?
   For now assume there aren't.  If you find one, then for God sakes
   document it here!)

   The Microsoft docs use the word "image" instead of "executable" because
   the former can also refer to a DLL (shared library).  Confusion can arise
   because the `i' in `pei' also refers to "image".  The `pe' format can
   also create images (i.e. executables), it's just that to run on a win32
   system you need to use the pei format.

   FIXME: Please add more docs here so the next poor fool that has to hack
   on this code has a chance of getting something accomplished without
   wasting too much time.
*/

/* This expands into COFF_WITH_pe or COFF_WITH_pep depending on whether
   we're compiling for straight PE or PE+.  */
#define COFF_WITH_XX

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "coff/internal.h"

/* NOTE: it's strange to be including an architecture specific header
   in what's supposed to be general (to PE/PEI) code.  However, that's
   where the definitions are, and they don't vary per architecture
   within PE/PEI, so we get them from there.  FIXME: The lack of
   variance is an assumption which may prove to be incorrect if new
   PE/PEI targets are created.  */
#ifdef COFF_WITH_pep
# include "coff/ia64.h"
#else
# include "coff/i386.h"
#endif

#include "coff/pe.h"
#include "libcoff.h"
#include "libpei.h"

#ifdef COFF_WITH_pep
# undef AOUTSZ
# define AOUTSZ		PEPAOUTSZ
# define PEAOUTHDR	PEPAOUTHDR
#endif

/* FIXME: This file has various tests of POWERPC_LE_PE.  Those tests
   worked when the code was in peicode.h, but no longer work now that
   the code is in peigen.c.  PowerPC NT is said to be dead.  If
   anybody wants to revive the code, you will have to figure out how
   to handle those issues.  */

static void add_data_entry
  PARAMS ((bfd *, struct internal_extra_pe_aouthdr *, int, char *, bfd_vma));
static boolean pe_print_pdata PARAMS ((bfd *, PTR));
static boolean pe_print_reloc PARAMS ((bfd *, PTR));

/**********************************************************************/

void
_bfd_XXi_swap_sym_in (abfd, ext1, in1)
     bfd *abfd;
     PTR ext1;
     PTR in1;
{
  SYMENT *ext = (SYMENT *) ext1;
  struct internal_syment *in = (struct internal_syment *) in1;

  if (ext->e.e_name[0] == 0)
    {
      in->_n._n_n._n_zeroes = 0;
      in->_n._n_n._n_offset =
	bfd_h_get_32 (abfd, (bfd_byte *) ext->e.e.e_offset);
    }
  else
    {
      memcpy (in->_n._n_name, ext->e.e_name, SYMNMLEN);
    }

  in->n_value = bfd_h_get_32 (abfd, (bfd_byte *) ext->e_value);
  in->n_scnum = bfd_h_get_16 (abfd, (bfd_byte *) ext->e_scnum);
  if (sizeof (ext->e_type) == 2)
    {
      in->n_type = bfd_h_get_16 (abfd, (bfd_byte *) ext->e_type);
    }
  else
    {
      in->n_type = bfd_h_get_32 (abfd, (bfd_byte *) ext->e_type);
    }
  in->n_sclass = bfd_h_get_8 (abfd, ext->e_sclass);
  in->n_numaux = bfd_h_get_8 (abfd, ext->e_numaux);

#ifndef STRICT_PE_FORMAT
  /* This is for Gnu-created DLLs.  */

  /* The section symbols for the .idata$ sections have class 0x68
     (C_SECTION), which MS documentation indicates is a section
     symbol.  Unfortunately, the value field in the symbol is simply a
     copy of the .idata section's flags rather than something useful.
     When these symbols are encountered, change the value to 0 so that
     they will be handled somewhat correctly in the bfd code.  */
  if (in->n_sclass == C_SECTION)
    {
      in->n_value = 0x0;

#if 0
      /* FIXME: This is clearly wrong.  The problem seems to be that
         undefined C_SECTION symbols appear in the first object of a
         MS generated .lib file, and the symbols are not defined
         anywhere.  */
      in->n_scnum = 1;

      /* I have tried setting the class to 3 and using the following
	 to set the section number.  This will put the address of the
	 pointer to the string kernel32.dll at addresses 0 and 0x10
	 off start of idata section which is not correct.  */
#if 0
      if (strcmp (in->_n._n_name, ".idata$4") == 0)
	in->n_scnum = 3;
      else
	in->n_scnum = 2;
#endif
#else
      /* Create synthetic empty sections as needed.  DJ */
      if (in->n_scnum == 0)
	{
	  asection *sec;
	  for (sec = abfd->sections; sec; sec = sec->next)
	    {
	      if (strcmp (sec->name, in->n_name) == 0)
		{
		  in->n_scnum = sec->target_index;
		  break;
		}
	    }
	}
      if (in->n_scnum == 0)
	{
	  int unused_section_number = 0;
	  asection *sec;
	  char *name;
	  for (sec = abfd->sections; sec; sec = sec->next)
	    if (unused_section_number <= sec->target_index)
	      unused_section_number = sec->target_index + 1;

	  name = bfd_alloc (abfd, strlen (in->n_name) + 10);
	  if (name == NULL)
	    return;
	  strcpy (name, in->n_name);
	  sec = bfd_make_section_anyway (abfd, name);

	  sec->vma = 0;
	  sec->lma = 0;
	  sec->_cooked_size = 0;
	  sec->_raw_size = 0;
	  sec->filepos = 0;
	  sec->rel_filepos = 0;
	  sec->reloc_count = 0;
	  sec->line_filepos = 0;
	  sec->lineno_count = 0;
	  sec->userdata = NULL;
	  sec->next = (asection *) NULL;
	  sec->flags = 0;
	  sec->alignment_power = 2;
	  sec->flags = SEC_HAS_CONTENTS | SEC_ALLOC | SEC_DATA | SEC_LOAD;

	  sec->target_index = unused_section_number;

	  in->n_scnum = unused_section_number;
	}
      in->n_sclass = C_STAT;
#endif
    }
#endif

#ifdef coff_swap_sym_in_hook
  /* This won't work in peigen.c, but since it's for PPC PE, it's not
     worth fixing.  */
  coff_swap_sym_in_hook (abfd, ext1, in1);
#endif
}

unsigned int
_bfd_XXi_swap_sym_out (abfd, inp, extp)
     bfd       *abfd;
     PTR	inp;
     PTR	extp;
{
  struct internal_syment *in = (struct internal_syment *) inp;
  SYMENT *ext = (SYMENT *) extp;
  if (in->_n._n_name[0] == 0)
    {
      bfd_h_put_32 (abfd, 0, (bfd_byte *) ext->e.e.e_zeroes);
      bfd_h_put_32 (abfd, in->_n._n_n._n_offset, (bfd_byte *) ext->e.e.e_offset);
    }
  else
    {
      memcpy (ext->e.e_name, in->_n._n_name, SYMNMLEN);
    }

  bfd_h_put_32 (abfd, in->n_value, (bfd_byte *) ext->e_value);
  bfd_h_put_16 (abfd, in->n_scnum, (bfd_byte *) ext->e_scnum);
  if (sizeof (ext->e_type) == 2)
    {
      bfd_h_put_16 (abfd, in->n_type, (bfd_byte *) ext->e_type);
    }
  else
    {
      bfd_h_put_32 (abfd, in->n_type, (bfd_byte *) ext->e_type);
    }
  bfd_h_put_8 (abfd, in->n_sclass, ext->e_sclass);
  bfd_h_put_8 (abfd, in->n_numaux, ext->e_numaux);

  return SYMESZ;
}

void
_bfd_XXi_swap_aux_in (abfd, ext1, type, class, indx, numaux, in1)
     bfd            *abfd;
     PTR	     ext1;
     int             type;
     int             class;
     int	     indx ATTRIBUTE_UNUSED;
     int	     numaux ATTRIBUTE_UNUSED;
     PTR 	     in1;
{
  AUXENT *ext = (AUXENT *) ext1;
  union internal_auxent *in = (union internal_auxent *) in1;

  switch (class)
    {
    case C_FILE:
      if (ext->x_file.x_fname[0] == 0)
	{
	  in->x_file.x_n.x_zeroes = 0;
	  in->x_file.x_n.x_offset =
	    bfd_h_get_32 (abfd, (bfd_byte *) ext->x_file.x_n.x_offset);
	}
      else
	{
	  memcpy (in->x_file.x_fname, ext->x_file.x_fname, FILNMLEN);
	}
      return;

    case C_STAT:
    case C_LEAFSTAT:
    case C_HIDDEN:
      if (type == T_NULL)
	{
	  in->x_scn.x_scnlen = GET_SCN_SCNLEN (abfd, ext);
	  in->x_scn.x_nreloc = GET_SCN_NRELOC (abfd, ext);
	  in->x_scn.x_nlinno = GET_SCN_NLINNO (abfd, ext);
	  in->x_scn.x_checksum =
	    bfd_h_get_32 (abfd, (bfd_byte *) ext->x_scn.x_checksum);
	  in->x_scn.x_associated =
	    bfd_h_get_16 (abfd, (bfd_byte *) ext->x_scn.x_associated);
	  in->x_scn.x_comdat =
	    bfd_h_get_8 (abfd, (bfd_byte *) ext->x_scn.x_comdat);
	  return;
	}
      break;
    }

  in->x_sym.x_tagndx.l = bfd_h_get_32 (abfd, (bfd_byte *) ext->x_sym.x_tagndx);
  in->x_sym.x_tvndx = bfd_h_get_16 (abfd, (bfd_byte *) ext->x_sym.x_tvndx);

  if (class == C_BLOCK || class == C_FCN || ISFCN (type) || ISTAG (class))
    {
      in->x_sym.x_fcnary.x_fcn.x_lnnoptr = GET_FCN_LNNOPTR (abfd, ext);
      in->x_sym.x_fcnary.x_fcn.x_endndx.l = GET_FCN_ENDNDX (abfd, ext);
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

  if (ISFCN (type))
    {
      in->x_sym.x_misc.x_fsize =
	bfd_h_get_32 (abfd, (bfd_byte *) ext->x_sym.x_misc.x_fsize);
    }
  else
    {
      in->x_sym.x_misc.x_lnsz.x_lnno = GET_LNSZ_LNNO (abfd, ext);
      in->x_sym.x_misc.x_lnsz.x_size = GET_LNSZ_SIZE (abfd, ext);
    }
}

unsigned int
_bfd_XXi_swap_aux_out (abfd, inp, type, class, indx, numaux, extp)
     bfd  *abfd;
     PTR   inp;
     int   type;
     int   class;
     int   indx ATTRIBUTE_UNUSED;
     int   numaux ATTRIBUTE_UNUSED;
     PTR   extp;
{
  union internal_auxent *in = (union internal_auxent *) inp;
  AUXENT *ext = (AUXENT *) extp;

  memset ((PTR) ext, 0, AUXESZ);
  switch (class)
    {
    case C_FILE:
      if (in->x_file.x_fname[0] == 0)
	{
	  bfd_h_put_32 (abfd, 0, (bfd_byte *) ext->x_file.x_n.x_zeroes);
	  bfd_h_put_32 (abfd,
			in->x_file.x_n.x_offset,
			(bfd_byte *) ext->x_file.x_n.x_offset);
	}
      else
	{
	  memcpy (ext->x_file.x_fname, in->x_file.x_fname, FILNMLEN);
	}
      return AUXESZ;

    case C_STAT:
    case C_LEAFSTAT:
    case C_HIDDEN:
      if (type == T_NULL)
	{
	  PUT_SCN_SCNLEN (abfd, in->x_scn.x_scnlen, ext);
	  PUT_SCN_NRELOC (abfd, in->x_scn.x_nreloc, ext);
	  PUT_SCN_NLINNO (abfd, in->x_scn.x_nlinno, ext);
	  bfd_h_put_32 (abfd, in->x_scn.x_checksum,
			(bfd_byte *) ext->x_scn.x_checksum);
	  bfd_h_put_16 (abfd, in->x_scn.x_associated,
			(bfd_byte *) ext->x_scn.x_associated);
	  bfd_h_put_8 (abfd, in->x_scn.x_comdat,
		       (bfd_byte *) ext->x_scn.x_comdat);
	  return AUXESZ;
	}
      break;
    }

  bfd_h_put_32 (abfd, in->x_sym.x_tagndx.l, (bfd_byte *) ext->x_sym.x_tagndx);
  bfd_h_put_16 (abfd, in->x_sym.x_tvndx, (bfd_byte *) ext->x_sym.x_tvndx);

  if (class == C_BLOCK || class == C_FCN || ISFCN (type) || ISTAG (class))
    {
      PUT_FCN_LNNOPTR (abfd, in->x_sym.x_fcnary.x_fcn.x_lnnoptr,  ext);
      PUT_FCN_ENDNDX  (abfd, in->x_sym.x_fcnary.x_fcn.x_endndx.l, ext);
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
    bfd_h_put_32 (abfd, in->x_sym.x_misc.x_fsize,
		  (bfd_byte *) ext->x_sym.x_misc.x_fsize);
  else
    {
      PUT_LNSZ_LNNO (abfd, in->x_sym.x_misc.x_lnsz.x_lnno, ext);
      PUT_LNSZ_SIZE (abfd, in->x_sym.x_misc.x_lnsz.x_size, ext);
    }

  return AUXESZ;
}

void
_bfd_XXi_swap_lineno_in (abfd, ext1, in1)
     bfd *abfd;
     PTR ext1;
     PTR in1;
{
  LINENO *ext = (LINENO *) ext1;
  struct internal_lineno *in = (struct internal_lineno *) in1;

  in->l_addr.l_symndx = bfd_h_get_32 (abfd, (bfd_byte *) ext->l_addr.l_symndx);
  in->l_lnno = GET_LINENO_LNNO (abfd, ext);
}

unsigned int
_bfd_XXi_swap_lineno_out (abfd, inp, outp)
     bfd       *abfd;
     PTR	inp;
     PTR	outp;
{
  struct internal_lineno *in = (struct internal_lineno *) inp;
  struct external_lineno *ext = (struct external_lineno *) outp;
  bfd_h_put_32 (abfd, in->l_addr.l_symndx, (bfd_byte *)
	  ext->l_addr.l_symndx);

  PUT_LINENO_LNNO (abfd, in->l_lnno, ext);
  return LINESZ;
}

void
_bfd_XXi_swap_aouthdr_in (abfd, aouthdr_ext1, aouthdr_int1)
     bfd *abfd;
     PTR aouthdr_ext1;
     PTR aouthdr_int1;
{
  struct internal_extra_pe_aouthdr *a;
  PEAOUTHDR *src = (PEAOUTHDR *) (aouthdr_ext1);
  AOUTHDR        *aouthdr_ext = (AOUTHDR *) aouthdr_ext1;
  struct internal_aouthdr *aouthdr_int = (struct internal_aouthdr *)aouthdr_int1;

  aouthdr_int->magic = bfd_h_get_16 (abfd, (bfd_byte *) aouthdr_ext->magic);
  aouthdr_int->vstamp = bfd_h_get_16 (abfd, (bfd_byte *) aouthdr_ext->vstamp);
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
#ifndef COFF_WITH_pep
  /* PE32+ does not have data_start member! */
  aouthdr_int->data_start =
    GET_AOUTHDR_DATA_START (abfd, (bfd_byte *) aouthdr_ext->data_start);
#endif

  a = &aouthdr_int->pe;
  a->ImageBase = GET_OPTHDR_IMAGE_BASE (abfd, (bfd_byte *) src->ImageBase);
  a->SectionAlignment = bfd_h_get_32 (abfd, (bfd_byte *) src->SectionAlignment);
  a->FileAlignment = bfd_h_get_32 (abfd, (bfd_byte *) src->FileAlignment);
  a->MajorOperatingSystemVersion =
    bfd_h_get_16 (abfd, (bfd_byte *) src->MajorOperatingSystemVersion);
  a->MinorOperatingSystemVersion =
    bfd_h_get_16 (abfd, (bfd_byte *) src->MinorOperatingSystemVersion);
  a->MajorImageVersion = bfd_h_get_16 (abfd, (bfd_byte *) src->MajorImageVersion);
  a->MinorImageVersion = bfd_h_get_16 (abfd, (bfd_byte *) src->MinorImageVersion);
  a->MajorSubsystemVersion = bfd_h_get_16 (abfd, (bfd_byte *) src->MajorSubsystemVersion);
  a->MinorSubsystemVersion = bfd_h_get_16 (abfd, (bfd_byte *) src->MinorSubsystemVersion);
  a->Reserved1 = bfd_h_get_32 (abfd, (bfd_byte *) src->Reserved1);
  a->SizeOfImage = bfd_h_get_32 (abfd, (bfd_byte *) src->SizeOfImage);
  a->SizeOfHeaders = bfd_h_get_32 (abfd, (bfd_byte *) src->SizeOfHeaders);
  a->CheckSum = bfd_h_get_32 (abfd, (bfd_byte *) src->CheckSum);
  a->Subsystem = bfd_h_get_16 (abfd, (bfd_byte *) src->Subsystem);
  a->DllCharacteristics = bfd_h_get_16 (abfd, (bfd_byte *) src->DllCharacteristics);
  a->SizeOfStackReserve = GET_OPTHDR_SIZE_OF_STACK_RESERVE (abfd, (bfd_byte *) src->SizeOfStackReserve);
  a->SizeOfStackCommit = GET_OPTHDR_SIZE_OF_STACK_COMMIT (abfd, (bfd_byte *) src->SizeOfStackCommit);
  a->SizeOfHeapReserve = GET_OPTHDR_SIZE_OF_HEAP_RESERVE (abfd, (bfd_byte *) src->SizeOfHeapReserve);
  a->SizeOfHeapCommit = GET_OPTHDR_SIZE_OF_HEAP_COMMIT (abfd, (bfd_byte *) src->SizeOfHeapCommit);
  a->LoaderFlags = bfd_h_get_32 (abfd, (bfd_byte *) src->LoaderFlags);
  a->NumberOfRvaAndSizes = bfd_h_get_32 (abfd, (bfd_byte *) src->NumberOfRvaAndSizes);

  {
    int idx;
    for (idx = 0; idx < 16; idx++)
      {
        /* If data directory is empty, rva also should be 0.  */
	int size =
	  bfd_h_get_32 (abfd, (bfd_byte *) src->DataDirectory[idx][1]);
	a->DataDirectory[idx].Size = size;

	if (size)
	  {
	    a->DataDirectory[idx].VirtualAddress =
	      bfd_h_get_32 (abfd, (bfd_byte *) src->DataDirectory[idx][0]);
	  }
	else
	  a->DataDirectory[idx].VirtualAddress = 0;
      }
  }

  if (aouthdr_int->entry)
    {
      aouthdr_int->entry += a->ImageBase;
#ifndef COFF_WITH_pep
      aouthdr_int->entry &= 0xffffffff;
#endif
    }
  if (aouthdr_int->tsize)
    {
      aouthdr_int->text_start += a->ImageBase;
#ifndef COFF_WITH_pep
      aouthdr_int->text_start &= 0xffffffff;
#endif
    }
#ifndef COFF_WITH_pep
  /* PE32+ does not have data_start member! */
  if (aouthdr_int->dsize)
    {
      aouthdr_int->data_start += a->ImageBase;
      aouthdr_int->data_start &= 0xffffffff;
    }
#endif

#ifdef POWERPC_LE_PE
  /* These three fields are normally set up by ppc_relocate_section.
     In the case of reading a file in, we can pick them up from the
     DataDirectory.  */
  first_thunk_address = a->DataDirectory[12].VirtualAddress;
  thunk_size = a->DataDirectory[12].Size;
  import_table_size = a->DataDirectory[1].Size;
#endif

}

/* A support function for below.  */

static void
add_data_entry (abfd, aout, idx, name, base)
     bfd *abfd;
     struct internal_extra_pe_aouthdr *aout;
     int idx;
     char *name;
     bfd_vma base;
{
  asection *sec = bfd_get_section_by_name (abfd, name);

  /* add import directory information if it exists */
  if ((sec != NULL)
      && (coff_section_data (abfd, sec) != NULL)
      && (pei_section_data (abfd, sec) != NULL))
    {
      /* If data directory is empty, rva also should be 0 */
      int size = pei_section_data (abfd, sec)->virt_size;
      aout->DataDirectory[idx].Size = size;

      if (size)
	{
	  aout->DataDirectory[idx].VirtualAddress =
	    (sec->vma - base) & 0xffffffff;
	  sec->flags |= SEC_DATA;
	}
    }
}

unsigned int
_bfd_XXi_swap_aouthdr_out (abfd, in, out)
     bfd       *abfd;
     PTR	in;
     PTR	out;
{
  struct internal_aouthdr *aouthdr_in = (struct internal_aouthdr *) in;
  pe_data_type *pe = pe_data (abfd);
  struct internal_extra_pe_aouthdr *extra = &pe->pe_opthdr;
  PEAOUTHDR *aouthdr_out = (PEAOUTHDR *) out;
  bfd_vma sa, fa, ib;

  if (pe->force_minimum_alignment)
    {
      if (!extra->FileAlignment)
	extra->FileAlignment = PE_DEF_FILE_ALIGNMENT;
      if (!extra->SectionAlignment)
	extra->SectionAlignment = PE_DEF_SECTION_ALIGNMENT;
    }

  if (extra->Subsystem == IMAGE_SUBSYSTEM_UNKNOWN)
    extra->Subsystem = pe->target_subsystem;

  sa = extra->SectionAlignment;
  fa = extra->FileAlignment;
  ib = extra->ImageBase;

  if (aouthdr_in->tsize)
    {
      aouthdr_in->text_start -= ib;
#ifndef COFF_WITH_pep
      aouthdr_in->text_start &= 0xffffffff;
#endif
    }
  if (aouthdr_in->dsize)
    {
      aouthdr_in->data_start -= ib;
#ifndef COFF_WITH_pep
      aouthdr_in->data_start &= 0xffffffff;
#endif
    }
  if (aouthdr_in->entry)
    {
      aouthdr_in->entry -= ib;
#ifndef COFF_WITH_pep
      aouthdr_in->entry &= 0xffffffff;
#endif
    }

#define FA(x) (((x) + fa -1 ) & (- fa))
#define SA(x) (((x) + sa -1 ) & (- sa))

  /* We like to have the sizes aligned.  */

  aouthdr_in->bsize = FA (aouthdr_in->bsize);

  extra->NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

  /* first null out all data directory entries ..  */
  memset (extra->DataDirectory, sizeof (extra->DataDirectory), 0);

  add_data_entry (abfd, extra, 0, ".edata", ib);

  /* Don't call add_data_entry for .idata$2 or .idata$5.  It's done in
     bfd_coff_final_link where all the required information is
     available.  */

  /* However, until other .idata fixes are made (pending patch), the
     entry for .idata is needed for backwards compatability.  FIXME.  */
  add_data_entry (abfd, extra, 1, ".idata", ib);

  add_data_entry (abfd, extra, 2, ".rsrc", ib);

  add_data_entry (abfd, extra, 3, ".pdata", ib);

  /* For some reason, the virtual size (which is what's set by
     add_data_entry) for .reloc is not the same as the size recorded
     in this slot by MSVC; it doesn't seem to cause problems (so far),
     but since it's the best we've got, use it.  It does do the right
     thing for .pdata.  */
  if (pe->has_reloc_section)
    add_data_entry (abfd, extra, 5, ".reloc", ib);

  {
    asection *sec;
    bfd_vma dsize = 0;
    bfd_vma isize = SA(abfd->sections->filepos);
    bfd_vma tsize = 0;

    for (sec = abfd->sections; sec; sec = sec->next)
      {
	int rounded = FA(sec->_raw_size);

	if (sec->flags & SEC_DATA)
	  dsize += rounded;
	if (sec->flags & SEC_CODE)
	  tsize += rounded;
	/* The image size is the total VIRTUAL size (which is what is
	   in the virt_size field).  Files have been seen (from MSVC
	   5.0 link.exe) where the file size of the .data segment is
	   quite small compared to the virtual size.  Without this
	   fix, strip munges the file.  */
	isize += SA (FA (pei_section_data (abfd, sec)->virt_size));
      }

    aouthdr_in->dsize = dsize;
    aouthdr_in->tsize = tsize;
    extra->SizeOfImage = isize;
  }

  extra->SizeOfHeaders = abfd->sections->filepos;
  bfd_h_put_16 (abfd, aouthdr_in->magic, (bfd_byte *) aouthdr_out->standard.magic);

#define LINKER_VERSION 256 /* That is, 2.56 */

  /* This piece of magic sets the "linker version" field to
     LINKER_VERSION.  */
  bfd_h_put_16 (abfd,
		LINKER_VERSION / 100 + (LINKER_VERSION % 100) * 256,
		(bfd_byte *) aouthdr_out->standard.vstamp);

  PUT_AOUTHDR_TSIZE (abfd, aouthdr_in->tsize, (bfd_byte *) aouthdr_out->standard.tsize);
  PUT_AOUTHDR_DSIZE (abfd, aouthdr_in->dsize, (bfd_byte *) aouthdr_out->standard.dsize);
  PUT_AOUTHDR_BSIZE (abfd, aouthdr_in->bsize, (bfd_byte *) aouthdr_out->standard.bsize);
  PUT_AOUTHDR_ENTRY (abfd, aouthdr_in->entry, (bfd_byte *) aouthdr_out->standard.entry);
  PUT_AOUTHDR_TEXT_START (abfd, aouthdr_in->text_start,
			  (bfd_byte *) aouthdr_out->standard.text_start);

#ifndef COFF_WITH_pep
  /* PE32+ does not have data_start member! */
  PUT_AOUTHDR_DATA_START (abfd, aouthdr_in->data_start,
			  (bfd_byte *) aouthdr_out->standard.data_start);
#endif

  PUT_OPTHDR_IMAGE_BASE (abfd, extra->ImageBase,
			 (bfd_byte *) aouthdr_out->ImageBase);
  bfd_h_put_32 (abfd, extra->SectionAlignment,
		(bfd_byte *) aouthdr_out->SectionAlignment);
  bfd_h_put_32 (abfd, extra->FileAlignment,
		(bfd_byte *) aouthdr_out->FileAlignment);
  bfd_h_put_16 (abfd, extra->MajorOperatingSystemVersion,
		(bfd_byte *) aouthdr_out->MajorOperatingSystemVersion);
  bfd_h_put_16 (abfd, extra->MinorOperatingSystemVersion,
		(bfd_byte *) aouthdr_out->MinorOperatingSystemVersion);
  bfd_h_put_16 (abfd, extra->MajorImageVersion,
		(bfd_byte *) aouthdr_out->MajorImageVersion);
  bfd_h_put_16 (abfd, extra->MinorImageVersion,
		(bfd_byte *) aouthdr_out->MinorImageVersion);
  bfd_h_put_16 (abfd, extra->MajorSubsystemVersion,
		(bfd_byte *) aouthdr_out->MajorSubsystemVersion);
  bfd_h_put_16 (abfd, extra->MinorSubsystemVersion,
		(bfd_byte *) aouthdr_out->MinorSubsystemVersion);
  bfd_h_put_32 (abfd, extra->Reserved1,
		(bfd_byte *) aouthdr_out->Reserved1);
  bfd_h_put_32 (abfd, extra->SizeOfImage,
		(bfd_byte *) aouthdr_out->SizeOfImage);
  bfd_h_put_32 (abfd, extra->SizeOfHeaders,
		(bfd_byte *) aouthdr_out->SizeOfHeaders);
  bfd_h_put_32 (abfd, extra->CheckSum,
		(bfd_byte *) aouthdr_out->CheckSum);
  bfd_h_put_16 (abfd, extra->Subsystem,
		(bfd_byte *) aouthdr_out->Subsystem);
  bfd_h_put_16 (abfd, extra->DllCharacteristics,
		(bfd_byte *) aouthdr_out->DllCharacteristics);
  PUT_OPTHDR_SIZE_OF_STACK_RESERVE (abfd, extra->SizeOfStackReserve,
				    (bfd_byte *) aouthdr_out->SizeOfStackReserve);
  PUT_OPTHDR_SIZE_OF_STACK_COMMIT (abfd, extra->SizeOfStackCommit,
				   (bfd_byte *) aouthdr_out->SizeOfStackCommit);
  PUT_OPTHDR_SIZE_OF_HEAP_RESERVE (abfd, extra->SizeOfHeapReserve,
				   (bfd_byte *) aouthdr_out->SizeOfHeapReserve);
  PUT_OPTHDR_SIZE_OF_HEAP_COMMIT (abfd, extra->SizeOfHeapCommit,
				  (bfd_byte *) aouthdr_out->SizeOfHeapCommit);
  bfd_h_put_32 (abfd, extra->LoaderFlags,
		(bfd_byte *) aouthdr_out->LoaderFlags);
  bfd_h_put_32 (abfd, extra->NumberOfRvaAndSizes,
		(bfd_byte *) aouthdr_out->NumberOfRvaAndSizes);
  {
    int idx;
    for (idx = 0; idx < 16; idx++)
      {
	bfd_h_put_32 (abfd, extra->DataDirectory[idx].VirtualAddress,
		      (bfd_byte *) aouthdr_out->DataDirectory[idx][0]);
	bfd_h_put_32 (abfd, extra->DataDirectory[idx].Size,
		      (bfd_byte *) aouthdr_out->DataDirectory[idx][1]);
      }
  }

  return AOUTSZ;
}

unsigned int
_bfd_XXi_only_swap_filehdr_out (abfd, in, out)
     bfd       *abfd;
     PTR	in;
     PTR	out;
{
  int idx;
  struct internal_filehdr *filehdr_in = (struct internal_filehdr *) in;
  struct external_PEI_filehdr *filehdr_out = (struct external_PEI_filehdr *) out;

  if (pe_data (abfd)->has_reloc_section)
    filehdr_in->f_flags &= ~F_RELFLG;

  if (pe_data (abfd)->dll)
    filehdr_in->f_flags |= F_DLL;

  filehdr_in->pe.e_magic    = DOSMAGIC;
  filehdr_in->pe.e_cblp     = 0x90;
  filehdr_in->pe.e_cp       = 0x3;
  filehdr_in->pe.e_crlc     = 0x0;
  filehdr_in->pe.e_cparhdr  = 0x4;
  filehdr_in->pe.e_minalloc = 0x0;
  filehdr_in->pe.e_maxalloc = 0xffff;
  filehdr_in->pe.e_ss       = 0x0;
  filehdr_in->pe.e_sp       = 0xb8;
  filehdr_in->pe.e_csum     = 0x0;
  filehdr_in->pe.e_ip       = 0x0;
  filehdr_in->pe.e_cs       = 0x0;
  filehdr_in->pe.e_lfarlc   = 0x40;
  filehdr_in->pe.e_ovno     = 0x0;

  for (idx = 0; idx < 4; idx++)
    filehdr_in->pe.e_res[idx] = 0x0;

  filehdr_in->pe.e_oemid   = 0x0;
  filehdr_in->pe.e_oeminfo = 0x0;

  for (idx = 0; idx < 10; idx++)
    filehdr_in->pe.e_res2[idx] = 0x0;

  filehdr_in->pe.e_lfanew = 0x80;

  /* This next collection of data are mostly just characters.  It
     appears to be constant within the headers put on NT exes.  */
  filehdr_in->pe.dos_message[0]  = 0x0eba1f0e;
  filehdr_in->pe.dos_message[1]  = 0xcd09b400;
  filehdr_in->pe.dos_message[2]  = 0x4c01b821;
  filehdr_in->pe.dos_message[3]  = 0x685421cd;
  filehdr_in->pe.dos_message[4]  = 0x70207369;
  filehdr_in->pe.dos_message[5]  = 0x72676f72;
  filehdr_in->pe.dos_message[6]  = 0x63206d61;
  filehdr_in->pe.dos_message[7]  = 0x6f6e6e61;
  filehdr_in->pe.dos_message[8]  = 0x65622074;
  filehdr_in->pe.dos_message[9]  = 0x6e757220;
  filehdr_in->pe.dos_message[10] = 0x206e6920;
  filehdr_in->pe.dos_message[11] = 0x20534f44;
  filehdr_in->pe.dos_message[12] = 0x65646f6d;
  filehdr_in->pe.dos_message[13] = 0x0a0d0d2e;
  filehdr_in->pe.dos_message[14] = 0x24;
  filehdr_in->pe.dos_message[15] = 0x0;
  filehdr_in->pe.nt_signature = NT_SIGNATURE;

  bfd_h_put_16 (abfd, filehdr_in->f_magic, (bfd_byte *) filehdr_out->f_magic);
  bfd_h_put_16 (abfd, filehdr_in->f_nscns, (bfd_byte *) filehdr_out->f_nscns);

  bfd_h_put_32 (abfd, time (0), (bfd_byte *) filehdr_out->f_timdat);
  PUT_FILEHDR_SYMPTR (abfd, (bfd_vma) filehdr_in->f_symptr,
		      (bfd_byte *) filehdr_out->f_symptr);
  bfd_h_put_32 (abfd, filehdr_in->f_nsyms, (bfd_byte *) filehdr_out->f_nsyms);
  bfd_h_put_16 (abfd, filehdr_in->f_opthdr, (bfd_byte *) filehdr_out->f_opthdr);
  bfd_h_put_16 (abfd, filehdr_in->f_flags, (bfd_byte *) filehdr_out->f_flags);

  /* put in extra dos header stuff.  This data remains essentially
     constant, it just has to be tacked on to the beginning of all exes
     for NT */
  bfd_h_put_16 (abfd, filehdr_in->pe.e_magic, (bfd_byte *) filehdr_out->e_magic);
  bfd_h_put_16 (abfd, filehdr_in->pe.e_cblp, (bfd_byte *) filehdr_out->e_cblp);
  bfd_h_put_16 (abfd, filehdr_in->pe.e_cp, (bfd_byte *) filehdr_out->e_cp);
  bfd_h_put_16 (abfd, filehdr_in->pe.e_crlc, (bfd_byte *) filehdr_out->e_crlc);
  bfd_h_put_16 (abfd, filehdr_in->pe.e_cparhdr,
	       (bfd_byte *) filehdr_out->e_cparhdr);
  bfd_h_put_16 (abfd, filehdr_in->pe.e_minalloc,
	       (bfd_byte *) filehdr_out->e_minalloc);
  bfd_h_put_16 (abfd, filehdr_in->pe.e_maxalloc,
	       (bfd_byte *) filehdr_out->e_maxalloc);
  bfd_h_put_16 (abfd, filehdr_in->pe.e_ss, (bfd_byte *) filehdr_out->e_ss);
  bfd_h_put_16 (abfd, filehdr_in->pe.e_sp, (bfd_byte *) filehdr_out->e_sp);
  bfd_h_put_16 (abfd, filehdr_in->pe.e_csum, (bfd_byte *) filehdr_out->e_csum);
  bfd_h_put_16 (abfd, filehdr_in->pe.e_ip, (bfd_byte *) filehdr_out->e_ip);
  bfd_h_put_16 (abfd, filehdr_in->pe.e_cs, (bfd_byte *) filehdr_out->e_cs);
  bfd_h_put_16 (abfd, filehdr_in->pe.e_lfarlc, (bfd_byte *) filehdr_out->e_lfarlc);
  bfd_h_put_16 (abfd, filehdr_in->pe.e_ovno, (bfd_byte *) filehdr_out->e_ovno);
  {
    int idx;
    for (idx = 0; idx < 4; idx++)
      bfd_h_put_16 (abfd, filehdr_in->pe.e_res[idx],
		    (bfd_byte *) filehdr_out->e_res[idx]);
  }
  bfd_h_put_16 (abfd, filehdr_in->pe.e_oemid, (bfd_byte *) filehdr_out->e_oemid);
  bfd_h_put_16 (abfd, filehdr_in->pe.e_oeminfo,
		(bfd_byte *) filehdr_out->e_oeminfo);
  {
    int idx;
    for (idx = 0; idx < 10; idx++)
      bfd_h_put_16 (abfd, filehdr_in->pe.e_res2[idx],
		    (bfd_byte *) filehdr_out->e_res2[idx]);
  }
  bfd_h_put_32 (abfd, filehdr_in->pe.e_lfanew, (bfd_byte *) filehdr_out->e_lfanew);

  {
    int idx;
    for (idx = 0; idx < 16; idx++)
      bfd_h_put_32 (abfd, filehdr_in->pe.dos_message[idx],
		    (bfd_byte *) filehdr_out->dos_message[idx]);
  }

  /* Also put in the NT signature.  */
  bfd_h_put_32 (abfd, filehdr_in->pe.nt_signature,
		(bfd_byte *) filehdr_out->nt_signature);

  return FILHSZ;
}

unsigned int
_bfd_XX_only_swap_filehdr_out (abfd, in, out)
     bfd       *abfd;
     PTR	in;
     PTR	out;
{
  struct internal_filehdr *filehdr_in = (struct internal_filehdr *) in;
  FILHDR *filehdr_out = (FILHDR *) out;

  bfd_h_put_16 (abfd, filehdr_in->f_magic, (bfd_byte *) filehdr_out->f_magic);
  bfd_h_put_16 (abfd, filehdr_in->f_nscns, (bfd_byte *) filehdr_out->f_nscns);
  bfd_h_put_32 (abfd, filehdr_in->f_timdat, (bfd_byte *) filehdr_out->f_timdat);
  PUT_FILEHDR_SYMPTR (abfd, (bfd_vma) filehdr_in->f_symptr,
		      (bfd_byte *) filehdr_out->f_symptr);
  bfd_h_put_32 (abfd, filehdr_in->f_nsyms, (bfd_byte *) filehdr_out->f_nsyms);
  bfd_h_put_16 (abfd, filehdr_in->f_opthdr, (bfd_byte *) filehdr_out->f_opthdr);
  bfd_h_put_16 (abfd, filehdr_in->f_flags, (bfd_byte *) filehdr_out->f_flags);

  return FILHSZ;
}

unsigned int
_bfd_XXi_swap_scnhdr_out (abfd, in, out)
     bfd       *abfd;
     PTR	in;
     PTR	out;
{
  struct internal_scnhdr *scnhdr_int = (struct internal_scnhdr *) in;
  SCNHDR *scnhdr_ext = (SCNHDR *) out;
  unsigned int ret = SCNHSZ;
  bfd_vma ps;
  bfd_vma ss;

  memcpy (scnhdr_ext->s_name, scnhdr_int->s_name, sizeof (scnhdr_int->s_name));

  PUT_SCNHDR_VADDR (abfd,
		    ((scnhdr_int->s_vaddr
		      - pe_data (abfd)->pe_opthdr.ImageBase)
		     & 0xffffffff),
		    (bfd_byte *) scnhdr_ext->s_vaddr);

  /* NT wants the size data to be rounded up to the next
     NT_FILE_ALIGNMENT, but zero if it has no content (as in .bss,
     sometimes).  */

  if ((scnhdr_int->s_flags & IMAGE_SCN_CNT_UNINITIALIZED_DATA) != 0)
    {
      ps = scnhdr_int->s_size;
      ss = 0;
    }
  else
    {
      ps = scnhdr_int->s_paddr;
      ss = scnhdr_int->s_size;
    }

  PUT_SCNHDR_SIZE (abfd, ss,
		   (bfd_byte *) scnhdr_ext->s_size);

  /* s_paddr in PE is really the virtual size.  */
  PUT_SCNHDR_PADDR (abfd, ps, (bfd_byte *) scnhdr_ext->s_paddr);

  PUT_SCNHDR_SCNPTR (abfd, scnhdr_int->s_scnptr,
		     (bfd_byte *) scnhdr_ext->s_scnptr);
  PUT_SCNHDR_RELPTR (abfd, scnhdr_int->s_relptr,
		     (bfd_byte *) scnhdr_ext->s_relptr);
  PUT_SCNHDR_LNNOPTR (abfd, scnhdr_int->s_lnnoptr,
		      (bfd_byte *) scnhdr_ext->s_lnnoptr);

  /* Extra flags must be set when dealing with NT.  All sections should also
     have the IMAGE_SCN_MEM_READ (0x40000000) flag set.  In addition, the
     .text section must have IMAGE_SCN_MEM_EXECUTE (0x20000000) and the data
     sections (.idata, .data, .bss, .CRT) must have IMAGE_SCN_MEM_WRITE set
     (this is especially important when dealing with the .idata section since
     the addresses for routines from .dlls must be overwritten).  If .reloc
     section data is ever generated, we must add IMAGE_SCN_MEM_DISCARDABLE
     (0x02000000).  Also, the resource data should also be read and
     writable.  */

  /* FIXME: alignment is also encoded in this field, at least on ppc (krk) */
  /* FIXME: even worse, I don't see how to get the original alignment field*/
  /*        back...                                                        */

  {
    int flags = scnhdr_int->s_flags;
    bfd_h_put_32 (abfd, flags, (bfd_byte *) scnhdr_ext->s_flags);
  }

  if (coff_data (abfd)->link_info
      && ! coff_data (abfd)->link_info->relocateable
      && ! coff_data (abfd)->link_info->shared
      && strcmp (scnhdr_int->s_name, ".text") == 0)
    {
      /* By inference from looking at MS output, the 32 bit field
	 which is the combintion of the number_of_relocs and
	 number_of_linenos is used for the line number count in
	 executables.  A 16-bit field won't do for cc1.  The MS
	 document says that the number of relocs is zero for
	 executables, but the 17-th bit has been observed to be there.
	 Overflow is not an issue: a 4G-line program will overflow a
	 bunch of other fields long before this!  */
      bfd_h_put_16 (abfd, scnhdr_int->s_nlnno & 0xffff,
		    (bfd_byte *) scnhdr_ext->s_nlnno);
      bfd_h_put_16 (abfd, scnhdr_int->s_nlnno >> 16,
		    (bfd_byte *) scnhdr_ext->s_nreloc);
    }
  else
    {
      if (scnhdr_int->s_nlnno <= 0xffff)
	bfd_h_put_16 (abfd, scnhdr_int->s_nlnno,
		      (bfd_byte *) scnhdr_ext->s_nlnno);
      else
	{
	  (*_bfd_error_handler) (_("%s: line number overflow: 0x%lx > 0xffff"),
				 bfd_get_filename (abfd),
				 scnhdr_int->s_nlnno);
	  bfd_set_error (bfd_error_file_truncated);
	  bfd_h_put_16 (abfd, 0xffff, (bfd_byte *) scnhdr_ext->s_nlnno);
	  ret = 0;
	}
      if (scnhdr_int->s_nreloc <= 0xffff)
	bfd_h_put_16 (abfd, scnhdr_int->s_nreloc,
		      (bfd_byte *) scnhdr_ext->s_nreloc);
      else
	{
	  /* PE can deal with large #s of relocs, but not here */
	  bfd_h_put_16 (abfd, 0xffff, (bfd_byte *) scnhdr_ext->s_nreloc);
	  scnhdr_int->s_flags |= IMAGE_SCN_LNK_NRELOC_OVFL;
	  bfd_h_put_32 (abfd, scnhdr_int->s_flags,
			(bfd_byte *) scnhdr_ext->s_flags);
#if 0
	  (*_bfd_error_handler) (_("%s: reloc overflow 1: 0x%lx > 0xffff"),
				 bfd_get_filename (abfd),
				 scnhdr_int->s_nreloc);
	  bfd_set_error (bfd_error_file_truncated);
	  bfd_h_put_16 (abfd, 0xffff, (bfd_byte *) scnhdr_ext->s_nreloc);
	  ret = 0;
#endif
	}
    }
  return ret;
}

static char * dir_names[IMAGE_NUMBEROF_DIRECTORY_ENTRIES] = {
  N_("Export Directory [.edata (or where ever we found it)]"),
  N_("Import Directory [parts of .idata]"),
  N_("Resource Directory [.rsrc]"),
  N_("Exception Directory [.pdata]"),
  N_("Security Directory"),
  N_("Base Relocation Directory [.reloc]"),
  N_("Debug Directory"),
  N_("Description Directory"),
  N_("Special Directory"),
  N_("Thread Storage Directory [.tls]"),
  N_("Load Configuration Directory"),
  N_("Bound Import Directory"),
  N_("Import Address Table Directory"),
  N_("Delay Import Directory"),
  N_("Reserved"),
  N_("Reserved")
};

/**********************************************************************/
#ifdef POWERPC_LE_PE
/* The code for the PPC really falls in the "architecture dependent"
   category.  However, it's not clear that anyone will ever care, so
   we're ignoring the issue for now; if/when PPC matters, some of this
   may need to go into peicode.h, or arguments passed to enable the
   PPC- specific code.  */
#endif

/**********************************************************************/
static boolean
pe_print_idata (abfd, vfile)
     bfd *abfd;
     PTR vfile;
{
  FILE *file = (FILE *) vfile;
  bfd_byte *data;
  asection *section;
  bfd_signed_vma adj;

#ifdef POWERPC_LE_PE
  asection *rel_section = bfd_get_section_by_name (abfd, ".reldata");
#endif

  bfd_size_type datasize = 0;
  bfd_size_type dataoff;
  bfd_size_type i;
  int onaline = 20;

  pe_data_type *pe = pe_data (abfd);
  struct internal_extra_pe_aouthdr *extra = &pe->pe_opthdr;

  bfd_vma addr;

  addr = extra->DataDirectory[1].VirtualAddress;

  if (addr == 0 && extra->DataDirectory[1].Size == 0)
    {
      /* Maybe the extra header isn't there.  Look for the section.  */
      section = bfd_get_section_by_name (abfd, ".idata");
      if (section == NULL)
	return true;

      addr = section->vma;
      datasize = bfd_section_size (abfd, section);
      if (datasize == 0)
	return true;
    }
  else
    {
      addr += extra->ImageBase;
      for (section = abfd->sections; section != NULL; section = section->next)
	{
	  datasize = bfd_section_size (abfd, section);
	  if (addr >= section->vma && addr < section->vma + datasize)
	    break;
	}

      if (section == NULL)
	{
	  fprintf (file,
		   _("\nThere is an import table, but the section containing it could not be found\n"));
	  return true;
	}
    }

  fprintf (file, _("\nThere is an import table in %s at 0x%lx\n"),
	   section->name, (unsigned long) addr);

  dataoff = addr - section->vma;
  datasize -= dataoff;

#ifdef POWERPC_LE_PE
  if (rel_section != 0 && bfd_section_size (abfd, rel_section) != 0)
    {
      /* The toc address can be found by taking the starting address,
	 which on the PPC locates a function descriptor. The
	 descriptor consists of the function code starting address
	 followed by the address of the toc. The starting address we
	 get from the bfd, and the descriptor is supposed to be in the
	 .reldata section.  */

      bfd_vma loadable_toc_address;
      bfd_vma toc_address;
      bfd_vma start_address;
      bfd_byte *data = 0;
      int offset;

      data = (bfd_byte *) bfd_malloc ((size_t) bfd_section_size (abfd,
								 rel_section));
      if (data == NULL && bfd_section_size (abfd, rel_section) != 0)
	return false;

      bfd_get_section_contents (abfd,
				rel_section,
				(PTR) data, 0,
				bfd_section_size (abfd, rel_section));

      offset = abfd->start_address - rel_section->vma;

      start_address = bfd_get_32 (abfd, data + offset);
      loadable_toc_address = bfd_get_32 (abfd, data + offset + 4);
      toc_address = loadable_toc_address - 32768;

      fprintf (file,
	       _("\nFunction descriptor located at the start address: %04lx\n"),
	       (unsigned long int) (abfd->start_address));
      fprintf (file,
	       _("\tcode-base %08lx toc (loadable/actual) %08lx/%08lx\n"),
	       start_address, loadable_toc_address, toc_address);
    }
  else
    {
      fprintf (file,
	       _("\nNo reldata section! Function descriptor not decoded.\n"));
    }
#endif

  fprintf (file,
	   _("\nThe Import Tables (interpreted %s section contents)\n"),
	   section->name);
  fprintf (file,
	   _(" vma:            Hint    Time      Forward  DLL       First\n"));
  fprintf (file,
	   _("                 Table   Stamp     Chain    Name      Thunk\n"));

  data = (bfd_byte *) bfd_malloc (dataoff + datasize);
  if (data == NULL)
    return false;

  /* Read the whole section.  Some of the fields might be before dataoff.  */
  if (! bfd_get_section_contents (abfd, section, (PTR) data,
				  0, dataoff + datasize))
    return false;

  adj = section->vma - extra->ImageBase;

  for (i = 0; i < datasize; i += onaline)
    {
      bfd_vma hint_addr;
      bfd_vma time_stamp;
      bfd_vma forward_chain;
      bfd_vma dll_name;
      bfd_vma first_thunk;
      int idx = 0;
      bfd_size_type j;
      char *dll;

      /* print (i + extra->DataDirectory[1].VirtualAddress)  */
      fprintf (file, " %08lx\t", (unsigned long) (i + adj + dataoff));

      if (i + 20 > datasize)
	{
	  /* Check stuff.  */
	  ;
	}

      hint_addr = bfd_get_32 (abfd, data + i + dataoff);
      time_stamp = bfd_get_32 (abfd, data + i + 4 + dataoff);
      forward_chain = bfd_get_32 (abfd, data + i + 8 + dataoff);
      dll_name = bfd_get_32 (abfd, data + i + 12 + dataoff);
      first_thunk = bfd_get_32 (abfd, data + i + 16 + dataoff);

      fprintf (file, "%08lx %08lx %08lx %08lx %08lx\n",
	       (unsigned long) hint_addr,
	       (unsigned long) time_stamp,
	       (unsigned long) forward_chain,
	       (unsigned long) dll_name,
	       (unsigned long) first_thunk);

      if (hint_addr == 0 && first_thunk == 0)
	break;

      dll = (char *) data + dll_name - adj;
      fprintf (file, _("\n\tDLL Name: %s\n"), dll);

      if (hint_addr != 0)
	{
	  fprintf (file, _("\tvma:  Hint/Ord Member-Name\n"));

	  idx = hint_addr - adj;

	  for (j = 0; j < datasize; j += 4)
	    {
	      unsigned long member = bfd_get_32 (abfd, data + idx + j);

	      if (member == 0)
		break;
	      if (member & 0x80000000)
		fprintf (file, "\t%04lx\t %4lu", member,
			 member & 0x7fffffff);
	      else
		{
		  int ordinal;
		  char *member_name;

		  ordinal = bfd_get_16 (abfd, data + member - adj);
		  member_name = (char *) data + member - adj + 2;
		  fprintf (file, "\t%04lx\t %4d  %s",
			   member, ordinal, member_name);
		}

	      /* If the time stamp is not zero, the import address
                 table holds actual addresses.  */
	      if (time_stamp != 0
		  && first_thunk != 0
		  && first_thunk != hint_addr)
		fprintf (file, "\t%04lx",
			 (long) bfd_get_32 (abfd, data + first_thunk - adj + j));

	      fprintf (file, "\n");
	    }
	}

      if (hint_addr != first_thunk && time_stamp == 0)
	{
	  int differ = 0;
	  int idx2;

	  idx2 = first_thunk - adj;

	  for (j = 0; j < datasize; j += 4)
	    {
	      int ordinal;
	      char *member_name;
	      bfd_vma hint_member = 0;
	      bfd_vma iat_member;

	      if (hint_addr != 0)
		hint_member = bfd_get_32 (abfd, data + idx + j);
	      iat_member = bfd_get_32 (abfd, data + idx2 + j);

	      if (hint_addr == 0 && iat_member == 0)
		break;

	      if (hint_addr == 0 || hint_member != iat_member)
		{
		  if (differ == 0)
		    {
		      fprintf (file,
			       _("\tThe Import Address Table (difference found)\n"));
		      fprintf (file, _("\tvma:  Hint/Ord Member-Name\n"));
		      differ = 1;
		    }
		  if (iat_member == 0)
		    {
		      fprintf (file,
			      _("\t>>> Ran out of IAT members!\n"));
		    }
		  else
		    {
		      ordinal = bfd_get_16 (abfd, data + iat_member - adj);
		      member_name = (char *) data + iat_member - adj + 2;
		      fprintf (file, "\t%04lx\t %4d  %s\n",
			      (unsigned long) iat_member,
			      ordinal,
			      member_name);
		    }
		}

	      if (hint_addr != 0 && hint_member == 0)
		break;
	    }
	  if (differ == 0)
	    {
	      fprintf (file,
		      _("\tThe Import Address Table is identical\n"));
	    }
	}

      fprintf (file, "\n");

    }

  free (data);

  return true;
}

static boolean
pe_print_edata (abfd, vfile)
     bfd *abfd;
     PTR vfile;
{
  FILE *file = (FILE *) vfile;
  bfd_byte *data;
  asection *section;

  bfd_size_type datasize = 0;
  bfd_size_type dataoff;
  bfd_size_type i;

  bfd_signed_vma adj;
  struct EDT_type {
    long export_flags;             /* reserved - should be zero */
    long time_stamp;
    short major_ver;
    short minor_ver;
    bfd_vma name;                  /* rva - relative to image base */
    long base;                     /* ordinal base */
    unsigned long num_functions;   /* Number in the export address table */
    unsigned long num_names;       /* Number in the name pointer table */
    bfd_vma eat_addr;    /* rva to the export address table */
    bfd_vma npt_addr;        /* rva to the Export Name Pointer Table */
    bfd_vma ot_addr; /* rva to the Ordinal Table */
  } edt;

  pe_data_type *pe = pe_data (abfd);
  struct internal_extra_pe_aouthdr *extra = &pe->pe_opthdr;

  bfd_vma addr;

  addr = extra->DataDirectory[0].VirtualAddress;

  if (addr == 0 && extra->DataDirectory[0].Size == 0)
    {
      /* Maybe the extra header isn't there.  Look for the section.  */
      section = bfd_get_section_by_name (abfd, ".edata");
      if (section == NULL)
	return true;

      addr = section->vma;
      datasize = bfd_section_size (abfd, section);
      if (datasize == 0)
	return true;
    }
  else
    {
      addr += extra->ImageBase;
      for (section = abfd->sections; section != NULL; section = section->next)
	{
	  datasize = bfd_section_size (abfd, section);
	  if (addr >= section->vma && addr < section->vma + datasize)
	    break;
	}

      if (section == NULL)
	{
	  fprintf (file,
		   _("\nThere is an export table, but the section containing it could not be found\n"));
	  return true;
	}
    }

  fprintf (file, _("\nThere is an export table in %s at 0x%lx\n"),
	   section->name, (unsigned long) addr);

  dataoff = addr - section->vma;
  datasize -= dataoff;

  data = (bfd_byte *) bfd_malloc (datasize);
  if (data == NULL)
    return false;

  if (! bfd_get_section_contents (abfd, section, (PTR) data, dataoff,
				  datasize))
    return false;

  /* Go get Export Directory Table.  */
  edt.export_flags   = bfd_get_32 (abfd, data +  0);
  edt.time_stamp     = bfd_get_32 (abfd, data +  4);
  edt.major_ver      = bfd_get_16 (abfd, data +  8);
  edt.minor_ver      = bfd_get_16 (abfd, data + 10);
  edt.name           = bfd_get_32 (abfd, data + 12);
  edt.base           = bfd_get_32 (abfd, data + 16);
  edt.num_functions  = bfd_get_32 (abfd, data + 20);
  edt.num_names      = bfd_get_32 (abfd, data + 24);
  edt.eat_addr       = bfd_get_32 (abfd, data + 28);
  edt.npt_addr       = bfd_get_32 (abfd, data + 32);
  edt.ot_addr        = bfd_get_32 (abfd, data + 36);

  adj = section->vma - extra->ImageBase + dataoff;

  /* Dump the EDT first first */
  fprintf (file,
	   _("\nThe Export Tables (interpreted %s section contents)\n\n"),
	   section->name);

  fprintf (file,
	   _("Export Flags \t\t\t%lx\n"), (unsigned long) edt.export_flags);

  fprintf (file,
	   _("Time/Date stamp \t\t%lx\n"), (unsigned long) edt.time_stamp);

  fprintf (file,
	   _("Major/Minor \t\t\t%d/%d\n"), edt.major_ver, edt.minor_ver);

  fprintf (file,
	   _("Name \t\t\t\t"));
  fprintf_vma (file, edt.name);
  fprintf (file,
	   " %s\n", data + edt.name - adj);

  fprintf (file,
	   _("Ordinal Base \t\t\t%ld\n"), edt.base);

  fprintf (file,
	   _("Number in:\n"));

  fprintf (file,
	   _("\tExport Address Table \t\t%08lx\n"),
	   edt.num_functions);

  fprintf (file,
	   _("\t[Name Pointer/Ordinal] Table\t%08lx\n"), edt.num_names);

  fprintf (file,
	   _("Table Addresses\n"));

  fprintf (file,
	   _("\tExport Address Table \t\t"));
  fprintf_vma (file, edt.eat_addr);
  fprintf (file, "\n");

  fprintf (file,
	   _("\tName Pointer Table \t\t"));
  fprintf_vma (file, edt.npt_addr);
  fprintf (file, "\n");

  fprintf (file,
	   _("\tOrdinal Table \t\t\t"));
  fprintf_vma (file, edt.ot_addr);
  fprintf (file, "\n");

  /* The next table to find is the Export Address Table. It's basically
     a list of pointers that either locate a function in this dll, or
     forward the call to another dll. Something like:
      typedef union {
        long export_rva;
        long forwarder_rva;
      } export_address_table_entry;
  */

  fprintf (file,
	  _("\nExport Address Table -- Ordinal Base %ld\n"),
	  edt.base);

  for (i = 0; i < edt.num_functions; ++i)
    {
      bfd_vma eat_member = bfd_get_32 (abfd,
				       data + edt.eat_addr + (i * 4) - adj);
      if (eat_member == 0)
	continue;

      if (eat_member - adj <= datasize)
	{
	  /* This rva is to a name (forwarding function) in our section.  */
	  /* Should locate a function descriptor.  */
	  fprintf (file,
		   "\t[%4ld] +base[%4ld] %04lx %s -- %s\n",
		   (long) i,
		   (long) (i + edt.base),
		   (unsigned long) eat_member,
		   _("Forwarder RVA"),
		   data + eat_member - adj);
	}
      else
	{
	  /* Should locate a function descriptor in the reldata section.  */
	  fprintf (file,
		   "\t[%4ld] +base[%4ld] %04lx %s\n",
		   (long) i,
		   (long) (i + edt.base),
		   (unsigned long) eat_member,
		   _("Export RVA"));
	}
    }

  /* The Export Name Pointer Table is paired with the Export Ordinal Table.  */
  /* Dump them in parallel for clarity.  */
  fprintf (file,
	   _("\n[Ordinal/Name Pointer] Table\n"));

  for (i = 0; i < edt.num_names; ++i)
    {
      bfd_vma name_ptr = bfd_get_32 (abfd,
				    data +
				    edt.npt_addr
				    + (i*4) - adj);

      char *name = (char *) data + name_ptr - adj;

      bfd_vma ord = bfd_get_16 (abfd,
				    data +
				    edt.ot_addr
				    + (i*2) - adj);
      fprintf (file,
	      "\t[%4ld] %s\n", (long) ord, name);
    }

  free (data);

  return true;
}

/* This really is architecture dependent.  On IA-64, a .pdata entry
   consists of three dwords containing relative virtual addresses that
   specify the start and end address of the code range the entry
   covers and the address of the corresponding unwind info data.  */

static boolean
pe_print_pdata (abfd, vfile)
     bfd *abfd;
     PTR vfile;
{
#ifdef COFF_WITH_pep
# define PDATA_ROW_SIZE	(3*8)
#else
# define PDATA_ROW_SIZE	(5*4)
#endif
  FILE *file = (FILE *) vfile;
  bfd_byte *data = 0;
  asection *section = bfd_get_section_by_name (abfd, ".pdata");
  bfd_size_type datasize = 0;
  bfd_size_type i;
  bfd_size_type start, stop;
  int onaline = PDATA_ROW_SIZE;

  if (section == NULL
      || coff_section_data (abfd, section) == NULL
      || pei_section_data (abfd, section) == NULL)
    return true;

  stop = pei_section_data (abfd, section)->virt_size;
  if ((stop % onaline) != 0)
    fprintf (file,
	     _("Warning, .pdata section size (%ld) is not a multiple of %d\n"),
	     (long) stop, onaline);

  fprintf (file,
	   _("\nThe Function Table (interpreted .pdata section contents)\n"));
#ifdef COFF_WITH_pep
  fprintf (file,
	   _(" vma:\t\t\tBegin Address    End Address      Unwind Info\n"));
#else
  fprintf (file,
	   _(" vma:\t\tBegin    End      EH       EH       PrologEnd  Exception\n"));
  fprintf (file,
	   _("     \t\tAddress  Address  Handler  Data     Address    Mask\n"));
#endif

  if (bfd_section_size (abfd, section) == 0)
    return true;

  data = (bfd_byte *) bfd_malloc ((size_t) bfd_section_size (abfd, section));
  datasize = bfd_section_size (abfd, section);
  if (data == NULL && datasize != 0)
    return false;

  bfd_get_section_contents (abfd,
			    section,
			    (PTR) data, 0,
			    bfd_section_size (abfd, section));

  start = 0;

  for (i = start; i < stop; i += onaline)
    {
      bfd_vma begin_addr;
      bfd_vma end_addr;
      bfd_vma eh_handler;
      bfd_vma eh_data;
      bfd_vma prolog_end_addr;
      int em_data;

      if (i + PDATA_ROW_SIZE > stop)
	break;

      begin_addr      = GET_PDATA_ENTRY (abfd, data + i     );
      end_addr        = GET_PDATA_ENTRY (abfd, data + i +  4);
      eh_handler      = GET_PDATA_ENTRY (abfd, data + i +  8);
      eh_data         = GET_PDATA_ENTRY (abfd, data + i + 12);
      prolog_end_addr = GET_PDATA_ENTRY (abfd, data + i + 16);

      if (begin_addr == 0 && end_addr == 0 && eh_handler == 0
	  && eh_data == 0 && prolog_end_addr == 0)
	{
	  /* We are probably into the padding of the section now.  */
	  break;
	}

      em_data = ((eh_handler & 0x1) << 2) | (prolog_end_addr & 0x3);
      eh_handler &= ~(bfd_vma) 0x3;
      prolog_end_addr &= ~(bfd_vma) 0x3;

      fputc (' ', file);
      fprintf_vma (file, i + section->vma); fputc ('\t', file);
      fprintf_vma (file, begin_addr); fputc (' ', file);
      fprintf_vma (file, end_addr); fputc (' ', file);
      fprintf_vma (file, eh_handler);
#ifndef COFF_WITH_pep
      fputc (' ', file);
      fprintf_vma (file, eh_data); fputc (' ', file);
      fprintf_vma (file, prolog_end_addr);
      fprintf (file, "   %x", em_data);
#endif

#ifdef POWERPC_LE_PE
      if (eh_handler == 0 && eh_data != 0)
	{
	  /* Special bits here, although the meaning may be a little
	     mysterious. The only one I know for sure is 0x03.  */
	  /* Code Significance                           */
	  /* 0x00 None                                   */
	  /* 0x01 Register Save Millicode                */
	  /* 0x02 Register Restore Millicode             */
	  /* 0x03 Glue Code Sequence                     */
	  switch (eh_data)
	    {
	    case 0x01:
	      fprintf (file, _(" Register save millicode"));
	      break;
	    case 0x02:
	      fprintf (file, _(" Register restore millicode"));
	      break;
	    case 0x03:
	      fprintf (file, _(" Glue code sequence"));
	      break;
	    default:
	      break;
	    }
	}
#endif
      fprintf (file, "\n");
    }

  free (data);

  return true;
}

#define IMAGE_REL_BASED_HIGHADJ 4
static const char * const tbl[] = {
  "ABSOLUTE",
  "HIGH",
  "LOW",
  "HIGHLOW",
  "HIGHADJ",
  "MIPS_JMPADDR",
  "SECTION",
  "REL32",
  "RESERVED1",
  "MIPS_JMPADDR16",
  "DIR64",
  "HIGH3ADJ"
  "UNKNOWN",   /* MUST be last */
};

static boolean
pe_print_reloc (abfd, vfile)
     bfd *abfd;
     PTR vfile;
{
  FILE *file = (FILE *) vfile;
  bfd_byte *data = 0;
  asection *section = bfd_get_section_by_name (abfd, ".reloc");
  bfd_size_type datasize = 0;
  bfd_size_type i;
  bfd_size_type start, stop;

  if (section == NULL)
    return true;

  if (bfd_section_size (abfd, section) == 0)
    return true;

  fprintf (file,
	   _("\n\nPE File Base Relocations (interpreted .reloc section contents)\n"));

  data = (bfd_byte *) bfd_malloc ((size_t) bfd_section_size (abfd, section));
  datasize = bfd_section_size (abfd, section);
  if (data == NULL && datasize != 0)
    return false;

  bfd_get_section_contents (abfd,
			    section,
			    (PTR) data, 0,
			    bfd_section_size (abfd, section));

  start = 0;

  stop = bfd_section_size (abfd, section);

  for (i = start; i < stop;)
    {
      int j;
      bfd_vma virtual_address;
      long number, size;

      /* The .reloc section is a sequence of blocks, with a header consisting
	 of two 32 bit quantities, followed by a number of 16 bit entries */

      virtual_address = bfd_get_32 (abfd, data+i);
      size = bfd_get_32 (abfd, data+i+4);
      number = (size - 8) / 2;

      if (size == 0)
	{
	  break;
	}

      fprintf (file,
	       _("\nVirtual Address: %08lx Chunk size %ld (0x%lx) Number of fixups %ld\n"),
	       (unsigned long) virtual_address, size, size, number);

      for (j = 0; j < number; ++j)
	{
	  unsigned short e = bfd_get_16 (abfd, data + i + 8 + j * 2);
	  unsigned int t = (e & 0xF000) >> 12;
	  int off = e & 0x0FFF;

	  if (t >= sizeof (tbl) / sizeof (tbl[0]))
	    t = (sizeof (tbl) / sizeof (tbl[0])) - 1;

	  fprintf (file,
		   _("\treloc %4d offset %4x [%4lx] %s"),
		   j, off, (long) (off + virtual_address), tbl[t]);

	  /* HIGHADJ takes an argument, - the next record *is* the
	     low 16 bits of addend.  */
	  if (t == IMAGE_REL_BASED_HIGHADJ)
	    {
	      fprintf (file, " (%4x)",
		       ((unsigned int)
			bfd_get_16 (abfd, data + i + 8 + j * 2 + 2)));
	      j++;
	    }

	  fprintf (file, "\n");
	}
      i += size;
    }

  free (data);

  return true;
}

/* Print out the program headers.  */

boolean
_bfd_XX_print_private_bfd_data_common (abfd, vfile)
     bfd *abfd;
     PTR vfile;
{
  FILE *file = (FILE *) vfile;
  int j;
  pe_data_type *pe = pe_data (abfd);
  struct internal_extra_pe_aouthdr *i = &pe->pe_opthdr;
  const char *subsystem_name = NULL;

  /* The MS dumpbin program reportedly ands with 0xff0f before
     printing the characteristics field.  Not sure why.  No reason to
     emulate it here.  */
  fprintf (file, _("\nCharacteristics 0x%x\n"), pe->real_flags);
#undef PF
#define PF(x, y) if (pe->real_flags & x) { fprintf (file, "\t%s\n", y); }
  PF (F_RELFLG, "relocations stripped");
  PF (F_EXEC, "executable");
  PF (F_LNNO, "line numbers stripped");
  PF (F_LSYMS, "symbols stripped");
  PF (0x80, "little endian");
  PF (F_AR32WR, "32 bit words");
  PF (0x200, "debugging information removed");
  PF (0x1000, "system file");
  PF (F_DLL, "DLL");
  PF (0x8000, "big endian");
#undef PF

  /* ctime implies '\n'.  */
  {
    time_t t = pe->coff.timestamp;
    fprintf (file, "\nTime/Date\t\t%s", ctime (&t));
  }
  fprintf (file, "\nImageBase\t\t");
  fprintf_vma (file, i->ImageBase);
  fprintf (file, "\nSectionAlignment\t");
  fprintf_vma (file, i->SectionAlignment);
  fprintf (file, "\nFileAlignment\t\t");
  fprintf_vma (file, i->FileAlignment);
  fprintf (file, "\nMajorOSystemVersion\t%d\n", i->MajorOperatingSystemVersion);
  fprintf (file, "MinorOSystemVersion\t%d\n", i->MinorOperatingSystemVersion);
  fprintf (file, "MajorImageVersion\t%d\n", i->MajorImageVersion);
  fprintf (file, "MinorImageVersion\t%d\n", i->MinorImageVersion);
  fprintf (file, "MajorSubsystemVersion\t%d\n", i->MajorSubsystemVersion);
  fprintf (file, "MinorSubsystemVersion\t%d\n", i->MinorSubsystemVersion);
  fprintf (file, "Win32Version\t\t%08lx\n", i->Reserved1);
  fprintf (file, "SizeOfImage\t\t%08lx\n", i->SizeOfImage);
  fprintf (file, "SizeOfHeaders\t\t%08lx\n", i->SizeOfHeaders);
  fprintf (file, "CheckSum\t\t%08lx\n", i->CheckSum);
  switch (i->Subsystem)
    {
    case IMAGE_SUBSYSTEM_UNKNOWN:
      subsystem_name = "unspecified";
      break;
    case IMAGE_SUBSYSTEM_NATIVE:
      subsystem_name = "NT native";
      break;
    case IMAGE_SUBSYSTEM_WINDOWS_GUI:
      subsystem_name = "Windows GUI";
      break;
    case IMAGE_SUBSYSTEM_WINDOWS_CUI:
      subsystem_name = "Windows CUI";
      break;
    case IMAGE_SUBSYSTEM_POSIX_CUI:
      subsystem_name = "POSIX CUI";
      break;
    case IMAGE_SUBSYSTEM_WINDOWS_CE_GUI:
      subsystem_name = "Wince CUI";
      break;
    case IMAGE_SUBSYSTEM_EFI_APPLICATION:
      subsystem_name = "EFI application";
      break;
    case IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER:
      subsystem_name = "EFI boot service driver";
      break;
    case IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER:
      subsystem_name = "EFI runtime driver";
      break;
    }
  fprintf (file, "Subsystem\t\t%08x", i->Subsystem);
  if (subsystem_name)
    fprintf (file, "\t(%s)", subsystem_name);
  fprintf (file, "\nDllCharacteristics\t%08x\n", i->DllCharacteristics);
  fprintf (file, "SizeOfStackReserve\t");
  fprintf_vma (file, i->SizeOfStackReserve);
  fprintf (file, "\nSizeOfStackCommit\t");
  fprintf_vma (file, i->SizeOfStackCommit);
  fprintf (file, "\nSizeOfHeapReserve\t");
  fprintf_vma (file, i->SizeOfHeapReserve);
  fprintf (file, "\nSizeOfHeapCommit\t");
  fprintf_vma (file, i->SizeOfHeapCommit);
  fprintf (file, "\nLoaderFlags\t\t%08lx\n", i->LoaderFlags);
  fprintf (file, "NumberOfRvaAndSizes\t%08lx\n", i->NumberOfRvaAndSizes);

  fprintf (file, "\nThe Data Directory\n");
  for (j = 0; j < IMAGE_NUMBEROF_DIRECTORY_ENTRIES; j++)
    {
      fprintf (file, "Entry %1x ", j);
      fprintf_vma (file, i->DataDirectory[j].VirtualAddress);
      fprintf (file, " %08lx ", i->DataDirectory[j].Size);
      fprintf (file, "%s\n", dir_names[j]);
    }

  pe_print_idata (abfd, vfile);
  pe_print_edata (abfd, vfile);
  pe_print_pdata (abfd, vfile);
  pe_print_reloc (abfd, vfile);

  return true;
}

/* Copy any private info we understand from the input bfd
   to the output bfd.  */

boolean
_bfd_XX_bfd_copy_private_bfd_data_common (ibfd, obfd)
     bfd *ibfd, *obfd;
{
  /* One day we may try to grok other private data.  */
  if (ibfd->xvec->flavour != bfd_target_coff_flavour
      || obfd->xvec->flavour != bfd_target_coff_flavour)
    return true;

  pe_data (obfd)->pe_opthdr = pe_data (ibfd)->pe_opthdr;
  pe_data (obfd)->dll = pe_data (ibfd)->dll;

  /* for strip: if we removed .reloc, we'll make a real mess of things
     if we don't remove this entry as well.  */
  if (! pe_data (obfd)->has_reloc_section)
    {
      pe_data (obfd)->pe_opthdr.DataDirectory[5].VirtualAddress = 0;
      pe_data (obfd)->pe_opthdr.DataDirectory[5].Size = 0;
    }
  return true;
}

/* Copy private section data.  */
boolean
_bfd_XX_bfd_copy_private_section_data (ibfd, isec, obfd, osec)
     bfd *ibfd;
     asection *isec;
     bfd *obfd;
     asection *osec;
{
  if (bfd_get_flavour (ibfd) != bfd_target_coff_flavour
      || bfd_get_flavour (obfd) != bfd_target_coff_flavour)
    return true;

  if (coff_section_data (ibfd, isec) != NULL
      && pei_section_data (ibfd, isec) != NULL)
    {
      if (coff_section_data (obfd, osec) == NULL)
	{
	  osec->used_by_bfd =
	    (PTR) bfd_zalloc (obfd, sizeof (struct coff_section_tdata));
	  if (osec->used_by_bfd == NULL)
	    return false;
	}
      if (pei_section_data (obfd, osec) == NULL)
	{
	  coff_section_data (obfd, osec)->tdata =
	    (PTR) bfd_zalloc (obfd, sizeof (struct pei_section_tdata));
	  if (coff_section_data (obfd, osec)->tdata == NULL)
	    return false;
	}
      pei_section_data (obfd, osec)->virt_size =
	pei_section_data (ibfd, isec)->virt_size;
      pei_section_data (obfd, osec)->pe_flags =
	pei_section_data (ibfd, isec)->pe_flags;
    }

  return true;
}

void
_bfd_XX_get_symbol_info (abfd, symbol, ret)
     bfd *abfd;
     asymbol *symbol;
     symbol_info *ret;
{
  coff_get_symbol_info (abfd, symbol, ret);
#if 0 /* This code no longer appears to be necessary.
	 ImageBase has already been added in by coff_swap_scnhdr_in.  */
  if (pe_data (abfd) != NULL
      && ((symbol->flags & BSF_DEBUGGING) == 0
	  || (symbol->flags & BSF_DEBUGGING_RELOC) != 0)
      && ! bfd_is_abs_section (symbol->section))
    ret->value += pe_data (abfd)->pe_opthdr.ImageBase;
#endif
}

/* Handle the .idata section and other things that need symbol table
   access.  */

boolean
_bfd_XXi_final_link_postscript (abfd, pfinfo)
     bfd *abfd;
     struct coff_final_link_info *pfinfo;
{
  struct coff_link_hash_entry *h1;
  struct bfd_link_info *info = pfinfo->info;

  /* There are a few fields that need to be filled in now while we
     have symbol table access.

     The .idata subsections aren't directly available as sections, but
     they are in the symbol table, so get them from there.  */

  /* The import directory.  This is the address of .idata$2, with size
     of .idata$2 + .idata$3.  */
  h1 = coff_link_hash_lookup (coff_hash_table (info),
			      ".idata$2", false, false, true);
  if (h1 != NULL)
    {
      pe_data (abfd)->pe_opthdr.DataDirectory[1].VirtualAddress =
	(h1->root.u.def.value
	 + h1->root.u.def.section->output_section->vma
	 + h1->root.u.def.section->output_offset);
      h1 = coff_link_hash_lookup (coff_hash_table (info),
				  ".idata$4", false, false, true);
      pe_data (abfd)->pe_opthdr.DataDirectory[1].Size =
	((h1->root.u.def.value
	  + h1->root.u.def.section->output_section->vma
	  + h1->root.u.def.section->output_offset)
	 - pe_data (abfd)->pe_opthdr.DataDirectory[1].VirtualAddress);

      /* The import address table.  This is the size/address of
         .idata$5.  */
      h1 = coff_link_hash_lookup (coff_hash_table (info),
				  ".idata$5", false, false, true);
      pe_data (abfd)->pe_opthdr.DataDirectory[12].VirtualAddress =
	(h1->root.u.def.value
	 + h1->root.u.def.section->output_section->vma
	 + h1->root.u.def.section->output_offset);
      h1 = coff_link_hash_lookup (coff_hash_table (info),
				  ".idata$6", false, false, true);
      pe_data (abfd)->pe_opthdr.DataDirectory[12].Size =
	((h1->root.u.def.value
	  + h1->root.u.def.section->output_section->vma
	  + h1->root.u.def.section->output_offset)
	 - pe_data (abfd)->pe_opthdr.DataDirectory[12].VirtualAddress);
    }

  /* If we couldn't find idata$2, we either have an excessively
     trivial program or are in DEEP trouble; we have to assume trivial
     program....  */
  return true;
}
