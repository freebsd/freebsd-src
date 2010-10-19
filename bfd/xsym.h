/* xSYM symbol-file support for BFD.
   Copyright 1999, 2000, 2001, 2002, 2003, 2005
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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "bfd.h"
#include <stdio.h>

#ifndef __xSYM_H__
#define __xSYM_H__

#define BFD_SYM_VERSION_STR_3_1 	"\013Version 3.1"
#define BFD_SYM_VERSION_STR_3_2 	"\013Version 3.2"
#define BFD_SYM_VERSION_STR_3_3 	"\013Version 3.3"
#define BFD_SYM_VERSION_STR_3_4 	"\013Version 3.4"
#define BFD_SYM_VERSION_STR_3_5 	"\013Version 3.5"
#define BFD_SYM_END_OF_LIST_3_2		0xffff
#define BFD_SYM_END_OF_LIST_3_4		0xffffffff
#define BFD_SYM_END_OF_LIST 		BFD_SYM_END_OF_LIST_3_4
#define BFD_SYM_FILE_NAME_INDEX_3_2	0xfffe
#define BFD_SYM_FILE_NAME_INDEX_3_4	0xfffffffe
#define BFD_SYM_FILE_NAME_INDEX		BFD_SYM_FILE_NAME_INDEX_3_4
#define BFD_SYM_SOURCE_FILE_CHANGE_3_2	0xfffe
#define BFD_SYM_SOURCE_FILE_CHANGE_3_4	0xfffffffe
#define BFD_SYM_SOURCE_FILE_CHANGE	BFD_SYM_SOURCE_FILE_CHANGE_3_4
#define BFD_SYM_MAXIMUM_LEGAL_INDEX_3_2 0xfffd
#define BFD_SYM_MAXIMUM_LEGAL_INDEX_3_4 0xfffffffd
#define BFD_SYM_MAXIMUM_LEGAL_INDEX	BFD_SYM_MAXIMUM_LEGAL_INDEX_3_4

enum bfd_sym_storage_class
{
  BFD_SYM_STORAGE_CLASS_REGISTER = 0,
  BFD_SYM_STORAGE_CLASS_GLOBAL = 1,
  BFD_SYM_STORAGE_CLASS_FRAME_RELATIVE = 2,
  BFD_SYM_STORAGE_CLASS_STACK_RELATIVE = 3,
  BFD_SYM_STORAGE_CLASS_ABSOLUTE = 4,
  BFD_SYM_STORAGE_CLASS_CONSTANT = 5,
  BFD_SYM_STORAGE_CLASS_BIGCONSTANT = 6,
  BFD_SYM_STORAGE_CLASS_RESOURCE = 99
};
typedef enum bfd_sym_storage_class bfd_sym_storage_class;

enum bfd_sym_storage_kind
{
  BFD_SYM_STORAGE_KIND_LOCAL = 0,
  BFD_SYM_STORAGE_KIND_VALUE = 1,
  BFD_SYM_STORAGE_KIND_REFERENCE = 2,
  BFD_SYM_STORAGE_KIND_WITH = 3
};
typedef enum bfd_sym_storage_kind bfd_sym_storage_kind;

enum bfd_sym_version
{
  BFD_SYM_VERSION_3_1,
  BFD_SYM_VERSION_3_2,
  BFD_SYM_VERSION_3_3,
  BFD_SYM_VERSION_3_4,
  BFD_SYM_VERSION_3_5
};
typedef enum bfd_sym_version bfd_sym_version;

enum bfd_sym_module_kind
{
  BFD_SYM_MODULE_KIND_NONE = 0,
  BFD_SYM_MODULE_KIND_PROGRAM = 1,
  BFD_SYM_MODULE_KIND_UNIT = 2,
  BFD_SYM_MODULE_KIND_PROCEDURE = 3,
  BFD_SYM_MODULE_KIND_FUNCTION = 4,
  BFD_SYM_MODULE_KIND_DATA = 5,
  BFD_SYM_MODULE_KIND_BLOCK = 6
};
typedef enum bfd_sym_module_kind bfd_sym_module_kind;

enum bfd_sym_symbol_scope
{
  BFD_SYM_SYMBOL_SCOPE_LOCAL = 0,  /* Object is seen only inside current scope.  */
  BFD_SYM_SYMBOL_SCOPE_GLOBAL = 1  /* Object has global scope.  */
};
typedef enum bfd_sym_symbol_scope bfd_sym_symbol_scope;

struct bfd_sym_file_reference
{
  unsigned long fref_frte_index; /* File reference table index.  */
  unsigned long fref_offset;     /* Absolute offset into source file.  */
};
typedef struct bfd_sym_file_reference bfd_sym_file_reference;

/* NAME TABLE (NTE).  */

/* RESOURCES TABLE (RTE)

   All code and data is *defined* to reside in a resource.  Even A5
   relative data is defined to reside in a dummy resource of ResType
   'gbld'.  Code always resides in a resource.  Because a code/data
   is built of many modules, when walking through a resource we must
   point back to the modules in the order they were defined.  This is
   done by requiring the entries in the Modules Entry table to be
   ordered by resource/resource-number and by the location in that
   resource.  Hence, the resource table entry points to the first
   module making up that resource.  All modules table entries following
   that first one with the same restype/resnum are contiguous and offset
   from that first entry.  */

struct bfd_sym_resources_table_entry
{
  unsigned char rte_res_type[4];  /* Resource Type.  */
  unsigned short rte_res_number;  /* Resource Number.  */
  unsigned long rte_nte_index;    /* Name of the resource.  */
  unsigned long rte_mte_first;    /* Index of first module in the resource.  */
  unsigned long rte_mte_last;     /* Index of the last module in the resource.	*/
  unsigned long	rte_res_size;     /* Size of the resource.  */
};
typedef struct bfd_sym_resources_table_entry bfd_sym_resources_table_entry;

/* MODULES TABLE (MTE)

   Modules table entries are ordered by their appearance in a resource.
   (Note that having a single module copied into two resources is not
   possible).  Modules map back to their resource via an index into the
   resource table and an offset into the resource.  Modules also point
   to their source files, both the definition module and implementation
   module.  Because modules can be textually nested within other
   modules, a link to the parent (containing) module is required.  This
   module can textually contain other modules.  A link to the contiguous
   list of child (contained) modules is required.  Variables, statements,
   and types defined in the module are pointed to by indexing the head of
   the contiguous lists of contained variables, contained statements,
   and contained types.  */

struct bfd_sym_modules_table_entry
{
  unsigned long mte_rte_index;         /* Which resource it is in.  */
  unsigned long mte_res_offset;        /* Offset into the resource.  */
  unsigned long mte_size;              /* Size of module.  */
  char mte_kind;                       /* What kind of module this is.  */
  char mte_scope;                      /* How visible is it?  */
  unsigned long mte_parent;            /* Containing module.  */
  bfd_sym_file_reference mte_imp_fref; /* Implementation source.  */
  unsigned long mte_imp_end;           /* End of implementation source.  */
  unsigned long mte_nte_index;         /* The name of the module.  */
  unsigned long mte_cmte_index;        /* Modules contained in this.  */
  unsigned long mte_cvte_index;        /* Variables contained in this.  */
  unsigned long mte_clte_index;        /* Local labels defined here.  */
  unsigned long mte_ctte_index;        /* Types contained in this.  */
  unsigned long mte_csnte_idx_1;       /* CSNTE index of mte_snbr_first.  */
  unsigned long mte_csnte_idx_2;       /* CSNTE index of mte_snbr_last.  */
};
typedef struct bfd_sym_modules_table_entry bfd_sym_modules_table_entry;

/* FILE REFERENCES TABLE (FRTE)

   The FILE REFERENCES TABLE maps from source file to module & offset.
   The table is ordered by increasing file offset.  Each new offset
   references a module.

 				FRT	= FILE_SOURCE_START
 							FILE_SOURCE_INCREMENT*
 							END_OF_LIST.

	*** THIS MECHANISM IS VERY SLOW FOR FILE+STATEMENT_NUMBER TO
 	*** MODULE/CODE ADDRESS OPERATIONS.  ANOTHER MECHANISM IS
 	***	REQUIRED!!  */

union bfd_sym_file_references_table_entry
{
  struct
  {
    /* END_OF_LIST, FILE_NAME_INDEX, or module table entry.  */
    unsigned long type;
  }
  generic;

  struct
  {
    /* FILE_NAME_INDEX.  */
    unsigned long type;
    unsigned long nte_index;
    unsigned long mod_date;
  }
  filename;

  struct
  {
    /* < FILE_NAME_INDEX.  */
    unsigned long mte_index;
    unsigned long file_offset;
  }
  entry;
};
typedef union bfd_sym_file_references_table_entry bfd_sym_file_references_table_entry;

/* CONTAINED MODULES TABLE (CMTE)

   Contained Modules are lists of indices into the modules table.  The
   lists are terminated by an END_OF_LIST index.  All entries are of the
   same size, hence mapping an index into a CMTE list is simple.

   CMT = MTE_INDEX* END_OF_LIST.  */

union bfd_sym_contained_modules_table_entry
{
  struct
  {
    /* END_OF_LIST, index.  */
    unsigned long type;
  }
  generic;

  struct
  {
    unsigned long mte_index; /* Index into the Modules Table.  */
    unsigned long nte_index; /* The name of the module.  */
  }
  entry;
};
typedef union bfd_sym_contained_modules_table_entry bfd_sym_contained_modules_table_entry;

/* CONTAINED VARIABLES TABLE (CVTE)

   Contained Variables map into the module table, file table, name table, and type
   table.  Contained Variables are a contiguous list of source file change record,
   giving the name of and offset into the source file corresponding to all variables
   following.  Variable definition records contain an index into the name table (giving
   the text of the variable as it appears in the source code), an index into the type
   table giving the type of the variable, an increment added to the source file
   offset giving the start of the implementation of the variable, and a storage
   class address, giving information on variable's runtime address.

   CVT = SOURCE_FILE_CHANGE SYMBOL_INFO* END_OF_LIST.
   SYMBOL_INFO = SYMBOL_DEFINITION | SOURCE_FILE_CHANGE .

   All entries are of the same size, making the fetching of data simple.  The
   variable entries in the list are in ALPHABETICAL ORDER to simplify the display of
   available variables for several of the debugger's windows.  */

/* 'la_size' determines the variant used below:

     == BFD_SYM_CVTE_SCA
     Traditional STORAGE_CLASS_ADDRESS;

     <= BFD_SYM_CVTE_LA_MAX_SIZE
     That many logical address bytes ("in-situ");

     == BFD_SYM_CVTE_BIG_LA
     Logical address bytes in constant pool, at offset 'big_la'.  */

#define	BFD_SYM_CVTE_SCA 0          /* Indicate SCA variant of CVTE.  */
#define	BFD_SYM_CVTE_LA_MAX_SIZE 13 /* Max# of logical address bytes in a CVTE.  */
#define	BFD_SYM_CVTE_BIG_LA 127     /* Indicates LA redirection to constant pool.  */

union bfd_sym_contained_variables_table_entry
{
  struct
  {
    /* END_OF_LIST, SOURCE_FILE_CHANGE, or type table entry.  */
    unsigned long type;
  }
  generic;

  struct
  {
    /* SOURCE_FILE_CHANGE.  */
    unsigned long type;
    bfd_sym_file_reference fref;
  }
  file;

  struct
  {
    /* < SOURCE_FILE_CHANGE.  */
    unsigned long tte_index;
    unsigned long nte_index;
    unsigned long file_delta;                       /* Increment from previous source.  */
    unsigned char scope;
    unsigned char la_size;                          /* #bytes of LAs below.  */

    union
    {
      /* la_size == BFD_SYM_CVTE_SCA.  */
      struct
      {
	unsigned char sca_kind;	                    /* Distinguish local from value/var formal.  */
	unsigned char sca_class;                    /* The storage class itself.  */
	unsigned long sca_offset;
      }
      scstruct;

      /* la_size <= BFD_SYM_CVTE_LA_MAX_SIZE.  */
      struct {
	unsigned char la[BFD_SYM_CVTE_LA_MAX_SIZE]; /* Logical address bytes.  */
	unsigned char la_kind;                      /* Eqv. cvte_location.sca_kind.  */
      }
      lastruct;

      /* la_size == BFD_SYM_CVTE_BIG_LA 127.  */
      struct
      {
	unsigned long big_la;                       /* Logical address bytes in constant pool.  */
	unsigned char big_la_kind;                  /* Eqv. cvte_location.sca_kind.  */
      }
      biglastruct;
    }
    address;
  }
  entry;
};
typedef union bfd_sym_contained_variables_table_entry bfd_sym_contained_variables_table_entry;

/* CONTAINED STATEMENTS TABLE (CSNTE)

   Contained Statements table.  This table is similar to the Contained
   Variables table except that instead of VARIABLE_DEFINITION entries, this
   module contains STATEMENT_NUMBER_DEFINITION entries.  A statement number
   definition points back to the containing module (via an index into
   the module entry table) and contains the file and resource deltas
   to add to the previous values to get to this statement.
   All entries are of the same size, making the fetching of data simple.  The
   entries in the table are in order of increasing statement number within the
   source file.

   The Contained Statements table is indexed from two places.  An MTE contains
   an index to the first statement number within the module.  An FRTE contains
   an index to the first statement in the table (Possibly.  This is slow.)  Or
   a table of fast statement number to CSNTE entry mappings indexes into the
   table.  Choice not yet made.  */

union bfd_sym_contained_statements_table_entry
{
  struct
  {
    /* END_OF_LIST, SOURCE_FILE_CHANGE, or statement table entry.  */
    unsigned long type;
  }
  generic;

  struct
  {
    /* SOURCE_FILE_CHANGE.  */
    unsigned long type;
    bfd_sym_file_reference fref; /* File name table.  */
  }
  file;

  struct
  {
    unsigned long mte_index;     /* Which module contains it.  */
    unsigned long file_delta;    /* Where it is defined.  */
    unsigned long mte_offset;    /* Where it is in the module.  */
  }
  entry;
};
typedef union bfd_sym_contained_statements_table_entry bfd_sym_contained_statements_table_entry;

/* CONTAINED LABELS TABLE (CLTE)

   Contained Labels table names those labels local to the module.  It is similar
   to the Contained Statements table.  */

union bfd_sym_contained_labels_table_entry
{
  struct
  {
    /* END_OF_LIST, SOURCE_FILE_CHANGE, index.  */
    unsigned long type;
  }
  generic;

  struct
  {
    /* SOURCE_FILE_CHANGE.  */
    unsigned long type;
    bfd_sym_file_reference fref;
  }
  file;

  struct
  {
    /* < SOURCE_FILE_CHANGE.  */
    unsigned long mte_index;   /* Which module contains us.  */
    unsigned long mte_offset;  /* Where it is in the module.  */
    unsigned long nte_index;   /* The name of the label.  */
    unsigned long file_delta;  /* Where it is defined.  */
    unsigned short scope;      /* How visible the label is.  */
  }
  entry;
};
typedef union bfd_sym_contained_labels_table_entry bfd_sym_contained_labels_table_entry;

/* CONTAINED TYPES TABLE (CTTE)

   Contained Types define the named types that are in the module.  It is used to
   map name indices into type indices.  The type entries in the table are in
   alphabetical order by type name.  */

union bfd_sym_contained_types_table_entry
{
  struct
  {
    /* END_OF_LIST, SOURCE_FILE_CHANGE, or type table entry.  */
    unsigned long type;
  }
  generic;

  struct
  {
    /* SOURCE_FILE_CHANGE.  */
    unsigned long type;
    bfd_sym_file_reference fref;
  }
  file;

  struct
  {
    /* < SOURCE_FILE_CHANGE.  */
    unsigned long tte_index;
    unsigned long nte_index;
    unsigned long file_delta; /* From last file definition.  */
  }
  entry;
};
typedef union bfd_sym_contained_types_table_entry bfd_sym_contained_types_table_entry;

/* TYPE TABLE (TTE).  */

typedef unsigned long bfd_sym_type_table_entry;

/* TYPE INFORMATION TABLE (TINFO).  */

struct bfd_sym_type_information_table_entry
{
  unsigned long nte_index;
  unsigned long physical_size;
  unsigned long logical_size;
  unsigned long offset;
};
typedef struct bfd_sym_type_information_table_entry bfd_sym_type_information_table_entry;

/* FILE REFERENCES INDEX TABLE (FITE)

   The FRTE INDEX TABLE indexes into the FILE REFERENCE TABLE above.  The FRTE
   at that index is the FILE_SOURCE_START for a series of files.  The FRTEs are
   indexed from 1.  The list is terminated with an END_OF_LIST.  */

union bfd_sym_file_references_index_table_entry
{
  struct
  {
    unsigned long type;
  }
  generic;

  struct
  {
    unsigned long frte_index;  /* Index into the FRTE table.  */
    unsigned long nte_index;   /* Name table index, gives filename.  */
  }
  entry;
};
typedef union bfd_sym_file_references_index_table_entry bfd_sym_file_references_index_table_entry;

/* CONSTANT POOL (CONST)

   The CONSTANT_POOL consists of entries that start on word boundaries.  The entries
   are referenced by byte index into the constant pool, not by record number.

   Each entry takes the form:

   <16-bit size>
   <that many bytes of stuff>

   Entries do not cross page boundaries.  */

typedef short bfd_sym_constant_pool_entry;

/* The DISK_SYMBOL_HEADER_BLOCK is the first record in a .SYM file,
   defining the physical characteristics of the symbolic information.
   The remainder of the * .SYM file is stored in fixed block
   allocations. For the purposes of paging, the * file is considered
   to be an array of dshb_page_size blocks, with block 0 (and *
   possibly more) devoted to the DISK_SYMBOL_HEADER_BLOCK.

   The dti_object_count field means that the allowed indices for that
   type of object are 0 .. dti_object_count. An index of 0, although
   allowed, is never done.  However, an 0th entry is created in the
   table.  That entry is filled with all zeroes.  The reason for this
   is to avoid off-by-one programming errors that would otherwise
   occur: an index of k *MEANS* k, not k-1 when going to the disk
   table.  */

struct bfd_sym_table_info
{
  unsigned long dti_first_page;   /* First page for this table.  */
  unsigned long dti_page_count;   /* Number of pages for the table.  */
  unsigned long dti_object_count; /* Number of objects in the table.  */
};
typedef struct bfd_sym_table_info bfd_sym_table_info;

struct bfd_sym_header_block
{
  unsigned char dshb_id[32];      /* Version information.  */
  unsigned short dshb_page_size;  /* Size of the pages/blocks.  */
  unsigned long dshb_hash_page;   /* Disk page for the hash table.  */
  unsigned long dshb_root_mte;    /* MTE index of the program root.  */
  unsigned long dshb_mod_date;    /* modification date of executable.  */
  bfd_sym_table_info dshb_frte;   /* Per TABLE information.  */
  bfd_sym_table_info dshb_rte;
  bfd_sym_table_info dshb_mte;
  bfd_sym_table_info dshb_cmte;
  bfd_sym_table_info dshb_cvte;
  bfd_sym_table_info dshb_csnte;
  bfd_sym_table_info dshb_clte;
  bfd_sym_table_info dshb_ctte;
  bfd_sym_table_info dshb_tte;
  bfd_sym_table_info dshb_nte;
  bfd_sym_table_info dshb_tinfo;
  bfd_sym_table_info dshb_fite;   /* File information.  */
  bfd_sym_table_info dshb_const;  /* Constant pool.  */

  unsigned char dshb_file_creator[4]; /* Executable's creator.  */
  unsigned char dshb_file_type[4];    /* Executable's file type.  */
};
typedef struct bfd_sym_header_block bfd_sym_header_block;

struct bfd_sym_data_struct
{
  unsigned char *name_table;
  bfd_sym_header_block header;
  bfd_sym_version version;
  bfd *sbfd;
};
typedef struct bfd_sym_data_struct bfd_sym_data_struct;

extern bfd_boolean bfd_sym_mkobject
  (bfd *);
extern void bfd_sym_print_symbol
  (bfd *, PTR, asymbol *, bfd_print_symbol_type);
extern bfd_boolean bfd_sym_valid
  (bfd *);
extern unsigned char * bfd_sym_read_name_table
  (bfd *, bfd_sym_header_block *);
extern void bfd_sym_parse_file_reference_v32
  (unsigned char *, size_t, bfd_sym_file_reference *);
extern void bfd_sym_parse_disk_table_v32
  (unsigned char *, size_t, bfd_sym_table_info *);
extern void bfd_sym_parse_header_v32
  (unsigned char *, size_t, bfd_sym_header_block *);
extern int bfd_sym_read_header_v32
  (bfd *, bfd_sym_header_block *);
extern int bfd_sym_read_header_v34
  (bfd *, bfd_sym_header_block *);
extern int bfd_sym_read_header
  (bfd *, bfd_sym_header_block *, bfd_sym_version);
extern int bfd_sym_read_version
  (bfd *, bfd_sym_version *);
extern void bfd_sym_display_table_summary
  (FILE *, bfd_sym_table_info *, const char *);
extern void bfd_sym_display_header
  (FILE *, bfd_sym_header_block *);
extern void bfd_sym_parse_resources_table_entry_v32
  (unsigned char *, size_t, bfd_sym_resources_table_entry *);
extern void bfd_sym_parse_modules_table_entry_v33
  (unsigned char *, size_t, bfd_sym_modules_table_entry *);
extern void bfd_sym_parse_file_references_table_entry_v32
  (unsigned char *, size_t, bfd_sym_file_references_table_entry *);
extern void bfd_sym_parse_contained_modules_table_entry_v32
  (unsigned char *, size_t, bfd_sym_contained_modules_table_entry *);
extern void bfd_sym_parse_contained_variables_table_entry_v32
  (unsigned char *, size_t, bfd_sym_contained_variables_table_entry *);
extern void bfd_sym_parse_contained_statements_table_entry_v32
  (unsigned char *, size_t, bfd_sym_contained_statements_table_entry *);
extern void bfd_sym_parse_contained_labels_table_entry_v32
  (unsigned char *, size_t, bfd_sym_contained_labels_table_entry *);
extern void bfd_sym_parse_type_table_entry_v32
  (unsigned char *, size_t, bfd_sym_type_table_entry *);
extern int bfd_sym_fetch_resources_table_entry
  (bfd *, bfd_sym_resources_table_entry *, unsigned long);
extern int bfd_sym_fetch_modules_table_entry
  (bfd *, bfd_sym_modules_table_entry *, unsigned long);
extern int bfd_sym_fetch_file_references_table_entry
  (bfd *, bfd_sym_file_references_table_entry *, unsigned long);
extern int bfd_sym_fetch_contained_modules_table_entry
  (bfd *, bfd_sym_contained_modules_table_entry *, unsigned long);
extern int bfd_sym_fetch_contained_variables_table_entry
  (bfd *, bfd_sym_contained_variables_table_entry *, unsigned long);
extern int bfd_sym_fetch_contained_statements_table_entry
  (bfd *, bfd_sym_contained_statements_table_entry *, unsigned long);
extern int bfd_sym_fetch_contained_labels_table_entry
  (bfd *, bfd_sym_contained_labels_table_entry *, unsigned long);
extern int bfd_sym_fetch_contained_types_table_entry
  (bfd *, bfd_sym_contained_types_table_entry *, unsigned long);
extern int bfd_sym_fetch_file_references_index_table_entry
  (bfd *, bfd_sym_file_references_index_table_entry *, unsigned long);
extern int bfd_sym_fetch_constant_pool_entry
  (bfd *, bfd_sym_constant_pool_entry *, unsigned long);
extern int bfd_sym_fetch_type_table_entry
  (bfd *, bfd_sym_type_table_entry *, unsigned long);
extern int bfd_sym_fetch_type_information_table_entry
  (bfd *, bfd_sym_type_information_table_entry *, unsigned long);
extern int bfd_sym_fetch_type_table_information
  (bfd *, bfd_sym_type_information_table_entry *, unsigned long);
extern const unsigned char * bfd_sym_symbol_name
  (bfd *, unsigned long);
extern const unsigned char * bfd_sym_module_name
  (bfd *, unsigned long);
extern const char * bfd_sym_unparse_storage_kind
  (enum bfd_sym_storage_kind);
extern const char * bfd_sym_unparse_storage_class
  (enum bfd_sym_storage_class);
extern const char * bfd_sym_unparse_module_kind
  (enum bfd_sym_module_kind);
extern const char * bfd_sym_unparse_symbol_scope
  (enum bfd_sym_symbol_scope);
extern void bfd_sym_print_file_reference
  (bfd *, FILE *, bfd_sym_file_reference *);
extern void bfd_sym_print_resources_table_entry
  (bfd *, FILE *, bfd_sym_resources_table_entry *);
extern void bfd_sym_print_modules_table_entry
  (bfd *, FILE *, bfd_sym_modules_table_entry *);
extern void bfd_sym_print_file_references_table_entry
  (bfd *, FILE *, bfd_sym_file_references_table_entry *);
extern void bfd_sym_print_contained_modules_table_entry
  (bfd *, FILE *, bfd_sym_contained_modules_table_entry *);
extern void bfd_sym_print_contained_variables_table_entry
  (bfd *, FILE *f, bfd_sym_contained_variables_table_entry *);
extern void bfd_sym_print_contained_statements_table_entry
  (bfd *, FILE *, bfd_sym_contained_statements_table_entry *);
extern void bfd_sym_print_contained_labels_table_entry
  (bfd *, FILE *, bfd_sym_contained_labels_table_entry *);
extern void bfd_sym_print_contained_types_table_entry
  (bfd *, FILE *, bfd_sym_contained_types_table_entry *);
extern const char * bfd_sym_type_operator_name
  (unsigned char);
extern const char * bfd_sym_type_basic_name
  (unsigned char);
extern int bfd_sym_fetch_long
  (unsigned char *, unsigned long, unsigned long, unsigned long *, long *);
extern void bfd_sym_print_type_information
  (bfd *, FILE *, unsigned char *, unsigned long, unsigned long, unsigned long *);
extern void bfd_sym_print_type_information_table_entry
  (bfd *, FILE *, bfd_sym_type_information_table_entry *);
extern void bfd_sym_print_file_references_index_table_entry
  (bfd *, FILE *, bfd_sym_file_references_index_table_entry *);
extern void bfd_sym_print_constant_pool_entry
  (bfd *, FILE *, bfd_sym_constant_pool_entry *);
extern unsigned char * bfd_sym_display_name_table_entry
  (bfd *, FILE *, unsigned char *);
extern void bfd_sym_display_name_table
  (bfd *, FILE *);
extern void bfd_sym_display_resources_table
  (bfd *, FILE *);
extern void bfd_sym_display_modules_table
  (bfd *, FILE *);
extern void bfd_sym_display_file_references_table
  (bfd *, FILE *);
extern void bfd_sym_display_contained_modules_table
  (bfd *, FILE *);
extern void bfd_sym_display_contained_variables_table
  (bfd *, FILE *);
extern void bfd_sym_display_contained_statements_table
  (bfd *, FILE *);
extern void bfd_sym_display_contained_labels_table
  (bfd *, FILE *);
extern void bfd_sym_display_contained_types_table
  (bfd *, FILE *);
extern void bfd_sym_display_file_references_index_table
  (bfd *, FILE *);
extern void bfd_sym_display_constant_pool
  (bfd *, FILE *);
extern void bfd_sym_display_type_information_table
  (bfd *, FILE *);
extern int bfd_sym_scan
  (bfd *, bfd_sym_version, bfd_sym_data_struct *);
extern const bfd_target * bfd_sym_object_p
  (bfd *);
extern asymbol * bfd_sym_make_empty_symbol
  (bfd *);
extern void bfd_sym_get_symbol_info
  (bfd *, asymbol *, symbol_info *);
extern long bfd_sym_get_symtab_upper_bound
  (bfd *);
extern long bfd_sym_canonicalize_symtab
  (bfd *, asymbol **);
extern int bfd_sym_sizeof_headers
  (bfd *, bfd_boolean);

#endif /* __xSYM_H__ */
