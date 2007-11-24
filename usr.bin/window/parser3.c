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
static char sccsid[] = "@(#)parser3.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include "parser.h"

/*
 * =
 * ? :
 * ||
 * &&
 * |
 * ^
 * &
 * == !=
 * <= >=
 * << >>
 * + -
 * * / %
 * unary - + ~ !
 */
p_expr(v, flag)
register struct value *v;
char flag;
{
	struct value t;
	int ret;

	if (p_expr0(&t, flag) < 0)
		return -1;

	if (token != T_ASSIGN) {
		*v = t;
		return 0;
	}
	switch (t.v_type) {
	case V_NUM:
		p_error("%d: Not a variable.", t.v_num);
	case V_ERR:
		t.v_str = 0;
		break;
	}
	ret = p_assign(t.v_str, v, flag);
	if (t.v_str != 0)
		str_free(t.v_str);
	return ret;
}

/*
 * ? :
 */
p_expr0(v, flag)
register struct value *v;
char flag;
{
	struct value t;
	char true;

	if (p_expr1(v, flag) < 0)
		return -1;
	if (token != T_QUEST)
		return 0;
	switch (v->v_type) {
	case V_NUM:
		true = v->v_num != 0;
		break;
	case V_STR:
		p_error("?: Numeric left operand required.");
		str_free(v->v_str);
		v->v_type = V_ERR;
	case V_ERR:
		flag = 0;
		break;
	}
	(void) s_gettok();
	v->v_type = V_ERR;
	if ((flag && true ? p_expr1(v, 1) : p_expr1(&t, 0)) < 0)
		return -1;
	if (token != T_COLON) {
		val_free(*v);
		p_synerror();
		return -1;
	}
	(void) s_gettok();
	return flag && !true ? p_expr1(v, 1) : p_expr1(&t, 0);
}

/*
 * ||
 */
p_expr1(v, flag)
register struct value *v;
char flag;
{
	char true = 0;

	if (p_expr2(v, flag) < 0)
		return -1;
	if (token != T_OROR)
		return 0;
	for (;;) {
		switch (v->v_type) {
		case V_NUM:
			v->v_num = true = true || v->v_num != 0;
			break;
		case V_STR:
			p_error("||: Numeric operands required.");
			str_free(v->v_str);
			v->v_type = V_ERR;
		case V_ERR:
			flag = 0;
			break;
		}
		if (token != T_OROR)
			return 0;
		(void) s_gettok();
		if (p_expr2(v, flag && !true) < 0)
			return -1;
	}
}

/*
 * &&
 */
p_expr2(v, flag)
register struct value *v;
char flag;
{
	char true = 1;

	if (p_expr3_10(3, v, flag) < 0)
		return -1;
	if (token != T_ANDAND)
		return 0;
	for (;;) {
		switch (v->v_type) {
		case V_NUM:
			v->v_num = true = true && v->v_num != 0;
			break;
		case V_STR:
			p_error("&&: Numeric operands required.");
			str_free(v->v_str);
			v->v_type = V_ERR;
		case V_ERR:
			flag = 0;
			break;
		}
		if (token != T_ANDAND)
			return 0;
		(void) s_gettok();
		if (p_expr3_10(3, v, flag && true) < 0)
			return -1;
	}
	/*NOTREACHED*/
}
