/* tc-a29k.c -- Assemble for the AMD 29000.
   Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.

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

/* John Gilmore has reorganized this module somewhat, to make it easier
   to convert it to new machines' assemblers as desired.  There was too
   much bloody rewriting required before.  There still probably is.  */

#include "as.h"

#include "opcode/a29k.h"

/* Make it easier to clone this machine desc into another one.  */
#define	machine_opcode	a29k_opcode
#define	machine_opcodes	a29k_opcodes
#define	machine_ip	a29k_ip
#define	machine_it	a29k_it

const relax_typeS md_relax_table[] = { 0 };

#define	IMMEDIATE_BIT	0x01000000	/* Turns RB into Immediate */
#define	ABSOLUTE_BIT	0x01000000	/* Turns PC-relative to Absolute */
#define	CE_BIT		0x00800000	/* Coprocessor enable in LOAD */
#define	UI_BIT		0x00000080	/* Unsigned integer in CONVERT */

/* handle of the OPCODE hash table */
static struct hash_control *op_hash = NULL;

struct machine_it {
	char    *error;
	unsigned long opcode;
	struct nlist *nlistp;
	expressionS exp;
	int pcrel;
	int  reloc_offset;		/* Offset of reloc within insn */
	enum reloc_type reloc;
} the_insn;

#if __STDC__ == 1

/* static int getExpression(char *str); */
static void machine_ip(char *str);
/* static void print_insn(struct machine_it *insn); */
static void s_data1(void);
static void s_use(void);

#else /* not __STDC__ */

/* static int getExpression(); */
static void machine_ip();
/* static void print_insn(); */
static void s_data1();
static void s_use();

#endif /* not __STDC__ */

const pseudo_typeS
    md_pseudo_table[] = {
	    { "align",	s_align_bytes,	4 },
	    { "block", 	s_space,	0 },
	    { "cputype",	s_ignore,	0 },	/* CPU as 29000 or 29050 */
	    { "reg",	s_lsym,		0 },	/* Register equate, same as equ */
	    { "space",	s_ignore,	0 },	/* Listing control */
	    { "sect",	s_ignore,	0 },	/* Creation of coff sections */
	    { "use",	s_use,		0 },
	    { "word",	cons,		4 },
	    { NULL,	0,		0 },
    };

int md_short_jump_size = 4;
int md_long_jump_size = 4;
#if defined(BFD_HEADERS)
#ifdef RELSZ
const int md_reloc_size = RELSZ;	/* Coff headers */
#else
const int md_reloc_size = 12;		/* something else headers */
#endif
#else
const int md_reloc_size = 12;		/* Not bfdized*/
#endif

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful */
char comment_chars[] = ";";

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output. */
/* Also note that comments like this one will always work */
char line_comment_chars[] = "#";

/* We needed an unused char for line separation to work around the
   lack of macros, using sed and such.  */
char line_separator_chars[] = "@";

/* Chars that can be used to separate mant from exp in floating point nums */
char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant */
/* As in 0f12.456 */
/* or    0d1.2345e12 */
char FLT_CHARS[] = "rRsSfFdDxXpP";

/* Also be aware that MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT may have to be
   changed in read.c. Ideally it shouldn't have to know about it at all,
   but nothing is ideal around here.
   */

static unsigned char octal[256];
#define isoctal(c)  octal[c]
    static unsigned char toHex[256];

/*
 *  anull bit - causes the branch delay slot instructions to not be executed
 */
#define ANNUL       (1 << 29)

static void
    s_use()
{

	if (strncmp(input_line_pointer, ".text", 5) == 0) {
		input_line_pointer += 5;
		s_text();
		return;
	}
	if (strncmp(input_line_pointer, ".data", 5) == 0) {
		input_line_pointer += 5;
		s_data();
		return;
	}
	if (strncmp(input_line_pointer, ".data1", 6) == 0) {
		input_line_pointer += 6;
		s_data1();
		return;
	}
	/* Literals can't go in the text segment because you can't read
	   from instruction memory on some 29k's.  So, into initialized data. */
	if (strncmp(input_line_pointer, ".lit", 4) == 0) {
		input_line_pointer += 4;
		subseg_new(SEG_DATA, 200);
		demand_empty_rest_of_line();
		return;
	}

	as_bad("Unknown segment type");
	demand_empty_rest_of_line();
	return;
}

static void
    s_data1()
{
	subseg_new(SEG_DATA, 1);
	demand_empty_rest_of_line();
	return;
}

/* Install symbol definition that maps REGNAME to REGNO.
   FIXME-SOON:  These are not recognized in mixed case.  */

static void
    insert_sreg (regname, regnum)
char *regname;
int regnum;
{
	/* FIXME-SOON, put something in these syms so they won't be output to the symbol
	   table of the resulting object file.  */

	/* Must be large enough to hold the names of the special registers.  */
	char buf[80];
	int i;

	symbol_table_insert(symbol_new(regname, SEG_REGISTER, regnum, &zero_address_frag));
	for (i = 0; regname[i]; i++)
	    buf[i] = islower (regname[i]) ? toupper (regname[i]) : regname[i];
	buf[i] = '\0';

	symbol_table_insert(symbol_new(buf, SEG_REGISTER, regnum, &zero_address_frag));
} /* insert_sreg() */

/* Install symbol definitions for assorted special registers.
   See ASM29K Ref page 2-9.  */

void define_some_regs() {
#define SREG	256

	/* Protected special-purpose register names */
	insert_sreg ("vab", SREG+0);
	insert_sreg ("ops", SREG+1);
	insert_sreg ("cps", SREG+2);
	insert_sreg ("cfg", SREG+3);
	insert_sreg ("cha", SREG+4);
	insert_sreg ("chd", SREG+5);
	insert_sreg ("chc", SREG+6);
	insert_sreg ("rbp", SREG+7);
	insert_sreg ("tmc", SREG+8);
	insert_sreg ("tmr", SREG+9);
	insert_sreg ("pc0", SREG+10);
	insert_sreg ("pc1", SREG+11);
	insert_sreg ("pc2", SREG+12);
	insert_sreg ("mmu", SREG+13);
	insert_sreg ("lru", SREG+14);

	/* Unprotected special-purpose register names */
	insert_sreg ("ipc", SREG+128);
	insert_sreg ("ipa", SREG+129);
	insert_sreg ("ipb", SREG+130);
	insert_sreg ("q",   SREG+131);
	insert_sreg ("alu", SREG+132);
	insert_sreg ("bp",  SREG+133);
	insert_sreg ("fc",  SREG+134);
	insert_sreg ("cr",  SREG+135);
	insert_sreg ("fpe", SREG+160);
	insert_sreg ("inte",SREG+161);
	insert_sreg ("fps", SREG+162);
	/*  "",    SREG+163);	  Reserved */
	insert_sreg ("exop",SREG+164);
} /* define_some_regs() */

/* This function is called once, at assembler startup time.  It should
   set up all the tables, etc. that the MD part of the assembler will need.  */
void
    md_begin()
{
	register char *retval = NULL;
	int lose = 0;
	register int skipnext = 0;
	register unsigned int i;
	register char *strend, *strend2;

	/* Hash up all the opcodes for fast use later.  */

	op_hash = hash_new();
	if (op_hash == NULL)
	    as_fatal("Virtual memory exhausted");

	for (i = 0; i < num_opcodes; i++)
	    {
		    const char *name = machine_opcodes[i].name;

		    if (skipnext) {
			    skipnext = 0;
			    continue;
		    }

		    /* Hack to avoid multiple opcode entries.  We pre-locate all the
		       variations (b/i field and P/A field) and handle them. */

		    if (!strcmp (name, machine_opcodes[i+1].name)) {
			    if ((machine_opcodes[i].opcode ^ machine_opcodes[i+1].opcode)
				!= 0x01000000)
				goto bad_table;
			    strend =  machine_opcodes[i  ].args+strlen(machine_opcodes[i  ].args)-1;
			    strend2 = machine_opcodes[i+1].args+strlen(machine_opcodes[i+1].args)-1;
			    switch (*strend) {
			    case 'b':
				    if (*strend2 != 'i')  goto bad_table;
				    break;
			    case 'i':
				    if (*strend2 != 'b')  goto bad_table;
				    break;
			    case 'P':
				    if (*strend2 != 'A')  goto bad_table;
				    break;
			    case 'A':
				    if (*strend2 != 'P')  goto bad_table;
				    break;
			    default:
			    bad_table:
				    fprintf (stderr, "internal error: can't handle opcode %s\n", name);
				    lose = 1;
			    }

			    /* OK, this is an i/b or A/P pair.  We skip the higher-valued one,
			       and let the code for operand checking handle OR-ing in the bit.  */
			    if (machine_opcodes[i].opcode & 1)
				continue;
			    else
				skipnext = 1;
		    }

		    retval = hash_insert (op_hash, name, &machine_opcodes[i]);
		    if (retval != NULL && *retval != '\0')
			{
				fprintf (stderr, "internal error: can't hash `%s': %s\n",
					 machine_opcodes[i].name, retval);
				lose = 1;
			}
	    }

	if (lose)
	    as_fatal("Broken assembler.  No assembly attempted.");

	for (i = '0'; i < '8'; ++i)
	    octal[i] = 1;
	for (i = '0'; i <= '9'; ++i)
	    toHex[i] = i - '0';
	for (i = 'a'; i <= 'f'; ++i)
	    toHex[i] = i + 10 - 'a';
	for (i = 'A'; i <= 'F'; ++i)
	    toHex[i] = i + 10 - 'A';

	define_some_regs ();
}

void md_end() {
	return;
}

/* Assemble a single instruction.  Its label has already been handled
   by the generic front end.  We just parse opcode and operands, and
   produce the bytes of data and relocation.  */

void md_assemble(str)
char *str;
{
	char *toP;
	/* !!!!    int rsd; */

	know(str);
	machine_ip(str);
	toP = frag_more(4);
	/* put out the opcode */
	md_number_to_chars(toP, the_insn.opcode, 4);

	/* put out the symbol-dependent stuff */
	if (the_insn.reloc != NO_RELOC) {
		fix_new(
			frag_now,                           /* which frag */
			(toP - frag_now->fr_literal + the_insn.reloc_offset), /* where */
			4,                                  /* size */
			the_insn.exp.X_add_symbol,
			the_insn.exp.X_subtract_symbol,
			the_insn.exp.X_add_number,
			the_insn.pcrel,
			the_insn.reloc
			);
	}
}

char *
    parse_operand (s, operandp)
char *s;
expressionS *operandp;
{
	char *save = input_line_pointer;
	char *new;
	segT seg;

	input_line_pointer = s;
	seg = expr (0, operandp);
	new = input_line_pointer;
	input_line_pointer = save;

	switch (seg) {
	case SEG_ABSOLUTE:
	case SEG_TEXT:
	case SEG_DATA:
	case SEG_BSS:
	case SEG_UNKNOWN:
	case SEG_DIFFERENCE:
	case SEG_BIG:
	case SEG_REGISTER:
		return new;

	case SEG_ABSENT:
		as_bad("Missing operand");
		return new;

	default:
		as_bad("Don't understand operand of type %s", segment_name (seg));
		return new;
	}
}

/* Instruction parsing.  Takes a string containing the opcode.
   Operands are at input_line_pointer.  Output is in the_insn.
   Warnings or errors are generated.  */

static void
    machine_ip(str)
char *str;
{
	char *s;
	const char *args;
	/* !!!!    char c; */
	/* !!!!    unsigned long i; */
	struct machine_opcode *insn;
	char *argsStart;
	unsigned long   opcode;
	/* !!!!    unsigned int mask; */
	expressionS the_operand;
	expressionS *operand = &the_operand;
	unsigned int reg;

	/* Must handle `div0' opcode.  */
	s = str;
	if (isalpha(*s))
	    for (; isalnum(*s); ++s)
		if (isupper (*s))
		    *s = tolower (*s);

	switch (*s) {
	case '\0':
		break;

	case ' ':		/* FIXME-SOMEDAY more whitespace */
		*s++ = '\0';
		break;

	default:
		as_bad("Unknown opcode: `%s'", str);
		return;
	}
	if ((insn = (struct machine_opcode *) hash_find(op_hash, str)) == NULL) {
		as_bad("Unknown opcode `%s'.", str);
		return;
	}
	argsStart = s;
	opcode = insn->opcode;
	memset(&the_insn, '\0', sizeof(the_insn));
	the_insn.reloc = NO_RELOC;

	/*
	 * Build the opcode, checking as we go to make
	 * sure that the operands match.
	 *
	 * If an operand matches, we modify the_insn or opcode appropriately,
	 * and do a "continue".  If an operand fails to match, we "break".
	 */
	if (insn->args[0] != '\0')
	    s = parse_operand (s, operand);	/* Prime the pump */

	for (args = insn->args; ; ++args) {
		switch (*args) {

		case '\0':  /* end of args */
			if (*s == '\0') {
				/* We are truly done. */
				the_insn.opcode = opcode;
				return;
			}
			as_bad("Too many operands: %s", s);
			break;

		case ',':	/* Must match a comma */
			if (*s++ == ',') {
				s = parse_operand (s, operand);	/* Parse next opnd */
				continue;
			}
			break;

		case 'v':		/* Trap numbers (immediate field) */
			if (operand->X_seg == SEG_ABSOLUTE) {
				if (operand->X_add_number < 256) {
					opcode |= (operand->X_add_number << 16);
					continue;
				} else {
					as_bad("Immediate value of %d is too large",
					       operand->X_add_number);
					continue;
				}
			}
			the_insn.reloc = RELOC_8;
			the_insn.reloc_offset = 1;	/* BIG-ENDIAN Byte 1 of insn */
			the_insn.exp = *operand;
			continue;

		case 'b':	/* A general register or 8-bit immediate */
		case 'i':
			/* We treat the two cases identically since we mashed
			   them together in the opcode table.  */
			if (operand->X_seg == SEG_REGISTER)
			    goto general_reg;

			opcode |= IMMEDIATE_BIT;
			if (operand->X_seg == SEG_ABSOLUTE) {
				if (operand->X_add_number < 256) {
					opcode |= operand->X_add_number;
					continue;
				} else {
					as_bad("Immediate value of %d is too large",
					       operand->X_add_number);
					continue;
				}
			}
			the_insn.reloc = RELOC_8;
			the_insn.reloc_offset = 3;	/* BIG-ENDIAN Byte 3 of insn */
			the_insn.exp = *operand;
			continue;

		case 'a':   /* next operand must be a register */
		case 'c':
		general_reg:
			/* lrNNN or grNNN or %%expr or a user-def register name */
			if (operand->X_seg != SEG_REGISTER)
			    break;		/* Only registers */
			know (operand->X_add_symbol == 0);
			know (operand->X_subtract_symbol == 0);
			reg = operand->X_add_number;
			if (reg >= SREG)
			    break;		/* No special registers */

			/*
			 * Got the register, now figure out where
			 * it goes in the opcode.
			 */
			switch (*args) {
			case 'a':
				opcode |= reg << 8;
				continue;

			case 'b':
			case 'i':
				opcode |= reg;
				continue;

			case 'c':
				opcode |= reg << 16;
				continue;
			}
			as_fatal("failed sanity check.");
			break;

		case 'x':		/* 16 bit constant, zero-extended */
		case 'X':		/* 16 bit constant, one-extended */
			if (operand->X_seg == SEG_ABSOLUTE) {
				opcode |=  (operand->X_add_number & 0xFF)   << 0 |
				    ((operand->X_add_number & 0xFF00) << 8);
				continue;
			}
			the_insn.reloc = RELOC_CONST;
			the_insn.exp = *operand;
			continue;

		case 'h':
			if (operand->X_seg == SEG_ABSOLUTE) {
				opcode |=  (operand->X_add_number & 0x00FF0000) >> 16 |
				    (((unsigned long)operand->X_add_number
				      /* avoid sign ext */	  & 0xFF000000) >> 8);
				continue;
			}
			the_insn.reloc = RELOC_CONSTH;
			the_insn.exp = *operand;
			continue;

		case 'P':		/* PC-relative jump address */
		case 'A':		/* Absolute jump address */
			/* These two are treated together since we folded the
			   opcode table entries together.  */
			if (operand->X_seg == SEG_ABSOLUTE) {
				opcode |= ABSOLUTE_BIT |
				    (operand->X_add_number & 0x0003FC00) << 6 |
					((operand->X_add_number & 0x000003FC) >> 2);
				continue;
			}
			the_insn.reloc = RELOC_JUMPTARG;
			the_insn.exp = *operand;
			the_insn.pcrel = 1;		/* Assume PC-relative jump */
			/* FIXME-SOON, Do we figure out whether abs later, after know sym val? */
			continue;

		case 'e':		/* Coprocessor enable bit for LOAD/STORE insn */
			if (operand->X_seg == SEG_ABSOLUTE) {
				if (operand->X_add_number == 0)
				    continue;
				if (operand->X_add_number == 1) {
					opcode |= CE_BIT;
					continue;
				}
			}
			break;

		case 'n':		/* Control bits for LOAD/STORE instructions */
			if (operand->X_seg == SEG_ABSOLUTE &&
			    operand->X_add_number < 128) {
				opcode |= (operand->X_add_number << 16);
				continue;
			}
			break;

		case 's':		/* Special register number */
			if (operand->X_seg != SEG_REGISTER)
			    break;		/* Only registers */
			if (operand->X_add_number < SREG)
			    break;		/* Not a special register */
			opcode |= (operand->X_add_number & 0xFF) << 8;
			continue;

		case 'u':		/* UI bit of CONVERT */
			if (operand->X_seg == SEG_ABSOLUTE) {
				if (operand->X_add_number == 0)
				    continue;
				if (operand->X_add_number == 1) {
					opcode |= UI_BIT;
					continue;
				}
			}
			break;

		case 'r':		/* RND bits of CONVERT */
			if (operand->X_seg == SEG_ABSOLUTE &&
			    operand->X_add_number < 8) {
				opcode |= operand->X_add_number << 4;
				continue;
			}
			break;

		case 'd':		/* FD bits of CONVERT */
			if (operand->X_seg == SEG_ABSOLUTE &&
			    operand->X_add_number < 4) {
				opcode |= operand->X_add_number << 2;
				continue;
			}
			break;


		case 'f':		/* FS bits of CONVERT */
			if (operand->X_seg == SEG_ABSOLUTE &&
			    operand->X_add_number < 4) {
				opcode |= operand->X_add_number << 0;
				continue;
			}
			break;

		case 'C':
			if (operand->X_seg == SEG_ABSOLUTE &&
			    operand->X_add_number < 4) {
				opcode |= operand->X_add_number << 16;
				continue;
			}
			break;

		case 'F':
			if (operand->X_seg == SEG_ABSOLUTE &&
			    operand->X_add_number < 16) {
				opcode |= operand->X_add_number << 18;
				continue;
			}
			break;

		default:
			BAD_CASE (*args);
		}
		/* Types or values of args don't match.  */
		as_bad("Invalid operands");
		return;
	}
}

/*
  This is identical to the md_atof in m68k.c.  I think this is right,
  but I'm not sure.

  Turn a string in input_line_pointer into a floating point constant of type
  type, and store the appropriate bytes in *litP.  The number of LITTLENUMS
  emitted is stored in *sizeP. An error message is returned, or NULL on OK.
  */

/* Equal to MAX_PRECISION in atof-ieee.c */
#define MAX_LITTLENUMS 6

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

/*
 * Write out big-endian.
 */
void
    md_number_to_chars(buf, val, n)
char *buf;
long val;
int n;
{

	switch (n) {

	case 4:
		*buf++ = val >> 24;
		*buf++ = val >> 16;
	case 2:
		*buf++ = val >> 8;
	case 1:
		*buf = val;
		break;

	default:
		as_fatal("failed sanity check.");
	}
	return;
}

void md_apply_fix(fixP, val)
fixS *fixP;
long val;
{
	char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;

	fixP->fx_addnumber = val;	/* Remember value for emit_reloc */


	know(fixP->fx_size == 4);
	know(fixP->fx_r_type < NO_RELOC);

	/*
	 * This is a hack.  There should be a better way to
	 * handle this.
	 */
	if (fixP->fx_r_type == RELOC_WDISP30 && fixP->fx_addsy) {
		val += fixP->fx_where + fixP->fx_frag->fr_address;
	}

	switch (fixP->fx_r_type) {

	case RELOC_32:
		buf[0] = val >> 24;
		buf[1] = val >> 16;
		buf[2] = val >> 8;
		buf[3] = val;
		break;

	case RELOC_8:
		buf[0] = val;
		break;

	case RELOC_WDISP30:
		val = (val >>= 2) + 1;
		buf[0] |= (val >> 24) & 0x3f;
		buf[1]= (val >> 16);
		buf[2] = val >> 8;
		buf[3] = val;
		break;

	case RELOC_HI22:
		buf[1] |= (val >> 26) & 0x3f;
		buf[2] = val >> 18;
		buf[3] = val >> 10;
		break;

	case RELOC_LO10:
		buf[2] |= (val >> 8) & 0x03;
		buf[3] = val;
		break;

	case RELOC_BASE13:
		buf[2] |= (val >> 8) & 0x1f;
		buf[3] = val;
		break;

	case RELOC_WDISP22:
		val = (val >>= 2) + 1;
		/* FALLTHROUGH */
	case RELOC_BASE22:
		buf[1] |= (val >> 16) & 0x3f;
		buf[2] = val >> 8;
		buf[3] = val;
		break;

#if 0
	case RELOC_PC10:
	case RELOC_PC22:
	case RELOC_JMP_TBL:
	case RELOC_SEGOFF16:
	case RELOC_GLOB_DAT:
	case RELOC_JMP_SLOT:
	case RELOC_RELATIVE:
#endif
	case RELOC_JUMPTARG:	/* 00XX00XX pattern in a word */
		buf[1] = val >> 10;	/* Holds bits 0003FFFC of address */
		buf[3] = val >> 2;
		break;

	case RELOC_CONST:		/* 00XX00XX pattern in a word */
		buf[1] = val >> 8;	/* Holds bits 0000XXXX */
		buf[3] = val;
		break;

	case RELOC_CONSTH:		/* 00XX00XX pattern in a word */
		buf[1] = val >> 24;	/* Holds bits XXXX0000 */
		buf[3] = val >> 16;
		break;

	case NO_RELOC:
	default:
		as_bad("bad relocation type: 0x%02x", fixP->fx_r_type);
		break;
	}
	return;
}

#ifdef OBJ_COFF
short tc_coff_fix2rtype(fixP)
fixS *fixP;
{

	/* FIXME-NOW: relocation type handling is not yet written for
	   a29k. */


	switch (fixP->fx_r_type) {
	case RELOC_32:	return(R_WORD);
	case RELOC_8:	return(R_BYTE);
	case RELOC_CONST: return (R_ILOHALF);
	case RELOC_CONSTH: return (R_IHIHALF);
	case RELOC_JUMPTARG: return (R_IREL);
	default:	printf("need %o3\n", fixP->fx_r_type);
		abort(0);
	} /* switch on type */

	return(0);
} /* tc_coff_fix2rtype() */
#endif /* OBJ_COFF */

/* should never be called for sparc */
void md_create_short_jump(ptr, from_addr, to_addr, frag, to_symbol)
char *ptr;
long from_addr, to_addr;
fragS *frag;
symbolS *to_symbol;
{
	as_fatal("a29k_create_short_jmp\n");
}

/* should never be called for 29k */
void md_convert_frag(headers, fragP)
object_headers *headers;
register fragS *fragP;
{
	as_fatal("sparc_convert_frag\n");
}

/* should never be called for 29k */
void md_create_long_jump(ptr, from_addr, to_addr, frag, to_symbol)
char	*ptr;
long	from_addr;
long    to_addr;
fragS	*frag;
symbolS	*to_symbol;
{
	as_fatal("sparc_create_long_jump\n");
}

/* should never be called for a29k */
int md_estimate_size_before_relax(fragP, segtype)
register fragS *fragP;
segT segtype;
{
	as_fatal("sparc_estimate_size_before_relax\n");
	return(0);
}

#if 0
/* for debugging only */
static void
    print_insn(insn)
struct machine_it *insn;
{
	char *Reloc[] = {
		"RELOC_8",
		"RELOC_16",
		"RELOC_32",
		"RELOC_DISP8",
		"RELOC_DISP16",
		"RELOC_DISP32",
		"RELOC_WDISP30",
		"RELOC_WDISP22",
		"RELOC_HI22",
		"RELOC_22",
		"RELOC_13",
		"RELOC_LO10",
		"RELOC_SFA_BASE",
		"RELOC_SFA_OFF13",
		"RELOC_BASE10",
		"RELOC_BASE13",
		"RELOC_BASE22",
		"RELOC_PC10",
		"RELOC_PC22",
		"RELOC_JMP_TBL",
		"RELOC_SEGOFF16",
		"RELOC_GLOB_DAT",
		"RELOC_JMP_SLOT",
		"RELOC_RELATIVE",
		"NO_RELOC"
	    };

	if (insn->error) {
		fprintf(stderr, "ERROR: %s\n");
	}
	fprintf(stderr, "opcode=0x%08x\n", insn->opcode);
	fprintf(stderr, "reloc = %s\n", Reloc[insn->reloc]);
	fprintf(stderr, "exp =  {\n");
	fprintf(stderr, "\t\tX_add_symbol = %s\n",
		insn->exp.X_add_symbol ?
		(S_GET_NAME(insn->exp.X_add_symbol) ?
		 S_GET_NAME(insn->exp.X_add_symbol) : "???") : "0");
	fprintf(stderr, "\t\tX_sub_symbol = %s\n",
		insn->exp.X_subtract_symbol ?
		(S_GET_NAME(insn->exp.X_subtract_symbol) ?
		 S_GET_NAME(insn->exp.X_subtract_symbol) : "???") : "0");
	fprintf(stderr, "\t\tX_add_number = %d\n",
		insn->exp.X_add_number);
	fprintf(stderr, "}\n");
	return;
}
#endif

/* Translate internal representation of relocation info to target format.

   On sparc/29k: first 4 bytes are normal unsigned long address, next three
   bytes are index, most sig. byte first.  Byte 7 is broken up with
   bit 7 as external, bits 6 & 5 unused, and the lower
   five bits as relocation type.  Next 4 bytes are long addend. */
/* Thanx and a tip of the hat to Michael Bloom, mb@ttidca.tti.com */

#ifdef OBJ_AOUT

void tc_aout_fix_to_chars(where, fixP, segment_address_in_file)
char *where;
fixS *fixP;
relax_addressT segment_address_in_file;
{
	long r_symbolnum;

	know(fixP->fx_r_type < NO_RELOC);
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
	where[7] = (((!S_IS_DEFINED(fixP->fx_addsy)) << 7)  & 0x80) | (0 & 0x60) | (fixP->fx_r_type & 0x1F);
	/* Also easy */
	md_number_to_chars(&where[8], fixP->fx_addnumber, 4);

	return;
} /* tc_aout_fix_to_chars() */

#endif /* OBJ_AOUT */

int
    md_parse_option(argP,cntP,vecP)
char **argP;
int *cntP;
char ***vecP;
{
	return(0);
}


/* Default the values of symbols known that should be "predefined".  We
   don't bother to predefine them unless you actually use one, since there
   are a lot of them.  */

symbolS *md_undefined_symbol (name)
char *name;
{
	long regnum;
	char testbuf[5+ /*SLOP*/ 5];

	if (name[0] == 'g' || name[0] == 'G' || name[0] == 'l' || name[0] == 'L')
	    {
		    /* Perhaps a global or local register name */
		    if (name[1] == 'r' || name[1] == 'R')
			{
				/* Parse the number, make sure it has no extra zeroes or trailing
				   chars */
				regnum = atol(&name[2]);
				if (regnum > 127)
				    return 0;
				sprintf(testbuf, "%ld", regnum);
				if (strcmp (testbuf, &name[2]) != 0)
				    return 0;	/* gr007 or lr7foo or whatever */

				/* We have a wiener!  Define and return a new symbol for it.  */
				if (name[0] == 'l' || name[0] == 'L')
				    regnum += 128;
				return(symbol_new(name, SEG_REGISTER, regnum, &zero_address_frag));
			}
	    }

	return 0;
}

/* Parse an operand that is machine-specific.  */

void md_operand(expressionP)
expressionS *expressionP;
{

	if (input_line_pointer[0] == '%' && input_line_pointer[1] == '%')
	    {
		    /* We have a numeric register expression.  No biggy.  */
		    input_line_pointer += 2;	/* Skip %% */
		    (void)expression (expressionP);
		    if (expressionP->X_seg != SEG_ABSOLUTE
			|| expressionP->X_add_number > 255)
			as_bad("Invalid expression after %%%%\n");
		    expressionP->X_seg = SEG_REGISTER;
	    }
	else if (input_line_pointer[0] == '&')
	    {
		    /* We are taking the 'address' of a register...this one is not
		       in the manual, but it *is* in traps/fpsymbol.h!  What they
		       seem to want is the register number, as an absolute number.  */
		    input_line_pointer++;		/* Skip & */
		    (void)expression (expressionP);
		    if (expressionP->X_seg != SEG_REGISTER)
			as_bad("Invalid register in & expression");
		    else
			expressionP->X_seg = SEG_ABSOLUTE;
	    }
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
   On the 29000, they're relative to the address of the instruction,
   which we have set up as the address of the fixup too.  */
long md_pcrel_from (fixP)
fixS *fixP;
{
	return fixP->fx_where + fixP->fx_frag->fr_address;
}

/*
 * Local Variables:
 * comment-column: 0
 * End:
 */

/* end of tc-a29k.c */
