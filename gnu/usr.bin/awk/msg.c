/*
 * msg.c - routines for error messages
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991, 1992, 1993 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Progamming Language.
 * 
 * GAWK is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
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

int sourceline = 0;
char *source = NULL;

/* VARARGS2 */
void
err(s, emsg, argp)
const char *s;
const char *emsg;
va_list argp;
{
	char *file;

	(void) fflush(stdout);
	(void) fprintf(stderr, "%s: ", myname);
	if (sourceline) {
		if (source)
			(void) fprintf(stderr, "%s:", source);
		else
			(void) fprintf(stderr, "cmd. line:");

		(void) fprintf(stderr, "%d: ", sourceline);
	}
	if (FNR) {
		file = FILENAME_node->var_value->stptr;
		(void) putc('(', stderr);
		if (file)
			(void) fprintf(stderr, "FILENAME=%s ", file);
		(void) fprintf(stderr, "FNR=%ld) ", FNR);
	}
	(void) fprintf(stderr, s);
	vfprintf(stderr, emsg, argp);
	(void) fprintf(stderr, "\n");
	(void) fflush(stderr);
}

/*VARARGS0*/
void
msg(va_alist)
va_dcl
{
	va_list args;
	char *mesg;

	va_start(args);
	mesg = va_arg(args, char *);
	err("", mesg, args);
	va_end(args);
}

/*VARARGS0*/
void
warning(va_alist)
va_dcl
{
	va_list args;
	char *mesg;

	va_start(args);
	mesg = va_arg(args, char *);
	err("warning: ", mesg, args);
	va_end(args);
}

/*VARARGS0*/
void
fatal(va_alist)
va_dcl
{
	va_list args;
	char *mesg;

	va_start(args);
	mesg = va_arg(args, char *);
	err("fatal: ", mesg, args);
	va_end(args);
#ifdef DEBUG
	abort();
#endif
	exit(2);
}
