/*
 * msg.c - routines for error messages
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991-1997 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Programming Language.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 */

#include "awk.h"

int sourceline = 0;
char *source = NULL;

static char *srcfile = NULL;
static int srcline;

/* prototype needed for ansi / gcc */
void err P((const char *s, const char *emsg, va_list argp));

/* err --- print an error message with source line and file and record */

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
#ifdef DEBUG
	if (srcfile != NULL) {
		fprintf(stderr, "%s:%d:", srcfile, srcline);
		srcfile = NULL;
	}
#endif /* DEBUG */
	if (sourceline != 0) {
		if (source != NULL)
			(void) fprintf(stderr, "%s:", source);
		else
			(void) fprintf(stderr, "cmd. line:");

		(void) fprintf(stderr, "%d: ", sourceline);
	}
	if (FNR > 0) {
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

/* msg --- take a varargs error message and print it */

#if defined(HAVE_STDARG_H) && defined(__STDC__) && __STDC__
void
msg(char *mesg, ...)
#else
/*VARARGS0*/
void
msg(va_alist)
va_dcl
#endif
{
	va_list args;
#if defined(HAVE_STDARG_H) && defined(__STDC__) && __STDC__
	va_start(args, mesg);
#else
	char *mesg;

	va_start(args);
	mesg = va_arg(args, char *);
#endif
	err("", mesg, args);
	va_end(args);
}

/* warning --- print a warning message */

#if defined(HAVE_STDARG_H) && defined(__STDC__) && __STDC__
void
warning(char *mesg, ...)
#else
/*VARARGS0*/
void
warning(va_alist)
va_dcl
#endif
{
	va_list args;
#if defined(HAVE_STDARG_H) && defined(__STDC__) && __STDC__
	va_start(args, mesg);
#else
	char *mesg;

	va_start(args);
	mesg = va_arg(args, char *);
#endif
	err("warning: ", mesg, args);
	va_end(args);
}

#if defined(HAVE_STDARG_H) && defined(__STDC__) && __STDC__
void
error(char *mesg, ...)
#else
/*VARARGS0*/
void
error(va_alist)
va_dcl
#endif
{
	va_list args;
#if defined(HAVE_STDARG_H) && defined(__STDC__) && __STDC__
	va_start(args, mesg);
#else
	char *mesg;

	va_start(args);
	mesg = va_arg(args, char *);
#endif
	err("error: ", mesg, args);
	va_end(args);
}

/* set_loc --- set location where a fatal error happened */

void
set_loc(file, line)
char *file;
int line;
{
	srcfile = file;
	srcline = line;
}

/* fatal --- print an error message and die */

#if defined(HAVE_STDARG_H) && defined(__STDC__) && __STDC__
void
r_fatal(char *mesg, ...)
#else
/*VARARGS0*/
void
r_fatal(va_alist)
va_dcl
#endif
{
	va_list args;
#if defined(HAVE_STDARG_H) && defined(__STDC__) && __STDC__
	va_start(args, mesg);
#else
	char *mesg;

	va_start(args);
	mesg = va_arg(args, char *);
#endif
	err("fatal: ", mesg, args);
	va_end(args);
#ifdef DEBUG
	abort();
#endif
	exit(2);
}

