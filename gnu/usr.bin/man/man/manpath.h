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
  int mandatory;
} DIRLIST;

DIRLIST list[MAXDIRS];

char *tmplist[MAXDIRS];
