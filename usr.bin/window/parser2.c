/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)parser2.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include "parser.h"
#include "var.h"
#include "lcmd.h"
#include "alias.h"

/*
 * name == 0 means we don't have a function name but
 * want to parse the arguments anyway.  flag == 0 in this case.
 */
p_function(name, v, flag)
char *name;
register struct value *v;
{
	struct value t;
	register struct lcmd_tab *c = 0;
	register struct alias *a = 0;
	register struct lcmd_arg *ap;		/* this arg */
	struct lcmd_arg *lp = 0;		/* list arg */
	register i;
	struct value av[LCMD_NARG + 1];
	register struct value *vp;

	if (name != 0)
		if (c = lcmd_lookup(name))
			name = c->lc_name;
		else if (a = alias_lookup(name))
			name = a->a_name;
		else {
			p_error("%s: No such command or alias.", name);
			flag = 0;
		}

	for (vp = av; vp < &av[LCMD_NARG + 1]; vp++)
		vp->v_type = V_ERR;

	if (token == T_LP)
		(void) s_gettok();
	i = 0;
	for (;;) {
		ap = 0;
		vp = 0;
		if (token == T_COMMA)		/* null argument */
			t.v_type = V_ERR;
		else {
			if (p_expr0(&t, flag) < 0)
				break;
			if (t.v_type == V_ERR)
				flag = 0;
		}
		if (token != T_ASSIGN) {
			if (i >= LCMD_NARG ||
			    c != 0 && (ap = lp) == 0 &&
			    (ap = c->lc_arg + i)->arg_name == 0) {
				p_error("%s: Too many arguments.", name);
				flag = 0;
			} else
				vp = &av[i++];
		} else {
			char *tmp;
			if (p_convstr(&t) < 0)
				goto abort;
			tmp = t.v_type == V_STR ? t.v_str : 0;
			(void) s_gettok();
			if (p_expr(&t, flag) < 0) {
				if (tmp)
					str_free(tmp);
				p_synerror();
				goto abort;
			}
			if (t.v_type == V_ERR)
				flag = 0;
			if (tmp) {
				if (c == 0) {
					/* an aliase */
					p_error("%s: Bad alias syntax.", name);
					flag = 0;
				} else {
					for (ap = c->lc_arg, vp = av;
					     ap != 0 && ap->arg_name != 0 &&
					     (*ap->arg_name == '\0' ||
					      !str_match(tmp, ap->arg_name,
							ap->arg_minlen));
					     ap++, vp++)
						;
					if (ap == 0 || ap->arg_name == 0) {
						p_error("%s: Unknown argument \"%s\".",
							name, tmp);
						flag = 0;
						ap = 0;
						vp = 0;
					}
				}
				str_free(tmp);
			}
		}
		if (ap != 0) {
			if (ap->arg_flags & ARG_LIST) {
				i = vp - av + 1;
				lp = ap;
			}
			if (vp->v_type != V_ERR) {
				if (*ap->arg_name)
					p_error("%s: Argument %d (%s) duplicated.",
						name, vp - av + 1,
						ap->arg_name);
				else
					p_error("%s: Argument %d duplicated.",
						name, vp - av + 1);
				flag = 0;
				vp = 0;
			} else if (t.v_type == V_ERR) {
				/* do nothing */
			} else if ((ap->arg_flags&ARG_TYPE) == ARG_NUM &&
				   t.v_type != V_NUM ||
				   (ap->arg_flags&ARG_TYPE) == ARG_STR &&
				   t.v_type != V_STR) {
				if (*ap->arg_name)
					p_error("%s: Argument %d (%s) type mismatch.",
						name, vp - av + 1,
						ap->arg_name);
				else
					p_error("%s: Argument %d type mismatch.",
						name, vp - av + 1);
				flag = 0;
				vp = 0;
			}
		}
		if (vp != 0)
			*vp = t;
		else
			val_free(t);
		if (token == T_COMMA)
			(void) s_gettok();
	}

	if (p_erred())
		flag = 0;
	if (token == T_RP)
		(void) s_gettok();
	else if (token != T_EOL && token != T_EOF)
		flag = 0;		/* look for legal follow set */
	v->v_type = V_ERR;
	if (flag)
		if (c != 0)
			(*c->lc_func)(v, av);
		else
			if (a->a_flags & A_INUSE)
				p_error("%s: Recursive alias.", a->a_name);
			else {
				a->a_flags |= A_INUSE;
				if (dolongcmd(a->a_buf, av, i) < 0)
					p_memerror();
				a->a_flags &= ~A_INUSE;
			}
	if (p_abort()) {
		val_free(*v);
		v->v_type = V_ERR;
		goto abort;
	}
	for (vp = av; vp < &av[LCMD_NARG]; vp++)
		val_free(*vp);
	return 0;
abort:
	for (vp = av; vp < &av[LCMD_NARG]; vp++)
		val_free(*vp);
	return -1;
}

p_assign(name, v, flag)
char *name;
struct value *v;
char flag;
{
	(void) s_gettok();

	if (p_expr(v, flag) < 0) {
		p_synerror();
		return -1;
	}
	switch (v->v_type) {
	case V_STR:
	case V_NUM:
		if (flag && var_set(name, v) == 0) {
			p_memerror();
			val_free(*v);
			return -1;
		}
		break;
	}
	return 0;
}
