/* Definitions for reading symbol files into GDB.
   Copyright (C) 1990, 1991, 1992, 1993, 1994, 1996
   Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#if !defined (SYMFILE_H)
#define SYMFILE_H

/* This file requires that you first include "bfd.h".  */

/* Partial symbols are stored in the psymbol_cache and pointers to them
   are kept in a dynamically grown array that is obtained from malloc and
   grown as necessary via realloc.  Each objfile typically has two of these,
   one for global symbols and one for static symbols.  Although this adds
   a level of indirection for storing or accessing the partial symbols,
   it allows us to throw away duplicate psymbols and set all pointers
   to the single saved instance. */

struct psymbol_allocation_list {

  /* Pointer to beginning of dynamically allocated array of pointers to
   partial symbols.  The array is dynamically expanded as necessary to
   accommodate more pointers. */

  struct partial_symbol **list;

  /* Pointer to next available slot in which to store a pointer to a partial
     symbol. */

  struct partial_symbol **next;

  /* Number of allocated pointer slots in current dynamic array (not the
     number of bytes of storage).  The "next" pointer will always point
     somewhere between list[0] and list[size], and when at list[size] the
     array will be expanded on the next attempt to store a pointer. */

  int size;
};

/* Structure to keep track of symbol reading functions for various
   object file types.  */

struct sym_fns {

  /* BFD flavour that we handle, or (as a special kludge, see xcoffread.c,
     (enum bfd_flavour)-1 for xcoff).  */

  enum bfd_flavour sym_flavour;

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

/* The default version of sym_fns.sym_offsets for readers that don't
   do anything special.  */

extern struct section_offsets *
default_symfile_offsets PARAMS ((struct objfile *objfile, CORE_ADDR addr));


extern void
extend_psymbol_list PARAMS ((struct psymbol_allocation_list *,
			     struct objfile *));

/* Add any kind of symbol to a psymbol_allocation_list. */

/* #include "demangle.h" */

extern void
add_psymbol_to_list PARAMS ((char *, int, namespace_enum, enum address_class,
			     struct psymbol_allocation_list *, long, CORE_ADDR,
			     enum language, struct objfile *));

extern void
add_psymbol_with_dem_name_to_list PARAMS ((char *, int, char *, int, namespace_enum, 
                                           enum address_class,
                                           struct psymbol_allocation_list *, 
                                           long, CORE_ADDR,
                                           enum language, struct objfile *));


extern void init_psymbol_list PARAMS ((struct objfile *, int));

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
			      struct partial_symbol **,
			      struct partial_symbol **));

/* Sorting your symbols for fast lookup or alphabetical printing.  */

extern void
sort_block_syms PARAMS ((struct block *));

extern void
sort_symtab_syms PARAMS ((struct symtab *));

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

/* whether to auto load solibs at startup time:  0/1. 

   On all platforms, 0 means "don't auto load".

   On HP-UX, > 0 means a threshhold, in megabytes, of symbol table which will
   be auto loaded.  When the cumulative size of solib symbol table exceeds
   this threshhold, solibs' symbol tables will not be loaded.

   On other platforms, > 0 means, "always auto load".
   */

extern int auto_solib_add;

/* From symfile.c */

extern CORE_ADDR
entry_point_address PARAMS ((void));

extern struct partial_symtab *
allocate_psymtab PARAMS ((char *, struct objfile *));

extern void
discard_psymtab PARAMS ((struct partial_symtab *));

extern void find_lowest_section PARAMS ((bfd *, asection *, PTR));

extern bfd * symfile_bfd_open PARAMS ((char *));

/* Remote targets may wish to use this as their load function.  */
extern void generic_load PARAMS ((char *name, int from_tty));

/* Utility functions for overlay sections: */
extern int overlay_debugging;
extern int overlay_cache_invalid;

/* return the "mapped" overlay section  containing the PC */
extern asection * 
find_pc_mapped_section PARAMS ((CORE_ADDR));

/* return any overlay section containing the PC (even in its LMA region) */
extern asection *
find_pc_overlay PARAMS ((CORE_ADDR));

/* return true if the section is an overlay */
extern int
section_is_overlay PARAMS ((asection *));

/* return true if the overlay section is currently "mapped" */
extern int
section_is_mapped PARAMS ((asection *));

/* return true if pc belongs to section's VMA */
extern CORE_ADDR
pc_in_mapped_range PARAMS ((CORE_ADDR, asection *));

/* return true if pc belongs to section's LMA */
extern CORE_ADDR
pc_in_unmapped_range PARAMS ((CORE_ADDR, asection *));

/* map an address from a section's LMA to its VMA */
extern CORE_ADDR
overlay_mapped_address PARAMS ((CORE_ADDR, asection *));

/* map an address from a section's VMA to its LMA */
extern CORE_ADDR
overlay_unmapped_address PARAMS ((CORE_ADDR, asection *));

/* convert an address in an overlay section (force into VMA range) */
extern CORE_ADDR 
symbol_overlayed_address PARAMS ((CORE_ADDR, asection *));

/* From dwarfread.c */

extern void
dwarf_build_psymtabs PARAMS ((struct objfile *, struct section_offsets *, int,
			      file_ptr, unsigned int, file_ptr, unsigned int));

/* From dwarf2read.c */

extern int dwarf2_has_info PARAMS ((bfd *abfd));

extern void dwarf2_build_psymtabs PARAMS ((struct objfile *,
					   struct section_offsets *,
					   int));
/* From mdebugread.c */

/* Hack to force structures to exist before use in parameter list.  */
struct ecoff_debug_hack
{
  struct ecoff_debug_swap *a;
  struct ecoff_debug_info *b;
};
extern void
mdebug_build_psymtabs PARAMS ((struct objfile *,
			       const struct ecoff_debug_swap *,
			       struct ecoff_debug_info *,
			       struct section_offsets *));

extern void
elfmdebug_build_psymtabs PARAMS ((struct objfile *,
				  const struct ecoff_debug_swap *,
				  asection *,
				  struct section_offsets *));

#endif	/* !defined(SYMFILE_H) */
