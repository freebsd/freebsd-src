/* Definitions for data structures callers pass the regex library.
   Copyright (C) 1985, 1989 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


   In other words, you are welcome to use, share and improve this program.
   You are forbidden to forbid anyone else to use, share and improve
   what you give them.   Help stamp out software-hoarding!  */


/* Define number of parens for which we record the beginnings and ends.
   This affects how much space the `struct re_registers' type takes up.  */
#ifndef RE_NREGS
#define RE_NREGS 10
#endif

/* These bits are used in the obscure_syntax variable to choose among
   alternative regexp syntaxes.  */

/* 1 means plain parentheses serve as grouping, and backslash
     parentheses are needed for literal searching.
   0 means backslash-parentheses are grouping, and plain parentheses
     are for literal searching.  */
#define RE_NO_BK_PARENS 1

/* 1 means plain | serves as the "or"-operator, and \| is a literal.
   0 means \| serves as the "or"-operator, and | is a literal.  */
#define RE_NO_BK_VBAR 2

/* 0 means plain + or ? serves as an operator, and \+, \? are literals.
   1 means \+, \? are operators and plain +, ? are literals.  */
#define RE_BK_PLUS_QM 4

/* 1 means | binds tighter than ^ or $.
   0 means the contrary.  */
#define RE_TIGHT_VBAR 8

/* 1 means treat \n as an _OR operator
   0 means treat it as a normal character */
#define RE_NEWLINE_OR 16

/* 0 means that a special characters (such as *, ^, and $) always have
     their special meaning regardless of the surrounding context.
   1 means that special characters may act as normal characters in some
     contexts.  Specifically, this applies to:
	^ - only special at the beginning, or after ( or |
	$ - only special at the end, or before ) or |
	*, +, ? - only special when not after the beginning, (, or | */
#define RE_CONTEXT_INDEP_OPS 32

/* Now define combinations of bits for the standard possibilities.  */
#define RE_SYNTAX_AWK (RE_NO_BK_PARENS | RE_NO_BK_VBAR | RE_CONTEXT_INDEP_OPS)
#define RE_SYNTAX_EGREP (RE_SYNTAX_AWK | RE_NEWLINE_OR)
#define RE_SYNTAX_GREP (RE_BK_PLUS_QM | RE_NEWLINE_OR)
#define RE_SYNTAX_EMACS 0

/* This data structure is used to represent a compiled pattern. */

struct re_pattern_buffer
  {
    char *buffer;	/* Space holding the compiled pattern commands. */
    int allocated;	/* Size of space that  buffer  points to */
    int used;		/* Length of portion of buffer actually occupied */
    char *fastmap;	/* Pointer to fastmap, if any, or zero if none. */
			/* re_search uses the fastmap, if there is one,
			   to skip quickly over totally implausible characters */
    char *translate;	/* Translate table to apply to all characters before comparing.
			   Or zero for no translation.
			   The translation is applied to a pattern when it is compiled
			   and to data when it is matched. */
    char fastmap_accurate;
			/* Set to zero when a new pattern is stored,
			   set to one when the fastmap is updated from it. */
    char can_be_null;   /* Set to one by compiling fastmap
			   if this pattern might match the null string.
			   It does not necessarily match the null string
			   in that case, but if this is zero, it cannot.
			   2 as value means can match null string
			   but at end of range or before a character
			   listed in the fastmap.  */
  };

/* Structure to store "register" contents data in.

   Pass the address of such a structure as an argument to re_match, etc.,
   if you want this information back.

   start[i] and end[i] record the string matched by \( ... \) grouping i,
   for i from 1 to RE_NREGS - 1.
   start[0] and end[0] record the entire string matched. */

struct re_registers
  {
    int start[RE_NREGS];
    int end[RE_NREGS];
  };

/* These are the command codes that appear in compiled regular expressions, one per byte.
  Some command codes are followed by argument bytes.
  A command code can specify any interpretation whatever for its arguments.
  Zero-bytes may appear in the compiled regular expression. */

enum regexpcode
  {
    unused,
    exactn,    /* followed by one byte giving n, and then by n literal bytes */
    begline,   /* fails unless at beginning of line */
    endline,   /* fails unless at end of line */
    jump,	 /* followed by two bytes giving relative address to jump to */
    on_failure_jump,	 /* followed by two bytes giving relative address of place
		            to resume at in case of failure. */
    finalize_jump,	 /* Throw away latest failure point and then jump to address. */
    maybe_finalize_jump, /* Like jump but finalize if safe to do so.
			    This is used to jump back to the beginning
			    of a repeat.  If the command that follows
			    this jump is clearly incompatible with the
			    one at the beginning of the repeat, such that
			    we can be sure that there is no use backtracking
			    out of repetitions already completed,
			    then we finalize. */
    dummy_failure_jump,  /* jump, and push a dummy failure point.
			    This failure point will be thrown away
			    if an attempt is made to use it for a failure.
			    A + construct makes this before the first repeat.  */
    anychar,	 /* matches any one character */
    charset,     /* matches any one char belonging to specified set.
		    First following byte is # bitmap bytes.
		    Then come bytes for a bit-map saying which chars are in.
		    Bits in each byte are ordered low-bit-first.
		    A character is in the set if its bit is 1.
		    A character too large to have a bit in the map
		    is automatically not in the set */
    charset_not, /* similar but match any character that is NOT one of those specified */
    start_memory, /* starts remembering the text that is matched
		    and stores it in a memory register.
		    followed by one byte containing the register number.
		    Register numbers must be in the range 0 through NREGS. */
    stop_memory, /* stops remembering the text that is matched
		    and stores it in a memory register.
		    followed by one byte containing the register number.
		    Register numbers must be in the range 0 through NREGS. */
    duplicate,    /* match a duplicate of something remembered.
		    Followed by one byte containing the index of the memory register. */
    before_dot,	 /* Succeeds if before dot */
    at_dot,	 /* Succeeds if at dot */
    after_dot,	 /* Succeeds if after dot */
    begbuf,      /* Succeeds if at beginning of buffer */
    endbuf,      /* Succeeds if at end of buffer */
    wordchar,    /* Matches any word-constituent character */
    notwordchar, /* Matches any char that is not a word-constituent */
    wordbeg,	 /* Succeeds if at word beginning */
    wordend,	 /* Succeeds if at word end */
    wordbound,   /* Succeeds if at a word boundary */
    notwordbound, /* Succeeds if not at a word boundary */
    syntaxspec,  /* Matches any character whose syntax is specified.
		    followed by a byte which contains a syntax code, Sword or such like */
    notsyntaxspec /* Matches any character whose syntax differs from the specified. */
  };

extern char *re_compile_pattern ();
/* Is this really advertised? */
extern void re_compile_fastmap ();
extern int re_search (), re_search_2 ();
extern int re_match (), re_match_2 ();

/* 4.2 bsd compatibility (yuck) */
extern char *re_comp ();
extern int re_exec ();

#ifdef SYNTAX_TABLE
extern char *re_syntax_table;
#endif
