/*-
 * Copyright (c) 1980, 1992, 1993
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
static char sccsid[] = "@(#)disks.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/dkstat.h>

#include <nlist.h>
#include <ctype.h>
#include <paths.h>
#include <string.h>
#include <stdlib.h>
#include "systat.h"
#include "extern.h"

static void dkselect __P((char *, int, int []));
static int read_names __P((void));

static struct nlist namelist[] = {
#define	X_DK_NDRIVE	0
	{ "_dk_ndrive" },
#define	X_DK_WPMS	1
	{ "_dk_wpms" },
#if defined(hp300) || defined(luna68k)
#define X_HPDINIT       (X_DK_WPMS+1)
        { "_hp_dinit" },
#endif
#if defined(i386)
#define X_DK_NAMES	(X_DK_WPMS+1)
	{ "_dk_names" },
#endif
#ifdef mips
#define X_SCSI_DINIT	(X_DK_WPMS+1)
	{ "_scsi_dinit" },
#endif
#ifdef sun
#define	X_MBDINIT	(X_DK_WPMS+1)
	{ "_mbdinit" },
#endif
#ifdef tahoe
#define	X_VBDINIT	(X_DK_WPMS+1)
	{ "_vbdinit" },
#endif
#ifdef vax
#define	X_MBDINIT	(X_DK_WPMS+1)
	{ "_mbdinit" },
#define	X_UBDINIT	(X_DK_WPMS+2)
	{ "_ubdinit" },
#endif
	{ "" },
};

float *dk_mspw;
int dk_ndrive, *dk_select;
char **dr_name;

#include "names.c"					/* XXX */

int
dkinit()
{
	register int i;
	register char *cp;
	static int once = 0;
	static char buf[1024];

	if (once)
		return(1);

	if (kvm_nlist(kd, namelist)) {
		nlisterr(namelist);
		return(0);
	}
	if (namelist[X_DK_NDRIVE].n_value == 0) {
		error("dk_ndrive undefined in kernel");
		return(0);
	}
	NREAD(X_DK_NDRIVE, &dk_ndrive, sizeof(dk_ndrive));
	if (dk_ndrive <= 0) {
		error("dk_ndrive=%d according to %s", dk_ndrive, getbootfile());
		return(0);
	}
	dk_mspw = (float *)calloc(dk_ndrive, sizeof (float));
	{
		long *wpms = (long *)calloc(dk_ndrive, sizeof(long));
		KREAD(NPTR(X_DK_WPMS), wpms, dk_ndrive * sizeof (long));
		for (i = 0; i < dk_ndrive; i++)
			*(dk_mspw + i) = (*(wpms + i) == 0)? 0.0:
			                 (float) 1.0 / *(wpms + i);
		free(wpms);
	}
	dr_name = (char **)calloc(dk_ndrive, sizeof (char *));
	dk_select = (int *)calloc(dk_ndrive, sizeof (int));
	for (cp = buf, i = 0; i < dk_ndrive; i++) {
		dr_name[i] = cp;
		snprintf(cp, sizeof(buf) - (cp - buf), "dk%d", i);
		cp += strlen(cp) + 1;
		if (cp > buf + sizeof(buf))
			errx(1, "buf too small in dkinit, aborting");
		if (dk_mspw[i] != 0.0)
			dk_select[i] = 1;
	}
	if (!read_names()) {
		free(dr_name);
		free(dk_select);
		free(dk_mspw);
		return(0);
	}
	once = 1;
	return(1);
}

int
dkcmd(cmd, args)
	char *cmd, *args;
{
	if (prefix(cmd, "display") || prefix(cmd, "add")) {
		dkselect(args, 1, dk_select);
		return (1);
	}
	if (prefix(cmd, "ignore") || prefix(cmd, "delete")) {
		dkselect(args, 0, dk_select);
		return (1);
	}
	if (prefix(cmd, "drives")) {
		register int i;

		move(CMDLINE, 0); clrtoeol();
		for (i = 0; i < dk_ndrive; i++)
			if (dk_mspw[i] != 0.0)
				printw("%s ", dr_name[i]);
		return (1);
	}
	return (0);
}

static void
dkselect(args, truefalse, selections)
	char *args;
	int truefalse, selections[];
{
	register char *cp;
	register int i;
	char *index();

	cp = index(args, '\n');
	if (cp)
		*cp = '\0';
	for (;;) {
		for (cp = args; *cp && isspace(*cp); cp++)
			;
		args = cp;
		for (; *cp && !isspace(*cp); cp++)
			;
		if (*cp)
			*cp++ = '\0';
		if (cp - args == 0)
			break;
		for (i = 0; i < dk_ndrive; i++)
			if (strcmp(args, dr_name[i]) == 0) {
				if (dk_mspw[i] != 0.0)
					selections[i] = truefalse;
				else
					error("%s: drive not configured",
					    dr_name[i]);
				break;
			}
		if (i >= dk_ndrive)
			error("%s: unknown drive", args);
		args = cp;
	}
}
