/* tc-m68k.c  All the m68020 specific stuff in one convenient, huge,
   slow to compile, easy to find file.
   
   Copyright (C) 1987, 1991, 1992 Free Software Foundation, Inc.
   
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

#include <ctype.h>

#include "as.h"

#include "obstack.h"

/* note that this file includes real declarations and thus can only be included by one source file per executable. */
#include "opcode/m68k.h"
#ifdef TE_SUN
/* This variable contains the value to write out at the beginning of
   the a.out file.  The 2<<16 means that this is a 68020 file instead
   of an old-style 68000 file */

long omagic = 2<<16|OMAGIC;	/* Magic byte for header file */
#else
long omagic = OMAGIC;
#endif

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful */
const char comment_chars[] = "|";

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output. */
/* Also note that comments like this one will always work. */
const char line_comment_chars[] = "#";

/* Chars that can be used to separate mant from exp in floating point nums */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant */
/* As in 0f12.456 */
/* or    0d1.2345e12 */

const char FLT_CHARS[] = "rRsSfFdDxXeEpP";

/* Also be aware that MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT may have to be
   changed in read.c. Ideally it shouldn't have to know about it at all,
   but nothing is ideal around here.
   */

int md_reloc_size = 8;		/* Size of relocation record */

/* Its an arbitrary name:  This means I don't approve of it */
/* See flames below */
static struct obstack robyn;

#define TAB(x,y)	(((x)<<2)+(y))
#define TABTYPE(xy)     ((xy) >> 2)
#define BYTE		0
#define SHORT		1
#define LONG		2
#define SZ_UNDEF	3

#define BRANCH		1
#define FBRANCH		2
#define PCREL		3
#define BCC68000        4
#define DBCC            5
#define PCLEA		6

/* Operands we can parse:  (And associated modes)
   
   numb:	8 bit num
   numw:	16 bit num
   numl:	32 bit num
   dreg:	data reg 0-7
   reg:	address or data register
   areg:	address register
   apc:	address register, PC, ZPC or empty string
   num:	16 or 32 bit num
   num2:	like num
   sz:	w or l		if omitted, l assumed
   scale:	1 2 4 or 8	if omitted, 1 assumed
   
   7.4 IMMED #num				--> NUM
   0.? DREG  dreg				--> dreg
   1.? AREG  areg				--> areg
   2.? AINDR areg@				--> *(areg)
   3.? AINC  areg@+			--> *(areg++)
   4.? ADEC  areg@-			--> *(--areg)
   5.? AOFF  apc@(numw)			--> *(apc+numw)	-- empty string and ZPC not allowed here
   6.? AINDX apc@(num,reg:sz:scale)	--> *(apc+num+reg*scale)
   6.? AINDX apc@(reg:sz:scale)		--> same, with num=0
   6.? APODX apc@(num)@(num2,reg:sz:scale)	--> *(*(apc+num)+num2+reg*scale)
   6.? APODX apc@(num)@(reg:sz:scale)	--> same, with num2=0
   6.? AMIND apc@(num)@(num2)		--> *(*(apc+num)+num2) (previous mode without an index reg)
   6.? APRDX apc@(num,reg:sz:scale)@(num2)	--> *(*(apc+num+reg*scale)+num2)
   6.? APRDX apc@(reg:sz:scale)@(num2)	--> same, with num=0
   7.0 ABSL  num:sz			--> *(num)
   num				--> *(num) (sz L assumed)
   *** MSCR  otherreg			--> Magic
   With -l option
   5.? AOFF  apc@(num)			--> *(apc+num) -- empty string and ZPC not allowed here still
   
   examples:
   #foo	#0x35	#12
   d2
   a4
   a3@
   a5@+
   a6@-
   a2@(12)	pc@(14)
   a1@(5,d2:w:1)	@(45,d6:l:4)
   pc@(a2)		@(d4)
   etc...
   
   
   #name@(numw)	-->turn into PC rel mode
   apc@(num8,reg:sz:scale)		--> *(apc+num8+reg*scale)
   
   */

enum operand_type {
	IMMED = 1,
	DREG,
	AREG,
	AINDR,
	ADEC,
	AINC,
	AOFF,
	AINDX,
	APODX,
	AMIND,
	APRDX,
	ABSL,
	MSCR,
	REGLST,
};


struct m68k_exp {
	char	*e_beg;
	char	*e_end;
	expressionS e_exp;
	short	e_siz;		/* 0 == default 1 == short/byte 2 == word 3 == long */
};

/* DATA and ADDR have to be contiguous, so that reg-DATA gives 0-7 == data reg,
   8-15 == addr reg for operands that take both types */

enum _register {
	DATA = 1,		/*   1- 8 == data registers 0-7 */
	DATA0 = DATA,
	DATA1,
	DATA2,
	DATA3,
	DATA4,
	DATA5,
	DATA6,
	DATA7,
	
	ADDR,
	ADDR0 = ADDR,
	ADDR1,
	ADDR2,
	ADDR3,
	ADDR4,
	ADDR5,
	ADDR6,
	ADDR7,
	
	/* Note that COPNUM == processor #1 -- COPNUM+7 == #8, which stores as 000 */
	/* I think...  */
	
	SP = ADDR7,
	
	FPREG, /* Eight FP registers */
	FP0 = FPREG,
	FP1,
	FP2,
	FP3,
	FP4,
	FP5,
	FP6,
	FP7,
	COPNUM = (FPREG+8),	/* Co-processor #1-#8 */
	COP0 = COPNUM,
	COP1,
	COP2,
	COP3,
	COP4,
	COP5,
	COP6,
	COP7,
	PC,	/* Program counter */
	ZPC, /* Hack for Program space, but 0 addressing */
	SR,	/* Status Reg */
	CCR, /* Condition code Reg */
	
	/* These have to be in order for the movec instruction to work. */
	USP,	/*  User Stack Pointer */
	ISP,	/*  Interrupt stack pointer */
	SFC,
	DFC,
	CACR,
	VBR,
	CAAR,
	MSP,
	ITT0,
	ITT1,
	DTT0,
	DTT1,
	MMUSR,
	TC,
	SRP,
	URP,
	/* end of movec ordering constraints */
	
	FPI,
	FPS,
	FPC,
	
	DRP,
	CRP,
	CAL,
	VAL,
	SCC,
	AC,
	BAD,
	BAD0 = BAD,
	BAD1,
	BAD2,
	BAD3,
	BAD4,
	BAD5,
	BAD6,
	BAD7,
	BAC,
	BAC0 = BAC,
	BAC1,
	BAC2,
	BAC3,
	BAC4,
	BAC5,
	BAC6,
	BAC7,
	PSR,
	PCSR,
	
	IC,	/* instruction cache token */
	DC,	/* data cache token */
	NC,	/* no cache token */
	BC,	/* both caches token */
	
};

/* Internal form of an operand.  */
struct m68k_op {
	char	*error;		/* Couldn't parse it */
	enum operand_type mode;	/* What mode this instruction is in.  */
	enum _register reg;		/* Base register */
	struct m68k_exp *con1;
	int	ireg;		/* Index register */
	int	isiz;		/* 0 == unspec  1 == byte(?)  2 == short  3 == long  */
	int	imul;		/* Multipy ireg by this (1,2,4,or 8) */
	struct	m68k_exp *con2;
};

/* internal form of a 68020 instruction */
struct m68k_it {
	char	*error;
	char	*args;		/* list of opcode info */
	int	numargs;
	
	int	numo;		/* Number of shorts in opcode */
	short	opcode[11];
	
	struct m68k_op operands[6];
	
	int	nexp;		/* number of exprs in use */
	struct m68k_exp exprs[4];
	
	int	nfrag;		/* Number of frags we have to produce */
	struct {
		int fragoff;	/* Where in the current opcode[] the frag ends */
		symbolS *fadd;
		long foff;
		int fragty;
	} fragb[4];
	
	int	nrel;		/* Num of reloc strucs in use */
	struct	{
		int	n;
		symbolS	*add,
		*sub;
		long off;
		char	wid;
		char	pcrel;
	} reloc[5];		/* Five is enough??? */
};

#define cpu_of_arch(x)		((x) & m68000up)
#define float_of_arch(x)	((x) & mfloat)
#define mmu_of_arch(x)		((x) & mmmu)

static struct m68k_it the_ins;		/* the instruction being assembled */

/* Macros for adding things to the m68k_it struct */

#define addword(w)	the_ins.opcode[the_ins.numo++]=(w)

/* Like addword, but goes BEFORE general operands */
#define insop(w)	{int z;\
			     for (z=the_ins.numo;z>opcode->m_codenum;--z)\
				 the_ins.opcode[z]=the_ins.opcode[z-1];\
				     for (z=0;z<the_ins.nrel;z++)\
					 the_ins.reloc[z].n+=2;\
					     the_ins.opcode[opcode->m_codenum]=w;\
						 the_ins.numo++;\
					     }


#define add_exp(beg,end) (\
			  the_ins.exprs[the_ins.nexp].e_beg=beg,\
			  the_ins.exprs[the_ins.nexp].e_end=end,\
			  &the_ins.exprs[the_ins.nexp++]\
			  )


/* The numo+1 kludge is so we can hit the low order byte of the prev word. Blecch*/
#define add_fix(width,exp,pc_rel) {\
				       the_ins.reloc[the_ins.nrel].n= ((width) == 'B') ? (the_ins.numo*2-1) : \
					   (((width) == 'b') ? ((the_ins.numo-1)*2) : (the_ins.numo*2));\
					       the_ins.reloc[the_ins.nrel].add=adds((exp));\
						   the_ins.reloc[the_ins.nrel].sub=subs((exp));\
						       the_ins.reloc[the_ins.nrel].off=offs((exp));\
							   the_ins.reloc[the_ins.nrel].wid=width;\
							       the_ins.reloc[the_ins.nrel++].pcrel=pc_rel;\
							   }

#define add_frag(add,off,type)  {\
				     the_ins.fragb[the_ins.nfrag].fragoff=the_ins.numo;\
					 the_ins.fragb[the_ins.nfrag].fadd=add;\
					     the_ins.fragb[the_ins.nfrag].foff=off;\
						 the_ins.fragb[the_ins.nfrag++].fragty=type;\
					     }

#define isvar(exp)	((exp) && (adds(exp) || subs(exp)))

#define seg(exp)	((exp)->e_exp.X_seg)
#define adds(exp)	((exp)->e_exp.X_add_symbol)
#define subs(exp)	((exp)->e_exp.X_subtract_symbol)
#define offs(exp)	((exp)->e_exp.X_add_number)


struct m68k_incant {
	char *m_operands;
	unsigned long m_opcode;
	short m_opnum;
	short m_codenum;
	enum m68k_architecture m_arch;
	struct m68k_incant *m_next;
};

#define getone(x)	((((x)->m_opcode)>>16)&0xffff)
#define gettwo(x)	(((x)->m_opcode)&0xffff)


#if __STDC__ == 1

static char *crack_operand(char *str, struct m68k_op *opP);
static int get_num(struct m68k_exp *exp, int ok);
static int get_regs(int i, char *str, struct m68k_op *opP);
static int reverse_16_bits(int in);
static int reverse_8_bits(int in);
static int try_index(char **s, struct m68k_op *opP);
static void install_gen_operand(int mode, int val);
static void install_operand(int mode, int val);
static void s_bss(void);
static void s_data1(void);
static void s_data2(void);
static void s_even(void);
static void s_proc(void);

#else /* not __STDC__ */

static char *crack_operand();
static int get_num();
static int get_regs();
static int reverse_16_bits();
static int reverse_8_bits();
static int try_index();
static void install_gen_operand();
static void install_operand();
static void s_bss();
static void s_data1();
static void s_data2();
static void s_even();
static void s_proc();

#endif /* not __STDC__ */

static enum m68k_architecture current_architecture = 0;

/* BCC68000 is for patching in an extra jmp instruction for long offsets
   on the 68000.  The 68000 doesn't support long branches with branchs */

/* This table desribes how you change sizes for the various types of variable
   size expressions.  This version only supports two kinds. */

/* Note that calls to frag_var need to specify the maximum expansion needed */
/* This is currently 10 bytes for DBCC */

/* The fields are:
   How far Forward this mode will reach:
   How far Backward this mode will reach:
   How many bytes this mode will add to the size of the frag
   Which mode to go to if the offset won't fit in this one
   */
const relax_typeS
    md_relax_table[] = {
	    { 1,		1,		0,	0 },	/* First entries aren't used */
	    { 1,		1,		0,	0 },	/* For no good reason except */
	    { 1,		1,		0,	0 },	/* that the VAX doesn't either */
	    { 1,		1,		0,	0 },
	    
	    { (127),	(-128),		0,	TAB(BRANCH,SHORT)},
	    { (32767),	(-32768),	2,	TAB(BRANCH,LONG) },
	    { 0,		0,		4,	0 },
	    { 1,		1,		0,	0 },
	    
	    { 1,		1,		0,	0 },	/* FBRANCH doesn't come BYTE */
	    { (32767),	(-32768),	2,	TAB(FBRANCH,LONG)},
	    { 0,		0,		4,	0 },
	    { 1,		1,		0,	0 },
	    
	    { 1,		1,		0,	0 },	/* PCREL doesn't come BYTE */
	    { (32767),	(-32768),	2,	TAB(PCREL,LONG)},
	    { 0,		0,		4,	0 },
	    { 1,		1,		0,	0 },
	    
	    { (127),	(-128),		0,	TAB(BCC68000,SHORT)},
	    { (32767),	(-32768),	2,	TAB(BCC68000,LONG) },
	    { 0,		0,		6,	0 },	/* jmp long space */
	    { 1,		1,		0,	0 },
	    
	    { 1,		1,		0,	0 },	/* DBCC doesn't come BYTE */
	    { (32767),	(-32768),	2,	TAB(DBCC,LONG) },
	    { 0,		0,		10,	0 },	/* bra/jmp long space */
	    { 1,		1,		0,	0 },
	    
	    { 1,		1,		0,	0 },	/* PCLEA doesn't come BYTE */
	    { 32767,	-32768,		2,	TAB(PCLEA,LONG) },
	    { 0,		0,		6,	0 },
	    { 1,		1,		0,	0 },
	    
    };

/* These are the machine dependent pseudo-ops.  These are included so
   the assembler can work on the output from the SUN C compiler, which
   generates these.
   */

/* This table describes all the machine specific pseudo-ops the assembler
   has to support.  The fields are:
   pseudo-op name without dot
   function to call to execute this pseudo-op
   Integer arg to pass to the function
   */
const pseudo_typeS md_pseudo_table[] = {
	{ "data1",	s_data1,	0	},
	{ "data2",	s_data2,	0	},
	{ "bss",	s_bss,		0	},
	{ "even",	s_even,		0	},
	{ "skip",	s_space,	0	},
	{ "proc",	s_proc,		0	},
	{ 0,		0,		0	}
};


/* #define isbyte(x)	((x) >= -128 && (x) <= 127) */
/* #define isword(x)	((x) >= -32768 && (x) <= 32767) */

#define issbyte(x)	((x) >= -128 && (x) <= 127)
#define isubyte(x)	((x) >= 0 && (x) <= 255)
#define issword(x)	((x) >= -32768 && (x) <= 32767)
#define isuword(x)	((x) >= 0 && (x) <= 65535)

#define isbyte(x)	((x) >= -128 && (x) <= 255)
#define isword(x)	((x) >= -32768 && (x) <= 65535)
#define islong(x)	(1)

extern char *input_line_pointer;

enum {
	FAIL = 0,
	OK = 1,
};

/* JF these tables here are for speed at the expense of size */
/* You can replace them with the #if 0 versions if you really
   need space and don't mind it running a bit slower */

static char mklower_table[256];
#define mklower(c) (mklower_table[(unsigned char)(c)])
static char notend_table[256];
static char alt_notend_table[256];
#define notend(s) (!(notend_table[(unsigned char)(*s)] || (*s == ':' &&\
							    alt_notend_table[(unsigned char)(s[1])])))

#if 0
#define mklower(c)	(isupper(c) ? tolower(c) : c)
#endif


/* JF modified this to handle cases where the first part of a symbol name
   looks like a register */

/*
 * m68k_reg_parse() := if it looks like a register, return it's token &
 * advance the pointer.
 */

enum _register m68k_reg_parse(ccp)
register char **ccp;
{
#ifndef MAX_REG_NAME_LEN
#define MAX_REG_NAME_LEN (6)
#endif /* MAX_REG_NAME_LEN */
	register char c[MAX_REG_NAME_LEN];
	char *p, *q;
	register int n = 0,
	ret = FAIL;
	
	c[0] = mklower(ccp[0][0]);
#ifdef REGISTER_PREFIX
	if (c[0] != REGISTER_PREFIX) {
		return(FAIL);
	} /* need prefix */
#endif
	
	for (p = c, q = ccp[0]; p < c + MAX_REG_NAME_LEN; ++p, ++q)
	    {
		    if (*q == 0)
			{
				*p = 0;
				break;
			}
		    else
			*p = mklower(*q);
	    } /* downcase */
	
	switch (c[0]) {
	case 'a':
		if (c[1] >= '0' && c[1] <= '7') {
			n=2;
			ret=ADDR+c[1]-'0';
		}
#ifndef NO_68851
		else if (c[1] == 'c') {
			n = 2;
			ret = AC;
		}
#endif
		break;
#ifndef NO_68851
	case 'b':
		if (c[1] == 'a') {
			if (c[2] == 'd') {
				if (c[3] >= '0' && c[3] <= '7') {
					n = 4;
					ret = BAD + c[3] - '0';
				}
			} /* BAD */
			if (c[2] == 'c') {
				if (c[3] >= '0' && c[3] <= '7') {
					n = 4;
					ret = BAC + c[3] - '0';
				}
			} /* BAC */
		} else if (c[1] == 'c') {
			n = 2;
			ret = BC;
		} /* BC */
		break;
#endif
	case 'c':
#ifndef NO_68851
		if (c[1] == 'a' && c[2] == 'l') {
			n = 3;
			ret = CAL;
		} else
#endif
		    /* This supports both CCR and CC as the ccr reg. */
		    if (c[1] == 'c' && c[2] == 'r') {
			    n=3;
			    ret = CCR;
		    } else if (c[1] == 'c') {
			    n=2;
			    ret = CCR;
		    } else if (c[1] == 'a' && (c[2] == 'a' || c[2] == 'c') && c[3] == 'r') {
			    n=4;
			    ret = c[2] == 'a' ? CAAR : CACR;
		    }
#ifndef NO_68851
		    else if (c[1] == 'r' && c[2] == 'p') {
			    n = 3;
			    ret = (CRP);
		    }
#endif
		break;
	case 'd':
		if (c[1] >= '0' && c[1] <= '7') {
			n = 2;
			ret = DATA + c[1] - '0';
		} else if (c[1] == 'f' && c[2] == 'c') {
			n = 3;
			ret = DFC;
		} else if (c[1] == 'c') {
			n = 2;
			ret = DC;
		} else if (c[1] == 't' && c[2] == 't') {
			if ('0' <= c[3] && c[3] <= '1') {
				n = 4;
				ret = DTT0 + (c[3] - '0');
			} /* DTT[01] */
		}
#ifndef NO_68851
		else if (c[1] == 'r' && c[2] == 'p') {
			n = 3;
			ret = (DRP);
		}
#endif
		break;
	case 'f':
		if (c[1] == 'p') {
			if (c[2] >= '0' && c[2] <= '7') {
				n=3;
				ret = FPREG+c[2]-'0';
				if (c[3] == ':')
				    ccp[0][3]=',';
			} else if (c[2] == 'i') {
				n=3;
				ret = FPI;
			} else if (c[2] == 's') {
				n= (c[3] == 'r' ? 4 : 3);
				ret = FPS;
			} else if (c[2] == 'c') {
				n= (c[3] == 'r' ? 4 : 3);
				ret = FPC;
			}
		}
		break;
	case 'i':
		if (c[1] == 's' && c[2] == 'p') {
			n = 3;
			ret = ISP;
		} else if (c[1] == 'c') {
			n = 2;
			ret = IC;
		} else if (c[1] == 't' && c[2] == 't') {
			if ('0' <= c[3] && c[3] <= '1') {
				n = 4;
				ret = ITT0 + (c[3] - '0');
			} /* ITT[01] */
		}
		break;
	case 'm':
		if (c[1] == 's' && c[2] == 'p') {
			n = 3;
			ret = MSP;
		} else if (c[1] == 'm' && c[2] == 'u' && c[3] == 's' && c[4] == 'r') {
			n = 5;
			ret = MMUSR;
		}
		break;
	case 'n':
		if (c[1] == 'c') {
			n = 2;
			ret = NC;
		}
		break;
	case 'p':
		if (c[1] == 'c') {
#ifndef NO_68851
			if (c[2] == 's' && c[3] == 'r') {
				n=4;
				ret = (PCSR);
			} else
#endif
			    {
				    n=2;
				    ret = PC;
			    }
		}
#ifndef NO_68851
		else if (c[1] == 's' && c[2] == 'r') {
			n = 3;
			ret = (PSR);
		}
#endif
		break;
	case 's':
#ifndef NO_68851
		if (c[1] == 'c' && c[2] == 'c') {
			n = 3;
			ret = (SCC);
		} else 
#endif
		    if (c[1] == 'r') {
			    if (c[2] == 'p') {
				    n = 3;
				    ret = SRP;
			    } else {
				    n = 2;
				    ret = SR;
			    } /* srp else sr */
		    } else if (c[1] == 'p') {
			    n = 2;
			    ret = SP;
		    } else if (c[1] == 'f' && c[2] == 'c') {
			    n = 3;
			    ret = SFC;
		    }
		break;
	case 't':
		if (c[1] == 'c') {
			n = 2;
			ret = TC;
		}
		break;
	case 'u':
		if (c[1] == 's' && c[2] == 'p') {
			n=3;
			ret = USP;
		} else if (c[1] == 'r' && c[2] == 'p') {
			n = 3;
			ret = URP;
		}
		break;
	case 'v':
#ifndef NO_68851
		if (c[1] == 'a' && c[2] == 'l') {
			n = 3;
			ret = (VAL);
		} else
#endif
		    if (c[1] == 'b' && c[2] == 'r') {
			    n=3;
			    ret = VBR;
		    }
		break;
	case 'z':
		if (c[1] == 'p' && c[2] == 'c') {
			n=3;
			ret = ZPC;
		}
		break;
	default:
		break;
	}
	if (n) {
#ifdef REGISTER_PREFIX
		n++;
#endif
		if (isalnum(ccp[0][n]) || ccp[0][n] == '_')
		    ret=FAIL;
		else
		    ccp[0]+=n;
	} else
	    ret = FAIL;
	return ret;
}

#define SKIP_WHITE()	{ str++; if (*str == ' ') str++;}

/*
 * m68k_ip_op := '#' + <anything>
 *	| <register> + range_sep + get_regs
 *	;
 * 
 * range_sep := '/' | '-' ;
 *
 * SKIP_WHITE := <empty> | ' ' ;
 *
 */

int
    m68k_ip_op(str,opP)
char *str;
register struct m68k_op *opP;
{
	char	*strend;
	long	i;
	char	*parse_index();
	
	if (*str == ' ') {
		str++;
	} /* Find the beginning of the string */
	
	if (!*str) {
		opP->error="Missing operand";
		return FAIL;
	} /* Out of gas */
	
	for (strend = str; *strend; strend++) ;;
	
	--strend;
	
	if (*str == '#') {
		str++;
		opP->con1=add_exp(str,strend);
		opP->mode=IMMED;
		return OK;
	} /* Guess what:  A constant.  Shar and enjoy */
	
	i = m68k_reg_parse(&str);
	
	/* is a register, is exactly a register, and is followed by '@' */
	
	if ((i == FAIL || *str != '\0') && *str != '@') {
		char *stmp;
		
		if (i != FAIL && (*str == '/' || *str == '-')) {
			opP->mode=REGLST;
			return(get_regs(i,str,opP));
		}
		if ((stmp=strchr(str,'@')) != '\0') {
			opP->con1=add_exp(str,stmp-1);
			if (stmp == strend) {
				opP->mode=AINDX;
				return(OK);
			}
			
			if ((current_architecture & m68020up) == 0) {
				return(FAIL);
			} /* if target is not a '20 or better */
			
			stmp++;
			if (*stmp++ != '(' || *strend-- != ')') {
				opP->error="Malformed operand";
				return(FAIL);
			}
			i=try_index(&stmp,opP);
			opP->con2=add_exp(stmp,strend);
			
			if (i == FAIL) {
				opP->mode=AMIND;
			} else {
				opP->mode=APODX;
			}
			return(OK);
		} /* if there's an '@' */
		opP->mode = ABSL;
		opP->con1 = add_exp(str,strend);
		return(OK);
	} /* not a register, not exactly a register, or no '@' */
	
	opP->reg=i;
	
	if (*str == '\0') {
		if (i >= DATA+0 && i <= DATA+7)
		    opP->mode=DREG;
		else if (i >= ADDR+0 && i <= ADDR+7)
		    opP->mode=AREG;
		else
		    opP->mode=MSCR;
		return OK;
	}
	
	if ((i<ADDR+0 || i>ADDR+7) && i != PC && i != ZPC && i != FAIL) {	/* Can't indirect off non address regs */
		opP->error="Invalid indirect register";
		return FAIL;
	}
	know(*str == '@');
	
	str++;
	switch (*str) {
	case '\0':
		opP->mode=AINDR;
		return OK;
	case '-':
		opP->mode=ADEC;
		return OK;
	case '+':
		opP->mode=AINC;
		return OK;
	case '(':
		str++;
		break;
	default:
		opP->error="Junk after indirect";
		return FAIL;
	}
	/* Some kind of indexing involved.  Lets find out how bad it is */
	i=try_index(&str,opP);
	/* Didn't start with an index reg, maybe its offset or offset,reg */
	if (i == FAIL) {
		char *beg_str;
		
		beg_str=str;
		for (i=1;i;) {
			switch (*str++) {
			case '\0':
				opP->error="Missing )";
				return FAIL;
			case ',': i=0; break;
			case '(': i++; break;
			case ')': --i; break;
			}
		}
		/* if (str[-3] == ':') {
		   int siz;
		   
		   switch (str[-2]) {
		   case 'b':
		   case 'B':
		   siz=1;
		   break;
		   case 'w':
		   case 'W':
		   siz=2;
		   break;
		   case 'l':
		   case 'L':
		   siz=3;
		   break;
		   default:
		   opP->error="Specified size isn't :w or :l";
		   return FAIL;
		   }
		   opP->con1=add_exp(beg_str,str-4);
		   opP->con1->e_siz=siz;
		   } else */
		opP->con1=add_exp(beg_str,str-2);
		/* Should be offset,reg */
		if (str[-1] == ',') {
			i=try_index(&str,opP);
			if (i == FAIL) {
				opP->error="Malformed index reg";
				return FAIL;
			}
		}
	}
	/* We've now got offset)   offset,reg)   or    reg) */
	
	if (*str == '\0') {
		/* Th-the-thats all folks */
		if (opP->reg == FAIL) opP->mode = AINDX;	/* Other form of indirect */
		else if (opP->ireg == FAIL) opP->mode = AOFF;
		else opP->mode = AINDX;
		return(OK);
	}
	/* Next thing had better be another @ */
	if (*str != '@' || str[1] != '(') {
		opP->error = "junk after indirect";
		return(FAIL);
	}
	
	if ((current_architecture & m68020up) == 0) {
		return(FAIL);
	} /* if target is not a '20 or better */
	
	str+=2;
	
	if (opP->ireg != FAIL) {
		opP->mode = APRDX;
		
		i = try_index(&str, opP);
		if (i != FAIL) {
			opP->error = "Two index registers!  not allowed!";
			return(FAIL);
		}
	} else {
		i = try_index(&str, opP);
	}
	
	if (i == FAIL) {
		char *beg_str;
		
		beg_str = str;
		
		for (i = 1; i; ) {
			switch (*str++) {
			case '\0':
				opP->error="Missing )";
				return(FAIL);
			case ',': i=0; break;
			case '(': i++; break;
			case ')': --i; break;
			}
		}
		
		opP->con2=add_exp(beg_str,str-2);
		
		if (str[-1] == ',') {
			if (opP->ireg != FAIL) {
				opP->error = "Can't have two index regs";
				return(FAIL);
			}
			
			i = try_index(&str, opP);
			
			if (i == FAIL) {
				opP->error = "malformed index reg";
				return(FAIL);
			}
			
			opP->mode = APODX;
		} else if (opP->ireg != FAIL) {
			opP->mode = APRDX;
		} else {
			opP->mode = AMIND;
		}
	} else {
		opP->mode = APODX;
	}
	
	if (*str != '\0') {
		opP->error="Junk after indirect";
		return FAIL;
	}
	return(OK);
} /* m68k_ip_op() */

/*
 * 
 * try_index := data_or_address_register + ')' + SKIP_W
 *	| data_or_address_register + ':' + SKIP_W + size_spec + SKIP_W + multiplier + ')' + SKIP_W
 *
 * multiplier := <empty>
 *	| ':' + multiplier_number
 *	;
 *
 * multiplier_number := '1' | '2' | '4' | '8' ;
 *
 * size_spec := 'l' | 'L' | 'w' | 'W' ;
 *
 * SKIP_W := <empty> | ' ' ;
 *
 */

static int try_index(s,opP)
char **s;
struct m68k_op *opP;
{
	register int	i;
	char	*ss;
#define SKIP_W()	{ ss++; if (*ss == ' ') ss++;}
	
	ss= *s;
	/* SKIP_W(); */
	i=m68k_reg_parse(&ss);
	if (!(i >= DATA+0 && i <= ADDR+7)) {	/* if i is not DATA or ADDR reg */
		*s=ss;
		return FAIL;
	}
	opP->ireg=i;
	/* SKIP_W(); */
	if (*ss == ')') {
		opP->isiz=0;
		opP->imul=1;
		SKIP_W();
		*s=ss;
		return OK;
	}
	if (*ss != ':') {
		opP->error="Missing : in index register";
		*s=ss;
		return FAIL;
	}
	SKIP_W();
	switch (*ss) {
	case 'w':
	case 'W':
		opP->isiz=2;
		break;
	case 'l':
	case 'L':
		opP->isiz=3;
		break;
	default:
		opP->error="Index register size spec not :w or :l";
		*s=ss;
		return FAIL;
	}
	SKIP_W();
	if (*ss == ':') {
		SKIP_W();
		switch (*ss) {
		case '1':
		case '2':
		case '4':
		case '8':
			opP->imul= *ss-'0';
			break;
		default:
			opP->error="index multiplier not 1, 2, 4 or 8";
			*s=ss;
			return FAIL;
		}
		SKIP_W();
	} else opP->imul=1;
	if (*ss != ')') {
		opP->error="Missing )";
		*s=ss;
		return FAIL;
	}
	SKIP_W();
	*s=ss;
	return OK;
} /* try_index() */

#ifdef TEST1	/* TEST1 tests m68k_ip_op(), which parses operands */
main()
{
	char buf[128];
	struct m68k_op thark;
	
	for (;;) {
		if (!gets(buf))
		    break;
		memset(&thark, '\0', sizeof(thark));
		if (!m68k_ip_op(buf,&thark)) printf("FAIL:");
		if (thark.error)
		    printf("op1 error %s in %s\n",thark.error,buf);
		printf("mode %d, reg %d, ",thark.mode,thark.reg);
		if (thark.b_const)
		    printf("Constant: '%.*s',",1+thark.e_const-thark.b_const,thark.b_const);
		printf("ireg %d, isiz %d, imul %d ",thark.ireg,thark.isiz,thark.imul);
		if (thark.b_iadd)
		    printf("Iadd: '%.*s'",1+thark.e_iadd-thark.b_iadd,thark.b_iadd);
		printf("\n");
	}
	exit(0);
}

#endif


static struct hash_control*   op_hash = NULL;	/* handle of the OPCODE hash table
						   NULL means any use before m68k_ip_begin()
						   will crash */


/*
 *		m 6 8 k _ i p ( )
 *
 * This converts a string into a 68k instruction.
 * The string must be a bare single instruction in sun format
 * with RMS-style 68020 indirects
 *  (example:  )
 *
 * It provides some error messages: at most one fatal error message (which
 * stops the scan) and at most one warning message for each operand.
 * The 68k instruction is returned in exploded form, since we have no
 * knowledge of how you parse (or evaluate) your expressions.
 * We do however strip off and decode addressing modes and operation
 * mnemonic.
 *
 * This function's value is a string. If it is not "" then an internal
 * logic error was found: read this code to assign meaning to the string.
 * No argument string should generate such an error string:
 * it means a bug in our code, not in the user's text.
 *
 * You MUST have called m68k_ip_begin() once and m86_ip_end() never before using
 * this function.
 */

/* JF this function no longer returns a useful value.  Sorry */
void m68k_ip (instring)
char *instring;
{
	register char *p;
	register struct m68k_op *opP;
	register struct m68k_incant *opcode;
	register char *s;
	register int tmpreg = 0,
	baseo = 0,
	outro = 0,
	nextword;
	int	siz1,
	siz2;
	char	c;
	int	losing;
	int	opsfound;
	char	*crack_operand();
	LITTLENUM_TYPE words[6];
	LITTLENUM_TYPE *wordp;
	
	if (*instring == ' ')
	    instring++;			/* skip leading whitespace */
	
	/* Scan up to end of operation-code, which MUST end in end-of-string
	   or exactly 1 space. */
	for (p = instring; *p != '\0'; p++)
	    if (*p == ' ')
		break;
	
	
	if (p == instring) {
		the_ins.error = "No operator";
		the_ins.opcode[0] = NULL;
		/* the_ins.numo=1; */
		return;
	}
	
	/* p now points to the end of the opcode name, probably whitespace.
	   make sure the name is null terminated by clobbering the whitespace,
	   look it up in the hash table, then fix it back. */   
	c = *p;
	*p = '\0';
	opcode = (struct m68k_incant *)hash_find (op_hash, instring);
	*p = c;
	
	if (opcode == NULL) {
		the_ins.error = "Unknown operator";
		the_ins.opcode[0] = NULL;
		/* the_ins.numo=1; */
		return;
	}
	
	/* found a legitimate opcode, start matching operands */
	while (*p == ' ') ++p;
	
	for (opP = &the_ins.operands[0]; *p; opP++) {
		
		p = crack_operand(p, opP);
		
		if (opP->error) {
			the_ins.error=opP->error;
			return;
		}
	}
	
	opsfound = opP - &the_ins.operands[0];
	
	/* This ugly hack is to support the floating pt opcodes in their standard form */
	/* Essentially, we fake a first enty of type COP#1 */
	if (opcode->m_operands[0] == 'I') {
		int	n;
		
		for (n=opsfound;n>0;--n)
		    the_ins.operands[n]=the_ins.operands[n-1];
		
		/* memcpy((char *)(&the_ins.operands[1]), (char *)(&the_ins.operands[0]), opsfound*sizeof(the_ins.operands[0])); */
		memset((char *)(&the_ins.operands[0]), '\0', sizeof(the_ins.operands[0]));
		the_ins.operands[0].mode=MSCR;
		the_ins.operands[0].reg=COPNUM;		/* COP #1 */
		opsfound++;
	}
	
	/* We've got the operands.  Find an opcode that'll accept them */
	for (losing = 0; ; ) {
		/* if we didn't get the right number of ops,
		   or we have no common model with this pattern
		   then reject this pattern. */
		
		if (opsfound != opcode->m_opnum
		    || ((opcode->m_arch & current_architecture) == 0)) {
			
			++losing;
			
		} else {
			for (s=opcode->m_operands, opP = &the_ins.operands[0]; *s && !losing; s += 2, opP++) {
				/* Warning: this switch is huge! */
				/* I've tried to organize the cases into  this order:
				   non-alpha first, then alpha by letter.  lower-case goes directly
				   before uppercase counterpart. */
				/* Code with multiple case ...: gets sorted by the lowest case ...
				   it belongs to.  I hope this makes sense. */
				switch (*s) {
				case '!':
					if (opP->mode == MSCR || opP->mode == IMMED
					    || opP->mode == DREG || opP->mode == AREG
					    || opP->mode == AINC || opP->mode == ADEC
					    || opP->mode == REGLST)
					    losing++;
					break;
					
				case '#':
					if (opP->mode != IMMED)
					    losing++;
					else {
						long t;
						
						t=get_num(opP->con1,80);
						if (s[1] == 'b' && !isbyte(t))
						    losing++;
						else if (s[1] == 'w' && !isword(t))
						    losing++;
					}
					break;
					
				case '^':
				case 'T':
					if (opP->mode != IMMED)
					    losing++;
					break;
					
				case '$':
					if (opP->mode == MSCR || opP->mode == AREG ||
					   opP->mode == IMMED || opP->reg == PC || opP->reg == ZPC || opP->mode == REGLST)
					    losing++;
					break;
					
				case '%':
					if (opP->mode == MSCR || opP->reg == PC ||
					   opP->reg == ZPC || opP->mode == REGLST)
					    losing++;
					break;
					
					
				case '&':
					if (opP->mode == MSCR || opP->mode == DREG ||
					   opP->mode == AREG || opP->mode == IMMED || opP->reg == PC || opP->reg == ZPC ||
					   opP->mode == AINC || opP->mode == ADEC || opP->mode == REGLST)
					    losing++;
					break;
					
				case '*':
					if (opP->mode == MSCR || opP->mode == REGLST)
					    losing++;
					break;
					
				case '+':
					if (opP->mode != AINC)
					    losing++;
					break;
					
				case '-':
					if (opP->mode != ADEC)
					    losing++;
					break;
					
				case '/':
					if (opP->mode == MSCR || opP->mode == AREG ||
					   opP->mode == AINC || opP->mode == ADEC || opP->mode == IMMED || opP->mode == REGLST)
					    losing++;
					break;
					
				case ';':
					if (opP->mode == MSCR || opP->mode == AREG || opP->mode == REGLST)
					    losing++;
					break;
					
				case '?':
					if (opP->mode == MSCR || opP->mode == AREG ||
					   opP->mode == AINC || opP->mode == ADEC || opP->mode == IMMED || opP->reg == PC ||
					   opP->reg == ZPC || opP->mode == REGLST)
					    losing++;
					break;
					
				case '@':
					if (opP->mode == MSCR || opP->mode == AREG ||
					   opP->mode == IMMED || opP->mode == REGLST)
					    losing++;
					break;
					
				case '~':		/* For now! (JF FOO is this right?) */
					if (opP->mode == MSCR || opP->mode == DREG ||
					   opP->mode == AREG || opP->mode == IMMED || opP->reg == PC || opP->reg == ZPC || opP->mode == REGLST)
					    losing++;
					break;
					
				case 'A':
					if (opP->mode != AREG)
					    losing++;
					break;
				case 'a':
					if (opP->mode != AINDR) {
						++losing;
					} /* if not address register indirect */
					break;
				case 'B':	/* FOO */
					if (opP->mode != ABSL || (flagseen['S'] && instring[0] == 'j'
							       && instring[1] == 'b'
							       && instring[2] == 's'
							       && instring[3] == 'r'))
					    losing++;
					break;
					
				case 'C':
					if (opP->mode != MSCR || opP->reg != CCR)
					    losing++;
					break;
					
				case 'd':	/* FOO This mode is a KLUDGE!! */
					if (opP->mode != AOFF && (opP->mode != ABSL ||
							       opP->con1->e_beg[0] != '(' || opP->con1->e_end[0] != ')'))
					    losing++;
					break;
					
				case 'D':
					if (opP->mode != DREG)
					    losing++;
					break;
					
				case 'F':
					if (opP->mode != MSCR || opP->reg<(FPREG+0) || opP->reg>(FPREG+7))
					    losing++;
					break;
					
				case 'I':
					if (opP->mode != MSCR || opP->reg<COPNUM ||
					   opP->reg >= COPNUM+7)
					    losing++;
					break;
					
				case 'J':
					if (opP->mode != MSCR
					    || opP->reg < USP
					    || opP->reg > URP
					    || cpu_of_arch(current_architecture) < m68010 /* before 68010 had none */
					    || (cpu_of_arch(current_architecture) < m68020
						&& opP->reg != SFC
						&& opP->reg != DFC
						&& opP->reg != USP
						&& opP->reg != VBR) /* 68010's had only these */
					    || (cpu_of_arch(current_architecture) < m68040
						&& opP->reg != SFC
						&& opP->reg != DFC
						&& opP->reg != USP
						&& opP->reg != VBR
						&& opP->reg != CACR
						&& opP->reg != CAAR
						&& opP->reg != MSP
						&& opP->reg != ISP) /* 680[23]0's have only these */
					    || (cpu_of_arch(current_architecture) == m68040 /* 68040 has all but this */
						&& opP->reg == CAAR)) {
						losing++;
					} /* doesn't cut it */
					break;
					
				case 'k':
					if (opP->mode != IMMED)
					    losing++;
					break;
					
				case 'l':
				case 'L':
					if (opP->mode == DREG || opP->mode == AREG || opP->mode == FPREG) {
						if (s[1] == '8')
						    losing++;
						else {
							opP->mode=REGLST;
							opP->reg=1<<(opP->reg-DATA);
						}
					} else if (opP->mode != REGLST) {
						losing++;
					} else if (s[1] == '8' && opP->reg&0x0FFffFF)
					    losing++;
					else if (s[1] == '3' && opP->reg&0x7000000)
					    losing++;
					break;
					
				case 'M':
					if (opP->mode != IMMED)
					    losing++;
					else {
						long t;
						
						t=get_num(opP->con1,80);
						if (!issbyte(t) || isvar(opP->con1))
						    losing++;
					}
					break;
					
				case 'O':
					if (opP->mode != DREG && opP->mode != IMMED)
					    losing++;
					break;
					
				case 'Q':
					if (opP->mode != IMMED)
					    losing++;
					else {
						long t;
						
						t=get_num(opP->con1,80);
						if (t<1 || t>8 || isvar(opP->con1))
						    losing++;
					}
					break;
					
				case 'R':
					if (opP->mode != DREG && opP->mode != AREG)
					    losing++;
					break;
					
				case 's':
					if (opP->mode != MSCR || !(opP->reg == FPI || opP->reg == FPS || opP->reg == FPC))
					    losing++;
					break;
					
				case 'S':
					if (opP->mode != MSCR || opP->reg != SR)
					    losing++;
					break;
					
				case 'U':
					if (opP->mode != MSCR || opP->reg != USP)
					    losing++;
					break;
					
					/* JF these are out of order.  We could put them
					   in order if we were willing to put up with
					   bunches of #ifdef m68851s in the code */
#ifndef NO_68851
					/* Memory addressing mode used by pflushr */
				case '|':
					if (opP->mode == MSCR || opP->mode == DREG ||
					   opP->mode == AREG || opP->mode == REGLST)
					    losing++;
					break;
					
				case 'f':
					if (opP->mode != MSCR || (opP->reg != SFC && opP->reg != DFC))
					    losing++;
					break;
					
				case 'P':
					if (opP->mode != MSCR || (opP->reg != TC && opP->reg != CAL &&
								  opP->reg != VAL && opP->reg != SCC && opP->reg != AC))
					    losing++;
					break;
					
				case 'V':
					if (opP->reg != VAL)
					    losing++;
					break;
					
				case 'W':
					if (opP->mode != MSCR || (opP->reg != DRP && opP->reg != SRP &&
								  opP->reg != CRP))
					    losing++;
					break;
					
				case 'X':
					if (opP->mode != MSCR ||
					    (!(opP->reg >= BAD && opP->reg <= BAD+7) &&
					     !(opP->reg >= BAC && opP->reg <= BAC+7)))
					    losing++;
					break;
					
				case 'Y':
					if (opP->reg != PSR)
					    losing++;
					break;
					
				case 'Z':
					if (opP->reg != PCSR)
					    losing++;
					break;
#endif
				case 'c':
					if (opP->reg != NC
					    && opP->reg != IC
					    && opP->reg != DC
					    && opP->reg != BC) {
						losing++;
					} /* not a cache specifier. */
					break;
					
				case '_':
					if (opP->mode != ABSL) {
						++losing;
					} /* not absolute */
					break;
					
				default:
					as_fatal("Internal error:  Operand mode %c unknown in line %s of file \"%s\"",
						 *s, __LINE__, __FILE__);
				} /* switch on type of operand */
				
				if (losing) break;
			} /* for each operand */
		} /* if immediately wrong */
		
		if (!losing) {
			break;
		} /* got it. */
		
		opcode = opcode->m_next;
		
		if (!opcode) {
			the_ins.error = "instruction/operands mismatch";
			return;
		} /* Fell off the end */
		
		losing = 0;
	}
	
	/* now assemble it */
	
	the_ins.args=opcode->m_operands;
	the_ins.numargs=opcode->m_opnum;
	the_ins.numo=opcode->m_codenum;
	the_ins.opcode[0]=getone(opcode);
	the_ins.opcode[1]=gettwo(opcode);
	
	for (s = the_ins.args, opP = &the_ins.operands[0]; *s; s += 2, opP++) {
		/* This switch is a doozy.
		   Watch the first step; its a big one! */
		switch (s[0]) {
			
		case '*':
		case '~':
		case '%':
		case ';':
		case '@':
		case '!':
		case '&':
		case '$':
		case '?':
		case '/':
#ifndef NO_68851
		case '|':
#endif
			switch (opP->mode) {
			case IMMED:
				tmpreg=0x3c;	/* 7.4 */
				if (strchr("bwl",s[1])) nextword=get_num(opP->con1,80);
				else nextword=nextword=get_num(opP->con1,0);
				if (isvar(opP->con1))
				    add_fix(s[1],opP->con1,0);
				switch (s[1]) {
				case 'b':
					if (!isbyte(nextword))
					    opP->error="operand out of range";
					addword(nextword);
					baseo=0;
					break;
				case 'w':
					if (!isword(nextword))
					    opP->error="operand out of range";
					addword(nextword);
					baseo=0;
					break;
				case 'l':
					addword(nextword>>16);
					addword(nextword);
					baseo=0;
					break;
					
				case 'f':
					baseo=2;
					outro=8;
					break;
				case 'F':
					baseo=4;
					outro=11;
					break;
				case 'x':
					baseo=6;
					outro=15;
					break;
				case 'p':
					baseo=6;
					outro= -1;
					break;
				default:
					as_fatal("Internal error:  Can't decode %c%c in line %s of file \"%s\"",
						 *s, s[1], __LINE__, __FILE__);
				}
				if (!baseo)
				    break;
				
				/* We gotta put out some float */
				if (seg(opP->con1) != SEG_BIG) {
					int_to_gen(nextword);
					gen_to_words(words,baseo,(long int)outro);
					for (wordp=words;baseo--;wordp++)
					    addword(*wordp);
					break;
				}		/* Its BIG */
				if (offs(opP->con1)>0) {
					as_warn("Bignum assumed to be binary bit-pattern");
					if (offs(opP->con1)>baseo) {
						as_warn("Bignum too big for %c format; truncated",s[1]);
						offs(opP->con1)=baseo;
					}
					baseo-=offs(opP->con1);
					for (wordp=generic_bignum+offs(opP->con1)-1;offs(opP->con1)--;--wordp)
					    addword(*wordp);
					while (baseo--)
					    addword(0);
					break;
				}
				gen_to_words(words,baseo,(long)outro);
				for (wordp=words;baseo--;wordp++)
				    addword(*wordp);
				break;
			case DREG:
				tmpreg=opP->reg-DATA; /* 0.dreg */
				break;
			case AREG:
				tmpreg=0x08+opP->reg-ADDR; /* 1.areg */
				break;
			case AINDR:
				tmpreg=0x10+opP->reg-ADDR; /* 2.areg */
				break;
			case ADEC:
				tmpreg=0x20+opP->reg-ADDR; /* 4.areg */
				break;
			case AINC:
				tmpreg=0x18+opP->reg-ADDR; /* 3.areg */
				break;
			case AOFF:
				
				nextword=get_num(opP->con1,80);
				/* Force into index mode.  Hope this works */
				
				/* We do the first bit for 32-bit displacements,
				   and the second bit for 16 bit ones.  It is
				   possible that we should make the default be
				   WORD instead of LONG, but I think that'd
				   break GCC, so we put up with a little
				   inefficiency for the sake of working output.
				   */
				
				if (   !issword(nextword)
				   || (   isvar(opP->con1)
				       && ((opP->con1->e_siz == 0
					    && flagseen['l'] == 0)
					   || opP->con1->e_siz == 3))) {
					
					if (opP->reg == PC)
					    tmpreg=0x3B;	/* 7.3 */
					else
					    tmpreg=0x30+opP->reg-ADDR;	/* 6.areg */
					if (isvar(opP->con1)) {
						if (opP->reg == PC) {
							add_frag(adds(opP->con1),
								 offs(opP->con1),
								 TAB(PCLEA,SZ_UNDEF));
							break;
						} else {
							addword(0x0170);
							add_fix('l',opP->con1,1);
						}
					} else
					    addword(0x0170);
					addword(nextword>>16);
				} else {
					if (opP->reg == PC)
					    tmpreg=0x3A; /* 7.2 */
					else
					    tmpreg=0x28+opP->reg-ADDR; /* 5.areg */
					
					if (isvar(opP->con1)) {
						if (opP->reg == PC) {
							add_fix('w',opP->con1,1);
						} else
						    add_fix('w',opP->con1,0);
					}
				}
				addword(nextword);
				break;
				
			case APODX:
			case AMIND:
			case APRDX:
				know(current_architecture & m68020up);
				/* intentional fall-through */
			case AINDX:
				nextword=0;
				baseo=get_num(opP->con1,80);
				outro=get_num(opP->con2,80);
				/* Figure out the 'addressing mode' */
				/* Also turn on the BASE_DISABLE bit, if needed */
				if (opP->reg == PC || opP->reg == ZPC) {
					tmpreg=0x3b; /* 7.3 */
					if (opP->reg == ZPC)
					    nextword|=0x80;
				} else if (opP->reg == FAIL) {
					nextword|=0x80;
					tmpreg=0x30;	/* 6.garbage */
				} else tmpreg=0x30+opP->reg-ADDR; /* 6.areg */
				
				siz1= (opP->con1) ? opP->con1->e_siz : 0;
				siz2= (opP->con2) ? opP->con2->e_siz : 0;
				
				/* Index register stuff */
				if (opP->ireg >= DATA+0 && opP->ireg <= ADDR+7) {
					nextword|=(opP->ireg-DATA)<<12;
					
					if (opP->isiz == 0 || opP->isiz == 3)
					    nextword|=0x800;
					switch (opP->imul) {
					case 1: break;
					case 2: nextword|=0x200; break;
					case 4: nextword|=0x400; break;
					case 8: nextword|=0x600; break;
					default: as_fatal("failed sanity check.");
					}
					/* IF its simple,
					   GET US OUT OF HERE! */
					
					/* Must be INDEX, with an index
					   register.  Address register
					   cannot be ZERO-PC, and either
					   :b was forced, or we know
					   it will fit */
					if (opP->mode == AINDX
					    && opP->reg != FAIL
					    && opP->reg != ZPC
					    && (siz1 == 1
					       || (   issbyte(baseo)
						   && !isvar(opP->con1)))) {
						nextword +=baseo&0xff;
						addword(nextword);
						if (isvar(opP->con1))
						    add_fix('B',opP->con1,0);
						break;
					}
				} else
				    nextword|=0x40;	/* No index reg */
				
				/* It aint simple */
				nextword|=0x100;
				/* If the guy specified a width, we assume that
				   it is wide enough.  Maybe it isn't.  If so, we lose
				   */
				switch (siz1) {
				case 0:
					if (isvar(opP->con1) || !issword(baseo)) {
						siz1=3;
						nextword|=0x30;
					} else if (baseo == 0)
					    nextword|=0x10;
					else {	
						nextword|=0x20;
						siz1=2;
					}
					break;
				case 1:
					as_warn("Byte dispacement won't work.  Defaulting to :w");
				case 2:
					nextword|=0x20;
					break;
				case 3:
					nextword|=0x30;
					break;
				}
				
				/* Figure out innner displacement stuff */
				if (opP->mode != AINDX) {
					switch (siz2) {
					case 0:
						if (isvar(opP->con2) || !issword(outro)) {
							siz2=3;
							nextword|=0x3;
						} else if (outro == 0)
						    nextword|=0x1;
						else {	
							nextword|=0x2;
							siz2=2;
						}
						break;
					case 1:
						as_warn("Byte dispacement won't work.  Defaulting to :w");
					case 2:
						nextword|=0x2;
						break;
					case 3:
						nextword|=0x3;
						break;
					}
					if (opP->mode == APODX) nextword|=0x04;
					else if (opP->mode == AMIND) nextword|=0x40;
				}
				addword(nextword);
				
				if (isvar(opP->con1)) {
					if (opP->reg == PC || opP->reg == ZPC) {
						add_fix(siz1 == 3 ? 'l' : 'w',opP->con1,1);
						opP->con1->e_exp.X_add_number+=6;
					} else
					    add_fix(siz1 == 3 ? 'l' : 'w',opP->con1,0);
				}
				if (siz1 == 3)
				    addword(baseo>>16);
				if (siz1)
				    addword(baseo);
				
				if (isvar(opP->con2)) {
					if (opP->reg == PC || opP->reg == ZPC) {
						add_fix(siz2 == 3 ? 'l' : 'w',opP->con2,1);
						opP->con1->e_exp.X_add_number+=6;
					} else
					    add_fix(siz2 == 3 ? 'l' : 'w',opP->con2,0);
				}
				if (siz2 == 3)
				    addword(outro>>16);
				if (siz2)
				    addword(outro);
				
				break;
				
			case ABSL:
				nextword=get_num(opP->con1,80);
				switch (opP->con1->e_siz) {
				default:
					as_warn("Unknown size for absolute reference");
				case 0:
					if (!isvar(opP->con1) && issword(offs(opP->con1))) {
						tmpreg=0x38; /* 7.0 */
						addword(nextword);
						break;
					}
					/* Don't generate pc relative code
					   on 68010 and 68000 */
					if (isvar(opP->con1)
					   && !subs(opP->con1)
					   && seg(opP->con1) == SEG_TEXT
					   && now_seg == SEG_TEXT
					   && cpu_of_arch(current_architecture) >= m68020
					   && !flagseen['S']
					   && !strchr("~%&$?", s[0])) {
						tmpreg=0x3A; /* 7.2 */
						add_frag(adds(opP->con1),
							 offs(opP->con1),
							 TAB(PCREL,SZ_UNDEF));
						break;
					}
				case 3:		/* Fall through into long */
					if (isvar(opP->con1))
					    add_fix('l',opP->con1,0);
					
					tmpreg=0x39;	/* 7.1 mode */
					addword(nextword>>16);
					addword(nextword);
					break;
					
				case 2:		/* Word */
					if (isvar(opP->con1))
					    add_fix('w',opP->con1,0);
					
					tmpreg=0x38;	/* 7.0 mode */
					addword(nextword);
					break;
				}
				break;
			case MSCR:
			default:
				as_bad("unknown/incorrect operand");
				/* abort(); */
			}
			install_gen_operand(s[1],tmpreg);
			break;
			
		case '#':
		case '^':
			switch (s[1]) {	/* JF: I hate floating point! */
			case 'j':
				tmpreg=70;
				break;
			case '8':
				tmpreg=20;
				break;
			case 'C':
				tmpreg=50;
				break;
			case '3':
			default:
				tmpreg=80;
				break;
			}
			tmpreg=get_num(opP->con1,tmpreg);
			if (isvar(opP->con1))
			    add_fix(s[1],opP->con1,0);
			switch (s[1]) {
			case 'b':	/* Danger:  These do no check for
					   certain types of overflow.
					   user beware! */
				if (!isbyte(tmpreg))
				    opP->error="out of range";
				insop(tmpreg);
				if (isvar(opP->con1))
				    the_ins.reloc[the_ins.nrel-1].n=(opcode->m_codenum)*2;
				break;
			case 'w':
				if (!isword(tmpreg))
				    opP->error="out of range";
				insop(tmpreg);
				if (isvar(opP->con1))
				    the_ins.reloc[the_ins.nrel-1].n=(opcode->m_codenum)*2;
				break;
			case 'l':
				insop(tmpreg);		/* Because of the way insop works, we put these two out backwards */
				insop(tmpreg>>16);
				if (isvar(opP->con1))
				    the_ins.reloc[the_ins.nrel-1].n=(opcode->m_codenum)*2;
				break;
			case '3':
				tmpreg&=0xFF;
			case '8':
			case 'C':
				install_operand(s[1],tmpreg);
				break;
			default:
				as_fatal("Internal error:  Unknown mode #%c in line %s of file \"%s\"", s[1], __LINE__, __FILE__);
			}
			break;
			
		case '+':
		case '-':
		case 'A':
		case 'a':
			install_operand(s[1], opP->reg - ADDR);
			break;
			
		case 'B':
			tmpreg = get_num(opP->con1, 80);
			switch (s[1]) {
			case 'B':
				/* Needs no offsetting */
				add_fix('B', opP->con1, 1);
				break;
			case 'W':
				/* Offset the displacement to be relative to byte disp location */
				opP->con1->e_exp.X_add_number += 2;
				add_fix('w', opP->con1, 1);
				addword(0);
				break;
			case 'L':
			long_branch:
				if (cpu_of_arch(current_architecture) < m68020) 	/* 68000 or 010 */
				    as_warn("Can't use long branches on 68000/68010");
				the_ins.opcode[the_ins.numo-1]|=0xff;
				/* Offset the displacement to be relative to byte disp location */
				opP->con1->e_exp.X_add_number+=4;
				add_fix('l',opP->con1,1);
				addword(0);
				addword(0);
				break;
			case 'g':
				if (subs(opP->con1))	 /* We can't relax it */
				    goto long_branch;
				
				/* This could either be a symbol, or an
				   absolute address.  No matter, the
				   frag hacking will finger it out.
				   Not quite: it can't switch from
				   BRANCH to BCC68000 for the case
				   where opnd is absolute (it needs
				   to use the 68000 hack since no
				   conditional abs jumps).  */
				if (((cpu_of_arch(current_architecture) < m68020) || (0 == adds(opP->con1)))
				    && (the_ins.opcode[0] >= 0x6200)
				    && (the_ins.opcode[0] <= 0x6f00)) {
					add_frag(adds(opP->con1),offs(opP->con1),TAB(BCC68000,SZ_UNDEF));
				} else {
					add_frag(adds(opP->con1),offs(opP->con1),TAB(BRANCH,SZ_UNDEF));
				}
				break;
			case 'w':
				if (isvar(opP->con1)) {
					/* check for DBcc instruction */
					if ((the_ins.opcode[0] & 0xf0f8) == 0x50c8) {
						/* size varies if patch */
						/* needed for long form */
						add_frag(adds(opP->con1),offs(opP->con1),TAB(DBCC,SZ_UNDEF));
						break;
					}
					
					/* Don't ask! */
					opP->con1->e_exp.X_add_number+=2;
					add_fix('w',opP->con1,1);
				}
				addword(0);
				break;
			case 'C':		/* Fixed size LONG coproc branches */
				the_ins.opcode[the_ins.numo-1]|=0x40;
				/* Offset the displacement to be relative to byte disp location */
				/* Coproc branches don't have a byte disp option, but they are
				   compatible with the ordinary branches, which do... */
				opP->con1->e_exp.X_add_number+=4;
				add_fix('l',opP->con1,1);
				addword(0);
				addword(0);
				break;
			case 'c':		/* Var size Coprocesssor branches */
				if (subs(opP->con1)) {
					add_fix('l',opP->con1,1);
					add_frag((symbolS *)0,(long)0,TAB(FBRANCH,LONG));
				} else if (adds(opP->con1)) {
					add_frag(adds(opP->con1),offs(opP->con1),TAB(FBRANCH,SZ_UNDEF));
				} else {
					/* add_frag((symbolS *)0,offs(opP->con1),TAB(FBRANCH,SHORT)); */
					the_ins.opcode[the_ins.numo-1]|=0x40;
					add_fix('l',opP->con1,1);
					addword(0);
					addword(4);
				}
				break;
			default:
				as_fatal("Internal error:  operand type B%c unknown in line %s of file \"%s\"",
					 s[1], __LINE__, __FILE__);
			}
			break;
			
		case 'C':		/* Ignore it */
			break;
			
		case 'd':		/* JF this is a kludge */
			if (opP->mode == AOFF) {
				install_operand('s',opP->reg-ADDR);
			} else {
				char *tmpP;
				
				tmpP=opP->con1->e_end-2;
				opP->con1->e_beg++;
				opP->con1->e_end-=4;	/* point to the , */
				baseo=m68k_reg_parse(&tmpP);
				if (baseo<ADDR+0 || baseo>ADDR+7) {
					as_bad("Unknown address reg, using A0");
					baseo=0;
				} else baseo-=ADDR;
				install_operand('s',baseo);
			}
			tmpreg=get_num(opP->con1,80);
			if (!issword(tmpreg)) {
				as_warn("Expression out of range, using 0");
				tmpreg=0;
			}
			addword(tmpreg);
			break;
			
		case 'D':
			install_operand(s[1],opP->reg-DATA);
			break;
			
		case 'F':
			install_operand(s[1],opP->reg-FPREG);
			break;
			
		case 'I':
			tmpreg=1+opP->reg-COPNUM;
			if (tmpreg == 8)
			    tmpreg=0;
			install_operand(s[1],tmpreg);
			break;
			
		case 'J':		/* JF foo */
			switch (opP->reg) {
			case SFC:   tmpreg=0x000; break;
			case DFC:   tmpreg=0x001; break;
			case CACR:  tmpreg=0x002; break;
			case TC:    tmpreg=0x003; break;
			case ITT0:  tmpreg=0x004; break;
			case ITT1:  tmpreg=0x005; break;
			case DTT0:  tmpreg=0x006; break;
			case DTT1:  tmpreg=0x007; break;
				
			case USP:   tmpreg=0x800; break;
			case VBR:   tmpreg=0x801; break;
			case CAAR:  tmpreg=0x802; break;
			case MSP:   tmpreg=0x803; break;
			case ISP:   tmpreg=0x804; break;
			case MMUSR: tmpreg=0x805; break;
			case URP:   tmpreg=0x806; break;
			case SRP:   tmpreg=0x807; break;
			default:
				as_fatal("failed sanity check.");
			}
			install_operand(s[1],tmpreg);
			break;
			
		case 'k':
			tmpreg=get_num(opP->con1,55);
			install_operand(s[1],tmpreg&0x7f);
			break;
			
		case 'l':
			tmpreg=opP->reg;
			if (s[1] == 'w') {
				if (tmpreg&0x7FF0000)
				    as_bad("Floating point register in register list");
				insop(reverse_16_bits(tmpreg));
			} else {
				if (tmpreg&0x700FFFF)
				    as_bad("Wrong register in floating-point reglist");
				install_operand(s[1],reverse_8_bits(tmpreg>>16));
			}
			break;
			
		case 'L':
			tmpreg=opP->reg;
			if (s[1] == 'w') {
				if (tmpreg&0x7FF0000)
				    as_bad("Floating point register in register list");
				insop(tmpreg);
			} else if (s[1] == '8') {
				if (tmpreg&0x0FFFFFF)
				    as_bad("incorrect register in reglist");
				install_operand(s[1],tmpreg>>24);
			} else {
				if (tmpreg&0x700FFFF)
				    as_bad("wrong register in floating-point reglist");
				else
				    install_operand(s[1],tmpreg>>16);
			}
			break;
			
		case 'M':
			install_operand(s[1],get_num(opP->con1,60));
			break;
			
		case 'O':
			tmpreg= (opP->mode == DREG)
			    ? 0x20+opP->reg-DATA
				: (get_num(opP->con1,40)&0x1F);
			install_operand(s[1],tmpreg);
			break;
			
		case 'Q':
			tmpreg=get_num(opP->con1,10);
			if (tmpreg == 8)
			    tmpreg=0;
			install_operand(s[1],tmpreg);
			break;
			
		case 'R':
			/* This depends on the fact that ADDR registers are
			   eight more than their corresponding DATA regs, so
			   the result will have the ADDR_REG bit set */
			install_operand(s[1],opP->reg-DATA);
			break;
			
		case 's':
			if (opP->reg == FPI) tmpreg=0x1;
			else if (opP->reg == FPS) tmpreg=0x2;
			else if (opP->reg == FPC) tmpreg=0x4;
			else as_fatal("failed sanity check.");
			install_operand(s[1],tmpreg);
			break;
			
		case 'S':	/* Ignore it */
			break;
			
		case 'T':
			install_operand(s[1],get_num(opP->con1,30));
			break;
			
		case 'U':	/* Ignore it */
			break;
			
		case 'c':
			switch (opP->reg) {
			case NC: tmpreg = 0; break;
			case DC: tmpreg = 1; break;
			case IC: tmpreg = 2; break;
			case BC: tmpreg = 3; break;
			default:
				as_fatal("failed sanity check");
			} /* switch on cache token */
			install_operand(s[1], tmpreg);
			break;
#ifndef NO_68851
			/* JF: These are out of order, I fear. */
		case 'f':
			switch (opP->reg) {
			case SFC:
				tmpreg=0;
				break;
			case DFC:
				tmpreg=1;
				break;
			default:
				as_fatal("failed sanity check.");
			}
			install_operand(s[1],tmpreg);
			break;
			
		case 'P':
			switch (opP->reg) {
			case TC:
				tmpreg=0;
				break;
			case CAL:
				tmpreg=4;
				break;
			case VAL:
				tmpreg=5;
				break;
			case SCC:
				tmpreg=6;
				break;
			case AC:
				tmpreg=7;
				break;
			default:
				as_fatal("failed sanity check.");
			}
			install_operand(s[1],tmpreg);
			break;
			
		case 'V':
			if (opP->reg == VAL)
			    break;
			as_fatal("failed sanity check.");
			
		case 'W':
			switch (opP->reg) {
				
			case DRP:
				tmpreg=1;
				break;
			case SRP:
				tmpreg=2;
				break;
			case CRP:
				tmpreg=3;
				break;
			default:
				as_fatal("failed sanity check.");
			}
			install_operand(s[1],tmpreg);
			break;
			
		case 'X':
			switch (opP->reg) {
			case BAD: case BAD+1: case BAD+2: case BAD+3:
			case BAD+4: case BAD+5: case BAD+6: case BAD+7:
				tmpreg = (4 << 10) | ((opP->reg - BAD) << 2);
				break;
				
			case BAC: case BAC+1: case BAC+2: case BAC+3:
			case BAC+4: case BAC+5: case BAC+6: case BAC+7:
				tmpreg = (5 << 10) | ((opP->reg - BAC) << 2);
				break;
				
			default:
				as_fatal("failed sanity check.");
			}
			install_operand(s[1], tmpreg);
			break;
		case 'Y':
			know(opP->reg == PSR);
			break;
		case 'Z':
			know(opP->reg == PCSR);
			break;
#endif /* m68851 */
		case '_':
			tmpreg=get_num(opP->con1,80);
			install_operand(s[1], tmpreg);
			break;
		default:
			as_fatal("Internal error:  Operand type %c unknown in line %s of file \"%s\"", s[0], __LINE__, __FILE__);
		}
	}
	/* By the time whe get here (FINALLY) the_ins contains the complete
	   instruction, ready to be emitted... */
} /* m68k_ip() */

/*
 * get_regs := '/' + ?
 *	| '-' + <register>
 *	| '-' + <register> + ?
 *	| <empty>
 *	;
 *
 
 * The idea here must be to scan in a set of registers but I don't
 * understand it.  Looks awfully sloppy to me but I don't have any doc on
 * this format so...
 
 * 
 *
 */

static int get_regs(i,str,opP)
int i;
struct m68k_op *opP;
char *str;
{
	/*			     26, 25, 24, 23-16,  15-8, 0-7 */
	/* Low order 24 bits encoded fpc,fps,fpi,fp7-fp0,a7-a0,d7-d0 */
	unsigned long cur_regs = 0;
	int	reg1,
	reg2;
	
#define ADD_REG(x)	{     if (x == FPI) cur_regs|=(1<<24);\
else if (x == FPS) cur_regs|=(1<<25);\
else if (x == FPC) cur_regs|=(1<<26);\
else cur_regs|=(1<<(x-1));  }
	
	reg1=i;
	for (;;) {
		if (*str == '/') {
			ADD_REG(reg1);
			str++;
		} else if (*str == '-') {
			str++;
			reg2=m68k_reg_parse(&str);
			if (reg2<DATA || reg2 >= FPREG+8 || reg1 == FPI || reg1 == FPS || reg1 == FPC) {
				opP->error="unknown register in register list";
				return FAIL;
			}
			while (reg1 <= reg2) {
				ADD_REG(reg1);
				reg1++;
			}
			if (*str == '\0')
			    break;
		} else if (*str == '\0') {
			ADD_REG(reg1);
			break;
		} else {
			opP->error="unknow character in register list";
			return FAIL;
		}
		/* DJA -- Bug Fix.  Did't handle d1-d2/a1 until the following instruction was added */
		if (*str == '/')
		    str ++;
		reg1=m68k_reg_parse(&str);
		if ((reg1<DATA || reg1 >= FPREG+8) && !(reg1 == FPI || reg1 == FPS || reg1 == FPC)) {
			opP->error="unknown register in register list";
			return FAIL;
		}
	}
	opP->reg=cur_regs;
	return OK;
} /* get_regs() */

static int reverse_16_bits(in)
int in;
{
	int out=0;
	int n;
	
	static int mask[16] = {
		0x0001,0x0002,0x0004,0x0008,0x0010,0x0020,0x0040,0x0080,
		0x0100,0x0200,0x0400,0x0800,0x1000,0x2000,0x4000,0x8000
	    };
	for (n=0;n<16;n++) {
		if (in&mask[n])
		    out|=mask[15-n];
	}
	return out;
} /* reverse_16_bits() */

static int reverse_8_bits(in)
int in;
{
	int out=0;
	int n;
	
	static int mask[8] = {
		0x0001,0x0002,0x0004,0x0008,0x0010,0x0020,0x0040,0x0080,
	};
	
	for (n=0;n<8;n++) {
		if (in&mask[n])
		    out|=mask[7-n];
	}
	return out;
} /* reverse_8_bits() */

static void install_operand(mode,val)
int mode;
int val;
{
	switch (mode) {
	case 's':
		the_ins.opcode[0]|=val & 0xFF;	/* JF FF is for M kludge */
		break;
	case 'd':
		the_ins.opcode[0]|=val<<9;
		break;
	case '1':
		the_ins.opcode[1]|=val<<12;
		break;
	case '2':
		the_ins.opcode[1]|=val<<6;
		break;
	case '3':
		the_ins.opcode[1]|=val;
		break;
	case '4':
		the_ins.opcode[2]|=val<<12;
		break;
	case '5':
		the_ins.opcode[2]|=val<<6;
		break;
	case '6':
		/* DANGER!  This is a hack to force cas2l and cas2w cmds
		   to be three words long! */
		the_ins.numo++;
		the_ins.opcode[2]|=val;
		break;
	case '7':
		the_ins.opcode[1]|=val<<7;
		break;
	case '8':
		the_ins.opcode[1]|=val<<10;
		break;
#ifndef NO_68851
	case '9':
		the_ins.opcode[1]|=val<<5;
		break;
#endif
		
	case 't':
		the_ins.opcode[1]|=(val<<10)|(val<<7);
		break;
	case 'D':
		the_ins.opcode[1]|=(val<<12)|val;
		break;
	case 'g':
		the_ins.opcode[0]|=val=0xff;
		break;
	case 'i':
		the_ins.opcode[0]|=val<<9;
		break;
	case 'C':
		the_ins.opcode[1]|=val;
		break;
	case 'j':
		the_ins.opcode[1]|=val;
		the_ins.numo++;		/* What a hack */
		break;
	case 'k':
		the_ins.opcode[1]|=val<<4;
		break;
	case 'b':
	case 'w':
	case 'l':
		break;
	case 'e':
		the_ins.opcode[0] |= (val << 6);
		break;
	case 'L':
		the_ins.opcode[1] = (val >> 16);
		the_ins.opcode[2] = val & 0xffff;
		break;
	case 'c':
	default:
		as_fatal("failed sanity check.");
	}
} /* install_operand() */

static void install_gen_operand(mode,val)
int mode;
int val;
{
	switch (mode) {
	case 's':
		the_ins.opcode[0]|=val;
		break;
	case 'd':
		/* This is a kludge!!! */
		the_ins.opcode[0]|=(val&0x07)<<9|(val&0x38)<<3;
		break;
	case 'b':
	case 'w':
	case 'l':
	case 'f':
	case 'F':
	case 'x':
	case 'p':
		the_ins.opcode[0]|=val;
		break;
		/* more stuff goes here */
	default:
		as_fatal("failed sanity check.");
	}
} /* install_gen_operand() */

/*
 * verify that we have some number of paren pairs, do m68k_ip_op(), and
 * then deal with the bitfield hack.
 */

static char *crack_operand(str,opP)
register char *str;
register struct m68k_op *opP;
{
	register int parens;
	register int c;
	register char *beg_str;
	
	if (!str) {
		return str;
	}
	beg_str=str;
	for (parens=0;*str && (parens>0 || notend(str));str++) {
		if (*str == '(') parens++;
		else if (*str == ')') {
			if (!parens) {		/* ERROR */
				opP->error="Extra )";
				return str;
			}
			--parens;
		}
	}
	if (!*str && parens) {		/* ERROR */
		opP->error="Missing )";
		return str;
	}
	c= *str;
	*str='\0';
	if (m68k_ip_op(beg_str,opP) == FAIL) {
		*str=c;
		return str;
	}
	*str=c;
	if (c == '}')
	    c= *++str;		/* JF bitfield hack */
	if (c) {
		c= *++str;
		if (!c)
		    as_bad("Missing operand");
	}
	return str;
}

/* See the comment up above where the #define notend(... is */
#if 0
notend(s)
char *s;
{
	if (*s == ',') return 0;
	if (*s == '{' || *s == '}')
	    return 0;
	if (*s != ':') return 1;
	/* This kludge here is for the division cmd, which is a kludge */
	if (index("aAdD#",s[1])) return 0;
	return 1;
}
#endif

/* This is the guts of the machine-dependent assembler.  STR points to a
   machine dependent instruction.  This function is supposed to emit
   the frags/bytes it assembles to.
   */
void
    md_assemble(str)
char *str;
{
	char *er;
	short	*fromP;
	char	*toP = NULL;
	int	m,n = 0;
	char	*to_beg_P;
	int	shorts_this_frag;
	
	
	if (current_architecture == 0) {
		current_architecture = (m68020
#ifndef NO_68881
					| m68881
#endif
#ifndef NO_68851
					| m68851
#endif
					);
	} /* default current_architecture */
	
	memset((char *)(&the_ins), '\0', sizeof(the_ins));	/* JF for paranoia sake */
	m68k_ip(str);
	er=the_ins.error;
	if (!er) {
		for (n=the_ins.numargs;n;--n)
		    if (the_ins.operands[n].error) {
			    er=the_ins.operands[n].error;
			    break;
		    }
	}
	if (er) {
		as_bad("\"%s\" -- Statement '%s' ignored",er,str);
		return;
	}
	
	if (the_ins.nfrag == 0) {	/* No frag hacking involved; just put it out */
		toP=frag_more(2*the_ins.numo);
		fromP= &the_ins.opcode[0];
		for (m=the_ins.numo;m;--m) {
			md_number_to_chars(toP,(long)(*fromP),2);
			toP+=2;
			fromP++;
		}
		/* put out symbol-dependent info */
		for (m = 0; m < the_ins.nrel; m++) {
			switch (the_ins.reloc[m].wid) {
			case 'B':
				n=1;
				break;
			case 'b':
				n=1;
				break;
			case '3':
				n=2;
				break;
			case 'w':
				n=2;
				break;
			case 'l':
				n=4;
				break;
			default:
				as_fatal("Don't know how to figure width of %c in md_assemble()",the_ins.reloc[m].wid);
			}
			
			fix_new(frag_now,
				(toP-frag_now->fr_literal)-the_ins.numo*2+the_ins.reloc[m].n,
				n,
				the_ins.reloc[m].add,
				the_ins.reloc[m].sub,
				the_ins.reloc[m].off,
				the_ins.reloc[m].pcrel,
				NO_RELOC);
		}
		return;
	}
	
	/* There's some frag hacking */
	for (n=0,fromP= &the_ins.opcode[0];n<the_ins.nfrag;n++) {
		int wid;
		
		if (n == 0) wid=2*the_ins.fragb[n].fragoff;
		else wid=2*(the_ins.numo-the_ins.fragb[n-1].fragoff);
		toP=frag_more(wid);
		to_beg_P=toP;
		shorts_this_frag=0;
		for (m=wid/2;m;--m) {
			md_number_to_chars(toP,(long)(*fromP),2);
			toP+=2;
			fromP++;
			shorts_this_frag++;
		}
		for (m=0;m<the_ins.nrel;m++) {
			if ((the_ins.reloc[m].n) >= 2*shorts_this_frag /* 2*the_ins.fragb[n].fragoff */) {
				the_ins.reloc[m].n-= 2*shorts_this_frag /* 2*the_ins.fragb[n].fragoff */;
				break;
			}
			wid=the_ins.reloc[m].wid;
			if (wid == 0)
			    continue;
			the_ins.reloc[m].wid=0;
			wid = (wid == 'b') ? 1 : (wid == 'w') ? 2 : (wid == 'l') ? 4 : 4000;
			
			fix_new(frag_now,
				(toP-frag_now->fr_literal)-the_ins.numo*2+the_ins.reloc[m].n,
				wid,
				the_ins.reloc[m].add,
				the_ins.reloc[m].sub,
				the_ins.reloc[m].off,
				the_ins.reloc[m].pcrel,
				NO_RELOC);
		}
		/* know(the_ins.fragb[n].fadd); */
		(void)frag_var(rs_machine_dependent,10,0,(relax_substateT)(the_ins.fragb[n].fragty),
			       the_ins.fragb[n].fadd,the_ins.fragb[n].foff,to_beg_P);
	}
	n=(the_ins.numo-the_ins.fragb[n-1].fragoff);
	shorts_this_frag=0;
	if (n) {
		toP=frag_more(n*sizeof(short));
		while (n--) {
			md_number_to_chars(toP,(long)(*fromP),2);
			toP+=2;
			fromP++;
			shorts_this_frag++;
		}
	}
	for (m=0;m<the_ins.nrel;m++) {
		int wid;
		
		wid=the_ins.reloc[m].wid;
		if (wid == 0)
		    continue;
		the_ins.reloc[m].wid=0;
		wid = (wid == 'b') ? 1 : (wid == 'w') ? 2 : (wid == 'l') ? 4 : 4000;
		
		fix_new(frag_now,
			(the_ins.reloc[m].n + toP-frag_now->fr_literal)-/* the_ins.numo */ shorts_this_frag*2,
			wid,
			the_ins.reloc[m].add,
			the_ins.reloc[m].sub,
			the_ins.reloc[m].off,
			the_ins.reloc[m].pcrel,
			NO_RELOC);
	}
}

/* This function is called once, at assembler startup time.  This should
   set up all the tables, etc that the MD part of the assembler needs
   */
void
    md_begin()
{
	/*
	 * md_begin -- set up hash tables with 68000 instructions.
	 * similar to what the vax assembler does.  ---phr
	 */
	/* RMS claims the thing to do is take the m68k-opcode.h table, and make
	   a copy of it at runtime, adding in the information we want but isn't
	   there.  I think it'd be better to have an awk script hack the table
	   at compile time.  Or even just xstr the table and use it as-is.  But
	   my lord ghod hath spoken, so we do it this way.  Excuse the ugly var
	   names.  */
	
	register const struct m68k_opcode *ins;
	register struct m68k_incant *hack,
	*slak;
	register char *retval = 0;		/* empty string, or error msg text */
	register unsigned int i;
	register char c;
	
	if ((op_hash = hash_new()) == NULL)
	    as_fatal("Virtual memory exhausted");
	
	obstack_begin(&robyn,4000);
	for (ins = m68k_opcodes; ins < endop; ins++) {
		hack=slak=(struct m68k_incant *)obstack_alloc(&robyn,sizeof(struct m68k_incant));
		do {
			/* we *could* ignore insns that don't match our
			   arch here but just leaving them out of the
			   hash. */
			slak->m_operands=ins->args;
			slak->m_opnum=strlen(slak->m_operands)/2;
			slak->m_arch = ins->arch;
			slak->m_opcode=ins->opcode;
			/* This is kludgey */
			slak->m_codenum=((ins->match)&0xffffL) ? 2 : 1;
			if ((ins+1) != endop && !strcmp(ins->name,(ins+1)->name)) {
				slak->m_next=(struct m68k_incant *) obstack_alloc(&robyn,sizeof(struct m68k_incant));
				ins++;
			} else
			    slak->m_next=0;
			slak=slak->m_next;
		} while (slak);
		
		retval = hash_insert (op_hash, ins->name,(char *)hack);
		/* Didn't his mommy tell him about null pointers? */
		if (retval && *retval)
		    as_fatal("Internal Error:  Can't hash %s: %s",ins->name,retval);
	}
	
	for (i = 0; i < sizeof(mklower_table) ; i++)
	    mklower_table[i] = (isupper(c = (char) i)) ? tolower(c) : c;
	
	for (i = 0 ; i < sizeof(notend_table) ; i++) {
		notend_table[i] = 0;
		alt_notend_table[i] = 0;
	}
	notend_table[','] = 1;
	notend_table['{'] = 1;
	notend_table['}'] = 1;
	alt_notend_table['a'] = 1;
	alt_notend_table['A'] = 1;
	alt_notend_table['d'] = 1;
	alt_notend_table['D'] = 1;
	alt_notend_table['#'] = 1;
	alt_notend_table['f'] = 1;
	alt_notend_table['F'] = 1;
#ifdef REGISTER_PREFIX
	alt_notend_table[REGISTER_PREFIX] = 1;
#endif
}

#if 0
#define notend(s) ((*s == ',' || *s == '}' || *s == '{' \
		    || (*s == ':' && strchr("aAdD#", s[1]))) \
		   ? 0 : 1)
#endif

/* This funciton is called once, before the assembler exits.  It is
   supposed to do any final cleanup for this part of the assembler.
   */
void
    md_end()
{
}

/* Equal to MAX_PRECISION in atof-ieee.c */
#define MAX_LITTLENUMS 6

/* Turn a string in input_line_pointer into a floating point constant of type
   type, and store the appropriate bytes in *litP.  The number of LITTLENUMS
   emitted is stored in *sizeP. An error message is returned, or NULL on OK.
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
	char	*t;
	char	*atof_ieee();
	
	switch (type) {
	case 'f':
	case 'F':
	case 's':
	case 'S':
		prec = 2;
		break;
		
	case 'd':
	case 'D':
	case 'r':
	case 'R':
		prec = 4;
		break;
		
	case 'x':
	case 'X':
		prec = 6;
		break;
		
	case 'p':
	case 'P':
		prec = 6;
		break;
		
	default:
		*sizeP=0;
		return "Bad call to MD_ATOF()";
	}
	t=atof_ieee(input_line_pointer,type,words);
	if (t)
	    input_line_pointer=t;
	
	*sizeP=prec * sizeof(LITTLENUM_TYPE);
	for (wordP=words;prec--;) {
		md_number_to_chars(litP,(long)(*wordP++),sizeof(LITTLENUM_TYPE));
		litP+=sizeof(LITTLENUM_TYPE);
	}
	return "";	/* Someone should teach Dean about null pointers */
}

/* Turn an integer of n bytes (in val) into a stream of bytes appropriate
   for use in the a.out file, and stores them in the array pointed to by buf.
   This knows about the endian-ness of the target machine and does
   THE RIGHT THING, whatever it is.  Possible values for n are 1 (byte)
   2 (short) and 4 (long)  Floating numbers are put out as a series of
   LITTLENUMS (shorts, here at least)
   */
void
    md_number_to_chars(buf, val, n)
char	*buf;
long	val;
int n;
{
	switch (n) {
	case 1:
		*buf++=val;
		break;
	case 2:
		*buf++=(val>>8);
		*buf++=val;
		break;
	case 4:
		*buf++=(val>>24);
		*buf++=(val>>16);
		*buf++=(val>>8);
		*buf++=val;
		break;
	default:
		as_fatal("failed sanity check.");
	}
}

void
    md_apply_fix(fixP, val)
fixS *fixP;
long val;
{
	char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
	
	switch (fixP->fx_size) {
	case 1:
		*buf++ = val;
		break;
	case 2:
		*buf++ = (val >> 8);
		*buf++ = val;
		break;
	case 4:
		*buf++ = (val >> 24);
		*buf++ = (val >> 16);
		*buf++ = (val >> 8);
		*buf++ = val;
		break;
	default:
		BAD_CASE (fixP->fx_size);
	}
}


/* *fragP has been relaxed to its final size, and now needs to have
   the bytes inside it modified to conform to the new size  There is UGLY
   MAGIC here. ..
   */
void
    md_convert_frag(headers, fragP)
object_headers *headers;
register fragS *fragP;
{
	long disp;
	long ext = 0;
	
	/* Address in object code of the displacement.  */
	register int object_address = fragP->fr_fix + fragP->fr_address;
	
#ifdef IBM_COMPILER_SUX
	/* This is wrong but it convinces the native rs6000 compiler to
	   generate the code we want. */
	register char *buffer_address = fragP->fr_literal;
	buffer_address += fragP->fr_fix;
#else /* IBM_COMPILER_SUX */
	/* Address in gas core of the place to store the displacement.  */
	register char *buffer_address = fragP->fr_fix + fragP->fr_literal;
#endif /* IBM_COMPILER_SUX */
	
	/* No longer true:   know(fragP->fr_symbol); */
	
	/* The displacement of the address, from current location.  */
	disp = fragP->fr_symbol ? S_GET_VALUE(fragP->fr_symbol) : 0;
	disp = (disp + fragP->fr_offset) - object_address;
	
	switch (fragP->fr_subtype) {
	case TAB(BCC68000,BYTE):
    case TAB(BRANCH,BYTE):
	know(issbyte(disp));
	if (disp == 0)
	    as_bad("short branch with zero offset: use :w");
	fragP->fr_opcode[1]=disp;
	ext=0;
	break;
 case TAB(DBCC,SHORT):
     know(issword(disp));
	ext=2;
	break;
 case TAB(BCC68000,SHORT):
 case TAB(BRANCH,SHORT):
     know(issword(disp));
	fragP->fr_opcode[1]=0x00;
	ext=2;
	break;
 case TAB(BRANCH,LONG):
     if (cpu_of_arch(current_architecture) < m68020) {
	     if (fragP->fr_opcode[0] == 0x61) {
		     fragP->fr_opcode[0]= 0x4E;
		     fragP->fr_opcode[1]= 0xB9;	/* JBSR with ABSL LONG offset */
		     subseg_change(SEG_TEXT, 0);
		     
		     fix_new(fragP,
			     fragP->fr_fix,
			     4,
			     fragP->fr_symbol,
			     0,
			     fragP->fr_offset,
			     0,
			     NO_RELOC);
		     
		     fragP->fr_fix+=4;
		     ext=0;
	     } else if (fragP->fr_opcode[0] == 0x60) {
		     fragP->fr_opcode[0]= 0x4E;
		     fragP->fr_opcode[1]= 0xF9;      /* JMP  with ABSL LONG offset */
		     subseg_change(SEG_TEXT, 0);
		     fix_new(fragP, fragP->fr_fix, 4, fragP->fr_symbol, 0, fragP->fr_offset,0,
			     NO_RELOC);
		     fragP->fr_fix+=4;
		     ext=0;
	     } else {
		     as_bad("Long branch offset not supported.");
	     }
     } else {
	     fragP->fr_opcode[1]=0xff;
	     ext=4;
     }
	break;
 case TAB(BCC68000,LONG):
     /* only Bcc 68000 instructions can come here */
     /* change bcc into b!cc/jmp absl long */
     fragP->fr_opcode[0] ^= 0x01; /* invert bcc */
        fragP->fr_opcode[1] = 0x6;   /* branch offset = 6 */
	
	/* JF: these used to be fr_opcode[2,3], but they may be in a
	   different frag, in which case refering to them is a no-no.
	   Only fr_opcode[0,1] are guaranteed to work. */
        *buffer_address++ = 0x4e;  /* put in jmp long (0x4ef9) */ 
        *buffer_address++ = 0xf9;  
        fragP->fr_fix += 2;	     /* account for jmp instruction */
        subseg_change(SEG_TEXT,0);
        fix_new(fragP, fragP->fr_fix, 4, fragP->fr_symbol, 0, 
		fragP->fr_offset,0,
		NO_RELOC);
        fragP->fr_fix += 4;
        ext=0;
	break;
 case TAB(DBCC,LONG):
     /* only DBcc 68000 instructions can come here */
     /* change dbcc into dbcc/jmp absl long */
     /* JF: these used to be fr_opcode[2-7], but that's wrong */
     *buffer_address++ = 0x00;  /* branch offset = 4 */
        *buffer_address++ = 0x04;  
        *buffer_address++ = 0x60;  /* put in bra pc+6 */ 
        *buffer_address++ = 0x06;  
        *buffer_address++ = 0x4e;  /* put in jmp long (0x4ef9) */ 
        *buffer_address++ = 0xf9;  
	
        fragP->fr_fix += 6;	     /* account for bra/jmp instructions */
        subseg_change(SEG_TEXT,0);
        fix_new(fragP, fragP->fr_fix, 4, fragP->fr_symbol, 0, 
		fragP->fr_offset,0,
		NO_RELOC);
        fragP->fr_fix += 4;
        ext=0;
	break;
 case TAB(FBRANCH,SHORT):
     know((fragP->fr_opcode[1]&0x40) == 0);
	ext=2;
	break;
 case TAB(FBRANCH,LONG):
     fragP->fr_opcode[1]|=0x40;	/* Turn on LONG bit */
	ext=4;
	break;
 case TAB(PCREL,SHORT):
     ext=2;
	break;
 case TAB(PCREL,LONG):
     /* The thing to do here is force it to ABSOLUTE LONG, since
	PCREL is really trying to shorten an ABSOLUTE address anyway */
     /* JF FOO This code has not been tested */
     subseg_change(SEG_TEXT,0);
	fix_new(fragP, fragP->fr_fix, 4, fragP->fr_symbol, 0, fragP->fr_offset, 0, NO_RELOC);
	if ((fragP->fr_opcode[1] & 0x3F) != 0x3A)
	    as_bad("Internal error (long PC-relative operand) for insn 0x%04lx at 0x%lx",
		   fragP->fr_opcode[0],fragP->fr_address);
	fragP->fr_opcode[1]&= ~0x3F;
	fragP->fr_opcode[1]|=0x39;	/* Mode 7.1 */
	fragP->fr_fix+=4;
	/* md_number_to_chars(buffer_address,
	   (long)(fragP->fr_symbol->sy_value + fragP->fr_offset),
	   4); */
	ext=0;
	break;
 case TAB(PCLEA,SHORT):
     subseg_change(SEG_TEXT,0);
	fix_new(fragP, (int) (fragP->fr_fix), 2, fragP->fr_symbol, (symbolS *) 0, fragP->fr_offset, 1, NO_RELOC);
	fragP->fr_opcode[1] &= ~0x3F;
	fragP->fr_opcode[1] |= 0x3A;
	ext=2;
	break;
 case TAB(PCLEA,LONG):
     subseg_change(SEG_TEXT,0);
	fix_new(fragP, (int) (fragP->fr_fix) + 2, 4, fragP->fr_symbol, (symbolS *) 0, fragP->fr_offset + 2, 1, NO_RELOC);
	*buffer_address++ = 0x01;
	*buffer_address++ = 0x70;
	fragP->fr_fix+=2;
	/* buffer_address+=2; */
	ext=4;
	break;
	
} /* switch on subtype */
	
	if (ext) {
		md_number_to_chars(buffer_address, (long) disp, (int) ext);
		fragP->fr_fix += ext;
		/*	  H_SET_TEXT_SIZE(headers, H_GET_TEXT_SIZE(headers) + ext); */
	} /* if extending */
	
	return;
} /* md_convert_frag() */

/* Force truly undefined symbols to their maximum size, and generally set up
   the frag list to be relaxed
   */
int md_estimate_size_before_relax(fragP, segment)
register fragS *fragP;
segT segment;
{
	int	old_fix;
	register char *buffer_address = fragP->fr_fix + fragP->fr_literal;
	
	old_fix = fragP->fr_fix;
	
	/* handle SZ_UNDEF first, it can be changed to BYTE or SHORT */
	switch (fragP->fr_subtype) {
		
	case TAB(BRANCH,SZ_UNDEF): {
		if ((fragP->fr_symbol != NULL) 	/* Not absolute */
		   && S_GET_SEGMENT(fragP->fr_symbol) == segment) {
			fragP->fr_subtype = TAB(TABTYPE(fragP->fr_subtype), BYTE);
			break;
		} else if ((fragP->fr_symbol == 0) || (cpu_of_arch(current_architecture) < m68020)) {
			/* On 68000, or for absolute value, switch to abs long */
			/* FIXME, we should check abs val, pick short or long */
			if (fragP->fr_opcode[0] == 0x61) {
				fragP->fr_opcode[0]= 0x4E;
				fragP->fr_opcode[1]= 0xB9;	/* JBSR with ABSL LONG offset */
				subseg_change(SEG_TEXT, 0);
				fix_new(fragP, fragP->fr_fix, 4, 
					fragP->fr_symbol, 0, fragP->fr_offset, 0, NO_RELOC);
				fragP->fr_fix+=4;
				frag_wane(fragP);
			} else if (fragP->fr_opcode[0] == 0x60) {
				fragP->fr_opcode[0]= 0x4E;
				fragP->fr_opcode[1]= 0xF9;  /* JMP  with ABSL LONG offset */
				subseg_change(SEG_TEXT, 0);
				fix_new(fragP, fragP->fr_fix, 4, 
					fragP->fr_symbol, 0, fragP->fr_offset, 0, NO_RELOC);
				fragP->fr_fix+=4;
				frag_wane(fragP);
			} else {
				as_warn("Long branch offset to extern symbol not supported.");
			}
		} else if (flagseen['l']) { /* Symbol is still undefined.  Make it simple */
			fix_new(fragP, (int) (fragP->fr_fix), 2, fragP->fr_symbol,
				(symbolS *) 0, fragP->fr_offset, 1, NO_RELOC);
			fragP->fr_fix += 2;
			fragP->fr_opcode[1] = 0x00;
			frag_wane(fragP);
		} else {
			fix_new(fragP, (int) (fragP->fr_fix), 4, fragP->fr_symbol,
				(symbolS *) 0, fragP->fr_offset, 1, NO_RELOC);
			fragP->fr_fix += 4;
			fragP->fr_opcode[1] = 0xff;
			frag_wane(fragP);
			break;
		}
		
		break;
	} /* case TAB(BRANCH,SZ_UNDEF) */
		
	case TAB(FBRANCH,SZ_UNDEF): {
		if (S_GET_SEGMENT(fragP->fr_symbol) == segment || flagseen['l']) {
			fragP->fr_subtype = TAB(FBRANCH,SHORT);
			fragP->fr_var += 2;
		} else {
			fragP->fr_subtype = TAB(FBRANCH,LONG);
			fragP->fr_var += 4;
		}
		break;
	} /* TAB(FBRANCH,SZ_UNDEF) */
		
	case TAB(PCREL,SZ_UNDEF): {
		if (S_GET_SEGMENT(fragP->fr_symbol) == segment || flagseen['l']) {
			fragP->fr_subtype = TAB(PCREL,SHORT);
			fragP->fr_var += 2;
		} else {
			fragP->fr_subtype = TAB(PCREL,LONG);
			fragP->fr_var += 4;
		}
		break;
	} /* TAB(PCREL,SZ_UNDEF) */
		
	case TAB(BCC68000,SZ_UNDEF): {
		if ((fragP->fr_symbol != NULL)
		   && S_GET_SEGMENT(fragP->fr_symbol) == segment) {
			fragP->fr_subtype=TAB(BCC68000,BYTE);
			break;
		}
		/* only Bcc 68000 instructions can come here */
		/* change bcc into b!cc/jmp absl long */
		fragP->fr_opcode[0] ^= 0x01; /* invert bcc */
		if (flagseen['l']) {
			fragP->fr_opcode[1] = 0x04;   /* branch offset = 6 */
			/* JF: these were fr_opcode[2,3] */
			buffer_address[0] = 0x4e;  /* put in jmp long (0x4ef9) */ 
			buffer_address[1] = 0xf8;
			fragP->fr_fix += 2;	     /* account for jmp instruction */
			subseg_change(SEG_TEXT,0);
			fix_new(fragP, fragP->fr_fix, 2, fragP->fr_symbol, 0, 
				fragP->fr_offset, 0, NO_RELOC);
			fragP->fr_fix += 2;
		} else {
			fragP->fr_opcode[1] = 0x06;   /* branch offset = 6 */
			/* JF: these were fr_opcode[2,3] */
			buffer_address[2] = 0x4e;  /* put in jmp long (0x4ef9) */ 
			buffer_address[3] = 0xf9;
			fragP->fr_fix += 2;	     /* account for jmp instruction */
			subseg_change(SEG_TEXT,0);
			fix_new(fragP, fragP->fr_fix, 4, fragP->fr_symbol, 0, 
				fragP->fr_offset, 0, NO_RELOC);
			fragP->fr_fix += 4;
		}
		frag_wane(fragP);
		break;
	} /* case TAB(BCC68000,SZ_UNDEF) */
		
	case TAB(DBCC,SZ_UNDEF): {
		if (fragP->fr_symbol != NULL && S_GET_SEGMENT(fragP->fr_symbol) == segment) {
			fragP->fr_subtype=TAB(DBCC,SHORT);
			fragP->fr_var+=2;
			break;
		}
		/* only DBcc 68000 instructions can come here */
		/* change dbcc into dbcc/jmp absl long */
		/* JF: these used to be fr_opcode[2-4], which is wrong. */
		buffer_address[0] = 0x00;  /* branch offset = 4 */
		buffer_address[1] = 0x04;  
		buffer_address[2] = 0x60;  /* put in bra pc + ... */
		
		if (flagseen['l']) {
			/* JF: these were fr_opcode[5-7] */
			buffer_address[3] = 0x04; /* plus 4 */
			buffer_address[4] = 0x4e;/* Put in Jump Word */
			buffer_address[5] = 0xf8;
			fragP->fr_fix += 6;	  /* account for bra/jmp instruction */
			subseg_change(SEG_TEXT,0);
			fix_new(fragP, fragP->fr_fix, 2, fragP->fr_symbol, 0, 
				fragP->fr_offset, 0, NO_RELOC);
			fragP->fr_fix += 2;
		} else {
			/* JF: these were fr_opcode[5-7] */
			buffer_address[3] = 0x06;  /* Plus 6 */
			buffer_address[4] = 0x4e;  /* put in jmp long (0x4ef9) */ 
			buffer_address[5] = 0xf9;  
			fragP->fr_fix += 6;	  /* account for bra/jmp instruction */
			subseg_change(SEG_TEXT,0);
			fix_new(fragP, fragP->fr_fix, 4, fragP->fr_symbol, 0, 
				fragP->fr_offset, 0, NO_RELOC);
			fragP->fr_fix += 4;
		}
		
		frag_wane(fragP);
		break;
	} /* case TAB(DBCC,SZ_UNDEF) */
		
	case TAB(PCLEA,SZ_UNDEF): {
		if ((S_GET_SEGMENT(fragP->fr_symbol)) == segment || flagseen['l']) {
			fragP->fr_subtype=TAB(PCLEA,SHORT);
			fragP->fr_var+=2;
		} else {
			fragP->fr_subtype=TAB(PCLEA,LONG);
			fragP->fr_var+=6;
		}
		break;
	} /* TAB(PCLEA,SZ_UNDEF) */
		
	default:
		break;
		
	} /* switch on subtype looking for SZ_UNDEF's. */
	
	/* now that SZ_UNDEF are taken care of, check others */
	switch (fragP->fr_subtype) {
	case TAB(BCC68000,BYTE):
    case TAB(BRANCH,BYTE):
	/* We can't do a short jump to the next instruction,
	   so we force word mode.  */
	if (fragP->fr_symbol && S_GET_VALUE(fragP->fr_symbol) == 0 &&
	    fragP->fr_symbol->sy_frag == fragP->fr_next) {
		fragP->fr_subtype=TAB(TABTYPE(fragP->fr_subtype),SHORT);
		fragP->fr_var+=2;
	}
	break;
 default:
	break;
}
	return fragP->fr_var + fragP->fr_fix - old_fix;
}

#if defined(OBJ_AOUT) | defined(OBJ_BOUT)
/* the bit-field entries in the relocation_info struct plays hell 
   with the byte-order problems of cross-assembly.  So as a hack,
   I added this mach. dependent ri twiddler.  Ugly, but it gets
   you there. -KWK */
/* on m68k: first 4 bytes are normal unsigned long, next three bytes
   are symbolnum, most sig. byte first.  Last byte is broken up with
   bit 7 as pcrel, bits 6 & 5 as length, bit 4 as pcrel, and the lower
   nibble as nuthin. (on Sun 3 at least) */
/* Translate the internal relocation information into target-specific
   format. */
#ifdef comment
void
    md_ri_to_chars(the_bytes, ri)
char *the_bytes;
struct reloc_info_generic *ri;
{
	/* this is easy */
	md_number_to_chars(the_bytes, ri->r_address, 4);
	/* now the fun stuff */
	the_bytes[4] = (ri->r_symbolnum >> 16) & 0x0ff;
	the_bytes[5] = (ri->r_symbolnum >> 8) & 0x0ff;
	the_bytes[6] = ri->r_symbolnum & 0x0ff;
	the_bytes[7] = (((ri->r_pcrel << 7)  & 0x80) | ((ri->r_length << 5) & 0x60) | 
			((ri->r_extern << 4)  & 0x10)); 
}
#endif /* comment */

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
	
	know(fixP->fx_addsy != NULL);
	
	md_number_to_chars(where,
			   fixP->fx_frag->fr_address + fixP->fx_where - segment_address_in_file,
			   4);
	
	r_symbolnum = (S_IS_DEFINED(fixP->fx_addsy)
		       ? S_GET_TYPE(fixP->fx_addsy)
		       : fixP->fx_addsy->sy_number);
	
	where[4] = (r_symbolnum >> 16) & 0x0ff;
	where[5] = (r_symbolnum >> 8) & 0x0ff;
	where[6] = r_symbolnum & 0x0ff;
	where[7] = (((fixP->fx_pcrel << 7)  & 0x80) | ((nbytes_r_length[fixP->fx_size] << 5) & 0x60) | 
		    (((!S_IS_DEFINED(fixP->fx_addsy)) << 4)  & 0x10));
	
	return;
} /* tc_aout_fix_to_chars() */

#endif /* OBJ_AOUT or OBJ_BOUT */

#ifndef WORKING_DOT_WORD
const int md_short_jump_size = 4;
const int md_long_jump_size = 6;

void
    md_create_short_jump(ptr,from_addr,to_addr,frag,to_symbol)
char	*ptr;
long	from_addr,
    to_addr;
fragS	*frag;
symbolS	*to_symbol;
{
	long offset;
	
	offset = to_addr - (from_addr+2);
	
	md_number_to_chars(ptr  ,(long)0x6000,2);
	md_number_to_chars(ptr+2,(long)offset,2);
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
	
	if (cpu_of_arch(current_architecture) < m68020) {
		offset=to_addr-S_GET_VALUE(to_symbol);
		md_number_to_chars(ptr  ,(long)0x4EF9,2);
		md_number_to_chars(ptr+2,(long)offset,4);
		fix_new(frag,(ptr+2)-frag->fr_literal,4,to_symbol,(symbolS *)0,(long)0,0,
			NO_RELOC);
	} else {
		offset=to_addr - (from_addr+2);
		md_number_to_chars(ptr  ,(long)0x60ff,2);
		md_number_to_chars(ptr+2,(long)offset,4);
	}
}

#endif
/* Different values of OK tell what its OK to return.  Things that aren't OK are an error (what a shock, no?)
   
   0:  Everything is OK
   10:  Absolute 1:8	only
   20:  Absolute 0:7	only
   30:  absolute 0:15	only
   40:  Absolute 0:31	only
   50:  absolute 0:127	only
   55:  absolute -64:63    only
   60:  absolute -128:127	only
   70:  absolute 0:4095	only
   80:  No bignums
   
   */

static int get_num(exp,ok)
struct m68k_exp *exp;
int ok;
{
#ifdef TEST2
	long	l = 0;
	
	if (!exp->e_beg)
	    return 0;
	if (*exp->e_beg == '0') {
		if (exp->e_beg[1] == 'x')
		    sscanf(exp->e_beg+2,"%x",&l);
		else
		    sscanf(exp->e_beg+1,"%O",&l);
		return l;
	}
	return atol(exp->e_beg);
#else
	char	*save_in;
	char	c_save;
	
	if (!exp) {
		/* Can't do anything */
		return 0;
	}
	if (!exp->e_beg || !exp->e_end) {
		seg(exp)=SEG_ABSOLUTE;
		adds(exp)=0;
		subs(exp)=0;
		offs(exp)= (ok == 10) ? 1 : 0;
		as_warn("Null expression defaults to %ld",offs(exp));
		return 0;
	}
	
	exp->e_siz=0;
	if (/* ok != 80 && */exp->e_end[-1] == ':' && (exp->e_end-exp->e_beg) >= 2) {
		switch (exp->e_end[0]) {
		case 's':
		case 'S':
		case 'b':
		case 'B':
			exp->e_siz=1;
			break;
		case 'w':
		case 'W':
			exp->e_siz=2;
			break;
		case 'l':
		case 'L':
			exp->e_siz=3;
			break;
		default:
			as_bad("Unknown size for expression \"%c\"",exp->e_end[0]);
		}
		exp->e_end-=2;
	}
	c_save=exp->e_end[1];
	exp->e_end[1]='\0';
	save_in=input_line_pointer;
	input_line_pointer=exp->e_beg;
	switch (expression(&(exp->e_exp))) {
	case SEG_PASS1:
		seg(exp)=SEG_ABSOLUTE;
		adds(exp)=0;
		subs(exp)=0;
		offs(exp)= (ok == 10) ? 1 : 0;
		as_warn("Unknown expression: '%s' defaulting to %d",exp->e_beg,offs(exp));
		break;
		
	case SEG_ABSENT:
		/* Do the same thing the VAX asm does */
		seg(exp)=SEG_ABSOLUTE;
		adds(exp)=0;
		subs(exp)=0;
		offs(exp)=0;
		if (ok == 10) {
			as_warn("expression out of range: defaulting to 1");
			offs(exp)=1;
		}
		break;
	case SEG_ABSOLUTE:
		switch (ok) {
		case 10:
			if (offs(exp)<1 || offs(exp)>8) {
				as_warn("expression out of range: defaulting to 1");
				offs(exp)=1;
			}
			break;
		case 20:
			if (offs(exp)<0 || offs(exp)>7)
			    goto outrange;
			break;
		case 30:
			if (offs(exp)<0 || offs(exp)>15)
			    goto outrange;
			break;
		case 40:
			if (offs(exp)<0 || offs(exp)>32)
			    goto outrange;
			break;
		case 50:
			if (offs(exp)<0 || offs(exp)>127)
			    goto outrange;
			break;
		case 55:
			if (offs(exp)<-64 || offs(exp)>63)
			    goto outrange;
			break;
		case 60:
			if (offs(exp)<-128 || offs(exp)>127)
			    goto outrange;
			break;
		case 70:
			if (offs(exp)<0 || offs(exp)>4095) {
			outrange:
				as_warn("expression out of range: defaulting to 0");
				offs(exp)=0;
			}
			break;
		default:
			break;
		}
		break;
	case SEG_TEXT:
	case SEG_DATA:
	case SEG_BSS:
	case SEG_UNKNOWN:
	case SEG_DIFFERENCE:
		if (ok >= 10 && ok <= 70) {
			seg(exp)=SEG_ABSOLUTE;
			adds(exp)=0;
			subs(exp)=0;
			offs(exp)= (ok == 10) ? 1 : 0;
			as_warn("Can't deal with expression \"%s\": defaulting to %ld",exp->e_beg,offs(exp));
		}
		break;
	case SEG_BIG:
		if (ok == 80 && offs(exp)<0) {	/* HACK! Turn it into a long */
			LITTLENUM_TYPE words[6];
			
			gen_to_words(words,2,8L);/* These numbers are magic! */
			seg(exp)=SEG_ABSOLUTE;
			adds(exp)=0;
			subs(exp)=0;
			offs(exp)=words[1]|(words[0]<<16);
		} else if (ok != 0) {
			seg(exp)=SEG_ABSOLUTE;
			adds(exp)=0;
			subs(exp)=0;
			offs(exp)= (ok == 10) ? 1 : 0;
			as_warn("Can't deal with expression \"%s\": defaulting to %ld",exp->e_beg,offs(exp));
		}
		break;
	default:
		as_fatal("failed sanity check.");
	}
	if (input_line_pointer != exp->e_end+1)
	    as_bad("Ignoring junk after expression");
	exp->e_end[1]=c_save;
	input_line_pointer=save_in;
	if (exp->e_siz) {
		switch (exp->e_siz) {
		case 1:
			if (!isbyte(offs(exp)))
			    as_warn("expression doesn't fit in BYTE");
			break;
		case 2:
			if (!isword(offs(exp)))
			    as_warn("expression doesn't fit in WORD");
			break;
		}
	}
	return offs(exp);
#endif
} /* get_num() */

/* These are the back-ends for the various machine dependent pseudo-ops.  */
void demand_empty_rest_of_line();	/* Hate those extra verbose names */

static void s_data1() {
	subseg_new(SEG_DATA,1);
	demand_empty_rest_of_line();
} /* s_data1() */

static void s_data2() {
	subseg_new(SEG_DATA,2);
	demand_empty_rest_of_line();
} /* s_data2() */

static void s_bss() {
	/* We don't support putting frags in the BSS segment, but we
	   can put them into initialized data for now... */
	subseg_new(SEG_DATA,255);	/* FIXME-SOON */
	demand_empty_rest_of_line();
} /* s_bss() */

static void s_even() {
	register int temp;
	register long temp_fill;
	
	temp = 1;		/* JF should be 2? */
	temp_fill = get_absolute_expression ();
	if ( ! need_pass_2 ) /* Never make frag if expect extra pass. */
	    frag_align (temp, (int)temp_fill);
	demand_empty_rest_of_line();
} /* s_even() */

static void s_proc() {
	demand_empty_rest_of_line();
} /* s_proc() */

/* s_space is defined in read.c .skip is simply an alias to it. */

/*
 * md_parse_option
 *	Invocation line includes a switch not recognized by the base assembler.
 *	See if it's a processor-specific option.  These are:
 *
 *	-[A]m[c]68000, -[A]m[c]68008, -[A]m[c]68010, -[A]m[c]68020, -[A]m[c]68030, -[A]m[c]68040
 *	-[A]m[c]68881, -[A]m[c]68882, -[A]m[c]68851
 *		Select the architecture.  Instructions or features not
 *		supported by the selected architecture cause fatal
 *		errors.  More than one may be specified.  The default is
 *		-m68020 -m68851 -m68881.  Note that -m68008 is a synonym
 *		for -m68000, and -m68882 is a synonym for -m68881.
 *
 * MAYBE_FLOAT_TOO is defined below so that specifying a processor type
 * (e.g. m68020) also requests that float instructions be included.  This
 * is the default setup, mostly to avoid hassling users.  A better
 * rearrangement of this structure would be to add an option to DENY
 * floating point opcodes, for people who want to really know there's none
 * of that funny floaty stuff going on.  FIXME-later.
 */
#ifndef MAYBE_FLOAT_TOO
#define	MAYBE_FLOAT_TOO	m68881
#endif

int md_parse_option(argP,cntP,vecP)
char **argP;
int *cntP;
char ***vecP;
{
	switch (**argP) {
	case 'l':	/* -l means keep external to 2 bit offset
			   rather than 16 bit one */
		break;
		
	case 'S': /* -S means that jbsr's always turn into jsr's.  */
		break;
		
	case 'A':
		(*argP)++;
		/* intentional fall-through */
	case 'm':
		(*argP)++;
		
		if (**argP == 'c') {
			(*argP)++;
		} /* allow an optional "c" */
		
		if (!strcmp(*argP, "68000")
		    || !strcmp(*argP, "68008")) {
			current_architecture |= m68000;
		} else if (!strcmp(*argP, "68010")) {
#ifdef TE_SUN
			omagic= 1<<16|OMAGIC;
#endif
			current_architecture |= m68010;
			
		} else if (!strcmp(*argP, "68020")) { 
			current_architecture |= m68020 | MAYBE_FLOAT_TOO;
			
		} else if (!strcmp(*argP, "68030")) { 
			current_architecture |= m68030 | MAYBE_FLOAT_TOO;
			
		} else if (!strcmp(*argP, "68040")) { 
			current_architecture |= m68040 | MAYBE_FLOAT_TOO;
			
#ifndef NO_68881
		} else if (!strcmp(*argP, "68881")) {
			current_architecture |= m68881;
			
		} else if (!strcmp(*argP, "68882")) {
			current_architecture |= m68882;
			
#endif /* NO_68881 */
#ifndef NO_68851
		} else if (!strcmp(*argP,"68851")) { 
			current_architecture |= m68851;
			
#endif /* NO_68851 */
		} else {
			as_warn("Unknown architecture, \"%s\". option ignored", *argP);
		} /* switch on architecture */
		
		while (**argP) (*argP)++;
		
		break;
		
	case 'p':
		if (!strcmp(*argP,"pic")) {
			(*argP) += 3;
			break;		/* -pic, Position Independent Code */
		} else {
			return(0);
		} /* pic or not */
		
	default:
		return 0;
	}
	return 1;
}


#ifdef TEST2

/* TEST2:  Test md_assemble() */
/* Warning, this routine probably doesn't work anymore */

main()
{
	struct m68k_it the_ins;
	char buf[120];
	char *cp;
	int	n;
	
	m68k_ip_begin();
	for (;;) {
		if (!gets(buf) || !*buf)
		    break;
		if (buf[0] == '|' || buf[1] == '.')
		    continue;
		for (cp=buf;*cp;cp++)
		    if (*cp == '\t')
			*cp=' ';
		if (is_label(buf))
		    continue;
		memset(&the_ins, '\0', sizeof(the_ins));
		m68k_ip(&the_ins,buf);
		if (the_ins.error) {
			printf("Error %s in %s\n",the_ins.error,buf);
		} else {
			printf("Opcode(%d.%s): ",the_ins.numo,the_ins.args);
			for (n=0;n<the_ins.numo;n++)
			    printf(" 0x%x",the_ins.opcode[n]&0xffff);
			printf("    ");
			print_the_insn(&the_ins.opcode[0],stdout);
			(void)putchar('\n');
		}
		for (n=0;n<strlen(the_ins.args)/2;n++) {
			if (the_ins.operands[n].error) {
				printf("op%d Error %s in %s\n",n,the_ins.operands[n].error,buf);
				continue;
			}
			printf("mode %d, reg %d, ",the_ins.operands[n].mode,the_ins.operands[n].reg);
			if (the_ins.operands[n].b_const)
			    printf("Constant: '%.*s', ",1+the_ins.operands[n].e_const-the_ins.operands[n].b_const,the_ins.operands[n].b_const);
			printf("ireg %d, isiz %d, imul %d, ",the_ins.operands[n].ireg,the_ins.operands[n].isiz,the_ins.operands[n].imul);
			if (the_ins.operands[n].b_iadd)
			    printf("Iadd: '%.*s',",1+the_ins.operands[n].e_iadd-the_ins.operands[n].b_iadd,the_ins.operands[n].b_iadd);
			(void)putchar('\n');
		}
	}
	m68k_ip_end();
	return 0;
}

is_label(str)
char *str;
{
	while (*str == ' ')
	    str++;
	while (*str && *str != ' ')
	    str++;
	if (str[-1] == ':' || str[1] == '=')
	    return 1;
	return 0;
}

#endif

/* Possible states for relaxation:
   
   0 0	branch offset	byte	(bra, etc)
   0 1			word
   0 2			long
   
   1 0	indexed offsets	byte	a0@(32,d4:w:1) etc
   1 1			word
   1 2			long
   
   2 0	two-offset index word-word a0@(32,d4)@(45) etc
   2 1			word-long
   2 2			long-word
   2 3			long-long
   
   */



#ifdef DONTDEF
abort()
{
	printf("ABORT!\n");
	exit(12);
}

print_frags()
{
	fragS *fragP;
	extern fragS *text_frag_root;
	
	for (fragP=text_frag_root;fragP;fragP=fragP->fr_next) {
		printf("addr %lu  next 0x%x  fix %ld  var %ld  symbol 0x%x  offset %ld\n",
		       fragP->fr_address,fragP->fr_next,fragP->fr_fix,fragP->fr_var,fragP->fr_symbol,fragP->fr_offset);
		printf("opcode 0x%x  type %d  subtype %d\n\n",fragP->fr_opcode,fragP->fr_type,fragP->fr_subtype);
	}
	fflush(stdout);
	return 0;
}
#endif

#ifdef DONTDEF
/*VARARGS1*/
panic(format,args)
char *format;
{
	fputs("Internal error:",stderr);
	_doprnt(format,&args,stderr);
	(void)putc('\n',stderr);
	as_where();
	abort();
}
#endif

/* We have no need to default values of symbols.  */

/* ARGSUSED */
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
   On the 68k, they're relative to the address of the offset, plus
   its size. (??? Is this right?  FIXME-SOON!) */
long
    md_pcrel_from (fixP)
fixS *fixP;
{
	return(fixP->fx_where + fixP->fx_frag->fr_address);
}

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of tc-m68k.c */
