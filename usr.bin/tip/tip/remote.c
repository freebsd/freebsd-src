/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)remote.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD: src/usr.bin/tip/tip/remote.c,v 1.4 1999/08/28 01:06:35 peter Exp $";
#endif /* not lint */

#include <sys/syslimits.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "tipconf.h"
#include "tip.h"
#include "pathnames.h"

/*
 * Attributes to be gleened from remote host description
 *   data base.
 */
static char **caps[] = {
	&AT, &DV, &CM, &CU, &EL, &IE, &OE, &PN, &PR, &DI,
	&ES, &EX, &FO, &RC, &RE, &PA, &LI, &LO
};

static char *capstrings[] = {
	"at", "dv", "cm", "cu", "el", "ie", "oe", "pn", "pr",
	"di", "es", "ex", "fo", "rc", "re", "pa", "li", "lo", 0
};

static char	*db_array[3] = { _PATH_REMOTE, 0, 0 };

#define cgetflag(f)	(cgetcap(bp, f, ':') != NULL)

static void getremcap __P((char *));

/*
	Expand the start tilde sequence at the start of the
	specified path. Optionally, free space allocated to
	path before reinitializing it.
*/
static int
expand_tilde (char **path, void (*free) (char *p))
{
	int rc = 0;
	char buffer [PATH_MAX];
	char *tailp;
	if ((tailp = strchr (*path + 1, '/')) != NULL)
	{
		struct passwd *pwd;
		*tailp++ = '\0';
		if (*(*path + 1) == '\0')
			strcpy (buffer, getlogin ());
		else
			strcpy (buffer, *path + 1);
		if ((pwd = getpwnam (buffer)) != NULL)
		{
			strcpy (buffer, pwd->pw_dir);
			strcat (buffer, "/");
			strcat (buffer, tailp);
			if (free)
				free (*path);
			*path = strdup (buffer);
			rc++;
		}
		return rc;
	}
	return (-1);
}

static void
getremcap(host)
	register char *host;
{
	register char **p, ***q;
	char *bp;
	char *rempath;
	int   stat;

	rempath = getenv("REMOTE");
	if (rempath != NULL)
		if (*rempath != '/')
			/* we have an entry */
			cgetset(rempath);
		else {	/* we have a path */
			db_array[1] = rempath;
			db_array[2] = _PATH_REMOTE;
		}

	if ((stat = cgetent(&bp, db_array, host)) < 0) {
		if (DV ||
		    (host[0] == '/' && access(DV = host, R_OK | W_OK) == 0)) {
			CU = DV;
			HO = host;
			HW = 1;
			DU = 0;
			if (!BR)
				BR = DEFBR;
			FS = DEFFS;
			return;
		}
		switch(stat) {
		case -1:
			warnx("unknown host %s", host);
			break;
		case -2:
			warnx("can't open host description file");
			break;
		case -3:
			warnx("possible reference loop in host description file");
			break;
		}
		exit(3);
	}

	for (p = capstrings, q = caps; *p != NULL; p++, q++)
		if (**q == NULL)
			cgetstr(bp, *p, *q);
	if (!BR && (cgetnum(bp, "br", &BR) == -1))
		BR = DEFBR;
	if (cgetnum(bp, "fs", &FS) == -1)
		FS = DEFFS;
	if (DU < 0)
		DU = 0;
	else
		DU = cgetflag("du");
	if (DV == NOSTR) {
		fprintf(stderr, "%s: missing device spec\n", host);
		exit(3);
	}
	if (DU && CU == NOSTR)
		CU = DV;
	if (DU && PN == NOSTR) {
		fprintf(stderr, "%s: missing phone number\n", host);
		exit(3);
	}

	HD = cgetflag("hd");

	/*
	 * This effectively eliminates the "hw" attribute
	 *   from the description file
	 */
	if (!HW)
		HW = (CU == NOSTR) || (DU && equal(DV, CU));
	HO = host;

	/*
		If login script, verify access
	*/
	if (LI != NOSTR) {
		if (*LI == '~')
			(void) expand_tilde (&LI, NULL);
		if (access (LI, F_OK | X_OK) != 0) {
			printf("tip (warning): can't open login script \"%s\"\n", LI);
			LI = NOSTR;
		}
	}

	/*
		If logout script, verify access
	*/
	if (LO != NOSTR) {
		if (*LO == '~')
			(void) expand_tilde (&LO, NULL);
		if (access (LO, F_OK | X_OK) != 0) {
			printf("tip (warning): can't open logout script \"%s\"\n", LO);
			LO = NOSTR;
		}
	}

	/*
	 * see if uppercase mode should be turned on initially
	 */
	if (cgetflag("ra"))
		boolean(value(RAISE)) = 1;
	if (cgetflag("ec"))
		boolean(value(ECHOCHECK)) = 1;
	if (cgetflag("be"))
		boolean(value(BEAUTIFY)) = 1;
	if (cgetflag("nb"))
		boolean(value(BEAUTIFY)) = 0;
	if (cgetflag("sc"))
		boolean(value(SCRIPT)) = 1;
	if (cgetflag("tb"))
		boolean(value(TABEXPAND)) = 1;
	if (cgetflag("vb"))
		boolean(value(VERBOSE)) = 1;
	if (cgetflag("nv"))
		boolean(value(VERBOSE)) = 0;
	if (cgetflag("ta"))
		boolean(value(TAND)) = 1;
	if (cgetflag("nt"))
		boolean(value(TAND)) = 0;
	if (cgetflag("rw"))
		boolean(value(RAWFTP)) = 1;
	if (cgetflag("hd"))
		boolean(value(HALFDUPLEX)) = 1;
	if (RE == NOSTR)
		RE = (char *)"tip.record";
	if (EX == NOSTR)
		EX = (char *)"\t\n\b\f";
	if (ES != NOSTR)
		vstring("es", ES);
	if (FO != NOSTR)
		vstring("fo", FO);
	if (PR != NOSTR)
		vstring("pr", PR);
	if (RC != NOSTR)
		vstring("rc", RC);
	if (cgetnum(bp, "dl", &DL) == -1)
		DL = 0;
	if (cgetnum(bp, "cl", &CL) == -1)
		CL = 0;
	if (cgetnum(bp, "et", &ET) == -1)
		ET = 10;
}

char *
getremote(host)
	char *host;
{
	register char *cp;
	static char *next;
	static int lookedup = 0;

	if (!lookedup) {
		if (host == NOSTR && (host = getenv("HOST")) == NOSTR)
			errx(3, "no host specified");
		getremcap(host);
		next = DV;
		lookedup++;
	}
	/*
	 * We return a new device each time we're called (to allow
	 *   a rotary action to be simulated)
	 */
	if (next == NOSTR)
		return (NOSTR);
	if ((cp = index(next, ',')) == NULL) {
		DV = next;
		next = NOSTR;
	} else {
		*cp++ = '\0';
		DV = next;
		next = cp;
	}
	return (DV);
}
