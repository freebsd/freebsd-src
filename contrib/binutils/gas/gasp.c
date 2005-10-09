/* gasp.c - Gnu assembler preprocessor main program.
   Copyright 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   Written by Steve and Judy Chamberlain of Cygnus Support,
      sac@cygnus.com

   This file is part of GASP, the GNU Assembler Preprocessor.

   GASP is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GASP is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GASP; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/*
This program translates the input macros and stuff into a form
suitable for gas to consume.

  gasp [-sdhau] [-c char] [-o <outfile>] <infile>*

  -s copy source to output
  -c <char> comments are started with <char> instead of !
  -u allow unreasonable stuff
  -p print line numbers
  -d print debugging stats
  -s semi colons start comments
  -a use alternate syntax
     Pseudo ops can start with or without a .
     Labels have to be in first column.
  -I specify include dir
    Macro arg parameters subsituted by name, don't need the &.
     String can start with ' too.
     Strings can be surrounded by <..>
     A %<exp> in a string evaluates the expression
     Literal char in a string with !
*/

#include "config.h"
#include "bin-bugs.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "getopt.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef NEED_MALLOC_DECLARATION
extern char *malloc ();
#endif

#include "ansidecl.h"
#include "libiberty.h"
#include "safe-ctype.h"
#include "sb.h"
#include "macro.h"
#include "asintl.h"
#include "xregex.h"

char *program_version = "1.2";

/* This is normally declared in as.h, but we don't include that.  We
   need the function because other files linked with gasp.c might call
   it.  */
extern void as_abort PARAMS ((const char *, int, const char *));

/* The default obstack chunk size.  If we set this to zero, the
   obstack code will use whatever will fit in a 4096 byte block.  This
   is used by the hash table code used by macro.c.  */
int chunksize = 0;

#define MAX_INCLUDES 30		/* Maximum include depth.  */
#define MAX_REASONABLE 1000	/* Maximum number of expansions.  */

int unreasonable;		/* -u on command line.  */
int stats;			/* -d on command line.  */
int print_line_number;		/* -p flag on command line.  */
int copysource;			/* -c flag on command line.  */
int warnings;			/* Number of WARNINGs generated so far.  */
int errors;			/* Number of ERRORs generated so far.  */
int fatals;			/* Number of fatal ERRORs generated so far (either 0 or 1).  */
int alternate = 0;              /* -a on command line.  */
int mri = 0;			/* -M on command line.  */
char comment_char = '!';
int radix = 10;			/* Default radix.  */

int had_end; /* Seen .END.  */

/* The output stream.  */
FILE *outfile;

/* The attributes of each character are stored as a bit pattern
   chartype, which gives us quick tests.  */

#define FIRSTBIT 1
#define NEXTBIT  2
#define SEPBIT   4
#define WHITEBIT 8
#define COMMENTBIT 16
#define BASEBIT  32
#define ISCOMMENTCHAR(x) (chartype[(unsigned char)(x)] & COMMENTBIT)
#define ISFIRSTCHAR(x)  (chartype[(unsigned char)(x)] & FIRSTBIT)
#define ISNEXTCHAR(x)   (chartype[(unsigned char)(x)] & NEXTBIT)
#define ISSEP(x)        (chartype[(unsigned char)(x)] & SEPBIT)
#define ISWHITE(x)      (chartype[(unsigned char)(x)] & WHITEBIT)
#define ISBASE(x)       (chartype[(unsigned char)(x)] & BASEBIT)
static char chartype[256];

/* Conditional assembly uses the `ifstack'.  Each aif pushes another
   entry onto the stack, and sets the on flag if it should.  The aelse
   sets hadelse, and toggles on.  An aend pops a level.  We limit to
   100 levels of nesting, not because we're facists pigs with read
   only minds, but because more than 100 levels of nesting is probably
   a bug in the user's macro structure.  */

#define IFNESTING 100
struct {
  int on;			/* Is the level being output.  */
  int hadelse;			/* Has an aelse been seen.  */
} ifstack[IFNESTING];

int ifi;

/* The final and intermediate results of expression evaluation are kept in
   exp_t's.  Note that a symbol is not an sb, but a pointer into the input
   line.  It must be coped somewhere safe before the next line is read in.  */

typedef struct {
  char *name;
  int len;
} symbol;

typedef struct {
  int value;			/* Constant part.  */
  symbol add_symbol;		/* Name part.  */
  symbol sub_symbol;		/* Name part.  */
} exp_t;

/* Hashing is done in a pretty standard way.  A hash_table has a
   pointer to a vector of pointers to hash_entrys, and the size of the
   vector.  A hash_entry contains a union of all the info we like to
   store in hash table.  If there is a hash collision, hash_entries
   with the same hash are kept in a chain.  */

/* What the data in a hash_entry means.  */
typedef enum {
  hash_integer,			/* Name->integer mapping.  */
  hash_string,			/* Name->string mapping.  */
  hash_macro,			/* Name is a macro.  */
  hash_formal			/* Name is a formal argument.  */
} hash_type;

typedef struct hs {
  sb key;			/* Symbol name.  */
  hash_type type;		/* Symbol meaning.  */
  union {
    sb s;
    int i;
    struct macro_struct *m;
    struct formal_struct *f;
  } value;
  struct hs *next;		/* Next hash_entry with same hash key.  */
} hash_entry;

typedef struct {
  hash_entry **table;
  int size;
} hash_table;

/* How we nest files and expand macros etc.

   We keep a stack of of include_stack structs.  Each include file
   pushes a new level onto the stack.  We keep an sb with a pushback
   too.  unget chars are pushed onto the pushback sb, getchars first
   checks the pushback sb before reading from the input stream.

   Small things are expanded by adding the text of the item onto the
   pushback sb.  Larger items are grown by pushing a new level and
   allocating the entire pushback buf for the item.  Each time
   something like a macro is expanded, the stack index is changed.  We
   can then perform an exitm by popping all entries off the stack with
   the same stack index.  If we're being reasonable, we can detect
   recusive expansion by checking the index is reasonably small.  */

typedef enum {
  include_file, include_repeat, include_while, include_macro
} include_type;

struct include_stack {
  sb pushback;			/* Current pushback stream.  */
  int pushback_index;		/* Next char to read from stream.  */
  FILE *handle;			/* Open file.  */
  sb name;			/* Name of file.  */
  int linecount;		/* Number of lines read so far.  */
  include_type type;
  int index;			/* Index of this layer.  */
} include_stack[MAX_INCLUDES];

struct include_stack *sp;
#define isp (sp - include_stack)

/* Include file list.  */

typedef struct include_path {
  struct include_path *next;
  sb path;
} include_path;

include_path *paths_head;
include_path *paths_tail;

static void quit PARAMS ((void));
static void hash_new_table PARAMS ((int, hash_table *));
static int hash PARAMS ((sb *));
static hash_entry *hash_create PARAMS ((hash_table *, sb *));
static void hash_add_to_string_table PARAMS ((hash_table *, sb *, sb *, int));
static void hash_add_to_int_table PARAMS ((hash_table *, sb *, int));
static hash_entry *hash_lookup PARAMS ((hash_table *, sb *));
static void checkconst PARAMS ((int, exp_t *));
static int is_flonum PARAMS ((int, sb *));
static int chew_flonum PARAMS ((int, sb *, sb *));
static int sb_strtol PARAMS ((int, sb *, int, int *));
static int level_0 PARAMS ((int, sb *, exp_t *));
static int level_1 PARAMS ((int, sb *, exp_t *));
static int level_2 PARAMS ((int, sb *, exp_t *));
static int level_3 PARAMS ((int, sb *, exp_t *));
static int level_4 PARAMS ((int, sb *, exp_t *));
static int level_5 PARAMS ((int, sb *, exp_t *));
static int exp_parse PARAMS ((int, sb *, exp_t *));
static void exp_string PARAMS ((exp_t *, sb *));
static int exp_get_abs PARAMS ((const char *, int, sb *, int *));
#if 0
static void strip_comments PARAMS ((sb *));
#endif
static void unget PARAMS ((int));
static void include_buf PARAMS ((sb *, sb *, include_type, int));
static void include_print_where_line PARAMS ((FILE *));
static void include_print_line PARAMS ((FILE *));
static int get_line PARAMS ((sb *));
static int grab_label PARAMS ((sb *, sb *));
static void change_base PARAMS ((int, sb *, sb *));
static void do_end PARAMS ((sb *));
static void do_assign PARAMS ((int, int, sb *));
static void do_radix PARAMS ((sb *));
static int get_opsize PARAMS ((int, sb *, int *));
static int eol PARAMS ((int, sb *));
static void do_data PARAMS ((int, sb *, int));
static void do_datab PARAMS ((int, sb *));
static void do_align PARAMS ((int, sb *));
static void do_res PARAMS ((int, sb *, int));
static void do_export PARAMS ((sb *));
static void do_print PARAMS ((int, sb *));
static void do_heading PARAMS ((int, sb *));
static void do_page PARAMS ((void));
static void do_form PARAMS ((int, sb *));
static int get_any_string PARAMS ((int, sb *, sb *, int, int));
static int skip_openp PARAMS ((int, sb *));
static int skip_closep PARAMS ((int, sb *));
static int dolen PARAMS ((int, sb *, sb *));
static int doinstr PARAMS ((int, sb *, sb *));
static int dosubstr PARAMS ((int, sb *, sb *));
static void process_assigns PARAMS ((int, sb *, sb *));
static int get_and_process PARAMS ((int, sb *, sb *));
static void process_file PARAMS ((void));
static void free_old_entry PARAMS ((hash_entry *));
static void do_assigna PARAMS ((int, sb *));
static void do_assignc PARAMS ((int, sb *));
static void do_reg PARAMS ((int, sb *));
static int condass_lookup_name PARAMS ((sb *, int, sb *, int));
static int whatcond PARAMS ((int, sb *, int *));
static int istrue PARAMS ((int, sb *));
static void do_aif PARAMS ((int, sb *));
static void do_aelse PARAMS ((void));
static void do_aendi PARAMS ((void));
static int condass_on PARAMS ((void));
static void do_if PARAMS ((int, sb *, int));
static int get_mri_string PARAMS ((int, sb *, sb *, int));
static void do_ifc PARAMS ((int, sb *, int));
static void do_aendr PARAMS ((void));
static void do_awhile PARAMS ((int, sb *));
static void do_aendw PARAMS ((void));
static void do_exitm PARAMS ((void));
static void do_arepeat PARAMS ((int, sb *));
static void do_endm PARAMS ((void));
static void do_irp PARAMS ((int, sb *, int));
static void do_local PARAMS ((int, sb *));
static void do_macro PARAMS ((int, sb *));
static int macro_op PARAMS ((int, sb *));
static int getstring PARAMS ((int, sb *, sb *));
static void do_sdata PARAMS ((int, sb *, int));
static void do_sdatab PARAMS ((int, sb *));
static int new_file PARAMS ((const char *));
static void do_include PARAMS ((int, sb *));
static void include_pop PARAMS ((void));
static int get PARAMS ((void));
static int linecount PARAMS ((void));
static int include_next_index PARAMS ((void));
static void chartype_init PARAMS ((void));
static int process_pseudo_op PARAMS ((int, sb *, sb *));
static void add_keyword PARAMS ((const char *, int));
static void process_init PARAMS ((void));
static void do_define PARAMS ((const char *));
static void show_usage PARAMS ((FILE *, int));
static void show_help PARAMS ((void));

#define FATAL(x)				\
  do						\
    {						\
      include_print_where_line (stderr);	\
      fprintf x;				\
      fatals++;					\
      quit ();					\
    }						\
  while (0)

#define ERROR(x)				\
  do						\
    {						\
      include_print_where_line (stderr);	\
      fprintf x;				\
      errors++;					\
    }						\
  while (0)

#define WARNING(x)				\
  do						\
    {						\
      include_print_where_line (stderr);	\
      fprintf x;				\
      warnings++;				\
    }						\
  while (0)

/* Exit the program and return the right ERROR code.  */

static void
quit ()
{
  int exitcode;
  if (fatals + errors)
    exitcode = 1;
  else
    exitcode = 0;

  if (stats)
    {
      int i;
      for (i = 0; i < sb_max_power_two; i++)
	{
	  fprintf (stderr, "strings size %8d : %d\n",
		   1 << i, string_count[i]);
	}
    }
  exit (exitcode);
}

/* Hash table maintenance.  */

/* Build a new hash table with size buckets
   and fill in the info at ptr.  */

static void
hash_new_table (size, ptr)
     int size;
     hash_table *ptr;
{
  int i;
  ptr->size = size;
  ptr->table = (hash_entry **) xmalloc (size * (sizeof (hash_entry *)));
  /* Fill with null-pointer, not zero-bit-pattern.  */
  for (i = 0; i < size; i++)
    ptr->table[i] = 0;
}

/* Calculate and return the hash value of the sb at key.  */

static int
hash (key)
     sb *key;
{
  int k = 0x1234;
  int i;
  char *p = key->ptr;
  for (i = 0; i < key->len; i++)
    {
      k ^= (k << 2) ^ *p;
      p++;
    }
  return k & 0xf0fff;
}

/* Look up key in hash_table tab.  If present, then return it,
   otherwise build a new one and fill it with hash_integer.  */

static hash_entry *
hash_create (tab, key)
     hash_table *tab;
     sb *key;
{
  int k = hash (key) % tab->size;
  hash_entry *p;
  hash_entry **table = tab->table;

  p = table[k];

  while (1)
    {
      if (!p)
	{
	  hash_entry *n = (hash_entry *) xmalloc (sizeof (hash_entry));
	  n->next = table[k];
	  sb_new (&n->key);
	  sb_add_sb (&n->key, key);
	  table[k] = n;
	  n->type = hash_integer;
	  return n;
	}
      if (strncmp (table[k]->key.ptr, key->ptr, key->len) == 0)
	{
	  return p;
	}
      p = p->next;
    }
}

/* Add sb name with key into hash_table tab.
   If replacing old value and again, then ERROR.  */

static void
hash_add_to_string_table (tab, key, name, again)
     hash_table *tab;
     sb *key;
     sb *name;
     int again;
{
  hash_entry *ptr = hash_create (tab, key);
  if (ptr->type == hash_integer)
    {
      sb_new (&ptr->value.s);
    }
  if (ptr->value.s.len)
    {
      if (!again)
	ERROR ((stderr, _("redefinition not allowed\n")));
    }

  ptr->type = hash_string;
  sb_reset (&ptr->value.s);

  sb_add_sb (&ptr->value.s, name);
}

/* Add integer name to hash_table tab with sb key.  */

static void
hash_add_to_int_table (tab, key, name)
     hash_table *tab;
     sb *key;
     int name;
{
  hash_entry *ptr = hash_create (tab, key);
  ptr->value.i = name;
}

/* Look up sb key in hash_table tab.
   If found, return hash_entry result, else 0.  */

static hash_entry *
hash_lookup (tab, key)
     hash_table *tab;
     sb *key;
{
  int k = hash (key) % tab->size;
  hash_entry **table = tab->table;
  hash_entry *p = table[k];
  while (p)
    {
      if (p->key.len == key->len
	  && strncmp (p->key.ptr, key->ptr, key->len) == 0)
	return p;
      p = p->next;
    }
  return 0;
}

/* expressions

   are handled in a really simple recursive decent way. each bit of
   the machine takes an index into an sb and a pointer to an exp_t,
   modifies the *exp_t and returns the index of the first character
   past the part of the expression parsed.

 expression precedence:
  ( )
 unary + - ~
  * /
  + -
  &
  | ~
*/

/* Make sure that the exp_t at term is constant.
   If not the give the op ERROR.  */

static void
checkconst (op, term)
     int op;
     exp_t *term;
{
  if (term->add_symbol.len
      || term->sub_symbol.len)
    {
      ERROR ((stderr, _("the %c operator cannot take non-absolute arguments.\n"), op));
    }
}

/* Chew the flonum from the string starting at idx.  Adjust idx to
   point to the next character after the flonum.  */

static int
chew_flonum (idx, string, out)
     int idx;
     sb *string;
     sb *out;
{
  sb buf;
  regex_t reg;
  regmatch_t match;

  /* Duplicate and null terminate `string'.  */
  sb_new (&buf);
  sb_add_sb (&buf, string);
  sb_add_char (&buf, '\0');

  if (regcomp (&reg, "([0-9]*\\.[0-9]+([eE][+-]?[0-9]+)?)", REG_EXTENDED) != 0)
    return idx;
  if (regexec (&reg, &buf.ptr[idx], 1, &match, 0) != 0)
    return idx;

  /* Copy the match to the output.  */
  assert (match.rm_eo >= match.rm_so);
  sb_add_buffer (out, &buf.ptr[idx], match.rm_eo - match.rm_so);

  sb_kill (&buf);
  regfree (&reg);
  idx += match.rm_eo;
  return idx;
}

static int
is_flonum (idx, string)
     int idx;
     sb *string;
{
  sb buf;
  regex_t reg;
  int rc;

  /* Duplicate and null terminate `string'.  */
  sb_new (&buf);
  sb_add_sb (&buf, string);
  sb_add_char (&buf, '\0');

  if (regcomp (&reg, "^[0-9]*\\.[0-9]+([eE][+-]?[0-9]+)?", REG_EXTENDED) != 0)
    return 0;

  rc = regexec (&reg, &buf.ptr[idx], 0, NULL, 0);
  sb_kill (&buf);
  regfree (&reg);
  return (rc == 0);
}

/* Turn the number in string at idx into a number of base, fill in
   ptr, and return the index of the first character not in the number.  */

static int
sb_strtol (idx, string, base, ptr)
     int idx;
     sb *string;
     int base;
     int *ptr;
{
  int value = 0;
  idx = sb_skip_white (idx, string);

  while (idx < string->len)
    {
      int ch = string->ptr[idx];
      int dig = 0;
      if (ISDIGIT (ch))
	dig = ch - '0';
      else if (ch >= 'a' && ch <= 'f')
	dig = ch - 'a' + 10;
      else if (ch >= 'A' && ch <= 'F')
	dig = ch - 'A' + 10;
      else
	break;

      if (dig >= base)
	break;

      value = value * base + dig;
      idx++;
    }
  *ptr = value;
  return idx;
}

static int
level_0 (idx, string, lhs)
     int idx;
     sb *string;
     exp_t *lhs;
{
  lhs->add_symbol.len = 0;
  lhs->add_symbol.name = 0;

  lhs->sub_symbol.len = 0;
  lhs->sub_symbol.name = 0;

  idx = sb_skip_white (idx, string);

  lhs->value = 0;

  if (ISDIGIT (string->ptr[idx]))
    {
      idx = sb_strtol (idx, string, 10, &lhs->value);
    }
  else if (ISFIRSTCHAR (string->ptr[idx]))
    {
      int len = 0;
      lhs->add_symbol.name = string->ptr + idx;
      while (idx < string->len && ISNEXTCHAR (string->ptr[idx]))
	{
	  idx++;
	  len++;
	}
      lhs->add_symbol.len = len;
    }
  else if (string->ptr[idx] == '"')
    {
      sb acc;
      sb_new (&acc);
      ERROR ((stderr, _("string where expression expected.\n")));
      idx = getstring (idx, string, &acc);
      sb_kill (&acc);
    }
  else
    {
      ERROR ((stderr, _("can't find primary in expression.\n")));
      idx++;
    }
  return sb_skip_white (idx, string);
}

static int
level_1 (idx, string, lhs)
     int idx;
     sb *string;
     exp_t *lhs;
{
  idx = sb_skip_white (idx, string);

  switch (string->ptr[idx])
    {
    case '+':
      idx = level_1 (idx + 1, string, lhs);
      break;
    case '~':
      idx = level_1 (idx + 1, string, lhs);
      checkconst ('~', lhs);
      lhs->value = ~lhs->value;
      break;
    case '-':
      {
	symbol t;
	idx = level_1 (idx + 1, string, lhs);
	lhs->value = -lhs->value;
	t = lhs->add_symbol;
	lhs->add_symbol = lhs->sub_symbol;
	lhs->sub_symbol = t;
	break;
      }
    case '(':
      idx++;
      idx = level_5 (sb_skip_white (idx, string), string, lhs);
      if (string->ptr[idx] != ')')
	ERROR ((stderr, _("misplaced closing parens.\n")));
      else
	idx++;
      break;
    default:
      idx = level_0 (idx, string, lhs);
      break;
    }
  return sb_skip_white (idx, string);
}

static int
level_2 (idx, string, lhs)
     int idx;
     sb *string;
     exp_t *lhs;
{
  exp_t rhs;

  idx = level_1 (idx, string, lhs);

  while (idx < string->len && (string->ptr[idx] == '*'
			       || string->ptr[idx] == '/'))
    {
      char op = string->ptr[idx++];
      idx = level_1 (idx, string, &rhs);
      switch (op)
	{
	case '*':
	  checkconst ('*', lhs);
	  checkconst ('*', &rhs);
	  lhs->value *= rhs.value;
	  break;
	case '/':
	  checkconst ('/', lhs);
	  checkconst ('/', &rhs);
	  if (rhs.value == 0)
	    ERROR ((stderr, _("attempt to divide by zero.\n")));
	  else
	    lhs->value /= rhs.value;
	  break;
	}
    }
  return sb_skip_white (idx, string);
}

static int
level_3 (idx, string, lhs)
     int idx;
     sb *string;
     exp_t *lhs;
{
  exp_t rhs;

  idx = level_2 (idx, string, lhs);

  while (idx < string->len
	 && (string->ptr[idx] == '+'
	     || string->ptr[idx] == '-'))
    {
      char op = string->ptr[idx++];
      idx = level_2 (idx, string, &rhs);
      switch (op)
	{
	case '+':
	  lhs->value += rhs.value;
	  if (lhs->add_symbol.name && rhs.add_symbol.name)
	    {
	      ERROR ((stderr, _("can't add two relocatable expressions\n")));
	    }
	  /* Change nn+symbol to symbol + nn.  */
	  if (rhs.add_symbol.name)
	    {
	      lhs->add_symbol = rhs.add_symbol;
	    }
	  break;
	case '-':
	  lhs->value -= rhs.value;
	  lhs->sub_symbol = rhs.add_symbol;
	  break;
	}
    }
  return sb_skip_white (idx, string);
}

static int
level_4 (idx, string, lhs)
     int idx;
     sb *string;
     exp_t *lhs;
{
  exp_t rhs;

  idx = level_3 (idx, string, lhs);

  while (idx < string->len &&
	 string->ptr[idx] == '&')
    {
      char op = string->ptr[idx++];
      idx = level_3 (idx, string, &rhs);
      switch (op)
	{
	case '&':
	  checkconst ('&', lhs);
	  checkconst ('&', &rhs);
	  lhs->value &= rhs.value;
	  break;
	}
    }
  return sb_skip_white (idx, string);
}

static int
level_5 (idx, string, lhs)
     int idx;
     sb *string;
     exp_t *lhs;
{
  exp_t rhs;

  idx = level_4 (idx, string, lhs);

  while (idx < string->len
	 && (string->ptr[idx] == '|' || string->ptr[idx] == '~'))
    {
      char op = string->ptr[idx++];
      idx = level_4 (idx, string, &rhs);
      switch (op)
	{
	case '|':
	  checkconst ('|', lhs);
	  checkconst ('|', &rhs);
	  lhs->value |= rhs.value;
	  break;
	case '~':
	  checkconst ('~', lhs);
	  checkconst ('~', &rhs);
	  lhs->value ^= rhs.value;
	  break;
	}
    }
  return sb_skip_white (idx, string);
}

/* Parse the expression at offset idx into string, fill up res with
   the result.  Return the index of the first char past the
   expression.  */

static int
exp_parse (idx, string, res)
     int idx;
     sb *string;
     exp_t *res;
{
  return level_5 (sb_skip_white (idx, string), string, res);
}

/* Turn the expression at exp into text and glue it onto the end of
   string.  */

static void
exp_string (exp, string)
     exp_t *exp;
     sb *string;
{
  int np = 0;
  int ad = 0;
  sb_reset (string);

  if (exp->add_symbol.len)
    {
      sb_add_buffer (string, exp->add_symbol.name, exp->add_symbol.len);
      np = 1;
      ad = 1;
    }
  if (exp->value)
    {
      char buf[20];
      if (np)
	sb_add_char (string, '+');
      sprintf (buf, "%d", exp->value);
      sb_add_string (string, buf);
      np = 1;
      ad = 1;
    }
  if (exp->sub_symbol.len)
    {
      sb_add_char (string, '-');
      sb_add_buffer (string, exp->add_symbol.name, exp->add_symbol.len);
      np = 0;
      ad = 1;
    }

  if (!ad)
    sb_add_char (string, '0');
}

/* Parse the expression at offset idx into sb in.  Return the value in
   val.  If the expression is not constant, give ERROR emsg.  Return
   the index of the first character past the end of the expression.  */

static int
exp_get_abs (emsg, idx, in, val)
     const char *emsg;
     int idx;
     sb *in;
     int *val;
{
  exp_t res;
  idx = exp_parse (idx, in, &res);
  if (res.add_symbol.len || res.sub_symbol.len)
    ERROR ((stderr, "%s", emsg));
  *val = res.value;
  return idx;
}

/* Current label parsed from line.  */
sb label;

/* Hash table for all assigned variables.  */
hash_table assign_hash_table;

/* Hash table for keyword.  */
hash_table keyword_hash_table;

/* Hash table for eq variables.  */
hash_table vars;

#define in_comment ';'

#if 0
static void
strip_comments (out)
     sb *out;
{
  char *s = out->ptr;
  int i = 0;
  for (i = 0; i < out->len; i++)
    {
      if (ISCOMMENTCHAR (s[i]))
	{
	  out->len = i;
	  return;
	}
    }
}
#endif

/* Push back character ch so that it can be read again.  */

static void
unget (ch)
     int ch;
{
  if (ch == '\n')
    {
      sp->linecount--;
    }
  if (sp->pushback_index)
    sp->pushback_index--;
  else
    sb_add_char (&sp->pushback, ch);
}

/* Push the sb ptr onto the include stack, with the given name, type
   and index.  */

static void
include_buf (name, ptr, type, index)
     sb *name;
     sb *ptr;
     include_type type;
     int index;
{
  sp++;
  if (sp - include_stack >= MAX_INCLUDES)
    FATAL ((stderr, _("unreasonable nesting.\n")));
  sb_new (&sp->name);
  sb_add_sb (&sp->name, name);
  sp->handle = 0;
  sp->linecount = 1;
  sp->pushback_index = 0;
  sp->type = type;
  sp->index = index;
  sb_new (&sp->pushback);
  sb_add_sb (&sp->pushback, ptr);
}

/* Used in ERROR messages, print info on where the include stack is
   onto file.  */

static void
include_print_where_line (file)
     FILE *file;
{
  struct include_stack *p = include_stack + 1;

  while (p <= sp)
    {
      fprintf (file, "%s:%d ", sb_name (&p->name), p->linecount - 1);
      p++;
    }
}

/* Used in listings, print the line number onto file.  */

static void
include_print_line (file)
     FILE *file;
{
  int n;
  struct include_stack *p = include_stack + 1;

  n = fprintf (file, "%4d", p->linecount);
  p++;
  while (p <= sp)
    {
      n += fprintf (file, ".%d", p->linecount);
      p++;
    }
  while (n < 8 * 3)
    {
      fprintf (file, " ");
      n++;
    }
}

/* Read a line from the top of the include stack into sb in.  */

static int
get_line (in)
     sb *in;
{
  int online = 0;
  int more = 1;

  if (copysource)
    {
      putc (comment_char, outfile);
      if (print_line_number)
	include_print_line (outfile);
    }

  while (1)
    {
      int ch = get ();

      while (ch == '\r')
	ch = get ();

      if (ch == EOF)
	{
	  if (online)
	    {
	      WARNING ((stderr, _("End of file not at start of line.\n")));
	      if (copysource)
		putc ('\n', outfile);
	      ch = '\n';
	    }
	  else
	    more = 0;
	  break;
	}

      if (copysource)
	{
	  putc (ch, outfile);
	}

      if (ch == '\n')
	{
	  ch = get ();
	  online = 0;
	  if (ch == '+')
	    {
	      /* Continued line.  */
	      if (copysource)
		{
		  putc (comment_char, outfile);
		  putc ('+', outfile);
		}
	      ch = get ();
	    }
	  else
	    {
	      if (ch != EOF)
		unget (ch);
	      break;
	    }
	}
      else
	{
	  sb_add_char (in, ch);
	}
      online++;
    }

  return more;
}

/* Find a label from sb in and put it in out.  */

static int
grab_label (in, out)
     sb *in;
     sb *out;
{
  int i = 0;
  sb_reset (out);
  if (ISFIRSTCHAR (in->ptr[i]) || in->ptr[i] == '\\')
    {
      sb_add_char (out, in->ptr[i]);
      i++;
      while ((ISNEXTCHAR (in->ptr[i])
	      || in->ptr[i] == '\\'
	      || in->ptr[i] == '&')
	     && i < in->len)
	{
	  sb_add_char (out, in->ptr[i]);
	  i++;
	}
    }
  return i;
}

/* Find all strange base stuff and turn into decimal.  Also
   find all the other numbers and convert them from the default radix.  */

static void
change_base (idx, in, out)
     int idx;
     sb *in;
     sb *out;
{
  char buffer[20];

  while (idx < in->len)
    {
      if (in->ptr[idx] == '\\'
	  && idx + 1 < in->len
	  && in->ptr[idx + 1] == '(')
	{
	  idx += 2;
	  while (idx < in->len
		 && in->ptr[idx] != ')')
	    {
	      sb_add_char (out, in->ptr[idx]);
	      idx++;
	    }
	  if (idx < in->len)
	    idx++;
	}
      else if (idx < in->len - 1 && in->ptr[idx + 1] == '\'' && ! mri)
	{
	  int base;
	  int value;
	  switch (in->ptr[idx])
	    {
	    case 'b':
	    case 'B':
	      base = 2;
	      break;
	    case 'q':
	    case 'Q':
	      base = 8;
	      break;
	    case 'h':
	    case 'H':
	      base = 16;
	      break;
	    case 'd':
	    case 'D':
	      base = 10;
	      break;
	    default:
	      ERROR ((stderr, _("Illegal base character %c.\n"), in->ptr[idx]));
	      base = 10;
	      break;
	    }

	  idx = sb_strtol (idx + 2, in, base, &value);
	  sprintf (buffer, "%d", value);
	  sb_add_string (out, buffer);
	}
      else if (ISFIRSTCHAR (in->ptr[idx]))
	{
	  /* Copy entire names through quickly.  */
	  sb_add_char (out, in->ptr[idx]);
	  idx++;
	  while (idx < in->len && ISNEXTCHAR (in->ptr[idx]))
	    {
	      sb_add_char (out, in->ptr[idx]);
	      idx++;
	    }
	}
      else if (is_flonum (idx, in))
	{
	  idx = chew_flonum (idx, in, out);
	}
      else if (ISDIGIT (in->ptr[idx]))
	{
	  int value;
	  /* All numbers must start with a digit, let's chew it and
	     spit out decimal.  */
	  idx = sb_strtol (idx, in, radix, &value);
	  sprintf (buffer, "%d", value);
	  sb_add_string (out, buffer);

	  /* Skip all undigsested letters.  */
	  while (idx < in->len && ISNEXTCHAR (in->ptr[idx]))
	    {
	      sb_add_char (out, in->ptr[idx]);
	      idx++;
	    }
	}
      else if (in->ptr[idx] == '"' || in->ptr[idx] == '\'')
	{
	  char tchar = in->ptr[idx];
	  /* Copy entire names through quickly.  */
	  sb_add_char (out, in->ptr[idx]);
	  idx++;
	  while (idx < in->len && in->ptr[idx] != tchar)
	    {
	      sb_add_char (out, in->ptr[idx]);
	      idx++;
	    }
	}
      else
	{
	  /* Nothing special, just pass it through.  */
	  sb_add_char (out, in->ptr[idx]);
	  idx++;
	}
    }

}

/* .end  */

static void
do_end (in)
     sb *in;
{
  had_end = 1;
  if (mri)
    fprintf (outfile, "%s\n", sb_name (in));
}

/* .assign  */

static void
do_assign (again, idx, in)
     int again;
     int idx;
     sb *in;
{
  /* Stick label in symbol table with following value.  */
  exp_t e;
  sb acc;

  sb_new (&acc);
  idx = exp_parse (idx, in, &e);
  exp_string (&e, &acc);
  hash_add_to_string_table (&assign_hash_table, &label, &acc, again);
  sb_kill (&acc);
}

/* .radix [b|q|d|h]  */

static void
do_radix (ptr)
     sb *ptr;
{
  int idx = sb_skip_white (0, ptr);
  switch (ptr->ptr[idx])
    {
    case 'B':
    case 'b':
      radix = 2;
      break;
    case 'q':
    case 'Q':
      radix = 8;
      break;
    case 'd':
    case 'D':
      radix = 10;
      break;
    case 'h':
    case 'H':
      radix = 16;
      break;
    default:
      ERROR ((stderr, _("radix is %c must be one of b, q, d or h"), radix));
    }
}

/* Parse off a .b, .w or .l.  */

static int
get_opsize (idx, in, size)
     int idx;
     sb *in;
     int *size;
{
  *size = 4;
  if (in->ptr[idx] == '.')
    {
      idx++;
    }
  switch (in->ptr[idx])
    {
    case 'b':
    case 'B':
      *size = 1;
      break;
    case 'w':
    case 'W':
      *size = 2;
      break;
    case 'l':
    case 'L':
      *size = 4;
      break;
    case ' ':
    case '\t':
      break;
    default:
      ERROR ((stderr, _("size must be one of b, w or l, is %c.\n"), in->ptr[idx]));
      break;
    }
  idx++;

  return idx;
}

static int
eol (idx, line)
     int idx;
     sb *line;
{
  idx = sb_skip_white (idx, line);
  if (idx < line->len
      && ISCOMMENTCHAR(line->ptr[idx]))
    return 1;
  if (idx >= line->len)
    return 1;
  return 0;
}

/* .data [.b|.w|.l] <data>*
    or d[bwl] <data>*  */

static void
do_data (idx, in, size)
     int idx;
     sb *in;
     int size;
{
  int opsize = 4;
  char *opname = ".yikes!";
  sb acc;
  sb_new (&acc);

  if (!size)
    {
      idx = get_opsize (idx, in, &opsize);
    }
  else
    {
      opsize = size;
    }
  switch (opsize)
    {
    case 4:
      opname = ".long";
      break;
    case 2:
      opname = ".short";
      break;
    case 1:
      opname = ".byte";
      break;
    }

  fprintf (outfile, "%s\t", opname);

  idx = sb_skip_white (idx, in);

  if (alternate
      && idx < in->len
      && in->ptr[idx] == '"')
    {
      int i;
      idx = getstring (idx, in, &acc);
      for (i = 0; i < acc.len; i++)
	{
	  if (i)
	    fprintf (outfile, ",");
	  fprintf (outfile, "%d", acc.ptr[i]);
	}
    }
  else
    {
      while (!eol (idx, in))
	{
	  exp_t e;
	  idx = exp_parse (idx, in, &e);
	  exp_string (&e, &acc);
	  sb_add_char (&acc, 0);
	  fprintf (outfile, "%s", acc.ptr);
	  if (idx < in->len && in->ptr[idx] == ',')
	    {
	      fprintf (outfile, ",");
	      idx++;
	    }
	}
    }
  sb_kill (&acc);
  sb_print_at (outfile, idx, in);
  fprintf (outfile, "\n");
}

/* .datab [.b|.w|.l] <repeat>,<fill>  */

static void
do_datab (idx, in)
     int idx;
     sb *in;
{
  int opsize;
  int repeat;
  int fill;

  idx = get_opsize (idx, in, &opsize);

  idx = exp_get_abs (_("datab repeat must be constant.\n"), idx, in, &repeat);
  idx = sb_skip_comma (idx, in);
  idx = exp_get_abs (_("datab data must be absolute.\n"), idx, in, &fill);

  fprintf (outfile, ".fill\t%d,%d,%d\n", repeat, opsize, fill);
}

/* .align <size>  */

static void
do_align (idx, in)
     int idx;
     sb *in;
{
  int al, have_fill, fill;

  idx = exp_get_abs (_("align needs absolute expression.\n"), idx, in, &al);
  idx = sb_skip_white (idx, in);
  have_fill = 0;
  fill = 0;
  if (! eol (idx, in))
    {
      idx = sb_skip_comma (idx, in);
      idx = exp_get_abs (_(".align needs absolute fill value.\n"), idx, in,
			 &fill);
      have_fill = 1;
    }

  fprintf (outfile, ".align	%d", al);
  if (have_fill)
    fprintf (outfile, ",%d", fill);
  fprintf (outfile, "\n");
}

/* .res[.b|.w|.l] <size>  */

static void
do_res (idx, in, type)
     int idx;
     sb *in;
     int type;
{
  int size = 4;
  int count = 0;

  idx = get_opsize (idx, in, &size);
  while (!eol (idx, in))
    {
      idx = sb_skip_white (idx, in);
      if (in->ptr[idx] == ',')
	idx++;
      idx = exp_get_abs (_("res needs absolute expression for fill count.\n"), idx, in, &count);

      if (type == 'c' || type == 'z')
	count++;

      fprintf (outfile, ".space	%d\n", count * size);
    }
}

/* .export  */

static void
do_export (in)
     sb *in;
{
  fprintf (outfile, ".global	%s\n", sb_name (in));
}

/* .print [list] [nolist]  */

static void
do_print (idx, in)
     int idx;
     sb *in;
{
  idx = sb_skip_white (idx, in);
  while (idx < in->len)
    {
      if (strncasecmp (in->ptr + idx, "LIST", 4) == 0)
	{
	  fprintf (outfile, ".list\n");
	  idx += 4;
	}
      else if (strncasecmp (in->ptr + idx, "NOLIST", 6) == 0)
	{
	  fprintf (outfile, ".nolist\n");
	  idx += 6;
	}
      idx++;
    }
}

/* .head  */

static void
do_heading (idx, in)
     int idx;
     sb *in;
{
  sb head;
  sb_new (&head);
  idx = getstring (idx, in, &head);
  fprintf (outfile, ".title	\"%s\"\n", sb_name (&head));
  sb_kill (&head);
}

/* .page  */

static void
do_page ()
{
  fprintf (outfile, ".eject\n");
}

/* .form [lin=<value>] [col=<value>]  */

static void
do_form (idx, in)
     int idx;
     sb *in;
{
  int lines = 60;
  int columns = 132;
  idx = sb_skip_white (idx, in);

  while (idx < in->len)
    {

      if (strncasecmp (in->ptr + idx, "LIN=", 4) == 0)
	{
	  idx += 4;
	  idx = exp_get_abs (_("form LIN= needs absolute expresssion.\n"), idx, in, &lines);
	}

      if (strncasecmp (in->ptr + idx, _("COL="), 4) == 0)
	{
	  idx += 4;
	  idx = exp_get_abs (_("form COL= needs absolute expresssion.\n"), idx, in, &columns);
	}

      idx++;
    }
  fprintf (outfile, ".psize %d,%d\n", lines, columns);

}

/* Fetch string from the input stream,
   rules:
    'Bxyx<whitespace>  	-> return 'Bxyza
    %<char>		-> return string of decimal value of x
    "<string>"		-> return string
    xyx<whitespace>     -> return xyz
*/

static int
get_any_string (idx, in, out, expand, pretend_quoted)
     int idx;
     sb *in;
     sb *out;
     int expand;
     int pretend_quoted;
{
  sb_reset (out);
  idx = sb_skip_white (idx, in);

  if (idx < in->len)
    {
      if (in->len > 2 && in->ptr[idx + 1] == '\'' && ISBASE (in->ptr[idx]))
	{
	  while (!ISSEP (in->ptr[idx]))
	    sb_add_char (out, in->ptr[idx++]);
	}
      else if (in->ptr[idx] == '%'
	       && alternate
	       && expand)
	{
	  int val;
	  char buf[20];
	  /* Turns the next expression into a string.  */
	  /* xgettext: no-c-format */
	  idx = exp_get_abs (_("% operator needs absolute expression"),
			     idx + 1,
			     in,
			     &val);
	  sprintf (buf, "%d", val);
	  sb_add_string (out, buf);
	}
      else if (in->ptr[idx] == '"'
	       || in->ptr[idx] == '<'
	       || (alternate && in->ptr[idx] == '\''))
	{
	  if (alternate && expand)
	    {
	      /* Keep the quotes.  */
	      sb_add_char (out, '\"');

	      idx = getstring (idx, in, out);
	      sb_add_char (out, '\"');

	    }
	  else
	    {
	      idx = getstring (idx, in, out);
	    }
	}
      else
	{
	  while (idx < in->len
		 && (in->ptr[idx] == '"'
		     || in->ptr[idx] == '\''
		     || pretend_quoted
		     || !ISSEP (in->ptr[idx])))
	    {
	      if (in->ptr[idx] == '"'
		  || in->ptr[idx] == '\'')
		{
		  char tchar = in->ptr[idx];
		  sb_add_char (out, in->ptr[idx++]);
		  while (idx < in->len
			 && in->ptr[idx] != tchar)
		    sb_add_char (out, in->ptr[idx++]);
		  if (idx == in->len)
		    return idx;
		}
	      sb_add_char (out, in->ptr[idx++]);
	    }
	}
    }

  return idx;
}

/* Skip along sb in starting at idx, suck off whitespace a ( and more
   whitespace.  Return the idx of the next char.  */

static int
skip_openp (idx, in)
     int idx;
     sb *in;
{
  idx = sb_skip_white (idx, in);
  if (in->ptr[idx] != '(')
    ERROR ((stderr, _("misplaced ( .\n")));
  idx = sb_skip_white (idx + 1, in);
  return idx;
}

/* Skip along sb in starting at idx, suck off whitespace a ) and more
   whitespace.  Return the idx of the next char.  */

static int
skip_closep (idx, in)
     int idx;
     sb *in;
{
  idx = sb_skip_white (idx, in);
  if (in->ptr[idx] != ')')
    ERROR ((stderr, _("misplaced ).\n")));
  idx = sb_skip_white (idx + 1, in);
  return idx;
}

/* .len  */

static int
dolen (idx, in, out)
     int idx;
     sb *in;
     sb *out;
{

  sb stringout;
  char buffer[10];

  sb_new (&stringout);
  idx = skip_openp (idx, in);
  idx = get_and_process (idx, in, &stringout);
  idx = skip_closep (idx, in);
  sprintf (buffer, "%d", stringout.len);
  sb_add_string (out, buffer);

  sb_kill (&stringout);
  return idx;
}

/* .instr  */

static int
doinstr (idx, in, out)
     int idx;
     sb *in;
     sb *out;
{
  sb string;
  sb search;
  int i;
  int start;
  int res;
  char buffer[10];

  sb_new (&string);
  sb_new (&search);
  idx = skip_openp (idx, in);
  idx = get_and_process (idx, in, &string);
  idx = sb_skip_comma (idx, in);
  idx = get_and_process (idx, in, &search);
  idx = sb_skip_comma (idx, in);
  if (ISDIGIT (in->ptr[idx]))
    {
      idx = exp_get_abs (_(".instr needs absolute expresson.\n"), idx, in, &start);
    }
  else
    {
      start = 0;
    }
  idx = skip_closep (idx, in);
  res = -1;
  for (i = start; i < string.len; i++)
    {
      if (strncmp (string.ptr + i, search.ptr, search.len) == 0)
	{
	  res = i;
	  break;
	}
    }
  sprintf (buffer, "%d", res);
  sb_add_string (out, buffer);
  sb_kill (&string);
  sb_kill (&search);
  return idx;
}

static int
dosubstr (idx, in, out)
     int idx;
     sb *in;
     sb *out;
{
  sb string;
  int pos;
  int len;
  sb_new (&string);

  idx = skip_openp (idx, in);
  idx = get_and_process (idx, in, &string);
  idx = sb_skip_comma (idx, in);
  idx = exp_get_abs (_("need absolute position.\n"), idx, in, &pos);
  idx = sb_skip_comma (idx, in);
  idx = exp_get_abs (_("need absolute length.\n"), idx, in, &len);
  idx = skip_closep (idx, in);

  if (len < 0 || pos < 0 ||
      pos > string.len
      || pos + len > string.len)
    {
      sb_add_string (out, " ");
    }
  else
    {
      sb_add_char (out, '"');
      while (len > 0)
	{
	  sb_add_char (out, string.ptr[pos++]);
	  len--;
	}
      sb_add_char (out, '"');
    }
  sb_kill (&string);
  return idx;
}

/* Scan line, change tokens in the hash table to their replacements.  */

static void
process_assigns (idx, in, buf)
     int idx;
     sb *in;
     sb *buf;
{
  while (idx < in->len)
    {
      hash_entry *ptr;
      if (in->ptr[idx] == '\\'
	  && idx + 1 < in->len
	  && in->ptr[idx + 1] == '(')
	{
	  do
	    {
	      sb_add_char (buf, in->ptr[idx]);
	      idx++;
	    }
	  while (idx < in->len && in->ptr[idx - 1] != ')');
	}
      else if (in->ptr[idx] == '\\'
	  && idx + 1 < in->len
	  && in->ptr[idx + 1] == '&')
	{
	  idx = condass_lookup_name (in, idx + 2, buf, 1);
	}
      else if (in->ptr[idx] == '\\'
	       && idx + 1 < in->len
	       && in->ptr[idx + 1] == '$')
	{
	  idx = condass_lookup_name (in, idx + 2, buf, 0);
	}
      else if (idx + 3 < in->len
	       && in->ptr[idx] == '.'
	       && TOUPPER (in->ptr[idx + 1]) == 'L'
	       && TOUPPER (in->ptr[idx + 2]) == 'E'
	       && TOUPPER (in->ptr[idx + 3]) == 'N')
	idx = dolen (idx + 4, in, buf);
      else if (idx + 6 < in->len
	       && in->ptr[idx] == '.'
	       && TOUPPER (in->ptr[idx + 1]) == 'I'
	       && TOUPPER (in->ptr[idx + 2]) == 'N'
	       && TOUPPER (in->ptr[idx + 3]) == 'S'
	       && TOUPPER (in->ptr[idx + 4]) == 'T'
	       && TOUPPER (in->ptr[idx + 5]) == 'R')
	idx = doinstr (idx + 6, in, buf);
      else if (idx + 7 < in->len
	       && in->ptr[idx] == '.'
	       && TOUPPER (in->ptr[idx + 1]) == 'S'
	       && TOUPPER (in->ptr[idx + 2]) == 'U'
	       && TOUPPER (in->ptr[idx + 3]) == 'B'
	       && TOUPPER (in->ptr[idx + 4]) == 'S'
	       && TOUPPER (in->ptr[idx + 5]) == 'T'
	       && TOUPPER (in->ptr[idx + 6]) == 'R')
	idx = dosubstr (idx + 7, in, buf);
      else if (ISFIRSTCHAR (in->ptr[idx]))
	{
	  /* May be a simple name subsitution, see if we have a word.  */
	  sb acc;
	  int cur = idx + 1;
	  while (cur < in->len
		 && (ISNEXTCHAR (in->ptr[cur])))
	    cur++;

	  sb_new (&acc);
	  sb_add_buffer (&acc, in->ptr + idx, cur - idx);
	  ptr = hash_lookup (&assign_hash_table, &acc);
	  if (ptr)
	    {
	      /* Found a definition for it.  */
	      sb_add_sb (buf, &ptr->value.s);
	    }
	  else
	    {
	      /* No definition, just copy the word.  */
	      sb_add_sb (buf, &acc);
	    }
	  sb_kill (&acc);
	  idx = cur;
	}
      else
	{
	  sb_add_char (buf, in->ptr[idx++]);
	}
    }
}

static int
get_and_process (idx, in, out)
     int idx;
     sb *in;
     sb *out;
{
  sb t;
  sb_new (&t);
  idx = get_any_string (idx, in, &t, 1, 0);
  process_assigns (0, &t, out);
  sb_kill (&t);
  return idx;
}

static void
process_file ()
{
  sb line;
  sb t1, t2;
  sb acc;
  sb label_in;
  int more;

  sb_new (&line);
  sb_new (&t1);
  sb_new (&t2);
  sb_new (&acc);
  sb_new (&label_in);
  sb_reset (&line);
  more = get_line (&line);
  while (more)
    {
      /* Find any label and pseudo op that we're intested in.  */
      int l;
      if (line.len == 0)
	{
	  if (condass_on ())
	    fprintf (outfile, "\n");
	}
      else if (mri
	       && (line.ptr[0] == '*'
		   || line.ptr[0] == '!'))
	{
	  /* MRI line comment.  */
	  fprintf (outfile, "%s", sb_name (&line));
	}
      else
	{
	  l = grab_label (&line, &label_in);
	  sb_reset (&label);

	  if (line.ptr[l] == ':')
	    l++;
	  while (ISWHITE (line.ptr[l]) && l < line.len)
	    l++;

	  if (label_in.len)
	    {
	      int do_assigns;

	      /* Munge the label, unless this is EQU or ASSIGN.  */
	      do_assigns = 1;
	      if (l < line.len
		  && (line.ptr[l] == '.' || alternate || mri))
		{
		  int lx = l;

		  if (line.ptr[lx] == '.')
		    ++lx;
		  if (lx + 3 <= line.len
		      && strncasecmp ("EQU", line.ptr + lx, 3) == 0
		      && (lx + 3 == line.len
			  || ! ISFIRSTCHAR (line.ptr[lx + 3])))
		    do_assigns = 0;
		  else if (lx + 6 <= line.len
			   && strncasecmp ("ASSIGN", line.ptr + lx, 6) == 0
			   && (lx + 6 == line.len
			       || ! ISFIRSTCHAR (line.ptr[lx + 6])))
		    do_assigns = 0;
		}

	      if (do_assigns)
		process_assigns (0, &label_in, &label);
	      else
		sb_add_sb (&label, &label_in);
	    }

	  if (l < line.len)
	    {
	      if (process_pseudo_op (l, &line, &acc))
		{

		}
	      else if (condass_on ())
		{
		  if (macro_op (l, &line))
		    {

		    }
		  else
		    {
		      {
			if (label.len)
			  {
			    fprintf (outfile, "%s:\t", sb_name (&label));
			  }
			else
			  fprintf (outfile, "\t");
			sb_reset (&t1);
			process_assigns (l, &line, &t1);
			sb_reset (&t2);
			change_base (0, &t1, &t2);
			fprintf (outfile, "%s\n", sb_name (&t2));
		      }
		    }
		}
	    }
	  else
	    {
	      /* Only a label on this line.  */
	      if (label.len && condass_on ())
		{
		  fprintf (outfile, "%s:\n", sb_name (&label));
		}
	    }
	}

      if (had_end)
	break;
      sb_reset (&line);
      more = get_line (&line);
    }

  if (!had_end && !mri)
    WARNING ((stderr, _("END missing from end of file.\n")));
}

static void
free_old_entry (ptr)
     hash_entry *ptr;
{
  if (ptr)
    {
      if (ptr->type == hash_string)
	sb_kill (&ptr->value.s);
    }
}

/* name: .ASSIGNA <value>  */

static void
do_assigna (idx, in)
     int idx;
     sb *in;
{
  sb tmp;
  int val;
  sb_new (&tmp);

  process_assigns (idx, in, &tmp);
  idx = exp_get_abs (_(".ASSIGNA needs constant expression argument.\n"), 0, &tmp, &val);

  if (!label.len)
    {
      ERROR ((stderr, _(".ASSIGNA without label.\n")));
    }
  else
    {
      hash_entry *ptr = hash_create (&vars, &label);
      free_old_entry (ptr);
      ptr->type = hash_integer;
      ptr->value.i = val;
    }
  sb_kill (&tmp);
}

/* name: .ASSIGNC <string>  */

static void
do_assignc (idx, in)
     int idx;
     sb *in;
{
  sb acc;
  sb_new (&acc);
  idx = getstring (idx, in, &acc);

  if (!label.len)
    {
      ERROR ((stderr, _(".ASSIGNS without label.\n")));
    }
  else
    {
      hash_entry *ptr = hash_create (&vars, &label);
      free_old_entry (ptr);
      ptr->type = hash_string;
      sb_new (&ptr->value.s);
      sb_add_sb (&ptr->value.s, &acc);
    }
  sb_kill (&acc);
}

/* name: .REG (reg)  */

static void
do_reg (idx, in)
     int idx;
     sb *in;
{
  /* Remove reg stuff from inside parens.  */
  sb what;
  if (!mri)
    idx = skip_openp (idx, in);
  else
    idx = sb_skip_white (idx, in);
  sb_new (&what);
  while (idx < in->len
	 && (mri
	     ? ! eol (idx, in)
	     : in->ptr[idx] != ')'))
    {
      sb_add_char (&what, in->ptr[idx]);
      idx++;
    }
  hash_add_to_string_table (&assign_hash_table, &label, &what, 1);
  sb_kill (&what);
}

static int
condass_lookup_name (inbuf, idx, out, warn)
     sb *inbuf;
     int idx;
     sb *out;
     int warn;
{
  hash_entry *ptr;
  sb condass_acc;
  sb_new (&condass_acc);

  while (idx < inbuf->len
	 && ISNEXTCHAR (inbuf->ptr[idx]))
    {
      sb_add_char (&condass_acc, inbuf->ptr[idx++]);
    }

  if (inbuf->ptr[idx] == '\'')
    idx++;
  ptr = hash_lookup (&vars, &condass_acc);

  if (!ptr)
    {
      if (warn)
	{
	  WARNING ((stderr, _("Can't find preprocessor variable %s.\n"), sb_name (&condass_acc)));
	}
      else
	{
	  sb_add_string (out, "0");
	}
    }
  else
    {
      if (ptr->type == hash_integer)
	{
	  char buffer[30];
	  sprintf (buffer, "%d", ptr->value.i);
	  sb_add_string (out, buffer);
	}
      else
	{
	  sb_add_sb (out, &ptr->value.s);
	}
    }
  sb_kill (&condass_acc);
  return idx;
}

#define EQ 1
#define NE 2
#define GE 3
#define LT 4
#define LE 5
#define GT 6
#define NEVER 7

static int
whatcond (idx, in, val)
     int idx;
     sb *in;
     int *val;
{
  int cond;

  idx = sb_skip_white (idx, in);
  cond = NEVER;
  if (idx + 1 < in->len)
    {
      char *p;
      char a, b;

      p = in->ptr + idx;
      a = TOUPPER (p[0]);
      b = TOUPPER (p[1]);
      if (a == 'E' && b == 'Q')
	cond = EQ;
      else if (a == 'N' && b == 'E')
	cond = NE;
      else if (a == 'L' && b == 'T')
	cond = LT;
      else if (a == 'L' && b == 'E')
	cond = LE;
      else if (a == 'G' && b == 'T')
	cond = GT;
      else if (a == 'G' && b == 'E')
	cond = GE;
    }
  if (cond == NEVER)
    {
      ERROR ((stderr, _("Comparison operator must be one of EQ, NE, LT, LE, GT or GE.\n")));
      cond = NEVER;
    }
  idx = sb_skip_white (idx + 2, in);
  *val = cond;
  return idx;
}

static int
istrue (idx, in)
     int idx;
     sb *in;
{
  int res;
  sb acc_a;
  sb cond;
  sb acc_b;
  sb_new (&acc_a);
  sb_new (&cond);
  sb_new (&acc_b);
  idx = sb_skip_white (idx, in);

  if (in->ptr[idx] == '"')
    {
      int cond;
      int same;
      /* This is a string comparision.  */
      idx = getstring (idx, in, &acc_a);
      idx = whatcond (idx, in, &cond);
      idx = getstring (idx, in, &acc_b);
      same = acc_a.len == acc_b.len
	&& (strncmp (acc_a.ptr, acc_b.ptr, acc_a.len) == 0);

      if (cond != EQ && cond != NE)
	{
	  ERROR ((stderr, _("Comparison operator for strings must be EQ or NE\n")));
	  res = 0;
	}
      else
	res = (cond != EQ) ^ same;
    }
  else
    /* This is a numeric expression.  */
    {
      int vala;
      int valb;
      int cond;
      idx = exp_get_abs (_("Conditional operator must have absolute operands.\n"), idx, in, &vala);
      idx = whatcond (idx, in, &cond);
      idx = sb_skip_white (idx, in);
      if (in->ptr[idx] == '"')
	{
	  WARNING ((stderr, _("String compared against expression.\n")));
	  res = 0;
	}
      else
	{
	  idx = exp_get_abs (_("Conditional operator must have absolute operands.\n"), idx, in, &valb);
	  switch (cond)
	    {
	    default:
	      res = 42;
	      break;
	    case EQ:
	      res = vala == valb;
	      break;
	    case NE:
	      res = vala != valb;
	      break;
	    case LT:
	      res = vala < valb;
	      break;
	    case LE:
	      res = vala <= valb;
	      break;
	    case GT:
	      res = vala > valb;
	      break;
	    case GE:
	      res = vala >= valb;
	      break;
	    case NEVER:
	      res = 0;
	      break;
	    }
	}
    }

  sb_kill (&acc_a);
  sb_kill (&cond);
  sb_kill (&acc_b);
  return res;
}

/* .AIF  */

static void
do_aif (idx, in)
     int idx;
     sb *in;
{
  if (ifi >= IFNESTING)
    {
      FATAL ((stderr, _("AIF nesting unreasonable.\n")));
    }
  ifi++;
  ifstack[ifi].on = ifstack[ifi - 1].on ? istrue (idx, in) : 0;
  ifstack[ifi].hadelse = 0;
}

/* .AELSE  */

static void
do_aelse ()
{
  ifstack[ifi].on = ifstack[ifi - 1].on ? !ifstack[ifi].on : 0;
  if (ifstack[ifi].hadelse)
    {
      ERROR ((stderr, _("Multiple AELSEs in AIF.\n")));
    }
  ifstack[ifi].hadelse = 1;
}

/* .AENDI  */

static void
do_aendi ()
{
  if (ifi != 0)
    {
      ifi--;
    }
  else
    {
      ERROR ((stderr, _("AENDI without AIF.\n")));
    }
}

static int
condass_on ()
{
  return ifstack[ifi].on;
}

/* MRI IFEQ, IFNE, IFLT, IFLE, IFGE, IFGT.  */

static void
do_if (idx, in, cond)
     int idx;
     sb *in;
     int cond;
{
  int val;
  int res;

  if (ifi >= IFNESTING)
    {
      FATAL ((stderr, _("IF nesting unreasonable.\n")));
    }

  idx = exp_get_abs (_("Conditional operator must have absolute operands.\n"),
		     idx, in, &val);
  switch (cond)
    {
    default:
    case EQ: res = val == 0; break;
    case NE: res = val != 0; break;
    case LT: res = val <  0; break;
    case LE: res = val <= 0; break;
    case GE: res = val >= 0; break;
    case GT: res = val >  0; break;
    }

  ifi++;
  ifstack[ifi].on = ifstack[ifi - 1].on ? res : 0;
  ifstack[ifi].hadelse = 0;
}

/* Get a string for the MRI IFC or IFNC pseudo-ops.  */

static int
get_mri_string (idx, in, val, terminator)
     int idx;
     sb *in;
     sb *val;
     int terminator;
{
  idx = sb_skip_white (idx, in);

  if (idx < in->len
      && in->ptr[idx] == '\'')
    {
      sb_add_char (val, '\'');
      for (++idx; idx < in->len; ++idx)
	{
	  sb_add_char (val, in->ptr[idx]);
	  if (in->ptr[idx] == '\'')
	    {
	      ++idx;
	      if (idx >= in->len
		  || in->ptr[idx] != '\'')
		break;
	    }
	}
      idx = sb_skip_white (idx, in);
    }
  else
    {
      int i;

      while (idx < in->len
	     && in->ptr[idx] != terminator)
	{
	  sb_add_char (val, in->ptr[idx]);
	  ++idx;
	}
      i = val->len - 1;
      while (i >= 0 && ISWHITE (val->ptr[i]))
	--i;
      val->len = i + 1;
    }

  return idx;
}

/* MRI IFC, IFNC  */

static void
do_ifc (idx, in, ifnc)
     int idx;
     sb *in;
     int ifnc;
{
  sb first;
  sb second;
  int res;

  if (ifi >= IFNESTING)
    {
      FATAL ((stderr, _("IF nesting unreasonable.\n")));
    }

  sb_new (&first);
  sb_new (&second);

  idx = get_mri_string (idx, in, &first, ',');

  if (idx >= in->len || in->ptr[idx] != ',')
    {
      ERROR ((stderr, _("Bad format for IF or IFNC.\n")));
      return;
    }

  idx = get_mri_string (idx + 1, in, &second, ';');

  res = (first.len == second.len
	 && strncmp (first.ptr, second.ptr, first.len) == 0);
  res ^= ifnc;

  ifi++;
  ifstack[ifi].on = ifstack[ifi - 1].on ? res : 0;
  ifstack[ifi].hadelse = 0;
}

/* .ENDR  */

static void
do_aendr ()
{
  if (!mri)
    ERROR ((stderr, _("AENDR without a AREPEAT.\n")));
  else
    ERROR ((stderr, _("ENDR without a REPT.\n")));
}

/* .AWHILE  */

static void
do_awhile (idx, in)
     int idx;
     sb *in;
{
  int line = linecount ();
  sb exp;
  sb sub;
  int doit;

  sb_new (&sub);
  sb_new (&exp);

  process_assigns (idx, in, &exp);
  doit = istrue (0, &exp);

  if (! buffer_and_nest ("AWHILE", "AENDW", &sub, get_line))
    FATAL ((stderr, _("AWHILE without a AENDW at %d.\n"), line - 1));

  /* Turn
     	.AWHILE exp
	     foo
	.AENDW
     into
        foo
	.AWHILE exp
	foo
	.ENDW
  */

  if (doit)
    {
      int index = include_next_index ();

      sb copy;
      sb_new (&copy);
      sb_add_sb (&copy, &sub);
      sb_add_sb (&copy, in);
      sb_add_string (&copy, "\n");
      sb_add_sb (&copy, &sub);
      sb_add_string (&copy, "\t.AENDW\n");
      /* Push another WHILE.  */
      include_buf (&exp, &copy, include_while, index);
      sb_kill (&copy);
    }
  sb_kill (&exp);
  sb_kill (&sub);
}

/* .AENDW  */

static void
do_aendw ()
{
  ERROR ((stderr, _("AENDW without a AENDW.\n")));
}

/* .EXITM

   Pop things off the include stack until the type and index changes.  */

static void
do_exitm ()
{
  include_type type = sp->type;
  if (type == include_repeat
      || type == include_while
      || type == include_macro)
    {
      int index = sp->index;
      include_pop ();
      while (sp->index == index
	     && sp->type == type)
	{
	  include_pop ();
	}
    }
}

/* .AREPEAT  */

static void
do_arepeat (idx, in)
     int idx;
     sb *in;
{
  int line = linecount ();
  sb exp;			/* Buffer with expression in it.  */
  sb copy;			/* Expanded repeat block.  */
  sb sub;			/* Contents of AREPEAT.  */
  int rc;
  int ret;
  char buffer[30];

  sb_new (&exp);
  sb_new (&copy);
  sb_new (&sub);
  process_assigns (idx, in, &exp);
  idx = exp_get_abs (_("AREPEAT must have absolute operand.\n"), 0, &exp, &rc);
  if (!mri)
    ret = buffer_and_nest ("AREPEAT", "AENDR", &sub, get_line);
  else
    ret = buffer_and_nest ("REPT", "ENDR", &sub, get_line);
  if (! ret)
    FATAL ((stderr, _("AREPEAT without a AENDR at %d.\n"), line - 1));
  if (rc > 0)
    {
      /* Push back the text following the repeat, and another repeat block
	 so
	 .AREPEAT 20
	 foo
	 .AENDR
	 gets turned into
	 foo
	 .AREPEAT 19
	 foo
	 .AENDR
      */
      int index = include_next_index ();
      sb_add_sb (&copy, &sub);
      if (rc > 1)
	{
	  if (!mri)
	    sprintf (buffer, "\t.AREPEAT	%d\n", rc - 1);
	  else
	    sprintf (buffer, "\tREPT	%d\n", rc - 1);
	  sb_add_string (&copy, buffer);
	  sb_add_sb (&copy, &sub);
	  if (!mri)
	    sb_add_string (&copy, "	.AENDR\n");
	  else
	    sb_add_string (&copy, "	ENDR\n");
	}

      include_buf (&exp, &copy, include_repeat, index);
    }
  sb_kill (&exp);
  sb_kill (&sub);
  sb_kill (&copy);
}

/* .ENDM  */

static void
do_endm ()
{
  ERROR ((stderr, _(".ENDM without a matching .MACRO.\n")));
}

/* MRI IRP pseudo-op.  */

static void
do_irp (idx, in, irpc)
     int idx;
     sb *in;
     int irpc;
{
  const char *err;
  sb out;

  sb_new (&out);

  err = expand_irp (irpc, idx, in, &out, get_line, comment_char);
  if (err != NULL)
    ERROR ((stderr, "%s\n", err));

  fprintf (outfile, "%s", sb_terminate (&out));

  sb_kill (&out);
}

/* Macro processing.  */

/* Parse off LOCAL n1, n2,... Invent a label name for it.  */

static void
do_local (idx, line)
     int idx ATTRIBUTE_UNUSED;
     sb *line ATTRIBUTE_UNUSED;
{
  ERROR ((stderr, _("LOCAL outside of MACRO")));
}

static void
do_macro (idx, in)
     int idx;
     sb *in;
{
  const char *err;
  int line = linecount ();

  err = define_macro (idx, in, &label, get_line, (const char **) NULL);
  if (err != NULL)
    ERROR ((stderr, _("macro at line %d: %s\n"), line - 1, err));
}

static int
macro_op (idx, in)
     int idx;
     sb *in;
{
  const char *err;
  sb out;
  sb name;

  if (! macro_defined)
    return 0;

  sb_terminate (in);
  if (! check_macro (in->ptr + idx, &out, comment_char, &err, NULL))
    return 0;

  if (err != NULL)
    ERROR ((stderr, "%s\n", err));

  sb_new (&name);
  sb_add_string (&name, _("macro expansion"));

  include_buf (&name, &out, include_macro, include_next_index ());

  sb_kill (&name);
  sb_kill (&out);

  return 1;
}

/* String handling.  */

static int
getstring (idx, in, acc)
     int idx;
     sb *in;
     sb *acc;
{
  idx = sb_skip_white (idx, in);

  while (idx < in->len
	 && (in->ptr[idx] == '"'
	     || in->ptr[idx] == '<'
	     || (in->ptr[idx] == '\'' && alternate)))
    {
      if (in->ptr[idx] == '<')
	{
	  if (alternate || mri)
	    {
	      int nest = 0;
	      idx++;
	      while ((in->ptr[idx] != '>' || nest)
		     && idx < in->len)
		{
		  if (in->ptr[idx] == '!')
		    {
		      idx++;
		      sb_add_char (acc, in->ptr[idx++]);
		    }
		  else
		    {
		      if (in->ptr[idx] == '>')
			nest--;
		      if (in->ptr[idx] == '<')
			nest++;
		      sb_add_char (acc, in->ptr[idx++]);
		    }
		}
	      idx++;
	    }
	  else
	    {
	      int code;
	      idx++;
	      idx = exp_get_abs (_("Character code in string must be absolute expression.\n"),
				 idx, in, &code);
	      sb_add_char (acc, code);

	      if (in->ptr[idx] != '>')
		ERROR ((stderr, _("Missing > for character code.\n")));
	      idx++;
	    }
	}
      else if (in->ptr[idx] == '"' || in->ptr[idx] == '\'')
	{
	  char tchar = in->ptr[idx];
	  idx++;
	  while (idx < in->len)
	    {
	      if (alternate && in->ptr[idx] == '!')
		{
		  idx++;
		  sb_add_char (acc, in->ptr[idx++]);
		}
	      else
		{
		  if (in->ptr[idx] == tchar)
		    {
		      idx++;
		      if (idx >= in->len || in->ptr[idx] != tchar)
			break;
		    }
		  sb_add_char (acc, in->ptr[idx]);
		  idx++;
		}
	    }
	}
    }

  return idx;
}

/* .SDATA[C|Z] <string>  */

static void
do_sdata (idx, in, type)
     int idx;
     sb *in;
     int type;
{
  int nc = 0;
  int pidx = -1;
  sb acc;
  sb_new (&acc);
  fprintf (outfile, ".byte\t");

  while (!eol (idx, in))
    {
      int i;
      sb_reset (&acc);
      idx = sb_skip_white (idx, in);
      while (!eol (idx, in))
	{
	  pidx = idx = get_any_string (idx, in, &acc, 0, 1);
	  if (type == 'c')
	    {
	      if (acc.len > 255)
		{
		  ERROR ((stderr, _("string for SDATAC longer than 255 characters (%d).\n"), acc.len));
		}
	      fprintf (outfile, "%d", acc.len);
	      nc = 1;
	    }

	  for (i = 0; i < acc.len; i++)
	    {
	      if (nc)
		{
		  fprintf (outfile, ",");
		}
	      fprintf (outfile, "%d", acc.ptr[i]);
	      nc = 1;
	    }

	  if (type == 'z')
	    {
	      if (nc)
		fprintf (outfile, ",");
	      fprintf (outfile, "0");
	    }
	  idx = sb_skip_comma (idx, in);
	  if (idx == pidx)
	    break;
	}
      if (!alternate && in->ptr[idx] != ',' && idx != in->len)
	{
	  fprintf (outfile, "\n");
	  ERROR ((stderr, _("illegal character in SDATA line (0x%x).\n"),
		  in->ptr[idx]));
	  break;
	}
      idx++;
    }
  sb_kill (&acc);
  fprintf (outfile, "\n");
}

/* .SDATAB <count> <string>  */

static void
do_sdatab (idx, in)
     int idx;
     sb *in;
{
  int repeat;
  int i;
  sb acc;
  sb_new (&acc);

  idx = exp_get_abs (_("Must have absolute SDATAB repeat count.\n"), idx, in, &repeat);
  if (repeat <= 0)
    {
      ERROR ((stderr, _("Must have positive SDATAB repeat count (%d).\n"), repeat));
      repeat = 1;
    }

  idx = sb_skip_comma (idx, in);
  idx = getstring (idx, in, &acc);

  for (i = 0; i < repeat; i++)
    {
      if (i)
	fprintf (outfile, "\t");
      fprintf (outfile, ".byte\t");
      sb_print (outfile, &acc);
      fprintf (outfile, "\n");
    }
  sb_kill (&acc);

}

static int
new_file (name)
     const char *name;
{
  FILE *newone = fopen (name, "r");
  if (!newone)
    return 0;

  if (isp == MAX_INCLUDES)
    FATAL ((stderr, _("Unreasonable include depth (%ld).\n"), (long) isp));

  sp++;
  sp->handle = newone;

  sb_new (&sp->name);
  sb_add_string (&sp->name, name);

  sp->linecount = 1;
  sp->pushback_index = 0;
  sp->type = include_file;
  sp->index = 0;
  sb_new (&sp->pushback);
  return 1;
}

static void
do_include (idx, in)
     int idx;
     sb *in;
{
  sb t;
  sb cat;
  include_path *includes;

  sb_new (&t);
  sb_new (&cat);

  if (! mri)
    idx = getstring (idx, in, &t);
  else
    {
      idx = sb_skip_white (idx, in);
      while (idx < in->len && ! ISWHITE (in->ptr[idx]))
	{
	  sb_add_char (&t, in->ptr[idx]);
	  ++idx;
	}
    }

  for (includes = paths_head; includes; includes = includes->next)
    {
      sb_reset (&cat);
      sb_add_sb (&cat, &includes->path);
      sb_add_char (&cat, '/');
      sb_add_sb (&cat, &t);
      if (new_file (sb_name (&cat)))
	{
	  break;
	}
    }
  if (!includes)
    {
      if (! new_file (sb_name (&t)))
	FATAL ((stderr, _("Can't open include file `%s'.\n"), sb_name (&t)));
    }
  sb_kill (&cat);
  sb_kill (&t);
}

static void
include_pop ()
{
  if (sp != include_stack)
    {
      if (sp->handle)
	fclose (sp->handle);
      sp--;
    }
}

/* Get the next character from the include stack.  If there's anything
   in the pushback buffer, take that first.  If we're at eof, pop from
   the stack and try again.  Keep the linecount up to date.  */

static int
get ()
{
  int r;

  if (sp->pushback.len != sp->pushback_index)
    {
      r = (char) (sp->pushback.ptr[sp->pushback_index++]);
      /* When they've all gone, reset the pointer.  */
      if (sp->pushback_index == sp->pushback.len)
	{
	  sp->pushback.len = 0;
	  sp->pushback_index = 0;
	}
    }
  else if (sp->handle)
    {
      r = getc (sp->handle);
    }
  else
    r = EOF;

  if (r == EOF && isp)
    {
      include_pop ();
      r = get ();
      while (r == EOF && isp)
	{
	  include_pop ();
	  r = get ();
	}
      return r;
    }
  if (r == '\n')
    {
      sp->linecount++;
    }

  return r;
}

static int
linecount ()
{
  return sp->linecount;
}

static int
include_next_index ()
{
  static int index;
  if (!unreasonable
      && index > MAX_REASONABLE)
    FATAL ((stderr, _("Unreasonable expansion (-u turns off check).\n")));
  return ++index;
}

/* Initialize the chartype vector.  */

static void
chartype_init ()
{
  int x;
  for (x = 0; x < 256; x++)
    {
      if (ISALPHA (x) || x == '_' || x == '$')
	chartype[x] |= FIRSTBIT;

      if (mri && x == '.')
	chartype[x] |= FIRSTBIT;

      if (ISDIGIT (x) || ISALPHA (x) || x == '_' || x == '$')
	chartype[x] |= NEXTBIT;

      if (x == ' ' || x == '\t' || x == ',' || x == '"' || x == ';'
	  || x == '"' || x == '<' || x == '>' || x == ')' || x == '(')
	chartype[x] |= SEPBIT;

      if (x == 'b' || x == 'B'
	  || x == 'q' || x == 'Q'
	  || x == 'h' || x == 'H'
	  || x == 'd' || x == 'D')
	chartype [x] |= BASEBIT;

      if (x == ' ' || x == '\t')
	chartype[x] |= WHITEBIT;

      if (x == comment_char)
	chartype[x] |= COMMENTBIT;
    }
}

/* What to do with all the keywords.  */
#define PROCESS 	0x1000  /* Run substitution over the line.  */
#define LAB		0x2000  /* Spit out the label.  */

#define K_EQU 		(PROCESS|1)
#define K_ASSIGN 	(PROCESS|2)
#define K_REG 		(PROCESS|3)
#define K_ORG 		(PROCESS|4)
#define K_RADIX 	(PROCESS|5)
#define K_DATA 		(LAB|PROCESS|6)
#define K_DATAB 	(LAB|PROCESS|7)
#define K_SDATA 	(LAB|PROCESS|8)
#define K_SDATAB 	(LAB|PROCESS|9)
#define K_SDATAC 	(LAB|PROCESS|10)
#define K_SDATAZ	(LAB|PROCESS|11)
#define K_RES 		(LAB|PROCESS|12)
#define K_SRES 		(LAB|PROCESS|13)
#define K_SRESC 	(LAB|PROCESS|14)
#define K_SRESZ 	(LAB|PROCESS|15)
#define K_EXPORT 	(LAB|PROCESS|16)
#define K_GLOBAL 	(LAB|PROCESS|17)
#define K_PRINT 	(LAB|PROCESS|19)
#define K_FORM 		(LAB|PROCESS|20)
#define K_HEADING	(LAB|PROCESS|21)
#define K_PAGE		(LAB|PROCESS|22)
#define K_IMPORT	(LAB|PROCESS|23)
#define K_PROGRAM	(LAB|PROCESS|24)
#define K_END		(PROCESS|25)
#define K_INCLUDE	(PROCESS|26)
#define K_IGNORED	(PROCESS|27)
#define K_ASSIGNA	(PROCESS|28)
#define K_ASSIGNC	(29)
#define K_AIF		(PROCESS|30)
#define K_AELSE		(PROCESS|31)
#define K_AENDI		(PROCESS|32)
#define K_AREPEAT	(PROCESS|33)
#define K_AENDR		(PROCESS|34)
#define K_AWHILE	(35)
#define K_AENDW		(PROCESS|36)
#define K_EXITM		(37)
#define K_MACRO		(PROCESS|38)
#define K_ENDM		(39)
#define K_ALIGN		(PROCESS|LAB|40)
#define K_ALTERNATE     (41)
#define K_DB		(LAB|PROCESS|42)
#define K_DW		(LAB|PROCESS|43)
#define K_DL		(LAB|PROCESS|44)
#define K_LOCAL		(45)
#define K_IFEQ		(PROCESS|46)
#define K_IFNE		(PROCESS|47)
#define K_IFLT		(PROCESS|48)
#define K_IFLE		(PROCESS|49)
#define K_IFGE		(PROCESS|50)
#define K_IFGT		(PROCESS|51)
#define K_IFC		(PROCESS|52)
#define K_IFNC		(PROCESS|53)
#define K_IRP		(PROCESS|54)
#define K_IRPC		(PROCESS|55)

struct keyword {
  char *name;
  int code;
  int extra;
};

static struct keyword kinfo[] = {
  { "EQU", K_EQU, 0 },
  { "ALTERNATE", K_ALTERNATE, 0 },
  { "ASSIGN", K_ASSIGN, 0 },
  { "REG", K_REG, 0 },
  { "ORG", K_ORG, 0 },
  { "RADIX", K_RADIX, 0 },
  { "DATA", K_DATA, 0 },
  { "DB", K_DB, 0 },
  { "DW", K_DW, 0 },
  { "DL", K_DL, 0 },
  { "DATAB", K_DATAB, 0 },
  { "SDATA", K_SDATA, 0 },
  { "SDATAB", K_SDATAB, 0 },
  { "SDATAZ", K_SDATAZ, 0 },
  { "SDATAC", K_SDATAC, 0 },
  { "RES", K_RES, 0 },
  { "SRES", K_SRES, 0 },
  { "SRESC", K_SRESC, 0 },
  { "SRESZ", K_SRESZ, 0 },
  { "EXPORT", K_EXPORT, 0 },
  { "GLOBAL", K_GLOBAL, 0 },
  { "PRINT", K_PRINT, 0 },
  { "FORM", K_FORM, 0 },
  { "HEADING", K_HEADING, 0 },
  { "PAGE", K_PAGE, 0 },
  { "PROGRAM", K_IGNORED, 0 },
  { "END", K_END, 0 },
  { "INCLUDE", K_INCLUDE, 0 },
  { "ASSIGNA", K_ASSIGNA, 0 },
  { "ASSIGNC", K_ASSIGNC, 0 },
  { "AIF", K_AIF, 0 },
  { "AELSE", K_AELSE, 0 },
  { "AENDI", K_AENDI, 0 },
  { "AREPEAT", K_AREPEAT, 0 },
  { "AENDR", K_AENDR, 0 },
  { "EXITM", K_EXITM, 0 },
  { "MACRO", K_MACRO, 0 },
  { "ENDM", K_ENDM, 0 },
  { "AWHILE", K_AWHILE, 0 },
  { "ALIGN", K_ALIGN, 0 },
  { "AENDW", K_AENDW, 0 },
  { "ALTERNATE", K_ALTERNATE, 0 },
  { "LOCAL", K_LOCAL, 0 },
  { NULL, 0, 0 }
};

/* Although the conditional operators are handled by gas, we need to
   handle them here as well, in case they are used in a recursive
   macro to end the recursion.  */

static struct keyword mrikinfo[] = {
  { "IFEQ", K_IFEQ, 0 },
  { "IFNE", K_IFNE, 0 },
  { "IFLT", K_IFLT, 0 },
  { "IFLE", K_IFLE, 0 },
  { "IFGE", K_IFGE, 0 },
  { "IFGT", K_IFGT, 0 },
  { "IFC", K_IFC, 0 },
  { "IFNC", K_IFNC, 0 },
  { "ELSEC", K_AELSE, 0 },
  { "ENDC", K_AENDI, 0 },
  { "MEXIT", K_EXITM, 0 },
  { "REPT", K_AREPEAT, 0 },
  { "IRP", K_IRP, 0 },
  { "IRPC", K_IRPC, 0 },
  { "ENDR", K_AENDR, 0 },
  { NULL, 0, 0 }
};

/* Look for a pseudo op on the line. If one's there then call
   its handler.  */

static int
process_pseudo_op (idx, line, acc)
     int idx;
     sb *line;
     sb *acc;
{
  int oidx = idx;

  if (line->ptr[idx] == '.' || alternate || mri)
    {
      /* Scan forward and find pseudo name.  */
      char *in;
      hash_entry *ptr;

      char *s;
      char *e;
      if (line->ptr[idx] == '.')
	idx++;
      in = line->ptr + idx;
      s = in;
      e = s;
      sb_reset (acc);

      while (idx < line->len && *e && ISFIRSTCHAR (*e))
	{
	  sb_add_char (acc, *e);
	  e++;
	  idx++;
	}

      ptr = hash_lookup (&keyword_hash_table, acc);

      if (!ptr)
	{
#if 0
	  /* This one causes lots of pain when trying to preprocess
	     ordinary code.  */
	  WARNING ((stderr, _("Unrecognised pseudo op `%s'.\n"),
		    sb_name (acc)));
#endif
	  return 0;
	}
      if (ptr->value.i & LAB)
	{
	  /* Output the label.  */
	  if (label.len)
	    {
	      fprintf (outfile, "%s:\t", sb_name (&label));
	    }
	  else
	    fprintf (outfile, "\t");
	}

      if (mri && ptr->value.i == K_END)
	{
	  sb t;

	  sb_new (&t);
	  sb_add_buffer (&t, line->ptr + oidx, idx - oidx);
	  fprintf (outfile, "\t%s", sb_name (&t));
	  sb_kill (&t);
	}

      if (ptr->value.i & PROCESS)
	{
	  /* Polish the rest of the line before handling the pseudo op.  */
#if 0
	  strip_comments (line);
#endif
	  sb_reset (acc);
	  process_assigns (idx, line, acc);
	  sb_reset (line);
	  change_base (0, acc, line);
	  idx = 0;
	}
      if (!condass_on ())
	{
	  switch (ptr->value.i)
	    {
	    case K_AIF:
	      do_aif (idx, line);
	      break;
	    case K_AELSE:
	      do_aelse ();
	      break;
	    case K_AENDI:
	      do_aendi ();
	      break;
	    }
	  return 1;
	}
      else
	{
	  switch (ptr->value.i)
	    {
	    case K_ALTERNATE:
	      alternate = 1;
	      macro_init (1, mri, 0, exp_get_abs);
	      return 1;
	    case K_AELSE:
	      do_aelse ();
	      return 1;
	    case K_AENDI:
	      do_aendi ();
	      return 1;
	    case K_ORG:
	      ERROR ((stderr, _("ORG command not allowed.\n")));
	      break;
	    case K_RADIX:
	      do_radix (line);
	      return 1;
	    case K_DB:
	      do_data (idx, line, 1);
	      return 1;
	    case K_DW:
	      do_data (idx, line, 2);
	      return 1;
	    case K_DL:
	      do_data (idx, line, 4);
	      return 1;
	    case K_DATA:
	      do_data (idx, line, 0);
	      return 1;
	    case K_DATAB:
	      do_datab (idx, line);
	      return 1;
	    case K_SDATA:
	      do_sdata (idx, line, 0);
	      return 1;
	    case K_SDATAB:
	      do_sdatab (idx, line);
	      return 1;
	    case K_SDATAC:
	      do_sdata (idx, line, 'c');
	      return 1;
	    case K_SDATAZ:
	      do_sdata (idx, line, 'z');
	      return 1;
	    case K_ASSIGN:
	      do_assign (0, 0, line);
	      return 1;
	    case K_AIF:
	      do_aif (idx, line);
	      return 1;
	    case K_AREPEAT:
	      do_arepeat (idx, line);
	      return 1;
	    case K_AENDW:
	      do_aendw ();
	      return 1;
	    case K_AWHILE:
	      do_awhile (idx, line);
	      return 1;
	    case K_AENDR:
	      do_aendr ();
	      return 1;
	    case K_EQU:
	      do_assign (1, idx, line);
	      return 1;
	    case K_ALIGN:
	      do_align (idx, line);
	      return 1;
	    case K_RES:
	      do_res (idx, line, 0);
	      return 1;
	    case K_SRES:
	      do_res (idx, line, 's');
	      return 1;
	    case K_INCLUDE:
	      do_include (idx, line);
	      return 1;
	    case K_LOCAL:
	      do_local (idx, line);
	      return 1;
	    case K_MACRO:
	      do_macro (idx, line);
	      return 1;
	    case K_ENDM:
	      do_endm ();
	      return 1;
	    case K_SRESC:
	      do_res (idx, line, 'c');
	      return 1;
	    case K_PRINT:
	      do_print (idx, line);
	      return 1;
	    case K_FORM:
	      do_form (idx, line);
	      return 1;
	    case K_HEADING:
	      do_heading (idx, line);
	      return 1;
	    case K_PAGE:
	      do_page ();
	      return 1;
	    case K_GLOBAL:
	    case K_EXPORT:
	      do_export (line);
	      return 1;
	    case K_IMPORT:
	      return 1;
	    case K_SRESZ:
	      do_res (idx, line, 'z');
	      return 1;
	    case K_IGNORED:
	      return 1;
	    case K_END:
	      do_end (line);
	      return 1;
	    case K_ASSIGNA:
	      do_assigna (idx, line);
	      return 1;
	    case K_ASSIGNC:
	      do_assignc (idx, line);
	      return 1;
	    case K_EXITM:
	      do_exitm ();
	      return 1;
	    case K_REG:
	      do_reg (idx, line);
	      return 1;
	    case K_IFEQ:
	      do_if (idx, line, EQ);
	      return 1;
	    case K_IFNE:
	      do_if (idx, line, NE);
	      return 1;
	    case K_IFLT:
	      do_if (idx, line, LT);
	      return 1;
	    case K_IFLE:
	      do_if (idx, line, LE);
	      return 1;
	    case K_IFGE:
	      do_if (idx, line, GE);
	      return 1;
	    case K_IFGT:
	      do_if (idx, line, GT);
	      return 1;
	    case K_IFC:
	      do_ifc (idx, line, 0);
	      return 1;
	    case K_IFNC:
	      do_ifc (idx, line, 1);
	      return 1;
	    case K_IRP:
	      do_irp (idx, line, 0);
	      return 1;
	    case K_IRPC:
	      do_irp (idx, line, 1);
	      return 1;
	    }
	}
    }
  return 0;
}

/* Add a keyword to the hash table.  */

static void
add_keyword (name, code)
     const char *name;
     int code;
{
  sb label;
  int j;

  sb_new (&label);
  sb_add_string (&label, name);

  hash_add_to_int_table (&keyword_hash_table, &label, code);

  sb_reset (&label);
  for (j = 0; name[j]; j++)
    sb_add_char (&label, name[j] - 'A' + 'a');
  hash_add_to_int_table (&keyword_hash_table, &label, code);

  sb_kill (&label);
}

/* Build the keyword hash table - put each keyword in the table twice,
   once upper and once lower case.  */

static void
process_init ()
{
  int i;

  for (i = 0; kinfo[i].name; i++)
    add_keyword (kinfo[i].name, kinfo[i].code);

  if (mri)
    {
      for (i = 0; mrikinfo[i].name; i++)
	add_keyword (mrikinfo[i].name, mrikinfo[i].code);
    }
}

static void
do_define (string)
     const char *string;
{
  sb label;
  int res = 1;
  hash_entry *ptr;
  sb_new (&label);

  while (*string)
    {
      if (*string == '=')
	{
	  sb value;
	  sb_new (&value);
	  string++;
	  while (*string)
	    {
	      sb_add_char (&value, *string);
	      string++;
	    }
	  exp_get_abs (_("Invalid expression on command line.\n"),
		       0, &value, &res);
	  sb_kill (&value);
	  break;
	}
      sb_add_char (&label, *string);

      string++;
    }

  ptr = hash_create (&vars, &label);
  free_old_entry (ptr);
  ptr->type = hash_integer;
  ptr->value.i = res;
  sb_kill (&label);
}

char *program_name;

/* The list of long options.  */
static struct option long_options[] =
{
  { "alternate", no_argument, 0, 'a' },
  { "include", required_argument, 0, 'I' },
  { "commentchar", required_argument, 0, 'c' },
  { "copysource", no_argument, 0, 's' },
  { "debug", no_argument, 0, 'd' },
  { "help", no_argument, 0, 'h' },
  { "mri", no_argument, 0, 'M' },
  { "output", required_argument, 0, 'o' },
  { "print", no_argument, 0, 'p' },
  { "unreasonable", no_argument, 0, 'u' },
  { "version", no_argument, 0, 'v' },
  { "define", required_argument, 0, 'd' },
  { NULL, no_argument, 0, 0 }
};

/* Show a usage message and exit.  */
static void
show_usage (file, status)
     FILE *file;
     int status;
{
  fprintf (file, _("\
Usage: %s \n\
  [-a]      [--alternate]         enter alternate macro mode\n\
  [-c char] [--commentchar char]  change the comment character from !\n\
  [-d]      [--debug]             print some debugging info\n\
  [-h]      [--help]              print this message\n\
  [-M]      [--mri]               enter MRI compatibility mode\n\
  [-o out]  [--output out]        set the output file\n\
  [-p]      [--print]             print line numbers\n"), program_name);
  fprintf (file, _("\
  [-s]      [--copysource]        copy source through as comments \n\
  [-u]      [--unreasonable]      allow unreasonable nesting\n\
  [-v]      [--version]           print the program version\n\
  [-Dname=value]                  create preprocessor variable called name, with value\n\
  [-Ipath]                        add to include path list\n\
  [in-file]\n"));
  if (status == 0)
    printf (_("Report bugs to %s\n"), REPORT_BUGS_TO);
  exit (status);
}

/* Display a help message and exit.  */

static void
show_help ()
{
  printf (_("%s: Gnu Assembler Macro Preprocessor\n"), program_name);
  show_usage (stdout, 0);
}

int main PARAMS ((int, char **));

int
main (argc, argv)
     int argc;
     char **argv;
{
  int opt;
  char *out_name = 0;
  sp = include_stack;

  ifstack[0].on = 1;
  ifi = 0;

#if defined (HAVE_SETLOCALE) && defined (HAVE_LC_MESSAGES)
  setlocale (LC_MESSAGES, "");
#endif
#if defined (HAVE_SETLOCALE)
  setlocale (LC_CTYPE, "");
#endif
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  program_name = argv[0];
  xmalloc_set_program_name (program_name);

  hash_new_table (101, &keyword_hash_table);
  hash_new_table (101, &assign_hash_table);
  hash_new_table (101, &vars);

  sb_new (&label);

  while ((opt = getopt_long (argc, argv, "I:sdhavc:upo:D:M", long_options,
			     (int *) NULL))
	 != EOF)
    {
      switch (opt)
	{
	case 'o':
	  out_name = optarg;
	  break;
	case 'u':
	  unreasonable = 1;
	  break;
	case 'I':
	  {
	    include_path *p = (include_path *) xmalloc (sizeof (include_path));
	    p->next = NULL;
	    sb_new (&p->path);
	    sb_add_string (&p->path, optarg);
	    if (paths_tail)
	      paths_tail->next = p;
	    else
	      paths_head = p;
	    paths_tail = p;
	  }
	  break;
	case 'p':
	  print_line_number = 1;
	  break;
	case 'c':
	  comment_char = optarg[0];
	  break;
	case 'a':
	  alternate = 1;
	  break;
	case 's':
	  copysource = 1;
	  break;
	case 'd':
	  stats = 1;
	  break;
	case 'D':
	  do_define (optarg);
	  break;
	case 'M':
	  mri = 1;
	  comment_char = ';';
	  break;
	case 'h':
	  show_help ();
	  /* NOTREACHED  */
	case 'v':
	  /* This output is intended to follow the GNU standards document.  */
	  printf (_("GNU assembler pre-processor %s\n"), program_version);
	  printf (_("Copyright 1996 Free Software Foundation, Inc.\n"));
	  printf (_("\
This program is free software; you may redistribute it under the terms of\n\
the GNU General Public License.  This program has absolutely no warranty.\n"));
	  exit (0);
	  /* NOTREACHED  */
	case 0:
	  break;
	default:
	  show_usage (stderr, 1);
	  /* NOTREACHED  */
	}
    }

  process_init ();

  macro_init (alternate, mri, 0, exp_get_abs);

  if (out_name)
    {
      outfile = fopen (out_name, "w");
      if (!outfile)
	{
	  fprintf (stderr, _("%s: Can't open output file `%s'.\n"),
		   program_name, out_name);
	  exit (1);
	}
    }
  else
    {
      outfile = stdout;
    }

  chartype_init ();
  if (!outfile)
    outfile = stdout;

  /* Process all the input files.  */

  while (optind < argc)
    {
      if (new_file (argv[optind]))
	{
	  process_file ();
	}
      else
	{
	  fprintf (stderr, _("%s: Can't open input file `%s'.\n"),
		   program_name, argv[optind]);
	  exit (1);
	}
      optind++;
    }

  quit ();
  return 0;
}

/* This function is used because an abort in some of the other files
   may be compiled into as_abort because they include as.h.  */

void
as_abort (file, line, fn)
     const char *file, *fn;
     int line;
{
  fprintf (stderr, _("Internal error, aborting at %s line %d"), file, line);
  if (fn)
    fprintf (stderr, " in %s", fn);
  fprintf (stderr, _("\nPlease report this bug.\n"));
  exit (1);
}
