/*	$NetBSD: tree.c,v 1.12 1995/10/02 17:37:57 jpo Exp $	*/

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
 *
 * $FreeBSD$
 */

#ifndef lint
static char rcsid[] = "$NetBSD: tree.c,v 1.12 1995/10/02 17:37:57 jpo Exp $";
#endif

#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <limits.h>
#include <math.h>

#include "lint1.h"
#include "y.tab.h"

/* Various flags for each operator. */
static	mod_t	modtab[NOPS];

static	tnode_t	*getinode __P((tspec_t, quad_t));
static	void	ptrcmpok __P((op_t, tnode_t *, tnode_t *));
static	int	asgntypok __P((op_t, int, tnode_t *, tnode_t *));
static	void	chkbeop __P((op_t, tnode_t *, tnode_t *));
static	void	chkeop2 __P((op_t, int, tnode_t *, tnode_t *));
static	void	chkeop1 __P((op_t, int, tnode_t *, tnode_t *));
static	tnode_t	*mktnode __P((op_t, type_t *, tnode_t *, tnode_t *));
static	void	balance __P((op_t, tnode_t **, tnode_t **));
static	void	incompat __P((op_t, tspec_t, tspec_t));
static	void	illptrc __P((mod_t *, type_t *, type_t *));
static	void	mrgqual __P((type_t **, type_t *, type_t *));
static	int	conmemb __P((type_t *));
static	void	ptconv __P((int, tspec_t, tspec_t, type_t *, tnode_t *));
static	void	iiconv __P((op_t, int, tspec_t, tspec_t, type_t *, tnode_t *));
static	void	piconv __P((op_t, tspec_t, type_t *, tnode_t *));
static	void	ppconv __P((op_t, tnode_t *, type_t *));
static	tnode_t	*bldstr __P((op_t, tnode_t *, tnode_t *));
static	tnode_t	*bldincdec __P((op_t, tnode_t *));
static	tnode_t	*bldamper __P((tnode_t *, int));
static	tnode_t	*bldplmi __P((op_t, tnode_t *, tnode_t *));
static	tnode_t	*bldshft __P((op_t, tnode_t *, tnode_t *));
static	tnode_t	*bldcol __P((tnode_t *, tnode_t *));
static	tnode_t	*bldasgn __P((op_t, tnode_t *, tnode_t *));
static	tnode_t	*plength __P((type_t *));
static	tnode_t	*fold __P((tnode_t *));
static	tnode_t	*foldtst __P((tnode_t *));
static	tnode_t	*foldflt __P((tnode_t *));
static	tnode_t	*chkfarg __P((type_t *, tnode_t *));
static	tnode_t	*parg __P((int, type_t *, tnode_t *));
static	void	nulleff __P((tnode_t *));
static	void	displexpr __P((tnode_t *, int));
static	void	chkaidx __P((tnode_t *, int));
static	void	chkcomp __P((op_t, tnode_t *, tnode_t *));
static	void	precconf __P((tnode_t *));

/*
 * Initialize mods of operators.
 */
void
initmtab()
{
	static	struct {
		op_t	op;
		mod_t	m;
	} imods[] = {
		{ ARROW,  { 1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,
		    "->" } },
		{ POINT,  { 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		    "." } },
		{ NOT,    { 0,1,0,1,0,1,0,1,0,0,0,0,0,0,0,1,0,
		    "!" } },
		{ COMPL,  { 0,0,1,0,0,1,1,0,0,0,0,0,0,0,0,1,1,
		    "~" } },
		{ INCBEF, { 0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,
		    "prefix++" } },
		{ DECBEF, { 0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,
		    "prefix--" } },
		{ INCAFT, { 0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,
		    "postfix++" } },
		{ DECAFT, { 0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,
		    "postfix--" } },
		{ UPLUS,  { 0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,1,1,
		    "unary +" } },
		{ UMINUS, { 0,0,0,0,1,1,1,0,0,0,1,0,0,0,0,1,1,
		    "unary -" } },
		{ STAR,   { 0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,
		    "unary *" } },
		{ AMPER,  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		    "unary &" } },
		{ MULT,   { 1,0,0,0,1,1,1,0,1,0,0,1,0,0,0,1,1,
		    "*" } },
		{ DIV,    { 1,0,0,0,1,1,1,0,1,0,1,1,0,0,0,1,1,
		    "/" } },
		{ MOD,    { 1,0,1,0,0,1,1,0,1,0,1,1,0,0,0,1,1,
		    "%" } },
		{ PLUS,   { 1,0,0,1,0,1,1,0,1,0,0,0,0,0,0,1,0,
		    "+" } },
		{ MINUS,  { 1,0,0,1,0,1,1,0,1,0,0,0,0,0,0,1,0,
		    "-" } },
		{ SHL,    { 1,0,1,0,0,1,1,0,0,0,0,0,1,0,0,1,1,
		    "<<" } },
		{ SHR,    { 1,0,1,0,0,1,1,0,0,0,1,0,1,0,0,1,1,
		    ">>" } },
		{ LT,     { 1,1,0,1,0,1,1,0,1,0,1,1,0,1,1,0,1,
		    "<" } },
		{ LE,     { 1,1,0,1,0,1,1,0,1,0,1,1,0,1,1,0,1,
		    "<=" } },
		{ GT,     { 1,1,0,1,0,1,1,0,1,0,1,1,0,1,1,0,1,
		    ">" } },
		{ GE,     { 1,1,0,1,0,1,1,0,1,0,1,1,0,1,1,0,1,
		    ">=" } },
		{ EQ,     { 1,1,0,1,0,1,1,0,1,0,0,0,0,1,1,0,1,
		    "==" } },
		{ NE,     { 1,1,0,1,0,1,1,0,1,0,0,0,0,1,1,0,1,
		    "!=" } },
		{ AND,    { 1,0,1,0,0,1,1,0,1,0,0,0,1,0,0,1,0,
		    "&" } },
		{ XOR,    { 1,0,1,0,0,1,1,0,1,0,0,0,1,0,0,1,0,
		    "^" } },
		{ OR,     { 1,0,1,0,0,1,1,0,1,0,0,0,1,0,0,1,0,
		    "|" } },
		{ LOGAND, { 1,1,0,1,0,1,0,1,0,0,0,0,0,0,0,1,0,
		    "&&" } },
		{ LOGOR,  { 1,1,0,1,0,1,0,1,0,0,0,0,1,0,0,1,0,
		    "||" } },
		{ QUEST,  { 1,0,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,
		    "?" } },
		{ COLON,  { 1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0,0,
		    ":" } },
		{ ASSIGN, { 1,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,
		    "=" } },
		{ MULASS, { 1,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,
		    "*=" } },
		{ DIVASS, { 1,0,0,0,1,0,0,0,0,1,0,1,0,0,0,1,0,
		    "/=" } },
		{ MODASS, { 1,0,1,0,0,0,0,0,0,1,0,1,0,0,0,1,0,
		    "%=" } },
		{ ADDASS, { 1,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,
		    "+=" } },
		{ SUBASS, { 1,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,
		    "-=" } },
		{ SHLASS, { 1,0,1,0,0,0,0,0,0,1,0,0,0,0,0,1,0,
		    "<<=" } },
		{ SHRASS, { 1,0,1,0,0,0,0,0,0,1,0,0,0,0,0,1,0,
		    ">>=" } },
		{ ANDASS, { 1,0,1,0,0,0,0,0,0,1,0,0,0,0,0,1,0,
		    "&=" } },
		{ XORASS, { 1,0,1,0,0,0,0,0,0,1,0,0,0,0,0,1,0,
		    "^=" } },
		{ ORASS,  { 1,0,1,0,0,0,0,0,0,1,0,0,0,0,0,1,0,
		    "|=" } },
		{ NAME,   { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		    "NAME" } },
		{ CON,    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		    "CON" } },
		{ STRING, { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		    "STRING" } },
		{ FSEL,   { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		    "FSEL" } },
		{ CALL,   { 1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
		    "CALL" } },
		{ COMMA,  { 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		    "," } },
		{ CVT,    { 0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,
		    "CVT" } },
		{ ICALL,  { 1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
		    "ICALL" } },
		{ LOAD,	  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		    "LOAD" } },
		{ PUSH,   { 0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,
		    "PUSH" } },
		{ RETURN, { 1,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,
		    "RETURN" } },
		{ INIT,   { 1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
		    "INIT" } },
		{ FARG,   { 1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
		    "FARG" } },
		{ NOOP }
	};
	int	i;

	for (i = 0; imods[i].op != NOOP; i++)
		STRUCT_ASSIGN(modtab[imods[i].op], imods[i].m);
}

/*
 * Increase degree of reference.
 * This is most often used to change type "T" in type "pointer to T".
 */
type_t *
incref(tp, t)
	type_t	*tp;
	tspec_t	t;
{
	type_t	*tp2;

	tp2 = getblk(sizeof (type_t));
	tp2->t_tspec = t;
	tp2->t_subt = tp;
	return (tp2);
}

/*
 * same for use in expressions
 */
type_t *
tincref(tp, t)
	type_t	*tp;
	tspec_t	t;
{
	type_t	*tp2;

	tp2 = tgetblk(sizeof (type_t));
	tp2->t_tspec = t;
	tp2->t_subt = tp;
	return (tp2);
}

/*
 * Create a node for a constant.
 */
tnode_t *
getcnode(tp, v)
	type_t	*tp;
	val_t	*v;
{
	tnode_t	*n;

	n = getnode();
	n->tn_op = CON;
	n->tn_type = tp;
	n->tn_val = tgetblk(sizeof (val_t));
	n->tn_val->v_tspec = tp->t_tspec;
	n->tn_val->v_ansiu = v->v_ansiu;
	n->tn_val->v_u = v->v_u;
	free(v);
	return (n);
}

/*
 * Create a node for a integer constant.
 */
static tnode_t *
getinode(t, q)
	tspec_t	t;
	quad_t	q;
{
	tnode_t	*n;

	n = getnode();
	n->tn_op = CON;
	n->tn_type = gettyp(t);
	n->tn_val = tgetblk(sizeof (val_t));
	n->tn_val->v_tspec = t;
	n->tn_val->v_quad = q;
	return (n);
}

/*
 * Create a node for a name (symbol table entry).
 * ntok is the token which follows the name.
 */
tnode_t *
getnnode(sym, ntok)
	sym_t	*sym;
	int	ntok;
{
	tnode_t	*n;

	if (sym->s_scl == NOSCL) {
		sym->s_scl = EXTERN;
		sym->s_def = DECL;
		if (ntok == T_LPARN) {
			if (sflag) {
				/* function implicitly declared to ... */
				warning(215);
			}
			/*
			 * XXX if tflag is set the symbol should be
			 * exported to level 0
			 */
			sym->s_type = incref(sym->s_type, FUNC);
		} else {
			/* %s undefined */
			error(99, sym->s_name);
		}
	}

	if (sym->s_kind != FVFT && sym->s_kind != FMOS)
		lerror("getnnode() 1");

	n = getnode();
	n->tn_type = sym->s_type;
	if (sym->s_scl != ENUMCON) {
		n->tn_op = NAME;
		n->tn_sym = sym;
		if (sym->s_kind == FVFT && sym->s_type->t_tspec != FUNC)
			n->tn_lvalue = 1;
	} else {
		n->tn_op = CON;
		n->tn_val = tgetblk(sizeof (val_t));
		*n->tn_val = sym->s_value;
	}

	return (n);
}

/*
 * Create a node for a string.
 */
tnode_t *
getsnode(strg)
	strg_t	*strg;
{
	size_t	len;
	tnode_t	*n;

	len = strg->st_len;

	n = getnode();

	n->tn_op = STRING;
	n->tn_type = tincref(gettyp(strg->st_tspec), ARRAY);
	n->tn_type->t_dim = len + 1;
	n->tn_lvalue = 1;

	n->tn_strg = tgetblk(sizeof (strg_t));
	n->tn_strg->st_tspec = strg->st_tspec;
	n->tn_strg->st_len = len;

	if (strg->st_tspec == CHAR) {
		n->tn_strg->st_cp = tgetblk(len + 1);
		(void)memcpy(n->tn_strg->st_cp, strg->st_cp, len + 1);
		free(strg->st_cp);
	} else {
		n->tn_strg->st_wcp = tgetblk((len + 1) * sizeof (wchar_t));
		(void)memcpy(n->tn_strg->st_wcp, strg->st_wcp,
			     (len + 1) * sizeof (wchar_t));
		free(strg->st_wcp);
	}
	free(strg);

	return (n);
}

/*
 * Returns a symbol which has the same name as the msym argument and is a
 * member of the struct or union specified by the tn argument.
 */
sym_t *
strmemb(tn, op, msym)
	tnode_t	*tn;
	op_t	op;
	sym_t	*msym;
{
	str_t	*str;
	type_t	*tp;
	sym_t	*sym, *csym;
	int	eq;
	tspec_t	t;

	/*
	 * Remove the member if it was unknown until now (Which means
	 * that no defined struct or union has a member with the same name).
	 */
	if (msym->s_scl == NOSCL) {
		/* undefined struct/union member: %s */
		error(101, msym->s_name);
		rmsym(msym);
		msym->s_kind = FMOS;
		msym->s_scl = MOS;
		msym->s_styp = tgetblk(sizeof (str_t));
		msym->s_styp->stag = tgetblk(sizeof (sym_t));
		msym->s_styp->stag->s_name = unnamed;
		msym->s_value.v_tspec = INT;
		return (msym);
	}

	/* Set str to the tag of which msym is expected to be a member. */
	str = NULL;
	t = (tp = tn->tn_type)->t_tspec;
	if (op == POINT) {
		if (t == STRUCT || t == UNION)
			str = tp->t_str;
	} else if (op == ARROW && t == PTR) {
		t = (tp = tp->t_subt)->t_tspec;
		if (t == STRUCT || t == UNION)
			str = tp->t_str;
	}

	/*
	 * If this struct/union has a member with the name of msym, return
	 * return this it.
	 */
	if (str != NULL) {
		for (sym = msym; sym != NULL; sym = sym->s_link) {
			if (sym->s_scl != MOS && sym->s_scl != MOU)
				continue;
			if (sym->s_styp != str)
				continue;
			if (strcmp(sym->s_name, msym->s_name) != 0)
				continue;
			return (sym);
		}
	}

	/*
	 * Set eq to 0 if there are struct/union members with the same name
	 * and different types and/or offsets.
	 */
	eq = 1;
	for (csym = msym; csym != NULL; csym = csym->s_link) {
		if (csym->s_scl != MOS && csym->s_scl != MOU)
			continue;
		if (strcmp(msym->s_name, csym->s_name) != 0)
			continue;
		for (sym = csym->s_link ; sym != NULL; sym = sym->s_link) {
			int w;

			if (sym->s_scl != MOS && sym->s_scl != MOU)
				continue;
			if (strcmp(csym->s_name, sym->s_name) != 0)
				continue;
			if (csym->s_value.v_quad != sym->s_value.v_quad) {
				eq = 0;
				break;
			}
			w = 0;
			eq = eqtype(csym->s_type, sym->s_type, 0, 0, &w) && !w;
			if (!eq)
				break;
			if (csym->s_field != sym->s_field) {
				eq = 0;
				break;
			}
			if (csym->s_field) {
				type_t	*tp1, *tp2;

				tp1 = csym->s_type;
				tp2 = sym->s_type;
				if (tp1->t_flen != tp2->t_flen) {
					eq = 0;
					break;
				}
				if (tp1->t_foffs != tp2->t_foffs) {
					eq = 0;
					break;
				}
			}
		}
		if (!eq)
			break;
	}

	/*
	 * Now handle the case in which the left operand refers really
	 * to a struct/union, but the right operand is not member of it.
	 */
	if (str != NULL) {
		/* illegal member use: %s */
		if (eq && tflag) {
			warning(102, msym->s_name);
		} else {
			error(102, msym->s_name);
		}
		return (msym);
	}

	/*
	 * Now the left operand of ARROW does not point to a struct/union
	 * or the left operand of POINT is no struct/union.
	 */
	if (eq) {
		if (op == POINT) {
			/* left operand of "." must be struct/union object */
			if (tflag) {
				warning(103);
			} else {
				error(103);
			}
		} else {
			/* left operand of "->" must be pointer to ... */
			if (tflag && tn->tn_type->t_tspec == PTR) {
				warning(104);
			} else {
				error(104);
			}
		}
	} else {
		if (tflag) {
			/* non-unique member requires struct/union %s */
			error(105, op == POINT ? "object" : "pointer");
		} else {
			/* unacceptable operand of %s */
			error(111, modtab[op].m_name);
		}
	}

	return (msym);
}

/*
 * Create a tree node. Called for most operands except function calls,
 * sizeof and casts.
 *
 * op	operator
 * ln	left operand
 * rn	if not NULL, right operand
 */
tnode_t *
build(op, ln, rn)
	op_t	op;
	tnode_t	*ln, *rn;
{
	mod_t	*mp;
	tnode_t	*ntn;
	type_t	*rtp;

	mp = &modtab[op];

	/* If there was an error in one of the operands, return. */
	if (ln == NULL || (mp->m_binary && rn == NULL))
		return (NULL);

	/*
	 * Apply class conversions to the left operand, but only if its
	 * value is needed or it is compaired with null.
	 */
	if (mp->m_vctx || mp->m_tctx)
		ln = cconv(ln);
	/*
	 * The right operand is almost always in a test or value context,
	 * except if it is a struct or union member.
	 */
	if (mp->m_binary && op != ARROW && op != POINT)
		rn = cconv(rn);

	/*
	 * Print some warnings for comparisons of unsigned values with
	 * constants lower than or equal to null. This must be done
	 * before promote() because otherwise unsigned char and unsigned
	 * short would be promoted to int. Also types are tested to be
	 * CHAR, which would also become int.
	 */
	if (mp->m_comp)
		chkcomp(op, ln, rn);

	/*
	 * Promote the left operand if it is in a test or value context
	 */
	if (mp->m_vctx || mp->m_tctx)
		ln = promote(op, 0, ln);
	/*
	 * Promote the right operand, but only if it is no struct or
	 * union member, or if it is not to be assigned to the left operand
	 */
	if (mp->m_binary && op != ARROW && op != POINT &&
	    op != ASSIGN && op != RETURN) {
		rn = promote(op, 0, rn);
	}

	/*
	 * If the result of the operation is different for signed or
	 * unsigned operands and one of the operands is signed only in
	 * ANSI C, print a warning.
	 */
	if (mp->m_tlansiu && ln->tn_op == CON && ln->tn_val->v_ansiu) {
		/* ANSI C treats constant as unsigned, op %s */
		warning(218, mp->m_name);
		ln->tn_val->v_ansiu = 0;
	}
	if (mp->m_transiu && rn->tn_op == CON && rn->tn_val->v_ansiu) {
		/* ANSI C treats constant as unsigned, op %s */
		warning(218, mp->m_name);
		rn->tn_val->v_ansiu = 0;
	}

	/* Make sure both operands are of the same type */
	if (mp->m_balance || (tflag && (op == SHL || op == SHR)))
		balance(op, &ln, &rn);

	/*
	 * Check types for compatibility with the operation and mutual
	 * compatibility. Return if there are serios problems.
	 */
	if (!typeok(op, 0, ln, rn))
		return (NULL);

	/* And now create the node. */
	switch (op) {
	case POINT:
	case ARROW:
		ntn = bldstr(op, ln, rn);
		break;
	case INCAFT:
	case DECAFT:
	case INCBEF:
	case DECBEF:
		ntn = bldincdec(op, ln);
		break;
	case AMPER:
		ntn = bldamper(ln, 0);
		break;
	case STAR:
		ntn = mktnode(STAR, ln->tn_type->t_subt, ln, NULL);
		break;
	case PLUS:
	case MINUS:
		ntn = bldplmi(op, ln, rn);
		break;
	case SHL:
	case SHR:
		ntn = bldshft(op, ln, rn);
		break;
	case COLON:
		ntn = bldcol(ln, rn);
		break;
	case ASSIGN:
	case MULASS:
	case DIVASS:
	case MODASS:
	case ADDASS:
	case SUBASS:
	case SHLASS:
	case SHRASS:
	case ANDASS:
	case XORASS:
	case ORASS:
	case RETURN:
		ntn = bldasgn(op, ln, rn);
		break;
	case COMMA:
	case QUEST:
		ntn = mktnode(op, rn->tn_type, ln, rn);
		break;
	default:
		rtp = mp->m_logop ? gettyp(INT) : ln->tn_type;
		if (!mp->m_binary && rn != NULL)
			lerror("build() 1");
		ntn = mktnode(op, rtp, ln, rn);
		break;
	}

	/* Return if an error occured. */
	if (ntn == NULL)
		return (NULL);

	/* Print a warning if precedence confusion is possible */
	if (mp->m_tpconf)
		precconf(ntn);

	/*
	 * Print a warning if one of the operands is in a context where
	 * it is compared with null and if this operand is a constant.
	 */
	if (mp->m_tctx) {
		if (ln->tn_op == CON ||
		    ((mp->m_binary && op != QUEST) && rn->tn_op == CON)) {
			if (hflag && !ccflg)
				/* constant in conditional context */
				warning(161);
		}
	}

	/* Fold if the operator requires it */
	if (mp->m_fold) {
		if (ln->tn_op == CON && (!mp->m_binary || rn->tn_op == CON)) {
			if (mp->m_tctx) {
				ntn = foldtst(ntn);
			} else if (isftyp(ntn->tn_type->t_tspec)) {
				ntn = foldflt(ntn);
			} else {
				ntn = fold(ntn);
			}
		} else if (op == QUEST && ln->tn_op == CON) {
			ntn = ln->tn_val->v_quad ? rn->tn_left : rn->tn_right;
		}
	}

	return (ntn);
}

/*
 * Perform class conversions.
 *
 * Arrays of type T are converted into pointers to type T.
 * Functions are converted to pointers to functions.
 * Lvalues are converted to rvalues.
 */
tnode_t *
cconv(tn)
	tnode_t	*tn;
{
	type_t	*tp;

	/*
	 * Array-lvalue (array of type T) is converted into rvalue
	 * (pointer to type T)
	 */
	if (tn->tn_type->t_tspec == ARRAY) {
		if (!tn->tn_lvalue) {
			/* %soperand of '%s' must be lvalue */
			/* XXX print correct operator */
			(void)gnuism(114, "", modtab[AMPER].m_name);
		}
		tn = mktnode(AMPER, tincref(tn->tn_type->t_subt, PTR),
			     tn, NULL);
	}

	/*
	 * Expression of type function (function with return value of type T)
	 * in rvalue-expression (pointer to function with return value
	 * of type T)
	 */
	if (tn->tn_type->t_tspec == FUNC)
		tn = bldamper(tn, 1);

	/* lvalue to rvalue */
	if (tn->tn_lvalue) {
		tp = tduptyp(tn->tn_type);
		tp->t_const = tp->t_volatile = 0;
		tn = mktnode(LOAD, tp, tn, NULL);
	}

	return (tn);
}

/*
 * Perform most type checks. First the types are checked using
 * informations from modtab[]. After that it is done by hand for
 * more complicated operators and type combinations.
 *
 * If the types are ok, typeok() returns 1, otherwise 0.
 */
int
typeok(op, arg, ln, rn)
	op_t	op;
	int	arg;
	tnode_t	*ln, *rn;
{
	mod_t	*mp;
	tspec_t	lt, rt, lst, rst, olt, ort;
	type_t	*ltp, *rtp, *lstp, *rstp;
	tnode_t	*tn;

	mp = &modtab[op];

	if ((lt = (ltp = ln->tn_type)->t_tspec) == PTR)
		lst = (lstp = ltp->t_subt)->t_tspec;
	if (mp->m_binary) {
		if ((rt = (rtp = rn->tn_type)->t_tspec) == PTR)
			rst = (rstp = rtp->t_subt)->t_tspec;
	}

	if (mp->m_rqint) {
		/* integertypes required */
		if (!isityp(lt) || (mp->m_binary && !isityp(rt))) {
			incompat(op, lt, rt);
			return (0);
		}
	} else if (mp->m_rqsclt) {
		/* scalar types required */
		if (!issclt(lt) || (mp->m_binary && !issclt(rt))) {
			incompat(op, lt, rt);
			return (0);
		}
	} else if (mp->m_rqatyp) {
		/* arithmetic types required */
		if (!isatyp(lt) || (mp->m_binary && !isatyp(rt))) {
			incompat(op, lt, rt);
			return (0);
		}
	}

	if (op == SHL || op == SHR || op == SHLASS || op == SHRASS) {
		/*
		 * For these operations we need the types before promotion
		 * and balancing.
		 */
		for (tn=ln; tn->tn_op==CVT && !tn->tn_cast; tn=tn->tn_left) ;
		olt = tn->tn_type->t_tspec;
		for (tn=rn; tn->tn_op==CVT && !tn->tn_cast; tn=tn->tn_left) ;
		ort = tn->tn_type->t_tspec;
	}
		
	switch (op) {
	case POINT:
		/*
		 * Most errors required by ANSI C are reported in strmemb().
		 * Here we only must check for totaly wrong things.
		 */
		if (lt == FUNC || lt == VOID || ltp->t_isfield ||
		    ((lt != STRUCT && lt != UNION) && !ln->tn_lvalue)) {
			/* Without tflag we got already an error */
			if (tflag)
				/* unacceptable operand of %s */
				error(111, mp->m_name);
			return (0);
		}
		/* Now we have an object we can create a pointer to */
		break;
	case ARROW:
		if (lt != PTR && !(tflag && isityp(lt))) {
			/* Without tflag we got already an error */
			if (tflag)
				/* unacceptabel operand of %s */
				error(111, mp->m_name);
			return (0);
		}
		break;
	case INCAFT:
	case DECAFT:
	case INCBEF:
	case DECBEF:
		/* operands have scalar types (checked above) */
		if (!ln->tn_lvalue) {
			if (ln->tn_op == CVT && ln->tn_cast &&
			    ln->tn_left->tn_op == LOAD) {
				/* a cast does not yield an lvalue */
				error(163);
			}
			/* %soperand of %s must be lvalue */
			error(114, "", mp->m_name);
			return (0);
		} else if (ltp->t_const) {
			/* %soperand of %s must be modifiable lvalue */
			if (!tflag)
				warning(115, "", mp->m_name);
		}
		break;
	case AMPER:
		if (lt == ARRAY || lt == FUNC) {
			/* ok, a warning comes later (in bldamper()) */
		} else if (!ln->tn_lvalue) {
			if (ln->tn_op == CVT && ln->tn_cast &&
			    ln->tn_left->tn_op == LOAD) {
				/* a cast does not yield an lvalue */
				error(163);
			}
			/* %soperand of %s must be lvalue */
			error(114, "", mp->m_name);
			return (0);
		} else if (issclt(lt)) {
			if (ltp->t_isfield) {
				/* cannot take address of bit-field */
				error(112);
				return (0);
			}
		} else if (lt != STRUCT && lt != UNION) {
			/* unacceptable operand of %s */
			error(111, mp->m_name);
			return (0);
		}
		if (ln->tn_op == NAME && ln->tn_sym->s_reg) {
			/* cannot take address of register %s */
			error(113, ln->tn_sym->s_name);
			return (0);
		}
		break;
	case STAR:
		/* until now there were no type checks for this operator */
		if (lt != PTR) {
			/* cannot dereference non-pointer type */
			error(96);
			return (0);
		}
		break;
	case PLUS:
		/* operands have scalar types (checked above) */
		if ((lt == PTR && !isityp(rt)) || (rt == PTR && !isityp(lt))) {
			incompat(op, lt, rt);
			return (0);
		}
		break;
	case MINUS:
		/* operands have scalar types (checked above) */
		if (lt == PTR && (!isityp(rt) && rt != PTR)) {
			incompat(op, lt, rt);
			return (0);
		} else if (rt == PTR && lt != PTR) {
			incompat(op, lt, rt);
			return (0);
		}
		if (lt == PTR && rt == PTR) {
			if (!eqtype(lstp, rstp, 1, 0, NULL)) {
				/* illegal pointer subtraction */
				error(116);
			}
		}
		break;
	case SHR:
		/* operands have integer types (checked above) */
		if (pflag && !isutyp(lt)) {
			/*
			 * The left operand is signed. This means that
			 * the operation is (possibly) nonportable.
			 */
			/* bitwise operation on signed value nonportable */
			if (ln->tn_op != CON) {
				/* possibly nonportable */
				warning(117);
			} else if (ln->tn_val->v_quad < 0) {
				warning(120);
			}
		} else if (!tflag && !sflag && !isutyp(olt) && isutyp(ort)) {
			/*
			 * The left operand would become unsigned in
			 * traditional C.
			 */
			if (hflag &&
			    (ln->tn_op != CON || ln->tn_val->v_quad < 0)) {
				/* semantics of %s change in ANSI C; use ... */
				warning(118, mp->m_name);
			}
		} else if (!tflag && !sflag && !isutyp(olt) && !isutyp(ort) &&
			   psize(lt) < psize(rt)) {
			/*
			 * In traditional C the left operand would be extended,
			 * possibly with 1, and then shifted.
			 */
			if (hflag &&
			    (ln->tn_op != CON || ln->tn_val->v_quad < 0)) {
				/* semantics of %s change in ANSI C; use ... */
				warning(118, mp->m_name);
			}
		}
		goto shift;
	case SHL:
		/*
		 * ANSI C does not perform balancing for shift operations,
		 * but traditional C does. If the width of the right operand
		 * is greather than the width of the left operand, than in
		 * traditional C the left operand would be extendet to the
		 * width of the right operand. For SHL this may result in
		 * different results.
		 */
		if (psize(lt) < psize(rt)) {
			/*
			 * XXX If both operands are constant make sure
			 * that there is really a differencs between
			 * ANSI C and traditional C.
			 */
			if (hflag)
				/* semantics of %s change in ANSI C; use ... */
				warning(118, mp->m_name);
		}
	shift:
		if (rn->tn_op == CON) {
			if (!isutyp(rt) && rn->tn_val->v_quad < 0) {
				/* negative shift */
				warning(121);
			} else if ((u_quad_t)rn->tn_val->v_quad == size(lt)) {
				/* shift equal to size fo object */
				warning(267);
			} else if ((u_quad_t)rn->tn_val->v_quad > size(lt)) {
				/* shift greater than size of object */
				warning(122);
			}
		}
		break;
	case EQ:
	case NE:
		/*
		 * Accept some things which are allowed with EQ and NE,
		 * but not with ordered comparisons.
		 */
		if (lt == PTR && ((rt == PTR && rst == VOID) || isityp(rt))) {
			if (rn->tn_op == CON && rn->tn_val->v_quad == 0)
				break;
		}
		if (rt == PTR && ((lt == PTR && lst == VOID) || isityp(lt))) {
			if (ln->tn_op == CON && ln->tn_val->v_quad == 0)
				break;
		}
		/* FALLTHROUGH */
	case LT:
	case GT:
	case LE:
	case GE:
		if ((lt == PTR || rt == PTR) && lt != rt) {
			if (isityp(lt) || isityp(rt)) {
				/* illegal comb. of pointer and int., op %s */
				warning(123, mp->m_name);
			} else {
				incompat(op, lt, rt);
				return (0);
			}
		} else if (lt == PTR && rt == PTR) {
			ptrcmpok(op, ln, rn);
		}
		break;
	case QUEST:
		if (!issclt(lt)) {
			/* first operand must have scalar type, op ? : */
			error(170);
			return (0);
		}
		if (rn->tn_op != COLON)
			lerror("typeok() 2");
		break;
	case COLON:

		if (isatyp(lt) && isatyp(rt))
			break;

		if (lt == STRUCT && rt == STRUCT && ltp->t_str == rtp->t_str)
			break;
		if (lt == UNION && rt == UNION && ltp->t_str == rtp->t_str)
			break;

		/* combination of any pointer and 0, 0L or (void *)0 is ok */
		if (lt == PTR && ((rt == PTR && rst == VOID) || isityp(rt))) {
			if (rn->tn_op == CON && rn->tn_val->v_quad == 0)
				break;
		}
		if (rt == PTR && ((lt == PTR && lst == VOID) || isityp(lt))) {
			if (ln->tn_op == CON && ln->tn_val->v_quad == 0)
				break;
		}

		if ((lt == PTR && isityp(rt)) || (isityp(lt) && rt == PTR)) {
			/* illegal comb. of ptr. and int., op %s */
			warning(123, mp->m_name);
			break;
		}

		if (lt == VOID || rt == VOID) {
			if (lt != VOID || rt != VOID)
				/* incompatible types in conditional */
				warning(126);
			break;
		}

		if (lt == PTR && rt == PTR && ((lst == VOID && rst == FUNC) ||
					       (lst == FUNC && rst == VOID))) {
			/* (void *)0 handled above */
			if (sflag)
				/* ANSI C forbids conv. of %s to %s, op %s */
				warning(305, "function pointer", "'void *'",
					mp->m_name);
			break;
		}

		if (rt == PTR && lt == PTR) {
			if (!eqtype(lstp, rstp, 1, 0, NULL))
				illptrc(mp, ltp, rtp);
			break;
		}

		/* incompatible types in conditional */
		error(126);
		return (0);

	case ASSIGN:
	case INIT:
	case FARG:
	case RETURN:
		if (!asgntypok(op, arg, ln, rn))
			return (0);
		goto assign;
	case MULASS:
	case DIVASS:
	case MODASS:
		goto assign;
	case ADDASS:
	case SUBASS:
		/* operands have scalar types (checked above) */
		if ((lt == PTR && !isityp(rt)) || rt == PTR) {
			incompat(op, lt, rt);
			return (0);
		}
		goto assign;
	case SHLASS:
		goto assign;
	case SHRASS:
		if (pflag && !isutyp(lt) && !(tflag && isutyp(rt))) {
			/* bitwise operation on s.v. possibly nonportabel */
			warning(117);
		}
		goto assign;
	case ANDASS:
	case XORASS:
	case ORASS:
		goto assign;
	assign:
		if (!ln->tn_lvalue) {
			if (ln->tn_op == CVT && ln->tn_cast &&
			    ln->tn_left->tn_op == LOAD) {
				/* a cast does not yield an lvalue */
				error(163);
			}
			/* %soperand of %s must be lvalue */
			error(114, "left ", mp->m_name);
			return (0);
		} else if (ltp->t_const || ((lt == STRUCT || lt == UNION) &&
					    conmemb(ltp))) {
			/* %soperand of %s must be modifiable lvalue */
			if (!tflag)
				warning(115, "left ", mp->m_name);
		}
		break;
	case COMMA:
		if (!modtab[ln->tn_op].m_sideeff)
			nulleff(ln);
		break;
		/* LINTED (enumeration values not handled in switch) */
	default:
	}

	if (mp->m_badeop &&
	    (ltp->t_isenum || (mp->m_binary && rtp->t_isenum))) {
		chkbeop(op, ln, rn);
	} else if (mp->m_enumop && (ltp->t_isenum && rtp->t_isenum)) {
		chkeop2(op, arg, ln, rn);
	} else if (mp->m_enumop && (ltp->t_isenum || rtp->t_isenum)) {
		chkeop1(op, arg, ln, rn);
	}

	return (1);
}

static void
ptrcmpok(op, ln, rn)
	op_t	op;
	tnode_t	*ln, *rn;
{
	type_t	*ltp, *rtp;
	tspec_t	lt, rt;
	const	char *lts, *rts;

	lt = (ltp = ln->tn_type)->t_subt->t_tspec;
	rt = (rtp = rn->tn_type)->t_subt->t_tspec;

	if (lt == VOID || rt == VOID) {
		if (sflag && (lt == FUNC || rt == FUNC)) {
			/* (void *)0 already handled in typeok() */
			*(lt == FUNC ? &lts : &rts) = "function pointer";
			*(lt == VOID ? &lts : &rts) = "'void *'";
			/* ANSI C forbids comparison of %s with %s */
			warning(274, lts, rts);
		}
		return;
	}

	if (!eqtype(ltp->t_subt, rtp->t_subt, 1, 0, NULL)) {
		illptrc(&modtab[op], ltp, rtp);
		return;
	}

	if (lt == FUNC && rt == FUNC) {
		if (sflag && op != EQ && op != NE)
			/* ANSI C forbids ordered comp. of func ptr */
			warning(125);
	}
}

/*
 * Checks type compatibility for ASSIGN, INIT, FARG and RETURN
 * and prints warnings/errors if necessary.
 * If the types are (almost) compatible, 1 is returned, otherwise 0.
 */
static int
asgntypok(op, arg, ln, rn)
	op_t	op;
	int	arg;
	tnode_t	*ln, *rn;
{
	tspec_t	lt, rt, lst, rst;
	type_t	*ltp, *rtp, *lstp, *rstp;
	mod_t	*mp;
	const	char *lts, *rts;

	if ((lt = (ltp = ln->tn_type)->t_tspec) == PTR)
		lst = (lstp = ltp->t_subt)->t_tspec;
	if ((rt = (rtp = rn->tn_type)->t_tspec) == PTR)
		rst = (rstp = rtp->t_subt)->t_tspec;
	mp = &modtab[op];

	if (isatyp(lt) && isatyp(rt))
		return (1);

	if ((lt == STRUCT || lt == UNION) && (rt == STRUCT || rt == UNION))
		/* both are struct or union */
		return (ltp->t_str == rtp->t_str);

	/* 0, 0L and (void *)0 may be assigned to any pointer */
	if (lt == PTR && ((rt == PTR && rst == VOID) || isityp(rt))) {
		if (rn->tn_op == CON && rn->tn_val->v_quad == 0)
			return (1);
	}

	if (lt == PTR && rt == PTR && (lst == VOID || rst == VOID)) {
		/* two pointers, at least one pointer to void */
		if (sflag && (lst == FUNC || rst == FUNC)) {
			/* comb. of ptr to func and ptr to void */
			*(lst == FUNC ? &lts : &rts) = "function pointer";
			*(lst == VOID ? &lts : &rts) = "'void *'";
			switch (op) {
			case INIT:
			case RETURN:
				/* ANSI C forbids conversion of %s to %s */
				warning(303, rts, lts);
				break;
			case FARG:
				/* ANSI C forbids conv. of %s to %s, arg #%d */
				warning(304, rts, lts, arg);
				break;
			default:
				/* ANSI C forbids conv. of %s to %s, op %s */
				warning(305, rts, lts, mp->m_name);
				break;
			}
		}
	}

	if (lt == PTR && rt == PTR && (lst == VOID || rst == VOID ||
				       eqtype(lstp, rstp, 1, 0, NULL))) {
		/* compatible pointer types (qualifiers ignored) */
		if (!tflag &&
		    ((!lstp->t_const && rstp->t_const) ||
		     (!lstp->t_volatile && rstp->t_volatile))) {
			/* left side has not all qualifiers of right */
			switch (op) {
			case INIT:
			case RETURN:
				/* incompatible pointer types */
				warning(182);
				break;
			case FARG:
				/* argument has incompat. ptr. type, arg #%d */
				warning(153, arg);
				break;
			default:
				/* operands have incompat. ptr. types, op %s */
				warning(128, mp->m_name);
				break;
			}
		}
		return (1);
	}

	if ((lt == PTR && isityp(rt)) || (isityp(lt) && rt == PTR)) {
		switch (op) {
		case INIT:
		case RETURN:
			/* illegal combination of pointer and integer */
			warning(183);
			break;
		case FARG:
			/* illegal comb. of ptr. and int., arg #%d */
			warning(154, arg);
			break;
		default:
			/* illegal comb. of ptr. and int., op %s */
			warning(123, mp->m_name);
			break;
		}
		return (1);
	}

	if (lt == PTR && rt == PTR) {
		switch (op) {
		case INIT:
		case RETURN:
			illptrc(NULL, ltp, rtp);
			break;
		case FARG:
			/* argument has incompatible pointer type, arg #%d */
			warning(153, arg);
			break;
		default:
			illptrc(mp, ltp, rtp);
			break;
		}
		return (1);
	}

	switch (op) {
	case INIT:
		/* initialisation type mismatch */
		error(185);
		break;
	case RETURN:
		/* return value type mismatch */
		error(211);
		break;
	case FARG:
		/* argument is incompatible with prototype, arg #%d */
		warning(155, arg);
		break;
	default:
		incompat(op, lt, rt);
		break;
	}

	return (0);
}

/*
 * Prints a warning if an operator, which should be senseless for an
 * enum type, is applied to an enum type.
 */
static void
chkbeop(op, ln, rn)
	op_t	op;
	tnode_t	*ln, *rn;
{
	mod_t	*mp;

	if (!eflag)
		return;

	mp = &modtab[op];

	if (!(ln->tn_type->t_isenum ||
	      (mp->m_binary && rn->tn_type->t_isenum))) {
		return;
	}

	/*
	 * Enum as offset to a pointer is an exception (otherwise enums
	 * could not be used as array indizes).
	 */
	if (op == PLUS &&
	    ((ln->tn_type->t_isenum && rn->tn_type->t_tspec == PTR) ||
	     (rn->tn_type->t_isenum && ln->tn_type->t_tspec == PTR))) {
		return;
	}

	/* dubious operation on enum, op %s */
	warning(241, mp->m_name);

}

/*
 * Prints a warning if an operator is applied to two different enum types.
 */
static void
chkeop2(op, arg, ln, rn)
	op_t	op;
	int	arg;
	tnode_t	*ln, *rn;
{
	mod_t	*mp;

	mp = &modtab[op];

	if (ln->tn_type->t_enum != rn->tn_type->t_enum) {
		switch (op) {
		case INIT:
			/* enum type mismatch in initialisation */
			warning(210);
			break;
		case FARG:
			/* enum type mismatch, arg #%d */
			warning(156, arg);
			break;
		case RETURN:
			/* return value type mismatch */
			warning(211);
			break;
		default:
			/* enum type mismatch, op %s */
			warning(130, mp->m_name);
			break;
		}
#if 0
	} else if (mp->m_comp && op != EQ && op != NE) {
		if (eflag)
			/* dubious comparisons of enums */
			warning(243, mp->m_name);
#endif
	}
}

/*
 * Prints a warning if an operator has both enum end other integer
 * types.
 */
static void
chkeop1(op, arg, ln, rn)
	op_t	op;
	int	arg;
	tnode_t	*ln, *rn;
{
	if (!eflag)
		return;

	switch (op) {
	case INIT:
		/*
		 * Initializations with 0 should be allowed. Otherwise,
		 * we should complain about all uninitialized enums,
		 * consequently.
		 */
		if (!rn->tn_type->t_isenum && rn->tn_op == CON &&
		    isityp(rn->tn_type->t_tspec) && rn->tn_val->v_quad == 0) {
			return;
		}
		/* initialisation of '%s' with '%s' */
		warning(277, tyname(ln->tn_type), tyname(rn->tn_type));
		break;
	case FARG:
		/* combination of '%s' and '%s', arg #%d */
		warning(278, tyname(ln->tn_type), tyname(rn->tn_type), arg);
		break;
	case RETURN:
		/* combination of '%s' and '%s' in return */
		warning(279, tyname(ln->tn_type), tyname(rn->tn_type));
		break;
	default:
		/* combination of '%s' and %s, op %s */
		warning(242, tyname(ln->tn_type), tyname(rn->tn_type),
			modtab[op].m_name);
		break;
	}
}

/*
 * Build and initialize a new node.
 */
static tnode_t *
mktnode(op, type, ln, rn)
	op_t	op;
	type_t	*type;
	tnode_t	*ln, *rn;
{
	tnode_t	*ntn;
	tspec_t	t;

	ntn = getnode();

	ntn->tn_op = op;
	ntn->tn_type = type;
	ntn->tn_left = ln;
	ntn->tn_right = rn;

	if (op == STAR || op == FSEL) {
		if (ln->tn_type->t_tspec == PTR) {
			t = ln->tn_type->t_subt->t_tspec;
			if (t != FUNC && t != VOID)
				ntn->tn_lvalue = 1;
		} else {
			lerror("mktnode() 2");
		}
	}

	return (ntn);
}

/*
 * Performs usual conversion of operands to (unsigned) int.
 *
 * If tflag is set or the operand is a function argument with no
 * type information (no prototype or variable # of args), convert
 * float to double.
 */
tnode_t *
promote(op, farg, tn)
	op_t	op;
	int	farg;
	tnode_t	*tn;
{
	tspec_t	t;
	type_t	*ntp;
	int	len;

	t = tn->tn_type->t_tspec;

	if (!isatyp(t))
		return (tn);

	if (!tflag) {
		/*
		 * ANSI C requires that the result is always of type INT
		 * if INT can represent all possible values of the previous
		 * type.
		 */
		if (tn->tn_type->t_isfield) {
			len = tn->tn_type->t_flen;
			if (size(INT) > len) {
				t = INT;
			} else {
				if (size(INT) != len)
					lerror("promote() 1");
				if (isutyp(t)) {
					t = UINT;
				} else {
					t = INT;
				}
			}
		} else if (t == CHAR || t == UCHAR || t == SCHAR) {
			t = (size(CHAR) < size(INT) || t != UCHAR) ?
				INT : UINT;
		} else if (t == SHORT || t == USHORT) {
			t = (size(SHORT) < size(INT) || t == SHORT) ?
				INT : UINT;
		} else if (t == ENUM) {
			t = INT;
		} else if (farg && t == FLOAT) {
			t = DOUBLE;
		}
	} else {
		/*
		 * In traditional C, keep unsigned and promote FLOAT
		 * to DOUBLE.
		 */
		if (t == UCHAR || t == USHORT) {
			t = UINT;
		} else if (t == CHAR || t == SCHAR || t == SHORT) {
			t = INT;
		} else if (t == FLOAT) {
			t = DOUBLE;
		} else if (t == ENUM) {
			t = INT;
		}
	}

	if (t != tn->tn_type->t_tspec) {
		ntp = tduptyp(tn->tn_type);
		ntp->t_tspec = t;
		/*
		 * Keep t_isenum so we are later able to check compatibility
		 * of enum types.
		 */
		tn = convert(op, 0, ntp, tn);
	}

	return (tn);
}

/*
 * Insert conversions which are necessary to give both operands the same
 * type. This is done in different ways for traditional C and ANIS C.
 */
static void
balance(op, lnp, rnp)
	op_t	op;
	tnode_t	**lnp, **rnp;
{
	tspec_t	lt, rt, t;
	int	i, u;
	type_t	*ntp;
	static	tspec_t	tl[] = {
		LDOUBLE, DOUBLE, FLOAT, UQUAD, QUAD, ULONG, LONG, UINT, INT,
	};

	lt = (*lnp)->tn_type->t_tspec;
	rt = (*rnp)->tn_type->t_tspec;

	if (!isatyp(lt) || !isatyp(rt))
		return;

	if (!tflag) {
		if (lt == rt) {
			t = lt;
		} else if (lt == LDOUBLE || rt == LDOUBLE) {
			t = LDOUBLE;
		} else if (lt == DOUBLE || rt == DOUBLE) {
			t = DOUBLE;
		} else if (lt == FLOAT || rt == FLOAT) {
			t = FLOAT;
		} else {
			/*
			 * If type A has more bits than type B it should
			 * be able to hold all possible values of type B.
			 */
			if (size(lt) > size(rt)) {
				t = lt;
			} else if (size(lt) < size(rt)) {
				t = rt;
			} else {
				for (i = 3; tl[i] != INT; i++) {
					if (tl[i] == lt || tl[i] == rt)
						break;
				}
				if ((isutyp(lt) || isutyp(rt)) &&
				    !isutyp(tl[i])) {
					i--;
				}
				t = tl[i];
			}
		}
	} else {
		/* Keep unsigned in traditional C */
		u = isutyp(lt) || isutyp(rt);
		for (i = 0; tl[i] != INT; i++) {
			if (lt == tl[i] || rt == tl[i])
				break;
		}
		t = tl[i];
		if (u && isityp(t) && !isutyp(t))
			t = utyp(t);
	}

	if (t != lt) {
		ntp = tduptyp((*lnp)->tn_type);
		ntp->t_tspec = t;
		*lnp = convert(op, 0, ntp, *lnp);
	}
	if (t != rt) {
		ntp = tduptyp((*rnp)->tn_type);
		ntp->t_tspec = t;
		*rnp = convert(op, 0, ntp, *rnp);
	}
}

/*
 * Insert a conversion operator, which converts the type of the node
 * to another given type.
 * If op is FARG, arg is the number of the argument (used for warnings).
 */
tnode_t *
convert(op, arg, tp, tn)
	op_t	op;
	int	arg;
	type_t	*tp;
	tnode_t	*tn;
{
	tnode_t	*ntn;
	tspec_t	nt, ot, ost;

	if (tn->tn_lvalue)
		lerror("convert() 1");

	nt = tp->t_tspec;
	if ((ot = tn->tn_type->t_tspec) == PTR)
		ost = tn->tn_type->t_subt->t_tspec;

	if (!tflag && !sflag && op == FARG)
		ptconv(arg, nt, ot, tp, tn);
	if (isityp(nt) && isityp(ot)) {
		iiconv(op, arg, nt, ot, tp, tn);
	} else if (nt == PTR && ((ot == PTR && ost == VOID) || isityp(ot)) &&
		   tn->tn_op == CON && tn->tn_val->v_quad == 0) {
		/* 0, 0L and (void *)0 may be assigned to any pointer. */
	} else if (isityp(nt) && ot == PTR) {
		piconv(op, nt, tp, tn);
	} else if (nt == PTR && ot == PTR) {
		ppconv(op, tn, tp);
	}

	ntn = getnode();
	ntn->tn_op = CVT;
	ntn->tn_type = tp;
	ntn->tn_cast = op == CVT;
	if (tn->tn_op != CON || nt == VOID) {
		ntn->tn_left = tn;
	} else {
		ntn->tn_op = CON;
		ntn->tn_val = tgetblk(sizeof (val_t));
		cvtcon(op, arg, ntn->tn_type, ntn->tn_val, tn->tn_val);
	}

	return (ntn);
}

/*
 * Print a warning if a prototype causes a type conversion that is
 * different from what would happen to the same argument in the
 * absence of a prototype.
 *
 * Errors/Warnings about illegal type combinations are already printed
 * in asgntypok().
 */
static void
ptconv(arg, nt, ot, tp, tn)
	int	arg;
	tspec_t	nt, ot;
	type_t	*tp;
	tnode_t	*tn;
{
	tnode_t	*ptn;

	if (!isatyp(nt) || !isatyp(ot))
		return;

	/*
	 * If the type of the formal parameter is char/short, a warning
	 * would be useless, because functions declared the old style
	 * can't expect char/short arguments.
	 */
	if (nt == CHAR || nt == UCHAR || nt == SHORT || nt == USHORT)
		return;

	/* get default promotion */
	ptn = promote(NOOP, 1, tn);
	ot = ptn->tn_type->t_tspec;

	/* return if types are the same with and without prototype */
	if (nt == ot || (nt == ENUM && ot == INT))
		return;

	if (isftyp(nt) != isftyp(ot) || psize(nt) != psize(ot)) {
		/* representation and/or width change */
		if (styp(nt) != SHORT || !isityp(ot) || psize(ot) > psize(INT))
			/* conversion to '%s' due to prototype, arg #%d */
			warning(259, tyname(tp), arg);
	} else if (hflag) {
		/*
		 * they differ in sign or base type (char, short, int,
		 * long, long long, float, double, long double)
		 *
		 * if they differ only in sign and the argument is a constant
		 * and the msb of the argument is not set, print no warning
		 */
		if (ptn->tn_op == CON && isityp(nt) && styp(nt) == styp(ot) &&
		    msb(ptn->tn_val->v_quad, ot, -1) == 0) {
			/* ok */
		} else {
			/* conversion to '%s' due to prototype, arg #%d */
			warning(259, tyname(tp), arg);
		}
	}
}

/*
 * Print warnings for conversions of integer types which my cause
 * problems.
 */
/* ARGSUSED */
static void
iiconv(op, arg, nt, ot, tp, tn)
	op_t	op;
	int	arg;
	tspec_t	nt, ot;
	type_t	*tp;
	tnode_t	*tn;
{
	if (tn->tn_op == CON)
		return;

	if (op == CVT)
		return;

#if 0
	if (psize(nt) > psize(ot) && isutyp(nt) != isutyp(ot)) {
		/* conversion to %s may sign-extend incorrectly (, arg #%d) */
		if (aflag && pflag) {
			if (op == FARG) {
				warning(297, tyname(tp), arg);
			} else {
				warning(131, tyname(tp));
			}
		}
	}
#endif

	if (psize(nt) < psize(ot) &&
	    (ot == LONG || ot == ULONG || ot == QUAD || ot == UQUAD ||
	     aflag > 1)) {
		/* conversion from '%s' may lose accuracy */
		if (aflag) {
			if (op == FARG) {
				warning(298, tyname(tn->tn_type), arg);
			} else {
				warning(132, tyname(tn->tn_type));
			}
		} 
	}
}

/*
 * Print warnings for dubious conversions of pointer to integer.
 */
static void
piconv(op, nt, tp, tn)
	op_t	op;
	tspec_t	nt;
	type_t	*tp;
	tnode_t	*tn;
{
	if (tn->tn_op == CON)
		return;

	if (op != CVT) {
		/* We got already an error. */
		return;
	}

	if (psize(nt) < psize(PTR)) {
		if (pflag && size(nt) >= size(PTR)) {
			/* conv. of pointer to %s may lose bits */
			warning(134, tyname(tp));
		} else {
			/* conv. of pointer to %s loses bits */
			warning(133, tyname(tp));
		}
	}
}

/*
 * Print warnings for questionable pointer conversions.
 */
static void
ppconv(op, tn, tp)
	op_t	op;
	tnode_t	*tn;
	type_t	*tp;
{
	tspec_t nt, ot;
	const	char *nts, *ots;

	/*
	 * We got already an error (pointers of different types
	 * without a cast) or we will not get a warning.
	 */
	if (op != CVT)
		return;

	nt = tp->t_subt->t_tspec;
	ot = tn->tn_type->t_subt->t_tspec;

	if (nt == VOID || ot == VOID) {
		if (sflag && (nt == FUNC || ot == FUNC)) {
			/* (void *)0 already handled in convert() */
			*(nt == FUNC ? &nts : &ots) = "function pointer";
			*(nt == VOID ? &nts : &ots) = "'void *'";
			/* ANSI C forbids conversion of %s to %s */
			warning(303, ots, nts);
		}
		return;
	} else if (nt == FUNC && ot == FUNC) {
		return;
	} else if (nt == FUNC || ot == FUNC) {
		/* questionable conversion of function pointer */
		warning(229);
		return;
	}
	
	if (getbound(tp->t_subt) > getbound(tn->tn_type->t_subt)) {
		if (hflag)
			/* possible pointer alignment problem */
			warning(135);
	}
	if (((nt == STRUCT || nt == UNION) &&
	     tp->t_subt->t_str != tn->tn_type->t_subt->t_str) ||
	    psize(nt) != psize(ot)) {
		if (cflag) {
			/* pointer casts may be troublesome */
			warning(247);
		}
	}
}

/*
 * Converts a typed constant in a constant of another type.
 *
 * op		operator which requires conversion
 * arg		if op is FARG, # of argument
 * tp		type in which to convert the constant
 * nv		new constant
 * v		old constant
 */
void
cvtcon(op, arg, tp, nv, v)
	op_t	op;
	int	arg;
	type_t	*tp;
	val_t	*nv, *v;
{
	tspec_t	ot, nt;
	ldbl_t	max, min;
	int	sz, rchk;
	quad_t	xmask, xmsk1;
	int	osz, nsz;

	ot = v->v_tspec;
	nt = nv->v_tspec = tp->t_tspec;
	rchk = 0;

	if (ot == FLOAT || ot == DOUBLE || ot == LDOUBLE) {
		switch (nt) {
		case CHAR:
			max = CHAR_MAX;		min = CHAR_MIN;		break;
		case UCHAR:
			max = UCHAR_MAX;	min = 0;		break;
		case SCHAR:
			max = SCHAR_MAX;	min = SCHAR_MIN;	break;
		case SHORT:
			max = SHRT_MAX;		min = SHRT_MIN;		break;
		case USHORT:
			max = USHRT_MAX;	min = 0;		break;
		case ENUM:
		case INT:
			max = INT_MAX;		min = INT_MIN;		break;
		case UINT:
			max = (u_int)UINT_MAX;	min = 0;		break;
		case LONG:
			max = LONG_MAX;		min = LONG_MIN;		break;
		case ULONG:
			max = (u_long)ULONG_MAX; min = 0;		break;
		case QUAD:
			max = QUAD_MAX;		min = QUAD_MIN;		break;
		case UQUAD:
			max = (u_quad_t)UQUAD_MAX; min = 0;		break;
		case FLOAT:
			max = FLT_MAX;		min = -FLT_MAX;		break;
		case DOUBLE:
			max = DBL_MAX;		min = -DBL_MAX;		break;
		case PTR:
			/* Got already an error because of float --> ptr */
		case LDOUBLE:
			max = LDBL_MAX;		min = -LDBL_MAX;	break;
		default:
			lerror("cvtcon() 1");
		}
		if (v->v_ldbl > max || v->v_ldbl < min) {
			if (nt == LDOUBLE)
				lerror("cvtcon() 2");
			if (op == FARG) {
				/* conv. of %s to %s is out of rng., arg #%d */
				warning(295, tyname(gettyp(ot)), tyname(tp),
					arg);
			} else {
				/* conversion of %s to %s is out of range */
				warning(119, tyname(gettyp(ot)), tyname(tp));
			}
			v->v_ldbl = v->v_ldbl > 0 ? max : min;
		}
		if (nt == FLOAT) {
			nv->v_ldbl = (float)v->v_ldbl;
		} else if (nt == DOUBLE) {
			nv->v_ldbl = (double)v->v_ldbl;
		} else if (nt == LDOUBLE) {
			nv->v_ldbl = v->v_ldbl;
		} else {
			nv->v_quad = (nt == PTR || isutyp(nt)) ?
				(u_quad_t)v->v_ldbl : (quad_t)v->v_ldbl;
		}
	} else {
		if (nt == FLOAT) {
			nv->v_ldbl = (ot == PTR || isutyp(ot)) ?
			       (float)(u_quad_t)v->v_quad : (float)v->v_quad;
		} else if (nt == DOUBLE) {
			nv->v_ldbl = (ot == PTR || isutyp(ot)) ?
			       (double)(u_quad_t)v->v_quad : (double)v->v_quad;
		} else if (nt == LDOUBLE) {
			nv->v_ldbl = (ot == PTR || isutyp(ot)) ?
			       (ldbl_t)(u_quad_t)v->v_quad : (ldbl_t)v->v_quad;
		} else {
			rchk = 1;		/* Check for lost precision. */
			nv->v_quad = v->v_quad;
		}
	}

	if (v->v_ansiu && isftyp(nt)) {
		/* ANSI C treats constant as unsigned */
		warning(157);
		v->v_ansiu = 0;
	} else if (v->v_ansiu && (isityp(nt) && !isutyp(nt) &&
				  psize(nt) > psize(ot))) {
		/* ANSI C treats constant as unsigned */
		warning(157);
		v->v_ansiu = 0;
	}

	if (nt != FLOAT && nt != DOUBLE && nt != LDOUBLE) {
		sz = tp->t_isfield ? tp->t_flen : size(nt);
		nv->v_quad = xsign(nv->v_quad, nt, sz);
	}
	
	if (rchk && op != CVT) {
		osz = size(ot);
		nsz = tp->t_isfield ? tp->t_flen : size(nt);
		xmask = qlmasks[nsz] ^ qlmasks[osz];
		xmsk1 = qlmasks[nsz] ^ qlmasks[osz - 1];
		/*
		 * For bitwise operations we are not interested in the
		 * value, but in the bits itself.
		 */
		if (op == ORASS || op == OR || op == XOR) {
			/*
			 * Print a warning if bits which were set are
			 * lost due to the conversion.
			 * This can happen with operator ORASS only.
			 */
			if (nsz < osz && (v->v_quad & xmask) != 0) {
				/* constant truncated by conv., op %s */
				warning(306, modtab[op].m_name);
			}
		} else if (op == ANDASS || op == AND) {
			/*
			 * Print a warning if additional bits are not all 1
			 * and the most significant bit of the old value is 1,
			 * or if at least one (but not all) removed bit was 0.
			 */
			if (nsz > osz &&
			    (nv->v_quad & qbmasks[osz - 1]) != 0 &&
			    (nv->v_quad & xmask) != xmask) {
				/*
				 * extra bits set to 0 in conversion
				 * of '%s' to '%s', op %s
				 */
				warning(309, tyname(gettyp(ot)),
					tyname(tp), modtab[op].m_name);
			} else if (nsz < osz &&
				   (v->v_quad & xmask) != xmask &&
				   (v->v_quad & xmask) != 0) {
				/* const. truncated by conv., op %s */
				warning(306, modtab[op].m_name);
			}
		} else if ((nt != PTR && isutyp(nt)) &&
			   (ot != PTR && !isutyp(ot)) && v->v_quad < 0) {
			if (op == ASSIGN) {
				/* assignment of negative constant to ... */
				warning(164);
			} else if (op == INIT) {
				/* initialisation of unsigned with neg. ... */
				warning(221);
			} else if (op == FARG) {
				/* conversion of neg. const. to ..., arg #%d */
				warning(296, arg);
			} else if (modtab[op].m_comp) {
				/* we get this warning already in chkcomp() */
			} else {
				/* conversion of negative constant to ... */
				warning(222);
			}
		} else if (nv->v_quad != v->v_quad && nsz <= osz &&
			   (v->v_quad & xmask) != 0 &&
			   (isutyp(ot) || (v->v_quad & xmsk1) != xmsk1)) {
			/*
			 * Loss of significant bit(s). All truncated bits
			 * of unsigned types or all truncated bits plus the
			 * msb of the target for signed types are considered
			 * to be significant bits. Loss of significant bits
			 * means that at least on of the bits was set in an
			 * unsigned type or that at least one, but not all of
			 * the bits was set in an signed type.
			 * Loss of significant bits means that it is not
			 * possible, also not with necessary casts, to convert
			 * back to the original type. A example for a
			 * necessary cast is:
			 *	char c;	int	i; c = 128;
			 *	i = c;			** yields -128 **
			 *	i = (unsigned char)c;	** yields 128 **
			 */
			if (op == ASSIGN && tp->t_isfield) {
				/* precision lost in bit-field assignment */
				warning(166);
			} else if (op == ASSIGN) {
				/* constant truncated by assignment */
				warning(165);
			} else if (op == INIT && tp->t_isfield) {
				/* bit-field initializer does not fit */
				warning(180);
			} else if (op == INIT) {
				/* initializer does not fit */
				warning(178);
			} else if (op == CASE) {
				/* case label affected by conversion */
				warning(196);
			} else if (op == FARG) {
				/* conv. of %s to %s is out of rng., arg #%d */
				warning(295, tyname(gettyp(ot)), tyname(tp),
					arg);
			} else {
				/* conversion of %s to %s is out of range */
				warning(119, tyname(gettyp(ot)), tyname(tp));
			}
		} else if (nv->v_quad != v->v_quad) {
			if (op == ASSIGN && tp->t_isfield) {
				/* precision lost in bit-field assignment */
				warning(166);
			} else if (op == INIT && tp->t_isfield) {
				/* bit-field initializer out of range */
				warning(11);
			} else if (op == CASE) {
				/* case label affected by conversion */
				warning(196);
			} else if (op == FARG) {
				/* conv. of %s to %s is out of rng., arg #%d */
				warning(295, tyname(gettyp(ot)), tyname(tp),
					arg);
			} else {
				/* conversion of %s to %s is out of range */
				warning(119, tyname(gettyp(ot)), tyname(tp));
			}
		}
	}
}

/*
 * Called if incompatible types were detected.
 * Prints a appropriate warning.
 */
static void
incompat(op, lt, rt)
	op_t	op;
	tspec_t	lt, rt;
{
	mod_t	*mp;

	mp = &modtab[op];

	if (lt == VOID || (mp->m_binary && rt == VOID)) {
		/* void type illegal in expression */
		error(109);
	} else if (op == ASSIGN) {
		if ((lt == STRUCT || lt == UNION) &&
		    (rt == STRUCT || rt == UNION)) {
			/* assignment of different structures */
			error(240);
		} else {
			/* assignment type mismatch */
			error(171);
		}
	} else if (mp->m_binary) {
		/* operands of %s have incompatible types */
		error(107, mp->m_name);
	} else {
		/* operand of %s has incompatible type */
		error(108, mp->m_name);
	}
}

/*
 * Called if incompatible pointer types are detected.
 * Print an appropriate warning.
 */
static void
illptrc(mp, ltp, rtp)
	mod_t	*mp;
	type_t	*ltp, *rtp;
{
	tspec_t	lt, rt;

	if (ltp->t_tspec != PTR || rtp->t_tspec != PTR)
		lerror("illptrc() 1");

	lt = ltp->t_subt->t_tspec;
	rt = rtp->t_subt->t_tspec;

	if ((lt == STRUCT || lt == UNION) && (rt == STRUCT || rt == UNION)) {
		if (mp == NULL) {
			/* illegal structure pointer combination */
			warning(244);
		} else {
			/* illegal structure pointer combination, op %s */
			warning(245, mp->m_name);
		}
	} else {
		if (mp == NULL) {
			/* illegal pointer combination */
			warning(184);
		} else {
			/* illegal pointer combination, op %s */
			warning(124, mp->m_name);
		}
	}
}

/*
 * Make sure type (*tpp)->t_subt has at least the qualifiers
 * of tp1->t_subt and tp2->t_subt.
 */
static void
mrgqual(tpp, tp1, tp2)
	type_t	**tpp, *tp1, *tp2;
{
	if ((*tpp)->t_tspec != PTR ||
	    tp1->t_tspec != PTR || tp2->t_tspec != PTR) {
		lerror("mrgqual()");
	}

	if ((*tpp)->t_subt->t_const ==
	    (tp1->t_subt->t_const | tp2->t_subt->t_const) &&
	    (*tpp)->t_subt->t_volatile ==
	    (tp1->t_subt->t_volatile | tp2->t_subt->t_volatile)) {
		return;
	}

	*tpp = tduptyp(*tpp);
	(*tpp)->t_subt = tduptyp((*tpp)->t_subt);
	(*tpp)->t_subt->t_const =
		tp1->t_subt->t_const | tp2->t_subt->t_const;
	(*tpp)->t_subt->t_volatile =
		tp1->t_subt->t_volatile | tp2->t_subt->t_volatile;
}

/*
 * Returns 1 if the given structure or union has a constant member
 * (maybe recursively).
 */
static int
conmemb(tp)
	type_t	*tp;
{
	sym_t	*m;
	tspec_t	t;

	if ((t = tp->t_tspec) != STRUCT && t != UNION)
		lerror("conmemb()");
	for (m = tp->t_str->memb; m != NULL; m = m->s_nxt) {
		tp = m->s_type;
		if (tp->t_const)
			return (1);
		if ((t = tp->t_tspec) == STRUCT || t == UNION) {
			if (conmemb(m->s_type))
				return (1);
		}
	}
	return (0);
}

const char *
tyname(tp)
	type_t	*tp;
{
	tspec_t	t;
	const	char *s;

	if ((t = tp->t_tspec) == INT && tp->t_isenum)
		t = ENUM;

	switch (t) {
	case CHAR:	s = "char";			break;
	case UCHAR:	s = "unsigned char";		break;
	case SCHAR:	s = "signed char";		break;
	case SHORT:	s = "short";			break;
	case USHORT:	s = "unsigned short";		break;
	case INT:	s = "int";			break;
	case UINT:	s = "unsigned int";		break;
	case LONG:	s = "long";			break;
	case ULONG:	s = "unsigned long";		break;
	case QUAD:	s = "long long";		break;
	case UQUAD:	s = "unsigned long long";	break;
	case FLOAT:	s = "float";			break;
	case DOUBLE:	s = "double";			break;
	case LDOUBLE:	s = "long double";		break;
	case PTR:	s = "pointer";			break;
	case ENUM:	s = "enum";			break;
	case STRUCT:	s = "struct";			break;
	case UNION:	s = "union";			break;
	case FUNC:	s = "function";			break;
	case ARRAY:	s = "array";			break;
	default:
		lerror("tyname()");
	}
	return (s);
}

/*
 * Create a new node for one of the operators POINT and ARROW.
 */
static tnode_t *
bldstr(op, ln, rn)
	op_t	op;
	tnode_t	*ln, *rn;
{
	tnode_t	*ntn, *ctn;
	int	nolval;

	if (rn->tn_op != NAME)
		lerror("bldstr() 1");
	if (rn->tn_sym->s_value.v_tspec != INT)
		lerror("bldstr() 2");
	if (rn->tn_sym->s_scl != MOS && rn->tn_sym->s_scl != MOU)
		lerror("bldstr() 3");

	/*
	 * Remember if the left operand is an lvalue (structure members
	 * are lvalues if and only if the structure itself is an lvalue).
	 */
	nolval = op == POINT && !ln->tn_lvalue;

	if (op == POINT) {
		ln = bldamper(ln, 1);
	} else if (ln->tn_type->t_tspec != PTR) {
		if (!tflag || !isityp(ln->tn_type->t_tspec))
			lerror("bldstr() 4");
		ln = convert(NOOP, 0, tincref(gettyp(VOID), PTR), ln);
	}

#if PTRDIFF_IS_LONG
	ctn = getinode(LONG, rn->tn_sym->s_value.v_quad / CHAR_BIT);
#else
	ctn = getinode(INT, rn->tn_sym->s_value.v_quad / CHAR_BIT);
#endif

	ntn = mktnode(PLUS, tincref(rn->tn_type, PTR), ln, ctn);
	if (ln->tn_op == CON)
		ntn = fold(ntn);

	if (rn->tn_type->t_isfield) {
		ntn = mktnode(FSEL, ntn->tn_type->t_subt, ntn, NULL);
	} else {
		ntn = mktnode(STAR, ntn->tn_type->t_subt, ntn, NULL);
	}

	if (nolval)
		ntn->tn_lvalue = 0;

	return (ntn);
}

/*
 * Create a node for INCAFT, INCBEF, DECAFT and DECBEF.
 */
static tnode_t *
bldincdec(op, ln)
	op_t	op;
	tnode_t	*ln;
{
	tnode_t	*cn, *ntn;

	if (ln == NULL)
		lerror("bldincdec() 1");

	if (ln->tn_type->t_tspec == PTR) {
		cn = plength(ln->tn_type);
	} else {
		cn = getinode(INT, (quad_t)1);
	}
	ntn = mktnode(op, ln->tn_type, ln, cn);

	return (ntn);
}

/*
 * Create a tree node for the & operator
 */
static tnode_t *
bldamper(tn, noign)
	tnode_t	*tn;
	int	noign;
{
	tnode_t	*ntn;
	tspec_t	t;
	
	if (!noign && ((t = tn->tn_type->t_tspec) == ARRAY || t == FUNC)) {
		/* & before array or function: ignored */
		if (tflag)
			warning(127);
		return (tn);
	}

	/* eliminate &* */
	if (tn->tn_op == STAR &&
	    tn->tn_left->tn_type->t_tspec == PTR &&
	    tn->tn_left->tn_type->t_subt == tn->tn_type) {
		return (tn->tn_left);
	}
	    
	ntn = mktnode(AMPER, tincref(tn->tn_type, PTR), tn, NULL);

	return (ntn);
}

/*
 * Create a node for operators PLUS and MINUS.
 */
static tnode_t *
bldplmi(op, ln, rn)
	op_t	op;
	tnode_t	*ln, *rn;
{
	tnode_t	*ntn, *ctn;
	type_t	*tp;

	/* If pointer and integer, then pointer to the lhs. */
	if (rn->tn_type->t_tspec == PTR && isityp(ln->tn_type->t_tspec)) {
		ntn = ln;
		ln = rn;
		rn = ntn;
	}

	if (ln->tn_type->t_tspec == PTR && rn->tn_type->t_tspec != PTR) {

		if (!isityp(rn->tn_type->t_tspec))
			lerror("bldplmi() 1");

		ctn = plength(ln->tn_type);
		if (rn->tn_type->t_tspec != ctn->tn_type->t_tspec)
			rn = convert(NOOP, 0, ctn->tn_type, rn);
		rn = mktnode(MULT, rn->tn_type, rn, ctn);
		if (rn->tn_left->tn_op == CON)
			rn = fold(rn);
		ntn = mktnode(op, ln->tn_type, ln, rn);

	} else if (rn->tn_type->t_tspec == PTR) {

		if (ln->tn_type->t_tspec != PTR || op != MINUS)
			lerror("bldplmi() 2");
#if PTRDIFF_IS_LONG
		tp = gettyp(LONG);
#else
		tp = gettyp(INT);
#endif
		ntn = mktnode(op, tp, ln, rn);
		if (ln->tn_op == CON && rn->tn_op == CON)
			ntn = fold(ntn);
		ctn = plength(ln->tn_type);
		balance(NOOP, &ntn, &ctn);
		ntn = mktnode(DIV, tp, ntn, ctn);

	} else {

		ntn = mktnode(op, ln->tn_type, ln, rn);

	}
	return (ntn);
}

/*
 * Create a node for operators SHL and SHR.
 */
static tnode_t *
bldshft(op, ln, rn)
	op_t	op;
	tnode_t	*ln, *rn;
{
	tspec_t	t;
	tnode_t	*ntn;

	if ((t = rn->tn_type->t_tspec) != INT && t != UINT)
		rn = convert(CVT, 0, gettyp(INT), rn);
	ntn = mktnode(op, ln->tn_type, ln, rn);
	return (ntn);
}

/*
 * Create a node for COLON.
 */
static tnode_t *
bldcol(ln, rn)
	tnode_t	*ln, *rn;
{
	tspec_t	lt, rt, pdt;
	type_t	*rtp;
	tnode_t	*ntn;

	lt = ln->tn_type->t_tspec;
	rt = rn->tn_type->t_tspec;
#if PTRDIFF_IS_LONG
	pdt = LONG;
#else
	pdt = INT;
#endif

	/*
	 * Arithmetic types are balanced, all other type combinations
	 * still need to be handled.
	 */
	if (isatyp(lt) && isatyp(rt)) {
		rtp = ln->tn_type;
	} else if (lt == VOID || rt == VOID) {
		rtp = gettyp(VOID);
	} else if (lt == STRUCT || lt == UNION) {
		/* Both types must be identical. */
		if (rt != STRUCT && rt != UNION)
			lerror("bldcol() 1");
		if (ln->tn_type->t_str != rn->tn_type->t_str)
			lerror("bldcol() 2");
		if (incompl(ln->tn_type)) {
			/* unknown operand size, op %s */
			error(138, modtab[COLON].m_name);
			return (NULL);
		}
		rtp = ln->tn_type;
	} else if (lt == PTR && isityp(rt)) {
		if (rt != pdt) {
			rn = convert(NOOP, 0, gettyp(pdt), rn);
			rt = pdt;
		}
		rtp = ln->tn_type;
	} else if (rt == PTR && isityp(lt)) {
		if (lt != pdt) {
			ln = convert(NOOP, 0, gettyp(pdt), ln);
			lt = pdt;
		}
		rtp = rn->tn_type;
	} else if (lt == PTR && ln->tn_type->t_subt->t_tspec == VOID) {
		if (rt != PTR)
			lerror("bldcol() 4");
		rtp = ln->tn_type;
		mrgqual(&rtp, ln->tn_type, rn->tn_type);
	} else if (rt == PTR && rn->tn_type->t_subt->t_tspec == VOID) {
		if (lt != PTR)
			lerror("bldcol() 5");
		rtp = rn->tn_type;
		mrgqual(&rtp, ln->tn_type, rn->tn_type);
	} else {
		if (lt != PTR || rt != PTR)
			lerror("bldcol() 6");
		/*
		 * XXX For now we simply take the left type. This is
		 * probably wrong, if one type contains a functionprototype
		 * and the other one, at the same place, only an old style
		 * declaration.
		 */
		rtp = ln->tn_type;
		mrgqual(&rtp, ln->tn_type, rn->tn_type);
	}

	ntn = mktnode(COLON, rtp, ln, rn);

	return (ntn);
}

/*
 * Create a node for an assignment operator (both = and op= ).
 */
static tnode_t *
bldasgn(op, ln, rn)
	op_t	op;
	tnode_t	*ln, *rn;
{
	tspec_t	lt, rt;
	tnode_t	*ntn, *ctn;

	if (ln == NULL || rn == NULL)
		lerror("bldasgn() 1");

	lt = ln->tn_type->t_tspec;
	rt = rn->tn_type->t_tspec;

	if ((op == ADDASS || op == SUBASS) && lt == PTR) {
		if (!isityp(rt))
			lerror("bldasgn() 2");
		ctn = plength(ln->tn_type);
		if (rn->tn_type->t_tspec != ctn->tn_type->t_tspec)
			rn = convert(NOOP, 0, ctn->tn_type, rn);
		rn = mktnode(MULT, rn->tn_type, rn, ctn);
		if (rn->tn_left->tn_op == CON)
			rn = fold(rn);
	}

	if ((op == ASSIGN || op == RETURN) && (lt == STRUCT || rt == STRUCT)) {
		if (rt != lt || ln->tn_type->t_str != rn->tn_type->t_str)
			lerror("bldasgn() 3");
		if (incompl(ln->tn_type)) {
			if (op == RETURN) {
				/* cannot return incomplete type */
				error(212);
			} else {
				/* unknown operand size, op %s */
				error(138, modtab[op].m_name);
			}
			return (NULL);
		}
	}

	if (op == SHLASS || op == SHRASS) {
		if (rt != INT) {
			rn = convert(NOOP, 0, gettyp(INT), rn);
			rt = INT;
		}
	} else {
		if (op == ASSIGN || lt != PTR) {
			if (lt != rt ||
			    (ln->tn_type->t_isfield && rn->tn_op == CON)) {
				rn = convert(op, 0, ln->tn_type, rn);
				rt = lt;
			}
		}
	}

	ntn = mktnode(op, ln->tn_type, ln, rn);

	return (ntn);
}

/*
 * Get length of type tp->t_subt.
 */
static tnode_t *
plength(tp)
	type_t	*tp;
{
	int	elem, elsz;
	tspec_t	st;

	if (tp->t_tspec != PTR)
		lerror("plength() 1");
	tp = tp->t_subt;

	elem = 1;
	elsz = 0;

	while (tp->t_tspec == ARRAY) {
		elem *= tp->t_dim;
		tp = tp->t_subt;
	}

	switch (tp->t_tspec) {
	case FUNC:
		/* pointer to function is not allowed here */
		error(110);
		break;
	case VOID:
		/* cannot do pointer arithmetic on operand of ... */
		(void)gnuism(136);
		break;
	case STRUCT:
	case UNION:
		if ((elsz = tp->t_str->size) == 0)
			/* cannot do pointer arithmetic on operand of ... */
			error(136);
		break;
	case ENUM:
		if (incompl(tp)) {
			/* cannot do pointer arithmetic on operand of ... */
			warning(136);
		}
		/* FALLTHROUGH */
	default:
		if ((elsz = size(tp->t_tspec)) == 0) {
			/* cannot do pointer arithmetic on operand of ... */
			error(136);
		} else if (elsz == -1) {
			lerror("plength() 2");
		}
		break;
	}

	if (elem == 0 && elsz != 0) {
		/* cannot do pointer arithmetic on operand of ... */
		error(136);
	}

	if (elsz == 0)
		elsz = CHAR_BIT;

#if PTRDIFF_IS_LONG
	st = LONG;
#else
	st = INT;
#endif

	return (getinode(st, (quad_t)(elem * elsz / CHAR_BIT)));
}

#ifdef XXX_BROKEN_GCC
static int
quad_t_eq(x, y)
	quad_t x, y;
{
	return (x == y);
}

static int
u_quad_t_eq(x, y)
	u_quad_t x, y;
{
	return (x == y);
}
#endif

/*
 * Do only as much as necessary to compute constant expressions.
 * Called only if the operator allows folding and (both) operands
 * are constants.
 */
static tnode_t *
fold(tn)
	tnode_t	*tn;
{
	val_t	*v;
	tspec_t	t;
	int	utyp, ovfl;
	quad_t	sl, sr, q, mask;
	u_quad_t ul, ur;
	tnode_t	*cn;

	v = xcalloc(1, sizeof (val_t));
	v->v_tspec = t = tn->tn_type->t_tspec;

	utyp = t == PTR || isutyp(t);
	ul = sl = tn->tn_left->tn_val->v_quad;
	if (modtab[tn->tn_op].m_binary)
		ur = sr = tn->tn_right->tn_val->v_quad;

	ovfl = 0;

	switch (tn->tn_op) {
	case UPLUS:
		q = sl;
		break;
	case UMINUS:
		q = -sl;
		if (msb(q, t, -1) == msb(sl, t, -1))
			ovfl = 1;
		break;
	case COMPL:
		q = ~sl;
		break;
	case MULT:
		q = utyp ? ul * ur : sl * sr;
		if (msb(q, t, -1) != (msb(sl, t, -1) ^ msb(sr, t, -1)))
			ovfl = 1;
		break;
	case DIV:
		if (sr == 0) {
			/* division by 0 */
			error(139);
			q = utyp ? UQUAD_MAX : QUAD_MAX;
		} else {
			q = utyp ? ul / ur : sl / sr;
		}
		break;
	case MOD:
		if (sr == 0) {
			/* modulus by 0 */
			error(140);
			q = 0;
		} else {
			q = utyp ? ul % ur : sl % sr;
		}
		break;
	case PLUS:
		q = utyp ? ul + ur : sl + sr;
		if (msb(sl, t, -1)  != 0 && msb(sr, t, -1) != 0) {
			if (msb(q, t, -1) == 0)
				ovfl = 1;
		} else if (msb(sl, t, -1) == 0 && msb(sr, t, -1) == 0) {
			if (msb(q, t, -1) != 0)
				ovfl = 1;
		}
		break;
	case MINUS:
		q = utyp ? ul - ur : sl - sr;
		if (msb(sl, t, -1) != 0 && msb(sr, t, -1) == 0) {
			if (msb(q, t, -1) == 0)
				ovfl = 1;
		} else if (msb(sl, t, -1) == 0 && msb(sr, t, -1) != 0) {
			if (msb(q, t, -1) != 0)
				ovfl = 1;
		}
		break;
	case SHL:
		q = utyp ? ul << sr : sl << sr;
		break;
	case SHR:
		/*
		 * The sign must be explizitly extended because
		 * shifts of signed values are implementation dependent.
		 */
		q = ul >> sr;
		q = xsign(q, t, size(t) - (int)sr);
		break;
	case LT:
		q = utyp ? ul < ur : sl < sr;
		break;
	case LE:
		q = utyp ? ul <= ur : sl <= sr;
		break;
	case GE:
		q = utyp ? ul >= ur : sl >= sr;
		break;
	case GT:
		q = utyp ? ul > ur : sl > sr;
		break;
	case EQ:
#ifdef XXX_BROKEN_GCC
		q = utyp ? u_quad_t_eq(ul, ur) : quad_t_eq(sl, sr);
#else
		q = utyp ? ul == ur : sl == sr;
#endif
		break;
	case NE:
		q = utyp ? ul != ur : sl != sr;
		break;
	case AND:
		q = utyp ? ul & ur : sl & sr;
		break;
	case XOR:
		q = utyp ? ul ^ ur : sl ^ sr;
		break;
	case OR:
		q = utyp ? ul | ur : sl | sr;
		break;
	default:
		lerror("fold() 5");
	}

	mask = qlmasks[size(t)];

	/* XXX does not work for quads. */
	if (ovfl || ((q | mask) != ~(u_quad_t)0 && (q & ~mask) != 0)) {
		if (hflag)
			/* integer overflow detected, op %s */
			warning(141, modtab[tn->tn_op].m_name);
	}

	v->v_quad = xsign(q, t, -1);

	cn = getcnode(tn->tn_type, v);

	return (cn);
}

#ifdef XXX_BROKEN_GCC
int
ldbl_t_neq(x, y)
	ldbl_t x, y;
{
	return (x != y);
}
#endif

/*
 * Same for operators whose operands are compared with 0 (test context).
 */
static tnode_t *
foldtst(tn)
	tnode_t	*tn;
{
	int	l, r;
	val_t	*v;

	v = xcalloc(1, sizeof (val_t));
	v->v_tspec = tn->tn_type->t_tspec;
	if (tn->tn_type->t_tspec != INT)
		lerror("foldtst() 1");

	if (isftyp(tn->tn_left->tn_type->t_tspec)) {
#ifdef XXX_BROKEN_GCC
		l = ldbl_t_neq(tn->tn_left->tn_val->v_ldbl, 0.0);
#else
		l = tn->tn_left->tn_val->v_ldbl != 0.0;
#endif
	} else {
		l = tn->tn_left->tn_val->v_quad != 0;
	}

	if (modtab[tn->tn_op].m_binary) {
		if (isftyp(tn->tn_right->tn_type->t_tspec)) {
#ifdef XXX_BROKEN_GCC
			r = ldbl_t_neq(tn->tn_right->tn_val->v_ldbl, 0.0);
#else
			r = tn->tn_right->tn_val->v_ldbl != 0.0;
#endif
		} else {
			r = tn->tn_right->tn_val->v_quad != 0;
		}
	}

	switch (tn->tn_op) {
	case NOT:
		if (hflag)
			/* constant argument to NOT */
			warning(239);
		v->v_quad = !l;
		break;
	case LOGAND:
		v->v_quad = l && r;
		break;
	case LOGOR:
		v->v_quad = l || r;
		break;
	default:
		lerror("foldtst() 1");
	}

	return (getcnode(tn->tn_type, v));
}

/*
 * Same for operands with floating point type.
 */
static tnode_t *
foldflt(tn)
	tnode_t	*tn;
{
	val_t	*v;
	tspec_t	t;
	ldbl_t	l, r;

	v = xcalloc(1, sizeof (val_t));
	v->v_tspec = t = tn->tn_type->t_tspec;

	if (!isftyp(t))
		lerror("foldflt() 1");

	if (t != tn->tn_left->tn_type->t_tspec)
		lerror("foldflt() 2");
	if (modtab[tn->tn_op].m_binary && t != tn->tn_right->tn_type->t_tspec)
		lerror("foldflt() 3");

	l = tn->tn_left->tn_val->v_ldbl;
	if (modtab[tn->tn_op].m_binary)
		r = tn->tn_right->tn_val->v_ldbl;

	switch (tn->tn_op) {
	case UPLUS:
		v->v_ldbl = l;
		break;
	case UMINUS:
		v->v_ldbl = -l;
		break;
	case MULT:
		v->v_ldbl = l * r;
		break;
	case DIV:
		if (r == 0.0) {
			/* division by 0 */
			error(139);
			if (t == FLOAT) {
				v->v_ldbl = l < 0 ? -FLT_MAX : FLT_MAX;
			} else if (t == DOUBLE) {
				v->v_ldbl = l < 0 ? -DBL_MAX : DBL_MAX;
			} else {
				v->v_ldbl = l < 0 ? -LDBL_MAX : LDBL_MAX;
			}
		} else {
			v->v_ldbl = l / r;
		}
		break;
	case PLUS:
		v->v_ldbl = l + r;
		break;
	case MINUS:
		v->v_ldbl = l - r;
		break;
	case LT:
		v->v_quad = l < r;
		break;
	case LE:
		v->v_quad = l <= r;
		break;
	case GE:
		v->v_quad = l >= r;
		break;
	case GT:
		v->v_quad = l > r;
		break;
	case EQ:
		v->v_quad = l == r;
		break;
	case NE:
		v->v_quad = l != r;
		break;
	default:
		lerror("foldflt() 4");
	}

	if (isnan((double)v->v_ldbl))
		lerror("foldflt() 5");
	if (isinf((double)v->v_ldbl) ||
	    (t == FLOAT &&
	     (v->v_ldbl > FLT_MAX || v->v_ldbl < -FLT_MAX)) ||
	    (t == DOUBLE &&
	     (v->v_ldbl > DBL_MAX || v->v_ldbl < -DBL_MAX))) {
		/* floating point overflow detected, op %s */
		warning(142, modtab[tn->tn_op].m_name);
		if (t == FLOAT) {
			v->v_ldbl = v->v_ldbl < 0 ? -FLT_MAX : FLT_MAX;
		} else if (t == DOUBLE) {
			v->v_ldbl = v->v_ldbl < 0 ? -DBL_MAX : DBL_MAX;
		} else {
			v->v_ldbl = v->v_ldbl < 0 ? -LDBL_MAX: LDBL_MAX;
		}
	}

	return (getcnode(tn->tn_type, v));
}

/*
 * Create a constant node for sizeof.
 */
tnode_t *
bldszof(tp)
	type_t	*tp;
{
	int	elem, elsz;
	tspec_t	st;

	elem = 1;
	while (tp->t_tspec == ARRAY) {
		elem *= tp->t_dim;
		tp = tp->t_subt;
	}
	if (elem == 0) {
		/* cannot take size of incomplete type */
		error(143);
		elem = 1;
	}
	switch (tp->t_tspec) {
	case FUNC:
		/* cannot take size of function */
		error(144);
		elsz = 1;
		break;
	case STRUCT:
	case UNION:
		if (incompl(tp)) {
			/* cannot take size of incomplete type */
			error(143);
			elsz = 1;
		} else {
			elsz = tp->t_str->size;
		}
		break;
	case ENUM:
		if (incompl(tp)) {
			/* cannot take size of incomplete type */
			warning(143);
		}
		/* FALLTHROUGH */
	default:
		if (tp->t_isfield) {
			/* cannot take size of bit-field */
			error(145);
		}
		if (tp->t_tspec == VOID) {
			/* cannot take size of void */
			error(146);
			elsz = 1;
		} else {
			elsz = size(tp->t_tspec);
			if (elsz <= 0)
				lerror("bldszof() 1");
		}
		break;
	}

#if SIZEOF_IS_ULONG
	st = ULONG;
#else
	st = UINT;
#endif

	return (getinode(st, (quad_t)(elem * elsz / CHAR_BIT)));
}

/*
 * Type casts.
 */
tnode_t *
cast(tn, tp)
	tnode_t	*tn;
	type_t	*tp;
{
	tspec_t	nt, ot;

	if (tn == NULL)
		return (NULL);

	tn = cconv(tn);

	nt = tp->t_tspec;
	ot = tn->tn_type->t_tspec;

	if (nt == VOID) {
		/*
		 * XXX ANSI C requires scalar types or void (Plauger&Brodie).
		 * But this seams really questionable.
		 */
	} else if (nt == STRUCT || nt == UNION || nt == ARRAY || nt == FUNC) {
		/* invalid cast expression */
		error(147);
		return (NULL);
	} else if (ot == STRUCT || ot == UNION) {
		/* invalid cast expression */
		error(147);
		return (NULL);
	} else if (ot == VOID) {
		/* improper cast of void expression */
		error(148);
		return (NULL);
	} else if (isityp(nt) && issclt(ot)) {
		/* ok */
	} else if (isftyp(nt) && isatyp(ot)) {
		/* ok */
	} else if (nt == PTR && isityp(ot)) {
		/* ok */
	} else if (nt == PTR && ot == PTR) {
		if (!tp->t_subt->t_const && tn->tn_type->t_subt->t_const) {
			if (hflag)
				/* cast discards 'const' from ... */
				warning(275);
		}
	} else {
		/* invalid cast expression */
		error(147);
		return (NULL);
	}

	tn = convert(CVT, 0, tp, tn);
	tn->tn_cast = 1;

	return (tn);
}

/*
 * Create the node for a function argument.
 * All necessary conversions and type checks are done in funccall(), because
 * in funcarg() we have no information about expected argument types.
 */
tnode_t *
funcarg(args, arg)
	tnode_t	*args, *arg;
{
	tnode_t	*ntn;

	/*
	 * If there was a serious error in the expression for the argument,
	 * create a dummy argument so the positions of the remaining arguments
	 * will not change.
	 */
	if (arg == NULL)
		arg = getinode(INT, (quad_t)0);

	ntn = mktnode(PUSH, arg->tn_type, arg, args);

	return (ntn);
}

/*
 * Create the node for a function call. Also check types of
 * function arguments and insert conversions, if necessary.
 */
tnode_t *
funccall(func, args)
	tnode_t	*func, *args;
{
	tnode_t	*ntn;
	op_t	fcop;

	if (func == NULL)
		return (NULL);

	if (func->tn_op == NAME && func->tn_type->t_tspec == FUNC) {
		fcop = CALL;
	} else {
		fcop = ICALL;
	}

	/*
	 * after cconv() func will always be a pointer to a function
	 * if it is a valid function designator.
	 */
	func = cconv(func);

	if (func->tn_type->t_tspec != PTR ||
	    func->tn_type->t_subt->t_tspec != FUNC) {
		/* illegal function */
		error(149);
		return (NULL);
	}

	args = chkfarg(func->tn_type->t_subt, args);

	ntn = mktnode(fcop, func->tn_type->t_subt->t_subt, func, args);

	return (ntn);
}

/*
 * Check types of all function arguments and insert conversions,
 * if necessary.
 */
static tnode_t *
chkfarg(ftp, args)
	type_t	*ftp;		/* type of called function */
	tnode_t	*args;		/* arguments */
{
	tnode_t	*arg;
	sym_t	*asym;
	tspec_t	at;
	int	narg, npar, n, i;

	/* get # of args in the prototype */
	npar = 0;
	for (asym = ftp->t_args; asym != NULL; asym = asym->s_nxt)
		npar++;

	/* get # of args in function call */
	narg = 0;
	for (arg = args; arg != NULL; arg = arg->tn_right)
		narg++;

	asym = ftp->t_args;
	if (ftp->t_proto && npar != narg && !(ftp->t_vararg && npar < narg)) {
		/* argument mismatch: %d arg%s passed, %d expected */
		error(150, narg, narg > 1 ? "s" : "", npar);
		asym = NULL;
	}
	
	for (n = 1; n <= narg; n++) {

		/*
		 * The rightmost argument is at the top of the argument
		 * subtree.
		 */
		for (i = narg, arg = args; i > n; i--, arg = arg->tn_right) ;

		/* some things which are always not allowd */
		if ((at = arg->tn_left->tn_type->t_tspec) == VOID) {
			/* void expressions may not be arguments, arg #%d */
			error(151, n);
			return (NULL);
		} else if ((at == STRUCT || at == UNION) &&
			   incompl(arg->tn_left->tn_type)) {
			/* argument cannot have unknown size, arg #%d */
			error(152, n);
			return (NULL);
		} else if (isityp(at) && arg->tn_left->tn_type->t_isenum &&
			   incompl(arg->tn_left->tn_type)) {
			/* argument cannot have unknown size, arg #%d */
			warning(152, n);
		}

		/* class conversions (arg in value context) */
		arg->tn_left = cconv(arg->tn_left);

		if (asym != NULL) {
			arg->tn_left = parg(n, asym->s_type, arg->tn_left);
		} else {
			arg->tn_left = promote(NOOP, 1, arg->tn_left);
		}
		arg->tn_type = arg->tn_left->tn_type;

		if (asym != NULL)
			asym = asym->s_nxt;
	}

	return (args);
}

/*
 * Compare the type of an argument with the corresponding type of a
 * prototype parameter. If it is a valid combination, but both types
 * are not the same, insert a conversion to convert the argument into
 * the type of the parameter.
 */
static tnode_t *
parg(n, tp, tn)
	int	n;		/* pos of arg */
	type_t	*tp;		/* expected type (from prototype) */
	tnode_t	*tn;		/* argument */
{
	tnode_t	*ln;
	int	warn;

	ln = xcalloc(1, sizeof (tnode_t));
	ln->tn_type = tduptyp(tp);
	ln->tn_type->t_const = 0;
	ln->tn_lvalue = 1;
	if (typeok(FARG, n, ln, tn)) {
		if (!eqtype(tp, tn->tn_type, 1, 0, (warn = 0, &warn)) || warn)
			tn = convert(FARG, n, tp, tn);
	}
	free(ln);
	return (tn);
}

/*
 * Return the value of an integral constant expression.
 * If the expression is not constant or its type is not an integer
 * type, an error message is printed.
 */
val_t *
constant(tn)
	tnode_t	*tn;
{
	val_t	*v;

	if (tn != NULL)
		tn = cconv(tn);
	if (tn != NULL)
		tn = promote(NOOP, 0, tn);

	v = xcalloc(1, sizeof (val_t));

	if (tn == NULL) {
		if (nerr == 0)
			lerror("constant() 1");
		v->v_tspec = INT;
		v->v_quad = 1;
		return (v);
	}

	v->v_tspec = tn->tn_type->t_tspec;

	if (tn->tn_op == CON) {
		if (tn->tn_type->t_tspec != tn->tn_val->v_tspec)
			lerror("constant() 2");
		if (isityp(tn->tn_val->v_tspec)) {
			v->v_ansiu = tn->tn_val->v_ansiu;
			v->v_quad = tn->tn_val->v_quad;
			return (v);
		}
		v->v_quad = tn->tn_val->v_ldbl;
	} else {
		v->v_quad = 1;
	}

	/* integral constant expression expected */
	error(55);

	if (!isityp(v->v_tspec))
		v->v_tspec = INT;

	return (v);
}

/*
 * Perform some tests on expressions which can't be done in build() and
 * functions called by build(). These tests must be done here because
 * we need some information about the context in which the operations
 * are performed.
 * After all tests are performed, expr() frees the memory which is used
 * for the expression.
 */
void
expr(tn, vctx, tctx)
	tnode_t	*tn;
	int	vctx, tctx;
{
	if (tn == NULL && nerr == 0)
		lerror("expr() 1");

	if (tn == NULL) {
		tfreeblk();
		return;
	}

	/* expr() is also called in global initialisations */
	if (dcs->d_ctx != EXTERN)
		chkreach();

	chkmisc(tn, vctx, tctx, !tctx, 0, 0, 0);
	if (tn->tn_op == ASSIGN) {
		if (hflag && tctx)
			/* assignment in conditional context */
			warning(159);
	} else if (tn->tn_op == CON) {
		if (hflag && tctx && !ccflg)
			/* constant in conditional context */
			warning(161);
	}
	if (!modtab[tn->tn_op].m_sideeff) {
		/*
		 * for left operands of COMMA this warning is already
		 * printed
		 */
		if (tn->tn_op != COMMA && !vctx && !tctx)
			nulleff(tn);
	}
	if (dflag)
		displexpr(tn, 0);

	/* free the tree memory */
	tfreeblk();
}

static void
nulleff(tn)
	tnode_t	*tn;
{
	if (!hflag)
		return;

	while (!modtab[tn->tn_op].m_sideeff) {
		if (tn->tn_op == CVT && tn->tn_type->t_tspec == VOID) {
			tn = tn->tn_left;
		} else if (tn->tn_op == LOGAND || tn->tn_op == LOGOR) {
			/*
			 * && and || have a side effect if the right operand
			 * has a side effect.
			 */
			tn = tn->tn_right;
		} else if (tn->tn_op == QUEST) {
			/*
			 * ? has a side effect if at least one of its right
			 * operands has a side effect
			 */
			tn = tn->tn_right;
		} else if (tn->tn_op == COLON) {
			/*
			 * : has a side effect if at least one of its operands
			 * has a side effect
			 */
			if (modtab[tn->tn_left->tn_op].m_sideeff) {
				tn = tn->tn_left;
			} else if (modtab[tn->tn_right->tn_op].m_sideeff) {
				tn = tn->tn_right;
			} else {
				break;
			}
		} else {
			break;
		}
	}
	if (!modtab[tn->tn_op].m_sideeff)
		/* expression has null effect */
		warning(129);
}

/*
 * Dump an expression to stdout
 * only used for debugging
 */
static void
displexpr(tn, offs)
	tnode_t	*tn;
	int	offs;
{
	u_quad_t uq;

	if (tn == NULL) {
		(void)printf("%*s%s\n", offs, "", "NULL");
		return;
	}
	(void)printf("%*sop %s  ", offs, "", modtab[tn->tn_op].m_name);

	if (tn->tn_op == NAME) {
		(void)printf("%s: %s ",
			     tn->tn_sym->s_name, scltoa(tn->tn_sym->s_scl));
	} else if (tn->tn_op == CON && isftyp(tn->tn_type->t_tspec)) {
		(void)printf("%#g ", (double)tn->tn_val->v_ldbl);
	} else if (tn->tn_op == CON && isityp(tn->tn_type->t_tspec)) {
		uq = tn->tn_val->v_quad;
		(void)printf("0x %08lx %08lx ", (long)(uq >> 32) & 0xffffffffl,
			     (long)uq & 0xffffffffl);
	} else if (tn->tn_op == CON) {
		if (tn->tn_type->t_tspec != PTR)
			lerror("displexpr() 1");
		(void)printf("0x%0*lx ", (int)(sizeof (void *) * CHAR_BIT / 4),
			     (u_long)tn->tn_val->v_quad);
	} else if (tn->tn_op == STRING) {
		if (tn->tn_strg->st_tspec == CHAR) {
			(void)printf("\"%s\"", tn->tn_strg->st_cp);
		} else {
			char	*s;
			size_t	n;
			n = MB_CUR_MAX * (tn->tn_strg->st_len + 1);
			s = xmalloc(n);
			(void)wcstombs(s, tn->tn_strg->st_wcp, n);
			(void)printf("L\"%s\"", s);
			free(s);
		}
		(void)printf(" ");
	} else if (tn->tn_op == FSEL) {
		(void)printf("o=%d, l=%d ", tn->tn_type->t_foffs,
			     tn->tn_type->t_flen);
	}
	(void)printf("%s\n", ttos(tn->tn_type));
	if (tn->tn_op == NAME || tn->tn_op == CON || tn->tn_op == STRING)
		return;
	displexpr(tn->tn_left, offs + 2);
	if (modtab[tn->tn_op].m_binary ||
	    (tn->tn_op == PUSH && tn->tn_right != NULL)) {
		displexpr(tn->tn_right, offs + 2);
	}
}

/*
 * Called by expr() to recursively perform some tests.
 */
/* ARGSUSED */
void
chkmisc(tn, vctx, tctx, eqwarn, fcall, rvdisc, szof)
	tnode_t	*tn;
	int	vctx, tctx, eqwarn, fcall, rvdisc, szof;
{
	tnode_t	*ln, *rn;
	mod_t	*mp;
	int	nrvdisc, cvctx, ctctx;
	op_t	op;
	scl_t	sc;
	dinfo_t	*di;

	if (tn == NULL)
		return;

	ln = tn->tn_left;
	rn = tn->tn_right;
	mp = &modtab[op = tn->tn_op];

	switch (op) {
	case AMPER:
		if (ln->tn_op == NAME && (reached || rchflg)) {
			if (!szof)
				setsflg(ln->tn_sym);
			setuflg(ln->tn_sym, fcall, szof);
		}
		if (ln->tn_op == STAR && ln->tn_left->tn_op == PLUS)
			/* check the range of array indices */
			chkaidx(ln->tn_left, 1);
		break;
	case LOAD:
		if (ln->tn_op == STAR && ln->tn_left->tn_op == PLUS)
			/* check the range of array indices */
			chkaidx(ln->tn_left, 0);
		/* FALLTHROUGH */
	case PUSH:
	case INCBEF:
	case DECBEF:
	case INCAFT:
	case DECAFT:
	case ADDASS:
	case SUBASS:
	case MULASS:
	case DIVASS:
	case MODASS:
	case ANDASS:
	case ORASS:
	case XORASS:
	case SHLASS:
	case SHRASS:
		if (ln->tn_op == NAME && (reached || rchflg)) {
			sc = ln->tn_sym->s_scl;
			/*
			 * Look if there was a asm statement in one of the
			 * compound statements we are in. If not, we don't
			 * print a warning.
			 */
			for (di = dcs; di != NULL; di = di->d_nxt) {
				if (di->d_asm)
					break;
			}
			if (sc != EXTERN && sc != STATIC &&
			    !ln->tn_sym->s_set && !szof && di == NULL) {
				/* %s may be used before set */
				warning(158, ln->tn_sym->s_name);
				setsflg(ln->tn_sym);
			}
			setuflg(ln->tn_sym, 0, 0);
		}
		break;
	case ASSIGN:
		if (ln->tn_op == NAME && !szof && (reached || rchflg)) {
			setsflg(ln->tn_sym);
			if (ln->tn_sym->s_scl == EXTERN)
				outusg(ln->tn_sym);
		}
		if (ln->tn_op == STAR && ln->tn_left->tn_op == PLUS)
			/* check the range of array indices */
			chkaidx(ln->tn_left, 0);
		break;
	case CALL:
		if (ln->tn_op != AMPER || ln->tn_left->tn_op != NAME)
			lerror("chkmisc() 1");
		if (!szof)
			outcall(tn, vctx || tctx, rvdisc);
		break;
	case EQ:
		/* equality operator "==" found where "=" was exp. */
		if (hflag && eqwarn)
			warning(160);
		break;
	case CON:
	case NAME:
	case STRING:
		return;
		/* LINTED (enumeration values not handled in switch) */
	default:
	}

	cvctx = mp->m_vctx;
	ctctx = mp->m_tctx;
	/*
	 * values of operands of ':' are not used if the type of at least
	 * one of the operands (for gcc compatibility) is void
	 * XXX test/value context of QUEST should probably be used as
	 * context for both operands of COLON
	 */
	if (op == COLON && tn->tn_type->t_tspec == VOID)
		cvctx = ctctx = 0;
	nrvdisc = op == CVT && tn->tn_type->t_tspec == VOID;
	chkmisc(ln, cvctx, ctctx, mp->m_eqwarn, op == CALL, nrvdisc, szof);

	switch (op) {
	case PUSH:
		if (rn != NULL)
			chkmisc(rn, 0, 0, mp->m_eqwarn, 0, 0, szof);
		break;
	case LOGAND:
	case LOGOR:
		chkmisc(rn, 0, 1, mp->m_eqwarn, 0, 0, szof);
		break;
	case COLON:
		chkmisc(rn, cvctx, ctctx, mp->m_eqwarn, 0, 0, szof);
		break;
	default:
		if (mp->m_binary)
			chkmisc(rn, 1, 0, mp->m_eqwarn, 0, 0, szof);
		break;
	}

}

/*
 * Checks the range of array indices, if possible.
 * amper is set if only the address of the element is used. This
 * means that the index is allowd to refere to the first element
 * after the array.
 */
static void
chkaidx(tn, amper)
	tnode_t	*tn;
	int	amper;
{
	int	dim;
	tnode_t	*ln, *rn;
	int	elsz;
	quad_t	con;

	ln = tn->tn_left;
	rn = tn->tn_right;

	/* We can only check constant indices. */
	if (rn->tn_op != CON)
		return;

	/* Return if the left node does not stem from an array. */
	if (ln->tn_op != AMPER)
		return;
	if (ln->tn_left->tn_op != STRING && ln->tn_left->tn_op != NAME)
		return;
	if (ln->tn_left->tn_type->t_tspec != ARRAY)
		return;
	
	/*
	 * For incomplete array types, we can print a warning only if
	 * the index is negative.
	 */
	if (incompl(ln->tn_left->tn_type) && rn->tn_val->v_quad >= 0)
		return;

	/* Get the size of one array element */
	if ((elsz = length(ln->tn_type->t_subt, NULL)) == 0)
		return;
	elsz /= CHAR_BIT;

	/* Change the unit of the index from bytes to element size. */
	if (isutyp(rn->tn_type->t_tspec)) {
		con = (u_quad_t)rn->tn_val->v_quad / elsz;
	} else {
		con = rn->tn_val->v_quad / elsz;
	}

	dim = ln->tn_left->tn_type->t_dim + (amper ? 1 : 0);

	if (!isutyp(rn->tn_type->t_tspec) && con < 0) {
		/* array subscript cannot be negative: %ld */
		warning(167, (long)con);
	} else if (dim > 0 && (u_quad_t)con >= dim) {
		/* array subscript cannot be > %d: %ld */
		warning(168, dim - 1, (long)con);
	}
}

/*
 * Check for ordered comparisons of unsigned values with 0.
 */
static void
chkcomp(op, ln, rn)
	op_t	op;
	tnode_t	*ln, *rn;
{
	tspec_t	lt, rt;
	mod_t	*mp;

	lt = ln->tn_type->t_tspec;
	rt = rn->tn_type->t_tspec;
	mp = &modtab[op];

	if (ln->tn_op != CON && rn->tn_op != CON)
		return;

	if (!isityp(lt) || !isityp(rt))
		return;

	if ((hflag || pflag) && lt == CHAR && rn->tn_op == CON &&
	    (rn->tn_val->v_quad < 0 ||
	     rn->tn_val->v_quad > ~(~0 << (CHAR_BIT - 1)))) {
		/* nonportable character comparison, op %s */
		warning(230, mp->m_name);
		return;
	}
	if ((hflag || pflag) && rt == CHAR && ln->tn_op == CON &&
	    (ln->tn_val->v_quad < 0 ||
	     ln->tn_val->v_quad > ~(~0 << (CHAR_BIT - 1)))) {
		/* nonportable character comparison, op %s */
		warning(230, mp->m_name);
		return;
	}
	if (isutyp(lt) && !isutyp(rt) &&
	    rn->tn_op == CON && rn->tn_val->v_quad <= 0) {
		if (rn->tn_val->v_quad < 0) {
			/* comparison of %s with %s, op %s */
			warning(162, tyname(ln->tn_type), "negative constant",
				mp->m_name);
		} else if (op == LT || op == GE || (hflag && op == LE)) {
			/* comparison of %s with %s, op %s */
			warning(162, tyname(ln->tn_type), "0", mp->m_name);
		}
		return;
	}
	if (isutyp(rt) && !isutyp(lt) &&
	    ln->tn_op == CON && ln->tn_val->v_quad <= 0) {
		if (ln->tn_val->v_quad < 0) {
			/* comparison of %s with %s, op %s */
			warning(162, "negative constant", tyname(rn->tn_type),
				mp->m_name);
		} else if (op == GT || op == LE || (hflag && op == GE)) {
			/* comparison of %s with %s, op %s */
			warning(162, "0", tyname(rn->tn_type), mp->m_name);
		}
		return;
	}
}

/*
 * Takes an expression an returns 0 if this expression can be used
 * for static initialisation, otherwise -1.
 *
 * Constant initialisation expressions must be costant or an address
 * of a static object with an optional offset. In the first case,
 * the result is returned in *offsp. In the second case, the static
 * object is returned in *symp and the offset in *offsp.
 *
 * The expression can consist of PLUS, MINUS, AMPER, NAME, STRING and
 * CON. Type conversions are allowed if they do not change binary
 * representation (including width).
 */
int
conaddr(tn, symp, offsp)
	tnode_t	*tn;
	sym_t	**symp;
	ptrdiff_t *offsp;
{
	sym_t	*sym;
	ptrdiff_t offs1, offs2;
	tspec_t	t, ot;

	switch (tn->tn_op) {
	case MINUS:
		if (tn->tn_right->tn_op != CON)
			return (-1);
		/* FALLTHROUGH */
	case PLUS:
		offs1 = offs2 = 0;
		if (tn->tn_left->tn_op == CON) {
			offs1 = (ptrdiff_t)tn->tn_left->tn_val->v_quad;
			if (conaddr(tn->tn_right, &sym, &offs2) == -1)
				return (-1);
		} else if (tn->tn_right->tn_op == CON) {
			offs2 = (ptrdiff_t)tn->tn_right->tn_val->v_quad;
			if (tn->tn_op == MINUS)
				offs2 = -offs2;
			if (conaddr(tn->tn_left, &sym, &offs1) == -1)
				return (-1);
		} else {
			return (-1);
		}
		*symp = sym;
		*offsp = offs1 + offs2;
		break;
	case AMPER:
		if (tn->tn_left->tn_op == NAME) {
			*symp = tn->tn_left->tn_sym;
			*offsp = 0;
		} else if (tn->tn_left->tn_op == STRING) {
			/*
			 * If this would be the front end of a compiler we
			 * would return a label instead of 0.
			 */
			*offsp = 0;
		}
		break;
	case CVT:
		t = tn->tn_type->t_tspec;
		ot = tn->tn_left->tn_type->t_tspec;
		if ((!isityp(t) && t != PTR) || (!isityp(ot) && ot != PTR)) {
			return (-1);
		} else if (psize(t) != psize(ot)) {
			return (-1);
		}
		if (conaddr(tn->tn_left, symp, offsp) == -1)
			return (-1);
		break;
	default:
		return (-1);
	}
	return (0);
}

/*
 * Concatenate two string constants.
 */
strg_t *
catstrg(strg1, strg2)
	strg_t	*strg1, *strg2;
{
	size_t	len1, len2, len;

	if (strg1->st_tspec != strg2->st_tspec) {
		/* cannot concatenate wide and regular string literals */
		error(292);
		return (strg1);
	}

	len = (len1 = strg1->st_len) + (len2 = strg2->st_len);

	if (strg1->st_tspec == CHAR) {
		strg1->st_cp = xrealloc(strg1->st_cp, len + 1);
		(void)memcpy(strg1->st_cp + len1, strg2->st_cp, len2 + 1);
		free(strg2->st_cp);
	} else {
		strg1->st_wcp = xrealloc(strg1->st_wcp,
					 (len + 1) * sizeof (wchar_t));
		(void)memcpy(strg1->st_wcp + len1, strg2->st_wcp,
			     (len2 + 1) * sizeof (wchar_t));
		free(strg2->st_wcp);
	}
	free(strg2);

	return (strg1);
}

/*
 * Print a warning if the given node has operands which should be
 * parenthesized.
 *
 * XXX Does not work if an operand is a constant expression. Constant
 * expressions are already folded.
 */
static void
precconf(tn)
	tnode_t	*tn;
{
	tnode_t	*ln, *rn;
	op_t	lop, rop;
	int	lparn, rparn;
	mod_t	*mp;
	int	warn;

	if (!hflag)
		return;

	mp = &modtab[tn->tn_op];

	lparn = 0;
	for (ln = tn->tn_left; ln->tn_op == CVT; ln = ln->tn_left)
		lparn |= ln->tn_parn;
	lparn |= ln->tn_parn;
	lop = ln->tn_op;

	if (mp->m_binary) {
		rparn = 0;
		for (rn = tn->tn_right; tn->tn_op == CVT; rn = rn->tn_left)
			rparn |= rn->tn_parn;
		rparn |= rn->tn_parn;
		rop = rn->tn_op;
	}

	warn = 0;

	switch (tn->tn_op) {
	case SHL:
	case SHR:
		if (!lparn && (lop == PLUS || lop == MINUS)) {
			warn = 1;
		} else if (!rparn && (rop == PLUS || rop == MINUS)) {
			warn = 1;
		}
		break;
	case LOGOR:
		if (!lparn && lop == LOGAND) {
			warn = 1;
		} else if (!rparn && rop == LOGAND) {
			warn = 1;
		}
		break;
	case AND:
	case XOR:
	case OR:
		if (!lparn && lop != tn->tn_op) {
			if (lop == PLUS || lop == MINUS) {
				warn = 1;
			} else if (lop == AND || lop == XOR) {
				warn = 1;
			}
		}
		if (!warn && !rparn && rop != tn->tn_op) {
			if (rop == PLUS || rop == MINUS) {
				warn = 1;
			} else if (rop == AND || rop == XOR) {
				warn = 1;
			}
		}
		break;
		/* LINTED (enumeration values not handled in switch) */
	default:
	}

	if (warn) {
		/* precedence confusion possible: parenthesize! */
		warning(169);
	}

}
