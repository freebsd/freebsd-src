/*
 * FreeBSD install - a package for the installation and maintenance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Jordan K. Hubbard

 * 18 July 1993
 *
 * Semi-convenient place to stick some needed globals.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lib.h"

/* These are global for all utils */
Boolean	Quiet		= FALSE;
Boolean	Fake		= FALSE;
Boolean	Force		= FALSE;
int AutoAnswer		= FALSE;
int	Verbose			= 0;		/* Allow multiple levels of verbose. */
