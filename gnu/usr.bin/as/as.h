/* as.h - global header file
   Copyright (C) 1987 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef asH
#define asH			/* Don't declare things twice. */

#if !defined(__STDC__) && !defined(const)
#define const /* ignore */
#endif

#ifdef USG
#define index strchr
#define bzero(s,n) memset((s),0,(n))
#define bcopy(from,to,n) memcpy((to),(from),(n))
#define setbuffer(a,b,c)
#endif

/*
 * CAPITALISED names are #defined.
 * "lowercaseH" is #defined if "lowercase.h" has been #include-d.
 * "lowercaseT" is a typedef of "lowercase" objects.
 * "lowercaseP" is type "pointer to object of type 'lowercase'".
 * "lowercaseS" is typedef struct ... lowercaseS.
 *
 * #define SUSPECT when debugging.
 * #define DUMP to include data-structure dumpers.
 * #define COMMON as "extern" for all modules except one, where you #define
 *	COMMON as "".
 * If TEST is #defined, then we are testing a module: #define COMMON as "".
 */



/* These #defines are for parameters of entire assembler. */

/* #define SUSPECT JF remove for speed testing */
/* #define DUMP */
#define NDEBUG		/* JF disable asserts */
/* These #includes are for type definitions etc. */

/* #include "style.h" */
#include <stdio.h>
#include <assert.h>
#define obstack_chunk_alloc	xmalloc
#define obstack_chunk_free	xfree

/* These defines are potentially useful */
#define FALSE	(0)
#define TRUE	(!FALSE)
#define ASSERT	assert
#define BAD_CASE(value)							\
{									\
  as_fatal ("Case value %d unexpected at line %d of file \"%s\"\n",	\
	   value, __LINE__, __FILE__);					\
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

#ifdef SUSPECT
#define register		/* no registers: helps debugging */
#define know(p) ASSERT(p)	/* know() is less ugly than #ifdef SUSPECT/ */
				/* assert()/#endif. */
#else
#define know(p)			/* know() checks are no-op.ed */
#endif				/* #ifdef SUSPECT */


char	*xmalloc();		/* keep C compilers happy */
char	*xrealloc();		/* " */
void	free();			/* " */
#define xfree free

/* input_scrub.c */

/*
 * Supplies sanitised buffers to read.c.
 * Also understands printing line-number part of error messages.
 */

				/* Line number things. */
int	seen_at_least_1_file();
void	bump_line_counters();
void	new_logical_line();
void	as_where();
void	as_perror();
void	as_howmuch();
				/* Sanitising things. */
void	input_scrub_begin();
void	input_scrub_end();
char	*input_scrub_new_file();
char	*input_scrub_next_buffer();

/* subsegs.c     Sub-segments. Also, segment(=expression type)s.*/

/*
 * This table describes the use of segments as EXPRESSION types.
 *
 *	X_seg	X_add_symbol  X_subtract_symbol	X_add_number
 * SEG_NONE						no (legal) expression
 * SEG_PASS1						no (defined) "
 * SEG_BIG					*	> 32 bits const.
 * SEG_ABSOLUTE				     	0
 * SEG_DATA		*		     	0
 * SEG_TEXT		*			0
 * SEG_BSS		*			0
 * SEG_UNKNOWN		*			0
 * SEG_DIFFERENCE	0		*	0
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

typedef enum
{
	SEG_ABSOLUTE,
	SEG_TEXT,
	SEG_DATA,
	SEG_BSS,
	SEG_UNKNOWN,
	SEG_NONE,		/* Mythical Segment: NO expression seen. */
	SEG_PASS1,		/* Mythical Segment: Need another pass. */
	SEG_GOOF,		/* Only happens if AS has a logic error. */
				/* Invented so we don't crash printing */
				/* error message involving weird segment. */
	SEG_BIG,			/* Bigger than 32 bits constant. */
	SEG_DIFFERENCE		/* Mythical Segment: absolute difference. */
}		segT;
#define SEG_MAXIMUM_ORDINAL (SEG_DIFFERENCE)

typedef unsigned char	subsegT;

COMMON subsegT			now_subseg;
				/* What subseg we are accreting now? */


COMMON segT			now_seg;
				/* Segment our instructions emit to. */
				/* Only OK values are SEG_TEXT or SEG_DATA. */


extern char *const seg_name[];
extern const int   seg_N_TYPE[];
extern const segT  N_TYPE_seg[];
void	subsegs_begin();
void	subseg_change();
void	subseg_new();

/* relax() */

typedef enum
{
	rs_fill,		/* Variable chars to be repeated fr_offset */
				/* times. Fr_symbol unused. */
				/* Used with fr_offset == 0 for a constant */
				/* length frag. */

	rs_align,		/* Align: Fr_offset: power of 2. */
				/* 1 variable char: fill character. */
	rs_org,			/* Org: Fr_offset, fr_symbol: address. */
				/* 1 variable char: fill character. */

	rs_machine_dependent,
#ifndef WORKING_DOT_WORD
	rs_broken_word,		/* JF: gunpoint */
#endif
}
relax_stateT;

/* typedef unsigned char relax_substateT; */
/* JF this is more likely to leave the end of a struct frag on an align
   boundry.  Be very careful with this.  */
typedef unsigned long int relax_substateT;

typedef unsigned long int relax_addressT;/* Enough bits for address. */
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
notes for different frag kinds. See how code pans out.


 */
struct frag			/* a code fragment */
{
	long unsigned int fr_address; /* Object file address. */
	struct frag *fr_next;	/* Chain forward; ascending address order. */
				/* Rooted in frch_root. */

	long int fr_fix;	/* (Fixed) number of chars we know we have. */
				/* May be 0. */
	long int fr_var;	/* (Variable) number of chars after above. */
				/* May be 0. */
	struct symbol *fr_symbol; /* For variable-length tail. */
	long int fr_offset;	/* For variable-length tail. */
	char	*fr_opcode;	/*->opcode low addr byte,for relax()ation*/
	relax_stateT fr_type;   /* What state is my tail in? */
	relax_substateT	fr_subtype;
		/* These are needed only on the NS32K machines */
	char	fr_pcrel_adjust;
	char	fr_bsr;
	char	fr_literal [1];	/* Chars begin here. */
				/* One day we will compile fr_literal[0]. */
};
#define SIZEOF_STRUCT_FRAG \
 ((int)zero_address_frag.fr_literal-(int)&zero_address_frag)
				/* We want to say fr_literal[0] above. */

typedef struct frag fragS;

COMMON fragS *	frag_now;	/* -> current frag we are building. */
				/* This frag is incomplete. */
				/* It is, however, included in frchain_now. */
				/* Frag_now->fr_fix is bogus. Use: */
/* Virtual frag_now->fr_fix==obstack_next_free(&frags)-frag_now->fr_literal.*/

COMMON fragS zero_address_frag;	/* For foreign-segment symbol fixups. */
COMMON fragS  bss_address_frag;	/* For local common (N_BSS segment) fixups. */

void		frag_new();
char *		frag_more();
char *		frag_var();
void		frag_wane();
void		frag_align();


/* main program "as.c" (command arguments etc) */

COMMON char
flagseen[128];			/* ['x'] TRUE if "-x" seen. */

COMMON char *
out_file_name;			/* name of emitted object file */

COMMON int	need_pass_2;	/* TRUE if we need a second pass. */


#endif				/* #ifdef asH */

/* end: as.h */
