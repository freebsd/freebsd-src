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
static char sccsid[] = "@(#)parser4.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] = "@(#)$FreeBSD$";
#endif /* not lint */

#include <string.h>
#include "parser.h"

/*
 * |		3
 * ^		4
 * &		5
 * == !=	6
 * < <= > >=	7
 * << >>	8
 * + -		9
 * * / %	10
 */
p_expr3_10(level, v, flag)
register struct value *v;
char flag;
{
	struct value l, r;
	int op;
	char *opname;

	if ((level == 10 ? p_expr11(v, flag)
	     : p_expr3_10(level + 1, v, flag)) < 0)
		return -1;
	for (;;) {
		switch (level) {
		case 3:
			if (token != T_OR)
				return 0;
			opname = "|";
			break;
		case 4:
			if (token != T_XOR)
				return 0;
			opname = "^";
			break;
		case 5:
			if (token != T_AND)
				return 0;
			opname = "&";
			break;
		case 6:
			if (token == T_EQ)
				opname = "==";
			else if (token == T_NE)
				opname = "!=";
			else
				return 0;
			break;
		case 7:
			switch (token) {
			case T_LT:
				opname = "<";
				break;
			case T_LE:
				opname = "<=";
				break;
			case T_GT:
				opname = ">";
				break;
			case T_GE:
				opname = ">=";
				break;
			default:
				return 0;
			}
			break;
		case 8:
			if (token == T_LS)
				opname = "<<";
			else if (token == T_RS)
				opname = ">>";
			else
				return 0;
			break;
		case 9:
			if (token == T_PLUS)
				opname = "+";
			else if (token == T_MINUS)
				opname = "-";
			else
				return 0;
			break;
		case 10:
			switch (token) {
			case T_MUL:
				opname = "*";
				break;
			case T_DIV:
				opname = "/";
				break;
			case T_MOD:
				opname = "%";
				break;
			default:
				return 0;
			}
			break;
		}
		l = *v;
		if (l.v_type == V_ERR)
			flag = 0;

		op = token;
		(void) s_gettok();
		if ((level == 10 ? p_expr11(&r, flag)
		     : p_expr3_10(level + 1, &r, flag)) < 0) {
			p_synerror();
			val_free(l);
			return -1;
		}

		if (r.v_type == V_ERR)
			flag = 0;
		else switch (op) {
		case T_EQ:
		case T_NE:
		case T_LT:
		case T_LE:
		case T_GT:
		case T_GE:
		case T_PLUS:
			if (l.v_type == V_STR) {
				if (r.v_type == V_NUM)
					if (p_convstr(&r) < 0)
						flag = 0;
			} else
				if (r.v_type == V_STR)
					if (p_convstr(&l) < 0)
						flag = 0;
			break;
		case T_LS:
		case T_RS:
			if (r.v_type == V_STR) {
				char *p = r.v_str;
				r.v_type = V_NUM;
				r.v_num = strlen(p);
				str_free(p);
			}
			break;
		case T_OR:
		case T_XOR:
		case T_AND:
		case T_MINUS:
		case T_MUL:
		case T_DIV:
		case T_MOD:
		default:
			if (l.v_type == V_STR || r.v_type == V_STR) {
				p_error("%s: Numeric operands required.",
					opname);
				flag = 0;
			}
		}
		if (!flag) {
			val_free(l);
			val_free(r);
			v->v_type = V_ERR;
			if (p_abort())
				return -1;
			continue;
		}

		v->v_type = V_NUM;
		switch (op) {
		case T_EQ:
		case T_NE:
		case T_LT:
		case T_LE:
		case T_GT:
		case T_GE:
			if (l.v_type == V_STR) {
				int tmp = strcmp(l.v_str, r.v_str);
				str_free(l.v_str);
				str_free(r.v_str);
				l.v_type = V_NUM;
				l.v_num = tmp;
				r.v_type = V_NUM;
				r.v_num = 0;
			}
			break;
		}
		switch (op) {
		case T_OR:
			v->v_num = l.v_num | r.v_num;
			break;
		case T_XOR:
			v->v_num = l.v_num ^ r.v_num;
			break;
		case T_AND:
			v->v_num = l.v_num & r.v_num;
			break;
		case T_EQ:
			v->v_num = l.v_num == r.v_num;
			break;
		case T_NE:
			v->v_num = l.v_num != r.v_num;
			break;
		case T_LT:
			v->v_num = l.v_num < r.v_num;
			break;
		case T_LE:
			v->v_num = l.v_num <= r.v_num;
			break;
		case T_GT:
			v->v_num = l.v_num > r.v_num;
			break;
		case T_GE:
			v->v_num = l.v_num >= r.v_num;
			break;
		case T_LS:
			if (l.v_type == V_STR) {
				int i;
				if ((i = strlen(l.v_str)) > r.v_num)
					i = r.v_num;
				v->v_str = str_ncpy(l.v_str, i);
				v->v_type = V_STR;
			} else
				v->v_num = l.v_num << r.v_num;
			break;
		case T_RS:
			if (l.v_type == V_STR) {
				int i;
				if ((i = strlen(l.v_str)) > r.v_num)
					i -= r.v_num;
				else
					i = 0;
				v->v_str = str_cpy(l.v_str + i);
				v->v_type = V_STR;
			} else
				v->v_num = l.v_num >> r.v_num;
			break;
		case T_PLUS:
			if (l.v_type == V_STR) {
				v->v_str = str_cat(l.v_str, r.v_str);
				v->v_type = V_STR;
			} else
				v->v_num = l.v_num + r.v_num;
			break;
		case T_MINUS:
			v->v_num = l.v_num - r.v_num;
			break;
		case T_MUL:
			v->v_num = l.v_num * r.v_num;
			break;
		case T_DIV:
			v->v_num = l.v_num / r.v_num;
			break;
		case T_MOD:
			v->v_num = l.v_num % r.v_num;
			break;
		}
		val_free(l);
		val_free(r);
	}
	/*NOTREACHED*/
}
