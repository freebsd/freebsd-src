/* BFD back-end for a.out files encapsulated with COFF headers.
   Copyright 1990, 1991, 1994, 1995, 2000, 2001, 2002, 2003
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

/* THIS MODULE IS NOT FINISHED.  IT PROBABLY DOESN'T EVEN COMPILE.  */

#if 0
#define	TARGET_PAGE_SIZE	4096
#define	SEGMENT_SIZE	TARGET_PAGE_SIZE
#define TEXT_START_ADDR 0
#endif

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "aout/aout64.h"
#include "aout/stab_gnu.h"
#include "aout/ar.h"
#include "libaout.h"           /* BFD a.out internal data structures */

const bfd_target *encap_real_callback ();

const bfd_target *
encap_object_p (abfd)
     bfd *abfd;
{
  unsigned char magicbuf[4]; /* Raw bytes of magic number from file */
  unsigned long magic;		/* Swapped magic number */
  short coff_magic;
  struct external_exec exec_bytes;
  struct internal_exec exec;
  bfd_size_type amt = sizeof (magicbuf);

  if (bfd_bread ((PTR) magicbuf, amt, abfd) != amt)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return 0;
    }

  coff_magic = H_GET_16 (abfd, magicbuf);
  if (coff_magic != COFF_MAGIC)
    return 0;			/* Not an encap coff file */

  magic = H_GET_32 (abfd, magicbuf);

  if (N_BADMAG (*((struct internal_exec *) &magic)))
    return 0;

  if (bfd_seek (abfd, (file_ptr) sizeof (struct coffheader), SEEK_SET) != 0)
    return 0;

  amt = EXEC_BYTES_SIZE;
  if (bfd_bread ((PTR) &exec_bytes, amt, abfd) != amt)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return 0;
    }
  NAME(aout,swap_exec_header_in) (abfd, &exec_bytes, &exec);

  return aout_32_some_aout_object_p (abfd, &exec, encap_realcallback);
}

/* Finish up the reading of an encapsulated-coff a.out file header.  */
const bfd_target *
encap_real_callback (abfd)
     bfd *abfd;
{
  struct internal_exec *execp = exec_hdr (abfd);

  MY(callback) (abfd, execp);

  /* If we have a coff header, it can give us better values for
     text_start and exec_data_start.  This is particularly useful
     for remote debugging of embedded systems.  */
  if (N_FLAGS(exec_aouthdr) & N_FLAGS_COFF_ENCAPSULATE)
    {
      struct coffheader ch;
      int val;
      val = lseek (execchan, -(sizeof (AOUTHDR) + sizeof (ch)), 1);
      if (val == -1)
	perror_with_name (filename);
      val = myread (execchan, &ch, sizeof (ch));
      if (val < 0)
	perror_with_name (filename);
      text_start = ch.text_start;
      exec_data_start = ch.data_start;
    }
  else
    {
      text_start =
	IS_OBJECT_FILE (exec_aouthdr) ? 0 : N_TXTADDR (exec_aouthdr);
      exec_data_start = (IS_OBJECT_FILE (exec_aouthdr)
			 ? exec_aouthdr.a_text
			 : N_DATADDR (exec_aouthdr));
    }

  /* Determine the architecture and machine type of the object file.  */
  bfd_default_set_arch_mach(abfd, bfd_arch_m68k, 0); /* FIXME */

  return abfd->xvec;
}

/* Write an object file in Encapsulated COFF format.
   Section contents have already been written.  We write the
   file header, symbols, and relocation.  */

bfd_boolean
encap_write_object_contents (abfd)
     bfd *abfd;
{
  bfd_size_type data_pad = 0;
  struct external_exec exec_bytes;
  struct internal_exec *execp = exec_hdr (abfd);

  /* FIXME:  Fragments from the old GNU LD program for dealing with
     encap coff.  */
  struct coffheader coffheader;
  int need_coff_header;

  /* Determine whether to count the header as part of
     the text size, and initialize the text size accordingly.
     This depends on the kind of system and on the output format selected.  */

  N_SET_MAGIC (outheader, magic);
#ifdef INITIALIZE_HEADER
  INITIALIZE_HEADER;
#endif

  text_size = sizeof (struct exec);
#ifdef COFF_ENCAPSULATE
  if (relocatable_output == 0 && file_table[0].just_syms_flag == 0)
    {
      need_coff_header = 1;
      /* set this flag now, since it will change the values of N_TXTOFF, etc */
      N_SET_FLAGS (outheader, aout_backend_info (abfd)->exec_hdr_flags);
      text_size += sizeof (struct coffheader);
    }
#endif

#ifdef COFF_ENCAPSULATE
  if (need_coff_header)
    {
      /* We are encapsulating BSD format within COFF format.  */
      struct coffscn *tp, *dp, *bp;

      tp = &coffheader.scns[0];
      dp = &coffheader.scns[1];
      bp = &coffheader.scns[2];

      strcpy (tp->s_name, ".text");
      tp->s_paddr = text_start;
      tp->s_vaddr = text_start;
      tp->s_size = text_size;
      tp->s_scnptr = sizeof (struct coffheader) + sizeof (struct exec);
      tp->s_relptr = 0;
      tp->s_lnnoptr = 0;
      tp->s_nreloc = 0;
      tp->s_nlnno = 0;
      tp->s_flags = 0x20;
      strcpy (dp->s_name, ".data");
      dp->s_paddr = data_start;
      dp->s_vaddr = data_start;
      dp->s_size = data_size;
      dp->s_scnptr = tp->s_scnptr + tp->s_size;
      dp->s_relptr = 0;
      dp->s_lnnoptr = 0;
      dp->s_nreloc = 0;
      dp->s_nlnno = 0;
      dp->s_flags = 0x40;
      strcpy (bp->s_name, ".bss");
      bp->s_paddr = dp->s_vaddr + dp->s_size;
      bp->s_vaddr = bp->s_paddr;
      bp->s_size = bss_size;
      bp->s_scnptr = 0;
      bp->s_relptr = 0;
      bp->s_lnnoptr = 0;
      bp->s_nreloc = 0;
      bp->s_nlnno = 0;
      bp->s_flags = 0x80;

      coffheader.f_magic = COFF_MAGIC;
      coffheader.f_nscns = 3;
      /* store an unlikely time so programs can
       * tell that there is a bsd header
       */
      coffheader.f_timdat = 1;
      coffheader.f_symptr = 0;
      coffheader.f_nsyms = 0;
      coffheader.f_opthdr = 28;
      coffheader.f_flags = 0x103;
      /* aouthdr */
      coffheader.magic = ZMAGIC;
      coffheader.vstamp = 0;
      coffheader.tsize = tp->s_size;
      coffheader.dsize = dp->s_size;
      coffheader.bsize = bp->s_size;
      coffheader.entry = outheader.a_entry;
      coffheader.text_start = tp->s_vaddr;
      coffheader.data_start = dp->s_vaddr;
    }
#endif

#ifdef COFF_ENCAPSULATE
  if (need_coff_header)
    mywrite (&coffheader, sizeof coffheader, 1, outdesc);
#endif

#ifndef COFF_ENCAPSULATE
  padfile (N_TXTOFF (outheader) - sizeof outheader, outdesc);
#endif

  text_size -= N_TXTOFF (outheader);
  WRITE_HEADERS(abfd, execp);
  return TRUE;
}

#define MY_write_object_content encap_write_object_contents
#define MY_object_p encap_object_p
#define MY_exec_hdr_flags N_FLAGS_COFF_ENCAPSULATE

#include "aout-target.h"
