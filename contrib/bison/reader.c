/* Input parser for bison
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
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */


/* read in the grammar specification and record it in the format described in gram.h.
  All guards are copied into the fguard file and all actions into faction,
  in each case forming the body of a C function (yyguard or yyaction)
  which contains a switch statement to decide which guard or action to execute.

The entry point is reader().  */

#include <stdio.h>
#include <ctype.h>
#include "system.h"
#include "files.h"
#include "new.h"
#include "symtab.h"
#include "lex.h"
#include "gram.h"
#include "machine.h"

#define	LTYPESTR	"\n#ifndef YYLTYPE\ntypedef\n  struct yyltype\n\
    {\n      int timestamp;\n      int first_line;\n      int first_column;\
\n      int last_line;\n      int last_column;\n      char *text;\n   }\n\
  yyltype;\n\n#define YYLTYPE yyltype\n#endif\n\n"

/* Number of slots allocated (but not necessarily used yet) in `rline'  */
int rline_allocated;

extern char *program_name;
extern int definesflag;
extern int nolinesflag;
extern int noparserflag;
extern int rawtoknumflag;
extern bucket *symval;
extern int numval;
extern int expected_conflicts;
extern char *token_buffer;

extern void init_lex();
extern void tabinit();
extern void output_headers();
extern void output_trailers();
extern void free_symtab();
extern void open_extra_files();
extern char *int_to_string();
extern char *printable_version();
extern void fatal();
extern void fatals();
extern void warn();
extern void warni();
extern void warns();
extern void warnss();
extern void warnsss();
extern void unlex();
extern void done();

extern int skip_white_space();
extern int parse_percent_token();
extern int lex();

void reader_output_yylsp();
void read_declarations();
void copy_definition();
void parse_token_decl();
void parse_start_decl();
void parse_type_decl();
void parse_assoc_decl();
void parse_union_decl();
void parse_expect_decl();
void parse_thong_decl();
void copy_action();
void readgram();
void record_rule_line();
void packsymbols();
void output_token_defines();
void packgram();
int read_signed_integer();
static int get_type();

typedef
  struct symbol_list
    {
      struct symbol_list *next;
      bucket *sym;
      bucket *ruleprec;
    }
  symbol_list;



int lineno;
symbol_list *grammar;
int start_flag;
bucket *startval;
char **tags;
int *user_toknums;

/* Nonzero if components of semantic values are used, implying
   they must be unions.  */
static int value_components_used;

static int typed;  /* nonzero if %union has been seen.  */

static int lastprec;  /* incremented for each %left, %right or %nonassoc seen */

static int gensym_count;  /* incremented for each generated symbol */

static bucket *errtoken;

/* Nonzero if any action or guard uses the @n construct.  */
static int yylsp_needed;

extern char *version_string;


static void
skip_to_char(target)
     int target;
{
  int c;
  if (target == '\n')
    warn("   Skipping to next \\n");
  else
    warni("   Skipping to next %c", target);

  do
    c = skip_white_space();
  while (c != target && c != EOF);
  if (c != EOF) 
    ungetc(c, finput);
}


void
reader()
{
  start_flag = 0;
  startval = NULL;  /* start symbol not specified yet. */

#if 0
  translations = 0;  /* initially assume token number translation not needed.  */
#endif
  /* Nowadays translations is always set to 1,
     since we give `error' a user-token-number
     to satisfy the Posix demand for YYERRCODE==256.  */
  translations = 1;

  nsyms = 1;
  nvars = 0;
  nrules = 0;
  nitems = 0;
  rline_allocated = 10;
  rline = NEW2(rline_allocated, short);

  typed = 0;
  lastprec = 0;

  gensym_count = 0;

  semantic_parser = 0;
  pure_parser = 0;
  yylsp_needed = 0;

  grammar = NULL;

  init_lex();
  lineno = 1;

  /* initialize the symbol table.  */
  tabinit();
  /* construct the error token */
  errtoken = getsym("error");
  errtoken->class = STOKEN;
  errtoken->user_token_number = 256; /* Value specified by posix.  */
  /* construct a token that represents all undefined literal tokens. */
  /* it is always token number 2.  */
  getsym("$undefined.")->class = STOKEN;
  /* Read the declaration section.  Copy %{ ... %} groups to ftable and fdefines file.
     Also notice any %token, %left, etc. found there.  */
  if (noparserflag) 
    fprintf(ftable, "\n/*  Bison-generated parse tables, made from %s\n",
		infile);
  else
    fprintf(ftable, "\n/*  A Bison parser, made from %s\n", infile);
  fprintf(ftable, " by  %s  */\n\n", version_string);
  fprintf(ftable, "#define YYBISON 1  /* Identify Bison output.  */\n\n");
  read_declarations();
  /* start writing the guard and action files, if they are needed.  */
  output_headers();
  /* read in the grammar, build grammar in list form.  write out guards and actions.  */
  readgram();
  /* Now we know whether we need the line-number stack.
     If we do, write its type into the .tab.h file.  */
  if (fdefines)
    reader_output_yylsp(fdefines);
  /* write closing delimiters for actions and guards.  */
  output_trailers();
  if (yylsp_needed)
    fprintf(ftable, "#define YYLSP_NEEDED\n\n");
  /* assign the symbols their symbol numbers.
     Write #defines for the token symbols into fdefines if requested.  */
  packsymbols();
  /* convert the grammar into the format described in gram.h.  */
  packgram();
  /* free the symbol table data structure
     since symbols are now all referred to by symbol number.  */
  free_symtab();
}

void
reader_output_yylsp(f)
     FILE *f;
{
  if (yylsp_needed)
    fprintf(f, LTYPESTR);
}

/* read from finput until %% is seen.  Discard the %%.
Handle any % declarations,
and copy the contents of any %{ ... %} groups to fattrs.  */

void
read_declarations ()
{
  register int c;
  register int tok;

  for (;;)
    {
      c = skip_white_space();

      if (c == '%')
	{
	  tok = parse_percent_token();

	  switch (tok)
	    {
	    case TWO_PERCENTS:
	      return;

	    case PERCENT_LEFT_CURLY:
	      copy_definition();
	      break;

	    case TOKEN:
	      parse_token_decl (STOKEN, SNTERM);
	      break;
	
	    case NTERM:
	      parse_token_decl (SNTERM, STOKEN);
	      break;
	
	    case TYPE:
	      parse_type_decl();
	      break;
	
	    case START:
	      parse_start_decl();
	      break;
	
	    case UNION:
	      parse_union_decl();
	      break;
	
	    case EXPECT:
	      parse_expect_decl();
	      break;
	    case THONG:
	      parse_thong_decl();
	      break;
	    case LEFT:
	      parse_assoc_decl(LEFT_ASSOC);
	      break;

	    case RIGHT:
	      parse_assoc_decl(RIGHT_ASSOC);
	      break;

	    case NONASSOC:
	      parse_assoc_decl(NON_ASSOC);
	      break;

	    case SEMANTIC_PARSER:
	      if (semantic_parser == 0)
		{
		  semantic_parser = 1;
		  open_extra_files();
		}
	      break;

	    case PURE_PARSER:
	      pure_parser = 1;
	      break;

	    case NOOP:
	      break;

	    default:
	      warns("unrecognized: %s", token_buffer);
	      skip_to_char('%');
	  }
	}
      else if (c == EOF)
        fatal("no input grammar");
      else
	{
		char buff[100];
		sprintf(buff, "unknown character: %s", printable_version(c)); 
		warn(buff);
		skip_to_char('%');
	}
    }
}


/* copy the contents of a %{ ... %} into the definitions file.
The %{ has already been read.  Return after reading the %}.  */

void
copy_definition ()
{
  register int c;
  register int match;
  register int ended;
  register int after_percent;  /* -1 while reading a character if prev char was % */
  int cplus_comment;

  if (!nolinesflag)
    fprintf(fattrs, "#line %d \"%s\"\n", lineno, infile);

  after_percent = 0;

  c = getc(finput);

  for (;;)
    {
      switch (c)
	{
	case '\n':
	  putc(c, fattrs);
	  lineno++;
	  break;

	case '%':
          after_percent = -1;
	  break;
	      
	case '\'':
	case '"':
	  match = c;
	  putc(c, fattrs);
	  c = getc(finput);

	  while (c != match)
	    {
	      if (c == EOF)
		fatal("unterminated string at end of file");
	      if (c == '\n')
		{
		  warn("unterminated string");
		  ungetc(c, finput);
		  c = match;
		  continue;
		}

	      putc(c, fattrs);
	      
	      if (c == '\\')
		{
		  c = getc(finput);
		  if (c == EOF)
		    fatal("unterminated string at end of file");
		  putc(c, fattrs);
		  if (c == '\n')
		    lineno++;
		}

	      c = getc(finput);
	    }

	  putc(c, fattrs);
	  break;

	case '/':
	  putc(c, fattrs);
	  c = getc(finput);
	  if (c != '*' && c != '/')
	    continue;

	  cplus_comment = (c == '/');
	  putc(c, fattrs);
	  c = getc(finput);

	  ended = 0;
	  while (!ended)
	    {
	      if (!cplus_comment && c == '*')
		{
		  while (c == '*')
		    {
		      putc(c, fattrs);
		      c = getc(finput);
		    }

		  if (c == '/')
		    {
		      putc(c, fattrs);
		      ended = 1;
		    }
		}
	      else if (c == '\n')
		{
		  lineno++;
		  putc(c, fattrs);
		  if (cplus_comment)
		    ended = 1;
		  else
		    c = getc(finput);
		}
	      else if (c == EOF)
		fatal("unterminated comment in `%{' definition");
	      else
		{
		  putc(c, fattrs);
		  c = getc(finput);
		}
	    }

	  break;

	case EOF:
	  fatal("unterminated `%{' definition");

	default:
	  putc(c, fattrs);
	}

      c = getc(finput);

      if (after_percent)
	{
	  if (c == '}')
	    return;
	  putc('%', fattrs);
	}
      after_percent = 0;

    }

}



/* parse what comes after %token or %nterm.
For %token, what_is is STOKEN and what_is_not is SNTERM.
For %nterm, the arguments are reversed.  */

void
parse_token_decl (what_is, what_is_not)
     int what_is, what_is_not;
{
  register int token = 0;
  register char *typename = 0;
  register struct bucket *symbol = NULL;  /* pts to symbol being defined */
  int k;

  for (;;)
    {
      if(ungetc(skip_white_space(), finput) == '%')
	return;
      token = lex();
      if (token == COMMA)
	{
	  symbol = NULL;
	  continue;
	}
      if (token == TYPENAME)
	{
	  k = strlen(token_buffer);
	  typename = NEW2(k + 1, char);
	  strcpy(typename, token_buffer);
	  value_components_used = 1;
	  symbol = NULL;
	}
      else if (token == IDENTIFIER && *symval->tag == '\"'
		&& symbol) 
	{
	  translations = 1;
	  symval->class = STOKEN;
	  symval->type_name = typename;
	  symval->user_token_number = symbol->user_token_number;
	  symbol->user_token_number = SALIAS;

	  symval->alias = symbol;	
	  symbol->alias = symval;	
	  symbol = NULL;

 	  nsyms--;   /* symbol and symval combined are only one symbol */
	}
      else if (token == IDENTIFIER)
	{
	  int oldclass = symval->class;
	  symbol = symval;

	  if (symbol->class == what_is_not)
	    warns("symbol %s redefined", symbol->tag);
	  symbol->class = what_is;
	  if (what_is == SNTERM && oldclass != SNTERM)
	    symbol->value = nvars++;

	  if (typename)
	    {
	      if (symbol->type_name == NULL)
		symbol->type_name = typename;
	      else if (strcmp(typename, symbol->type_name) != 0)
		warns("type redeclaration for %s", symbol->tag);
	    }
	}
      else if (symbol && token == NUMBER)
        {
	  symbol->user_token_number = numval;
	  translations = 1;
        }
      else
	{
	  warnss("`%s' is invalid in %s",
		token_buffer, 
		(what_is == STOKEN) ? "%token" : "%nterm");
	  skip_to_char('%');
	}
    }

}

/* parse what comes after %thong 
	the full syntax is
		%thong <type> token number literal
 the <type> or number may be omitted.  The number specifies the
 user_token_number.

 Two symbols are entered in the table, one for the token symbol and
 one for the literal.  Both are given the <type>, if any, from the declaration.
 The ->user_token_number of the first is SALIAS and the ->user_token_number
 of the second is set to the number, if any, from the declaration.
 The two symbols are linked via pointers in their ->alias fields.
 
 during output_defines_table, the symbol is reported
 thereafter, only the literal string is retained
 it is the literal string that is output to yytname
*/

void
parse_thong_decl ()
{
  register int token;
  register struct bucket *symbol;
  register char *typename = 0;
  int k, usrtoknum;

  translations = 1;
  token = lex();		/* fetch typename or first token */
  if (token == TYPENAME) {
    k = strlen(token_buffer);
    typename = NEW2(k + 1, char);
    strcpy(typename, token_buffer);
    value_components_used = 1;
    token = lex();		/* fetch first token */
  }

  /* process first token */

  if (token != IDENTIFIER) 
    {
      warns("unrecognized item %s, expected an identifier", 
	    token_buffer);
      skip_to_char('%');
      return;
    }
  symval->class = STOKEN;
  symval->type_name = typename;
  symval->user_token_number = SALIAS;
  symbol = symval;

  token = lex();		/* get number or literal string */
	
  if (token == NUMBER) {
    usrtoknum = numval;
    token = lex();		/* okay, did number, now get literal */
  }
  else usrtoknum = 0;

  /* process literal string token */

  if (token != IDENTIFIER || *symval->tag != '\"') 
    {
      warns("expected string constant instead of %s", 
	    token_buffer);
      skip_to_char('%');
      return;
    }
  symval->class = STOKEN;
  symval->type_name = typename;
  symval->user_token_number = usrtoknum;

  symval->alias = symbol;	
  symbol->alias = symval;	

  nsyms--;			/* symbol and symval combined are only one symbol */
}


/* parse what comes after %start */

void
parse_start_decl ()
{
  if (start_flag)
    warn("multiple %start declarations");
  if (lex() != IDENTIFIER)
    warn("invalid %start declaration");
  else
    {
      start_flag = 1;
      startval = symval;
    }
}



/* read in a %type declaration and record its information for get_type_name to access */

void
parse_type_decl ()
{
  register int k;
  register char *name;

  if (lex() != TYPENAME)
    {
      warn("%type declaration has no <typename>");
      skip_to_char('%');
      return;
    }

  k = strlen(token_buffer);
  name = NEW2(k + 1, char);
  strcpy(name, token_buffer);

  for (;;)
    {
      register int t;

      if(ungetc(skip_white_space(), finput) == '%')
	return;

      t = lex();

      switch (t)
	{

	case COMMA:
	case SEMICOLON:
	  break;

	case IDENTIFIER:
	  if (symval->type_name == NULL)
	    symval->type_name = name;
	  else if (strcmp(name, symval->type_name) != 0)
	    warns("type redeclaration for %s", symval->tag);

	  break;

	default:
	  warns("invalid %%type declaration due to item: `%s'", token_buffer);
	  skip_to_char('%');
	}
    }
}



/* read in a %left, %right or %nonassoc declaration and record its information.  */
/* assoc is either LEFT_ASSOC, RIGHT_ASSOC or NON_ASSOC.  */

void
parse_assoc_decl (assoc)
int assoc;
{
  register int k;
  register char *name = NULL;
  register int prev = 0;

  lastprec++;  /* Assign a new precedence level, never 0.  */

  for (;;)
    {
      register int t;

      if(ungetc(skip_white_space(), finput) == '%')
	return;

      t = lex();

      switch (t)
	{

	case TYPENAME:
	  k = strlen(token_buffer);
	  name = NEW2(k + 1, char);
	  strcpy(name, token_buffer);
	  break;

	case COMMA:
	  break;

	case IDENTIFIER:
	  if (symval->prec != 0)
	    warns("redefining precedence of %s", symval->tag);
	  symval->prec = lastprec;
	  symval->assoc = assoc;
	  if (symval->class == SNTERM)
	    warns("symbol %s redefined", symval->tag);
	  symval->class = STOKEN;
	  if (name)
	    { /* record the type, if one is specified */
	      if (symval->type_name == NULL)
		symval->type_name = name;
	      else if (strcmp(name, symval->type_name) != 0)
		warns("type redeclaration for %s", symval->tag);
	    }
	  break;

	case NUMBER:
	  if (prev == IDENTIFIER)
            {
	      symval->user_token_number = numval;
	      translations = 1;
            }
          else	  
            {
	      warns("invalid text (%s) - number should be after identifier", 
			token_buffer);
	      skip_to_char('%');
            }
	  break;

	case SEMICOLON:
	  return;

	default:
	  warns("unexpected item: %s", token_buffer);
	  skip_to_char('%');
	}

      prev = t;

    }
}



/* copy the union declaration into fattrs (and fdefines),
   where it is made into the
   definition of YYSTYPE, the type of elements of the parser value stack.  */

void
parse_union_decl()
{
  register int c;
  register int count;
  register int in_comment;
  int cplus_comment;

  if (typed)
    warn("multiple %union declarations");

  typed = 1;

  if (!nolinesflag)
    fprintf(fattrs, "\n#line %d \"%s\"\n", lineno, infile);
  else
    fprintf(fattrs, "\n");

  fprintf(fattrs, "typedef union");
  if (fdefines)
    fprintf(fdefines, "typedef union");

  count = 0;
  in_comment = 0;

  c = getc(finput);

  while (c != EOF)
    {
      putc(c, fattrs);
      if (fdefines)
	putc(c, fdefines);

      switch (c)
	{
	case '\n':
	  lineno++;
	  break;

	case '/':
	  c = getc(finput);
	  if (c != '*' && c != '/')
	    ungetc(c, finput);
	  else
	    {
	      putc(c, fattrs);
	      if (fdefines)
		putc(c, fdefines);
	      cplus_comment = (c == '/');
	      in_comment = 1;
	      c = getc(finput);
	      while (in_comment)
		{
		  putc(c, fattrs);
		  if (fdefines)
		    putc(c, fdefines);

		  if (c == '\n')
		    {
		      lineno++;
		      if (cplus_comment)
			{
			  in_comment = 0;
			  break;
			}
		    }
		  if (c == EOF)
		    fatal("unterminated comment at end of file");

		  if (!cplus_comment && c == '*')
		    {
		      c = getc(finput);
		      if (c == '/')
			{
			  putc('/', fattrs);
			  if (fdefines)
			    putc('/', fdefines);
			  in_comment = 0;
			}
		    }
		  else
		    c = getc(finput);
		}
	    }
	  break;


	case '{':
	  count++;
	  break;

	case '}':
	  if (count == 0)
	    warn ("unmatched close-brace (`}')");
	  count--;
	  if (count <= 0)
	    {
	      fprintf(fattrs, " YYSTYPE;\n");
	      if (fdefines)
		fprintf(fdefines, " YYSTYPE;\n");
	      /* JF don't choke on trailing semi */
	      c=skip_white_space();
	      if(c!=';') ungetc(c,finput);
	      return;
	    }
	}

      c = getc(finput);
    }
}

/* parse the declaration %expect N which says to expect N
   shift-reduce conflicts.  */

void
parse_expect_decl()
{
  register int c;
  register int count;
  char buffer[20];

  c = getc(finput);
  while (c == ' ' || c == '\t')
    c = getc(finput);

  count = 0;
  while (c >= '0' && c <= '9')
    {
      if (count < 20)
	buffer[count++] = c;
      c = getc(finput);
    }
  buffer[count] = 0;

  ungetc (c, finput);

  if (count <= 0 || count > 10)
	warn("argument of %expect is not an integer");
  expected_conflicts = atoi (buffer);
}

/* that's all of parsing the declaration section */

/* Get the data type (alternative in the union) of the value for symbol n in rule rule.  */

char *
get_type_name(n, rule)
int n;
symbol_list *rule;
{
  static char *msg = "invalid $ value";

  register int i;
  register symbol_list *rp;

  if (n < 0)
    {
      warn(msg);
      return NULL;
    }

  rp = rule;
  i = 0;

  while (i < n)
    {
      rp = rp->next;
      if (rp == NULL || rp->sym == NULL)
	{
	  warn(msg);
	  return NULL;
	}
      i++;
    }

  return (rp->sym->type_name);
}


/* after %guard is seen in the input file,
copy the actual guard into the guards file.
If the guard is followed by an action, copy that into the actions file.
stack_offset is the number of values in the current rule so far,
which says where to find $0 with respect to the top of the stack,
for the simple parser in which the stack is not popped until after the guard is run.  */

void
copy_guard(rule, stack_offset)
symbol_list *rule;
int stack_offset;
{
  register int c;
  register int n;
  register int count;
  register int match;
  register int ended;
  register char *type_name;
  int brace_flag = 0;
  int cplus_comment;

  /* offset is always 0 if parser has already popped the stack pointer */
  if (semantic_parser) stack_offset = 0;

  fprintf(fguard, "\ncase %d:\n", nrules);
  if (!nolinesflag)
    fprintf(fguard, "#line %d \"%s\"\n", lineno, infile);
  putc('{', fguard);

  count = 0;
  c = getc(finput);

  while (brace_flag ? (count > 0) : (c != ';'))
    {
      switch (c)
	{
	case '\n':
	  putc(c, fguard);
	  lineno++;
	  break;

	case '{':
	  putc(c, fguard);
	  brace_flag = 1;
	  count++;
	  break;

	case '}':
	  putc(c, fguard);
	  if (count > 0)
	    count--;
	  else 
	    {
	      warn("unmatched right brace (`}')");
	      c = getc(finput);	/* skip it */
	    }
          break;

	case '\'':
	case '"':
	  match = c;
	  putc(c, fguard);
	  c = getc(finput);

	  while (c != match)
	    {
	      if (c == EOF)
		fatal("unterminated string at end of file");
	      if (c == '\n') 
		{
		  warn("unterminated string");
		  ungetc(c, finput);
		  c = match;		/* invent terminator */
		  continue;
		}

	      putc(c, fguard);
	      
	      if (c == '\\')
		{
		  c = getc(finput);
		  if (c == EOF)
		    fatal("unterminated string");
		  putc(c, fguard);
		  if (c == '\n')
		    lineno++;
		}

	      c = getc(finput);
	    }

	  putc(c, fguard);
	  break;

	case '/':
	  putc(c, fguard);
	  c = getc(finput);
	  if (c != '*' && c != '/')
	    continue;

	  cplus_comment = (c == '/');
	  putc(c, fguard);
	  c = getc(finput);

	  ended = 0;
	  while (!ended)
	    {
	      if (!cplus_comment && c == '*')
		{
		  while (c == '*')
		    {
		      putc(c, fguard);
		      c = getc(finput);
		    }

		  if (c == '/')
		    {
		      putc(c, fguard);
		      ended = 1;
		    }
		}
	      else if (c == '\n')
		{
		  lineno++;
		  putc(c, fguard);
		  if (cplus_comment)
		    ended = 1;
		  else
		    c = getc(finput);
		}
	      else if (c == EOF)
		fatal("unterminated comment");
	      else
		{
		  putc(c, fguard);
		  c = getc(finput);
		}
	    }

	  break;

	case '$':
	  c = getc(finput);
	  type_name = NULL;

	  if (c == '<')
	    {
	      register char *cp = token_buffer;

	      while ((c = getc(finput)) != '>' && c > 0)
		*cp++ = c;
	      *cp = 0;
	      type_name = token_buffer;

	      c = getc(finput);
	    }

	  if (c == '$')
	    {
	      fprintf(fguard, "yyval");
	      if (!type_name) type_name = rule->sym->type_name;
	      if (type_name)
		fprintf(fguard, ".%s", type_name);
	      if(!type_name && typed)
		warns("$$ of `%s' has no declared type", rule->sym->tag);
	    }

	  else if (isdigit(c) || c == '-')
	    {
	      ungetc (c, finput);
	      n = read_signed_integer(finput);
	      c = getc(finput);

	      if (!type_name && n > 0)
		type_name = get_type_name(n, rule);

	      fprintf(fguard, "yyvsp[%d]", n - stack_offset);
	      if (type_name)
		fprintf(fguard, ".%s", type_name);
	      if(!type_name && typed)
		warnss("$%s of `%s' has no declared type", int_to_string(n), rule->sym->tag);
	      continue;
	    }
	  else
	    warni("$%s is invalid", printable_version(c));

	  break;

	case '@':
	  c = getc(finput);
	  if (isdigit(c) || c == '-')
	    {
	      ungetc (c, finput);
	      n = read_signed_integer(finput);
	      c = getc(finput);
	    }
	  else
	    {
	      warni("@%s is invalid", printable_version(c));
	      n = 1;
	    }

	  fprintf(fguard, "yylsp[%d]", n - stack_offset);
	  yylsp_needed = 1;

	  continue;

	case EOF:
	  fatal("unterminated %%guard clause");

	default:
	  putc(c, fguard);
	}

      if (c != '}' || count != 0)
	c = getc(finput);
    }

  c = skip_white_space();

  fprintf(fguard, ";\n    break;}");
  if (c == '{')
    copy_action(rule, stack_offset);
  else if (c == '=')
    {
      c = getc(finput);	/* why not skip_white_space -wjh */
      if (c == '{')
	copy_action(rule, stack_offset);
    }
  else
    ungetc(c, finput);
}



/* Assuming that a { has just been seen, copy everything up to the matching }
into the actions file.
stack_offset is the number of values in the current rule so far,
which says where to find $0 with respect to the top of the stack.  */

void
copy_action(rule, stack_offset)
symbol_list *rule;
int stack_offset;
{
  register int c;
  register int n;
  register int count;
  register int match;
  register int ended;
  register char *type_name;
  int cplus_comment;

  /* offset is always 0 if parser has already popped the stack pointer */
  if (semantic_parser) stack_offset = 0;

  fprintf(faction, "\ncase %d:\n", nrules);
  if (!nolinesflag)
    fprintf(faction, "#line %d \"%s\"\n", lineno, infile);
  putc('{', faction);

  count = 1;
  c = getc(finput);

  while (count > 0)
    {
      while (c != '}')
        {
          switch (c)
	    {
	    case '\n':
	      putc(c, faction);
	      lineno++;
	      break;

	    case '{':
	      putc(c, faction);
	      count++;
	      break;

	    case '\'':
	    case '"':
	      match = c;
	      putc(c, faction);
	      c = getc(finput);

	      while (c != match)
		{
		  if (c == '\n')
		    {
		      warn("unterminated string");
		      ungetc(c, finput);
		      c = match;
		      continue;
		    }
		  else if (c == EOF)
		    fatal("unterminated string at end of file");

		  putc(c, faction);

		  if (c == '\\')
		    {
		      c = getc(finput);
		      if (c == EOF)
			fatal("unterminated string");
		      putc(c, faction);
		      if (c == '\n')
			lineno++;
		    }

		  c = getc(finput);
		}

	      putc(c, faction);
	      break;

	    case '/':
	      putc(c, faction);
	      c = getc(finput);
	      if (c != '*' && c != '/')
		continue;

	      cplus_comment = (c == '/');
	      putc(c, faction);
	      c = getc(finput);

	      ended = 0;
	      while (!ended)
		{
		  if (!cplus_comment && c == '*')
		    {
		      while (c == '*')
		        {
			  putc(c, faction);
			  c = getc(finput);
			}

		      if (c == '/')
			{
			  putc(c, faction);
			  ended = 1;
			}
		    }
		  else if (c == '\n')
		    {
		      lineno++;
		      putc(c, faction);
		      if (cplus_comment)
			ended = 1;
		      else
		        c = getc(finput);
		    }
		  else if (c == EOF)
		    fatal("unterminated comment");
		  else
		    {
		      putc(c, faction);
		      c = getc(finput);
		    }
		}

	      break;

	    case '$':
	      c = getc(finput);
	      type_name = NULL;

	      if (c == '<')
		{
		  register char *cp = token_buffer;

		  while ((c = getc(finput)) != '>' && c > 0)
		    *cp++ = c;
		  *cp = 0;
		  type_name = token_buffer;
		  value_components_used = 1;

		  c = getc(finput);
		}
	      if (c == '$')
		{
		  fprintf(faction, "yyval");
		  if (!type_name) type_name = get_type_name(0, rule);
		  if (type_name)
		    fprintf(faction, ".%s", type_name);
		  if(!type_name && typed)	
		    warns("$$ of `%s' has no declared type", rule->sym->tag);
		}
	      else if (isdigit(c) || c == '-')
		{
		  ungetc (c, finput);
		  n = read_signed_integer(finput);
		  c = getc(finput);

		  if (!type_name && n > 0)
		    type_name = get_type_name(n, rule);

		  fprintf(faction, "yyvsp[%d]", n - stack_offset);
		  if (type_name)
		    fprintf(faction, ".%s", type_name);
		  if(!type_name && typed)	
		    warnss("$%s of `%s' has no declared type", 
				int_to_string(n), rule->sym->tag);
		  continue;
		}
	      else
		warni("$%s is invalid", printable_version(c));

	      break;

	    case '@':
	      c = getc(finput);
	      if (isdigit(c) || c == '-')
		{
		  ungetc (c, finput);
		  n = read_signed_integer(finput);
		  c = getc(finput);
		}
	      else
		{
		  warn("invalid @-construct");
		  n = 1;
		}

	      fprintf(faction, "yylsp[%d]", n - stack_offset);
	      yylsp_needed = 1;

	      continue;

	    case EOF:
	      fatal("unmatched `{'");

	    default:
	      putc(c, faction);
	    }

          c = getc(finput);
        }

      /* above loop exits when c is '}' */

      if (--count)
        {
	  putc(c, faction);
	  c = getc(finput);
	}
    }

  fprintf(faction, ";\n    break;}");
}



/* generate a dummy symbol, a nonterminal,
whose name cannot conflict with the user's names. */

bucket *
gensym()
{
  register bucket *sym;

  sprintf (token_buffer, "@%d", ++gensym_count);
  sym = getsym(token_buffer);
  sym->class = SNTERM;
  sym->value = nvars++;
  return (sym);
}

/* Parse the input grammar into a one symbol_list structure.
Each rule is represented by a sequence of symbols: the left hand side
followed by the contents of the right hand side, followed by a null pointer
instead of a symbol to terminate the rule.
The next symbol is the lhs of the following rule.

All guards and actions are copied out to the appropriate files,
labelled by the rule number they apply to.  */

void
readgram()
{
  register int t;
  register bucket *lhs;
  register symbol_list *p;
  register symbol_list *p1;
  register bucket *bp;

  symbol_list *crule;	/* points to first symbol_list of current rule.  */
			/* its symbol is the lhs of the rule.   */
  symbol_list *crule1;  /* points to the symbol_list preceding crule.  */

  p1 = NULL;

  t = lex();

  while (t != TWO_PERCENTS && t != ENDFILE)
    {
      if (t == IDENTIFIER || t == BAR)
	{
	  register int actionflag = 0;
	  int rulelength = 0;  /* number of symbols in rhs of this rule so far  */
	  int xactions = 0;	/* JF for error checking */
	  bucket *first_rhs = 0;

	  if (t == IDENTIFIER)
	    {
	      lhs = symval;

	      if (!start_flag)
		{
		  startval = lhs;
		  start_flag = 1;
		}
    
	      t = lex();
	      if (t != COLON)
		{
		  warn("ill-formed rule: initial symbol not followed by colon");
		  unlex(t);
		}
	    }

	  if (nrules == 0 && t == BAR)
	    {
	      warn("grammar starts with vertical bar");
	      lhs = symval;	/* BOGUS: use a random symval */
	    }
	  /* start a new rule and record its lhs.  */

	  nrules++;
	  nitems++;

	  record_rule_line ();

	  p = NEW(symbol_list);
	  p->sym = lhs;

	  crule1 = p1;
	  if (p1)
	    p1->next = p;
	  else
	    grammar = p;

	  p1 = p;
	  crule = p;

	  /* mark the rule's lhs as a nonterminal if not already so.  */

	  if (lhs->class == SUNKNOWN)
	    {
	      lhs->class = SNTERM;
	      lhs->value = nvars;
	      nvars++;
	    }
	  else if (lhs->class == STOKEN)
	    warns("rule given for %s, which is a token", lhs->tag);

	  /* read the rhs of the rule.  */

	  for (;;)
	    {
	      t = lex();
	      if (t == PREC)
		{
		  t = lex();
		  crule->ruleprec = symval;
		  t = lex();
		}

	      if (! (t == IDENTIFIER || t == LEFT_CURLY)) break;

	      /* If next token is an identifier, see if a colon follows it.
		 If one does, exit this rule now.  */
	      if (t == IDENTIFIER)
		{
		  register bucket *ssave;
		  register int t1;

		  ssave = symval;
		  t1 = lex();
		  unlex(t1);
		  symval = ssave;
		  if (t1 == COLON) break;

		  if(!first_rhs)	/* JF */
		    first_rhs = symval;
		  /* Not followed by colon =>
		     process as part of this rule's rhs.  */
		}

	      /* If we just passed an action, that action was in the middle
		 of a rule, so make a dummy rule to reduce it to a
		 non-terminal.  */
	      if (actionflag)
		{
		  register bucket *sdummy;

		  /* Since the action was written out with this rule's */
		  /* number, we must give the new rule this number */
		  /* by inserting the new rule before it.  */

		  /* Make a dummy nonterminal, a gensym.  */
		  sdummy = gensym();

		  /* Make a new rule, whose body is empty,
		     before the current one, so that the action
		     just read can belong to it.  */
		  nrules++;
		  nitems++;
		  record_rule_line ();
		  p = NEW(symbol_list);
		  if (crule1)
		    crule1->next = p;
		  else grammar = p;
		  p->sym = sdummy;
		  crule1 = NEW(symbol_list);
		  p->next = crule1;
		  crule1->next = crule;

		  /* insert the dummy generated by that rule into this rule.  */
		  nitems++;
		  p = NEW(symbol_list);
		  p->sym = sdummy;
		  p1->next = p;
		  p1 = p;

		  actionflag = 0;
		}

	      if (t == IDENTIFIER)
		{
		  nitems++;
		  p = NEW(symbol_list);
		  p->sym = symval;
		  p1->next = p;
		  p1 = p;
		}
	      else /* handle an action.  */
		{
		  copy_action(crule, rulelength);
		  actionflag = 1;
		  xactions++;	/* JF */
		}
	      rulelength++;
	    }	/* end of  read rhs of rule */

	  /* Put an empty link in the list to mark the end of this rule  */
	  p = NEW(symbol_list);
	  p1->next = p;
	  p1 = p;

	  if (t == PREC)
	    {
	      warn("two @prec's in a row");
	      t = lex();
	      crule->ruleprec = symval;
	      t = lex();
	    }
	  if (t == GUARD)
	    {
	      if (! semantic_parser)
		warn("%%guard present but %%semantic_parser not specified");

	      copy_guard(crule, rulelength);
	      t = lex();
	    }
	  else if (t == LEFT_CURLY)
	    {
		/* This case never occurs -wjh */
	      if (actionflag)  warn("two actions at end of one rule");
	      copy_action(crule, rulelength);
	      actionflag = 1;
	      xactions++;	/* -wjh */
	      t = lex();
	    }
	  /* If $$ is being set in default way,
	     warn if any type mismatch.  */
	  else if (!xactions && first_rhs && lhs->type_name != first_rhs->type_name)
	    {
	      if (lhs->type_name == 0 || first_rhs->type_name == 0
		  || strcmp(lhs->type_name,first_rhs->type_name))
		warnss("type clash (`%s' `%s') on default action",
			lhs->type_name ? lhs->type_name : "",
			first_rhs->type_name ? first_rhs->type_name : "");
	    }
	  /* Warn if there is no default for $$ but we need one.  */
	  else if (!xactions && !first_rhs && lhs->type_name != 0)
	    warn("empty rule for typed nonterminal, and no action");
	  if (t == SEMICOLON)
	    t = lex();
	}    
#if 0
  /* these things can appear as alternatives to rules.  */
/* NO, they cannot.
	a) none of the documentation allows them
	b) most of them scan forward until finding a next %
		thus they may swallow lots of intervening rules
*/
      else if (t == TOKEN)
	{
	  parse_token_decl(STOKEN, SNTERM);
	  t = lex();
	}
      else if (t == NTERM)
	{
	  parse_token_decl(SNTERM, STOKEN);
	  t = lex();
	}
      else if (t == TYPE)
	{
	  t = get_type();
	}
      else if (t == UNION)
	{
	  parse_union_decl();
	  t = lex();
	}
      else if (t == EXPECT)
	{
	  parse_expect_decl();
	  t = lex();
	}
      else if (t == START)
	{
	  parse_start_decl();
	  t = lex();
	}
#endif

      else
	{
	  warns("invalid input: %s", token_buffer);
	  t = lex();
	}
    }

  /* grammar has been read.  Do some checking */

  if (nsyms > MAXSHORT)
    fatals("too many symbols (tokens plus nonterminals); maximum %s",
	   int_to_string(MAXSHORT));
  if (nrules == 0)
    fatal("no rules in the input grammar");

  if (typed == 0	/* JF put out same default YYSTYPE as YACC does */
      && !value_components_used)
    {
      /* We used to use `unsigned long' as YYSTYPE on MSDOS,
	 but it seems better to be consistent.
	 Most programs should declare their own type anyway.  */
      fprintf(fattrs, "#ifndef YYSTYPE\n#define YYSTYPE int\n#endif\n");
      if (fdefines)
	fprintf(fdefines, "#ifndef YYSTYPE\n#define YYSTYPE int\n#endif\n");
    }

  /* Report any undefined symbols and consider them nonterminals.  */

  for (bp = firstsymbol; bp; bp = bp->next)
    if (bp->class == SUNKNOWN)
      {
	warns("symbol %s is used, but is not defined as a token and has no rules",
			bp->tag);
	bp->class = SNTERM;
	bp->value = nvars++;
      }

  ntokens = nsyms - nvars;
}


void
record_rule_line ()
{
  /* Record each rule's source line number in rline table.  */

  if (nrules >= rline_allocated)
    {
      rline_allocated = nrules * 2;
      rline = (short *) xrealloc (rline,
				 rline_allocated * sizeof (short));
    }
  rline[nrules] = lineno;
}


/* read in a %type declaration and record its information for get_type_name to access */
/* this is unused.  it is only called from the #if 0 part of readgram */
static int
get_type()
{
  register int k;
  register int t;
  register char *name;

  t = lex();

  if (t != TYPENAME) 
    {
      warn("ill-formed %type declaration");
      return t;
    }

  k = strlen(token_buffer);
  name = NEW2(k + 1, char);
  strcpy(name, token_buffer);

  for (;;)
    {
      t = lex();

      switch (t)
	{
	case SEMICOLON:
	  return (lex());

	case COMMA:
	  break;

	case IDENTIFIER:
	  if (symval->type_name == NULL)
	    symval->type_name = name;
	  else if (strcmp(name, symval->type_name) != 0)
	    warns("type redeclaration for %s", symval->tag);

	  break;

	default:
	  return (t);
	}
    }
}



/* assign symbol numbers, and write definition of token names into fdefines.
Set up vectors tags and sprec of names and precedences of symbols.  */

void
packsymbols()
{
  register bucket *bp;
  register int tokno = 1;
  register int i;
  register int last_user_token_number;

  /* int lossage = 0; JF set but not used */

  tags = NEW2(nsyms + 1, char *);
  tags[0] = "$";
  user_toknums = NEW2(nsyms + 1, int);
  user_toknums[0] = 0;

  sprec = NEW2(nsyms, short);
  sassoc = NEW2(nsyms, short);

  max_user_token_number = 256;
  last_user_token_number = 256;

  for (bp = firstsymbol; bp; bp = bp->next)
    {
      if (bp->class == SNTERM)
	{
	  bp->value += ntokens;
	}
      else if (bp->alias)
	{
		/* this symbol and its alias are a single token defn.
		  allocate a tokno, and assign to both
		  check agreement of ->prec and ->assoc fields 
			and make both the same
		*/
		if (bp->value == 0)
			bp->value = bp->alias->value = tokno++;

		if (bp->prec != bp->alias->prec) {
			if (bp->prec != 0 && bp->alias->prec != 0
					&& bp->user_token_number == SALIAS)
				warnss("conflicting precedences for %s and %s",
					bp->tag, bp->alias->tag);
			if (bp->prec != 0) bp->alias->prec = bp->prec;
			else bp->prec = bp->alias->prec;
		}

		if (bp->assoc != bp->alias->assoc) {
			if (bp->assoc != 0 && bp->alias->assoc != 0
					&& bp->user_token_number == SALIAS)
				warnss("conflicting assoc values for %s and %s",
					bp->tag, bp->alias->tag);
			if (bp->assoc != 0) bp->alias->assoc = bp->assoc;
			else bp->assoc = bp->alias->assoc;
		}

		if (bp->user_token_number == SALIAS)
			continue;  /* do not do processing below for SALIASs */

 	}
      else  /* bp->class == STOKEN */
	{
	  bp->value = tokno++;
	}

      if (bp->class == STOKEN)
	{
	  if (translations && !(bp->user_token_number))
	    bp->user_token_number = ++last_user_token_number;
	  if (bp->user_token_number > max_user_token_number)
	    max_user_token_number = bp->user_token_number;
	}

      tags[bp->value] = bp->tag;
      user_toknums[bp->value] = bp->user_token_number;
      sprec[bp->value] = bp->prec;
      sassoc[bp->value] = bp->assoc;

    }

  if (translations)
    {
      register int i;

      token_translations = NEW2(max_user_token_number+1, short);

      /* initialize all entries for literal tokens to 2,
	 the internal token number for $undefined.,
	 which represents all invalid inputs.  */
      for (i = 0; i <= max_user_token_number; i++)
        token_translations[i] = 2;      

      for (bp = firstsymbol; bp; bp = bp->next)
        {
          if (bp->value >= ntokens) continue;		  /* non-terminal */
          if (bp->user_token_number == SALIAS) continue;  
          if (token_translations[bp->user_token_number] != 2)
	    warnsss("tokens %s and %s both assigned number %s",
			      tags[token_translations[bp->user_token_number]],
			      bp->tag,
			      int_to_string(bp->user_token_number));
          token_translations[bp->user_token_number] = bp->value;
        }
    }

  error_token_number = errtoken->value;

  if (! noparserflag)
    output_token_defines(ftable);

  if (startval->class == SUNKNOWN)
    fatals("the start symbol %s is undefined", startval->tag);
  else if (startval->class == STOKEN)
    fatals("the start symbol %s is a token", startval->tag);

  start_symbol = startval->value;

  if (definesflag)
    {
      output_token_defines(fdefines);

      if (!pure_parser)
	{
	  if (spec_name_prefix)
	    fprintf(fdefines, "\nextern YYSTYPE %slval;\n", spec_name_prefix);
	  else
	    fprintf(fdefines, "\nextern YYSTYPE yylval;\n");
	}

      if (semantic_parser)
	for (i = ntokens; i < nsyms; i++)
	  {
	    /* don't make these for dummy nonterminals made by gensym.  */
	    if (*tags[i] != '@')
	      fprintf(fdefines, "#define\tNT%s\t%d\n", tags[i], i);
	  }
#if 0
      /* `fdefines' is now a temporary file, so we need to copy its
         contents in `done', so we can't close it here.  */
      fclose(fdefines);
      fdefines = NULL;
#endif
    }
}
      
/* For named tokens, but not literal ones, define the name.  
   The value is the user token number.  
*/
void
output_token_defines(file)
FILE *file;
{
  bucket *bp;
  register char *cp, *symbol;
  register char c;

  for (bp = firstsymbol; bp; bp = bp->next)
    {
      symbol = bp->tag;				/* get symbol */

      if (bp->value >= ntokens) continue;
      if (bp->user_token_number == SALIAS) continue;
      if ('\'' == *symbol) continue;	/* skip literal character */
      if (bp == errtoken) continue; 	/* skip error token */
      if ('\"' == *symbol) 
	{
		/* use literal string only if given a symbol with an alias */
		if (bp->alias)
			symbol = bp->alias->tag;
		else
			continue;
	}

      /* Don't #define nonliteral tokens whose names contain periods.  */
      cp = symbol;
      while ((c = *cp++) && c != '.');
      if (c != '\0')  continue;

      fprintf(file, "#define\t%s\t%d\n", symbol,
		((translations && ! rawtoknumflag) 
			? bp->user_token_number 
			: bp->value));
      if (semantic_parser)
        fprintf(file, "#define\tT%s\t%d\n", symbol, bp->value);
    }

  putc('\n', file);
}



/* convert the rules into the representation using rrhs, rlhs and ritems.  */

void
packgram()
{
  register int itemno;
  register int ruleno;
  register symbol_list *p;
/*  register bucket *bp; JF unused */

  bucket *ruleprec;

  ritem = NEW2(nitems + 1, short);
  rlhs = NEW2(nrules, short) - 1;
  rrhs = NEW2(nrules, short) - 1;
  rprec = NEW2(nrules, short) - 1;
  rprecsym = NEW2(nrules, short) - 1;
  rassoc = NEW2(nrules, short) - 1;

  itemno = 0;
  ruleno = 1;

  p = grammar;
  while (p)
    {
      rlhs[ruleno] = p->sym->value;
      rrhs[ruleno] = itemno;
      ruleprec = p->ruleprec;

      p = p->next;
      while (p && p->sym)
	{
	  ritem[itemno++] = p->sym->value;
	  /* A rule gets by default the precedence and associativity
	     of the last token in it.  */
          if (p->sym->class == STOKEN)
	    {
	      rprec[ruleno] = p->sym->prec;
	      rassoc[ruleno] = p->sym->assoc;
	    }
	  if (p) p = p->next;
	}

      /* If this rule has a %prec,
	 the specified symbol's precedence replaces the default.  */
      if (ruleprec)
	{
          rprec[ruleno] = ruleprec->prec;
          rassoc[ruleno] = ruleprec->assoc;
	  rprecsym[ruleno] = ruleprec->value;
	}

      ritem[itemno++] = -ruleno;
      ruleno++;

      if (p) p = p->next;
    }

  ritem[itemno] = 0;
}

/* Read a signed integer from STREAM and return its value.  */

int
read_signed_integer (stream)
     FILE *stream;
{
  register int c = getc(stream);
  register int sign = 1;
  register int n;

  if (c == '-')
    {
      c = getc(stream);
      sign = -1;
    }
  n = 0;
  while (isdigit(c))
    {
      n = 10*n + (c - '0');
      c = getc(stream);
    }

  ungetc(c, stream);

  return n * sign;
}
