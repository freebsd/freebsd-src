/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)glob.h	8.1 (Berkeley) 6/6/93
 */

/*
 * A bunch of global variable declarations lie herein.
 * def.h must be included first.
 */

extern int	msgCount;			/* Count of messages read in */
extern int	rcvmode;			/* True if receiving mail */
extern int	sawcom;				/* Set after first command */
extern char	*Tflag;				/* -T temp file for netnews */
extern int	senderr;			/* An error while checking */
extern int	edit;				/* Indicates editing a file */
extern int	readonly;			/* Will be unable to rewrite file */
extern int	noreset;			/* String resets suspended */
extern int	sourcing;			/* Currently reading variant file */
extern int	loading;			/* Loading user definitions */
extern int	cond;				/* Current state of conditional exc. */
extern FILE	*itf;				/* Input temp file buffer */
extern FILE	*otf;				/* Output temp file buffer */
extern int	image;				/* File descriptor for image of msg */
extern FILE	*input;				/* Current command input file */
extern char	mailname[PATHSIZE];		/* Name of current file */
extern char	prevfile[PATHSIZE];		/* Name of previous file */
extern char	*homedir;			/* Path name of home directory */
extern char	*myname;			/* My login name */
extern off_t	mailsize;			/* Size of system mailbox */
extern int	lexnumber;			/* Number of TNUMBER from scan() */
extern char	lexstring[STRINGLEN];		/* String from TSTRING, scan() */
extern int	regretp;			/* Pointer to TOS of regret tokens */
extern int	regretstack[REGDEP];		/* Stack of regretted tokens */
extern char	*string_stack[REGDEP];		/* Stack of regretted strings */
extern int	numberstack[REGDEP];		/* Stack of regretted numbers */
extern struct	message	*dot;			/* Pointer to current message */
extern struct	message	*message;		/* The actual message structure */
extern struct	var	*variables[HSHSIZE];	/* Pointer to active var list */
extern struct	grouphead	*groups[HSHSIZE];/* Pointer to active groups */
extern struct	ignoretab	ignore[2];	/* ignored and retained fields
					   0 is ignore, 1 is retain */
extern struct	ignoretab	saveignore[2];	/* ignored and retained fields
					   on save to folder */
extern struct	ignoretab	ignoreall[2];	/* special, ignore all headers */
extern char	**altnames;			/* List of alternate names for user */
extern int	debug;				/* Debug flag set */
extern int	screenwidth;			/* Screen width, or best guess */
extern int	screenheight;			/* Screen height, or best guess,
					   for "header" command */
extern int	realscreenheight;		/* the real screen height */

#include <setjmp.h>

extern jmp_buf	srbuf;


/*
 * The pointers for the string allocation routines,
 * there are NSPACE independent areas.
 * The first holds STRINGSIZE bytes, the next
 * twice as much, and so on.
 */

#define	NSPACE	25			/* Total number of string spaces */
extern struct strings {
	char	*s_topFree;		/* Beginning of this area */
	char	*s_nextFree;		/* Next alloctable place here */
	unsigned s_nleft;		/* Number of bytes left here */
} stringdope[NSPACE];
