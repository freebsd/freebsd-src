/*
 * awk.y --- yacc/bison parser
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991-2001 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Programming Language.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 */

%{
#ifdef GAWKDEBUG
#define YYDEBUG 12
#endif

#include "awk.h"

#define CAN_FREE	TRUE
#define DONT_FREE	FALSE

#if defined(HAVE_STDARG_H) && defined(__STDC__) && __STDC__
static void yyerror(const char *m, ...) ;
#else
static void yyerror(); /* va_alist */
#endif
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
static int dup_parms P((NODE *func));
static void param_sanity P((NODE *arglist));
static void parms_shadow P((const char *fname, NODE *func));
static int isnoeffect P((NODETYPE t));
static int isassignable P((NODE *n));
static void dumpintlstr P((char *str, size_t len));
static void count_args P((NODE *n));

enum defref { FUNC_DEFINE, FUNC_USE };
static void func_use P((char *name, enum defref how));
static void check_funcs P((void));

static int want_assign;		/* lexical scanning kludge */
static int want_regexp;		/* lexical scanning kludge */
static int can_return;		/* lexical scanning kludge */
static int io_allowed = TRUE;	/* lexical scanning kludge */
static int parsing_end_rule = FALSE; /* for warnings */
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

static long func_count;		/* total number of functions */

#define HASHSIZE	1021	/* this constant only used here */
NODE *variables[HASHSIZE];
static int var_count;		/* total number of global variables */

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
%token <nodetypeval> LEX_GETLINE LEX_NEXTFILE
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
%nonassoc RELOP '<' '>' '|' APPEND_OP TWOWAYIO
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
		{
			expression_value = $2;
			check_funcs();
		}
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
						(NODE*) NULL);
				$$ = append_right($1,
				   node($2, Node_rule_list, (NODE *) NULL));
			}
			yyerrok;
		}
	| error	{ $$ = NULL; }
	| program error { $$ = NULL; }
	| /* empty */ { $$ = NULL; }
	;

rule
	: LEX_BEGIN { io_allowed = FALSE; }
	  action
	  {
		if (begin_block != NULL) {
			if (begin_block->type != Node_rule_list)
				begin_block = node(begin_block, Node_rule_list,
					(NODE *) NULL);
			(void) append_right(begin_block, node(
			    node((NODE *) NULL, Node_rule_node, $3),
			    Node_rule_list, (NODE *) NULL) );
		} else
			begin_block = node((NODE *) NULL, Node_rule_node, $3);
		$$ = NULL;
		io_allowed = TRUE;
		yyerrok;
	  }
	| LEX_END { io_allowed = FALSE; parsing_end_rule = TRUE; }
	  action
	  {
		if (end_block != NULL) {
			if (end_block->type != Node_rule_list)
				end_block = node(end_block, Node_rule_list,
					(NODE *) NULL);
			(void) append_right (end_block, node(
			    node((NODE *) NULL, Node_rule_node, $3),
			    Node_rule_list, (NODE *) NULL));
		} else
			end_block = node((NODE *) NULL, Node_rule_node, $3);
		$$ = NULL;
		io_allowed = TRUE;
		parsing_end_rule = FALSE;
		yyerrok;
	  }
	| LEX_BEGIN statement_term
	  {
		warning(_("BEGIN blocks must have an action part"));
		errcount++;
		yyerrok;
	  }
	| LEX_END statement_term
	  {
		warning(_("END blocks must have an action part"));
		errcount++;
		yyerrok;
	  }
	| pattern action
		{ $$ = node($1, Node_rule_node, $2); yyerrok; }
	| action
		{ $$ = node((NODE *) NULL, Node_rule_node, $1); yyerrok; }
	| pattern statement_term
		{
		  $$ = node($1,
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
		yyerror(_("`%s' is a built-in function, it cannot be redefined"),
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
			NODE *t;

			t = make_param($3);
			t->flags |= FUNC;
			$$ = append_right(t, $5);
			can_return = TRUE;
			/* check for duplicate parameter names */
			if (dup_parms($$))
				errcount++;
		}
	;

function_body
	: l_brace statements r_brace opt_semi opt_nls
	  {
		$$ = $2;
		can_return = FALSE;
	  }
	| l_brace r_brace opt_semi opt_nls
	  {
		$$ = node((NODE *) NULL, Node_K_return, (NODE *) NULL);
		can_return = FALSE;
	  }
	;


pattern
	: exp
		{ $$ = $1; }
	| exp ',' exp
		{ $$ = mkrangenode(node($1, Node_cond_pair, $3)); }
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
		  n->re_reg = make_regexp($3, len, FALSE, TRUE);
		  n->re_text = NULL;
		  n->re_flags = CONST;
		  n->re_cnt = 1;
		  $$ = n;
		}
	;

action
	: l_brace statements r_brace opt_semi opt_nls
		{ $$ = $2; }
	| l_brace r_brace opt_semi opt_nls
		{ $$ = NULL; }
	;

statements
	: statement
		{
			$$ = $1;
			if (do_lint && isnoeffect($$->type))
				lintwarn(_("statement may have no effect"));
		}
	| statements statement
		{
			if ($1 == NULL || $1->type != Node_statement_list)
				$1 = node($1, Node_statement_list, (NODE *) NULL);
	    		$$ = append_right($1,
				node($2, Node_statement_list, (NODE *)   NULL));
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
		{ $$ = node($3, Node_K_while, $6); }
	| LEX_DO opt_nls statement LEX_WHILE '(' exp r_paren opt_nls
		{ $$ = node($6, Node_K_do, $3); }
	| LEX_FOR '(' NAME LEX_IN NAME r_paren opt_nls statement
	  {
		/*
		 * Efficiency hack.  Recognize the special case of
		 *
		 * 	for (iggy in foo)
		 * 		delete foo[iggy]
		 *
		 * and treat it as if it were
		 *
		 * 	delete foo
		 *
		 * Check that the body is a `delete a[i]' statement,
		 * and that both the loop var and array names match.
		 */
		if ($8 != NULL && $8->type == Node_K_delete
		    && $8->rnode != NULL
		    && ($8->rnode->type == Node_var || $8->rnode->type == Node_param_list)
		    && strcmp($3, $8->rnode->var_value->vname) == 0
		    && strcmp($5, $8->lnode->vname) == 0) {
			$8->type = Node_K_delete_loop;
			$$ = $8;
		} else {
			$$ = node($8, Node_K_arrayfor,
				make_for_loop(variable($3, CAN_FREE, Node_var),
				(NODE *) NULL, variable($5, CAN_FREE, Node_var_array)));
		}
	  }
	| LEX_FOR '(' opt_exp semi opt_nls exp semi opt_nls opt_exp r_paren opt_nls statement
	  {
		$$ = node($12, Node_K_for, (NODE *) make_for_loop($3, $6, $9));
	  }
	| LEX_FOR '(' opt_exp semi opt_nls semi opt_nls opt_exp r_paren opt_nls statement
	  {
		$$ = node($11, Node_K_for,
			(NODE *) make_for_loop($3, (NODE *) NULL, $8));
	  }
	| LEX_BREAK statement_term
	   /* for break, maybe we'll have to remember where to break to */
		{ $$ = node((NODE *) NULL, Node_K_break, (NODE *) NULL); }
	| LEX_CONTINUE statement_term
	   /* similarly */
		{ $$ = node((NODE *) NULL, Node_K_continue, (NODE *) NULL); }
	| print '(' expression_list r_paren output_redir statement_term
		{
			$$ = node($3, $1, $5);
			if ($$->type == Node_K_printf)
				count_args($$)
		}
	| print opt_rexpression_list output_redir statement_term
		{
			if ($1 == Node_K_print && $2 == NULL) {
				static int warned = FALSE;

				$2 = node(node(make_number(0.0),
					       Node_field_spec,
					       (NODE *) NULL),
					  Node_expression_list,
					  (NODE *) NULL);

				if (do_lint && ! io_allowed && ! warned) {
					warned = TRUE;
					lintwarn(
	_("plain `print' in BEGIN or END rule should probably be `print \"\"'"));
				}
			}

			$$ = node($2, $1, $3);
			if ($$->type == Node_K_printf)
				count_args($$)
		}
	| LEX_NEXT statement_term
		{ NODETYPE type;

		  if (! io_allowed)
			yyerror(_("`next' used in BEGIN or END action"));
		  type = Node_K_next;
		  $$ = node((NODE *) NULL, type, (NODE *) NULL);
		}
	| LEX_NEXTFILE statement_term
		{
		  if (do_lint)
			lintwarn(_("`nextfile' is a gawk extension"));
		  if (do_traditional) {
			/*
			 * can't use yyerror, since may have overshot
			 * the source line
			 */
			errcount++;
			error(_("`nextfile' is a gawk extension"));
		  }
		  if (! io_allowed) {
			/* same thing */
			errcount++;
			error(_("`nextfile' used in BEGIN or END action"));
		  }
		  $$ = node((NODE *) NULL, Node_K_nextfile, (NODE *) NULL);
		}
	| LEX_EXIT opt_exp statement_term
		{ $$ = node($2, Node_K_exit, (NODE *) NULL); }
	| LEX_RETURN
		{
		  if (! can_return)
			yyerror(_("`return' used outside function context"));
		}
	  opt_exp statement_term
		{ $$ = node($3, Node_K_return, (NODE *) NULL); }
	| LEX_DELETE NAME '[' expression_list ']' statement_term
		{ $$ = node(variable($2, CAN_FREE, Node_var_array), Node_K_delete, $4); }
	| LEX_DELETE NAME  statement_term
		{
		  if (do_lint)
			lintwarn(_("`delete array' is a gawk extension"));
		  if (do_traditional) {
			/*
			 * can't use yyerror, since may have overshot
			 * the source line
			 */
			errcount++;
			error(_("`delete array' is a gawk extension"));
		  }
		  $$ = node(variable($2, CAN_FREE, Node_var_array), Node_K_delete, (NODE *) NULL);
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
			node($6, Node_if_branches, (NODE *) NULL));
	  }
	| LEX_IF '(' exp r_paren opt_nls statement
	     LEX_ELSE opt_nls statement
		{ $$ = node($3, Node_K_if,
				node($6, Node_if_branches, $9)); }
	;

nls
	: NEWLINE
		{ want_assign = FALSE; }
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
		{ $$ = node($2, Node_redirect_input, (NODE *) NULL); }
	;

output_redir
	: /* empty */
		{ $$ = NULL; }
	| '>' exp
		{ $$ = node($2, Node_redirect_output, (NODE *) NULL); }
	| APPEND_OP exp
		{ $$ = node($2, Node_redirect_append, (NODE *) NULL); }
	| '|' exp
		{ $$ = node($2, Node_redirect_pipe, (NODE *) NULL); }
	| TWOWAYIO exp
		{
		  if ($2->type == Node_K_getline
		      && $2->rnode->type == Node_redirect_twoway)
			yyerror(_("multistage two-way pipelines don't work"));
		  $$ = node($2, Node_redirect_twoway, (NODE *) NULL);
		}
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
		{ $$ = node($1, Node_expression_list, (NODE *) NULL); }
	| rexpression_list comma rexp
	  {
		$$ = append_right($1,
			node($3, Node_expression_list, (NODE *) NULL));
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
		{ $$ = node($1, Node_expression_list, (NODE *) NULL); }
	| expression_list comma exp
		{
			$$ = append_right($1,
				node($3, Node_expression_list, (NODE *) NULL));
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
		{ want_assign = FALSE; }
	  exp
		{
		  if (do_lint && $4->type == Node_regex)
			lintwarn(_("regular expression on right of assignment"));
		  $$ = node($1, $2, $4);
		}
	| '(' expression_list r_paren LEX_IN NAME
		{ $$ = node(variable($5, CAN_FREE, Node_var_array), Node_in_array, $2); }
	| exp '|' LEX_GETLINE opt_variable
		{
		  $$ = node($4, Node_K_getline,
			 node($1, Node_redirect_pipein, (NODE *) NULL));
		}
	| exp TWOWAYIO LEX_GETLINE opt_variable
		{
		  $$ = node($4, Node_K_getline,
			 node($1, Node_redirect_twoway, (NODE *) NULL));
		}
	| LEX_GETLINE opt_variable input_redir
		{
		  if (do_lint && ! io_allowed && parsing_end_rule && $3 == NULL)
			lintwarn(_("non-redirected `getline' undefined inside END action"));
		  $$ = node($2, Node_K_getline, $3);
		}
	| exp LEX_AND exp
		{ $$ = node($1, Node_and, $3); }
	| exp LEX_OR exp
		{ $$ = node($1, Node_or, $3); }
	| exp MATCHOP exp
		{
		  if ($1->type == Node_regex)
			warning(_("regular expression on left of `~' or `!~' operator"));
		  $$ = node($1, $2, mk_rexp($3));
		}
	| regexp
		{
		  $$ = $1;
		  if (do_lint && tokstart[0] == '*') {
			/* possible C comment */
			int n = strlen(tokstart) - 1;
			if (tokstart[n] == '*')
				lintwarn(_("regexp constant `/%s/' looks like a C comment, but is not"), tokstart);
		  }
		}
	| '!' regexp %prec UNARY
		{
		  $$ = node(node(make_number(0.0),
				 Node_field_spec,
				 (NODE *) NULL),
		            Node_nomatch,
			    $2);
		}
	| exp LEX_IN NAME
		{ $$ = node(variable($3, CAN_FREE, Node_var_array), Node_in_array, $1); }
	| exp RELOP exp
		{
		  if (do_lint && $3->type == Node_regex)
			lintwarn(_("regular expression on right of comparison"));
		  $$ = node($1, $2, $3);
		}
	| exp '<' exp
		{ $$ = node($1, Node_less, $3); }
	| exp '>' exp
		{ $$ = node($1, Node_greater, $3); }
	| exp '?' exp ':' exp
		{ $$ = node($1, Node_cond_exp, node($3, Node_if_branches, $5));}
	| simp_exp
		{ $$ = $1; }
	| exp simp_exp %prec CONCAT_OP
		{ $$ = node($1, Node_concat, $2); }
	;

rexp	
	: variable ASSIGNOP 
		{ want_assign = FALSE; }
	  rexp
		{ $$ = node($1, $2, $4); }
	| rexp LEX_AND rexp
		{ $$ = node($1, Node_and, $3); }
	| rexp LEX_OR rexp
		{ $$ = node($1, Node_or, $3); }
	| LEX_GETLINE opt_variable input_redir
		{
		  if (do_lint && ! io_allowed && $3 == NULL)
			lintwarn(_("non-redirected `getline' undefined inside BEGIN or END action"));
		  $$ = node($2, Node_K_getline, $3);
		}
	| regexp
		{ $$ = $1; } 
	| '!' regexp %prec UNARY
		{ $$ = node((NODE *) NULL, Node_nomatch, $2); }
	| rexp MATCHOP rexp
		 { $$ = node($1, $2, mk_rexp($3)); }
	| rexp LEX_IN NAME
		{ $$ = node(variable($3, CAN_FREE, Node_var_array), Node_in_array, $1); }
	| rexp RELOP rexp
		{ $$ = node($1, $2, $3); }
	| rexp '?' rexp ':' rexp
		{ $$ = node($1, Node_cond_exp, node($3, Node_if_branches, $5));}
	| simp_exp
		{ $$ = $1; }
	| rexp simp_exp %prec CONCAT_OP
		{ $$ = node($1, Node_concat, $2); }
	;

simp_exp
	: non_post_simp_exp
	/* Binary operators in order of decreasing precedence.  */
	| simp_exp '^' simp_exp
		{ $$ = node($1, Node_exp, $3); }
	| simp_exp '*' simp_exp
		{ $$ = node($1, Node_times, $3); }
	| simp_exp '/' simp_exp
		{ $$ = node($1, Node_quotient, $3); }
	| simp_exp '%' simp_exp
		{ $$ = node($1, Node_mod, $3); }
	| simp_exp '+' simp_exp
		{ $$ = node($1, Node_plus, $3); }
	| simp_exp '-' simp_exp
		{ $$ = node($1, Node_minus, $3); }
	| variable INCREMENT
		{ $$ = node($1, Node_postincrement, (NODE *) NULL); }
	| variable DECREMENT
		{ $$ = node($1, Node_postdecrement, (NODE *) NULL); }
	;

non_post_simp_exp
	: '!' simp_exp %prec UNARY
		{ $$ = node($2, Node_not, (NODE *) NULL); }
	| '(' exp r_paren
		{ $$ = $2; }
	| LEX_BUILTIN
	  '(' opt_expression_list r_paren
		{ $$ = snode($3, Node_builtin, (int) $1); }
	| LEX_LENGTH '(' opt_expression_list r_paren
		{ $$ = snode($3, Node_builtin, (int) $1); }
	| LEX_LENGTH
	  {
		if (do_lint)
			lintwarn(_("call of `length' without parentheses is not portable"));
		$$ = snode((NODE *) NULL, Node_builtin, (int) $1);
		if (do_posix)
			warning(_("call of `length' without parentheses is deprecated by POSIX"));
	  }
	| FUNC_CALL '(' opt_expression_list r_paren
	  {
		$$ = node($3, Node_func_call, make_string($1, strlen($1)));
		func_use($1, FUNC_USE);
		param_sanity($3);
		free($1);
	  }
	| variable
	| INCREMENT variable
		{ $$ = node($2, Node_preincrement, (NODE *) NULL); }
	| DECREMENT variable
		{ $$ = node($2, Node_predecrement, (NODE *) NULL); }
	| YNUMBER
		{ $$ = $1; }
	| YSTRING
		{ $$ = $1; }

	| '-' simp_exp    %prec UNARY
		{
		  if ($2->type == Node_val) {
			$2->numbr = -(force_number($2));
			$$ = $2;
		  } else
			$$ = node($2, Node_unary_minus, (NODE *) NULL);
		}
	| '+' simp_exp    %prec UNARY
		{
		  /*
		   * was: $$ = $2
		   * POSIX semantics: force a conversion to numeric type
		   */
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
		{ $$ = variable($1, CAN_FREE, Node_var); }
	| NAME '[' expression_list ']'
		{
		if ($3 == NULL) {
			fatal(_("invalid subscript expression"));
		} else if ($3->rnode == NULL) {
			$$ = node(variable($1, CAN_FREE, Node_var_array), Node_subscript, $3->lnode);
			freenode($3);
		} else
			$$ = node(variable($1, CAN_FREE, Node_var_array), Node_subscript, $3);
		}
	| '$' non_post_simp_exp
		{ $$ = node($2, Node_field_spec, (NODE *) NULL); }
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
	: ';'	{ yyerrok; want_assign = FALSE; }
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
#	define	VERSION_MASK	0xFF00	/* old awk is zero */
#	define	NOT_OLD		0x0100	/* feature not in old awk */
#	define	NOT_POSIX	0x0200	/* feature not in POSIX */
#	define	GAWKX		0x0400	/* gawk extension */
#	define	RESX		0x0800	/* Bell Labs Research extension */
	NODE *(*ptr)();		/* function that implements this keyword */
};

/* Tokentab is sorted ascii ascending order, so it can be binary searched. */
/* Function pointers come from declarations in awk.h. */

static struct token tokentab[] = {
{"BEGIN",	Node_illegal,	 LEX_BEGIN,	0,		0},
{"END",		Node_illegal,	 LEX_END,	0,		0},
#ifdef ARRAYDEBUG
{"adump",	Node_builtin,    LEX_BUILTIN,	GAWKX|A(1),	do_adump},
#endif
{"and",		Node_builtin,    LEX_BUILTIN,	GAWKX|A(2),	do_and},
{"asort",	Node_builtin,	 LEX_BUILTIN,	GAWKX|A(1)|A(2),	do_asort},
{"atan2",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(2),	do_atan2},
{"bindtextdomain",	Node_builtin,	 LEX_BUILTIN,	GAWKX|A(1)|A(2),	do_bindtextdomain},
{"break",	Node_K_break,	 LEX_BREAK,	0,		0},
{"close",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(1)|A(2),	do_close},
{"compl",	Node_builtin,    LEX_BUILTIN,	GAWKX|A(1),	do_compl},
{"continue",	Node_K_continue, LEX_CONTINUE,	0,		0},
{"cos",		Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(1),	do_cos},
{"dcgettext",	Node_builtin,	 LEX_BUILTIN,	GAWKX|A(1)|A(2)|A(3),	do_dcgettext},
{"delete",	Node_K_delete,	 LEX_DELETE,	NOT_OLD,	0},
{"do",		Node_K_do,	 LEX_DO,	NOT_OLD,	0},
{"else",	Node_illegal,	 LEX_ELSE,	0,		0},
{"exit",	Node_K_exit,	 LEX_EXIT,	0,		0},
{"exp",		Node_builtin,	 LEX_BUILTIN,	A(1),		do_exp},
{"extension",	Node_builtin,	 LEX_BUILTIN,	GAWKX|A(2),	do_ext},
{"fflush",	Node_builtin,	 LEX_BUILTIN,	RESX|A(0)|A(1), do_fflush},
{"for",		Node_K_for,	 LEX_FOR,	0,		0},
{"func",	Node_K_function, LEX_FUNCTION,	NOT_POSIX|NOT_OLD,	0},
{"function",	Node_K_function, LEX_FUNCTION,	NOT_OLD,	0},
{"gensub",	Node_builtin,	 LEX_BUILTIN,	GAWKX|A(3)|A(4), do_gensub},
{"getline",	Node_K_getline,	 LEX_GETLINE,	NOT_OLD,	0},
{"gsub",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(2)|A(3), do_gsub},
{"if",		Node_K_if,	 LEX_IF,	0,		0},
{"in",		Node_illegal,	 LEX_IN,	0,		0},
{"index",	Node_builtin,	 LEX_BUILTIN,	A(2),		do_index},
{"int",		Node_builtin,	 LEX_BUILTIN,	A(1),		do_int},
{"length",	Node_builtin,	 LEX_LENGTH,	A(0)|A(1),	do_length},
{"log",		Node_builtin,	 LEX_BUILTIN,	A(1),		do_log},
{"lshift",	Node_builtin,    LEX_BUILTIN,	GAWKX|A(2),	do_lshift},
{"match",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(2)|A(3), do_match},
{"mktime",	Node_builtin,	 LEX_BUILTIN,	GAWKX|A(1),	do_mktime},
{"next",	Node_K_next,	 LEX_NEXT,	0,		0},
{"nextfile",	Node_K_nextfile, LEX_NEXTFILE,	GAWKX,		0},
{"or",		Node_builtin,    LEX_BUILTIN,	GAWKX|A(2),	do_or},
{"print",	Node_K_print,	 LEX_PRINT,	0,		0},
{"printf",	Node_K_printf,	 LEX_PRINTF,	0,		0},
{"rand",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(0),	do_rand},
{"return",	Node_K_return,	 LEX_RETURN,	NOT_OLD,	0},
{"rshift",	Node_builtin,    LEX_BUILTIN,	GAWKX|A(2),	do_rshift},
{"sin",		Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(1),	do_sin},
{"split",	Node_builtin,	 LEX_BUILTIN,	A(2)|A(3),	do_split},
{"sprintf",	Node_builtin,	 LEX_BUILTIN,	0,		do_sprintf},
{"sqrt",	Node_builtin,	 LEX_BUILTIN,	A(1),		do_sqrt},
{"srand",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(0)|A(1), do_srand},
#if defined(GAWKDEBUG) || defined(ARRAYDEBUG) /* || ... */
{"stopme",	Node_builtin,    LEX_BUILTIN,	GAWKX|A(0),	stopme},
#endif
{"strftime",	Node_builtin,	 LEX_BUILTIN,	GAWKX|A(0)|A(1)|A(2), do_strftime},
{"strtonum",	Node_builtin,    LEX_BUILTIN,	GAWKX|A(1),	do_strtonum},
{"sub",		Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(2)|A(3), do_sub},
{"substr",	Node_builtin,	 LEX_BUILTIN,	A(2)|A(3),	do_substr},
{"system",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(1),	do_system},
{"systime",	Node_builtin,	 LEX_BUILTIN,	GAWKX|A(0),	do_systime},
{"tolower",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(1),	do_tolower},
{"toupper",	Node_builtin,	 LEX_BUILTIN,	NOT_OLD|A(1),	do_toupper},
{"while",	Node_K_while,	 LEX_WHILE,	0,		0},
{"xor",		Node_builtin,    LEX_BUILTIN,	GAWKX|A(2),	do_xor},
};

/* getfname --- return name of a builtin function (for pretty printing) */

const char *
getfname(register NODE *(*fptr)())
{
	register int i, j;

	j = sizeof(tokentab) / sizeof(tokentab[0]);
	/* linear search, no other way to do it */
	for (i = 0; i < j; i++) 
		if (tokentab[i].ptr == fptr)
			return tokentab[i].operator;

	fatal(_("fptr %x not in tokentab\n"), fptr);
	return NULL;    /* to stop warnings */
}

/* yyerror --- print a syntax error message, show where */

/*
 * Function identifier purposely indented to avoid mangling
 * by ansi2knr.  Sigh.
 */

static void
#if defined(HAVE_STDARG_H) && defined(__STDC__) && __STDC__
  yyerror(const char *m, ...)
#else
/* VARARGS0 */
  yyerror(va_alist)
  va_dcl
#endif
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
		if (thisline == NULL) {
			cp = lexeme;
			if (*cp == '\n') {
				cp--;
				mesg = _("unexpected newline");
			}
			for (; cp != lexptr_begin && *cp != '\n'; --cp)
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
	if (lexptr != NULL) {
		scan = thisline;
		while (bp < cp && scan < lexeme)
			if (*scan++ == '\t')
				*bp++ = '\t';
			else
				*bp++ = ' ';
		*bp++ = '^';
		*bp++ = ' ';
	}
#if defined(HAVE_STDARG_H) && defined(__STDC__) && __STDC__
	va_start(args, m);
	if (mesg == NULL)
		mesg = m;
#else
	va_start(args);
	if (mesg == NULL)
		mesg = va_arg(args, char *);
#endif
	strcpy(bp, mesg);
	err("", buf, args);
	va_end(args);
}

/* get_src_buf --- read the next buffer of source program */

static char *
get_src_buf()
{
	static int samefile = FALSE;
	static int nextfile = 0;
	static char *buf = NULL;
	static int fd;
	int n;
	register char *scan;
	static int len = 0;
	static int did_newline = FALSE;
	int newfile;
	struct stat sbuf;

#	define	SLOP	128	/* enough space to hold most source lines */

again:
	newfile = FALSE;
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
				static int warned = FALSE;

				if (do_lint && ! warned) {
					warned = TRUE;
					lintwarn(_("empty program text on command line"));
				}
				++nextfile;
				goto again;
			}
			sourceline = 1;
			lexptr = lexptr_begin = srcfiles[nextfile].val;
			lexend = lexptr + len;
		} else if (! did_newline && *(lexptr-1) != '\n') {
			/*
			 * The following goop is to ensure that the source
			 * ends with a newline and that the entire current
			 * line is available for error messages.
			 */
			int offset;

			did_newline = TRUE;
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
			goto again;
		return lexptr;
	}
	if (! samefile) {
		source = srcfiles[nextfile].val;
		if (source == NULL) {
			if (buf != NULL) {
				free(buf);
				buf = NULL;
			}
			len = 0;
			return lexeme = lexptr = lexptr_begin = NULL;
		}
		fd = pathopen(source);
		if (fd <= INVALID_HANDLE) {
			char *in;

			/* suppress file name and line no. in error mesg */
			in = source;
			source = NULL;
			fatal(_("can't open source file `%s' for reading (%s)"),
				in, strerror(errno));
		}
		len = optimal_bufsize(fd, & sbuf);
		newfile = TRUE;
		if (buf != NULL)
			free(buf);
		emalloc(buf, char *, len + SLOP, "get_src_buf");
		lexptr_begin = buf + SLOP;
		samefile = TRUE;
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
		fatal(_("can't read sourcefile `%s' (%s)"),
			source, strerror(errno));
	if (n == 0) {
		if (newfile) {
			static int warned = FALSE;

			if (do_lint && ! warned) {
				warned = TRUE;
				lintwarn(_("source file `%s' is empty"), source);
			}
		}
		if (fd != fileno(stdin)) /* safety */
			close(fd);
		samefile = FALSE;
		nextfile++;
		if (lexeme)
			*lexeme = '\0';
		len = 0;
		goto again;
	}
	lexptr = buf + SLOP;
	lexend = lexptr + n;
	return buf;
}

/* tokadd --- add a character to the token buffer */

#define	tokadd(x) (*tok++ = (x), tok == tokend ? tokexpand() : tok)

/* tokexpand --- grow the token buffer */

char *
tokexpand()
{
	static int toksize = 60;
	int tokoffset;

	tokoffset = tok - tokstart;
	toksize *= 2;
	if (tokstart != NULL)
		erealloc(tokstart, char *, toksize, "tokexpand");
	else
		emalloc(tokstart, char *, toksize, "tokexpand");
	tokend = tokstart + toksize;
	tok = tokstart + tokoffset;
	return tok;
}

/* nextc --- get the next input character */

#if GAWKDEBUG
int
nextc()
{
	int c;

	if (lexptr && lexptr < lexend)
		c = (int) (unsigned char) *lexptr++;
	else if (get_src_buf())
		c = (int) (unsigned char) *lexptr++;
	else
		c = EOF;

	return c;
}
#else
#define	nextc()	((lexptr && lexptr < lexend) ? \
		    ((int) (unsigned char) *lexptr++) : \
		    (get_src_buf() ? ((int) (unsigned char) *lexptr++) : EOF) \
		)
#endif

/* pushback --- push a character back on the input */

#define pushback() (lexptr && lexptr > lexptr_begin ? lexptr-- : lexptr)

/* allow_newline --- allow newline after &&, ||, ? and : */

static void
allow_newline()
{
	int c;

	for (;;) {
		c = nextc();
		if (c == EOF)
			break;
		if (c == '#') {
			while ((c = nextc()) != '\n' && c != EOF)
				continue;
			if (c == EOF)
				break;
		}
		if (c == '\n')
			sourceline++;
		if (! ISSPACE(c)) {
			pushback();
			break;
		}
	}
}

/* yylex --- Read the input and turn it into tokens. */

static int
yylex()
{
	register int c;
	int seen_e = FALSE;		/* These are for numbers */
	int seen_point = FALSE;
	int esc_seen;		/* for literal strings */
	int low, mid, high;
	static int did_newline = FALSE;
	char *tokkey;
	static int lasttok = 0, eof_warned = FALSE;
	int inhex = FALSE;
	int intlstr = FALSE;

	if (nextc() == EOF) {
		if (lasttok != NEWLINE) {
			lasttok = NEWLINE;
			if (do_lint && ! eof_warned) {
				lintwarn(_("source file does not end in newline"));
				eof_warned = TRUE;
			}
			return NEWLINE;	/* fake it */
		}
		return 0;
	}
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
		int in_brack = 0;	/* count brackets, [[:alnum:]] allowed */
		/*
		 * Counting brackets is non-trivial. [[] is ok,
		 * and so is [\]], with a point being that /[/]/ as a regexp
		 * constant has to work.
		 *
		 * Do not count [ or ] if either one is preceded by a \.
		 * A `[' should be counted if
		 *  a) it is the first one so far (in_brack == 0)
		 *  b) it is the `[' in `[:'
		 * A ']' should be counted if not preceded by a \, since
		 * it is either closing `:]' or just a plain list.
		 * According to POSIX, []] is how you put a ] into a set.
		 * Try to handle that too.
		 *
		 * The code for \ handles \[ and \].
		 */

		want_regexp = FALSE;
		tok = tokstart;
		for (;;) {
			c = nextc();
			switch (c) {
			case '[':
				/* one day check for `.' and `=' too */
				if (nextc() == ':' || in_brack == 0)
					in_brack++;
				pushback();
				break;
			case ']':
				if (tokstart[0] == '['
				    && (tok == tokstart + 1
					|| (tok == tokstart + 2
					    && tokstart[1] == '^')))
					/* do nothing */;
				else
					in_brack--;
				break;
			case '\\':
				if ((c = nextc()) == EOF) {
					yyerror(_("unterminated regexp ends with `\\' at end of file"));
					return lasttok = REGEXP; /* kludge */
				} else if (c == '\n') {
					sourceline++;
					continue;
				} else {
					tokadd('\\');
					tokadd(c);
					continue;
				}
				break;
			case '/':	/* end of the regexp */
				if (in_brack > 0)
					break;

				pushback();
				tokadd('\0');
				yylval.sval = tokstart;
				return lasttok = REGEXP;
			case '\n':
				pushback();
				yyerror(_("unterminated regexp"));
				return lasttok = REGEXP;	/* kludge */
			case EOF:
				yyerror(_("unterminated regexp at end of file"));
				return lasttok = REGEXP;	/* kludge */
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
	case EOF:
		if (lasttok != NEWLINE) {
			lasttok = NEWLINE;
			if (do_lint && ! eof_warned) {
				lintwarn(_("source file does not end in newline"));
				eof_warned = TRUE;
			}
			return NEWLINE;	/* fake it */
		}
		return 0;

	case '\n':
		sourceline++;
		return lasttok = NEWLINE;

	case '#':		/* it's a comment */
		while ((c = nextc()) != '\n') {
			if (c == EOF) {
				if (lasttok != NEWLINE) {
					lasttok = NEWLINE;
					if (do_lint && ! eof_warned) {
						lintwarn(
				_("source file does not end in newline"));
						eof_warned = TRUE;
					}
					return NEWLINE;	/* fake it */
				}
				return 0;
			}
		}
		sourceline++;
		return lasttok = NEWLINE;

	case '\\':
#ifdef RELAXED_CONTINUATION
		/*
		 * This code puports to allow comments and/or whitespace
		 * after the `\' at the end of a line used for continuation.
		 * Use it at your own risk. We think it's a bad idea, which
		 * is why it's not on by default.
		 */
		if (! do_traditional) {
			/* strip trailing white-space and/or comment */
			while ((c = nextc()) == ' ' || c == '\t')
				continue;
			if (c == '#') {
				if (do_lint)
					lintwarn(
		_("use of `\\ #...' line continuation is not portable"));
				while ((c = nextc()) != '\n')
					if (c == EOF)
						break;
			}
			pushback();
		}
#endif /* RELAXED_CONTINUATION */
		if (nextc() == '\n') {
			sourceline++;
			goto retry;
		} else {
			yyerror(_("backslash not last character on line"));
			exit(1);
		}
		break;

	case '$':
		want_assign = TRUE;
		return lasttok = '$';

	case ':':
	case '?':
		if (! do_posix)
			allow_newline();
		return lasttok = c;

	case ')':
	case '(':	
	case ';':
	case '{':
	case ',':
		want_assign = FALSE;
		/* fall through */
	case '[':
	case ']':
		return lasttok = c;

	case '*':
		if ((c = nextc()) == '=') {
			yylval.nodetypeval = Node_assign_times;
			return lasttok = ASSIGNOP;
		} else if (do_posix) {
			pushback();
			return lasttok = '*';
		} else if (c == '*') {
			/* make ** and **= aliases for ^ and ^= */
			static int did_warn_op = FALSE, did_warn_assgn = FALSE;

			if (nextc() == '=') {
				if (! did_warn_assgn) {
					did_warn_assgn = TRUE;
					if (do_lint)
						lintwarn(_("POSIX does not allow operator `**='"));
					if (do_lint_old)
						warning(_("old awk does not support operator `**='"));
				}
				yylval.nodetypeval = Node_assign_exp;
				return ASSIGNOP;
			} else {
				pushback();
				if (! did_warn_op) {
					did_warn_op = TRUE;
					if (do_lint)
						lintwarn(_("POSIX does not allow operator `**'"));
					if (do_lint_old)
						warning(_("old awk does not support operator `**'"));
				}
				return lasttok = '^';
			}
		}
		pushback();
		return lasttok = '*';

	case '/':
		if (want_assign) {
			if (nextc() == '=') {
				yylval.nodetypeval = Node_assign_quotient;
				return lasttok = ASSIGNOP;
			}
			pushback();
		}
		return lasttok = '/';

	case '%':
		if (nextc() == '=') {
			yylval.nodetypeval = Node_assign_mod;
			return lasttok = ASSIGNOP;
		}
		pushback();
		return lasttok = '%';

	case '^':
	{
		static int did_warn_op = FALSE, did_warn_assgn = FALSE;

		if (nextc() == '=') {
			if (do_lint_old && ! did_warn_assgn) {
				did_warn_assgn = TRUE;
				warning(_("operator `^=' is not supported in old awk"));
			}
			yylval.nodetypeval = Node_assign_exp;
			return lasttok = ASSIGNOP;
		}
		pushback();
		if (do_lint_old && ! did_warn_op) {
			did_warn_op = TRUE;
			warning(_("operator `^' is not supported in old awk"));
		}
		return lasttok = '^';
	}

	case '+':
		if ((c = nextc()) == '=') {
			yylval.nodetypeval = Node_assign_plus;
			return lasttok = ASSIGNOP;
		}
		if (c == '+')
			return lasttok = INCREMENT;
		pushback();
		return lasttok = '+';

	case '!':
		if ((c = nextc()) == '=') {
			yylval.nodetypeval = Node_notequal;
			return lasttok = RELOP;
		}
		if (c == '~') {
			yylval.nodetypeval = Node_nomatch;
			want_assign = FALSE;
			return lasttok = MATCHOP;
		}
		pushback();
		return lasttok = '!';

	case '<':
		if (nextc() == '=') {
			yylval.nodetypeval = Node_leq;
			return lasttok = RELOP;
		}
		yylval.nodetypeval = Node_less;
		pushback();
		return lasttok = '<';

	case '=':
		if (nextc() == '=') {
			yylval.nodetypeval = Node_equal;
			return lasttok = RELOP;
		}
		yylval.nodetypeval = Node_assign;
		pushback();
		return lasttok = ASSIGNOP;

	case '>':
		if ((c = nextc()) == '=') {
			yylval.nodetypeval = Node_geq;
			return lasttok = RELOP;
		} else if (c == '>') {
			yylval.nodetypeval = Node_redirect_append;
			return lasttok = APPEND_OP;
		}
		yylval.nodetypeval = Node_greater;
		pushback();
		return lasttok = '>';

	case '~':
		yylval.nodetypeval = Node_match;
		want_assign = FALSE;
		return lasttok = MATCHOP;

	case '}':
		/*
		 * Added did newline stuff.  Easier than
		 * hacking the grammar.
		 */
		if (did_newline) {
			did_newline = FALSE;
			return lasttok = c;
		}
		did_newline++;
		--lexptr;	/* pick up } next time */
		return lasttok = NEWLINE;

	case '"':
	string:
		esc_seen = FALSE;
		while ((c = nextc()) != '"') {
			if (c == '\n') {
				pushback();
				yyerror(_("unterminated string"));
				exit(1);
			}
			if (c == '\\') {
				c = nextc();
				if (c == '\n') {
					sourceline++;
					continue;
				}
				esc_seen = TRUE;
				tokadd('\\');
			}
			if (c == EOF) {
				pushback();
				yyerror(_("unterminated string"));
				exit(1);
			}
			tokadd(c);
		}
		yylval.nodeval = make_str_node(tokstart,
					tok - tokstart, esc_seen ? SCAN : 0);
		yylval.nodeval->flags |= PERM;
		if (intlstr) {
			yylval.nodeval->flags |= INTLSTR;
			intlstr = FALSE;
			if (do_intl)
				dumpintlstr(yylval.nodeval->stptr,
						yylval.nodeval->stlen);
 		}
		return lasttok = YSTRING;

	case '-':
		if ((c = nextc()) == '=') {
			yylval.nodetypeval = Node_assign_minus;
			return lasttok = ASSIGNOP;
		}
		if (c == '-')
			return lasttok = DECREMENT;
		pushback();
		return lasttok = '-';

	case '.':
		c = nextc();
		pushback();
		if (! ISDIGIT(c))
			return lasttok = '.';
		else
			c = '.';
		/* FALL THROUGH */
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
			int gotnumber = FALSE;

			tokadd(c);
			switch (c) {
			case 'x':
			case 'X':
				if (do_traditional)
					goto done;
				if (tok == tokstart + 2)
					inhex = TRUE;
				break;
			case '.':
				if (seen_point) {
					gotnumber = TRUE;
					break;
				}
				seen_point = TRUE;
				break;
			case 'e':
			case 'E':
				if (inhex)
					break;
				if (seen_e) {
					gotnumber = TRUE;
					break;
				}
				seen_e = TRUE;
				if ((c = nextc()) == '-' || c == '+')
					tokadd(c);
				else
					pushback();
				break;
			case 'a':
			case 'A':
			case 'b':
			case 'B':
			case 'c':
			case 'C':
			case 'D':
			case 'd':
			case 'f':
			case 'F':
				if (do_traditional || ! inhex)
					goto done;
				/* fall through */
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
			done:
				gotnumber = TRUE;
			}
			if (gotnumber)
				break;
			c = nextc();
		}
		if (c != EOF)
			pushback();
		else if (do_lint && ! eof_warned) {
			lintwarn(_("source file does not end in newline"));
			eof_warned = TRUE;
		}
		tokadd('\0');
		if (! do_traditional && isnondecimal(tokstart))
			yylval.nodeval = make_number(nondec2awknum(tokstart, strlen(tokstart)));
		else
			yylval.nodeval = make_number(atof(tokstart));
		yylval.nodeval->flags |= PERM;
		return lasttok = YNUMBER;

	case '&':
		if ((c = nextc()) == '&') {
			yylval.nodetypeval = Node_and;
			allow_newline();
			want_assign = FALSE;
			return lasttok = LEX_AND;
		}
		pushback();
		return lasttok = '&';

	case '|':
		if ((c = nextc()) == '|') {
			yylval.nodetypeval = Node_or;
			allow_newline();
			want_assign = FALSE;
			return lasttok = LEX_OR;
		} else if (! do_traditional && c == '&') {
			yylval.nodetypeval = Node_redirect_twoway;
			want_assign = FALSE;
			return lasttok = TWOWAYIO;
		}
		pushback();
		return lasttok = '|';
	}

	if (c != '_' && ! ISALPHA(c)) {
		yyerror(_("invalid char '%c' in expression"), c);
		exit(1);
	}

	if (! do_traditional && c == '_') {
		if ((c = nextc()) == '"') {
			intlstr = TRUE;
			goto string;
		}
		pushback();
		c = '_';
	}

	/* it's some type of name-type-thing.  Find its length. */
	tok = tokstart;
	while (is_identchar(c)) {
		tokadd(c);
		c = nextc();
	}
	tokadd('\0');
	emalloc(tokkey, char *, tok - tokstart, "yylex");
	memcpy(tokkey, tokstart, tok - tokstart);
	if (c != EOF)
		pushback();
	else if (do_lint && ! eof_warned) {
		lintwarn(_("source file does not end in newline"));
		eof_warned = TRUE;
	}

	/* See if it is a special token. */
	low = 0;
	high = (sizeof(tokentab) / sizeof(tokentab[0])) - 1;
	while (low <= high) {
		int i;

		mid = (low + high) / 2;
		c = *tokstart - tokentab[mid].operator[0];
		i = c ? c : strcmp(tokstart, tokentab[mid].operator);

		if (i < 0)		/* token < mid */
			high = mid - 1;
		else if (i > 0)		/* token > mid */
			low = mid + 1;
		else {
			if (do_lint) {
				if (tokentab[mid].flags & GAWKX)
					lintwarn(_("`%s' is a gawk extension"),
						tokentab[mid].operator);
				if (tokentab[mid].flags & RESX)
					lintwarn(_("`%s' is a Bell Labs extension"),
						tokentab[mid].operator);
				if (tokentab[mid].flags & NOT_POSIX)
					lintwarn(_("POSIX does not allow `%s'"),
						tokentab[mid].operator);
			}
			if (do_lint_old && (tokentab[mid].flags & NOT_OLD))
				warning(_("`%s' is not supported in old awk"),
						tokentab[mid].operator);
			if ((do_traditional && (tokentab[mid].flags & GAWKX))
			    || (do_posix && (tokentab[mid].flags & NOT_POSIX)))
				break;
			if (tokentab[mid].class == LEX_BUILTIN
			    || tokentab[mid].class == LEX_LENGTH
			   )
				yylval.lval = mid;
			else
				yylval.nodetypeval = tokentab[mid].value;

			free(tokkey);
			return lasttok = tokentab[mid].class;
		}
	}

	yylval.sval = tokkey;
	if (*lexptr == '(')
		return lasttok = FUNC_CALL;
	else {
		static short goto_warned = FALSE;

		want_assign = TRUE;
#define SMART_ALECK	1
		if (SMART_ALECK && do_lint
		    && ! goto_warned && strcasecmp(tokkey, "goto") == 0) {
			goto_warned = TRUE;
			lintwarn(_("`goto' considered harmful!\n"));
		}
		return lasttok = NAME;
	}
}

/* node_common --- common code for allocating a new node */

static NODE *
node_common(NODETYPE op)
{
	register NODE *r;

	getnode(r);
	r->type = op;
	r->flags = MALLOC;
	if (r->type == Node_var)
		r->flags |= UNINITIALIZED;
	/* if lookahead is NL, lineno is 1 too high */
	if (lexeme && *lexeme == '\n')
		r->source_line = sourceline - 1;
	else
		r->source_line = sourceline;
	r->source_file = source;
	return r;
}

/* node --- allocates a node with defined lnode and rnode. */

NODE *
node(NODE *left, NODETYPE op, NODE *right)
{
	register NODE *r;

	r = node_common(op);
	r->lnode = left;
	r->rnode = right;
	return r;
}

/* snode ---	allocate a node with defined subnode and proc for builtin
		functions. Checks for arg. count and supplies defaults where
		possible. */

static NODE *
snode(NODE *subn, NODETYPE op, int idx)
{
	register NODE *r;
	register NODE *n;
	int nexp = 0;
	int args_allowed;

	r = node_common(op);

	/* traverse expression list to see how many args. given */
	for (n = subn; n != NULL; n = n->rnode) {
		nexp++;
		if (nexp > 3)
			break;
	}

	/* check against how many args. are allowed for this builtin */
	args_allowed = tokentab[idx].flags & ARGS;
	if (args_allowed && (args_allowed & A(nexp)) == 0)
		fatal(_("%d is invalid as number of arguments for %s"),
				nexp, tokentab[idx].operator);

	r->proc = tokentab[idx].ptr;

	/* special case processing for a few builtins */
	if (nexp == 0 && r->proc == do_length) {
		subn = node(node(make_number(0.0), Node_field_spec, (NODE *) NULL),
		            Node_expression_list,
			    (NODE *) NULL);
	} else if (r->proc == do_match) {
		static short warned = FALSE;

		if (subn->rnode->lnode->type != Node_regex)
			subn->rnode->lnode = mk_rexp(subn->rnode->lnode);

		if (subn->rnode->rnode != NULL) {	/* 3rd argument there */
			if (do_lint && ! warned) {
				warned = TRUE;
				lintwarn(_("match: third argument is a gawk extension"));
			}
			if (do_traditional)
				fatal(_("match: third argument is a gawk extension"));
		}
	} else if (r->proc == do_sub || r->proc == do_gsub) {
		if (subn->lnode->type != Node_regex)
			subn->lnode = mk_rexp(subn->lnode);
		if (nexp == 2)
			append_right(subn, node(node(make_number(0.0),
						     Node_field_spec,
						     (NODE *) NULL),
					        Node_expression_list,
						(NODE *) NULL));
		else if (subn->rnode->rnode->lnode->type == Node_val) {
			if (do_lint) {
				char *f;

				f = (r->proc == do_sub) ? "sub" : "gsub";
				lintwarn(_("%s: string literal as last arg of substitute has no effect"), f);
			}
		} else if (! isassignable(subn->rnode->rnode->lnode)) {
			if (r->proc == do_sub)
				yyerror(_("sub third parameter is not a changeable object"));
			else
				yyerror(_("gsub third parameter is not a changeable object"));
		}
	} else if (r->proc == do_gensub) {
		if (subn->lnode->type != Node_regex)
			subn->lnode = mk_rexp(subn->lnode);
		if (nexp == 3)
			append_right(subn, node(node(make_number(0.0),
						     Node_field_spec,
						     (NODE *) NULL),
					        Node_expression_list,
						(NODE *) NULL));
	} else if (r->proc == do_split) {
		if (nexp == 2)
			append_right(subn,
			    node(FS_node, Node_expression_list, (NODE *) NULL));
		n = subn->rnode->rnode->lnode;
		if (n->type != Node_regex)
			subn->rnode->rnode->lnode = mk_rexp(n);
		if (nexp == 2)
			subn->rnode->rnode->lnode->re_flags |= FS_DFLT;
	} else if (r->proc == do_close) {
		static short warned = FALSE;

		if ( nexp == 2) {
			if (do_lint && nexp == 2 && ! warned) {
				warned = TRUE;
				lintwarn(_("close: second argument is a gawk extension"));
			}
			if (do_traditional)
				fatal(_("close: second argument is a gawk extension"));
		}
	} else if (do_intl					/* --gen-po */
			&& r->proc == do_dcgettext		/* dcgettext(...) */
			&& subn->lnode->type == Node_val	/* 1st arg is constant */
			&& (subn->lnode->flags & STR) != 0) {	/* it's a string constant */
		/* ala xgettext, dcgettext("some string" ...) dumps the string */
		NODE *str = subn->lnode;

		if ((str->flags & INTLSTR) != 0)
			warning(_("use of dcgettext(_\"...\") is incorrect: remove leading underscore"));
			/* don't dump it, the lexer already did */
		else
			dumpintlstr(str->stptr, str->stlen);
	}


	r->subnode = subn;
	if (r->proc == do_sprintf) {
		count_args(r);
		r->lnode->printf_count = r->printf_count; /* hack */
	}
	return r;
}

/*
 * mkrangenode:
 * This allocates a Node_line_range node with defined condpair and
 * zeroes the trigger word to avoid the temptation of assuming that calling
 * 'node( foo, Node_line_range, 0)' will properly initialize 'triggered'. 
 * Otherwise like node().
 */

static NODE *
mkrangenode(NODE *cpair)
{
	register NODE *r;

	getnode(r);
	r->type = Node_line_range;
	r->condpair = cpair;
	r->triggered = FALSE;
	return r;
}

/* make_for_loop --- build a for loop */

static NODE *
make_for_loop(NODE *init, NODE *cond, NODE *incr)
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

/* dup_parms --- return TRUE if there are duplicate parameters */

static int
dup_parms(NODE *func)
{
	register NODE *np;
	char *fname, **names;
	int count, i, j, dups;
	NODE *params;

	if (func == NULL)	/* error earlier */
		return TRUE;

	fname = func->param;
	count = func->param_cnt;
	params = func->rnode;

	if (count == 0)		/* no args, no problem */
		return FALSE;

	if (params == NULL)	/* error earlier */
		return TRUE;

	emalloc(names, char **, count * sizeof(char *), "dup_parms");

	i = 0;
	for (np = params; np != NULL; np = np->rnode) {
		if (np->param == NULL) { /* error earlier, give up, go home */
			free(names);
			return TRUE;
		}
		names[i++] = np->param;
	}

	dups = 0;
	for (i = 1; i < count; i++) {
		for (j = 0; j < i; j++) {
			if (strcmp(names[i], names[j]) == 0) {
				dups++;
				error(
	_("function `%s': parameter #%d, `%s', duplicates parameter #%d"),
					fname, i+1, names[j], j+1);
			}
		}
	}

	free(names);
	return (dups > 0 ? TRUE : FALSE);
}

/* parms_shadow --- check if parameters shadow globals */

static void
parms_shadow(const char *fname, NODE *func)
{
	int count, i;

	if (fname == NULL || func == NULL)	/* error earlier */
		return;

	count = func->lnode->param_cnt;

	if (count == 0)		/* no args, no problem */
		return;

	/*
	 * Use warning() and not lintwarn() so that can warn
	 * about all shadowed parameters.
	 */
	for (i = 0; i < count; i++) {
		if (lookup(func->parmlist[i]) != NULL) {
			warning(
	_("function `%s': parameter `%s' shadows global variable"),
					fname, func->parmlist[i]);
		}
	}
}

/*
 * install:
 * Install a name in the symbol table, even if it is already there.
 * Caller must check against redefinition if that is desired. 
 */

NODE *
install(char *name, NODE *value)
{
	register NODE *hp;
	register size_t len;
	register int bucket;

	var_count++;
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

/* lookup --- find the most recent hash node for name installed by install */

NODE *
lookup(const char *name)
{
	register NODE *bucket;
	register size_t len;

	len = strlen(name);
	for (bucket = variables[hash(name, len, (unsigned long) HASHSIZE)];
			bucket != NULL; bucket = bucket->hnext)
		if (bucket->hlength == len && STREQN(bucket->hname, name, len))
			return bucket->hvalue;

	return NULL;
}

/* var_comp --- compare two variable names */

static int
var_comp(const void *v1, const void *v2)
{
	NODE **npp1, **npp2;
	NODE *n1, *n2;
	int minlen;

	npp1 = (NODE **) v1;
	npp2 = (NODE **) v2;
	n1 = *npp1;
	n2 = *npp2;

	if (n1->hlength > n2->hlength)
		minlen = n1->hlength;
	else
		minlen = n2->hlength;

	return strncmp(n1->hname, n2->hname, minlen);
}

/* valinfo --- dump var info */

static void
valinfo(NODE *n, FILE *fp)
{
	if (n->flags & STRING) {
		fprintf(fp, "string (");
		pp_string_fp(fp, n->stptr, n->stlen, '"', FALSE);
		fprintf(fp, ")\n");
	} else if (n->flags & NUMBER)
		fprintf(fp, "number (%.17g)\n", n->numbr);
	else if (n->flags & STR) {
		fprintf(fp, "string value (");
		pp_string_fp(fp, n->stptr, n->stlen, '"', FALSE);
		fprintf(fp, ")\n");
	} else if (n->flags & NUM)
		fprintf(fp, "number value (%.17g)\n", n->numbr);
	else
		fprintf(fp, "?? flags %s\n", flags2str(n->flags));
}


/* dump_vars --- dump the symbol table */

void
dump_vars(const char *fname)
{
	int i, j;
	NODE **table;
	NODE *p;
	FILE *fp;

	emalloc(table, NODE **, var_count * sizeof(NODE *), "dump_vars");

	if (fname == NULL)
		fp = stderr;
	else if ((fp = fopen(fname, "w")) == NULL) {
		warning(_("could not open `%s' for writing (%s)"), fname, strerror(errno));
		warning(_("sending profile to standard error"));
		fp = stderr;
	}

	for (i = j = 0; i < HASHSIZE; i++)
		for (p = variables[i]; p != NULL; p = p->hnext)
			table[j++] = p;

	assert(j == var_count);

	/* Shazzam! */
	qsort(table, j, sizeof(NODE *), var_comp);

	for (i = 0; i < j; i++) {
		p = table[i];
		if (p->hvalue->type == Node_func)
			continue;
		fprintf(fp, "%.*s: ", (int) p->hlength, p->hname);
		if (p->hvalue->type == Node_var_array)
			fprintf(fp, "array, %ld elements\n", p->hvalue->table_size);
		else if (p->hvalue->type == Node_var)
			valinfo(p->hvalue->var_value, fp);
		else {
			NODE **lhs = get_lhs(p->hvalue, NULL, FALSE);

			valinfo(*lhs, fp);
		}
	}

	if (fp != stderr && fclose(fp) != 0)
		warning(_("%s: close failed (%s)"), fname, strerror(errno));

	free(table);
}

/* release_all_vars --- free all variable memory */

void
release_all_vars()
{
	int i;
	NODE *p, *next;

	for (i = 0; i < HASHSIZE; i++)
		for (p = variables[i]; p != NULL; p = next) {
			next = p->hnext;

			if (p->hvalue->type == Node_func)
				continue;
			else if (p->hvalue->type == Node_var_array)
				assoc_clear(p->hvalue);
			else if (p->hvalue->type == Node_var)
				unref(p->hvalue->var_value);
			else {
				NODE **lhs = get_lhs(p->hvalue, NULL, FALSE);

				unref((*lhs)->var_value);
			}
			unref(p);
	}
}

/* finfo --- for use in comparison and sorting of function names */

struct finfo {
	char *name;
	size_t nlen;
	NODE *func;
};

/* fcompare --- comparison function for qsort */

static int
fcompare(const void *p1, const void *p2)
{
	struct finfo *f1, *f2;
	int minlen;

	f1 = (struct finfo *) p1;
	f2 = (struct finfo *) p2;

	if (f1->nlen > f2->nlen)
		minlen = f2->nlen;
	else
		minlen = f1->nlen;

	return strncmp(f1->name, f2->name, minlen);
}

/* dump_funcs --- print all functions */

void
dump_funcs()
{
	int i, j;
	NODE *p;
	static struct finfo *tab = NULL;

	if (func_count == 0)
		return;

	if (tab == NULL)
		emalloc(tab, struct finfo *, func_count * sizeof(struct finfo), "dump_funcs");

	for (i = j = 0; i < HASHSIZE; i++) {
		for (p = variables[i]; p != NULL; p = p->hnext) {
			if (p->hvalue->type == Node_func) {
				tab[j].name = p->hname;
				tab[j].nlen = p->hlength;
				tab[j].func = p->hvalue;
				j++;
			}
		}
	}

	assert(j == func_count);

	/* Shazzam! */
	qsort(tab, func_count, sizeof(struct finfo), fcompare);

	for (i = 0; i < j; i++)
		pp_func(tab[i].name, tab[i].nlen, tab[i].func);

	free(tab);
}

/* shadow_funcs --- check all functions for parameters that shadow globals */

void
shadow_funcs()
{
	int i, j;
	NODE *p;
	struct finfo *tab;
	static int calls = 0;

	if (func_count == 0)
		return;

	if (calls++ != 0)
		fatal(_("shadow_funcs() called twice!"));

	emalloc(tab, struct finfo *, func_count * sizeof(struct finfo), "shadow_funcs");

	for (i = j = 0; i < HASHSIZE; i++) {
		for (p = variables[i]; p != NULL; p = p->hnext) {
			if (p->hvalue->type == Node_func) {
				tab[j].name = p->hname;
				tab[j].nlen = p->hlength;
				tab[j].func = p->hvalue;
				j++;
			}
		}
	}

	assert(j == func_count);

	/* Shazzam! */
	qsort(tab, func_count, sizeof(struct finfo), fcompare);

	for (i = 0; i < j; i++)
		parms_shadow(tab[i].name, tab[i].func);

	free(tab);
}

/*
 * append_right:
 * Add new to the rightmost branch of LIST.  This uses n^2 time, so we make
 * a simple attempt at optimizing it.
 */

static NODE *
append_right(NODE *list, NODE *new)
{
	register NODE *oldlist;
	static NODE *savefront = NULL, *savetail = NULL;

	if (list == NULL || new == NULL)
		return list;

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
 * func_install:
 * check if name is already installed;  if so, it had better have Null value,
 * in which case def is added as the value. Otherwise, install name with def
 * as value. 
 *
 * Extra work, build up and save a list of the parameter names in a table
 * and hang it off params->parmlist. This is used to set the `vname' field
 * of each function parameter during a function call. See eval.c.
 */

static void
func_install(NODE *params, NODE *def)
{
	NODE *r, *n, *thisfunc;
	char **pnames, *names, *sp;
	size_t pcount = 0, space = 0;
	int i;

	/* check for function foo(foo) { ... }.  bleah. */
	for (n = params->rnode; n != NULL; n = n->rnode) {
		if (strcmp(n->param, params->param) == 0)
			fatal(_("function `%s': can't use function name as parameter name"),
					params->param); 
	}

	thisfunc = NULL;	/* turn off warnings */

	/* symbol table managment */
	pop_var(params, FALSE);
	r = lookup(params->param);
	if (r != NULL) {
		fatal(_("function name `%s' previously defined"), params->param);
	} else {
		thisfunc = node(params, Node_func, def);
		(void) install(params->param, thisfunc);
	}

	/* figure out amount of space to allocate */
	for (n = params->rnode; n != NULL; n = n->rnode) {
		pcount++;
		space += strlen(n->param) + 1;
	}

	/* allocate it and fill it in */
	if (pcount != 0) {
		emalloc(names, char *, space, "func_install");
		emalloc(pnames, char **, pcount * sizeof(char *), "func_install");
		sp = names;
		for (i = 0, n = params->rnode; i < pcount; i++, n = n->rnode) {
			pnames[i] = sp;
			strcpy(sp, n->param);
			sp += strlen(n->param) + 1;
		}
		thisfunc->parmlist = pnames;
	} else {
		thisfunc->parmlist = NULL;
	}

	/* remove params from symbol table */
	pop_params(params->rnode);

	/* update lint table info */
	func_use(params->param, FUNC_DEFINE);

	func_count++;	/* used by profiling / pretty printer */
}

/* pop_var --- remove a variable from the symbol table */

static void
pop_var(NODE *np, int freeit)
{
	register NODE *bucket, **save;
	register size_t len;
	char *name;

	name = np->param;
	len = strlen(name);
	save = &(variables[hash(name, len, (unsigned long) HASHSIZE)]);
	for (bucket = *save; bucket != NULL; bucket = bucket->hnext) {
		if (len == bucket->hlength && STREQN(bucket->hname, name, len)) {
			var_count--;
			*save = bucket->hnext;
			freenode(bucket);
			if (freeit)
				free(np->param);
			return;
		}
		save = &(bucket->hnext);
	}
}

/* pop_params --- remove list of function parameters from symbol table */

/*
 * pop parameters out of the symbol table. do this in reverse order to
 * avoid reading freed memory if there were duplicated parameters.
 */
static void
pop_params(NODE *params)
{
	if (params == NULL)
		return;
	pop_params(params->rnode);
	pop_var(params, TRUE);
}

/* make_param --- make NAME into a function parameter */

static NODE *
make_param(char *name)
{
	NODE *r;

	getnode(r);
	r->type = Node_param_list;
	r->rnode = NULL;
	r->param = name;
	r->param_cnt = param_counter++;
	return (install(name, r));
}

static struct fdesc {
	char *name;
	short used;
	short defined;
	struct fdesc *next;
} *ftable[HASHSIZE];

/* func_use --- track uses and definitions of functions */

static void
func_use(char *name, enum defref how)
{
	struct fdesc *fp;
	int len;
	int ind;

	len = strlen(name);
	ind = hash(name, len, HASHSIZE);

	for (fp = ftable[ind]; fp != NULL; fp = fp->next) {
		if (strcmp(fp->name, name) == 0) {
			if (how == FUNC_DEFINE)
				fp->defined++;
			else
				fp->used++;
			return;
		}
	}

	/* not in the table, fall through to allocate a new one */

	emalloc(fp, struct fdesc *, sizeof(struct fdesc), "func_use");
	memset(fp, '\0', sizeof(struct fdesc));
	emalloc(fp->name, char *, len + 1, "func_use");
	strcpy(fp->name, name);
	if (how == FUNC_DEFINE)
		fp->defined++;
	else
		fp->used++;
	fp->next = ftable[ind];
	ftable[ind] = fp;
}

/* check_funcs --- verify functions that are called but not defined */

static void
check_funcs()
{
	struct fdesc *fp, *next;
	int i;

	for (i = 0; i < HASHSIZE; i++) {
		for (fp = ftable[i]; fp != NULL; fp = fp->next) {
#ifdef REALLYMEAN
			/* making this the default breaks old code. sigh. */
			if (fp->defined == 0) {
				error(
		_("function `%s' called but never defined"), fp->name);
				errcount++;
			}
#else
			if (do_lint && fp->defined == 0)
				lintwarn(
		_("function `%s' called but never defined"), fp->name);
#endif
			if (do_lint && fp->used == 0) {
				lintwarn(_("function `%s' defined but never called"),
					fp->name);
			}
		}
	}

	/* now let's free all the memory */
	for (i = 0; i < HASHSIZE; i++) {
		for (fp = ftable[i]; fp != NULL; fp = next) {
			next = fp->next;
			free(fp->name);
			free(fp);
		}
	}
}

/* param_sanity --- look for parameters that are regexp constants */

static void
param_sanity(NODE *arglist)
{
	NODE *argp, *arg;
	int i;

	for (i = 1, argp = arglist; argp != NULL; argp = argp->rnode, i++) {
		arg = argp->lnode;
		if (arg->type == Node_regex)
			warning(_("regexp constant for parameter #%d yields boolean value"), i);
	}
}

/* variable --- make sure NAME is in the symbol table */

NODE *
variable(char *name, int can_free, NODETYPE type)
{
	register NODE *r;
	static int env_loaded = FALSE;
	static int procinfo_loaded = FALSE;

	if (! env_loaded && STREQ(name, "ENVIRON")) {
		load_environ();
		env_loaded = TRUE;
	}
	if (! do_traditional && ! procinfo_loaded && STREQ(name, "PROCINFO")) {
		load_procinfo();
		procinfo_loaded = TRUE;
	}
	if ((r = lookup(name)) == NULL)
		r = install(name, node(Nnull_string, type, (NODE *) NULL));
	else if (can_free)
		free(name);
	return r;
}

/* mk_rexp --- make a regular expression constant */

static NODE *
mk_rexp(NODE *exp)
{
	NODE *n;

	if (exp->type == Node_regex)
		return exp;

	getnode(n);
	n->type = Node_regex;
	n->re_exp = exp;
	n->re_text = NULL;
	n->re_reg = NULL;
	n->re_flags = 0;
	n->re_cnt = 1;
	return n;
}

/* isnoeffect --- when used as a statement, has no side effects */

/*
 * To be completely general, we should recursively walk the parse
 * tree, to make sure that all the subexpressions also have no effect.
 * Instead, we just weaken the actual warning that's printed, up above
 * in the grammar.
 */

static int
isnoeffect(NODETYPE type)
{
	switch (type) {
	case Node_times:
	case Node_quotient:
	case Node_mod:
	case Node_plus:
	case Node_minus:
	case Node_subscript:
	case Node_concat:
	case Node_exp:
	case Node_unary_minus:
	case Node_field_spec:
	case Node_and:
	case Node_or:
	case Node_equal:
	case Node_notequal:
	case Node_less:
	case Node_greater:
	case Node_leq:
	case Node_geq:
	case Node_match:
	case Node_nomatch:
	case Node_not:
	case Node_val:
	case Node_in_array:
	case Node_NF:
	case Node_NR:
	case Node_FNR:
	case Node_FS:
	case Node_RS:
	case Node_FIELDWIDTHS:
	case Node_IGNORECASE:
	case Node_OFS:
	case Node_ORS:
	case Node_OFMT:
	case Node_CONVFMT:
	case Node_BINMODE:
	case Node_LINT:
		return TRUE;
	default:
		break;	/* keeps gcc -Wall happy */
	}

	return FALSE;
}

/* isassignable --- can this node be assigned to? */

static int
isassignable(register NODE *n)
{
	switch (n->type) {
	case Node_var:
	case Node_FIELDWIDTHS:
	case Node_RS:
	case Node_FS:
	case Node_FNR:
	case Node_NR:
	case Node_NF:
	case Node_IGNORECASE:
	case Node_OFMT:
	case Node_CONVFMT:
	case Node_ORS:
	case Node_OFS:
	case Node_LINT:
	case Node_BINMODE:
	case Node_field_spec:
	case Node_subscript:
		return TRUE;
	case Node_param_list:
		return ((n->flags & FUNC) == 0);  /* ok if not func name */
	default:
		break;	/* keeps gcc -Wall happy */
	}
	return FALSE;
}

/* stopme --- for debugging */

NODE *
stopme(NODE *tree)
{
	return tmp_number((AWKNUM) 0.0);
}

/* dumpintlstr --- write out an initial .po file entry for the string */

static void
dumpintlstr(char *str, size_t len)
{
	char *cp;

	/* See the GNU gettext distribution for details on the file format */

	if (source != NULL) {
		/* ala the gettext sources, remove leading `./'s */
		for (cp = source; cp[0] == '.' && cp[1] == '/'; cp += 2)
			continue;
		printf("#: %s:%d\n", cp, sourceline);
	}

	printf("msgid ");
	fflush(stdout);
	pp_string_fp(stdout, str, len, '"', TRUE);
	putchar('\n');
	printf("msgstr \"\"\n\n");
}

/* count_args --- count the number of printf arguments */

static void
count_args(NODE *tree)
{
	size_t count = 0;
	NODE *save_tree;

	assert(tree->type == Node_K_printf
		|| (tree->type == Node_builtin && tree->proc == do_sprintf));
	save_tree = tree;

	tree = tree->lnode;	/* printf format string */

	for (count = 0; tree != NULL; tree = tree->rnode)
		count++;

	save_tree->printf_count = count;
}
