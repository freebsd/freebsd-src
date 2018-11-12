/*	$NetBSD: func.c,v 1.22 2005/09/24 15:30:35 perry Exp $	*/

/*
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(__RCSID) && !defined(lint)
__RCSID("$NetBSD: func.c,v 1.16 2002/01/03 04:25:15 thorpej Exp $");
#endif
__FBSDID("$FreeBSD$");

#include <stdlib.h>
#include <string.h>

#include "lint1.h"
#include "cgram.h"

/*
 * Contains a pointer to the symbol table entry of the current function
 * definition.
 */
sym_t	*funcsym;

/* Is set as long as a statement can be reached. Must be set at level 0. */
int	reached = 1;

/*
 * Is set as long as NOTREACHED is in effect.
 * Is reset everywhere where reached can become 0.
 */
int	rchflg;

/*
 * In conjunction with reached controls printing of "fallthrough on ..."
 * warnings.
 * Reset by each statement and set by FALLTHROUGH, switch (switch1())
 * and case (label()).
 *
 * Control statements if, for, while and switch do not reset ftflg because
 * this must be done by the controlled statement. At least for if this is
 * important because ** FALLTHROUGH ** after "if (expr) stmnt" is evaluated
 * before the following token, which causes reduction of above, is read.
 * This means that ** FALLTHROUGH ** after "if ..." would always be ignored.
 */
int	ftflg;

/* Top element of stack for control statements */
cstk_t	*cstk;

/*
 * Number of arguments which will be checked for usage in following
 * function definition. -1 stands for all arguments.
 *
 * The position of the last ARGSUSED comment is stored in aupos.
 */
int	nargusg = -1;
pos_t	aupos;

/*
 * Number of arguments of the following function definition whose types
 * shall be checked by lint2. -1 stands for all arguments.
 *
 * The position of the last VARARGS comment is stored in vapos.
 */
int	nvararg = -1;
pos_t	vapos;

/*
 * Both prflstr and scflstrg contain the number of the argument which
 * shall be used to check the types of remaining arguments (for PRINTFLIKE
 * and SCANFLIKE).
 *
 * prflpos and scflpos are the positions of the last PRINTFLIKE or
 * SCANFLIKE comment.
 */
int	prflstrg = -1;
int	scflstrg = -1;
pos_t	prflpos;
pos_t	scflpos;

/*
 * Are both plibflg and llibflg set, prototypes are written as function
 * definitions to the output file.
 */
int	plibflg;

/*
 * Nonzero means that no warnings about constants in conditional
 * context are printed.
 */
int	ccflg;

/*
 * llibflg is set if a lint library shall be created. The effect of
 * llibflg is that all defined symbols are treated as used.
 * (The LINTLIBRARY comment also resets vflag.)
 */
int	llibflg;

/*
 * Nonzero if warnings are suppressed by a LINTED directive
 */
int	nowarn;

/*
 * Nonzero if bitfield type errors are suppressed by a BITFIELDTYPE
 * directive.
 */
int	bitfieldtype_ok;

/*
 * Nonzero if complaints about use of "long long" are suppressed in
 * the next statement or declaration.
 */
int	quadflg;

/*
 * Puts a new element at the top of the stack used for control statements.
 */
void
pushctrl(int env)
{
	cstk_t	*ci;

	if ((ci = calloc(1, sizeof (cstk_t))) == NULL)
		nomem();
	ci->c_env = env;
	ci->c_nxt = cstk;
	cstk = ci;
}

/*
 * Removes the top element of the stack used for control statements.
 */
void
popctrl(int env)
{
	cstk_t	*ci;
	clst_t	*cl;

	if (cstk == NULL || cstk->c_env != env)
		LERROR("popctrl()");

	cstk = (ci = cstk)->c_nxt;

	while ((cl = ci->c_clst) != NULL) {
		ci->c_clst = cl->cl_nxt;
		free(cl);
	}

	if (ci->c_swtype != NULL)
		free(ci->c_swtype);

	free(ci);
}

/*
 * Prints a warning if a statement cannot be reached.
 */
void
chkreach(void)
{
	if (!reached && !rchflg) {
		/* statement not reached */
		warning(193);
		reached = 1;
	}
}

/*
 * Called after a function declaration which introduces a function definition
 * and before an (optional) old style argument declaration list.
 *
 * Puts all symbols declared in the Prototype or in an old style argument
 * list back to the symbol table.
 *
 * Does the usual checking of storage class, type (return value),
 * redeclaration etc..
 */
void
funcdef(sym_t *fsym)
{
	int	n, warn;
	sym_t	*arg, *sym, *rdsym;

	funcsym = fsym;

	/*
	 * Put all symbols declared in the argument list back to the
	 * symbol table.
	 */
	for (sym = dcs->d_fpsyms; sym != NULL; sym = sym->s_dlnxt) {
		if (sym->s_blklev != -1) {
			if (sym->s_blklev != 1)
				LERROR("funcdef()");
			inssym(1, sym);
		}
	}

	/*
	 * In osfunc() we did not know whether it is an old style function
	 * definition or only an old style declaration, if there are no
	 * arguments inside the argument list ("f()").
	 */
	if (!fsym->s_type->t_proto && fsym->s_args == NULL)
		fsym->s_osdef = 1;

	chktyp(fsym);

	/*
	 * chktyp() checks for almost all possible errors, but not for
	 * incomplete return values (these are allowed in declarations)
	 */
	if (fsym->s_type->t_subt->t_tspec != VOID &&
	    incompl(fsym->s_type->t_subt)) {
		/* cannot return incomplete type */
		error(67);
	}

	fsym->s_def = DEF;

	if (fsym->s_scl == TYPEDEF) {
		fsym->s_scl = EXTERN;
		/* illegal storage class */
		error(8);
	}

	if (dcs->d_inline)
		fsym->s_inline = 1;

	/*
	 * Arguments in new style function declarations need a name.
	 * (void is already removed from the list of arguments)
	 */
	n = 1;
	for (arg = fsym->s_type->t_args; arg != NULL; arg = arg->s_nxt) {
		if (arg->s_scl == ABSTRACT) {
			if (arg->s_name != unnamed)
				LERROR("funcdef()");
			/* formal parameter lacks name: param #%d */
			error(59, n);
		} else {
			if (arg->s_name == unnamed)
				LERROR("funcdef()");
		}
		n++;
	}

	/*
	 * We must also remember the position. s_dpos is overwritten
	 * if this is an old style definition and we had already a
	 * prototype.
	 */
	STRUCT_ASSIGN(dcs->d_fdpos, fsym->s_dpos);

	if ((rdsym = dcs->d_rdcsym) != NULL) {

		if (!isredec(fsym, (warn = 0, &warn))) {

			/*
			 * Print nothing if the newly defined function
			 * is defined in old style. A better warning will
			 * be printed in cluparg().
			 */
			if (warn && !fsym->s_osdef) {
				/* redeclaration of %s */
				(*(sflag ? error : warning))(27, fsym->s_name);
				prevdecl(-1, rdsym);
			}

			/* copy usage information */
			cpuinfo(fsym, rdsym);

			/*
			 * If the old symbol was a prototype and the new
			 * one is none, overtake the position of the
			 * declaration of the prototype.
			 */
			if (fsym->s_osdef && rdsym->s_type->t_proto)
				STRUCT_ASSIGN(fsym->s_dpos, rdsym->s_dpos);

			/* complete the type */
			compltyp(fsym, rdsym);

			/* once a function is inline it remains inline */
			if (rdsym->s_inline)
				fsym->s_inline = 1;

		}

		/* remove the old symbol from the symbol table */
		rmsym(rdsym);

	}

	if (fsym->s_osdef && !fsym->s_type->t_proto) {
		if (sflag && hflag && strcmp(fsym->s_name, "main") != 0)
			/* function definition is not a prototype */
			warning(286);
	}

	if (dcs->d_notyp)
		/* return value is implicitly declared to be int */
		fsym->s_rimpl = 1;

	reached = 1;
}

/*
 * Called at the end of a function definition.
 */
void
funcend(void)
{
	sym_t	*arg;
	int	n;

	if (reached) {
		cstk->c_noretval = 1;
		if (funcsym->s_type->t_subt->t_tspec != VOID &&
		    !funcsym->s_rimpl) {
			/* func. %s falls off bottom without returning value */
			warning(217, funcsym->s_name);
		}
	}

	/*
	 * This warning is printed only if the return value was implicitly
	 * declared to be int. Otherwise the wrong return statement
	 * has already printed a warning.
	 */
	if (cstk->c_noretval && cstk->c_retval && funcsym->s_rimpl)
		/* function %s has return (e); and return; */
		warning(216, funcsym->s_name);

	/* Print warnings for unused arguments */
	arg = dcs->d_fargs;
	n = 0;
	while (arg != NULL && (nargusg == -1 || n < nargusg)) {
		chkusg1(dcs->d_asm, arg);
		arg = arg->s_nxt;
		n++;
	}
	nargusg = -1;

	/*
	 * write the information about the function definition to the
	 * output file
	 * inline functions explicitly declared extern are written as
	 * declarations only.
	 */
	if (dcs->d_scl == EXTERN && funcsym->s_inline) {
		outsym(funcsym, funcsym->s_scl, DECL);
	} else {
		outfdef(funcsym, &dcs->d_fdpos, cstk->c_retval,
			funcsym->s_osdef, dcs->d_fargs);
	}

	/*
	 * remove all symbols declared during argument declaration from
	 * the symbol table
	 */
	if (dcs->d_nxt != NULL || dcs->d_ctx != EXTERN)
		LERROR("funcend()");
	rmsyms(dcs->d_fpsyms);

	/* must be set on level 0 */
	reached = 1;
}

/*
 * Process a label.
 *
 * typ		type of the label (T_NAME, T_DEFAULT or T_CASE).
 * sym		symbol table entry of label if typ == T_NAME
 * tn		expression if typ == T_CASE
 */
void
label(int typ, sym_t *sym, tnode_t *tn)
{
	cstk_t	*ci;
	clst_t	*cl;
	val_t	*v;
	val_t	nv;
	tspec_t	t;

	switch (typ) {

	case T_NAME:
		if (sym->s_set) {
			/* label %s redefined */
			error(194, sym->s_name);
		} else {
			setsflg(sym);
		}
		break;

	case T_CASE:

		/* find the stack entry for the innermost switch statement */
		for (ci = cstk; ci != NULL && !ci->c_switch; ci = ci->c_nxt)
			continue;

		if (ci == NULL) {
			/* case not in switch */
			error(195);
			tn = NULL;
		} else if (tn != NULL && tn->tn_op != CON) {
			/* non-constant case expression */
			error(197);
			tn = NULL;
		} else if (tn != NULL && !isityp(tn->tn_type->t_tspec)) {
			/* non-integral case expression */
			error(198);
			tn = NULL;
		}

		if (tn != NULL) {

			if (ci->c_swtype == NULL)
				LERROR("label()");

			if (reached && !ftflg) {
				if (hflag)
					/* fallthrough on case statement */
					warning(220);
			}

			t = tn->tn_type->t_tspec;
			if (t == LONG || t == ULONG ||
			    t == QUAD || t == UQUAD) {
				if (tflag)
					/* case label must be of type ... */
					warning(203);
			}

			/*
			 * get the value of the expression and convert it
			 * to the type of the switch expression
			 */
			v = constant(tn, 1);
			(void) memset(&nv, 0, sizeof nv);
			cvtcon(CASE, 0, ci->c_swtype, &nv, v);
			free(v);

			/* look if we had this value already */
			for (cl = ci->c_clst; cl != NULL; cl = cl->cl_nxt) {
				if (cl->cl_val.v_quad == nv.v_quad)
					break;
			}
			if (cl != NULL && isutyp(nv.v_tspec)) {
				/* duplicate case in switch, %lu */
				error(200, (u_long)nv.v_quad);
			} else if (cl != NULL) {
				/* duplicate case in switch, %ld */
				error(199, (long)nv.v_quad);
			} else {
				/*
				 * append the value to the list of
				 * case values
				 */
				cl = xcalloc(1, sizeof (clst_t));
				STRUCT_ASSIGN(cl->cl_val, nv);
				cl->cl_nxt = ci->c_clst;
				ci->c_clst = cl;
			}
		}
		tfreeblk();
		break;

	case T_DEFAULT:

		/* find the stack entry for the innermost switch statement */
		for (ci = cstk; ci != NULL && !ci->c_switch; ci = ci->c_nxt)
			continue;

		if (ci == NULL) {
			/* default outside switch */
			error(201);
		} else if (ci->c_default) {
			/* duplicate default in switch */
			error(202);
		} else {
			if (reached && !ftflg) {
				if (hflag)
					/* fallthrough on default statement */
					warning(284);
			}
			ci->c_default = 1;
		}
		break;
	}
	reached = 1;
}

/*
 * T_IF T_LPARN expr T_RPARN
 */
void
if1(tnode_t *tn)
{

	if (tn != NULL)
		tn = cconv(tn);
	if (tn != NULL)
		tn = promote(NOOP, 0, tn);
	expr(tn, 0, 1, 1);
	pushctrl(T_IF);
}

/*
 * if_without_else
 * if_without_else T_ELSE
 */
void
if2(void)
{

	cstk->c_rchif = reached ? 1 : 0;
	reached = 1;
}

/*
 * if_without_else
 * if_without_else T_ELSE stmnt
 */
void
if3(int els)
{

	if (els) {
		reached |= cstk->c_rchif;
	} else {
		reached = 1;
	}
	popctrl(T_IF);
}

/*
 * T_SWITCH T_LPARN expr T_RPARN
 */
void
switch1(tnode_t *tn)
{
	tspec_t	t;
	type_t	*tp;

	if (tn != NULL)
		tn = cconv(tn);
	if (tn != NULL)
		tn = promote(NOOP, 0, tn);
	if (tn != NULL && !isityp(tn->tn_type->t_tspec)) {
		/* switch expression must have integral type */
		error(205);
		tn = NULL;
	}
	if (tn != NULL && tflag) {
		t = tn->tn_type->t_tspec;
		if (t == LONG || t == ULONG || t == QUAD || t == UQUAD) {
			/* switch expr. must be of type `int' in trad. C */
			warning(271);
		}
	}

	/*
	 * Remember the type of the expression. Because its possible
	 * that (*tp) is allocated on tree memory the type must be
	 * duplicated. This is not too complicated because it is
	 * only an integer type.
	 */
	if ((tp = calloc(1, sizeof (type_t))) == NULL)
		nomem();
	if (tn != NULL) {
		tp->t_tspec = tn->tn_type->t_tspec;
		if ((tp->t_isenum = tn->tn_type->t_isenum) != 0)
			tp->t_enum = tn->tn_type->t_enum;
	} else {
		tp->t_tspec = INT;
	}

	expr(tn, 1, 0, 1);

	pushctrl(T_SWITCH);
	cstk->c_switch = 1;
	cstk->c_swtype = tp;

	reached = rchflg = 0;
	ftflg = 1;
}

/*
 * switch_expr stmnt
 */
void
switch2(void)
{
	int	nenum = 0, nclab = 0;
	sym_t	*esym;
	clst_t	*cl;

	if (cstk->c_swtype == NULL)
		LERROR("switch2()");

	/*
	 * If the switch expression was of type enumeration, count the case
	 * labels and the number of enumerators. If both counts are not
	 * equal print a warning.
	 */
	if (cstk->c_swtype->t_isenum) {
		nenum = nclab = 0;
		if (cstk->c_swtype->t_enum == NULL)
			LERROR("switch2()");
		for (esym = cstk->c_swtype->t_enum->elem;
		     esym != NULL; esym = esym->s_nxt) {
			nenum++;
		}
		for (cl = cstk->c_clst; cl != NULL; cl = cl->cl_nxt)
			nclab++;
		if (hflag && eflag && nenum != nclab && !cstk->c_default) {
			/* enumeration value(s) not handled in switch */
			warning(206);
		}
	}

	if (cstk->c_break) {
		/*
		 * end of switch always reached (c_break is only set if the
		 * break statement can be reached).
		 */
		reached = 1;
	} else if (!cstk->c_default &&
		   (!hflag || !cstk->c_swtype->t_isenum || nenum != nclab)) {
		/*
		 * there are possible values which are not handled in
		 * switch
		 */
		reached = 1;
	}	/*
		 * otherwise the end of the switch expression is reached
		 * if the end of the last statement inside it is reached.
		 */

	popctrl(T_SWITCH);
}

/*
 * T_WHILE T_LPARN expr T_RPARN
 */
void
while1(tnode_t *tn)
{

	if (!reached) {
		/* loop not entered at top */
		warning(207);
		reached = 1;
	}

	if (tn != NULL)
		tn = cconv(tn);
	if (tn != NULL)
		tn = promote(NOOP, 0, tn);
	if (tn != NULL && !issclt(tn->tn_type->t_tspec)) {
		/* controlling expressions must have scalar type */
		error(204);
		tn = NULL;
	}

	pushctrl(T_WHILE);
	cstk->c_loop = 1;
	if (tn != NULL && tn->tn_op == CON) {
		if (isityp(tn->tn_type->t_tspec)) {
			cstk->c_infinite = tn->tn_val->v_quad != 0;
		} else {
			cstk->c_infinite = tn->tn_val->v_ldbl != 0.0;
		}
	}

	expr(tn, 0, 1, 1);
}

/*
 * while_expr stmnt
 * while_expr error
 */
void
while2(void)
{

	/*
	 * The end of the loop can be reached if it is no endless loop
	 * or there was a break statement which was reached.
	 */
	reached = !cstk->c_infinite || cstk->c_break;
	rchflg = 0;

	popctrl(T_WHILE);
}

/*
 * T_DO
 */
void
do1(void)
{

	if (!reached) {
		/* loop not entered at top */
		warning(207);
		reached = 1;
	}

	pushctrl(T_DO);
	cstk->c_loop = 1;
}

/*
 * do stmnt do_while_expr
 * do error
 */
void
do2(tnode_t *tn)
{

	/*
	 * If there was a continue statement the expression controlling the
	 * loop is reached.
	 */
	if (cstk->c_cont)
		reached = 1;

	if (tn != NULL)
		tn = cconv(tn);
	if (tn != NULL)
		tn = promote(NOOP, 0, tn);
	if (tn != NULL && !issclt(tn->tn_type->t_tspec)) {
		/* controlling expressions must have scalar type */
		error(204);
		tn = NULL;
	}

	if (tn != NULL && tn->tn_op == CON) {
		if (isityp(tn->tn_type->t_tspec)) {
			cstk->c_infinite = tn->tn_val->v_quad != 0;
		} else {
			cstk->c_infinite = tn->tn_val->v_ldbl != 0.0;
		}
	}

	expr(tn, 0, 1, 1);

	/*
	 * The end of the loop is only reached if it is no endless loop
	 * or there was a break statement which could be reached.
	 */
	reached = !cstk->c_infinite || cstk->c_break;
	rchflg = 0;

	popctrl(T_DO);
}

/*
 * T_FOR T_LPARN opt_expr T_SEMI opt_expr T_SEMI opt_expr T_RPARN
 */
void
for1(tnode_t *tn1, tnode_t *tn2, tnode_t *tn3)
{

	/*
	 * If there is no initialisation expression it is possible that
	 * it is intended not to enter the loop at top.
	 */
	if (tn1 != NULL && !reached) {
		/* loop not entered at top */
		warning(207);
		reached = 1;
	}

	pushctrl(T_FOR);
	cstk->c_loop = 1;

	/*
	 * Store the tree memory for the reinitialisation expression.
	 * Also remember this expression itself. We must check it at
	 * the end of the loop to get "used but not set" warnings correct.
	 */
	cstk->c_fexprm = tsave();
	cstk->c_f3expr = tn3;
	STRUCT_ASSIGN(cstk->c_fpos, curr_pos);
	STRUCT_ASSIGN(cstk->c_cfpos, csrc_pos);

	if (tn1 != NULL)
		expr(tn1, 0, 0, 1);

	if (tn2 != NULL)
		tn2 = cconv(tn2);
	if (tn2 != NULL)
		tn2 = promote(NOOP, 0, tn2);
	if (tn2 != NULL && !issclt(tn2->tn_type->t_tspec)) {
		/* controlling expressions must have scalar type */
		error(204);
		tn2 = NULL;
	}
	if (tn2 != NULL)
		expr(tn2, 0, 1, 1);

	if (tn2 == NULL) {
		cstk->c_infinite = 1;
	} else if (tn2->tn_op == CON) {
		if (isityp(tn2->tn_type->t_tspec)) {
			cstk->c_infinite = tn2->tn_val->v_quad != 0;
		} else {
			cstk->c_infinite = tn2->tn_val->v_ldbl != 0.0;
		}
	}

	/* Checking the reinitialisation expression is done in for2() */

	reached = 1;
}

/*
 * for_exprs stmnt
 * for_exprs error
 */
void
for2(void)
{
	pos_t	cpos, cspos;
	tnode_t	*tn3;

	if (cstk->c_cont)
		reached = 1;

	STRUCT_ASSIGN(cpos, curr_pos);
	STRUCT_ASSIGN(cspos, csrc_pos);

	/* Restore the tree memory for the reinitialisation expression */
	trestor(cstk->c_fexprm);
	tn3 = cstk->c_f3expr;
	STRUCT_ASSIGN(curr_pos, cstk->c_fpos);
	STRUCT_ASSIGN(csrc_pos, cstk->c_cfpos);

	/* simply "statement not reached" would be confusing */
	if (!reached && !rchflg) {
		/* end-of-loop code not reached */
		warning(223);
		reached = 1;
	}

	if (tn3 != NULL) {
		expr(tn3, 0, 0, 1);
	} else {
		tfreeblk();
	}

	STRUCT_ASSIGN(curr_pos, cpos);
	STRUCT_ASSIGN(csrc_pos, cspos);

	/* An endless loop without break will never terminate */
	reached = cstk->c_break || !cstk->c_infinite;
	rchflg = 0;

	popctrl(T_FOR);
}

/*
 * T_GOTO identifier T_SEMI
 * T_GOTO error T_SEMI
 */
void
dogoto(sym_t *lab)
{

	setuflg(lab, 0, 0);

	chkreach();

	reached = rchflg = 0;
}

/*
 * T_BREAK T_SEMI
 */
void
dobreak(void)
{
	cstk_t	*ci;

	ci = cstk;
	while (ci != NULL && !ci->c_loop && !ci->c_switch)
		ci = ci->c_nxt;

	if (ci == NULL) {
		/* break outside loop or switch */
		error(208);
	} else {
		if (reached)
			ci->c_break = 1;
	}

	if (bflag)
		chkreach();

	reached = rchflg = 0;
}

/*
 * T_CONTINUE T_SEMI
 */
void
docont(void)
{
	cstk_t	*ci;

	for (ci = cstk; ci != NULL && !ci->c_loop; ci = ci->c_nxt)
		continue;

	if (ci == NULL) {
		/* continue outside loop */
		error(209);
	} else {
		ci->c_cont = 1;
	}

	chkreach();

	reached = rchflg = 0;
}

/*
 * T_RETURN T_SEMI
 * T_RETURN expr T_SEMI
 */
void
doreturn(tnode_t *tn)
{
	tnode_t	*ln, *rn;
	cstk_t	*ci;
	op_t	op;

	for (ci = cstk; ci->c_nxt != NULL; ci = ci->c_nxt)
		continue;

	if (tn != NULL) {
		ci->c_retval = 1;
	} else {
		ci->c_noretval = 1;
	}

	if (tn != NULL && funcsym->s_type->t_subt->t_tspec == VOID) {
		/* void function %s cannot return value */
		error(213, funcsym->s_name);
		tfreeblk();
		tn = NULL;
	} else if (tn == NULL && funcsym->s_type->t_subt->t_tspec != VOID) {
		/*
		 * Assume that the function has a return value only if it
		 * is explicitly declared.
		 */
		if (!funcsym->s_rimpl)
			/* function %s expects to return value */
			warning(214, funcsym->s_name);
	}

	if (tn != NULL) {

		/* Create a temporary node for the left side */
		ln = tgetblk(sizeof (tnode_t));
		ln->tn_op = NAME;
		ln->tn_type = tduptyp(funcsym->s_type->t_subt);
		ln->tn_type->t_const = 0;
		ln->tn_lvalue = 1;
		ln->tn_sym = funcsym;		/* better than nothing */

		tn = build(RETURN, ln, tn);

		if (tn != NULL) {
			rn = tn->tn_right;
			while ((op = rn->tn_op) == CVT || op == PLUS)
				rn = rn->tn_left;
			if (rn->tn_op == AMPER && rn->tn_left->tn_op == NAME &&
			    rn->tn_left->tn_sym->s_scl == AUTO) {
				/* %s returns pointer to automatic object */
				warning(302, funcsym->s_name);
			}
		}

		expr(tn, 1, 0, 1);

	} else {

		chkreach();

	}

	reached = rchflg = 0;
}

/*
 * Do some cleanup after a global declaration or definition.
 * Especially remove informations about unused lint comments.
 */
void
glclup(int silent)
{
	pos_t	cpos;

	STRUCT_ASSIGN(cpos, curr_pos);

	if (nargusg != -1) {
		if (!silent) {
			STRUCT_ASSIGN(curr_pos, aupos);
			/* must precede function definition: %s */
			warning(282, "ARGSUSED");
		}
		nargusg = -1;
	}
	if (nvararg != -1) {
		if (!silent) {
			STRUCT_ASSIGN(curr_pos, vapos);
			/* must precede function definition: %s */
			warning(282, "VARARGS");
		}
		nvararg = -1;
	}
	if (prflstrg != -1) {
		if (!silent) {
			STRUCT_ASSIGN(curr_pos, prflpos);
			/* must precede function definition: %s */
			warning(282, "PRINTFLIKE");
		}
		prflstrg = -1;
	}
	if (scflstrg != -1) {
		if (!silent) {
			STRUCT_ASSIGN(curr_pos, scflpos);
			/* must precede function definition: %s */
			warning(282, "SCANFLIKE");
		}
		scflstrg = -1;
	}

	STRUCT_ASSIGN(curr_pos, cpos);

	dcs->d_asm = 0;
}

/*
 * ARGSUSED comment
 *
 * Only the first n arguments of the following function are checked
 * for usage. A missing argument is taken to be 0.
 */
void
argsused(int n)
{

	if (n == -1)
		n = 0;

	if (dcs->d_ctx != EXTERN) {
		/* must be outside function: ** %s ** */
		warning(280, "ARGSUSED");
		return;
	}
	if (nargusg != -1) {
		/* duplicate use of ** %s ** */
		warning(281, "ARGSUSED");
	}
	nargusg = n;
	STRUCT_ASSIGN(aupos, curr_pos);
}

/*
 * VARARGS comment
 *
 * Makes that lint2 checks only the first n arguments for compatibility
 * to the function definition. A missing argument is taken to be 0.
 */
void
varargs(int n)
{

	if (n == -1)
		n = 0;

	if (dcs->d_ctx != EXTERN) {
		/* must be outside function: ** %s ** */
		warning(280, "VARARGS");
		return;
	}
	if (nvararg != -1) {
		/* duplicate use of  ** %s ** */
		warning(281, "VARARGS");
	}
	nvararg = n;
	STRUCT_ASSIGN(vapos, curr_pos);
}

/*
 * PRINTFLIKE comment
 *
 * Check all arguments until the (n-1)-th as usual. The n-th argument is
 * used the check the types of remaining arguments.
 */
void
printflike(int n)
{

	if (n == -1)
		n = 0;

	if (dcs->d_ctx != EXTERN) {
		/* must be outside function: ** %s ** */
		warning(280, "PRINTFLIKE");
		return;
	}
	if (prflstrg != -1) {
		/* duplicate use of ** %s ** */
		warning(281, "PRINTFLIKE");
	}
	prflstrg = n;
	STRUCT_ASSIGN(prflpos, curr_pos);
}

/*
 * SCANFLIKE comment
 *
 * Check all arguments until the (n-1)-th as usual. The n-th argument is
 * used the check the types of remaining arguments.
 */
void
scanflike(int n)
{

	if (n == -1)
		n = 0;

	if (dcs->d_ctx != EXTERN) {
		/* must be outside function: ** %s ** */
		warning(280, "SCANFLIKE");
		return;
	}
	if (scflstrg != -1) {
		/* duplicate use of ** %s ** */
		warning(281, "SCANFLIKE");
	}
	scflstrg = n;
	STRUCT_ASSIGN(scflpos, curr_pos);
}

/*
 * Set the linenumber for a CONSTCOND comment. At this and the following
 * line no warnings about constants in conditional contexts are printed.
 */
/* ARGSUSED */
void
constcond(int n)
{

	ccflg = 1;
}

/*
 * Suppress printing of "fallthrough on ..." warnings until next
 * statement.
 */
/* ARGSUSED */
void
fallthru(int n)
{

	ftflg = 1;
}

/*
 * Stop warnings about statements which cannot be reached. Also tells lint
 * that the following statements cannot be reached (e.g. after exit()).
 */
/* ARGSUSED */
void
notreach(int n)
{

	reached = 0;
	rchflg = 1;
}

/* ARGSUSED */
void
lintlib(int n)
{

	if (dcs->d_ctx != EXTERN) {
		/* must be outside function: ** %s ** */
		warning(280, "LINTLIBRARY");
		return;
	}
	llibflg = 1;
	vflag = 0;
}

/*
 * Suppress most warnings at the current and the following line.
 */
/* ARGSUSED */
void
linted(int n)
{

#ifdef DEBUG
	printf("%s, %d: nowarn = 1\n", curr_pos.p_file, curr_pos.p_line);
#endif
	nowarn = 1;
}

/*
 * Suppress bitfield type errors on the current line.
 */
/* ARGSUSED */
void
bitfieldtype(int n)
{

#ifdef DEBUG
	printf("%s, %d: bitfieldtype_ok = 1\n", curr_pos.p_file,
	    curr_pos.p_line);
#endif
	bitfieldtype_ok = 1;
}

/*
 * PROTOTLIB in conjunction with LINTLIBRARY can be used to handle
 * prototypes like function definitions. This is done if the argument
 * to PROTOLIB is nonzero. Otherwise prototypes are handled normaly.
 */
void
protolib(int n)
{

	if (dcs->d_ctx != EXTERN) {
		/* must be outside function: ** %s ** */
		warning(280, "PROTOLIB");
		return;
	}
	plibflg = n == 0 ? 0 : 1;
}

/*
 * Set quadflg to nonzero which means that the next statement/declaration
 * may use "long long" without an error or warning.
 */
/* ARGSUSED */
void
longlong(int n)
{

	quadflg = 1;
}
