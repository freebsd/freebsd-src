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
 *	$Id: swapgeneric.c,v 1.14 1995/12/04 16:47:43 phk Exp $
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/disklabel.h>
#include <sys/kernel.h>
#include <sys/devconf.h>

#include <i386/i386/cons.h>
#include <machine/md_var.h>

#include "wd.h"
#include "fd.h"
#include "sd.h"
#include "vn.h"
#include "cd.h"
#include "mcd.h"
#include "scd.h"
#include "matcd.h"
#include "od.h"

/*
 * Generic configuration;  all in one
 */
dev_t	rootdev = NODEV;
dev_t	dumpdev = NODEV;

#ifdef notused
int	nswap;
struct	swdevt swdevt[] = {
	{ makedev (0xFF, 0x00000001),	0,	0 },
#if NVN > 0
	{ makedev (15, 0x00000001),	0,	0 },
#endif
	{ NODEV,	0,	0 }, /* For NFS diskless */
	{ NODEV,	0,	0 },
};
int	dmmin, dmmax, dmtext;
#endif

#ifdef NFS
extern int (*mountroot)  __P((void));
extern int nfs_mountroot __P((void));
#endif

extern struct kern_devconf *dc_list;
void gets __P((char *));

struct	genericconf {
	char	*gc_name;
	dev_t	gc_root;
} genericconf[] = {
#if NWD > 0
	{ "wd",		makedev(0, 0x00000000),	},
#endif
#if NFD > 0
	{ "fd",		makedev(2, 0x00000000),	},
#endif
#if NSD > 0
	{ "sd",		makedev(4, 0x00000000),	},
#endif
#if NCD > 0
	{ "cd",		makedev(6, 0x00000000),	},
#endif
#if NMCD > 0
	{ "mcd",	makedev(7, 0x00000000),	},
#endif
#if NSCD > 0
	{ "scd",	makedev(16,0x00000000),	},
#endif
#if NMATCD > 0
	{ "matcd",	makedev(17,0x00000000),	},
#endif
#if NOD > 0
	{ "od",		makedev(20,0x00000000),	},
#endif
	{ 0 },
};

void setconf(void)
{
	register struct genericconf *gc;
	register struct kern_devconf *kdc;
	int unit, swaponroot = 0;

	if (rootdev != NODEV)
		return;
	if (boothowto & RB_ASKNAME) {
		char name[128];
retry:
		printf("root device? ");
		gets(name);
		for (gc = genericconf; gc->gc_name; gc++)
			if (gc->gc_name[0] == name[0] &&
			    gc->gc_name[1] == name[1])
				goto gotit;
		goto bad;
gotit:
		if (name[3] == '*') {
			name[3] = name[4];
			swaponroot++;
		}
		if (name[2] >= '0' && name[2] <= '7' && name[3] == 0) {
			unit = name[2] - '0';
			goto found;
		}
		printf("bad/missing unit number\n");
bad:
		printf("use dk%%d\n");
		goto retry;
	}
#ifdef NFS
	if (mountroot == nfs_mountroot) {
		/*
		 * The NFS code in nfs_vfsops.c handles root and swap
		 * for us if we're booting diskless. This is just to
		 * make swapconf() happy.
		 */
		dumplo = -1;
		return;
	}
#endif
	unit = 0;
	for (gc = genericconf; gc->gc_name; gc++) {
		kdc = dc_list;
		while (kdc->kdc_next) {
			if (!strcmp(kdc->kdc_name, gc->gc_name) &&
				kdc->kdc_unit == 0) {
				printf("root on %s0\n", kdc->kdc_name);
				goto found;
			}
			kdc = kdc->kdc_next;
		}
	}
	printf("no suitable root -- press any key to reboot\n\n");
	cngetc();
	cpu_reset();
	for(;;) ;
found:
	gc->gc_root = makedev(major(gc->gc_root), unit * MAXPARTITIONS);
	rootdev = gc->gc_root;
}

void gets(cp)
	char *cp;
{
	register char *lp;
	register c;

	lp = cp;
	for (;;) {
		printf("%c", c = cngetc()&0177);
		switch (c) {
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
		case 'u'&037:
			lp = cp;
			printf("%c", '\n');
			continue;
		default:
			*lp++ = c;
		}
	}
}
