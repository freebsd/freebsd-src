/* VMS object file format
   Copyright (C) 1989, 1990, 1991 Free Software Foundation, Inc.

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
to the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

/* Tag to validate a.out object file format processing */
#define OBJ_VMS 1

#include "targ-cpu.h"

/* This flag is used to remember whether we are in the const or the
   data section.  By and large they are identical, but we set a no-write
   bit for psects in the const section.  */

extern char const_flag;


/* These are defined in obj-vms.c. */
extern const short seg_N_TYPE[];
extern const segT  N_TYPE_seg[];

enum reloc_type {
	NO_RELOC, RELOC_32
};

#define N_BADMAG(x)	(0)
#define N_TXTOFF(x)	( sizeof(struct exec) )
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

typedef struct {
    struct exec	header;			/* a.out header */
    long	string_table_size;	/* names + '\0' + sizeof(int) */
} object_headers;

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

#define	N_GSYM	0x20		/* global symbol: name,,0,type,0 */
#define	N_FNAME	0x22		/* procedure name (f77 kludge): name,,0 */
#define	N_FUN	0x24		/* procedure: name,,0,linenumber,address */
#define	N_STSYM	0x26		/* static symbol: name,,0,type,address */
#define	N_LCSYM	0x28		/* .lcomm symbol: name,,0,type,address */
#define	N_RSYM	0x40		/* register sym: name,,0,type,register */
#define	N_SLINE	0x44		/* src line: 0,,0,linenumber,address */
#define	N_CATCH	0x54		/* */
#define	N_SSYM	0x60		/* structure elt: name,,0,type,struct_offset */
#define	N_SO	0x64		/* source file name: name,,0,0,address */
#define	N_LSYM	0x80		/* local sym: name,,0,type,offset */
#define	N_SOL	0x84		/* #included file name: name,,0,0,address */
#define	N_PSYM	0xa0		/* parameter: name,,0,type,offset */
#define	N_ENTRY	0xa4		/* alternate entry: name,linenumber,address */
#define	N_LBRAC	0xc0		/* left bracket: 0,,0,nesting level,address */
#define	N_RBRAC	0xe0		/* right bracket: 0,,0,nesting level,address */
#define	N_BCOMM	0xe2		/* begin common: name,, */
#define	N_ECOMM	0xe4		/* end common: name,, */
#define	N_ECOML	0xe8		/* end common (local name): ,,address */
#define	N_LENG	0xfe		/* second stab entry with length information */

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
#define S_IS_DEFINED(s)		(S_GET_TYPE(s) != N_UNDF)

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

#define H_GET_TEXT_SIZE(h)		((h)->header.a_text)
#define H_GET_DATA_SIZE(h)		((h)->header.a_data)
#define H_GET_BSS_SIZE(h)		((h)->header.a_bss)

#define H_SET_TEXT_SIZE(h,v)		((h)->header.a_text = md_section_align(SEG_TEXT, (v)))
#define H_SET_DATA_SIZE(h,v)		((h)->header.a_data = md_section_align(SEG_DATA, (v)))
#define H_SET_BSS_SIZE(h,v)		((h)->header.a_bss = md_section_align(SEG_BSS, (v)))

#define H_SET_STRING_SIZE(h,v)		((h)->string_table_size = (v))
#define H_SET_SYMBOL_TABLE_SIZE(h,v)	((h)->header.a_syms = (v) * \
					 sizeof(struct nlist))

/* 
 * Current means for getting the name of a segment.
 * This will change for infinite-segments support (e.g. COFF).
 */
#define	segment_name(seg)  ( seg_name[(int)(seg)] )
extern char *const seg_name[];


/* line numbering stuff. */
#define OBJ_EMIT_LINENO(a, b, c)	{;}

#define obj_symbol_new_hook(s)	{;}

#ifdef __STDC__
struct fix;
void tc_aout_fix_to_chars(char *where, struct fix *fixP, relax_addressT segment_address);
#else
void tc_aout_fix_to_chars();
#endif /* __STDC__ */

/* The rest of this file contains definitions for constants used within the actual
   VMS object file.  We do not use a $ in the symbols (as per usual VMS
   convention) since System V gags on it.  */

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
#define	DST_S_C_C		7		/* Language == "C"	*/
#define DST_S_C_VERSION	153
#define	DST_S_C_SOURCE	155		/* Source file		*/
#define DST_S_C_PROLOG	162
#define	DST_S_C_BLKBEG	176		/* Beginning of block	*/
#define	DST_S_C_BLKEND	177		/* End of block	*/
#define DST_S_C_ENTRY	181
#define DST_S_C_PSECT	184
#define	DST_S_C_LINE_NUM	185		/* Line Number		*/
#define DST_S_C_LBLORLIT	186
#define DST_S_C_LABEL	187
#define	DST_S_C_MODBEG	188		/* Beginning of module	*/
#define	DST_S_C_MODEND	189		/* End of module	*/
#define	DST_S_C_RTNBEG	190		/* Beginning of routine	*/
#define	DST_S_C_RTNEND	191		/* End of routine	*/
#define	DST_S_C_DELTA_PC_W	1		/* Incr PC	*/
#define	DST_S_C_INCR_LINUM	2		/* Incr Line #	*/
#define	DST_S_C_INCR_LINUM_W	3		/* Incr Line #	*/
#define DST_S_C_SET_LINUM_INCR	4
#define DST_S_C_SET_LINUM_INCR_W	5
#define DST_S_C_RESET_LINUM_INCR	6
#define DST_S_C_BEG_STMT_MODE	7
#define DST_S_C_END_STMT_MODE	8
#define	DST_S_C_SET_LINE_NUM	9		/* Set Line #	*/
#define DST_S_C_SET_PC		10
#define DST_S_C_SET_PC_W		11
#define DST_S_C_SET_PC_L		12
#define DST_S_C_SET_STMTNUM	13
#define DST_S_C_TERM		14		/* End of lines	*/
#define DST_S_C_TERM_W		15		/* End of lines	*/
#define	DST_S_C_SET_ABS_PC	16		/* Set PC	*/
#define	DST_S_C_DELTA_PC_L	17		/* Incr PC	*/
#define DST_S_C_INCR_LINUM_L	18		/* Incr Line #	*/
#define DST_S_C_SET_LINUM_B	19		/* Set Line #	*/
#define DST_S_C_SET_LINUM_L	20		/* Set Line #	*/
#define	DST_S_C_TERM_L		21		/* End of lines	*/
/* these are used with DST_S_C_SOURCE */
#define	DST_S_C_SRC_FORMFEED	16		/* ^L counts	*/
#define	DST_S_C_SRC_DECLFILE	1		/* Declare file	*/
#define	DST_S_C_SRC_SETFILE	2		/* Set file	*/
#define	DST_S_C_SRC_SETREC_L	3		/* Set record	*/
#define	DST_S_C_SRC_DEFLINES_W	10		/* # of line	*/
/* the following are the codes for the various data types.  Anything not on
 * the list is included under 'advanced_type'
 */
#define DBG_S_C_UCHAR		0x02
#define DBG_S_C_USINT		0x03
#define DBG_S_C_ULINT		0x04
#define DBG_S_C_SCHAR		0x06
#define DBG_S_C_SSINT		0x07
#define DBG_S_C_SLINT		0x08
#define DBG_S_C_REAL4		0x0a
#define DBG_S_C_REAL8		0x0b
#define DBG_S_C_FUNCTION_ADDR	0x17
#define DBG_S_C_ADVANCED_TYPE	0xa3
/*  These are the codes that are used to generate the definitions of struct
 *  union and enum records
 */
#define DBG_S_C_ENUM_ITEM			0xa4
#define DBG_S_C_ENUM_START		0xa5
#define DBG_S_C_ENUM_END			0xa6
#define DBG_S_C_STRUCT_START		0xab
#define DBG_S_C_STRUCT_ITEM		0xff
#define DBG_S_C_STRUCT_END		0xac
/*  These are the codes that are used in the suffix records to determine the
 *  actual data type
 */
#define DBG_S_C_BASIC			0x01
#define DBG_S_C_BASIC_ARRAY		0x02
#define DBG_S_C_STRUCT			0x03
#define DBG_S_C_POINTER			0x04
#define DBG_S_C_VOID			0x05
#define DBG_S_C_COMPLEX_ARRAY		0x07
/* These codes are used in the generation of the symbol definition records
 */
#define DBG_S_C_FUNCTION_PARAMETER	0xc9
#define DBG_S_C_LOCAL_SYM			0xd9
/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of obj-vms.h */
