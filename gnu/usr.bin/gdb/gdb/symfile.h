/* Definitions for reading symbol files into GDB.
   Copyright (C) 1990, 1991, 1992  Free Software Foundation, Inc.

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

#if !defined (SYMFILE_H)
#define SYMFILE_H

/* This file requires that you first include "bfd.h".  */

struct psymbol_allocation_list {
  struct partial_symbol *list;
  struct partial_symbol *next;
  int size;
};

/* Structure to keep track of symbol reading functions for various
   object file types.  */

struct sym_fns {

  /* is the name, or name prefix, of the BFD "target type" that this
     set of functions handles.  E.g. "a.out" or "sunOs" or "coff" or "elf".  */

  char *sym_name;

  /* counts how many bytes of sym_name should be checked against the
     BFD target type of the file being read.  If an exact match is
     desired, specify the number of characters in sym_name plus 1 for
     the '\0'.  If a prefix match is desired, specify the number of
     characters in sym_name.  */

  int sym_namelen;

  /* Initializes anything that is global to the entire symbol table.  It is
     called during symbol_file_add, when we begin debugging an entirely new
     program. */

  void (*sym_new_init) PARAMS ((struct objfile *));

  /* Reads any initial information from a symbol file, and initializes the
     struct sym_fns SF in preparation for sym_read().  It is called every
     time we read a symbol file for any reason. */

  void (*sym_init) PARAMS ((struct objfile *));

  /* sym_read (objfile, addr, mainline)
     Reads a symbol file into a psymtab (or possibly a symtab).
     OBJFILE is the objfile struct for the file we are reading.
     SECTION_OFFSETS
     are the offset between the file's specified section addresses and
     their true addresses in memory.
     MAINLINE is 1 if this is the
     main symbol table being read, and 0 if a secondary
     symbol file (e.g. shared library or dynamically loaded file)
     is being read.  */

  void (*sym_read) PARAMS ((struct objfile *, struct section_offsets *, int));

  /* Called when we are finished with an objfile.  Should do all cleanup
     that is specific to the object file format for the particular objfile. */
 
  void (*sym_finish) PARAMS ((struct objfile *));

  /* This function produces a file-dependent section_offsets structure,
     allocated in the objfile's storage, and based on the parameter.
     The parameter is currently a CORE_ADDR (FIXME!) for backward compatibility
     with the higher levels of GDB.  It should probably be changed to
     a string, where NULL means the default, and others are parsed in a file
     dependent way.  The result of this function is handed in to sym_read.  */

  struct section_offsets *(*sym_offsets) PARAMS ((struct objfile *, CORE_ADDR));

  /* Finds the next struct sym_fns.  They are allocated and initialized
     in whatever module implements the functions pointed to; an 
     initializer calls add_symtab_fns to add them to the global chain.  */

  struct sym_fns *next;

};

extern void
extend_psymbol_list PARAMS ((struct psymbol_allocation_list *,
			     struct objfile *));

/* Add any kind of symbol to a psymbol_allocation_list. */

#ifndef INLINE_ADD_PSYMBOL
#define INLINE_ADD_PSYMBOL 1
#endif

#if !INLINE_ADD_PSYMBOL

/* Since one arg is a struct, we have to pass in a ptr and deref it (sigh) */

#define	ADD_PSYMBOL_TO_LIST(name, namelength, namespace, class, list, value, language, objfile) \
  add_psymbol_to_list (name, namelength, namespace, class, &list, value, language, objfile)

#define	ADD_PSYMBOL_ADDR_TO_LIST(name, namelength, namespace, class, list, value, language, objfile) \
  add_psymbol_addr_to_list (name, namelength, namespace, class, &list, value, language, objfile)

#else	/* !INLINE_ADD_PSYMBOL */

#include "demangle.h"

#define	ADD_PSYMBOL_VT_TO_LIST(NAME,NAMELENGTH,NAMESPACE,CLASS,LIST,VALUE,VT,LANGUAGE, OBJFILE) \
  do {		        						\
    register struct partial_symbol *psym;				\
    if ((LIST).next >= (LIST).list + (LIST).size)			\
      extend_psymbol_list (&(LIST),(OBJFILE));				\
    psym = (LIST).next++;						\
    SYMBOL_NAME (psym) =						\
      (char *) obstack_alloc (&objfile->psymbol_obstack,		\
			      (NAMELENGTH) + 1);			\
    memcpy (SYMBOL_NAME (psym), (NAME), (NAMELENGTH));			\
    SYMBOL_NAME (psym)[(NAMELENGTH)] = '\0';				\
    SYMBOL_NAMESPACE (psym) = (NAMESPACE);				\
    PSYMBOL_CLASS (psym) = (CLASS);					\
    VT (psym) = (VALUE); 						\
    SYMBOL_LANGUAGE (psym) = (LANGUAGE);				\
    SYMBOL_INIT_DEMANGLED_NAME (psym, &objfile->psymbol_obstack);	\
  } while (0);

/* Add a symbol with an integer value to a psymtab. */

#define ADD_PSYMBOL_TO_LIST(name, namelength, namespace, class, list, value, language, objfile) \
  ADD_PSYMBOL_VT_TO_LIST (name, namelength, namespace, class, list, value, SYMBOL_VALUE, language, objfile)

/* Add a symbol with a CORE_ADDR value to a psymtab. */

#define	ADD_PSYMBOL_ADDR_TO_LIST(name, namelength, namespace, class, list, value, language, objfile)\
  ADD_PSYMBOL_VT_TO_LIST (name, namelength, namespace, class, list, value, SYMBOL_VALUE_ADDRESS, language, objfile)

#endif	/* INLINE_ADD_PSYMBOL */

			/*   Functions   */

extern void
sort_pst_symbols PARAMS ((struct partial_symtab *));

extern struct symtab *
allocate_symtab PARAMS ((char *, struct objfile *));

extern int
free_named_symtabs PARAMS ((char *));

extern void
fill_in_vptr_fieldno PARAMS ((struct type *));

extern void
add_symtab_fns PARAMS ((struct sym_fns *));

extern void
init_entry_point_info PARAMS ((struct objfile *));

extern void
syms_from_objfile PARAMS ((struct objfile *, CORE_ADDR, int, int));

extern void
new_symfile_objfile PARAMS ((struct objfile *, int, int));

extern struct partial_symtab *
start_psymtab_common PARAMS ((struct objfile *, struct section_offsets *,
			      char *, CORE_ADDR,
			      struct partial_symbol *,
			      struct partial_symbol *));

/* Sorting your symbols for fast lookup or alphabetical printing.  */

extern void
sort_block_syms PARAMS ((struct block *));

extern void
sort_symtab_syms PARAMS ((struct symtab *));

extern void
sort_all_symtab_syms PARAMS ((void));

/* Make a copy of the string at PTR with SIZE characters in the symbol obstack
   (and add a null character at the end in the copy).
   Returns the address of the copy.  */

extern char *
obsavestring PARAMS ((char *, int, struct obstack *));

/* Concatenate strings S1, S2 and S3; return the new string.
   Space is found in the symbol_obstack.  */

extern char *
obconcat PARAMS ((struct obstack *obstackp, const char *, const char *,
		  const char *));

			/*   Variables   */

/* From symfile.c */

extern struct partial_symtab *
allocate_psymtab PARAMS ((char *, struct objfile *));

/* Remote targets may wish to use this as their load function.  */
extern void generic_load PARAMS ((char *name, int from_tty));

/* From dwarfread.c */

extern void
dwarf_build_psymtabs PARAMS ((struct objfile *, struct section_offsets *, int,
			      file_ptr, unsigned int, file_ptr, unsigned int));

/* From demangle.c */

extern void
set_demangling_style PARAMS ((char *));

#endif	/* !defined(SYMFILE_H) */
