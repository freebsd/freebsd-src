/*
 * dl.c - Example of adding a new builtin function to gawk.
 *
 * Christos Zoulas, Thu Jun 29 17:40:41 EDT 1995
 * Arnold Robbins, update for 3.1, Wed Sep 13 09:38:56 2000
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
#include <dlfcn.h>

static void *sdl = NULL;

static NODE *
zaxxon(tree)
NODE *tree;
{
	NODE *obj;
	int i;
	int comma = 0;

	/*
	 * Print the arguments
	 */
	printf("External linkage %s(", tree->param);

	for (i = 0; i < tree->param_cnt; i++) {

		obj = get_argument(tree, i);

		if (obj == NULL)
			break;

		force_string(obj);

		printf(comma ? ", %s" : "%s", obj->stptr);
		free_temp(obj);
		comma = 1;
	}

	printf(");\n");

	/*
	 * Do something useful
	 */
	obj = get_argument(tree, 0);

	if (obj != NULL) {
		force_string(obj);
		if (strcmp(obj->stptr, "unload") == 0 && sdl) {
			/*
			 * XXX: How to clean up the function? 
			 * I would like the ability to remove a function...
			 */
			dlclose(sdl);
			sdl = NULL;
		}
		free_temp(obj);
	}

	/* Set the return value */
	set_value(tmp_number((AWKNUM) 3.14));

	/* Just to make the interpreter happy */
	return tmp_number((AWKNUM) 0);
}

NODE *
dlload(tree, dl)
NODE *tree;
void *dl;
{
	sdl = dl;
	make_builtin("zaxxon", zaxxon, 4);
	return tmp_number((AWKNUM) 0);
}
