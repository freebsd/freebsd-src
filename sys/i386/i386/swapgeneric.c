/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *
 *	from: @(#)swapgeneric.c	5.5 (Berkeley) 5/9/91
 *	$Id: swapgeneric.c,v 1.29 1999/05/07 10:10:25 phk Exp $
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/disklabel.h>

#include <i386/i386/cons.h>

#include <machine/md_var.h>

static void	gets __P((char *));

void
setconf()
{
	char name[128];
	char *cp;
	int bd, unit;
	dev_t dev;

retry:
	printf("root device? ");
	gets(name);
	cp = name;
	while (cp != '\0' && (*cp < '0' || *cp > '9'))
		cp++;
	if (cp == name) {
		printf("missing device name\n");
		goto bad;
	}
	if (*cp == '\0') {
		printf("missing unit number\n");
		goto bad;
	}
	unit = *cp - '0';
	*cp++ = '\0';
	for (bd = 0; bd < nblkdev; bd++) {
		dev = makedev(bd, 0);
		if (bdevsw(dev) != NULL &&
		    strcmp(bdevsw(dev)->d_name, name) == 0)
			goto gotit;
	}
	goto bad;
gotit:
	while (*cp >= '0' && *cp <= '9')
		unit += 10 * unit + *cp++ - '0';
	if (*cp != '\0') {
		printf("junk after unit number\n");
		goto bad;
	}
	rootdev = makedev(bd, dkmakeminor(unit, 0, 0));
	return;
bad:
	printf("use dk%%d\n");
	goto retry;
}

static void
gets(cp)
	char *cp;
{
	register char *lp;
	register int c;

	lp = cp;
	for (;;) {
		printf("%c", c = cngetc() & 0177);
		switch (c) {
		case -1:
		case '\n':
		case '\r':
			*lp++ = '\0';
			return;
		case '\b':
		case '\177':
			if (lp > cp) {
				printf(" \b");
				lp--;
			}
			continue;
		case '#':
			lp--;
			if (lp < cp)
				lp = cp;
			continue;
		case '@':
		case 'u' & 037:
			lp = cp;
			printf("%c", '\n');
			continue;
		default:
			*lp++ = c;
		}
	}
}
