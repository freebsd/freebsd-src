/* i386.c -- Assemble code for the Intel 80386
   Copyright (C) 1989, 1991, 1992 Free Software Foundation.
   
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

/*
  Intel 80386 machine specific gas.
  Written by Eliot Dresselhaus (eliot@mgm.mit.edu).
  Bugs & suggestions are completely welcome.  This is free software.
  Please help us make it better.
  */

#ifndef lint
static char rcsid[] = "$Id: tc-i386.c,v 1.2 1993/11/30 20:57:41 jkh Exp $";
#endif

#include "as.h"

#include "obstack.h"
#include "opcode/i386.h"

/* 'md_assemble ()' gathers together information and puts it into a
   i386_insn. */

typedef struct {
	/* TM holds the template for the insn were currently assembling. */
	template tm;
	/* SUFFIX holds the opcode suffix (e.g. 'l' for 'movl') if given. */
	char suffix;
	/* Operands are coded with OPERANDS, TYPES, DISPS, IMMS, and REGS. */
	
	/* OPERANDS gives the number of given operands. */
	unsigned int operands;
	
	/* REG_OPERANDS, DISP_OPERANDS, MEM_OPERANDS, IMM_OPERANDS give the number of
	   given register, displacement, memory operands and immediate operands. */
	unsigned int reg_operands, disp_operands, mem_operands, imm_operands;
	
	/* TYPES [i] is the type (see above #defines) which tells us how to
	   search through DISPS [i] & IMMS [i] & REGS [i] for the required
	   operand. */
	unsigned int types[MAX_OPERANDS];
	
	/* Displacements (if given) for each operand. */
	expressionS *disps[MAX_OPERANDS];

#ifdef PIC
	/* Relocation type for operand */
	enum reloc_type disp_reloc[MAX_OPERANDS];
#endif
	
	/* Immediate operands (if given) for each operand. */
	expressionS *imms[MAX_OPERANDS];
	
	/* Register operands (if given) for each operand. */
	reg_entry *regs[MAX_OPERANDS];
	
	/* BASE_REG, INDEX_REG, and LOG2_SCALE_FACTOR are used to encode
	   the base index byte below.  */
	reg_entry *base_reg;
	reg_entry *index_reg;
	unsigned int log2_scale_factor;
	
	/* SEG gives the seg_entry of this insn.  It is equal to zero unless
	   an explicit segment override is given. */
	const seg_entry *seg;	/* segment for memory operands (if given) */
	
	/* PREFIX holds all the given prefix opcodes (usually null).
	   PREFIXES is the size of PREFIX. */
 /* richfix: really unsigned? */
	unsigned char prefix[MAX_PREFIXES];
	unsigned int prefixes;
	
	/* RM and IB are the modrm byte and the base index byte where the addressing
	   modes of this insn are encoded. */
	
	modrm_byte        rm;
	base_index_byte   bi;

} i386_insn;

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful */
const char comment_chars[] = "#";

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output. */
/* Also note that comments started like this one will always work if
   '/' isn't otherwise defined. */
const char line_comment_chars[] = "#/"; /* removed '#' xoxorich. */

/* Chars that can be used to separate mant from exp in floating point nums */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant */
/* As in 0f12.456 */
/* or    0d1.2345e12 */
const char FLT_CHARS[] = "fFdDxX";

/* tables for lexical analysis */
static char opcode_chars[256];
static char register_chars[256];
static char operand_chars[256];
static char space_chars[256];
static char identifier_chars[256];
static char digit_chars[256];

/* lexical macros */
#define is_opcode_char(x) (opcode_chars[(unsigned char) x])
#define is_operand_char(x) (operand_chars[(unsigned char) x])
#define is_register_char(x) (register_chars[(unsigned char) x])
#define is_space_char(x) (space_chars[(unsigned char) x])
#define is_identifier_char(x) (identifier_chars[(unsigned char) x])
#define is_digit_char(x) (digit_chars[(unsigned char) x])

/* put here all non-digit non-letter charcters that may occur in an operand */
static char operand_special_chars[] = "%$-+(,)*._~/<>|&^!:";

static char *ordinal_names[] = { "first", "second", "third" };	/* for printfs */

/* md_assemble() always leaves the strings it's passed unaltered.  To
   effect this we maintain a stack of saved characters that we've smashed
   with '\0's (indicating end of strings for various sub-fields of the
   assembler instruction). */
static char save_stack[32];
static char *save_stack_p;	/* stack pointer */
#define END_STRING_AND_SAVE(s)      *save_stack_p++ = *s; *s = '\0'
#define RESTORE_END_STRING(s)       *s = *--save_stack_p
    
    /* The instruction we're assembling. */
    static i386_insn i;

/* Per instruction expressionS buffers: 2 displacements & 2 immediate max. */
static expressionS disp_expressions[2], im_expressions[2];

/* pointers to ebp & esp entries in reg_hash hash table */
static reg_entry *ebp, *esp;

static int this_operand;	/* current operand we are working on */

/*
  Interface to relax_segment.
  There are 2 relax states for 386 jump insns: one for conditional & one
  for unconditional jumps.  This is because the these two types of jumps
  add different sizes to frags when we're figuring out what sort of jump
  to choose to reach a given label.  */

/* types */
#define COND_JUMP 1		/* conditional jump */
#define UNCOND_JUMP 2		/* unconditional jump */
/* sizes */
#define BYTE 0
#define WORD 1
#define DWORD 2
#define UNKNOWN_SIZE 3

#define ENCODE_RELAX_STATE(type,size) ((type<<2) | (size))
#define SIZE_FROM_RELAX_STATE(s) \
    ( (((s) & 0x3) == BYTE ? 1 : (((s) & 0x3) == WORD ? 2 : 4)) )

const relax_typeS md_relax_table[] = {
	/*
	  The fields are:
	  1) most positive reach of this state,
	  2) most negative reach of this state,
	  3) how many bytes this mode will add to the size of the current frag
	  4) which index into the table to try if we can't fit into this one.
	  */
	{1, 1, 0, 0},
	{1, 1, 0, 0},
	{1, 1, 0, 0},
	{1, 1, 0, 0},
	
	/* For now we don't use word displacement jumps:  they may be
	   untrustworthy. */
	{127+1, -128+1, 0, ENCODE_RELAX_STATE(COND_JUMP,DWORD) },
	/* word conditionals add 3 bytes to frag:
	   2 opcode prefix; 1 displacement bytes */
	{32767+2, -32768+2, 3, ENCODE_RELAX_STATE(COND_JUMP,DWORD) },
	/* dword conditionals adds 4 bytes to frag:
	   1 opcode prefix; 3 displacement bytes */
	{0, 0, 4, 0},
	{1, 1, 0, 0},
	
	{127+1, -128+1, 0, ENCODE_RELAX_STATE(UNCOND_JUMP,DWORD) },
	/* word jmp adds 2 bytes to frag:
	   1 opcode prefix; 1 displacement bytes */
	{32767+2, -32768+2, 2, ENCODE_RELAX_STATE(UNCOND_JUMP,DWORD) },
	/* dword jmp adds 3 bytes to frag:
	   0 opcode prefix; 3 displacement bytes */
	{0, 0, 3, 0},
	{1, 1, 0, 0},
	
};

#if __STDC__ == 1

static char *output_invalid(int c);
static int fits_in_signed_byte(long num);
static int fits_in_signed_word(long num);
static int fits_in_unsigned_byte(long num);
static int fits_in_unsigned_word(long num);
static int i386_operand(char *operand_string);
static int smallest_imm_type(long num);
static reg_entry *parse_register(char *reg_string);
static unsigned long mode_from_disp_size(unsigned long t);
static unsigned long opcode_suffix_to_type(unsigned long s);
static void s_bss(void);

#else /* not __STDC__ */

static char *output_invalid();
static int fits_in_signed_byte();
static int fits_in_signed_word();
static int fits_in_unsigned_byte();
static int fits_in_unsigned_word();
static int i386_operand();
static int smallest_imm_type();
static reg_entry *parse_register();
static unsigned long mode_from_disp_size();
static unsigned long opcode_suffix_to_type();
static void s_bss();

#endif /* not __STDC__ */


/* Ignore certain directives generated by gcc. This probably should
   not be here. */
void dummy ()
{
	while (*input_line_pointer && *input_line_pointer != '\n')
	    input_line_pointer++;
}

const pseudo_typeS md_pseudo_table[] = {
	{ "bss",	s_bss,		0 },

#ifndef OLD_GAS
	{ "align",	s_align_bytes,	0 },
#else /* OLD_GAS */
	{ "align",	s_align_ptwo,	0 },
#endif /* OLD_GAS */

	{ "ffloat",	float_cons,	'f' },
	{ "dfloat",	float_cons,	'd' },
	{ "tfloat",	float_cons,	'x' },
	{ "value",      cons,           2 },
	{ 0, 0, 0 }
};

/* for interface with expression () */
extern char * input_line_pointer;

/* obstack for constructing various things in md_begin */
struct obstack o;

/* hash table for opcode lookup */
static struct hash_control *op_hash = (struct hash_control *) 0;
/* hash table for register lookup */
static struct hash_control *reg_hash = (struct hash_control *) 0;
/* hash table for prefix lookup */
static struct hash_control *prefix_hash = (struct hash_control *) 0;


void md_begin ()
{
	char * hash_err;
	
	obstack_begin (&o,4096);
	
	/* initialize op_hash hash table */
	op_hash = hash_new();		/* xmalloc handles error */
	
	{
		register const template *optab;
		register templates *core_optab;
		char *prev_name;
		
		optab = i386_optab;		/* setup for loop */
		prev_name = optab->name;
		obstack_grow (&o, optab, sizeof(template));
		core_optab = (templates *) xmalloc (sizeof (templates));
		
		for (optab++; optab < i386_optab_end; optab++) {
			if (! strcmp (optab->name, prev_name)) {
				/* same name as before --> append to current template list */
				obstack_grow (&o, optab, sizeof(template));
			} else {
				/* different name --> ship out current template list;
				   add to hash table; & begin anew */
				/* Note: end must be set before start! since obstack_next_free changes
				   upon opstack_finish */
				core_optab->end = (template *) obstack_next_free(&o);
				core_optab->start = (template *) obstack_finish(&o);
				hash_err = hash_insert (op_hash, prev_name, (char *) core_optab);
				if (hash_err && *hash_err) {
				hash_error:
					as_fatal("Internal Error:  Can't hash %s: %s", prev_name, hash_err);
				}
				prev_name = optab->name;
				core_optab = (templates *) xmalloc (sizeof(templates));
				obstack_grow (&o, optab, sizeof(template));
			}
		}
	}
	
	/* initialize reg_hash hash table */
	reg_hash = hash_new();
	{
		register const reg_entry *regtab;
		
		for (regtab = i386_regtab; regtab < i386_regtab_end; regtab++) {
			hash_err = hash_insert (reg_hash, regtab->reg_name, regtab);
			if (hash_err && *hash_err) goto hash_error;
		}
	}
	
	esp = (reg_entry *) hash_find (reg_hash, "esp");
	ebp = (reg_entry *) hash_find (reg_hash, "ebp");
	
	/* initialize reg_hash hash table */
	prefix_hash = hash_new();
	{
		register const prefix_entry *prefixtab;
		
		for (prefixtab = i386_prefixtab;
		     prefixtab < i386_prefixtab_end; prefixtab++) {
			hash_err = hash_insert (prefix_hash, prefixtab->prefix_name, prefixtab);
			if (hash_err && *hash_err) goto hash_error;
		}
	}
	
	/* fill in lexical tables:  opcode_chars, operand_chars, space_chars */
	{  
		register unsigned int c;
		
		memset(opcode_chars, '\0', sizeof(opcode_chars));
		memset(operand_chars, '\0', sizeof(operand_chars));
		memset(space_chars, '\0', sizeof(space_chars));
		memset(identifier_chars, '\0', sizeof(identifier_chars));
		memset(digit_chars, '\0', sizeof(digit_chars));
		
		for (c = 0; c < 256; c++) {
			if (islower(c) || isdigit(c)) {
				opcode_chars[c] = c;
				register_chars[c] = c;
			} else if (isupper(c)) {
				opcode_chars[c] = tolower(c);
				register_chars[c] = opcode_chars[c];
			} else if (c == PREFIX_SEPERATOR) {
				opcode_chars[c] = c;
			} else if (c == ')' || c == '(') {
				register_chars[c] = c;
			}
			
			if (isupper(c) || islower(c) || isdigit(c))
			    operand_chars[c] = c;
			else if (c && strchr(operand_special_chars, c))
			    operand_chars[c] = c;
			
			if (isdigit(c) || c == '-') digit_chars[c] = c;
			
			if (isalpha(c) || c == '_' || c == '.' || isdigit(c))
			    identifier_chars[c] = c;
			
			if (c == ' ' || c == '\t') space_chars[c] = c;
		}
	}
}

void md_end() {}		/* not much to do here. */


#define DEBUG386
#ifdef DEBUG386

/* debugging routines for md_assemble */
static void pi (), pte (), pt (), pe (), ps ();

static void pi (line, x)
char * line;
i386_insn *x;
{
	register template *p;
	int i;
	
	fprintf (stdout, "%s: template ", line);
	pte (&x->tm);
	fprintf (stdout, "  modrm:  mode %x  reg %x  reg/mem %x",
		 x->rm.mode, x->rm.reg, x->rm.regmem);
	fprintf (stdout, " base %x  index %x  scale %x\n",
		 x->bi.base, x->bi.index, x->bi.scale);
	for (i = 0; i < x->operands; i++) {
		fprintf (stdout, "    #%d:  ", i+1);
		pt (x->types[i]);
		fprintf (stdout, "\n");
		if (x->types[i] & Reg) fprintf (stdout, "%s\n", x->regs[i]->reg_name);
		if (x->types[i] & Imm) pe (x->imms[i]);
		if (x->types[i] & (Disp|Abs)) pe (x->disps[i]);
	}
}

static void pte (t)
template *t;
{
	int i;
	fprintf (stdout, " %d operands ", t->operands);
	fprintf (stdout, "opcode %x ",
		 t->base_opcode);
	if (t->extension_opcode != None)
	    fprintf (stdout, "ext %x ", t->extension_opcode);
	if (t->opcode_modifier&D)
	    fprintf (stdout, "D");
	if (t->opcode_modifier&W)
	    fprintf (stdout, "W");
	fprintf (stdout, "\n");
	for (i = 0; i < t->operands; i++) {
		fprintf (stdout, "    #%d type ", i+1);
		pt (t->operand_types[i]);
		fprintf (stdout, "\n");
	}
}

static void pe (e)
expressionS *e;
{
	fprintf (stdout, "    segment       %s\n", segment_name (e->X_seg));
	fprintf (stdout, "    add_number    %d (%x)\n",
		 e->X_add_number, e->X_add_number);
	if (e->X_add_symbol) {
		fprintf (stdout, "    add_symbol    ");
		ps (e->X_add_symbol);
		fprintf (stdout, "\n");
	}
	if (e->X_subtract_symbol) {
		fprintf (stdout, "    sub_symbol    ");
		ps (e->X_subtract_symbol);
		fprintf (stdout, "\n");
	}
}

static void ps (s)
symbolS *s;
{
	fprintf (stdout, "%s type %s%s",
		 S_GET_NAME(s),
		 S_IS_EXTERNAL(s) ? "EXTERNAL " : "",
		 segment_name(S_GET_SEGMENT(s)));
}

struct type_name {
	unsigned int mask;
	char *tname;
} type_names[] = {
	{ Reg8, "r8" }, { Reg16, "r16" }, { Reg32, "r32" }, { Imm8, "i8" },
	{ Imm8S, "i8s" },
	{ Imm16, "i16" }, { Imm32, "i32" }, { Mem8, "Mem8"}, { Mem16, "Mem16"},
	{ Mem32, "Mem32"}, { BaseIndex, "BaseIndex" },
	{ Abs8, "Abs8" }, { Abs16, "Abs16" }, { Abs32, "Abs32" },
	{ Disp8, "d8" }, { Disp16, "d16" },
	{ Disp32, "d32" }, { SReg2, "SReg2" }, { SReg3, "SReg3" }, { Acc, "Acc" },
	{ InOutPortReg, "InOutPortReg" }, { ShiftCount, "ShiftCount" },
	{ Imm1, "i1" }, { Control, "control reg" }, {Test, "test reg"},
	{ FloatReg, "FReg"}, {FloatAcc, "FAcc"},
	{ JumpAbsolute, "Jump Absolute"},
	{ 0, "" }
};

static void pt (t)
unsigned int t;
{
	register struct type_name *ty;
	
	if (t == Unknown) {
		fprintf (stdout, "Unknown");
	} else {
		for (ty = type_names; ty->mask; ty++)
		    if (t & ty->mask) fprintf (stdout, "%s, ", ty->tname);
	}
	fflush (stdout);
}

#endif /* DEBUG386 */

/*
  This is the guts of the machine-dependent assembler.  LINE points to a
  machine dependent instruction.  This funciton is supposed to emit
  the frags/bytes it assembles to.
  */
void md_assemble (line)
char *line;
{
	/* Holds temlate once we've found it. */
	register template *t;
	
	/* Possible templates for current insn */
	templates *current_templates = (templates *) 0;
	
	/* Initialize globals. */
	memset(&i, '\0', sizeof(i));
	memset(disp_expressions, '\0', sizeof(disp_expressions));
	memset(im_expressions, '\0', sizeof(im_expressions));
	save_stack_p = save_stack;	/* reset stack pointer */
	
	/* Fist parse an opcode & call i386_operand for the operands.
	   We assume that the scrubber has arranged it so that line[0] is the valid 
	   start of a (possibly prefixed) opcode. */
	{
		register char *l = line;		/* Fast place to put LINE. */
		
		/* 1 if operand is pending after ','. */
		unsigned int expecting_operand = 0;
		/* 1 if we found a prefix only acceptable with string insns. */
		unsigned int expecting_string_instruction = 0;
		/* Non-zero if operand parens not balenced. */
		unsigned int paren_not_balenced;
		char * token_start = l;
		
		while (! is_space_char(*l) && *l != END_OF_INSN) {
			if (! is_opcode_char(*l)) {
				as_bad("invalid character %s in opcode", output_invalid(*l));
				return;
			} else if (*l != PREFIX_SEPERATOR) {
				*l = opcode_chars[(unsigned char) *l];	/* fold case of opcodes */
				l++;
			} else {      /* this opcode's got a prefix */
				register unsigned int q;
				register prefix_entry * prefix;
				
				if (l == token_start) {
					as_bad("expecting prefix; got nothing");
					return;
				}
				END_STRING_AND_SAVE (l);
				prefix = (prefix_entry *) hash_find (prefix_hash, token_start);
				if (! prefix) {
					as_bad("no such opcode prefix ('%s')", token_start);
					return;
				}
				RESTORE_END_STRING (l);
				/* check for repeated prefix */
				for (q = 0; q < i.prefixes; q++)
				    if (i.prefix[q] == prefix->prefix_code) {
					    as_bad("same prefix used twice; you don't really want this!");
					    return;
				    }
				if (i.prefixes == MAX_PREFIXES) {
					as_bad("too many opcode prefixes");
					return;
				}
				i.prefix[i.prefixes++] = prefix->prefix_code;
				if (prefix->prefix_code == REPE || prefix->prefix_code == REPNE)
				    expecting_string_instruction = 1;
				/* skip past PREFIX_SEPERATOR and reset token_start */
				token_start = ++l;
			}
		}
		END_STRING_AND_SAVE (l);
		if (token_start == l) {
			as_bad("expecting opcode; got nothing");
			return;
		}
		
		/* Lookup insn in hash; try intel & att naming conventions if appropriate;
		   that is:  we only use the opcode suffix 'b' 'w' or 'l' if we need to. */
		current_templates = (templates *) hash_find (op_hash, token_start);
		if (! current_templates) {
			int last_index = strlen(token_start) - 1;
			char last_char = token_start[last_index];
			switch (last_char) {
			case DWORD_OPCODE_SUFFIX:
			case WORD_OPCODE_SUFFIX:
			case BYTE_OPCODE_SUFFIX:
				token_start[last_index] = '\0';
				current_templates = (templates *) hash_find (op_hash, token_start);
				token_start[last_index] = last_char;
				i.suffix = last_char;
			}
			if (!current_templates) {
				as_bad("no such 386 instruction: `%s'", token_start); return;
			}
		}
		RESTORE_END_STRING (l);
		
		/* check for rep/repne without a string instruction */
		if (expecting_string_instruction &&
		    ! IS_STRING_INSTRUCTION (current_templates->
					     start->base_opcode)) {
			as_bad("expecting string instruction after rep/repne");
			return;
		}
		
		/* There may be operands to parse. */
		if (*l != END_OF_INSN &&
		    /* For string instructions, we ignore any operands if given.  This
		       kludges, for example, 'rep/movsb %ds:(%esi), %es:(%edi)' where
		       the operands are always going to be the same, and are not really
		       encoded in machine code. */
		    ! IS_STRING_INSTRUCTION (current_templates->
					     start->base_opcode)) {
			/* parse operands */
			do {
				/* skip optional white space before operand */
				while (! is_operand_char(*l) && *l != END_OF_INSN) {
					if (! is_space_char(*l)) {
						as_bad("invalid character %s before %s operand",
						       output_invalid(*l),
						       ordinal_names[i.operands]);
						return;
					}
					l++;
				}
				token_start = l;		/* after white space */
				paren_not_balenced = 0;
				while (paren_not_balenced || *l != ',') {
					if (*l == END_OF_INSN) {
						if (paren_not_balenced) {
							as_bad("unbalenced parenthesis in %s operand.",
							       ordinal_names[i.operands]);
							return;
						} else break;		/* we are done */
					} else if (! is_operand_char(*l)) {
						as_bad("invalid character %s in %s operand",
						       output_invalid(*l),
						       ordinal_names[i.operands]);
						return;
					}
					if (*l == '(') ++paren_not_balenced;
					if (*l == ')') --paren_not_balenced;
					l++;
				}
				if (l != token_start) {	/* yes, we've read in another operand */
					unsigned int operand_ok;
					this_operand = i.operands++;
					if (i.operands > MAX_OPERANDS) {
						as_bad("spurious operands; (%d operands/instruction max)",
						       MAX_OPERANDS);
						return;
					}
					/* now parse operand adding info to 'i' as we go along */
					END_STRING_AND_SAVE (l);
					operand_ok = i386_operand (token_start);
					RESTORE_END_STRING (l);	/* restore old contents */
					if (!operand_ok) return;
				} else {
					if (expecting_operand) {
					expecting_operand_after_comma:
						as_bad("expecting operand after ','; got nothing");
						return;
					}
					if (*l == ',') {
						as_bad("expecting operand before ','; got nothing");
						return;
					}
				}
				
				/* now *l must be either ',' or END_OF_INSN */
				if (*l == ',') {
					if (*++l == END_OF_INSN) { /* just skip it, if it's \n complain */
						goto expecting_operand_after_comma;
					}
					expecting_operand = 1;
				}
			} while (*l != END_OF_INSN); /* until we get end of insn */
		}
	}
	
	/* Now we've parsed the opcode into a set of templates, and have the
	   operands at hand.
	   Next, we find a template that matches the given insn,
	   making sure the overlap of the given operands types is consistent
	   with the template operand types. */
	
#define MATCH(overlap,given_type) \
	(overlap && \
	 (overlap & (JumpAbsolute|BaseIndex|Mem8)) \
	 == (given_type & (JumpAbsolute|BaseIndex|Mem8)))
	    
	    /* If m0 and m1 are register matches they must be consistent
	       with the expected operand types t0 and t1.
	       That is, if both m0 & m1 are register matches
	       i.e. ( ((m0 & (Reg)) && (m1 & (Reg)) ) ?
	       then, either 1. or 2. must be true:
	       1. the expected operand type register overlap is null:
	       (t0 & t1 & Reg) == 0
	       AND
	       the given register overlap is null:
	       (m0 & m1 & Reg) == 0
	       2. the expected operand type register overlap == the given
	       operand type overlap:  (t0 & t1 & m0 & m1 & Reg).
	       */
#define CONSISTENT_REGISTER_MATCH(m0, m1, t0, t1) \
	    ( ((m0 & (Reg)) && (m1 & (Reg))) ? \
	     ( ((t0 & t1 & (Reg)) == 0 && (m0 & m1 & (Reg)) == 0) || \
	      ((t0 & t1) & (m0 & m1) & (Reg)) \
	      ) : 1)
	      {
			register unsigned int overlap0, overlap1;
			expressionS * exp;
			unsigned int overlap2;
			unsigned int found_reverse_match;
			
			overlap0 = overlap1 = overlap2 = found_reverse_match = 0;
			for (t = current_templates->start;
			     t < current_templates->end;
			     t++) {
				
				/* must have right number of operands */
				if (i.operands != t->operands) continue;
				else if (!t->operands) break;	/* 0 operands always matches */
				
				overlap0 = i.types[0] & t->operand_types[0];
				switch (t->operands) {
				case 1:
					if (! MATCH (overlap0,i.types[0])) continue;
					break;
				case 2: case 3:
					overlap1 = i.types[1] & t->operand_types[1];
					if (! MATCH (overlap0,i.types[0]) ||
					    ! MATCH (overlap1,i.types[1]) ||
					    ! CONSISTENT_REGISTER_MATCH(overlap0, overlap1,
									t->operand_types[0],
									t->operand_types[1])) {
						
						/* check if other direction is valid ... */
						if (! (t->opcode_modifier & COMES_IN_BOTH_DIRECTIONS))
						    continue;
						
						/* try reversing direction of operands */
						overlap0 = i.types[0] & t->operand_types[1];
						overlap1 = i.types[1] & t->operand_types[0];
						if (! MATCH (overlap0,i.types[0]) ||
						    ! MATCH (overlap1,i.types[1]) ||
						    ! CONSISTENT_REGISTER_MATCH (overlap0, overlap1, 
										 t->operand_types[0],
										 t->operand_types[1])) {
							/* does not match either direction */
							continue;
						}
						/* found a reverse match here -- slip through */
						/* found_reverse_match holds which of D or FloatD we've found */
						found_reverse_match = t->opcode_modifier & COMES_IN_BOTH_DIRECTIONS;
					}				/* endif: not forward match */
					/* found either forward/reverse 2 operand match here */
					if (t->operands == 3) {
						overlap2 = i.types[2] & t->operand_types[2];
						if (! MATCH (overlap2,i.types[2]) ||
						    ! CONSISTENT_REGISTER_MATCH (overlap0, overlap2,
										 t->operand_types[0],
										 t->operand_types[2]) ||
						    ! CONSISTENT_REGISTER_MATCH (overlap1, overlap2, 
										 t->operand_types[1],
										 t->operand_types[2]))
						    continue;
					}
					/* found either forward/reverse 2 or 3 operand match here:
					   slip through to break */
				}
				break;			/* we've found a match; break out of loop */
			}				/* for (t = ... */
			if (t == current_templates->end) { /* we found no match */
				as_bad("operands given don't match any known 386 instruction");
				return;
			}
			
			/* Copy the template we found (we may change it!). */
			memcpy(&i.tm, t, sizeof(template));
			t = &i.tm;			/* alter new copy of template */
			
			/* If there's no opcode suffix we try to invent one based on register
			   operands. */
			if (! i.suffix && i.reg_operands) {
				/* We take i.suffix from the LAST register operand specified.  This
				   assumes that the last register operands is the destination register
				   operand. */
				int o;
				for (o = 0; o < MAX_OPERANDS; o++)
				    if (i.types[o] & Reg) {
					    i.suffix = (i.types[o] == Reg8) ? BYTE_OPCODE_SUFFIX :
						(i.types[o] == Reg16) ? WORD_OPCODE_SUFFIX :
						    DWORD_OPCODE_SUFFIX;
				    }
			}
			
			/* Make still unresolved immediate matches conform to size of immediate
			   given in i.suffix. Note:  overlap2 cannot be an immediate!
			   We assume this. */
			if ((overlap0 & (Imm8|Imm8S|Imm16|Imm32))
			    && overlap0 != Imm8 && overlap0 != Imm8S
			    && overlap0 != Imm16 && overlap0 != Imm32) {
				if (! i.suffix) {
					as_bad("no opcode suffix given; can't determine immediate size");
					return;
				}
				overlap0 &= (i.suffix == BYTE_OPCODE_SUFFIX ? (Imm8|Imm8S) :
					     (i.suffix == WORD_OPCODE_SUFFIX ? Imm16 : Imm32));
			}
			if ((overlap1 & (Imm8|Imm8S|Imm16|Imm32))
			    && overlap1 != Imm8 && overlap1 != Imm8S
			    && overlap1 != Imm16 && overlap1 != Imm32) {
				if (! i.suffix) {
					as_bad("no opcode suffix given; can't determine immediate size");
					return;
				}
				overlap1 &= (i.suffix == BYTE_OPCODE_SUFFIX ? (Imm8|Imm8S) :
					     (i.suffix == WORD_OPCODE_SUFFIX ? Imm16 : Imm32));
			}
			
			i.types[0] = overlap0;
			i.types[1] = overlap1;
			i.types[2] = overlap2;
			
			if (overlap0 & ImplicitRegister) i.reg_operands--;
			if (overlap1 & ImplicitRegister) i.reg_operands--;
			if (overlap2 & ImplicitRegister) i.reg_operands--;
			if (overlap0 & Imm1) i.imm_operands = 0; /* kludge for shift insns */
			
			if (found_reverse_match) {
				unsigned int save;
				save = t->operand_types[0];
				t->operand_types[0] = t->operand_types[1];
				t->operand_types[1] = save;
			}
			
			/* Finalize opcode.  First, we change the opcode based on the operand
			   size given by i.suffix: we never have to change things for byte insns,
			   or when no opcode suffix is need to size the operands. */
			
			if (! i.suffix && (t->opcode_modifier & W)) {
				as_bad("no opcode suffix given and no register operands; can't size instruction");
				return;
			}
			
			if (i.suffix && i.suffix != BYTE_OPCODE_SUFFIX) {
				/* Select between byte and word/dword operations. */
				if (t->opcode_modifier & W)
				    t->base_opcode |= W;
				/* Now select between word & dword operations via the
				   operand size prefix. */
				if (i.suffix == WORD_OPCODE_SUFFIX) {
					if (i.prefixes == MAX_PREFIXES) {
						as_bad("%d prefixes given and 'w' opcode suffix gives too many prefixes",
						       MAX_PREFIXES);
						return;
					}
					i.prefix[i.prefixes++] = WORD_PREFIX_OPCODE;
				}
			}
			
			/* For insns with operands there are more diddles to do to the opcode. */
			if (i.operands) {
				/* If we found a reverse match we must alter the opcode direction bit
				   found_reverse_match holds bit to set (different for int &
				   float insns). */
				
				if (found_reverse_match) {
					t->base_opcode |= found_reverse_match;
				}
				
				/*
				  The imul $imm, %reg instruction is converted into
				  imul $imm, %reg, %reg. */
				if (t->opcode_modifier & imulKludge) {
					i.regs[2] = i.regs[1]; /* Pretend we saw the 3 operand case. */
					i.reg_operands = 2;
				}
				
				/* Certain instructions expect the destination to be in the i.rm.reg
				   field.  This is by far the exceptional case.  For these instructions,
				   if the source operand is a register, we must reverse the i.rm.reg
				   and i.rm.regmem fields.  We accomplish this by faking that the
				   two register operands were given in the reverse order. */
				if ((t->opcode_modifier & ReverseRegRegmem) && i.reg_operands == 2) {
					unsigned int first_reg_operand = (i.types[0] & Reg) ? 0 : 1;
					unsigned int second_reg_operand = first_reg_operand + 1;
					reg_entry *tmp = i.regs[first_reg_operand];
					i.regs[first_reg_operand] = i.regs[second_reg_operand];
					i.regs[second_reg_operand] = tmp;
				}
				
				if (t->opcode_modifier & ShortForm) {
					/* The register or float register operand is in operand 0 or 1. */
					unsigned int o = (i.types[0] & (Reg|FloatReg)) ? 0 : 1;
					/* Register goes in low 3 bits of opcode. */
					t->base_opcode |= i.regs[o]->reg_num;
				} else if (t->opcode_modifier & ShortFormW) {
					/* Short form with 0x8 width bit.  Register is always dest. operand */
					t->base_opcode |= i.regs[1]->reg_num;
					if (i.suffix == WORD_OPCODE_SUFFIX ||
					    i.suffix == DWORD_OPCODE_SUFFIX)
					    t->base_opcode |= 0x8;
				} else if (t->opcode_modifier & Seg2ShortForm) {
					if (t->base_opcode == POP_SEG_SHORT && i.regs[0]->reg_num == 1) {
						as_bad("you can't 'pop cs' on the 386.");
						return;
					}
					t->base_opcode |= (i.regs[0]->reg_num << 3);
				} else if (t->opcode_modifier & Seg3ShortForm) {
					/* 'push %fs' is 0x0fa0; 'pop %fs' is 0x0fa1.
					   'push %gs' is 0x0fa8; 'pop %fs' is 0x0fa9.
					   So, only if i.regs[0]->reg_num == 5 (%gs) do we need
					   to change the opcode. */
					if (i.regs[0]->reg_num == 5)
					    t->base_opcode |= 0x08;
				} else if (t->opcode_modifier & Modrm) {
					/* The opcode is completed (modulo t->extension_opcode which must
					   be put into the modrm byte.
					   Now, we make the modrm & index base bytes based on all the info
					   we've collected. */
					
					/* i.reg_operands MUST be the number of real register operands;
					   implicit registers do not count. */
					if (i.reg_operands == 2) {
						unsigned int source, dest;
						source = (i.types[0] & (Reg|SReg2|SReg3|Control|Debug|Test)) ? 0 : 1;
						dest = source + 1;
						i.rm.mode = 3;
						/* We must be careful to make sure that all segment/control/test/
						   debug registers go into the i.rm.reg field (despite the whether
						   they are source or destination operands). */
						if (i.regs[dest]->reg_type & (SReg2|SReg3|Control|Debug|Test)) {
							i.rm.reg = i.regs[dest]->reg_num;
							i.rm.regmem = i.regs[source]->reg_num;
						} else {
							i.rm.reg = i.regs[source]->reg_num;
							i.rm.regmem = i.regs[dest]->reg_num;
						}
					} else {		/* if it's not 2 reg operands... */
						if (i.mem_operands) {
							unsigned int fake_zero_displacement = 0;
							unsigned int o = (i.types[0] & Mem) ? 0 : ((i.types[1] & Mem) ? 1 : 2);
							
							/* Encode memory operand into modrm byte and base index byte. */
							
							if (i.base_reg == esp && ! i.index_reg) {
								/* <disp>(%esp) becomes two byte modrm with no index register. */
								i.rm.regmem = ESCAPE_TO_TWO_BYTE_ADDRESSING;
								i.rm.mode = mode_from_disp_size(i.types[o]);
								i.bi.base = ESP_REG_NUM;
								i.bi.index = NO_INDEX_REGISTER;
								i.bi.scale = 0;		/* Must be zero! */
							} else if (i.base_reg == ebp && !i.index_reg) {
								if (! (i.types[o] & Disp)) {
									/* Must fake a zero byte displacement.
									   There is no direct way to code '(%ebp)' directly. */
									fake_zero_displacement = 1;
									/* fake_zero_displacement code does not set this. */
									i.types[o] |= Disp8;
								}
								i.rm.mode = mode_from_disp_size(i.types[o]);
								i.rm.regmem = EBP_REG_NUM;
							} else if (! i.base_reg && (i.types[o] & BaseIndex)) {
								/* There are three cases here.
								   Case 1:  '<32bit disp>(,1)' -- indirect absolute.
								   (Same as cases 2 & 3 with NO index register)
								   Case 2:  <32bit disp> (,<index>) -- no base register with disp
								   Case 3:  (, <index>)       --- no base register;
								   no disp (must add 32bit 0 disp). */
								i.rm.regmem = ESCAPE_TO_TWO_BYTE_ADDRESSING;
								i.rm.mode = 0;		/* 32bit mode */
								i.bi.base = NO_BASE_REGISTER;
								i.types[o] &= ~Disp;
								i.types[o] |= Disp32;	/* Must be 32bit! */
								if (i.index_reg) {		/* case 2 or case 3 */
									i.bi.index = i.index_reg->reg_num;
									i.bi.scale = i.log2_scale_factor;
									if (i.disp_operands == 0)
									    fake_zero_displacement = 1; /* case 3 */
								} else {
									i.bi.index = NO_INDEX_REGISTER;
									i.bi.scale = 0;
								}
							} else if (i.disp_operands && !i.base_reg && !i.index_reg) {
								/* Operand is just <32bit disp> */
								i.rm.regmem = EBP_REG_NUM;
								i.rm.mode = 0;
								i.types[o] &= ~Disp;
								i.types[o] |= Disp32;
							} else {
								/* It's not a special case; rev'em up. */
								i.rm.regmem = i.base_reg->reg_num;
								i.rm.mode = mode_from_disp_size(i.types[o]);
								if (i.index_reg) {
									i.rm.regmem = ESCAPE_TO_TWO_BYTE_ADDRESSING;
									i.bi.base = i.base_reg->reg_num;
									i.bi.index = i.index_reg->reg_num;
									i.bi.scale = i.log2_scale_factor;
									if (i.base_reg == ebp && i.disp_operands == 0) { /* pace */
										fake_zero_displacement = 1;
										i.types[o] |= Disp8;
										i.rm.mode = mode_from_disp_size(i.types[o]);
									}
								}
							}
							if (fake_zero_displacement) {
								/* Fakes a zero displacement assuming that i.types[o] holds
								   the correct displacement size. */
								exp = &disp_expressions[i.disp_operands++];
								i.disps[o] = exp;
								exp->X_seg = SEG_ABSOLUTE;
								exp->X_add_number = 0;
								exp->X_add_symbol = (symbolS *) 0;
								exp->X_subtract_symbol = (symbolS *) 0;
							}
							
							/* Select the correct segment for the memory operand. */
							if (i.seg) {
								unsigned int seg_index;
								const seg_entry *default_seg;
								
								if (i.rm.regmem == ESCAPE_TO_TWO_BYTE_ADDRESSING) {
									seg_index = (i.rm.mode<<3) | i.bi.base;
									default_seg = two_byte_segment_defaults[seg_index];
								} else {
									seg_index = (i.rm.mode<<3) | i.rm.regmem;
									default_seg = one_byte_segment_defaults[seg_index];
								}
								/* If the specified segment is not the default, use an
								   opcode prefix to select it */
								if (i.seg != default_seg) {
									if (i.prefixes == MAX_PREFIXES) {
										as_bad("%d prefixes given and %s segment override gives too many prefixes",
										       MAX_PREFIXES, i.seg->seg_name);
										return;
									}
									i.prefix[i.prefixes++] = i.seg->seg_prefix;
								}
							}
						}
						
						/* Fill in i.rm.reg or i.rm.regmem field with register operand
						   (if any) based on t->extension_opcode. Again, we must be careful
						   to make sure that segment/control/debug/test registers are coded
						   into the i.rm.reg field. */
						if (i.reg_operands) {
							unsigned int o =
							    (i.types[0] & (Reg|SReg2|SReg3|Control|Debug|Test)) ? 0 :
								(i.types[1] & (Reg|SReg2|SReg3|Control|Debug|Test)) ? 1 : 2;
							/* If there is an extension opcode to put here, the register number
							   must be put into the regmem field. */
							if (t->extension_opcode != None)
							    i.rm.regmem = i.regs[o]->reg_num;
							else i.rm.reg = i.regs[o]->reg_num;
							
							/* Now, if no memory operand has set i.rm.mode = 0, 1, 2
							   we must set it to 3 to indicate this is a register operand
							   int the regmem field */
							if (! i.mem_operands) i.rm.mode = 3;
						}
						
						/* Fill in i.rm.reg field with extension opcode (if any). */
						if (t->extension_opcode != None)
						    i.rm.reg = t->extension_opcode;
					}
				}
			}
		}
	
	/* Handle conversion of 'int $3' --> special int3 insn. */
	if (t->base_opcode == INT_OPCODE && i.imms[0]->X_add_number == 3) {
		t->base_opcode = INT3_OPCODE;
		i.imm_operands = 0;
	}
	
	/* We are ready to output the insn. */
	{
		register char * p;
		
		/* Output jumps. */
		if (t->opcode_modifier & Jump) {
			int n = i.disps[0]->X_add_number;
			
			switch (i.disps[0]->X_seg) {
			case SEG_ABSOLUTE:
				if (fits_in_signed_byte(n)) {
					p = frag_more (2);
					p[0] = t->base_opcode;
					p[1] = n;
#if 0 /* leave out 16 bit jumps - pace */
				} else if (fits_in_signed_word(n)) {
					p = frag_more (4);
					p[0] = WORD_PREFIX_OPCODE;
					p[1] = t->base_opcode;
					md_number_to_chars (&p[2], n, 2);
#endif
				} else {		/* It's an absolute dword displacement. */
					if (t->base_opcode == JUMP_PC_RELATIVE) { /* pace */
						/* unconditional jump */
						p = frag_more (5);
						p[0] = 0xe9;
						md_number_to_chars (&p[1], n, 4);
					} else {
						/* conditional jump */
						p = frag_more (6);
						p[0] = TWO_BYTE_OPCODE_ESCAPE;
						p[1] = t->base_opcode + 0x10;
						md_number_to_chars (&p[2], n, 4);
					}
				}
				break;
			default:
				/* It's a symbol; end frag & setup for relax.
				   Make sure there are 6 chars left in the current frag; if not
				   we'll have to start a new one. */
				/* I caught it failing with obstack_room == 6,
				   so I changed to <= pace */
				if (obstack_room (&frags) <= 6) {
					frag_wane(frag_now);
					frag_new (0);
				}
				p = frag_more (1);
				p[0] = t->base_opcode;
				frag_var (rs_machine_dependent,
					  6,		/* 2 opcode/prefix + 4 displacement */
					  1,
					  ((unsigned char) *p == JUMP_PC_RELATIVE
					   ? ENCODE_RELAX_STATE (UNCOND_JUMP, BYTE)
					   : ENCODE_RELAX_STATE (COND_JUMP, BYTE)),
					  i.disps[0]->X_add_symbol,
					  n, p);
/*
 * XXX - what do we do about jmp x@PLT ??
 * kludged in md_estimate_size_before_relax() below
 */
				break;
			}
		} else if (t->opcode_modifier & (JumpByte|JumpDword)) {
			int size = (t->opcode_modifier & JumpByte) ? 1 : 4;
			int n = i.disps[0]->X_add_number;
			
			if (fits_in_unsigned_byte(t->base_opcode)) {
				FRAG_APPEND_1_CHAR (t->base_opcode);
			} else {
				p = frag_more (2);	/* opcode can be at most two bytes */
				/* put out high byte first: can't use md_number_to_chars! */
				*p++ = (t->base_opcode >> 8) & 0xff;
				*p = t->base_opcode & 0xff;
			}
			
			p =  frag_more (size);
			switch (i.disps[0]->X_seg) {
			case SEG_ABSOLUTE:
				md_number_to_chars (p, n, size);
				if (size == 1 && ! fits_in_signed_byte(n)) {
					as_bad("loop/jecx only takes byte displacement; %d shortened to %d",
					       n, *p);
				}
				break;
			default:
				fix_new (frag_now, p - frag_now->fr_literal, size,
					 i.disps[0]->X_add_symbol, i.disps[0]->X_subtract_symbol,
					 i.disps[0]->X_add_number, 1, i.disp_reloc[0], i.disps[0]->X_got_symbol);
				break;
			}
		} else if (t->opcode_modifier & JumpInterSegment) {
			p =  frag_more (1 + 2 + 4);	/* 1 opcode; 2 segment; 4 offset */
			p[0] = t->base_opcode;
			if (i.imms[1]->X_seg == SEG_ABSOLUTE)
			    md_number_to_chars (p + 1, i.imms[1]->X_add_number, 4);
			else
			    fix_new (frag_now, p + 1 -  frag_now->fr_literal, 4,
				     i.imms[1]->X_add_symbol,
				     i.imms[1]->X_subtract_symbol,
				     i.imms[1]->X_add_number, 0, NO_RELOC, i.imms[1]->X_got_symbol);
			if (i.imms[0]->X_seg != SEG_ABSOLUTE)
			    as_bad("can't handle non absolute segment in long call/jmp");
			md_number_to_chars (p + 5, i.imms[0]->X_add_number, 2);
		} else {
			/* Output normal instructions here. */
			unsigned char *q;
#ifdef PIC
			/*
			 * Remember # of opcode bytes to put in pcrel_adjust
			 * for use in _GLOBAL_OFFSET_TABLE_ expressions.
			 */
			int	nopbytes = 0;
#endif
			
			/* First the prefix bytes. */
			for (q = i.prefix; q < i.prefix + i.prefixes; q++) {
				p =  frag_more (1);
				nopbytes += 1;
				md_number_to_chars (p, (unsigned int) *q, 1);
			}
			
			/* Now the opcode; be careful about word order here! */
			if (fits_in_unsigned_byte(t->base_opcode)) {
				nopbytes += 1;
				FRAG_APPEND_1_CHAR (t->base_opcode);
			} else if (fits_in_unsigned_word(t->base_opcode)) {
				p =  frag_more (2);
				nopbytes += 2;
				/* put out high byte first: can't use md_number_to_chars! */
				*p++ = (t->base_opcode >> 8) & 0xff;
				*p = t->base_opcode & 0xff;
			} else {			/* opcode is either 3 or 4 bytes */
				if (t->base_opcode & 0xff000000) {
					p = frag_more (4);
					nopbytes += 4;
					*p++ = (t->base_opcode >> 24) & 0xff;
				} else {
					p = frag_more (3);
					nopbytes += 3;
				}
				*p++ = (t->base_opcode >> 16) & 0xff;
				*p++ = (t->base_opcode >>  8) & 0xff;
				*p =   (t->base_opcode      ) & 0xff;
			}
			
			/* Now the modrm byte and base index byte (if present). */
			if (t->opcode_modifier & Modrm) {
				p =  frag_more (1);
				nopbytes += 1;
				/* md_number_to_chars (p, i.rm, 1); */
				md_number_to_chars (p, (i.rm.regmem<<0 | i.rm.reg<<3 | i.rm.mode<<6), 1);
				/* If i.rm.regmem == ESP (4) && i.rm.mode != Mode 3 (Register mode)
				   ==> need second modrm byte. */
				if (i.rm.regmem == ESCAPE_TO_TWO_BYTE_ADDRESSING && i.rm.mode != 3) {
					p =  frag_more (1);
					nopbytes += 1;
					/* md_number_to_chars (p, i.bi, 1); */
					md_number_to_chars (p,(i.bi.base<<0 | i.bi.index<<3 | i.bi.scale<<6), 1);
				}
			}
			
			if (i.disp_operands) {
				register unsigned int n;
				
				for (n = 0; n < i.operands; n++) {
					if (i.disps[n]) {
						if (i.disps[n]->X_seg == SEG_ABSOLUTE) {
							if (i.types[n] & (Disp8|Abs8)) {
								p =  frag_more (1);
								md_number_to_chars (p, i.disps[n]->X_add_number, 1);
							} else if (i.types[n] & (Disp16|Abs16)) {
								p =  frag_more (2);
								md_number_to_chars (p, i.disps[n]->X_add_number, 2);
							} else {		/* Disp32|Abs32 */
								p =  frag_more (4);
								md_number_to_chars (p, i.disps[n]->X_add_number, 4);
							}
						} else {			/* not SEG_ABSOLUTE */
							/* need a 32-bit fixup (don't support 8bit non-absolute disps) */

							fixS *fixP;
							p =  frag_more (4);
							fixP = fix_new (frag_now, p -  frag_now->fr_literal, 4,
								 i.disps[n]->X_add_symbol, i.disps[n]->X_subtract_symbol,
								 i.disps[n]->X_add_number, 0, i.disp_reloc[n], i.disps[n]->X_got_symbol);
#ifdef PIC
							if (i.disps[n]->X_got_symbol) {
								fixP->fx_pcrel_adjust = nopbytes;
							}
#endif
						}
					}
				}
			}				/* end displacement output */
			
			/* output immediate */
			if (i.imm_operands) {
				register unsigned int n;
				
				for (n = 0; n < i.operands; n++) {
					if (i.imms[n]) {
						if (i.imms[n]->X_seg == SEG_ABSOLUTE) {
							if (i.types[n] & (Imm8|Imm8S)) {
								p =  frag_more (1);
								md_number_to_chars (p, i.imms[n]->X_add_number, 1);
							} else if (i.types[n] & Imm16) {
								p =  frag_more (2);
								md_number_to_chars (p, i.imms[n]->X_add_number, 2);
							} else {
								p =  frag_more (4);
								md_number_to_chars (p, i.imms[n]->X_add_number, 4);
							}
						} else {			/* not SEG_ABSOLUTE */
							/* need a 32-bit fixup (don't support 8bit non-absolute ims) */
							/* try to support other sizes ... */
							fixS *fixP;
							int size;
							if (i.types[n] & (Imm8|Imm8S))
							    size = 1;
							else if (i.types[n] & Imm16)
							    size = 2;
							else
							    size = 4;
							p = frag_more (size);
							fixP = fix_new (frag_now, p - frag_now->fr_literal, size,
								 i.imms[n]->X_add_symbol, i.imms[n]->X_subtract_symbol,
								 i.imms[n]->X_add_number, 0, NO_RELOC, i.imms[n]->X_got_symbol);
#ifdef PIC
							if (i.imms[n]->X_got_symbol) {
								fixP->fx_pcrel_adjust = nopbytes;
							}
#endif
						}
					}
				}
			}				/* end immediate output */
		}
		
#ifdef DEBUG386
		if (flagseen['D']) {
			pi (line, &i);
		}
#endif /* DEBUG386 */
		
	}
	return;
}

/* Parse OPERAND_STRING into the i386_insn structure I.  Returns non-zero
   on error. */

static int i386_operand (operand_string)
char *operand_string;
{
	register char *op_string = operand_string;
	
	/* Address of '\0' at end of operand_string. */
	char * end_of_operand_string = operand_string + strlen(operand_string);
	
	/* Start and end of displacement string expression (if found). */
	char *displacement_string_start = NULL;
	char *displacement_string_end = NULL;
	
	/* We check for an absolute prefix (differentiating,
	   for example, 'jmp pc_relative_label' from 'jmp *absolute_label'. */
	if (*op_string == ABSOLUTE_PREFIX) {
		op_string++;
		i.types[this_operand] |= JumpAbsolute;
	}
	
	/* Check if operand is a register. */
	if (*op_string == REGISTER_PREFIX) {
		register reg_entry *r;
		if (!(r = parse_register (op_string))) {
			as_bad("bad register name ('%s')", op_string);
			return 0;
		}
		/* Check for segment override, rather than segment register by
		   searching for ':' after %<x>s where <x> = s, c, d, e, f, g. */
		if ((r->reg_type & (SReg2|SReg3)) && op_string[3] == ':') {
			switch (r->reg_num) {
			case 0:
				i.seg = (seg_entry *) &es; break;
			case 1:
				i.seg = (seg_entry *) &cs; break;
			case 2:
				i.seg = (seg_entry *) &ss; break;
			case 3:
				i.seg = (seg_entry *) &ds; break;
			case 4:
				i.seg = (seg_entry *) &fs; break;
			case 5:
				i.seg = (seg_entry *) &gs; break;
			}
			op_string += 4;		/* skip % <x> s : */
			operand_string = op_string; /* Pretend given string starts here. */
			if (!is_digit_char(*op_string) && !is_identifier_char(*op_string)
			    && *op_string != '(' && *op_string != ABSOLUTE_PREFIX) {
				as_bad("bad memory operand after segment override");
				return 0;
			}
			/* Handle case of %es:*foo. */
			if (*op_string == ABSOLUTE_PREFIX) {
				op_string++;
				i.types[this_operand] |= JumpAbsolute;
			}
			goto do_memory_reference;
		}
		i.types[this_operand] |= r->reg_type;
		i.regs[this_operand] = r;
		i.reg_operands++;
	} else if (*op_string == IMMEDIATE_PREFIX) { /* ... or an immediate */
		char *save_input_line_pointer;
		segT exp_seg = SEG_GOOF;
		expressionS *exp;

		if (i.imm_operands == MAX_IMMEDIATE_OPERANDS) {
			as_bad("only 1 or 2 immediate operands are allowed");
			return 0;
		}

		exp = &im_expressions[i.imm_operands++];
		i.imms[this_operand] = exp;
		save_input_line_pointer = input_line_pointer;
		/* must advance op_string! */
		input_line_pointer = ++op_string;

		exp_seg = expression(exp);
		input_line_pointer = save_input_line_pointer;

		switch (exp_seg) {
		case SEG_ABSENT:    /* missing or bad expr becomes absolute 0 */
			as_bad("missing or invalid immediate expression '%s' taken as 0",
			       operand_string);
			exp->X_seg = SEG_ABSOLUTE;
			exp->X_add_number = 0;
			exp->X_add_symbol = (symbolS *) 0;
			exp->X_subtract_symbol = (symbolS *) 0;
			i.types[this_operand] |= Imm;
			break;
		case SEG_ABSOLUTE:
			i.types[this_operand] |= smallest_imm_type(exp->X_add_number);
			break;
		case SEG_TEXT: case SEG_DATA: case SEG_BSS: case SEG_UNKNOWN:
		case SEG_DIFFERENCE:
			i.types[this_operand] |= Imm32; /* this is an address ==> 32bit */
			break;
		default:
		seg_unimplemented:
			as_bad("Unimplemented segment type %d in parse_operand", exp_seg);
			return 0;
		}
		/* shorten this type of this operand if the instruction wants
		 * fewer bits than are present in the immediate.  The bit field
		 * code can put out 'andb $0xffffff, %al', for example.   pace
		 * also 'movw $foo,(%eax)'
		 */
		switch (i.suffix) {
		case WORD_OPCODE_SUFFIX:
			i.types[this_operand] |= Imm16;
			break;
		case BYTE_OPCODE_SUFFIX:
			i.types[this_operand] |= Imm16 | Imm8 | Imm8S;
			break;
		}
	} else if (is_digit_char(*op_string) || is_identifier_char(*op_string)
		   || *op_string == '(') {
		/* This is a memory reference of some sort. */
		register char * base_string;
		unsigned int found_base_index_form;
		
	do_memory_reference:
		if (i.mem_operands == MAX_MEMORY_OPERANDS) {
			as_bad("more than 1 memory reference in instruction");
			return 0;
		}
		i.mem_operands++;
		
		/* Determine type of memory operand from opcode_suffix;
		   no opcode suffix implies general memory references. */
		switch (i.suffix) {
		case BYTE_OPCODE_SUFFIX:
			i.types[this_operand] |= Mem8;
			break;
		case WORD_OPCODE_SUFFIX:
			i.types[this_operand] |= Mem16;
			break;
		case DWORD_OPCODE_SUFFIX:
		default:
			i.types[this_operand] |= Mem32;
		}
		
		/*  Check for base index form.  We detect the base index form by
		    looking for an ')' at the end of the operand, searching
		    for the '(' matching it, and finding a REGISTER_PREFIX or ','
		    after it. */
		base_string = end_of_operand_string - 1;
		found_base_index_form = 0;
		if (*base_string == ')') {
			unsigned int parens_balenced = 1;
			/* We've already checked that the number of left & right ()'s are equal,
			   so this loop will not be infinite. */
			do {
				base_string--;
				if (*base_string == ')') parens_balenced++;
				if (*base_string == '(') parens_balenced--;
			} while (parens_balenced);
			base_string++;			/* Skip past '('. */
			if (*base_string == REGISTER_PREFIX || *base_string == ',')
			    found_base_index_form = 1;
		}
		
		/* If we can't parse a base index register expression, we've found
		   a pure displacement expression.  We set up displacement_string_start
		   and displacement_string_end for the code below. */
		if (! found_base_index_form) {
			displacement_string_start = op_string;
			displacement_string_end = end_of_operand_string;
		} else {
			char *base_reg_name, *index_reg_name, *num_string;
			int num;
			
			i.types[this_operand] |= BaseIndex;
			
			/* If there is a displacement set-up for it to be parsed later. */
			if (base_string != op_string + 1) {
				displacement_string_start = op_string;
				displacement_string_end = base_string - 1;
			}
			
			/* Find base register (if any). */
			if (*base_string != ',') {
				base_reg_name = base_string++;
				/* skip past register name & parse it */
				while (isalpha(*base_string)) base_string++;
				if (base_string == base_reg_name+1) {
					as_bad("can't find base register name after '(%c'",
					       REGISTER_PREFIX);
					return 0;
				}
				END_STRING_AND_SAVE (base_string);
				if (! (i.base_reg = parse_register (base_reg_name))) {
					as_bad("bad base register name ('%s')", base_reg_name);
					return 0;
				}
				RESTORE_END_STRING (base_string);
			}
			
			/* Now check seperator; must be ',' ==> index reg
			   OR num ==> no index reg. just scale factor
			   OR ')' ==> end. (scale factor = 1) */
			if (*base_string != ',' && *base_string != ')') {
				as_bad("expecting ',' or ')' after base register in `%s'",
				       operand_string);
				return 0;
			}
			
			/* There may index reg here; and there may be a scale factor. */
			if (*base_string == ',' && *(base_string+1) == REGISTER_PREFIX) {
				index_reg_name = ++base_string;
				while (isalpha(*++base_string));
				END_STRING_AND_SAVE (base_string);
				if (! (i.index_reg = parse_register(index_reg_name))) {
					as_bad("bad index register name ('%s')", index_reg_name);
					return 0;
				}
				RESTORE_END_STRING (base_string);
			}
			
			/* Check for scale factor. */
			if (*base_string == ',' && isdigit(*(base_string+1))) {
				num_string = ++base_string;
				while (is_digit_char(*base_string)) base_string++;
				if (base_string == num_string) {
					as_bad("can't find a scale factor after ','");
					return 0;
				}
				END_STRING_AND_SAVE (base_string);
				/* We've got a scale factor. */
				if (! sscanf (num_string, "%d", &num)) {
					as_bad("can't parse scale factor from '%s'", num_string);
					return 0;
				}
				RESTORE_END_STRING (base_string);
				switch (num) {	/* must be 1 digit scale */
				case 1: i.log2_scale_factor = 0; break;
				case 2: i.log2_scale_factor = 1; break;
				case 4: i.log2_scale_factor = 2; break;
				case 8: i.log2_scale_factor = 3; break;
				default:
					as_bad("expecting scale factor of 1, 2, 4, 8; got %d", num);
					return 0;
				}
			} else {
				if (! i.index_reg && *base_string == ',') {
					as_bad("expecting index register or scale factor after ','; got '%c'",
					       *(base_string+1));
					return 0;
				}
			}
		}
		
		/* If there's an expression begining the operand, parse it,
		   assuming displacement_string_start and displacement_string_end
		   are meaningful. */
		if (displacement_string_start) {
			register expressionS *exp;
			segT exp_seg = SEG_GOOF;
			char *save_input_line_pointer;
			exp = &disp_expressions[i.disp_operands];
			i.disps[this_operand] = exp;
			i.disp_reloc[this_operand] = NO_RELOC;
			i.disp_operands++;
			save_input_line_pointer = input_line_pointer;
			input_line_pointer = displacement_string_start;
			END_STRING_AND_SAVE (displacement_string_end);
#ifdef PIC
			{
			/*
			 * We can have operands of the form
			 *   <symbol>@GOTOFF+<nnn>
			 * Take the easy way out here and copy everything
			 * into a temporary buffer...
			 */
			register char *cp;
			if (flagseen['k'] &&
				(cp = strchr(input_line_pointer,'@'))) {
				char tmpbuf[BUFSIZ];

				if (strncmp(cp+1, "PLT", 3) == 0) {
					i.disp_reloc[this_operand] = RELOC_JMP_TBL;
					*cp = '\0';
					strcpy(tmpbuf, input_line_pointer);
					strcat(tmpbuf, cp+1+3);
					*cp = '@';
				} else if (strncmp(cp+1, "GOTOFF", 6) == 0) {
					i.disp_reloc[this_operand] = RELOC_GOTOFF;
					*cp = '\0';
					strcpy(tmpbuf, input_line_pointer);
					strcat(tmpbuf, cp+1+6);
					*cp = '@';
				} else if (strncmp(cp+1, "GOT", 3) == 0) {
					i.disp_reloc[this_operand] = RELOC_GOT;
					*cp = '\0';
					strcpy(tmpbuf, input_line_pointer);
					strcat(tmpbuf, cp+1+3);
					*cp = '@';
				} else
					as_bad("Bad reloc specifier '%s' in expression", cp+1);
				input_line_pointer = tmpbuf;
			}
			}
#endif
			exp_seg = expression(exp);
#ifdef PIC
			if (i.disp_reloc[this_operand] == RELOC_GOTOFF)
				exp->X_add_symbol->sy_forceout = 1;
#endif
			if (*input_line_pointer)
			    as_bad("Ignoring junk '%s' after expression",input_line_pointer);
			RESTORE_END_STRING (displacement_string_end);
			input_line_pointer = save_input_line_pointer;
			switch (exp_seg) {
			case SEG_ABSENT:
				/* missing expr becomes absolute 0 */
				as_bad("missing or invalid displacement '%s' taken as 0",
				       operand_string);
				i.types[this_operand] |= (Disp|Abs);
				exp->X_seg = SEG_ABSOLUTE;
				exp->X_add_number = 0;
				exp->X_add_symbol = (symbolS *) 0;
				exp->X_subtract_symbol = (symbolS *) 0;
				break;
			case SEG_ABSOLUTE:
				i.types[this_operand] |= SMALLEST_DISP_TYPE (exp->X_add_number);
				break;
			case SEG_TEXT: case SEG_DATA: case SEG_BSS:
			case SEG_UNKNOWN:	/* must be 32 bit displacement (i.e. address) */
				i.types[this_operand] |= Disp32;
				break;
			default:
				goto seg_unimplemented;
			}
		}
		
		/* Make sure the memory operand we've been dealt is valid. */
		if (i.base_reg && i.index_reg &&
		    ! (i.base_reg->reg_type & i.index_reg->reg_type & Reg)) {
			as_bad("register size mismatch in (base,index,scale) expression");
			return 0;
		}
		/*
		 * special case for (%dx) while doing input/output op
		 */
		if ((i.base_reg &&
		     (i.base_reg->reg_type == (Reg16|InOutPortReg)) &&
		     (i.index_reg == 0)))
		  return 1;
		if ((i.base_reg && (i.base_reg->reg_type & Reg32) == 0) ||
		    (i.index_reg && (i.index_reg->reg_type & Reg32) == 0)) {
			as_bad("base/index register must be 32 bit register");
			return 0;
		}
		if (i.index_reg && i.index_reg == esp) {
			as_bad("%s may not be used as an index register", esp->reg_name);
			return 0;
		}
	} else {			/* it's not a memory operand; argh! */
		as_bad("invalid char %s begining %s operand '%s'",
		       output_invalid(*op_string), ordinal_names[this_operand],
		       op_string);
		return 0;
	}
	return 1;			/* normal return */
}

/*
 *			md_estimate_size_before_relax()
 *
 * Called just before relax().
 * Any symbol that is now undefined will not become defined.
 * Return the correct fr_subtype in the frag.
 * Return the initial "guess for fr_var" to caller.
 * The guess for fr_var is ACTUALLY the growth beyond fr_fix.
 * Whatever we do to grow fr_fix or fr_var contributes to our returned value.
 * Although it may not be explicit in the frag, pretend fr_var starts with a
 * 0 value.
 */
int
    md_estimate_size_before_relax (fragP, segment)
register fragS *	fragP;
register segT	segment;
{
	register unsigned char *	opcode;
	register int		old_fr_fix;
	
	old_fr_fix = fragP->fr_fix;
	opcode = (unsigned char *) fragP->fr_opcode;
	/* We've already got fragP->fr_subtype right;  all we have to do is check
	   for un-relaxable symbols. */
	if (S_GET_SEGMENT(fragP->fr_symbol) != segment) {
		/* symbol is undefined in this segment */
		switch (opcode[0]) {
		case JUMP_PC_RELATIVE:	/* make jmp (0xeb) a dword displacement jump */
			opcode[0] = 0xe9;		/* dword disp jmp */
			fragP->fr_fix += 4;
			fix_new (fragP, old_fr_fix, 4,
				 fragP->fr_symbol,
				 (symbolS *) 0,
				 fragP->fr_offset, 1,
#ifdef PIC
/* XXX - oops, the JMP_TBL relocation info should have percolated through
 * here, define a field in frag to this?
 */
	(flagseen['k'] && S_GET_SEGMENT(fragP->fr_symbol) == SEG_UNKNOWN)?
			RELOC_JMP_TBL :
#endif
				 NO_RELOC, (symbolS *)0);
			break;
			
		default:
			/* This changes the byte-displacement jump 0x7N -->
			   the dword-displacement jump 0x0f8N */
			opcode[1] = opcode[0] + 0x10;
			opcode[0] = TWO_BYTE_OPCODE_ESCAPE;		/* two-byte escape */
			fragP->fr_fix += 1 + 4;	/* we've added an opcode byte */
			fix_new (fragP, old_fr_fix + 1, 4,
				 fragP->fr_symbol,
				 (symbolS *) 0,
				 fragP->fr_offset, 1,
#ifdef PIC
/*XXX*/	(flagseen['k'] && S_GET_SEGMENT(fragP->fr_symbol) == SEG_UNKNOWN)?
			RELOC_JMP_TBL :
#endif
				NO_RELOC, (symbolS *)0);
			break;
		}
		frag_wane (fragP);
	}
	return (fragP->fr_var + fragP->fr_fix - old_fr_fix);
}				/* md_estimate_size_before_relax() */

/*
 *			md_convert_frag();
 *
 * Called after relax() is finished.
 * In:	Address of frag.
 *	fr_type == rs_machine_dependent.
 *	fr_subtype is what the address relaxed to.
 *
 * Out:	Any fixSs and constants are set up.
 *	Caller will turn frag into a ".space 0".
 */
void
    md_convert_frag (headers, fragP)
object_headers *headers;
register fragS *	fragP;
{
	register unsigned char *opcode;
	unsigned char *where_to_put_displacement = NULL;
	unsigned int target_address;
	unsigned int opcode_address;
	unsigned int extension = 0;
	int displacement_from_opcode_start;
	
	opcode = (unsigned char *) fragP->fr_opcode;
	
	/* Address we want to reach in file space. */
	target_address = S_GET_VALUE(fragP->fr_symbol) + fragP->fr_offset;
	
	/* Address opcode resides at in file space. */
	opcode_address = fragP->fr_address + fragP->fr_fix;
	
	/* Displacement from opcode start to fill into instruction. */
	displacement_from_opcode_start = target_address - opcode_address;
	
	switch (fragP->fr_subtype) {
	case ENCODE_RELAX_STATE (COND_JUMP, BYTE):
    case ENCODE_RELAX_STATE (UNCOND_JUMP, BYTE):
	/* don't have to change opcode */
	extension = 1;		/* 1 opcode + 1 displacement */
	where_to_put_displacement = &opcode[1];
	break;
	
 case ENCODE_RELAX_STATE (COND_JUMP, WORD):
     opcode[1] = TWO_BYTE_OPCODE_ESCAPE;
	opcode[2] = opcode[0] + 0x10;
	opcode[0] = WORD_PREFIX_OPCODE;
	extension = 4;		/* 3 opcode + 2 displacement */
	where_to_put_displacement = &opcode[3];
	break;
	
 case ENCODE_RELAX_STATE (UNCOND_JUMP, WORD):
     opcode[1] = 0xe9;
	opcode[0] = WORD_PREFIX_OPCODE;
	extension = 3;		/* 2 opcode + 2 displacement */
	where_to_put_displacement = &opcode[2];
	break;
	
 case ENCODE_RELAX_STATE (COND_JUMP, DWORD):
     opcode[1] = opcode[0] + 0x10;
	opcode[0] = TWO_BYTE_OPCODE_ESCAPE;
	extension = 5;		/* 2 opcode + 4 displacement */
	where_to_put_displacement = &opcode[2];
	break;
	
 case ENCODE_RELAX_STATE (UNCOND_JUMP, DWORD):
     opcode[0] = 0xe9;
	extension = 4;		/* 1 opcode + 4 displacement */
	where_to_put_displacement = &opcode[1];
	break;
	
 default:
	BAD_CASE(fragP->fr_subtype);
	break;
}
	/* now put displacement after opcode */
	md_number_to_chars ((char *) where_to_put_displacement,
			    displacement_from_opcode_start - extension,
			    SIZE_FROM_RELAX_STATE (fragP->fr_subtype));
	fragP->fr_fix += extension;
}


int md_short_jump_size = 2;	/* size of byte displacement jmp */
int md_long_jump_size  = 5;	/* size of dword displacement jmp */
int md_reloc_size = 8;		/* Size of relocation record */

void md_create_short_jump(ptr, from_addr, to_addr, frag, to_symbol)
char	*ptr;
long	from_addr, to_addr;
fragS *frag;
symbolS *to_symbol;
{
	long offset;
	
	offset = to_addr - (from_addr + 2);
	md_number_to_chars (ptr, (long) 0xeb, 1); /* opcode for byte-disp jump */
	md_number_to_chars (ptr + 1, offset, 1);
}

void md_create_long_jump (ptr, from_addr, to_addr, frag, to_symbol)
char	*ptr;
long	from_addr, to_addr;
fragS	*frag;
symbolS	*to_symbol;
{
	long offset;
	
	if (flagseen['m']) {
		offset = to_addr - S_GET_VALUE(to_symbol);
		md_number_to_chars (ptr, 0xe9, 1); /* opcode for long jmp */
		md_number_to_chars (ptr + 1, offset, 4);
		fix_new (frag, (ptr+1) - frag->fr_literal, 4,
			 to_symbol, (symbolS *) 0, (long) 0, 0, NO_RELOC, (symbolS *)0);
	} else {
		offset = to_addr - (from_addr + 5);
		md_number_to_chars(ptr, (long) 0xe9, 1);
		md_number_to_chars(ptr + 1, offset, 4);
	}
}

int
    md_parse_option(argP,cntP,vecP)
char **argP;
int *cntP;
char ***vecP;
{
#ifdef PIC
	if (argP && *argP && **argP == 'k') {
#if 00
		char *tmp = xmalloc(3+1+strlen(operand_special_chars));
		strcpy(tmp, operand_special_chars);
		strcat(tmp, "@[]");
		operand_special_chars = tmp;
#endif
		/* Allow `[', `]' in expressions and `@' in operands */
		operand_chars['@'] = '@';
		operand_chars['['] = '[';
		operand_chars[']'] = ']';

		/* Disallow `[' as a name beginner */
		lex_type['['] = 0;

		/* Predefine GOT symbol */
		GOT_symbol = symbol_find_or_make("__GLOBAL_OFFSET_TABLE_");
	}
#endif
	return 1;
}

 /* write out in little endian. */
void /* Knows about order of bytes in address. */
    md_number_to_chars(con, value, nbytes)
char	con[];	/* Return 'nbytes' of chars here. */
long	value;		/* The value of the bits. */
int	nbytes;		/* Number of bytes in the output. */
{
	register char * p = con;
	
	switch (nbytes) {
	case 1:
		p[0] = value & 0xff;
		break;
	case 2:
		p[0] = value & 0xff;
		p[1] = (value >> 8) & 0xff;
		break;
	case 4:
		p[0] = value & 0xff;
		p[1] = (value>>8) & 0xff;
		p[2] = (value>>16) & 0xff;
		p[3] = (value>>24) & 0xff;
		break;
	default:
		BAD_CASE (nbytes);
	}
}


/* Apply a fixup (fixS) to segment data, once it has been determined
   by our caller that we have all the info we need to fix it up. 
   
   On the 386, immediates, displacements, and data pointers are all in
   the same (little-endian) format, so we don't need to care about which
   we are handling.  */

void
    md_apply_fix (fixP, value)
fixS * fixP;		/* The fix we're to put in */
long	value;		/* The value of the bits. */
{
	register char * p = fixP->fx_where + fixP->fx_frag->fr_literal;
	
	switch (fixP->fx_size) {
	case 1:
		*p = value;
		break;
	case 2:
		*p++ = value;
		*p = (value>>8);
		break;
	case 4:
		*p++ = value;
		*p++ = (value>>8);
		*p++ = (value>>16);
		*p = (value>>24);
		break;
	default:
		BAD_CASE (fixP->fx_size);
	}
}

long			/* Knows about the byte order in a word. */
    md_chars_to_number (con, nbytes)
unsigned     char	con[];	/* Low order byte 1st. */
int	nbytes;		/* Number of bytes in the input. */
{
	long	retval;
	for (retval=0, con+=nbytes-1; nbytes--; con--)
	    {
		    retval <<= BITS_PER_CHAR;
		    retval |= *con;
	    }
	return retval;
}

/* Not needed for coff since relocation structure does not 
   contain bitfields. */
#if defined(OBJ_AOUT) | defined(OBJ_BOUT)
#ifdef comment
/* Output relocation information in the target's format.  */
void
    md_ri_to_chars(the_bytes, ri)
char *the_bytes;
struct reloc_info_generic *ri;
{
	/* this is easy */
	md_number_to_chars(the_bytes, ri->r_address, 4);
	/* now the fun stuff */
	the_bytes[6] = (ri->r_symbolnum >> 16) & 0x0ff;
	the_bytes[5] = (ri->r_symbolnum >> 8) & 0x0ff;
	the_bytes[4] = ri->r_symbolnum & 0x0ff;
	the_bytes[7] = (((ri->r_extern << 3)  & 0x08) | ((ri->r_length << 1) & 0x06) | 
			((ri->r_pcrel << 0)  & 0x01)) & 0x0F; 
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
	
#ifdef PIC
	{
	int	extra_bits = 0;
	int	extrn_bit = !S_IS_DEFINED(fixP->fx_addsy);

	switch (fixP->fx_r_type) {
	case NO_RELOC:
		break;
	case RELOC_32:
		if (!flagseen['k'] || !S_IS_EXTERNAL(fixP->fx_addsy))
			break;
		r_symbolnum = fixP->fx_addsy->sy_number;
		extrn_bit = 1;
		break;
	case RELOC_GOT:
		extra_bits = (1 << 4) & 0x10; /* r_baserel */
		r_symbolnum = fixP->fx_addsy->sy_number;
		if (!extrn_bit && !S_IS_EXTERNAL(fixP->fx_addsy))
			as_warn("GOT relocation burb: `%s' should be global",
					S_GET_NAME(fixP->fx_addsy));
		extrn_bit = 1;
		break;
	case RELOC_GOTOFF:
		extra_bits = (1 << 4) & 0x10; /* r_baserel */
		r_symbolnum = fixP->fx_addsy->sy_number;
		if (extrn_bit || S_IS_EXTERNAL(fixP->fx_addsy))
			as_warn("GOT relocation burb: `%s' should be static",
					S_GET_NAME(fixP->fx_addsy));
		break;
	case RELOC_JMP_TBL:
		extra_bits = (1 << 5) & 0x20; /* r_jmptable */
		break;
	case RELOC_RELATIVE:
		/* consider using this bit (together with r_baserel) for
		 * GOTOFFs, so ld can check
		 */
		as_fatal("relocation botch");
		extra_bits = (1 << 6) & 0x40; /* r_relative */
		break;
	}
	where[6] = (r_symbolnum >> 16) & 0x0ff;
	where[5] = (r_symbolnum >> 8) & 0x0ff;
	where[4] = r_symbolnum & 0x0ff;
	where[7] = (      ((extrn_bit << 3)  & 0x08)
			| ((nbytes_r_length[fixP->fx_size] << 1) & 0x06)
			| ((fixP->fx_pcrel << 0) & 0x01)
			| (extra_bits)
		   );
	}
#else
	where[6] = (r_symbolnum >> 16) & 0x0ff;
	where[5] = (r_symbolnum >> 8) & 0x0ff;
	where[4] = r_symbolnum & 0x0ff;
	where[7] = ((((!S_IS_DEFINED(fixP->fx_addsy)) << 3)  & 0x08)
		    | ((nbytes_r_length[fixP->fx_size] << 1) & 0x06)
		    | (((fixP->fx_pcrel << 0) & 0x01) & 0x0f));
#endif
	
	return;
} /* tc_aout_fix_to_chars() */

#endif /* OBJ_AOUT or OBJ_BOUT */


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
	char	*t;
	
	switch (type) {
	case 'f':
	case 'F':
		prec = 2;
		break;
		
	case 'd':
	case 'D':
		prec = 4;
		break;
		
	case 'x':
	case 'X':
		prec = 5;
		break;
		
	default:
		*sizeP=0;
		return "Bad call to md_atof ()";
	}
	t = atof_ieee (input_line_pointer,type,words);
	if (t)
	    input_line_pointer=t;
	
	*sizeP = prec * sizeof(LITTLENUM_TYPE);
	/* this loops outputs the LITTLENUMs in REVERSE order; in accord with
	   the bigendian 386 */
	for (wordP = words + prec - 1;prec--;) {
		md_number_to_chars (litP, (long) (*wordP--), sizeof(LITTLENUM_TYPE));
		litP += sizeof(LITTLENUM_TYPE);
	}
	return "";	/* Someone should teach Dean about null pointers */
}

char output_invalid_buf[8];

static char * output_invalid (c)
char c;
{
	if (isprint(c)) sprintf (output_invalid_buf, "'%c'", c);
	else sprintf (output_invalid_buf, "(0x%x)", (unsigned) c);
	return output_invalid_buf;
}

static reg_entry *parse_register (reg_string)
char *reg_string;          /* reg_string starts *before* REGISTER_PREFIX */
{
	register char *s = reg_string;
	register char *p;
	char reg_name_given[MAX_REG_NAME_SIZE];
	
	s++;				/* skip REGISTER_PREFIX */
	for (p = reg_name_given; is_register_char (*s); p++, s++) {
		*p = register_chars[*s];
		if (p >= reg_name_given + MAX_REG_NAME_SIZE)
		    return (reg_entry *) 0;
	}
	*p = '\0';
	return (reg_entry *) hash_find (reg_hash, reg_name_given);
}


/* We have no need to default values of symbols.  */

/* ARGSUSED */
symbolS *
    md_undefined_symbol (name)
char *name;
{
#ifdef PIC
	/* HACK:
	 * Sun's ld expects __GLOBAL_OFFSET_TABLE_,
	 * gcc generates _GLOBAL_OFFSET_TABLE_
	 * should probably fix ld - new SVR4 style??
	 */
	if (*name == '_' && *(name+1) == 'G' &&
				strcmp(name, "_GLOBAL_OFFSET_TABLE_") == 0)
		return symbol_find("__GLOBAL_OFFSET_TABLE_");
#endif
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
   On the i386, they're relative to the address of the offset, plus
   its size. (??? Is this right?  FIXME-SOON!) */
long
    md_pcrel_from (fixP)
fixS *fixP;
{
#ifdef PIC
	/*
	 * _GLOBAL_OFFSET_TABLE_ refs are relative to the offset of the
	 * current instruction. fx_pcrel_adjust has been setup to account
	 * for the number of opcode bytes preceding the fixup location,
	 * it is zero for eg. .long pseudo-ops.
	 */
	if (fixP->fx_gotsy)
		return fixP->fx_where + fixP->fx_frag->fr_address - fixP->fx_pcrel_adjust;
	else
#endif
	return fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address;
}

 /* these were macros, but I don't trust macros that eval their
    arguments more than once.  Besides, gcc can static inline them.
    xoxorich.  */

static unsigned long mode_from_disp_size(t)
unsigned long t;
{
	return((t & (Disp8))
	       ? 1
	       : ((t & (Disp32)) ? 2 : 0));
} /* mode_from_disp_size() */

/* convert opcode suffix ('b' 'w' 'l' typically) into type specifyer */

static unsigned long opcode_suffix_to_type(s)
unsigned long s;
{
	return(s == BYTE_OPCODE_SUFFIX
	       ? Byte : (s == WORD_OPCODE_SUFFIX
			 ? Word : DWord));
} /* opcode_suffix_to_type() */

static int fits_in_signed_byte(num)
long num;
{
	return((num >= -128) && (num <= 127));
} /* fits_in_signed_byte() */

static int fits_in_unsigned_byte(num)
long num;
{
	return((num & 0xff) == num);
} /* fits_in_unsigned_byte() */

static int fits_in_unsigned_word(num)
long num;
{
	return((num & 0xffff) == num);
} /* fits_in_unsigned_word() */

static int fits_in_signed_word(num)
long num;
{
	return((-32768 <= num) && (num <= 32767));
} /* fits_in_signed_word() */

static int smallest_imm_type(num)
long num;
{
	return((num == 1)
	       ? (Imm1|Imm8|Imm8S|Imm16|Imm32)
	       : (fits_in_signed_byte(num)
		  ? (Imm8S|Imm8|Imm16|Imm32)
		  : (fits_in_unsigned_byte(num)
		     ? (Imm8|Imm16|Imm32)
		     : ((fits_in_signed_word(num) || fits_in_unsigned_word(num))
			? (Imm16|Imm32)
			: (Imm32)))));
} /* smallest_imm_type() */

static void s_bss()
{
  register int temp;

  temp = get_absolute_expression ();
  subseg_new (SEG_BSS, (subsegT)temp);
  demand_empty_rest_of_line();
}
  
/*
 * Local Variables:
 * comment-column: 0
 * End:
 */

/* end of tc-i386.c */
