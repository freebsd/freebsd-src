/* read.c - read a source file -

   Copyright (C) 1986, 1987, 1990, 1991, 1992 Free Software Foundation, Inc.
   
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
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#ifndef lint
static char rcsid[] = "$Id: read.c,v 1.4 1993/12/12 17:01:16 jkh Exp $";
#endif

#define MASK_CHAR (0xFF)	/* If your chars aren't 8 bits, you will
				   change this a bit.  But then, GNU isn't
				   spozed to run on your machine anyway.
				   (RMS is so shortsighted sometimes.)
				   */

#define MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT (16)
/* This is the largest known floating point */
/* format (for now). It will grow when we */
/* do 4361 style flonums. */


/* Routines that read assembler source text to build spagetti in memory. */
/* Another group of these functions is in the as-expr.c module */

#include "as.h"

#include "obstack.h"

char *input_line_pointer;	/*->next char of source file to parse. */

#ifndef NOP_OPCODE
# define NOP_OPCODE 0x00
#endif

#if BITS_PER_CHAR != 8
The following table is indexed by [ (char) ] and will break if
    a char does not have exactly 256 states (hopefully 0:255!) !
#endif
    
#ifdef ALLOW_ATSIGN
#define AT 2
#else
#define AT 0
#endif

#ifndef PIC
    const
#endif
    char /* used by is_... macros. our ctype[] */
    lex_type[256] = {
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,       /* @ABCDEFGHIJKLMNO */
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,       /* PQRSTUVWXYZ[\]^_ */
	    0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0,       /* _!"#$%&'()*+,-./ */
	    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,       /* 0123456789:;<=>? */
	   AT, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,       /* @ABCDEFGHIJKLMNO */
	    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 3,       /* PQRSTUVWXYZ[\]^_ */
	    0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,       /* `abcdefghijklmno */
	    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0,       /* pqrstuvwxyz{|}~. */
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};


/*
 * In: a character.
 * Out: 1 if this character ends a line.
 */
#define _ (0)
char is_end_of_line[256] = {
#ifdef CR_EOL
	_, _, _, _, _, _, _, _, _, _,99, _, _, 99, _, _,/* @abcdefghijklmno */
#else
	_, _, _, _, _, _, _, _, _, _,99, _, _, _, _, _, /* @abcdefghijklmno */
#endif
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, /* */
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, /* */
	_, _, _, _, _, _, _, _, _, _, _,99, _, _, _, _, /* 0123456789:;<=>? */
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, /* */
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, /* */
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, /* */
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, /* */
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, /* */
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, /* */
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, /* */
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, /* */
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _  /* */
    };
#undef _

/* Functions private to this file. */

extern const char line_comment_chars[];
const char line_separator_chars[1];

static char *buffer;		/* 1st char of each buffer of lines is here. */
static char *buffer_limit;	/*->1 + last char in buffer. */

static char *bignum_low;	/* Lowest char of bignum. */
static char *bignum_limit;	/* 1st illegal address of bignum. */
static char *bignum_high;	/* Highest char of bignum. */
/* May point to (bignum_start-1). */
/* Never >= bignum_limit. */
static char *old_buffer = 0;	/* JF a hack */
static char *old_input;
static char *old_limit;

/* Variables for handling include file directory list. */

char **include_dirs;		/* List of pointers to directories to
				   search for .include's */
int include_dir_count;		/* How many are in the list */
int include_dir_maxlen = 1;	/* Length of longest in list */

#ifndef WORKING_DOT_WORD
struct broken_word *broken_words;
int new_broken_words = 0;
#endif

#if __STDC__ == 1

static char *demand_copy_string(int *lenP);
int is_it_end_of_statement(void);
unsigned int next_char_of_string(void);
static segT get_known_segmented_expression(expressionS *expP);
static void grow_bignum(void);
static void pobegin(void);
void stringer(int append_zero);

#else /* __STDC__ */

static char *demand_copy_string();
int is_it_end_of_statement();
unsigned int next_char_of_string();
static segT get_known_segmented_expression();
static void grow_bignum();
static void pobegin();
void stringer();

#endif /* not __STDC__ */

extern int listing;


void
    read_begin()
{
	const char *p;
	
	pobegin();
	obj_read_begin_hook();
	
	obstack_begin(&notes, 5000);
	obstack_begin(&cond_obstack, 960);
	
#define BIGNUM_BEGIN_SIZE (16)
	bignum_low = xmalloc((long)BIGNUM_BEGIN_SIZE);
	bignum_limit = bignum_low + BIGNUM_BEGIN_SIZE;
	
	/* Use machine dependent syntax */
	for (p = line_separator_chars; *p; p++)
	    is_end_of_line[*p] = 1;
	/* Use more.  FIXME-SOMEDAY. */
}

/* set up pseudo-op tables */

struct hash_control *
    po_hash = NULL;			/* use before set up: NULL->address error */

static const pseudo_typeS
    potable[] =
{
	{ "abort",	s_abort,	0	},
	{ "align",	s_align_ptwo,	0	},
	{ "ascii",	stringer,	0	},
	{ "asciz",	stringer,	1	},
	/* block */
	{ "byte",	cons,		1	},
	{ "comm",	s_comm,		0	},
	{ "data",	s_data,		0	},
	/* dim */
	{ "double",	float_cons,	'd'	},
	/* dsect */
#ifdef NO_LISTING
	{ "eject",	s_ignore,	0	},	/* Formfeed listing */
#else
	{ "eject",	listing_eject,	0	},	/* Formfeed listing */
#endif /* NO_LISTING */
	{ "else",	s_else,		0	},
	{ "end",	s_end,		0	},
	{ "endif",	s_endif,	0	},
	/* endef */
	{ "equ",	s_set,		0	},
	/* err */
	/* extend */
	{ "extern",	s_ignore,	0	},	/* We treat all undef as ext */
	{ "app-file",	s_app_file,	0	},
	{ "file",	s_app_file,	0	},
	{ "fill",	s_fill,		0	},
	{ "float",	float_cons,	'f'	},
	{ "global",	s_globl,	0	},
	{ "globl",	s_globl,	0	},
	{ "hword",	cons,		2	},
	{ "if",	s_if,		0	},
	{ "ifdef",	s_ifdef,	0	},
	{ "ifeqs",	s_ifeqs,	0	},
	{ "ifndef",	s_ifdef,	1	},
	{ "ifnes",	s_ifeqs,	1	},
	{ "ifnotdef",	s_ifdef,	1	},
	{ "include",	s_include,	0	},
	{ "int",	cons,		4	},
	{ "lcomm",	s_lcomm,	0	},
#ifdef NO_LISTING
	{ "lflags",	s_ignore,	0	},	/* Listing flags */
	{ "list",	s_ignore,	1	},	/* Turn listing on */
#else
	{ "lflags",	listing_flags,	0	},	/* Listing flags */
	{ "list",	listing_list,	1	},	/* Turn listing on */
#endif /* NO_LISTING */
	{ "long",	cons,		4	},
	{ "lsym",	s_lsym,		0	},
#ifdef NO_LISTING
	{ "nolist",	s_ignore,	0	},	/* Turn listing off */
#else
	{ "nolist",	listing_list,	0	},	/* Turn listing off */
#endif /* NO_LISTING */
	{ "octa",	big_cons,	16	},
	{ "org",	s_org,		0	},
#ifdef NO_LISTING
	{ "psize",	s_ignore,	0       },   /* set paper size */
#else
	{ "psize",	listing_psize,	0       },   /* set paper size */
#endif /* NO_LISTING */
	/* print */
	{ "quad",	big_cons,	8	},
#ifdef NO_LISTING
	{ "sbttl",	s_ignore,	1	},	/* Subtitle of listing */
#else
	{ "sbttl",	listing_title,	1	},	/* Subtitle of listing */
#endif /* NO_LISTING */
	/* scl */
	/* sect */
#ifndef TC_M88K
	{ "set",	s_set,		0	},
#endif /* TC_M88K */
	{ "short",	cons,		2	},
	{ "single",	float_cons,	'f'	},
	/* size */
	{ "space",	s_space,	0	},
	/* tag */
	{ "text",	s_text,		0	},
#ifdef NO_LISTING
	{ "title",	s_ignore,	0	},	/* Listing title */
#else
	{ "title",	listing_title,	0	},	/* Listing title */
#endif /* NO_LISTING */
	/* type */
	/* use */
	/* val */
	{ "word",	cons,		2	},
	{ NULL}	/* end sentinel */
};

static void pobegin() {
	char *errtxt;		/* error text */
	const pseudo_typeS * pop;
	
	po_hash = hash_new();
	
	/* Do the target-specific pseudo ops. */
	for (pop = md_pseudo_table; pop->poc_name; pop++) {
		errtxt = hash_insert(po_hash, pop->poc_name, (char *)pop);
		if (errtxt && *errtxt) {
			as_fatal("error constructing md pseudo-op table");
		} /* on error */
	} /* for each op */
	
	/* Now object specific.  Skip any that were in the target table. */
	for (pop = obj_pseudo_table; pop->poc_name; pop++) {
		errtxt = hash_insert(po_hash, pop->poc_name, (char *) pop);
		if (errtxt && *errtxt) {
			if (!strcmp(errtxt, "exists")) {
#ifdef DIE_ON_OVERRIDES
				as_fatal("pseudo op \".%s\" overridden.\n", pop->poc_name);
#endif /* DIE_ON_OVERRIDES */
				continue;	/* OK if target table overrides. */
			} else {
				as_fatal("error constructing obj pseudo-op table");
			} /* if overridden */
		} /* on error */
	} /* for each op */
	
	/* Now portable ones.  Skip any that we've seen already. */
	for (pop = potable; pop->poc_name; pop++) {
		errtxt = hash_insert(po_hash, pop->poc_name, (char *) pop);
		if (errtxt && *errtxt) {
			if (!strcmp (errtxt, "exists")) {
#ifdef DIE_ON_OVERRIDES
				as_fatal("pseudo op \".%s\" overridden.\n", pop->poc_name);
#endif /* DIE_ON_OVERRIDES */
				continue;	/* OK if target table overrides. */
			} else {
				as_fatal("error constructing obj pseudo-op table");
			} /* if overridden */
		} /* on error */
	} /* for each op */
	
	return;
} /* pobegin() */

#define HANDLE_CONDITIONAL_ASSEMBLY()	\
    if (ignore_input ())					\
{							\
							    while (! is_end_of_line[*input_line_pointer++])	\
								if (input_line_pointer == buffer_limit)		\
								    break;					\
									continue;						\
								    }


/*	read_a_source_file()
 *
 * We read the file, putting things into a web that
 * represents what we have been reading.
 */
void read_a_source_file(name)
char *name;
{
	register char c;
	register char *	s;	/* string of symbol, '\0' appended */
	register int temp;
	pseudo_typeS *pop = NULL;
	
	buffer = input_scrub_new_file(name);
	
	listing_file(name);
	listing_newline("");
	
	while ((buffer_limit = input_scrub_next_buffer(&input_line_pointer)) != 0) { /* We have another line to parse. */
		know(buffer_limit[-1] == '\n'); /* Must have a sentinel. */
	contin:	/* JF this goto is my fault I admit it.  Someone brave please re-write
		   the whole input section here?  Pleeze??? */
		while (input_line_pointer < buffer_limit) {			/* We have more of this buffer to parse. */
			
			/*
			 * We now have input_line_pointer->1st char of next line.
			 * If input_line_pointer[-1] == '\n' then we just
			 * scanned another line: so bump line counters.
			 */
			if (input_line_pointer[-1] == '\n') {
				bump_line_counters();
			} /* just passed a newline */
			
			
			
			/*
			 * We are at the begining of a line, or similar place.
			 * We expect a well-formed assembler statement.
			 * A "symbol-name:" is a statement.
			 *
			 * Depending on what compiler is used, the order of these tests
			 * may vary to catch most common case 1st.
			 * Each test is independent of all other tests at the (top) level.
			 * PLEASE make a compiler that doesn't use this assembler.
			 * It is crufty to waste a compiler's time encoding things for this
			 * assembler, which then wastes more time decoding it.
			 * (And communicating via (linear) files is silly!
			 * If you must pass stuff, please pass a tree!)
			 */
			if ((c = *input_line_pointer++) == '\t' || c == ' ' || c == '\f' || c == 0) {
				c = *input_line_pointer++;
			}
			know(c != ' ');	/* No further leading whitespace. */
			LISTING_NEWLINE();
			/*
			 * C is the 1st significant character.
			 * Input_line_pointer points after that character.
			 */
			if (is_name_beginner(c)) {			/* want user-defined label or pseudo/opcode */
				HANDLE_CONDITIONAL_ASSEMBLY();
				
				s = input_line_pointer - 1;
				c = get_symbol_end(); /* name's delimiter */
				/*
				 * C is character after symbol.
				 * That character's place in the input line is now '\0'.
				 * S points to the beginning of the symbol.
				 *   [In case of pseudo-op, s->'.'.]
				 * Input_line_pointer->'\0' where c was.
				 */
				if (c == ':') {
					colon(s);	/* user-defined label */
					* input_line_pointer ++ = ':'; /* Put ':' back for error messages' sake. */
					/* Input_line_pointer->after ':'. */
					SKIP_WHITESPACE();
					
					
				} else if (c == '=' || input_line_pointer[1] == '=') { /* JF deal with FOO=BAR */
					equals(s);
					demand_empty_rest_of_line();
				} else {		/* expect pseudo-op or machine instruction */
					if (*s == '.'
#ifdef NO_DOT_PSEUDOS
					    || (pop= (pseudo_typeS *) hash_find(po_hash, s))
#endif
					    ) {
						/*
						 * PSEUDO - OP.
						 *
						 * WARNING: c has next char, which may be end-of-line.
						 * We lookup the pseudo-op table with s+1 because we
						 * already know that the pseudo-op begins with a '.'.
						 */
						
#ifdef NO_DOT_PSEUDOS
						if (*s == '.')
#endif
						    pop = (pseudo_typeS *) hash_find(po_hash, s+1);
						
						/* Print the error msg now, while we still can */
						if (!pop) {
							as_bad("Unknown pseudo-op:  `%s'",s);
							*input_line_pointer = c;
							s_ignore(0);
							break;
						}
						
						/* Put it back for error messages etc. */
						*input_line_pointer = c;
						/* The following skip of whitespace is compulsory. */
						/* A well shaped space is sometimes all that separates keyword from operands. */
						if (c == ' ' || c == '\t') {
							input_line_pointer++;
						} /* Skip seperator after keyword. */
						/*
						 * Input_line is restored.
						 * Input_line_pointer->1st non-blank char
						 * after pseudo-operation.
						 */
						if (!pop) {
							ignore_rest_of_line();
							break;
						} else {
							(*pop->poc_handler)(pop->poc_val);
						} /* if we have one */
					} else {		/* machine instruction */
						/* WARNING: c has char, which may be end-of-line. */
						/* Also: input_line_pointer->`\0` where c was. */
						* input_line_pointer = c;
						while (!is_end_of_line[*input_line_pointer]) {
							input_line_pointer++;
						}
						c = *input_line_pointer;
						*input_line_pointer = '\0';
						md_assemble(s); /* Assemble 1 instruction. */
						*input_line_pointer++ = c;
						/* We resume loop AFTER the end-of-line from this instruction */
					} /* if (*s == '.') */
					
				} /* if c == ':' */
				continue;
			} /* if (is_name_beginner(c) */
			
			
			if (is_end_of_line[c]) {
				continue;
			} /* empty statement */
			
			
			if (isdigit(c)) { /* local label  ("4:") */
				HANDLE_CONDITIONAL_ASSEMBLY ();
				
				temp = c - '0';
#ifdef LOCAL_LABELS_DOLLAR
				if (*input_line_pointer == '$')
				    input_line_pointer++;
#endif
				if (*input_line_pointer++ == ':') {
					local_colon (temp);
				} else {
					as_bad("Spurious digit %d.", temp);
					input_line_pointer -- ;
					ignore_rest_of_line();
				}
				continue;
			} /* local label  ("4:") */
			
			if (c && strchr(line_comment_chars, c)) { /* Its a comment.  Better say APP or NO_APP */
				char *ends;
				char *new_buf;
				char *new_tmp;
				int new_length;
				char *tmp_buf = 0;
				extern char *scrub_string, *scrub_last_string;
				
				bump_line_counters();
				s = input_line_pointer;
				if (strncmp(s,"APP\n",4))
				    continue;	/* We ignore it */
				s += 4;
				
				ends = strstr(s,"#NO_APP\n");
				
				if (!ends) {
					int tmp_len;
					int num;
					
					/* The end of the #APP wasn't in this buffer.  We
					   keep reading in buffers until we find the #NO_APP
					   that goes with this #APP  There is one.  The specs
					   guarentee it... */
					tmp_len = buffer_limit - s;
					tmp_buf = xmalloc(tmp_len);
					memcpy(tmp_buf, s, tmp_len);
					do {
						new_tmp = input_scrub_next_buffer(&buffer);
						if (!new_tmp)
						    break;
						else
						    buffer_limit = new_tmp;
						input_line_pointer = buffer;
						ends = strstr(buffer,"#NO_APP\n");
						if (ends)
						    num = ends - buffer;
						else
						    num = buffer_limit - buffer;
						
						tmp_buf = xrealloc(tmp_buf, tmp_len + num);
						memcpy(tmp_buf + tmp_len, buffer, num);
						tmp_len += num;
					} while (!ends);
					
					input_line_pointer = ends ? ends + 8 : NULL;
					
					s = tmp_buf;
					ends = s + tmp_len;
					
				} else {
					input_line_pointer = ends + 8;
				}
				new_buf=xmalloc(100);
				new_length=100;
				new_tmp=new_buf;
				
				scrub_string = s;
				scrub_last_string = ends;
				for (;;) {
					int ch;
					
					ch = do_scrub_next_char(scrub_from_string, scrub_to_string);
					if (ch == EOF) break;
					*new_tmp++ = ch;
					if (new_tmp == new_buf + new_length) {
						new_buf = xrealloc(new_buf, new_length + 100);
						new_tmp = new_buf + new_length;
						new_length += 100;
					}
				}
				
				if (tmp_buf)
				    free(tmp_buf);
				old_buffer = buffer;
				old_input = input_line_pointer;
				old_limit = buffer_limit;
				buffer = new_buf;
				input_line_pointer = new_buf;
				buffer_limit = new_tmp;
				continue;
			}
			
			HANDLE_CONDITIONAL_ASSEMBLY();
			
			/* as_warn("Junk character %d.",c);  Now done by ignore_rest */
			input_line_pointer--;		/* Report unknown char as ignored. */
			ignore_rest_of_line();
		}			/* while (input_line_pointer<buffer_limit) */
		if (old_buffer) {
			bump_line_counters();
			if (old_input != 0) {
				buffer=old_buffer;
				input_line_pointer=old_input;
				buffer_limit=old_limit;
				old_buffer = 0;
				goto contin;
			}
		}
	} /* while (more buffers to scan) */
	input_scrub_close();	/* Close the input file */
	
} /* read_a_source_file() */

void s_abort() {
	as_fatal(".abort detected.  Abandoning ship.");
} /* s_abort() */

/* For machines where ".align 4" means align to a 4 byte boundary. */
void s_align_bytes(arg)
int arg;
{
	register unsigned int temp;
	register long temp_fill;
	unsigned int i = 0;
	unsigned long max_alignment = 1 << 15;
	
	if (is_end_of_line[*input_line_pointer])
	    temp = arg;		/* Default value from pseudo-op table */
	else
	    temp = get_absolute_expression();
	
	if (temp > max_alignment) {
		as_bad("Alignment too large: %d. assumed.", temp = max_alignment);
	}
	
	/*
	 * For the sparc, `.align (1<<n)' actually means `.align n'
	 * so we have to convert it.
	 */
	if (temp != 0) {
		for (i = 0; (temp & 1) == 0; temp >>= 1, ++i)
		    ;
	}
	if (temp != 1)
	    as_bad("Alignment not a power of 2");
	
	temp = i;
	if (*input_line_pointer == ',') {
		input_line_pointer ++;
		temp_fill = get_absolute_expression();
	} else {
		temp_fill = NOP_OPCODE;
	}
	/* Only make a frag if we HAVE to... */
	if (temp && ! need_pass_2)
	    frag_align(temp, (int)temp_fill);
	
	demand_empty_rest_of_line();
} /* s_align_bytes() */

/* For machines where ".align 4" means align to 2**4 boundary. */
void s_align_ptwo() {
	register int temp;
	register long temp_fill;
	long max_alignment = 15;
	
	temp = get_absolute_expression();
	if (temp > max_alignment)
	    as_bad("Alignment too large: %d. assumed.", temp = max_alignment);
	else if (temp < 0) {
		as_bad("Alignment negative. 0 assumed.");
		temp = 0;
	}
	if (*input_line_pointer == ',') {
		input_line_pointer ++;
		temp_fill = get_absolute_expression();
	} else
	    temp_fill = NOP_OPCODE;
	/* Only make a frag if we HAVE to... */
	if (temp && ! need_pass_2)
	    frag_align (temp, (int)temp_fill);
	
	record_alignment(now_seg, temp);
	
	demand_empty_rest_of_line();
} /* s_align_ptwo() */

void s_comm() {
	register char *name;
	register char c;
	register char *p;
	register int temp;
	register symbolS *symbolP;
	
	name = input_line_pointer;
	c = get_symbol_end();
	/* just after name is now '\0' */
	p = input_line_pointer;
	*p = c;
	SKIP_WHITESPACE();
	if (*input_line_pointer != ',') {
		as_bad("Expected comma after symbol-name: rest of line ignored.");
		ignore_rest_of_line();
		return;
	}
	input_line_pointer ++; /* skip ',' */
	if ((temp = get_absolute_expression()) < 0) {
		as_warn(".COMMon length (%d.) <0! Ignored.", temp);
		ignore_rest_of_line();
		return;
	}
	*p = 0;
	symbolP = symbol_find_or_make(name);
	*p = c;
	if (S_IS_DEFINED(symbolP)) {
		as_bad("Ignoring attempt to re-define symbol");
		ignore_rest_of_line();
		return;
	}
	if (S_GET_VALUE(symbolP)) {
		if (S_GET_VALUE(symbolP) != temp)
		    as_bad("Length of .comm \"%s\" is already %d. Not changed to %d.",
			   S_GET_NAME(symbolP),
			   S_GET_VALUE(symbolP),
			   temp);
	} else {
		S_SET_VALUE(symbolP, temp);
		S_SET_EXTERNAL(symbolP);
	}
#ifdef OBJ_VMS
	if ( (!temp) || !flagseen['1'])
		S_GET_OTHER(symbolP)  = const_flag;
#endif /* not OBJ_VMS */
	know(symbolP->sy_frag == &zero_address_frag);
	demand_empty_rest_of_line();
} /* s_comm() */

void
    s_data()
{
	register int temp;
	
	temp = get_absolute_expression();
#ifdef MANY_SEGMENTS
	subseg_new (SEG_E1, (subsegT)temp);
#else
	subseg_new (SEG_DATA, (subsegT)temp);
#endif
	
#ifdef OBJ_VMS
	const_flag = 0;
#endif /* not OBJ_VMS */
	demand_empty_rest_of_line();
}

void s_app_file() {
	register char *s;
	int length;
	
	/* Some assemblers tolerate immediately following '"' */
	if ((s = demand_copy_string(&length)) != 0) {
		new_logical_line(s, -1);
		demand_empty_rest_of_line();
	}
#ifdef OBJ_COFF
	c_dot_file_symbol(s);
#endif /* OBJ_COFF */
} /* s_app_file() */

void s_fill() {
	long temp_repeat;
	long temp_size;
	register long temp_fill;
	char *p;
	
	if (get_absolute_expression_and_terminator(& temp_repeat) != ',') {
		input_line_pointer --; /* Backup over what was not a ','. */
		as_bad("Expect comma after rep-size in .fill:");
		ignore_rest_of_line();
		return;
	}
	if (get_absolute_expression_and_terminator(& temp_size) != ',') {
		input_line_pointer --; /* Backup over what was not a ','. */
		as_bad("Expected comma after size in .fill");
		ignore_rest_of_line();
		return;
	}
	/*
	 * This is to be compatible with BSD 4.2 AS, not for any rational reason.
	 */
#define BSD_FILL_SIZE_CROCK_8 (8)
	if (temp_size > BSD_FILL_SIZE_CROCK_8) {
		as_bad(".fill size clamped to %d.", BSD_FILL_SIZE_CROCK_8);
		temp_size = BSD_FILL_SIZE_CROCK_8 ;
	} if (temp_size < 0) {
		as_warn("Size negative: .fill ignored.");
		temp_size = 0;
	} else if (temp_repeat <= 0) {
		as_warn("Repeat < 0, .fill ignored");
		temp_size = 0;
	}
	temp_fill = get_absolute_expression();
	if (temp_size && !need_pass_2) {
		p = frag_var(rs_fill, (int)temp_size, (int)temp_size, (relax_substateT)0, (symbolS *)0, temp_repeat, (char *)0);
		memset(p, '\0', (int) temp_size);
		/*
		 * The magic number BSD_FILL_SIZE_CROCK_4 is from BSD 4.2 VAX flavoured AS.
		 * The following bizzare behaviour is to be compatible with above.
		 * I guess they tried to take up to 8 bytes from a 4-byte expression
		 * and they forgot to sign extend. Un*x Sux.
		 */
#define BSD_FILL_SIZE_CROCK_4 (4)
		md_number_to_chars (p, temp_fill, temp_size > BSD_FILL_SIZE_CROCK_4 ? BSD_FILL_SIZE_CROCK_4 : (int)temp_size);
		/*
		 * Note: .fill (),0 emits no frag (since we are asked to .fill 0 bytes)
		 * but emits no error message because it seems a legal thing to do.
		 * It is a degenerate case of .fill but could be emitted by a compiler.
		 */
	}
	demand_empty_rest_of_line();
}

void s_globl() {
	register char *name;
	register int c;
	register symbolS *	symbolP;
	
	do {
		name = input_line_pointer;
		c = get_symbol_end();
		symbolP = symbol_find_or_make(name);
		* input_line_pointer = c;
		SKIP_WHITESPACE();
		S_SET_EXTERNAL(symbolP);
		if (c == ',') {
			input_line_pointer++;
			SKIP_WHITESPACE();
			if (*input_line_pointer == '\n')
			    c='\n';
		}
	} while (c == ',');
	demand_empty_rest_of_line();
} /* s_globl() */

void s_lcomm(needs_align)
int needs_align;	/* 1 if this was a ".bss" directive, which may require
			 *	a 3rd argument (alignment).
			 * 0 if it was an ".lcomm" (2 args only)
			 */
{
	register char *name;
	register char c;
	register char *p;
	register int temp;
	register symbolS *	symbolP;
	const int max_alignment = 15;
	int align = 0;
	
	name = input_line_pointer;
	c = get_symbol_end();
	p = input_line_pointer;
	*p = c;
	SKIP_WHITESPACE();
	if (*input_line_pointer != ',') {
		as_bad("Expected comma after name");
		ignore_rest_of_line();
		return;
	}
	
	++input_line_pointer;
	
	if (*input_line_pointer == '\n') {
		as_bad("Missing size expression");
		return;
	}
	
	if ((temp = get_absolute_expression()) < 0) {
		as_warn("BSS length (%d.) <0! Ignored.", temp);
		ignore_rest_of_line();
		return;
	}
	
	if (needs_align) {
		align = 0;
		SKIP_WHITESPACE();
		if (*input_line_pointer != ',') {
			as_bad("Expected comma after size");
			ignore_rest_of_line();
			return;
		}
		input_line_pointer++;
		SKIP_WHITESPACE();
		if (*input_line_pointer == '\n') {
			as_bad("Missing alignment");
			return;
		}
		align = get_absolute_expression();
		if (align > max_alignment){
			align = max_alignment;
			as_warn("Alignment too large: %d. assumed.", align);
		} else if (align < 0) {
			align = 0;
			as_warn("Alignment negative. 0 assumed.");
		}
#ifdef MANY_SEGMENTS
#define SEG_BSS SEG_E2
		record_alignment(SEG_E2, align);
#else
		record_alignment(SEG_BSS, align);
#endif
	} /* if needs align */
	
	*p = 0;
	symbolP = symbol_find_or_make(name);
	*p = c;
	
	if (
#if defined(OBJ_AOUT) | defined(OBJ_BOUT)
	    S_GET_OTHER(symbolP) == 0 &&
	    S_GET_DESC(symbolP) == 0 &&
#endif /* OBJ_AOUT or OBJ_BOUT */
	    (((S_GET_SEGMENT(symbolP) == SEG_BSS) && (S_GET_VALUE(symbolP) == local_bss_counter))
	     || (!S_IS_DEFINED(symbolP) && S_GET_VALUE(symbolP) == 0))) {
		if (needs_align){
			/* Align */
			align = ~ ((~0) << align);	/* Convert to a mask */
			local_bss_counter =
			    (local_bss_counter + align) & (~align);
		}
		
		S_SET_VALUE(symbolP, local_bss_counter);
		S_SET_SEGMENT(symbolP, SEG_BSS);
#ifdef OBJ_COFF
		/* The symbol may already have been created with a preceding
		 * ".globl" directive -- be careful not to step on storage
		 * class in that case.  Otherwise, set it to static.
		 */
		if (S_GET_STORAGE_CLASS(symbolP) != C_EXT){
			S_SET_STORAGE_CLASS(symbolP, C_STAT);
		}
#endif /* OBJ_COFF */
		symbolP->sy_frag  = &bss_address_frag;
		local_bss_counter += temp;
	} else {
		as_bad("Ignoring attempt to re-define symbol from %d. to %d.",
		       S_GET_VALUE(symbolP), local_bss_counter);
	}
	demand_empty_rest_of_line();
	
	return;
} /* s_lcomm() */

void
    s_long()
{
	cons(4);
}

void
    s_int()
{
	cons(4);
}

void s_lsym() {
	register char *name;
	register char c;
	register char *p;
	register segT segment;
	expressionS exp;
	register symbolS *symbolP;
	
	/* we permit ANY defined expression: BSD4.2 demands constants */
	name = input_line_pointer;
	c = get_symbol_end();
	p = input_line_pointer;
	*p = c;
	SKIP_WHITESPACE();
	if (* input_line_pointer != ',') {
		*p = 0;
		as_bad("Expected comma after name \"%s\"", name);
		*p = c;
		ignore_rest_of_line();
		return;
	}
	input_line_pointer ++;
	segment = expression(& exp);
	if (segment != SEG_ABSOLUTE
#ifdef MANY_SEGMENTS
	    && ! ( segment >= SEG_E0 && segment <= SEG_UNKNOWN)
#else
	    && segment != SEG_DATA
	    && segment != SEG_TEXT
	    && segment != SEG_BSS
#endif
	    && segment != SEG_REGISTER) {
		as_bad("Bad expression: %s", segment_name(segment));
		ignore_rest_of_line();
		return;
	}
	*p = 0;
	symbolP = symbol_find_or_make(name);
	
	/* FIXME-SOON I pulled a (&& symbolP->sy_other == 0
	   && symbolP->sy_desc == 0) out of this test
	   because coff doesn't have those fields, and I
	   can't see when they'd ever be tripped.  I don't
	   think I understand why they were here so I may
	   have introduced a bug. As recently as 1.37 didn't
	   have this test anyway.  xoxorich. */
	
	if (S_GET_SEGMENT(symbolP) == SEG_UNKNOWN
	    && S_GET_VALUE(symbolP) == 0) {
		/* The name might be an undefined .global symbol; be
		   sure to keep the "external" bit. */
		S_SET_SEGMENT(symbolP, segment);
		S_SET_VALUE(symbolP, (valueT)(exp.X_add_number));
	} else {
		as_bad("Symbol %s already defined", name);
	}
	*p = c;
	demand_empty_rest_of_line();
} /* s_lsym() */

void s_org() {
	register segT segment;
	expressionS exp;
	register long temp_fill;
	register char *p;
	/*
	 * Don't believe the documentation of BSD 4.2 AS.
	 * There is no such thing as a sub-segment-relative origin.
	 * Any absolute origin is given a warning, then assumed to be segment-relative.
	 * Any segmented origin expression ("foo+42") had better be in the right
	 * segment or the .org is ignored.
	 *
	 * BSD 4.2 AS warns if you try to .org backwards. We cannot because we
	 * never know sub-segment sizes when we are reading code.
	 * BSD will crash trying to emit -ve numbers of filler bytes in certain
	 * .orgs. We don't crash, but see as-write for that code.
	 */
	/*
	 * Don't make frag if need_pass_2 == 1.
	 */
	segment = get_known_segmented_expression(&exp);
	if (*input_line_pointer == ',') {
		input_line_pointer ++;
		temp_fill = get_absolute_expression();
	} else
	    temp_fill = 0;
	if (! need_pass_2) {
		if (segment != now_seg && segment != SEG_ABSOLUTE)
		    as_bad("Invalid segment \"%s\". Segment \"%s\" assumed.",
			   segment_name(segment), segment_name(now_seg));
		p = frag_var (rs_org, 1, 1, (relax_substateT)0, exp.X_add_symbol,
			      exp.X_add_number, (char *)0);
		* p = temp_fill;
	} /* if (ok to make frag) */
	demand_empty_rest_of_line();
} /* s_org() */

void s_set() {
	register char *name;
	register char delim;
	register char *end_name;
	register symbolS *symbolP;
	
	/*
	 * Especial apologies for the random logic:
	 * this just grew, and could be parsed much more simply!
	 * Dean in haste.
	 */
	name = input_line_pointer;
	delim = get_symbol_end();
	end_name = input_line_pointer;
	*end_name = delim;
	SKIP_WHITESPACE();
	
	if (*input_line_pointer != ',') {
		*end_name = 0;
		as_bad("Expected comma after name \"%s\"", name);
		*end_name = delim;
		ignore_rest_of_line();
		return;
	}
	
	input_line_pointer ++;
	*end_name = 0;
	
	if (name[0] == '.' && name[1] == '\0') {
		/* Turn '. = mumble' into a .org mumble */
		register segT segment;
		expressionS exp;
		register char *ptr;
		
		segment = get_known_segmented_expression(& exp);
		
		if (!need_pass_2) {
			if (segment != now_seg && segment != SEG_ABSOLUTE)
			    as_bad("Invalid segment \"%s\". Segment \"%s\" assumed.",
				   segment_name(segment),
				   segment_name (now_seg));
			ptr = frag_var(rs_org, 1, 1, (relax_substateT)0, exp.X_add_symbol,
				       exp.X_add_number, (char *)0);
			*ptr= 0;
		} /* if (ok to make frag) */
		
		*end_name = delim;
		return;
	}
	
	if ((symbolP = symbol_find(name)) == NULL
	    && (symbolP = md_undefined_symbol(name)) == NULL) {
		symbolP = symbol_new(name,
				     SEG_UNKNOWN,
				     0,
				     &zero_address_frag);
#ifdef OBJ_COFF
		/* "set" symbols are local unless otherwise specified. */
		SF_SET_LOCAL(symbolP);
#endif /* OBJ_COFF */
		
	} /* make a new symbol */
	
	symbol_table_insert(symbolP);
	
	*end_name = delim;
	pseudo_set(symbolP);
	demand_empty_rest_of_line();
} /* s_set() */

void s_size() {
	register char *name;
	register char c;
	register char *p;
	register int temp;
	register symbolS *symbolP;
	expressionS	*exp;
	segT		seg;
	
	SKIP_WHITESPACE();
	name = input_line_pointer;
	c = get_symbol_end();
	/* just after name is now '\0' */
	p = input_line_pointer;
	*p = c;
	SKIP_WHITESPACE();
	if (*input_line_pointer != ',') {
		as_bad("Expected comma after symbol-name: rest of line ignored.");
		ignore_rest_of_line();
		return;
	}
	input_line_pointer ++; /* skip ',' */
	if ((exp = (expressionS *)malloc(sizeof(expressionS))) == NULL) {
		as_bad("Virtual memory exhausted");
		return;
	}
	switch (get_known_segmented_expression(exp)) {
	case SEG_ABSOLUTE:
		break;
	case SEG_DIFFERENCE:
		if (exp->X_add_symbol == NULL || exp->X_subtract_symbol == NULL
			|| S_GET_SEGMENT(exp->X_add_symbol) !=
				S_GET_SEGMENT(exp->X_subtract_symbol)) {
			as_bad("Illegal .size expression");
			ignore_rest_of_line();
			return;
		}
		break;
	default:
		as_bad("Illegal .size expression");
		ignore_rest_of_line();
		return;
	}
	*p = 0;
	symbolP = symbol_find_or_make(name);
	*p = c;
	if (symbolP->sy_sizexp) {
		as_warn("\"%s\" already has a size", S_GET_NAME(symbolP));
	} else
		symbolP->sy_sizexp = (void *)exp;

	demand_empty_rest_of_line();
} /* s_size() */

void s_type() {
	register char *name, *type;
	register char c, c1;
	register char *p;
	register symbolS *symbolP;
	int	aux;
	
	SKIP_WHITESPACE();
	name = input_line_pointer;
	c = get_symbol_end();
	/* just after name is now '\0' */
	p = input_line_pointer;
	*p = c;
	SKIP_WHITESPACE();
	if (*input_line_pointer != ',') {
		as_bad("Expected comma after symbol-name: rest of line ignored.");
		ignore_rest_of_line();
		return;
	}
	input_line_pointer ++; /* skip ',' */
	SKIP_WHITESPACE();
	if (*input_line_pointer != TYPE_OPERAND_FMT) {
		as_bad("Expected `%c' as start of operand: rest of line ignored.", TYPE_OPERAND_FMT);
		ignore_rest_of_line();
		return;
	}
	input_line_pointer ++; /* skip '@' */
	type = input_line_pointer;
	c1 = get_symbol_end();
	if (strcmp(type, "function") == 0) {
		aux = AUX_FUNC;
	} else if (strcmp(type, "object") == 0) {
		aux = AUX_OBJECT;
	} else {
		as_warn("Unrecognized .type operand: \"%s\": rest of line ignored.",
				type);
		ignore_rest_of_line();
		return;
	}
	*input_line_pointer = c1;

	*p = 0;
	symbolP = symbol_find_or_make(name);
	*p = c;

	if (symbolP->sy_aux && symbolP->sy_aux != aux) {
	    as_bad("Type of \"%s\" is already %d. Not changed to %d.",
		   S_GET_NAME(symbolP), symbolP->sy_aux, aux);
	} else
		symbolP->sy_aux = aux;

	demand_empty_rest_of_line();
} /* s_type() */

void s_space() {
	long temp_repeat;
	register long temp_fill;
	register char *p;
	
	/* Just like .fill, but temp_size = 1 */
	if (get_absolute_expression_and_terminator(& temp_repeat) == ',') {
		temp_fill = get_absolute_expression();
	} else {
		input_line_pointer --; /* Backup over what was not a ','. */
		temp_fill = 0;
	}
	if (temp_repeat <= 0) {
		as_warn("Repeat < 0, .space ignored");
		ignore_rest_of_line();
		return;
	}
	if (! need_pass_2) {
		p = frag_var (rs_fill, 1, 1, (relax_substateT)0, (symbolS *)0,
			      temp_repeat, (char *)0);
		* p = temp_fill;
	}
	demand_empty_rest_of_line();
} /* s_space() */

void
    s_text()
{
	register int temp;
	
	temp = get_absolute_expression();
#ifdef MANY_SEGMENTS
	subseg_new (SEG_E0, (subsegT)temp);
#else
	subseg_new (SEG_TEXT, (subsegT)temp);
#endif
	demand_empty_rest_of_line();
} /* s_text() */


/*(JF was static, but can't be if machine dependent pseudo-ops are to use it */

void demand_empty_rest_of_line() {
	SKIP_WHITESPACE();
	if (is_end_of_line[*input_line_pointer]) {
		input_line_pointer++;
	} else {
		ignore_rest_of_line();
	}
	/* Return having already swallowed end-of-line. */
} /* Return pointing just after end-of-line. */

void
    ignore_rest_of_line()		/* For suspect lines: gives warning. */
{
	if (!is_end_of_line[*input_line_pointer])
	    {
		    if (isprint(*input_line_pointer))
			as_bad("Rest of line ignored. First ignored character is `%c'.",
			       *input_line_pointer);
		    else
			as_bad("Rest of line ignored. First ignored character valued 0x%x.",
			       *input_line_pointer);
		    while (input_line_pointer < buffer_limit
			   && !is_end_of_line[*input_line_pointer])
			{
				input_line_pointer ++;
			}
	    }
	input_line_pointer ++;	/* Return pointing just after end-of-line. */
	know(is_end_of_line[input_line_pointer[-1]]);
}

/*
 *			pseudo_set()
 *
 * In:	Pointer to a symbol.
 *	Input_line_pointer->expression.
 *
 * Out:	Input_line_pointer->just after any whitespace after expression.
 *	Tried to set symbol to value of expression.
 *	Will change symbols type, value, and frag;
 *	May set need_pass_2 == 1.
 */
void
    pseudo_set (symbolP)
symbolS *	symbolP;
{
	expressionS	exp;
	register segT	segment;
#if defined(OBJ_AOUT) | defined(OBJ_BOUT)
	int ext;
#endif /* OBJ_AOUT or OBJ_BOUT */
	
	know(symbolP);		/* NULL pointer is logic error. */
#if defined(OBJ_AOUT) | defined(OBJ_BOUT)
	ext=S_IS_EXTERNAL(symbolP);
#endif /* OBJ_AOUT or OBJ_BOUT */
	
	if ((segment = expression(& exp)) == SEG_ABSENT)
	    {
		    as_bad("Missing expression: absolute 0 assumed");
		    exp.X_seg		= SEG_ABSOLUTE;
		    exp.X_add_number	= 0;
	    }
	
	switch (segment)
	    {
	    case SEG_BIG:
		    as_bad("%s number invalid. Absolute 0 assumed.",
			   exp.X_add_number > 0 ? "Bignum" : "Floating-Point");
		    S_SET_SEGMENT(symbolP, SEG_ABSOLUTE);
#if defined(OBJ_AOUT) | defined(OBJ_BOUT)
		    ext ? S_SET_EXTERNAL(symbolP) :
			S_CLEAR_EXTERNAL(symbolP);
#endif /* OBJ_AOUT or OBJ_BOUT */
		    S_SET_VALUE(symbolP, 0);
		    symbolP->sy_frag = & zero_address_frag;
		    break;
		    
	    case SEG_ABSENT:
		    as_warn("No expression:  Using absolute 0");
		    S_SET_SEGMENT(symbolP, SEG_ABSOLUTE);
#if defined(OBJ_AOUT) | defined(OBJ_BOUT)
		    ext ? S_SET_EXTERNAL(symbolP) :
			S_CLEAR_EXTERNAL(symbolP);
#endif /* OBJ_AOUT or OBJ_BOUT */
		    S_SET_VALUE(symbolP, 0);
		    symbolP->sy_frag = & zero_address_frag;
		    break;
		    
	    case SEG_DIFFERENCE:
		    if (exp.X_add_symbol && exp.X_subtract_symbol
			&& (S_GET_SEGMENT(exp.X_add_symbol) == 
			    S_GET_SEGMENT(exp.X_subtract_symbol))) {
			    if (exp.X_add_symbol->sy_frag != exp.X_subtract_symbol->sy_frag) {
				    as_bad("Unknown expression: symbols %s and %s are in different frags.",
					   S_GET_NAME(exp.X_add_symbol), S_GET_NAME(exp.X_subtract_symbol));
				    need_pass_2++;
			    }
			    exp.X_add_number+=S_GET_VALUE(exp.X_add_symbol) -
				S_GET_VALUE(exp.X_subtract_symbol);
		    } else
			as_bad("Complex expression. Absolute segment assumed.");
	    case SEG_ABSOLUTE:
		    S_SET_SEGMENT(symbolP, SEG_ABSOLUTE);
#if defined(OBJ_AOUT) | defined(OBJ_BOUT)
		    ext ? S_SET_EXTERNAL(symbolP) :
			S_CLEAR_EXTERNAL(symbolP);
#endif /* OBJ_AOUT or OBJ_BOUT */
		    S_SET_VALUE(symbolP, exp.X_add_number);
		    symbolP->sy_frag = & zero_address_frag;
		    break;
		    
	    default:
#ifdef MANY_SEGMENTS
		    S_SET_SEGMENT(symbolP, segment);
#else
		    switch (segment) {
		    case SEG_DATA:	S_SET_SEGMENT(symbolP, SEG_DATA); break;
		    case SEG_TEXT:	S_SET_SEGMENT(symbolP, SEG_TEXT); break;
		    case SEG_BSS:	S_SET_SEGMENT(symbolP, SEG_BSS); break;
		    default:	as_fatal("failed sanity check.");
		    }	/* switch on segment */
#endif
#if defined(OBJ_AOUT) | defined(OBJ_BOUT)
		    if (ext) {
			    S_SET_EXTERNAL(symbolP);
		    } else {
			    S_CLEAR_EXTERNAL(symbolP);
		    }	/* if external */
#endif /* OBJ_AOUT or OBJ_BOUT */
		    
		    S_SET_VALUE(symbolP, exp.X_add_number + S_GET_VALUE(exp.X_add_symbol));
		    symbolP->sy_frag = exp.X_add_symbol->sy_frag;
		    break;
		    
	    case SEG_PASS1:		/* Not an error. Just try another pass. */
		    symbolP->sy_forward=exp.X_add_symbol;
		    as_bad("Unknown expression");
		    know(need_pass_2 == 1);
		    break;
		    
	    case SEG_UNKNOWN:
		    symbolP->sy_forward=exp.X_add_symbol;
		    /* as_warn("unknown symbol"); */
		    /* need_pass_2 = 1; */
		    break;
		    
		    
		    
	    }
}

/*
 *			cons()
 *
 * CONStruct more frag of .bytes, or .words etc.
 * Should need_pass_2 be 1 then emit no frag(s).
 * This understands EXPRESSIONS, as opposed to big_cons().
 *
 * Bug (?)
 *
 * This has a split personality. We use expression() to read the
 * value. We can detect if the value won't fit in a byte or word.
 * But we can't detect if expression() discarded significant digits
 * in the case of a long. Not worth the crocks required to fix it.
 */

/* worker to do .byte etc statements */
/* clobbers input_line_pointer, checks */
/* end-of-line. */
void cons(nbytes)
register unsigned int nbytes;	/* 1=.byte, 2=.word, 4=.long */
{
	register char c;
	register long mask;	/* High-order bits we will left-truncate, */
	/* but includes sign bit also. */
	register long get;	/* what we get */
	register long use;	/* get after truncation. */
	register long unmask;	/* what bits we will store */
	register char *	p;
	register segT		segment;
	expressionS	exp;
	
	/*
	 * Input_line_pointer->1st char after pseudo-op-code and could legally
	 * be a end-of-line. (Or, less legally an eof - which we cope with.)
	 */
	/* JF << of >= number of bits in the object is undefined.  In particular
	   SPARC (Sun 4) has problems */
	
	if (nbytes >= sizeof(long)) {
		mask = 0;
	} else {
		mask = ~0 << (BITS_PER_CHAR * nbytes); /* Don't store these bits. */
	} /* bigger than a long */
	
	unmask = ~mask;		/* Do store these bits. */
	
#ifdef NEVER
	"Do this mod if you want every overflow check to assume SIGNED 2's complement data.";
	mask = ~ (unmask >> 1);	/* Includes sign bit now. */
#endif
	
	/*
	 * The following awkward logic is to parse ZERO or more expressions,
	 * comma seperated. Recall an expression includes its leading &
	 * trailing blanks. We fake a leading ',' if there is (supposed to
	 * be) a 1st expression, and keep demanding 1 expression for each ','.
	 */
	if (is_it_end_of_statement()) {
		c = 0;			/* Skip loop. */
		input_line_pointer++;	/* Matches end-of-loop 'correction'. */
	} else {
		c = ',';
	} /* if the end else fake it */
	
	/* Do loop. */
	while (c == ',') {
#ifdef WANT_BITFIELDS
		unsigned int bits_available = BITS_PER_CHAR * nbytes;
		/* used for error messages and rescanning */
		char *hold = input_line_pointer;
#endif /* WANT_BITFIELDS */
		
		/* At least scan over the expression. */
		segment = expression(&exp);
		
#ifdef WANT_BITFIELDS
		/* Some other assemblers, (eg, asm960), allow
		   bitfields after ".byte" as w:x,y:z, where w and
		   y are bitwidths and x and y are values.  They
		   then pack them all together. We do a little
		   better in that we allow them in words, longs,
		   etc. and we'll pack them in target byte order
		   for you.
		   
		   The rules are: pack least significat bit first,
		   if a field doesn't entirely fit, put it in the
		   next unit.  Overflowing the bitfield is
		   explicitly *not* even a warning.  The bitwidth
		   should be considered a "mask".
		   
		   FIXME-SOMEDAY: If this is considered generally
		   useful, this logic should probably be reworked.
		   xoxorich. */
		
		if (*input_line_pointer == ':') { /* bitfields */
			long value = 0;
			
			for (;;) {
				unsigned long width;
				
				if (*input_line_pointer != ':') {
					input_line_pointer = hold;
					break;
				} /* next piece is not a bitfield */
				
				/* In the general case, we can't allow
				   full expressions with symbol
				   differences and such.  The relocation
				   entries for symbols not defined in this
				   assembly would require arbitrary field
				   widths, positions, and masks which most
				   of our current object formats don't
				   support.
				   
				   In the specific case where a symbol
				   *is* defined in this assembly, we
				   *could* build fixups and track it, but
				   this could lead to confusion for the
				   backends.  I'm lazy. I'll take any
				   SEG_ABSOLUTE. I think that means that
				   you can use a previous .set or
				   .equ type symbol.  xoxorich. */
				
				if (segment == SEG_ABSENT) {
					as_warn("Using a bit field width of zero.");
					exp.X_add_number = 0;
					segment = SEG_ABSOLUTE;
				} /* implied zero width bitfield */
				
				if (segment != SEG_ABSOLUTE) {
					*input_line_pointer = '\0';
					as_bad("Field width \"%s\" too complex for a bitfield.\n", hold);
					*input_line_pointer = ':';
					demand_empty_rest_of_line();
					return;
				} /* too complex */
				
				if ((width = exp.X_add_number) > (BITS_PER_CHAR * nbytes)) {
					as_warn("Field width %d too big to fit in %d bytes: truncated to %d bits.",
						width, nbytes, (BITS_PER_CHAR * nbytes));
					width = BITS_PER_CHAR * nbytes;
				} /* too big */
				
				if (width > bits_available) {
					/* FIXME-SOMEDAY: backing up and
					   reparsing is wasteful */
					input_line_pointer = hold;
					exp.X_add_number = value;
					break;
				} /* won't fit */
				
				hold = ++input_line_pointer; /* skip ':' */
				
				if ((segment = expression(&exp)) != SEG_ABSOLUTE) {
					char cache = *input_line_pointer;
					
					*input_line_pointer = '\0';
					as_bad("Field value \"%s\" too complex for a bitfield.\n", hold);
					*input_line_pointer = cache;
					demand_empty_rest_of_line();
					return;
				} /* too complex */
				
				value |= (~(-1 << width) & exp.X_add_number)
				    << ((BITS_PER_CHAR * nbytes) - bits_available);
				
				if ((bits_available -= width) == 0
				    || is_it_end_of_statement()
				    || *input_line_pointer != ',') {
					break;
				} /* all the bitfields we're gonna get */
				
				hold = ++input_line_pointer;
				segment = expression(&exp);
			} /* forever loop */
			
			exp.X_add_number = value;
			segment = SEG_ABSOLUTE;
		}	/* if looks like a bitfield */
#endif /* WANT_BITFIELDS */
		
		if (!need_pass_2) { /* Still worthwhile making frags. */
			
			/* Don't call this if we are going to junk this pass anyway! */
			know(segment != SEG_PASS1);
			
			if (segment == SEG_DIFFERENCE && exp.X_add_symbol == NULL) {
				as_bad("Subtracting symbol \"%s\"(segment\"%s\") is too hard. Absolute segment assumed.",
				       S_GET_NAME(exp.X_subtract_symbol),
				       segment_name(S_GET_SEGMENT(exp.X_subtract_symbol)));
				segment = SEG_ABSOLUTE;
				/* Leave exp.X_add_number alone. */
			}

			p = frag_more(nbytes);

			switch (segment) {
			case SEG_BIG:
				as_bad("%s number invalid. Absolute 0 assumed.",
				       exp.X_add_number > 0 ? "Bignum" : "Floating-Point");
				md_number_to_chars (p, (long)0, nbytes);
				break;
				
			case SEG_ABSENT:
				as_warn("0 assumed for missing expression");
				exp.X_add_number = 0;
				know(exp.X_add_symbol == NULL);
				/* fall into SEG_ABSOLUTE */
			case SEG_ABSOLUTE:
				get = exp.X_add_number;
				use = get & unmask;
				if ((get & mask) && (get & mask) != mask)
				    {		/* Leading bits contain both 0s & 1s. */
					    as_warn("Value 0x%x truncated to 0x%x.", get, use);
				    }
				md_number_to_chars (p, use, nbytes); /* put bytes in right order. */
				break;
				
			case SEG_DIFFERENCE:
#ifndef WORKING_DOT_WORD
				if (nbytes == 2) {
					struct broken_word *x;
					
					x = (struct broken_word *) xmalloc(sizeof(struct broken_word));
					x->next_broken_word = broken_words;
					broken_words = x;
					x->frag = frag_now;
					x->word_goes_here = p;
					x->dispfrag = 0;
					x->add = exp.X_add_symbol;
					x->sub = exp.X_subtract_symbol;
					x->addnum = exp.X_add_number;
					x->added = 0;
					new_broken_words++;
					break;
				}
				/* Else Fall through into... */
#endif
			default:
			case SEG_UNKNOWN:
#ifdef TC_NS32K
				fix_new_ns32k(frag_now, p - frag_now->fr_literal, nbytes,
					       exp.X_add_symbol, exp.X_subtract_symbol,
					       exp.X_add_number, 0, 0, 2, 0, 0);
#else
#ifdef PIC
				fix_new(frag_now, p - frag_now->fr_literal, nbytes,
					 exp.X_add_symbol, exp.X_subtract_symbol,
					 exp.X_add_number, 0, RELOC_32,
					 exp.X_got_symbol);
#else
				fix_new(frag_now, p - frag_now->fr_literal, nbytes,
					 exp.X_add_symbol, exp.X_subtract_symbol,
					 exp.X_add_number, 0, RELOC_32);
#endif
#endif /* TC_NS32K */
				break;
			} /* switch (segment) */
		} /* if (!need_pass_2) */
		c = *input_line_pointer++;
	} /* while (c == ',') */
	input_line_pointer--;	/* Put terminator back into stream. */
	demand_empty_rest_of_line();
} /* cons() */

/*
 *			big_cons()
 *
 * CONStruct more frag(s) of .quads, or .octa etc.
 * Makes 0 or more new frags.
 * If need_pass_2 == 1, generate no frag.
 * This understands only bignums, not expressions. Cons() understands
 * expressions.
 *
 * Constants recognised are '0...'(octal) '0x...'(hex) '...'(decimal).
 *
 * This creates objects with struct obstack_control objs, destroying
 * any context objs held about a partially completed object. Beware!
 *
 *
 * I think it sucks to have 2 different types of integers, with 2
 * routines to read them, store them etc.
 * It would be nicer to permit bignums in expressions and only
 * complain if the result overflowed. However, due to "efficiency"...
 */
/* worker to do .quad etc statements */
/* clobbers input_line_pointer, checks */
/* end-of-line. */
/* 8=.quad 16=.octa ... */

void big_cons(nbytes)
register int nbytes;
{
	register char c;	/* input_line_pointer->c. */
	register int radix;
	register long length;	/* Number of chars in an object. */
	register int digit;	/* Value of 1 digit. */
	register int carry;	/* For multi-precision arithmetic. */
	register int work;	/* For multi-precision arithmetic. */
	register char *	p;	/* For multi-precision arithmetic. */
	
	extern const char hex_value[];	/* In hex_value.c. */

	/*
	 * The following awkward logic is to parse ZERO or more strings,
	 * comma seperated. Recall an expression includes its leading &
	 * trailing blanks. We fake a leading ',' if there is (supposed to
	 * be) a 1st expression, and keep demanding 1 expression for each ','.
	 */
	if (is_it_end_of_statement())
	    {
		    c = 0;			/* Skip loop. */
	    }
	else
	    {
		    c = ',';			/* Do loop. */
		    -- input_line_pointer;
	    }
	while (c == ',')
	    {
		    ++ input_line_pointer;
		    SKIP_WHITESPACE();
		    c = * input_line_pointer;
		    /* C contains 1st non-blank character of what we hope is a number. */
		    if (c == '0')
			{
				c = * ++ input_line_pointer;
				if (c == 'x' || c == 'X')
				    {
					    c = * ++ input_line_pointer;
					    radix = 16;
				    }
				else
				    {
					    radix = 8;
				    }
			}
		    else
			{
				radix = 10;
			}
		    /*
		     * This feature (?) is here to stop people worrying about
		     * mysterious zero constants: which is what they get when
		     * they completely omit digits.
		     */
		    if (hex_value[c] >= radix) {
			    as_bad("Missing digits. 0 assumed.");
		    }
		    bignum_high = bignum_low - 1; /* Start constant with 0 chars. */
		    for (; (digit = hex_value[c]) < radix; c = *++input_line_pointer)
			{
				/* Multiply existing number by radix, then add digit. */
				carry = digit;
				for (p=bignum_low;   p <= bignum_high;   p++)
				    {
					    work = (*p & MASK_CHAR) * radix + carry;
					    *p = work & MASK_CHAR;
					    carry = work >> BITS_PER_CHAR;
				    }
				if (carry)
				    {
					    grow_bignum();
					    * bignum_high = carry & MASK_CHAR;
					    know((carry & ~ MASK_CHAR) == 0);
				    }
			}
		    length = bignum_high - bignum_low + 1;
		    if (length > nbytes)
			{
				as_warn("Most significant bits truncated in integer constant.");
			}
		    else
			{
				register long leading_zeroes;
				
				for (leading_zeroes = nbytes - length;
				    leading_zeroes;
				    leading_zeroes --)
				    {
					    grow_bignum();
					    * bignum_high = 0;
				    }
			}
		    if (! need_pass_2)
			{
				p = frag_more (nbytes);
				memcpy(p, bignum_low, (int) nbytes);
			}
		    /* C contains character after number. */
		    SKIP_WHITESPACE();
		    c = *input_line_pointer;
		    /* C contains 1st non-blank character after number. */
	    }
	demand_empty_rest_of_line();
} /* big_cons() */

/* Extend bignum by 1 char. */
static void grow_bignum() {
	register long length;
	
	bignum_high ++;
	if (bignum_high >= bignum_limit)
	    {
		    length = bignum_limit - bignum_low;
		    bignum_low = xrealloc(bignum_low, length + length);
		    bignum_high = bignum_low + length;
		    bignum_limit = bignum_low + length + length;
	    }
} /* grow_bignum(); */

/*
 *			float_cons()
 *
 * CONStruct some more frag chars of .floats .ffloats etc.
 * Makes 0 or more new frags.
 * If need_pass_2 == 1, no frags are emitted.
 * This understands only floating literals, not expressions. Sorry.
 *
 * A floating constant is defined by atof_generic(), except it is preceded
 * by 0d 0f 0g or 0h. After observing the STRANGE way my BSD AS does its
 * reading, I decided to be incompatible. This always tries to give you
 * rounded bits to the precision of the pseudo-op. Former AS did premature
 * truncatation, restored noisy bits instead of trailing 0s AND gave you
 * a choice of 2 flavours of noise according to which of 2 floating-point
 * scanners you directed AS to use.
 *
 * In:	input_line_pointer->whitespace before, or '0' of flonum.
 *
 */

void	/* JF was static, but can't be if VAX.C is goning to use it */
    float_cons(float_type)		/* Worker to do .float etc statements. */
/* Clobbers input_line-pointer, checks end-of-line. */
register int float_type;	/* 'f':.ffloat ... 'F':.float ... */
{
	register char *	p;
	register char c;
	int length;	/* Number of chars in an object. */
	register char *	err;	/* Error from scanning floating literal. */
	char temp[MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT];
	
	/*
	 * The following awkward logic is to parse ZERO or more strings,
	 * comma seperated. Recall an expression includes its leading &
	 * trailing blanks. We fake a leading ',' if there is (supposed to
	 * be) a 1st expression, and keep demanding 1 expression for each ','.
	 */
	if (is_it_end_of_statement())
	    {
		    c = 0;			/* Skip loop. */
		    ++ input_line_pointer;	/*->past termintor. */
	    }
	else
	    {
		    c = ',';			/* Do loop. */
	    }
	while (c == ',') {
		/* input_line_pointer->1st char of a flonum (we hope!). */
		SKIP_WHITESPACE();
		/* Skip any 0{letter} that may be present. Don't even check if the
		 * letter is legal. Someone may invent a "z" format and this routine
		 * has no use for such information. Lusers beware: you get
		 * diagnostics if your input is ill-conditioned.
		 */
		
		if (input_line_pointer[0] == '0' && isalpha(input_line_pointer[1]))
		    input_line_pointer+=2;
		
		err = md_atof (float_type, temp, &length);
		know(length <= MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT);
		know(length > 0);
		if (* err) {
			as_bad("Bad floating literal: %s", err);
			ignore_rest_of_line();
			/* Input_line_pointer->just after end-of-line. */
			c = 0;		/* Break out of loop. */
		} else {
			if (! need_pass_2) {
				p = frag_more (length);
				memcpy(p, temp, length);
			}
			SKIP_WHITESPACE();
			c = *input_line_pointer++;
			/* C contains 1st non-white character after number. */
			/* input_line_pointer->just after terminator (c). */
		}
	}
	--input_line_pointer; /*->terminator (is not ','). */
	demand_empty_rest_of_line();
} /* float_cons() */

/*
 * stringer() Worker to do .ascii etc statements.  Checks end-of-line.
 *
 * We read 0 or more ',' seperated, double-quoted strings.
 *
 * Caller should have checked need_pass_2 is FALSE because we don't check it.
 */

void stringer(append_zero)
int append_zero; /* 0: don't append '\0', else 1 */
{
	unsigned int c;
	
	/*
	 * The following awkward logic is to parse ZERO or more strings,
	 * comma seperated. Recall a string expression includes spaces
	 * before the opening '\"' and spaces after the closing '\"'.
	 * We fake a leading ',' if there is (supposed to be)
	 * a 1st, expression. We keep demanding expressions for each
	 * ','.
	 */
	if (is_it_end_of_statement()) {
		c = 0; /* Skip loop. */
		++ input_line_pointer; /* Compensate for end of loop. */
	} else {
		c = ','; /* Do loop. */
	}
	
	while (c == ',' || c == '<' || c == '"' || ('0' <= c && c <= '9')) {
		int i;

		SKIP_WHITESPACE();
		switch (*input_line_pointer) {
		case  '\"':
			++input_line_pointer; /* ->1st char of string. */
			while (is_a_char(c = next_char_of_string())) {
				FRAG_APPEND_1_CHAR(c);
			}
			if (append_zero) {
				FRAG_APPEND_1_CHAR(0);
			}
			know(input_line_pointer[-1] == '\"');
			break;

		case '<':
			input_line_pointer++;
			c = get_single_number();
			FRAG_APPEND_1_CHAR(c);
			if (*input_line_pointer != '>') {
				as_bad("Expected <nn>");
			}
			input_line_pointer++;
			break;

		case ',':
			input_line_pointer++;
			break;

		default:
			i = get_absolute_expression();
			FRAG_APPEND_1_CHAR(i);
			break;
		} /* switch on next char */

		SKIP_WHITESPACE();
		c = *input_line_pointer;
	}
	
	demand_empty_rest_of_line();
} /* stringer() */

/* FIXME-SOMEDAY: I had trouble here on characters with the
   high bits set.  We'll probably also have trouble with
   multibyte chars, wide chars, etc.  Also be careful about
   returning values bigger than 1 byte.  xoxorich. */

unsigned int next_char_of_string() {
	register unsigned int c;
	
	c = *input_line_pointer++ & CHAR_MASK;
	switch (c) {
	case '\"':
		c = NOT_A_CHAR;
		break;
		
	case '\\':
		switch (c = *input_line_pointer++) {
		case 'b':
			c = '\b';
			break;
			
		case 'f':
			c = '\f';
			break;
			
		case 'n':
			c = '\n';
			break;
			
		case 'r':
			c = '\r';
			break;
			
		case 't':
			c = '\t';
			break;
			
#ifdef BACKSLASH_V
		case 'v':
			c = '\013';
			break;
#endif
			
		case '\\':
		case '"':
			break;		/* As itself. */
			
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9': {
			long number;
			
			for (number = 0; isdigit(c); c = *input_line_pointer++) {
				number = number * 8 + c - '0';
			}
			c = number & 0xff;
		}
			--input_line_pointer;
			break;
			
		case '\n':
			/* To be compatible with BSD 4.2 as: give the luser a linefeed!! */
			as_warn("Unterminated string: Newline inserted.");
			c = '\n';
			break;
			
		default:
			
#ifdef ONLY_STANDARD_ESCAPES
			as_bad("Bad escaped character in string, '?' assumed");
			c = '?';
#endif /* ONLY_STANDARD_ESCAPES */
			
			break;
		} /* switch on escaped char */
		break;
		
	default:
		break;
	} /* switch on char */
	return(c);
} /* next_char_of_string() */

static segT
    get_segmented_expression (expP)
register expressionS *	expP;
{
	register segT		retval;
	
	if ((retval = expression(expP)) == SEG_PASS1 || retval == SEG_ABSENT || retval == SEG_BIG)
	    {
		    as_bad("Expected address expression: absolute 0 assumed");
		    retval = expP->X_seg = SEG_ABSOLUTE;
		    expP->X_add_number   = 0;
		    expP->X_add_symbol   = expP->X_subtract_symbol = 0;
	    }
	return (retval);		/* SEG_ ABSOLUTE,UNKNOWN,DATA,TEXT,BSS */
}

static segT get_known_segmented_expression(expP)
register expressionS *expP;
{
	register segT		retval;
	register char *	name1;
	register char *	name2;
	
	if ((retval = get_segmented_expression (expP)) == SEG_UNKNOWN)
	    {
		    name1 = expP->X_add_symbol ? S_GET_NAME(expP->X_add_symbol) : "";
		    name2 = expP->X_subtract_symbol ?
			S_GET_NAME(expP->X_subtract_symbol) :
			    "";
		    if (name1 && name2)
			{
				as_warn("Symbols \"%s\" \"%s\" are undefined: absolute 0 assumed.",
					name1, name2);
			}
		    else
			{
				as_warn("Symbol \"%s\" undefined: absolute 0 assumed.",
					name1 ? name1 : name2);
			}
		    retval = expP->X_seg = SEG_ABSOLUTE;
		    expP->X_add_number   = 0;
		    expP->X_add_symbol   = expP->X_subtract_symbol = NULL;
	    }
#ifndef MANY_SEGMENTS
	know(retval == SEG_ABSOLUTE || retval == SEG_DATA || retval == SEG_TEXT || retval == SEG_BSS || retval == SEG_DIFFERENCE);
#endif
	return (retval);
	
}				/* get_known_segmented_expression() */



/* static */ long /* JF was static, but can't be if the MD pseudos are to use it */
    get_absolute_expression()
{
	expressionS exp;
	register segT s;
	
	if ((s = expression(&exp)) != SEG_ABSOLUTE) {
		if (s != SEG_ABSENT) {
			as_bad("Bad Absolute Expression, absolute 0 assumed.");
		}
		exp.X_add_number = 0;
	}
	return(exp.X_add_number);
} /* get_absolute_expression() */

char /* return terminator */
    get_absolute_expression_and_terminator(val_pointer)
long *		val_pointer; /* return value of expression */
{
	*val_pointer = get_absolute_expression();
	return (*input_line_pointer++);
}

/*
 *			demand_copy_C_string()
 *
 * Like demand_copy_string, but return NULL if the string contains any '\0's.
 * Give a warning if that happens.
 */
char *
    demand_copy_C_string (len_pointer)
int *len_pointer;
{
	register char *s;
	
	if ((s = demand_copy_string(len_pointer)) != 0) {
		register int len;
		
		for (len = *len_pointer;
		     len > 0;
		     len--) {
			if (*s == 0) {
				s = 0;
				len = 1;
				*len_pointer = 0;
				as_bad("This string may not contain \'\\0\'");
			}
		}
	}
	return(s);
}

/*
 *			demand_copy_string()
 *
 * Demand string, but return a safe (=private) copy of the string.
 * Return NULL if we can't read a string here.
 */
static char *demand_copy_string(lenP)
int *lenP;
{
	register unsigned int c;
	register int len;
	char *retval;
	
	len = 0;
	SKIP_WHITESPACE();
	if (*input_line_pointer == '\"') {
		input_line_pointer++;	/* Skip opening quote. */
		
		while (is_a_char(c = next_char_of_string())) {
			obstack_1grow(&notes, c);
			len ++;
		}
		/* JF this next line is so demand_copy_C_string will return a null
		   termanated string. */
		obstack_1grow(&notes,'\0');
		retval=obstack_finish(&notes);
	} else {
		as_warn("Missing string");
		retval = NULL;
		ignore_rest_of_line();
	}
	*lenP = len;
	return(retval);
} /* demand_copy_string() */

/*
 *		is_it_end_of_statement()
 *
 * In:	Input_line_pointer->next character.
 *
 * Do:	Skip input_line_pointer over all whitespace.
 *
 * Out:	1 if input_line_pointer->end-of-line.
 */
int is_it_end_of_statement() {
	SKIP_WHITESPACE();
	return(is_end_of_line[*input_line_pointer]);
} /* is_it_end_of_statement() */

void equals(sym_name)
char *sym_name;
{
	register symbolS *symbolP; /* symbol we are working with */
	
	input_line_pointer++;
	if (*input_line_pointer == '=')
	    input_line_pointer++;
	
	while (*input_line_pointer == ' ' || *input_line_pointer == '\t')
	    input_line_pointer++;
	
	if (sym_name[0] == '.' && sym_name[1] == '\0') {
		/* Turn '. = mumble' into a .org mumble */
		register segT segment;
		expressionS exp;
		register char *p;
		
		segment = get_known_segmented_expression(& exp);
		if (! need_pass_2) {
			if (segment != now_seg && segment != SEG_ABSOLUTE)
			    as_warn("Illegal segment \"%s\". Segment \"%s\" assumed.",
				    segment_name(segment),
				    segment_name(now_seg));
			p = frag_var(rs_org, 1, 1, (relax_substateT)0, exp.X_add_symbol,
				     exp.X_add_number, (char *)0);
			* p = 0;
		} /* if (ok to make frag) */
	} else {
		symbolP=symbol_find_or_make(sym_name);
		pseudo_set(symbolP);
	}
} /* equals() */

/* .include -- include a file at this point. */

/* ARGSUSED */
void s_include(arg)
int arg;
{
	char *newbuf;
	char *filename;
	int i;
	FILE *try;
	char *path;
	
	filename = demand_copy_string(&i);
	demand_empty_rest_of_line();
	path = xmalloc(i + include_dir_maxlen + 5 /* slop */);
	for (i = 0; i < include_dir_count; i++) {
		strcpy(path, include_dirs[i]);
		strcat(path, "/");
		strcat(path, filename);
		if (0 != (try = fopen(path, "r")))
		    {
			    fclose (try);
			    goto gotit;
		    }
	}
	free(path);
	path = filename;
 gotit:
	/* malloc Storage leak when file is found on path.  FIXME-SOMEDAY. */
	newbuf = input_scrub_include_file (path, input_line_pointer);
	buffer_limit = input_scrub_next_buffer (&input_line_pointer);
} /* s_include() */

void add_include_dir(path)
char *path;
{
	int i;
	
	if (include_dir_count == 0)
	    {
		    include_dirs = (char **)xmalloc (2 * sizeof (*include_dirs));
		    include_dirs[0] = ".";	/* Current dir */
		    include_dir_count = 2;
	    }
	else
	    {
		    include_dir_count++;
		    include_dirs = (char **) realloc(include_dirs,
						     include_dir_count*sizeof (*include_dirs));
	    }
	
	include_dirs[include_dir_count-1] = path;	/* New one */
	
	i = strlen (path);
	if (i > include_dir_maxlen)
	    include_dir_maxlen = i;
} /* add_include_dir() */

void s_ignore(arg)
int arg;
{
	while (!is_end_of_line[*input_line_pointer]) {
		++input_line_pointer;
	}
	++input_line_pointer;
	
	return;
} /* s_ignore() */

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of read.c */
