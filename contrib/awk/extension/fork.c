/*
 * fork.c - Provide fork and waitpid functions for gawk.
 */

/*
 * Copyright (C) 2001 the Free Software Foundation, Inc.
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
#include <sys/wait.h>

/*  do_fork --- provide dynamically loaded fork() builtin for gawk */

static NODE *
do_fork(tree)
NODE *tree;
{
	int ret = -1;
	NODE **aptr;

	if  (do_lint && tree->param_cnt > 0)
		lintwarn("fork: called with too many arguments");

	ret = fork();

	if (ret < 0)
		update_ERRNO();
	else if (ret == 0) {
		/* update PROCINFO in the child */

		aptr = assoc_lookup(PROCINFO_node, tmp_string("pid", 3), FALSE);
		(*aptr)->numbr = (AWKNUM) getpid();

		aptr = assoc_lookup(PROCINFO_node, tmp_string("ppid", 4), FALSE);
		(*aptr)->numbr = (AWKNUM) getppid();
	}

	/* Set the return value */
	set_value(tmp_number((AWKNUM) ret));

	/* Just to make the interpreter happy */
	return tmp_number((AWKNUM) 0);
}


/*  do_waitpid --- provide dynamically loaded waitpid() builtin for gawk */

static NODE *
do_waitpid(tree)
NODE *tree;
{
	NODE *pidnode;
	int ret = -1;
	double pidval;
	pid_t pid;
	int options = 0;

	if  (do_lint && tree->param_cnt > 1)
		lintwarn("waitpid: called with too many arguments");

	pidnode = get_argument(tree, 0);
	if (pidnode != NULL) {
		pidval = force_number(pidnode);
		pid = (int) pidval;
		options = WNOHANG|WUNTRACED;
		ret = waitpid(pid, NULL, options);
		if (ret < 0)
			update_ERRNO();
	} else if (do_lint)
		lintwarn("wait: called with no arguments");

	/* Set the return value */
	set_value(tmp_number((AWKNUM) ret));

	/* Just to make the interpreter happy */
	return tmp_number((AWKNUM) 0);
}

/* dlload --- load new builtins in this library */

NODE *
dlload(tree, dl)
NODE *tree;
void *dl;
{
	make_builtin("fork", do_fork, 0);
	make_builtin("waitpid", do_waitpid, 1);
	return tmp_number((AWKNUM) 0);
}
