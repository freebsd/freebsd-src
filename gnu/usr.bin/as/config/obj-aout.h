/* obj-aout.h, a.out object file format for gas, the assembler.
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
   to the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 $FreeBSD$
 */


/* Tag to validate a.out object file format processing */
#define OBJ_AOUT 1

#include "targ-cpu.h"

#include "aout.h"		/* Needed to define struct nlist. Sigh. */

#ifndef AOUT_MACHTYPE
#define AOUT_MACHTYPE 0
#endif

#ifndef AOUT_VERSION
#define AOUT_VERSION 0
#endif

#ifndef AOUT_FLAGS
#define AOUT_FLAGS 0
#endif

extern const short seg_N_TYPE[];
extern const segT  N_TYPE_seg[];
#define N_REGISTER		0x12	/* Fake register type */

#ifndef DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE
#define DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE	(OMAGIC)
#endif /* DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE */

/* First character of operand in `.type' directives */
#define TYPE_OPERAND_FMT	'@'

/* SYMBOL TABLE */
/* Symbol table entry data type */

typedef struct nlist obj_symbol_type; /* Symbol table entry */

/* Symbol table macros and constants */

/*
 *  Macros to extract information from a symbol table entry.
 *  This syntaxic indirection allows independence regarding a.out or coff.
 *  The argument (s) of all these macros is a pointer to a symbol table entry.
 */

/* True if the symbol is external */
#define S_IS_EXTERNAL(s)	((s)->sy_symbol.n_type & N_EXT)

/* True if symbol has been defined, ie is in N_{TEXT,DATA,BSS,ABS} or N_EXT */
#define S_IS_DEFINED(s)		((S_GET_TYPE(s) != N_UNDF) || (S_GET_OTHER(s) != 0) || (S_GET_DESC(s) != 0))

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
#define S_IS_STABD(s)		(S_GET_NAME(s) == (char *)0)

/* Accessors */
/* The value of the symbol */
#define S_GET_VALUE(s)		(((s)->sy_symbol.n_value))
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

#define H_GET_FILE_SIZE(h)	(H_GET_HEADER_SIZE(h) \
				 + H_GET_TEXT_SIZE(h) \
				 + H_GET_DATA_SIZE(h) \
				 + H_GET_SYMBOL_TABLE_SIZE(h) \
				 + H_GET_TEXT_RELOCATION_SIZE(h) \
				 + H_GET_DATA_RELOCATION_SIZE(h) \
				 + H_GET_STRING_SIZE(h))

#ifndef H_GET_HEADER_SIZE
#define H_GET_HEADER_SIZE(h)		(sizeof(struct exec))
#endif /* not H_GET_HEADER_SIZE */

#define H_GET_TEXT_SIZE(h)		((h)->header.a_text)
#define H_GET_DATA_SIZE(h)		((h)->header.a_data)
#define H_GET_BSS_SIZE(h)		((h)->header.a_bss)
#define H_GET_TEXT_RELOCATION_SIZE(h)	((h)->header.a_trsize)
#define H_GET_DATA_RELOCATION_SIZE(h)	((h)->header.a_drsize)
#define H_GET_SYMBOL_TABLE_SIZE(h)	((h)->header.a_syms)
#define H_GET_ENTRY_POINT(h)		((h)->header.a_entry)
#define H_GET_STRING_SIZE(h)		((h)->string_table_size)
#define H_GET_LINENO_SIZE(h)		(0)

#if defined(FREEBSD_AOUT) || defined(NETBSD_AOUT)

#if defined(FREEBSD_AOUT)
/* duplicate part of <sys/imgact_aout.h> */
#define H_GET_FLAGS(h)						\
	( (((h)->header.a_info)&0xffff)				\
		? ((h)->header.a_info >> 26) & 0x3f )		\
		: 0						\
	)
#define H_GET_MACHTYPE(h)					\
	( (((h)->header.a_info)&0xffff)				\
		? ((h)->header.a_info >>16 ) & 0x3ff)		\
		: 0						\
	)

#define H_GET_MAGIC_NUMBER(h)					\
	( (((h)->header.a_info)&0xffff)				\
		? ((h)->header.a_info & 0xffff)			\
		: (ntohl(((h)->header.a_info))&0xffff)		\
	)

#define H_SET_INFO(h,mag,mid,f,v)				\
	( (h)->header.a_info =					\
	   (((f)&0x3f)<<26) | (((mid)&0x03ff)<<16) | (((mag)&0xffff)) )

#endif /* FREEBSD_AOUT */

#if defined(NETBSD_AOUT)
/* SH*T, duplicate part of <a.out.h> */
#define H_GET_FLAGS(h)						\
	( (((h)->header.a_info)&0xffff0000)			\
		? ((ntohl(((h)->header.a_info))>>26)&0x3f)	\
		: 0						\
	)

#define H_GET_MACHTYPE(h)					\
	( (((h)->header.a_info)&0xffff0000)			\
		? ((ntohl(((h)->header.a_info))>>16)&0x3ff)	\
		: 0						\
	)

#define H_GET_MAGIC_NUMBER(h)					\
	( (((h)->header.a_info)&0xffff0000)			\
		? (ntohl(((h)->header.a_info))&0xffff)		\
		: ((h)->header.a_info & 0xffff)			\
	)

#define H_SET_INFO(h,mag,mid,f,v)				\
	( (h)->header.a_info =					\
	   htonl( (((f)&0x3f)<<26) | (((mid)&0x03ff)<<16) | (((mag)&0xffff)) ) )

#endif /* NETBSD_AOUT */

#define EX_DYNAMIC			0x20
#define EX_PIC				0x10
#undef AOUT_FLAGS
#define AOUT_FLAGS			(picmode ? EX_PIC : 0)

#define H_GET_DYNAMIC(h)		(H_GET_FLAGS(h) & EX_DYNAMIC)

#define H_GET_VERSION(h)		(0)

#define H_SET_DYNAMIC(h,v)					\
	H_SET_INFO(h, H_GET_MAGIC_NUMBER(h), H_GET_MACHTYPE(h),	\
		   (v)?(H_GET_FLAGS(h)|0x20):(H_GET_FLAGS(h)&(~0x20)), 0)

#define H_SET_VERSION(h,v)

#define H_SET_MACHTYPE(h,v)					\
	H_SET_INFO(h, H_GET_MAGIC_NUMBER(h), (v), H_GET_FLAGS(h), 0)

#define H_SET_MAGIC_NUMBER(h,v)					\
	H_SET_INFO(h, (v), H_GET_MACHTYPE(h), H_GET_FLAGS(h), 0)

#else /* !(FREEBSD_AOUT || NETBSD_AOUT) */

#define H_GET_DYNAMIC(h)		((h)->header.a_info >> 31)
#define H_GET_VERSION(h)		(((h)->header.a_info >> 24) & 0x7f)
#define H_GET_MACHTYPE(h)		(((h)->header.a_info >> 16) & 0xff)
#define H_GET_MAGIC_NUMBER(h)		((h)->header.a_info & 0xffff)

#define H_SET_DYNAMIC(h,v)		((h)->header.a_info = \
						(((v) << 31) \
						| (H_GET_VERSION(h) << 24) \
						| (H_GET_MACHTYPE(h) << 16) \
						| (H_GET_MAGIC_NUMBER(h))))

#define H_SET_VERSION(h,v)		((h)->header.a_info = \
						((H_GET_DYNAMIC(h) << 31) \
						| ((v) << 24) \
						| (H_GET_MACHTYPE(h) << 16) \
						| (H_GET_MAGIC_NUMBER(h))))

#define H_SET_MACHTYPE(h,v)		((h)->header.a_info = \
						((H_GET_DYNAMIC(h) << 31) \
						| (H_GET_VERSION(h) << 24) \
						| ((v) << 16) \
						| (H_GET_MAGIC_NUMBER(h))))

#define H_SET_MAGIC_NUMBER(h,v)		((h)->header.a_info = \
						((H_GET_DYNAMIC(h) << 31) \
						| (H_GET_VERSION(h) << 24) \
						| (H_GET_MACHTYPE(h) << 16) \
						| ((v))))
#define H_SET_INFO(h,mag,mid,f,v)	((h)->header.a_info = \
						((((f)==0x20) << 31) \
						| ((v) << 24) \
						| ((mid) << 16) \
						| ((mag))) )
#endif /* FREEBSD_AOUT || NETBSD_AOUT */

#define H_SET_TEXT_SIZE(h,v)		((h)->header.a_text = md_section_align(SEG_TEXT, (v)))
#define H_SET_DATA_SIZE(h,v)		((h)->header.a_data = md_section_align(SEG_DATA, (v)))
#define H_SET_BSS_SIZE(h,v)		((h)->header.a_bss = md_section_align(SEG_BSS, (v)))

#define H_SET_RELOCATION_SIZE(h,t,d)	(H_SET_TEXT_RELOCATION_SIZE((h),(t)),\
					 H_SET_DATA_RELOCATION_SIZE((h),(d)))

#define H_SET_TEXT_RELOCATION_SIZE(h,v)	((h)->header.a_trsize = (v))
#define H_SET_DATA_RELOCATION_SIZE(h,v)	((h)->header.a_drsize = (v))
#define H_SET_SYMBOL_TABLE_SIZE(h,v)	((h)->header.a_syms = (v) * \
					 sizeof(struct nlist))

#define H_SET_ENTRY_POINT(h,v)		((h)->header.a_entry = (v))
#define H_SET_STRING_SIZE(h,v)		((h)->string_table_size = (v))

/*
 * Current means for getting the name of a segment.
 * This will change for infinite-segments support (e.g. COFF).
 */
#define	segment_name(seg)  (seg_name[(int)(seg)])
extern char *const seg_name[];

typedef struct {
	struct exec header;			/* a.out header */
	long string_table_size;	/* names + '\0' + sizeof(int) */
} object_headers;

/* line numbering stuff. */
#define OBJ_EMIT_LINENO(a, b, c)	{;}

#define obj_symbol_new_hook(s)	{;}

#if __STDC__ == 1
struct fix;
void tc_aout_fix_to_chars(char *where, struct fix *fixP, relax_addressT segment_address);
#else /* not __STDC__ */
void tc_aout_fix_to_chars();
#endif /* not __STDC__ */

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of obj-aout.h */
