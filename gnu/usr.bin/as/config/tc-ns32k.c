/* ns32k.c  -- Assemble on the National Semiconductor 32k series
   Copyright (C) 1987, 1992 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

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

/*#define SHOW_NUM 1*/ /* uncomment for debugging */

#include <stdio.h>
#include <ctype.h>
#ifdef USG
#include <string.h>
#else
#include <strings.h>
#endif
#include "opcode/ns32k.h"

#include "as.h"

#include "obstack.h"

/* Macros */
#define IIF_ENTRIES 13 /* number of entries in iif */
#define PRIVATE_SIZE 256 /* size of my garbage memory */
#define MAX_ARGS 4
#define DEFAULT	-1 /* addr_mode returns this value when plain constant or label is encountered */

#define IIF(ptr,a1,c1,e1,g1,i1,k1,m1,o1,q1,s1,u1) \
    iif.iifP[ptr].type= a1; \
    iif.iifP[ptr].size= c1; \
    iif.iifP[ptr].object= e1; \
    iif.iifP[ptr].object_adjust= g1; \
    iif.iifP[ptr].pcrel= i1; \
    iif.iifP[ptr].pcrel_adjust= k1; \
    iif.iifP[ptr].im_disp= m1; \
    iif.iifP[ptr].relax_substate= o1; \
    iif.iifP[ptr].bit_fixP= q1; \
    iif.iifP[ptr].addr_mode= s1; \
    iif.iifP[ptr].bsr= u1;

#ifdef TE_SEQUENT
#define LINE_COMMENT_CHARS "|"
#define ABSOLUTE_PREFIX '@'
#define IMMEDIATE_PREFIX '#'
#endif

#ifndef LINE_COMMENT_CHARS
#define LINE_COMMENT_CHARS "#"
#endif

const char comment_chars[] = "#";
const char line_comment_chars[] = LINE_COMMENT_CHARS;
#if !defined(ABSOLUTE_PREFIX) && !defined(IMMEDIATE_PREFIX)
#define ABSOLUTE_PREFIX '@'	/* One or the other MUST be defined */
#endif

struct addr_mode {
	char mode;			/* addressing mode of operand (0-31) */
	char scaled_mode;		/* mode combined with scaled mode */
	char scaled_reg;		/* register used in scaled+1 (1-8) */
	char float_flag;		/* set if R0..R7 was F0..F7 ie a floating-point-register */
	char am_size;			/* estimated max size of general addr-mode parts*/
	char im_disp;			/* if im_disp == 1 we have a displacement */
	char pcrel;			/* 1 if pcrel, this is really redundant info */
	char disp_suffix[2];		/* length of displacement(s), 0=undefined */
	char *disp[2];		/* pointer(s) at displacement(s)
				   or immediates(s)     (ascii) */
	char index_byte;		/* index byte */
};
typedef struct addr_mode addr_modeS;


char *freeptr,*freeptr_static; /* points at some number of free bytes */
struct hash_control *inst_hash_handle;

struct ns32k_opcode *desc; /* pointer at description of instruction */
addr_modeS addr_modeP;
char EXP_CHARS[] = "eE";
char FLT_CHARS[] = "fd"; /* we don't want to support lowercase, do we */

/* UPPERCASE denotes live names
 * when an instruction is built, IIF is used as an intermidiate form to store
 * the actual parts of the instruction. A ns32k machine instruction can
 * be divided into a couple of sub PARTs. When an instruction is assembled
 * the appropriate PART get an assignment. When an IIF has been completed it's
 * converted to a FRAGment as specified in AS.H */

/* internal structs */
struct option {
	char *pattern;
	unsigned long or;
	unsigned long and;
};

typedef struct {
	int type;			/* how to interpret object */
	int size;			/* Estimated max size of object */
	unsigned long object;		/* binary data */
	int object_adjust;		/* number added to object */
	int pcrel;			/* True if object is pcrel */
	int pcrel_adjust;		/* length in bytes from the
					   instruction start to the
					   displacement */
	int im_disp;			/* True if the object is a displacement */
	relax_substateT	relax_substate; /* Initial relaxsubstate */
	bit_fixS *bit_fixP;		/* Pointer at bit_fix struct */
	int addr_mode;			/* What addrmode do we associate with this iif-entry */
	char bsr;			/* Sequent hack */
}iif_entryT; /* Internal Instruction Format */

struct int_ins_form {
	int instr_size; /* Max size of instruction in bytes. */
	iif_entryT iifP[IIF_ENTRIES + 1];
};
struct int_ins_form iif;
expressionS exprP;
char *input_line_pointer;
/* description of the PARTs in IIF
 *object[n]:
 * 0	total length in bytes of entries in iif
 * 1	opcode
 * 2	index_byte_a
 * 3	index_byte_b
 * 4	disp_a_1
 * 5	disp_a_2
 * 6	disp_b_1
 * 7	disp_b_2
 * 8	imm_a
 * 9	imm_b
 * 10	implied1
 * 11	implied2
 *
 * For every entry there is a datalength in bytes. This is stored in size[n].
 *	 0,	the objectlength is not explicitly given by the instruction
 *		and the operand is undefined. This is a case for relaxation.
 *		Reserve 4 bytes for the final object.
 *
 *	 1,	the entry contains one byte
 *	 2,	the entry contains two bytes
 *	 3,	the entry contains three bytes
 *	 4,	the entry contains four bytes
 *	etc
 *
 * Furthermore, every entry has a data type identifier in type[n].
 *
 * 	 0,	the entry is void, ignore it.
 * 	 1,	the entry is a binary number.
 *	 2,	the entry is a pointer at an expression.
 *		Where expression may be as simple as a single '1',
 *		and as complicated as  foo-bar+12,
 * 		foo and bar may be undefined but suffixed by :{b|w|d} to
 *		control the length of the object.
 *
 *	 3,	the entry is a pointer at a bignum struct
 *
 *
 * The low-order-byte coresponds to low physical memory.
 * Obviously a FRAGment must be created for each valid disp in PART whose
 * datalength is undefined (to bad) .
 * The case where just the expression is undefined is less severe and is
 * handled by fix. Here the number of bytes in the objectfile is known.
 * With this representation we simplify the assembly and separates the
 * machine dependent/independent parts in a more clean way (said OE)
 */

struct option opt1[]= /* restore, exit */
{
	{ "r0",	0x80,	0xff	},
	{ "r1",	0x40,	0xff	},
	{ "r2",	0x20,	0xff	},
	{ "r3",	0x10,	0xff	},
	{ "r4",	0x08,	0xff	},
	{ "r5",	0x04,	0xff	},
	{ "r6",	0x02,	0xff	},
	{ "r7",	0x01,	0xff	},
	{  0 ,	0x00,	0xff	}
};
struct option opt2[]= /* save, enter */
{
	{ "r0",	0x01,	0xff	},
	{ "r1",	0x02,	0xff	},
	{ "r2",	0x04,	0xff	},
	{ "r3",	0x08,	0xff	},
	{ "r4",	0x10,	0xff	},
	{ "r5",	0x20,	0xff	},
	{ "r6",	0x40,	0xff	},
	{ "r7",	0x80,	0xff	},
	{  0 ,	0x00,	0xff	}
};
struct option opt3[]= /* setcfg */
{
	{ "c",	0x8,	0xff	},
	{ "m",	0x4,	0xff	},
	{ "f",	0x2,	0xff	},
	{ "i",	0x1,	0xff	},
	{  0 ,	0x0,	0xff	}
};
struct option opt4[]= /* cinv */
{
	{ "a",	0x4,	0xff	},
	{ "i",	0x2,	0xff	},
	{ "d",	0x1,	0xff	},
	{  0 ,	0x0,	0xff	}
};
struct option opt5[]= /* string inst */
{
	{ "b",	0x2,	0xff	},
	{ "u",	0xc,	0xff	},
	{ "w",	0x4,	0xff	},
	{  0 ,	0x0,	0xff	}
};
struct option opt6[]= /* plain reg ext,cvtp etc */
{
	{ "r0",	0x00,	0xff	},
	{ "r1",	0x01,	0xff	},
	{ "r2",	0x02,	0xff	},
	{ "r3",	0x03,	0xff	},
	{ "r4",	0x04,	0xff	},
	{ "r5",	0x05,	0xff	},
	{ "r6",	0x06,	0xff	},
	{ "r7",	0x07,	0xff	},
	{  0 ,	0x00,	0xff	}
};

#if !defined(NS32032) && !defined(NS32532)
#define NS32032
#endif

struct option cpureg_532[]= /* lpr spr */
{
	{ "us",	0x0,	0xff	},
	{ "dcr",	0x1,	0xff	},
	{ "bpc",	0x2,	0xff	},
	{ "dsr",	0x3,	0xff	},
	{ "car",	0x4,	0xff	},
	{ "fp",	0x8,	0xff	},
	{ "sp",	0x9,	0xff	},
	{ "sb",	0xa,	0xff	},
	{ "usp",	0xb,	0xff	},
	{ "cfg",	0xc,	0xff	},
	{ "psr",	0xd,	0xff	},
	{ "intbase",	0xe,	0xff	},
	{ "mod",	0xf,	0xff	},
	{  0 ,	0x00,	0xff	}
};
struct option mmureg_532[]= /* lmr smr */
{
	{ "mcr",	0x9,	0xff	},
	{ "msr",	0xa,	0xff	},
	{ "tear",	0xb,	0xff	},
	{ "ptb0",	0xc,	0xff	},
	{ "ptb1",	0xd,	0xff	},
	{ "ivar0",	0xe,	0xff	},
	{ "ivar1",	0xf,	0xff	},
	{  0 ,	0x0,	0xff	}
};

struct option cpureg_032[]= /* lpr spr */
{
	{ "upsr",	0x0,	0xff	},
	{ "fp",	0x8,	0xff	},
	{ "sp",	0x9,	0xff	},
	{ "sb",	0xa,	0xff	},
	{ "psr",	0xd,	0xff	},
	{ "intbase",	0xe,	0xff	},
	{ "mod",	0xf,	0xff	},
	{  0 ,	0x0,	0xff	}
};
struct option mmureg_032[]= /* lmr smr */
{
	{ "bpr0",	0x0,	0xff	},
	{ "bpr1",	0x1,	0xff	},
	{ "pf0",	0x4,	0xff	},
	{ "pf1",	0x5,	0xff	},
	{ "sc",	0x8,	0xff	},
	{ "msr",	0xa,	0xff	},
	{ "bcnt",	0xb,	0xff	},
	{ "ptb0",	0xc,	0xff	},
	{ "ptb1",	0xd,	0xff	},
	{ "eia",	0xf,	0xff	},
	{  0 ,	0x0,	0xff	}
};

#if defined(NS32532)
struct option *cpureg = cpureg_532;
struct option *mmureg = mmureg_532;
#else
struct option *cpureg = cpureg_032;
struct option *mmureg = mmureg_032;
#endif


const pseudo_typeS md_pseudo_table[]={ /* so far empty */
	{ 0,	0,	0	}
};

#define IND(x,y)	(((x)<<2)+(y))

/* those are index's to relax groups in md_relax_table
   ie it must be multiplied by 4 to point at a group start. Viz IND(x,y)
   Se function relax_segment in write.c for more info */

#define BRANCH		1
#define PCREL		2

/* those are index's to entries in a relax group */

#define BYTE		0
#define WORD		1
#define DOUBLE		2
#define UNDEF           3
/* Those limits are calculated from the displacement start in memory.
   The ns32k uses the begining of the instruction as displacement base.
   This type of displacements could be handled here by moving the limit window
   up or down. I choose to use an internal displacement base-adjust as there
   are other routines that must consider this. Also, as we have two various
   offset-adjusts in the ns32k (acb versus br/brs/jsr/bcond), two set of limits
   would have had to be used.
   Now we dont have to think about that. */


const relax_typeS md_relax_table[] = {
	{ 1,		1,		0,	0			},
	{ 1,		1,		0,	0			},
	{ 1,		1,		0,	0			},
	{ 1,		1,		0,	0			},

	{ (63),		(-64),		1,	IND(BRANCH,WORD)	},
	{ (8191),	(-8192),	2,	IND(BRANCH,DOUBLE)	},
	{ 0,		0,		4,	0			},
	{ 1,		1,		0,	0			}
};

/* Array used to test if mode contains displacements.
   Value is true if mode contains displacement. */

char disp_test[] = { 0,0,0,0,0,0,0,0,
		     1,1,1,1,1,1,1,1,
		     1,1,1,0,0,1,1,0,
		     1,1,1,1,1,1,1,1 };

/* Array used to calculate max size of displacements */

char disp_size[] = { 4,1,2,0,4 };

#ifdef PIC
/* In pic-mode all external pc-relative references are jmpslot
   references except references to __GLOBAL_OFFSET_TABLE_. */
static symbolS *got_symbol;

/* The size of got-offsets. */
static int got_offset_size = 2;
#endif



#if __STDC__ == 1

static segT evaluate_expr(expressionS *resultP, char *ptr);
static void md_number_to_disp(char *buf, long val, int n);
static void md_number_to_imm(char *buf, long val, int n);

#else /* not __STDC__ */

static segT evaluate_expr();
static void md_number_to_disp();
static void md_number_to_imm();

#endif /* not __STDC__ */

/* Parses a general operand into an addressingmode struct

   in:  pointer at operand in ascii form
   pointer at addr_mode struct for result
   the level of recursion. (always 0 or 1)

   out: data in addr_mode struct
   */
int addr_mode(operand,addr_modeP,recursive_level)
char *operand;
register addr_modeS *addr_modeP;
int recursive_level;
{
	register char *str;
	register int i;
	register int strl;
	register int mode;
	int j;
	mode = DEFAULT;			 	/* default */
	addr_modeP->scaled_mode=0;		/* why not */
	addr_modeP->scaled_reg=0;		/* if 0, not scaled index */
	addr_modeP->float_flag=0;
	addr_modeP->am_size=0;
	addr_modeP->im_disp=0;
	addr_modeP->pcrel=0;			/* not set in this function */
	addr_modeP->disp_suffix[0]=0;
	addr_modeP->disp_suffix[1]=0;
	addr_modeP->disp[0]=NULL;
	addr_modeP->disp[1]=NULL;
	str=operand;
	if (str[0] == 0) {return (0);}		/* we don't want this */
	strl=strlen(str);
	switch (str[0]) {
		/* the following three case statements controls the mode-chars
		   this is the place to ed if you want to change them */
#ifdef ABSOLUTE_PREFIX
	case ABSOLUTE_PREFIX:
		if (str[strl-1] == ']') break;
		addr_modeP->mode=21;		/* absolute */
		addr_modeP->disp[0]=str+1;
		return (-1);
#endif
#ifdef IMMEDIATE_PREFIX
	case IMMEDIATE_PREFIX:
		if (str[strl-1] == ']') break;
		addr_modeP->mode=20;		/* immediate */
		addr_modeP->disp[0]=str+1;
		return (-1);
#endif
	case '.':
		if (str[strl-1] != ']') {
			switch (str[1]) {
				case'-':case'+':
				if (str[2] != '\000') {
					addr_modeP->mode=27;		/* pc-relativ */
					addr_modeP->disp[0]=str+2;
					return (-1);
				}
			default:
				as_warn("Invalid syntax in PC-relative addressing mode");
				return(0);
			}
		}
		break;
		case'e':
		if (str[strl-1] != ']') {
			if ((!strncmp(str,"ext(",4)) && strl>7) {	/* external */
				addr_modeP->disp[0]=str+4;
				i=0;
				j=2;
				do {				 /* disp[0]'s termination point */
					j+=1;
					if (str[j] == '(') i++;
					if (str[j] == ')') i--;
				} while (j < strl && i != 0);
				if (i != 0 || !(str[j+1] == '-' || str[j+1] == '+') ) {
					as_warn("Invalid syntax in External addressing mode");
					return(0);
				}
				str[j]='\000';			/* null terminate disp[0] */
				addr_modeP->disp[1]=str+j+2;
				addr_modeP->mode=22;
				return (-1);
			}
		}
		break;
	default:;
	}
	strl=strlen(str);
	switch (strl) {
	case 2:
		switch (str[0]) {
			case'f':addr_modeP->float_flag=1;
			case'r':
			if (str[1] >= '0' && str[1] < '8') {
				addr_modeP->mode=str[1]-'0';
				return (-1);
			}
		}
	case 3:
		if (!strncmp(str,"tos",3)) {
			addr_modeP->mode=23; /* TopOfStack */
			return (-1);
		}
	default:;
	}
	if (strl>4) {
		if (str[strl - 1] == ')') {
			if (str[strl - 2] == ')') {
				if (!strncmp(&str[strl-5],"(fp",3)) {
					mode=16; /* Memory Relative */
				}
				if (!strncmp(&str[strl-5],"(sp",3)) {
					mode=17;
				}
				if (!strncmp(&str[strl-5],"(sb",3)) {
					mode=18;
				}
				if (mode != DEFAULT) {	      /* memory relative */
					addr_modeP->mode=mode;
					j=strl-5; /* temp for end of disp[0] */
					i=0;
					do {
						strl-=1;
						if (str[strl] == ')') i++;
						if (str[strl] == '(') i--;
					} while (strl>-1 && i != 0);
					if (i != 0) {
						as_warn("Invalid syntax in Memory Relative addressing mode");
						return(0);
					}
					addr_modeP->disp[1]=str;
					addr_modeP->disp[0]=str+strl+1;
					str[j]='\000'; /* null terminate disp[0] */
					str[strl]='\000'; /* null terminate disp[1] */
					return (-1);
				}
			}
			switch (str[strl-3]) {
				case'r':case'R':
				if (str[strl - 2] >= '0' && str[strl - 2] < '8' && str[strl - 4] == '(') {
					addr_modeP->mode=str[strl-2]-'0'+8;
					addr_modeP->disp[0]=str;
					str[strl-4]=0;
					return (-1); /* reg rel */
				}
			default:
				if (!strncmp(&str[strl-4],"(fp",3)) {
					mode=24;
				}
				if (!strncmp(&str[strl-4],"(sp",3)) {
					mode=25;
				}
				if (!strncmp(&str[strl-4],"(sb",3)) {
					mode=26;
				}
				if (!strncmp(&str[strl-4],"(pc",3)) {
					mode=27;
				}
				if (mode != DEFAULT) {
					addr_modeP->mode=mode;
					addr_modeP->disp[0]=str;
					str[strl-4]='\0';
					return (-1); /* memory space */
				}
			}
		}
		/* no trailing ')' do we have a ']' ? */
		if (str[strl - 1] == ']') {
			switch (str[strl-2]) {
				case'b':mode=28;break;
				case'w':mode=29;break;
				case'd':mode=30;break;
				case'q':mode=31;break;
			default:;
				as_warn("Invalid scaled-indexed mode, use (b,w,d,q)");
				if (str[strl - 3] != ':' || str[strl - 6] != '[' ||
				    str[strl - 5] == 'r' || str[strl - 4] < '0' || str[strl - 4] > '7') {
					as_warn("Syntax in scaled-indexed mode, use [Rn:m] where n=[0..7] m={b,w,d,q}");
				}
			}  /* scaled index */
			{
				if (recursive_level>0) {
					as_warn("Scaled-indexed addressing mode combined with scaled-index");
					return(0);
				}
				addr_modeP->am_size+=1;		/* scaled index byte */
				j=str[strl-4]-'0';		/* store temporary */
				str[strl-6]='\000';		/* nullterminate for recursive call */
				i=addr_mode(str,addr_modeP,1);
				if (!i || addr_modeP->mode == 20) {
					as_warn("Invalid or illegal addressing mode combined with scaled-index");
					return(0);
				}
				addr_modeP->scaled_mode=addr_modeP->mode; /* store the inferior mode */
				addr_modeP->mode=mode;
				addr_modeP->scaled_reg=j+1;
				return (-1);
			}
		}
	}
	addr_modeP->mode = DEFAULT;	/* default to whatever */
	addr_modeP->disp[0]=str;
	return (-1);
}

/* ptr points at string
   addr_modeP points at struct with result
   This routine calls addr_mode to determine the general addr.mode of
   the operand. When this is ready it parses the displacements for size
   specifying suffixes and determines size of immediate mode via ns32k-opcode.
   Also builds index bytes if needed.
   */
int get_addr_mode(ptr,addr_modeP)
char *ptr;
addr_modeS *addr_modeP;
{
	int tmp;
	addr_mode(ptr,addr_modeP,0);
	if (addr_modeP->mode == DEFAULT || addr_modeP->scaled_mode == -1) {
		/* resolve ambigious operands, this shouldn't
		   be necessary if one uses standard NSC operand
		   syntax. But the sequent compiler doesn't!!!
		   This finds a proper addressinging mode if it
		   is implicitly stated. See ns32k-opcode.h */
		(void)evaluate_expr(&exprP,ptr); /* this call takes time Sigh! */
		if (addr_modeP->mode == DEFAULT) {
			if (exprP.X_add_symbol || exprP.X_subtract_symbol) {
				addr_modeP->mode=desc->default_model; /* we have a label */
			} else {
				addr_modeP->mode=desc->default_modec; /* we have a constant */
			}
		} else {
			if (exprP.X_add_symbol || exprP.X_subtract_symbol) {
				addr_modeP->scaled_mode=desc->default_model;
			} else {
				addr_modeP->scaled_mode=desc->default_modec;
			}
		}
		/* must put this mess down in addr_mode to handle the scaled case better */
	}
	/* It appears as the sequent compiler wants an absolute when we have a
	   label without @. Constants becomes immediates besides the addr case.
	   Think it does so with local labels too, not optimum, pcrel is better.
	   When I have time I will make gas check this and select pcrel when possible
	   Actually that is trivial.
	   */
	if (tmp=addr_modeP->scaled_reg) { /* build indexbyte */
		tmp--; /* remember regnumber comes incremented for flagpurpose */
		tmp|=addr_modeP->scaled_mode<<3;
		addr_modeP->index_byte=(char)tmp;
		addr_modeP->am_size+=1;
	}
	if (disp_test[addr_modeP->mode]) { /* there was a displacement, probe for length specifying suffix*/
		{
			register char c;
			register char suffix;
			register char suffix_sub;
			register int i;
			register char *toP;
			register char *fromP;

			addr_modeP->pcrel=0;
			if (disp_test[addr_modeP->mode]) { /* there is a displacement */
				if (addr_modeP->mode == 27 || addr_modeP->scaled_mode == 27) { /* do we have pcrel. mode */
					addr_modeP->pcrel=1;
				}
				addr_modeP->im_disp=1;
				for (i=0;i<2;i++) {
					suffix_sub=suffix=0;
					if (toP=addr_modeP->disp[i]) { /* suffix of expression, the largest size rules */
						fromP=toP;
						while (c = *fromP++) {
							*toP++=c;
							if (c == ':') {
								switch (*fromP) {
								case '\0':
									as_warn("Premature end of suffix--Defaulting to d");
									suffix=4;
									continue;
								case 'b':suffix_sub=1;break;
								case 'w':suffix_sub=2;break;
								case 'd':suffix_sub=4;break;
								default:
									as_warn("Bad suffix after ':' use {b|w|d} Defaulting to d");
									suffix=4;
								}
								fromP++;
								toP--; /* So we write over the ':' */
								if (suffix<suffix_sub) suffix=suffix_sub;
							}
						}
						*toP='\0'; /* terminate properly */
						addr_modeP->disp_suffix[i]=suffix;
						addr_modeP->am_size+=suffix ? suffix : 4;
					}
				}
			}
		}
	} else {
		if (addr_modeP->mode == 20) { /* look in ns32k_opcode for size */
			addr_modeP->disp_suffix[0]=addr_modeP->am_size=desc->im_size;
			addr_modeP->im_disp=0;
		}
	}
	return addr_modeP->mode;
}


/* read an optionlist */
void optlist(str,optionP,default_map)
char *str;			 /* the string to extract options from */
struct option *optionP;	 /* how to search the string */
unsigned long *default_map; /* default pattern and output */
{
	register int i,j,k,strlen1,strlen2;
	register char *patternP,*strP;
	strlen1=strlen(str);
	if (strlen1<1) {
		as_fatal("Very short instr to option, ie you can't do it on a NULLstr");
	}
	for (i = 0; optionP[i].pattern != 0; i++) {
		strlen2=strlen(optionP[i].pattern);
		for (j=0;j<strlen1;j++) {
			patternP=optionP[i].pattern;
			strP = &str[j];
			for (k=0;k<strlen2;k++) {
				if (*(strP++) != *(patternP++)) break;
			}
			if (k == strlen2) { /* match */
				*default_map|=optionP[i].or;
				*default_map&=optionP[i].and;
			}
		}
	}
}
/* search struct for symbols
   This function is used to get the short integer form of reg names
   in the instructions lmr, smr, lpr, spr
   return true if str is found in list */

int list_search(str,optionP,default_map)
char *str;			 /* the string to match */
struct option *optionP;	 /* list to search */
unsigned long *default_map; /* default pattern and output */
{
	register int i;
	for (i = 0; optionP[i].pattern != 0; i++) {
		if (!strncmp(optionP[i].pattern,str,20)) { /* use strncmp to be safe */
			*default_map|=optionP[i].or;
			*default_map&=optionP[i].and;
			return -1;
		}
	}
	as_warn("No such entry in list. (cpu/mmu register)");
	return 0;
}
static segT evaluate_expr(resultP,ptr)
expressionS *resultP;
char *ptr;
{
	register char *tmp_line;
	register segT segment;
	tmp_line = input_line_pointer;
	input_line_pointer = ptr;
	segment = expression(&exprP);
	input_line_pointer = tmp_line;
	return(segment);
}

/* Convert operands to iif-format and adds bitfields to the opcode.
   Operands are parsed in such an order that the opcode is updated from
   its most significant bit, that is when the operand need to alter the
   opcode.
   Be carefull not to put to objects in the same iif-slot.
   */

void encode_operand(argc,argv,operandsP,suffixP,im_size,opcode_bit_ptr)
int argc;
char **argv;
char *operandsP;
char *suffixP;
char im_size;
char opcode_bit_ptr;
{
	register int i,j;
	int pcrel,tmp,b,loop,pcrel_adjust;
	for (loop=0;loop<argc;loop++) {
		i=operandsP[loop<<1]-'1'; /* what operand are we supposed to work on */
		if (i>3) as_fatal("Internal consistency error.  check ns32k-opcode.h");
		pcrel=0;
		pcrel_adjust=0;
		tmp=0;
		switch (operandsP[(loop<<1)+1]) {
		case 'f':  /* operand of sfsr turns out to be a nasty specialcase */
			opcode_bit_ptr-=5;
		case 'F':		/* 32 bit float	general form */
		case 'L':		/* 64 bit float	*/
		case 'Q':		/* quad-word	*/
		case 'B':		/* byte	 */
		case 'W':		/* word	 */
		case 'D':		/* double-word	*/
		case 'A':		/* double-word	gen-address-form ie no regs allowed */
			get_addr_mode(argv[i],&addr_modeP);
			iif.instr_size+=addr_modeP.am_size;
			if (opcode_bit_ptr == desc->opcode_size) b = 4; else b = 6;
			for (j=b;j<(b+2);j++) {
				if (addr_modeP.disp[j-b]) {
					IIF(j,
					    2,
					    addr_modeP.disp_suffix[j-b],
					    (unsigned long)addr_modeP.disp[j-b],
					    0,
					    addr_modeP.pcrel,
					    iif.instr_size-addr_modeP.am_size, /* this aint used (now) */
					    addr_modeP.im_disp,
					    IND(BRANCH,BYTE),
					    NULL,
					    addr_modeP.scaled_reg ? addr_modeP.scaled_mode:addr_modeP.mode,
					    0);
				}
			}
			opcode_bit_ptr-=5;
			iif.iifP[1].object|=((long)addr_modeP.mode)<<opcode_bit_ptr;
			if (addr_modeP.scaled_reg) {
				j=b/2;
				IIF(j,1,1, (unsigned long)addr_modeP.index_byte,0,0,0,0,0, NULL,-1,0);
			}
			break;
		case 'b':		/* multiple instruction disp */
			freeptr++;	/* OVE:this is an useful hack */
			tmp = (int) sprintf(freeptr,
					    "((%s-1)*%d)\000",
					    argv[i], desc->im_size);
			argv[i]=freeptr;
			freeptr=(char*)tmp;
			pcrel-=1; /* make pcrel 0 inspite of what case 'p': wants */
			/* fall thru */
		case 'p':		/* displacement - pc relative addressing */
			pcrel+=1;
			/* fall thru */
		case 'd':					/* displacement */
			iif.instr_size+=suffixP[i] ? suffixP[i] : 4;
			IIF(12, 2, suffixP[i], (unsigned long)argv[i], 0,
			    pcrel, pcrel_adjust, 1, IND(BRANCH,BYTE), NULL,-1,0);
			break;
		case 'H': /* sequent-hack: the linker wants a bit set when bsr */
			pcrel=1;
			iif.instr_size+=suffixP[i] ? suffixP[i] : 4;
			IIF(12, 2, suffixP[i], (unsigned long)argv[i], 0,
			    pcrel, pcrel_adjust, 1, IND(BRANCH,BYTE), NULL,-1, 1);
			break;
		case 'q':					/* quick */
			opcode_bit_ptr-=4;
			IIF(11,2,42,(unsigned long)argv[i],0,0,0,0,0,
			    bit_fix_new(4,opcode_bit_ptr,-8,7,0,1,0),-1,0);
			break;
		case 'r':				/* register number (3 bits) */
			list_search(argv[i],opt6,&tmp);
			opcode_bit_ptr-=3;
			iif.iifP[1].object|=tmp<<opcode_bit_ptr;
			break;
		case 'O':				/* setcfg instruction optionslist */
			optlist(argv[i],opt3,&tmp);
			opcode_bit_ptr-=4;
			iif.iifP[1].object|=tmp<<15;
			break;
		case 'C':				/* cinv instruction optionslist */
			optlist(argv[i],opt4,&tmp);
			opcode_bit_ptr-=4;
			iif.iifP[1].object|=tmp<<15;/*insert the regtype in opcode */
			break;
		case 'S':				/* stringinstruction optionslist */
			optlist(argv[i],opt5,&tmp);
			opcode_bit_ptr-=4;
			iif.iifP[1].object|=tmp<<15;
			break;
		case 'u':case 'U':				/* registerlist */
			IIF(10,1,1,0,0,0,0,0,0,NULL,-1,0);
			switch (operandsP[(i<<1)+1]) {
			case 'u':			       	/* restore, exit */
				optlist(argv[i],opt1,&iif.iifP[10].object);
				break;
			case 'U':					/* save,enter */
				optlist(argv[i],opt2,&iif.iifP[10].object);
				break;
			}
			iif.instr_size+=1;
			break;
		case 'M':					/* mmu register */
			list_search(argv[i],mmureg,&tmp);
			opcode_bit_ptr-=4;
			iif.iifP[1].object|=tmp<<opcode_bit_ptr;
			break;
		case 'P':					/* cpu register  */
			list_search(argv[i],cpureg,&tmp);
			opcode_bit_ptr-=4;
			iif.iifP[1].object|=tmp<<opcode_bit_ptr;
			break;
		case 'g': /* inss exts */
			iif.instr_size+=1; /* 1 byte is allocated after the opcode */
			IIF(10,2,1,
			    (unsigned long)argv[i], /* i always 2 here */
			    0,0,0,0,0,
			    bit_fix_new(3,5,0,7,0,0,0), /* a bit_fix is targeted to the byte */
			    -1,0);
		case 'G':
			IIF(11,2,42,
			    (unsigned long)argv[i], /* i always 3 here */
			    0,0,0,0,0,
			    bit_fix_new(5,0,1,32,-1,0,-1),-1,0);
			break;
		case 'i':
			iif.instr_size+=1;
			b=2+i;  /* put the extension byte after opcode */
			IIF(b,2,1,0,0,0,0,0,0,0,-1,0);
		default:
			as_fatal("Bad opcode-table-option, check in file ns32k-opcode.h");
		}
	}
}

/* in:  instruction line
   out: internal structure of instruction
   that has been prepared for direct conversion to fragment(s) and
   fixes in a systematical fashion
   Return-value = recursive_level
   */
/* build iif of one assembly text line */
int parse(line,recursive_level)
char *line;
int recursive_level;
{
	register char			*lineptr,c,suffix_separator;
	register int			i;
	int				argc,arg_type;
	char				sqr,sep;
	char suffix[MAX_ARGS],*argv[MAX_ARGS];/* no more than 4 operands */
	if (recursive_level <= 0) { /* called from md_assemble */
		for (lineptr=line; (*lineptr) != '\0' && (*lineptr) != ' '; lineptr++);
		c = *lineptr;
		*lineptr = '\0';
		desc = (struct ns32k_opcode*) hash_find(inst_hash_handle,line);
		if (!desc) {
			as_fatal("No such opcode");
		}
		*lineptr = c;
	} else {
		lineptr = line;
	}
	argc = 0;
	if (*desc->operands != NULL) {
		if (*lineptr++ != '\0') {
			sqr='[';
			sep=',';
			while (*lineptr != '\0') {
				if (desc->operands[argc << 1]) {
					suffix[argc] = 0;
					arg_type =
					  desc->operands[(argc << 1) + 1];
					switch (arg_type) {
					case 'd':
					case 'b':
					case 'p':
					case 'H': /* the operand is supposed to be a displacement */
 /* Hackwarning: do not forget to update the 4 cases above when editing ns32k-opcode.h */
						suffix_separator = ':';
						break;
					default:
						suffix_separator = '\255'; /* if this char occurs we loose */
					}
					suffix[argc] = 0; /* 0 when no ':' is encountered */
					argv[argc] = freeptr;
					*freeptr = '\0';
					while ((c = *lineptr) != '\0' && c != sep) {
						if (c == sqr) {
							if (sqr == '[') {
								sqr = ']';
								sep = '\0';
							} else {
								sqr = '[';
								sep = ',';
							}
						}
						if (c == suffix_separator) { /* ':' - label/suffix separator */
							switch (lineptr[1]) {
							case 'b': suffix[argc] = 1; break;
							case 'w': suffix[argc] = 2; break;
							case 'd': suffix[argc] = 4; break;
							default: as_warn("Bad suffix, defaulting to d");
								suffix[argc] = 4;
								if (lineptr[1] == '\0' || lineptr[1] == sep) {
									lineptr += 1;
									continue;
								}
							}
							lineptr += 2;
							continue;
						}
						*freeptr++ = c;
						lineptr++;
					}
					*freeptr++ = '\0';
					argc += 1;
					if (*lineptr == '\0') continue;
					lineptr += 1;
				} else {
					as_fatal("Too many operands passed to instruction");
				}
			}
		}
	}
	if (argc != strlen(desc->operands) / 2) {
		if (strlen(desc->default_args) != 0) { /* we can apply default, dont goof */
			if (parse(desc->default_args,1) != 1) { /* check error in default */
				as_fatal("Wrong numbers of operands in default, check ns32k-opcodes.h");
			}
		} else {
			as_fatal("Wrong number of operands");
		}

	}
	for (i = 0; i < IIF_ENTRIES; i++) {
		iif.iifP[i].type = 0; /* mark all entries as void*/
	}

	/* build opcode iif-entry */
	iif.instr_size = desc->opcode_size / 8;
	IIF(1,1,iif.instr_size,desc->opcode_seed,0,0,0,0,0,0,-1,0);

	/* this call encodes operands to iif format */
	if (argc) {
		encode_operand(argc,
			       argv,
			       &desc->operands[0],
			       &suffix[0],
			       desc->im_size,
			       desc->opcode_size);
	}
	return(recursive_level);
}


/* Convert iif to fragments.
   From this point we start to dribble with functions in other files than
   this one.(Except hash.c) So, if it's possible to make an iif for an other
   CPU, you don't need to know what frags, relax, obstacks, etc is in order
   to port this assembler. You only need to know if it's possible to reduce
   your cpu-instruction to iif-format (takes some work) and adopt the other
   md_? parts according to given instructions
   Note that iif was invented for the clean ns32k`s architecure.
   */
void convert_iif() {
	int i;
	bit_fixS *bit_fixP;
	fragS *inst_frag;
	char *inst_offset;
	char *inst_opcode;
	char *memP;
	segT segment;
	int l;
	int k;
	int rem_size; /* count the remaining bytes of instruction */
	char type;
	char size = 0;
	int size_so_far = 0; /* used to calculate pcrel_adjust */
	int   pcrel_symbols = 0;/* kludge by jkp@hut.fi to make
 				   movd _foo(pc),_bar(pc) work.
 				   It should be done with two frags
 				   for one insn, but I don't understand
 				   enough to make it work */

	rem_size = iif.instr_size;
	memP = frag_more(iif.instr_size); /* make sure we have enough bytes for instruction */
	inst_opcode = memP;
	inst_offset = (char *)(memP-frag_now->fr_literal);
	inst_frag = frag_now;
	for (i=0; i < IIF_ENTRIES; i++) { /* jkp kludge alert */
	  if (iif.iifP[i].type && iif.iifP[i].size == 0 &&
	      iif.iifP[i].pcrel) {
	    evaluate_expr(&exprP, (char *)iif.iifP[i].object);
	    if (exprP.X_add_symbol || exprP.X_subtract_symbol)
	      pcrel_symbols++;
	  }
	}
	for (i=0;i<IIF_ENTRIES;i++) {
		if (type=iif.iifP[i].type) {			/* the object exist, so handle it */
#ifdef PIC
			int reloc_mode;
			if ((i == 4 || i == 6)
			    && picmode
			    && (iif.iifP[i].addr_mode == 18 || iif.iifP[i].addr_mode == 26))
				reloc_mode = RELOC_GLOB_DAT;
			else
				reloc_mode = NO_RELOC;
#else
			int reloc_mode = NO_RELOC;
#endif
			switch (size=iif.iifP[i].size) {
			case 42: size=0; /* it's a bitfix that operates on an existing object*/
				if (iif.iifP[i].bit_fixP->fx_bit_base) { /* expand fx_bit_base to point at opcode */
					iif.iifP[i].bit_fixP->fx_bit_base = (long) inst_opcode;
				}
			case 8: /* bignum or doublefloat */
				memset(memP, '\0', 8);
			case 1:case 2:case 3:case 4:/* the final size in objectmemory is known */
				bit_fixP = iif.iifP[i].bit_fixP;
				switch (type) {
				case 1:				/* the object is pure binary */
					if (bit_fixP || iif.iifP[i].pcrel) {
						fix_new_ns32k(frag_now,
							      (long)(memP-frag_now->fr_literal),
							      size,
							      0,
							      0,
							      iif.iifP[i].object,
							      iif.iifP[i].pcrel,
							      (char)size_so_far, /*iif.iifP[i].pcrel_adjust,*/
							      iif.iifP[i].im_disp,
							      bit_fixP,
							      iif.iifP[i].bsr, /* sequent hack */
							      reloc_mode);
					} else {			/* good, just put them bytes out */
						switch (iif.iifP[i].im_disp) {
						case 0:
							md_number_to_chars(memP,iif.iifP[i].object,size);break;
						case 1:
							md_number_to_disp(memP,iif.iifP[i].object,size);break;
						default: as_fatal("iif convert internal pcrel/binary");
						}
					}
					memP+=size;
					rem_size-=size;
					break;
				case 2:	/* the object is a pointer at an expression, so unpack
					   it, note that bignums may result from the expression
					   */
					if ((segment = evaluate_expr(&exprP, (char*)iif.iifP[i].object)) == SEG_BIG || size == 8) {
						if ((k=exprP.X_add_number)>0) { /* we have a bignum ie a quad */
							/* this can only happens in a long suffixed instruction */
							memset(memP, '\0', size); /* size normally is 8 */
							if (k*2>size) as_warn("Bignum too big for long");
							if (k == 3) memP += 2;
							for (l=0;k>0;k--,l+=2) {
								md_number_to_chars(memP+l,generic_bignum[l>>1],sizeof(LITTLENUM_TYPE));
							}
						} else { /* flonum */
							LITTLENUM_TYPE words[4];

							switch (size) {
							case 4:
								gen_to_words(words,2,8);
								md_number_to_imm(memP                       ,(long)words[0],sizeof(LITTLENUM_TYPE));
								md_number_to_imm(memP+sizeof(LITTLENUM_TYPE),(long)words[1],sizeof(LITTLENUM_TYPE));
								break;
							case 8:
								gen_to_words(words,4,11);
								md_number_to_imm(memP                         ,(long)words[0],sizeof(LITTLENUM_TYPE));
								md_number_to_imm(memP+sizeof(LITTLENUM_TYPE)  ,(long)words[1],sizeof(LITTLENUM_TYPE));
								md_number_to_imm(memP+2*sizeof(LITTLENUM_TYPE),(long)words[2],sizeof(LITTLENUM_TYPE));
								md_number_to_imm(memP+3*sizeof(LITTLENUM_TYPE),(long)words[3],sizeof(LITTLENUM_TYPE));
								break;
							}
						}
						memP+=size;
						rem_size-=size;
						break;
					}
					if (bit_fixP ||
					    exprP.X_add_symbol ||
					    exprP.X_subtract_symbol ||
					    iif.iifP[i].pcrel) {		/* fixit */
						/* the expression was undefined due to an undefined label */
						/* create a fix so we can fix the object later */
						exprP.X_add_number+=iif.iifP[i].object_adjust;
						fix_new_ns32k(frag_now,
							      (long)(memP-frag_now->fr_literal),
							      size,
							      exprP.X_add_symbol,
							      exprP.X_subtract_symbol,
							      exprP.X_add_number,
							      iif.iifP[i].pcrel,
							      (char)size_so_far, /*iif.iifP[i].pcrel_adjust,*/
							      iif.iifP[i].im_disp,
							      bit_fixP,
							      iif.iifP[i].bsr, /* sequent hack */
							      reloc_mode);

					} else {			/* good, just put them bytes out */
						switch (iif.iifP[i].im_disp) {
						case 0:
							md_number_to_imm(memP,exprP.X_add_number,size);break;
						case 1:
							md_number_to_disp(memP,exprP.X_add_number,size);break;
						default: as_fatal("iif convert internal pcrel/pointer");
						}
					}
					memP+=size;
					rem_size-=size;
					break;
				default: as_fatal("Internal logic error in iif.iifP[n].type");
				}
				break;
			case 0: 	 /* To bad, the object may be undefined as far as its final
					    nsize in object memory is concerned. The size of the object
					    in objectmemory is not explicitly given.
					    If the object is defined its length can be determined and
					    a fix can replace the frag.
					    */
				{
					int temp;
					segment = evaluate_expr(&exprP, (char*)iif.iifP[i].object);
					if (((exprP.X_add_symbol || exprP.X_subtract_symbol) &&
					    !iif.iifP[i].pcrel) || pcrel_symbols >= 2 /*jkp*/) { /* OVE: hack, clamp to 4 bytes */
#ifdef PIC
						if (reloc_mode == RELOC_GLOB_DAT && got_offset_size == 2) {
							size = 2;
							/* rewind the bytes not used */
							obstack_blank_fast(&frags, -2);
						} else
#endif
						size=4; /* we dont wan't to frag this, use 4 so it reaches */
						fix_new_ns32k(frag_now,
							      (long)(memP-frag_now->fr_literal),
							      size,
							      exprP.X_add_symbol,
							      exprP.X_subtract_symbol,
							      exprP.X_add_number,
							      pcrel_symbols >= 2 ? iif.iifP[i].pcrel : 0, /*jkp*//* never iif.iifP[i].pcrel, */
							      (char)size_so_far, /*iif.iifP[i].pcrel_adjust,*/
							      1, /* always iif.iifP[i].im_disp, */
							      0,0,
							      reloc_mode);
						memP+=size;
						rem_size-=4;
						break; /* exit this absolute hack */
					}

					if (exprP.X_add_symbol || exprP.X_subtract_symbol) { /* frag it */
						if (exprP.X_subtract_symbol) { /* We cant relax this case */
							as_fatal("Can't relax difference");
						} else {
							/* at this stage we must undo some of the effect caused
							   by frag_more, ie we must make sure that frag_var causes
							   frag_new to creat a valid fix-size in the frag it`s closing
							   */
							temp = -(rem_size-4);
							obstack_blank_fast(&frags,temp);
							/* we rewind none, some or all of the requested size we
							   requested by the first frag_more for this iif chunk.
							   Note: that we allocate 4 bytes to an object we NOT YET
							   know the size of, thus rem_size-4.
							   */
							(void) frag_variant(rs_machine_dependent,
									    4,
									    0,
									    IND(BRANCH,UNDEF), /* expecting the worst */
									    exprP.X_add_symbol,
									    exprP.X_add_number,
									    inst_opcode,
									    (char)size_so_far, /*iif.iifP[i].pcrel_adjust);*/
									    iif.iifP[i].bsr); /* sequent linker hack */
							rem_size -= 4;
							if (rem_size > 0) {
								memP = frag_more(rem_size);
							}
						}
					} else {/* Double work, this is done in md_number_to_disp */
						/* exprP.X_add_number; fixme-soon what was this supposed to be? xoxorich. */
						if (-64 <= exprP.X_add_number && exprP.X_add_number <= 63) {
							size = 1;
						} else {
							if (-8192 <= exprP.X_add_number && exprP.X_add_number <= 8191) {
								size = 2;

								/* Dave Taylor <taylor@think.com> says: Note: The reason the lower
								   limit is -0x1f000000 and not -0x20000000 is that, according to
								   Nat'l Semi's data sheet on the ns32532, ``the pattern 11100000
								   for the most significant byte of the displacement is reserved by
								   National for future enhancements''.  */
							} else if (/* -0x40000000 <= exprP.X_add_number &&
								    exprP.X_add_number <= 0x3fffffff */
								   -0x1f000000 <= exprP.X_add_number &&
								    exprP.X_add_number <= 0x1fffffff) {
								size = 4;
							} else {
								as_warn("Displacement too large for :d");
								size = 4;
							}
						}
						/* rewind the bytes not used */
						temp = -(4-size);
						md_number_to_disp(memP,exprP.X_add_number,size);
						obstack_blank_fast(&frags,temp);
						memP += size;
						rem_size -= 4; /* we allocated this amount */
					}
				}
				break;
			default:
				as_fatal("Internal logic error in iif.iifP[].type");
			}
			size_so_far += size;
			size = 0;
		}
	}
}

void md_assemble(line)
char *line;
{
	freeptr=freeptr_static;
	parse(line,0); /* explode line to more fix form in iif */
	convert_iif(); /* convert iif to frags, fix's etc */
#ifdef SHOW_NUM
	printf(" \t\t\t%s\n",line);
#endif
}


void md_begin() {
	/* build a hashtable of the instructions */
	register const struct ns32k_opcode *ptr;
	register char *stat;
	inst_hash_handle=hash_new();
	for (ptr=ns32k_opcodes;ptr<endop;ptr++) {
		if (*(stat=hash_insert(inst_hash_handle,ptr->name,(char*)ptr))) {
			as_fatal("Can't hash %s: %s", ptr->name,stat); /*fatal*/
			exit(0);
		}
	}
	freeptr_static=(char*)malloc(PRIVATE_SIZE); /* some private space please! */
}


void
    md_end() {
	    free(freeptr_static);
    }

/* Must be equal to MAX_PRECISON in atof-ieee.c */
#define MAX_LITTLENUMS 6

/* Turn the string pointed to by litP into a floating point constant of type
   type, and emit the appropriate bytes.  The number of LITTLENUMS emitted
   is stored in *sizeP. An error message is returned, or NULL on OK.
   */
char *
    md_atof(type,litP,sizeP)
char type;
char *litP;
int *sizeP;
{
	int	prec;
	LITTLENUM_TYPE words[MAX_LITTLENUMS];
	LITTLENUM_TYPE *wordP;
	extern char *atof_ieee();
	char *t;

	switch (type) {
	case 'f':
		prec = 2;
		break;

	case 'd':
		prec = 4;
		break;
	default:
		*sizeP = 0;
		return "Bad call to MD_ATOF()";
	}
	t = atof_ieee(input_line_pointer, type, words);
	if (t)
	    input_line_pointer=t;

	*sizeP = prec * sizeof(LITTLENUM_TYPE);
	for (wordP = words +prec; prec--;) {
		md_number_to_chars(litP, (long)(*--wordP), sizeof(LITTLENUM_TYPE));
		litP+=sizeof(LITTLENUM_TYPE);
	}
	return "";	/* Someone should teach Dean about null pointers */
}

/* Convert number to chars in correct order */

void
    md_number_to_chars(buf, value, nbytes)
char *buf;
long value;
int nbytes;
{
	while (nbytes--) {
#ifdef SHOW_NUM
		printf("%x ",value & 0xff);
#endif
		*buf++ = value;		/* Lint wants & MASK_CHAR. */
		value >>= BITS_PER_CHAR;
	}
} /* md_number_to_chars() */


/* This is a variant of md_numbers_to_chars. The reason for its' existence
   is the fact that ns32k uses Huffman coded displacements. This implies
   that the bit order is reversed in displacements and that they are prefixed
   with a size-tag.

   binary: msb->lsb
   0xxxxxxx				byte
   10xxxxxx xxxxxxxx			word
   11xxxxxx xxxxxxxx xxxxxxxx xxxxxxxx	double word

   This must be taken care of and we do it here!
   */
static void md_number_to_disp(buf, val, n)
char *buf;
long val;
char n;
{
	switch (n) {
	case 1:
		if (val < -64 || val > 63)
{
fprintf(stderr, "val = %d\n", val);
		    as_warn("Byte displacement out of range.  line number not valid");
}
		val &= 0x7f;
#ifdef SHOW_NUM
		printf("%x ",val & 0xff);
#endif
		*buf++ = val;
		break;
	case 2:
		if (val < -8192 || val > 8191)
		    as_warn("Word displacement out of range.  line number not valid");
		val&=0x3fff;
		val|=0x8000;
#ifdef SHOW_NUM
		printf("%x ",val>>8 & 0xff);
#endif
		*buf++=(val>>8);
#ifdef SHOW_NUM
		printf("%x ",val & 0xff);
#endif
		*buf++=val;
		break;
	case 4:

		/* Dave Taylor <taylor@think.com> says: Note: The reason the
		   lower limit is -0x1f000000 and not -0x20000000 is that,
		   according to Nat'l Semi's data sheet on the ns32532, ``the
		   pattern 11100000 for the most significant byte of the
		   displacement is reserved by National for future
		   enhancements''.  */

		if (val < -0x1f000000 || val >= 0x20000000)
		    as_warn("Double word displacement out of range");
		val|=0xc0000000;
#ifdef SHOW_NUM
		printf("%x ",val>>24 & 0xff);
#endif
		*buf++=(val>>24);
#ifdef SHOW_NUM
		printf("%x ",val>>16 & 0xff);
#endif
		*buf++=(val>>16);
#ifdef SHOW_NUM
		printf("%x ",val>>8 & 0xff);
#endif
		*buf++=(val>>8);
#ifdef SHOW_NUM
		printf("%x ",val & 0xff);
#endif
		*buf++=val;
		break;
	default:
		as_fatal("Internal logic error.  line %s, file \"%s\"", __LINE__, __FILE__);
	}
}

static void md_number_to_imm(buf,val,n)
char	*buf;
long	val;
char       n;
{
	switch (n) {
	case 1:
#ifdef SHOW_NUM
		printf("%x ",val & 0xff);
#endif
		*buf++=val;
		break;
	case 2:
#ifdef SHOW_NUM
		printf("%x ",val>>8 & 0xff);
#endif
		*buf++=(val>>8);
#ifdef SHOW_NUM
		printf("%x ",val & 0xff);
#endif
		*buf++=val;
		break;
	case 4:
#ifdef SHOW_NUM
		printf("%x ",val>>24 & 0xff);
#endif
		*buf++=(val>>24);
#ifdef SHOW_NUM
		printf("%x ",val>>16 & 0xff);
#endif
		*buf++=(val>>16);
#ifdef SHOW_NUM
		printf("%x ",val>>8 & 0xff);
#endif
		*buf++=(val>>8);
#ifdef SHOW_NUM
		printf("%x ",val & 0xff);
#endif
		*buf++=val;
		break;
	default:
		as_fatal("Internal logic error. line %s, file \"%s\"", __LINE__, __FILE__);
	}
}

/* Translate internal representation of relocation info into target format.

   OVE: on a ns32k the twiddling continues at an even deeper level
   here we have to distinguish between displacements and immediates.

   The sequent has a bit for this. It also has a bit for relocobjects that
   points at the target for a bsr (BranchSubRoutine) !?!?!?!

   This md_ri.... is tailored for sequent.
   */

#ifdef comment
void
    md_ri_to_chars(the_bytes, ri)
char *the_bytes;
struct reloc_info_generic *ri;
{
	if (ri->r_bsr) { ri->r_pcrel = 0; } /* sequent seems to want this */
	md_number_to_chars(the_bytes, ri->r_address, sizeof(ri->r_address));
	md_number_to_chars(the_bytes+4, ((long)(ri->r_symbolnum )
					 | (long)(ri->r_pcrel      << 24 )
					 | (long)(ri->r_length	   << 25 )
					 | (long)(ri->r_extern	   << 27 )
					 | (long)(ri->r_bsr	   << 28 )
					 | (long)(ri->r_disp	   << 29 )),
			   4);
	/* the first and second md_number_to_chars never overlaps (32bit cpu case) */
}
#endif /* comment */

#ifdef OBJ_AOUT
void tc_aout_fix_to_chars(where, fixP, segment_address_in_file)
char *where;
fixS *fixP;
relax_addressT segment_address_in_file;
{
	/*
	 * In: length of relocation (or of address) in chars: 1, 2 or 4.
	 * Out: GNU LD relocation length code: 0, 1, or 2.
	 */

	static unsigned char nbytes_r_length[] = { 42, 0, 1, 42, 2 };
	long r_symbolnum;
	int r_flags;

	know(fixP->fx_addsy != NULL);

	md_number_to_chars(where,
			   fixP->fx_frag->fr_address + fixP->fx_where - segment_address_in_file,
			   4);

	r_symbolnum = (S_IS_DEFINED(fixP->fx_addsy)
		       ? S_GET_TYPE(fixP->fx_addsy)
		       : fixP->fx_addsy->sy_number);
	r_flags = (fixP->fx_pcrel ? 1 : 0)
	    | ((nbytes_r_length[fixP->fx_size] & 3) << 1)
	    | (!S_IS_DEFINED(fixP->fx_addsy) ? 8 : 0)
#if defined(TE_SEQUENT)
	    | (fixP->fx_bsr ? 0x10 : 0)
#elif defined(PIC)
	    /* Undefined pc-relative relocations are of type jmpslot */
	    | ((!S_IS_DEFINED(fixP->fx_addsy)
	       && fixP->fx_pcrel
	       && fixP->fx_addsy != got_symbol
	       && picmode) ? 0x10 : 0)
#endif
	    | (fixP->fx_im_disp & 3) << 5;

#ifdef	PIC
	switch (fixP->fx_r_type) {
	case NO_RELOC:
	    break;
	case RELOC_32:
	    if (picmode && S_IS_EXTERNAL(fixP->fx_addsy)) {
		r_symbolnum = fixP->fx_addsy->sy_number;
		r_flags |= 8;	/* set extern bit */
	    }
	    break;
	case RELOC_GLOB_DAT:
	    if (!fixP->fx_pcrel) {
		r_flags |= 0x80;	    /* set baserel bit */
		r_symbolnum = fixP->fx_addsy->sy_number;
		if (S_IS_EXTERNAL(fixP->fx_addsy))
		    r_flags |= 8;
	    }
	    break;
	case RELOC_RELATIVE:
	    /* should never happen */
	    as_fatal("relocation botch");
	    break;
	}
#endif	/* PIC */

	where[4] = r_symbolnum & 0x0ff;
	where[5] = (r_symbolnum >> 8) & 0x0ff;
	where[6] = (r_symbolnum >> 16) & 0x0ff;
	where[7] = r_flags;

	return;
} /* tc_aout_fix_to_chars() */


#endif /* OBJ_AOUT */

/* fast bitfiddling support */
/* mask used to zero bitfield before oring in the true field */

static unsigned long l_mask[] = {
	0xffffffff, 0xfffffffe, 0xfffffffc, 0xfffffff8,
	0xfffffff0, 0xffffffe0, 0xffffffc0, 0xffffff80,
	0xffffff00, 0xfffffe00, 0xfffffc00, 0xfffff800,
	0xfffff000, 0xffffe000, 0xffffc000, 0xffff8000,
	0xffff0000, 0xfffe0000, 0xfffc0000, 0xfff80000,
	0xfff00000, 0xffe00000, 0xffc00000, 0xff800000,
	0xff000000, 0xfe000000, 0xfc000000, 0xf8000000,
	0xf0000000, 0xe0000000, 0xc0000000, 0x80000000,
};
static unsigned long r_mask[] = {
	0x00000000, 0x00000001, 0x00000003, 0x00000007,
	0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
	0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
	0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
	0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
	0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
	0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
	0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
};
#define MASK_BITS 31
/* Insert bitfield described by field_ptr and val at buf
   This routine is written for modification of the first 4 bytes pointed
   to by buf, to yield speed.
   The ifdef stuff is for selection between a ns32k-dependent routine
   and a general version. (My advice: use the general version!)
   */

static void
    md_number_to_field(buf,val,field_ptr)
register char	*buf;
register long	val;
register bit_fixS  *field_ptr;
{
	register unsigned long object;
	register unsigned long mask;
	/* define ENDIAN on a ns32k machine */
#ifdef ENDIAN
	register unsigned long *mem_ptr;
#else
	register char *mem_ptr;
#endif
	if (field_ptr->fx_bit_min <= val && val <= field_ptr->fx_bit_max) {
#ifdef ENDIAN
		if (field_ptr->fx_bit_base) { /* override buf */
			mem_ptr=(unsigned long*)field_ptr->fx_bit_base;
		} else {
			mem_ptr=(unsigned long*)buf;
		}
#else
		if (field_ptr->fx_bit_base) { /* override buf */
			mem_ptr=(char*)field_ptr->fx_bit_base;
		} else {
			mem_ptr=buf;
		}
#endif
		mem_ptr+=field_ptr->fx_bit_base_adj;
#ifdef ENDIAN  /* we have a nice ns32k machine with lowbyte at low-physical mem */
		object = *mem_ptr; /* get some bytes */
#else /* OVE Goof! the machine is a m68k or dito */
		/* That takes more byte fiddling */
		object=0;
		object|=mem_ptr[3] & 0xff;
		object<<=8;
		object|=mem_ptr[2] & 0xff;
		object<<=8;
		object|=mem_ptr[1] & 0xff;
		object<<=8;
		object|=mem_ptr[0] & 0xff;
#endif
		mask=0;
		mask|=(r_mask[field_ptr->fx_bit_offset]);
		mask|=(l_mask[field_ptr->fx_bit_offset+field_ptr->fx_bit_size]);
		object&=mask;
		val+=field_ptr->fx_bit_add;
		object|=((val<<field_ptr->fx_bit_offset) & (mask ^ 0xffffffff));
#ifdef ENDIAN
		*mem_ptr=object;
#else
		mem_ptr[0]=(char)object;
		object>>=8;
		mem_ptr[1]=(char)object;
		object>>=8;
		mem_ptr[2]=(char)object;
		object>>=8;
		mem_ptr[3]=(char)object;
#endif
	} else {
		as_warn("Bit field out of range");
	}
}

/* Apply a fixS (fixup of an instruction or data that we didn't have
   enough info to complete immediately) to the data in a frag.

   On the ns32k, everything is in a different format, so we have broken
   out separate functions for each kind of thing we could be fixing.
   They all get called from here.  */

void
    md_apply_fix(fixP, val)
fixS *fixP;
long val;
{
	char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;

	if (fixP->fx_bit_fixP) {	/* Bitfields to fix, sigh */
		md_number_to_field (buf, val, fixP->fx_bit_fixP);
	} else switch (fixP->fx_im_disp) {

	case 0:			/* Immediate field */
		md_number_to_imm (buf, val, fixP->fx_size);
		break;

	case 1:			/* Displacement field */
		md_number_to_disp (buf,
				   fixP->fx_pcrel? val + fixP->fx_pcrel_adjust: val,
				   fixP->fx_size);
		break;

	case 2:			/* Pointer in a data object */
		md_number_to_chars (buf, val, fixP->fx_size);
		break;
	}
}

/* Convert a relaxed displacement to ditto in final output */

void
    md_convert_frag(headers, fragP)
object_headers *headers;
register fragS *fragP;
{
	long disp;
	long ext = 0;

	/* Address in gas core of the place to store the displacement.  */
	register char *buffer_address = fragP->fr_fix + fragP->fr_literal;
	/* Address in object code of the displacement.  */
	register int object_address = fragP->fr_fix + fragP->fr_address;

	know(fragP->fr_symbol);

	/* The displacement of the address, from current location.  */
	disp = (S_GET_VALUE(fragP->fr_symbol) + fragP->fr_offset) - object_address;
	disp += fragP->fr_pcrel_adjust;

	switch (fragP->fr_subtype) {
	case IND(BRANCH,BYTE):
	    ext = 1;
	    break;
	case IND(BRANCH,WORD):
	    ext = 2;
	    break;
	case IND(BRANCH,DOUBLE):
	    ext = 4;
	    break;
	}
	if (ext) {
		md_number_to_disp(buffer_address, (long)disp, (int)ext);
		fragP->fr_fix += ext;
	}
} /* md_convert_frag() */



/* This function returns the estimated size a variable object will occupy,
   one can say that we tries to guess the size of the objects before we
   actually know it */

int md_estimate_size_before_relax(fragP, segment)
register fragS *fragP;
segT segment;
{
	int	old_fix;
	old_fix = fragP->fr_fix;
	switch (fragP->fr_subtype) {
	case IND(BRANCH,UNDEF):
	    if (S_GET_SEGMENT(fragP->fr_symbol) == segment) {
		    /* the symbol has been assigned a value */
		    fragP->fr_subtype = IND(BRANCH,BYTE);
	    } else {
		    /* we don't relax symbols defined in an other segment
		       the thing to do is to assume the object will occupy 4 bytes */
		    fix_new_ns32k(fragP,
				  (int)(fragP->fr_fix),
				  4,
				  fragP->fr_symbol,
				  (symbolS *)0,
				  fragP->fr_offset,
				  1,
				  fragP->fr_pcrel_adjust,
				  1,
				  0,
				  fragP->fr_bsr, /*sequent hack */
				  NO_RELOC);
		    fragP->fr_fix+=4;
		    /* fragP->fr_opcode[1]=0xff; */
		    frag_wane(fragP);
		    break;
	    }
    case IND(BRANCH,BYTE):
	fragP->fr_var+=1;
	    break;
    default:
	    break;
    }
	return fragP->fr_var + fragP->fr_fix - old_fix;
}

int md_short_jump_size = 3;
int md_long_jump_size  = 5;
const int md_reloc_size = 8;		/* Size of relocation record */

void
    md_create_short_jump(ptr,from_addr,to_addr,frag,to_symbol)
char	*ptr;
long	from_addr,
    to_addr;
fragS	*frag;
symbolS	*to_symbol;
{
	long offset;

	offset = to_addr - from_addr;
	md_number_to_chars(ptr, (long)0xEA  ,1);
	md_number_to_disp(ptr+1,(long)offset,2);
}

void
    md_create_long_jump(ptr,from_addr,to_addr,frag,to_symbol)
char	*ptr;
long	from_addr,
    to_addr;
fragS	*frag;
symbolS	*to_symbol;
{
	long offset;

	offset= to_addr - from_addr;
	md_number_to_chars(ptr, (long)0xEA,  2);
	md_number_to_disp(ptr+2,(long)offset,4);
}

/* JF this is a new function to parse machine-dep options */
int
    md_parse_option(argP,cntP,vecP)
char **argP;
int *cntP;
char ***vecP;
{
	switch (**argP) {
	case 'm':
		(*argP)++;

		if (!strcmp(*argP,"32032")) {
			cpureg = cpureg_032;
			mmureg = mmureg_032;
		} else if (!strcmp(*argP, "32532")) {
			cpureg = cpureg_532;
			mmureg = mmureg_532;
		} else
		    as_warn("Unknown -m option ignored");

		while (**argP)
		    (*argP)++;
		break;

#ifdef PIC
	case 'K':
		got_offset_size = 4;
		/*FALLTHROUGH*/
	case 'k':
		got_symbol = symbol_find_or_make("__GLOBAL_OFFSET_TABLE_");
		break;
#endif

	default:
		return 0;
	}
	return 1;
}

/*
 *			bit_fix_new()
 *
 * Create a bit_fixS in obstack 'notes'.
 * This struct is used to profile the normal fix. If the bit_fixP is a
 * valid pointer (not NULL) the bit_fix data will be used to format the fix.
 */
bit_fixS *bit_fix_new(size, offset, min, max, add, base_type, base_adj)
char	size;		/* Length of bitfield		*/
char	offset;		/* Bit offset to bitfield	*/
long	base_type;	/* 0 or 1, if 1 it's exploded to opcode ptr */
long	base_adj;
long	min;		/* Signextended min for bitfield */
long	max;		/* Signextended max for bitfield */
long	add;		/* Add mask, used for huffman prefix */
{
	register bit_fixS *	bit_fixP;

	bit_fixP = (bit_fixS *)obstack_alloc(&notes,sizeof(bit_fixS));

	bit_fixP->fx_bit_size	= size;
	bit_fixP->fx_bit_offset	= offset;
	bit_fixP->fx_bit_base	= base_type;
	bit_fixP->fx_bit_base_adj	= base_adj;
	bit_fixP->fx_bit_max	= max;
	bit_fixP->fx_bit_min	= min;
	bit_fixP->fx_bit_add	= add;

	return(bit_fixP);
}

void
    fix_new_ns32k(frag, where, size, add_symbol, sub_symbol, offset, pcrel,
		   pcrel_adjust, im_disp, bit_fixP, bsr, r_type)
fragS *frag;	     /* Which frag? */
int where;	     /* Where in that frag? */
int size;	     /* 1, 2  or 4 usually. */
symbolS *add_symbol; /* X_add_symbol. */
symbolS *sub_symbol; /* X_subtract_symbol. */
long offset;	     /* X_add_number. */
int pcrel;	     /* TRUE if PC-relative relocation. */
char pcrel_adjust;   /* not zero if adjustment of pcrel offset is needed */
char im_disp;	     /* true if the value to write is a displacement */
bit_fixS *bit_fixP;  /* pointer at struct of bit_fix's, ignored if NULL */
char bsr;	     /* sequent-linker-hack: 1 when relocobject is a bsr */
int r_type;	     /* Relocation type */

{
#ifdef	PIC
	fixS *fixP = fix_new(frag, where, size, add_symbol, sub_symbol,
			     offset, pcrel, r_type, NULL);
#else
	fixS *fixP = fix_new(frag, where, size, add_symbol, sub_symbol,
			     offset, pcrel, r_type);
#endif	/* PIC */
	fixP->fx_pcrel_adjust = pcrel_adjust;
	fixP->fx_im_disp = im_disp;
	fixP->fx_bit_fixP = bit_fixP;
	fixP->fx_bsr = bsr;
#ifdef	PIC
	if (r_type == RELOC_GLOB_DAT)
		add_symbol->sy_forceout = 1;
#endif	/* PIC */
} /* fix_new_ns32k() */

/* We have no need to default values of symbols.  */

symbolS *
    md_undefined_symbol (name)
char *name;
{
	return 0;
}

/* Parse an operand that is machine-specific.
   We just return without modifying the expression if we have nothing
   to do.  */

/* ARGSUSED */
void
    md_operand (expressionP)
expressionS *expressionP;
{
}

/* Round up a section size to the appropriate boundary.  */
long
    md_section_align (segment, size)
segT segment;
long size;
{
	return size;		/* Byte alignment is fine */
}

/* Exactly what point is a PC-relative offset relative TO?
   On the National warts, they're relative to the address of the offset,
   with some funny adjustments in some circumstances during blue moons.
   (??? Is this right?  FIXME-SOON) */
long
    md_pcrel_from (fixP)
fixS *fixP;
{
	long res;
	res = fixP->fx_where + fixP->fx_frag->fr_address;
#ifdef TE_SEQUENT
	if (fixP->fx_frag->fr_bsr)
	    res += 0x12; /* FOO Kludge alert! */
#endif
		return(res);
}

/*
 * Local Variables:
 * comment-column: 0
 * End:
 */

/* end of tc-ns32k.c */
