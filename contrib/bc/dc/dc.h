/* 
 * Header file for dc routines
 *
 * Copyright (C) 1994, 1997, 1998 Free Software Foundation, Inc.
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

#ifndef DC_DEFS_H
#define DC_DEFS_H

/* 'I' is a command, and bases 17 and 18 are quite
 * unusual, so we limit ourselves to bases 2 to 16
 */
#define DC_IBASE_MAX	16

#define DC_SUCCESS		0
#define DC_DOMAIN_ERROR	1
#define DC_FAIL			2	/* generic failure */


#ifndef __STDC__
# define DC_PROTO(x)			()
# define DC_DECLVOID()			()
# define DC_DECLARG(arglist)	arglist
# define DC_DECLSEP				;
# define DC_DECLEND				;
#else /* __STDC__ */
# define DC_PROTO(x)			x
# define DC_DECLVOID()			(void)
# define DC_DECLARG(arglist)	(
# define DC_DECLSEP				,
# define DC_DECLEND				)
#endif /* __STDC__ */


typedef enum {DC_TOSS, DC_KEEP}   dc_discard;
typedef enum {DC_NONL, DC_WITHNL} dc_newline;


/* type discriminant for dc_data */
typedef enum {DC_UNINITIALIZED, DC_NUMBER, DC_STRING} dc_value_type;

/* only numeric.c knows what dc_num's *really* look like */
typedef struct dc_number *dc_num;

/* only string.c knows what dc_str's *really* look like */
typedef struct dc_string *dc_str;


/* except for the two implementation-specific modules, all
 * dc functions only know of this one generic type of object
 */
typedef struct {
	dc_value_type dc_type;	/* discriminant for union */
	union {
		dc_num number;
		dc_str string;
	} v;
} dc_data;


/* This is dc's only global variable: */
extern const char *progname;	/* basename of program invocation */

#endif /* not DC_DEFS_H */
