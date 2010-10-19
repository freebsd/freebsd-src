/* m68k.y -- bison grammar for m68k operand parsing
   Copyright 1995, 1996, 1997, 1998, 2001, 2003, 2004, 2005
   Free Software Foundation, Inc.
   Written by Ken Raeburn and Ian Lance Taylor, Cygnus Support

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
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

/* This file holds a bison grammar to parse m68k operands.  The m68k
   has a complicated operand syntax, and gas supports two main
   variations of it.  Using a grammar is probably overkill, but at
   least it makes clear exactly what we do support.  */

%{

#include "as.h"
#include "tc-m68k.h"
#include "m68k-parse.h"
#include "safe-ctype.h"

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror,
   etc), as well as gratuitously global symbol names If other parser
   generators (bison, byacc, etc) produce additional global names that
   conflict at link time, then those parser generators need to be
   fixed instead of adding those names to this list.  */

#define	yymaxdepth m68k_maxdepth
#define	yyparse	m68k_parse
#define	yylex	m68k_lex
#define	yyerror	m68k_error
#define	yylval	m68k_lval
#define	yychar	m68k_char
#define	yydebug	m68k_debug
#define	yypact	m68k_pact	
#define	yyr1	m68k_r1			
#define	yyr2	m68k_r2			
#define	yydef	m68k_def		
#define	yychk	m68k_chk		
#define	yypgo	m68k_pgo		
#define	yyact	m68k_act		
#define	yyexca	m68k_exca
#define yyerrflag m68k_errflag
#define yynerrs	m68k_nerrs
#define	yyps	m68k_ps
#define	yypv	m68k_pv
#define	yys	m68k_s
#define	yy_yys	m68k_yys
#define	yystate	m68k_state
#define	yytmp	m68k_tmp
#define	yyv	m68k_v
#define	yy_yyv	m68k_yyv
#define	yyval	m68k_val
#define	yylloc	m68k_lloc
#define yyreds	m68k_reds		/* With YYDEBUG defined */
#define yytoks	m68k_toks		/* With YYDEBUG defined */
#define yylhs	m68k_yylhs
#define yylen	m68k_yylen
#define yydefred m68k_yydefred
#define yydgoto	m68k_yydgoto
#define yysindex m68k_yysindex
#define yyrindex m68k_yyrindex
#define yygindex m68k_yygindex
#define yytable	 m68k_yytable
#define yycheck	 m68k_yycheck

#ifndef YYDEBUG
#define YYDEBUG 1
#endif

/* Internal functions.  */

static enum m68k_register m68k_reg_parse (char **);
static int yylex (void);
static void yyerror (const char *);

/* The parser sets fields pointed to by this global variable.  */
static struct m68k_op *op;

%}

%union
{
  struct m68k_indexreg indexreg;
  enum m68k_register reg;
  struct m68k_exp exp;
  unsigned long mask;
  int onereg;
  int trailing_ampersand;
}

%token <reg> DR AR FPR FPCR LPC ZAR ZDR LZPC CREG
%token <indexreg> INDEXREG
%token <exp> EXPR

%type <indexreg> zireg zdireg
%type <reg> zadr zdr apc zapc zpc optzapc optczapc
%type <exp> optcexpr optexprc
%type <mask> reglist ireglist reglistpair
%type <onereg> reglistreg
%type <trailing_ampersand> optional_ampersand

%%

/* An operand.  */

operand:
	  generic_operand
	| motorola_operand optional_ampersand
		{
		  op->trailing_ampersand = $2;
		}
	| mit_operand optional_ampersand
		{
		  op->trailing_ampersand = $2;
		}
	;

/* A trailing ampersand(for MAC/EMAC mask addressing).  */
optional_ampersand:
	/* empty */
		{ $$ = 0; }
	| '&'
		{ $$ = 1; }
	;

/* A generic operand.  */

generic_operand:
	  '<' '<'
		{
		  op->mode = LSH;
		}

	| '>' '>'
		{
		  op->mode = RSH;
		}

	| DR
		{
		  op->mode = DREG;
		  op->reg = $1;
		}
	| AR
		{
		  op->mode = AREG;
		  op->reg = $1;
		}
	| FPR
		{
		  op->mode = FPREG;
		  op->reg = $1;
		}
	| FPCR
		{
		  op->mode = CONTROL;
		  op->reg = $1;
		}
	| CREG
		{
		  op->mode = CONTROL;
		  op->reg = $1;
		}
	| EXPR
		{
		  op->mode = ABSL;
		  op->disp = $1;
		}
	| '#' EXPR
		{
		  op->mode = IMMED;
		  op->disp = $2;
		}
	| '&' EXPR
		{
		  op->mode = IMMED;
		  op->disp = $2;
		}
	| reglist
		{
		  op->mode = REGLST;
		  op->mask = $1;
		}
	;

/* An operand in Motorola syntax.  This includes MRI syntax as well,
   which may or may not be different in that it permits commutativity
   of index and base registers, and permits an offset expression to
   appear inside or outside of the parentheses.  */

motorola_operand:
	  '(' AR ')'
		{
		  op->mode = AINDR;
		  op->reg = $2;
		}
	| '(' AR ')' '+'
		{
		  op->mode = AINC;
		  op->reg = $2;
		}
	| '-' '(' AR ')'
		{
		  op->mode = ADEC;
		  op->reg = $3;
		}
	| '(' EXPR ',' zapc ')'
		{
		  op->reg = $4;
		  op->disp = $2;
		  if (($4 >= ZADDR0 && $4 <= ZADDR7)
		      || $4 == ZPC)
		    op->mode = BASE;
		  else
		    op->mode = DISP;
		}
	| '(' zapc ',' EXPR ')'
		{
		  op->reg = $2;
		  op->disp = $4;
		  if (($2 >= ZADDR0 && $2 <= ZADDR7)
		      || $2 == ZPC)
		    op->mode = BASE;
		  else
		    op->mode = DISP;
		}
	| EXPR '(' zapc ')'
		{
		  op->reg = $3;
		  op->disp = $1;
		  if (($3 >= ZADDR0 && $3 <= ZADDR7)
		      || $3 == ZPC)
		    op->mode = BASE;
		  else
		    op->mode = DISP;
		}
	| '(' LPC ')'
		{
		  op->mode = DISP;
		  op->reg = $2;
		}
	| '(' ZAR ')'
		{
		  op->mode = BASE;
		  op->reg = $2;
		}
	| '(' LZPC ')'
		{
		  op->mode = BASE;
		  op->reg = $2;
		}
	| '(' EXPR ',' zapc ',' zireg ')'
		{
		  op->mode = BASE;
		  op->reg = $4;
		  op->disp = $2;
		  op->index = $6;
		}
	| '(' EXPR ',' zapc ',' zpc ')'
		{
		  if ($4 == PC || $4 == ZPC)
		    yyerror (_("syntax error"));
		  op->mode = BASE;
		  op->reg = $6;
		  op->disp = $2;
		  op->index.reg = $4;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		}
	| '(' EXPR ',' zdireg optczapc ')'
		{
		  op->mode = BASE;
		  op->reg = $5;
		  op->disp = $2;
		  op->index = $4;
		}
	| '(' zdireg ',' EXPR ')'
		{
		  op->mode = BASE;
		  op->disp = $4;
		  op->index = $2;
		}
	| EXPR '(' zapc ',' zireg ')'
		{
		  op->mode = BASE;
		  op->reg = $3;
		  op->disp = $1;
		  op->index = $5;
		}
	| '(' zapc ',' zireg ')'
		{
		  op->mode = BASE;
		  op->reg = $2;
		  op->index = $4;
		}
	| EXPR '(' zapc ',' zpc ')'
		{
		  if ($3 == PC || $3 == ZPC)
		    yyerror (_("syntax error"));
		  op->mode = BASE;
		  op->reg = $5;
		  op->disp = $1;
		  op->index.reg = $3;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		}
	| '(' zapc ',' zpc ')'
		{
		  if ($2 == PC || $2 == ZPC)
		    yyerror (_("syntax error"));
		  op->mode = BASE;
		  op->reg = $4;
		  op->index.reg = $2;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		}
	| EXPR '(' zdireg optczapc ')'
		{
		  op->mode = BASE;
		  op->reg = $4;
		  op->disp = $1;
		  op->index = $3;
		}
	| '(' zdireg optczapc ')'
		{
		  op->mode = BASE;
		  op->reg = $3;
		  op->index = $2;
		}
	| '(' '[' EXPR optczapc ']' ',' zireg optcexpr ')'
		{
		  op->mode = POST;
		  op->reg = $4;
		  op->disp = $3;
		  op->index = $7;
		  op->odisp = $8;
		}
	| '(' '[' EXPR optczapc ']' optcexpr ')'
		{
		  op->mode = POST;
		  op->reg = $4;
		  op->disp = $3;
		  op->odisp = $6;
		}
	| '(' '[' zapc ']' ',' zireg optcexpr ')'
		{
		  op->mode = POST;
		  op->reg = $3;
		  op->index = $6;
		  op->odisp = $7;
		}
	| '(' '[' zapc ']' optcexpr ')'
		{
		  op->mode = POST;
		  op->reg = $3;
		  op->odisp = $5;
		}
	| '(' '[' EXPR ',' zapc ',' zireg ']' optcexpr ')'
		{
		  op->mode = PRE;
		  op->reg = $5;
		  op->disp = $3;
		  op->index = $7;
		  op->odisp = $9;
		}
	| '(' '[' zapc ',' zireg ']' optcexpr ')'
		{
		  op->mode = PRE;
		  op->reg = $3;
		  op->index = $5;
		  op->odisp = $7;
		}
	| '(' '[' EXPR ',' zapc ',' zpc ']' optcexpr ')'
		{
		  if ($5 == PC || $5 == ZPC)
		    yyerror (_("syntax error"));
		  op->mode = PRE;
		  op->reg = $7;
		  op->disp = $3;
		  op->index.reg = $5;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		  op->odisp = $9;
		}
	| '(' '[' zapc ',' zpc ']' optcexpr ')'
		{
		  if ($3 == PC || $3 == ZPC)
		    yyerror (_("syntax error"));
		  op->mode = PRE;
		  op->reg = $5;
		  op->index.reg = $3;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		  op->odisp = $7;
		}
	| '(' '[' optexprc zdireg optczapc ']' optcexpr ')'
		{
		  op->mode = PRE;
		  op->reg = $5;
		  op->disp = $3;
		  op->index = $4;
		  op->odisp = $7;
		}
	;

/* An operand in MIT syntax.  */

mit_operand:
	  optzapc '@'
		{
		  /* We use optzapc to avoid a shift/reduce conflict.  */
		  if ($1 < ADDR0 || $1 > ADDR7)
		    yyerror (_("syntax error"));
		  op->mode = AINDR;
		  op->reg = $1;
		}
	| optzapc '@' '+'
		{
		  /* We use optzapc to avoid a shift/reduce conflict.  */
		  if ($1 < ADDR0 || $1 > ADDR7)
		    yyerror (_("syntax error"));
		  op->mode = AINC;
		  op->reg = $1;
		}
	| optzapc '@' '-'
		{
		  /* We use optzapc to avoid a shift/reduce conflict.  */
		  if ($1 < ADDR0 || $1 > ADDR7)
		    yyerror (_("syntax error"));
		  op->mode = ADEC;
		  op->reg = $1;
		}
	| optzapc '@' '(' EXPR ')'
		{
		  op->reg = $1;
		  op->disp = $4;
		  if (($1 >= ZADDR0 && $1 <= ZADDR7)
		      || $1 == ZPC)
		    op->mode = BASE;
		  else
		    op->mode = DISP;
		}
	| optzapc '@' '(' optexprc zireg ')'
		{
		  op->mode = BASE;
		  op->reg = $1;
		  op->disp = $4;
		  op->index = $5;
		}
	| optzapc '@' '(' EXPR ')' '@' '(' optexprc zireg ')'
		{
		  op->mode = POST;
		  op->reg = $1;
		  op->disp = $4;
		  op->index = $9;
		  op->odisp = $8;
		}
	| optzapc '@' '(' EXPR ')' '@' '(' EXPR ')'
		{
		  op->mode = POST;
		  op->reg = $1;
		  op->disp = $4;
		  op->odisp = $8;
		}
	| optzapc '@' '(' optexprc zireg ')' '@' '(' EXPR ')'
		{
		  op->mode = PRE;
		  op->reg = $1;
		  op->disp = $4;
		  op->index = $5;
		  op->odisp = $9;
		}
	;

/* An index register, possibly suppressed, which need not have a size
   or scale.  */

zireg:
	  INDEXREG
	| zadr
		{
		  $$.reg = $1;
		  $$.size = SIZE_UNSPEC;
		  $$.scale = 1;
		}
	;

/* A register which may be an index register, but which may not be an
   address register.  This nonterminal is used to avoid ambiguity when
   trying to parse something like (0,d5,a6) as compared to (0,a6,d5).  */

zdireg:
	  INDEXREG
	| zdr
		{
		  $$.reg = $1;
		  $$.size = SIZE_UNSPEC;
		  $$.scale = 1;
		}
	;

/* An address or data register, or a suppressed address or data
   register.  */

zadr:
	  zdr
	| AR
	| ZAR
	;

/* A data register which may be suppressed.  */

zdr:
	  DR
	| ZDR
	;

/* Either an address register or the PC.  */

apc:
	  AR
	| LPC
	;

/* Either an address register, or the PC, or a suppressed address
   register, or a suppressed PC.  */

zapc:
	  apc
	| LZPC
	| ZAR
	;

/* An optional zapc.  */

optzapc:
	  /* empty */
		{
		  $$ = ZADDR0;
		}
	| zapc
	;

/* The PC, optionally suppressed.  */

zpc:
	  LPC
	| LZPC
	;

/* ',' zapc when it may be omitted.  */

optczapc:
	  /* empty */
		{
		  $$ = ZADDR0;
		}
	| ',' zapc
		{
		  $$ = $2;
		}
	;

/* ',' EXPR when it may be omitted.  */

optcexpr:
	  /* empty */
		{
		  $$.exp.X_op = O_absent;
		  $$.size = SIZE_UNSPEC;
		}
	| ',' EXPR
		{
		  $$ = $2;
		}
	;

/* EXPR ',' when it may be omitted.  */

optexprc:
	  /* empty */
		{
		  $$.exp.X_op = O_absent;
		  $$.size = SIZE_UNSPEC;
		}
	| EXPR ','
		{
		  $$ = $1;
		}
	;

/* A register list for the movem instruction.  */

reglist:
	  reglistpair
	| reglistpair '/' ireglist
		{
		  $$ = $1 | $3;
		}
	| reglistreg '/' ireglist
		{
		  $$ = (1 << $1) | $3;
		}
	;

/* We use ireglist when we know we are looking at a reglist, and we
   can safely reduce a simple register to reglistreg.  If we permitted
   reglist to reduce to reglistreg, it would be ambiguous whether a
   plain register were a DREG/AREG/FPREG or a REGLST.  */

ireglist:
	  reglistreg
		{
		  $$ = 1 << $1;
		}
	| reglistpair
	| reglistpair '/' ireglist
		{
		  $$ = $1 | $3;
		}
	| reglistreg '/' ireglist
		{
		  $$ = (1 << $1) | $3;
		}
	;

reglistpair:
	  reglistreg '-' reglistreg
		{
		  if ($1 <= $3)
		    $$ = (1 << ($3 + 1)) - 1 - ((1 << $1) - 1);
		  else
		    $$ = (1 << ($1 + 1)) - 1 - ((1 << $3) - 1);
		}
	;

reglistreg:
	  DR
		{
		  $$ = $1 - DATA0;
		}
	| AR
		{
		  $$ = $1 - ADDR0 + 8;
		}
	| FPR
		{
		  $$ = $1 - FP0 + 16;
		}
	| FPCR
		{
		  if ($1 == FPI)
		    $$ = 24;
		  else if ($1 == FPS)
		    $$ = 25;
		  else
		    $$ = 26;
		}
	;

%%

/* The string to parse is stored here, and modified by yylex.  */

static char *str;

/* The original string pointer.  */

static char *strorig;

/* If *CCP could be a register, return the register number and advance
   *CCP.  Otherwise don't change *CCP, and return 0.  */

static enum m68k_register
m68k_reg_parse (ccp)
     register char **ccp;
{
  char *start = *ccp;
  char c;
  char *p;
  symbolS *symbolp;

  if (flag_reg_prefix_optional)
    {
      if (*start == REGISTER_PREFIX)
	start++;
      p = start;
    }
  else
    {
      if (*start != REGISTER_PREFIX)
	return 0;
      p = start + 1;
    }

  if (! is_name_beginner (*p))
    return 0;

  p++;
  while (is_part_of_name (*p) && *p != '.' && *p != ':' && *p != '*')
    p++;

  c = *p;
  *p = 0;
  symbolp = symbol_find (start);
  *p = c;

  if (symbolp != NULL && S_GET_SEGMENT (symbolp) == reg_section)
    {
      *ccp = p;
      return S_GET_VALUE (symbolp);
    }

  /* In MRI mode, something like foo.bar can be equated to a register
     name.  */
  while (flag_mri && c == '.')
    {
      ++p;
      while (is_part_of_name (*p) && *p != '.' && *p != ':' && *p != '*')
	p++;
      c = *p;
      *p = '\0';
      symbolp = symbol_find (start);
      *p = c;
      if (symbolp != NULL && S_GET_SEGMENT (symbolp) == reg_section)
	{
	  *ccp = p;
	  return S_GET_VALUE (symbolp);
	}
    }

  return 0;
}

/* The lexer.  */

static int
yylex ()
{
  enum m68k_register reg;
  char *s;
  int parens;
  int c = 0;
  int tail = 0;
  char *hold;

  if (*str == ' ')
    ++str;

  if (*str == '\0')
    return 0;

  /* Various special characters are just returned directly.  */
  switch (*str)
    {
    case '@':
      /* In MRI mode, this can be the start of an octal number.  */
      if (flag_mri)
	{
	  if (ISDIGIT (str[1])
	      || ((str[1] == '+' || str[1] == '-')
		  && ISDIGIT (str[2])))
	    break;
	}
      /* Fall through.  */
    case '#':
    case '&':
    case ',':
    case ')':
    case '/':
    case '[':
    case ']':
    case '<':
    case '>':
      return *str++;
    case '+':
      /* It so happens that a '+' can only appear at the end of an
	 operand, or if it is trailed by an '&'(see mac load insn).
	 If it appears anywhere else, it must be a unary.  */
      if (str[1] == '\0' || (str[1] == '&' && str[2] == '\0'))
	return *str++;
      break;
    case '-':
      /* A '-' can only appear in -(ar), rn-rn, or ar@-.  If it
         appears anywhere else, it must be a unary minus on an
         expression, unless it it trailed by a '&'(see mac load insn).  */
      if (str[1] == '\0' || (str[1] == '&' && str[2] == '\0'))
	return *str++;
      s = str + 1;
      if (*s == '(')
	++s;
      if (m68k_reg_parse (&s) != 0)
	return *str++;
      break;
    case '(':
      /* A '(' can only appear in `(reg)', `(expr,...', `([', `@(', or
         `)('.  If it appears anywhere else, it must be starting an
         expression.  */
      if (str[1] == '['
	  || (str > strorig
	      && (str[-1] == '@'
		  || str[-1] == ')')))
	return *str++;
      s = str + 1;
      if (m68k_reg_parse (&s) != 0)
	return *str++;
      /* Check for the case of '(expr,...' by scanning ahead.  If we
         find a comma outside of balanced parentheses, we return '('.
         If we find an unbalanced right parenthesis, then presumably
         the '(' really starts an expression.  */
      parens = 0;
      for (s = str + 1; *s != '\0'; s++)
	{
	  if (*s == '(')
	    ++parens;
	  else if (*s == ')')
	    {
	      if (parens == 0)
		break;
	      --parens;
	    }
	  else if (*s == ',' && parens == 0)
	    {
	      /* A comma can not normally appear in an expression, so
		 this is a case of '(expr,...'.  */
	      return *str++;
	    }
	}
    }

  /* See if it's a register.  */

  reg = m68k_reg_parse (&str);
  if (reg != 0)
    {
      int ret;

      yylval.reg = reg;

      if (reg >= DATA0 && reg <= DATA7)
	ret = DR;
      else if (reg >= ADDR0 && reg <= ADDR7)
	ret = AR;
      else if (reg >= FP0 && reg <= FP7)
	return FPR;
      else if (reg == FPI
	       || reg == FPS
	       || reg == FPC)
	return FPCR;
      else if (reg == PC)
	return LPC;
      else if (reg >= ZDATA0 && reg <= ZDATA7)
	ret = ZDR;
      else if (reg >= ZADDR0 && reg <= ZADDR7)
	ret = ZAR;
      else if (reg == ZPC)
	return LZPC;
      else
	return CREG;

      /* If we get here, we have a data or address register.  We
	 must check for a size or scale; if we find one, we must
	 return INDEXREG.  */

      s = str;

      if (*s != '.' && *s != ':' && *s != '*')
	return ret;

      yylval.indexreg.reg = reg;

      if (*s != '.' && *s != ':')
	yylval.indexreg.size = SIZE_UNSPEC;
      else
	{
	  ++s;
	  switch (*s)
	    {
	    case 'w':
	    case 'W':
	      yylval.indexreg.size = SIZE_WORD;
	      ++s;
	      break;
	    case 'l':
	    case 'L':
	      yylval.indexreg.size = SIZE_LONG;
	      ++s;
	      break;
	    default:
	      yyerror (_("illegal size specification"));
	      yylval.indexreg.size = SIZE_UNSPEC;
	      break;
	    }
	}

      yylval.indexreg.scale = 1;

      if (*s == '*' || *s == ':')
	{
	  expressionS scale;

	  ++s;

	  hold = input_line_pointer;
	  input_line_pointer = s;
	  expression (&scale);
	  s = input_line_pointer;
	  input_line_pointer = hold;

	  if (scale.X_op != O_constant)
	    yyerror (_("scale specification must resolve to a number"));
	  else
	    {
	      switch (scale.X_add_number)
		{
		case 1:
		case 2:
		case 4:
		case 8:
		  yylval.indexreg.scale = scale.X_add_number;
		  break;
		default:
		  yyerror (_("invalid scale value"));
		  break;
		}
	    }
	}

      str = s;

      return INDEXREG;
    }

  /* It must be an expression.  Before we call expression, we need to
     look ahead to see if there is a size specification.  We must do
     that first, because otherwise foo.l will be treated as the symbol
     foo.l, rather than as the symbol foo with a long size
     specification.  The grammar requires that all expressions end at
     the end of the operand, or with ',', '(', ']', ')'.  */

  parens = 0;
  for (s = str; *s != '\0'; s++)
    {
      if (*s == '(')
	{
	  if (parens == 0
	      && s > str
	      && (s[-1] == ')' || ISALNUM (s[-1])))
	    break;
	  ++parens;
	}
      else if (*s == ')')
	{
	  if (parens == 0)
	    break;
	  --parens;
	}
      else if (parens == 0
	       && (*s == ',' || *s == ']'))
	break;
    }

  yylval.exp.size = SIZE_UNSPEC;
  if (s <= str + 2
      || (s[-2] != '.' && s[-2] != ':'))
    tail = 0;
  else
    {
      switch (s[-1])
	{
	case 's':
	case 'S':
	case 'b':
	case 'B':
	  yylval.exp.size = SIZE_BYTE;
	  break;
	case 'w':
	case 'W':
	  yylval.exp.size = SIZE_WORD;
	  break;
	case 'l':
	case 'L':
	  yylval.exp.size = SIZE_LONG;
	  break;
	default:
	  break;
	}
      if (yylval.exp.size != SIZE_UNSPEC)
	tail = 2;
    }

#ifdef OBJ_ELF
  {
    /* Look for @PLTPC, etc.  */
    char *cp;

    yylval.exp.pic_reloc = pic_none;
    cp = s - tail;
    if (cp - 6 > str && cp[-6] == '@')
      {
	if (strncmp (cp - 6, "@PLTPC", 6) == 0)
	  {
	    yylval.exp.pic_reloc = pic_plt_pcrel;
	    tail += 6;
	  }
	else if (strncmp (cp - 6, "@GOTPC", 6) == 0)
	  {
	    yylval.exp.pic_reloc = pic_got_pcrel;
	    tail += 6;
	  }
      }
    else if (cp - 4 > str && cp[-4] == '@')
      {
	if (strncmp (cp - 4, "@PLT", 4) == 0)
	  {
	    yylval.exp.pic_reloc = pic_plt_off;
	    tail += 4;
	  }
	else if (strncmp (cp - 4, "@GOT", 4) == 0)
	  {
	    yylval.exp.pic_reloc = pic_got_off;
	    tail += 4;
	  }
      }
  }
#endif

  if (tail != 0)
    {
      c = s[-tail];
      s[-tail] = 0;
    }

  hold = input_line_pointer;
  input_line_pointer = str;
  expression (&yylval.exp.exp);
  str = input_line_pointer;
  input_line_pointer = hold;

  if (tail != 0)
    {
      s[-tail] = c;
      str = s;
    }

  return EXPR;
}

/* Parse an m68k operand.  This is the only function which is called
   from outside this file.  */

int
m68k_ip_op (s, oparg)
     char *s;
     struct m68k_op *oparg;
{
  memset (oparg, 0, sizeof *oparg);
  oparg->error = NULL;
  oparg->index.reg = ZDATA0;
  oparg->index.scale = 1;
  oparg->disp.exp.X_op = O_absent;
  oparg->odisp.exp.X_op = O_absent;

  str = strorig = s;
  op = oparg;

  return yyparse ();
}

/* The error handler.  */

static void
yyerror (s)
     const char *s;
{
  op->error = s;
}
