/*
 * eval.c - gawk parse tree interpreter 
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991-1999 the Free Software Foundation, Inc.
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
 *
 * $FreeBSD: src/contrib/awk/eval.c,v 1.4 1999/09/27 08:56:57 sheldonh Exp $
 */

#include "awk.h"

extern double pow P((double x, double y));
extern double modf P((double x, double *yp));
extern double fmod P((double x, double y));

static int eval_condition P((NODE *tree));
static NODE *op_assign P((NODE *tree));
static NODE *func_call P((NODE *name, NODE *arg_list));
static NODE *match_op P((NODE *tree));
static void push_args P((int count, NODE *arglist, NODE **oldstack, char *func_name));
static void pop_fcall_stack P((void));
static void pop_fcall P((void));
static int in_function P((void));
char *nodetype2str P((NODETYPE type));
char *flags2str P((int flagval));

#if __GNUC__ < 2
NODE *_t;		/* used as a temporary in macros */
#endif
#ifdef MSDOS
double _msc51bug;	/* to get around a bug in MSC 5.1 */
#endif
NODE *ret_node;
int OFSlen;
int ORSlen;
int OFMTidx;
int CONVFMTidx;

/* Macros and variables to save and restore function and loop bindings */
/*
 * the val variable allows return/continue/break-out-of-context to be
 * caught and diagnosed
 */
#define PUSH_BINDING(stack, x, val) (memcpy((char *)(stack), (char *)(x), sizeof(jmp_buf)), val++)
#define RESTORE_BINDING(stack, x, val) (memcpy((char *)(x), (char *)(stack), sizeof(jmp_buf)), val--)

static jmp_buf loop_tag;		/* always the current binding */
static int loop_tag_valid = FALSE;	/* nonzero when loop_tag valid */
static int func_tag_valid = FALSE;
static jmp_buf func_tag;
extern int exiting, exit_val;

/*
 * This table is used by the regexp routines to do case independant
 * matching. Basically, every ascii character maps to itself, except
 * uppercase letters map to lower case ones. This table has 256
 * entries, for ISO 8859-1. Note also that if the system this
 * is compiled on doesn't use 7-bit ascii, casetable[] should not be
 * defined to the linker, so gawk should not load.
 *
 * Do NOT make this array static, it is used in several spots, not
 * just in this file.
 */
#if 'a' == 97	/* it's ascii */
char casetable[] = {
	'\000', '\001', '\002', '\003', '\004', '\005', '\006', '\007',
	'\010', '\011', '\012', '\013', '\014', '\015', '\016', '\017',
	'\020', '\021', '\022', '\023', '\024', '\025', '\026', '\027',
	'\030', '\031', '\032', '\033', '\034', '\035', '\036', '\037',
	/* ' '     '!'     '"'     '#'     '$'     '%'     '&'     ''' */
	'\040', '\041', '\042', '\043', '\044', '\045', '\046', '\047',
	/* '('     ')'     '*'     '+'     ','     '-'     '.'     '/' */
	'\050', '\051', '\052', '\053', '\054', '\055', '\056', '\057',
	/* '0'     '1'     '2'     '3'     '4'     '5'     '6'     '7' */
	'\060', '\061', '\062', '\063', '\064', '\065', '\066', '\067',
	/* '8'     '9'     ':'     ';'     '<'     '='     '>'     '?' */
	'\070', '\071', '\072', '\073', '\074', '\075', '\076', '\077',
	/* '@'     'A'     'B'     'C'     'D'     'E'     'F'     'G' */
	'\100', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
	/* 'H'     'I'     'J'     'K'     'L'     'M'     'N'     'O' */
	'\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
	/* 'P'     'Q'     'R'     'S'     'T'     'U'     'V'     'W' */
	'\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
	/* 'X'     'Y'     'Z'     '['     '\'     ']'     '^'     '_' */
	'\170', '\171', '\172', '\133', '\134', '\135', '\136', '\137',
	/* '`'     'a'     'b'     'c'     'd'     'e'     'f'     'g' */
	'\140', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
	/* 'h'     'i'     'j'     'k'     'l'     'm'     'n'     'o' */
	'\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
	/* 'p'     'q'     'r'     's'     't'     'u'     'v'     'w' */
	'\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
	/* 'x'     'y'     'z'     '{'     '|'     '}'     '~' */
	'\170', '\171', '\172', '\173', '\174', '\175', '\176', '\177',
#ifndef USE_PURE_ASCII
	'\200', '\201', '\202', '\203', '\204', '\205', '\206', '\207',
	'\210', '\211', '\212', '\213', '\214', '\215', '\216', '\217',
	'\220', '\221', '\222', '\223', '\224', '\225', '\226', '\227',
	'\230', '\231', '\232', '\233', '\234', '\235', '\236', '\237',
	'\240', '\241', '\242', '\243', '\244', '\245', '\246', '\247',
	'\250', '\251', '\252', '\253', '\254', '\255', '\256', '\257',
	'\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',
	'\270', '\271', '\272', '\273', '\274', '\275', '\276', '\277',
	'\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
	'\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
	'\360', '\361', '\362', '\363', '\364', '\365', '\366', '\327',
	'\370', '\371', '\372', '\373', '\374', '\375', '\376', '\337',
	'\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
	'\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
	'\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
	'\370', '\371', '\372', '\373', '\374', '\375', '\376', '\377',
#else
	'\200', '\201', '\202', '\203', '\204', '\205', '\206', '\207',
	'\210', '\211', '\212', '\213', '\214', '\215', '\216', '\217',
	'\220', '\221', '\222', '\223', '\224', '\225', '\226', '\227',
	'\230', '\231', '\232', '\233', '\234', '\235', '\236', '\237',
	'\240', '\241', '\242', '\243', '\244', '\245', '\246', '\247',
	'\250', '\251', '\252', '\253', '\254', '\255', '\256', '\257',
	'\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',
	'\270', '\271', '\272', '\273', '\274', '\275', '\276', '\277',
	'\300', '\301', '\302', '\303', '\304', '\305', '\306', '\307',
	'\310', '\311', '\312', '\313', '\314', '\315', '\316', '\317',
	'\320', '\321', '\322', '\323', '\324', '\325', '\326', '\327',
	'\330', '\331', '\332', '\333', '\334', '\335', '\336', '\337',
	'\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
	'\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
	'\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
	'\370', '\371', '\372', '\373', '\374', '\375', '\376', '\377',
#endif
};
#else
#include "You lose. You will need a translation table for your character set."
#endif

/*
 * This table maps node types to strings for debugging.
 * KEEP IN SYNC WITH awk.h!!!!
 */
static char *nodetypes[] = {
	"Node_illegal",
	"Node_times",
	"Node_quotient",
	"Node_mod",
	"Node_plus",
	"Node_minus",
	"Node_cond_pair",
	"Node_subscript",
	"Node_concat",
	"Node_exp",
	"Node_preincrement",
	"Node_predecrement",
	"Node_postincrement",
	"Node_postdecrement",
	"Node_unary_minus",
	"Node_field_spec",
	"Node_assign",
	"Node_assign_times",
	"Node_assign_quotient",
	"Node_assign_mod",
	"Node_assign_plus",
	"Node_assign_minus",
	"Node_assign_exp",
	"Node_and",
	"Node_or",
	"Node_equal",
	"Node_notequal",
	"Node_less",
	"Node_greater",
	"Node_leq",
	"Node_geq",
	"Node_match",
	"Node_nomatch",
	"Node_not",
	"Node_rule_list",
	"Node_rule_node",
	"Node_statement_list",
	"Node_if_branches",
	"Node_expression_list",
	"Node_param_list",
	"Node_K_if",
	"Node_K_while",	
	"Node_K_for",
	"Node_K_arrayfor",
	"Node_K_break",
	"Node_K_continue",
	"Node_K_print",
	"Node_K_printf",
	"Node_K_next",
	"Node_K_exit",
	"Node_K_do",
	"Node_K_return",
	"Node_K_delete",
	"Node_K_getline",
	"Node_K_function",
	"Node_K_nextfile",
	"Node_redirect_output",
	"Node_redirect_append",
	"Node_redirect_pipe",
	"Node_redirect_pipein",
	"Node_redirect_input",
	"Node_var",
	"Node_var_array",
	"Node_val",
	"Node_builtin",
	"Node_line_range",
	"Node_in_array",
	"Node_func",
	"Node_func_call",
	"Node_cond_exp",
	"Node_regex",
	"Node_hashnode",
	"Node_ahash",
	"Node_NF",
	"Node_NR",
	"Node_FNR",
	"Node_FS",
	"Node_RS",
	"Node_FIELDWIDTHS",
	"Node_IGNORECASE",
	"Node_OFS",
	"Node_ORS",
	"Node_OFMT",
	"Node_CONVFMT",
	"Node_final",
	NULL
};

char *
nodetype2str(type)
NODETYPE type;
{
	static char buf[40];

	if (type >= Node_illegal && type <= Node_final)
		return nodetypes[(int) type];

	sprintf(buf, "unknown nodetype %d", (int) type);
	return buf;
}

/* flags2str --- make a flags value readable */

char *
flags2str(flagval)
int flagval;
{
	static char buffer[BUFSIZ];
	char *sp;

	sp = buffer;

	if (flagval & MALLOC) {
		strcpy(sp, "MALLOC");
		sp += strlen(sp);
	}
	if (flagval & TEMP) {
		if (sp != buffer)
			*sp++ = '|';
		strcpy(sp, "TEMP");
		sp += strlen(sp);
	}
	if (flagval & PERM) {
		if (sp != buffer)
			*sp++ = '|';
		strcpy(sp, "PERM");
		sp += strlen(sp);
	}
	if (flagval & STRING) {
		if (sp != buffer)
			*sp++ = '|';
		strcpy(sp, "STRING");
		sp += strlen(sp);
	}
	if (flagval & STR) {
		if (sp != buffer)
			*sp++ = '|';
		strcpy(sp, "STR");
		sp += strlen(sp);
	}
	if (flagval & NUM) {
		if (sp != buffer)
			*sp++ = '|';
		strcpy(sp, "NUM");
		sp += strlen(sp);
	}
	if (flagval & NUMBER) {
		if (sp != buffer)
			*sp++ = '|';
		strcpy(sp, "NUMBER");
		sp += strlen(sp);
	}
	if (flagval & MAYBE_NUM) {
		if (sp != buffer)
			*sp++ = '|';
		strcpy(sp, "MAYBE_NUM");
		sp += strlen(sp);
	}
	if (flagval & ARRAYMAXED) {
		if (sp != buffer)
			*sp++ = '|';
		strcpy(sp, "ARRAYMAXED");
		sp += strlen(sp);
	}
	if (flagval & SCALAR) {
		if (sp != buffer)
			*sp++ = '|';
		strcpy(sp, "SCALAR");
		sp += strlen(sp);
	}
	if (flagval & FUNC) {
		if (sp != buffer)
			*sp++ = '|';
		strcpy(sp, "FUNC");
		sp += strlen(sp);
	}
	if (flagval & FIELD) {
		if (sp != buffer)
			*sp++ = '|';
		strcpy(sp, "FIELD");
		sp += strlen(sp);
	}

	return buffer;
}

/*
 * interpret:
 * Tree is a bunch of rules to run. Returns zero if it hit an exit()
 * statement 
 */
int
interpret(tree)
register NODE *volatile tree;
{
	jmp_buf volatile loop_tag_stack; /* shallow binding stack for loop_tag */
	static jmp_buf rule_tag; /* tag the rule currently being run, for NEXT
				  * and EXIT statements.  It is static because
				  * there are no nested rules */
	register NODE *volatile t = NULL;	/* temporary */
	NODE **volatile lhs;	/* lhs == Left Hand Side for assigns, etc */
	NODE *volatile stable_tree;
	int volatile traverse = TRUE;	/* True => loop thru tree (Node_rule_list) */

	/* avoid false source indications */
	source = NULL;
	sourceline = 0;

	if (tree == NULL)
		return 1;
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
			switch (setjmp(rule_tag)) {
			case 0:	/* normal non-jump */
				/* test pattern, if any */
				if (tree->lnode == NULL ||
				    eval_condition(tree->lnode))
					(void) interpret(tree->rnode);
				break;
			case TAG_CONTINUE:	/* NEXT statement */
				return 1;
			case TAG_BREAK:
				return 0;
			default:
				cant_happen();
			}
			if (! traverse) 	/* case Node_rule_node */
				break;		/* don't loop */
		}
		break;

	case Node_statement_list:
		for (t = tree; t != NULL; t = t->rnode)
			(void) interpret(t->lnode);
		break;

	case Node_K_if:
		if (eval_condition(tree->lnode))
			(void) interpret(tree->rnode->lnode);
		else
			(void) interpret(tree->rnode->rnode);
		break;

	case Node_K_while:
		PUSH_BINDING(loop_tag_stack, loop_tag, loop_tag_valid);

		stable_tree = tree;
		while (eval_condition(stable_tree->lnode)) {
			switch (setjmp(loop_tag)) {
			case 0:	/* normal non-jump */
				(void) interpret(stable_tree->rnode);
				break;
			case TAG_CONTINUE:	/* continue statement */
				break;
			case TAG_BREAK:	/* break statement */
				RESTORE_BINDING(loop_tag_stack, loop_tag, loop_tag_valid);
				return 1;
			default:
				cant_happen();
			}
		}
		RESTORE_BINDING(loop_tag_stack, loop_tag, loop_tag_valid);
		break;

	case Node_K_do:
		PUSH_BINDING(loop_tag_stack, loop_tag, loop_tag_valid);
		stable_tree = tree;
		do {
			switch (setjmp(loop_tag)) {
			case 0:	/* normal non-jump */
				(void) interpret(stable_tree->rnode);
				break;
			case TAG_CONTINUE:	/* continue statement */
				break;
			case TAG_BREAK:	/* break statement */
				RESTORE_BINDING(loop_tag_stack, loop_tag, loop_tag_valid);
				return 1;
			default:
				cant_happen();
			}
		} while (eval_condition(stable_tree->lnode));
		RESTORE_BINDING(loop_tag_stack, loop_tag, loop_tag_valid);
		break;

	case Node_K_for:
		PUSH_BINDING(loop_tag_stack, loop_tag, loop_tag_valid);
		(void) interpret(tree->forloop->init);
		stable_tree = tree;
		while (eval_condition(stable_tree->forloop->cond)) {
			switch (setjmp(loop_tag)) {
			case 0:	/* normal non-jump */
				(void) interpret(stable_tree->lnode);
				/* fall through */
			case TAG_CONTINUE:	/* continue statement */
				(void) interpret(stable_tree->forloop->incr);
				break;
			case TAG_BREAK:	/* break statement */
				RESTORE_BINDING(loop_tag_stack, loop_tag, loop_tag_valid);
				return 1;
			default:
				cant_happen();
			}
		}
		RESTORE_BINDING(loop_tag_stack, loop_tag, loop_tag_valid);
		break;

	case Node_K_arrayfor:
		{
		volatile struct search l;	/* For array_for */
		Func_ptr after_assign = NULL;

#define hakvar forloop->init
#define arrvar forloop->incr
		PUSH_BINDING(loop_tag_stack, loop_tag, loop_tag_valid);
		lhs = get_lhs(tree->hakvar, &after_assign);
		t = tree->arrvar;
		if (t->type == Node_param_list)
			t = stack_ptr[t->param_cnt];
		stable_tree = tree;
		if ((t->flags & SCALAR) != 0)
			fatal("attempt to use scalar as array");
		for (assoc_scan(t, (struct search *)&l);
		     l.retval;
		     assoc_next((struct search *)&l)) {
			unref(*((NODE **) lhs));
			*lhs = dupnode(l.retval);
			if (after_assign)
				(*after_assign)();
			switch (setjmp(loop_tag)) {
			case 0:
				(void) interpret(stable_tree->lnode);
			case TAG_CONTINUE:
				break;

			case TAG_BREAK:
				RESTORE_BINDING(loop_tag_stack, loop_tag, loop_tag_valid);
				return 1;
			default:
				cant_happen();
			}
		}
		RESTORE_BINDING(loop_tag_stack, loop_tag, loop_tag_valid);
		break;
		}

	case Node_K_break:
		if (! loop_tag_valid) {
			/*
			 * Old AT&T nawk treats break outside of loops like
			 * next. New ones catch it at parse time. Allow it if
			 * do_traditional is on, and complain if lint.
			 */
			static int warned = FALSE;

			if (do_lint && ! warned) {
				warning("use of `break' outside a loop is not portable");
				warned = TRUE;
			}
			if (! do_traditional || do_posix)
				fatal("use of `break' outside a loop is not allowed");
			if (in_function())
				pop_fcall_stack();
			longjmp(rule_tag, TAG_CONTINUE);
		} else
			longjmp(loop_tag, TAG_BREAK);
		break;

	case Node_K_continue:
		if (! loop_tag_valid) {
			/*
			 * Old AT&T nawk treats continue outside of loops like
			 * next. New ones catch it at parse time. Allow it if
			 * do_traditional is on, and complain if lint.
			 */
			static int warned = FALSE;

			if (do_lint && ! warned) {
				warning("use of `continue' outside a loop is not portable");
				warned = TRUE;
			}
			if (! do_traditional || do_posix)
				fatal("use of `continue' outside a loop is not allowed");
			if (in_function())
				pop_fcall_stack();
			longjmp(rule_tag, TAG_CONTINUE);
		} else
			longjmp(loop_tag, TAG_CONTINUE);
		break;

	case Node_K_print:
		do_print(tree);
		break;

	case Node_K_printf:
		do_printf(tree);
		break;

	case Node_K_delete:
		do_delete(tree->lnode, tree->rnode);
		break;

	case Node_K_next:
		if (in_begin_rule)
			fatal("`next' cannot be called from a BEGIN rule");
		else if (in_end_rule)
			fatal("`next' cannot be called from an END rule");

		if (in_function())
			pop_fcall_stack();

		longjmp(rule_tag, TAG_CONTINUE);
		break;

	case Node_K_nextfile:
		if (in_begin_rule)
			fatal("`nextfile' cannot be called from a BEGIN rule");
		else if (in_end_rule)
			fatal("`nextfile' cannot be called from an END rule");

		if (in_function())
			pop_fcall_stack();

		do_nextfile();
		break;

	case Node_K_exit:
		/*
		 * In A,K,&W, p. 49, it says that an exit statement "...
		 * causes the program to behave as if the end of input had
		 * occurred; no more input is read, and the END actions, if
		 * any are executed." This implies that the rest of the rules
		 * are not done. So we immediately break out of the main loop.
		 */
		exiting = TRUE;
		if (tree->lnode != NULL) {
			t = tree_eval(tree->lnode);
			exit_val = (int) force_number(t);
			free_temp(t);
		}
		longjmp(rule_tag, TAG_BREAK);
		break;

	case Node_K_return:
		t = tree_eval(tree->lnode);
		ret_node = dupnode(t);
		free_temp(t);
		longjmp(func_tag, TAG_RETURN);
		break;

	default:
		/*
		 * Appears to be an expression statement.  Throw away the
		 * value. 
		 */
		if (do_lint && tree->type == Node_var)
			warning("statement has no effect");
		t = tree_eval(tree);
		free_temp(t);
		break;
	}
	return 1;
}

/* r_tree_eval --- evaluate a subtree */

NODE *
r_tree_eval(tree, iscond)
register NODE *tree;
int iscond;
{
	register NODE *r, *t1, *t2;	/* return value & temporary subtrees */
	register NODE **lhs;
	register int di;
	AWKNUM x, x1, x2;
	long lx;
#ifdef _CRAY
	long lx2;
#endif
	char namebuf[100];

#ifdef DEBUG
	if (tree == NULL)
		return Nnull_string;
	else if (tree->type == Node_val) {
		if (tree->stref <= 0)
			cant_happen();
		return tree;
	} else if (tree->type == Node_var) {
		if (tree->var_value->stref <= 0)
			cant_happen();
		return tree->var_value;
	}
#endif

	if (tree->type == Node_param_list) {
		int paramnum = tree->param_cnt + 1;

		tree = stack_ptr[tree->param_cnt];
		if (tree == NULL)
			return Nnull_string;
		sprintf(namebuf, "parameter #%d", paramnum);
		tree->vname = namebuf;
	}

	switch (tree->type) {
	case Node_var:
		return tree->var_value;

	case Node_and:
		return tmp_number((AWKNUM) (eval_condition(tree->lnode)
					    && eval_condition(tree->rnode)));

	case Node_or:
		return tmp_number((AWKNUM) (eval_condition(tree->lnode)
					    || eval_condition(tree->rnode)));

	case Node_not:
		return tmp_number((AWKNUM) ! eval_condition(tree->lnode));

		/* Builtins */
	case Node_builtin:
		return (*tree->proc)(tree->subnode);

	case Node_K_getline:
		return (do_getline(tree));

	case Node_in_array:
		return tmp_number((AWKNUM) in_array(tree->lnode, tree->rnode));

	case Node_func_call:
		return func_call(tree->rnode, tree->lnode);

		/* unary operations */
	case Node_NR:
	case Node_FNR:
	case Node_NF:
	case Node_FIELDWIDTHS:
	case Node_FS:
	case Node_RS:
	case Node_field_spec:
	case Node_subscript:
	case Node_IGNORECASE:
	case Node_OFS:
	case Node_ORS:
	case Node_OFMT:
	case Node_CONVFMT:
		lhs = get_lhs(tree, (Func_ptr *) NULL);
		return *lhs;

	case Node_var_array:
		fatal("attempt to use array `%s' in a scalar context",
			tree->vname);

	case Node_unary_minus:
		t1 = tree_eval(tree->subnode);
		x = -force_number(t1);
		free_temp(t1);
		return tmp_number(x);

	case Node_cond_exp:
		if (eval_condition(tree->lnode))
			return tree_eval(tree->rnode->lnode);
		return tree_eval(tree->rnode->rnode);

	case Node_match:
	case Node_nomatch:
	case Node_regex:
		return match_op(tree);

	case Node_func:
		fatal("function `%s' called with space between name and (,\n%s",
			tree->lnode->param,
			"or used in other expression context");

		/* assignments */
	case Node_assign:
		{
		Func_ptr after_assign = NULL;

		if (iscond && do_lint)
			warning("assignment used in conditional context");
		r = tree_eval(tree->rnode);
		lhs = get_lhs(tree->lnode, &after_assign);
		if (r != *lhs) {
			NODE *save;

			save = *lhs;
			*lhs = dupnode(r);
			unref(save);
		}
		free_temp(r);
		tree->lnode->flags |= SCALAR;
		if (after_assign)
			(*after_assign)();
		return *lhs;
		}

	case Node_concat:
		{
		NODE **treelist;
		NODE **strlist;
		NODE *save_tree;
		register NODE **treep;
		register NODE **strp;
		register size_t len;
		char *str;
		register char *dest;
		int alloc_count, str_count;
		int i;

		/*
		 * This is an efficiency hack for multiple adjacent string
		 * concatenations, to avoid recursion and string copies.
		 *
		 * Node_concat trees grow downward to the left, so
		 * descend to lowest (first) node, accumulating nodes
		 * to evaluate to strings as we go.
		 */

		/*
		 * But first, no arbitrary limits. Count the number of
		 * nodes and malloc the treelist and strlist arrays.
		 * There will be alloc_count + 1 items to concatenate. We
		 * also leave room for an extra pointer at the end to
		 * use as a sentinel.  Thus, start alloc_count at 2.
		 */
		save_tree = tree;
		for (alloc_count = 2; tree && tree->type == Node_concat; tree = tree->lnode)
			alloc_count++;
		tree = save_tree;
		emalloc(treelist, NODE **, sizeof(NODE *) * alloc_count, "tree_eval");
		emalloc(strlist, NODE **, sizeof(NODE *) * alloc_count, "tree_eval");

		/* Now, here we go. */
		treep = treelist;
		while (tree && tree->type == Node_concat) {
			*treep++ = tree->rnode;
			tree = tree->lnode;
		}
		*treep = tree;
		/*
		 * Now, evaluate to strings in LIFO order, accumulating
		 * the string length, so we can do a single malloc at the
		 * end.
		 *
		 * Evaluate the expressions first, then get their
		 * lengthes, in case one of the expressions has a
		 * side effect that changes one of the others.
		 * See test/nasty.awk.
		 */
		strp = strlist;
		len = 0;
		while (treep >= treelist) {
			*strp = force_string(tree_eval(*treep--));
			strp++;
		}
		*strp = NULL;

		str_count = strp - strlist;
		strp = strlist;
		for (i = 0; i < str_count; i++) {
			len += (*strp)->stlen;
			strp++;
		}
		emalloc(str, char *, len+2, "tree_eval");
		str[len] = str[len+1] = '\0';	/* for good measure */
		dest = str;
		strp = strlist;
		while (*strp) {
			memcpy(dest, (*strp)->stptr, (*strp)->stlen);
			dest += (*strp)->stlen;
			free_temp(*strp);
			strp++;
		}
		r = make_str_node(str, len, ALREADY_MALLOCED);
		r->flags |= TEMP;

		free(strlist);
		free(treelist);
		}
		return r;

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
		return op_assign(tree);
	default:
		break;	/* handled below */
	}

	/* evaluate subtrees in order to do binary operation, then keep going */
	t1 = tree_eval(tree->lnode);
	t2 = tree_eval(tree->rnode);

	switch (tree->type) {
	case Node_geq:
	case Node_leq:
	case Node_greater:
	case Node_less:
	case Node_notequal:
	case Node_equal:
		di = cmp_nodes(t1, t2);
		free_temp(t1);
		free_temp(t2);
		switch (tree->type) {
		case Node_equal:
			return tmp_number((AWKNUM) (di == 0));
		case Node_notequal:
			return tmp_number((AWKNUM) (di != 0));
		case Node_less:
			return tmp_number((AWKNUM) (di < 0));
		case Node_greater:
			return tmp_number((AWKNUM) (di > 0));
		case Node_leq:
			return tmp_number((AWKNUM) (di <= 0));
		case Node_geq:
			return tmp_number((AWKNUM) (di >= 0));
		default:
			cant_happen();
		}
		break;
	default:
		break;	/* handled below */
	}

	x1 = force_number(t1);
	free_temp(t1);
	x2 = force_number(t2);
	free_temp(t2);
	switch (tree->type) {
	case Node_exp:
		if ((lx = x2) == x2 && lx >= 0) {	/* integer exponent */
			if (lx == 0)
				x = 1;
			else if (lx == 1)
				x = x1;
			else {
				/* doing it this way should be more precise */
				for (x = x1; --lx; )
					x *= x1;
			}
		} else
			x = pow((double) x1, (double) x2);
		return tmp_number(x);

	case Node_times:
		return tmp_number(x1 * x2);

	case Node_quotient:
		if (x2 == 0)
			fatal("division by zero attempted");
#ifdef _CRAY
		/* special case for integer division, put in for Cray */
		lx2 = x2;
		if (lx2 == 0)
			return tmp_number(x1 / x2);
		lx = (long) x1 / lx2;
		if (lx * x2 == x1)
			return tmp_number((AWKNUM) lx);
		else
#endif
			return tmp_number(x1 / x2);

	case Node_mod:
		if (x2 == 0)
			fatal("division by zero attempted in mod");
#ifdef HAVE_FMOD
		return tmp_number(fmod(x1, x2));
#else	/* ! HAVE_FMOD */
		(void) modf(x1 / x2, &x);
		return tmp_number(x1 - x * x2);
#endif	/* ! HAVE_FMOD */

	case Node_plus:
		return tmp_number(x1 + x2);

	case Node_minus:
		return tmp_number(x1 - x2);

	case Node_var_array:
		fatal("attempt to use array `%s' in a scalar context",
			tree->vname);

	default:
		fatal("illegal type (%s) in tree_eval", nodetype2str(tree->type));
	}
	return 0;
}

/* eval_condition --- is TREE true or false? Returns 0==false, non-zero==true */

static int
eval_condition(tree)
register NODE *tree;
{
	register NODE *t1;
	register int ret;

	if (tree == NULL)	/* Null trees are the easiest kinds */
		return TRUE;
	if (tree->type == Node_line_range) {
		/*
		 * Node_line_range is kind of like Node_match, EXCEPT: the
		 * lnode field (more properly, the condpair field) is a node
		 * of a Node_cond_pair; whether we evaluate the lnode of that
		 * node or the rnode depends on the triggered word.  More
		 * precisely:  if we are not yet triggered, we tree_eval the
		 * lnode; if that returns true, we set the triggered word. 
		 * If we are triggered (not ELSE IF, note), we tree_eval the
		 * rnode, clear triggered if it succeeds, and perform our
		 * action (regardless of success or failure).  We want to be
		 * able to begin and end on a single input record, so this
		 * isn't an ELSE IF, as noted above.
		 */
		if (! tree->triggered)
			if (! eval_condition(tree->condpair->lnode))
				return FALSE;
			else
				tree->triggered = TRUE;
		/* Else we are triggered */
		if (eval_condition(tree->condpair->rnode))
			tree->triggered = FALSE;
		return TRUE;
	}

	/*
	 * Could just be J.random expression. in which case, null and 0 are
	 * false, anything else is true 
	 */

	t1 = m_tree_eval(tree, TRUE);
	if (t1->flags & MAYBE_NUM)
		(void) force_number(t1);
	if (t1->flags & NUMBER)
		ret = (t1->numbr != 0.0);
	else
		ret = (t1->stlen != 0);
	free_temp(t1);
	return ret;
}

/* cmp_nodes --- compare two nodes, returning negative, 0, positive */

int
cmp_nodes(t1, t2)
register NODE *t1, *t2;
{
	register int ret;
	register size_t len1, len2;
	register int l;
	int ldiff;

	if (t1 == t2)
		return 0;
	if (t1->flags & MAYBE_NUM)
		(void) force_number(t1);
	if (t2->flags & MAYBE_NUM)
		(void) force_number(t2);
	if ((t1->flags & NUMBER) && (t2->flags & NUMBER)) {
		if (t1->numbr == t2->numbr)
			return 0;
		/* don't subtract, in case one or both are infinite */
		else if (t1->numbr < t2->numbr)
			return -1;
		else
			return 1;
	}
	(void) force_string(t1);
	(void) force_string(t2);
	len1 = t1->stlen;
	len2 = t2->stlen;
	ldiff = len1 - len2;
	if (len1 == 0 || len2 == 0)
		return ldiff;
	l = (ldiff <= 0 ? len1 : len2);
	if (IGNORECASE) {
		register unsigned char *cp1 = (unsigned char *) t1->stptr;
		register unsigned char *cp2 = (unsigned char *) t2->stptr;

		for (ret = 0; l-- > 0 && ret == 0; cp1++, cp2++)
			ret = casetable[*cp1] - casetable[*cp2];
	} else
		ret = memcmp(t1->stptr, t2->stptr, l);
	return (ret == 0 ? ldiff : ret);
}

/* op_assign --- do +=, -=, etc. */

static NODE *
op_assign(tree)
register NODE *tree;
{
	AWKNUM rval, lval;
	NODE **lhs;
	AWKNUM t1, t2;
	long ltemp;
	NODE *tmp;
	Func_ptr after_assign = NULL;

	lhs = get_lhs(tree->lnode, &after_assign);
	lval = force_number(*lhs);

	/*
	 * Can't unref *lhs until we know the type; doing so
	 * too early breaks   x += x   sorts of things.
	 */
	switch(tree->type) {
	case Node_preincrement:
	case Node_predecrement:
		unref(*lhs);
		*lhs = make_number(lval +
			       (tree->type == Node_preincrement ? 1.0 : -1.0));
		tree->lnode->flags |= SCALAR;
		if (after_assign)
			(*after_assign)();
		return *lhs;

	case Node_postincrement:
	case Node_postdecrement:
		unref(*lhs);
		*lhs = make_number(lval +
			       (tree->type == Node_postincrement ? 1.0 : -1.0));
		tree->lnode->flags |= SCALAR;
		if (after_assign)
			(*after_assign)();
		return tmp_number(lval);
	default:
		break;	/* handled below */
	}

	tmp = tree_eval(tree->rnode);
	rval = force_number(tmp);
	free_temp(tmp);

	/*
	 * Do this again; the lhs and the rhs could both be fields.
	 * Accessing the rhs could cause the lhs to have moved around.
	 * (Yet another special case. Gack.)
	 */
	lhs = get_lhs(tree->lnode, &after_assign);

	unref(*lhs);
	switch(tree->type) {
	case Node_assign_exp:
		if ((ltemp = rval) == rval) {	/* integer exponent */
			if (ltemp == 0)
				*lhs = make_number((AWKNUM) 1);
			else if (ltemp == 1)
				*lhs = make_number(lval);
			else {
				/* doing it this way should be more precise */
				for (t1 = t2 = lval; --ltemp; )
					t1 *= t2;
				*lhs = make_number(t1);
			}
		} else
			*lhs = make_number((AWKNUM) pow((double) lval, (double) rval));
		break;

	case Node_assign_times:
		*lhs = make_number(lval * rval);
		break;

	case Node_assign_quotient:
		if (rval == (AWKNUM) 0)
			fatal("division by zero attempted in /=");
#ifdef _CRAY
		/* special case for integer division, put in for Cray */
		ltemp = rval;
		if (ltemp == 0) {
			*lhs = make_number(lval / rval);
			break;
		}
		ltemp = (long) lval / ltemp;
		if (ltemp * lval == rval)
			*lhs = make_number((AWKNUM) ltemp);
		else
#endif	/* _CRAY */
			*lhs = make_number(lval / rval);
		break;

	case Node_assign_mod:
		if (rval == (AWKNUM) 0)
			fatal("division by zero attempted in %%=");
#ifdef HAVE_FMOD
		*lhs = make_number(fmod(lval, rval));
#else	/* ! HAVE_FMOD */
		(void) modf(lval / rval, &t1);
		t2 = lval - rval * t1;
		*lhs = make_number(t2);
#endif	/* ! HAVE_FMOD */
		break;

	case Node_assign_plus:
		*lhs = make_number(lval + rval);
		break;

	case Node_assign_minus:
		*lhs = make_number(lval - rval);
		break;
	default:
		cant_happen();
	}
	tree->lnode->flags |= SCALAR;
	if (after_assign)
		(*after_assign)();
	return *lhs;
}

static struct fcall {
	char *fname;
	unsigned long count;
	NODE *arglist;
	NODE **prevstack;
	NODE **stack;
} *fcall_list = NULL;

static long fcall_list_size = 0;
static long curfcall = -1;

/* in_function --- return true/false if we need to unwind awk functions */

static int
in_function()
{
	return (curfcall >= 0);
}

/* pop_fcall --- pop off a single function call */

static void
pop_fcall()
{
	NODE *n, **sp, *arg, *argp;
	int count;
	struct fcall *f;

	assert(curfcall >= 0);
	f = & fcall_list[curfcall];
	stack_ptr = f->prevstack;

	/*
	 * here, we pop each parameter and check whether
	 * it was an array.  If so, and if the arg. passed in was
	 * a simple variable, then the value should be copied back.
	 * This achieves "call-by-reference" for arrays.
	 */
	sp = f->stack;
	count = f->count;

	for (argp = f->arglist; count > 0 && argp != NULL; argp = argp->rnode) {
		arg = argp->lnode;
		if (arg->type == Node_param_list)
			arg = stack_ptr[arg->param_cnt];
		n = *sp++;
		if ((arg->type == Node_var || arg->type == Node_var_array)
		    && n->type == Node_var_array) {
			/* should we free arg->var_value ? */
			arg->var_array = n->var_array;
			arg->type = Node_var_array;
			arg->array_size = n->array_size;
			arg->table_size = n->table_size;
			arg->flags = n->flags;
		}
		/* n->lnode overlays the array size, don't unref it if array */
		if (n->type != Node_var_array)
			unref(n->lnode);
		freenode(n);
		count--;
	}
	while (count-- > 0) {
		n = *sp++;
		/* if n is a local array, all the elements should be freed */
		if (n->type == Node_var_array)
			assoc_clear(n);
		unref(n->lnode);
		freenode(n);
	}
	if (f->stack)
		free((char *) f->stack);
	memset(f, '\0', sizeof(struct fcall));
	curfcall--;
}

/* pop_fcall_stack --- pop off all function args, don't leak memory */

static void
pop_fcall_stack()
{
	while (curfcall >= 0)
		pop_fcall();
}

/* push_args --- push function arguments onto the stack */

static void
push_args(count, arglist, oldstack, func_name)
int count;
NODE *arglist;
NODE **oldstack;
char *func_name;
{
	struct fcall *f;
	NODE *arg, *argp, *r, **sp, *n;

	if (fcall_list_size == 0) {	/* first time */
		emalloc(fcall_list, struct fcall *, 10 * sizeof(struct fcall),
			"push_args");
		fcall_list_size = 10;
	}

	if (++curfcall >= fcall_list_size) {
		fcall_list_size *= 2;
		erealloc(fcall_list, struct fcall *,
			fcall_list_size * sizeof(struct fcall), "push_args");
	}
	f = & fcall_list[curfcall];
	memset(f, '\0', sizeof(struct fcall));

	if (count > 0)
		emalloc(f->stack, NODE **, count*sizeof(NODE *), "func_call");
	f->count = count;
	f->fname = func_name;	/* not used, for debugging, just in case */
	f->arglist = arglist;
	f->prevstack = oldstack;

	sp = f->stack;

	/* for each calling arg. add NODE * on stack */
	for (argp = arglist; count > 0 && argp != NULL; argp = argp->rnode) {
		arg = argp->lnode;
		getnode(r);
		r->type = Node_var;

		/* call by reference for arrays; see below also */
		if (arg->type == Node_param_list)
			arg = f->prevstack[arg->param_cnt];
		if (arg->type == Node_var_array)
			*r = *arg;
		else {
			n = tree_eval(arg);
			r->lnode = dupnode(n);
			r->rnode = (NODE *) NULL;
  			if ((n->flags & SCALAR) != 0)
	  			r->flags |= SCALAR;
			free_temp(n);
  		}
		*sp++ = r;
		count--;
	}
	if (argp != NULL)	/* left over calling args. */
		warning(
		    "function `%s' called with more arguments than declared",
		    func_name);

	/* add remaining params. on stack with null value */
	while (count-- > 0) {
		getnode(r);
		r->type = Node_var;
		r->lnode = Nnull_string;
		r->flags &= ~SCALAR;
		r->rnode = (NODE *) NULL;
		*sp++ = r;
	}

	/*
	 * We have to reassign f. Why, you may ask?  It is possible that
	 * other functions were called during the course of tree_eval()-ing
	 * the arguments to this function. As a result of that, fcall_list
	 * may have been realloc()'ed, with the result that f is now
	 * pointing into free()'d space.  This was a nasty one to track down.
	 */
	f = & fcall_list[curfcall];

	stack_ptr = f->stack;
}

/* func_call --- call a function, call by reference for arrays */

NODE **stack_ptr;

static NODE *
func_call(name, arg_list)
NODE *name;		/* name is a Node_val giving function name */
NODE *arg_list;		/* Node_expression_list of calling args. */
{
	register NODE *r;
	NODE *f;
	jmp_buf volatile func_tag_stack;
	jmp_buf volatile loop_tag_stack;
	int volatile save_loop_tag_valid = FALSE;
	NODE *save_ret_node;
	extern NODE *ret_node;

	/* retrieve function definition node */
	f = lookup(name->stptr);
	if (f == NULL || f->type != Node_func)
		fatal("function `%s' not defined", name->stptr);
#ifdef FUNC_TRACE
	fprintf(stderr, "function %s called\n", name->stptr);
#endif
	push_args(f->lnode->param_cnt, arg_list, stack_ptr, name->stptr);

	/*
	 * Execute function body, saving context, as a return statement
	 * will longjmp back here.
	 *
	 * Have to save and restore the loop_tag stuff so that a return
	 * inside a loop in a function body doesn't scrog any loops going
	 * on in the main program.  We save the necessary info in variables
	 * local to this function so that function nesting works OK.
	 * We also only bother to save the loop stuff if we're in a loop
	 * when the function is called.
	 */
	if (loop_tag_valid) {
		int junk = 0;

		save_loop_tag_valid = (volatile int) loop_tag_valid;
		PUSH_BINDING(loop_tag_stack, loop_tag, junk);
		loop_tag_valid = FALSE;
	}
	PUSH_BINDING(func_tag_stack, func_tag, func_tag_valid);
	save_ret_node = ret_node;
	ret_node = Nnull_string;	/* default return value */
	if (setjmp(func_tag) == 0)
		(void) interpret(f->rnode);

	r = ret_node;
	ret_node = (NODE *) save_ret_node;
	RESTORE_BINDING(func_tag_stack, func_tag, func_tag_valid);
	pop_fcall();

	/* Restore the loop_tag stuff if necessary. */
	if (save_loop_tag_valid) {
		int junk = 0;

		loop_tag_valid = (int) save_loop_tag_valid;
		RESTORE_BINDING(loop_tag_stack, loop_tag, junk);
	}

	if ((r->flags & PERM) == 0)
		r->flags |= TEMP;
	return r;
}

/*
 * r_get_lhs:
 * This returns a POINTER to a node pointer. get_lhs(ptr) is the current
 * value of the var, or where to store the var's new value 
 *
 * For the special variables, don't unref their current value if it's
 * the same as the internal copy; perhaps the current one is used in
 * a concatenation or some other expression somewhere higher up in the
 * call chain.  Ouch.
 */

NODE **
r_get_lhs(ptr, assign)
register NODE *ptr;
Func_ptr *assign;
{
	register NODE **aptr = NULL;
	register NODE *n;

	if (assign)
		*assign = NULL;	/* for safety */
	if (ptr->type == Node_param_list) {
		if ((ptr->flags & FUNC) != 0)
			fatal("can't use function name `%s' as variable or array", ptr->vname);
		ptr = stack_ptr[ptr->param_cnt];
	}

	switch (ptr->type) {
	case Node_var_array:
		fatal("attempt to use array `%s' in a scalar context",
			ptr->vname);

	case Node_var:
		aptr = &(ptr->var_value);
#ifdef DEBUG
		if (ptr->var_value->stref <= 0)
			cant_happen();
#endif
		break;

	case Node_FIELDWIDTHS:
		aptr = &(FIELDWIDTHS_node->var_value);
		if (assign != NULL)
			*assign = set_FIELDWIDTHS;
		break;

	case Node_RS:
		aptr = &(RS_node->var_value);
		if (assign != NULL)
			*assign = set_RS;
		break;

	case Node_FS:
		aptr = &(FS_node->var_value);
		if (assign != NULL)
			*assign = set_FS;
		break;

	case Node_FNR:
		if (FNR_node->var_value->numbr != FNR) {
			unref(FNR_node->var_value);
			FNR_node->var_value = make_number((AWKNUM) FNR);
		}
		aptr = &(FNR_node->var_value);
		if (assign != NULL)
			*assign = set_FNR;
		break;

	case Node_NR:
		if (NR_node->var_value->numbr != NR) {
			unref(NR_node->var_value);
			NR_node->var_value = make_number((AWKNUM) NR);
		}
		aptr = &(NR_node->var_value);
		if (assign != NULL)
			*assign = set_NR;
		break;

	case Node_NF:
		if (NF == -1 || NF_node->var_value->numbr != NF) {
			if (NF == -1)
				(void) get_field(HUGE-1, assign); /* parse record */
			unref(NF_node->var_value);
			NF_node->var_value = make_number((AWKNUM) NF);
		}
		aptr = &(NF_node->var_value);
		if (assign != NULL)
			*assign = set_NF;
		break;

	case Node_IGNORECASE:
		aptr = &(IGNORECASE_node->var_value);
		if (assign != NULL)
			*assign = set_IGNORECASE;
		break;

	case Node_OFMT:
		aptr = &(OFMT_node->var_value);
		if (assign != NULL)
			*assign = set_OFMT;
		break;

	case Node_CONVFMT:
		aptr = &(CONVFMT_node->var_value);
		if (assign != NULL)
			*assign = set_CONVFMT;
		break;

	case Node_ORS:
		aptr = &(ORS_node->var_value);
		if (assign != NULL)
			*assign = set_ORS;
		break;

	case Node_OFS:
		aptr = &(OFS_node->var_value);
		if (assign != NULL)
			*assign = set_OFS;
		break;

	case Node_param_list:
		aptr = &(stack_ptr[ptr->param_cnt]->var_value);
		break;

	case Node_field_spec:
		{
		int field_num;

		n = tree_eval(ptr->lnode);
		field_num = (int) force_number(n);
		free_temp(n);
		if (field_num < 0)
			fatal("attempt to access field %d", field_num);
		if (field_num == 0 && field0_valid) {	/* short circuit */
			aptr = &fields_arr[0];
			if (assign != NULL)
				*assign = reset_record;
			break;
		}
		aptr = get_field(field_num, assign);
		break;
		}
	case Node_subscript:
		n = ptr->lnode;
		if (n->type == Node_param_list) {
			int i = n->param_cnt + 1;

			n = stack_ptr[n->param_cnt];
			if ((n->flags & SCALAR) != 0)
				fatal("attempt to use scalar parameter %d as an array", i);
		} else if (n->type == Node_func) {
			fatal("attempt to use function `%s' as array",
				n->lnode->param);
		}
		aptr = assoc_lookup(n, concat_exp(ptr->rnode));
		break;

	case Node_func:
		fatal("`%s' is a function, assignment is not allowed",
			ptr->lnode->param);

	case Node_builtin:
		fatal("assignment is not allowed to result of builtin function");
	default:
		cant_happen();
	}
	return aptr;
}

/* match_op --- do ~ and !~ */

static NODE *
match_op(tree)
register NODE *tree;
{
	register NODE *t1;
	register Regexp *rp;
	int i;
	int match = TRUE;
	int kludge_need_start = FALSE;	/* FIXME: --- see below */

	if (tree->type == Node_nomatch)
		match = FALSE;
	if (tree->type == Node_regex)
		t1 = *get_field(0, (Func_ptr *) 0);
	else {
		t1 = force_string(tree_eval(tree->lnode));
		tree = tree->rnode;
	}
	rp = re_update(tree);
	/*
	 * FIXME:
	 *
	 * Any place where research() is called with a last parameter of
	 * FALSE, we need to use the avoid_dfa test. This is the only place
	 * at the moment.
	 *
	 * A new or improved dfa that distinguishes beginning/end of
	 * string from beginning/end of line will allow us to get rid of
	 * this temporary hack.
	 *
	 * The avoid_dfa() function is in re.c; it is not very smart.
	 */
	if (avoid_dfa(tree, t1->stptr, t1->stlen))
		kludge_need_start = TRUE;
	i = research(rp, t1->stptr, 0, t1->stlen, kludge_need_start);
	i = (i == -1) ^ (match == TRUE);
	free_temp(t1);
	return tmp_number((AWKNUM) i);
}

/* set_IGNORECASE --- update IGNORECASE as appropriate */

void
set_IGNORECASE()
{
	static int warned = FALSE;

	if ((do_lint || do_traditional) && ! warned) {
		warned = TRUE;
		warning("IGNORECASE not supported in compatibility mode");
	}
	if (do_traditional)
		IGNORECASE = FALSE;
	else if ((IGNORECASE_node->var_value->flags & (STRING|STR)) != 0) {
		if ((IGNORECASE_node->var_value->flags & MAYBE_NUM) == 0)
			IGNORECASE = (force_string(IGNORECASE_node->var_value)->stlen > 0);
		else
			IGNORECASE = (force_number(IGNORECASE_node->var_value) != 0.0);
	} else if ((IGNORECASE_node->var_value->flags & (NUM|NUMBER)) != 0)
		IGNORECASE = (force_number(IGNORECASE_node->var_value) != 0.0);
	else
		IGNORECASE = FALSE;		/* shouldn't happen */
	set_FS_if_not_FIELDWIDTHS();
}

/* set_OFS --- update OFS related variables when OFS assigned to */

void
set_OFS()
{
	OFS = force_string(OFS_node->var_value)->stptr;
	OFSlen = OFS_node->var_value->stlen;
	OFS[OFSlen] = '\0';
}

/* set_ORS --- update ORS related variables when ORS assigned to */

void
set_ORS()
{
	ORS = force_string(ORS_node->var_value)->stptr;
	ORSlen = ORS_node->var_value->stlen;
	ORS[ORSlen] = '\0';
}

/* fmt_ok --- is the conversion format a valid one? */

NODE **fmt_list = NULL;
static int fmt_ok P((NODE *n));
static int fmt_index P((NODE *n));

static int
fmt_ok(n)
NODE *n;
{
	NODE *tmp = force_string(n);
	char *p = tmp->stptr;

	if (*p++ != '%')
		return 0;
	while (*p && strchr(" +-#", *p) != NULL)	/* flags */
		p++;
	while (*p && isdigit(*p))	/* width - %*.*g is NOT allowed */
		p++;
	if (*p == '\0' || (*p != '.' && ! isdigit(*p)))
		return 0;
	if (*p == '.')
		p++;
	while (*p && isdigit(*p))	/* precision */
		p++;
	if (*p == '\0' || strchr("efgEG", *p) == NULL)
		return 0;
	if (*++p != '\0')
		return 0;
	return 1;
}

/* fmt_index --- track values of OFMT and CONVFMT to keep semantics correct */

static int
fmt_index(n)
NODE *n;
{
	register int ix = 0;
	static int fmt_num = 4;
	static int fmt_hiwater = 0;

	if (fmt_list == NULL)
		emalloc(fmt_list, NODE **, fmt_num*sizeof(*fmt_list), "fmt_index");
	(void) force_string(n);
	while (ix < fmt_hiwater) {
		if (cmp_nodes(fmt_list[ix], n) == 0)
			return ix;
		ix++;
	}
	/* not found */
	n->stptr[n->stlen] = '\0';
	if (do_lint && ! fmt_ok(n))
		warning("bad %sFMT specification",
			    n == CONVFMT_node->var_value ? "CONV"
			  : n == OFMT_node->var_value ? "O"
			  : "");

	if (fmt_hiwater >= fmt_num) {
		fmt_num *= 2;
		emalloc(fmt_list, NODE **, fmt_num, "fmt_index");
	}
	fmt_list[fmt_hiwater] = dupnode(n);
	return fmt_hiwater++;
}

/* set_OFMT --- track OFMT correctly */

void
set_OFMT()
{
	OFMTidx = fmt_index(OFMT_node->var_value);
	OFMT = fmt_list[OFMTidx]->stptr;
}

/* set_CONVFMT --- track CONVFMT correctly */

void
set_CONVFMT()
{
	CONVFMTidx = fmt_index(CONVFMT_node->var_value);
	CONVFMT = fmt_list[CONVFMTidx]->stptr;
}
