/* Include file for stabs debugging format support functions.
   Copyright 1986-1991, 1992, 1993 Free Software Foundation, Inc.

This file is part of GDB.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Definitions, prototypes, etc for stabs debugging format support
   functions.

   Variables declared in this file can be defined by #define-ing
   the name EXTERN to null.  It is used to declare variables that
   are normally extern, but which get defined in a single module
   using this technique.  */

#ifndef EXTERN
#define	EXTERN extern
#endif

/* Convert stab register number (from `r' declaration) to a gdb REGNUM.  */

#ifndef STAB_REG_TO_REGNUM
#define STAB_REG_TO_REGNUM(VALUE) (VALUE)
#endif

/* Hash table of global symbols whose values are not known yet.
   They are chained thru the SYMBOL_VALUE_CHAIN, since we don't
   have the correct data for that slot yet.

   The use of the LOC_BLOCK code in this chain is nonstandard--
   it refers to a FORTRAN common block rather than the usual meaning.  */

EXTERN struct symbol *global_sym_chain[HASHSIZE];

extern void common_block_start PARAMS ((char *, struct objfile *));
extern void common_block_end PARAMS ((struct objfile *));

/* Kludge for xcoffread.c */

struct pending_stabs
{
  int count;
  int length;
  char *stab[1];
};

EXTERN struct pending_stabs *global_stabs;

/* The type code that process_one_symbol saw on its previous invocation.
   Used to detect pairs of N_SO symbols. */

EXTERN int previous_stab_code;

/* Support for Sun changes to dbx symbol format */

/* For each identified header file, we have a table of types defined
   in that header file.

   header_files maps header file names to their type tables.
   It is a vector of n_header_files elements.
   Each element describes one header file.
   It contains a vector of types.

   Sometimes it can happen that the same header file produces
   different results when included in different places.
   This can result from conditionals or from different
   things done before including the file.
   When this happens, there are multiple entries for the file in this table,
   one entry for each distinct set of results.
   The entries are distinguished by the INSTANCE field.
   The INSTANCE field appears in the N_BINCL and N_EXCL symbol table and is
   used to match header-file references to their corresponding data.  */

struct header_file
{

  /* Name of header file */
  
  char *name;

  /* Numeric code distinguishing instances of one header file that produced
     different results when included.  It comes from the N_BINCL or N_EXCL. */

  int instance;

  /* Pointer to vector of types */

  struct type **vector;

  /* Allocated length (# elts) of that vector */

  int length;

};

EXTERN struct header_file *header_files;

EXTERN int n_header_files;

EXTERN int n_allocated_header_files;

/* Within each object file, various header files are assigned numbers.
   A type is defined or referred to with a pair of numbers
   (FILENUM,TYPENUM) where FILENUM is the number of the header file
   and TYPENUM is the number within that header file.
   TYPENUM is the index within the vector of types for that header file.

   FILENUM == 1 is special; it refers to the main source of the object file,
   and not to any header file.  FILENUM != 1 is interpreted by looking it up
   in the following table, which contains indices in header_files.  */

EXTERN int *this_object_header_files;

EXTERN int n_this_object_header_files;

EXTERN int n_allocated_this_object_header_files;

extern struct complaint unknown_symtype_complaint;
extern struct complaint unknown_symchar_complaint;

extern struct type *
read_type PARAMS ((char **, struct objfile *));

extern void
cleanup_undefined_types PARAMS ((void));

extern struct type **
dbx_lookup_type PARAMS ((int [2]));

extern long
read_number PARAMS ((char **, int));

extern void
add_undefined_type PARAMS ((struct type *));

extern struct symbol *
define_symbol PARAMS ((CORE_ADDR, char *, int, int, struct objfile *));

extern void
stabsread_init PARAMS ((void));

extern void
stabsread_new_init PARAMS ((void));

extern void
start_stabs PARAMS ((void));

extern void
end_stabs PARAMS ((void));

extern void
finish_global_stabs PARAMS ((struct objfile *objfile));

/* Functions exported by dbxread.c.  These are not in stabsread.h because
   they are only used by some stabs readers.  */

extern struct partial_symtab *
start_psymtab PARAMS ((struct objfile *, struct section_offsets *, char *,
		       CORE_ADDR, int, struct partial_symbol *,
		       struct partial_symbol *));

extern struct partial_symtab *
end_psymtab PARAMS ((struct partial_symtab *, char **, int, int, CORE_ADDR,
		     struct partial_symtab **, int));

extern void
process_one_symbol PARAMS ((int, int, CORE_ADDR, char *,
			    struct section_offsets *, struct objfile *));

extern void
elfstab_build_psymtabs PARAMS ((struct objfile *objfile,
				struct section_offsets *section_offsets,
				int mainline,
				file_ptr staboff, unsigned int stabsize,
				file_ptr stabstroffset,
				unsigned int stabstrsize));

extern void
pastab_build_psymtabs PARAMS ((struct objfile *, struct section_offsets *,
			       int));

#undef EXTERN
