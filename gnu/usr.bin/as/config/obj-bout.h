/* b.out object file format
   Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2,
   or (at your option) any later version.

   GAS is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with GAS; see the file COPYING.  If not, write
   to the Free Software Foundation, 675 Mass Ave, Cambridge, MA
   02139, USA. */

/*
 * This file is a modified version of 'a.out.h'.  It is to be used in all GNU
 * tools modified to support the i80960 b.out format (or tools that operate on
 * object files created by such tools).
 *
 * All i80960 development is done in a CROSS-DEVELOPMENT environment.  I.e.,
 * object code is generated on, and executed under the direction of a symbolic
 * debugger running on, a host system.  We do not want to be subject to the
 * vagaries of which host it is or whether it supports COFF or a.out format, or
 * anything else.  We DO want to:
 *
 *	o always generate the same format object files, regardless of host.
 *
 *	o have an 'a.out' header that we can modify for our own purposes
 *	  (the 80960 is typically an embedded processor and may require
 *	  enhanced linker support that the normal a.out.h header can't
 *	  accommodate).
 *
 * As for byte-ordering, the following rules apply:
 *
 *	o Text and data that is actually downloaded to the target is always
 *	  in i80960 (little-endian) order.
 *
 *	o All other numbers (in the header, symbols, relocation directives)
 *	  are in host byte-order:  object files CANNOT be lifted from a
 *	  little-end host and used on a big-endian (or vice versa) without
 *	  modification.
 *
 *	o The downloader ('comm960') takes care to generate a pseudo-header
 *	  with correct (i80960) byte-ordering before shipping text and data
 *	  off to the NINDY monitor in the target systems.  Symbols and
 *	  relocation info are never sent to the target.
 */


#define OBJ_BOUT 1

#include "targ-cpu.h"

/* bout uses host byte order for headers */
#ifdef CROSS_COMPILE
#undef CROSS_COMPILE
#endif /* CROSS_COMPILE */

/* We want \v. */
#define BACKSLASH_V 1

#define OBJ_DEFAULT_OUTPUT_FILE_NAME	"b.out"

extern const short seg_N_TYPE[];
extern const segT  N_TYPE_seg[];

#define BMAGIC	0415
/* We don't accept the following (see N_BADMAG macro).
 * They're just here so GNU code will compile.
 */
#define	OMAGIC	0407		/* old impure format */
#define	NMAGIC	0410		/* read-only text */
#define	ZMAGIC	0413		/* demand load format */

/* FILE HEADER
 *	All 'lengths' are given as a number of bytes.
 *	All 'alignments' are for relinkable files only;  an alignment of
 *		'n' indicates the corresponding segment must begin at an
 *		address that is a multiple of (2**n).
 */
struct exec {
	/* Standard stuff */
	unsigned long a_magic;	/* Identifies this as a b.out file	*/
	unsigned long a_text;	/* Length of text			*/
	unsigned long a_data;	/* Length of data			*/
	unsigned long a_bss;	/* Length of runtime uninitialized data area */
	unsigned long a_syms;	/* Length of symbol table		*/
	unsigned long a_entry;	/* Runtime start address		*/
	unsigned long a_trsize;	/* Length of text relocation info	*/
	unsigned long a_drsize;	/* Length of data relocation info	*/

	/* Added for i960 */
	unsigned long a_tload;	/* Text runtime load address		*/
	unsigned long a_dload;	/* Data runtime load address		*/
	unsigned char a_talign;	/* Alignment of text segment		*/
	unsigned char a_dalign;	/* Alignment of data segment		*/
	unsigned char a_balign;	/* Alignment of bss segment		*/
	unsigned char unused;	/* (Just to make struct size a multiple of 4) */
};

#define N_BADMAG(x)	(((x).a_magic) != BMAGIC)
#define N_TXTOFF(x)	( sizeof(struct exec) )
#define N_DATOFF(x)	( N_TXTOFF(x) + (x).a_text )
#define N_TROFF(x)	( N_DATOFF(x) + (x).a_data )
#define N_DROFF(x)	( N_TROFF(x) + (x).a_trsize )
#define N_SYMOFF(x)	( N_DROFF(x) + (x).a_drsize )
#define N_STROFF(x)	( N_SYMOFF(x) + (x).a_syms )

/* A single entry in the symbol table
 */
struct nlist {
	union {
		char	*n_name;
		struct nlist *n_next;
		long	n_strx;		/* Index into string table	*/
	} n_un;
	unsigned char n_type;	/* See below				*/
	char	n_other;	/* Used in i80960 support -- see below	*/
	short	n_desc;
	unsigned long n_value;
};

typedef struct nlist obj_symbol_type;

/* Legal values of n_type
 */
#define N_UNDF	0	/* Undefined symbol	*/
#define N_ABS	2	/* Absolute symbol	*/
#define N_TEXT	4	/* Text symbol		*/
#define N_DATA	6	/* Data symbol		*/
#define N_BSS	8	/* BSS symbol		*/
#define N_FN	31	/* Filename symbol	*/

#define N_EXT	1	/* External symbol (OR'd in with one of above)	*/
#define N_TYPE	036	/* Mask for all the type bits			*/
#define N_STAB	0340	/* Mask for all bits used for SDB entries 	*/

#ifndef CUSTOM_RELOC_FORMAT
struct relocation_info {
	int	 r_address;	/* File address of item to be relocated	*/
	unsigned
    r_index:24,/* Index of symbol on which relocation is based*/
    r_pcrel:1,	/* 1 => relocate PC-relative; else absolute
		 *	On i960, pc-relative implies 24-bit
		 *	address, absolute implies 32-bit.
		 */
    r_length:2,	/* Number of bytes to relocate:
		 *	0 => 1 byte
		 *	1 => 2 bytes
		 *	2 => 4 bytes -- only value used for i960
		 */
    r_extern:1,
    r_bsr:1,	/* Something for the GNU NS32K assembler */
    r_disp:1,	/* Something for the GNU NS32K assembler */
    r_callj:1,	/* 1 if relocation target is an i960 'callj' */
    nuthin:1;	/* Unused				*/
};
#endif /* CUSTOM_RELOC_FORMAT */

/*
 *  Macros to extract information from a symbol table entry.
 *  This syntaxic indirection allows independence regarding a.out or coff.
 *  The argument (s) of all these macros is a pointer to a symbol table entry.
 */

/* Predicates */
/* True if the symbol is external */
#define S_IS_EXTERNAL(s)	((s)->sy_symbol.n_type & N_EXT)

/* True if symbol has been defined, ie is in N_{TEXT,DATA,BSS,ABS} or N_EXT */
#define S_IS_DEFINED(s)		((S_GET_TYPE(s) != N_UNDF) || (S_GET_DESC(s) != 0))
#define S_IS_REGISTER(s)	((s)->sy_symbol.n_type == N_REGISTER)

/* True if a debug special symbol entry */
#define S_IS_DEBUG(s)		((s)->sy_symbol.n_type & N_STAB)
/* True if a symbol is local symbol name */
/* A symbol name whose name begin with ^A is a gas internal pseudo symbol
   nameless symbols come from .stab directives. */
#define S_IS_LOCAL(s)		(S_GET_NAME(s) && \
				 !S_IS_DEBUG(s) && \
				 (S_GET_NAME(s)[0] == '\001' || \
				  (S_LOCAL_NAME(s) && !flagseen['L'])))
/* True if a symbol is not defined in this file */
#define S_IS_EXTERN(s)		((s)->sy_symbol.n_type & N_EXT)
/* True if the symbol has been generated because of a .stabd directive */
#define S_IS_STABD(s)		(S_GET_NAME(s) == NULL)

/* Accessors */
/* The value of the symbol */
#define S_GET_VALUE(s)		((unsigned long) ((s)->sy_symbol.n_value))
/* The name of the symbol */
#define S_GET_NAME(s)		((s)->sy_symbol.n_un.n_name)
/* The pointer to the string table */
#define S_GET_OFFSET(s)		((s)->sy_symbol.n_un.n_strx)
/* The type of the symbol */
#define S_GET_TYPE(s)		((s)->sy_symbol.n_type & N_TYPE)
/* The numeric value of the segment */
#define S_GET_SEGMENT(s)	(N_TYPE_seg[S_GET_TYPE(s)])
/* The n_other expression value */
#define S_GET_OTHER(s)		((s)->sy_symbol.n_other)
/* The n_desc expression value */
#define S_GET_DESC(s)		((s)->sy_symbol.n_desc)

/* Modifiers */
/* Set the value of the symbol */
#define S_SET_VALUE(s,v)	((s)->sy_symbol.n_value = (unsigned long) (v))
/* Assume that a symbol cannot be simultaneously in more than on segment */
/* set segment */
#define S_SET_SEGMENT(s,seg)	((s)->sy_symbol.n_type &= ~N_TYPE,(s)->sy_symbol.n_type|=SEGMENT_TO_SYMBOL_TYPE(seg))
/* The symbol is external */
#define S_SET_EXTERNAL(s)	((s)->sy_symbol.n_type |= N_EXT)
/* The symbol is not external */
#define S_CLEAR_EXTERNAL(s)	((s)->sy_symbol.n_type &= ~N_EXT)
/* Set the name of the symbol */
#define S_SET_NAME(s,v)		((s)->sy_symbol.n_un.n_name = (v))
/* Set the offset in the string table */
#define S_SET_OFFSET(s,v)	((s)->sy_symbol.n_un.n_strx = (v))
/* Set the n_other expression value */
#define S_SET_OTHER(s,v)	((s)->sy_symbol.n_other = (v))
/* Set the n_desc expression value */
#define S_SET_DESC(s,v)		((s)->sy_symbol.n_desc = (v))

/* File header macro and type definition */

#define H_GET_FILE_SIZE(h)	(sizeof(struct exec) + \
				 H_GET_TEXT_SIZE(h) + H_GET_DATA_SIZE(h) + \
				 H_GET_SYMBOL_TABLE_SIZE(h) + \
				 H_GET_TEXT_RELOCATION_SIZE(h) + \
				 H_GET_DATA_RELOCATION_SIZE(h) + \
				 (h)->string_table_size)

#define H_GET_HEADER_SIZE(h)		(sizeof(struct exec))
#define H_GET_TEXT_SIZE(h)		((h)->header.a_text)
#define H_GET_DATA_SIZE(h)		((h)->header.a_data)
#define H_GET_BSS_SIZE(h)		((h)->header.a_bss)
#define H_GET_TEXT_RELOCATION_SIZE(h)	((h)->header.a_trsize)
#define H_GET_DATA_RELOCATION_SIZE(h)	((h)->header.a_drsize)
#define H_GET_SYMBOL_TABLE_SIZE(h)	((h)->header.a_syms)
#define H_GET_MAGIC_NUMBER(h)		((h)->header.a_info)
#define H_GET_ENTRY_POINT(h)		((h)->header.a_entry)
#define H_GET_STRING_SIZE(h)		((h)->string_table_size)
#define H_GET_LINENO_SIZE(h)		(0)

#ifdef EXEC_MACHINE_TYPE
#define H_GET_MACHINE_TYPE(h)		((h)->header.a_machtype)
#endif /* EXEC_MACHINE_TYPE */
#ifdef EXEC_VERSION
#define H_GET_VERSION(h)		((h)->header.a_version)
#endif /* EXEC_VERSION */

#define H_SET_TEXT_SIZE(h,v)		((h)->header.a_text = (v))
#define H_SET_DATA_SIZE(h,v)		((h)->header.a_data = (v))
#define H_SET_BSS_SIZE(h,v)		((h)->header.a_bss = (v))

#define H_SET_RELOCATION_SIZE(h,t,d)	(H_SET_TEXT_RELOCATION_SIZE((h),(t)),\
					 H_SET_DATA_RELOCATION_SIZE((h),(d)))

#define H_SET_TEXT_RELOCATION_SIZE(h,v)	((h)->header.a_trsize = (v))
#define H_SET_DATA_RELOCATION_SIZE(h,v)	((h)->header.a_drsize = (v))
#define H_SET_SYMBOL_TABLE_SIZE(h,v)	((h)->header.a_syms = (v) * \
					 sizeof(struct nlist))

#define H_SET_MAGIC_NUMBER(h,v)		((h)->header.a_magic = (v))

#define H_SET_ENTRY_POINT(h,v)		((h)->header.a_entry = (v))
#define H_SET_STRING_SIZE(h,v)		((h)->string_table_size = (v))
#ifdef EXEC_MACHINE_TYPE
#define H_SET_MACHINE_TYPE(h,v)		((h)->header.a_machtype = (v))
#endif /* EXEC_MACHINE_TYPE */
#ifdef EXEC_VERSION
#define H_SET_VERSION(h,v)		((h)->header.a_version = (v))
#endif /* EXEC_VERSION */

/*
 * Current means for getting the name of a segment.
 * This will change for infinite-segments support (e.g. COFF).
 */
#define	segment_name(seg)  ( seg_name[(int)(seg)] )
extern char *const seg_name[];

typedef struct {
	struct exec	header;			/* a.out header */
	long	string_table_size;	/* names + '\0' + sizeof(int) */
} object_headers;

/* unused hooks. */
#define OBJ_EMIT_LINENO(a, b, c)	{;}

#if __STDC__
struct fix;
void tc_aout_fix_to_chars(char *where, struct fix *fixP, relax_addressT segment_address);
#else /* not __STDC__ */
void tc_aout_fix_to_chars();
#endif /* not __STDC__ */

enum reloc_type {
	NO_RELOC, RELOC_32,
};

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of obj-bout.h */
