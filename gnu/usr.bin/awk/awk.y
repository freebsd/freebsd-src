/*
 * awk.y --- yacc/bison parser
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991, 1992, 1993 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Progamming Language.
 * 
 * GAWK is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * GAWK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GAWK; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

%{
#ifdef DEBUG
#define YYDEBUG 12
#endif

#include "awk.h"

static void yyerror (); /* va_alist */
static char *get_src_buf P((void));
static int yylex P((void));
static NODE *node_common P((NODETYPE op));
static NODE *snode P((NODE *subn, NODETYPE op, int sindex));
static NODE *mkrangenode P((NODE *cpair));
static NODE *make_for_loop P((NODE *init, NODE *cond, NODE *incr));
static NODE *append_right P((NODE *list, NODE *new));
static void func_install P((NODE *params, NODE *def));
static void pop_var P((NODE *np, int freeit));
static void pop_params P((NODE *params));
static NODE *make_param P((char *name));
static NODE *mk_rexp P((NODE *exp));

static int want_assign;		/* lexical scanning kludge */
static int want_regexp;		/* lexical scanning kludge */
static int can_return;		/* lexical scanning kludge */
static int io_allowed = 1;	/* lexical scanning kludge */
static char *lexptr;		/* pointer to next char during parsing */
static char *lexend;
static char *lexptr_begin;	/* keep track of where we were for error msgs */
static char *lexeme;		/* beginning of lexeme for debugging */
static char *thisline = NULL;
#define YYDEBUG_LEXER_TEXT (lexeme)
static int param_counter;
static char *tokstart = NULL;
static char *tok = NULL;
static char *tokend;

#define HASHSIZE	1021	/* this constant only used here */
NODE *variables[HASHSIZE];

extern char *source;
extern int sourceline;
extern struct src *srcfiles;
extern int numfiles;
extern int errcount;
extern NODE *begin_block;
extern NODE *end_block;
%}

%union {
	long lval;
	AWKNUM fval;
	NODE *nodeval;
	NODETYPE nodetypeval;
	char *sval;
	NODE *(*ptrval)();
}

%type <nodeval> function_prologue function_body
%type <nodeval> rexp exp start program rule simp_exp
%type <nodeval> non_post_simp_exp
%type <nodeval> pattern 
%type <nodeval>	action variable param_list
%type <nodeval>	rexpression_list opt_rexpression_list
%type <nodeval>	expression_list opt_expression_list
%type <nodeval>	statements statement if_statement opt_param_list 
%type <nodeval> opt_exp opt_variable regexp 
%type <nodeval> input_redir output_redir
%type <nodetypeval> print
%type <sval> func_name
%type <lval> lex_builtin

%token <sval> FUNC_CALL NAME REGEXP
%token <lval> ERROR
%token <nodeval> YNUMBER YSTRING
%token <nodetypeval> RELOP APPEND_OP
%token <nodetypeval> ASSIGNOP MATCHOP NEWLINE CONCAT_OP
%token <nodetypeval> LEX_BEGIN LEX_END LEX_IF LEX_ELSE LEX_RETURN LEX_DELETE
%token <nodetypeval> LEX_WHILE LEX_DO LEX_FOR LEX_BREAK LEX_CONTINUE
%token <nodetypeval> LEX_PRINT LEX_PRINTF LEX_NEXT LEX_EXIT LEX_FUNCTION
%token <nodetypeval> LEX_GETLINE
%token <nodetypeval> LEX_IN
%token <lval> LEX_AND LEX_OR INCREMENT DECREMENT
%token <lval> LEX_BUILTIN LEX_LENGTH

/* these are just yylval numbers */

/* Lowest to highest */
%right ASSIGNOP
%right '?' ':'
%left LEX_OR
%left LEX_AND
%left LEX_GETLINE
%nonassoc LEX_IN
%left FUNC_CALL LEX_BUILTIN LEX_LENGTH
%nonassoc ','
%nonassoc MATCHOP
%nonassoc RELOP '<' '>' '|' APPEND_OP
%left CONCAT_OP
%left YSTRING YNUMBER
%left '+' '-'
%left '*' '/' '%'
%right '!' UNARY
%right '^'
%left INCREMENT DECREMENT
%left '$'
%left '(' ')'
%%

start
	: opt_nls program opt_nls
		{ expression_value = $2; }
	;

program
	: rule
		{ 
			if ($1 != NULL)
				$$ = $1;
			else
				$$ = NULL;
			yyerrok;
		}
	| program rule
		/* add the rule to the tail of list */
		{
			if ($2 == NULL)
				$$ = $1;
			else if ($1 == NULL)
				$$ = $2;
			else {
				if ($1->type != Node_rule_list)
					$1 = node($1, Node_rule_list,
						(NODE*)NULL);
				$$ = append_right ($1,
				   node($2, Node_rule_list,(NODE *) NULL));
			}
			yyerrok;
		}
	| error	{ $$ = NULL; }
	| program error { $$ = NULL; }
	| /* empty */ { $$ = NULL; }
	;

rule
	: LEX_BEGIN { io_allowed = 0; }
	  action
	  {
		if (begin_block) {
			if (begin_block->type != Node_rule_list)
				begin_block = node(begin_block, Node_rule_list,
					(NODE *)NULL);
			(void) append_right (begin_block, node(
			    node((NODE *)NULL, Node_rule_node, $3),
			    Node_rule_list, (NODE *)NULL) );
		} else
			begin_block = node((NODE *)NULL, Node_rule_node, $3);
		$$ = NULL;
		io_allowed = 1;
		yyerrok;
	  }
	| LEX_END { io_allowed = 0; }
	  action
	  {
		if (end_block) {
			if (end_block->type != Node_rule_list)
				end_block = node(end_block, Node_rule_list,
					(NODE *)NULL);
			(void) append_right (end_block, node(
			    node((NODE *)NULL, Node_rule_node, $3),
			    Node_rule_list, (NODE *)NULL));
		} else
			end_block = node((NODE *)NULL, Node_rule_node, $3);
		$$ = NULL;
		io_allowed = 1;
		yyerrok;
	  }
	| LEX_BEGIN statement_term
	  {
		warning("BEGIN blocks must have an action part");
		errcount++;
		yyerrok;
	  }
	| LEX_END statement_term
	  {
		warning("END blocks must have an action part");
		errcount++;
		yyerrok;
	  }
	| pattern action
		{ $$ = node ($1, Node_rule_node, $2); yyerrok; }
	| action
		{ $$ = node ((NODE *)NULL, Node_rule_node, $1); yyerrok; }
	| pattern statement_term
		{
		  $$ = node ($1,
			     Node_rule_node,
			     node(node(node(make_number(0.0),
					    Node_field_spec,
					    (NODE *) NULL),
					Node_expression_list,
					(NODE *) NULL),
				  Node_K_print,
				  (NODE *) NULL));
		  yyerrok;
		}
	| function_prologue function_body
		{
			func_install($1, $2);
			$$ = NULL;
			yyerrok;
		}
	;

func_name
	: NAME
		{ $$ = $1; }
	| FUNC_CALL
		{ $$ = $1; }
	| lex_builtin
	  {
		yyerror("%s() is a built-in function, it cannot be redefined",
			tokstart);
		errcount++;
		/* yyerrok; */
	  }
	;

lex_builtin
	: LEX_BUILTIN
	| LEX_LENGTH
	;
		
function_prologue
	: LEX_FUNCTION 
		{
			param_counter = 0;
		}
	  func_name '(' opt_param_list r_paren opt_nls
		{
			$$ = append_right(make_param($3), $5);
			can_return = 1;
		}
	;

function_body
	: l_brace statements r_brace opt_semi
	  {
		$$ = $2;
		can_return = 0;
	  }
	;


pattern
	: exp
		{ $$ = $1; }
	| exp ',' exp
		{ $$ = mkrangenode ( node($1, Node_cond_pair, $3) ); }
	;

regexp
	/*
	 * In this rule, want_regexp tells yylex that the next thing
	 * is a regexp so it should read up to the closing slash.
	 */
	: '/'
		{ ++want_regexp; }
	  REGEXP '/'
		{
		  NODE *n;
		  size_t len;

		  getnode(n);
		  n->type = Node_regex;
		  len = strlen($3);
		  n->re_exp = make_string($3, len);
		  n->re_reg = make_regexp($3, len, 0, 1);
		  n->re_text = NULL;
		  n->re_flags = CONST;
		  n->re_cnt = 1;
		  $$ = n;
		}
	;

action
	: l_brace statements r_brace opt_semi opt_nls
		{ $$ = $2 ; }
	| l_brace r_brace opt_semi opt_nls
		{ $$ = NULL; }
	;

statements
	: statement
		{ $$ = $1; }
	| statements statement
		{
			if ($1 == NULL || $1->type != Node_statement_list)
				$1 = node($1, Node_statement_list,(NODE *)NULL);
	    		$$ = append_right($1,
				node( $2, Node_statement_list, (NODE *)NULL));
	    		yyerrok;
		}
	| error
		{ $$ = NULL; }
	| statements error
		{ $$ = NULL; }
	;

statement_term
	: nls
	| semi opt_nls
	;

statement
	: semi opt_nls
		{ $$ = NULL; }
	| l_brace r_brace
		{ $$ = NULL; }
	| l_brace statements r_brace
		{ $$ = $2; }
	| if_statement
		{ $$ = $1; }
	| LEX_WHILE '(' exp r_paren opt_nls statement
		{ $$ = node ($3, Node_K_while, $6); }
	| LEX_DO opt_nls statement LEX_WHILE '(' exp r_paren opt_nls
		{ $$ = node ($6, Node_K_do, $3); }
	| LEX_FOR '(' NAME LEX_IN NAME r_paren opt_nls statement
	  {
		$$ = node ($8, Node_K_arrayfor, make_for_loop(variable($3,1),
			(NODE *)NULL, variable($5,1)));
	  }
	| LEX_FOR '(' opt_exp semi exp semi opt_exp r_paren opt_nls statement
	  {
		$$ = node($10, Node_K_for, (NODE *)make_for_loop($3, $5, $7));
	  }
	| LEX_FOR '(' opt_exp semi semi opt_exp r_paren opt_nls statement
	  {
		$$ = node ($9, Node_K_for,
			(NODE *)make_for_loop($3, (NODE *)NULL, $6));
	  }
	| LEX_BREAK statement_term
	   /* for break, maybe we'll have to remember where to break to */
		{ $$ = node ((NODE *)NULL, Node_K_break, (NODE *)NULL); }
	| LEX_CONTINUE statement_term
	   /* similarly */
		{ $$ = node ((NODE *)NULL, Node_K_continue, (NODE *)NULL); }
	| print '(' expression_list r_paren output_redir statement_term
		{ $$ = node ($3, $1, $5); }
	| print opt_rexpression_list output_redir statement_term
		{
			if ($1 == Node_K_print && $2 == NULL)
				$2 = node(node(make_number(0.0),
					       Node_field_spec,
					       (NODE *) NULL),
					  Node_expression_list,
					  (NODE *) NULL);

			$$ = node ($2, $1, $3);
		}
	| LEX_NEXT opt_exp statement_term
		{ NODETYPE type;

		  if ($2 && $2 == lookup("file")) {
			if (do_lint)
				warning("`next file' is a gawk extension");
			if (do_unix || do_posix) {
				/*
				 * can't use yyerror, since may have overshot
				 * the source line
				 */
				errcount++;
				msg("`next file' is a gawk extension");
			}
			if (! io_allowed) {
				/* same thing */
				errcount++;
				msg("`next file' used in BEGIN or END action");
			}
			type = Node_K_nextfile;
		  } else {
			if (! io_allowed)
				yyerror("next used in BEGIN or END action");
			type = Node_K_next;
		}
		  $$ = node ((NODE *)NULL, type, (NODE *)NULL);
		}
	| LEX_EXIT opt_exp statement_term
		{ $$ = node ($2, Node_K_exit, (NODE *)NULL); }
	| LEX_RETURN
		{ if (! can_return) yyerror("return used outside function context"); }
	  opt_exp statement_term
		{ $$ = node ($3, Node_K_return, (NODE *)NULL); }
	| LEX_DELETE NAME '[' expression_list ']' statement_term
		{ $$ = node (variable($2,1), Node_K_delete, $4); }
	| LEX_DELETE NAME  statement_term
		{
		  if (do_lint)
			warning("`delete array' is a gawk extension");
		  if (do_unix || do_posix) {
			/*
			 * can't use yyerror, since may have overshot
			 * the source line
			 */
			errcount++;
			msg("`delete array' is a gawk extension");
		  }
		  $$ = node (variable($2,1), Node_K_delete, (NODE *) NULL);
		}
	| exp statement_term
		{ $$ = $1; }
	;

print
	: LEX_PRINT
		{ $$ = $1; }
	| LEX_PRINTF
		{ $$ = $1; }
	;

if_statement
	: LEX_IF '(' exp r_paren opt_nls statement
	  {
		$$ = node($3, Node_K_if, 
			node($6, Node_if_branches, (NODE *)NULL));
	  }
	| LEX_IF '(' exp r_paren opt_nls statement
	     LEX_ELSE opt_nls statement
		{ $$ = node ($3, Node_K_if,
				node ($6, Node_if_branches, $9)); }
	;

nls
	: NEWLINE
		{ want_assign = 0; }
	| nls NEWLINE
	;

opt_nls
	: /* empty */
	| nls
	;

input_redir
	: /* empty */
		{ $$ = NULL; }
	| '<' simp_exp
		{ $$ = node ($2, Node_redirect_input, (NODE *)NULL); }
	;

output_redir
	: /* empty */
		{ $$ = NULL; }
	| '>' exp
		{ $$ = node ($2, Node_redirect_output, (NODE *)NULL); }
	| APPEND_OP exp
		{ $$ = node ($2, Node_redirect_append, (NODE *)NULL); }
	| '|' exp
		{ $$ = node ($2, Node_redirect_pipe, (NODE *)NULL); }
	;

opt_param_list
	: /* empty */
		{ $$ = NULL; }
	| param_list
		{ $$ = $1; }
	;

param_list
	: NAME
		{ $$ = make_param($1); }
	| param_list comma NAME
		{ $$ = append_right($1, make_param($3)); yyerrok; }
	| error
		{ $$ = NULL; }
	| param_list error
		{ $$ = NULL; }
	| param_list comma error
		{ $$ = NULL; }
	;

/* optional expression, as in for loop */
opt_exp
	: /* empty */
		{ $$ = NULL; }
	| exp
		{ $$ = $1; }
	;

opt_rexpression_list
	: /* empty */
		{ $$ = NULL; }
	| rexpression_list
		{ $$ = $1; }
	;

rexpression_list
	: rexp
		{ $$ = node ($1, Node_expression_list, (NODE *)NULL); }
	| rexpression_list comma rexp
	  {
		$$ = append_right($1,
			node( $3, Node_expression_list, (NODE *)NULL));
		yyerrok;
	  }
	| error
		{ $$ = NULL; }
	| rexpression_list error
		{ $$ = NULL; }
	| rexpression_list error rexp
		{ $$ = NULL; }
	| rexpression_list comma error
		{ $$ = NULL; }
	;

opt_expression_list
	: /* empty */
		{ $$ = NULL; }
	| expression_list
		{ $$ = $1; }
	;

expression_list
	: exp
		{ $$ = node ($1, Node_expression_list, (NODE *)NULL); }
	| expression_list comma exp
		{
			$$ = append_right($1,
				node( $3, Node_expression_list, (NODE *)NULL));
			yyerrok;
		}
	| error
		{ $$ = NULL; }
	| expression_list error
		{ $$ = NULL; }
	| expression_list error exp
		{ $$ = NULL; }
	| expression_list comma error
		{ $$ = NULL; }
	;

/* Expressions, not including the comma operator.  */
exp	: variable ASSIGNOP 
		{ want_assign = 0; }
	  exp
		{
		  if (do_lint && $4->type == Node_regex)
			warning("Regular expression on left of assignment.");
		  $$ = node ($1, $2, $4);
		}
	| '(' expression_list r_paren LEX_IN NAME
		{ $$ = node (variable($5,1), Node_in_array, $2); }
	| exp '|' LEX_GETLINE opt_variable
		{
		  $$ = node ($4, Node_K_getline,
			 node ($1, Node_redirect_pipein, (NODE *)NULL));
		}
	| LEX_GETLINE opt_variable input_redir
		{
		  if (do_lint && ! io_allowed && $3 == NULL)
			warning("non-redirected getline undefined inside BEGIN or END action");
		  $$ = node ($2, Node_K_getline, $3);
		}
	| exp LEX_AND exp
		{ $$ = node ($1, Node_and, $3); }
	| exp LEX_OR exp
		{ $$ = node ($1, Node_or, $3); }
	| exp MATCHOP exp
		{
		  if ($1->type == Node_regex)
			warning("Regular expression on left of MATCH operator.");
		  $$ = node ($1, $2, mk_rexp($3));
		}
	| regexp
		{ $$ = $1; }
	| '!' regexp %prec UNARY
		{
		  $$ = node(node(make_number(0.0),
				 Node_field_spec,
				 (NODE *) NULL),
		            Node_nomatch,
			    $2);
		}
	| exp LEX_IN NAME
		{ $$ = node (variable($3,1), Node_in_array, $1); }
	| exp RELOP exp
		{
		  if (do_lint && $3->type == Node_regex)
			warning("Regular expression on left of comparison.");
		  $$ = node ($1, $2, $3);
		}
	| exp '<' exp
		{ $$ = node ($1, Node_less, $3); }
	| exp '>' exp
		{ $$ = node ($1, Node_greater, $3); }
	| exp '?' exp ':' exp
		{ $$ = node($1, Node_cond_exp, node($3, Node_if_branches, $5));}
	| simp_exp
		{ $$ = $1; }
	| exp simp_exp %prec CONCAT_OP
		{ $$ = node ($1, Node_concat, $2); }
	;

rexp	
	: variable ASSIGNOP 
		{ want_assign = 0; }
	  rexp
		{ $$ = node ($1, $2, $4); }
	| rexp LEX_AND rexp
		{ $$ = node ($1, Node_and, $3); }
	| rexp LEX_OR rexp
		{ $$ = node ($1, Node_or, $3); }
	| LEX_GETLINE opt_variable input_redir
		{
		  if (do_lint && ! io_allowed && $3 == NULL)
			warning("non-redirected getline undefined inside BEGIN or END action");
		  $$ = node ($2, Node_K_getline, $3);
		}
	| regexp
		{ $$ = $1; } 
	| '!' regexp %prec UNARY
		{ $$ = node((NODE *) NULL, Node_nomatch, $2); }
	| rexp MATCHOP rexp
		 { $$ = node ($1, $2, mk_rexp($3)); }
	| rexp LEX_IN NAME
		{ $$ = node (variable($3,1), Node_in_array, $1); }
	| rexp RELOP rexp
		{ $$ = node ($1, $2, $3); }
	| rexp '?' rexp ':' rexp
		{ $$ = node($1, Node_cond_exp, node($3, Node_if_branches, $5));}
	| simp_exp
		{ $$ = $1; }
	| rexp simp_exp %prec CONCAT_OP
		{ $$ = node ($1, Node_concat, $2); }
	;

simp_exp
	: non_post_simp_exp
	/* Binary operators in order of decreasing precedence.  */
	| simp_exp '^' simp_exp
		{ $$ = node ($1, Node_exp, $3); }
	| simp_exp '*' simp_exp
		{ $$ = node ($1, Node_times, $3); }
	| simp_exp '/' simp_exp
		{ $$ = node ($1, Node_quotient, $3); }
	| simp_exp '%' simp_exp
		{ $$ = node ($1, Node_mod, $3); }
	| simp_exp '+' simp_exp
		{ $$ = node ($1, Node_plus, $3); }
	| simp_exp '-' simp_exp
		{ $$ = node ($1, Node_minus, $3); }
	| variable INCREMENT
		{ $$ = node ($1, Node_postincrement, (NODE *)NULL); }
	| variable DECREMENT
		{ $$ = node ($1, Node_postdecrement, (NODE *)NULL); }
	;

non_post_simp_exp
	: '!' simp_exp %prec UNARY
		{ $$ = node ($2, Node_not,(NODE *) NULL); }
	| '(' exp r_paren
		{ $$ = $2; }
	| LEX_BUILTIN
	  '(' opt_expression_list r_paren
		{ $$ = snode ($3, Node_builtin, (int) $1); }
	| LEX_LENGTH '(' opt_expression_list r_paren
		{ $$ = snode ($3, Node_builtin, (int) $1); }
	| LEX_LENGTH
	  {
		if (do_lint)
			warning("call of `length' without parentheses is not portable");
		$$ = snode ((NODE *)NULL, Node_builtin, (int) $1);
		if (do_posix)
			warning( "call of `length' without parentheses is deprecated by POSIX");
	  }
	| FUNC_CALL '(' opt_expression_list r_paren
	  {
		$$ = node ($3, Node_func_call, make_string($1, strlen($1)));
	  }
	| variable
	| INCREMENT variable
		{ $$ = node ($2, Node_preincrement, (NODE *)NULL); }
	| DECREMENT variable
		{ $$ = node ($2, Node_predecrement, (NODE *)NULL); }
	| YNUMBER
		{ $$ = $1; }
	| YSTRING
		{ $$ = $1; }

	| '-' simp_exp    %prec UNARY
		{ if ($2->type == Node_val) {
			$2->numbr = -(force_number($2));
			$$ = $2;
		  } else
			$$ = node ($2, Node_unary_minus, (NODE *)NULL);
		}
	| '+' simp_exp    %prec UNARY
		{
		  /* was: $$ = $2 */
		  /* POSIX semantics: force a conversion to numeric type */
		  $$ = node (make_number(0.0), Node_plus, $2);
		}
	;

opt_variable
	: /* empty */
		{ $$ = NULL; }
	| variable
		{ $$ = $1; }
	;

variable
	: NAME
		{ $$ = variable($1,1); }
	| NAME '[' expression_list ']'
		{
		if ($3->rnode == NULL) {
			$$ = node (variable($1,1), Node_subscript, $3->lnode);
			freenode($3);
		} else
			$$ = node (variable($1,1), Node_subscript, $3);
		}
	| '$' non_post_simp_exp
		{ $$ = node ($2, Node_field_spec, (NODE *)NULL); }
	;

l_brace
	: '{' opt_nls
	;

r_brace
	: '}' opt_nls	{ yyerrok; }
	;

r_paren
	: ')' { yyerrok; }
	;

opt_semi
	: /* empty */
	| semi
	;

semi
	: ';'	{ yyerrok; want_assign = 0; }
	;

comma	: ',' opt_nls	{ yyerrok; }
	;

%%

struct token {
	const char *operator;		/* text to match */
	NODETYPE value;		/* node type */
	int class;		/* lexical class */
	unsigned flags;		/* # of args. allowed and compatability */
#	define	ARGS	0xFF	/* 0, 1, 2, 3 args allowed (any combination */
#	define	A(n)	(1<<(n))
#	define	VERSION	0xFF00	/* old awk is zero */
#	define	NOT_OLD		0x0100	/* feature not in old awk */
#	define	NOT_POSIX	0x0200	/* feature not in POSIX */
#	define	GAWKX		0x0400	/* gawk extension */
	NODE *(*ptr) ();	/* function that implements this keyword */
};

extern NODE
	*do_exp(),	*do_getline(),	*do_index(),	*do_length(),
	*do_sqrt(),	*do_log(),	*do_sprintf(),	*do_substr(),
	*do_split(),	*do_system(),	*do_int(),	*do_close(),
	*do_atan2(),	*do_sin(),	*do_cos(),	*do_rand(),
	*do_srand(),	*do_match(),	*do_tolower(),	*do_toupper(),
	*do_sub(),	*do_gsub(),	*do_strftime(),	*do_systime();

/* Tokentab is sorted ascii ascending order, so it can be binary searched. */

static struct token tokentab[] = {
{"BEGIN",	Node_illegal,	 LEX_BEGIN,	0,		0},
{"END",		Node_illegal,	 LEX_END,	0,		0},
{"atan2",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(2),	do_atan2},
{"break",	Node_K_break,	 LEX_BREAK,	0,		0},
{"close",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(1),	do_close},
{"continue",	Node_K_continue, LEX_CONTINUE,	0,		0},
{"cos",		Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(1),	do_cos},
{"delete",	Node_K_delete,	 LEX_DELETE,	NOT_OLD,	0},
{"do",		Node_K_do,	 LEX_DO,	NOT_OLD,	0},
{"else",	Node_illegal,	 LEX_ELSE,	0,		0},
{"exit",	Node_K_exit,	 LEX_EXIT,	0,		0},
{"exp",		Node_builtin,	 LEX_BUILTIN,	A(1),		do_exp},
{"for",		Node_K_for,	 LEX_FOR,	0,		0},
{"func",	Node_K_function, LEX_FUNCTION,	NOT_POSIX|NOT_OLD,	0},
{"function",	Node_K_function, LEX_FUNCTION,	NOT_OLD,	0},
{"getline",	Node_K_getline,	 LEX_GETLINE,	NOT_OLD,	0},
{"gsub",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(2)|A(3), do_gsub},
{"if",		Node_K_if,	 LEX_IF,	0,		0},
{"in",		Node_illegal,	 LEX_IN,	0,		0},
{"index",	Node_builtin,	 LEX_BUILTIN,	A(2),		do_index},
{"int",		Node_builtin,	 LEX_BUILTIN,	A(1),		do_int},
{"length",	Node_builtin,	 LEX_LENGTH,	A(0)|A(1),	do_length},
{"log",		Node_builtin,	 LEX_BUILTIN,	A(1),		do_log},
{"match",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(2),	do_match},
{"next",	Node_K_next,	 LEX_NEXT,	0,		0},
{"print",	Node_K_print,	 LEX_PRINT,	0,		0},
{"printf",	Node_K_printf,	 LEX_PRINTF,	0,		0},
{"rand",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(0),	do_rand},
{"return",	Node_K_return,	 LEX_RETURN,	NOT_OLD,	0},
{"sin",		Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(1),	do_sin},
{"split",	Node_builtin,	 LEX_BUILTIN,	A(2)|A(3),	do_split},
{"sprintf",	Node_builtin,	 LEX_BUILTIN,	0,		do_sprintf},
{"sqrt",	Node_builtin,	 LEX_BUILTIN,	A(1),		do_sqrt},
{"srand",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(0)|A(1), do_srand},
{"strftime",	Node_builtin,	 LEX_BUILTIN,	GAWKX|A(1)|A(2), do_strftime},
{"sub",		Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(2)|A(3), do_sub},
{"substr",	Node_builtin,	 LEX_BUILTIN,	A(2)|A(3),	do_substr},
{"system",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(1),	do_system},
{"systime",	Node_builtin,	 LEX_BUILTIN,	GAWKX|A(0),	do_systime},
{"tolower",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(1),	do_tolower},
{"toupper",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(1),	do_toupper},
{"while",	Node_K_while,	 LEX_WHILE,	0,		0},
};

/* VARARGS0 */
static void
yyerror(va_alist)
va_dcl
{
	va_list args;
	const char *mesg = NULL;
	register char *bp, *cp;
	char *scan;
	char buf[120];
	static char end_of_file_line[] = "(END OF FILE)";

	errcount++;
	/* Find the current line in the input file */
	if (lexptr && lexeme) {
		if (!thisline) {
			cp = lexeme;
			if (*cp == '\n') {
				cp--;
				mesg = "unexpected newline";
			}
			for ( ; cp != lexptr_begin && *cp != '\n'; --cp)
				continue;
			if (*cp == '\n')
				cp++;
			thisline = cp;
		}
		/* NL isn't guaranteed */
		bp = lexeme;
		while (bp < lexend && *bp && *bp != '\n')
			bp++;
	} else {
		thisline = end_of_file_line;
		bp = thisline + strlen(thisline);
	}
	msg("%.*s", (int) (bp - thisline), thisline);
	bp = buf;
	cp = buf + sizeof(buf) - 24;	/* 24 more than longest msg. input */
	if (lexptr) {
		scan = thisline;
		while (bp < cp && scan < lexeme)
			if (*scan++ == '\t')
				*bp++ = '\t';
			else
				*bp++ = ' ';
		*bp++ = '^';
		*bp++ = ' ';
	}
	va_start(args);
	if (mesg == NULL)
		mesg = va_arg(args, char *);
	strcpy(bp, mesg);
	err("", buf, args);
	va_end(args);
	exit(2);
}

static char *
get_src_buf()
{
	static int samefile = 0;
	static int nextfile = 0;
	static char *buf = NULL;
	static int fd;
	int n;
	register char *scan;
	static int len = 0;
	static int did_newline = 0;
#	define	SLOP	128	/* enough space to hold most source lines */

again:
	if (nextfile > numfiles)
		return NULL;

	if (srcfiles[nextfile].stype == CMDLINE) {
		if (len == 0) {
			len = strlen(srcfiles[nextfile].val);
			if (len == 0) {
				/*
				 * Yet Another Special case:
				 *	gawk '' /path/name
				 * Sigh.
				 */
				++nextfile;
				goto again;
			}
			sourceline = 1;
			lexptr = lexptr_begin = srcfiles[nextfile].val;
			lexend = lexptr + len;
		} else if (!did_newline && *(lexptr-1) != '\n') {
			/*
			 * The following goop is to ensure that the source
			 * ends with a newline and that the entire current
			 * line is available for error messages.
			 */
			int offset;

			did_newline = 1;
			offset = lexptr - lexeme;
			for (scan = lexeme; scan > lexptr_begin; scan--)
				if (*scan == '\n') {
					scan++;
					break;
				}
			len = lexptr - scan;
			emalloc(buf, char *, len+1, "get_src_buf");
			memcpy(buf, scan, len);
			thisline = buf;
			lexptr = buf + len;
			*lexptr = '\n';
			lexeme = lexptr - offset;
			lexptr_begin = buf;
			lexend = lexptr + 1;
		} else {
			len = 0;
			lexeme = lexptr = lexptr_begin = NULL;
		}
		if (lexptr == NULL && ++nextfile <= numfiles)
			return get_src_buf();
		return lexptr;
	}
	if (!samefile) {
		source = srcfiles[nextfile].val;
		if (source == NULL) {
			if (buf) {
				free(buf);
				buf = NULL;
			}
			len = 0;
			return lexeme = lexptr = lexptr_begin = NULL;
		}
		fd = pathopen(source);
		if (fd == -1)
			fatal("can't open source file \"%s\" for reading (%s)",
				source, strerror(errno));
		len = optimal_bufsize(fd);
		if (buf)
			free(buf);
		emalloc(buf, char *, len + SLOP, "get_src_buf");
		lexptr_begin = buf + SLOP;
		samefile = 1;
		sourceline = 1;
	} else {
		/*
		 * Here, we retain the current source line (up to length SLOP)
		 * in the beginning of the buffer that was overallocated above
		 */
		int offset;
		int linelen;

		offset = lexptr - lexeme;
		for (scan = lexeme; scan > lexptr_begin; scan--)
			if (*scan == '\n') {
				scan++;
				break;
			}
		linelen = lexptr - scan;
		if (linelen > SLOP)
			linelen = SLOP;
		thisline = buf + SLOP - linelen;
		memcpy(thisline, scan, linelen);
		lexeme = buf + SLOP - offset;
		lexptr_begin = thisline;
	}
	n = read(fd, buf + SLOP, len);
	if (n == -1)
		fatal("can't read sourcefile \"%s\" (%s)",
			source, strerror(errno));
	if (n == 0) {
		samefile = 0;
		nextfile++;
		*lexeme = '\0';
		len = 0;
		return get_src_buf();
	}
	lexptr = buf + SLOP;
	lexend = lexptr + n;
	return buf;
}

#define	tokadd(x) (*tok++ = (x), tok == tokend ? tokexpand() : tok)

char *
tokexpand()
{
	static int toksize = 60;
	int tokoffset;

	tokoffset = tok - tokstart;
	toksize *= 2;
	if (tokstart)
		erealloc(tokstart, char *, toksize, "tokexpand");
	else
		emalloc(tokstart, char *, toksize, "tokexpand");
	tokend = tokstart + toksize;
	tok = tokstart + tokoffset;
	return tok;
}

#if DEBUG
char
nextc() {
	if (lexptr && lexptr < lexend)
		return *lexptr++;
	else if (get_src_buf())
		return *lexptr++;
	else
		return '\0';
}
#else
#define	nextc()	((lexptr && lexptr < lexend) ? \
			*lexptr++ : \
			(get_src_buf() ? *lexptr++ : '\0') \
		)
#endif
#define pushback() (lexptr && lexptr > lexptr_begin ? lexptr-- : lexptr)

/*
 * Read the input and turn it into tokens.
 */

static int
yylex()
{
	register int c;
	int seen_e = 0;		/* These are for numbers */
	int seen_point = 0;
	int esc_seen;		/* for literal strings */
	int low, mid, high;
	static int did_newline = 0;
	char *tokkey;

	if (!nextc())
		return 0;
	pushback();
#ifdef OS2
	/*
	 * added for OS/2's extproc feature of cmd.exe
	 * (like #! in BSD sh)
	 */
	if (strncasecmp(lexptr, "extproc ", 8) == 0) {
		while (*lexptr && *lexptr != '\n')
			lexptr++;
	}
#endif
	lexeme = lexptr;
	thisline = NULL;
	if (want_regexp) {
		int in_brack = 0;

		want_regexp = 0;
		tok = tokstart;
		while ((c = nextc()) != 0) {
			switch (c) {
			case '[':
				in_brack = 1;
				break;
			case ']':
				in_brack = 0;
				break;
			case '\\':
				if ((c = nextc()) == '\0') {
					yyerror("unterminated regexp ends with \\ at end of file");
				} else if (c == '\n') {
					sourceline++;
					continue;
				} else
					tokadd('\\');
				break;
			case '/':	/* end of the regexp */
				if (in_brack)
					break;

				pushback();
				tokadd('\0');
				yylval.sval = tokstart;
				return REGEXP;
			case '\n':
				pushback();
				yyerror("unterminated regexp");
			case '\0':
				yyerror("unterminated regexp at end of file");
			}
			tokadd(c);
		}
	}
retry:
	while ((c = nextc()) == ' ' || c == '\t')
		continue;

	lexeme = lexptr ? lexptr - 1 : lexptr;
	thisline = NULL;
	tok = tokstart;
	yylval.nodetypeval = Node_illegal;

	switch (c) {
	case 0:
		return 0;

	case '\n':
		sourceline++;
		return NEWLINE;

	case '#':		/* it's a comment */
		while ((c = nextc()) != '\n') {
			if (c == '\0')
				return 0;
		}
		sourceline++;
		return NEWLINE;

	case '\\':
#ifdef RELAXED_CONTINUATION
		/*
		 * This code puports to allow comments and/or whitespace
		 * after the `\' at the end of a line used for continuation.
		 * Use it at your own risk. We think it's a bad idea, which
		 * is why it's not on by default.
		 */
		if (!do_unix) {
			/* strip trailing white-space and/or comment */
			while ((c = nextc()) == ' ' || c == '\t')
				continue;
			if (c == '#')
				while ((c = nextc()) != '\n')
					if (c == '\0')
						break;
			pushback();
		}
#endif /* RELAXED_CONTINUATION */
		if (nextc() == '\n') {
			sourceline++;
			goto retry;
		} else
			yyerror("backslash not last character on line");
		break;

	case '$':
		want_assign = 1;
		return '$';

	case ')':
	case ']':
	case '(':	
	case '[':
	case ';':
	case ':':
	case '?':
	case '{':
	case ',':
		return c;

	case '*':
		if ((c = nextc()) == '=') {
			yylval.nodetypeval = Node_assign_times;
			return ASSIGNOP;
		} else if (do_posix) {
			pushback();
			return '*';
		} else if (c == '*') {
			/* make ** and **= aliases for ^ and ^= */
			static int did_warn_op = 0, did_warn_assgn = 0;

			if (nextc() == '=') {
				if (do_lint && ! did_warn_assgn) {
					did_warn_assgn = 1;
					warning("**= is not allowed by POSIX");
				}
				yylval.nodetypeval = Node_assign_exp;
				return ASSIGNOP;
			} else {
				pushback();
				if (do_lint && ! did_warn_op) {
					did_warn_op = 1;
					warning("** is not allowed by POSIX");
				}
				return '^';
			}
		}
		pushback();
		return '*';

	case '/':
		if (want_assign) {
			if (nextc() == '=') {
				yylval.nodetypeval = Node_assign_quotient;
				return ASSIGNOP;
			}
			pushback();
		}
		return '/';

	case '%':
		if (nextc() == '=') {
			yylval.nodetypeval = Node_assign_mod;
			return ASSIGNOP;
		}
		pushback();
		return '%';

	case '^':
	{
		static int did_warn_op = 0, did_warn_assgn = 0;

		if (nextc() == '=') {

			if (do_lint && ! did_warn_assgn) {
				did_warn_assgn = 1;
				warning("operator `^=' is not supported in old awk");
			}
			yylval.nodetypeval = Node_assign_exp;
			return ASSIGNOP;
		}
		pushback();
		if (do_lint && ! did_warn_op) {
			did_warn_op = 1;
			warning("operator `^' is not supported in old awk");
		}
		return '^';
	}

	case '+':
		if ((c = nextc()) == '=') {
			yylval.nodetypeval = Node_assign_plus;
			return ASSIGNOP;
		}
		if (c == '+')
			return INCREMENT;
		pushback();
		return '+';

	case '!':
		if ((c = nextc()) == '=') {
			yylval.nodetypeval = Node_notequal;
			return RELOP;
		}
		if (c == '~') {
			yylval.nodetypeval = Node_nomatch;
			want_assign = 0;
			return MATCHOP;
		}
		pushback();
		return '!';

	case '<':
		if (nextc() == '=') {
			yylval.nodetypeval = Node_leq;
			return RELOP;
		}
		yylval.nodetypeval = Node_less;
		pushback();
		return '<';

	case '=':
		if (nextc() == '=') {
			yylval.nodetypeval = Node_equal;
			return RELOP;
		}
		yylval.nodetypeval = Node_assign;
		pushback();
		return ASSIGNOP;

	case '>':
		if ((c = nextc()) == '=') {
			yylval.nodetypeval = Node_geq;
			return RELOP;
		} else if (c == '>') {
			yylval.nodetypeval = Node_redirect_append;
			return APPEND_OP;
		}
		yylval.nodetypeval = Node_greater;
		pushback();
		return '>';

	case '~':
		yylval.nodetypeval = Node_match;
		want_assign = 0;
		return MATCHOP;

	case '}':
		/*
		 * Added did newline stuff.  Easier than
		 * hacking the grammar
		 */
		if (did_newline) {
			did_newline = 0;
			return c;
		}
		did_newline++;
		--lexptr;	/* pick up } next time */
		return NEWLINE;

	case '"':
		esc_seen = 0;
		while ((c = nextc()) != '"') {
			if (c == '\n') {
				pushback();
				yyerror("unterminated string");
			}
			if (c == '\\') {
				c = nextc();
				if (c == '\n') {
					sourceline++;
					continue;
				}
				esc_seen = 1;
				tokadd('\\');
			}
			if (c == '\0') {
				pushback();
				yyerror("unterminated string");
			}
			tokadd(c);
		}
		yylval.nodeval = make_str_node(tokstart,
					tok - tokstart, esc_seen ? SCAN : 0);
		yylval.nodeval->flags |= PERM;
		return YSTRING;

	case '-':
		if ((c = nextc()) == '=') {
			yylval.nodetypeval = Node_assign_minus;
			return ASSIGNOP;
		}
		if (c == '-')
			return DECREMENT;
		pushback();
		return '-';

	case '.':
		c = nextc();
		pushback();
		if (!isdigit(c))
			return '.';
		else
			c = '.';	/* FALL THROUGH */
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		/* It's a number */
		for (;;) {
			int gotnumber = 0;

			tokadd(c);
			switch (c) {
			case '.':
				if (seen_point) {
					gotnumber++;
					break;
				}
				++seen_point;
				break;
			case 'e':
			case 'E':
				if (seen_e) {
					gotnumber++;
					break;
				}
				++seen_e;
				if ((c = nextc()) == '-' || c == '+')
					tokadd(c);
				else
					pushback();
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				break;
			default:
				gotnumber++;
			}
			if (gotnumber)
				break;
			c = nextc();
		}
		pushback();
		yylval.nodeval = make_number(atof(tokstart));
		yylval.nodeval->flags |= PERM;
		return YNUMBER;

	case '&':
		if ((c = nextc()) == '&') {
			yylval.nodetypeval = Node_and;
			for (;;) {
				c = nextc();
				if (c == '\0')
					break;
				if (c == '#') {
					while ((c = nextc()) != '\n' && c != '\0')
						continue;
					if (c == '\0')
						break;
				}
				if (c == '\n')
					sourceline++;
				if (! isspace(c)) {
					pushback();
					break;
				}
			}
			want_assign = 0;
			return LEX_AND;
		}
		pushback();
		return '&';

	case '|':
		if ((c = nextc()) == '|') {
			yylval.nodetypeval = Node_or;
			for (;;) {
				c = nextc();
				if (c == '\0')
					break;
				if (c == '#') {
					while ((c = nextc()) != '\n' && c != '\0')
						continue;
					if (c == '\0')
						break;
				}
				if (c == '\n')
					sourceline++;
				if (! isspace(c)) {
					pushback();
					break;
				}
			}
			want_assign = 0;
			return LEX_OR;
		}
		pushback();
		return '|';
	}

	if (c != '_' && ! isalpha(c))
		yyerror("Invalid char '%c' in expression\n", c);

	/* it's some type of name-type-thing.  Find its length */
	tok = tokstart;
	while (is_identchar(c)) {
		tokadd(c);
		c = nextc();
	}
	tokadd('\0');
	emalloc(tokkey, char *, tok - tokstart, "yylex");
	memcpy(tokkey, tokstart, tok - tokstart);
	pushback();

	/* See if it is a special token.  */
	low = 0;
	high = (sizeof (tokentab) / sizeof (tokentab[0])) - 1;
	while (low <= high) {
		int i/* , c */;

		mid = (low + high) / 2;
		c = *tokstart - tokentab[mid].operator[0];
		i = c ? c : strcmp (tokstart, tokentab[mid].operator);

		if (i < 0) {		/* token < mid */
			high = mid - 1;
		} else if (i > 0) {	/* token > mid */
			low = mid + 1;
		} else {
			if (do_lint) {
				if (tokentab[mid].flags & GAWKX)
					warning("%s() is a gawk extension",
						tokentab[mid].operator);
				if (tokentab[mid].flags & NOT_POSIX)
					warning("POSIX does not allow %s",
						tokentab[mid].operator);
				if (tokentab[mid].flags & NOT_OLD)
					warning("%s is not supported in old awk",
						tokentab[mid].operator);
			}
			if ((do_unix && (tokentab[mid].flags & GAWKX))
			    || (do_posix && (tokentab[mid].flags & NOT_POSIX)))
				break;
			if (tokentab[mid].class == LEX_BUILTIN
			    || tokentab[mid].class == LEX_LENGTH
			   )
				yylval.lval = mid;
			else
				yylval.nodetypeval = tokentab[mid].value;

			free(tokkey);
			return tokentab[mid].class;
		}
	}

	yylval.sval = tokkey;
	if (*lexptr == '(')
		return FUNC_CALL;
	else {
		want_assign = 1;
		return NAME;
	}
}

static NODE *
node_common(op)
NODETYPE op;
{
	register NODE *r;

	getnode(r);
	r->type = op;
	r->flags = MALLOC;
	/* if lookahead is NL, lineno is 1 too high */
	if (lexeme && *lexeme == '\n')
		r->source_line = sourceline - 1;
	else
		r->source_line = sourceline;
	r->source_file = source;
	return r;
}

/*
 * This allocates a node with defined lnode and rnode. 
 */
NODE *
node(left, op, right)
NODE *left, *right;
NODETYPE op;
{
	register NODE *r;

	r = node_common(op);
	r->lnode = left;
	r->rnode = right;
	return r;
}

/*
 * This allocates a node with defined subnode and proc for builtin functions
 * Checks for arg. count and supplies defaults where possible.
 */
static NODE *
snode(subn, op, idx)
NODETYPE op;
int idx;
NODE *subn;
{
	register NODE *r;
	register NODE *n;
	int nexp = 0;
	int args_allowed;

	r = node_common(op);

	/* traverse expression list to see how many args. given */
	for (n= subn; n; n= n->rnode) {
		nexp++;
		if (nexp > 3)
			break;
	}

	/* check against how many args. are allowed for this builtin */
	args_allowed = tokentab[idx].flags & ARGS;
	if (args_allowed && !(args_allowed & A(nexp)))
		fatal("%s() cannot have %d argument%c",
			tokentab[idx].operator, nexp, nexp == 1 ? ' ' : 's');

	r->proc = tokentab[idx].ptr;

	/* special case processing for a few builtins */
	if (nexp == 0 && r->proc == do_length) {
		subn = node(node(make_number(0.0),Node_field_spec,(NODE *)NULL),
		            Node_expression_list,
			    (NODE *) NULL);
	} else if (r->proc == do_match) {
		if (subn->rnode->lnode->type != Node_regex)
			subn->rnode->lnode = mk_rexp(subn->rnode->lnode);
	} else if (r->proc == do_sub || r->proc == do_gsub) {
		if (subn->lnode->type != Node_regex)
			subn->lnode = mk_rexp(subn->lnode);
		if (nexp == 2)
			append_right(subn, node(node(make_number(0.0),
						     Node_field_spec,
						     (NODE *) NULL),
					        Node_expression_list,
						(NODE *) NULL));
		else if (do_lint && subn->rnode->rnode->lnode->type == Node_val)
			warning("string literal as last arg of substitute");
	} else if (r->proc == do_split) {
		if (nexp == 2)
			append_right(subn,
			    node(FS_node, Node_expression_list, (NODE *) NULL));
		n = subn->rnode->rnode->lnode;
		if (n->type != Node_regex)
			subn->rnode->rnode->lnode = mk_rexp(n);
		if (nexp == 2)
			subn->rnode->rnode->lnode->re_flags |= FS_DFLT;
	}

	r->subnode = subn;
	return r;
}

/*
 * This allocates a Node_line_range node with defined condpair and
 * zeroes the trigger word to avoid the temptation of assuming that calling
 * 'node( foo, Node_line_range, 0)' will properly initialize 'triggered'. 
 */
/* Otherwise like node() */
static NODE *
mkrangenode(cpair)
NODE *cpair;
{
	register NODE *r;

	getnode(r);
	r->type = Node_line_range;
	r->condpair = cpair;
	r->triggered = 0;
	return r;
}

/* Build a for loop */
static NODE *
make_for_loop(init, cond, incr)
NODE *init, *cond, *incr;
{
	register FOR_LOOP_HEADER *r;
	NODE *n;

	emalloc(r, FOR_LOOP_HEADER *, sizeof(FOR_LOOP_HEADER), "make_for_loop");
	getnode(n);
	n->type = Node_illegal;
	r->init = init;
	r->cond = cond;
	r->incr = incr;
	n->sub.nodep.r.hd = r;
	return n;
}

/*
 * Install a name in the symbol table, even if it is already there.
 * Caller must check against redefinition if that is desired. 
 */
NODE *
install(name, value)
char *name;
NODE *value;
{
	register NODE *hp;
	register size_t len;
	register int bucket;

	len = strlen(name);
	bucket = hash(name, len, (unsigned long) HASHSIZE);
	getnode(hp);
	hp->type = Node_hashnode;
	hp->hnext = variables[bucket];
	variables[bucket] = hp;
	hp->hlength = len;
	hp->hvalue = value;
	hp->hname = name;
	hp->hvalue->vname = name;
	return hp->hvalue;
}

/* find the most recent hash node for name installed by install */
NODE *
lookup(name)
const char *name;
{
	register NODE *bucket;
	register size_t len;

	len = strlen(name);
	bucket = variables[hash(name, len, (unsigned long) HASHSIZE)];
	while (bucket) {
		if (bucket->hlength == len && STREQN(bucket->hname, name, len))
			return bucket->hvalue;
		bucket = bucket->hnext;
	}
	return NULL;
}

/*
 * Add new to the rightmost branch of LIST.  This uses n^2 time, so we make
 * a simple attempt at optimizing it.
 */
static NODE *
append_right(list, new)
NODE *list, *new;
{
	register NODE *oldlist;
	static NODE *savefront = NULL, *savetail = NULL;

	oldlist = list;
	if (savefront == oldlist) {
		savetail = savetail->rnode = new;
		return oldlist;
	} else
		savefront = oldlist;
	while (list->rnode != NULL)
		list = list->rnode;
	savetail = list->rnode = new;
	return oldlist;
}

/*
 * check if name is already installed;  if so, it had better have Null value,
 * in which case def is added as the value. Otherwise, install name with def
 * as value. 
 */
static void
func_install(params, def)
NODE *params;
NODE *def;
{
	NODE *r;

	pop_params(params->rnode);
	pop_var(params, 0);
	r = lookup(params->param);
	if (r != NULL) {
		fatal("function name `%s' previously defined", params->param);
	} else
		(void) install(params->param, node(params, Node_func, def));
}

static void
pop_var(np, freeit)
NODE *np;
int freeit;
{
	register NODE *bucket, **save;
	register size_t len;
	char *name;

	name = np->param;
	len = strlen(name);
	save = &(variables[hash(name, len, (unsigned long) HASHSIZE)]);
	for (bucket = *save; bucket; bucket = bucket->hnext) {
		if (len == bucket->hlength && STREQN(bucket->hname, name, len)) {
			*save = bucket->hnext;
			freenode(bucket);
			if (freeit)
				free(np->param);
			return;
		}
		save = &(bucket->hnext);
	}
}

static void
pop_params(params)
NODE *params;
{
	register NODE *np;

	for (np = params; np != NULL; np = np->rnode)
		pop_var(np, 1);
}

static NODE *
make_param(name)
char *name;
{
	NODE *r;

	getnode(r);
	r->type = Node_param_list;
	r->rnode = NULL;
	r->param = name;
	r->param_cnt = param_counter++;
	return (install(name, r));
}

/* Name points to a variable name.  Make sure its in the symbol table */
NODE *
variable(name, can_free)
char *name;
int can_free;
{
	register NODE *r;
	static int env_loaded = 0;

	if (!env_loaded && STREQ(name, "ENVIRON")) {
		load_environ();
		env_loaded = 1;
	}
	if ((r = lookup(name)) == NULL)
		r = install(name, node(Nnull_string, Node_var, (NODE *) NULL));
	else if (can_free)
		free(name);
	return r;
}

static NODE *
mk_rexp(exp)
NODE *exp;
{
	if (exp->type == Node_regex)
		return exp;
	else {
		NODE *n;

		getnode(n);
		n->type = Node_regex;
		n->re_exp = exp;
		n->re_text = NULL;
		n->re_reg = NULL;
		n->re_flags = 0;
		n->re_cnt = 1;
		return n;
	}
}
