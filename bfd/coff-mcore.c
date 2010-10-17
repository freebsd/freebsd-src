/* BFD back-end for Motorola MCore COFF/PE
   Copyright 1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

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
Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "coff/mcore.h"
#include "coff/internal.h"
#include "coff/pe.h"
#include "libcoff.h"

#ifdef BADMAG
#undef BADMAG
#endif
#define BADMAG(x) MCOREBADMAG(x)

#ifndef NUM_ELEM
#define NUM_ELEM(A) (sizeof (A) / sizeof (A)[0])
#endif

/* This file is compiled more than once, but we only compile the
   final_link routine once.  */
extern bfd_boolean mcore_bfd_coff_final_link
  PARAMS ((bfd *, struct bfd_link_info *));
#if 0
static struct bfd_link_hash_table *coff_mcore_link_hash_table_create
  PARAMS ((bfd *));
#endif
static bfd_reloc_status_type mcore_coff_unsupported_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_boolean coff_mcore_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   struct internal_reloc *, struct internal_syment *, asection **));
static reloc_howto_type *mcore_coff_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static reloc_howto_type *coff_mcore_rtype_to_howto
  PARAMS ((bfd *, asection *, struct internal_reloc *,
	   struct coff_link_hash_entry *, struct internal_syment *,
	   bfd_vma *));
static void mcore_emit_base_file_entry
  PARAMS ((struct bfd_link_info *, bfd *, asection *, bfd_vma));
static bfd_boolean in_reloc_p PARAMS ((bfd *, reloc_howto_type *));

/* The NT loader points the toc register to &toc + 32768, in order to
   use the complete range of a 16-bit displacement. We have to adjust
   for this when we fix up loads displaced off the toc reg.  */
#define TOC_LOAD_ADJUSTMENT (-32768)
#define TOC_SECTION_NAME ".private.toc"

/* The main body of code is in coffcode.h.  */
#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER 2

/* In case we're on a 32-bit machine, construct a 64-bit "-1" value
   from smaller values.  Start with zero, widen, *then* decrement.  */
#define MINUS_ONE	(((bfd_vma)0) - 1)

static reloc_howto_type mcore_coff_howto_table[] =
{
  /* Unused: */
  HOWTO (IMAGE_REL_MCORE_ABSOLUTE,/* type */
	 0,	                 /* rightshift */
	 0,	                 /* size (0 = byte, 1 = short, 2 = long) */
	 0,	                 /* bitsize */
	 FALSE,	                 /* pc_relative */
	 0,	                 /* bitpos */
	 complain_overflow_dont, /* dont complain_on_overflow */
	 NULL,		         /* special_function */
	 "ABSOLUTE",             /* name */
	 FALSE,	                 /* partial_inplace */
	 0x00,	 	         /* src_mask */
	 0x00,        		 /* dst_mask */
	 FALSE),                 /* pcrel_offset */

  HOWTO (IMAGE_REL_MCORE_ADDR32,/* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 NULL,		        /* special_function */
	 "ADDR32",              /* name */
	 TRUE,	                /* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,            /* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* 8 bits + 2 zero bits; jmpi/jsri/lrw instructions.
     Should not appear in object files.  */
  HOWTO (IMAGE_REL_MCORE_PCREL_IMM8BY4,	/* type */
	 2,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 mcore_coff_unsupported_reloc, /* special_function */
	 "IMM8BY4",             /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* bsr/bt/bf/br instructions; 11 bits + 1 zero bit
     Span 2k instructions == 4k bytes.
     Only useful pieces at the relocated address are the opcode (5 bits) */
  HOWTO (IMAGE_REL_MCORE_PCREL_IMM11BY2,/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 11,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 NULL,	                /* special_function */
	 "IMM11BY2",            /* name */
	 FALSE,			/* partial_inplace */
	 0x0,			/* src_mask */
	 0x7ff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 4 bits + 1 zero bit; 'loopt' instruction only; unsupported.  */
  HOWTO (IMAGE_REL_MCORE_PCREL_IMM4BY2,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 4,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 mcore_coff_unsupported_reloc, /* special_function */
	 "IMM4BY2",              /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 32-bit pc-relative. Eventually this will help support PIC code.  */
  HOWTO (IMAGE_REL_MCORE_PCREL_32,/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 NULL,	                /* special_function */
	 "PCREL_32",	        /* name */
	 FALSE,			/* partial_inplace */
	 0x0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* Like PCREL_IMM11BY2, this relocation indicates that there is a
     'jsri' at the specified address. There is a separate relocation
     entry for the literal pool entry that it references, but we
     might be able to change the jsri to a bsr if the target turns out
     to be close enough [even though we won't reclaim the literal pool
     entry, we'll get some runtime efficiency back]. Note that this
     is a relocation that we are allowed to safely ignore.  */
  HOWTO (IMAGE_REL_MCORE_PCREL_JSR_IMM11BY2,/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 11,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 NULL,	                /* special_function */
	 "JSR_IMM11BY2",        /* name */
	 FALSE,			/* partial_inplace */
	 0x0,			/* src_mask */
	 0x7ff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (IMAGE_REL_MCORE_RVA,   /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 NULL,                  /* special_function */
	 "MCORE_RVA",           /* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE)			/* pcrel_offset */
};

/* Extend the coff_link_hash_table structure with a few M*Core specific fields.
   This allows us to store global data here without actually creating any
   global variables, which is a no-no in the BFD world.  */
typedef struct coff_mcore_link_hash_table
{
  /* The original coff_link_hash_table structure.  MUST be first field.  */
  struct coff_link_hash_table	root;

  bfd *                         bfd_of_toc_owner;
  long int                      global_toc_size;
  long int                      import_table_size;
  long int                      first_thunk_address;
  long int                      thunk_size;
}
mcore_hash_table;

/* Get the MCore coff linker hash table from a link_info structure.  */
#define coff_mcore_hash_table(info) \
  ((mcore_hash_table *) ((info)->hash))

#if 0
/* Create an MCore coff linker hash table.  */

static struct bfd_link_hash_table *
coff_mcore_link_hash_table_create (abfd)
     bfd * abfd;
{
  mcore_hash_table * ret;

  ret = (mcore_hash_table *) bfd_malloc ((bfd_size_type) sizeof (* ret));
  if (ret == (mcore_hash_table *) NULL)
    return NULL;

  if (! _bfd_coff_link_hash_table_init
      (& ret->root, abfd, _bfd_coff_link_hash_newfunc))
    {
      free (ret);
      return (struct bfd_link_hash_table *) NULL;
    }

  ret->bfd_of_toc_owner = NULL;
  ret->global_toc_size  = 0;
  ret->import_table_size = 0;
  ret->first_thunk_address = 0;
  ret->thunk_size = 0;

  return & ret->root.root;
}
#endif

/* Add an entry to the base file.  */

static void
mcore_emit_base_file_entry (info, output_bfd, input_section, reloc_offset)
      struct bfd_link_info * info;
      bfd *                  output_bfd;
      asection *             input_section;
      bfd_vma                reloc_offset;
{
  bfd_vma addr = reloc_offset
                 - input_section->vma
                 + input_section->output_offset
                 + input_section->output_section->vma;

  if (coff_data (output_bfd)->pe)
     addr -= pe_data (output_bfd)->pe_opthdr.ImageBase;

  fwrite (&addr, 1, sizeof (addr), (FILE *) info->base_file);
}

static bfd_reloc_status_type
mcore_coff_unsupported_reloc (abfd, reloc_entry, symbol, data, input_section,
			   output_bfd, error_message)
     bfd * abfd;
     arelent * reloc_entry;
     asymbol * symbol ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection * input_section ATTRIBUTE_UNUSED;
     bfd * output_bfd ATTRIBUTE_UNUSED;
     char ** error_message ATTRIBUTE_UNUSED;
{
  BFD_ASSERT (reloc_entry->howto != (reloc_howto_type *)0);

  _bfd_error_handler (_("%s: Relocation %s (%d) is not currently supported.\n"),
		      bfd_archive_filename (abfd),
		      reloc_entry->howto->name,
		      reloc_entry->howto->type);

  return bfd_reloc_notsupported;
}

/* A cheesy little macro to make the code a little more readable.  */
#define HOW2MAP(bfd_rtype, mcore_rtype)  \
 case bfd_rtype: return & mcore_coff_howto_table [mcore_rtype]

static reloc_howto_type *
mcore_coff_reloc_type_lookup (abfd, code)
     bfd * abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
      HOW2MAP (BFD_RELOC_32,                       IMAGE_REL_MCORE_ADDR32);
      HOW2MAP (BFD_RELOC_MCORE_PCREL_IMM8BY4,      IMAGE_REL_MCORE_PCREL_IMM8BY4);
      HOW2MAP (BFD_RELOC_MCORE_PCREL_IMM11BY2,     IMAGE_REL_MCORE_PCREL_IMM11BY2);
      HOW2MAP (BFD_RELOC_MCORE_PCREL_IMM4BY2,      IMAGE_REL_MCORE_PCREL_IMM4BY2);
      HOW2MAP (BFD_RELOC_32_PCREL,                 IMAGE_REL_MCORE_PCREL_32);
      HOW2MAP (BFD_RELOC_MCORE_PCREL_JSR_IMM11BY2, IMAGE_REL_MCORE_PCREL_JSR_IMM11BY2);
      HOW2MAP (BFD_RELOC_RVA,                      IMAGE_REL_MCORE_RVA);
   default:
      return NULL;
    }
  /*NOTREACHED*/
}

#undef HOW2MAP

#define RTYPE2HOWTO(cache_ptr, dst) \
  (cache_ptr)->howto = mcore_coff_howto_table + (dst)->r_type;

static reloc_howto_type *
coff_mcore_rtype_to_howto (abfd, sec, rel, h, sym, addendp)
     bfd * abfd ATTRIBUTE_UNUSED;
     asection * sec;
     struct internal_reloc * rel;
     struct coff_link_hash_entry * h ATTRIBUTE_UNUSED;
     struct internal_syment * sym;
     bfd_vma * addendp;
{
  reloc_howto_type * howto;

  if (rel->r_type >= NUM_ELEM (mcore_coff_howto_table))
    return NULL;

  howto = mcore_coff_howto_table + rel->r_type;

  if (rel->r_type == IMAGE_REL_MCORE_RVA)
    * addendp -= pe_data (sec->output_section->owner)->pe_opthdr.ImageBase;

  else if (howto->pc_relative)
    {
      * addendp = sec->vma - 2; /* XXX guess - is this right ? */

      /* If the symbol is defined, then the generic code is going to
         add back the symbol value in order to cancel out an
         adjustment it made to the addend.  However, we set the addend
         to 0 at the start of this function.  We need to adjust here,
         to avoid the adjustment the generic code will make.  FIXME:
         This is getting a bit hackish.  */
      if (sym != NULL && sym->n_scnum != 0)
	* addendp -= sym->n_value;
    }
  else
    * addendp = 0;

  return howto;
}

/* Return TRUE if this relocation should appear in the output .reloc section.
   This function is referenced in pe_mkobject in peicode.h.  */

static bfd_boolean
in_reloc_p (abfd, howto)
     bfd * abfd ATTRIBUTE_UNUSED;
     reloc_howto_type * howto;
{
  return ! howto->pc_relative && howto->type != IMAGE_REL_MCORE_RVA;
}

/* The reloc processing routine for the optimized COFF linker.  */
static bfd_boolean
coff_mcore_relocate_section (output_bfd, info, input_bfd, input_section,
			   contents, relocs, syms, sections)
     bfd * output_bfd;
     struct bfd_link_info * info;
     bfd * input_bfd;
     asection * input_section;
     bfd_byte * contents;
     struct internal_reloc * relocs;
     struct internal_syment * syms;
     asection ** sections;
{
  struct internal_reloc * rel;
  struct internal_reloc * relend;
  bfd_boolean hihalf;
  bfd_vma hihalf_val;

  /* If we are performing a relocatable link, we don't need to do a
     thing.  The caller will take care of adjusting the reloc
     addresses and symbol indices.  */
  if (info->relocatable)
    return TRUE;

  /* Check if we have the same endianess */
  if (   input_bfd->xvec->byteorder != output_bfd->xvec->byteorder
      && output_bfd->xvec->byteorder != BFD_ENDIAN_UNKNOWN)
    {
      (*_bfd_error_handler)
	(_("%s: compiled for a %s system and target is %s.\n"),
	 bfd_archive_filename (input_bfd),
         bfd_big_endian (input_bfd) ? _("big endian") : _("little endian"),
         bfd_big_endian (output_bfd) ? _("big endian") : _("little endian"));

      bfd_set_error (bfd_error_wrong_format);
      return FALSE;
    }

  hihalf = FALSE;
  hihalf_val = 0;

  rel = relocs;
  relend = rel + input_section->reloc_count;

  for (; rel < relend; rel++)
    {
      long                           symndx;
      struct internal_syment *       sym;
      bfd_vma                        val;
      bfd_vma                        addend;
      bfd_reloc_status_type          rstat;
      bfd_byte *                     loc;
      unsigned short                 r_type = rel->r_type;
      reloc_howto_type *             howto = NULL;
      struct coff_link_hash_entry *  h;
      const char *                   my_name;

      symndx = rel->r_symndx;
      loc = contents + rel->r_vaddr - input_section->vma;

      if (symndx == -1)
	{
	  h = NULL;
	  sym = NULL;
	}
      else
	{
	  h = obj_coff_sym_hashes (input_bfd)[symndx];
	  sym = syms + symndx;
	}

      addend = 0;

      /* Get the howto and initialise the addend.  */
      howto = bfd_coff_rtype_to_howto (input_bfd, input_section, rel, h,
				       sym, & addend);
      if (howto == NULL)
	return FALSE;

      val = 0;

      if (h == NULL)
	{
	  if (symndx == -1)
	    my_name = "*ABS*";
	  else
	    {
	      asection * sec = sections[symndx];

	      val = (sym->n_value
		     + sec->output_section->vma
		     + sec->output_offset);

	      if (sym == NULL)
		my_name = "*unknown*";
	      else if (   sym->_n._n_n._n_zeroes == 0
		       && sym->_n._n_n._n_offset != 0)
		my_name = obj_coff_strings (input_bfd) + sym->_n._n_n._n_offset;
	      else
		{
		  static char buf [SYMNMLEN + 1];

		  strncpy (buf, sym->_n._n_name, SYMNMLEN);
		  buf[SYMNMLEN] = '\0';
		  my_name = buf;
		}
	    }
	}
      else
	{
	  if (   h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      asection * sec = h->root.u.def.section;

	      val = (h->root.u.def.value
		     + sec->output_section->vma
		     + sec->output_offset);
	    }
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd, input_section,
		      rel->r_vaddr - input_section->vma, TRUE)))
		return FALSE;
	    }

	  my_name = h->root.root.string;
	}

      rstat = bfd_reloc_ok;

      /* Each case must do its own relocation, setting rstat appropriately.  */
      switch (r_type)
	{
	default:
	  _bfd_error_handler (_("%s: unsupported relocation type 0x%02x"),
			      bfd_archive_filename (input_bfd), r_type);
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;

	case IMAGE_REL_MCORE_ABSOLUTE:
	  fprintf (stderr,
		   _("Warning: unsupported reloc %s <file %s, section %s>\n"),
		   howto->name,
		   bfd_archive_filename (input_bfd),
		   input_section->name);

	  fprintf (stderr,"sym %ld (%s), r_vaddr %ld (%lx)\n",
		   rel->r_symndx, my_name, (long) rel->r_vaddr,
		   (unsigned long) rel->r_vaddr);
	  break;

	case IMAGE_REL_MCORE_PCREL_IMM8BY4:
	case IMAGE_REL_MCORE_PCREL_IMM11BY2:
	case IMAGE_REL_MCORE_PCREL_IMM4BY2:
	case IMAGE_REL_MCORE_PCREL_32:
	case IMAGE_REL_MCORE_PCREL_JSR_IMM11BY2:
	case IMAGE_REL_MCORE_ADDR32:
	  /* XXX fixme - shouldn't this be like the code for the RVA reloc ? */
	  rstat = _bfd_relocate_contents (howto, input_bfd, val, loc);
	  break;

	case IMAGE_REL_MCORE_RVA:
	  rstat = _bfd_final_link_relocate
	    (howto, input_bfd,
	     input_section, contents, rel->r_vaddr - input_section->vma,
	     val, addend);
	  break;
	}

      if (info->base_file)
	{
	  /* Emit a reloc if the backend thinks it needs it.  */
	  if (sym && pe_data (output_bfd)->in_reloc_p (output_bfd, howto))
            mcore_emit_base_file_entry (info, output_bfd, input_section, rel->r_vaddr);
	}

      switch (rstat)
	{
	default:
	  abort ();

	case bfd_reloc_ok:
	  break;

	case bfd_reloc_overflow:
	  if (! ((*info->callbacks->reloc_overflow)
		 (info, my_name, howto->name,
		  (bfd_vma) 0, input_bfd,
		  input_section, rel->r_vaddr - input_section->vma)))
	    return FALSE;
	}
    }

  return TRUE;
}

/* Tailor coffcode.h -- macro heaven.  */

/* We use the special COFF backend linker, with our own special touch.  */

#define coff_bfd_reloc_type_lookup   mcore_coff_reloc_type_lookup
#define coff_relocate_section        coff_mcore_relocate_section
#define coff_rtype_to_howto          coff_mcore_rtype_to_howto

#define SELECT_RELOC(internal, howto) {internal.r_type = howto->type;}

/* Make sure that the 'r_offset' field is copied properly
   so that identical binaries will compare the same.  */
#define SWAP_IN_RELOC_OFFSET         H_GET_32
#define SWAP_OUT_RELOC_OFFSET        H_PUT_32

#define COFF_PAGE_SIZE               0x1000

#include "coffcode.h"

/* Forward declaration to initialise alternative_target field.  */
extern const bfd_target TARGET_LITTLE_SYM;

/* The transfer vectors that lead the outside world to all of the above.  */
CREATE_BIG_COFF_TARGET_VEC (TARGET_BIG_SYM, TARGET_BIG_NAME, D_PAGED,
			    (SEC_CODE | SEC_DATA | SEC_DEBUGGING | SEC_READONLY | SEC_LINK_ONCE | SEC_LINK_DUPLICATES),
			    0, & TARGET_LITTLE_SYM, COFF_SWAP_TABLE)
CREATE_LITTLE_COFF_TARGET_VEC (TARGET_LITTLE_SYM, TARGET_LITTLE_NAME, D_PAGED,
			       (SEC_CODE | SEC_DATA | SEC_DEBUGGING | SEC_READONLY | SEC_LINK_ONCE | SEC_LINK_DUPLICATES),
			       0, & TARGET_BIG_SYM, COFF_SWAP_TABLE)
