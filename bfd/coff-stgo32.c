/* BFD back-end for Intel 386 COFF files (DJGPP variant with a stub).
   Copyright 1997, 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Written by Robert Hoehne.

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

/* This file handles now also stubbed coff images. The stub is a small
   DOS executable program before the coff image to load it in memory
   and execute it. This is needed, because DOS cannot run coff files.

   All the functions below are called by the corresponding functions
   from coffswap.h.
   The only thing what they do is to adjust the information stored in
   the COFF file which are offset into the file.
   This is needed, because DJGPP uses a very special way to load and run
   the coff image. It loads the image in memory and assumes then, that the
   image had no stub by using the filepointers as pointers in the coff
   image and NOT in the file.

   To be compatible with any existing executables I have fixed this
   here and NOT in the DJGPP startup code.  */

#define TARGET_SYM		go32stubbedcoff_vec
#define TARGET_NAME		"coff-go32-exe"
#define TARGET_UNDERSCORE	'_'
#define COFF_GO32_EXE
#define COFF_LONG_SECTION_NAMES
#define COFF_SUPPORT_GNU_LINKONCE
#define COFF_LONG_FILENAMES

#define COFF_SECTION_ALIGNMENT_ENTRIES \
{ COFF_SECTION_NAME_EXACT_MATCH (".data"), \
  COFF_ALIGNMENT_FIELD_EMPTY, COFF_ALIGNMENT_FIELD_EMPTY, 4 }, \
{ COFF_SECTION_NAME_EXACT_MATCH (".text"), \
  COFF_ALIGNMENT_FIELD_EMPTY, COFF_ALIGNMENT_FIELD_EMPTY, 4 }, \
{ COFF_SECTION_NAME_PARTIAL_MATCH (".debug"), \
  COFF_ALIGNMENT_FIELD_EMPTY, COFF_ALIGNMENT_FIELD_EMPTY, 0 }, \
{ COFF_SECTION_NAME_PARTIAL_MATCH (".gnu.linkonce.wi"), \
  COFF_ALIGNMENT_FIELD_EMPTY, COFF_ALIGNMENT_FIELD_EMPTY, 0 }

#include "bfd.h"

/* At first the prototypes.  */

static void
adjust_filehdr_in_post PARAMS ((bfd *, PTR, PTR));
static void
adjust_filehdr_out_pre PARAMS ((bfd *, PTR, PTR));
static void
adjust_filehdr_out_post PARAMS ((bfd *, PTR, PTR));
static void
adjust_scnhdr_in_post PARAMS ((bfd *, PTR, PTR));
static void
adjust_scnhdr_out_pre PARAMS ((bfd *, PTR, PTR));
static void
adjust_scnhdr_out_post PARAMS ((bfd *, PTR, PTR));
static void
adjust_aux_in_post PARAMS ((bfd *, PTR, int, int, int, int, PTR));
static void
adjust_aux_out_pre PARAMS ((bfd *, PTR, int, int, int, int, PTR));
static void
adjust_aux_out_post PARAMS ((bfd *, PTR, int, int, int, int, PTR));
static void
create_go32_stub PARAMS ((bfd *));

/* All that ..._PRE and ...POST functions are called from the corresponding
   coff_swap... functions. The ...PRE functions are called at the beginning
   of the function and the ...POST functions at the end of the swap routines.  */

#define COFF_ADJUST_FILEHDR_IN_POST adjust_filehdr_in_post
#define COFF_ADJUST_FILEHDR_OUT_PRE adjust_filehdr_out_pre
#define COFF_ADJUST_FILEHDR_OUT_POST adjust_filehdr_out_post

#define COFF_ADJUST_SCNHDR_IN_POST adjust_scnhdr_in_post
#define COFF_ADJUST_SCNHDR_OUT_PRE adjust_scnhdr_out_pre
#define COFF_ADJUST_SCNHDR_OUT_POST adjust_scnhdr_out_post

#define COFF_ADJUST_AUX_IN_POST adjust_aux_in_post
#define COFF_ADJUST_AUX_OUT_PRE adjust_aux_out_pre
#define COFF_ADJUST_AUX_OUT_POST adjust_aux_out_post

static bfd_boolean
  go32_stubbed_coff_bfd_copy_private_bfd_data PARAMS ((bfd *, bfd *));

#define coff_bfd_copy_private_bfd_data go32_stubbed_coff_bfd_copy_private_bfd_data

#include "coff-i386.c"

/* I hold in the usrdata the stub.  */
#define bfd_coff_go32stub bfd_usrdata

/* This macro is used, because I cannot assume the endianess of the
   host system.  */
#define _H(index) (H_GET_16 (abfd, (header+index*2)))

/* These bytes are a 2048-byte DOS executable, which loads the COFF
   image into memory and then runs it. It is called 'stub'.  */

static const unsigned char stub_bytes[STUBSIZE] =
{
#include "go32stub.h"
};

/*
   I have not commented each swap function below, because the
   technique is in any function the same. For the ...in function,
   all the pointers are adjusted by adding STUBSIZE and for the
   ...out function, it is subtracted first and after calling the
   standard swap function it is reset to the old value.  */

/* This macro is used for adjusting the filepointers, which
   is done only, if the pointer is nonzero.  */

#define ADJUST_VAL(val,diff) \
  if (val != 0) val += diff

static void
adjust_filehdr_in_post  (abfd, src, dst)
     bfd *abfd;
     PTR src;
     PTR dst;
{
  FILHDR *filehdr_src = (FILHDR *) src;
  struct internal_filehdr *filehdr_dst = (struct internal_filehdr *) dst;

  ADJUST_VAL (filehdr_dst->f_symptr, STUBSIZE);

  /* Save now the stub to be used later.  */
  bfd_coff_go32stub (abfd) = (PTR) bfd_alloc (abfd, (bfd_size_type) STUBSIZE);

  /* Since this function returns no status, I do not set here
     any bfd_error_...
     That means, before the use of bfd_coff_go32stub (), this value
     should be checked if it is != NULL.  */
  if (bfd_coff_go32stub (abfd) == NULL)
    return;
  memcpy (bfd_coff_go32stub (abfd), filehdr_src->stub, STUBSIZE);
}

static void
adjust_filehdr_out_pre  (abfd, in, out)
     bfd *abfd;
     PTR in;
     PTR out;
{
  struct internal_filehdr *filehdr_in = (struct internal_filehdr *) in;
  FILHDR *filehdr_out = (FILHDR *) out;

  /* Generate the stub.  */
  create_go32_stub (abfd);

  /* Copy the stub to the file header.  */
  if (bfd_coff_go32stub (abfd) != NULL)
    memcpy (filehdr_out->stub, bfd_coff_go32stub (abfd), STUBSIZE);
  else
    /* Use the default.  */
    memcpy (filehdr_out->stub, stub_bytes, STUBSIZE);

  ADJUST_VAL (filehdr_in->f_symptr, -STUBSIZE);
}

static void
adjust_filehdr_out_post  (abfd, in, out)
     bfd *abfd ATTRIBUTE_UNUSED;
     PTR in;
     PTR out ATTRIBUTE_UNUSED;
{
  struct internal_filehdr *filehdr_in = (struct internal_filehdr *) in;
  /* Undo the above change.  */
  ADJUST_VAL (filehdr_in->f_symptr, STUBSIZE);
}

static void
adjust_scnhdr_in_post  (abfd, ext, in)
     bfd *abfd ATTRIBUTE_UNUSED;
     PTR ext ATTRIBUTE_UNUSED;
     PTR in;
{
  struct internal_scnhdr *scnhdr_int = (struct internal_scnhdr *) in;

  ADJUST_VAL (scnhdr_int->s_scnptr, STUBSIZE);
  ADJUST_VAL (scnhdr_int->s_relptr, STUBSIZE);
  ADJUST_VAL (scnhdr_int->s_lnnoptr, STUBSIZE);
}

static void
adjust_scnhdr_out_pre  (abfd, in, out)
     bfd *abfd ATTRIBUTE_UNUSED;
     PTR in;
     PTR out ATTRIBUTE_UNUSED;
{
  struct internal_scnhdr *scnhdr_int = (struct internal_scnhdr *) in;

  ADJUST_VAL (scnhdr_int->s_scnptr, -STUBSIZE);
  ADJUST_VAL (scnhdr_int->s_relptr, -STUBSIZE);
  ADJUST_VAL (scnhdr_int->s_lnnoptr, -STUBSIZE);
}

static void
adjust_scnhdr_out_post (abfd, in, out)
     bfd *abfd ATTRIBUTE_UNUSED;
     PTR in;
     PTR out ATTRIBUTE_UNUSED;
{
  struct internal_scnhdr *scnhdr_int = (struct internal_scnhdr *) in;

  ADJUST_VAL (scnhdr_int->s_scnptr, STUBSIZE);
  ADJUST_VAL (scnhdr_int->s_relptr, STUBSIZE);
  ADJUST_VAL (scnhdr_int->s_lnnoptr, STUBSIZE);
}

static void
adjust_aux_in_post  (abfd, ext1, type, class, indx, numaux, in1)
     bfd *abfd ATTRIBUTE_UNUSED;
     PTR ext1 ATTRIBUTE_UNUSED;
     int type;
     int class;
     int indx ATTRIBUTE_UNUSED;
     int numaux ATTRIBUTE_UNUSED;
     PTR in1;
{
  union internal_auxent *in = (union internal_auxent *) in1;

  if (class == C_BLOCK || class == C_FCN || ISFCN (type) || ISTAG (class))
    {
      ADJUST_VAL (in->x_sym.x_fcnary.x_fcn.x_lnnoptr, STUBSIZE);
    }
}

static void
adjust_aux_out_pre  (abfd, inp, type, class, indx, numaux, extp)
     bfd *abfd ATTRIBUTE_UNUSED;
     PTR inp;
     int type;
     int class;
     int indx ATTRIBUTE_UNUSED;
     int numaux ATTRIBUTE_UNUSED;
     PTR extp ATTRIBUTE_UNUSED;
{
  union internal_auxent *in = (union internal_auxent *) inp;

  if (class == C_BLOCK || class == C_FCN || ISFCN (type) || ISTAG (class))
    {
      ADJUST_VAL (in->x_sym.x_fcnary.x_fcn.x_lnnoptr, -STUBSIZE);
    }
}

static void
adjust_aux_out_post (abfd, inp, type, class, indx, numaux, extp)
     bfd *abfd ATTRIBUTE_UNUSED;
     PTR inp;
     int type;
     int class;
     int indx ATTRIBUTE_UNUSED;
     int numaux ATTRIBUTE_UNUSED;
     PTR extp ATTRIBUTE_UNUSED;
{
  union internal_auxent *in = (union internal_auxent *) inp;

  if (class == C_BLOCK || class == C_FCN || ISFCN (type) || ISTAG (class))
    {
      ADJUST_VAL (in->x_sym.x_fcnary.x_fcn.x_lnnoptr, STUBSIZE);
    }
}

/* That's the function, which creates the stub. There are
   different cases from where the stub is taken.
   At first the environment variable $(GO32STUB) is checked and then
   $(STUB) if it was not set.
   If it exists and points to a valid stub the stub is taken from
   that file. This file can be also a whole executable file, because
   the stub is computed from the exe information at the start of that
   file.

   If there was any error, the standard stub (compiled in this file)
   is taken.  */

static void
create_go32_stub (abfd)
     bfd *abfd;
{
  /* Do it only once.  */
  if (bfd_coff_go32stub (abfd) == NULL)
    {
      char *stub;
      struct stat st;
      int f;
      unsigned char header[10];
      char magic[8];
      unsigned long coff_start;
      long exe_start;

      /* Check at first the environment variable $(GO32STUB).  */
      stub = getenv ("GO32STUB");
      /* Now check the environment variable $(STUB).  */
      if (stub == NULL)
	stub = getenv ("STUB");
      if (stub == NULL)
	goto stub_end;
      if (stat (stub, &st) != 0)
	goto stub_end;
#ifdef O_BINARY
      f = open (stub, O_RDONLY | O_BINARY);
#else
      f = open (stub, O_RDONLY);
#endif
      if (f < 0)
	goto stub_end;
      if (read (f, &header, sizeof (header)) < 0)
	{
	  close (f);
	  goto stub_end;
	}
      if (_H (0) != 0x5a4d)	/* It is not an exe file.  */
	{
	  close (f);
	  goto stub_end;
	}
      /* Compute the size of the stub (it is every thing up
         to the beginning of the coff image).  */
      coff_start = (long) _H (2) * 512L;
      if (_H (1))
	coff_start += (long) _H (1) - 512L;

      /* Currently there is only a fixed stub size of 2048 bytes
         supported.  */
      if (coff_start != 2048)
	{
	  close (f);
	  goto stub_end;
	}
      exe_start = _H (4) * 16;
      if ((long) lseek (f, exe_start, SEEK_SET) != exe_start)
	{
	  close (f);
	  goto stub_end;
	}
      if (read (f, &magic, 8) != 8)
	{
	  close (f);
	  goto stub_end;
	}
      if (memcmp (magic, "go32stub", 8) != 0)
	{
	  close (f);
	  goto stub_end;
	}
      /* Now we found a correct stub (hopefully).  */
      bfd_coff_go32stub (abfd)
	= (PTR) bfd_alloc (abfd, (bfd_size_type) coff_start);
      if (bfd_coff_go32stub (abfd) == NULL)
	{
	  close (f);
	  return;
	}
      lseek (f, 0L, SEEK_SET);
      if ((unsigned long) read (f, bfd_coff_go32stub (abfd), coff_start)
	  != coff_start)
	{
	  bfd_release (abfd, bfd_coff_go32stub (abfd));
	  bfd_coff_go32stub (abfd) = NULL;
	}
      close (f);
    }
stub_end:
  /* There was something wrong above, so use now the standard builtin
     stub.  */
  if (bfd_coff_go32stub (abfd) == NULL)
    {
      bfd_coff_go32stub (abfd)
	= (PTR) bfd_alloc (abfd, (bfd_size_type) STUBSIZE);
      if (bfd_coff_go32stub (abfd) == NULL)
	return;
      memcpy (bfd_coff_go32stub (abfd), stub_bytes, STUBSIZE);
    }
}

/* If ibfd was a stubbed coff image, copy the stub from that bfd
   to the new obfd.  */

static bfd_boolean
go32_stubbed_coff_bfd_copy_private_bfd_data  (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  /* Check if both are the same targets.  */
  if (ibfd->xvec != obfd->xvec)
    return TRUE;

  /* Check if both have a valid stub.  */
  if (bfd_coff_go32stub (ibfd) == NULL
      || bfd_coff_go32stub (obfd) == NULL)
    return TRUE;

  /* Now copy the stub.  */
  memcpy (bfd_coff_go32stub (obfd), bfd_coff_go32stub (ibfd), STUBSIZE);

  return TRUE;
}
