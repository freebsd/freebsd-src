/* VMS object file format
   Copyright 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 2000,
   2002, 2003 Free Software Foundation, Inc.

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
02111-1307, USA.  */

/* Tag to validate a.out object file format processing */
#define OBJ_VMS 1

#include "targ-cpu.h"

#define LONGWORD_ALIGNMENT	2

/* This macro controls subsection alignment within a section.
 *
 * Under VAX/VMS, the linker (and PSECT specifications)
 * take care of correctly aligning the segments.
 * Doing the alignment here (on initialized data) can
 * mess up the calculation of global data PSECT sizes.
 */
#define SUB_SEGMENT_ALIGN(SEG, FRCHAIN)	\
  (((SEG) == data_section) ? 0 : LONGWORD_ALIGNMENT)

/* This flag is used to remember whether we are in the const or the
   data section.  By and large they are identical, but we set a no-write
   bit for psects in the const section.  */

extern unsigned char const_flag;

/* This is overloaded onto const_flag, for convenience.  It's used to flag
   dummy labels like "gcc2_compiled."  which occur before the first .text
   or .data section directive.  */

#define IN_DEFAULT_SECTION 0x80

/* These are defined in obj-vms.c.  */
extern const short seg_N_TYPE[];
extern const segT N_TYPE_seg[];

#undef NO_RELOC
enum reloc_type
  {
    NO_RELOC, RELOC_32
  };

#define N_BADMAG(x)	(0)
#define N_TXTOFF(x)	( sizeof (struct exec) )
#define N_DATOFF(x)	( N_TXTOFF(x) + (x).a_text )
#define N_TROFF(x)	( N_DATOFF(x) + (x).a_data )
#define N_DROFF(x)	( N_TROFF(x) + (x).a_trsize )
#define N_SYMOFF(x)	( N_DROFF(x) + (x).a_drsize )
#define N_STROFF(x)	( N_SYMOFF(x) + (x).a_syms )

/* We use this copy of the exec header for VMS.  We do not actually use it, but
   what we actually do is let gas fill in the relevant slots, and when we get
   around to writing an obj file, we just pick out what we need.  */

struct exec
{
  unsigned long a_text;		/* length of text, in bytes */
  unsigned long a_data;		/* length of data, in bytes */
  unsigned long a_bss;		/* length of uninitialized data area for file, in bytes */
  unsigned long a_trsize;	/* length of relocation info for text, in bytes */
  unsigned long a_drsize;	/* length of relocation info for data, in bytes */
  unsigned long a_entry;	/* start address */
  unsigned long a_syms;		/* length of symbol table data in file, in bytes */
};

typedef struct
  {
    struct exec header;		/* a.out header */
    long string_table_size;	/* names + '\0' + sizeof (int) */
  }
object_headers;

/* A single entry in the symbol table
 * (this started as a clone of bout.h's nlist, but much was unneeded).
 */
struct nlist
  {
    char *n_name;
    unsigned char n_type;	/* See below				*/
    unsigned char n_other;	/* used for const_flag and "default section" */
    unsigned	: 16;		/* padding for alignment */
    int n_desc;			/* source line number for N_SLINE stabs */
  };

/* Legal values of n_type (see aout/stab.def for the majority of the codes).
 */
#define N_UNDF	0		/* Undefined symbol	*/
#define N_ABS	2		/* Absolute symbol	*/
#define N_TEXT	4		/* Text symbol		*/
#define N_DATA	6		/* Data symbol		*/
#define N_BSS	8		/* BSS symbol		*/
#define N_FN	31		/* Filename symbol	*/

#define N_EXT	1		/* External symbol (OR'd in with one of above)	*/
#define N_TYPE	036		/* Mask for all the type bits			*/

#define N_STAB	0340		/* Mask for all bits used for SDB entries 	*/

#include "aout/stab_gnu.h"

/* SYMBOL TABLE */
/* Symbol table entry data type */

typedef struct nlist obj_symbol_type;	/* Symbol table entry */

/* Symbol table macros and constants */

#define OBJ_SYMFIELD_TYPE struct VMS_Symbol *

/*
 *  Macros to extract information from a symbol table entry.
 *  This syntactic indirection allows independence regarding a.out or coff.
 *  The argument (s) of all these macros is a pointer to a symbol table entry.
 */

/* True if the symbol is external */
#define S_IS_EXTERNAL(s)	((s)->sy_symbol.n_type & N_EXT)

/* True if symbol has been defined, ie is in N_{TEXT,DATA,BSS,ABS} or N_EXT */
#define S_IS_DEFINED(s)		(S_GET_TYPE(s) != N_UNDF)

#define S_IS_COMMON(s)	(S_GET_TYPE(s) == N_UNDF && S_GET_VALUE(s) != 0)

/* Return true for symbols that should not be reduced to section
   symbols or eliminated from expressions, because they may be
   overridden by the linker.  */
#define S_FORCE_RELOC(s, strict) \
  (!SEG_NORMAL (S_GET_SEGMENT (s)))

#define S_IS_REGISTER(s)	((s)->sy_symbol.n_type == N_REGISTER)

/* True if a debug special symbol entry */
#define S_IS_DEBUG(s)		((s)->sy_symbol.n_type & N_STAB)
/* True if a symbol is local symbol name */
/* A symbol name whose name begin with ^A is a gas internal pseudo symbol
   nameless symbols come from .stab directives.  */
#define S_IS_LOCAL(s)		(S_GET_NAME(s) && \
				 !S_IS_DEBUG(s) && \
				 (strchr(S_GET_NAME(s), '\001') != 0 || \
				  strchr(S_GET_NAME(s), '\002') != 0 || \
				  (S_LOCAL_NAME(s) && !flag_keep_locals)))
/* True if a symbol is not defined in this file */
#define S_IS_EXTERN(s)		((s)->sy_symbol.n_type & N_EXT)
/* True if the symbol has been generated because of a .stabd directive */
#define S_IS_STABD(s)		(S_GET_NAME(s) == (char *)0)

/* Accessors */
/* The name of the symbol */
#define S_GET_NAME(s)		((s)->sy_symbol.n_name)
/* The pointer to the string table */
#define S_GET_OFFSET(s)		((s)->sy_name_offset)
/* The raw type of the symbol */
#define S_GET_RAW_TYPE(s)		((s)->sy_symbol.n_type)
/* The type of the symbol */
#define S_GET_TYPE(s)		((s)->sy_symbol.n_type & N_TYPE)
/* The numeric value of the segment */
#define S_GET_SEGMENT(s)	(N_TYPE_seg[S_GET_TYPE(s)])
/* The n_other expression value */
#define S_GET_OTHER(s)		((s)->sy_symbol.n_other)
/* The n_desc expression value */
#define S_GET_DESC(s)		((s)->sy_symbol.n_desc)

/* Modifiers */
/* Assume that a symbol cannot be simultaneously in more than on segment */
/* set segment */
#define S_SET_SEGMENT(s,seg)	((s)->sy_symbol.n_type &= ~N_TYPE,(s)->sy_symbol.n_type|=SEGMENT_TO_SYMBOL_TYPE(seg))
/* The symbol is external */
#define S_SET_EXTERNAL(s)	((s)->sy_symbol.n_type |= N_EXT)
/* The symbol is not external */
#define S_CLEAR_EXTERNAL(s)	((s)->sy_symbol.n_type &= ~N_EXT)
/* Set the name of the symbol */
#define S_SET_NAME(s,v)		((s)->sy_symbol.n_name = (v))
/* Set the offset in the string table */
#define S_SET_OFFSET(s,v)	((s)->sy_name_offset = (v))
/* Set the n_other expression value */
#define S_SET_OTHER(s,v)	((s)->sy_symbol.n_other = (v))
/* Set the n_desc expression value */
#define S_SET_DESC(s,v)		((s)->sy_symbol.n_desc = (v))
/* Set the n_type expression value */
#define S_SET_TYPE(s,v)		((s)->sy_symbol.n_type = (v))

/* File header macro and type definition */

#define H_GET_TEXT_SIZE(h)		((h)->header.a_text)
#define H_GET_DATA_SIZE(h)		((h)->header.a_data)
#define H_GET_BSS_SIZE(h)		((h)->header.a_bss)

#define H_SET_TEXT_SIZE(h,v)		((h)->header.a_text = md_section_align(SEG_TEXT, (v)))
#define H_SET_DATA_SIZE(h,v)		((h)->header.a_data = md_section_align(SEG_DATA, (v)))
#define H_SET_BSS_SIZE(h,v)		((h)->header.a_bss = md_section_align(SEG_BSS, (v)))

#define H_SET_STRING_SIZE(h,v)		((h)->string_table_size = (v))
#define H_SET_SYMBOL_TABLE_SIZE(h,v)	((h)->header.a_syms = (v) * \
					 sizeof (struct nlist))

/* line numbering stuff.  */
#define OBJ_EMIT_LINENO(a, b, c)	{;}

#define obj_symbol_new_hook(s)	{;}

/* Force structure tags into scope so that their use in prototypes
   will never be their first occurrence.  */
struct fix;
struct frag;

/* obj-vms routines visible to the rest of gas.  */

extern void tc_aout_fix_to_chars PARAMS ((char *,struct fix *,relax_addressT));

extern int vms_resolve_symbol_redef PARAMS ((symbolS *));
#define RESOLVE_SYMBOL_REDEFINITION(X)	vms_resolve_symbol_redef(X)

/* Compiler-generated label "__vax_g_doubles" is used to augment .stabs.  */
extern void vms_check_for_special_label PARAMS ((symbolS *));
#define obj_frob_label(X) vms_check_for_special_label(X)

extern void vms_check_for_main PARAMS ((void));

extern void vms_write_object_file PARAMS ((unsigned,unsigned,unsigned,
					   struct frag *,struct frag *));

/* VMS executables are nothing like a.out, but the VMS port of gcc uses
   a.out format stabs which obj-vms.c then translates.  */

#define AOUT_STABS


#ifdef WANT_VMS_OBJ_DEFS

/* The rest of this file contains definitions for constants used within
   the actual VMS object file.  We do not use a $ in the symbols (as per
   usual VMS convention) since System V gags on it.  */

#define	OBJ_S_C_HDR	0
#define	OBJ_S_C_HDR_MHD	0
#define	OBJ_S_C_HDR_LNM	1
#define	OBJ_S_C_HDR_SRC	2
#define	OBJ_S_C_HDR_TTL	3
#define	OBJ_S_C_HDR_CPR	4
#define	OBJ_S_C_HDR_MTC	5
#define	OBJ_S_C_HDR_GTX	6
#define	OBJ_S_C_GSD	1
#define	OBJ_S_C_GSD_PSC	0
#define	OBJ_S_C_GSD_SYM	1
#define	OBJ_S_C_GSD_EPM	2
#define	OBJ_S_C_GSD_PRO	3
#define	OBJ_S_C_GSD_SYMW	4
#define	OBJ_S_C_GSD_EPMW	5
#define	OBJ_S_C_GSD_PROW	6
#define	OBJ_S_C_GSD_IDC	7
#define	OBJ_S_C_GSD_ENV	8
#define	OBJ_S_C_GSD_LSY	9
#define	OBJ_S_C_GSD_LEPM	10
#define	OBJ_S_C_GSD_LPRO	11
#define	OBJ_S_C_GSD_SPSC	12
#define	OBJ_S_C_TIR	2
#define	OBJ_S_C_EOM	3
#define	OBJ_S_C_DBG	4
#define	OBJ_S_C_TBT	5
#define	OBJ_S_C_LNK	6
#define	OBJ_S_C_EOMW	7
#define	OBJ_S_C_MAXRECTYP	7
#define	OBJ_S_K_SUBTYP	1
#define	OBJ_S_C_SUBTYP	1
#define	OBJ_S_C_MAXRECSIZ	2048
#define	OBJ_S_C_STRLVL	0
#define	OBJ_S_C_SYMSIZ	31
#define	OBJ_S_C_STOREPLIM	-1
#define	OBJ_S_C_PSCALILIM	9

#define	MHD_S_C_MHD	0
#define	MHD_S_C_LNM	1
#define	MHD_S_C_SRC	2
#define	MHD_S_C_TTL	3
#define	MHD_S_C_CPR	4
#define	MHD_S_C_MTC	5
#define	MHD_S_C_GTX	6
#define	MHD_S_C_MAXHDRTYP	6

#define	GSD_S_K_ENTRIES	1
#define	GSD_S_C_ENTRIES	1
#define	GSD_S_C_PSC	0
#define	GSD_S_C_SYM	1
#define	GSD_S_C_EPM	2
#define	GSD_S_C_PRO	3
#define	GSD_S_C_SYMW	4
#define	GSD_S_C_EPMW	5
#define	GSD_S_C_PROW	6
#define	GSD_S_C_IDC	7
#define	GSD_S_C_ENV	8
#define	GSD_S_C_LSY	9
#define	GSD_S_C_LEPM	10
#define	GSD_S_C_LPRO	11
#define	GSD_S_C_SPSC	12
#define	GSD_S_C_SYMV	13
#define	GSD_S_C_EPMV	14
#define	GSD_S_C_PROV	15
#define	GSD_S_C_MAXRECTYP	15

#define	GSY_S_M_WEAK	1
#define	GSY_S_M_DEF	2
#define	GSY_S_M_UNI	4
#define	GSY_S_M_REL	8

#define	LSY_S_M_DEF	2
#define	LSY_S_M_REL	8

#define	ENV_S_M_DEF	1
#define	ENV_S_M_NESTED	2

#define	GPS_S_M_PIC	1
#define	GPS_S_M_LIB	2
#define	GPS_S_M_OVR	4
#define	GPS_S_M_REL	8
#define	GPS_S_M_GBL	16
#define	GPS_S_M_SHR	32
#define	GPS_S_M_EXE	64
#define	GPS_S_M_RD	128
#define	GPS_S_M_WRT	256
#define	GPS_S_M_VEC	512
#define	GPS_S_K_NAME	9
#define	GPS_S_C_NAME	9

#define	TIR_S_C_STA_GBL	0
#define	TIR_S_C_STA_SB	1
#define	TIR_S_C_STA_SW	2
#define	TIR_S_C_STA_LW	3
#define	TIR_S_C_STA_PB	4
#define	TIR_S_C_STA_PW	5
#define	TIR_S_C_STA_PL	6
#define	TIR_S_C_STA_UB	7
#define	TIR_S_C_STA_UW	8
#define	TIR_S_C_STA_BFI	9
#define	TIR_S_C_STA_WFI	10
#define	TIR_S_C_STA_LFI	11
#define	TIR_S_C_STA_EPM	12
#define	TIR_S_C_STA_CKARG	13
#define	TIR_S_C_STA_WPB	14
#define	TIR_S_C_STA_WPW	15
#define	TIR_S_C_STA_WPL	16
#define	TIR_S_C_STA_LSY	17
#define	TIR_S_C_STA_LIT	18
#define	TIR_S_C_STA_LEPM	19
#define	TIR_S_C_MAXSTACOD	19
#define	TIR_S_C_MINSTOCOD	20
#define	TIR_S_C_STO_SB	20
#define	TIR_S_C_STO_SW	21
#define	TIR_S_C_STO_L	22
#define	TIR_S_C_STO_BD	23
#define	TIR_S_C_STO_WD	24
#define	TIR_S_C_STO_LD	25
#define	TIR_S_C_STO_LI	26
#define	TIR_S_C_STO_PIDR	27
#define	TIR_S_C_STO_PICR	28
#define	TIR_S_C_STO_RSB	29
#define	TIR_S_C_STO_RSW	30
#define	TIR_S_C_STO_RL	31
#define	TIR_S_C_STO_VPS	32
#define	TIR_S_C_STO_USB	33
#define	TIR_S_C_STO_USW	34
#define	TIR_S_C_STO_RUB	35
#define	TIR_S_C_STO_RUW	36
#define	TIR_S_C_STO_B	37
#define	TIR_S_C_STO_W	38
#define	TIR_S_C_STO_RB	39
#define	TIR_S_C_STO_RW	40
#define	TIR_S_C_STO_RIVB	41
#define	TIR_S_C_STO_PIRR	42
#define	TIR_S_C_MAXSTOCOD	42
#define	TIR_S_C_MINOPRCOD	50
#define	TIR_S_C_OPR_NOP	50
#define	TIR_S_C_OPR_ADD	51
#define	TIR_S_C_OPR_SUB	52
#define	TIR_S_C_OPR_MUL	53
#define	TIR_S_C_OPR_DIV	54
#define	TIR_S_C_OPR_AND	55
#define	TIR_S_C_OPR_IOR	56
#define	TIR_S_C_OPR_EOR	57
#define	TIR_S_C_OPR_NEG	58
#define	TIR_S_C_OPR_COM	59
#define	TIR_S_C_OPR_INSV	60
#define	TIR_S_C_OPR_ASH	61
#define	TIR_S_C_OPR_USH	62
#define	TIR_S_C_OPR_ROT	63
#define	TIR_S_C_OPR_SEL	64
#define	TIR_S_C_OPR_REDEF	65
#define	TIR_S_C_OPR_DFLIT	66
#define	TIR_S_C_MAXOPRCOD	66
#define	TIR_S_C_MINCTLCOD	80
#define	TIR_S_C_CTL_SETRB	80
#define	TIR_S_C_CTL_AUGRB	81
#define	TIR_S_C_CTL_DFLOC	82
#define	TIR_S_C_CTL_STLOC	83
#define	TIR_S_C_CTL_STKDL	84
#define	TIR_S_C_MAXCTLCOD	84

/*
 *	Debugger symbol definitions:  These are done by hand, as no
 *					machine-readable version seems
 *					to be available.
 */
#define DST_S_C_C	  7	/* Language == "C"	*/
#define DST_S_C_CXX	 15	/* Language == "C++"	*/
#define DST_S_C_VERSION	153
#define	DST_S_C_SOURCE	155	/* Source file		*/
#define DST_S_C_PROLOG	162
#define	DST_S_C_BLKBEG	176	/* Beginning of block	*/
#define	DST_S_C_BLKEND	177	/* End of block	*/
#define DST_S_C_ENTRY	181
#define DST_S_C_PSECT	184
#define	DST_S_C_LINE_NUM	185	/* Line Number		*/
#define DST_S_C_LBLORLIT	186
#define DST_S_C_LABEL	187
#define	DST_S_C_MODBEG	188	/* Beginning of module	*/
#define	DST_S_C_MODEND	189	/* End of module	*/
#define	DST_S_C_RTNBEG	190	/* Beginning of routine	*/
#define	DST_S_C_RTNEND	191	/* End of routine	*/
#define	DST_S_C_DELTA_PC_W	1	/* Incr PC	*/
#define	DST_S_C_INCR_LINUM	2	/* Incr Line #	*/
#define	DST_S_C_INCR_LINUM_W	3	/* Incr Line #	*/
#define DST_S_C_SET_LINUM_INCR	4
#define DST_S_C_SET_LINUM_INCR_W	5
#define DST_S_C_RESET_LINUM_INCR	6
#define DST_S_C_BEG_STMT_MODE	7
#define DST_S_C_END_STMT_MODE	8
#define	DST_S_C_SET_LINE_NUM	9	/* Set Line #	*/
#define DST_S_C_SET_PC		10
#define DST_S_C_SET_PC_W		11
#define DST_S_C_SET_PC_L		12
#define DST_S_C_SET_STMTNUM	13
#define DST_S_C_TERM		14	/* End of lines	*/
#define DST_S_C_TERM_W		15	/* End of lines	*/
#define	DST_S_C_SET_ABS_PC	16	/* Set PC	*/
#define	DST_S_C_DELTA_PC_L	17	/* Incr PC	*/
#define DST_S_C_INCR_LINUM_L	18	/* Incr Line #	*/
#define DST_S_C_SET_LINUM_B	19	/* Set Line #	*/
#define DST_S_C_SET_LINUM_L	20	/* Set Line #	*/
#define	DST_S_C_TERM_L		21	/* End of lines	*/
/* these are used with DST_S_C_SOURCE */
#define DST_S_C_SRC_DECLFILE	 1	/* Declare source file */
#define DST_S_C_SRC_SETFILE	 2	/* Set source file */
#define DST_S_C_SRC_SETREC_L	 3	/* Set record, longword value */
#define DST_S_C_SRC_SETREC_W	 4	/* Set record, word value */
#define DST_S_C_SRC_DEFLINES_W	10	/* # of line, word counter */
#define DST_S_C_SRC_DEFLINES_B	11	/* # of line, byte counter */
#define DST_S_C_SRC_FORMFEED	16	/* ^L counts as a record */
/* the following are the codes for the various data types.  Anything not on
 * the list is included under 'advanced_type'
 */
#define DBG_S_C_UCHAR		0x02
#define DBG_S_C_USINT		0x03
#define DBG_S_C_ULINT		0x04
#define DBG_S_C_UQUAD		0x05
#define DBG_S_C_SCHAR		0x06
#define DBG_S_C_SSINT		0x07
#define DBG_S_C_SLINT		0x08
#define DBG_S_C_SQUAD		0x09
#define DBG_S_C_REAL4		0x0a
#define DBG_S_C_REAL8		0x0b		/* D_float double */
#define DBG_S_C_COMPLX4		0x0c		/* 2xF_float complex float */
#define DBG_S_C_COMPLX8		0x0d		/* 2xD_float complex double */
#define DBG_S_C_REAL8_G		0x1b		/* G_float double */
#define DBG_S_C_COMPLX8_G	0x1d		/* 2xG_float complex double */
#define DBG_S_C_FUNCTION_ADDR	0x17
#define DBG_S_C_ADVANCED_TYPE	0xa3
/*  Some of these are just for future reference.  [pr]
 */
#define DBG_S_C_UBITA		0x01	/* unsigned, aligned bit field */
#define DBG_S_C_UBITU		0x22	/* unsigned, unaligned bit field */
#define DBG_S_C_SBITA		0x29	/* signed, aligned bit field */
#define DBG_S_C_SBITU		0x2a	/* signed, unaligned bit field */
#define DBG_S_C_CSTRING		0x2e	/* asciz ('\0' terminated) string */
#define DBG_S_C_WCHAR		0x38	/* wchar_t */
/*  These are descriptor class codes.
 */
#define DSC_K_CLASS_S		0x01	/* static (fixed length) */
#define DSC_K_CLASS_D		0x02	/* dynamic string (not via malloc!) */
#define DSC_K_CLASS_A		0x04	/* array */
#define DSC_K_CLASS_UBS		0x0d	/* unaligned bit string */
/*  These are the codes that are used to generate the definitions of struct
 *  union and enum records
 */
#define DBG_S_C_ENUM_ITEM		0xa4
#define DBG_S_C_ENUM_START		0xa5
#define DBG_S_C_ENUM_END		0xa6
#define DBG_S_C_STRUCT_ITEM		DST_K_VFLAGS_BITOFFS	/* 0xff */
#define DBG_S_C_STRUCT_START		0xab
#define DBG_S_C_STRUCT_END		0xac
#define DST_K_TYPSPEC			0xaf		/* type specification */
/* These codes are used in the generation of the symbol definition records
 */
#define DST_K_VFLAGS_NOVAL		0x80	/* struct definition only */
#define DST_K_VFLAGS_DSC		0xfa	/* descriptor used */
#define DST_K_VFLAGS_TVS		0xfb	/* trailing value specified */
#define DST_K_VS_FOLLOWS		0xfd	/* value spec follows */
#define DST_K_VFLAGS_BITOFFS		0xff	/* value contains bit offset */
#define DST_K_VALKIND_LITERAL	0
#define DST_K_VALKIND_ADDR	1
#define DST_K_VALKIND_DESC	2
#define DST_K_VALKIND_REG	3
#define DST_K_REG_VAX_AP	0x0c	/* R12 */
#define DST_K_REG_VAX_FP	0x0d	/* R13 */
#define DST_K_REG_VAX_SP	0x0e	/* R14 */
#define DST_V_VALKIND		0	/* offset of valkind field */
#define DST_V_INDIRECT		2	/* offset to indirect bit */
#define DST_V_DISP		3	/* offset to displacement bit */
#define DST_V_REGNUM		4	/* offset to register number */
#define DST_M_INDIRECT		(1<<DST_V_INDIRECT)
#define DST_M_DISP		(1<<DST_V_DISP)
#define DBG_C_FUNCTION_PARAM	/* 0xc9 */	\
	(DST_K_VALKIND_ADDR|DST_M_DISP|(DST_K_REG_VAX_AP<<DST_V_REGNUM))
#define DBG_C_LOCAL_SYM		/* 0xd9 */	\
	(DST_K_VALKIND_ADDR|DST_M_DISP|(DST_K_REG_VAX_FP<<DST_V_REGNUM))
/* Kinds of value specifications
 */
#define DST_K_VS_ALLOC_SPLIT	3	/* split lifetime */
/* Kinds of type specifications
 */
#define DST_K_TS_ATOM		0x01	/* atomic type specification */
#define DST_K_TS_DSC		0x02	/* descriptor type spec */
#define DST_K_TS_IND		0x03	/* indirect type specification */
#define DST_K_TS_TPTR		0x04	/* typed pointer type spec */
#define DST_K_TS_PTR		0x05	/* pointer type spec */
#define DST_K_TS_ARRAY		0x07	/* array type spec */
#define DST_K_TS_NOV_LENG	0x0e	/* novel length type spec */
/*  These are the codes that are used in the suffix records to determine the
 *  actual data type
 */
#define DBG_S_C_BASIC			DST_K_TS_ATOM
#define DBG_S_C_BASIC_ARRAY		DST_K_TS_DSC
#define DBG_S_C_STRUCT			DST_K_TS_IND
#define DBG_S_C_POINTER			DST_K_TS_TPTR
#define DBG_S_C_VOID			DST_K_TS_PTR
#define DBG_S_C_COMPLEX_ARRAY		DST_K_TS_ARRAY

#endif	/* WANT_VMS_OBJ_DEFS */
