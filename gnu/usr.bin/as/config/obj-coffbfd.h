/* coff object file format
   Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
   
   This file is part of GAS.
   
   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef OBJ_FORMAT_H
#define OBJ_FORMAT_H

#define OBJ_COFF 1

#include "targ-cpu.h"

#include "bfd.h"

/*extern bfd *stdoutput;*/
/* This internal_lineno crap is to stop namespace pollution from the
   bfd internal coff headerfile. */

#define internal_lineno bfd_internal_lineno
#include "coff/internal.h"
#undef internal_lineno

#if defined(TC_H8300)
#include "coff/h8300.h"
#define TARGET_FORMAT "coff-h8300"
#elif defined(TC_A29K)
#include "coff/a29k.h"
#define TARGET_FORMAT "coff-a29k-big"
#else
help me
#endif
    
#if 0
    /* Define some processor dependent values according to the processor we are
       on. */
#if defined(TC_H8300)
#define BYTE_ORDERING          0
#define FILE_HEADER_MAGIC      H8300MAGIC
#elif defined(TC_M68K)

#define BYTE_ORDERING		F_AR32W    /* See filehdr.h for more info. */
#ifndef FILE_HEADER_MAGIC
#define FILE_HEADER_MAGIC	MC68MAGIC  /* ... */
#endif /* FILE_HEADER_MAGIC */

#elif defined(TC_I386)

#define BYTE_ORDERING		F_AR32WR   /* See filehdr.h for more info. */
#ifndef FILE_HEADER_MAGIC
#define FILE_HEADER_MAGIC	I386MAGIC  /* ... */
#endif /* FILE_HEADER_MAGIC */

#elif defined(TC_I960)

#define BYTE_ORDERING		F_AR32WR   /* See filehdr.h for more info. */
#ifndef FILE_HEADER_MAGIC
#define FILE_HEADER_MAGIC	I960ROMAGIC  /* ... */
#endif /* FILE_HEADER_MAGIC */

#elif defined(TC_A29K)

#define BYTE_ORDERING		F_AR32W	/* big endian. */
#ifndef FILE_HEADER_MAGIC
#define FILE_HEADER_MAGIC	SIPFBOMAGIC
#endif /* FILE_HEADER_MAGIC */

#else
you lose
#endif 
    
#endif
    
#ifndef OBJ_COFF_MAX_AUXENTRIES
#define OBJ_COFF_MAX_AUXENTRIES 1
#endif /* OBJ_COFF_MAX_AUXENTRIES */
    
    
    extern const segT  N_TYPE_seg[];

/* Magic number of paged executable. */
#define DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE	0x8300


/* SYMBOL TABLE */

/* targets may also set this */
#ifndef SYMBOLS_NEED_BACKPOINTERS
#define SYMBOLS_NEED_BACKPOINTERS 1
#endif /* SYMBOLS_NEED_BACKPOINTERS */

/* Symbol table entry data type */

typedef struct 
{
	struct internal_syment  ost_entry; /* Basic symbol */
	union internal_auxent ost_auxent[OBJ_COFF_MAX_AUXENTRIES]; /* Auxiliary entry. */
	
	unsigned int ost_flags; /* obj_coff internal use only flags */
} obj_symbol_type;

#ifndef DO_NOT_STRIP
#define DO_NOT_STRIP	0
#define DO_STRIP	1
#endif
/* Symbol table macros and constants */

/* Possible and usefull section number in symbol table 
 * The values of TEXT, DATA and BSS may not be portable.
 */

#define C_ABS_SECTION		N_ABS
#define C_UNDEF_SECTION		N_UNDEF
#define C_DEBUG_SECTION		N_DEBUG
#define C_NTV_SECTION		N_TV
#define C_PTV_SECTION		P_TV
#define C_REGISTER_SECTION	20

/*
 *  Macros to extract information from a symbol table entry.
 *  This syntaxic indirection allows independence regarding a.out or coff.
 *  The argument (s) of all these macros is a pointer to a symbol table entry.
 */

/* Predicates */
/* True if the symbol is external */
#define S_IS_EXTERNAL(s)        ((s)->sy_symbol.ost_entry.n_scnum == C_UNDEF_SECTION)
/* True if symbol has been defined, ie :
   section > 0 (DATA, TEXT or BSS)
   section == 0 and value > 0 (external bss symbol) */
#define S_IS_DEFINED(s)         ((s)->sy_symbol.ost_entry.n_scnum > C_UNDEF_SECTION || \
				 ((s)->sy_symbol.ost_entry.n_scnum == C_UNDEF_SECTION && \
				  (s)->sy_symbol.ost_entry.n_value > 0))
/* True if a debug special symbol entry */
#define S_IS_DEBUG(s)		((s)->sy_symbol.ost_entry.n_scnum == C_DEBUG_SECTION)
/* True if a symbol is local symbol name */
/* A symbol name whose name begin with ^A is a gas internal pseudo symbol */
#define S_IS_LOCAL(s)		(S_GET_NAME(s)[0] == '\001' || \
				 (s)->sy_symbol.ost_entry.n_scnum == C_REGISTER_SECTION || \
				 (S_LOCAL_NAME(s) && !flagseen['L']))
/* True if a symbol is not defined in this file */
#define S_IS_EXTERN(s)		((s)->sy_symbol.ost_entry.n_scnum == 0 && (s)->sy_symbol.ost_entry.n_value == 0)
/*
 * True if a symbol can be multiply defined (bss symbols have this def
 * though it is bad practice)
 */
#define S_IS_COMMON(s)		((s)->sy_symbol.ost_entry.n_scnum == 0 && (s)->sy_symbol.ost_entry.n_value != 0)
/* True if a symbol name is in the string table, i.e. its length is > 8. */
#define S_IS_STRING(s)		(strlen(S_GET_NAME(s)) > 8 ? 1 : 0)

/* Accessors */
/* The name of the symbol */
#define S_GET_NAME(s)		((char*)(s)->sy_symbol.ost_entry.n_offset)
/* The pointer to the string table */
#define S_GET_OFFSET(s)         ((s)->sy_symbol.ost_entry.n_offset)
/* The zeroes if symbol name is longer than 8 chars */
#define S_GET_ZEROES(s)		((s)->sy_symbol.ost_entry.n_zeroes)
/* The value of the symbol */
#define S_GET_VALUE(s)		((unsigned) ((s)->sy_symbol.ost_entry.n_value))	
/* The numeric value of the segment */
#define S_GET_SEGMENT(s)   s_get_segment(s)
/* The data type */
#define S_GET_DATA_TYPE(s)	((s)->sy_symbol.ost_entry.n_type)
/* The storage class */
#define S_GET_STORAGE_CLASS(s)	((s)->sy_symbol.ost_entry.n_sclass)
/* The number of auxiliary entries */
#define S_GET_NUMBER_AUXILIARY(s)	((s)->sy_symbol.ost_entry.n_numaux)

/* Modifiers */
/* Set the name of the symbol */
#define S_SET_NAME(s,v)		((s)->sy_symbol.ost_entry.n_offset = (unsigned long)(v))
/* Set the offset of the symbol */
#define S_SET_OFFSET(s,v)	((s)->sy_symbol.ost_entry.n_offset = (v))
/* The zeroes if symbol name is longer than 8 chars */
#define S_SET_ZEROES(s,v)		((s)->sy_symbol.ost_entry.n_zeroes = (v))
/* Set the value of the symbol */
#define S_SET_VALUE(s,v)	((s)->sy_symbol.ost_entry.n_value = (v))
/* The numeric value of the segment */
#define S_SET_SEGMENT(s,v)	((s)->sy_symbol.ost_entry.n_scnum = SEGMENT_TO_SYMBOL_TYPE(v))
/* The data type */
#define S_SET_DATA_TYPE(s,v)	((s)->sy_symbol.ost_entry.n_type = (v))
/* The storage class */
#define S_SET_STORAGE_CLASS(s,v)	((s)->sy_symbol.ost_entry.n_sclass = (v))
/* The number of auxiliary entries */
#define S_SET_NUMBER_AUXILIARY(s,v)	((s)->sy_symbol.ost_entry.n_numaux = (v))

/* Additional modifiers */
/* The symbol is external (does not mean undefined) */
#define S_SET_EXTERNAL(s)       { S_SET_STORAGE_CLASS(s, C_EXT) ; SF_CLEAR_LOCAL(s); }

/* Auxiliary entry macros. SA_ stands for symbol auxiliary */
/* Omit the tv related fields */
/* Accessors */
#ifdef BFD_HEADERS
#define SA_GET_SYM_TAGNDX(s)	((s)->sy_symbol.ost_auxent[0].x_sym.x_tagndx.l)
#else
#define SA_GET_SYM_TAGNDX(s)	((s)->sy_symbol.ost_auxent[0].x_sym.x_tagndx)
#endif
#define SA_GET_SYM_LNNO(s)	((s)->sy_symbol.ost_auxent[0].x_sym.x_misc.x_lnsz.x_lnno)
#define SA_GET_SYM_SIZE(s)	((s)->sy_symbol.ost_auxent[0].x_sym.x_misc.x_lnsz.x_size)
#define SA_GET_SYM_FSIZE(s)	((s)->sy_symbol.ost_auxent[0].x_sym.x_misc.x_fsize)
#define SA_GET_SYM_LNNOPTR(s)	((s)->sy_symbol.ost_auxent[0].x_sym.x_fcnary.x_fcn.x_lnnoptr)
#ifdef BFD_HEADERS
#define SA_GET_SYM_ENDNDX(s)	((s)->sy_symbol.ost_auxent[0].x_sym.x_fcnary.x_fcn.x_endndx.l)
#else
#define SA_GET_SYM_ENDNDX(s)	((s)->sy_symbol.ost_auxent[0].x_sym.x_fcnary.x_fcn.x_endndx)
#endif
#define SA_GET_SYM_DIMEN(s,i)	((s)->sy_symbol.ost_auxent[0].x_sym.x_fcnary.x_ary.x_dimen[(i)])
#define SA_GET_FILE_FNAME(s)	((s)->sy_symbol.ost_auxent[0].x_file.x_fname)
#define SA_GET_SCN_SCNLEN(s)	((s)->sy_symbol.ost_auxent[0].x_scn.x_scnlen)
#define SA_GET_SCN_NRELOC(s)	((s)->sy_symbol.ost_auxent[0].x_scn.x_nreloc)
#define SA_GET_SCN_NLINNO(s)	((s)->sy_symbol.ost_auxent[0].x_scn.x_nlinno)

/* Modifiers */
#ifdef BFD_HEADERS
#define SA_SET_SYM_TAGNDX(s,v)	((s)->sy_symbol.ost_auxent[0].x_sym.x_tagndx.l=(v))
#else
#define SA_SET_SYM_TAGNDX(s,v)	((s)->sy_symbol.ost_auxent[0].x_sym.x_tagndx=(v))
#endif
#define SA_SET_SYM_LNNO(s,v)	((s)->sy_symbol.ost_auxent[0].x_sym.x_misc.x_lnsz.x_lnno=(v))
#define SA_SET_SYM_SIZE(s,v)	((s)->sy_symbol.ost_auxent[0].x_sym.x_misc.x_lnsz.x_size=(v))
#define SA_SET_SYM_FSIZE(s,v)	((s)->sy_symbol.ost_auxent[0].x_sym.x_misc.x_fsize=(v))
#define SA_SET_SYM_LNNOPTR(s,v)	((s)->sy_symbol.ost_auxent[0].x_sym.x_fcnary.x_fcn.x_lnnoptr=(v))
#ifdef BFD_HEADERS
#define SA_SET_SYM_ENDNDX(s,v)	((s)->sy_symbol.ost_auxent[0].x_sym.x_fcnary.x_fcn.x_endndx.l=(v))
#else
#define SA_SET_SYM_ENDNDX(s,v)	((s)->sy_symbol.ost_auxent[0].x_sym.x_fcnary.x_fcn.x_endndx=(v))
#endif
#define SA_SET_SYM_DIMEN(s,i,v)	((s)->sy_symbol.ost_auxent[0].x_sym.x_fcnary.x_ary.x_dimen[(i)]=(v))
#define SA_SET_FILE_FNAME(s,v)	strncpy((s)->sy_symbol.ost_auxent[0].x_file.x_fname,(v),FILNMLEN)
#define SA_SET_SCN_SCNLEN(s,v)	((s)->sy_symbol.ost_auxent[0].x_scn.x_scnlen=(v))
#define SA_SET_SCN_NRELOC(s,v)	((s)->sy_symbol.ost_auxent[0].x_scn.x_nreloc=(v))
#define SA_SET_SCN_NLINNO(s,v)	((s)->sy_symbol.ost_auxent[0].x_scn.x_nlinno=(v))

/*
 * Internal use only definitions. SF_ stands for symbol flags.
 *
 * These values can be assigned to sy_symbol.ost_flags field of a symbolS.
 *
 * You'll break i960 if you shift the SYSPROC bits anywhere else.  for
 * more on the balname/callname hack, see tc-i960.h.  b.out is done
 * differently.
 */

#define SF_I960_MASK	(0x000001ff) /* Bits 0-8 are used by the i960 port. */
#define SF_SYSPROC	(0x0000003f) 	     /* bits 0-5 are used to store the sysproc number */
#define SF_IS_SYSPROC	(0x00000040)	     /* bit 6 marks symbols that are sysprocs */
#define SF_BALNAME	(0x00000080)	     /* bit 7 marks BALNAME symbols */
#define SF_CALLNAME	(0x00000100)	     /* bit 8 marks CALLNAME symbols */

#define SF_NORMAL_MASK	(0x0000ffff) /* bits 12-15 are general purpose. */

#define SF_STATICS	(0x00001000)	     /* Mark the .text & all symbols */
#define SF_DEFINED	(0x00002000)	     /* Symbol is defined in this file */
#define SF_STRING	(0x00004000)	     /* Symbol name length > 8 */
#define SF_LOCAL	(0x00008000)	     /* Symbol must not be emitted */

#define SF_DEBUG_MASK	(0xffff0000) /* bits 16-31 are debug info */

#define SF_FUNCTION	(0x00010000)	     /* The symbol is a function */
#define SF_PROCESS	(0x00020000)	     /* Process symbol before write */
#define SF_TAGGED	(0x00040000)	     /* Is associated with a tag */
#define SF_TAG		(0x00080000)	     /* Is a tag */
#define SF_DEBUG	(0x00100000)	     /* Is in debug or abs section */
#define SF_GET_SEGMENT	(0x00200000)	     /* Get the section of the forward symbol. */
/* All other bits are unused. */

/* Accessors */
#define SF_GET(s)		((s)->sy_symbol.ost_flags)
#define SF_GET_NORMAL_FIELD(s)	((s)->sy_symbol.ost_flags & SF_NORMAL_MASK)
#define SF_GET_DEBUG_FIELD(s)	((s)->sy_symbol.ost_flags & SF_DEBUG_MASK)
#define SF_GET_FILE(s)		((s)->sy_symbol.ost_flags & SF_FILE)
#define SF_GET_STATICS(s)	((s)->sy_symbol.ost_flags & SF_STATICS)
#define SF_GET_DEFINED(s)	((s)->sy_symbol.ost_flags & SF_DEFINED)
#define SF_GET_STRING(s)	((s)->sy_symbol.ost_flags & SF_STRING)
#define SF_GET_LOCAL(s)		((s)->sy_symbol.ost_flags & SF_LOCAL)
#define SF_GET_FUNCTION(s)      ((s)->sy_symbol.ost_flags & SF_FUNCTION)
#define SF_GET_PROCESS(s)	((s)->sy_symbol.ost_flags & SF_PROCESS)
#define SF_GET_DEBUG(s)		((s)->sy_symbol.ost_flags & SF_DEBUG)
#define SF_GET_TAGGED(s)	((s)->sy_symbol.ost_flags & SF_TAGGED)
#define SF_GET_TAG(s)		((s)->sy_symbol.ost_flags & SF_TAG)
#define SF_GET_GET_SEGMENT(s)	((s)->sy_symbol.ost_flags & SF_GET_SEGMENT)
#define SF_GET_I960(s)		((s)->sy_symbol.ost_flags & SF_I960_MASK) /* used by i960 */
#define SF_GET_BALNAME(s)	((s)->sy_symbol.ost_flags & SF_BALNAME) /* used by i960 */
#define SF_GET_CALLNAME(s)	((s)->sy_symbol.ost_flags & SF_CALLNAME) /* used by i960 */
#define SF_GET_IS_SYSPROC(s)	((s)->sy_symbol.ost_flags & SF_IS_SYSPROC) /* used by i960 */
#define SF_GET_SYSPROC(s)	((s)->sy_symbol.ost_flags & SF_SYSPROC) /* used by i960 */

/* Modifiers */
#define SF_SET(s,v)		((s)->sy_symbol.ost_flags = (v))
#define SF_SET_NORMAL_FIELD(s,v)((s)->sy_symbol.ost_flags |= ((v) & SF_NORMAL_MASK))
#define SF_SET_DEBUG_FIELD(s,v)	((s)->sy_symbol.ost_flags |= ((v) & SF_DEBUG_MASK))
#define SF_SET_FILE(s)		((s)->sy_symbol.ost_flags |= SF_FILE)
#define SF_SET_STATICS(s)	((s)->sy_symbol.ost_flags |= SF_STATICS)
#define SF_SET_DEFINED(s)	((s)->sy_symbol.ost_flags |= SF_DEFINED)
#define SF_SET_STRING(s)	((s)->sy_symbol.ost_flags |= SF_STRING)
#define SF_SET_LOCAL(s)		((s)->sy_symbol.ost_flags |= SF_LOCAL)
#define SF_CLEAR_LOCAL(s)	((s)->sy_symbol.ost_flags &= ~SF_LOCAL)
#define SF_SET_FUNCTION(s)      ((s)->sy_symbol.ost_flags |= SF_FUNCTION)
#define SF_SET_PROCESS(s)	((s)->sy_symbol.ost_flags |= SF_PROCESS)
#define SF_SET_DEBUG(s)		((s)->sy_symbol.ost_flags |= SF_DEBUG)
#define SF_SET_TAGGED(s)	((s)->sy_symbol.ost_flags |= SF_TAGGED)
#define SF_SET_TAG(s)		((s)->sy_symbol.ost_flags |= SF_TAG)
#define SF_SET_GET_SEGMENT(s)	((s)->sy_symbol.ost_flags |= SF_GET_SEGMENT)
#define SF_SET_I960(s,v)	((s)->sy_symbol.ost_flags |= ((v) & SF_I960_MASK)) /* used by i960 */
#define SF_SET_BALNAME(s)	((s)->sy_symbol.ost_flags |= SF_BALNAME) /* used by i960 */
#define SF_SET_CALLNAME(s)	((s)->sy_symbol.ost_flags |= SF_CALLNAME) /* used by i960 */
#define SF_SET_IS_SYSPROC(s)	((s)->sy_symbol.ost_flags |= SF_IS_SYSPROC) /* used by i960 */
#define SF_SET_SYSPROC(s,v)	((s)->sy_symbol.ost_flags |= ((v) & SF_SYSPROC)) /* used by i960 */

/* File header macro and type definition */

/*
 * File position calculators. Beware to use them when all the
 * appropriate fields are set in the header.
 */

#ifdef OBJ_COFF_OMIT_OPTIONAL_HEADER
#define OBJ_COFF_AOUTHDRSZ (0)
#else
#define OBJ_COFF_AOUTHDRSZ (AOUTHDRSZ)
#endif /* OBJ_COFF_OMIT_OPTIONAL_HEADER */

#define H_GET_FILE_SIZE(h) \
    (long)(FILHSZ + OBJ_COFF_AOUTHDRSZ + \
	   H_GET_NUMBER_OF_SECTIONS(h) * SCNHSZ + \
	   H_GET_TEXT_SIZE(h) + H_GET_DATA_SIZE(h) + \
	   H_GET_RELOCATION_SIZE(h) + H_GET_LINENO_SIZE(h) + \
	   H_GET_SYMBOL_TABLE_SIZE(h) + \
	   (h)->string_table_size)
#define H_GET_TEXT_FILE_OFFSET(h) \
    (long)(FILHSZ + OBJ_COFF_AOUTHDRSZ + \
	   H_GET_NUMBER_OF_SECTIONS(h) * SCNHSZ)
#define H_GET_DATA_FILE_OFFSET(h) \
    (long)(FILHSZ + OBJ_COFF_AOUTHDRSZ + \
	   H_GET_NUMBER_OF_SECTIONS(h) * SCNHSZ + \
	   H_GET_TEXT_SIZE(h))
#define H_GET_BSS_FILE_OFFSET(h) 0
#define H_GET_RELOCATION_FILE_OFFSET(h) \
    (long)(FILHSZ + OBJ_COFF_AOUTHDRSZ + \
	   H_GET_NUMBER_OF_SECTIONS(h) * SCNHSZ + \
	   H_GET_TEXT_SIZE(h) + H_GET_DATA_SIZE(h))
#define H_GET_LINENO_FILE_OFFSET(h) \
    (long)(FILHSZ + OBJ_COFF_AOUTHDRSZ + \
	   H_GET_NUMBER_OF_SECTIONS(h) * SCNHSZ + \
	   H_GET_TEXT_SIZE(h) + H_GET_DATA_SIZE(h) + \
	   H_GET_RELOCATION_SIZE(h))
#define H_GET_SYMBOL_TABLE_FILE_OFFSET(h) \
    (long)(FILHSZ + OBJ_COFF_AOUTHDRSZ + \
	   H_GET_NUMBER_OF_SECTIONS(h) * SCNHSZ + \
	   H_GET_TEXT_SIZE(h) + H_GET_DATA_SIZE(h) + \
	   H_GET_RELOCATION_SIZE(h) + H_GET_LINENO_SIZE(h))

/* Accessors */
/* aouthdr */
#define H_GET_MAGIC_NUMBER(h)           ((h)->aouthdr.magic)
#define H_GET_VERSION_STAMP(h)		((h)->aouthdr.vstamp)
#define H_GET_TEXT_SIZE(h)              ((h)->aouthdr.tsize)
#define H_GET_DATA_SIZE(h)              ((h)->aouthdr.dsize)
#define H_GET_BSS_SIZE(h)               ((h)->aouthdr.bsize)
#define H_GET_ENTRY_POINT(h)            ((h)->aouthdr.entry)
#define H_GET_TEXT_START(h)		((h)->aouthdr.text_start)
#define H_GET_DATA_START(h)		((h)->aouthdr.data_start)
/* filehdr */
#define H_GET_FILE_MAGIC_NUMBER(h)	((h)->filehdr.f_magic)
#define H_GET_NUMBER_OF_SECTIONS(h)	((h)->filehdr.f_nscns)
#define H_GET_TIME_STAMP(h)		((h)->filehdr.f_timdat)
#define H_GET_SYMBOL_TABLE_POINTER(h)	((h)->filehdr.f_symptr)
#define H_GET_SYMBOL_COUNT(h)		((h)->filehdr.f_nsyms)
#define H_GET_SYMBOL_TABLE_SIZE(h)	(H_GET_SYMBOL_COUNT(h) * SYMESZ)
#define H_GET_SIZEOF_OPTIONAL_HEADER(h)	((h)->filehdr.f_opthdr)
#define H_GET_FLAGS(h)			((h)->filehdr.f_flags)
/* Extra fields to achieve bsd a.out compatibility and for convenience */
#define H_GET_RELOCATION_SIZE(h)   	((h)->relocation_size)
#define H_GET_STRING_SIZE(h)            ((h)->string_table_size)
#define H_GET_LINENO_SIZE(h)            ((h)->lineno_size)

#ifndef OBJ_COFF_OMIT_OPTIONAL_HEADER
#define H_GET_HEADER_SIZE(h)		(sizeof(FILHDR) \
					 + sizeof(AOUTHDR)\
					 + (H_GET_NUMBER_OF_SECTIONS(h) * SCNHSZ))
#else /* OBJ_COFF_OMIT_OPTIONAL_HEADER */
#define H_GET_HEADER_SIZE(h)		(sizeof(FILHDR) \
					 + (H_GET_NUMBER_OF_SECTIONS(h) * SCNHSZ))
#endif /* OBJ_COFF_OMIT_OPTIONAL_HEADER */

#define H_GET_TEXT_RELOCATION_SIZE(h)	(text_section_header.s_nreloc * RELSZ)
#define H_GET_DATA_RELOCATION_SIZE(h)	(data_section_header.s_nreloc * RELSZ)

/* Modifiers */
/* aouthdr */
#define H_SET_MAGIC_NUMBER(h,v)         ((h)->aouthdr.magic = (v))
#define H_SET_VERSION_STAMP(h,v)	((h)->aouthdr.vstamp = (v))
#define H_SET_TEXT_SIZE(h,v)            ((h)->aouthdr.tsize = (v))
#define H_SET_DATA_SIZE(h,v)            ((h)->aouthdr.dsize = (v))
#define H_SET_BSS_SIZE(h,v)             ((h)->aouthdr.bsize = (v))
#define H_SET_ENTRY_POINT(h,v)          ((h)->aouthdr.entry = (v))
#define H_SET_TEXT_START(h,v)		((h)->aouthdr.text_start = (v))
#define H_SET_DATA_START(h,v)		((h)->aouthdr.data_start = (v))
/* filehdr */
#define H_SET_FILE_MAGIC_NUMBER(h,v)	((h)->filehdr.f_magic = (v))
#define H_SET_NUMBER_OF_SECTIONS(h,v)	((h)->filehdr.f_nscns = (v))
#define H_SET_TIME_STAMP(h,v)		((h)->filehdr.f_timdat = (v))
#define H_SET_SYMBOL_TABLE_POINTER(h,v)	((h)->filehdr.f_symptr = (v))
#define H_SET_SYMBOL_TABLE_SIZE(h,v)    ((h)->filehdr.f_nsyms = (v))
#define H_SET_SIZEOF_OPTIONAL_HEADER(h,v) ((h)->filehdr.f_opthdr = (v))
#define H_SET_FLAGS(h,v)		((h)->filehdr.f_flags = (v))
/* Extra fields to achieve bsd a.out compatibility and for convinience */
#define H_SET_RELOCATION_SIZE(h,t,d) 	((h)->relocation_size = (t)+(d))
#define H_SET_STRING_SIZE(h,v)          ((h)->string_table_size = (v))
#define H_SET_LINENO_SIZE(h,v)          ((h)->lineno_size = (v))

/* Segment flipping */
#define segment_name(v)	(seg_name[(int) (v)])

typedef struct {
#ifdef BFD_HEADERS
	struct internal_aouthdr	   aouthdr;             /* a.out header */
	struct internal_filehdr	   filehdr;		/* File header, not machine dep. */
#else
	AOUTHDR	   aouthdr;             /* a.out header */
	FILHDR	   filehdr;		/* File header, not machine dep. */
#endif
	long       string_table_size;   /* names + '\0' + sizeof(int) */
	long	   relocation_size;	/* Cumulated size of relocation
					   information for all sections in
					   bytes. */
	long	   lineno_size;		/* Size of the line number information
					   table in bytes */
} object_headers;



struct lineno_list
{
	
	struct bfd_internal_lineno line;
	char* frag;			/* Frag to which the line number is related */
	struct lineno_list* next;	/* Forward chain pointer */
} ;




/* stack stuff */
typedef struct {
	unsigned long chunk_size;
	unsigned long element_size;
	unsigned long size;
	char*	  data;
	unsigned long pointer;
} stack;



char *EXFUN(stack_pop,(stack *st));
char *EXFUN(stack_push,(stack *st, char *element));
char *EXFUN(stack_top,(stack *st));
stack *EXFUN(stack_init,(unsigned long chunk_size, unsigned long element_size));
void EXFUN(c_dot_file_symbol,(char *filename));
void EXFUN(obj_extra_stuff,(object_headers *headers));
void EXFUN(stack_delete,(stack *st));



void EXFUN(c_section_header,(
			     
			     struct internal_scnhdr *header,
			     char *name,
			     long core_address,
			     long size,
			     long data_ptr,
			     long reloc_ptr,
			     long lineno_ptr,
			     long reloc_number,
			     long lineno_number,
			     long alignment));


/* sanity check */

#ifdef TC_I960
#ifndef C_LEAFSTAT
hey!  Where is the C_LEAFSTAT definition?  i960-coff support is depending on it.
#endif /* no C_LEAFSTAT */
#endif /* TC_I960 */
#ifdef BFD_HEADERS
    extern struct internal_scnhdr data_section_header;
extern struct internal_scnhdr text_section_header;
#else
extern SCNHDR data_section_header;
extern SCNHDR text_section_header;
#endif
#endif

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of obj-coffbfd.h */
