/* 
 * misc. functions for the "dc" Desk Calculator language.
 *
 * Copyright (C) 1994, 1997 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can either send email to this
 * program's author (see below) or write to: The Free Software Foundation,
 * Inc.; 675 Mass Ave. Cambridge, MA 02139, USA.
 */

/* This module contains miscelaneous functions that have no
 * special knowledge of any private data structures.
 * They could all be moved to their own separate modules, but
 * are agglomerated here for convenience.
 */

#include "config.h"

#include <stdio.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include <ctype.h>
#ifndef isgraph
# ifndef HAVE_ISGRAPH
#  define isgraph isprint
# endif
#endif
#include <getopt.h>
#include "dc.h"
#include "dc-proto.h"

#include "version.h"

#ifndef EXIT_FAILURE	/* C89 <stdlib.h> */
# define EXIT_FAILURE	1
#endif


/* print an "out of memory" diagnostic and exit program */
void
dc_memfail DC_DECLVOID()
{
	fprintf(stderr, "%s: out of memory\n", progname);
	exit(EXIT_FAILURE);
}

/* malloc or die */
void *
dc_malloc DC_DECLARG((len))
	size_t len DC_DECLEND
{
	void *result = malloc(len);

	if (!result)
		dc_memfail();
	return result;
}


/* print the id in a human-understandable form
 *  fp is the output stream to place the output on
 *  id is the name of the register (or command) to be printed
 *  suffix is a modifier (such as "stack") to be printed
 */
void
dc_show_id DC_DECLARG((fp, id, suffix))
	FILE *fp DC_DECLSEP
	int id DC_DECLSEP
	const char *suffix DC_DECLEND
{
	if (isgraph(id))
		fprintf(fp, "'%c' (%#o)%s", id, id, suffix);
	else
		fprintf(fp, "%#o%s", id, suffix);
}


/* report that corrupt data has been detected;
 * use the msg and regid (if nonnegative) to give information
 * about where the garbage was found,
 *
 * will abort() so that a debugger might be used to help find
 * the bug
 */
/* If this routine is called, then there is a bug in the code;
 * i.e. it is _not_ a data or user error
 */
void
dc_garbage DC_DECLARG((msg, regid))
	const char *msg DC_DECLSEP
	int regid DC_DECLEND
{
	if (regid < 0) {
		fprintf(stderr, "%s: garbage %s\n", progname, msg);
	} else {
		fprintf(stderr, "%s:%s register ", progname, msg);
		dc_show_id(stderr, regid, " is garbage\n");
	}
	abort();
}


/* call system() with the passed string;
 * if the string contains a newline, terminate the string
 * there before calling system.
 * Return a pointer to the first unused character in the string
 * (i.e. past the '\n' if there was one, to the '\0' otherwise).
 */
const char *
dc_system DC_DECLARG((s))
	const char *s DC_DECLEND
{
	const char *p;
	char *tmpstr;
	size_t len;

	p = strchr(s, '\n');
	if (p) {
		len = p - s;
		tmpstr = dc_malloc(len + 1);
		strncpy(tmpstr, s, len);
		tmpstr[len] = '\0';
		system(tmpstr);
		free(tmpstr);
		return p + 1;
	}
	system(s);
	return s + strlen(s);
}


/* print out the indicated value */
void
dc_print DC_DECLARG((value, obase))
	dc_data value DC_DECLSEP
	int obase DC_DECLEND
{
	if (value.dc_type == DC_NUMBER) {
		dc_out_num(value.v.number, obase, DC_TRUE, DC_FALSE);
	} else if (value.dc_type == DC_STRING) {
		dc_out_str(value.v.string, DC_TRUE, DC_FALSE);
	} else {
		dc_garbage("in data being printed", -1);
	}
}

/* return a duplicate of the passed value, regardless of type */
dc_data
dc_dup DC_DECLARG((value))
	dc_data value DC_DECLEND
{
	if (value.dc_type!=DC_NUMBER && value.dc_type!=DC_STRING)
		dc_garbage("in value being duplicated", -1);
	if (value.dc_type == DC_NUMBER)
		return dc_dup_num(value.v.number);
	/*else*/
	return dc_dup_str(value.v.string);
}
