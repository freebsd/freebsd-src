/* HP PA-RISC SOM object file format:  definitions internal to BFD.
   Copyright (C) 1990, 91, 92, 93, 94 Free Software Foundation, Inc.

   Contributed by the Center for Software Science at the
   University of Utah (pa-gdb-bugs@cs.utah.edu).

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

#ifndef _SOM_H
#define _SOM_H

#include "../bfd/sysdep.h"
#include "libhppa.h"

#include <a.out.h>
#include <lst.h>
#include <ar.h>

/* The SOM BFD backend doesn't currently use anything from these
   two include files, but it's likely to need them in the future.  */
#ifdef R_DLT_REL
#include <shl.h>
#include <dl.h>
#endif

#if defined(HOST_HPPABSD) || defined (HOST_HPPAOSF)
/* BSD uses a completely different scheme for object file identification.
   so for now, define _PA_RISC_ID to accept any random value for a model
   number.  */
#undef _PA_RISC_ID
#define _PA_RISC_ID(__m_num) 1
#endif /* HOST_HPPABSD */

#define FILE_HDR_SIZE sizeof(struct header)
#define AUX_HDR_SIZE sizeof(struct som_exec_auxhdr)

typedef struct som_symbol
  {
    asymbol symbol;
    unsigned int som_type;

    /* Structured like the ELF tc_data union.  Allows more code sharing
       in GAS this way.  */
    union
      {
	unsigned int hppa_arg_reloc;
	PTR any;
      }
    tc_data;

    /* Index of this symbol in the symbol table.  Only used when
       building relocation streams for incomplete objects.  */
    int index;

    /* How many times this symbol is used in a relocation.  By sorting
       the symbols from most used to least used we can significantly
       reduce the size of the relocation stream for incomplete objects.  */
    int reloc_count;

    /* During object file writing, the offset of the name of this symbol
       in the SOM string table. */
    int stringtab_offset;
  }
som_symbol_type;

/* A structure containing all the magic information stored in a BFD's
   private data which needs to be copied during an objcopy/strip run.  */
struct som_exec_data
  {
    /* Sort-of a magic number.  BSD uses it to distinguish between
       native executables and hpux executables.  */
    short system_id;

    /* Magic exec flags.  These control things like whether or not
       null pointer dereferencing is allowed and the like.  */
    long exec_flags;

    /* Add more stuff here as needed.  Good examples of information
       we might want to pass would be presumed_dp, entry_* and maybe
       others from the file header.  */
  };

struct somdata
  {
    /* All the magic information about an executable which lives
       in the private BFD structure and needs to be copied from
       the input bfd to the output bfd during a objcopy/strip.  */
    struct som_exec_data *exec_data;

    /* These three fields are only used when writing files and are
       generated from scratch.  They need not be copied for objcopy
       or strip to work.  */
    struct header *file_hdr;
    struct copyright_aux_hdr *copyright_aux_hdr;
    struct user_string_aux_hdr *version_aux_hdr;
    struct som_exec_auxhdr *exec_hdr;

    /* Pointers to a saved copy of the symbol and string tables.  These
       need not be copied for objcopy or strip to work.  */
    som_symbol_type *symtab;
    char *stringtab;
    asymbol **sorted_syms;

    /* We remember these offsets so that after check_file_format, we have
       no dependencies on the particular format of the exec_hdr.
       These offsets need not be copied for objcopy or strip to work.  */

    file_ptr sym_filepos;
    file_ptr str_filepos;
    file_ptr reloc_filepos;
    unsigned stringtab_size;
  };

struct som_data_struct
  {
    struct somdata a;
  };

/* Substructure of som_section_data_struct used to hold information
   which can't be represented by the generic BFD section structure,
   but which must be copied during objcopy or strip.  */
struct som_copyable_section_data_struct
  {
    /* Various fields in space and subspace headers that we need
       to pass around.  */
    unsigned int sort_key : 8;
    unsigned int access_control_bits : 7;
    unsigned int is_defined : 1;
    unsigned int is_private : 1;
    unsigned int quadrant : 2;

    /* For subspaces, this points to the section which represents the
       space in which the subspace is contained.  For spaces it points
       back to the section for this space.  */
    asection *container;

    /* The user-specified space number.  It is wrong to use this as
       an index since duplicates and holes are allowed.  */
    int space_number;

    /* Add more stuff here as needed.  Good examples of information
       we might want to pass would be initialization pointers, 
       and the many subspace flags we do not represent yet.  */
  };

/* Used to keep extra SOM specific information for a given section.

   reloc_size holds the size of the relocation stream, note this
   is very different from the number of relocations as SOM relocations
   are variable length. 

   reloc_stream is the actual stream of relocation entries.  */

struct som_section_data_struct
  {
    struct som_copyable_section_data_struct *copy_data;
    unsigned int reloc_size;
    char *reloc_stream;
    struct space_dictionary_record *space_dict;
    struct subspace_dictionary_record *subspace_dict;
  };

#define somdata(bfd)			((bfd)->tdata.som_data->a)
#define obj_som_exec_data(bfd)		(somdata(bfd).exec_data)
#define obj_som_file_hdr(bfd)		(somdata(bfd).file_hdr)
#define obj_som_exec_hdr(bfd)		(somdata(bfd).exec_hdr)
#define obj_som_copyright_hdr(bfd)	(somdata(bfd).copyright_aux_hdr)
#define obj_som_version_hdr(bfd)	(somdata(bfd).version_aux_hdr)
#define obj_som_symtab(bfd)		(somdata(bfd).symtab)
#define obj_som_stringtab(bfd)		(somdata(bfd).stringtab)
#define obj_som_sym_filepos(bfd)	(somdata(bfd).sym_filepos)
#define obj_som_str_filepos(bfd)	(somdata(bfd).str_filepos)
#define obj_som_stringtab_size(bfd)	(somdata(bfd).stringtab_size)
#define obj_som_reloc_filepos(bfd)	(somdata(bfd).reloc_filepos)
#define obj_som_sorted_syms(bfd)	(somdata(bfd).sorted_syms)
#define som_section_data(sec) \
  ((struct som_section_data_struct *)sec->used_by_bfd)
#define som_symbol_data(symbol)		((som_symbol_type *) symbol)


/* Defines groups of basic relocations.  FIXME:  These should
   be the only basic relocations created by GAS.  The rest
   should be internal to the BFD backend.

   The idea is both SOM and ELF define these basic relocation
   types so they map into a SOM or ELF specific reloation as
   appropriate.  This allows GAS to share much more code 
   between the two object formats.  */

#define R_HPPA_NONE			R_NO_RELOCATION
#define	R_HPPA				R_CODE_ONE_SYMBOL
#define	R_HPPA_PCREL_CALL		R_PCREL_CALL
#define R_HPPA_ABS_CALL			R_ABS_CALL
#define	R_HPPA_GOTOFF			R_DP_RELATIVE
#define R_HPPA_ENTRY			R_ENTRY
#define R_HPPA_EXIT			R_EXIT
#define R_HPPA_COMPLEX			R_COMP1
#define R_HPPA_BEGIN_BRTAB		R_BEGIN_BRTAB
#define R_HPPA_END_BRTAB		R_END_BRTAB

/* Exported functions, mostly for use by GAS.  */
boolean bfd_som_set_section_attributes PARAMS ((asection *, int, int,
						unsigned int, int));
boolean bfd_som_set_subsection_attributes PARAMS ((asection *, asection *,
						   int, unsigned int, int));
void bfd_som_set_symbol_type PARAMS ((asymbol *, unsigned int));
boolean bfd_som_attach_aux_hdr PARAMS ((bfd *, int, char *));
int ** hppa_som_gen_reloc_type
  PARAMS ((bfd *, int, int, enum hppa_reloc_field_selector_type_alt, int));
#endif /* _SOM_H */
