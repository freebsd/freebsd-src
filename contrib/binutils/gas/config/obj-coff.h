/* coff object file format
   Copyright (C) 1989, 90, 91, 92, 94, 95, 96, 97, 98, 99, 2000
   Free Software Foundation, Inc.

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
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef OBJ_FORMAT_H
#define OBJ_FORMAT_H

#define OBJ_COFF 1

#ifndef BFD_ASSEMBLER

#define WORKING_DOT_WORD
#define WARN_SIGNED_OVERFLOW_WORD
#define OBJ_COFF_OMIT_OPTIONAL_HEADER
#define BFD_HEADERS
#define BFD

#endif

#include "targ-cpu.h"

#include "bfd.h"

/* This internal_lineno crap is to stop namespace pollution from the
   bfd internal coff headerfile.  */
#define internal_lineno bfd_internal_lineno
#include "coff/internal.h"
#undef internal_lineno

/* CPU-specific setup:  */

#ifdef TC_ARM
#include "coff/arm.h"
#ifndef TARGET_FORMAT
#define TARGET_FORMAT "coff-arm"
#endif
#endif

#ifdef TC_PPC
#ifdef TE_PE
#include "coff/powerpc.h"
#else
#include "coff/rs6000.h"
#endif
#endif

#ifdef TC_SPARC
#include "coff/sparc.h"
#endif

#ifdef TC_I386
#include "coff/i386.h"

#ifdef TE_PE
#define TARGET_FORMAT "pe-i386"
#endif

#ifndef TARGET_FORMAT
#define TARGET_FORMAT "coff-i386"
#endif
#endif

#ifdef TC_M68K
#include "coff/m68k.h"
#ifndef TARGET_FORMAT
#define TARGET_FORMAT "coff-m68k"
#endif
#endif

#ifdef TC_A29K
#include "coff/a29k.h"
#define TARGET_FORMAT "coff-a29k-big"
#endif

#ifdef TC_I960
#include "coff/i960.h"
#define TARGET_FORMAT "coff-Intel-little"
#endif

#ifdef TC_Z8K
#include "coff/z8k.h"
#define TARGET_FORMAT "coff-z8k"
#endif

#ifdef TC_H8300
#include "coff/h8300.h"
#define TARGET_FORMAT "coff-h8300"
#endif

#ifdef TC_H8500
#include "coff/h8500.h"
#define TARGET_FORMAT "coff-h8500"
#endif

#ifdef TC_SH

#ifdef TE_PE
#define COFF_WITH_PE
#endif

#include "coff/sh.h"

#ifdef TE_PE
#define TARGET_FORMAT "pe-shl"
#else
#define TARGET_FORMAT					\
  (shl							\
   ? (sh_small ? "coff-shl-small" : "coff-shl")		\
   : (sh_small ? "coff-sh-small" : "coff-sh"))
#endif
#endif

#ifdef TC_MIPS
#define COFF_WITH_PE
#include "coff/mipspe.h"
#undef  TARGET_FORMAT
#define TARGET_FORMAT "pe-mips"
#endif

#ifdef TC_M88K
#include "coff/m88k.h"
#define TARGET_FORMAT "coff-m88kbcs"
#endif

#ifdef TC_W65
#include "coff/w65.h"
#define TARGET_FORMAT "coff-w65"
#endif

#ifdef TC_TIC30
#include "coff/tic30.h"
#define TARGET_FORMAT "coff-tic30"
#endif

#ifdef TC_TIC54X
#include "coff/tic54x.h"
#define TARGET_FORMAT "coff1-c54x"
#endif

#ifdef TC_TIC80
#include "coff/tic80.h"
#define TARGET_FORMAT "coff-tic80"
#define ALIGNMENT_IN_S_FLAGS 1
#endif

#ifdef TC_MCORE
#include "coff/mcore.h"
#ifndef TARGET_FORMAT
#define TARGET_FORMAT "pe-mcore"
#endif
#endif

/* Targets may also set this.  Also, if BFD_ASSEMBLER is defined, this
   will already have been defined.  */
#undef SYMBOLS_NEED_BACKPOINTERS
#define SYMBOLS_NEED_BACKPOINTERS 1

#ifndef OBJ_COFF_MAX_AUXENTRIES
#define OBJ_COFF_MAX_AUXENTRIES 1
#endif /* OBJ_COFF_MAX_AUXENTRIES */

extern void coff_obj_symbol_new_hook PARAMS ((symbolS *));
#define obj_symbol_new_hook coff_obj_symbol_new_hook

extern void coff_obj_read_begin_hook PARAMS ((void));
#define obj_read_begin_hook coff_obj_read_begin_hook

/* ***********************************************************************

   This file really contains two implementations of the COFF back end.
   They are in the process of being merged, but this is only a
   preliminary, mechanical merging.  Many definitions that are
   identical between the two are still found in both versions.

   The first version, with BFD_ASSEMBLER defined, uses high-level BFD
   interfaces and data structures.  The second version, with
   BFD_ASSEMBLER not defined, also uses BFD, but mostly for swapping
   data structures and for doing the actual I/O.  The latter defines
   the preprocessor symbols BFD and BFD_HEADERS.  Try not to let this
   confuse you.

   These two are in the process of being merged, and eventually the
   BFD_ASSEMBLER version should take over completely.  Release timing
   issues and namespace problems convinced me to merge the two
   together in this fashion, a little sooner than I would have liked.
   The real merge should be much better done by the time the next
   release comes out.

   For now, the structure of this file is:
	<common>
	#ifdef BFD_ASSEMBLER
	<one version>
	#else
	<other version>
	#endif
	<common>
   Unfortunately, the common portions are very small at the moment,
   and many declarations or definitions are duplicated.  The structure
   of obj-coff.c is similar.

   See doc/internals.texi for a brief discussion of the history, if
   you care.

   Ken Raeburn, 5 May 1994

   *********************************************************************** */

#ifdef BFD_ASSEMBLER

#include "bfd/libcoff.h"

#define OUTPUT_FLAVOR bfd_target_coff_flavour

/* SYMBOL TABLE */

/* Alter the field names, for now, until we've fixed up the other
   references to use the new name.  */
#ifdef TC_I960
#define TC_SYMFIELD_TYPE	symbolS *
#define sy_tc			bal
#endif

#define OBJ_SYMFIELD_TYPE	unsigned long
#define sy_obj			sy_flags

#define SYM_AUXENT(S) \
  (&coffsymbol (symbol_get_bfdsym (S))->native[1].u.auxent)
#define SYM_AUXINFO(S) \
  (&coffsymbol (symbol_get_bfdsym (S))->native[1])

#define DO_NOT_STRIP	0

extern void obj_coff_section PARAMS ((int));

/* The number of auxiliary entries */
#define S_GET_NUMBER_AUXILIARY(s) \
  (coffsymbol (symbol_get_bfdsym (s))->native->u.syment.n_numaux)
/* The number of auxiliary entries */
#define S_SET_NUMBER_AUXILIARY(s,v)	(S_GET_NUMBER_AUXILIARY (s) = (v))

/* True if a symbol name is in the string table, i.e. its length is > 8.  */
#define S_IS_STRING(s)		(strlen(S_GET_NAME(s)) > 8 ? 1 : 0)

extern int S_SET_DATA_TYPE PARAMS ((symbolS *, int));
extern int S_SET_STORAGE_CLASS PARAMS ((symbolS *, int));
extern int S_GET_STORAGE_CLASS PARAMS ((symbolS *));
extern void SA_SET_SYM_ENDNDX PARAMS ((symbolS *, symbolS *));

/* Auxiliary entry macros. SA_ stands for symbol auxiliary */
/* Omit the tv related fields */
/* Accessors */

#define SA_GET_SYM_TAGNDX(s)	(SYM_AUXENT (s)->x_sym.x_tagndx.l)
#define SA_GET_SYM_LNNO(s)	(SYM_AUXENT (s)->x_sym.x_misc.x_lnsz.x_lnno)
#define SA_GET_SYM_SIZE(s)	(SYM_AUXENT (s)->x_sym.x_misc.x_lnsz.x_size)
#define SA_GET_SYM_FSIZE(s)	(SYM_AUXENT (s)->x_sym.x_misc.x_fsize)
#define SA_GET_SYM_LNNOPTR(s)	(SYM_AUXENT (s)->x_sym.x_fcnary.x_fcn.x_lnnoptr)
#define SA_GET_SYM_ENDNDX(s)	(SYM_AUXENT (s)->x_sym.x_fcnary.x_fcn.x_endndx)
#define SA_GET_SYM_DIMEN(s,i)	(SYM_AUXENT (s)->x_sym.x_fcnary.x_ary.x_dimen[(i)])
#define SA_GET_FILE_FNAME(s)	(SYM_AUXENT (s)->x_file.x_fname)
#define SA_GET_SCN_SCNLEN(s)	(SYM_AUXENT (s)->x_scn.x_scnlen)
#define SA_GET_SCN_NRELOC(s)	(SYM_AUXENT (s)->x_scn.x_nreloc)
#define SA_GET_SCN_NLINNO(s)	(SYM_AUXENT (s)->x_scn.x_nlinno)

#define SA_SET_SYM_LNNO(s,v)	(SYM_AUXENT (s)->x_sym.x_misc.x_lnsz.x_lnno=(v))
#define SA_SET_SYM_SIZE(s,v)	(SYM_AUXENT (s)->x_sym.x_misc.x_lnsz.x_size=(v))
#define SA_SET_SYM_FSIZE(s,v)	(SYM_AUXENT (s)->x_sym.x_misc.x_fsize=(v))
#define SA_SET_SYM_LNNOPTR(s,v)	(SYM_AUXENT (s)->x_sym.x_fcnary.x_fcn.x_lnnoptr=(v))
#define SA_SET_SYM_DIMEN(s,i,v)	(SYM_AUXENT (s)->x_sym.x_fcnary.x_ary.x_dimen[(i)]=(v))
#define SA_SET_FILE_FNAME(s,v)	strncpy(SYM_AUXENT (s)->x_file.x_fname,(v),FILNMLEN)
#define SA_SET_SCN_SCNLEN(s,v)	(SYM_AUXENT (s)->x_scn.x_scnlen=(v))
#define SA_SET_SCN_NRELOC(s,v)	(SYM_AUXENT (s)->x_scn.x_nreloc=(v))
#define SA_SET_SCN_NLINNO(s,v)	(SYM_AUXENT (s)->x_scn.x_nlinno=(v))

/*
 * Internal use only definitions. SF_ stands for symbol flags.
 *
 * These values can be assigned to sy_symbol.ost_flags field of a symbolS.
 *
 * You'll break i960 if you shift the SYSPROC bits anywhere else.  for
 * more on the balname/callname hack, see tc-i960.h.  b.out is done
 * differently.
 */

#define SF_I960_MASK	(0x000001ff)	/* Bits 0-8 are used by the i960 port.  */
#define SF_SYSPROC	(0x0000003f)	/* bits 0-5 are used to store the sysproc number */
#define SF_IS_SYSPROC	(0x00000040)	/* bit 6 marks symbols that are sysprocs */
#define SF_BALNAME	(0x00000080)	/* bit 7 marks BALNAME symbols */
#define SF_CALLNAME	(0x00000100)	/* bit 8 marks CALLNAME symbols */

#define SF_NORMAL_MASK	(0x0000ffff)	/* bits 12-15 are general purpose.  */

#define SF_STATICS	(0x00001000)	/* Mark the .text & all symbols */
#define SF_DEFINED	(0x00002000)	/* Symbol is defined in this file */
#define SF_STRING	(0x00004000)	/* Symbol name length > 8 */
#define SF_LOCAL	(0x00008000)	/* Symbol must not be emitted */

#define SF_DEBUG_MASK	(0xffff0000)	/* bits 16-31 are debug info */

#define SF_FUNCTION	(0x00010000)	/* The symbol is a function */
#define SF_PROCESS	(0x00020000)	/* Process symbol before write */
#define SF_TAGGED	(0x00040000)	/* Is associated with a tag */
#define SF_TAG		(0x00080000)	/* Is a tag */
#define SF_DEBUG	(0x00100000)	/* Is in debug or abs section */
#define SF_GET_SEGMENT	(0x00200000)	/* Get the section of the forward symbol.  */
/* All other bits are unused.  */

/* Accessors */
#define SF_GET(s)		(*symbol_get_obj (s))
#define SF_GET_DEBUG(s)		(symbol_get_bfdsym (s)->flags & BSF_DEBUGGING)
#define SF_SET_DEBUG(s)		(symbol_get_bfdsym (s)->flags |= BSF_DEBUGGING)
#define SF_GET_NORMAL_FIELD(s)	(SF_GET (s) & SF_NORMAL_MASK)
#define SF_GET_DEBUG_FIELD(s)	(SF_GET (s) & SF_DEBUG_MASK)
#define SF_GET_FILE(s)		(SF_GET (s) & SF_FILE)
#define SF_GET_STATICS(s)	(SF_GET (s) & SF_STATICS)
#define SF_GET_DEFINED(s)	(SF_GET (s) & SF_DEFINED)
#define SF_GET_STRING(s)	(SF_GET (s) & SF_STRING)
#define SF_GET_LOCAL(s)		(SF_GET (s) & SF_LOCAL)
#define SF_GET_FUNCTION(s)      (SF_GET (s) & SF_FUNCTION)
#define SF_GET_PROCESS(s)	(SF_GET (s) & SF_PROCESS)
#define SF_GET_TAGGED(s)	(SF_GET (s) & SF_TAGGED)
#define SF_GET_TAG(s)		(SF_GET (s) & SF_TAG)
#define SF_GET_GET_SEGMENT(s)	(SF_GET (s) & SF_GET_SEGMENT)
#define SF_GET_I960(s)		(SF_GET (s) & SF_I960_MASK)	/* used by i960 */
#define SF_GET_BALNAME(s)	(SF_GET (s) & SF_BALNAME)	/* used by i960 */
#define SF_GET_CALLNAME(s)	(SF_GET (s) & SF_CALLNAME)	/* used by i960 */
#define SF_GET_IS_SYSPROC(s)	(SF_GET (s) & SF_IS_SYSPROC)	/* used by i960 */
#define SF_GET_SYSPROC(s)	(SF_GET (s) & SF_SYSPROC)	/* used by i960 */

/* Modifiers */
#define SF_SET(s,v)		(SF_GET (s) = (v))
#define SF_SET_NORMAL_FIELD(s,v) (SF_GET (s) |= ((v) & SF_NORMAL_MASK))
#define SF_SET_DEBUG_FIELD(s,v)	(SF_GET (s) |= ((v) & SF_DEBUG_MASK))
#define SF_SET_FILE(s)		(SF_GET (s) |= SF_FILE)
#define SF_SET_STATICS(s)	(SF_GET (s) |= SF_STATICS)
#define SF_SET_DEFINED(s)	(SF_GET (s) |= SF_DEFINED)
#define SF_SET_STRING(s)	(SF_GET (s) |= SF_STRING)
#define SF_SET_LOCAL(s)		(SF_GET (s) |= SF_LOCAL)
#define SF_CLEAR_LOCAL(s)	(SF_GET (s) &= ~SF_LOCAL)
#define SF_SET_FUNCTION(s)      (SF_GET (s) |= SF_FUNCTION)
#define SF_SET_PROCESS(s)	(SF_GET (s) |= SF_PROCESS)
#define SF_SET_TAGGED(s)	(SF_GET (s) |= SF_TAGGED)
#define SF_SET_TAG(s)		(SF_GET (s) |= SF_TAG)
#define SF_SET_GET_SEGMENT(s)	(SF_GET (s) |= SF_GET_SEGMENT)
#define SF_SET_I960(s,v)	(SF_GET (s) |= ((v) & SF_I960_MASK))	/* used by i960 */
#define SF_SET_BALNAME(s)	(SF_GET (s) |= SF_BALNAME)	/* used by i960 */
#define SF_SET_CALLNAME(s)	(SF_GET (s) |= SF_CALLNAME)	/* used by i960 */
#define SF_SET_IS_SYSPROC(s)	(SF_GET (s) |= SF_IS_SYSPROC)	/* used by i960 */
#define SF_SET_SYSPROC(s,v)	(SF_GET (s) |= ((v) & SF_SYSPROC))	/* used by i960 */

/* --------------  Line number handling ------- */
extern int text_lineno_number;
extern int coff_line_base;
extern int coff_n_line_nos;

#define obj_emit_lineno(WHERE,LINE,FILE_START)	abort ()
extern void coff_add_linesym PARAMS ((symbolS *));

void c_dot_file_symbol PARAMS ((const char *filename));
#define obj_app_file c_dot_file_symbol

extern void coff_frob_symbol PARAMS ((symbolS *, int *));
extern void coff_adjust_symtab PARAMS ((void));
extern void coff_frob_section PARAMS ((segT));
extern void coff_adjust_section_syms PARAMS ((bfd *, asection *, PTR));
extern void coff_frob_file_after_relocs PARAMS ((void));
#define obj_frob_symbol(S,P) 	coff_frob_symbol(S,&P)
#ifndef obj_adjust_symtab
#define obj_adjust_symtab()	coff_adjust_symtab()
#endif
#define obj_frob_section(S)	coff_frob_section (S)
#define obj_frob_file_after_relocs() coff_frob_file_after_relocs ()

extern symbolS *coff_last_function;

/* Forward the segment of a forwarded symbol, handle assignments that
   just copy symbol values, etc.  */
#ifndef OBJ_COPY_SYMBOL_ATTRIBUTES
#ifndef TE_I386AIX
#define OBJ_COPY_SYMBOL_ATTRIBUTES(dest,src) \
  (SF_GET_GET_SEGMENT (dest) \
   ? (S_SET_SEGMENT (dest, S_GET_SEGMENT (src)), 0) \
   : 0)
#else
#define OBJ_COPY_SYMBOL_ATTRIBUTES(dest,src) \
  (SF_GET_GET_SEGMENT (dest) && S_GET_SEGMENT (dest) == SEG_UNKNOWN \
   ? (S_SET_SEGMENT (dest, S_GET_SEGMENT (src)), 0) \
   : 0)
#endif
#endif

/* sanity check */

#ifdef TC_I960
#ifndef C_LEAFSTAT
hey ! Where is the C_LEAFSTAT definition ? i960 - coff support is depending on it.
#endif /* no C_LEAFSTAT */
#endif /* TC_I960 */

#else /* not BFD_ASSEMBLER */

#ifdef TC_A29K
/* Allow translate from aout relocs to coff relocs */
#define NO_RELOC 20
#define RELOC_32 1
#define RELOC_8 2
#define RELOC_CONST 3
#define RELOC_CONSTH 4
#define RELOC_JUMPTARG 5
#define RELOC_BASE22 6
#define RELOC_HI22 7
#define RELOC_LO10 8
#define RELOC_BASE13 9
#define RELOC_WDISP22 10
#define RELOC_WDISP30 11
#endif

extern const segT N_TYPE_seg[];

/* Magic number of paged executable.  */
#define DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE	0x8300

/* SYMBOL TABLE */

/* Symbol table entry data type */

typedef struct
{
  /* Basic symbol */
  struct internal_syment ost_entry;
  /* Auxiliary entry.  */
  union internal_auxent ost_auxent[OBJ_COFF_MAX_AUXENTRIES];
  /* obj_coff internal use only flags */
  unsigned int ost_flags;
} obj_symbol_type;

#ifndef DO_NOT_STRIP
#define DO_NOT_STRIP	0
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
#define C_REGISTER_SECTION	50

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
#define S_IS_DEFINED(s) \
  ((s)->sy_symbol.ost_entry.n_scnum > C_UNDEF_SECTION \
   || ((s)->sy_symbol.ost_entry.n_scnum == C_UNDEF_SECTION \
       && S_GET_VALUE (s) > 0) \
   || ((s)->sy_symbol.ost_entry.n_scnum == C_ABS_SECTION))
/* True if a debug special symbol entry */
#define S_IS_DEBUG(s)		((s)->sy_symbol.ost_entry.n_scnum == C_DEBUG_SECTION)
/* True if a symbol is local symbol name */
/* A symbol name whose name includes ^A is a gas internal pseudo symbol */
#define S_IS_LOCAL(s) \
  ((s)->sy_symbol.ost_entry.n_scnum == C_REGISTER_SECTION \
   || (S_LOCAL_NAME(s) && ! flag_keep_locals && ! S_IS_DEBUG (s)) \
   || strchr (S_GET_NAME (s), '\001') != NULL \
   || strchr (S_GET_NAME (s), '\002') != NULL \
   || (flag_strip_local_absolute \
       && !S_IS_EXTERNAL(s) \
       && (s)->sy_symbol.ost_entry.n_scnum == C_ABS_SECTION))
/* True if a symbol is not defined in this file */
#define S_IS_EXTERN(s)		((s)->sy_symbol.ost_entry.n_scnum == 0 \
				 && S_GET_VALUE (s) == 0)
/*
 * True if a symbol can be multiply defined (bss symbols have this def
 * though it is bad practice)
 */
#define S_IS_COMMON(s)		((s)->sy_symbol.ost_entry.n_scnum == 0 \
				 && S_GET_VALUE (s) != 0)
/* True if a symbol name is in the string table, i.e. its length is > 8.  */
#define S_IS_STRING(s)		(strlen(S_GET_NAME(s)) > 8 ? 1 : 0)

/* True if a symbol is defined as weak.  */
#ifdef TE_PE
#define S_IS_WEAK(s) \
  ((s)->sy_symbol.ost_entry.n_sclass == C_NT_WEAK \
   || (s)->sy_symbol.ost_entry.n_sclass == C_WEAKEXT)
#else
#define S_IS_WEAK(s) \
  ((s)->sy_symbol.ost_entry.n_sclass == C_WEAKEXT)
#endif

/* Accessors */
/* The name of the symbol */
#define S_GET_NAME(s)		((char*) (s)->sy_symbol.ost_entry.n_offset)
/* The pointer to the string table */
#define S_GET_OFFSET(s)         ((s)->sy_symbol.ost_entry.n_offset)
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
#define S_SET_NAME(s,v)		((s)->sy_symbol.ost_entry.n_offset = (unsigned long) (v))
/* Set the offset of the symbol */
#define S_SET_OFFSET(s,v)	((s)->sy_symbol.ost_entry.n_offset = (v))
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
#define SYM_AUXENT(S)	(&(S)->sy_symbol.ost_auxent[0])

#define SA_GET_SYM_TAGNDX(s)	(SYM_AUXENT (s)->x_sym.x_tagndx.l)
#define SA_GET_SYM_LNNO(s)	(SYM_AUXENT (s)->x_sym.x_misc.x_lnsz.x_lnno)
#define SA_GET_SYM_SIZE(s)	(SYM_AUXENT (s)->x_sym.x_misc.x_lnsz.x_size)
#define SA_GET_SYM_FSIZE(s)	(SYM_AUXENT (s)->x_sym.x_misc.x_fsize)
#define SA_GET_SYM_LNNOPTR(s)	(SYM_AUXENT (s)->x_sym.x_fcnary.x_fcn.x_lnnoptr)
#define SA_GET_SYM_ENDNDX(s)	(SYM_AUXENT (s)->x_sym.x_fcnary.x_fcn.x_endndx.l)
#define SA_GET_SYM_DIMEN(s,i)	(SYM_AUXENT (s)->x_sym.x_fcnary.x_ary.x_dimen[(i)])
#define SA_GET_FILE_FNAME(s)	(SYM_AUXENT (s)->x_file.x_fname)
#define SA_GET_FILE_FNAME_OFFSET(s)  (SYM_AUXENT (s)->x_file.x_n.x_offset)
#define SA_GET_FILE_FNAME_ZEROS(s)   (SYM_AUXENT (s)->x_file.x_n.x_zeroes)
#define SA_GET_SCN_SCNLEN(s)	(SYM_AUXENT (s)->x_scn.x_scnlen)
#define SA_GET_SCN_NRELOC(s)	(SYM_AUXENT (s)->x_scn.x_nreloc)
#define SA_GET_SCN_NLINNO(s)	(SYM_AUXENT (s)->x_scn.x_nlinno)

/* Modifiers */
#define SA_SET_SYM_TAGNDX(s,v)	(SYM_AUXENT (s)->x_sym.x_tagndx.l=(v))
#define SA_SET_SYM_LNNO(s,v)	(SYM_AUXENT (s)->x_sym.x_misc.x_lnsz.x_lnno=(v))
#define SA_SET_SYM_SIZE(s,v)	(SYM_AUXENT (s)->x_sym.x_misc.x_lnsz.x_size=(v))
#define SA_SET_SYM_FSIZE(s,v)	(SYM_AUXENT (s)->x_sym.x_misc.x_fsize=(v))
#define SA_SET_SYM_LNNOPTR(s,v)	(SYM_AUXENT (s)->x_sym.x_fcnary.x_fcn.x_lnnoptr=(v))
#define SA_SET_SYM_ENDNDX(s,v)	(SYM_AUXENT (s)->x_sym.x_fcnary.x_fcn.x_endndx.l=(v))
#define SA_SET_SYM_DIMEN(s,i,v)	(SYM_AUXENT (s)->x_sym.x_fcnary.x_ary.x_dimen[(i)]=(v))
#define SA_SET_FILE_FNAME(s,v)	strncpy(SYM_AUXENT (s)->x_file.x_fname,(v),FILNMLEN)
#define SA_SET_FILE_FNAME_OFFSET(s,v) (SYM_AUXENT (s)->x_file.x_n.x_offset=(v))
#define SA_SET_FILE_FNAME_ZEROS(s,v)  (SYM_AUXENT (s)->x_file.x_n.x_zeroes=(v))
#define SA_SET_SCN_SCNLEN(s,v)	(SYM_AUXENT (s)->x_scn.x_scnlen=(v))
#define SA_SET_SCN_NRELOC(s,v)	(SYM_AUXENT (s)->x_scn.x_nreloc=(v))
#define SA_SET_SCN_NLINNO(s,v)	(SYM_AUXENT (s)->x_scn.x_nlinno=(v))

/*
 * Internal use only definitions. SF_ stands for symbol flags.
 *
 * These values can be assigned to sy_symbol.ost_flags field of a symbolS.
 *
 * You'll break i960 if you shift the SYSPROC bits anywhere else.  for
 * more on the balname/callname hack, see tc-i960.h.  b.out is done
 * differently.
 */

#define SF_I960_MASK	(0x000001ff)	/* Bits 0-8 are used by the i960 port.  */
#define SF_SYSPROC	(0x0000003f)	/* bits 0-5 are used to store the sysproc number */
#define SF_IS_SYSPROC	(0x00000040)	/* bit 6 marks symbols that are sysprocs */
#define SF_BALNAME	(0x00000080)	/* bit 7 marks BALNAME symbols */
#define SF_CALLNAME	(0x00000100)	/* bit 8 marks CALLNAME symbols */

#define SF_NORMAL_MASK	(0x0000ffff)	/* bits 12-15 are general purpose.  */

#define SF_STATICS	(0x00001000)	/* Mark the .text & all symbols */
#define SF_DEFINED	(0x00002000)	/* Symbol is defined in this file */
#define SF_STRING	(0x00004000)	/* Symbol name length > 8 */
#define SF_LOCAL	(0x00008000)	/* Symbol must not be emitted */

#define SF_DEBUG_MASK	(0xffff0000)	/* bits 16-31 are debug info */

#define SF_FUNCTION	(0x00010000)	/* The symbol is a function */
#define SF_PROCESS	(0x00020000)	/* Process symbol before write */
#define SF_TAGGED	(0x00040000)	/* Is associated with a tag */
#define SF_TAG		(0x00080000)	/* Is a tag */
#define SF_DEBUG	(0x00100000)	/* Is in debug or abs section */
#define SF_GET_SEGMENT	(0x00200000)	/* Get the section of the forward symbol.  */
#define SF_ADJ_LNNOPTR	(0x00400000)	/* Has a lnnoptr */
/* All other bits are unused.  */

/* Accessors */
#define SF_GET(s)		((s)->sy_symbol.ost_flags)
#define SF_GET_NORMAL_FIELD(s)	(SF_GET (s) & SF_NORMAL_MASK)
#define SF_GET_DEBUG_FIELD(s)	(SF_GET (s) & SF_DEBUG_MASK)
#define SF_GET_FILE(s)		(SF_GET (s) & SF_FILE)
#define SF_GET_STATICS(s)	(SF_GET (s) & SF_STATICS)
#define SF_GET_DEFINED(s)	(SF_GET (s) & SF_DEFINED)
#define SF_GET_STRING(s)	(SF_GET (s) & SF_STRING)
#define SF_GET_LOCAL(s)		(SF_GET (s) & SF_LOCAL)
#define SF_GET_FUNCTION(s)      (SF_GET (s) & SF_FUNCTION)
#define SF_GET_PROCESS(s)	(SF_GET (s) & SF_PROCESS)
#define SF_GET_DEBUG(s)		(SF_GET (s) & SF_DEBUG)
#define SF_GET_TAGGED(s)	(SF_GET (s) & SF_TAGGED)
#define SF_GET_TAG(s)		(SF_GET (s) & SF_TAG)
#define SF_GET_GET_SEGMENT(s)	(SF_GET (s) & SF_GET_SEGMENT)
#define SF_GET_ADJ_LNNOPTR(s)	(SF_GET (s) & SF_ADJ_LNNOPTR)
#define SF_GET_I960(s)		(SF_GET (s) & SF_I960_MASK)	/* used by i960 */
#define SF_GET_BALNAME(s)	(SF_GET (s) & SF_BALNAME)	/* used by i960 */
#define SF_GET_CALLNAME(s)	(SF_GET (s) & SF_CALLNAME)	/* used by i960 */
#define SF_GET_IS_SYSPROC(s)	(SF_GET (s) & SF_IS_SYSPROC)	/* used by i960 */
#define SF_GET_SYSPROC(s)	(SF_GET (s) & SF_SYSPROC)	/* used by i960 */

/* Modifiers */
#define SF_SET(s,v)		(SF_GET (s) = (v))
#define SF_SET_NORMAL_FIELD(s,v) (SF_GET (s) |= ((v) & SF_NORMAL_MASK))
#define SF_SET_DEBUG_FIELD(s,v)	(SF_GET (s) |= ((v) & SF_DEBUG_MASK))
#define SF_SET_FILE(s)		(SF_GET (s) |= SF_FILE)
#define SF_SET_STATICS(s)	(SF_GET (s) |= SF_STATICS)
#define SF_SET_DEFINED(s)	(SF_GET (s) |= SF_DEFINED)
#define SF_SET_STRING(s)	(SF_GET (s) |= SF_STRING)
#define SF_SET_LOCAL(s)		(SF_GET (s) |= SF_LOCAL)
#define SF_CLEAR_LOCAL(s)	(SF_GET (s) &= ~SF_LOCAL)
#define SF_SET_FUNCTION(s)      (SF_GET (s) |= SF_FUNCTION)
#define SF_SET_PROCESS(s)	(SF_GET (s) |= SF_PROCESS)
#define SF_SET_DEBUG(s)		(SF_GET (s) |= SF_DEBUG)
#define SF_SET_TAGGED(s)	(SF_GET (s) |= SF_TAGGED)
#define SF_SET_TAG(s)		(SF_GET (s) |= SF_TAG)
#define SF_SET_GET_SEGMENT(s)	(SF_GET (s) |= SF_GET_SEGMENT)
#define SF_SET_ADJ_LNNOPTR(s)	(SF_GET (s) |= SF_ADJ_LNNOPTR)
#define SF_SET_I960(s,v)	(SF_GET (s) |= ((v) & SF_I960_MASK))	/* used by i960 */
#define SF_SET_BALNAME(s)	(SF_GET (s) |= SF_BALNAME)	/* used by i960 */
#define SF_SET_CALLNAME(s)	(SF_GET (s) |= SF_CALLNAME)	/* used by i960 */
#define SF_SET_IS_SYSPROC(s)	(SF_GET (s) |= SF_IS_SYSPROC)	/* used by i960 */
#define SF_SET_SYSPROC(s,v)	(SF_GET (s) |= ((v) & SF_SYSPROC))	/* used by i960 */

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
    (long) (FILHSZ + OBJ_COFF_AOUTHDRSZ + \
	   H_GET_NUMBER_OF_SECTIONS(h) * SCNHSZ + \
	   H_GET_TEXT_SIZE(h) + H_GET_DATA_SIZE(h) + \
	   H_GET_RELOCATION_SIZE(h) + H_GET_LINENO_SIZE(h) + \
	   H_GET_SYMBOL_TABLE_SIZE(h) + \
	   (h)->string_table_size)
#define H_GET_TEXT_FILE_OFFSET(h) \
    (long) (FILHSZ + OBJ_COFF_AOUTHDRSZ + \
	   H_GET_NUMBER_OF_SECTIONS(h) * SCNHSZ)
#define H_GET_DATA_FILE_OFFSET(h) \
    (long) (FILHSZ + OBJ_COFF_AOUTHDRSZ + \
	   H_GET_NUMBER_OF_SECTIONS(h) * SCNHSZ + \
	   H_GET_TEXT_SIZE(h))
#define H_GET_BSS_FILE_OFFSET(h) 0
#define H_GET_RELOCATION_FILE_OFFSET(h) \
    (long) (FILHSZ + OBJ_COFF_AOUTHDRSZ + \
	   H_GET_NUMBER_OF_SECTIONS(h) * SCNHSZ + \
	   H_GET_TEXT_SIZE(h) + H_GET_DATA_SIZE(h))
#define H_GET_LINENO_FILE_OFFSET(h) \
    (long) (FILHSZ + OBJ_COFF_AOUTHDRSZ + \
	   H_GET_NUMBER_OF_SECTIONS(h) * SCNHSZ + \
	   H_GET_TEXT_SIZE(h) + H_GET_DATA_SIZE(h) + \
	   H_GET_RELOCATION_SIZE(h))
#define H_GET_SYMBOL_TABLE_FILE_OFFSET(h) \
    (long) (FILHSZ + OBJ_COFF_AOUTHDRSZ + \
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
#define H_GET_HEADER_SIZE(h)		(sizeof (FILHDR) \
					 + sizeof (AOUTHDR)\
					 + (H_GET_NUMBER_OF_SECTIONS(h) * SCNHSZ))
#else /* OBJ_COFF_OMIT_OPTIONAL_HEADER */
#define H_GET_HEADER_SIZE(h)		(sizeof (FILHDR) \
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

typedef struct
{
  struct internal_aouthdr aouthdr;	/* a.out header */
  struct internal_filehdr filehdr;	/* File header, not machine dep.  */
  long string_table_size;	/* names + '\0' + sizeof (int) */
  long relocation_size;	/* Cumulated size of relocation
			   information for all sections in
			   bytes.  */
  long lineno_size;		/* Size of the line number information
				   table in bytes */
} object_headers;

struct lineno_list
{
  struct bfd_internal_lineno line;
  char *frag;			/* Frag to which the line number is related */
  struct lineno_list *next;	/* Forward chain pointer */
};

#define obj_segment_name(i) (segment_info[(int) (i)].scnhdr.s_name)

#define obj_add_segment(s) obj_coff_add_segment (s)

extern segT obj_coff_add_segment PARAMS ((const char *));

extern void obj_coff_section PARAMS ((int));

extern void c_dot_file_symbol PARAMS ((char *filename));
#define obj_app_file c_dot_file_symbol
extern void obj_extra_stuff PARAMS ((object_headers * headers));

extern segT s_get_segment PARAMS ((symbolS *ptr));

extern void c_section_header PARAMS ((struct internal_scnhdr * header,
				      char *name,
				      long core_address,
				      long size,
				      long data_ptr,
				      long reloc_ptr,
				      long lineno_ptr,
				      long reloc_number,
				      long lineno_number,
				      long alignment));

#ifndef tc_coff_symbol_emit_hook
void tc_coff_symbol_emit_hook PARAMS ((symbolS *));
#endif

/* sanity check */

#ifdef TC_I960
#ifndef C_LEAFSTAT
hey ! Where is the C_LEAFSTAT definition ? i960 - coff support is depending on it.
#endif /* no C_LEAFSTAT */
#endif /* TC_I960 */
extern struct internal_scnhdr data_section_header;
extern struct internal_scnhdr text_section_header;

/* Forward the segment of a forwarded symbol.  */
#define OBJ_COPY_SYMBOL_ATTRIBUTES(dest,src) \
  (SF_GET_GET_SEGMENT (dest) \
   ? (S_SET_SEGMENT (dest, S_GET_SEGMENT (src)), 0) \
   : 0)

#ifdef TE_PE
#define obj_handle_link_once(t) obj_coff_pe_handle_link_once (t)
extern void obj_coff_pe_handle_link_once ();
#endif

#endif /* not BFD_ASSEMBLER */

extern const pseudo_typeS coff_pseudo_table[];

#ifndef obj_pop_insert
#define obj_pop_insert() pop_insert (coff_pseudo_table)
#endif

/* In COFF, if a symbol is defined using .def/.val SYM/.endef, it's OK
   to redefine the symbol later on.  This can happen if C symbols use
   a prefix, and a symbol is defined both with and without the prefix,
   as in start/_start/__start in gcc/libgcc1-test.c.  */
#define RESOLVE_SYMBOL_REDEFINITION(sym)		\
(SF_GET_GET_SEGMENT (sym)				\
 ? (sym->sy_frag = frag_now,				\
    S_SET_VALUE (sym, frag_now_fix ()),			\
    S_SET_SEGMENT (sym, now_seg),			\
    0)							\
 : 0)

/* Stabs in a coff file go into their own section.  */
#define SEPARATE_STAB_SECTIONS 1

/* We need 12 bytes at the start of the section to hold some initial
   information.  */
extern void obj_coff_init_stab_section PARAMS ((segT));
#define INIT_STAB_SECTION(seg) obj_coff_init_stab_section (seg)

/* Store the number of relocations in the section aux entry.  */
#define SET_SECTION_RELOCS(sec, relocs, n) \
  SA_SET_SCN_NRELOC (section_symbol (sec), n)

#endif /* OBJ_FORMAT_H */
