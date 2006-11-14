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
static char sccsid[] = "@(#)var.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include "value.h"
#include "var.h"
#include "mystring.h"

struct var *
var_set1(head, name, v)
struct var **head;
char *name;
struct value *v;
{
	register struct var **p;
	register struct var *r;
	struct value val;

	/* do this first, easier to recover */
	val = *v;
	if (val.v_type == V_STR && val.v_str != 0 &&
	    (val.v_str = str_cpy(val.v_str)) == 0)
		return 0;
	if (*(p = var_lookup1(head, name)) == 0) {
		r = (struct var *) malloc(sizeof (struct var));
		if (r == 0) {
			val_free(val);
			return 0;
		}
		if ((r->r_name = str_cpy(name)) == 0) {
			val_free(val);
			free((char *) r);
			return 0;
		}
		r->r_left = r->r_right = 0;
		*p = r;
	} else {
		r = *p;
		val_free(r->r_val);
	}
	r->r_val = val;
	return r;
}

struct var *
var_setstr1(head, name, str)
struct var **head;
char *name;
char *str;
{
	struct value v;

	v.v_type = V_STR;
	v.v_str = str;
	return var_set1(head, name, &v);
}

struct var *
var_setnum1(head, name, num)
struct var **head;
char *name;
int num;
{
	struct value v;

	v.v_type = V_NUM;
	v.v_num = num;
	return var_set1(head, name, &v);
}

var_unset1(head, name)
struct var **head;
char *name;
{
	register struct var **p;
	register struct var *r;

	if (*(p = var_lookup1(head, name)) == 0)
		return -1;
	r = *p;
	*p = r->r_left;
	while (*p != 0)
		p = &(*p)->r_right;
	*p = r->r_right;
	val_free(r->r_val);
	str_free(r->r_name);
	free((char *) r);
	return 0;
}

struct var **
var_lookup1(p, name)
register struct var **p;
register char *name;
{
	register cmp;

	while (*p != 0) {
		if ((cmp = strcmp(name, (*p)->r_name)) < 0)
			p = &(*p)->r_left;
		else if (cmp > 0)
			p = &(*p)->r_right;
		else
			break;
	}
	return p;
}

var_walk1(r, func, a)
register struct var *r;
int (*func)();
long a;
{
	if (r == 0)
		return 0;
	if (var_walk1(r->r_left, func, a) < 0 || (*func)(a, r) < 0
	    || var_walk1(r->r_right, func, a) < 0)
		return -1;
	return 0;
}
