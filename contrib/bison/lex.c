/* Token-reader for Bison's input parser,
   Copyright (C) 1984, 1986, 1989, 1992 Free Software Foundation, Inc.

This file is part of Bison, the GNU Compiler Compiler.

Bison is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Bison is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Bison; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */


/*
   lex is the entry point.  It is called from reader.c.
   It returns one of the token-type codes defined in lex.h.
   When an identifier is seen, the code IDENTIFIER is returned
   and the name is looked up in the symbol table using symtab.c;
   symval is set to a pointer to the entry found.  */

#include <stdio.h>
#include "system.h"
#include "files.h"
#include "getopt.h"		/* for optarg */
#include "symtab.h"
#include "lex.h"
#include "alloc.h"

/* flags set by % directives */
extern int definesflag;    	/* for -d */
extern int toknumflag;   	/* for -k */
extern int noparserflag;	/* for -n */
extern int fixed_outfiles;  	/* for -y */
extern int nolinesflag;    	/* for -l */
extern int rawtoknumflag;	/* for -r */
extern int verboseflag;	/* for -v */
extern int debugflag;  		/* for -t */
extern char *spec_name_prefix; 	/* for -p */
extern char *spec_file_prefix;	/* for -b */
/*spec_outfile is declared in files.h, for -o */

extern int lineno;
extern int translations;

void init_lex PARAMS((void));
char *grow_token_buffer PARAMS((char *));
int skip_white_space PARAMS((void));
int safegetc PARAMS((FILE *));
int literalchar PARAMS((char **, int *, char));
void unlex PARAMS((int));
int lex PARAMS((void));
int parse_percent_token PARAMS((void));

/* functions from main.c */
extern char *printable_version PARAMS((int));
extern void fatal PARAMS((char *));
extern void warn PARAMS((char *));
extern void warni PARAMS((char *, int));
extern void warns PARAMS((char *, char *));

/* Buffer for storing the current token.  */
char *token_buffer;

/* Allocated size of token_buffer, not including space for terminator.  */
int maxtoken;

bucket *symval;
int numval;

static int unlexed;		/* these two describe a token to be reread */
static bucket *unlexed_symval;	/* by the next call to lex */


void
init_lex (void)
{
  maxtoken = 100;
  token_buffer = NEW2 (maxtoken + 1, char);
  unlexed = -1;
}


char *
grow_token_buffer (char *p)
{
  int offset = p - token_buffer;
  maxtoken *= 2;
  token_buffer = (char *) xrealloc(token_buffer, maxtoken + 1);
  return token_buffer + offset;
}


int
skip_white_space (void)
{
  register int c;
  register int inside;

  c = getc(finput);

  for (;;)
    {
      int cplus_comment;

      switch (c)
	{
	case '/':
	  c = getc(finput);
	  if (c != '*' && c != '/')
	    {
	      warn(_("unexpected `/' found and ignored"));
	      break;
	    }
	  cplus_comment = (c == '/');

	  c = getc(finput);

	  inside = 1;
	  while (inside)
	    {
	      if (!cplus_comment && c == '*')
		{
		  while (c == '*')
		    c = getc(finput);

		  if (c == '/')
		    {
		      inside = 0;
		      c = getc(finput);
		    }
		}
	      else if (c == '\n')
		{
		  lineno++;
		  if (cplus_comment)
		    inside = 0;
		  c = getc(finput);
		}
	      else if (c == EOF)
		fatal(_("unterminated comment"));
	      else
		c = getc(finput);
	    }

	  break;

	case '\n':
	  lineno++;

	case ' ':
	case '\t':
	case '\f':
	  c = getc(finput);
	  break;

	default:
	  return (c);
	}
    }
}

/* do a getc, but give error message if EOF encountered */
int
safegetc (FILE *f)
{
  register int c = getc(f);
  if (c == EOF)
    fatal(_("Unexpected end of file"));
  return c;
}

/* read one literal character from finput.  process \ escapes.
   append the normalized string version of the char to *pp.
   assign the character code to *pcode
   return 1 unless the character is an unescaped `term' or \n
	report error for \n
*/
int
literalchar (char **pp, int *pcode, char term)
{
  register int c;
  register char *p;
  register int code;
  int wasquote = 0;

  c = safegetc(finput);
  if (c == '\n')
    {
      warn(_("unescaped newline in constant"));
      ungetc(c, finput);
      code = '?';
      wasquote = 1;
    }
  else if (c != '\\')
    {
      code = c;
      if (c == term)
	wasquote = 1;
    }
  else
    {
      c = safegetc(finput);
      if (c == 't')  code = '\t';
      else if (c == 'n')  code = '\n';
      else if (c == 'a')  code = '\007';
      else if (c == 'r')  code = '\r';
      else if (c == 'f')  code = '\f';
      else if (c == 'b')  code = '\b';
      else if (c == 'v')  code = '\013';
      else if (c == '\\')  code = '\\';
      else if (c == '\'')  code = '\'';
      else if (c == '\"')  code = '\"';
      else if (c <= '7' && c >= '0')
	{
	  code = 0;
	  while (c <= '7' && c >= '0')
	    {
	      code = (code * 8) + (c - '0');
	      if (code >= 256 || code < 0)
		{
		  warni(_("octal value outside range 0...255: `\\%o'"), code);
		  code &= 0xFF;
		  break;
		}
	      c = safegetc(finput);
	    }
	  ungetc(c, finput);
	}
      else if (c == 'x')
	{
	  c = safegetc(finput);
	  code = 0;
	  while (1)
	    {
	      if (c >= '0' && c <= '9')
		code *= 16,  code += c - '0';
	      else if (c >= 'a' && c <= 'f')
		code *= 16,  code += c - 'a' + 10;
	      else if (c >= 'A' && c <= 'F')
		code *= 16,  code += c - 'A' + 10;
	      else
		break;
	      if (code >= 256 || code<0)
		{
		  warni(_("hexadecimal value above 255: `\\x%x'"), code);
		  code &= 0xFF;
		  break;
		}
	      c = safegetc(finput);
	    }
	  ungetc(c, finput);
	}
      else
	{
	  warns (_("unknown escape sequence: `\\' followed by `%s'"),
		 printable_version(c));
	  code = '?';
	}
    } /* has \ */

  /* now fill token_buffer with the canonical name for this character
     as a literal token.  Do not use what the user typed,
     so that `\012' and `\n' can be interchangeable.  */

  p = *pp;
  if (code == term && wasquote)
    *p++ = code;
  else if (code == '\\')  {*p++ = '\\'; *p++ = '\\';}
  else if (code == '\'')  {*p++ = '\\'; *p++ = '\'';}
  else if (code == '\"')  {*p++ = '\\'; *p++ = '\"';}
  else if (code >= 040 && code < 0177)
    *p++ = code;
  else if (code == '\t')  {*p++ = '\\'; *p++ = 't';}
  else if (code == '\n')  {*p++ = '\\'; *p++ = 'n';}
  else if (code == '\r')  {*p++ = '\\'; *p++ = 'r';}
  else if (code == '\v')  {*p++ = '\\'; *p++ = 'v';}
  else if (code == '\b')  {*p++ = '\\'; *p++ = 'b';}
  else if (code == '\f')  {*p++ = '\\'; *p++ = 'f';}
  else
    {
      *p++ = '\\';
      *p++ = code / 0100 + '0';
      *p++ = ((code / 010) & 07) + '0';
      *p++ = (code & 07) + '0';
    }
  *pp = p;
  *pcode = code;
  return  ! wasquote;
}


void
unlex (int token)
{
  unlexed = token;
  unlexed_symval = symval;
}


int
lex (void)
{
  register int c;
  char *p;

  if (unlexed >= 0)
    {
      symval = unlexed_symval;
      c = unlexed;
      unlexed = -1;
      return (c);
    }

  c = skip_white_space();
  *token_buffer = c;	/* for error messages (token buffer always valid) */
  token_buffer[1] = 0;

  switch (c)
    {
    case EOF:
      strcpy(token_buffer, "EOF");
      return (ENDFILE);

    case 'A':  case 'B':  case 'C':  case 'D':  case 'E':
    case 'F':  case 'G':  case 'H':  case 'I':  case 'J':
    case 'K':  case 'L':  case 'M':  case 'N':  case 'O':
    case 'P':  case 'Q':  case 'R':  case 'S':  case 'T':
    case 'U':  case 'V':  case 'W':  case 'X':  case 'Y':
    case 'Z':
    case 'a':  case 'b':  case 'c':  case 'd':  case 'e':
    case 'f':  case 'g':  case 'h':  case 'i':  case 'j':
    case 'k':  case 'l':  case 'm':  case 'n':  case 'o':
    case 'p':  case 'q':  case 'r':  case 's':  case 't':
    case 'u':  case 'v':  case 'w':  case 'x':  case 'y':
    case 'z':
    case '.':  case '_':
      p = token_buffer;
      while (isalnum(c) || c == '_' || c == '.')
	{
	  if (p == token_buffer + maxtoken)
	    p = grow_token_buffer(p);

	  *p++ = c;
	  c = getc(finput);
	}

      *p = 0;
      ungetc(c, finput);
      symval = getsym(token_buffer);
      return (IDENTIFIER);

    case '0':  case '1':  case '2':  case '3':  case '4':
    case '5':  case '6':  case '7':  case '8':  case '9':
      {
	numval = 0;

	p = token_buffer;
	while (isdigit(c))
	  {
	    if (p == token_buffer + maxtoken)
	      p = grow_token_buffer(p);

	    *p++ = c;
	    numval = numval*10 + c - '0';
	    c = getc(finput);
	  }
	*p = 0;
	ungetc(c, finput);
	return (NUMBER);
      }

    case '\'':

      /* parse the literal token and compute character code in  code  */

      translations = -1;
      {
	int code, discode;
	char discard[10], *dp;

	p = token_buffer;
	*p++ = '\'';
	literalchar(&p, &code, '\'');

	c = getc(finput);
	if (c != '\'')
	  {
	    warn(_("use \"...\" for multi-character literal tokens"));
	    while (1)
	      {
		dp = discard;
		if (! literalchar(&dp, &discode, '\''))
		  break;
	      }
	  }
	*p++ = '\'';
	*p = 0;
	symval = getsym(token_buffer);
	symval->class = STOKEN;
	if (! symval->user_token_number)
	  symval->user_token_number = code;
	return (IDENTIFIER);
      }

    case '\"':

      /* parse the literal string token and treat as an identifier */

      translations = -1;
      {
	int code;	/* ignored here */
	p = token_buffer;
	*p++ = '\"';
	while (literalchar(&p, &code, '\"'))  /* read up to and including " */
	  {
	    if (p >= token_buffer + maxtoken - 4)
	      p = grow_token_buffer(p);
	  }
	*p = 0;

	symval = getsym(token_buffer);
	symval->class = STOKEN;

	return (IDENTIFIER);
      }

    case ',':
      return (COMMA);

    case ':':
      return (COLON);

    case ';':
      return (SEMICOLON);

    case '|':
      return (BAR);

    case '{':
      return (LEFT_CURLY);

    case '=':
      do
	{
	  c = getc(finput);
	  if (c == '\n') lineno++;
	}
      while(c==' ' || c=='\n' || c=='\t');

      if (c == '{')
	{
	  strcpy(token_buffer, "={");
	  return(LEFT_CURLY);
	}
      else
	{
	  ungetc(c, finput);
	  return(ILLEGAL);
	}

    case '<':
      p = token_buffer;
      c = getc(finput);
      while (c != '>')
	{
	  if (c == EOF)
	    fatal(_("unterminated type name at end of file"));
	  if (c == '\n')
	    {
	      warn(_("unterminated type name"));
	      ungetc(c, finput);
	      break;
	    }

	  if (p == token_buffer + maxtoken)
	    p = grow_token_buffer(p);

	  *p++ = c;
	  c = getc(finput);
	}
      *p = 0;
      return (TYPENAME);


    case '%':
      return (parse_percent_token());

    default:
      return (ILLEGAL);
    }
}

/* the following table dictates the action taken for the various
	% directives.  A setflag value causes the named flag to be
	set.  A retval action returns the code.
*/
struct percent_table_struct {
	char *name;
	void *setflag;
	int retval;
} percent_table[] =
{
  {"token", NULL, TOKEN},
  {"term", NULL, TOKEN},
  {"nterm", NULL, NTERM},
  {"type", NULL, TYPE},
  {"guard", NULL, GUARD},
  {"union", NULL, UNION},
  {"expect", NULL, EXPECT},
  {"thong", NULL, THONG},
  {"start", NULL, START},
  {"left", NULL, LEFT},
  {"right", NULL, RIGHT},
  {"nonassoc", NULL, NONASSOC},
  {"binary", NULL, NONASSOC},
  {"semantic_parser", NULL, SEMANTIC_PARSER},
  {"pure_parser", NULL, PURE_PARSER},
  {"prec", NULL, PREC},

  {"no_lines", &nolinesflag, NOOP}, /* -l */
  {"raw", &rawtoknumflag, NOOP}, /* -r */
  {"token_table", &toknumflag, NOOP}, /* -k */

#if 0
  /* These can be utilized after main is reoganized so
     open_files() is deferred 'til after read_declarations().
     But %{ and %union both put information into files
     that have to be opened before read_declarations().
     */
  {"yacc", &fixed_outfiles, NOOP}, /* -y */
  {"fixed_output_files", &fixed_outfiles, NOOP}, /* -y */
  {"defines", &definesflag, NOOP}, /* -d */
  {"no_parser", &noparserflag, NOOP}, /* -n */
  {"output_file", &spec_outfile, SETOPT}, /* -o */
  {"file_prefix", &spec_file_prefix, SETOPT}, /* -b */
  {"name_prefix", &spec_name_prefix, SETOPT}, /* -p */

  /* These would be acceptable, but they do not affect processing */
  {"verbose", &verboseflag, NOOP}, /* -v */
  {"debug", &debugflag, NOOP},	/* -t */
  /*	{"help", <print usage stmt>, NOOP},*/	/* -h */
  /*	{"version", <print version number> ,  NOOP},*/	/* -V */
#endif

  {NULL, NULL, ILLEGAL}
};

/* Parse a token which starts with %.
   Assumes the % has already been read and discarded.  */

int
parse_percent_token (void)
{
  register int c;
  register char *p;
  register struct percent_table_struct *tx;

  p = token_buffer;
  c = getc(finput);
  *p++ = '%';
  *p++ = c;	/* for error msg */
  *p = 0;

  switch (c)
    {
    case '%':
      return (TWO_PERCENTS);

    case '{':
      return (PERCENT_LEFT_CURLY);

    case '<':
      return (LEFT);

    case '>':
      return (RIGHT);

    case '2':
      return (NONASSOC);

    case '0':
      return (TOKEN);

    case '=':
      return (PREC);
    }
  if (!isalpha(c))
    return (ILLEGAL);

  p = token_buffer;
  *p++ = '%';
  while (isalpha(c) || c == '_' || c == '-')
    {
      if (p == token_buffer + maxtoken)
	p = grow_token_buffer(p);

      if (c == '-') c = '_';
      *p++ = c;
      c = getc(finput);
    }

  ungetc(c, finput);

  *p = 0;

  /* table lookup % directive */
  for (tx = percent_table; tx->name; tx++)
    if (strcmp(token_buffer+1, tx->name) == 0)
      break;
  if (tx->retval == SETOPT)
    {
      *((char **)(tx->setflag)) = optarg;
      return NOOP;
    }
  if (tx->setflag)
    {
      *((int *)(tx->setflag)) = 1;
      return NOOP;
    }
  return tx->retval;
}
