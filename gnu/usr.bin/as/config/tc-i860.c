/* tc-i860.c -- Assemble for the I860
   Copyright (C) 1989, 1992 Free Software Foundation, Inc.
   
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

#include "as.h"

#include "opcode/i860.h"

void md_begin();
void md_end();
void md_number_to_chars();
void md_assemble();
char *md_atof();
void md_convert_frag();
void md_create_short_jump();
void md_create_long_jump();
int  md_estimate_size_before_relax();
void md_number_to_imm();
void md_number_to_disp();
void md_number_to_field();
void md_ri_to_chars();
static void i860_ip();

const relax_typeS md_relax_table[] = { 0 };

/* handle of the OPCODE hash table */
static struct hash_control *op_hash = NULL;

static void s_dual(), s_enddual();
static void s_atmp();

const pseudo_typeS
    md_pseudo_table[] = {
	    { "dual",       s_dual,     4 },
	    { "enddual",    s_enddual,  4 },
	    { "atmp",	    s_atmp,	4 },
	    { NULL,         0,          0 },
    };

int md_short_jump_size = 4;
int md_long_jump_size = 4;

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful */
char comment_chars[] = "!/";	/* JF removed '|' from comment_chars */

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output. */
/* Also note that comments like this one will always work. */
char line_comment_chars[] = "#/";

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
int size_reloc_info = sizeof(struct relocation_info);

static unsigned char octal[256];
#define isoctal(c)  octal[c]
    static unsigned char toHex[256];

struct i860_it {
	char    *error;
	unsigned long opcode;
	struct nlist *nlistp;
	expressionS exp;
	int pcrel;
	enum expand_type expand;
	enum highlow_type highlow;
	enum reloc_type reloc;
} the_insn;

#if __STDC__ == 1

#ifdef comment
static void print_insn(struct i860_it *insn);
#endif /* comment */

static int getExpression(char *str);

#else /* not __STDC__ */

#ifdef comment
static void print_insn();
#endif /* comment */

static int getExpression();

#endif /* not __STDC__ */

static char *expr_end;
static char last_expand;	/* error if expansion after branch */

enum dual
{
	DUAL_OFF = 0, DUAL_ON, DUAL_DDOT, DUAL_ONDDOT,
};
static enum dual dual_mode = DUAL_OFF;	/* dual-instruction mode */

static void
    s_dual()	/* floating point instructions have dual set */
{
	dual_mode = DUAL_ON;
}

static void
    s_enddual()	/* floating point instructions have dual set */
{
	dual_mode = DUAL_OFF;
}

static int atmp = 31; /* temporary register for pseudo's */

static void
    s_atmp()
{
	register int temp;
	if (strncmp(input_line_pointer, "sp", 2) == 0) {
		input_line_pointer += 2;
		atmp = 2;
	}
	else if (strncmp(input_line_pointer, "fp", 2) == 0) {
		input_line_pointer += 2;
		atmp = 3;
	}
	else if (strncmp(input_line_pointer, "r", 1) == 0) {
		input_line_pointer += 1;
		temp = get_absolute_expression();
		if (temp >= 0 && temp <= 31)
		    atmp = temp;
		else
		    as_bad("Unknown temporary pseudo register");
	}
	else {
		as_bad("Unknown temporary pseudo register");
	}
	demand_empty_rest_of_line();
	return;
}

/* This function is called once, at assembler startup time.  It should
   set up all the tables, etc. that the MD part of the assembler will need.  */
void
    md_begin()
{
	register char *retval = NULL;
	int lose = 0;
	register unsigned int i = 0;
	
	op_hash = hash_new();
	if (op_hash == NULL)
	    as_fatal("Virtual memory exhausted");
	
	while (i < NUMOPCODES)
	    {
		    const char *name = i860_opcodes[i].name;
		    retval = hash_insert(op_hash, name, &i860_opcodes[i]);
		    if (retval != NULL && *retval != '\0')
			{
				fprintf (stderr, "internal error: can't hash `%s': %s\n",
					 i860_opcodes[i].name, retval);
				lose = 1;
			}
		    do
			{
				if (i860_opcodes[i].match & i860_opcodes[i].lose)
				    {
					    fprintf (stderr, "internal error: losing opcode: `%s' \"%s\"\n",
						     i860_opcodes[i].name, i860_opcodes[i].args);
					    lose = 1;
				    }
				++i;
			} while (i < NUMOPCODES
				 && !strcmp(i860_opcodes[i].name, name));
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
}

void
    md_end()
{
	return;
}

void
    md_assemble(str)
char *str;
{
	char *toP;
/*	int rsd; FIXME: remove this line. */
	int no_opcodes = 1;
	int i;
	struct i860_it pseudo[3];
	
	assert(str);
	i860_ip(str);
	
	/* check for expandable flag to produce pseudo-instructions */
	if (the_insn.expand != 0 && the_insn.highlow == NO_SPEC) {
		for (i = 0; i < 3; i++)
		    pseudo[i] = the_insn;
		
		switch (the_insn.expand) {
			
		case E_DELAY:
			no_opcodes = 1;
			break;
			
		case E_MOV:
			if (the_insn.exp.X_add_symbol == NULL &&
			    the_insn.exp.X_subtract_symbol == NULL &&
			    (the_insn.exp.X_add_number < (1 << 15) &&
			     the_insn.exp.X_add_number >= -(1 << 15)))
			    break;
			/* or l%const,r0,ireg_dest */
			pseudo[0].opcode = (the_insn.opcode & 0x001f0000) | 0xe4000000;
			pseudo[0].highlow = PAIR;
			/* orh h%const,ireg_dest,ireg_dest */
			pseudo[1].opcode = (the_insn.opcode & 0x03ffffff) | 0xec000000 |
			    ((the_insn.opcode & 0x001f0000) << 5);
			pseudo[1].highlow = HIGH;
			no_opcodes = 2;
			break;
			
		case E_ADDR:
			if (the_insn.exp.X_add_symbol == NULL &&
			    the_insn.exp.X_subtract_symbol == NULL)
			    break;
			/* orh ha%addr_expr,r0,r31 */
			pseudo[0].opcode = 0xec000000 | (atmp<<16);
			pseudo[0].highlow = HIGHADJ;
			pseudo[0].reloc = LOW0;	/* must overwrite */
			/* l%addr_expr(r31),ireg_dest */
			pseudo[1].opcode = (the_insn.opcode & ~0x003e0000) | (atmp << 21);
			pseudo[1].highlow = PAIR;
			no_opcodes = 2;
			break;
			
		case E_U32:	/* 2nd version emulates Intel as, not doc. */
			if (the_insn.exp.X_add_symbol == NULL &&
			    the_insn.exp.X_subtract_symbol == NULL &&
			    (the_insn.exp.X_add_number < (1 << 16) &&
			     the_insn.exp.X_add_number >= 0))
			    break;
			/* $(opcode)h h%const,ireg_src2,ireg_dest
			   pseudo[0].opcode = (the_insn.opcode & 0xf3ffffff) | 0x0c000000; */
			/* $(opcode)h h%const,ireg_src2,r31 */
			pseudo[0].opcode = (the_insn.opcode & 0xf3e0ffff) | 0x0c000000 |
			    (atmp << 16);
			pseudo[0].highlow = HIGH;
			/* $(opcode) l%const,ireg_dest,ireg_dest
			   pseudo[1].opcode = (the_insn.opcode & 0xf01f0000) | 0x04000000 |
			   ((the_insn.opcode & 0x001f0000) << 5); */
			/* $(opcode) l%const,r31,ireg_dest */
			pseudo[1].opcode = (the_insn.opcode & 0xf01f0000) | 0x04000000 |
			    (atmp << 21);
			pseudo[1].highlow = PAIR;
			no_opcodes = 2;
			break;
			
		case E_AND:	/* 2nd version emulates Intel as, not doc. */
			if (the_insn.exp.X_add_symbol == NULL &&
			    the_insn.exp.X_subtract_symbol == NULL &&
			    (the_insn.exp.X_add_number < (1 << 16) &&
			     the_insn.exp.X_add_number >= 0))
			    break;
			/* andnot h%const,ireg_src2,ireg_dest
			   pseudo[0].opcode = (the_insn.opcode & 0x03ffffff) | 0xd4000000; */
			/* andnot h%const,ireg_src2,r31 */
			pseudo[0].opcode = (the_insn.opcode & 0x03e0ffff) | 0xd4000000 |
			    (atmp << 16);
			pseudo[0].highlow = HIGH;
			pseudo[0].exp.X_add_number = -1 - the_insn.exp.X_add_number;
			/* andnot l%const,ireg_dest,ireg_dest
			   pseudo[1].opcode = (the_insn.opcode & 0x001f0000) | 0xd4000000 |
			   ((the_insn.opcode & 0x001f0000) << 5); */
			/* andnot l%const,r31,ireg_dest */
			pseudo[1].opcode = (the_insn.opcode & 0x001f0000) | 0xd4000000 |
			    (atmp << 21);
			pseudo[1].highlow = PAIR;
			pseudo[1].exp.X_add_number = -1 - the_insn.exp.X_add_number;
			no_opcodes = 2;
			break;
			
		case E_S32:
			if (the_insn.exp.X_add_symbol == NULL &&
			    the_insn.exp.X_subtract_symbol == NULL &&
			    (the_insn.exp.X_add_number < (1 << 15) &&
			     the_insn.exp.X_add_number >= -(1 << 15)))
			    break;
			/* orh h%const,r0,r31 */
			pseudo[0].opcode = 0xec000000 | (atmp << 16);
			pseudo[0].highlow = HIGH;
			/* or l%const,r31,r31 */
			pseudo[1].opcode = 0xe4000000 | (atmp << 21) | (atmp << 16);
			pseudo[1].highlow = PAIR;
			/* r31,ireg_src2,ireg_dest */
			pseudo[2].opcode = (the_insn.opcode & ~0x0400ffff) | (atmp << 11);
			pseudo[2].reloc = NO_RELOC;
			no_opcodes = 3;
			break;
			
		default:
			as_fatal("failed sanity check.");
		}
		
		the_insn = pseudo[0];
		/* check for expanded opcode after branch or in dual */
		if (no_opcodes > 1 && last_expand == 1)
		    as_warn("Expanded opcode after delayed branch: `%s'", str);
		if (no_opcodes > 1 && dual_mode != DUAL_OFF)
		    as_warn("Expanded opcode in dual mode: `%s'", str);
	}
	
	i = 0;
	do {	/* always produce at least one opcode */
		toP = frag_more(4);
		/* put out the opcode */
		md_number_to_chars(toP, the_insn.opcode, 4);
		
		/* check for expanded opcode after branch or in dual */
		last_expand = the_insn.pcrel;
		
		/* put out the symbol-dependent stuff */
		if (the_insn.reloc != NO_RELOC) {
			fix_new(frag_now, /* which frag */
				(toP - frag_now->fr_literal), /* where */
				4, /* size */
				the_insn.exp.X_add_symbol,
				the_insn.exp.X_subtract_symbol,
				the_insn.exp.X_add_number,
				the_insn.pcrel,
				/* merge bit fields into one argument */
				(int)(((the_insn.highlow & 0x3) << 4) | (the_insn.reloc & 0xf)));
		}
		the_insn = pseudo[++i];
	} while (--no_opcodes > 0);
	
}

static void
    i860_ip(str)
char *str;
{
	char *s;
	const char *args;
	char c;
/*	unsigned long i; FIXME: remove this line. */
	struct i860_opcode *insn;
	char *argsStart;
	unsigned long opcode;
	unsigned int mask;
	int match = 0;
	int comma = 0;
	
	
	for (s = str; islower(*s) || *s == '.' || *s == '3'; ++s)
	    ;
	switch (*s) {
		
	case '\0':
		break;
		
	case ',':
		comma = 1;
		
		/*FALLTHROUGH*/
		
	case ' ':
		*s++ = '\0';
		break;
		
	default:
		as_bad("Unknown opcode: `%s'", str);
		exit(1);
	}
	
	if (strncmp(str, "d.", 2) == 0) {	/* check for d. opcode prefix */
		if (dual_mode == DUAL_ON)
		    dual_mode = DUAL_ONDDOT;
		else
		    dual_mode = DUAL_DDOT;
		str += 2;
	}
	
	if ((insn = (struct i860_opcode *) hash_find(op_hash, str)) == NULL) {
		if (dual_mode == DUAL_DDOT || dual_mode == DUAL_ONDDOT)
		    str -= 2;
		as_bad("Unknown opcode: `%s'", str);
		return;
	}
	if (comma) {
		*--s = ',';
	}
	argsStart = s;
	for (;;) {
		opcode = insn->match;
		memset(&the_insn, '\0', sizeof(the_insn));
		the_insn.reloc = NO_RELOC;
		
		/*
		 * Build the opcode, checking as we go to make
		 * sure that the operands match
		 */
		for (args = insn->args; ; ++args) {
			switch (*args) {
				
			case '\0':  /* end of args */
				if (*s == '\0') {
					match = 1;
				}
				break;
				
			case '+':
			case '(':   /* these must match exactly */
			case ')':
			case ',':
			case ' ':
				if (*s++ == *args)
				    continue;
				break;
				
			case '#':   /* must be at least one digit */
				if (isdigit(*s++)) {
					while (isdigit(*s)) {
						++s;
					}
					continue;
				}
				break;
				
			case '1':   /* next operand must be a register */
			case '2':
			case 'd':
				switch (*s) {
					
				case 'f':   /* frame pointer */
					s++;
					if (*s++ == 'p') {
						mask = 0x3;
						break;
					}
					goto error;
					
				case 's':   /* stack pointer */
					s++;
					if (*s++ == 'p') {
						mask= 0x2;
						break;
					}
					goto error;
					
				case 'r': /* any register */
					s++;
					if (!isdigit(c = *s++)) {
						goto error;
					}
					if (isdigit(*s)) {
						if ((c = 10 * (c - '0') + (*s++ - '0')) >= 32) {
							goto error;
						}
					} else {
						c -= '0';
					}
					mask= c;
					break;
					
				default:	/* not this opcode */
					goto error;
				}
				/*
				 * Got the register, now figure out where
				 * it goes in the opcode.
				 */
				switch (*args) {
					
				case '1':
					opcode |= mask << 11;
					continue;
					
				case '2':
					opcode |= mask << 21;
					continue;
					
				case 'd':
					opcode |= mask << 16;
					continue;
					
				}
				break;
				
			case 'e':    /* next operand is a floating point register */
			case 'f':
			case 'g':
				if (*s++ == 'f' && isdigit(*s)) {
					mask = *s++;
					if (isdigit(*s)) {
						mask = 10 * (mask - '0') + (*s++ - '0');
						if (mask >= 32) {
							break;
						}
					} else {
						mask -= '0';
					}
					switch (*args) {
						
					case 'e':
						opcode |= mask << 11;
						continue;
						
					case 'f':
						opcode |= mask << 21;
						continue;
						
					case 'g':
						opcode |= mask << 16;
						if (dual_mode != DUAL_OFF)
						    opcode |= (1 << 9);	/* dual mode instruction */
						if (dual_mode == DUAL_DDOT)
						    dual_mode = DUAL_OFF;
						if (dual_mode == DUAL_ONDDOT)
						    dual_mode = DUAL_ON;
						if ((opcode & (1 << 10)) && (mask == ((opcode >> 11) & 0x1f)))
						    as_warn("Fsr1 equals fdest with Pipelining");
						continue;
					}
				}
				break;
				
			case 'c': /* next operand must be a control register */
				if (strncmp(s, "fir", 3) == 0) {
					opcode |= 0x0 << 21;
					s += 3;
					continue;
				}
				if (strncmp(s, "psr", 3) == 0) {
					opcode |= 0x1 << 21;
					s += 3;
					continue;
				}
				if (strncmp(s, "dirbase", 7) == 0) {
					opcode |= 0x2 << 21;
					s += 7;
					continue;
				}
				if (strncmp(s, "db", 2) == 0) {
					opcode |= 0x3 << 21;
					s += 2;
					continue;
				}
				if (strncmp(s, "fsr", 3) == 0) {
					opcode |= 0x4 << 21;
					s += 3;
					continue;
				}
				if (strncmp(s, "epsr", 4) == 0) {
					opcode |= 0x5 << 21;
					s += 4;
					continue;
				}
				break;
				
			case '5':   /* 5 bit immediate in src1 */
				memset(&the_insn, '\0', sizeof(the_insn));
				if ( !getExpression(s)) {
					s = expr_end;
					if (the_insn.exp.X_add_number & ~0x1f)
					    as_bad("5-bit immediate too large");
					opcode |= (the_insn.exp.X_add_number & 0x1f) << 11;
					memset(&the_insn, '\0', sizeof(the_insn));
					the_insn.reloc = NO_RELOC;
					continue;
				}
				break;
				
			case 'l':   /* 26 bit immediate, relative branch */
				the_insn.reloc = BRADDR;
				the_insn.pcrel = 1;
				goto immediate;
				
			case 's':   /* 16 bit immediate, split relative branch */
				/* upper 5 bits of offset in dest field */
				the_insn.pcrel = 1;
				the_insn.reloc = SPLIT0;
				goto immediate;
				
			case 'S':   /* 16 bit immediate, split (st), aligned */
				if (opcode & (1 << 28))
				    if (opcode & 0x1)
					the_insn.reloc = SPLIT2;
				    else
					the_insn.reloc = SPLIT1;
				else
				    the_insn.reloc = SPLIT0;
				goto immediate;
				
			case 'I':   /* 16 bit immediate, aligned */
				if (opcode & (1 << 28))
				    if (opcode & 0x1)
					the_insn.reloc = LOW2;
				    else
					the_insn.reloc = LOW1;
				else
				    the_insn.reloc = LOW0;
				goto immediate;
				
			case 'i':   /* 16 bit immediate */
				the_insn.reloc = LOW0;
				
				/*FALLTHROUGH*/
				
			immediate:
				if (*s == ' ')
				    s++;
				if (strncmp(s, "ha%", 3) == 0) {
					the_insn.highlow = HIGHADJ;
					s += 3;
				} else if (strncmp(s, "h%", 2) == 0) {
					the_insn.highlow = HIGH;
					s += 2;
				} else if (strncmp(s, "l%", 2) == 0) {
					the_insn.highlow = PAIR;
					s += 2;
				}
				the_insn.expand = insn->expand; 
				
				/* Note that if the getExpression() fails, we will still have
				   created U entries in the symbol table for the 'symbols'
				   in the input string.  Try not to create U symbols for
				   registers, etc. */
				
				if ( !getExpression(s)) {
					s = expr_end;
					continue;
				}
				break;
				
			default:
				as_fatal("failed sanity check.");
			}
			break;
		}
	error:
		if (match == 0)
		    {
			    /* Args don't match.  */
			    if (&insn[1] - i860_opcodes < NUMOPCODES
				&& !strcmp(insn->name, insn[1].name))
				{
					++insn;
					s = argsStart;
					continue;
				}
			    else
				{
					as_bad("Illegal operands");
					return;
				}
		    }
		break;
	}
	
	the_insn.opcode = opcode;
	return;
}

static int
    getExpression(str)
char *str;
{
	char *save_in;
	segT seg;
	
	save_in = input_line_pointer;
	input_line_pointer = str;
	switch (seg = expression(&the_insn.exp)) {
		
	case SEG_ABSOLUTE:
	case SEG_TEXT:
	case SEG_DATA:
	case SEG_BSS:
	case SEG_UNKNOWN:
	case SEG_DIFFERENCE:
	case SEG_BIG:
	case SEG_ABSENT:
		break;
		
	default:
		the_insn.error = "bad segment";
		expr_end = input_line_pointer;
		input_line_pointer=save_in;
		return 1;
	}
	expr_end = input_line_pointer;
	input_line_pointer = save_in;
	return 0;
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

void md_number_to_imm(buf, val, n, fixP)
char *buf;
long val;
int n;
fixS *fixP;
{
	enum reloc_type reloc = fixP->fx_r_type & 0xf;
	enum highlow_type highlow = (fixP->fx_r_type >> 4) & 0x3;
	
	assert(buf);
	assert(n == 4);	/* always on i860 */
	
	switch (highlow) {
		
	case HIGHADJ:	/* adjusts the high-order 16-bits */
		if (val & (1 << 15))
		    val += (1 << 16);
		
		/*FALLTHROUGH*/
		
	case HIGH:	/* selects the high-order 16-bits */
		val >>= 16;
		break;
		
	case PAIR:	/* selects the low-order 16-bits */
		val = val & 0xffff;
		break;
		
	default:
		break;
	}
	
	switch (reloc) {
		
	case BRADDR:	/* br, call, bc, bc.t, bnc, bnc.t w/26-bit immediate */
		if (fixP->fx_pcrel != 1)
		    as_bad("26-bit branch w/o pc relative set: 0x%08x", val);
		val >>= 2;	/* align pcrel offset, see manual */
		
		if (val >= (1 << 25) || val < -(1 << 25))	/* check for overflow */
		    as_bad("26-bit branch offset overflow: 0x%08x", val);
		buf[0] = (buf[0] & 0xfc) | ((val >> 24) & 0x3);
		buf[1] = val >> 16;
		buf[2] = val >> 8;
		buf[3] = val;
		break;
		
	case SPLIT2:	/* 16 bit immediate, 4-byte aligned */
		if (val & 0x3)
		    as_bad("16-bit immediate 4-byte alignment error: 0x%08x", val);
		val &= ~0x3;	/* 4-byte align value */
		/*FALLTHROUGH*/
	case SPLIT1:	/* 16 bit immediate, 2-byte aligned */
		if (val & 0x1)
		    as_bad("16-bit immediate 2-byte alignment error: 0x%08x", val);
		val &= ~0x1;	/* 2-byte align value */
		/*FALLTHROUGH*/
	case SPLIT0:	/* st,bla,bte,btne w/16-bit immediate */
		if (fixP->fx_pcrel == 1)
		    val >>= 2;	/* align pcrel offset, see manual */
		/* check for bounds */
		if (highlow != PAIR && (val >= (1 << 16) || val < -(1 << 15)))
		    as_bad("16-bit branch offset overflow: 0x%08x", val);
		buf[1] = (buf[1] & ~0x1f) | ((val >> 11) & 0x1f);
		buf[2] = (buf[2] & ~0x7) | ((val >> 8) & 0x7);
		buf[3] |= val;	/* perserve bottom opcode bits */	
		break;
		
	case LOW4:	/* fld,pfld,pst,flush 16-byte aligned */
		if (val & 0xf)
		    as_bad("16-bit immediate 16-byte alignment error: 0x%08x", val);
		val &= ~0xf;	/* 16-byte align value */
		/*FALLTHROUGH*/
	case LOW3:	/* fld,pfld,pst,flush 8-byte aligned */
		if (val & 0x7)
		    as_bad("16-bit immediate 8-byte alignment error: 0x%08x", val);
		val &= ~0x7;	/* 8-byte align value */
		/*FALLTHROUGH*/
	case LOW2:	/* 16 bit immediate, 4-byte aligned */
		if (val & 0x3)
		    as_bad("16-bit immediate 4-byte alignment error: 0x%08x", val);
		val &= ~0x3;	/* 4-byte align value */
		/*FALLTHROUGH*/
	case LOW1:	/* 16 bit immediate, 2-byte aligned */
		if (val & 0x1)
		    as_bad("16-bit immediate 2-byte alignment error: 0x%08x", val);
		val &= ~0x1;	/* 2-byte align value */
		/*FALLTHROUGH*/
	case LOW0:	/* 16 bit immediate, byte aligned */
		/* check for bounds */
		if (highlow != PAIR && (val >= (1 << 16) || val < -(1 << 15)))
		    as_bad("16-bit immediate overflow: 0x%08x", val);
		buf[2] = val >> 8;
		buf[3] |= val;	/* perserve bottom opcode bits */	
		break;
		
	case RELOC_32:
		md_number_to_chars(buf, val, 4);
		break;

	case NO_RELOC:
	default:
		as_bad("bad relocation type: 0x%02x", reloc);
		break;
	}
	return;
}

/* should never be called for i860 */
void
    md_create_short_jump(ptr, from_addr, to_addr, frag, to_symbol)
char *ptr;
long from_addr, to_addr;
fragS *frag;
symbolS *to_symbol;
{
	as_fatal("i860_create_short_jmp\n");
}

/* should never be called for i860 */
void
    md_number_to_disp(buf, val, n)
char *buf;
long val;
int n;
{
	as_fatal("md_number_to_disp\n");
}

/* should never be called for i860 */
void
    md_number_to_field(buf,val,fix)
char *buf;
long val;
void *fix;
{
	as_fatal("i860_number_to_field\n");
}

/* the bit-field entries in the relocation_info struct plays hell 
   with the byte-order problems of cross-assembly.  So as a hack,
   I added this mach. dependent ri twiddler.  Ugly, but it gets
   you there. -KWK */
/* on i860: first 4 bytes are normal unsigned long address, next three
   bytes are index, most sig. byte first.  Byte 7 is broken up with
   bit 7 as pcrel, bit 6 as extern, and the lower six bits as
   relocation type (highlow 5-4).  Next 4 bytes are long addend. */
/* Thanx and a tip of the hat to Michael Bloom, mb@ttidca.tti.com */
void
    md_ri_to_chars(ri_p, ri)
struct relocation_info *ri_p, ri;
{
#if 0
	unsigned char the_bytes[sizeof(*ri_p)];
	
	/* this is easy */
	md_number_to_chars(the_bytes, ri.r_address, sizeof(ri.r_address));
	/* now the fun stuff */
	the_bytes[4] = (ri.r_index >> 16) & 0x0ff;
	the_bytes[5] = (ri.r_index >> 8) & 0x0ff;
	the_bytes[6] = ri.r_index & 0x0ff;
	the_bytes[7] = ((ri.r_extern << 7)  & 0x80) | (0 & 0x60) | (ri.r_type & 0x1F);
	/* Also easy */
	md_number_to_chars(&the_bytes[8], ri.r_addend, sizeof(ri.r_addend));
	/* now put it back where you found it, Junior... */
	memcpy((char *) ri_p, the_bytes, sizeof(*ri_p));
#endif
}

/* should never be called for i860 */
void
    md_convert_frag(headers, fragP)
object_headers *headers;
register fragS *fragP;
{
	as_fatal("i860_convert_frag\n");
}

/* should never be called for i860 */
void
    md_create_long_jump(ptr, from_addr, to_addr, frag, to_symbol)
char	*ptr;
long	from_addr,
    to_addr;
fragS	*frag;
symbolS	*to_symbol;
{
	as_fatal("i860_create_long_jump\n");
}

/* should never be called for i860 */
int
    md_estimate_size_before_relax(fragP, segtype)
register fragS *fragP;
segT segtype;
{
	as_fatal("i860_estimate_size_before_relax\n");
	return(0);
}

#ifdef comment
/* for debugging only, must match enum reloc_type */
static char *Reloc[] = {
	"NO_RELOC",
	"BRADDR", 
	"LOW0", 
	"LOW1", 
	"LOW2", 
	"LOW3", 
	"LOW4", 
	"SPLIT0", 
	"SPLIT1", 
	"SPLIT2", 
	"RELOC_32", 
};
static char *Highlow[] = {
	"NO_SPEC",
	"PAIR", 
	"HIGH", 
	"HIGHADJ", 
};

static void
    print_insn(insn)
struct i860_it *insn;
{
	if (insn->error) {
		fprintf(stderr, "ERROR: %s\n", insn->error);
	}
	fprintf(stderr, "opcode=0x%08x\t", insn->opcode);
	fprintf(stderr, "expand=0x%08x\t", insn->expand);
	fprintf(stderr, "reloc = %s\t", Reloc[insn->reloc]);
	fprintf(stderr, "highlow = %s\n", Highlow[insn->highlow]);
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
#endif /* comment */

int
    md_parse_option(argP,cntP,vecP)
char **argP;
int *cntP;
char ***vecP;
{
	return 1;
}

#ifdef comment
/*
 * I860 relocations are completely different, so it needs
 * this machine dependent routine to emit them.
 */
void
    emit_machine_reloc(fixP, segment_address_in_file)
register fixS *fixP;
relax_addressT segment_address_in_file;
{
	struct reloc_info_i860 ri;
	register symbolS *symbolP;
	extern char *next_object_file_charP;
	long add_number;
	
	memset((char *) &ri, '\0', sizeof(ri));
	for (; fixP; fixP = fixP->fx_next) {
		
		if (fixP->fx_r_type & ~0x3f) {
			as_fatal("fixP->fx_r_type = %d\n", fixP->fx_r_type);
		}
		ri.r_pcrel = fixP->fx_pcrel;
		ri.r_type = fixP->fx_r_type;
		
		if ((symbolP = fixP->fx_addsy) != NULL) {
			ri.r_address = fixP->fx_frag->fr_address +
			    fixP->fx_where - segment_address_in_file;
			if (!S_IS_DEFINED(symbolP)) {
				ri.r_extern = 1;
				ri.r_symbolnum = symbolP->sy_number;
			} else {
				ri.r_extern = 0;
				ri.r_symbolnum = S_GET_TYPE(symbolP);
			}
			if (symbolP && symbolP->sy_frag) {
				ri.r_addend = symbolP->sy_frag->fr_address;
			}
			ri.r_type = fixP->fx_r_type;
			if (fixP->fx_pcrel) {
				/* preserve actual offset vs. pc + 4 */
				ri.r_addend -= (ri.r_address + 4);
			} else {
				ri.r_addend = fixP->fx_addnumber;
			}
			
			md_ri_to_chars((char *) &ri, ri);
			append(&next_object_file_charP, (char *)& ri, sizeof(ri));
		}
	}
	return;
}
#endif /* comment */

#ifdef OBJ_AOUT

/* on i860: first 4 bytes are normal unsigned long address, next three
   bytes are index, most sig. byte first.  Byte 7 is broken up with
   bit 7 as pcrel, bit 6 as extern, and the lower six bits as
   relocation type (highlow 5-4).  Next 4 bytes are long addend.

   ie,

   struct reloc_info_i860 {
       unsigned long r_address;
       unsigned int r_symbolnum : 24;
       unsigned int r_pcrel : 1;
       unsigned int r_extern : 1;
       unsigned int r_type : 6;
       long r_addend;
   }

 */

int md_reloc_size = 12;

void tc_aout_fix_to_chars(where, fixP, segment_address_in_file)
char *where;
fixS *fixP;
relax_addressT segment_address_in_file;
{
	long r_index;
	long r_extern;
	long r_addend = 0;
	long r_address;
	
	know(fixP->fx_addsy);
	know(!(fixP->fx_r_type & ~0x3f));

	if (!S_IS_DEFINED(fixP->fx_addsy)) {
		r_extern = 1;
		r_index = fixP->fx_addsy->sy_number;
	} else {
		r_extern = 0;
		r_index = S_GET_TYPE(fixP->fx_addsy);
	}
	
	md_number_to_chars(where,
			   r_address = fixP->fx_frag->fr_address + fixP->fx_where - segment_address_in_file,
			   4);
	
	where[4] = (r_index >> 16) & 0x0ff;
	where[5] = (r_index >> 8) & 0x0ff;
	where[6] = r_index & 0x0ff;
	where[7] = (((fixP->fx_pcrel << 7) & 0x80)
		    | ((r_extern << 6)  & 0x40)
		    | (fixP->fx_r_type & 0x3F));
	
	if (fixP->fx_addsy->sy_frag) {
		r_addend = fixP->fx_addsy->sy_frag->fr_address;
	}
	
	if (fixP->fx_pcrel) {
		/* preserve actual offset vs. pc + 4 */
		r_addend -= (r_address + 4);
	} else {
		r_addend = fixP->fx_addnumber;
	}

	md_number_to_chars(&where[8], r_addend, 4);

	return;
} /* tc_aout_fix_to_chars() */

#endif /* OBJ_AOUT */

/* Parse an operand that is machine-specific.  
   We just return without modifying the expression if we have nothing
   to do.  */

/* ARGSUSED */
void
    md_operand (expressionP)
expressionS *expressionP;
{
}

/* We have no need to default values of symbols.  */

/* ARGSUSED */
symbolS *
    md_undefined_symbol (name)
char *name;
{
	return 0;
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
   On the i860, they're relative to the address of the offset, plus
   its size. (??? Is this right?  FIXME-SOON!) */
long
    md_pcrel_from (fixP)
fixS *fixP;
{
	return fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address;
}

void
    md_apply_fix(fixP, val)
fixS *fixP;
long val;
{
	char *place = fixP->fx_where + fixP->fx_frag->fr_literal;

	/* fixme-soon: looks to me like i860 never has bit fixes. Let's see. xoxorich. */
	know(fixP->fx_bit_fixP == NULL);
	if (!fixP->fx_bit_fixP) {
		
		/* fixme-soon: also looks like fx_im_disp is always 0.  Let's see.  xoxorich. */
		know(fixP->fx_im_disp == 0);
		switch (fixP->fx_im_disp) {
		case 0:
			fixP->fx_addnumber = val;
			md_number_to_imm(place, val, fixP->fx_size, fixP);
			break;
		case 1:
			md_number_to_disp(place,
					   fixP->fx_pcrel ? val + fixP->fx_pcrel_adjust : val,
					   fixP->fx_size);
			break;
		case 2: /* fix requested for .long .word etc */
			md_number_to_chars(place, val, fixP->fx_size);
			break;
		default:
			as_fatal("Internal error in md_apply_fix() in file \"%s\"", __FILE__);
		} /* OVE: maybe one ought to put _imm _disp _chars in one md-func */
	} else {
		md_number_to_field(place, val, fixP->fx_bit_fixP);
	}
	
	return;
} /* md_apply_fix() */

/*
 * Local Variables:
 * fill-column: 131
 * comment-column: 0
 * End:
 */

/* end of tc-i860.c */
