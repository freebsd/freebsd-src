/* as.h - global header file
   Copyright (C) 1987, 1990, 1991, 1992 Free Software Foundation, Inc.
   
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
 * $Id: as.h,v 1.2 1993/11/03 00:51:11 paul Exp $
 */

#define GAS 1
/* #include <ansidecl.h> */
#include "host.h"
#include "flonum.h"

#if __STDC__ != 1
#define	volatile	/**/
#ifndef const
#define	const		/**/
#endif /* const */
#endif /* __STDC__ */

#ifdef __GNUC__
#define alloca __builtin_alloca
#define register
#endif /* __GNUC__ */

#ifndef __LINE__
#define __LINE__ "unknown"
#endif /* __LINE__ */

#ifndef __FILE__
#define __FILE__ "unknown"
#endif /* __FILE__ */

/*
 * I think this stuff is largely out of date.  xoxorich.
 *
 * CAPITALISED names are #defined.
 * "lowercaseH" is #defined if "lowercase.h" has been #include-d.
 * "lowercaseT" is a typedef of "lowercase" objects.
 * "lowercaseP" is type "pointer to object of type 'lowercase'".
 * "lowercaseS" is typedef struct ... lowercaseS.
 *
 * #define DEBUG to enable all the "know" assertion tests.
 * #define SUSPECT when debugging.
 * #define COMMON as "extern" for all modules except one, where you #define
 *	COMMON as "".
 * If TEST is #defined, then we are testing a module: #define COMMON as "".
 */

/* These #defines are for parameters of entire assembler. */

/* #define SUSPECT JF remove for speed testing */
/* These #includes are for type definitions etc. */

#include <stdio.h>
#include <assert.h>
#include <string.h>

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free xfree

#define xfree free

#define BAD_CASE(value) \
{ \
      as_fatal("Case value %d unexpected at line %d of file \"%s\"\n", \
	       value, __LINE__, __FILE__); \
	   }


/* These are assembler-wide concepts */


#ifndef COMMON
#ifdef TEST
#define COMMON			/* declare our COMMONs storage here. */
#else
#define COMMON extern		/* our commons live elswhere */
#endif
#endif
/* COMMON now defined */
#define DEBUG /* temporary */

#ifdef DEBUG
#undef NDEBUG
#ifndef know
#define know(p) assert(p)	/* Verify our assumptions! */
#endif /* not yet defined */
#else
#define know(p)			/* know() checks are no-op.ed */
#endif

/* input_scrub.c */

/*
 * Supplies sanitised buffers to read.c.
 * Also understands printing line-number part of error messages.
 */


/* subsegs.c     Sub-segments. Also, segment(=expression type)s.*/

/*
 * This table describes the use of segments as EXPRESSION types.
 *
 *	X_seg	X_add_symbol  X_subtract_symbol	X_add_number
 * SEG_ABSENT						no (legal) expression
 * SEG_PASS1						no (defined) "
 * SEG_BIG					*	> 32 bits const.
 * SEG_ABSOLUTE				     	0
 * SEG_DATA		*		     	0
 * SEG_TEXT		*			0
 * SEG_BSS		*			0
 * SEG_UNKNOWN		*			0
 * SEG_DIFFERENCE	0		*	0
 * SEG_REGISTER					*
 *
 * The blank fields MUST be 0, and are nugatory.
 * The '0' fields MAY be 0. The '*' fields MAY NOT be 0.
 *
 * SEG_BIG: X_add_number is < 0 if the result is in
 *	generic_floating_point_number.  The value is -'c' where c is the
 *	character that introduced the constant.  e.g. "0f6.9" will have  -'f'
 *	as a X_add_number value.
 *	X_add_number > 0 is a count of how many littlenums it took to
 *	represent a bignum.
 * SEG_DIFFERENCE:
 * If segments of both symbols are known, they are the same segment.
 * X_add_symbol != X_sub_symbol (then we just cancel them, => SEG_ABSOLUTE).
 */


#ifdef MANY_SEGMENTS
#define N_SEGMENTS 10
#define SEG_NORMAL(x) ((x) >= SEG_E0 && (x) <= SEG_E9)
#define SEG_LIST SEG_E0,SEG_E1,SEG_E2,SEG_E3,SEG_E4,SEG_E5,SEG_E6,SEG_E7,SEG_E8,SEG_E9
#define SEG_DATA SEG_E1
#define SEG_TEXT SEG_E0
#define SEG_BSS SEG_E2
#else
#define N_SEGMENTS 3
#define SEG_NORMAL(x) ((x) == SEG_TEXT || (x) == SEG_DATA || (x) == SEG_BSS)
#define SEG_LIST SEG_TEXT,SEG_DATA,SEG_BSS
#endif

typedef enum _segT {
	SEG_ABSOLUTE = 0,
	SEG_LIST,
	SEG_UNKNOWN,
	SEG_ABSENT,		/* Mythical Segment (absent): NO expression seen. */
	SEG_PASS1,		/* Mythical Segment: Need another pass. */
	SEG_GOOF,		/* Only happens if AS has a logic error. */
	/* Invented so we don't crash printing */
	/* error message involving weird segment. */
	SEG_BIG,		/* Bigger than 32 bits constant. */
	SEG_DIFFERENCE,		/* Mythical Segment: absolute difference. */
	SEG_DEBUG,		/* Debug segment */
	SEG_NTV,		/* Transfert vector preload segment */
	SEG_PTV,		/* Transfert vector postload segment */
	SEG_REGISTER,		/* Mythical: a register-valued expression */
} segT;

#define SEG_MAXIMUM_ORDINAL (SEG_REGISTER)

typedef int subsegT;

COMMON subsegT			now_subseg;
/* What subseg we are accreting now? */


COMMON segT			now_seg;
/* Segment our instructions emit to. */
/* Only OK values are SEG_TEXT or SEG_DATA. */


extern char *const seg_name[];
extern int section_alignment[];


/* relax() */

typedef enum _relax_state {
	rs_fill, /* Variable chars to be repeated fr_offset times. Fr_symbol
		    unused. Used with fr_offset == 0 for a constant length
		    frag. */
	
	rs_align, /* Align: Fr_offset: power of 2. 1 variable char: fill
		     character. */
	
	rs_org,	/* Org: Fr_offset, fr_symbol: address. 1 variable char: fill
		   character. */
	
	rs_machine_dependent,
	
#ifndef WORKING_DOT_WORD
	rs_broken_word,		/* JF: gunpoint */
#endif
} relax_stateT;

/* typedef unsigned char relax_substateT; */
/* JF this is more likely to leave the end of a struct frag on an align
   boundry.  Be very careful with this.  */
typedef unsigned long relax_substateT;

typedef unsigned long relax_addressT;/* Enough bits for address. */
/* Still an integer type. */


/* frags.c */

/*
 * A code fragment (frag) is some known number of chars, followed by some
 * unknown number of chars. Typically the unknown number of chars is an
 * instruction address whose size is yet unknown. We always know the greatest
 * possible size the unknown number of chars may become, and reserve that
 * much room at the end of the frag.
 * Once created, frags do not change address during assembly.
 * We chain the frags in (a) forward-linked list(s). The object-file address
 * of the 1st char of a frag is generally not known until after relax().
 * Many things at assembly time describe an address by {object-file-address
 * of a particular frag}+offset.
 
 BUG: it may be smarter to have a single pointer off to various different
 notes for different frag kinds. See how code pans 
 */
struct frag			/* a code fragment */
{
	unsigned long fr_address; /* Object file address. */
	struct frag *fr_next;	/* Chain forward; ascending address order. */
	/* Rooted in frch_root. */
	
	long fr_fix;	/* (Fixed) number of chars we know we have. */
	/* May be 0. */
	long fr_var;	/* (Variable) number of chars after above. */
	/* May be 0. */
	struct symbol *fr_symbol; /* For variable-length tail. */
	long fr_offset;	/* For variable-length tail. */
	char	*fr_opcode;	/*->opcode low addr byte,for relax()ation*/
	relax_stateT fr_type;   /* What state is my tail in? */
	relax_substateT	fr_subtype;
	/* These are needed only on the NS32K machines */
	char	fr_pcrel_adjust;
	char	fr_bsr;
#ifndef NO_LISTING
	struct list_info_struct *line;
#endif
	char	fr_literal[1];	/* Chars begin here. */
	/* One day we will compile fr_literal[0]. */
};
#define SIZEOF_STRUCT_FRAG \
((int)zero_address_frag.fr_literal-(int)&zero_address_frag)
/* We want to say fr_literal[0] above. */

typedef struct frag fragS;

COMMON fragS *frag_now;	/* -> current frag we are building. */
/* This frag is incomplete. */
/* It is, however, included in frchain_now. */
/* Frag_now->fr_fix is bogus. Use: */
/* Virtual frag_now->fr_fix == obstack_next_free(&frags)-frag_now->fr_literal.*/

COMMON fragS zero_address_frag;	/* For foreign-segment symbol fixups. */
COMMON fragS  bss_address_frag;	/* For local common (N_BSS segment) fixups. */

/* main program "as.c" (command arguments etc) */

COMMON char
    flagseen[128];			/* ['x'] TRUE if "-x" seen. */

COMMON char *
    out_file_name;			/* name of emitted object file */

COMMON int	need_pass_2;	/* TRUE if we need a second pass. */

typedef struct {
	char *	poc_name;	/* assembler mnemonic, lower case, no '.' */
	void		(*poc_handler)();	/* Do the work */
	int		poc_val;	/* Value to pass to handler */
} pseudo_typeS;

#if (__STDC__ == 1) & !defined(NO_STDARG)

int had_errors(void);
int had_warnings(void);
void as_bad(const char *Format, ...);
void as_fatal(const char *Format, ...);
void as_tsktsk(const char *Format, ...);
void as_warn(const char *Format, ...);

#else

int had_errors();
int had_warnings();
void as_bad();
void as_fatal();
void as_tsktsk();
void as_warn();

#endif /* __STDC__ & !NO_STDARG */

#if __STDC__ == 1

char *app_push(void);
char *atof_ieee(char *str, int what_kind, LITTLENUM_TYPE *words);
char *input_scrub_include_file(char *filename, char *position);
char *input_scrub_new_file(char *filename);
char *input_scrub_next_buffer(char **bufp);
char *strstr(const char *s, const char *wanted);
char *xmalloc(int size);
char *xrealloc(char *ptr, long n);
int do_scrub_next_char(int (*get)(), void (*unget)());
int gen_to_words(LITTLENUM_TYPE *words, int precision, long exponent_bits);
int had_err(void);
int had_errors(void);
int had_warnings(void);
int ignore_input(void);
int scrub_from_file(void);
int scrub_from_file(void);
int scrub_from_string(void);
int seen_at_least_1_file(void);
void app_pop(char *arg);
void as_howmuch(FILE *stream);
void as_perror(char *gripe, char *filename);
void as_where(void);
void bump_line_counters(void);
void do_scrub_begin(void);
void input_scrub_begin(void);
void input_scrub_close(void);
void input_scrub_end(void);
void int_to_gen(long x);
void new_logical_line(char *fname, int line_number);
void scrub_to_file(int ch);
void scrub_to_string(int ch);
void subseg_change(segT seg, int subseg);
void subseg_new(segT seg, subsegT subseg);
void subsegs_begin(void);

#else /* not __STDC__ */

char *app_push();
char *atof_ieee();
char *input_scrub_include_file();
char *input_scrub_new_file();
char *input_scrub_next_buffer();
char *strstr();
char *xmalloc();
char *xrealloc();
int do_scrub_next_char();
int gen_to_words();
int had_err();
int had_errors();
int had_warnings();
int ignore_input();
int scrub_from_file();
int scrub_from_file();
int scrub_from_string();
int seen_at_least_1_file();
void app_pop();
void as_howmuch();
void as_perror();
void as_where();
void bump_line_counters();
void do_scrub_begin();
void input_scrub_begin();
void input_scrub_close();
void input_scrub_end();
void int_to_gen();
void new_logical_line();
void scrub_to_file();
void scrub_to_string();
void subseg_change();
void subseg_new();
void subsegs_begin();

#endif /* not __STDC__ */

/* this one starts the chain of target dependant headers */
#include "targ-env.h"

/* these define types needed by the interfaces */
#include "struc-symbol.h"
#include "write.h"
#include "expr.h"
#include "frags.h"
#include "hash.h"
#include "read.h"
#include "symbols.h"

#include "tc.h"
#include "obj.h"

#include "listing.h"

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of as.h */
