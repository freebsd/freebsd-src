/* Definitions for symbol-reading containing "stabs", for GDB.
   Copyright 1992 Free Software Foundation, Inc.
   Contributed by Cygnus Support.  Written by John Gilmore.

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

/* This file exists to hold the common definitions required of most of
   the symbol-readers that end up using stabs.  The common use of
   these `symbol-type-specific' customizations of the generic data
   structures makes the stabs-oriented symbol readers able to call
   each others' functions as required.  */

#if !defined (GDBSTABS_H)
#define GDBSTABS_H

/* Offsets in the psymtab's section_offsets array for various kinds of
   stabs symbols.  Every psymtab built from stabs will have these offsets
   filled in by these guidelines, so that when actually reading symbols, the
   proper offset can simply be selected and added to the symbol value.  */

#define	SECT_OFF_TEXT	0
#define	SECT_OFF_DATA	1
#define	SECT_OFF_BSS	2
#define	SECT_OFF_RODATA	3
#define	SECT_OFF_MAX	4		/* Count of possible values */

/* The stab_section_info chain remembers info from the ELF symbol table,
   while psymtabs are being built for the other symbol tables in the 
   objfile.  It is destroyed at the complation of psymtab-reading.
   Any info that was used from it has been copied into psymtabs.  */

struct stab_section_info {
  char *filename;
  CORE_ADDR sections[SECT_OFF_MAX];
  struct stab_section_info *next;
  int found;		/* Count of times it's found in searching */
};

/* Information is passed among various dbxread routines for accessing
   symbol files.  A pointer to this structure is kept in the sym_stab_info
   field of the objfile struct.  */
 
struct dbx_symfile_info {
  CORE_ADDR text_addr;		/* Start of text section */
  int text_size;		/* Size of text section */
  int symcount;			/* How many symbols are there in the file */
  char *stringtab;		/* The actual string table */
  int stringtab_size;		/* Its size */
  file_ptr symtab_offset;	/* Offset in file to symbol table */
  int symbol_size;		/* Bytes in a single symbol */
  struct stab_section_info *stab_section_info; 	/* section starting points
				   of the original .o files before linking. */
};

#define DBX_SYMFILE_INFO(o)	((struct dbx_symfile_info *)((o)->sym_stab_info))
#define DBX_TEXT_ADDR(o)	(DBX_SYMFILE_INFO(o)->text_addr)
#define DBX_TEXT_SIZE(o)	(DBX_SYMFILE_INFO(o)->text_size)
#define DBX_SYMCOUNT(o)		(DBX_SYMFILE_INFO(o)->symcount)
#define DBX_STRINGTAB(o)	(DBX_SYMFILE_INFO(o)->stringtab)
#define DBX_STRINGTAB_SIZE(o)	(DBX_SYMFILE_INFO(o)->stringtab_size)
#define DBX_SYMTAB_OFFSET(o)	(DBX_SYMFILE_INFO(o)->symtab_offset)
#define DBX_SYMBOL_SIZE(o)	(DBX_SYMFILE_INFO(o)->symbol_size)

#endif /* GDBSTABS_H */
