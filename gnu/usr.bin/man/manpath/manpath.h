/*
 * manpath.h
 *
 * Copyright (c) 1990, 1991, John W. Eaton.
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with the man
 * distribution.
 *
 * John W. Eaton
 * jwe@che.utexas.edu
 * Department of Chemical Engineering
 * The University of Texas at Austin
 * Austin, Texas  78712
 */

typedef struct
{
  char mandir[MAXPATHLEN];
  char bin[MAXPATHLEN];
  int type;
} DIRLIST;

/* manpath types */
#define MANPATH_NONE		0
#define MANPATH_MANDATORY	1		/* manpath is mandatory */
#define MANPATH_OPTIONAL	2		/* manpath is optional */
#define MANPATH_MAP		3		/* maps path to manpath */

DIRLIST list[MAXDIRS];

char *tmplist[MAXDIRS];
