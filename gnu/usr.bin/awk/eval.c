/*
 * eval.c - gawk parse tree interpreter 
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991, 1992 the Free Software Foundation, Inc.
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

#include "awk.h"

extern double pow P((double x, double y));
extern double modf P((double x, double *yp));
extern double fmod P((double x, double y));

static int eval_condition P((NODE *tree));
static NODE *op_assign P((NODE *tree));
static NODE *func_call P((NODE *name, NODE *arg_list));
static NODE *match_op P((NODE *tree));

NODE *_t;		/* used as a temporary in macros */
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
#define PUSH_BINDING(stack, x, val) (memcpy ((char *)(stack), (char *)(x), sizeof (jmp_buf)), val++)
#define RESTORE_BINDING(stack, x, val) (memcpy ((char *)(x), (char *)(stack), sizeof (jmp_buf)), val--)

static jmp_buf loop_tag;	/* always the current binding */
static int loop_tag_valid = 0;	/* nonzero when loop_tag valid */
static int func_tag_valid = 0;
static jmp_buf func_tag;
extern int exiting, exit_val;

/*
 * This table is used by the regexp routines to do case independant
 * matching. Basically, every ascii character maps to itself, except
 * uppercase letters map to lower case ones. This table has 256
 * entries, which may be overkill. Note also that if the system this
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
};
#else
#include "You lose. You will need a translation table for your character set."
#endif

/*
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
	int volatile traverse = 1;	/* True => loop thru tree (Node_rule_list) */

	if (tree == NULL)
		return 1;
	sourceline = tree->source_line;
	source = tree->source_file;
	switch (tree->type) {
	case Node_rule_node:
		traverse = 0;   /* False => one for-loop iteration only */
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
			if (!traverse)          /* case Node_rule_node */
				break;          /* don't loop */
		}
		break;

	case Node_statement_list:
		for (t = tree; t != NULL; t = t->rnode)
			(void) interpret(t->lnode);
		break;

	case Node_K_if:
		if (eval_condition(tree->lnode)) {
			(void) interpret(tree->rnode->lnode);
		} else {
			(void) interpret(tree->rnode->rnode);
		}
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
		if (loop_tag_valid == 0)
			fatal("unexpected break");
		longjmp(loop_tag, TAG_BREAK);
		break;

	case Node_K_continue:
		if (loop_tag_valid == 0) {
			/*
			 * AT&T nawk treats continue outside of loops like
			 * next.  Allow it if not posix, and complain if
			 * lint.
			 */
			static int warned = 0;

			if (do_lint && ! warned) {
				warning("use of `continue' outside of loop is not portable");
				warned = 1;
			}
			if (do_posix)
				fatal("use of `continue' outside of loop is not allowed");
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
		longjmp(rule_tag, TAG_CONTINUE);
		break;

	case Node_K_nextfile:
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
		exiting = 1;
		if (tree) {
			t = tree_eval(tree->lnode);
			exit_val = (int) force_number(t);
		}
		free_temp(t);
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

/* evaluate a subtree */

NODE *
r_tree_eval(tree)
register NODE *tree;
{
	register NODE *r, *t1, *t2;	/* return value & temporary subtrees */
	register NODE **lhs;
	register int di;
	AWKNUM x, x1, x2;
	long lx;
#ifdef CRAY
	long lx2;
#endif

#ifdef DEBUG
	if (tree == NULL)
		return Nnull_string;
	if (tree->type == Node_val) {
		if (tree->stref <= 0) cant_happen();
		return tree;
	}
	if (tree->type == Node_var) {
		if (tree->var_value->stref <= 0) cant_happen();
		return tree->var_value;
	}
	if (tree->type == Node_param_list) {
		if (stack_ptr[tree->param_cnt] == NULL)
			return Nnull_string;
		else
			return stack_ptr[tree->param_cnt]->var_value;
	}
#endif
	switch (tree->type) {
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
		return ((*tree->proc) (tree->subnode));

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
		lhs = get_lhs(tree, (Func_ptr *)0);
		return *lhs;

	case Node_var_array:
		fatal("attempt to use an array in a scalar context");

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

		r = tree_eval(tree->rnode);
		lhs = get_lhs(tree->lnode, &after_assign);
		if (r != *lhs) {
			NODE *save;

			save = *lhs;
			*lhs = dupnode(r);
			unref(save);
		}
		free_temp(r);
		if (after_assign)
			(*after_assign)();
		return *lhs;
		}

	case Node_concat:
		{
#define	STACKSIZE	10
		NODE *stack[STACKSIZE];
		register NODE **sp;
		register int len;
		char *str;
		register char *dest;

		sp = stack;
		len = 0;
		while (tree->type == Node_concat) {
			*sp = force_string(tree_eval(tree->lnode));
			tree = tree->rnode;
			len += (*sp)->stlen;
			if (++sp == &stack[STACKSIZE-2]) /* one more and NULL */
				break;
		}
		*sp = force_string(tree_eval(tree));
		len += (*sp)->stlen;
		*++sp = NULL;
		emalloc(str, char *, len+2, "tree_eval");
		dest = str;
		sp = stack;
		while (*sp) {
			memcpy(dest, (*sp)->stptr, (*sp)->stlen);
			dest += (*sp)->stlen;
			free_temp(*sp);
			sp++;
		}
		r = make_str_node(str, len, ALREADY_MALLOCED);
		r->flags |= TEMP;
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
		/*
		 * special case for integer division, put in for Cray
		 */
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
#ifndef FMOD_MISSING
		return tmp_number(fmod (x1, x2));
#else
		(void) modf(x1 / x2, &x);
		return tmp_number(x1 - x * x2);
#endif

	case Node_plus:
		return tmp_number(x1 + x2);

	case Node_minus:
		return tmp_number(x1 - x2);

	case Node_var_array:
		fatal("attempt to use an array in a scalar context");

	default:
		fatal("illegal type (%d) in tree_eval", tree->type);
	}
	return 0;
}

/* Is TREE true or false?  Returns 0==false, non-zero==true */
static int
eval_condition(tree)
register NODE *tree;
{
	register NODE *t1;
	register int ret;

	if (tree == NULL)	/* Null trees are the easiest kinds */
		return 1;
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
		if (!tree->triggered)
			if (!eval_condition(tree->condpair->lnode))
				return 0;
			else
				tree->triggered = 1;
		/* Else we are triggered */
		if (eval_condition(tree->condpair->rnode))
			tree->triggered = 0;
		return 1;
	}

	/*
	 * Could just be J.random expression. in which case, null and 0 are
	 * false, anything else is true 
	 */

	t1 = tree_eval(tree);
	if (t1->flags & MAYBE_NUM)
		(void) force_number(t1);
	if (t1->flags & NUMBER)
		ret = t1->numbr != 0.0;
	else
		ret = t1->stlen != 0;
	free_temp(t1);
	return ret;
}

/*
 * compare two nodes, returning negative, 0, positive
 */
int
cmp_nodes(t1, t2)
register NODE *t1, *t2;
{
	register int ret;
	register int len1, len2;

	if (t1 == t2)
		return 0;
	if (t1->flags & MAYBE_NUM)
		(void) force_number(t1);
	if (t2->flags & MAYBE_NUM)
		(void) force_number(t2);
	if ((t1->flags & NUMBER) && (t2->flags & NUMBER)) {
		if (t1->numbr == t2->numbr) return 0;
		else if (t1->numbr - t2->numbr < 0)  return -1;
		else return 1;
	}
	(void) force_string(t1);
	(void) force_string(t2);
	len1 = t1->stlen;
	len2 = t2->stlen;
	if (len1 == 0 || len2 == 0)
		return len1 - len2;
	ret = memcmp(t1->stptr, t2->stptr, len1 <= len2 ? len1 : len2);
	return ret == 0 ? len1-len2 : ret;
}

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
		if (after_assign)
			(*after_assign)();
		return *lhs;

	case Node_postincrement:
	case Node_postdecrement:
		unref(*lhs);
		*lhs = make_number(lval +
			       (tree->type == Node_postincrement ? 1.0 : -1.0));
		if (after_assign)
			(*after_assign)();
		return tmp_number(lval);
	default:
		break;	/* handled below */
	}

	tmp = tree_eval(tree->rnode);
	rval = force_number(tmp);
	free_temp(tmp);
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
		/*
		 * special case for integer division, put in for Cray
		 */
		ltemp = rval;
		if (ltemp == 0) {
			*lhs = make_number(lval / rval);
			break;
		}
		ltemp = (long) lval / ltemp;
		if (ltemp * lval == rval)
			*lhs = make_number((AWKNUM) ltemp);
		else
#endif
			*lhs = make_number(lval / rval);
		break;

	case Node_assign_mod:
		if (rval == (AWKNUM) 0)
			fatal("division by zero attempted in %=");
#ifndef FMOD_MISSING
		*lhs = make_number(fmod(lval, rval));
#else
		(void) modf(lval / rval, &t1);
		t2 = lval - rval * t1;
		*lhs = make_number(t2);
#endif
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
	if (after_assign)
		(*after_assign)();
	return *lhs;
}

NODE **stack_ptr;

static NODE *
func_call(name, arg_list)
NODE *name;		/* name is a Node_val giving function name */
NODE *arg_list;		/* Node_expression_list of calling args. */
{
	register NODE *arg, *argp, *r;
	NODE *n, *f;
	jmp_buf volatile func_tag_stack;
	jmp_buf volatile loop_tag_stack;
	int volatile save_loop_tag_valid = 0;
	NODE **volatile save_stack, *save_ret_node;
	NODE **volatile local_stack = NULL, **sp;
	int count;
	extern NODE *ret_node;

	/*
	 * retrieve function definition node
	 */
	f = lookup(name->stptr);
	if (!f || f->type != Node_func)
		fatal("function `%s' not defined", name->stptr);
#ifdef FUNC_TRACE
	fprintf(stderr, "function %s called\n", name->stptr);
#endif
	count = f->lnode->param_cnt;
	if (count)
		emalloc(local_stack, NODE **, count*sizeof(NODE *), "func_call");
	sp = local_stack;

	/*
	 * for each calling arg. add NODE * on stack
	 */
	for (argp = arg_list; count && argp != NULL; argp = argp->rnode) {
		arg = argp->lnode;
		getnode(r);
		r->type = Node_var;
		/*
		 * call by reference for arrays; see below also
		 */
		if (arg->type == Node_param_list)
			arg = stack_ptr[arg->param_cnt];
		if (arg->type == Node_var_array)
			*r = *arg;
		else {
			n = tree_eval(arg);
			r->lnode = dupnode(n);
			r->rnode = (NODE *) NULL;
			free_temp(n);
  		}
		*sp++ = r;
		count--;
	}
	if (argp != NULL)	/* left over calling args. */
		warning(
		    "function `%s' called with more arguments than declared",
		    name->stptr);
	/*
	 * add remaining params. on stack with null value
	 */
	while (count-- > 0) {
		getnode(r);
		r->type = Node_var;
		r->lnode = Nnull_string;
		r->rnode = (NODE *) NULL;
		*sp++ = r;
	}

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
		loop_tag_valid = 0;
	}
	save_stack = stack_ptr;
	stack_ptr = local_stack;
	PUSH_BINDING(func_tag_stack, func_tag, func_tag_valid);
	save_ret_node = ret_node;
	ret_node = Nnull_string;	/* default return value */
	if (setjmp(func_tag) == 0)
		(void) interpret(f->rnode);

	r = ret_node;
	ret_node = (NODE *) save_ret_node;
	RESTORE_BINDING(func_tag_stack, func_tag, func_tag_valid);
	stack_ptr = (NODE **) save_stack;

	/*
	 * here, we pop each parameter and check whether
	 * it was an array.  If so, and if the arg. passed in was
	 * a simple variable, then the value should be copied back.
	 * This achieves "call-by-reference" for arrays.
	 */
	sp = local_stack;
	count = f->lnode->param_cnt;
	for (argp = arg_list; count > 0 && argp != NULL; argp = argp->rnode) {
		arg = argp->lnode;
		if (arg->type == Node_param_list)
			arg = stack_ptr[arg->param_cnt];
		n = *sp++;
		if (arg->type == Node_var && n->type == Node_var_array) {
			/* should we free arg->var_value ? */
			arg->var_array = n->var_array;
			arg->type = Node_var_array;
		}
		unref(n->lnode);
		freenode(n);
		count--;
	}
	while (count-- > 0) {
		n = *sp++;
		/* if n is an (local) array, all the elements should be freed */
		if (n->type == Node_var_array) {
			assoc_clear(n);
			free(n->var_array);
		}
		unref(n->lnode);
		freenode(n);
	}
	if (local_stack)
		free((char *) local_stack);

	/* Restore the loop_tag stuff if necessary. */
	if (save_loop_tag_valid) {
		int junk = 0;

		loop_tag_valid = (int) save_loop_tag_valid;
		RESTORE_BINDING(loop_tag_stack, loop_tag, junk);
	}

	if (!(r->flags & PERM))
		r->flags |= TEMP;
	return r;
}

/*
 * This returns a POINTER to a node pointer. get_lhs(ptr) is the current
 * value of the var, or where to store the var's new value 
 */

NODE **
get_lhs(ptr, assign)
register NODE *ptr;
Func_ptr *assign;
{
	register NODE **aptr = NULL;
	register NODE *n;

	switch (ptr->type) {
	case Node_var_array:
		fatal("attempt to use an array in a scalar context");
	case Node_var:
		aptr = &(ptr->var_value);
#ifdef DEBUG
		if (ptr->var_value->stref <= 0)
			cant_happen();
#endif
		break;

	case Node_FIELDWIDTHS:
		aptr = &(FIELDWIDTHS_node->var_value);
		if (assign)
			*assign = set_FIELDWIDTHS;
		break;

	case Node_RS:
		aptr = &(RS_node->var_value);
		if (assign)
			*assign = set_RS;
		break;

	case Node_FS:
		aptr = &(FS_node->var_value);
		if (assign)
			*assign = set_FS;
		break;

	case Node_FNR:
		unref(FNR_node->var_value);
		FNR_node->var_value = make_number((AWKNUM) FNR);
		aptr = &(FNR_node->var_value);
		if (assign)
			*assign = set_FNR;
		break;

	case Node_NR:
		unref(NR_node->var_value);
		NR_node->var_value = make_number((AWKNUM) NR);
		aptr = &(NR_node->var_value);
		if (assign)
			*assign = set_NR;
		break;

	case Node_NF:
		if (NF == -1)
			(void) get_field(HUGE-1, assign); /* parse record */
		unref(NF_node->var_value);
		NF_node->var_value = make_number((AWKNUM) NF);
		aptr = &(NF_node->var_value);
		if (assign)
			*assign = set_NF;
		break;

	case Node_IGNORECASE:
		unref(IGNORECASE_node->var_value);
		IGNORECASE_node->var_value = make_number((AWKNUM) IGNORECASE);
		aptr = &(IGNORECASE_node->var_value);
		if (assign)
			*assign = set_IGNORECASE;
		break;

	case Node_OFMT:
		aptr = &(OFMT_node->var_value);
		if (assign)
			*assign = set_OFMT;
		break;

	case Node_CONVFMT:
		aptr = &(CONVFMT_node->var_value);
		if (assign)
			*assign = set_CONVFMT;
		break;

	case Node_ORS:
		aptr = &(ORS_node->var_value);
		if (assign)
			*assign = set_ORS;
		break;

	case Node_OFS:
		aptr = &(OFS_node->var_value);
		if (assign)
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
			if (assign)
				*assign = reset_record;
			break;
		}
		aptr = get_field(field_num, assign);
		break;
		}
	case Node_subscript:
		n = ptr->lnode;
		if (n->type == Node_param_list)
			n = stack_ptr[n->param_cnt];
		aptr = assoc_lookup(n, concat_exp(ptr->rnode));
		break;

	case Node_func:
		fatal ("`%s' is a function, assignment is not allowed",
			ptr->lnode->param);
	default:
		cant_happen();
	}
	return aptr;
}

static NODE *
match_op(tree)
register NODE *tree;
{
	register NODE *t1;
	register Regexp *rp;
	int i;
	int match = 1;

	if (tree->type == Node_nomatch)
		match = 0;
	if (tree->type == Node_regex)
		t1 = *get_field(0, (Func_ptr *) 0);
	else {
		t1 = force_string(tree_eval(tree->lnode));
		tree = tree->rnode;
	}
	rp = re_update(tree);
	i = research(rp, t1->stptr, 0, t1->stlen, 0);
	i = (i == -1) ^ (match == 1);
	free_temp(t1);
	return tmp_number((AWKNUM) i);
}

void
set_IGNORECASE()
{
	static int warned = 0;

	if ((do_lint || do_unix) && ! warned) {
		warned = 1;
		warning("IGNORECASE not supported in compatibility mode");
	}
	IGNORECASE = (force_number(IGNORECASE_node->var_value) != 0.0);
	set_FS();
}

void
set_OFS()
{
	OFS = force_string(OFS_node->var_value)->stptr;
	OFSlen = OFS_node->var_value->stlen;
	OFS[OFSlen] = '\0';
}

void
set_ORS()
{
	ORS = force_string(ORS_node->var_value)->stptr;
	ORSlen = ORS_node->var_value->stlen;
	ORS[ORSlen] = '\0';
}

static NODE **fmt_list = NULL;
static int fmt_ok P((NODE *n));
static int fmt_index P((NODE *n));

static int
fmt_ok(n)
NODE *n;
{
	/* to be done later */
	return 1;
}

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
	if (!fmt_ok(n))
		warning("bad FMT specification");
	if (fmt_hiwater >= fmt_num) {
		fmt_num *= 2;
		emalloc(fmt_list, NODE **, fmt_num, "fmt_index");
	}
	fmt_list[fmt_hiwater] = dupnode(n);
	return fmt_hiwater++;
}

void
set_OFMT()
{
	OFMTidx = fmt_index(OFMT_node->var_value);
	OFMT = fmt_list[OFMTidx]->stptr;
}

void
set_CONVFMT()
{
	CONVFMTidx = fmt_index(CONVFMT_node->var_value);
	CONVFMT = fmt_list[CONVFMTidx]->stptr;
}
