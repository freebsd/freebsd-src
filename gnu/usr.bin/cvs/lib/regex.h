/* Definitions for data structures and routines for the regular
   expression library, version REPLACE-WITH-VERSION.

   Copyright (C) 1985, 1989, 1990, 1991, 1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef __REGEXP_LIBRARY_H__
#define __REGEXP_LIBRARY_H__

/* POSIX says that <sys/types.h> must be included before <regex.h>.  */

/* The following bits are used to determine the regexp syntax we
   recognize.  The set/not-set meanings are chosen so that Emacs syntax
   remains the value 0.  The bits are given in alphabetical order, and
   the definitions shifted by one from the previous bit; thus, when we
   add or remove a bit, only one other definition need change.  */
typedef unsigned reg_syntax_t;

/* If this bit is not set, then \ inside a bracket expression is literal.
   If set, then such a \ quotes the following character.  */
#define RE_BACKSLASH_ESCAPE_IN_LISTS (1)

/* If this bit is not set, then + and ? are operators, and \+ and \? are
     literals. 
   If set, then \+ and \? are operators and + and ? are literals.  */
#define RE_BK_PLUS_QM (RE_BACKSLASH_ESCAPE_IN_LISTS << 1)

/* If this bit is set, then character classes are supported.  They are:
     [:alpha:], [:upper:], [:lower:],  [:digit:], [:alnum:], [:xdigit:],
     [:space:], [:print:], [:punct:], [:graph:], and [:cntrl:].
   If not set, then character classes are not supported.  */
#define RE_CHAR_CLASSES (RE_BK_PLUS_QM << 1)

/* If this bit is set, then ^ and $ are always anchors (outside bracket
     expressions).
   If this bit is not set, then it depends:
        ^  is an anchor if it is at the beginning of a regular
           expression or after an open-group or an alternation operator;
        $  is an anchor if it is at the end of a regular expression, or
           before a close-group or an alternation operator.  
   This bit could be (re)combined with RE_CONTEXT_INDEP_OPS, because
   POSIX now says that the behavior of * etc. in leading positions is
   undefined.  We have already implemented a previous draft which
   made those constructs invalid, so we may as well not change the code
   back.  */
#define RE_CONTEXT_INDEP_ANCHORS (RE_CHAR_CLASSES << 1)

/* If this bit is set, then special characters are always special
     regardless of where they are in the pattern.
   If this bit is not set, then special characters are special only in
     some contexts; otherwise they are ordinary.  Specifically, 
     * + ? and intervals are only special when not after the beginning,
     open-group, or alternation operator.  */
#define RE_CONTEXT_INDEP_OPS (RE_CONTEXT_INDEP_ANCHORS << 1)

/* If this bit is set, then *, +, ?, and { cannot be first in an re or
   immediately after an alternation or begin-group operator.
   Furthermore, alternation cannot be first or last in an re, or
   immediately follow another alternation or begin-group.  */
#define RE_CONTEXT_INVALID_OPS (RE_CONTEXT_INDEP_OPS << 1)

/* If this bit is set, then . matches a newline.
   If not set, then it doesn't.  */
#define RE_DOT_NEWLINE (RE_CONTEXT_INVALID_OPS << 1)

/* If this bit is set, then period doesn't match a null.
   If not set, then it does.  */
#define RE_DOT_NOT_NULL (RE_DOT_NEWLINE << 1)

/* If this bit is set, nonmatching lists [^...] do not match newline.
   If not set, they do.  */
#define RE_HAT_LISTS_NOT_NEWLINE (RE_DOT_NOT_NULL << 1)

/* If this bit is set, either \{...\} or {...} defines an
     interval, depending on RE_NO_BK_BRACES. 
   If not set, \{, \}, {, and } are literals.  */
#define RE_INTERVALS (RE_HAT_LISTS_NOT_NEWLINE << 1)

/* If this bit is set,  +, ? and | aren't recognized as operators.
   If not set, they are.  */
#define RE_LIMITED_OPS (RE_INTERVALS << 1)

/* If this bit is set, newline is an alternation operator.
   If not set, newline is literal.  */
#define RE_NEWLINE_ALT (RE_LIMITED_OPS << 1)

/* If this bit is set, newline in the pattern is an ordinary character.
   If not set, newline before ^ or after $ allows the ^ or $ to be an
     anchor.  */
#define RE_NEWLINE_ORDINARY (RE_NEWLINE_ALT << 1)

/* If this bit is not set, then \{ and \} defines an interval,
     and { and } are literals.
   If set, then { and } defines an interval, and \{ and \} are literals.  */
#define RE_NO_BK_BRACES (RE_NEWLINE_ORDINARY << 1)

/* If this bit is set, (...) defines a group, and \( and \) are literals.
   If not set, \(...\) defines a group, and ( and ) are literals.  */
#define RE_NO_BK_PARENS (RE_NO_BK_BRACES << 1)

/* If this bit is set, then back references (i.e., \<digit>) are not
     recognized.
   If not set, then they are.  */
#define RE_NO_BK_REFS (RE_NO_BK_PARENS << 1)

/* If this bit is set, then | is an alternation operator, and \| is literal. 
   If not set, then \| is an alternation operator, and | is literal.  */
#define RE_NO_BK_VBAR (RE_NO_BK_REFS << 1)

/* If this bit is set, then you can't have empty alternatives.
   If not set, then you can.  */
#define RE_NO_EMPTY_ALTS (RE_NO_BK_VBAR << 1)

/* If this bit is set, then you can't have empty groups.
   If not set, then you can.  */
#define RE_NO_EMPTY_GROUPS (RE_NO_EMPTY_ALTS << 1)

/* If this bit is set, then an ending range point has to collate higher
     than or equal to the starting range point.
   If not set, then when the ending range point collates higher than the
     starting range point, we consider such a range to be empty.  */ 
#define RE_NO_EMPTY_RANGES (RE_NO_EMPTY_GROUPS << 1)

/* If this bit is set, then all back references must refer to a preceding
     subexpression.
   If not set, then a back reference to a nonexistent subexpression is
     treated as literal characters.  */ 
#define RE_NO_MISSING_BK_REF (RE_NO_EMPTY_RANGES << 1)

/* If this bit is set, then Regex considers an unmatched close-group
     operator to be the ordinary character parenthesis.
   If not set, then an unmatched close-group operator is invalid.  */
#define RE_UNMATCHED_RIGHT_PAREN_ORD (RE_NO_MISSING_BK_REF << 1)

/* This global variable defines the particular regexp syntax to use (for
   some interfaces).  When a regexp is compiled, the syntax used is
   stored in the pattern buffer, so changing this does not affect
   already-compiled regexps.  */
extern reg_syntax_t obscure_syntax;



/* Define combinations of the above bits for the standard possibilities.
   (The [[[ comments delimit what gets put into the Texinfo file.) */ 
/* [[[begin syntaxes]]] */
#define RE_SYNTAX_EMACS 0

#define RE_SYNTAX_POSIX_AWK 						\
  (RE_CONTEXT_INDEP_ANCHORS | RE_CONTEXT_INDEP_OPS | RE_NO_BK_PARENS	\
   | RE_NO_BK_VBAR) 

#define RE_SYNTAX_AWK							\
  (RE_BACKSLASH_ESCAPE_IN_LISTS | RE_SYNTAX_POSIX_AWK)

#define RE_SYNTAX_GREP							\
  (RE_BK_PLUS_QM | RE_NEWLINE_ALT)

#define RE_SYNTAX_EGREP							\
  (RE_CONTEXT_INDEP_ANCHORS | RE_CONTEXT_INDEP_OPS			\
   | RE_NEWLINE_ALT | RE_NO_BK_PARENS | RE_NO_BK_VBAR)

#define RE_SYNTAX_POSIX_BASIC						\
  (RE_CHAR_CLASSES | RE_DOT_NEWLINE					\
   | RE_DOT_NOT_NULL | RE_INTERVALS | RE_LIMITED_OPS			\
   | RE_NEWLINE_ORDINARY | RE_NO_EMPTY_RANGES | RE_NO_MISSING_BK_REF)

#define RE_SYNTAX_POSIX_EXTENDED					\
  (RE_CHAR_CLASSES | RE_CONTEXT_INDEP_ANCHORS				\
   | RE_CONTEXT_INVALID_OPS | RE_DOT_NEWLINE | RE_DOT_NOT_NULL		\
   | RE_INTERVALS | RE_NEWLINE_ORDINARY | RE_NO_BK_BRACES		\
   | RE_NO_BK_PARENS | RE_NO_BK_REFS | RE_NO_BK_VBAR			\
   | RE_NO_EMPTY_ALTS | RE_NO_EMPTY_GROUPS | RE_NO_EMPTY_RANGES		\
   | RE_UNMATCHED_RIGHT_PAREN_ORD)
/* [[[end syntaxes]]] */




/* Maximum number of duplicates an interval can allow.  */
#undef RE_DUP_MAX
#define RE_DUP_MAX  ((1 << 15) - 1) 


/* POSIX `cflags' bits (i.e., information for regcomp).  */

/* If this bit is set, then use extended regular expression syntax.
   If not set, then use basic regular expression syntax.  */
#define REG_EXTENDED 1

/* If this bit is set, then ignore case when matching.
   If not set, then case is significant.  */
#define REG_ICASE (REG_EXTENDED << 1)
 
/* If this bit is set, then anchors do not match at newline
     characters in the string.
   If not set, then anchors do match at newlines.  */
#define REG_NEWLINE (REG_ICASE << 1)

/* If this bit is set, then report only success or fail in regexec.
   If not set, then returns differ between not matching and errors.  */
#define REG_NOSUB (REG_NEWLINE << 1)


/* POSIX `eflags' bits (i.e., information for regexec).  */

/* If this bit is set, then the beginning-of-line operator doesn't match
     the beginning of the string (presumably because it's not the
     beginning of a line).
   If not set, then the beginning-of-line operator does match the
     beginning of the string.  */
#define REG_NOTBOL 1

/* Like REG_NOTBOL, except for the end-of-line.  */
#define REG_NOTEOL (1 << 1)


/* If any error codes are removed, changed, or added, update the
   `re_error_msg' table in regex.c.  */
typedef enum
{
  REG_NOERROR = 0,	/* Success.  */
  REG_NOMATCH,		/* Didn't find a match (for regexec).  */

  /* POSIX regcomp return error codes.  (In the order listed in the
     standard.)  */
  REG_BADPAT,		/* Invalid pattern.  */
  REG_ECOLLATE,		/* Not implemented.  */
  REG_ECTYPE,		/* Invalid character class name.  */
  REG_EESCAPE,		/* Trailing backslash.  */
  REG_ESUBREG,		/* Invalid back reference.  */
  REG_EBRACK,		/* Unmatched left bracket.  */
  REG_EPAREN,		/* Parenthesis imbalance.  */ 
  REG_EBRACE,		/* Unmatched \{.  */
  REG_BADBR,		/* Invalid contents of \{\}.  */
  REG_ERANGE,		/* Invalid range end.  */
  REG_ESPACE,		/* Ran out of memory.  */
  REG_BADRPT,		/* No preceding re for repetition op.  */

  /* Error codes we've added.  */
  REG_EEND,		/* Premature end.  */
  REG_ESIZE,		/* Compiled pattern bigger than 2^16 bytes.  */
  REG_ERPAREN		/* Unmatched ) or \); not returned from regcomp.  */
} reg_errcode_t;




/* This data structure represents a compiled pattern.  Before calling
   the pattern compiler, the fields `buffer', `allocated', `fastmap',
   `translate', and `no_sub' can be set.  After the pattern has been
   compiled, the `re_nsub' field is available.  All other fields are
   private to the regex routines.  */

struct re_pattern_buffer
{
/* [[[begin pattern_buffer]]] */
	/* Space that holds the compiled pattern.  It is declared as
          `unsigned char *' because its elements are
           sometimes used as array indexes.  */
  unsigned char *buffer;

	/* Number of bytes to which `buffer' points.  */
  unsigned long allocated;

	/* Number of bytes actually used in `buffer'.  */
  unsigned long used;	

        /* Syntax setting with which the pattern was compiled.  */
  reg_syntax_t syntax;

        /* Pointer to a fastmap, if any, otherwise zero.  re_search uses
           the fastmap, if there is one, to skip over impossible
           starting points for matches.  */
  char *fastmap;

        /* Either a translate table to apply to all characters before
           comparing them, or zero for no translation.  The translation
           is applied to a pattern when it is compiled and to a string
           when it is matched.  */
  char *translate;

	/* Number of subexpressions found by the compiler.  */
  size_t re_nsub;

        /* Set to 1 by re_compile_fastmap if this pattern can match the
           null string; 0 prevents the searcher from matching it with
           the null string.  Set to 2 if it might match the null string
           either at the end of a search range or just before a
           character listed in the fastmap.  */
  unsigned can_be_null : 2;

        /* Set to zero when regex_compile compiles a pattern; set to one
           by re_compile_fastmap when it updates the fastmap, if any.  */
  unsigned fastmap_accurate : 1;

        /* If set, regexec reports only success or failure and does not
           return anything in pmatch.  */
  unsigned no_sub : 1;

        /* If set, a beginning-of-line anchor doesn't match at the
           beginning of the string.  */ 
  unsigned not_bol : 1;

        /* Similarly for an end-of-line anchor.  */
  unsigned not_eol : 1;

        /* If true, an anchor at a newline matches.  */
  unsigned newline_anchor : 1;

        /* If set, re_match_2 assumes a non-null REGS argument is
           initialized.  If not set, REGS is initialized to the max of
           RE_NREGS and re_nsub + 1 registers.  */
  unsigned caller_allocated_regs : 1;
/* [[[end pattern_buffer]]] */
};

typedef struct re_pattern_buffer regex_t;


/* search.c (search_buffer) in Emacs needs this one opcode value.  It is
   defined both in `regex.c' and here.  */
#define RE_EXACTN_VALUE 1




/* Type for byte offsets within the string.  POSIX mandates us defining
   this.  */
typedef int regoff_t;


/* This is the structure we store register match data in.  See
   regex.texinfo for a full description of what registers match.  */
struct re_registers
{
  unsigned num_regs;
  regoff_t *start;
  regoff_t *end;
};


/* If `caller_allocated_regs' is zero in the pattern buffer, re_match_2
   returns information about this many registers.  */
#ifndef RE_NREGS
#define RE_NREGS 30
#endif


/* POSIX specification for registers.  Aside from the different names than
   `re_registers', POSIX uses an array of structures, instead of a
   structure of arrays.  */
typedef struct
{
  regoff_t rm_so;  /* Byte offset from string's start to substring's start.  */
  regoff_t rm_eo;  /* Byte offset from string's start to substring's end.  */
} regmatch_t;




/* Declarations for routines.  */

#if __STDC__

/* Sets the current syntax to SYNTAX.  You can also simply assign to the
   `obscure_syntax' variable.  */
extern reg_syntax_t re_set_syntax (reg_syntax_t syntax);

/* Compile the regular expression PATTERN, with length LENGTH
   and syntax given by the global `obscure_syntax', into the buffer
   BUFFER.  Return NULL if successful, and an error string if not.  */
extern const char *re_compile_pattern (const char *pattern, int length,
                                       struct re_pattern_buffer *buffer);


/* Compile a fastmap for the compiled pattern in BUFFER; used to
   accelerate searches.  Return 0 if successful and -2 if was an
   internal error.  */
extern int re_compile_fastmap (struct re_pattern_buffer *buffer);


/* Search in the string STRING (with length LENGTH) for the pattern
   compiled into BUFFER.  Start searching at position START, for RANGE
   characters.  Return the starting position of the match, -1 for no
   match, or -2 for an internal error.  Also return register
   information in REGS (if REGS and BUFFER->no_sub are nonzero).  */
extern int re_search (struct re_pattern_buffer *buffer,
                      const char *string, int length,
                      int start, int range, 
		      struct re_registers *regs);


/* Like `re_search', but search in the concatenation of STRING1 and
   STRING2.  Also, stop searching at index START + STOP.  */
extern int re_search_2 (struct re_pattern_buffer *buffer,
                        const char *string1, int length1,
		        const char *string2, int length2,
                        int start, int range, 
                        struct re_registers *regs,
                        int stop);


/* Like `re_search', but return how many characters in STRING the regexp
   in BUFFER matched, starting at position START.  */
extern int re_match (const struct re_pattern_buffer *buffer,
                     const char *string, int length,
                     int start, struct re_registers *regs);


/* Relates to `re_match' as `re_search_2' relates to `re_search'.  */
extern int re_match_2 (const struct re_pattern_buffer *buffer,
                       const char *string1, int length1,
	               const char *string2, int length2,
                       int start,
                       struct re_registers *regs,
                       int stop);


#ifndef	__386BSD__
/* 4.2 bsd compatibility.  */
#ifndef bsdi
extern const char *re_comp (const char *);
#endif
extern int re_exec (const char *);
#endif

/* POSIX compatibility.  */
extern int regcomp (regex_t *preg, const char *pattern, int cflags);
extern int regexec (const regex_t *preg, const char *string, size_t nmatch,
		    regmatch_t pmatch[], int eflags);
extern size_t regerror (int errcode, const regex_t *preg, char *errbuf, 
			size_t errbuf_size);
extern void regfree (regex_t *preg);

#else /* not __STDC__ */

/* Support old C compilers.  */
#define const

extern reg_syntax_t re_set_syntax ();
extern char *re_compile_pattern ();
extern int re_search (), re_search_2 ();
extern int re_match (), re_match_2 ();

/* 4.2 BSD compatibility.  */
extern char *re_comp ();
extern int re_exec ();

/* POSIX compatibility.  */
extern int regcomp ();
extern int regexec ();
extern size_t regerror ();
extern void regfree ();

#endif /* not __STDC__ */
#endif /* not __REGEXP_LIBRARY_H__ */



/*
Local variables:
make-backup-files: t
version-control: t
trim-versions-without-asking: nil
End:
*/
