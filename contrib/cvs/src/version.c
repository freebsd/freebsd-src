/*
 * Copyright (c) 1994 david d `zoo' zuhn
 * Copyright (c) 1994 Free Software Foundation, Inc.
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with this  CVS source distribution.
 * 
 * version.c - the CVS version number
 */

#include "cvs.h"

char *version_string = "\nConcurrent Versions System (CVS) 1.9.24";

#ifdef CLIENT_SUPPORT
#ifdef SERVER_SUPPORT
char *config_string = " (client/server)\n";
#else
char *config_string = " (client)\n";
#endif
#else
#ifdef SERVER_SUPPORT
char *config_string = " (server)\n";
#else
char *config_string = "\n";
#endif
#endif
