/* $Header: /src/pub/tcsh/tw.help.c,v 3.17 2000/01/14 22:57:30 christos Exp $ */
/* tw.help.c: actually look up and print documentation on a file.
 *	      Look down the path for an appropriate file, then print it.
 *	      Note that the printing is NOT PAGED.  This is because the
 *	      function is NOT meant to look at manual pages, it only does so
 *	      if there is no .help file to look in.
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 */
#include "sh.h"

RCSID("$Id: tw.help.c,v 3.17 2000/01/14 22:57:30 christos Exp $")

#include "tw.h"
#include "tc.h"


static int f = -1;
static	sigret_t	 cleanf		__P((int));
static	Char    	*skipslist	__P((Char *));
static	void		 nextslist 	__P((Char *, Char *));

static char *h_ext[] = {
    ".help", ".1", ".8", ".6", "", NULL
};

void
do_help(command)
    Char   *command;
{
    Char    name[FILSIZ + 1];
    Char   *cmd_p, *ep;
    char  **sp;

    signalfun_t orig_intr;
    Char    curdir[MAXPATHLEN];	/* Current directory being looked at */
    register Char *hpath;	/* The environment parameter */
    Char    full[MAXPATHLEN];
    char    buf[512];		/* full path name and buffer for read */
    int     len;		/* length of read buffer */
    Char   *thpath;


    /* trim off the whitespace at the beginning */
    for (cmd_p = command; *cmd_p == ' ' || *cmd_p == '\t'; cmd_p++)
	continue;
		
    /* copy the string to a safe place */
    copyn(name, cmd_p, FILSIZ + 1);

    /* trim off the whitespace that may be at the end */
    for (cmd_p = name; 
	 *cmd_p != ' ' && *cmd_p != '\t' && *cmd_p != '\0'; cmd_p++)
	continue;
    *cmd_p = '\0';

    /* if nothing left, return */
    if (*name == '\0')
	return;

    if (adrof1(STRhelpcommand, &aliases)) {	/* if we have an alias */
	jmp_buf_t osetexit;

	getexit(osetexit);	/* make sure to come back here */
	if (setexit() == 0)
	    aliasrun(2, STRhelpcommand, name);	/* then use it. */
	resexit(osetexit);	/* and finish up */
    }
    else {			/* else cat something to them */
	/* got is, now "cat" the file based on the path $HPATH */

	hpath = str2short(getenv(SEARCHLIST));
	if (hpath == NULL)
	    hpath = str2short(DEFAULTLIST);
	thpath = hpath = Strsave(hpath);

	for (;;) {
	    if (!*hpath) {
		xprintf(CGETS(29, 1, "No help file for %S\n"), name);
		break;
	    }
	    nextslist(hpath, curdir);
	    hpath = skipslist(hpath);

	    /*
	     * now make the full path name - try first /bar/foo.help, then
	     * /bar/foo.1, /bar/foo.8, then finally /bar/foo.6.  This is so
	     * that you don't spit a binary at the tty when $HPATH == $PATH.
	     */
	    copyn(full, curdir, (int) (sizeof(full) / sizeof(Char)));
	    catn(full, STRslash, (int) (sizeof(full) / sizeof(Char)));
	    catn(full, name, (int) (sizeof(full) / sizeof(Char)));
	    ep = &full[Strlen(full)];
	    for (sp = h_ext; *sp; sp++) {
		*ep = '\0';
		catn(full, str2short(*sp), (int) (sizeof(full) / sizeof(Char)));
		if ((f = open(short2str(full), O_RDONLY)) != -1)
		    break;
	    }
	    if (f != -1) {
		/* so cat it to the terminal */
		orig_intr = (signalfun_t) sigset(SIGINT, cleanf);
		while (f != -1 && (len = read(f, (char *) buf, 512)) > 0)
		    (void) write(SHOUT, (char *) buf, (size_t) len);
#ifdef convex
		/* print error in case file is migrated */
		if (len == -1)
		    stderror(ERR_SYSTEM, progname, strerror(errno));
#endif /* convex */
		(void) sigset(SIGINT, orig_intr);
		if (f != -1)
		    (void) close(f);
		break;
	    }
	}
	xfree((ptr_t) thpath);
    }
}

static  sigret_t
/*ARGSUSED*/
cleanf(snum)
int snum;
{
    USE(snum);
#ifdef UNRELSIGS
    if (snum)
	(void) sigset(SIGINT, cleanf);
#endif /* UNRELSIGS */
    if (f != -1)
	(void) close(f);
    f = -1;
#ifndef SIGVOID
    return (snum);
#endif
}

/* these next two are stolen from CMU's man(1) command for looking down
 * paths.  they are prety straight forward. */

/*
 * nextslist takes a search list and copies the next path in it
 * to np.  A null search list entry is expanded to ".".
 * If there are no entries in the search list, then np will point
 * to a null string.
 */

static void
nextslist(sl, np)
    register Char *sl;
    register Char *np;
{
    if (!*sl)
	*np = '\000';
    else if (*sl == ':') {
	*np++ = '.';
	*np = '\000';
    }
    else {
	while (*sl && *sl != ':')
	    *np++ = *sl++;
	*np = '\000';
    }
}

/*
 * skipslist returns the pointer to the next entry in the search list.
 */

static Char *
skipslist(sl)
    register Char *sl;
{
    while (*sl && *sl++ != ':')
	continue;
    return (sl);
}
