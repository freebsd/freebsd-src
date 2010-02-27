/*
 * Copyright 2007 Jon Loeliger, Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *                                                                   USA
 */

#ifndef _SRCPOS_H_
#define _SRCPOS_H_

/*
 * Augment the standard YYLTYPE with a filenum index into an
 * array of all opened filenames.
 */

#include <stdio.h>

struct dtc_file {
	char *dir;
	const char *name;
	FILE *file;
};

#if ! defined(YYLTYPE) && ! defined(YYLTYPE_IS_DECLARED)
typedef struct YYLTYPE {
    int first_line;
    int first_column;
    int last_line;
    int last_column;
    struct dtc_file *file;
} YYLTYPE;

#define YYLTYPE_IS_DECLARED	1
#define YYLTYPE_IS_TRIVIAL	1
#endif

/* Cater to old parser templates. */
#ifndef YYID
#define YYID(n)	(n)
#endif

#define YYLLOC_DEFAULT(Current, Rhs, N)					\
    do									\
      if (YYID (N))							\
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	  (Current).file         = YYRHSLOC (Rhs, N).file;		\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	  (Current).file         = YYRHSLOC (Rhs, 0).file;		\
	}								\
    while (YYID (0))


typedef YYLTYPE srcpos;

/*
 * Fictional source position used for IR nodes that are
 * created without otherwise knowing a true source position.
 * For example,constant definitions from the command line.
 */
extern srcpos srcpos_empty;

extern struct dtc_file *srcpos_file;

struct search_path {
	const char *dir; /* NULL for current directory */
	struct search_path *prev, *next;
};

extern struct dtc_file *dtc_open_file(const char *fname,
                                      const struct search_path *search);
extern void dtc_close_file(struct dtc_file *file);

extern srcpos *srcpos_copy(srcpos *pos);
extern char *srcpos_string(srcpos *pos);
extern void srcpos_dump(srcpos *pos);

extern void srcpos_error(srcpos *pos, char const *, ...)
     __attribute__((format(printf, 2, 3)));
extern void srcpos_warn(srcpos *pos, char const *, ...)
     __attribute__((format(printf, 2, 3)));

#endif /* _SRCPOS_H_ */
