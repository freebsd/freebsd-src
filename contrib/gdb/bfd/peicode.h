/* Support for the generic parts of most COFF variants, for BFD.
   Copyright 1995 Free Software Foundation, Inc.
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

/*
Most of this hacked by  Steve Chamberlain,
			sac@cygnus.com
*/



#define coff_bfd_print_private_bfd_data pe_print_private_bfd_data
#define coff_mkobject pe_mkobject
#define coff_mkobject_hook pe_mkobject_hook

#ifndef GET_FCN_LNNOPTR
#define GET_FCN_LNNOPTR(abfd, ext) \
     bfd_h_get_32(abfd, (bfd_byte *) ext->x_sym.x_fcnary.x_fcn.x_lnnoptr)
#endif

#ifndef GET_FCN_ENDNDX
#define GET_FCN_ENDNDX(abfd, ext)  \
	bfd_h_get_32(abfd, (bfd_byte *) ext->x_sym.x_fcnary.x_fcn.x_endndx)
#endif

#ifndef PUT_FCN_LNNOPTR
#define PUT_FCN_LNNOPTR(abfd, in, ext)  bfd_h_put_32(abfd,  in, (bfd_byte *) ext->x_sym.x_fcnary.x_fcn.x_lnnoptr)
#endif
#ifndef PUT_FCN_ENDNDX
#define PUT_FCN_ENDNDX(abfd, in, ext) bfd_h_put_32(abfd, in, (bfd_byte *) ext->x_sym.x_fcnary.x_fcn.x_endndx)
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



/**********************************************************************/

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

  reloc_dst->r_type = bfd_h_get_16(abfd, (bfd_byte *) reloc_src->r_type);

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

  bfd_h_put_16(abfd, reloc_src->r_type, (bfd_byte *)
	       reloc_dst->r_type);

#ifdef SWAP_OUT_RELOC_OFFSET
  SWAP_OUT_RELOC_OFFSET(abfd,
			reloc_src->r_offset,
			(bfd_byte *) reloc_dst->r_offset);
#endif
#ifdef SWAP_OUT_RELOC_EXTRA
  SWAP_OUT_RELOC_EXTRA(abfd,reloc_src, reloc_dst);
#endif
  return sizeof(struct external_reloc);
}


static void
coff_swap_filehdr_in (abfd, src, dst)
     bfd            *abfd;
     PTR	     src;
     PTR	     dst;
{
  FILHDR *filehdr_src = (FILHDR *) src;
  struct internal_filehdr *filehdr_dst = (struct internal_filehdr *) dst;
  filehdr_dst->f_magic = bfd_h_get_16(abfd, (bfd_byte *) filehdr_src->f_magic);
  filehdr_dst->f_nscns = bfd_h_get_16(abfd, (bfd_byte *)filehdr_src-> f_nscns);
  filehdr_dst->f_timdat = bfd_h_get_32(abfd, (bfd_byte *)filehdr_src-> f_timdat);

  filehdr_dst->f_nsyms = bfd_h_get_32(abfd, (bfd_byte *)filehdr_src-> f_nsyms);
  filehdr_dst->f_flags = bfd_h_get_16(abfd, (bfd_byte *)filehdr_src-> f_flags);
  filehdr_dst->f_symptr = bfd_h_get_32 (abfd, (bfd_byte *) filehdr_src->f_symptr);

  /* Other people's tools sometimes generate headers
     with an nsyms but a zero symptr. */
  if (filehdr_dst->f_nsyms && filehdr_dst->f_symptr)
    {
      filehdr_dst->f_flags |= HAS_SYMS;
    }
  else 
    {
      filehdr_dst->f_symptr = 0;
      filehdr_dst->f_nsyms = 0;
      filehdr_dst->f_flags &= ~HAS_SYMS;
    }

  filehdr_dst->f_opthdr = bfd_h_get_16(abfd, 
				       (bfd_byte *)filehdr_src-> f_opthdr);
}

#ifdef COFF_IMAGE_WITH_PE

static  unsigned int
coff_swap_filehdr_out (abfd, in, out)
     bfd       *abfd;
     PTR	in;
     PTR	out;
{
  int idx;
  struct internal_filehdr *filehdr_in = (struct internal_filehdr *)in;
  FILHDR *filehdr_out = (FILHDR *)out;

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

  for (idx=0; idx < 4; idx++)
    filehdr_in->pe.e_res[idx] = 0x0;

  filehdr_in->pe.e_oemid   = 0x0;
  filehdr_in->pe.e_oeminfo = 0x0;

  for (idx=0; idx < 10; idx++)
    filehdr_in->pe.e_res2[idx] = 0x0;

  filehdr_in->pe.e_lfanew = 0x80;

  /* this next collection of data are mostly just characters.  It appears
     to be constant within the headers put on NT exes */
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



  bfd_h_put_16(abfd, filehdr_in->f_magic, (bfd_byte *) filehdr_out->f_magic);
  bfd_h_put_16(abfd, filehdr_in->f_nscns, (bfd_byte *) filehdr_out->f_nscns);

  bfd_h_put_32(abfd, time (0), (bfd_byte *) filehdr_out->f_timdat);
  PUT_FILEHDR_SYMPTR (abfd, (bfd_vma) filehdr_in->f_symptr,
		      (bfd_byte *) filehdr_out->f_symptr);
  bfd_h_put_32(abfd, filehdr_in->f_nsyms, (bfd_byte *) filehdr_out->f_nsyms);
  bfd_h_put_16(abfd, filehdr_in->f_opthdr, (bfd_byte *) filehdr_out->f_opthdr);
  bfd_h_put_16(abfd, filehdr_in->f_flags, (bfd_byte *) filehdr_out->f_flags);

  /* put in extra dos header stuff.  This data remains essentially
     constant, it just has to be tacked on to the beginning of all exes 
     for NT */
  bfd_h_put_16(abfd, filehdr_in->pe.e_magic, (bfd_byte *) filehdr_out->e_magic);
  bfd_h_put_16(abfd, filehdr_in->pe.e_cblp, (bfd_byte *) filehdr_out->e_cblp);
  bfd_h_put_16(abfd, filehdr_in->pe.e_cp, (bfd_byte *) filehdr_out->e_cp);
  bfd_h_put_16(abfd, filehdr_in->pe.e_crlc, (bfd_byte *) filehdr_out->e_crlc);
  bfd_h_put_16(abfd, filehdr_in->pe.e_cparhdr, 
	       (bfd_byte *) filehdr_out->e_cparhdr);
  bfd_h_put_16(abfd, filehdr_in->pe.e_minalloc, 
	       (bfd_byte *) filehdr_out->e_minalloc);
  bfd_h_put_16(abfd, filehdr_in->pe.e_maxalloc, 
	       (bfd_byte *) filehdr_out->e_maxalloc);
  bfd_h_put_16(abfd, filehdr_in->pe.e_ss, (bfd_byte *) filehdr_out->e_ss);
  bfd_h_put_16(abfd, filehdr_in->pe.e_sp, (bfd_byte *) filehdr_out->e_sp);
  bfd_h_put_16(abfd, filehdr_in->pe.e_csum, (bfd_byte *) filehdr_out->e_csum);
  bfd_h_put_16(abfd, filehdr_in->pe.e_ip, (bfd_byte *) filehdr_out->e_ip);
  bfd_h_put_16(abfd, filehdr_in->pe.e_cs, (bfd_byte *) filehdr_out->e_cs);
  bfd_h_put_16(abfd, filehdr_in->pe.e_lfarlc, (bfd_byte *) filehdr_out->e_lfarlc);
  bfd_h_put_16(abfd, filehdr_in->pe.e_ovno, (bfd_byte *) filehdr_out->e_ovno);
  {
    int idx;
    for (idx=0; idx < 4; idx++)
      bfd_h_put_16(abfd, filehdr_in->pe.e_res[idx], 
		   (bfd_byte *) filehdr_out->e_res[idx]);
  }
  bfd_h_put_16(abfd, filehdr_in->pe.e_oemid, (bfd_byte *) filehdr_out->e_oemid);
  bfd_h_put_16(abfd, filehdr_in->pe.e_oeminfo,
	       (bfd_byte *) filehdr_out->e_oeminfo);
  {
    int idx;
    for (idx=0; idx < 10; idx++)
      bfd_h_put_16(abfd, filehdr_in->pe.e_res2[idx],
		   (bfd_byte *) filehdr_out->e_res2[idx]);
  }
  bfd_h_put_32(abfd, filehdr_in->pe.e_lfanew, (bfd_byte *) filehdr_out->e_lfanew);

  {
    int idx;
    for (idx=0; idx < 16; idx++)
      bfd_h_put_32(abfd, filehdr_in->pe.dos_message[idx],
		   (bfd_byte *) filehdr_out->dos_message[idx]);
  }

  /* also put in the NT signature */
  bfd_h_put_32(abfd, filehdr_in->pe.nt_signature, 
	       (bfd_byte *) filehdr_out->nt_signature);




  return sizeof(FILHDR);
}
#else

static  unsigned int
coff_swap_filehdr_out (abfd, in, out)
     bfd       *abfd;
     PTR	in;
     PTR	out;
{
  struct internal_filehdr *filehdr_in = (struct internal_filehdr *)in;
  FILHDR *filehdr_out = (FILHDR *)out;

  bfd_h_put_16(abfd, filehdr_in->f_magic, (bfd_byte *) filehdr_out->f_magic);
  bfd_h_put_16(abfd, filehdr_in->f_nscns, (bfd_byte *) filehdr_out->f_nscns);
  bfd_h_put_32(abfd, filehdr_in->f_timdat, (bfd_byte *) filehdr_out->f_timdat);
  PUT_FILEHDR_SYMPTR (abfd, (bfd_vma) filehdr_in->f_symptr,
		      (bfd_byte *) filehdr_out->f_symptr);
  bfd_h_put_32(abfd, filehdr_in->f_nsyms, (bfd_byte *) filehdr_out->f_nsyms);
  bfd_h_put_16(abfd, filehdr_in->f_opthdr, (bfd_byte *) filehdr_out->f_opthdr);
  bfd_h_put_16(abfd, filehdr_in->f_flags, (bfd_byte *) filehdr_out->f_flags);

  return sizeof(FILHDR);
}

#endif


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

  /* The section symbols for the .idata$ sections have class 68, which MS
     documentation indicates is a section symbol.  The problem is that the
     value field in the symbol is simply a copy of the .idata section's flags
     rather than something useful.  When these symbols are encountered, change
     the value to 0 and the section number to 1 so that they will be handled
     somewhat correctly in the bfd code. */
  if (in->n_sclass == 0x68) {
    in->n_value = 0x0;
    in->n_scnum = 1;
    /* I have tried setting the class to 3 and using the following to set
       the section number.  This will put the address of the pointer to the
       string kernel32.dll at addresses 0 and 0x10 off start of idata section
       which is not correct */
    /*    if (strcmp (in->_n._n_name, ".idata$4") == 0) */
    /*      in->n_scnum = 3; */
    /*    else */
    /*      in->n_scnum = 2; */
  }

#ifdef coff_swap_sym_in_hook
  coff_swap_sym_in_hook(abfd, ext1, in1);
#endif
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

  return sizeof(SYMENT);
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
    return;


  case C_STAT:
#ifdef C_LEAFSTAT
  case C_LEAFSTAT:
#endif
  case C_HIDDEN:
    if (type == T_NULL) {
      in->x_scn.x_scnlen = GET_SCN_SCNLEN(abfd, ext);
      in->x_scn.x_nreloc = GET_SCN_NRELOC(abfd, ext);
      in->x_scn.x_nlinno = GET_SCN_NLINNO(abfd, ext);
      return;
    }
    break;
  }

  in->x_sym.x_tagndx.l = bfd_h_get_32(abfd, (bfd_byte *) ext->x_sym.x_tagndx);
#ifndef NO_TVNDX
  in->x_sym.x_tvndx = bfd_h_get_16(abfd, (bfd_byte *) ext->x_sym.x_tvndx);
#endif

  if (class == C_BLOCK || ISFCN (type) || ISTAG (class))
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

  memset((PTR)ext, 0, AUXESZ);
  switch (class) {
  case C_FILE:
    if (in->x_file.x_fname[0] == 0) {
      bfd_h_put_32(abfd, 0, (bfd_byte *) ext->x_file.x_n.x_zeroes);
      bfd_h_put_32(abfd,
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
    return sizeof (AUXENT);


  case C_STAT:
#ifdef C_LEAFSTAT
  case C_LEAFSTAT:
#endif
  case C_HIDDEN:
    if (type == T_NULL) {
      PUT_SCN_SCNLEN(abfd, in->x_scn.x_scnlen, ext);
      PUT_SCN_NRELOC(abfd, in->x_scn.x_nreloc, ext);
      PUT_SCN_NLINNO(abfd, in->x_scn.x_nlinno, ext);
      return sizeof (AUXENT);
    }
    break;
  }

  bfd_h_put_32(abfd, in->x_sym.x_tagndx.l, (bfd_byte *) ext->x_sym.x_tagndx);
#ifndef NO_TVNDX
  bfd_h_put_16(abfd, in->x_sym.x_tvndx , (bfd_byte *) ext->x_sym.x_tvndx);
#endif

  if (class == C_BLOCK || ISFCN (type) || ISTAG (class))
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
    bfd_h_put_32 (abfd, in->x_sym.x_misc.x_fsize,
	     (bfd_byte *)  ext->x_sym.x_misc.x_fsize);
  else
    {
      PUT_LNSZ_LNNO (abfd, in->x_sym.x_misc.x_lnsz.x_lnno, ext);
      PUT_LNSZ_SIZE (abfd, in->x_sym.x_misc.x_lnsz.x_size, ext);
    }

  return sizeof(AUXENT);
}


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
  bfd_h_put_32(abfd, in->l_addr.l_symndx, (bfd_byte *)
	  ext->l_addr.l_symndx);

  PUT_LINENO_LNNO (abfd, in->l_lnno, ext);
  return sizeof(struct external_lineno);
}



static void
coff_swap_aouthdr_in (abfd, aouthdr_ext1, aouthdr_int1)
     bfd            *abfd;
     PTR aouthdr_ext1;
     PTR aouthdr_int1;
{
  struct internal_extra_pe_aouthdr *a;
  PEAOUTHDR *src = (PEAOUTHDR *)(aouthdr_ext1);
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

  a = &aouthdr_int->pe;
  a->ImageBase = bfd_h_get_32 (abfd, src->ImageBase);
  a->SectionAlignment = bfd_h_get_32 (abfd, src->SectionAlignment);
  a->FileAlignment = bfd_h_get_32 (abfd, src->FileAlignment);
  a->MajorOperatingSystemVersion = 
    bfd_h_get_16 (abfd, src->MajorOperatingSystemVersion);
  a->MinorOperatingSystemVersion = 
    bfd_h_get_16 (abfd, src->MinorOperatingSystemVersion);
  a->MajorImageVersion = bfd_h_get_16 (abfd, src->MajorImageVersion);
  a->MinorImageVersion = bfd_h_get_16 (abfd, src->MinorImageVersion);
  a->MajorSubsystemVersion = bfd_h_get_16 (abfd, src->MajorSubsystemVersion);
  a->MinorSubsystemVersion = bfd_h_get_16 (abfd, src->MinorSubsystemVersion);
  a->Reserved1 = bfd_h_get_32 (abfd, src->Reserved1);
  a->SizeOfImage = bfd_h_get_32 (abfd, src->SizeOfImage);
  a->SizeOfHeaders = bfd_h_get_32 (abfd, src->SizeOfHeaders);
  a->CheckSum = bfd_h_get_32 (abfd, src->CheckSum);
  a->Subsystem = bfd_h_get_16 (abfd, src->Subsystem);
  a->DllCharacteristics = bfd_h_get_16 (abfd, src->DllCharacteristics);
  a->SizeOfStackReserve = bfd_h_get_32 (abfd, src->SizeOfStackReserve);
  a->SizeOfStackCommit = bfd_h_get_32 (abfd, src->SizeOfStackCommit);
  a->SizeOfHeapReserve = bfd_h_get_32 (abfd, src->SizeOfHeapReserve);
  a->SizeOfHeapCommit = bfd_h_get_32 (abfd, src->SizeOfHeapCommit);
  a->LoaderFlags = bfd_h_get_32 (abfd, src->LoaderFlags);
  a->NumberOfRvaAndSizes = bfd_h_get_32 (abfd, src->NumberOfRvaAndSizes);

  {
    int idx;
    for (idx=0; idx < 16; idx++)
      {
	a->DataDirectory[idx].VirtualAddress =
	  bfd_h_get_32 (abfd, src->DataDirectory[idx][0]);
	a->DataDirectory[idx].Size =
	  bfd_h_get_32 (abfd, src->DataDirectory[idx][1]);
      }
  }

  if (aouthdr_int->entry)
    aouthdr_int->entry += a->ImageBase;
  if (aouthdr_int->tsize) 
    aouthdr_int->text_start += a->ImageBase;
  if (aouthdr_int->dsize) 
    aouthdr_int->data_start += a->ImageBase;
}


static void add_data_entry (abfd, aout, idx, name, base)
     bfd *abfd;
     struct internal_extra_pe_aouthdr *aout;
     int idx;
     char *name;
     bfd_vma base;
{
  asection *sec = bfd_get_section_by_name (abfd, name);

  /* add import directory information if it exists */
  if (sec != NULL)
    {
      aout->DataDirectory[idx].VirtualAddress = sec->vma - base;
      aout->DataDirectory[idx].Size = sec->_cooked_size;
      sec->flags |= SEC_DATA;
    }
}

static unsigned int
coff_swap_aouthdr_out (abfd, in, out)
     bfd       *abfd;
     PTR	in;
     PTR	out;
{
  struct internal_aouthdr *aouthdr_in = (struct internal_aouthdr *)in;
  struct internal_extra_pe_aouthdr *extra = &pe_data (abfd)->pe_opthdr;
  PEAOUTHDR *aouthdr_out = (PEAOUTHDR *)out;

  bfd_vma sa = extra->SectionAlignment;
  bfd_vma fa = extra->FileAlignment;
  bfd_vma ib = extra->ImageBase ;

  if (aouthdr_in->tsize) 
    aouthdr_in->text_start -= ib;
  if (aouthdr_in->dsize) 
    aouthdr_in->data_start -= ib;
  if (aouthdr_in->entry) 
    aouthdr_in->entry -= ib;

#define FA(x)  (((x) + fa -1 ) & (- fa))
#define SA(x)  (((x) + sa -1 ) & (- sa))

  /* We like to have the sizes aligned */

  aouthdr_in->bsize = FA (aouthdr_in->bsize);


  extra->NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

  /* first null out all data directory entries .. */
  memset (extra->DataDirectory, sizeof (extra->DataDirectory), 0);

  add_data_entry (abfd, extra, 0, ".edata", ib);
  add_data_entry (abfd, extra, 1, ".idata", ib);
  add_data_entry (abfd, extra, 2, ".rsrc" ,ib);

#ifdef POWERPC_LE_PE
  /* FIXME: do other PE platforms use this? */
  add_data_entry (abfd, extra, 3, ".pdata" ,ib);
#endif

  add_data_entry (abfd, extra, 5, ".reloc", ib);

#ifdef POWERPC_LE_PE
  /* On the PPC NT system, this field is set up as follows. It is
     not an "officially" reserved field, so it currently has no title.
     first_thunk_address is idata$5, and the thunk_size is the size
     of the idata$5 chunk of the idata section.
  */
  extra->DataDirectory[12].VirtualAddress = first_thunk_address;
  extra->DataDirectory[12].Size = thunk_size;

  /* On the PPC NT system, the size of the directory entry is not the
     size of the entire section. It's actually offset to the end of 
     the idata$3 component of the idata section. This is the size of
     the entire import table. (also known as the start of idata$4)
  */
  extra->DataDirectory[1].Size = import_table_size;
#endif

  {
    asection *sec;
    bfd_vma dsize= 0;
    bfd_vma isize = SA(abfd->sections->filepos);
    bfd_vma tsize= 0;

    for (sec = abfd->sections; sec; sec = sec->next)
      {
	int rounded = FA(sec->_raw_size);

	if (strcmp(sec->name,".junk") == 0)
	  {
	    continue;
	  }

	if (sec->flags & SEC_DATA) 
	  dsize += rounded;
	if (sec->flags & SEC_CODE)
	  tsize += rounded;
	isize += SA(rounded);
      }

    aouthdr_in->dsize = dsize;
    aouthdr_in->tsize = tsize;
    extra->SizeOfImage = isize;
  }

  extra->SizeOfHeaders = abfd->sections->filepos;
  bfd_h_put_16(abfd, aouthdr_in->magic, (bfd_byte *) aouthdr_out->standard.magic);

#ifdef POWERPC_LE_PE
  /* this little piece of magic sets the "linker version" field to 2.60 */
  bfd_h_put_16(abfd, 2  + 60 * 256, (bfd_byte *) aouthdr_out->standard.vstamp);
#else
  /* this little piece of magic sets the "linker version" field to 2.55 */
  bfd_h_put_16(abfd, 2  + 55 * 256, (bfd_byte *) aouthdr_out->standard.vstamp);
#endif

  PUT_AOUTHDR_TSIZE (abfd, aouthdr_in->tsize, (bfd_byte *) aouthdr_out->standard.tsize);
  PUT_AOUTHDR_DSIZE (abfd, aouthdr_in->dsize, (bfd_byte *) aouthdr_out->standard.dsize);
  PUT_AOUTHDR_BSIZE (abfd, aouthdr_in->bsize, (bfd_byte *) aouthdr_out->standard.bsize);
  PUT_AOUTHDR_ENTRY (abfd, aouthdr_in->entry, (bfd_byte *) aouthdr_out->standard.entry);
  PUT_AOUTHDR_TEXT_START (abfd, aouthdr_in->text_start,
			  (bfd_byte *) aouthdr_out->standard.text_start);

  PUT_AOUTHDR_DATA_START (abfd, aouthdr_in->data_start,
			  (bfd_byte *) aouthdr_out->standard.data_start);


  bfd_h_put_32 (abfd, extra->ImageBase, 
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
  bfd_h_put_32 (abfd, extra->SizeOfStackReserve,
		(bfd_byte *) aouthdr_out->SizeOfStackReserve);
  bfd_h_put_32 (abfd, extra->SizeOfStackCommit,
		(bfd_byte *) aouthdr_out->SizeOfStackCommit);
  bfd_h_put_32 (abfd, extra->SizeOfHeapReserve,
		(bfd_byte *) aouthdr_out->SizeOfHeapReserve);
  bfd_h_put_32 (abfd, extra->SizeOfHeapCommit,
		(bfd_byte *) aouthdr_out->SizeOfHeapCommit);
  bfd_h_put_32 (abfd, extra->LoaderFlags,
		(bfd_byte *) aouthdr_out->LoaderFlags);
  bfd_h_put_32 (abfd, extra->NumberOfRvaAndSizes,
		(bfd_byte *) aouthdr_out->NumberOfRvaAndSizes);
  {
    int idx;
    for (idx=0; idx < 16; idx++)
      {
	bfd_h_put_32 (abfd, extra->DataDirectory[idx].VirtualAddress,
		      (bfd_byte *) aouthdr_out->DataDirectory[idx][0]);
	bfd_h_put_32 (abfd, extra->DataDirectory[idx].Size,
		      (bfd_byte *) aouthdr_out->DataDirectory[idx][1]);
      }
  }

  return sizeof(AOUTHDR);
}

static void
    coff_swap_scnhdr_in (abfd, ext, in)
      bfd            *abfd;
  PTR	     ext;
  PTR	     in;
{
  SCNHDR *scnhdr_ext = (SCNHDR *) ext;
  struct internal_scnhdr *scnhdr_int = (struct internal_scnhdr *) in;

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
  scnhdr_int->s_flags = bfd_h_get_32(abfd, (bfd_byte *) scnhdr_ext->s_flags);

  scnhdr_int->s_nreloc = bfd_h_get_16(abfd, (bfd_byte *) scnhdr_ext->s_nreloc);
  scnhdr_int->s_nlnno = bfd_h_get_16(abfd, (bfd_byte *) scnhdr_ext->s_nlnno);

  if (scnhdr_int->s_vaddr != 0) 
    {
      scnhdr_int->s_vaddr += pe_data (abfd)->pe_opthdr.ImageBase;
    }
  if (strcmp (scnhdr_int->s_name, _BSS) == 0) 
    {
      scnhdr_int->s_size = scnhdr_int->s_paddr;
      scnhdr_int->s_paddr = 0;
    }
}

static unsigned int
coff_swap_scnhdr_out (abfd, in, out)
     bfd       *abfd;
     PTR	in;
     PTR	out;
{
  struct internal_scnhdr *scnhdr_int = (struct internal_scnhdr *)in;
  SCNHDR *scnhdr_ext = (SCNHDR *)out;
  unsigned int ret = sizeof (SCNHDR);
  bfd_vma ps;
  bfd_vma ss;

  memcpy(scnhdr_ext->s_name, scnhdr_int->s_name, sizeof(scnhdr_int->s_name));

  PUT_SCNHDR_VADDR (abfd, 
		    (scnhdr_int->s_vaddr 
		     - pe_data(abfd)->pe_opthdr.ImageBase),
		    (bfd_byte *) scnhdr_ext->s_vaddr);

  /* NT wants the size data to be rounded up to the next NT_FILE_ALIGNMENT
     value except for the BSS section, its s_size should be 0 */


  if (strcmp (scnhdr_int->s_name, _BSS) == 0) 
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
    if (strcmp (scnhdr_int->s_name, ".data")  == 0 ||
	strcmp (scnhdr_int->s_name, ".CRT")   == 0 ||
	strcmp (scnhdr_int->s_name, ".rsrc")  == 0 ||
	strcmp (scnhdr_int->s_name, ".bss")   == 0)
      flags |= IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
    else if (strcmp (scnhdr_int->s_name, ".text") == 0)
      flags |= IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
    else if (strcmp (scnhdr_int->s_name, ".reloc") == 0)
      flags = SEC_DATA| IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_DISCARDABLE;
    else if (strcmp (scnhdr_int->s_name, ".idata") == 0)
      flags = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | SEC_DATA;     
    else if (strcmp (scnhdr_int->s_name, ".rdata") == 0
	     || strcmp (scnhdr_int->s_name, ".edata") == 0)
      flags =  IMAGE_SCN_MEM_READ | SEC_DATA;     
    /* ppc-nt additions */
    else if (strcmp (scnhdr_int->s_name, ".pdata") == 0)
      flags = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_ALIGN_4BYTES |
			  IMAGE_SCN_MEM_READ ;
    /* Remember this field is a max of 8 chars, so the null is _not_ there
       for an 8 character name like ".reldata". (yep. Stupid bug) */
    else if (strncmp (scnhdr_int->s_name, ".reldata", strlen(".reldata")) == 0)
      flags =  IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_ALIGN_8BYTES |
	       IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE ;
    else if (strcmp (scnhdr_int->s_name, ".ydata") == 0)
      flags =  IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_ALIGN_8BYTES |
	       IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE ;
    else if (strcmp (scnhdr_int->s_name, ".drectve") == 0)
      flags =  IMAGE_SCN_LNK_INFO | IMAGE_SCN_LNK_REMOVE ;
    /* end of ppc-nt additions */
#ifdef POWERPC_LE_PE
    else if (strncmp (scnhdr_int->s_name, ".stabstr", strlen(".stabstr")) == 0)
      {
	flags =  IMAGE_SCN_LNK_INFO;
      }
    else if (strcmp (scnhdr_int->s_name, ".stab") == 0)
      {
	flags =  IMAGE_SCN_LNK_INFO;
      }
#endif

    bfd_h_put_32(abfd, flags, (bfd_byte *) scnhdr_ext->s_flags);
  }

  if (scnhdr_int->s_nlnno <= 0xffff)
    bfd_h_put_16(abfd, scnhdr_int->s_nlnno, (bfd_byte *) scnhdr_ext->s_nlnno);
  else
    {
      (*_bfd_error_handler) ("%s: line number overflow: 0x%lx > 0xffff",
			     bfd_get_filename (abfd),
			     scnhdr_int->s_nlnno);
      bfd_set_error (bfd_error_file_truncated);
      bfd_h_put_16 (abfd, 0xffff, (bfd_byte *) scnhdr_ext->s_nlnno);
      ret = 0;
    }
  if (scnhdr_int->s_nreloc <= 0xffff)
    bfd_h_put_16(abfd, scnhdr_int->s_nreloc, (bfd_byte *) scnhdr_ext->s_nreloc);
  else
    {
      (*_bfd_error_handler) ("%s: reloc overflow: 0x%lx > 0xffff",
			     bfd_get_filename (abfd),
			     scnhdr_int->s_nreloc);
      bfd_set_error (bfd_error_file_truncated);
      bfd_h_put_16 (abfd, 0xffff, (bfd_byte *) scnhdr_ext->s_nreloc);
      ret = 0;
    }
  return ret;
}

static char * dir_names[IMAGE_NUMBEROF_DIRECTORY_ENTRIES] = 
{
  "Export Directory [.edata]",
  "Import Directory [parts of .idata]",
  "Resource Directory [.rsrc]",
  "Exception Directory [.pdata]",
  "Security Directory",
  "Base Relocation Directory [.reloc]",
  "Debug Directory",
  "Description Directory",
  "Special Directory",
  "Thread Storage Directory [.tls]",
  "Load Configuration Directory",
  "Bound Import Directory",
  "Import Address Table Directory",
  "Reserved",
  "Reserved",
  "Reserved"
};

/**********************************************************************/
static boolean
pe_print_idata(abfd, vfile)
     bfd*abfd;
     void *vfile;
{
  FILE *file = vfile;
  bfd_byte *data = 0;
  asection *section = bfd_get_section_by_name (abfd, ".idata");

#ifdef POWERPC_LE_PE
  asection *rel_section = bfd_get_section_by_name (abfd, ".reldata");
#endif

  bfd_size_type datasize = 0;
  bfd_size_type i;
  bfd_size_type start, stop;
  int onaline = 20;
  bfd_vma addr_value;
  bfd_vma loadable_toc_address;
  bfd_vma toc_address;
  bfd_vma start_address;

  pe_data_type *pe = pe_data (abfd);
  struct internal_extra_pe_aouthdr *extra = &pe->pe_opthdr;

  if (section == 0)
    return true;

#ifdef POWERPC_LE_PE
  if (rel_section != 0 && bfd_section_size (abfd, rel_section) != 0)
    {
      /* The toc address can be found by taking the starting address,
	 which on the PPC locates a function descriptor. The descriptor
	 consists of the function code starting address followed by the
	 address of the toc. The starting address we get from the bfd,
	 and the descriptor is supposed to be in the .reldata section. 
      */

      bfd_byte *data = 0;
      int offset;
      data = (bfd_byte *) bfd_malloc ((size_t) bfd_section_size (abfd, 
								 rel_section));
      if (data == NULL && bfd_section_size (abfd, rel_section) != 0)
	return false;

      datasize = bfd_section_size (abfd, rel_section);
  
      bfd_get_section_contents (abfd, 
				rel_section, 
				(PTR) data, 0, 
				bfd_section_size (abfd, rel_section));

      offset = abfd->start_address - rel_section->vma;

      start_address = bfd_get_32(abfd, data+offset);
      loadable_toc_address = bfd_get_32(abfd, data+offset+4);
      toc_address = loadable_toc_address - 32768;

      fprintf(file,
	      "\nFunction descriptor located at the start address: %04lx\n",
	      (unsigned long int) (abfd->start_address));
      fprintf (file,
	       "\tcode-base %08lx toc (loadable/actual) %08lx/%08lx\n", 
	       start_address, loadable_toc_address, toc_address);
    }
  else
    {
      loadable_toc_address = 0; 
      toc_address = 0; 
      start_address = 0;
    }
#endif

  fprintf(file,
	  "\nThe Import Tables (interpreted .idata section contents)\n");
  fprintf(file,
	  " vma:    Hint    Time      Forward  DLL       First\n");
  fprintf(file,
	  "         Table   Stamp     Chain    Name      Thunk\n");

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

  stop = bfd_section_size (abfd, section);

  for (i = start; i < stop; i += onaline)
    {
      bfd_vma hint_addr;
      bfd_vma time_stamp;
      bfd_vma forward_chain;
      bfd_vma dll_name;
      bfd_vma first_thunk;
      int idx;
      int j;
      char *dll;
      int adj = extra->ImageBase - section->vma;

      fprintf (file,
	       " %04lx\t", 
	       (unsigned long int) (i + section->vma));
      
      if (i+20 > stop)
	{
	  /* check stuff */
	  ;
	}
      
      hint_addr = bfd_get_32(abfd, data+i);
      time_stamp = bfd_get_32(abfd, data+i+4);
      forward_chain = bfd_get_32(abfd, data+i+8);
      dll_name = bfd_get_32(abfd, data+i+12);
      first_thunk = bfd_get_32(abfd, data+i+16);
      
      fprintf(file, "%08lx %08lx %08lx %08lx %08lx\n",
	      hint_addr,
	      time_stamp,
	      forward_chain,
	      dll_name,
	      first_thunk);

      if (hint_addr ==0)
	{
	  break;
	}

      /* the image base is present in the section->vma */
      dll = data + dll_name + adj;
      fprintf(file, "\n\tDLL Name: %s\n", dll);
      fprintf(file, "\tvma:  Ordinal  Member-Name\n");

      idx = hint_addr + adj;

      for (j=0;j<stop;j+=4)
	{
	  int ordinal;
	  char *member_name;
	  bfd_vma member = bfd_get_32(abfd, data + idx + j);
	  if (member == 0)
	    break;
	  ordinal = bfd_get_16(abfd,
			       data + member + adj);
	  member_name = data + member + adj + 2;
	  fprintf(file, "\t%04lx\t %4d  %s\n",
		  member, ordinal, member_name);
	}

      if (hint_addr != first_thunk) 
	{
	  int differ = 0;
	  int idx2;

	  idx2 = first_thunk + adj;

	  for (j=0;j<stop;j+=4)
	    {
	      int ordinal;
	      char *member_name;
	      bfd_vma hint_member = bfd_get_32(abfd, data + idx + j);
	      bfd_vma iat_member = bfd_get_32(abfd, data + idx2 + j);
	      if (hint_member != iat_member)
		{
		  if (differ == 0)
		    {
		      fprintf(file, 
			      "\tThe Import Address Table (difference found)\n");
		      fprintf(file, "\tvma:  Ordinal  Member-Name\n");
		      differ = 1;
		    }
		  if (iat_member == 0)
		    {
		      fprintf(file,
			      "\t>>> Ran out of IAT members!\n");
		    }
		  else 
		    {
		      ordinal = bfd_get_16(abfd,
					   data + iat_member + adj);
		      member_name = data + iat_member + adj + 2;
		      fprintf(file, "\t%04lx\t %4d  %s\n",
			      iat_member, ordinal, member_name);
		    }
		  break;
		}
	      if (hint_member == 0)
		break;
	    }
	  if (differ == 0)
	    {
	      fprintf(file,
		      "\tThe Import Address Table is identical\n");
	    }
	}

      fprintf(file, "\n");

    }

  free (data);
}

static boolean
pe_print_edata(abfd, vfile)
     bfd*abfd;
     void *vfile;
{
  FILE *file = vfile;
  bfd_byte *data = 0;
  asection *section = bfd_get_section_by_name (abfd, ".edata");

  bfd_size_type datasize = 0;
  bfd_size_type i;

  int adj;
  struct EDT_type 
    {
      long export_flags;             /* reserved - should be zero */
      long time_stamp;
      short major_ver;
      short minor_ver;
      bfd_vma name;                  /* rva - relative to image base */
      long base;                     /* ordinal base */
      long num_functions;        /* Number in the export address table */
      long num_names;            /* Number in the name pointer table */
      bfd_vma eat_addr;    /* rva to the export address table */
      bfd_vma npt_addr;        /* rva to the Export Name Pointer Table */
      bfd_vma ot_addr; /* rva to the Ordinal Table */
    } edt;

  pe_data_type *pe = pe_data (abfd);
  struct internal_extra_pe_aouthdr *extra = &pe->pe_opthdr;

  if (section == 0)
    return true;

  data = (bfd_byte *) bfd_malloc ((size_t) bfd_section_size (abfd, 
							     section));
  datasize = bfd_section_size (abfd, section);

  if (data == NULL && datasize != 0)
    return false;

  bfd_get_section_contents (abfd, 
			    section, 
			    (PTR) data, 0, 
			    bfd_section_size (abfd, section));

  /* Go get Export Directory Table */
  edt.export_flags   = bfd_get_32(abfd, data+0); 
  edt.time_stamp     = bfd_get_32(abfd, data+4);
  edt.major_ver      = bfd_get_16(abfd, data+8);
  edt.minor_ver      = bfd_get_16(abfd, data+10);
  edt.name           = bfd_get_32(abfd, data+12);
  edt.base           = bfd_get_32(abfd, data+16);
  edt.num_functions  = bfd_get_32(abfd, data+20); 
  edt.num_names      = bfd_get_32(abfd, data+24); 
  edt.eat_addr       = bfd_get_32(abfd, data+28);
  edt.npt_addr       = bfd_get_32(abfd, data+32); 
  edt.ot_addr        = bfd_get_32(abfd, data+36);

  adj = extra->ImageBase - section->vma;


  /* Dump the EDT first first */
  fprintf(file,
	  "\nThe Export Tables (interpreted .edata section contents)\n\n");

  fprintf(file,
	  "Export Flags \t\t\t%x\n",edt.export_flags);

  fprintf(file,
	  "Time/Date stamp \t\t%x\n",edt.time_stamp);

  fprintf(file,
	  "Major/Minor \t\t\t%d/%d\n", edt.major_ver, edt.minor_ver);

  fprintf(file,
	  "Name \t\t\t\t%x %s\n", edt.name, data + edt.name + adj);

  fprintf(file,
	  "Ordinal Base \t\t\t%d\n", edt.base);

  fprintf(file,
	  "Number in:\n");

  fprintf(file,
	  "\tExport Address Table \t\t%x\n", edt.num_functions);

  fprintf(file,
	  "\t[Name Pointer/Ordinal] Table\t%d\n", edt.num_names);

  fprintf(file,
	  "Table Addresses\n");

  fprintf(file,
	  "\tExport Address Table \t\t%x\n",
	  edt.eat_addr);

  fprintf(file,
	  "\tName Pointer Table \t\t%x\n",
	  edt.npt_addr);

  fprintf(file,
	  "\tOrdinal Table \t\t\t%x\n",
	  edt.ot_addr);

  
  /* The next table to find si the Export Address Table. It's basically
     a list of pointers that either locate a function in this dll, or
     forward the call to another dll. Something like:
      typedef union 
      {
        long export_rva;
        long forwarder_rva;
      } export_address_table_entry;
  */

  fprintf(file,
	  "\nExport Address Table -- Ordinal Base %d\n",
	  edt.base);

  for (i = 0; i < edt.num_functions; ++i)
    {
      bfd_vma eat_member = bfd_get_32(abfd, 
				      data + edt.eat_addr + (i*4) + adj);
      bfd_vma eat_actual = extra->ImageBase + eat_member;
      bfd_vma edata_start = bfd_get_section_vma(abfd,section);
      bfd_vma edata_end = edata_start + bfd_section_size (abfd, section);


      if (eat_member == 0)
	continue;

      if (edata_start < eat_actual && eat_actual < edata_end) 
	{
	  /* this rva is to a name (forwarding function) in our section */
	  /* Should locate a function descriptor */
	  fprintf(file,
		  "\t[%4d] +base[%4d] %04lx %s -- %s\n", 
		  i, i+edt.base, eat_member, "Forwarder RVA",
		  data + eat_member + adj);
	}
      else
	{
	  /* Should locate a function descriptor in the reldata section */
	  fprintf(file,
		  "\t[%4d] +base[%4d] %04lx %s\n", 
		  i, i+edt.base, eat_member, "Export RVA");
	}
    }

  /* The Export Name Pointer Table is paired with the Export Ordinal Table */
  /* Dump them in parallel for clarity */
  fprintf(file,
	  "\n[Ordinal/Name Pointer] Table\n");

  for (i = 0; i < edt.num_names; ++i)
    {
      bfd_vma name_ptr = bfd_get_32(abfd, 
				    data + 
				    edt.npt_addr
				    + (i*4) + adj);
      
      char *name = data + name_ptr + adj;

      bfd_vma ord = bfd_get_16(abfd, 
				    data + 
				    edt.ot_addr
				    + (i*2) + adj);
      fprintf(file,
	      "\t[%4d] %s\n", ord, name);

    }

  free (data);
}

static boolean
pe_print_pdata(abfd, vfile)
     bfd*abfd;
     void *vfile;
{
  FILE *file = vfile;
  bfd_byte *data = 0;
  asection *section = bfd_get_section_by_name (abfd, ".pdata");
  bfd_size_type datasize = 0;
  bfd_size_type i;
  bfd_size_type start, stop;
  int onaline = 20;
  bfd_vma addr_value;

  if (section == 0)
    return true;

  fprintf(file,
	  "\nThe Function Table (interpreted .pdata section contents)\n");
  fprintf(file,
	  " vma:\t\tBegin    End      EH       EH       PrologEnd\n");
  fprintf(file,
	  "     \t\tAddress  Address  Handler  Data     Address\n");

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

  stop = bfd_section_size (abfd, section);

  for (i = start; i < stop; i += onaline)
    {
      bfd_vma begin_addr;
      bfd_vma end_addr;
      bfd_vma eh_handler;
      bfd_vma eh_data;
      bfd_vma prolog_end_addr;

      if (i+20 > stop)
	  break;
      
      begin_addr = bfd_get_32(abfd, data+i);
      end_addr = bfd_get_32(abfd, data+i+4);
      eh_handler = bfd_get_32(abfd, data+i+8);
      eh_data = bfd_get_32(abfd, data+i+12);
      prolog_end_addr = bfd_get_32(abfd, data+i+16);
      
      if (begin_addr == 0 && end_addr == 0 && eh_handler == 0
	  && eh_data == 0 && prolog_end_addr == 0)
	{
	  /* We are probably into the padding of the
	     section now */
	  break;
	}

      fprintf (file,
	       " %08lx\t", 
	       (unsigned long int) (i + section->vma));

      fprintf(file, "%08lx %08lx %08lx %08lx %08lx",
	      begin_addr,
	      end_addr,
	      eh_handler,
	      eh_data,
	      prolog_end_addr);

#ifdef POWERPC_LE_PE
      if (eh_handler == 0 && eh_data != 0)
	{
	  /* Special bits here, although the meaning may */
	  /* be a little mysterious. The only one I know */
	  /* for sure is 0x03.                           */
	  /* Code Significance                           */
	  /* 0x00 None                                   */
	  /* 0x01 Register Save Millicode                */
	  /* 0x02 Register Restore Millicode             */
	  /* 0x03 Glue Code Sequence                     */
	  switch (eh_data)
	    {
	    case 0x01:
	      fprintf(file, " Register save millicode");
	      break;
	    case 0x02:
	      fprintf(file, " Register restore millicode");
	      break;
	    case 0x03:
	      fprintf(file, " Glue code sequence");
	      break;
	    default:
	      break;
	    }
	}
#endif	   
      fprintf(file, "\n");
    }

  free (data);
}

static const char *tbl[6] =
{
"ABSOLUTE",
"HIGH",
"LOW",
"HIGHLOW",
"HIGHADJ",
"unknown"
};

static boolean
pe_print_reloc(abfd, vfile)
     bfd*abfd;
     void *vfile;
{
  FILE *file = vfile;
  bfd_byte *data = 0;
  asection *section = bfd_get_section_by_name (abfd, ".reloc");
  bfd_size_type datasize = 0;
  bfd_size_type i;
  bfd_size_type start, stop;
  int onaline = 20;
  bfd_vma addr_value;

  if (section == 0)
    return true;

  if (bfd_section_size (abfd, section) == 0)
    return true;

  fprintf(file,
	  "\n\nPE File Base Relocations (interpreted .reloc"
	  " section contents)\n");

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

      virtual_address = bfd_get_32(abfd, data+i);
      size = bfd_get_32(abfd, data+i+4);
      number = (size - 8) / 2;

      if (size == 0) 
	{
	  break;
	}

      fprintf (file,
	       "\nVirtual Address: %08lx Chunk size %d (0x%x) "
	       "Number of fixups %d\n", 
	       virtual_address, size, size, number);

      for (j = 0; j < number; ++j)
	{
	  unsigned short e = bfd_get_16(abfd, data + i + 8 + j*2);
	  int t =   (e & 0xF000) >> 12;
	  int off = e & 0x0FFF;

	  if (t > 5) 
	    abort();

	  fprintf(file,
		  "\treloc %4d offset %4x [%4x] %s\n", 
		  j, off, off+virtual_address, tbl[t]);
	  
	}
      i += size;
    }

  free (data);
}

static boolean
pe_print_private_bfd_data (abfd, vfile)
     bfd *abfd;
     PTR vfile;
{
  FILE *file = (FILE *) vfile;
  int j;
  pe_data_type *pe = pe_data (abfd);
  struct internal_extra_pe_aouthdr *i = &pe->pe_opthdr;

  fprintf (file,"\nImageBase\t\t");
  fprintf_vma (file, i->ImageBase);
  fprintf (file,"\nSectionAlignment\t");
  fprintf_vma (file, i->SectionAlignment);
  fprintf (file,"\nFileAlignment\t\t");
  fprintf_vma (file, i->FileAlignment);
  fprintf (file,"\nMajorOSystemVersion\t%d\n", i->MajorOperatingSystemVersion);
  fprintf (file,"MinorOSystemVersion\t%d\n", i->MinorOperatingSystemVersion);
  fprintf (file,"MajorImageVersion\t%d\n", i->MajorImageVersion);
  fprintf (file,"MinorImageVersion\t%d\n", i->MinorImageVersion);
  fprintf (file,"MajorSubsystemVersion\t%d\n", i->MajorSubsystemVersion);
  fprintf (file,"MinorSubsystemVersion\t%d\n", i->MinorSubsystemVersion);
  fprintf (file,"Reserved1\t\t%08lx\n", i->Reserved1);
  fprintf (file,"SizeOfImage\t\t%08lx\n", i->SizeOfImage);
  fprintf (file,"SizeOfHeaders\t\t%08lx\n", i->SizeOfHeaders);
  fprintf (file,"CheckSum\t\t%08lx\n", i->CheckSum);
  fprintf (file,"Subsystem\t\t%08x\n", i->Subsystem);
  fprintf (file,"DllCharacteristics\t%08x\n", i->DllCharacteristics);
  fprintf (file,"SizeOfStackReserve\t");
  fprintf_vma (file, i->SizeOfStackReserve);
  fprintf (file,"\nSizeOfStackCommit\t");
  fprintf_vma (file, i->SizeOfStackCommit);
  fprintf (file,"\nSizeOfHeapReserve\t");
  fprintf_vma (file, i->SizeOfHeapReserve);
  fprintf (file,"\nSizeOfHeapCommit\t");
  fprintf_vma (file, i->SizeOfHeapCommit);
  fprintf (file,"\nLoaderFlags\t\t%08lx\n", i->LoaderFlags);
  fprintf (file,"NumberOfRvaAndSizes\t%08lx\n", i->NumberOfRvaAndSizes);

  fprintf (file,"\nThe Data Directory\n");
  for (j = 0; j < IMAGE_NUMBEROF_DIRECTORY_ENTRIES; j++) 
    {
      fprintf (file, "Entry %1x ", j);
      fprintf_vma (file, i->DataDirectory[j].VirtualAddress);
      fprintf (file, " %08lx ", i->DataDirectory[j].Size);
      fprintf (file, "%s\n", dir_names[j]);
    }

  pe_print_idata(abfd, vfile);
  pe_print_edata(abfd, vfile);
  pe_print_pdata(abfd, vfile);
  pe_print_reloc(abfd, vfile);

  return true;
}

static boolean
pe_mkobject (abfd)
     bfd * abfd;
{
  pe_data_type *pe;
  abfd->tdata.pe_obj_data = 
    (struct pe_tdata *) bfd_zalloc (abfd, sizeof (pe_data_type));

  if (abfd->tdata.pe_obj_data == 0)
    return false;

  pe = pe_data (abfd);

  pe->coff.pe = 1;
  pe->in_reloc_p = in_reloc_p;
  return true;
}

/* Create the COFF backend specific information.  */
static PTR
pe_mkobject_hook (abfd, filehdr, aouthdr)
     bfd * abfd;
     PTR filehdr;
     PTR aouthdr;
{
  struct internal_filehdr *internal_f = (struct internal_filehdr *) filehdr;
  pe_data_type *pe;

  if (pe_mkobject (abfd) == false)
    return NULL;

  pe = pe_data (abfd);
  pe->coff.sym_filepos = internal_f->f_symptr;
  /* These members communicate important constants about the symbol
     table to GDB's symbol-reading code.  These `constants'
     unfortunately vary among coff implementations...  */
  pe->coff.local_n_btmask = N_BTMASK;
  pe->coff.local_n_btshft = N_BTSHFT;
  pe->coff.local_n_tmask = N_TMASK;
  pe->coff.local_n_tshift = N_TSHIFT;
  pe->coff.local_symesz = SYMESZ;
  pe->coff.local_auxesz = AUXESZ;
  pe->coff.local_linesz = LINESZ;

  obj_raw_syment_count (abfd) =
    obj_conv_table_size (abfd) =
      internal_f->f_nsyms;

  pe->real_flags = internal_f->f_flags;

#ifdef COFF_IMAGE_WITH_PE
  if (aouthdr) 
    {
      pe->pe_opthdr = ((struct internal_aouthdr *)aouthdr)->pe;
    }
#endif

  return (PTR) pe;
}



/* Copy any private info we understand from the input bfd
   to the output bfd.  */

#define coff_bfd_copy_private_bfd_data pe_bfd_copy_private_bfd_data

static boolean
pe_bfd_copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd, *obfd;
{
  /* One day we may try to grok other private data.  */
  if (ibfd->xvec->flavour != bfd_target_coff_flavour
      || obfd->xvec->flavour != bfd_target_coff_flavour)
    return true;

  pe_data(obfd)->pe_opthdr = pe_data (ibfd)->pe_opthdr;

  return true;
}
