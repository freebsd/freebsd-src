/* obj-aout.h, a.out object file format for gas, the assembler.
   Copyright (C) 1989, 90, 91, 92, 93, 94, 95, 96, 1998
   Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2,
   or (at your option) any later version.

   GAS is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA. */

/* Tag to validate a.out object file format processing */
#define OBJ_AOUT 1

#include "targ-cpu.h"

#ifdef BFD_ASSEMBLER

#include "bfd/libaout.h"

#define OUTPUT_FLAVOR bfd_target_aout_flavour

#else /* ! BFD_ASSEMBLER */

#ifndef VMS
#include "aout_gnu.h"		/* Needed to define struct nlist. Sigh. */
#else
#include "a_out.h"
#endif

#ifndef AOUT_MACHTYPE
#define AOUT_MACHTYPE 0
#endif /* AOUT_MACHTYPE */

extern const short seg_N_TYPE[];
extern const segT N_TYPE_seg[];

#ifndef DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE
#define DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE	(OMAGIC)
#endif /* DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE */

#endif /* ! BFD_ASSEMBLER */

/* SYMBOL TABLE */
/* Symbol table entry data type */

typedef struct nlist obj_symbol_type;	/* Symbol table entry */

/* Symbol table macros and constants */

#ifdef BFD_ASSEMBLER

#define S_SET_OTHER(S,V)		(aout_symbol((S)->bsym)->other = (V))
#define S_SET_TYPE(S,T)			(aout_symbol((S)->bsym)->type = (T))
#define S_SET_DESC(S,D)			(aout_symbol((S)->bsym)->desc = (D))
#define S_GET_OTHER(S)			(aout_symbol((S)->bsym)->other)
#define S_GET_TYPE(S)			(aout_symbol((S)->bsym)->type)
#define S_GET_DESC(S)			(aout_symbol((S)->bsym)->desc)

asection *text_section, *data_section, *bss_section;

#define obj_frob_symbol(S,PUNT)	obj_aout_frob_symbol (S, &PUNT)
#define obj_frob_file()		obj_aout_frob_file ()
extern void obj_aout_frob_symbol PARAMS ((struct symbol *, int *));
extern void obj_aout_frob_file PARAMS ((void));

#define obj_sec_sym_ok_for_reloc(SEC)	(1)

#else

/* We use the sy_obj field to record whether a symbol is weak.  */
#define OBJ_SYMFIELD_TYPE char

/*
 *  Macros to extract information from a symbol table entry.
 *  This syntaxic indirection allows independence regarding a.out or coff.
 *  The argument (s) of all these macros is a pointer to a symbol table entry.
 */

/* True if the symbol is external */
#define S_IS_EXTERNAL(s)	((s)->sy_symbol.n_type & N_EXT)

/* True if symbol has been defined, ie is in N_{TEXT,DATA,BSS,ABS} or N_EXT */
#define S_IS_DEFINED(s) \
  (S_GET_TYPE (s) != N_UNDF || S_GET_DESC (s) != 0)

#define S_IS_COMMON(s) \
  (S_GET_TYPE (s) == N_UNDF && S_GET_VALUE (s) != 0)

#define S_IS_REGISTER(s)	((s)->sy_symbol.n_type == N_REGISTER)

/* True if a debug special symbol entry */
#define S_IS_DEBUG(s)		((s)->sy_symbol.n_type & N_STAB)
/* True if a symbol is local symbol name */
#define S_IS_LOCAL(s) 					\
  ((S_GET_NAME (s) 					\
    && !S_IS_DEBUG (s) 					\
    && (strchr (S_GET_NAME (s), '\001') != NULL		\
        || strchr (S_GET_NAME (s), '\002') != NULL	\
        || (S_LOCAL_NAME(s) && !flag_keep_locals)))	\
   || (flag_strip_local_absolute			\
       && ! S_IS_EXTERNAL(s)				\
       && S_GET_SEGMENT (s) == absolute_section))
/* True if a symbol is not defined in this file */
#define S_IS_EXTERN(s)		((s)->sy_symbol.n_type & N_EXT)
/* True if the symbol has been generated because of a .stabd directive */
#define S_IS_STABD(s)		(S_GET_NAME(s) == (char *)0)

/* Accessors */
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
/* Whether the symbol is weak.  */
#define S_GET_WEAK(s)		((s)->sy_obj)

/* Modifiers */
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
/* Set the n_type field */
#define S_SET_TYPE(s,t)		((s)->sy_symbol.n_type = (t))
/* Set the n_other expression value */
#define S_SET_OTHER(s,v)	((s)->sy_symbol.n_other = (v))
/* Set the n_desc expression value */
#define S_SET_DESC(s,v)		((s)->sy_symbol.n_desc = (v))
/* Mark the symbol as weak.  This causes n_type to be adjusted when
   the symbol is written out.  */
#define S_SET_WEAK(s)		((s)->sy_obj = 1)

/* File header macro and type definition */

#define H_GET_FILE_SIZE(h)	(H_GET_HEADER_SIZE(h) \
				 + H_GET_TEXT_SIZE(h) \
				 + H_GET_DATA_SIZE(h) \
				 + H_GET_SYMBOL_TABLE_SIZE(h) \
				 + H_GET_TEXT_RELOCATION_SIZE(h) \
				 + H_GET_DATA_RELOCATION_SIZE(h) \
				 + H_GET_STRING_SIZE(h))

#define H_GET_HEADER_SIZE(h)		(EXEC_BYTES_SIZE)
#define H_GET_TEXT_SIZE(h)		((h)->header.a_text)
#define H_GET_DATA_SIZE(h)		((h)->header.a_data)
#define H_GET_BSS_SIZE(h)		((h)->header.a_bss)
#define H_GET_TEXT_RELOCATION_SIZE(h)	((h)->header.a_trsize)
#define H_GET_DATA_RELOCATION_SIZE(h)	((h)->header.a_drsize)
#define H_GET_SYMBOL_TABLE_SIZE(h)	((h)->header.a_syms)
#define H_GET_ENTRY_POINT(h)		((h)->header.a_entry)
#define H_GET_STRING_SIZE(h)		((h)->string_table_size)
#define H_GET_LINENO_SIZE(h)		(0)

#define H_GET_DYNAMIC(h)		((h)->header.a_info >> 31)
#define H_GET_VERSION(h)		(((h)->header.a_info >> 24) & 0x7f)
#define H_GET_MACHTYPE(h)		(((h)->header.a_info >> 16) & 0xff)
#define H_GET_MAGIC_NUMBER(h)		((h)->header.a_info & 0xffff)

#define H_SET_DYNAMIC(h,v)		((h)->header.a_info = (((v) << 31) \
							       | (H_GET_VERSION(h) << 24) \
							       | (H_GET_MACHTYPE(h) << 16) \
							       | (H_GET_MAGIC_NUMBER(h))))

#define H_SET_VERSION(h,v)		((h)->header.a_info = ((H_GET_DYNAMIC(h) << 31) \
							       | ((v) << 24) \
							       | (H_GET_MACHTYPE(h) << 16) \
							       | (H_GET_MAGIC_NUMBER(h))))

#define H_SET_MACHTYPE(h,v)		((h)->header.a_info = ((H_GET_DYNAMIC(h) << 31) \
							       | (H_GET_VERSION(h) << 24) \
							       | ((v) << 16) \
							       | (H_GET_MAGIC_NUMBER(h))))

#define H_SET_MAGIC_NUMBER(h,v)		((h)->header.a_info = ((H_GET_DYNAMIC(h) << 31) \
							       | (H_GET_VERSION(h) << 24) \
							       | (H_GET_MACHTYPE(h) << 16) \
							       | ((v))))

#define H_SET_TEXT_SIZE(h,v)		((h)->header.a_text = md_section_align(SEG_TEXT, (v)))
#define H_SET_DATA_SIZE(h,v)		((h)->header.a_data = md_section_align(SEG_DATA, (v)))
#define H_SET_BSS_SIZE(h,v)		((h)->header.a_bss = md_section_align(SEG_BSS, (v)))

#define H_SET_RELOCATION_SIZE(h,t,d)	(H_SET_TEXT_RELOCATION_SIZE((h),(t)),\
					 H_SET_DATA_RELOCATION_SIZE((h),(d)))

#define H_SET_TEXT_RELOCATION_SIZE(h,v)	((h)->header.a_trsize = (v))
#define H_SET_DATA_RELOCATION_SIZE(h,v)	((h)->header.a_drsize = (v))
#define H_SET_SYMBOL_TABLE_SIZE(h,v)	((h)->header.a_syms = (v) * 12)

#define H_SET_ENTRY_POINT(h,v)		((h)->header.a_entry = (v))
#define H_SET_STRING_SIZE(h,v)		((h)->string_table_size = (v))

typedef struct
  {
    struct exec header;		/* a.out header */
    long string_table_size;	/* names + '\0' + sizeof(int) */
  }

object_headers;

/* line numbering stuff. */
#define OBJ_EMIT_LINENO(a, b, c)	{;}

struct fix;
void tc_aout_fix_to_chars PARAMS ((char *where, struct fix *fixP, relax_addressT segment_address));

#endif

#define obj_symbol_new_hook(s)	{;}

#define EMIT_SECTION_SYMBOLS		0

#define AOUT_STABS

/* end of obj-aout.h */
