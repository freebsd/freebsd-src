/*
 * profile.c - gawk parse tree pretty-printer with counts
 */

/* 
 * Copyright (C) 1999-2001 the Free Software Foundation, Inc.
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

#include "awk.h"

/* where to place redirections for getline, print, printf */
enum redir_placement {
	BEFORE = 0,
	AFTER = 1
};

#undef tree_eval
static void tree_eval P((NODE *tree));
static void parenthesize P((NODETYPE parent_type, NODE *tree));
static void eval_condition P((NODE *tree));
static void pp_op_assign P((NODE *tree));
static void pp_func_call P((NODE *name, NODE *arg_list));
static void pp_match_op P((NODE *tree));
static void pp_lhs P((NODE *ptr));
static void pp_print_stmt P((const char *command, NODE *tree));
static void pp_delete P((NODE *tree));
static void pp_in_array P((NODE *array, NODE *subscript));
static void pp_getline P((NODE *tree));
static void pp_builtin P((NODE *tree));
static void pp_list P((NODE *tree));
static void pp_string P((char *str, size_t len, int delim));
static int is_scalar P((NODETYPE type));
static int prec_level P((NODETYPE type));
#ifdef PROFILING
static RETSIGTYPE dump_and_exit P((int signum));
static RETSIGTYPE just_dump P((int signum));
#endif

/* pretty printing related functions and variables */

static char **fparms;	/* function parameter names */
static FILE *prof_fp;	/* where to send the profile */

static long indent_level = 0;

static int in_BEGIN_or_END = FALSE;

static int in_expr = FALSE;

#define SPACEOVER	0

/* init_profiling --- do needed initializations, see also main.c */

void
init_profiling(int *flag, const char *def_file)
{
	/* run time init avoids glibc innovations */
	prof_fp = stderr;

#ifdef PROFILING
	if (*flag == FALSE) {
		*flag = TRUE;
		set_prof_file(def_file);
	}
#endif
}

/* set_prof_file --- set the output file for profiling */

void
set_prof_file(const char *file)
{
	assert(file != NULL);

	prof_fp = fopen(file, "w");
	if (prof_fp == NULL) {
		warning(_("could not open `%s' for writing: %s"),
				file, strerror(errno));
		warning(_("sending profile to standard error"));
		prof_fp = stderr;
	}
}

void
init_profiling_signals()
{
#ifdef PROFILING
#ifdef SIGHUP
	signal(SIGHUP, dump_and_exit);
#endif
#ifdef SIGUSR1
	signal(SIGUSR1, just_dump);
#endif
#endif
}

/* indent --- print out enough tabs */

static void
indent(long count)
{
	int i;

	if (count == 0)
		putc('\t', prof_fp);
	else
		fprintf(prof_fp, "%6ld  ", count);

	assert(indent_level >= 0);
	for (i = 0; i < indent_level; i++)
		putc('\t', prof_fp);
}

/* indent_in --- increase the level, with error checking */

static void
indent_in()
{
	assert(indent_level >= 0);
	indent_level++;
}

/* indent_out --- decrease the level, with error checking */

static void
indent_out()
{
	indent_level--;
	assert(indent_level >= 0);
}

/*
 * pprint:
 * Tree is a bunch of rules to run. Returns zero if it hit an exit()
 * statement 
 */
static void
pprint(register NODE *volatile tree)
{
	register NODE *volatile t = NULL;	/* temporary */
	int volatile traverse = TRUE;	/* True => loop thru tree (Node_rule_list) */

	/* avoid false source indications */
	source = NULL;
	sourceline = 0;

	if (tree == NULL)
		return;
	sourceline = tree->source_line;
	source = tree->source_file;
	switch (tree->type) {
	case Node_rule_node:
		traverse = FALSE;  /* False => one for-loop iteration only */
		/* FALL THROUGH */
	case Node_rule_list:
		for (t = tree; t != NULL; t = t->rnode) {
			if (traverse)
				tree = t->lnode;
			sourceline = tree->source_line;
			source = tree->source_file;

			if (! in_BEGIN_or_END)
				indent(tree->exec_count);

			if (tree->lnode) {
				eval_condition(tree->lnode);
				if (tree->rnode)
					fprintf(prof_fp, "\t");
			}

			if (tree->rnode) {
				if (! in_BEGIN_or_END) {
					fprintf(prof_fp, "{");
					if (tree->lnode != NULL
					    && tree->lnode->exec_count)
						fprintf(prof_fp, " # %ld",
							tree->lnode->exec_count);
					fprintf(prof_fp, "\n");
				}
				indent_in();
				pprint(tree->rnode);
				indent_out();
				if (! in_BEGIN_or_END) {
					indent(SPACEOVER);
					fprintf(prof_fp, "}\n");
				}
			}

			if (! traverse) 	/* case Node_rule_node */
				break;		/* don't loop */

			if (t->rnode && ! in_BEGIN_or_END)
				fprintf(prof_fp, "\n");
		}
		break;

	case Node_statement_list:
		for (t = tree; t != NULL; t = t->rnode) {
			pprint(t->lnode);
		}
		break;

	case Node_K_if:
		indent(tree->exec_count);
		fprintf(prof_fp, "if (");
		in_expr++;
		eval_condition(tree->lnode);
		in_expr--;
		fprintf(prof_fp, ") {");
#ifdef PROFILING
		if (tree->rnode->exec_count)
			fprintf(prof_fp, " # %ld", tree->rnode->exec_count);
#endif
		fprintf(prof_fp, "\n");
		indent_in();
		pprint(tree->rnode->lnode);
		indent_out();
		if (tree->rnode->rnode != NULL) {
			if (tree->exec_count - tree->rnode->exec_count > 0)
				indent(tree->exec_count - tree->rnode->exec_count);
			else
				indent(0);
			fprintf(prof_fp, "} else {\n");
			indent_in();
			pprint(tree->rnode->rnode);
			indent_out();
		}
		indent(SPACEOVER);
		fprintf(prof_fp, "}\n");
		break;

	case Node_K_while:
		indent(tree->exec_count);
		fprintf(prof_fp, "while (");
		in_expr++;
		eval_condition(tree->lnode);
		in_expr--;
		fprintf(prof_fp, ") {\n");
		indent_in();
		pprint(tree->rnode);
		indent_out();
		indent(SPACEOVER);
		fprintf(prof_fp, "}\n");
		break;

	case Node_K_do:
		indent(tree->exec_count);
		fprintf(prof_fp, "do {\n");
		indent_in();
		pprint(tree->rnode);
		indent_out();
		indent(SPACEOVER);
		fprintf(prof_fp, "} while (");
		in_expr++;
		eval_condition(tree->lnode);
		in_expr--;
		fprintf(prof_fp, ")\n");
		break;

	case Node_K_for:
		indent(tree->exec_count);
		fprintf(prof_fp, "for (");
		in_expr++;
		pprint(tree->forloop->init);
		fprintf(prof_fp, "; ");
		eval_condition(tree->forloop->cond);
		fprintf(prof_fp, "; ");
		pprint(tree->forloop->incr);
		fprintf(prof_fp, ") {\n");
		in_expr--;
		indent_in();
		pprint(tree->lnode);
		indent_out();
		indent(SPACEOVER);
		fprintf(prof_fp, "}\n");
		break;

	case Node_K_arrayfor:
#define hakvar forloop->init
#define arrvar forloop->incr
		indent(tree->exec_count);
		fprintf(prof_fp, "for (");
		in_expr++;
		pp_lhs(tree->hakvar);
		in_expr--;
		fprintf(prof_fp, " in ");
		t = tree->arrvar;
		if (t->type == Node_param_list)
			fprintf(prof_fp, "%s", fparms[t->param_cnt]);
		else
			fprintf(prof_fp, "%s", t->vname);
		fprintf(prof_fp, ") {\n");
		indent_in();
		pprint(tree->lnode);
		indent_out();
		indent(SPACEOVER);
		fprintf(prof_fp, "}\n");
		break;

	case Node_K_break:
		indent(tree->exec_count);
		fprintf(prof_fp, "break\n");
		break;

	case Node_K_continue:
		indent(tree->exec_count);
		fprintf(prof_fp, "continue\n");
		break;

	case Node_K_print:
		pp_print_stmt("print", tree);
		break;

	case Node_K_printf:
		pp_print_stmt("printf", tree);
		break;

	case Node_K_delete:
		pp_delete(tree);
		break;

	case Node_K_next:
		indent(tree->exec_count);
		fprintf(prof_fp, "next\n");
		break;

	case Node_K_nextfile:
		indent(tree->exec_count);
		fprintf(prof_fp, "nextfile\n");
		break;

	case Node_K_exit:
		indent(tree->exec_count);
		fprintf(prof_fp, "exit");
		if (tree->lnode != NULL) {
			fprintf(prof_fp, " ");
			tree_eval(tree->lnode);
		}
		fprintf(prof_fp, "\n");
		break;

	case Node_K_return:
		indent(tree->exec_count);
		fprintf(prof_fp, "return");
		if (tree->lnode != NULL) {
			fprintf(prof_fp, " ");
			tree_eval(tree->lnode);
		}
		fprintf(prof_fp, "\n");
		break;

	default:
		/*
		 * Appears to be an expression statement.
		 * Throw away the value. 
		 */
		if (in_expr)
			tree_eval(tree);
		else {
			indent(tree->exec_count);
			tree_eval(tree);
			fprintf(prof_fp, "\n");
		}
		break;
	}
}

/* tree_eval --- evaluate a subtree */

static void
tree_eval(register NODE *tree)
{
	if (tree == NULL)
		return;

	switch (tree->type) {
	case Node_param_list:
		fprintf(prof_fp, "%s", fparms[tree->param_cnt]);
		return;

	case Node_var:
		if (tree->vname != NULL)
			fprintf(prof_fp, "%s", tree->vname);
		else
			fatal(_("internal error: Node_var with null vname"));
		return;

	case Node_val:
		if ((tree->flags & (NUM|NUMBER)) != 0)
			fprintf(prof_fp, "%g", tree->numbr);
		else {
			if ((tree->flags & INTLSTR) != 0)
				fprintf(prof_fp, "_");
			pp_string(tree->stptr, tree->stlen, '"');
		}
		return;

	case Node_and:
		eval_condition(tree->lnode);
		fprintf(prof_fp, " && ");
	   	eval_condition(tree->rnode);
		return;

	case Node_or:
		eval_condition(tree->lnode);
		fprintf(prof_fp, " || ");
	   	eval_condition(tree->rnode);
		return;

	case Node_not:
		parenthesize(tree->type, tree->lnode);
		return;

		/* Builtins */
	case Node_builtin:
		pp_builtin(tree);
		return;

	case Node_in_array:
		in_expr++;
		pp_in_array(tree->lnode, tree->rnode);
		in_expr--;
		return;

	case Node_func_call:
		pp_func_call(tree->rnode, tree->lnode);
		return;

	case Node_K_getline:
		pp_getline(tree);
		return;

		/* unary operations */
	case Node_NR:
		fprintf(prof_fp, "NR");
		return;

	case Node_FNR:
		fprintf(prof_fp, "FNR");
		return;

	case Node_NF:
		fprintf(prof_fp, "NF");
		return;

	case Node_FIELDWIDTHS:
		fprintf(prof_fp, "FIELDWIDTHS");
		return;

	case Node_FS:
		fprintf(prof_fp, "FS");
		return;

	case Node_RS:
		fprintf(prof_fp, "RS");
		return;

	case Node_IGNORECASE:
		fprintf(prof_fp, "IGNORECASE");
		return;

	case Node_OFS:
		fprintf(prof_fp, "OFS");
		return;

	case Node_ORS:
		fprintf(prof_fp, "ORS");
		return;

	case Node_OFMT:
		fprintf(prof_fp, "OFMT");
		return;

	case Node_CONVFMT:
		fprintf(prof_fp, "CONVFMT");
		return;

	case Node_BINMODE:
		fprintf(prof_fp, "BINMODE");
		return;

	case Node_field_spec:
	case Node_subscript:
		pp_lhs(tree);
		return;

	case Node_var_array:
		if (tree->vname != NULL)
			fprintf(prof_fp, "%s", tree->vname);
		else
			fatal(_("internal error: Node_var_array with null vname"));
		return;

	case Node_unary_minus:
		fprintf(prof_fp, " -");
		tree_eval(tree->subnode);
		return;

	case Node_cond_exp:
		eval_condition(tree->lnode);
		fprintf(prof_fp, " ? ");
		tree_eval(tree->rnode->lnode);
		fprintf(prof_fp, " : ");
		tree_eval(tree->rnode->rnode);
		return;

	case Node_match:
	case Node_nomatch:
	case Node_regex:
		pp_match_op(tree);
		return;

	case Node_func:
		fatal(_("function `%s' called with space between name and `(',\n%s"),
			tree->lnode->param,
			_("or used in other expression context"));

		/* assignments */
	case Node_assign:
		tree_eval(tree->lnode);
		fprintf(prof_fp, " = ");
		tree_eval(tree->rnode);
		return;

	case Node_concat:
		fprintf(prof_fp, "(");
		tree_eval(tree->lnode);
		fprintf(prof_fp, " ");
		tree_eval(tree->rnode);
		fprintf(prof_fp, ")");
		return;

	/* other assignment types are easier because they are numeric */
	case Node_preincrement:
	case Node_predecrement:
	case Node_postincrement:
	case Node_postdecrement:
	case Node_assign_exp:
	case Node_assign_times:
	case Node_assign_quotient:
	case Node_assign_mod:
	case Node_assign_plus:
	case Node_assign_minus:
		pp_op_assign(tree);
		return;

	default:
		break;	/* handled below */
	}

	/* handle binary ops */
	in_expr++;
	parenthesize(tree->type, tree->lnode);

	switch (tree->type) {
	case Node_geq:
		fprintf(prof_fp, " >= ");
		break;
	case Node_leq:
		fprintf(prof_fp, " <= ");
		break;
	case Node_greater:
		fprintf(prof_fp, " > ");
		break;
	case Node_less:
		fprintf(prof_fp, " < ");
		break;
	case Node_notequal:
		fprintf(prof_fp, " != ");
		break;
	case Node_equal:
		fprintf(prof_fp, " == ");
		break;
	case Node_exp:
		fprintf(prof_fp, " ^ ");
		break;
	case Node_times:
		fprintf(prof_fp, " * ");
		break;
	case Node_quotient:
		fprintf(prof_fp, " / ");
		break;
	case Node_mod:
		fprintf(prof_fp, " %% ");
		break;
	case Node_plus:
		fprintf(prof_fp, " + ");
		break;
	case Node_minus:
		fprintf(prof_fp, " - ");
		break;
	case Node_var_array:
		fatal(_("attempt to use array `%s' in a scalar context"),
			tree->vname);
		return;
	default:
		fatal(_("illegal type (%s) in tree_eval"), nodetype2str(tree->type));
	}
	parenthesize(tree->type, tree->rnode);
	in_expr--;

	return;
}

/* eval_condition --- is TREE true or false */

static void
eval_condition(register NODE *tree)
{
	if (tree == NULL)	/* Null trees are the easiest kinds */
		return;

	if (tree->type == Node_line_range) {
		/* /.../, /.../ */
		eval_condition(tree->condpair->lnode);
		fprintf(prof_fp,", ");
		eval_condition(tree->condpair->rnode);
		return;
	}

	/*
	 * Could just be J.random expression. in which case, null and 0 are
	 * false, anything else is true 
	 */

	tree_eval(tree);
	return;
}

/* pp_op_assign --- do +=, -=, etc. */

static void
pp_op_assign(register NODE *tree)
{
	char *op = NULL;
	enum Order {
		NA = 0,
		PRE = 1,
		POST = 2
	} order = NA;

	switch(tree->type) {
	case Node_preincrement:
		op = "++";
		order = PRE;
		break;

	case Node_predecrement:
		op = "--";
		order = PRE;
		break;

	case Node_postincrement:
		op = "++";
		order = POST;
		break;

	case Node_postdecrement:
		op = "--";
		order = POST;
		break;

	default:
		break;	/* handled below */
	}

	if (order == PRE) {
		fprintf(prof_fp, "%s", op);
		pp_lhs(tree->lnode);
		return;
	} else if (order == POST) {
		pp_lhs(tree->lnode);
		fprintf(prof_fp, "%s", op);
		return;
	}

	/* a binary op */
	pp_lhs(tree->lnode);

	switch(tree->type) {
	case Node_assign_exp:
		fprintf(prof_fp, " ^= ");
		break;

	case Node_assign_times:
		fprintf(prof_fp, " *= ");
		break;

	case Node_assign_quotient:
		fprintf(prof_fp, " /= ");
		break;

	case Node_assign_mod:
		fprintf(prof_fp, " %%= ");
		break;

	case Node_assign_plus:
		fprintf(prof_fp, " += ");
		break;

	case Node_assign_minus:
		fprintf(prof_fp, " -= ");
		break;

	default:
		cant_happen();
	}

	tree_eval(tree->rnode);
}

/* pp_lhs --- print the lhs */

static void
pp_lhs(register NODE *ptr)
{
	register NODE *n;

	switch (ptr->type) {
	case Node_var_array:
		fatal(_("attempt to use array `%s' in a scalar context"),
			ptr->vname);

	case Node_var:
		fprintf(prof_fp, "%s", ptr->vname);
		break;

	case Node_FIELDWIDTHS:
		fprintf(prof_fp, "FIELDWIDTHS");
		break;

	case Node_RS:
		fprintf(prof_fp, "RS");
		break;

	case Node_FS:
		fprintf(prof_fp, "FS");
		break;

	case Node_FNR:
		fprintf(prof_fp, "FNR");
		break;

	case Node_NR:
		fprintf(prof_fp, "NR");
		break;

	case Node_NF:
		fprintf(prof_fp, "NF");
		break;

	case Node_IGNORECASE:
		fprintf(prof_fp, "IGNORECASE");
		break;

	case Node_BINMODE:
		fprintf(prof_fp, "BINMODE");
		break;

	case Node_LINT:
		fprintf(prof_fp, "LINT");
		break;

	case Node_OFMT:
		fprintf(prof_fp, "OFMT");
		break;

	case Node_CONVFMT:
		fprintf(prof_fp, "CONVFMT");
		break;

	case Node_ORS:
		fprintf(prof_fp, "ORS");
		break;

	case Node_OFS:
		fprintf(prof_fp, "OFS");
		break;

	case Node_param_list:
		fprintf(prof_fp, "%s", fparms[ptr->param_cnt]);
		break;

	case Node_field_spec:
		fprintf(prof_fp, "$");
		if (is_scalar(ptr->lnode->type))
			tree_eval(ptr->lnode);
		else {
			fprintf(prof_fp, "(");
			tree_eval(ptr->lnode);
			fprintf(prof_fp, ")");
		}
		break;

	case Node_subscript:
		n = ptr->lnode;
		if (n->type == Node_func) {
			fatal(_("attempt to use function `%s' as array"),
				n->lnode->param);
		} else if (n->type == Node_param_list) {
			fprintf(prof_fp, "%s[", fparms[n->param_cnt]);
		} else
			fprintf(prof_fp, "%s[", n->vname);
		if (ptr->rnode->type == Node_expression_list)
			pp_list(ptr->rnode);
		else
			tree_eval(ptr->rnode);
		fprintf(prof_fp, "]");
		break;

	case Node_func:
		fatal(_("`%s' is a function, assignment is not allowed"),
			ptr->lnode->param);

	case Node_builtin:
		fatal(_("assignment is not allowed to result of builtin function"));

	default:
		cant_happen();
	}
}

/* match_op --- do ~ and !~ */

static void
pp_match_op(register NODE *tree)
{
	register NODE *re;
	char *op;
	char *restr;
	size_t relen;
	NODE *text = NULL;

	if (tree->type == Node_regex)
		re = tree->re_exp;
	else {
		re = tree->rnode->re_exp;
		text = tree->lnode;
	}

	if ((re->re_flags & CONST) != 0) {
		restr = re->stptr;
		relen = re->stlen;
	} else {
		restr = re->stptr;
		relen = re->stlen;
	}

	if (tree->type == Node_regex) {
		pp_string(restr, relen, '/');
		return;
	}

	if (tree->type == Node_nomatch)
		op = "!~";
	else if (tree->type == Node_match)
		op = "~";
	else
		op = "";

	tree_eval(text);
	fprintf(prof_fp, " %s ", op);
	fprintf(prof_fp, "/%.*s/", (int) relen, restr);
}

/* pp_redir --- print a redirection */

static void
pp_redir(register NODE *tree, enum redir_placement dir)
{
	char *op = "[BOGUS]";	/* should never be seen */

	if (tree == NULL)
		return;

	switch (tree->type) {
	case Node_redirect_output:
		op = ">";
		break;
	case Node_redirect_append:
		op = ">>";
		break;
	case Node_redirect_pipe:
		op = "|";
		break;
	case Node_redirect_pipein:
		op = "|";
		break;
	case Node_redirect_input:
		op = "<";
		break;
	case Node_redirect_twoway:
		op = "|&";
		break;
	default:
		cant_happen();
	}
	
	if (dir == BEFORE) {
		if (! is_scalar(tree->subnode->type)) {
			fprintf(prof_fp, "(");
			tree_eval(tree->subnode);
			fprintf(prof_fp, ")");
		} else
			tree_eval(tree->subnode);
		fprintf(prof_fp, " %s ", op);
	} else {
		fprintf(prof_fp, " %s ", op);
		if (! is_scalar(tree->subnode->type)) {
			fprintf(prof_fp, "(");
			tree_eval(tree->subnode);
			fprintf(prof_fp, ")");
		} else
			tree_eval(tree->subnode);
	}
}

/* pp_list --- dump a list of arguments, without parens */

static void
pp_list(register NODE *tree)
{
	for (; tree != NULL; tree = tree->rnode) {
		if (tree->type != Node_expression_list) {
			fprintf(stderr, "pp_list: got %s\n",
					nodetype2str(tree->type));
			fflush(stderr);
		}
		assert(tree->type == Node_expression_list);
		tree_eval(tree->lnode);
		if (tree->rnode != NULL)
			fprintf(prof_fp, ", ");
	}
}

/* pp_print_stmt --- print a "print" or "printf" statement */

static void
pp_print_stmt(const char *command, register NODE *tree)
{
	NODE *redir = tree->rnode;

	indent(tree->exec_count);
	fprintf(prof_fp, "%s", command);
	if (redir != NULL) {	/* parenthesize if have a redirection */
		fprintf(prof_fp, "(");
		pp_list(tree->lnode);
		fprintf(prof_fp, ")");
		pp_redir(redir, AFTER);
	} else {
		fprintf(prof_fp, " ");
		pp_list(tree->lnode);
	}
	fprintf(prof_fp, "\n");
}

/* pp_delete --- print a "delete" statement */

static void
pp_delete(register NODE *tree)
{
	NODE *array, *subscript;

	array = tree->lnode;
	subscript = tree->rnode;
	indent(array->exec_count);
	if (array->type == Node_param_list)
		fprintf(prof_fp, "delete %s", fparms[array->param_cnt]);
	else
		fprintf(prof_fp, "delete %s", array->vname);
	if (subscript != NULL) {
		fprintf(prof_fp, "[");
		pp_list(subscript);
		fprintf(prof_fp, "]");
	}
	fprintf(prof_fp, "\n");
}

/* pp_in_array --- pretty print "foo in array" test */

static void
pp_in_array(NODE *array, NODE *subscript)
{
	if (subscript->type == Node_expression_list) {
		fprintf(prof_fp, "(");
		pp_list(subscript);
		fprintf(prof_fp, ")");
	} else
		pprint(subscript);

	if (array->type == Node_param_list)
		fprintf(prof_fp, " in %s", fparms[array->param_cnt]);
	else
		fprintf(prof_fp, " in %s", array->vname);
}

/* pp_getline --- print a getline statement */

static void
pp_getline(register NODE *tree)
{
	NODE *redir = tree->rnode;
	int before, after;

	/*
	 * command | getline
	 *     or
	 * command |& getline
	 *     or
	 * getline < file
	 */
	if (redir != NULL) {
		before = (redir->type == Node_redirect_pipein
				|| redir->type == Node_redirect_twoway);
		after = ! before;
	} else
		before = after = FALSE;

	if (before)
		pp_redir(redir, BEFORE);

	fprintf(prof_fp, "getline");
	if (tree->lnode != NULL) {	/* optional var */
		fprintf(prof_fp, " ");
		pp_lhs(tree->lnode);
	}

	if (after)
		pp_redir(redir, AFTER);
}

/* pp_builtin --- print a builtin function */

static void
pp_builtin(register NODE *tree)
{
	fprintf(prof_fp, "%s(", getfname(tree->proc));
	pp_list(tree->subnode);
	fprintf(prof_fp, ")");
}

/* pp_func_call --- print a function call */

static void
pp_func_call(NODE *name, NODE *arglist)
{
	fprintf(prof_fp, "%s(", name->stptr);
	pp_list(arglist);
	fprintf(prof_fp, ")");
}

/* dump_prog --- dump the program */

/*
 * XXX: I am not sure it is right to have the strings in the dump
 * be translated, but I'll leave it alone for now.
 */

void
dump_prog(NODE *begin, NODE *prog, NODE *end)
{
	time_t now;

	(void) time(& now);
	/* \n on purpose, with \n in ctime() output */
	fprintf(prof_fp, _("\t# gawk profile, created %s\n"), ctime(& now));

	if (begin != NULL) {
		fprintf(prof_fp, _("\t# BEGIN block(s)\n\n"));
		fprintf(prof_fp, "\tBEGIN {\n");
		in_BEGIN_or_END = TRUE;
		pprint(begin);
		in_BEGIN_or_END = FALSE;
		fprintf(prof_fp, "\t}\n");
		if (prog != NULL || end != NULL)
			fprintf(prof_fp, "\n");
	}
	if (prog != NULL) {
		fprintf(prof_fp, _("\t# Rule(s)\n\n"));
		pprint(prog);
		if (end != NULL)
			fprintf(prof_fp, "\n");
	}
	if (end != NULL) {
		fprintf(prof_fp, _("\t# END block(s)\n\n"));
		fprintf(prof_fp, "\tEND {\n");
		in_BEGIN_or_END = TRUE;
		pprint(end);
		in_BEGIN_or_END = FALSE;
		fprintf(prof_fp, "\t}\n");
	}
}

/* pp_func --- pretty print a function */

void
pp_func(char *name, size_t namelen, NODE *f)
{
	int j;
	char **pnames;
	static int first = TRUE;

	if (first) {
		first = FALSE;
		fprintf(prof_fp, _("\n\t# Functions, listed alphabetically\n"));
	}

	fprintf(prof_fp, "\n");
	indent(f->exec_count);
	fprintf(prof_fp, "function %.*s(", (int) namelen, name);
	pnames = f->parmlist;
	fparms = pnames;
	for (j = 0; j < f->lnode->param_cnt; j++) {
		fprintf(prof_fp, "%s", pnames[j]);
		if (j < f->lnode->param_cnt - 1)
			fprintf(prof_fp, ", ");
	}
	fprintf(prof_fp, ")\n\t{\n");
	indent_in();
	pprint(f->rnode);	/* body */
	indent_out();
	fprintf(prof_fp, "\t}\n");
}

/* pp_string --- pretty print a string or regex constant */

static void
pp_string(char *str, size_t len, int delim)
{
	pp_string_fp(prof_fp, str, len, delim, FALSE);
}

/* pp_string_fp --- printy print a string to the fp */

/*
 * This routine concentrates string pretty printing in one place,
 * so that it can be called from multiple places within gawk.
 */

void
pp_string_fp(FILE *fp, char *in_str, size_t len, int delim, int breaklines)
{
	static char escapes[] = "\b\f\n\r\t\v\\";
	static char printables[] = "bfnrtv\\";
	char *cp;
	int i;
	int count;
#define BREAKPOINT	70 /* arbitrary */
	unsigned char *str = (unsigned char *) in_str;

	fprintf(fp, "%c", delim);
	for (count = 0; len > 0; len--, str++) {
		if (++count >= BREAKPOINT && breaklines) {
			fprintf(fp, "%c\n%c", delim, delim);
			count = 0;
		}
		if (*str == delim) {
			fprintf(fp, "\\%c", delim);
			count++;
		} else if (*str == BELL) {
			fprintf(fp, "\\a");
			count++;
		} else if ((cp = strchr(escapes, *str)) != NULL) {
			i = cp - escapes;
			putc('\\', fp);
			count++;
			putc(printables[i], fp);
			if (breaklines && *str == '\n' && delim == '"') {
				fprintf(fp, "\"\n\"");
				count = 0;
			}
		/* NB: Deliberate use of lower-case versions. */
		} else if (isascii(*str) && isprint(*str)) {
			putc(*str, fp);
		} else {
			char buf[10];

			sprintf(buf, "\\%03o", *str & 0xff);
			count += strlen(buf) - 1;
			fprintf(fp, "%s", buf);
		}
	}
	fprintf(fp, "%c", delim);
}

/* is_scalar --- true or false if we'll get a scalar value */

static int
is_scalar(NODETYPE type)
{
	switch (type) {
	case Node_var:
	case Node_var_array:
	case Node_val:
	case Node_BINMODE:
	case Node_CONVFMT:
	case Node_FIELDWIDTHS:
	case Node_FNR:
	case Node_FS:
	case Node_IGNORECASE:
	case Node_LINT:
	case Node_NF:
	case Node_NR:
	case Node_OFMT:
	case Node_OFS:
	case Node_ORS:
	case Node_RS:
	case Node_subscript:
		return TRUE;
	default:
		return FALSE;
	}
}

/* prec_level --- return the precedence of an operator, for paren tests */

static int
prec_level(NODETYPE type)
{
	switch (type) {
	case Node_var:
	case Node_var_array:
	case Node_param_list:
	case Node_subscript:
	case Node_func_call:
	case Node_val:
	case Node_builtin:
	case Node_BINMODE:
	case Node_CONVFMT:
	case Node_FIELDWIDTHS:
	case Node_FNR:
	case Node_FS:
	case Node_IGNORECASE:
	case Node_LINT:
	case Node_NF:
	case Node_NR:
	case Node_OFMT:
	case Node_OFS:
	case Node_ORS:
	case Node_RS:
		return 15;

	case Node_field_spec:
		return 14;

	case Node_exp:
		return 13;

	case Node_preincrement:
	case Node_predecrement:
	case Node_postincrement:
	case Node_postdecrement:
		return 12;

	case Node_unary_minus:
	case Node_not:
		return 11;

	case Node_times:
	case Node_quotient:
	case Node_mod:
		return 10;

	case Node_plus:
	case Node_minus:
		return 9;

	case Node_concat:
		return 8;

	case Node_equal:
	case Node_notequal:
	case Node_greater:
	case Node_leq:
	case Node_geq:
	case Node_match:
	case Node_nomatch:
		return 7;

	case Node_K_getline:
		return 6;

	case Node_less:
		return 5;

	case Node_in_array:
		return 5;

	case Node_and:
		return 4;

	case Node_or:
		return 3;

	case Node_cond_exp:
		return 2;

	case Node_assign:
	case Node_assign_times:
	case Node_assign_quotient:
	case Node_assign_mod:
	case Node_assign_plus:
	case Node_assign_minus:
	case Node_assign_exp:
		return 1;

	default:
		fatal(_("unexpected type %s in prec_level"), nodetype2str(type));
		return 0;	/* keep the compiler happy */
	}
}

/* parenthesize --- print a subtree in parentheses if need be */

static void
parenthesize(NODETYPE parent_type, NODE *tree)
{
	NODETYPE child_type;

	if (tree == NULL)
		return;

	child_type = tree->type;

	in_expr++;
	/* first the special cases, then the general ones */
	if (parent_type == Node_not && child_type == Node_in_array) {
		fprintf(prof_fp, "! (");
		pp_in_array(tree->lnode, tree->rnode);
		fprintf(prof_fp, ")");
	/* other special cases here, as needed */
	} else if (prec_level(child_type) < prec_level(parent_type)) {
		fprintf(prof_fp, "(");
		tree_eval(tree);
		fprintf(prof_fp, ")");
	} else
		tree_eval(tree);
	in_expr--;
}

#ifdef PROFILING
/* just_dump --- dump the profile and function stack and keep going */

static RETSIGTYPE
just_dump(int signum)
{
	extern NODE *begin_block, *expression_value, *end_block;

	dump_prog(begin_block, expression_value, end_block);
	dump_funcs();
	dump_fcall_stack(prof_fp);
	fflush(prof_fp);
	signal(signum, just_dump);	/* for OLD Unix systems ... */
}

/* dump_and_exit --- dump the profile, the function stack, and exit */

static RETSIGTYPE
dump_and_exit(int signum)
{
	just_dump(signum);
	exit(1);
}
#endif
