/* BFD back-end for IBM RS/6000 "XCOFF" files.
   Copyright 1990, 91, 92, 93, 94, 95, 1996 Free Software Foundation, Inc.
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

/* Internalcoff.h and coffcode.h modify themselves based on this flag.  */
#define RS6000COFF_C 1

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "obstack.h"
#include "coff/internal.h"
#include "coff/rs6000.h"
#include "libcoff.h"

/* The main body of code is in coffcode.h.  */

static boolean xcoff_mkobject PARAMS ((bfd *));
static boolean xcoff_copy_private_bfd_data PARAMS ((bfd *, bfd *));
static void xcoff_rtype2howto
  PARAMS ((arelent *, struct internal_reloc *));
static reloc_howto_type *xcoff_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static boolean xcoff_slurp_armap PARAMS ((bfd *));
static const bfd_target *xcoff_archive_p PARAMS ((bfd *));
static PTR xcoff_read_ar_hdr PARAMS ((bfd *));
static bfd *xcoff_openr_next_archived_file PARAMS ((bfd *, bfd *));
static int xcoff_generic_stat_arch_elt PARAMS ((bfd *, struct stat *));
static const char *normalize_filename PARAMS ((bfd *));
static boolean xcoff_write_armap
  PARAMS ((bfd *, unsigned int, struct orl *, unsigned int, int));
static boolean xcoff_write_archive_contents PARAMS ((bfd *));

/* We use our own tdata type.  Its first field is the COFF tdata type,
   so the COFF routines are compatible.  */

static boolean
xcoff_mkobject (abfd)
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

static boolean
xcoff_copy_private_bfd_data (ibfd, obfd)
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

/* The XCOFF reloc table.  Actually, XCOFF relocations specify the
   bitsize and whether they are signed or not, along with a
   conventional type.  This table is for the types, which are used for
   different algorithms for putting in the reloc.  Many of these
   relocs need special_function entries, which I have not written.  */

static reloc_howto_type xcoff_howto_table[] =
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
  
  { 7 },
  
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
  
  { 9 },

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
  
  { 0xb },

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
  
  { 0xe },
  
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
  
  { 0x10 },
  { 0x11 },
  
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
	 16,	                /* bitsize */                   
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
	 16,	                /* bitsize */                   
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
	 false)                 /* pcrel_offset */
};

static void
xcoff_rtype2howto (relent, internal)
     arelent *relent;
     struct internal_reloc *internal;
{
  relent->howto = xcoff_howto_table + internal->r_type;

  /* The r_size field of an XCOFF reloc encodes the bitsize of the
     relocation, as well as indicating whether it is signed or not.
     Doublecheck that the relocation information gathered from the
     type matches this information.  */
  if (relent->howto->bitsize != ((unsigned int) internal->r_size & 0x1f) + 1)
    abort ();
#if 0
  if ((internal->r_size & 0x80) != 0
      ? (relent->howto->complain_on_overflow != complain_overflow_signed)
      : (relent->howto->complain_on_overflow != complain_overflow_bitfield))
    abort ();
#endif
}

static reloc_howto_type *
xcoff_reloc_type_lookup (abfd, code)
     bfd *abfd;
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
    default:
      return NULL;
    }
}

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

#define RTYPE2HOWTO(cache_ptr, dst) xcoff_rtype2howto (cache_ptr, dst)

#define coff_mkobject xcoff_mkobject
#define coff_bfd_copy_private_bfd_data xcoff_copy_private_bfd_data
#define coff_bfd_reloc_type_lookup xcoff_reloc_type_lookup
#define coff_relocate_section _bfd_ppc_xcoff_relocate_section

#include "coffcode.h"

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
   null terminated strings, which are symbol names.  */

/* XCOFF archives use this as a magic string.  */

#define XCOFFARMAG "<aiaff>\012"
#define SXCOFFARMAG 8

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

/* We store a copy of the xcoff_ar_file_hdr in the tdata field of the
   artdata structure.  */
#define xcoff_ardata(abfd) \
  ((struct xcoff_ar_file_hdr *) bfd_ardata (abfd)->tdata)

/* We store a copy of the xcoff_ar_hdr in the arelt_data field of an
   archive element.  */
#define arch_eltdata(bfd) ((struct areltdata *) ((bfd)->arelt_data))
#define arch_xhdr(bfd) \
  ((struct xcoff_ar_hdr *) arch_eltdata (bfd)->arch_header)

/* XCOFF archives do not have anything which corresponds to an
   extended name table.  */

#define xcoff_slurp_extended_name_table bfd_false
#define xcoff_construct_extended_name_table \
  ((boolean (*) PARAMS ((bfd *, char **, bfd_size_type *, const char **))) \
   bfd_false)
#define xcoff_truncate_arname bfd_dont_truncate_arname

/* We can use the standard get_elt_at_index routine.  */

#define xcoff_get_elt_at_index _bfd_generic_get_elt_at_index

/* XCOFF archives do not have a timestamp.  */

#define xcoff_update_armap_timestamp bfd_true

/* Read in the armap of an XCOFF archive.  */

static boolean
xcoff_slurp_armap (abfd)
     bfd *abfd;
{
  file_ptr off;
  struct xcoff_ar_hdr hdr;
  size_t namlen;
  bfd_size_type sz;
  bfd_byte *contents, *cend;
  unsigned int c, i;
  carsym *arsym;
  bfd_byte *p;

  if (xcoff_ardata (abfd) == NULL)
    {
      bfd_has_map (abfd) = false;
      return true;
    }

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

  /* Read in the entire symbol table.  */
  sz = strtol (hdr.size, (char **) NULL, 10);
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

static const bfd_target *
xcoff_archive_p (abfd)
     bfd *abfd;
{
  struct xcoff_ar_file_hdr hdr;

  if (bfd_read ((PTR) &hdr, SIZEOF_AR_FILE_HDR, 1, abfd)
      != SIZEOF_AR_FILE_HDR)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (strncmp (hdr.magic, XCOFFARMAG, SXCOFFARMAG) != 0)
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

  bfd_ardata (abfd)->first_file_filepos = strtol (hdr.firstmemoff,
						  (char **) NULL, 10);
  bfd_ardata (abfd)->cache = NULL;
  bfd_ardata (abfd)->archive_head = NULL;
  bfd_ardata (abfd)->symdefs = NULL;
  bfd_ardata (abfd)->extended_names = NULL;

  bfd_ardata (abfd)->tdata = bfd_zalloc (abfd, SIZEOF_AR_FILE_HDR);
  if (bfd_ardata (abfd)->tdata == NULL)
    return NULL;

  memcpy (bfd_ardata (abfd)->tdata, &hdr, SIZEOF_AR_FILE_HDR);

  if (! xcoff_slurp_armap (abfd))
    {
      bfd_release (abfd, bfd_ardata (abfd));
      abfd->tdata.aout_ar_data = (struct artdata *) NULL;
      return NULL;
    }

  return abfd->xvec;
}

/* Read the archive header in an XCOFF archive.  */

static PTR
xcoff_read_ar_hdr (abfd)
     bfd *abfd;
{
  struct xcoff_ar_hdr hdr;
  size_t namlen;
  struct xcoff_ar_hdr *hdrp;
  struct areltdata *ret;

  if (bfd_read ((PTR) &hdr, SIZEOF_AR_HDR, 1, abfd) != SIZEOF_AR_HDR)
    return NULL;

  namlen = strtol (hdr.namlen, (char **) NULL, 10);
  hdrp = bfd_alloc (abfd, SIZEOF_AR_HDR + namlen + 1);
  if (hdrp == NULL)
    return NULL;
  memcpy (hdrp, &hdr, SIZEOF_AR_HDR);
  if (bfd_read ((char *) hdrp + SIZEOF_AR_HDR, 1, namlen, abfd) != namlen)
    return NULL;
  ((char *) hdrp)[SIZEOF_AR_HDR + namlen] = '\0';

  ret = (struct areltdata *) bfd_alloc (abfd, sizeof (struct areltdata));
  if (ret == NULL)
    return NULL;
  ret->arch_header = (char *) hdrp;
  ret->parsed_size = strtol (hdr.size, (char **) NULL, 10);
  ret->filename = (char *) hdrp + SIZEOF_AR_HDR;

  /* Skip over the XCOFFARFMAG at the end of the file name.  */
  if (bfd_seek (abfd, (namlen & 1) + SXCOFFARFMAG, SEEK_CUR) != 0)
    return NULL;

  return (PTR) ret;
}

/* Open the next element in an XCOFF archive.  */

static bfd *
xcoff_openr_next_archived_file (archive, last_file)
     bfd *archive;
     bfd *last_file;
{
  file_ptr filestart;

  if (xcoff_ardata (archive) == NULL)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return NULL;
    }

  if (last_file == NULL)
    filestart = bfd_ardata (archive)->first_file_filepos;
  else
    filestart = strtol (arch_xhdr (last_file)->nextoff, (char **) NULL, 10);

  if (filestart == 0
      || filestart == strtol (xcoff_ardata (archive)->memoff,
			      (char **) NULL, 10)
      || filestart == strtol (xcoff_ardata (archive)->symoff,
			      (char **) NULL, 10))
    {
      bfd_set_error (bfd_error_no_more_archived_files);
      return NULL;
    }

  return _bfd_get_elt_at_filepos (archive, filestart);
}

/* Stat an element in an XCOFF archive.  */

static int
xcoff_generic_stat_arch_elt (abfd, s)
     bfd *abfd;
     struct stat *s;
{
  struct xcoff_ar_hdr *hdrp;

  if (abfd->arelt_data == NULL)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }

  hdrp = arch_xhdr (abfd);

  s->st_mtime = strtol (hdrp->date, (char **) NULL, 10);
  s->st_uid = strtol (hdrp->uid, (char **) NULL, 10);
  s->st_gid = strtol (hdrp->gid, (char **) NULL, 10);
  s->st_mode = strtol (hdrp->mode, (char **) NULL, 8);
  s->st_size = arch_eltdata (abfd)->parsed_size;

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

/*ARGSUSED*/
static boolean
xcoff_write_armap (abfd, elength, map, orl_count, stridx)
     bfd *abfd;
     unsigned int elength;
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

/* Write out an XCOFF archive.  We always write an entire archive,
   rather than fussing with the freelist and so forth.  */

static boolean
xcoff_write_archive_contents (abfd)
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
	      sub->arelt_data = ((struct areltdata *)
				 bfd_alloc (sub, sizeof (struct areltdata)));
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

/* We can't use the usual coff_sizeof_headers routine, because AIX
   always uses an a.out header.  */

/*ARGSUSED*/
static int
_bfd_xcoff_sizeof_headers (abfd, reloc)
     bfd *abfd;
     boolean reloc;
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

#define CORE_FILE_P _bfd_dummy_target

#define coff_core_file_failing_command _bfd_nocore_core_file_failing_command
#define coff_core_file_failing_signal _bfd_nocore_core_file_failing_signal
#define coff_core_file_matches_executable_p \
  _bfd_nocore_core_file_matches_executable_p

#ifdef AIX_CORE
#undef CORE_FILE_P
#define CORE_FILE_P rs6000coff_core_p
extern const bfd_target * rs6000coff_core_p ();
extern boolean rs6000coff_get_section_contents ();
extern boolean rs6000coff_core_file_matches_executable_p ();

#undef	coff_core_file_matches_executable_p
#define coff_core_file_matches_executable_p  \
				     rs6000coff_core_file_matches_executable_p

extern char *rs6000coff_core_file_failing_command PARAMS ((bfd *abfd));
#undef coff_core_file_failing_command
#define coff_core_file_failing_command rs6000coff_core_file_failing_command

extern int rs6000coff_core_file_failing_signal PARAMS ((bfd *abfd));
#undef coff_core_file_failing_signal
#define coff_core_file_failing_signal rs6000coff_core_file_failing_signal

#undef	coff_get_section_contents
#define	coff_get_section_contents	rs6000coff_get_section_contents
#endif /* AIX_CORE */

#ifdef LYNX_CORE

#undef CORE_FILE_P
#define CORE_FILE_P lynx_core_file_p
extern const bfd_target *lynx_core_file_p PARAMS ((bfd *abfd));

extern boolean lynx_core_file_matches_executable_p PARAMS ((bfd *core_bfd,
							    bfd *exec_bfd));
#undef	coff_core_file_matches_executable_p
#define coff_core_file_matches_executable_p lynx_core_file_matches_executable_p

extern char *lynx_core_file_failing_command PARAMS ((bfd *abfd));
#undef coff_core_file_failing_command
#define coff_core_file_failing_command lynx_core_file_failing_command

extern int lynx_core_file_failing_signal PARAMS ((bfd *abfd));
#undef coff_core_file_failing_signal
#define coff_core_file_failing_signal lynx_core_file_failing_signal

#endif /* LYNX_CORE */

#define _bfd_xcoff_bfd_get_relocated_section_contents \
  coff_bfd_get_relocated_section_contents
#define _bfd_xcoff_bfd_relax_section coff_bfd_relax_section
#define _bfd_xcoff_bfd_link_split_section coff_bfd_link_split_section

/* The transfer vector that leads the outside world to all of the above. */

const bfd_target
#ifdef TARGET_SYM
  TARGET_SYM =
#else
  rs6000coff_vec =
#endif
{
#ifdef TARGET_NAME
  TARGET_NAME,
#else
  "aixcoff-rs6000",		/* name */
#endif
  bfd_target_coff_flavour,	
  BFD_ENDIAN_BIG,		/* data byte order is big */
  BFD_ENDIAN_BIG,		/* header byte order is big */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG | DYNAMIC |
   HAS_SYMS | HAS_LOCALS | WP_TEXT),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
  0,				/* leading char */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen??? FIXMEmgo */

  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* hdrs */

  {_bfd_dummy_target, coff_object_p, 	/* bfd_check_format */
     xcoff_archive_p, CORE_FILE_P},
  {bfd_false, coff_mkobject,		/* bfd_set_format */
     _bfd_generic_mkarchive, bfd_false},
  {bfd_false, coff_write_object_contents,	/* bfd_write_contents */
     xcoff_write_archive_contents, bfd_false},

     BFD_JUMP_TABLE_GENERIC (coff),
     BFD_JUMP_TABLE_COPY (coff),
     BFD_JUMP_TABLE_CORE (coff),
     BFD_JUMP_TABLE_ARCHIVE (xcoff),
     BFD_JUMP_TABLE_SYMBOLS (coff),
     BFD_JUMP_TABLE_RELOCS (coff),
     BFD_JUMP_TABLE_WRITE (coff),
     BFD_JUMP_TABLE_LINK (_bfd_xcoff),
     BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  COFF_SWAP_TABLE,
};
