/* Routines to link ECOFF debugging information.
   Copyright 1993, 94, 95, 96, 97, 99, 2000 Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support, <ian@cygnus.com>.

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
#include "objalloc.h"
#include "aout/stab_gnu.h"
#include "coff/internal.h"
#include "coff/sym.h"
#include "coff/symconst.h"
#include "coff/ecoff.h"
#include "libcoff.h"
#include "libecoff.h"

static boolean ecoff_add_bytes PARAMS ((char **buf, char **bufend,
					size_t need));
static struct bfd_hash_entry *string_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *,
	   const char *));
static void ecoff_align_debug PARAMS ((bfd *abfd,
				       struct ecoff_debug_info *debug,
				       const struct ecoff_debug_swap *swap));
static boolean ecoff_write_symhdr PARAMS ((bfd *, struct ecoff_debug_info *,
					   const struct ecoff_debug_swap *,
					   file_ptr where));
static int cmp_fdrtab_entry PARAMS ((const PTR, const PTR));
static boolean mk_fdrtab PARAMS ((bfd *,
				  struct ecoff_debug_info * const,
				  const struct ecoff_debug_swap * const,
				  struct ecoff_find_line *));
static long fdrtab_lookup PARAMS ((struct ecoff_find_line *, bfd_vma));
static boolean lookup_line
  PARAMS ((bfd *, struct ecoff_debug_info * const,
	   const struct ecoff_debug_swap * const, struct ecoff_find_line *));

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

  /* now the fun stuff...  */
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
    abort ();
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

  /* now the fun stuff...  */
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
    abort ();
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

  /* now the fun stuff...  */
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
    abort ();
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

  /* now the fun stuff...  */
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
    abort ();
#endif
}

/* The minimum amount of data to allocate.  */
#define ALLOC_SIZE (4064)

/* Add bytes to a buffer.  Return success.  */

static boolean
ecoff_add_bytes (buf, bufend, need)
     char **buf;
     char **bufend;
     size_t need;
{
  size_t have;
  size_t want;
  char *newbuf;

  have = *bufend - *buf;
  if (have > need)
    want = ALLOC_SIZE;
  else
    {
      want = need - have;
      if (want < ALLOC_SIZE)
	want = ALLOC_SIZE;
    }
  newbuf = (char *) bfd_realloc (*buf, have + want);
  if (newbuf == NULL)
    return false;
  *buf = newbuf;
  *bufend = *buf + have + want;
  return true;
}

/* We keep a hash table which maps strings to numbers.  We use it to
   map FDR names to indices in the output file, and to map local
   strings when combining stabs debugging information.  */

struct string_hash_entry
{
  struct bfd_hash_entry root;
  /* FDR index or string table offset.  */
  long val;
  /* Next entry in string table.  */
  struct string_hash_entry *next;
};

struct string_hash_table
{
  struct bfd_hash_table table;
};

/* Routine to create an entry in a string hash table.  */

static struct bfd_hash_entry *
string_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct string_hash_entry *ret = (struct string_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct string_hash_entry *) NULL)
    ret = ((struct string_hash_entry *)
	   bfd_hash_allocate (table, sizeof (struct string_hash_entry)));
  if (ret == (struct string_hash_entry *) NULL)
    return NULL;

  /* Call the allocation method of the superclass.  */
  ret = ((struct string_hash_entry *)
	 bfd_hash_newfunc ((struct bfd_hash_entry *) ret, table, string));

  if (ret)
    {
      /* Initialize the local fields.  */
      ret->val = -1;
      ret->next = NULL;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Look up an entry in an string hash table.  */

#define string_hash_lookup(t, string, create, copy) \
  ((struct string_hash_entry *) \
   bfd_hash_lookup (&(t)->table, (string), (create), (copy)))

/* We can't afford to read in all the debugging information when we do
   a link.  Instead, we build a list of these structures to show how
   different parts of the input file map to the output file.  */

struct shuffle
{
  /* The next entry in this linked list.  */
  struct shuffle *next;
  /* The length of the information.  */
  unsigned long size;
  /* Whether this information comes from a file or not.  */
  boolean filep;
  union
    {
      struct
	{
	  /* The BFD the data comes from.  */
	  bfd *input_bfd;
	  /* The offset within input_bfd.  */
	  file_ptr offset;
	} file;
      /* The data to be written out.  */
      PTR memory;
    } u;
};

/* This structure holds information across calls to
   bfd_ecoff_debug_accumulate.  */

struct accumulate
{
  /* The FDR hash table.  */
  struct string_hash_table fdr_hash;
  /* The strings hash table.  */
  struct string_hash_table str_hash;
  /* Linked lists describing how to shuffle the input debug
     information into the output file.  We keep a pointer to both the
     head and the tail.  */
  struct shuffle *line;
  struct shuffle *line_end;
  struct shuffle *pdr;
  struct shuffle *pdr_end;
  struct shuffle *sym;
  struct shuffle *sym_end;
  struct shuffle *opt;
  struct shuffle *opt_end;
  struct shuffle *aux;
  struct shuffle *aux_end;
  struct shuffle *ss;
  struct shuffle *ss_end;
  struct string_hash_entry *ss_hash;
  struct string_hash_entry *ss_hash_end;
  struct shuffle *fdr;
  struct shuffle *fdr_end;
  struct shuffle *rfd;
  struct shuffle *rfd_end;
  /* The size of the largest file shuffle.  */
  unsigned long largest_file_shuffle;
  /* An objalloc for debugging information.  */
  struct objalloc *memory;
};

/* Add a file entry to a shuffle list.  */

static boolean add_file_shuffle PARAMS ((struct accumulate *,
				      struct shuffle **,
				      struct shuffle **, bfd *, file_ptr,
				      unsigned long));

static boolean
add_file_shuffle (ainfo, head, tail, input_bfd, offset, size)
     struct accumulate *ainfo;
     struct shuffle **head;
     struct shuffle **tail;
     bfd *input_bfd;
     file_ptr offset;
     unsigned long size;
{
  struct shuffle *n;

  if (*tail != (struct shuffle *) NULL
      && (*tail)->filep
      && (*tail)->u.file.input_bfd == input_bfd
      && (*tail)->u.file.offset + (*tail)->size == (unsigned long) offset)
    {
      /* Just merge this entry onto the existing one.  */
      (*tail)->size += size;
      if ((*tail)->size > ainfo->largest_file_shuffle)
	ainfo->largest_file_shuffle = (*tail)->size;
      return true;
    }

  n = (struct shuffle *) objalloc_alloc (ainfo->memory,
					 sizeof (struct shuffle));
  if (!n)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  n->next = NULL;
  n->size = size;
  n->filep = true;
  n->u.file.input_bfd = input_bfd;
  n->u.file.offset = offset;
  if (*head == (struct shuffle *) NULL)
    *head = n;
  if (*tail != (struct shuffle *) NULL)
    (*tail)->next = n;
  *tail = n;
  if (size > ainfo->largest_file_shuffle)
    ainfo->largest_file_shuffle = size;
  return true;
}

/* Add a memory entry to a shuffle list.  */

static boolean add_memory_shuffle PARAMS ((struct accumulate *,
					   struct shuffle **head,
					   struct shuffle **tail,
					   bfd_byte *data, unsigned long size));

static boolean
add_memory_shuffle (ainfo, head, tail, data, size)
     struct accumulate *ainfo;
     struct shuffle **head;
     struct shuffle **tail;
     bfd_byte *data;
     unsigned long size;
{
  struct shuffle *n;

  n = (struct shuffle *) objalloc_alloc (ainfo->memory,
					 sizeof (struct shuffle));
  if (!n)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  n->next = NULL;
  n->size = size;
  n->filep = false;
  n->u.memory = (PTR) data;
  if (*head == (struct shuffle *) NULL)
    *head = n;
  if (*tail != (struct shuffle *) NULL)
    (*tail)->next = n;
  *tail = n;
  return true;
}

/* Initialize the FDR hash table.  This returns a handle which is then
   passed in to bfd_ecoff_debug_accumulate, et. al.  */

PTR
bfd_ecoff_debug_init (output_bfd, output_debug, output_swap, info)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct ecoff_debug_info *output_debug;
     const struct ecoff_debug_swap *output_swap ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  struct accumulate *ainfo;

  ainfo = (struct accumulate *) bfd_malloc (sizeof (struct accumulate));
  if (!ainfo)
    return NULL;
  if (! bfd_hash_table_init_n (&ainfo->fdr_hash.table, string_hash_newfunc,
			       1021))
    return NULL;

  ainfo->line = NULL;
  ainfo->line_end = NULL;
  ainfo->pdr = NULL;
  ainfo->pdr_end = NULL;
  ainfo->sym = NULL;
  ainfo->sym_end = NULL;
  ainfo->opt = NULL;
  ainfo->opt_end = NULL;
  ainfo->aux = NULL;
  ainfo->aux_end = NULL;
  ainfo->ss = NULL;
  ainfo->ss_end = NULL;
  ainfo->ss_hash = NULL;
  ainfo->ss_hash_end = NULL;
  ainfo->fdr = NULL;
  ainfo->fdr_end = NULL;
  ainfo->rfd = NULL;
  ainfo->rfd_end = NULL;

  ainfo->largest_file_shuffle = 0;

  if (! info->relocateable)
    {
      if (! bfd_hash_table_init (&ainfo->str_hash.table, string_hash_newfunc))
	return NULL;

      /* The first entry in the string table is the empty string.  */
      output_debug->symbolic_header.issMax = 1;
    }

  ainfo->memory = objalloc_create ();
  if (ainfo->memory == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return NULL;
    }

  return (PTR) ainfo;
}

/* Free the accumulated debugging information.  */

void
bfd_ecoff_debug_free (handle, output_bfd, output_debug, output_swap, info)
     PTR handle;
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct ecoff_debug_info *output_debug ATTRIBUTE_UNUSED;
     const struct ecoff_debug_swap *output_swap ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  struct accumulate *ainfo = (struct accumulate *) handle;

  bfd_hash_table_free (&ainfo->fdr_hash.table);

  if (! info->relocateable)
    bfd_hash_table_free (&ainfo->str_hash.table);

  objalloc_free (ainfo->memory);

  free (ainfo);
}

/* Accumulate the debugging information from INPUT_BFD into
   OUTPUT_BFD.  The INPUT_DEBUG argument points to some ECOFF
   debugging information which we want to link into the information
   pointed to by the OUTPUT_DEBUG argument.  OUTPUT_SWAP and
   INPUT_SWAP point to the swapping information needed.  INFO is the
   linker information structure.  HANDLE is returned by
   bfd_ecoff_debug_init.  */

boolean
bfd_ecoff_debug_accumulate (handle, output_bfd, output_debug, output_swap,
			    input_bfd, input_debug, input_swap,
			    info)
     PTR handle;
     bfd *output_bfd;
     struct ecoff_debug_info *output_debug;
     const struct ecoff_debug_swap *output_swap;
     bfd *input_bfd;
     struct ecoff_debug_info *input_debug;
     const struct ecoff_debug_swap *input_swap;
     struct bfd_link_info *info;
{
  struct accumulate *ainfo = (struct accumulate *) handle;
  void (* const swap_sym_in) PARAMS ((bfd *, PTR, SYMR *))
    = input_swap->swap_sym_in;
  void (* const swap_rfd_in) PARAMS ((bfd *, PTR, RFDT *))
    = input_swap->swap_rfd_in;
  void (* const swap_sym_out) PARAMS ((bfd *, const SYMR *, PTR))
    = output_swap->swap_sym_out;
  void (* const swap_fdr_out) PARAMS ((bfd *, const FDR *, PTR))
    = output_swap->swap_fdr_out;
  void (* const swap_rfd_out) PARAMS ((bfd *, const RFDT *, PTR))
    = output_swap->swap_rfd_out;
  bfd_size_type external_pdr_size = output_swap->external_pdr_size;
  bfd_size_type external_sym_size = output_swap->external_sym_size;
  bfd_size_type external_opt_size = output_swap->external_opt_size;
  bfd_size_type external_fdr_size = output_swap->external_fdr_size;
  bfd_size_type external_rfd_size = output_swap->external_rfd_size;
  HDRR * const output_symhdr = &output_debug->symbolic_header;
  HDRR * const input_symhdr = &input_debug->symbolic_header;
  bfd_vma section_adjust[scMax];
  asection *sec;
  bfd_byte *fdr_start;
  bfd_byte *fdr_ptr;
  bfd_byte *fdr_end;
  bfd_size_type fdr_add;
  unsigned int copied;
  RFDT i;
  unsigned long sz;
  bfd_byte *rfd_out;
  bfd_byte *rfd_in;
  bfd_byte *rfd_end;
  long newrfdbase = 0;
  long oldrfdbase = 0;
  bfd_byte *fdr_out;

  /* Use section_adjust to hold the value to add to a symbol in a
     particular section.  */
  memset ((PTR) section_adjust, 0, sizeof section_adjust);

#define SET(name, indx) \
  sec = bfd_get_section_by_name (input_bfd, name); \
  if (sec != NULL) \
    section_adjust[indx] = (sec->output_section->vma \
			    + sec->output_offset \
			    - sec->vma);

  SET (".text", scText);
  SET (".data", scData);
  SET (".bss", scBss);
  SET (".sdata", scSData);
  SET (".sbss", scSBss);
  /* scRdata section may be either .rdata or .rodata.  */
  SET (".rdata", scRData);
  SET (".rodata", scRData);
  SET (".init", scInit);
  SET (".fini", scFini);
  SET (".rconst", scRConst);

#undef SET

  /* Find all the debugging information based on the FDR's.  We need
     to handle them whether they are swapped or not.  */
  if (input_debug->fdr != (FDR *) NULL)
    {
      fdr_start = (bfd_byte *) input_debug->fdr;
      fdr_add = sizeof (FDR);
    }
  else
    {
      fdr_start = (bfd_byte *) input_debug->external_fdr;
      fdr_add = input_swap->external_fdr_size;
    }
  fdr_end = fdr_start + input_symhdr->ifdMax * fdr_add;

  input_debug->ifdmap = (RFDT *) bfd_alloc (input_bfd,
					    (input_symhdr->ifdMax
					     * sizeof (RFDT)));

  sz = (input_symhdr->crfd + input_symhdr->ifdMax) * external_rfd_size;
  rfd_out = (bfd_byte *) objalloc_alloc (ainfo->memory, sz);
  if (!input_debug->ifdmap || !rfd_out)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  if (!add_memory_shuffle (ainfo, &ainfo->rfd, &ainfo->rfd_end, rfd_out, sz))
    return false;

  copied = 0;

  /* Look through the FDR's to see which ones we are going to include
     in the final output.  We do not want duplicate FDR information
     for header files, because ECOFF debugging is often very large.
     When we find an FDR with no line information which can be merged,
     we look it up in a hash table to ensure that we only include it
     once.  We keep a table mapping FDR numbers to the final number
     they get with the BFD, so that we can refer to it when we write
     out the external symbols.  */
  for (fdr_ptr = fdr_start, i = 0;
       fdr_ptr < fdr_end;
       fdr_ptr += fdr_add, i++, rfd_out += external_rfd_size)
    {
      FDR fdr;

      if (input_debug->fdr != (FDR *) NULL)
	fdr = *(FDR *) fdr_ptr;
      else
	(*input_swap->swap_fdr_in) (input_bfd, (PTR) fdr_ptr, &fdr);

      /* See if this FDR can be merged with an existing one.  */
      if (fdr.cbLine == 0 && fdr.rss != -1 && fdr.fMerge)
	{
	  const char *name;
	  char *lookup;
	  struct string_hash_entry *fh;

	  /* We look up a string formed from the file name and the
	     number of symbols and aux entries.  Sometimes an include
	     file will conditionally define a typedef or something
	     based on the order of include files.  Using the number of
	     symbols and aux entries as a hash reduces the chance that
	     we will merge symbol information that should not be
	     merged.  */
	  name = input_debug->ss + fdr.issBase + fdr.rss;

	  lookup = (char *) bfd_malloc (strlen (name) + 20);
	  if (lookup == NULL)
	    return false;
	  sprintf (lookup, "%s %lx %lx", name, fdr.csym, fdr.caux);

	  fh = string_hash_lookup (&ainfo->fdr_hash, lookup, true, true);
	  free (lookup);
	  if (fh == (struct string_hash_entry *) NULL)
	    return false;

	  if (fh->val != -1)
	    {
	      input_debug->ifdmap[i] = fh->val;
	      (*swap_rfd_out) (output_bfd, input_debug->ifdmap + i,
			       (PTR) rfd_out);

	      /* Don't copy this FDR.  */
	      continue;
	    }

	  fh->val = output_symhdr->ifdMax + copied;
	}

      input_debug->ifdmap[i] = output_symhdr->ifdMax + copied;
      (*swap_rfd_out) (output_bfd, input_debug->ifdmap + i, (PTR) rfd_out);
      ++copied;
    }

  newrfdbase = output_symhdr->crfd;
  output_symhdr->crfd += input_symhdr->ifdMax;

  /* Copy over any existing RFD's.  RFD's are only created by the
     linker, so this will only happen for input files which are the
     result of a partial link.  */
  rfd_in = (bfd_byte *) input_debug->external_rfd;
  rfd_end = rfd_in + input_symhdr->crfd * input_swap->external_rfd_size;
  for (;
       rfd_in < rfd_end;
       rfd_in += input_swap->external_rfd_size)
    {
      RFDT rfd;

      (*swap_rfd_in) (input_bfd, (PTR) rfd_in, &rfd);
      BFD_ASSERT (rfd >= 0 && rfd < input_symhdr->ifdMax);
      rfd = input_debug->ifdmap[rfd];
      (*swap_rfd_out) (output_bfd, &rfd, (PTR) rfd_out);
      rfd_out += external_rfd_size;
    }

  oldrfdbase = output_symhdr->crfd;
  output_symhdr->crfd += input_symhdr->crfd;

  /* Look through the FDR's and copy over all associated debugging
     information.  */
  sz = copied * external_fdr_size;
  fdr_out = (bfd_byte *) objalloc_alloc (ainfo->memory, sz);
  if (!fdr_out)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  if (!add_memory_shuffle (ainfo, &ainfo->fdr, &ainfo->fdr_end, fdr_out, sz))
    return false;
  for (fdr_ptr = fdr_start, i = 0;
       fdr_ptr < fdr_end;
       fdr_ptr += fdr_add, i++)
    {
      FDR fdr;
      bfd_vma fdr_adr;
      bfd_byte *sym_out;
      bfd_byte *lraw_src;
      bfd_byte *lraw_end;
      boolean fgotfilename;

      if (input_debug->ifdmap[i] < output_symhdr->ifdMax)
	{
	  /* We are not copying this FDR.  */
	  continue;
	}

      if (input_debug->fdr != (FDR *) NULL)
	fdr = *(FDR *) fdr_ptr;
      else
	(*input_swap->swap_fdr_in) (input_bfd, (PTR) fdr_ptr, &fdr);

      fdr_adr = fdr.adr;

      /* Adjust the FDR address for any changes that may have been
	 made by relaxing.  */
      if (input_debug->adjust != (struct ecoff_value_adjust *) NULL)
	{
	  struct ecoff_value_adjust *adjust;

	  for (adjust = input_debug->adjust;
	       adjust != (struct ecoff_value_adjust *) NULL;
	       adjust = adjust->next)
	    if (fdr_adr >= adjust->start
		&& fdr_adr < adjust->end)
	      fdr.adr += adjust->adjust;
	}

      /* FIXME: It is conceivable that this FDR points to the .init or
	 .fini section, in which case this will not do the right
	 thing.  */
      fdr.adr += section_adjust[scText];

      /* Swap in the local symbols, adjust their values, and swap them
	 out again.  */
      fgotfilename = false;
      sz = fdr.csym * external_sym_size;
      sym_out = (bfd_byte *) objalloc_alloc (ainfo->memory, sz);
      if (!sym_out)
	{
	  bfd_set_error (bfd_error_no_memory);
	  return false;
	}
      if (!add_memory_shuffle (ainfo, &ainfo->sym, &ainfo->sym_end, sym_out,
			       sz))
	return false;
      lraw_src = ((bfd_byte *) input_debug->external_sym
		  + fdr.isymBase * input_swap->external_sym_size);
      lraw_end = lraw_src + fdr.csym * input_swap->external_sym_size;
      for (;  lraw_src < lraw_end;  lraw_src += input_swap->external_sym_size)
	{
	  SYMR internal_sym;

	  (*swap_sym_in) (input_bfd, (PTR) lraw_src, &internal_sym);

	  BFD_ASSERT (internal_sym.sc != scCommon
		      && internal_sym.sc != scSCommon);

	  /* Adjust the symbol value if appropriate.  */
	  switch (internal_sym.st)
	    {
	    case stNil:
	      if (ECOFF_IS_STAB (&internal_sym))
		break;
	      /* Fall through.  */
	    case stGlobal:
	    case stStatic:
	    case stLabel:
	    case stProc:
	    case stStaticProc:
	      if (input_debug->adjust != (struct ecoff_value_adjust *) NULL)
		{
		  bfd_vma value;
		  struct ecoff_value_adjust *adjust;

		  value = internal_sym.value;
		  for (adjust = input_debug->adjust;
		       adjust != (struct ecoff_value_adjust *) NULL;
		       adjust = adjust->next)
		    if (value >= adjust->start
			&& value < adjust->end)
		      internal_sym.value += adjust->adjust;
		}
	      internal_sym.value += section_adjust[internal_sym.sc];
	      break;

	    default:
	      break;
	    }

	  /* If we are doing a final link, we hash all the strings in
	     the local symbol table together.  This reduces the amount
	     of space required by debugging information.  We don't do
	     this when performing a relocateable link because it would
	     prevent us from easily merging different FDR's.  */
	  if (! info->relocateable)
	    {
	      boolean ffilename;
	      const char *name;

	      if (! fgotfilename && internal_sym.iss == fdr.rss)
		ffilename = true;
	      else
		ffilename = false;

	      /* Hash the name into the string table.  */
	      name = input_debug->ss + fdr.issBase + internal_sym.iss;
	      if (*name == '\0')
		internal_sym.iss = 0;
	      else
		{
		  struct string_hash_entry *sh;

		  sh = string_hash_lookup (&ainfo->str_hash, name, true, true);
		  if (sh == (struct string_hash_entry *) NULL)
		    return false;
		  if (sh->val == -1)
		    {
		      sh->val = output_symhdr->issMax;
		      output_symhdr->issMax += strlen (name) + 1;
		      if (ainfo->ss_hash == (struct string_hash_entry *) NULL)
			ainfo->ss_hash = sh;
		      if (ainfo->ss_hash_end
			  != (struct string_hash_entry *) NULL)
			ainfo->ss_hash_end->next = sh;
		      ainfo->ss_hash_end = sh;
		    }
		  internal_sym.iss = sh->val;
		}

	      if (ffilename)
		{
		  fdr.rss = internal_sym.iss;
		  fgotfilename = true;
		}
	    }

	  (*swap_sym_out) (output_bfd, &internal_sym, sym_out);
	  sym_out += external_sym_size;
	}

      fdr.isymBase = output_symhdr->isymMax;
      output_symhdr->isymMax += fdr.csym;

      /* Copy the information that does not need swapping.  */

      /* FIXME: If we are relaxing, we need to adjust the line
	 numbers.  Frankly, forget it.  Anybody using stabs debugging
	 information will not use this line number information, and
	 stabs are adjusted correctly.  */
      if (fdr.cbLine > 0)
	{
	  if (!add_file_shuffle (ainfo, &ainfo->line, &ainfo->line_end,
				 input_bfd,
				 input_symhdr->cbLineOffset + fdr.cbLineOffset,
				 fdr.cbLine))
	    return false;
	  fdr.ilineBase = output_symhdr->ilineMax;
	  fdr.cbLineOffset = output_symhdr->cbLine;
	  output_symhdr->ilineMax += fdr.cline;
	  output_symhdr->cbLine += fdr.cbLine;
	}
      if (fdr.caux > 0)
	{
	  if (!add_file_shuffle (ainfo, &ainfo->aux, &ainfo->aux_end,
				 input_bfd,
				 (input_symhdr->cbAuxOffset
				  + fdr.iauxBase * sizeof (union aux_ext)),
				 fdr.caux * sizeof (union aux_ext)))
	    return false;
	  fdr.iauxBase = output_symhdr->iauxMax;
	  output_symhdr->iauxMax += fdr.caux;
	}
      if (! info->relocateable)
	{

	  /* When are are hashing strings, we lie about the number of
	     strings attached to each FDR.  We need to set cbSs
	     because some versions of dbx apparently use it to decide
	     how much of the string table to read in.  */
	  fdr.issBase = 0;
	  fdr.cbSs = output_symhdr->issMax;
	}
      else if (fdr.cbSs > 0)
	{
	  if (!add_file_shuffle (ainfo, &ainfo->ss, &ainfo->ss_end,
				 input_bfd,
				 input_symhdr->cbSsOffset + fdr.issBase,
				 fdr.cbSs))
	    return false;
	  fdr.issBase = output_symhdr->issMax;
	  output_symhdr->issMax += fdr.cbSs;
	}

      if ((output_bfd->xvec->header_byteorder
	   == input_bfd->xvec->header_byteorder)
	  && input_debug->adjust == (struct ecoff_value_adjust *) NULL)
	{
	  /* The two BFD's have the same endianness, and we don't have
	     to adjust the PDR addresses, so simply copying the
	     information will suffice.  */
	  BFD_ASSERT (external_pdr_size == input_swap->external_pdr_size);
	  if (fdr.cpd > 0)
	    {
	      if (!add_file_shuffle (ainfo, &ainfo->pdr, &ainfo->pdr_end,
				     input_bfd,
				     (input_symhdr->cbPdOffset
				      + fdr.ipdFirst * external_pdr_size),
				     fdr.cpd * external_pdr_size))
		return false;
	    }
	  BFD_ASSERT (external_opt_size == input_swap->external_opt_size);
	  if (fdr.copt > 0)
	    {
	      if (!add_file_shuffle (ainfo, &ainfo->opt, &ainfo->opt_end,
				     input_bfd,
				     (input_symhdr->cbOptOffset
				      + fdr.ioptBase * external_opt_size),
				     fdr.copt * external_opt_size))
		return false;
	    }
	}
      else
	{
	  bfd_size_type outsz, insz;
	  bfd_byte *in;
	  bfd_byte *end;
	  bfd_byte *out;

	  /* The two BFD's have different endianness, so we must swap
	     everything in and out.  This code would always work, but
	     it would be unnecessarily slow in the normal case.  */
	  outsz = external_pdr_size;
	  insz = input_swap->external_pdr_size;
	  in = ((bfd_byte *) input_debug->external_pdr
		+ fdr.ipdFirst * insz);
	  end = in + fdr.cpd * insz;
	  sz = fdr.cpd * outsz;
	  out = (bfd_byte *) objalloc_alloc (ainfo->memory, sz);
	  if (!out)
	    {
	      bfd_set_error (bfd_error_no_memory);
	      return false;
	    }
	  if (!add_memory_shuffle (ainfo, &ainfo->pdr, &ainfo->pdr_end, out,
				   sz))
	    return false;
	  for (; in < end; in += insz, out += outsz)
	    {
	      PDR pdr;

	      (*input_swap->swap_pdr_in) (input_bfd, (PTR) in, &pdr);

	      /* If we have been relaxing, we may have to adjust the
		 address.  */
	      if (input_debug->adjust != (struct ecoff_value_adjust *) NULL)
		{
		  bfd_vma adr;
		  struct ecoff_value_adjust *adjust;

		  adr = fdr_adr + pdr.adr;
		  for (adjust = input_debug->adjust;
		       adjust != (struct ecoff_value_adjust *) NULL;
		       adjust = adjust->next)
		    if (adr >= adjust->start
			&& adr < adjust->end)
		      pdr.adr += adjust->adjust;
		}

	      (*output_swap->swap_pdr_out) (output_bfd, &pdr, (PTR) out);
	    }

	  /* Swap over the optimization information.  */
	  outsz = external_opt_size;
	  insz = input_swap->external_opt_size;
	  in = ((bfd_byte *) input_debug->external_opt
		+ fdr.ioptBase * insz);
	  end = in + fdr.copt * insz;
	  sz = fdr.copt * outsz;
	  out = (bfd_byte *) objalloc_alloc (ainfo->memory, sz);
	  if (!out)
	    {
	      bfd_set_error (bfd_error_no_memory);
	      return false;
	    }
	  if (!add_memory_shuffle (ainfo, &ainfo->opt, &ainfo->opt_end, out,
				   sz))
	    return false;
	  for (; in < end; in += insz, out += outsz)
	    {
	      OPTR opt;

	      (*input_swap->swap_opt_in) (input_bfd, (PTR) in, &opt);
	      (*output_swap->swap_opt_out) (output_bfd, &opt, (PTR) out);
	    }
	}

      fdr.ipdFirst = output_symhdr->ipdMax;
      output_symhdr->ipdMax += fdr.cpd;
      fdr.ioptBase = output_symhdr->ioptMax;
      output_symhdr->ioptMax += fdr.copt;

      if (fdr.crfd <= 0)
	{
	  /* Point this FDR at the table of RFD's we created.  */
	  fdr.rfdBase = newrfdbase;
	  fdr.crfd = input_symhdr->ifdMax;
	}
      else
	{
	  /* Point this FDR at the remapped RFD's.  */
	  fdr.rfdBase += oldrfdbase;
	}

      (*swap_fdr_out) (output_bfd, &fdr, fdr_out);
      fdr_out += external_fdr_size;
      ++output_symhdr->ifdMax;
    }

  return true;
}

/* Add a string to the debugging information we are accumulating.
   Return the offset from the fdr string base.  */

static long ecoff_add_string PARAMS ((struct accumulate *,
				      struct bfd_link_info *,
				      struct ecoff_debug_info *,
				      FDR *fdr, const char *string));

static long
ecoff_add_string (ainfo, info, debug, fdr, string)
     struct accumulate *ainfo;
     struct bfd_link_info *info;
     struct ecoff_debug_info *debug;
     FDR *fdr;
     const char *string;
{
  HDRR *symhdr;
  size_t len;
  bfd_size_type ret;

  symhdr = &debug->symbolic_header;
  len = strlen (string);
  if (info->relocateable)
    {
      if (!add_memory_shuffle (ainfo, &ainfo->ss, &ainfo->ss_end, (PTR) string,
			       len + 1))
	return -1;
      ret = symhdr->issMax;
      symhdr->issMax += len + 1;
      fdr->cbSs += len + 1;
    }
  else
    {
      struct string_hash_entry *sh;

      sh = string_hash_lookup (&ainfo->str_hash, string, true, true);
      if (sh == (struct string_hash_entry *) NULL)
	return -1;
      if (sh->val == -1)
	{
	  sh->val = symhdr->issMax;
	  symhdr->issMax += len + 1;
	  if (ainfo->ss_hash == (struct string_hash_entry *) NULL)
	    ainfo->ss_hash = sh;
	  if (ainfo->ss_hash_end
	      != (struct string_hash_entry *) NULL)
	    ainfo->ss_hash_end->next = sh;
	  ainfo->ss_hash_end = sh;
	}
      ret = sh->val;
    }

  return ret;
}

/* Add debugging information from a non-ECOFF file.  */

boolean
bfd_ecoff_debug_accumulate_other (handle, output_bfd, output_debug,
				  output_swap, input_bfd, info)
     PTR handle;
     bfd *output_bfd;
     struct ecoff_debug_info *output_debug;
     const struct ecoff_debug_swap *output_swap;
     bfd *input_bfd;
     struct bfd_link_info *info;
{
  struct accumulate *ainfo = (struct accumulate *) handle;
  void (* const swap_sym_out) PARAMS ((bfd *, const SYMR *, PTR))
    = output_swap->swap_sym_out;
  HDRR *output_symhdr = &output_debug->symbolic_header;
  FDR fdr;
  asection *sec;
  asymbol **symbols;
  asymbol **sym_ptr;
  asymbol **sym_end;
  long symsize;
  long symcount;
  PTR external_fdr;

  memset ((PTR) &fdr, 0, sizeof fdr);

  sec = bfd_get_section_by_name (input_bfd, ".text");
  if (sec != NULL)
    fdr.adr = sec->output_section->vma + sec->output_offset;
  else
    {
      /* FIXME: What about .init or .fini?  */
      fdr.adr = 0;
    }

  fdr.issBase = output_symhdr->issMax;
  fdr.cbSs = 0;
  fdr.rss = ecoff_add_string (ainfo, info, output_debug, &fdr,
			      bfd_get_filename (input_bfd));
  if (fdr.rss == -1)
    return false;
  fdr.isymBase = output_symhdr->isymMax;

  /* Get the local symbols from the input BFD.  */
  symsize = bfd_get_symtab_upper_bound (input_bfd);
  if (symsize < 0)
    return false;
  symbols = (asymbol **) bfd_alloc (output_bfd, symsize);
  if (symbols == (asymbol **) NULL)
    return false;
  symcount = bfd_canonicalize_symtab (input_bfd, symbols);
  if (symcount < 0)
    return false;
  sym_end = symbols + symcount;

  /* Handle the local symbols.  Any external symbols are handled
     separately.  */
  fdr.csym = 0;
  for (sym_ptr = symbols; sym_ptr != sym_end; sym_ptr++)
    {
      SYMR internal_sym;
      PTR external_sym;

      if (((*sym_ptr)->flags & BSF_EXPORT) != 0)
	continue;
      memset ((PTR) &internal_sym, 0, sizeof internal_sym);
      internal_sym.iss = ecoff_add_string (ainfo, info, output_debug, &fdr,
					   (*sym_ptr)->name);

      if (internal_sym.iss == -1)
	return false;
      if (bfd_is_com_section ((*sym_ptr)->section)
	  || bfd_is_und_section ((*sym_ptr)->section))
	internal_sym.value = (*sym_ptr)->value;
      else
	internal_sym.value = ((*sym_ptr)->value
			      + (*sym_ptr)->section->output_offset
			      + (*sym_ptr)->section->output_section->vma);
      internal_sym.st = stNil;
      internal_sym.sc = scUndefined;
      internal_sym.index = indexNil;

      external_sym = (PTR) objalloc_alloc (ainfo->memory,
					   output_swap->external_sym_size);
      if (!external_sym)
	{
	  bfd_set_error (bfd_error_no_memory);
	  return false;
	}
      (*swap_sym_out) (output_bfd, &internal_sym, external_sym);
      add_memory_shuffle (ainfo, &ainfo->sym, &ainfo->sym_end,
			  external_sym, output_swap->external_sym_size);
      ++fdr.csym;
      ++output_symhdr->isymMax;
    }

  bfd_release (output_bfd, (PTR) symbols);

  /* Leave everything else in the FDR zeroed out.  This will cause
     the lang field to be langC.  The fBigendian field will
     indicate little endian format, but it doesn't matter because
     it only applies to aux fields and there are none.  */
  external_fdr = (PTR) objalloc_alloc (ainfo->memory,
				       output_swap->external_fdr_size);
  if (!external_fdr)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  (*output_swap->swap_fdr_out) (output_bfd, &fdr, external_fdr);
  add_memory_shuffle (ainfo, &ainfo->fdr, &ainfo->fdr_end,
		      external_fdr, output_swap->external_fdr_size);

  ++output_symhdr->ifdMax;

  return true;
}

/* Set up ECOFF debugging information for the external symbols.
   FIXME: This is done using a memory buffer, but it should be
   probably be changed to use a shuffle structure.  The assembler uses
   this interface, so that must be changed to do something else.  */

boolean
bfd_ecoff_debug_externals (abfd, debug, swap, relocateable, get_extr,
			   set_index)
     bfd *abfd;
     struct ecoff_debug_info *debug;
     const struct ecoff_debug_swap *swap;
     boolean relocateable;
     boolean (*get_extr) PARAMS ((asymbol *, EXTR *));
     void (*set_index) PARAMS ((asymbol *, bfd_size_type));
{
  HDRR * const symhdr = &debug->symbolic_header;
  asymbol **sym_ptr_ptr;
  size_t c;

  sym_ptr_ptr = bfd_get_outsymbols (abfd);
  if (sym_ptr_ptr == NULL)
    return true;

  for (c = bfd_get_symcount (abfd); c > 0; c--, sym_ptr_ptr++)
    {
      asymbol *sym_ptr;
      EXTR esym;

      sym_ptr = *sym_ptr_ptr;

      /* Get the external symbol information.  */
      if ((*get_extr) (sym_ptr, &esym) == false)
	continue;

      /* If we're producing an executable, move common symbols into
	 bss.  */
      if (relocateable == false)
	{
	  if (esym.asym.sc == scCommon)
	    esym.asym.sc = scBss;
	  else if (esym.asym.sc == scSCommon)
	    esym.asym.sc = scSBss;
	}

      if (bfd_is_com_section (sym_ptr->section)
	  || bfd_is_und_section (sym_ptr->section)
	  || sym_ptr->section->output_section == (asection *) NULL)
	{
	  /* FIXME: gas does not keep the value of a small undefined
	     symbol in the symbol itself, because of relocation
	     problems.  */
	  if (esym.asym.sc != scSUndefined
	      || esym.asym.value == 0
	      || sym_ptr->value != 0)
	    esym.asym.value = sym_ptr->value;
	}
      else
	esym.asym.value = (sym_ptr->value
			   + sym_ptr->section->output_offset
			   + sym_ptr->section->output_section->vma);

      if (set_index)
	(*set_index) (sym_ptr, (bfd_size_type) symhdr->iextMax);

      if (! bfd_ecoff_debug_one_external (abfd, debug, swap,
					  sym_ptr->name, &esym))
	return false;
    }

  return true;
}

/* Add a single external symbol to the debugging information.  */

boolean
bfd_ecoff_debug_one_external (abfd, debug, swap, name, esym)
     bfd *abfd;
     struct ecoff_debug_info *debug;
     const struct ecoff_debug_swap *swap;
     const char *name;
     EXTR *esym;
{
  const bfd_size_type external_ext_size = swap->external_ext_size;
  void (* const swap_ext_out) PARAMS ((bfd *, const EXTR *, PTR))
    = swap->swap_ext_out;
  HDRR * const symhdr = &debug->symbolic_header;
  size_t namelen;

  namelen = strlen (name);

  if ((size_t) (debug->ssext_end - debug->ssext)
      < symhdr->issExtMax + namelen + 1)
    {
      if (ecoff_add_bytes ((char **) &debug->ssext,
			   (char **) &debug->ssext_end,
			   symhdr->issExtMax + namelen + 1)
	  == false)
	return false;
    }
  if ((size_t) ((char *) debug->external_ext_end
		- (char *) debug->external_ext)
      < (symhdr->iextMax + 1) * external_ext_size)
    {
      if (ecoff_add_bytes ((char **) &debug->external_ext,
			   (char **) &debug->external_ext_end,
			   (symhdr->iextMax + 1) * external_ext_size)
	  == false)
	return false;
    }

  esym->asym.iss = symhdr->issExtMax;

  (*swap_ext_out) (abfd, esym,
		   ((char *) debug->external_ext
		    + symhdr->iextMax * swap->external_ext_size));

  ++symhdr->iextMax;

  strcpy (debug->ssext + symhdr->issExtMax, name);
  symhdr->issExtMax += namelen + 1;

  return true;
}

/* Align the ECOFF debugging information.  */

static void
ecoff_align_debug (abfd, debug, swap)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct ecoff_debug_info *debug;
     const struct ecoff_debug_swap *swap;
{
  HDRR * const symhdr = &debug->symbolic_header;
  bfd_size_type debug_align, aux_align, rfd_align;
  size_t add;

  /* Adjust the counts so that structures are aligned.  */
  debug_align = swap->debug_align;
  aux_align = debug_align / sizeof (union aux_ext);
  rfd_align = debug_align / swap->external_rfd_size;

  add = debug_align - (symhdr->cbLine & (debug_align - 1));
  if (add != debug_align)
    {
      if (debug->line != (unsigned char *) NULL)
	memset ((PTR) (debug->line + symhdr->cbLine), 0, add);
      symhdr->cbLine += add;
    }

  add = debug_align - (symhdr->issMax & (debug_align - 1));
  if (add != debug_align)
    {
      if (debug->ss != (char *) NULL)
	memset ((PTR) (debug->ss + symhdr->issMax), 0, add);
      symhdr->issMax += add;
    }

  add = debug_align - (symhdr->issExtMax & (debug_align - 1));
  if (add != debug_align)
    {
      if (debug->ssext != (char *) NULL)
	memset ((PTR) (debug->ssext + symhdr->issExtMax), 0, add);
      symhdr->issExtMax += add;
    }

  add = aux_align - (symhdr->iauxMax & (aux_align - 1));
  if (add != aux_align)
    {
      if (debug->external_aux != (union aux_ext *) NULL)
	memset ((PTR) (debug->external_aux + symhdr->iauxMax), 0,
		add * sizeof (union aux_ext));
      symhdr->iauxMax += add;
    }

  add = rfd_align - (symhdr->crfd & (rfd_align - 1));
  if (add != rfd_align)
    {
      if (debug->external_rfd != (PTR) NULL)
	memset ((PTR) ((char *) debug->external_rfd
		       + symhdr->crfd * swap->external_rfd_size),
		0, (size_t) (add * swap->external_rfd_size));
      symhdr->crfd += add;
    }
}

/* Return the size required by the ECOFF debugging information.  */

bfd_size_type
bfd_ecoff_debug_size (abfd, debug, swap)
     bfd *abfd;
     struct ecoff_debug_info *debug;
     const struct ecoff_debug_swap *swap;
{
  bfd_size_type tot;

  ecoff_align_debug (abfd, debug, swap);
  tot = swap->external_hdr_size;

#define ADD(count, size) \
  tot += debug->symbolic_header.count * size

  ADD (cbLine, sizeof (unsigned char));
  ADD (idnMax, swap->external_dnr_size);
  ADD (ipdMax, swap->external_pdr_size);
  ADD (isymMax, swap->external_sym_size);
  ADD (ioptMax, swap->external_opt_size);
  ADD (iauxMax, sizeof (union aux_ext));
  ADD (issMax, sizeof (char));
  ADD (issExtMax, sizeof (char));
  ADD (ifdMax, swap->external_fdr_size);
  ADD (crfd, swap->external_rfd_size);
  ADD (iextMax, swap->external_ext_size);

#undef ADD

  return tot;
}

/* Write out the ECOFF symbolic header, given the file position it is
   going to be placed at.  This assumes that the counts are set
   correctly.  */

static boolean
ecoff_write_symhdr (abfd, debug, swap, where)
     bfd *abfd;
     struct ecoff_debug_info *debug;
     const struct ecoff_debug_swap *swap;
     file_ptr where;
{
  HDRR * const symhdr = &debug->symbolic_header;
  char *buff = NULL;

  ecoff_align_debug (abfd, debug, swap);

  /* Go to the right location in the file.  */
  if (bfd_seek (abfd, where, SEEK_SET) != 0)
    return false;

  where += swap->external_hdr_size;

  symhdr->magic = swap->sym_magic;

  /* Fill in the file offsets.  */
#define SET(offset, count, size) \
  if (symhdr->count == 0) \
    symhdr->offset = 0; \
  else \
    { \
      symhdr->offset = where; \
      where += symhdr->count * size; \
    }

  SET (cbLineOffset, cbLine, sizeof (unsigned char));
  SET (cbDnOffset, idnMax, swap->external_dnr_size);
  SET (cbPdOffset, ipdMax, swap->external_pdr_size);
  SET (cbSymOffset, isymMax, swap->external_sym_size);
  SET (cbOptOffset, ioptMax, swap->external_opt_size);
  SET (cbAuxOffset, iauxMax, sizeof (union aux_ext));
  SET (cbSsOffset, issMax, sizeof (char));
  SET (cbSsExtOffset, issExtMax, sizeof (char));
  SET (cbFdOffset, ifdMax, swap->external_fdr_size);
  SET (cbRfdOffset, crfd, swap->external_rfd_size);
  SET (cbExtOffset, iextMax, swap->external_ext_size);
#undef SET

  buff = (PTR) bfd_malloc ((size_t) swap->external_hdr_size);
  if (buff == NULL && swap->external_hdr_size != 0)
    goto error_return;

  (*swap->swap_hdr_out) (abfd, symhdr, buff);
  if (bfd_write (buff, 1, swap->external_hdr_size, abfd)
      != swap->external_hdr_size)
    goto error_return;

  if (buff != NULL)
    free (buff);
  return true;
 error_return:
  if (buff != NULL)
    free (buff);
  return false;
}

/* Write out the ECOFF debugging information.  This function assumes
   that the information (the pointers and counts) in *DEBUG have been
   set correctly.  WHERE is the position in the file to write the
   information to.  This function fills in the file offsets in the
   symbolic header.  */

boolean
bfd_ecoff_write_debug (abfd, debug, swap, where)
     bfd *abfd;
     struct ecoff_debug_info *debug;
     const struct ecoff_debug_swap *swap;
     file_ptr where;
{
  HDRR * const symhdr = &debug->symbolic_header;

  if (! ecoff_write_symhdr (abfd, debug, swap, where))
    return false;

#define WRITE(ptr, count, size, offset) \
  BFD_ASSERT (symhdr->offset == 0 \
	      || (bfd_vma) bfd_tell (abfd) == symhdr->offset); \
  if (bfd_write ((PTR) debug->ptr, size, symhdr->count, abfd) \
      != size * symhdr->count) \
    return false;

  WRITE (line, cbLine, sizeof (unsigned char), cbLineOffset);
  WRITE (external_dnr, idnMax, swap->external_dnr_size, cbDnOffset);
  WRITE (external_pdr, ipdMax, swap->external_pdr_size, cbPdOffset);
  WRITE (external_sym, isymMax, swap->external_sym_size, cbSymOffset);
  WRITE (external_opt, ioptMax, swap->external_opt_size, cbOptOffset);
  WRITE (external_aux, iauxMax, sizeof (union aux_ext), cbAuxOffset);
  WRITE (ss, issMax, sizeof (char), cbSsOffset);
  WRITE (ssext, issExtMax, sizeof (char), cbSsExtOffset);
  WRITE (external_fdr, ifdMax, swap->external_fdr_size, cbFdOffset);
  WRITE (external_rfd, crfd, swap->external_rfd_size, cbRfdOffset);
  WRITE (external_ext, iextMax, swap->external_ext_size, cbExtOffset);
#undef WRITE

  return true;
}

/* Write out a shuffle list.  */

static boolean ecoff_write_shuffle PARAMS ((bfd *,
					    const struct ecoff_debug_swap *,
					    struct shuffle *, PTR space));

static boolean
ecoff_write_shuffle (abfd, swap, shuffle, space)
     bfd *abfd;
     const struct ecoff_debug_swap *swap;
     struct shuffle *shuffle;
     PTR space;
{
  register struct shuffle *l;
  unsigned long total;

  total = 0;
  for (l = shuffle; l != (struct shuffle *) NULL; l = l->next)
    {
      if (! l->filep)
	{
	  if (bfd_write (l->u.memory, 1, l->size, abfd) != l->size)
	    return false;
	}
      else
	{
	  if (bfd_seek (l->u.file.input_bfd, l->u.file.offset, SEEK_SET) != 0
	      || bfd_read (space, 1, l->size, l->u.file.input_bfd) != l->size
	      || bfd_write (space, 1, l->size, abfd) != l->size)
	    return false;
	}
      total += l->size;
    }

  if ((total & (swap->debug_align - 1)) != 0)
    {
      unsigned int i;
      bfd_byte *s;

      i = swap->debug_align - (total & (swap->debug_align - 1));
      s = (bfd_byte *) bfd_malloc (i);
      if (s == NULL && i != 0)
	return false;

      memset ((PTR) s, 0, i);
      if (bfd_write ((PTR) s, 1, i, abfd) != i)
	{
	  free (s);
	  return false;
	}
      free (s);
    }

  return true;
}

/* Write out debugging information using accumulated linker
   information.  */

boolean
bfd_ecoff_write_accumulated_debug (handle, abfd, debug, swap, info, where)
     PTR handle;
     bfd *abfd;
     struct ecoff_debug_info *debug;
     const struct ecoff_debug_swap *swap;
     struct bfd_link_info *info;
     file_ptr where;
{
  struct accumulate *ainfo = (struct accumulate *) handle;
  PTR space = NULL;

  if (! ecoff_write_symhdr (abfd, debug, swap, where))
    goto error_return;

  space = (PTR) bfd_malloc (ainfo->largest_file_shuffle);
  if (space == NULL && ainfo->largest_file_shuffle != 0)
    goto error_return;

  if (! ecoff_write_shuffle (abfd, swap, ainfo->line, space)
      || ! ecoff_write_shuffle (abfd, swap, ainfo->pdr, space)
      || ! ecoff_write_shuffle (abfd, swap, ainfo->sym, space)
      || ! ecoff_write_shuffle (abfd, swap, ainfo->opt, space)
      || ! ecoff_write_shuffle (abfd, swap, ainfo->aux, space))
    goto error_return;

  /* The string table is written out from the hash table if this is a
     final link.  */
  if (info->relocateable)
    {
      BFD_ASSERT (ainfo->ss_hash == (struct string_hash_entry *) NULL);
      if (! ecoff_write_shuffle (abfd, swap, ainfo->ss, space))
	goto error_return;
    }
  else
    {
      unsigned long total;
      bfd_byte null;
      struct string_hash_entry *sh;

      BFD_ASSERT (ainfo->ss == (struct shuffle *) NULL);
      null = 0;
      if (bfd_write ((PTR) &null, 1, 1, abfd) != 1)
	goto error_return;
      total = 1;
      BFD_ASSERT (ainfo->ss_hash == NULL || ainfo->ss_hash->val == 1);
      for (sh = ainfo->ss_hash;
	   sh != (struct string_hash_entry *) NULL;
	   sh = sh->next)
	{
	  size_t len;

	  len = strlen (sh->root.string);
	  if (bfd_write ((PTR) sh->root.string, 1, len + 1, abfd) != len + 1)
	    goto error_return;
	  total += len + 1;
	}

      if ((total & (swap->debug_align - 1)) != 0)
	{
	  unsigned int i;
	  bfd_byte *s;

	  i = swap->debug_align - (total & (swap->debug_align - 1));
	  s = (bfd_byte *) bfd_malloc (i);
	  if (s == NULL && i != 0)
	    goto error_return;
	  memset ((PTR) s, 0, i);
	  if (bfd_write ((PTR) s, 1, i, abfd) != i)
	    {
	      free (s);
	      goto error_return;
	    }
	  free (s);
	}
    }

  /* The external strings and symbol are not converted over to using
     shuffles.  FIXME: They probably should be.  */
  if (bfd_write (debug->ssext, 1, debug->symbolic_header.issExtMax, abfd)
      != (bfd_size_type) debug->symbolic_header.issExtMax)
    goto error_return;
  if ((debug->symbolic_header.issExtMax & (swap->debug_align - 1)) != 0)
    {
      unsigned int i;
      bfd_byte *s;

      i = (swap->debug_align
	   - (debug->symbolic_header.issExtMax & (swap->debug_align - 1)));
      s = (bfd_byte *) bfd_malloc (i);
      if (s == NULL && i != 0)
	goto error_return;
      memset ((PTR) s, 0, i);
      if (bfd_write ((PTR) s, 1, i, abfd) != i)
	{
	  free (s);
	  goto error_return;
	}
      free (s);
    }

  if (! ecoff_write_shuffle (abfd, swap, ainfo->fdr, space)
      || ! ecoff_write_shuffle (abfd, swap, ainfo->rfd, space))
    goto error_return;

  BFD_ASSERT (debug->symbolic_header.cbExtOffset == 0
	      || (debug->symbolic_header.cbExtOffset
		  == (bfd_vma) bfd_tell (abfd)));

  if (bfd_write (debug->external_ext, swap->external_ext_size,
		 debug->symbolic_header.iextMax, abfd)
      != debug->symbolic_header.iextMax * swap->external_ext_size)
    goto error_return;

  if (space != NULL)
    free (space);
  return true;

 error_return:
  if (space != NULL)
    free (space);
  return false;
}

/* Handle the find_nearest_line function for both ECOFF and MIPS ELF
   files.  */

/* Compare FDR entries.  This is called via qsort.  */

static int
cmp_fdrtab_entry (leftp, rightp)
     const PTR leftp;
     const PTR rightp;
{
  const struct ecoff_fdrtab_entry *lp =
    (const struct ecoff_fdrtab_entry *) leftp;
  const struct ecoff_fdrtab_entry *rp =
    (const struct ecoff_fdrtab_entry *) rightp;

  if (lp->base_addr < rp->base_addr)
    return -1;
  if (lp->base_addr > rp->base_addr)
    return 1;
  return 0;
}

/* Each file descriptor (FDR) has a memory address, to simplify
   looking up an FDR by address, we build a table covering all FDRs
   that have a least one procedure descriptor in them.  The final
   table will be sorted by address so we can look it up via binary
   search.  */

static boolean
mk_fdrtab (abfd, debug_info, debug_swap, line_info)
     bfd *abfd;
     struct ecoff_debug_info * const debug_info;
     const struct ecoff_debug_swap * const debug_swap;
     struct ecoff_find_line *line_info;
{
  struct ecoff_fdrtab_entry *tab;
  FDR *fdr_ptr;
  FDR *fdr_start;
  FDR *fdr_end;
  boolean stabs;
  long len;

  fdr_start = debug_info->fdr;
  fdr_end = fdr_start + debug_info->symbolic_header.ifdMax;

  /* First, let's see how long the table needs to be: */
  for (len = 0, fdr_ptr = fdr_start; fdr_ptr < fdr_end; fdr_ptr++)
    {
      if (fdr_ptr->cpd == 0)	/* skip FDRs that have no PDRs */
	continue;
      ++len;
    }

  /* Now, create and fill in the table: */

  line_info->fdrtab = ((struct ecoff_fdrtab_entry*)
		       bfd_zalloc (abfd,
				   len * sizeof (struct ecoff_fdrtab_entry)));
  if (line_info->fdrtab == NULL)
    return false;
  line_info->fdrtab_len = len;

  tab = line_info->fdrtab;
  for (fdr_ptr = fdr_start; fdr_ptr < fdr_end; fdr_ptr++)
    {
      if (fdr_ptr->cpd == 0)
	continue;

      /* Check whether this file has stabs debugging information.  In
	 a file with stabs debugging information, the second local
	 symbol is named @stabs.  */
      stabs = false;
      if (fdr_ptr->csym >= 2)
	{
	  char *sym_ptr;
	  SYMR sym;

	  sym_ptr = ((char *) debug_info->external_sym
		     + (fdr_ptr->isymBase + 1)*debug_swap->external_sym_size);
	  (*debug_swap->swap_sym_in) (abfd, sym_ptr, &sym);
	  if (strcmp (debug_info->ss + fdr_ptr->issBase + sym.iss,
		      STABS_SYMBOL) == 0)
	    stabs = true;
	}

      if (!stabs)
	{
	  bfd_size_type external_pdr_size;
	  char *pdr_ptr;
	  PDR pdr;

	  external_pdr_size = debug_swap->external_pdr_size;

	  pdr_ptr = ((char *) debug_info->external_pdr
		     + fdr_ptr->ipdFirst * external_pdr_size);
	  (*debug_swap->swap_pdr_in) (abfd, (PTR) pdr_ptr, &pdr);
	  /* The address of the first PDR is the offset of that
	     procedure relative to the beginning of file FDR.  */
	  tab->base_addr = fdr_ptr->adr - pdr.adr;
	}
      else
	{
	  /* XXX I don't know about stabs, so this is a guess
	     (davidm@cs.arizona.edu): */
	  tab->base_addr = fdr_ptr->adr;
	}
      tab->fdr = fdr_ptr;
      ++tab;
    }

  /* Finally, the table is sorted in increasing memory-address order.
     The table is mostly sorted already, but there are cases (e.g.,
     static functions in include files), where this does not hold.
     Use "odump -PFv" to verify...  */
  qsort ((PTR) line_info->fdrtab, len,
	 sizeof (struct ecoff_fdrtab_entry), cmp_fdrtab_entry);

  return true;
}

/* Return index of first FDR that covers to OFFSET.  */

static long
fdrtab_lookup (line_info, offset)
     struct ecoff_find_line *line_info;
     bfd_vma offset;
{
  long low, high, len;
  long mid = -1;
  struct ecoff_fdrtab_entry *tab;

  len = line_info->fdrtab_len;
  if (len == 0)
    return -1;

  tab = line_info->fdrtab;
  for (low = 0, high = len - 1 ; low != high ;)
    {
      mid = (high + low) / 2;
      if (offset >= tab[mid].base_addr && offset < tab[mid + 1].base_addr)
	goto find_min;

      if (tab[mid].base_addr > offset)
	high = mid;
      else
	low = mid + 1;
    }
  ++mid;

  /* last entry is catch-all for all higher addresses: */
  if (offset < tab[mid].base_addr)
    return -1;

 find_min:

  while (mid > 0 && tab[mid - 1].base_addr == tab[mid].base_addr)
    --mid;

  return mid;
}

/* Look up a line given an address, storing the information in
   LINE_INFO->cache.  */

static boolean
lookup_line (abfd, debug_info, debug_swap, line_info)
     bfd *abfd;
     struct ecoff_debug_info * const debug_info;
     const struct ecoff_debug_swap * const debug_swap;
     struct ecoff_find_line *line_info;
{
  struct ecoff_fdrtab_entry *tab;
  bfd_vma offset;
  boolean stabs;
  FDR *fdr_ptr;
  int i;

  offset = line_info->cache.start;

  /* Build FDR table (sorted by object file's base-address) if we
     don't have it already.  */
  if (line_info->fdrtab == NULL
      && !mk_fdrtab (abfd, debug_info, debug_swap, line_info))
    return false;

  tab = line_info->fdrtab;

  /* find first FDR for address OFFSET */
  i = fdrtab_lookup (line_info, offset);
  if (i < 0)
    return false;		/* no FDR, no fun...  */
  fdr_ptr = tab[i].fdr;

  /* Check whether this file has stabs debugging information.  In a
     file with stabs debugging information, the second local symbol is
     named @stabs.  */
  stabs = false;
  if (fdr_ptr->csym >= 2)
    {
      char *sym_ptr;
      SYMR sym;

      sym_ptr = ((char *) debug_info->external_sym
		 + (fdr_ptr->isymBase + 1) * debug_swap->external_sym_size);
      (*debug_swap->swap_sym_in) (abfd, sym_ptr, &sym);
      if (strcmp (debug_info->ss + fdr_ptr->issBase + sym.iss,
		  STABS_SYMBOL) == 0)
	stabs = true;
    }

  if (!stabs)
    {
      bfd_size_type external_pdr_size;
      char *pdr_ptr;
      char *best_pdr = NULL;
      FDR *best_fdr;
      bfd_vma best_dist = ~0;
      PDR pdr;
      unsigned char *line_ptr;
      unsigned char *line_end;
      int lineno;
      /* This file uses ECOFF debugging information.  Each FDR has a
         list of procedure descriptors (PDR).  The address in the FDR
         is the absolute address of the first procedure.  The address
         in the first PDR gives the offset of that procedure relative
         to the object file's base-address.  The addresses in
         subsequent PDRs specify each procedure's address relative to
         the object file's base-address.  To make things more juicy,
         whenever the PROF bit in the PDR is set, the real entry point
         of the procedure may be 16 bytes below what would normally be
         the procedure's entry point.  Instead, DEC came up with a
         wicked scheme to create profiled libraries "on the fly":
         instead of shipping a regular and a profiled version of each
         library, they insert 16 bytes of unused space in front of
         each procedure and set the "prof" bit in the PDR to indicate
         that there is a gap there (this is done automagically by "as"
         when option "-pg" is specified).  Thus, normally, you link
         against such a library and, except for lots of 16 byte gaps
         between functions, things will behave as usual.  However,
         when invoking "ld" with option "-pg", it will fill those gaps
         with code that calls mcount().  It then moves the function's
         entry point down by 16 bytes, and out pops a binary that has
         all functions profiled.

         NOTE: Neither FDRs nor PDRs are strictly sorted in memory
               order.  For example, when including header-files that
               define functions, the FDRs follow behind the including
               file, even though their code may have been generated at
               a lower address.  File coff-alpha.c from libbfd
               illustrates this (use "odump -PFv" to look at a file's
               FDR/PDR).  Similarly, PDRs are sometimes out of order
               as well.  An example of this is OSF/1 v3.0 libc's
               malloc.c.  I'm not sure why this happens, but it could
               be due to optimizations that reorder a function's
               position within an object-file.

         Strategy:

         On the first call to this function, we build a table of FDRs
         that is sorted by the base-address of the object-file the FDR
         is referring to.  Notice that each object-file may contain
         code from multiple source files (e.g., due to code defined in
         include files).  Thus, for any given base-address, there may
         be multiple FDRs (but this case is, fortunately, uncommon).
         lookup(addr) guarantees to return the first FDR that applies
         to address ADDR.  Thus, after invoking lookup(), we have a
         list of FDRs that may contain the PDR for ADDR.  Next, we
         walk through the PDRs of these FDRs and locate the one that
         is closest to ADDR (i.e., for which the difference between
         ADDR and the PDR's entry point is positive and minimal).
         Once, the right FDR and PDR are located, we simply walk
         through the line-number table to lookup the line-number that
         best matches ADDR.  Obviously, things could be sped up by
         keeping a sorted list of PDRs instead of a sorted list of
         FDRs.  However, this would increase space requirements
         considerably, which is undesirable.  */
      external_pdr_size = debug_swap->external_pdr_size;

      /* Make offset relative to object file's start-address: */
      offset -= tab[i].base_addr;
      /* Search FDR list starting at tab[i] for the PDR that best matches
         OFFSET.  Normally, the FDR list is only one entry long.  */
      best_fdr = NULL;
      do
	{
	  bfd_vma dist, min_dist = 0;
	  char *pdr_hold;
	  char *pdr_end;

	  fdr_ptr = tab[i].fdr;

	  pdr_ptr = ((char *) debug_info->external_pdr
		     + fdr_ptr->ipdFirst * external_pdr_size);
	  pdr_end = pdr_ptr + fdr_ptr->cpd * external_pdr_size;
	  (*debug_swap->swap_pdr_in) (abfd, (PTR) pdr_ptr, &pdr);
	  /* Find PDR that is closest to OFFSET.  If pdr.prof is set,
	     the procedure entry-point *may* be 0x10 below pdr.adr.  We
	     simply pretend that pdr.prof *implies* a lower entry-point.
	     This is safe because it just means that may identify 4 NOPs
	     in front of the function as belonging to the function.  */
	  for (pdr_hold = NULL;
	       pdr_ptr < pdr_end;
	       (pdr_ptr += external_pdr_size,
		(*debug_swap->swap_pdr_in) (abfd, (PTR) pdr_ptr, &pdr)))
	    {
	      if (offset >= (pdr.adr - 0x10 * pdr.prof))
		{
		  dist = offset - (pdr.adr - 0x10 * pdr.prof);
		  if (!pdr_hold || dist < min_dist)
		    {
		      min_dist = dist;
		      pdr_hold = pdr_ptr;
		    }
		}
	    }

	  if (!best_pdr || min_dist < best_dist)
	    {
	      best_dist = min_dist;
	      best_fdr = fdr_ptr;
	      best_pdr = pdr_hold;
	    }
	  /* continue looping until base_addr of next entry is different: */
	}
      while (++i < line_info->fdrtab_len
	     && tab[i].base_addr == tab[i - 1].base_addr);

      if (!best_fdr || !best_pdr)
	return false;			/* shouldn't happen...  */

      /* phew, finally we got something that we can hold onto: */
      fdr_ptr = best_fdr;
      pdr_ptr = best_pdr;
      (*debug_swap->swap_pdr_in) (abfd, (PTR) pdr_ptr, &pdr);
      /* Now we can look for the actual line number.  The line numbers
         are stored in a very funky format, which I won't try to
         describe.  The search is bounded by the end of the FDRs line
         number entries.  */
      line_end = debug_info->line + fdr_ptr->cbLineOffset + fdr_ptr->cbLine;

      /* Make offset relative to procedure entry: */
      offset -= pdr.adr - 0x10 * pdr.prof;
      lineno = pdr.lnLow;
      line_ptr = debug_info->line + fdr_ptr->cbLineOffset + pdr.cbLineOffset;
      while (line_ptr < line_end)
	{
	  int delta;
	  unsigned int count;

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
	    {
	      line_info->cache.stop += count * 4 - offset;
	      break;
	    }
	  offset -= count * 4;
	}

      /* If fdr_ptr->rss is -1, then this file does not have full
         symbols, at least according to gdb/mipsread.c.  */
      if (fdr_ptr->rss == -1)
	{
	  line_info->cache.filename = NULL;
	  if (pdr.isym == -1)
	    line_info->cache.functionname = NULL;
	  else
	    {
	      EXTR proc_ext;

	      (*debug_swap->swap_ext_in)
		(abfd,
		 ((char *) debug_info->external_ext
		  + pdr.isym * debug_swap->external_ext_size),
		 &proc_ext);
	      line_info->cache.functionname = (debug_info->ssext
					       + proc_ext.asym.iss);
	    }
	}
      else
	{
	  SYMR proc_sym;

	  line_info->cache.filename = (debug_info->ss
				       + fdr_ptr->issBase
				       + fdr_ptr->rss);
	  (*debug_swap->swap_sym_in)
	    (abfd,
	     ((char *) debug_info->external_sym
	      + ((fdr_ptr->isymBase + pdr.isym)
		 * debug_swap->external_sym_size)),
	     &proc_sym);
	  line_info->cache.functionname = (debug_info->ss
					   + fdr_ptr->issBase
					   + proc_sym.iss);
	}
      if (lineno == ilineNil)
	lineno = 0;
      line_info->cache.line_num = lineno;
    }
  else
    {
      bfd_size_type external_sym_size;
      const char *directory_name;
      const char *main_file_name;
      const char *current_file_name;
      const char *function_name;
      const char *line_file_name;
      bfd_vma low_func_vma;
      bfd_vma low_line_vma;
      boolean past_line;
      boolean past_fn;
      char *sym_ptr, *sym_ptr_end;
      size_t len, funclen;
      char *buffer = NULL;

      /* This file uses stabs debugging information.  When gcc is not
	 optimizing, it will put the line number information before
	 the function name stabs entry.  When gcc is optimizing, it
	 will put the stabs entry for all the function first, followed
	 by the line number information.  (This appears to happen
	 because of the two output files used by the -mgpopt switch,
	 which is implied by -O).  This means that we must keep
	 looking through the symbols until we find both a line number
	 and a function name which are beyond the address we want.  */

      line_info->cache.filename = NULL;
      line_info->cache.functionname = NULL;
      line_info->cache.line_num = 0;

      directory_name = NULL;
      main_file_name = NULL;
      current_file_name = NULL;
      function_name = NULL;
      line_file_name = NULL;
      low_func_vma = 0;
      low_line_vma = 0;
      past_line = false;
      past_fn = false;

      external_sym_size = debug_swap->external_sym_size;

      sym_ptr = ((char *) debug_info->external_sym
		 + (fdr_ptr->isymBase + 2) * external_sym_size);
      sym_ptr_end = sym_ptr + (fdr_ptr->csym - 2) * external_sym_size;
      for (;
	   sym_ptr < sym_ptr_end && (! past_line || ! past_fn);
	   sym_ptr += external_sym_size)
	{
	  SYMR sym;

	  (*debug_swap->swap_sym_in) (abfd, sym_ptr, &sym);

	  if (ECOFF_IS_STAB (&sym))
	    {
	      switch (ECOFF_UNMARK_STAB (sym.index))
		{
		case N_SO:
		  main_file_name = current_file_name =
		    debug_info->ss + fdr_ptr->issBase + sym.iss;

		  /* Check the next symbol to see if it is also an
                     N_SO symbol.  */
		  if (sym_ptr + external_sym_size < sym_ptr_end)
		    {
		      SYMR nextsym;

		      (*debug_swap->swap_sym_in) (abfd,
						  sym_ptr + external_sym_size,
						  &nextsym);
		      if (ECOFF_IS_STAB (&nextsym)
			  && ECOFF_UNMARK_STAB (nextsym.index) == N_SO)
			{
 			  directory_name = current_file_name;
			  main_file_name = current_file_name =
			    debug_info->ss + fdr_ptr->issBase + nextsym.iss;
			  sym_ptr += external_sym_size;
			}
		    }
		  break;

		case N_SOL:
		  current_file_name =
		    debug_info->ss + fdr_ptr->issBase + sym.iss;
		  break;

		case N_FUN:
		  if (sym.value > offset)
		    past_fn = true;
		  else if (sym.value >= low_func_vma)
		    {
		      low_func_vma = sym.value;
		      function_name =
			debug_info->ss + fdr_ptr->issBase + sym.iss;
		    }
		  break;
		}
	    }
	  else if (sym.st == stLabel && sym.index != indexNil)
	    {
	      if (sym.value > offset)
		past_line = true;
	      else if (sym.value >= low_line_vma)
		{
		  low_line_vma = sym.value;
		  line_file_name = current_file_name;
		  line_info->cache.line_num = sym.index;
		}
	    }
	}

      if (line_info->cache.line_num != 0)
	main_file_name = line_file_name;

      /* We need to remove the stuff after the colon in the function
         name.  We also need to put the directory name and the file
         name together.  */
      if (function_name == NULL)
	len = funclen = 0;
      else
	len = funclen = strlen (function_name) + 1;

      if (main_file_name != NULL
	  && directory_name != NULL
	  && main_file_name[0] != '/')
	len += strlen (directory_name) + strlen (main_file_name) + 1;

      if (len != 0)
	{
	  if (line_info->find_buffer != NULL)
	    free (line_info->find_buffer);
	  buffer = (char *) bfd_malloc (len);
	  if (buffer == NULL)
	    return false;
	  line_info->find_buffer = buffer;
	}

      if (function_name != NULL)
	{
	  char *colon;

	  strcpy (buffer, function_name);
	  colon = strchr (buffer, ':');
	  if (colon != NULL)
	    *colon = '\0';
	  line_info->cache.functionname = buffer;
	}

      if (main_file_name != NULL)
	{
	  if (directory_name == NULL || main_file_name[0] == '/')
	    line_info->cache.filename = main_file_name;
	  else
	    {
	      sprintf (buffer + funclen, "%s%s", directory_name,
		       main_file_name);
	      line_info->cache.filename = buffer + funclen;
	    }
	}
    }

  return true;
}

/* Do the work of find_nearest_line.  */

boolean
_bfd_ecoff_locate_line (abfd, section, offset, debug_info, debug_swap,
			line_info, filename_ptr, functionname_ptr, retline_ptr)
     bfd *abfd;
     asection *section;
     bfd_vma offset;
     struct ecoff_debug_info * const debug_info;
     const struct ecoff_debug_swap * const debug_swap;
     struct ecoff_find_line *line_info;
     const char **filename_ptr;
     const char **functionname_ptr;
     unsigned int *retline_ptr;
{
  offset += section->vma;

  if (line_info->cache.sect == NULL
      || line_info->cache.sect != section
      || offset < line_info->cache.start
      || offset >= line_info->cache.stop)
    {
      line_info->cache.sect = section;
      line_info->cache.start = offset;
      line_info->cache.stop = offset;
      if (! lookup_line (abfd, debug_info, debug_swap, line_info))
	{
	  line_info->cache.sect = NULL;
	  return false;
	}
    }

  *filename_ptr = line_info->cache.filename;
  *functionname_ptr = line_info->cache.functionname;
  *retline_ptr = line_info->cache.line_num;

  return true;
}

/* These routines copy symbolic information into a memory buffer.

   FIXME: The whole point of the shuffle code is to avoid storing
   everything in memory, since the linker is such a memory hog.  This
   code makes that effort useless.  It is only called by the MIPS ELF
   code when generating a shared library, so it is not that big a
   deal, but it should be fixed eventually.  */

/* Collect a shuffle into a memory buffer.  */

static boolean ecoff_collect_shuffle PARAMS ((struct shuffle *, bfd_byte *));

static boolean
ecoff_collect_shuffle (l, buff)
     struct shuffle *l;
     bfd_byte *buff;
{
  unsigned long total;

  total = 0;
  for (; l != (struct shuffle *) NULL; l = l->next)
    {
      if (! l->filep)
	memcpy (buff, l->u.memory, l->size);
      else
	{
	  if (bfd_seek (l->u.file.input_bfd, l->u.file.offset, SEEK_SET) != 0
	      || bfd_read (buff, 1, l->size, l->u.file.input_bfd) != l->size)
	    return false;
	}
      total += l->size;
      buff += l->size;
    }

  return true;
}

/* Copy PDR information into a memory buffer.  */

boolean
_bfd_ecoff_get_accumulated_pdr (handle, buff)
     PTR handle;
     bfd_byte *buff;
{
  struct accumulate *ainfo = (struct accumulate *) handle;

  return ecoff_collect_shuffle (ainfo->pdr, buff);
}

/* Copy symbol information into a memory buffer.  */

boolean
_bfd_ecoff_get_accumulated_sym (handle, buff)
     PTR handle;
     bfd_byte *buff;
{
  struct accumulate *ainfo = (struct accumulate *) handle;

  return ecoff_collect_shuffle (ainfo->sym, buff);
}

/* Copy the string table into a memory buffer.  */

boolean
_bfd_ecoff_get_accumulated_ss (handle, buff)
     PTR handle;
     bfd_byte *buff;
{
  struct accumulate *ainfo = (struct accumulate *) handle;
  struct string_hash_entry *sh;
  unsigned long total;

  /* The string table is written out from the hash table if this is a
     final link.  */
  BFD_ASSERT (ainfo->ss == (struct shuffle *) NULL);
  *buff++ = '\0';
  total = 1;
  BFD_ASSERT (ainfo->ss_hash == NULL || ainfo->ss_hash->val == 1);
  for (sh = ainfo->ss_hash;
       sh != (struct string_hash_entry *) NULL;
       sh = sh->next)
    {
      size_t len;

      len = strlen (sh->root.string);
      memcpy (buff, (PTR) sh->root.string, len + 1);
      total += len + 1;
      buff += len + 1;
    }

  return true;
}
