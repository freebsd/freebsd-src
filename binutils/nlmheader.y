%{/* nlmheader.y - parse NLM header specification keywords.
     Copyright 1993, 1994, 1995, 1997, 1998, 2001, 2002, 2003
     Free Software Foundation, Inc.

This file is part of GNU Binutils.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Written by Ian Lance Taylor <ian@cygnus.com>.

   This bison file parses the commands recognized by the NetWare NLM
   linker, except for lists of object files.  It stores the
   information in global variables.

   This implementation is based on the description in the NetWare Tool
   Maker Specification manual, edition 1.0.  */

#include "ansidecl.h"
#include <stdio.h>
#include "safe-ctype.h"
#include "bfd.h"
#include "bucomm.h"
#include "nlm/common.h"
#include "nlm/internal.h"
#include "nlmconv.h"

/* Information is stored in the structures pointed to by these
   variables.  */

Nlm_Internal_Fixed_Header *fixed_hdr;
Nlm_Internal_Variable_Header *var_hdr;
Nlm_Internal_Version_Header *version_hdr;
Nlm_Internal_Copyright_Header *copyright_hdr;
Nlm_Internal_Extended_Header *extended_hdr;

/* Procedure named by CHECK.  */
char *check_procedure;
/* File named by CUSTOM.  */
char *custom_file;
/* Whether to generate debugging information (DEBUG).  */
bfd_boolean debug_info;
/* Procedure named by EXIT.  */
char *exit_procedure;
/* Exported symbols (EXPORT).  */
struct string_list *export_symbols;
/* List of files from INPUT.  */
struct string_list *input_files;
/* Map file name (MAP, FULLMAP).  */
char *map_file;
/* Whether a full map has been requested (FULLMAP).  */
bfd_boolean full_map;
/* File named by HELP.  */
char *help_file;
/* Imported symbols (IMPORT).  */
struct string_list *import_symbols;
/* File named by MESSAGES.  */
char *message_file;
/* Autoload module list (MODULE).  */
struct string_list *modules;
/* File named by OUTPUT.  */
char *output_file;
/* File named by SHARELIB.  */
char *sharelib_file;
/* Start procedure name (START).  */
char *start_procedure;
/* VERBOSE.  */
bfd_boolean verbose;
/* RPC description file (XDCDATA).  */
char *rpc_file;

/* The number of serious errors that have occurred.  */
int parse_errors;

/* The current symbol prefix when reading a list of import or export
   symbols.  */
static char *symbol_prefix;

/* Parser error message handler.  */
#define yyerror(msg) nlmheader_error (msg);

/* Local functions.  */
static int yylex (void);
static void nlmlex_file_push (const char *);
static bfd_boolean nlmlex_file_open (const char *);
static int nlmlex_buf_init (void);
static char nlmlex_buf_add (int);
static long nlmlex_get_number (const char *);
static void nlmheader_identify (void);
static void nlmheader_warn (const char *, int);
static void nlmheader_error (const char *);
static struct string_list * string_list_cons (char *, struct string_list *);
static struct string_list * string_list_append (struct string_list *,
						struct string_list *);
static struct string_list * string_list_append1 (struct string_list *,
						 char *);
static char *xstrdup (const char *);

%}

%union
{
  char *string;
  struct string_list *list;
};

/* The reserved words.  */

%token CHECK CODESTART COPYRIGHT CUSTOM DATE DEBUG DESCRIPTION EXIT
%token EXPORT FLAG_ON FLAG_OFF FULLMAP HELP IMPORT INPUT MAP MESSAGES
%token MODULE MULTIPLE OS_DOMAIN OUTPUT PSEUDOPREEMPTION REENTRANT
%token SCREENNAME SHARELIB STACK START SYNCHRONIZE
%token THREADNAME TYPE VERBOSE VERSIONK XDCDATA

/* Arguments.  */

%token <string> STRING
%token <string> QUOTED_STRING

/* Typed non-terminals.  */
%type <list> symbol_list_opt symbol_list string_list
%type <string> symbol

%%

/* Keywords must start in the leftmost column of the file.  Arguments
   may appear anywhere else.  The lexer uses this to determine what
   token to return, so we don't have to worry about it here.  */

/* The entire file is just a list of commands.  */

file:
	  commands
	;

/* A possibly empty list of commands.  */

commands:
	  /* May be empty.  */
	| command commands
	;

/* A single command.  There is where most of the work takes place.  */

command:
	  CHECK STRING
	  {
	    check_procedure = $2;
	  }
	| CODESTART STRING
	  {
	    nlmheader_warn (_("CODESTART is not implemented; sorry"), -1);
	    free ($2);
	  }
	| COPYRIGHT QUOTED_STRING
	  {
	    int len;

	    strncpy (copyright_hdr->stamp, "CoPyRiGhT=", 10);
	    len = strlen ($2);
	    if (len >= NLM_MAX_COPYRIGHT_MESSAGE_LENGTH)
	      {
		nlmheader_warn (_("copyright string is too long"),
				NLM_MAX_COPYRIGHT_MESSAGE_LENGTH - 1);
		len = NLM_MAX_COPYRIGHT_MESSAGE_LENGTH - 1;
	      }
	    copyright_hdr->copyrightMessageLength = len;
	    strncpy (copyright_hdr->copyrightMessage, $2, len);
	    copyright_hdr->copyrightMessage[len] = '\0';
	    free ($2);
	  }
	| CUSTOM STRING
	  {
	    custom_file = $2;
	  }
	| DATE STRING STRING STRING
	  {
	    /* We don't set the version stamp here, because we use the
	       version stamp to detect whether the required VERSION
	       keyword was given.  */
	    version_hdr->month = nlmlex_get_number ($2);
	    version_hdr->day = nlmlex_get_number ($3);
	    version_hdr->year = nlmlex_get_number ($4);
	    free ($2);
	    free ($3);
	    free ($4);
	    if (version_hdr->month < 1 || version_hdr->month > 12)
	      nlmheader_warn (_("illegal month"), -1);
	    if (version_hdr->day < 1 || version_hdr->day > 31)
	      nlmheader_warn (_("illegal day"), -1);
	    if (version_hdr->year < 1900 || version_hdr->year > 3000)
	      nlmheader_warn (_("illegal year"), -1);
	  }
	| DEBUG
	  {
	    debug_info = TRUE;
	  }
	| DESCRIPTION QUOTED_STRING
	  {
	    int len;

	    len = strlen ($2);
	    if (len > NLM_MAX_DESCRIPTION_LENGTH)
	      {
		nlmheader_warn (_("description string is too long"),
				NLM_MAX_DESCRIPTION_LENGTH);
		len = NLM_MAX_DESCRIPTION_LENGTH;
	      }
	    var_hdr->descriptionLength = len;
	    strncpy (var_hdr->descriptionText, $2, len);
	    var_hdr->descriptionText[len] = '\0';
	    free ($2);
	  }
	| EXIT STRING
	  {
	    exit_procedure = $2;
	  }
	| EXPORT
	  {
	    symbol_prefix = NULL;
	  }
	  symbol_list_opt
	  {
	    export_symbols = string_list_append (export_symbols, $3);
	  }
	| FLAG_ON STRING
	  {
	    fixed_hdr->flags |= nlmlex_get_number ($2);
	    free ($2);
	  }
	| FLAG_OFF STRING
	  {
	    fixed_hdr->flags &=~ nlmlex_get_number ($2);
	    free ($2);
	  }
	| FULLMAP
	  {
	    map_file = "";
	    full_map = TRUE;
	  }
	| FULLMAP STRING
	  {
	    map_file = $2;
	    full_map = TRUE;
	  }
	| HELP STRING
	  {
	    help_file = $2;
	  }
	| IMPORT
	  {
	    symbol_prefix = NULL;
	  }
	  symbol_list_opt
	  {
	    import_symbols = string_list_append (import_symbols, $3);
	  }
	| INPUT string_list
	  {
	    input_files = string_list_append (input_files, $2);
	  }
	| MAP
	  {
	    map_file = "";
	  }
	| MAP STRING
	  {
	    map_file = $2;
	  }
	| MESSAGES STRING
	  {
	    message_file = $2;
	  }
	| MODULE string_list
	  {
	    modules = string_list_append (modules, $2);
	  }
	| MULTIPLE
	  {
	    fixed_hdr->flags |= 0x2;
	  }
	| OS_DOMAIN
	  {
	    fixed_hdr->flags |= 0x10;
	  }
	| OUTPUT STRING
	  {
	    if (output_file == NULL)
	      output_file = $2;
	    else
	      nlmheader_warn (_("ignoring duplicate OUTPUT statement"), -1);
	  }
	| PSEUDOPREEMPTION
	  {
	    fixed_hdr->flags |= 0x8;
	  }
	| REENTRANT
	  {
	    fixed_hdr->flags |= 0x1;
	  }
	| SCREENNAME QUOTED_STRING
	  {
	    int len;

	    len = strlen ($2);
	    if (len >= NLM_MAX_SCREEN_NAME_LENGTH)
	      {
		nlmheader_warn (_("screen name is too long"),
				NLM_MAX_SCREEN_NAME_LENGTH);
		len = NLM_MAX_SCREEN_NAME_LENGTH;
	      }
	    var_hdr->screenNameLength = len;
	    strncpy (var_hdr->screenName, $2, len);
	    var_hdr->screenName[NLM_MAX_SCREEN_NAME_LENGTH] = '\0';
	    free ($2);
	  }
	| SHARELIB STRING
	  {
	    sharelib_file = $2;
	  }
	| STACK STRING
	  {
	    var_hdr->stackSize = nlmlex_get_number ($2);
	    free ($2);
	  }
	| START STRING
	  {
	    start_procedure = $2;
	  }
	| SYNCHRONIZE
	  {
	    fixed_hdr->flags |= 0x4;
	  }
	| THREADNAME QUOTED_STRING
	  {
	    int len;

	    len = strlen ($2);
	    if (len >= NLM_MAX_THREAD_NAME_LENGTH)
	      {
		nlmheader_warn (_("thread name is too long"),
				NLM_MAX_THREAD_NAME_LENGTH);
		len = NLM_MAX_THREAD_NAME_LENGTH;
	      }
	    var_hdr->threadNameLength = len;
	    strncpy (var_hdr->threadName, $2, len);
	    var_hdr->threadName[len] = '\0';
	    free ($2);
	  }
	| TYPE STRING
	  {
	    fixed_hdr->moduleType = nlmlex_get_number ($2);
	    free ($2);
	  }
	| VERBOSE
	  {
	    verbose = TRUE;
	  }
	| VERSIONK STRING STRING STRING
	  {
	    long val;

	    strncpy (version_hdr->stamp, "VeRsIoN#", 8);
	    version_hdr->majorVersion = nlmlex_get_number ($2);
	    val = nlmlex_get_number ($3);
	    if (val < 0 || val > 99)
	      nlmheader_warn (_("illegal minor version number (must be between 0 and 99)"),
			      -1);
	    else
	      version_hdr->minorVersion = val;
	    val = nlmlex_get_number ($4);
	    if (val < 0)
	      nlmheader_warn (_("illegal revision number (must be between 0 and 26)"),
			      -1);
	    else if (val > 26)
	      version_hdr->revision = 0;
	    else
	      version_hdr->revision = val;
	    free ($2);
	    free ($3);
	    free ($4);
	  }
	| VERSIONK STRING STRING
	  {
	    long val;

	    strncpy (version_hdr->stamp, "VeRsIoN#", 8);
	    version_hdr->majorVersion = nlmlex_get_number ($2);
	    val = nlmlex_get_number ($3);
	    if (val < 0 || val > 99)
	      nlmheader_warn (_("illegal minor version number (must be between 0 and 99)"),
			      -1);
	    else
	      version_hdr->minorVersion = val;
	    version_hdr->revision = 0;
	    free ($2);
	    free ($3);
	  }
	| XDCDATA STRING
	  {
	    rpc_file = $2;
	  }
	;

/* A possibly empty list of symbols.  */

symbol_list_opt:
	  /* Empty.  */
	  {
	    $$ = NULL;
	  }
	| symbol_list
	  {
	    $$ = $1;
	  }
	;

/* A list of symbols in an import or export list.  Prefixes may appear
   in parentheses.  We need to use left recursion here to avoid
   building up a large import list on the parser stack.  */

symbol_list:
	  symbol
	  {
	    $$ = string_list_cons ($1, NULL);
	  }
	| symbol_prefix
	  {
	    $$ = NULL;
	  }
	| symbol_list symbol
	  {
	    $$ = string_list_append1 ($1, $2);
	  }
	| symbol_list symbol_prefix
	  {
	    $$ = $1;
	  }
	;

/* A prefix for subsequent symbols.  */

symbol_prefix:
	  '(' STRING ')'
	  {
	    if (symbol_prefix != NULL)
	      free (symbol_prefix);
	    symbol_prefix = $2;
	  }
	;

/* A single symbol.  */

symbol:
	  STRING
	  {
	    if (symbol_prefix == NULL)
	      $$ = $1;
	    else
	      {
		$$ = xmalloc (strlen (symbol_prefix) + strlen ($1) + 2);
		sprintf ($$, "%s@%s", symbol_prefix, $1);
		free ($1);
	      }
	  }
	;

/* A list of strings.  */

string_list:
	  /* May be empty.  */
	  {
	    $$ = NULL;
	  }
	| STRING string_list
	  {
	    $$ = string_list_cons ($1, $2);
	  }
	;

%%

/* If strerror is just a macro, we want to use the one from libiberty
   since it will handle undefined values.  */
#undef strerror
extern char *strerror PARAMS ((int));

/* The lexer is simple, too simple for flex.  Keywords are only
   recognized at the start of lines.  Everything else must be an
   argument.  A comma is treated as whitespace.  */

/* The states the lexer can be in.  */

enum lex_state
{
  /* At the beginning of a line.  */
  BEGINNING_OF_LINE,
  /* In the middle of a line.  */
  IN_LINE
};

/* We need to keep a stack of files to handle file inclusion.  */

struct input
{
  /* The file to read from.  */
  FILE *file;
  /* The name of the file.  */
  char *name;
  /* The current line number.  */
  int lineno;
  /* The current state.  */
  enum lex_state state;
  /* The next file on the stack.  */
  struct input *next;
};

/* The current input file.  */

static struct input current;

/* The character which introduces comments.  */
#define COMMENT_CHAR '#'

/* Start the lexer going on the main input file.  */

bfd_boolean
nlmlex_file (const char *name)
{
  current.next = NULL;
  return nlmlex_file_open (name);
}

/* Start the lexer going on a subsidiary input file.  */

static void
nlmlex_file_push (const char *name)
{
  struct input *push;

  push = (struct input *) xmalloc (sizeof (struct input));
  *push = current;
  if (nlmlex_file_open (name))
    current.next = push;
  else
    {
      current = *push;
      free (push);
    }
}

/* Start lexing from a file.  */

static bfd_boolean
nlmlex_file_open (const char *name)
{
  current.file = fopen (name, "r");
  if (current.file == NULL)
    {
      fprintf (stderr, "%s:%s: %s\n", program_name, name, strerror (errno));
      ++parse_errors;
      return FALSE;
    }
  current.name = xstrdup (name);
  current.lineno = 1;
  current.state = BEGINNING_OF_LINE;
  return TRUE;
}

/* Table used to turn keywords into tokens.  */

struct keyword_tokens_struct
{
  const char *keyword;
  int token;
};

static struct keyword_tokens_struct keyword_tokens[] =
{
  { "CHECK", CHECK },
  { "CODESTART", CODESTART },
  { "COPYRIGHT", COPYRIGHT },
  { "CUSTOM", CUSTOM },
  { "DATE", DATE },
  { "DEBUG", DEBUG },
  { "DESCRIPTION", DESCRIPTION },
  { "EXIT", EXIT },
  { "EXPORT", EXPORT },
  { "FLAG_ON", FLAG_ON },
  { "FLAG_OFF", FLAG_OFF },
  { "FULLMAP", FULLMAP },
  { "HELP", HELP },
  { "IMPORT", IMPORT },
  { "INPUT", INPUT },
  { "MAP", MAP },
  { "MESSAGES", MESSAGES },
  { "MODULE", MODULE },
  { "MULTIPLE", MULTIPLE },
  { "OS_DOMAIN", OS_DOMAIN },
  { "OUTPUT", OUTPUT },
  { "PSEUDOPREEMPTION", PSEUDOPREEMPTION },
  { "REENTRANT", REENTRANT },
  { "SCREENNAME", SCREENNAME },
  { "SHARELIB", SHARELIB },
  { "STACK", STACK },
  { "STACKSIZE", STACK },
  { "START", START },
  { "SYNCHRONIZE", SYNCHRONIZE },
  { "THREADNAME", THREADNAME },
  { "TYPE", TYPE },
  { "VERBOSE", VERBOSE },
  { "VERSION", VERSIONK },
  { "XDCDATA", XDCDATA }
};

#define KEYWORD_COUNT (sizeof (keyword_tokens) / sizeof (keyword_tokens[0]))

/* The lexer accumulates strings in these variables.  */
static char *lex_buf;
static int lex_size;
static int lex_pos;

/* Start accumulating strings into the buffer.  */
#define BUF_INIT() \
  ((void) (lex_buf != NULL ? lex_pos = 0 : nlmlex_buf_init ()))

static int
nlmlex_buf_init (void)
{
  lex_size = 10;
  lex_buf = xmalloc (lex_size + 1);
  lex_pos = 0;
  return 0;
}

/* Finish a string in the buffer.  */
#define BUF_FINISH() ((void) (lex_buf[lex_pos] = '\0'))

/* Accumulate a character into the buffer.  */
#define BUF_ADD(c) \
  ((void) (lex_pos < lex_size \
	   ? lex_buf[lex_pos++] = (c) \
	   : nlmlex_buf_add (c)))

static char
nlmlex_buf_add (int c)
{
  if (lex_pos >= lex_size)
    {
      lex_size *= 2;
      lex_buf = xrealloc (lex_buf, lex_size + 1);
    }

  return lex_buf[lex_pos++] = c;
}

/* The lexer proper.  This is called by the bison generated parsing
   code.  */

static int
yylex (void)
{
  int c;

tail_recurse:

  c = getc (current.file);

  /* Commas are treated as whitespace characters.  */
  while (ISSPACE (c) || c == ',')
    {
      current.state = IN_LINE;
      if (c == '\n')
	{
	  ++current.lineno;
	  current.state = BEGINNING_OF_LINE;
	}
      c = getc (current.file);
    }

  /* At the end of the file we either pop to the previous file or
     finish up.  */
  if (c == EOF)
    {
      fclose (current.file);
      free (current.name);
      if (current.next == NULL)
	return 0;
      else
	{
	  struct input *next;

	  next = current.next;
	  current = *next;
	  free (next);
	  goto tail_recurse;
	}
    }

  /* A comment character always means to drop everything until the
     next newline.  */
  if (c == COMMENT_CHAR)
    {
      do
	{
	  c = getc (current.file);
	}
      while (c != '\n');
      ++current.lineno;
      current.state = BEGINNING_OF_LINE;
      goto tail_recurse;
    }

  /* An '@' introduces an include file.  */
  if (c == '@')
    {
      do
	{
	  c = getc (current.file);
	  if (c == '\n')
	    ++current.lineno;
	}
      while (ISSPACE (c));
      BUF_INIT ();
      while (! ISSPACE (c) && c != EOF)
	{
	  BUF_ADD (c);
	  c = getc (current.file);
	}
      BUF_FINISH ();

      ungetc (c, current.file);

      nlmlex_file_push (lex_buf);
      goto tail_recurse;
    }

  /* A non-space character at the start of a line must be the start of
     a keyword.  */
  if (current.state == BEGINNING_OF_LINE)
    {
      BUF_INIT ();
      while (ISALNUM (c) || c == '_')
	{
	  BUF_ADD (TOUPPER (c));
	  c = getc (current.file);
	}
      BUF_FINISH ();

      if (c != EOF && ! ISSPACE (c) && c != ',')
	{
	  nlmheader_identify ();
	  fprintf (stderr, _("%s:%d: illegal character in keyword: %c\n"),
		   current.name, current.lineno, c);
	}
      else
	{
	  unsigned int i;

	  for (i = 0; i < KEYWORD_COUNT; i++)
	    {
	      if (lex_buf[0] == keyword_tokens[i].keyword[0]
		  && strcmp (lex_buf, keyword_tokens[i].keyword) == 0)
		{
		  /* Pushing back the final whitespace avoids worrying
		     about \n here.  */
		  ungetc (c, current.file);
		  current.state = IN_LINE;
		  return keyword_tokens[i].token;
		}
	    }

	  nlmheader_identify ();
	  fprintf (stderr, _("%s:%d: unrecognized keyword: %s\n"),
		   current.name, current.lineno, lex_buf);
	}

      ++parse_errors;
      /* Treat the rest of this line as a comment.  */
      ungetc (COMMENT_CHAR, current.file);
      goto tail_recurse;
    }

  /* Parentheses just represent themselves.  */
  if (c == '(' || c == ')')
    return c;

  /* Handle quoted strings.  */
  if (c == '"' || c == '\'')
    {
      int quote;
      int start_lineno;

      quote = c;
      start_lineno = current.lineno;

      c = getc (current.file);
      BUF_INIT ();
      while (c != quote && c != EOF)
	{
	  BUF_ADD (c);
	  if (c == '\n')
	    ++current.lineno;
	  c = getc (current.file);
	}
      BUF_FINISH ();

      if (c == EOF)
	{
	  nlmheader_identify ();
	  fprintf (stderr, _("%s:%d: end of file in quoted string\n"),
		   current.name, start_lineno);
	  ++parse_errors;
	}

      /* FIXME: Possible memory leak.  */
      yylval.string = xstrdup (lex_buf);
      return QUOTED_STRING;
    }

  /* Gather a generic argument.  */
  BUF_INIT ();
  while (! ISSPACE (c)
	 && c != ','
	 && c != COMMENT_CHAR
	 && c != '('
	 && c != ')')
    {
      BUF_ADD (c);
      c = getc (current.file);
    }
  BUF_FINISH ();

  ungetc (c, current.file);

  /* FIXME: Possible memory leak.  */
  yylval.string = xstrdup (lex_buf);
  return STRING;
}

/* Get a number from a string.  */

static long
nlmlex_get_number (const char *s)
{
  long ret;
  char *send;

  ret = strtol (s, &send, 10);
  if (*send != '\0')
    nlmheader_warn (_("bad number"), -1);
  return ret;
}

/* Prefix the nlmconv warnings with a note as to where they come from.
   We don't use program_name on every warning, because then some
   versions of the emacs next-error function can't recognize the line
   number.  */

static void
nlmheader_identify (void)
{
  static int done;

  if (! done)
    {
      fprintf (stderr, _("%s: problems in NLM command language input:\n"),
	       program_name);
      done = 1;
    }
}

/* Issue a warning.  */

static void
nlmheader_warn (const char *s, int imax)
{
  nlmheader_identify ();
  fprintf (stderr, "%s:%d: %s", current.name, current.lineno, s);
  if (imax != -1)
    fprintf (stderr, " (max %d)", imax);
  fprintf (stderr, "\n");
}

/* Report an error.  */

static void
nlmheader_error (const char *s)
{
  nlmheader_warn (s, -1);
  ++parse_errors;
}

/* Add a string to a string list.  */

static struct string_list *
string_list_cons (char *s, struct string_list *l)
{
  struct string_list *ret;

  ret = (struct string_list *) xmalloc (sizeof (struct string_list));
  ret->next = l;
  ret->string = s;
  return ret;
}

/* Append a string list to another string list.  */

static struct string_list *
string_list_append (struct string_list *l1, struct string_list *l2)
{
  register struct string_list **pp;

  for (pp = &l1; *pp != NULL; pp = &(*pp)->next)
    ;
  *pp = l2;
  return l1;
}

/* Append a string to a string list.  */

static struct string_list *
string_list_append1 (struct string_list *l, char *s)
{
  struct string_list *n;
  register struct string_list **pp;

  n = (struct string_list *) xmalloc (sizeof (struct string_list));
  n->next = NULL;
  n->string = s;
  for (pp = &l; *pp != NULL; pp = &(*pp)->next)
    ;
  *pp = n;
  return l;
}

/* Duplicate a string in memory.  */

static char *
xstrdup (const char *s)
{
  unsigned long len;
  char *ret;

  len = strlen (s);
  ret = xmalloc (len + 1);
  strcpy (ret, s);
  return ret;
}
