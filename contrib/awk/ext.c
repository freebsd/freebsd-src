/*
 * ext.c - Builtin function that links external gawk functions and related
 *	   utilities.
 *
 * Christos Zoulas, Thu Jun 29 17:40:41 EDT 1995
 * Arnold Robbins, update for 3.1, Mon Nov 23 12:53:39 EST 1998
 */

/*
 * Copyright (C) 1995 - 2001 the Free Software Foundation, Inc.
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

#ifdef DYNAMIC

#include <dlfcn.h>

#ifdef __GNUC__
static unsigned long long dummy;	/* fake out gcc for dynamic loading? */
#endif

extern int errcount;

/* do_ext --- load an extension */

NODE *
do_ext(NODE *tree)
{
	NODE *obj;
	NODE *fun;
	NODE *(*func) P((NODE *, void *));
	void *dl;
	int flags = RTLD_LAZY;

#ifdef __GNUC__
	AWKNUM junk;

	junk = (AWKNUM) dummy;
#endif

	if (do_lint)
		lintwarn(_("`extension' is a gawk extension"));

	if (do_traditional || do_posix) {
		errcount++;
		error(_("`extension' is a gawk extension"));
	}

	obj = tree_eval(tree->lnode);
	force_string(obj);

#ifdef RTLD_GLOBAL
	flags |= RTLD_GLOBAL;
#endif
	if ((dl = dlopen(obj->stptr, flags)) == NULL)
		fatal(_("extension: cannot open `%s' (%s)\n"), obj->stptr,
		      dlerror());

	fun = tree_eval(tree->rnode->lnode);
	force_string(fun);

	func = (NODE *(*) P((NODE *, void *))) dlsym(dl, fun->stptr);
	if (func == NULL)
		fatal(_("extension: library `%s': cannot call function `%s' (%s)\n"),
				obj->stptr, fun->stptr, dlerror());
	free_temp(obj);
	free_temp(fun);

	return (*func)(tree, dl);
}

/* make_builtin --- register name to be called as func with a builtin body */

void
make_builtin(char *name, NODE *(*func) P((NODE *)), int count)
{
	NODE *p, *b, *f;
	char **vnames, *parm_names, *sp;
	char buf[200];
	int space_needed, i;

	/* count parameters, create artificial list of param names */
	space_needed = 0;
	for (i = 0; i < count; i++) {
		sprintf(buf, "p%d", i);
		space_needed += strlen(buf) + 1;
	}
	emalloc(parm_names, char *, space_needed, "make_builtin");
	emalloc(vnames, char **, count * sizeof(char  *), "make_builtin");
	sp = parm_names;
	for (i = 0; i < count; i++) {
		sprintf(sp, "p%d",i);
		vnames[i] = sp;
		sp += strlen(sp) + 1;
	}

	getnode(p);
	p->type = Node_param_list;
	p->rnode = NULL;
	p->param = name;
	p->param_cnt = count;
#if 0
	/* setting these  blows away the param_cnt. dang unions! */
	p->source_line = __LINE__;
	p->source_file = __FILE__;
#endif

	getnode(b);
	b->type = Node_builtin;
	b->proc = func;
	b->subnode = p;
	b->source_line = __LINE__;
	b->source_file = __FILE__;

	f = node(p, Node_func, b);
	f->parmlist = vnames;
	install(name, f);
}

/* get_argument --- Get the n'th argument of a dynamically linked function */

NODE *
get_argument(NODE *tree, int i)
{
	extern NODE **stack_ptr;

	if (i < 0 || i >= tree->param_cnt)
		return NULL;

	tree = stack_ptr[i];
	if (tree->lnode == Nnull_string)
		return NULL;

	if (tree->type == Node_array_ref)
		tree = tree->orig_array;

	if (tree->type == Node_var_array)
		return tree;

	return tree->lnode;
}

/* set_value --- set the return value of a dynamically linked function */

void
set_value(NODE *tree)
{
	extern NODE *ret_node;

	if (tree)
		ret_node = tree;
	else
		ret_node = Nnull_string;
}
#else

/* do_ext --- dummy version if extensions not available */

NODE *
do_ext(NODE *tree)
{
	char *emsg = _("Operation Not Supported");

	unref(ERRNO_node->var_value);
	ERRNO_node->var_value = make_string(emsg, strlen(emsg));
	return tmp_number((AWKNUM) -1);
}
#endif
