/*
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
static char sccsid[] = "@(#)mkswapconf.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * Build a swap configuration file.
 */
#include "config.h"

#include <sys/disklabel.h>
#include <sys/diskslice.h>

#include <ctype.h>
#include <stdio.h>

swapconf()
{
	register struct file_list *fl;
	struct file_list *do_swap();

	fl = conf_list;
	while (fl) {
		if (fl->f_type != SYSTEMSPEC) {
			fl = fl->f_next;
			continue;
		}
		fl = do_swap(fl);
	}
}

struct file_list *
do_swap(fl)
	register struct file_list *fl;
{
	FILE *fp;
	char  newswapname[80];
	char  swapname[80];
	register struct file_list *swap;
	dev_t dev;

	if (eq(fl->f_fn, "generic")) {
		fl = fl->f_next;
		return (fl->f_next);
	}
	(void) sprintf(swapname, "swap%s.c", fl->f_fn);
	(void) sprintf(newswapname, "swap%s.c.new", fl->f_fn);
	fp = fopen(path(newswapname), "w");
	if (fp == 0) {
		perror(path(newswapname));
		exit(1);
	}
	fprintf(fp, "#include <sys/param.h>\n");
	fprintf(fp, "#include <sys/conf.h>\n");
	fprintf(fp, "\n");
	/*
	 * If there aren't any swap devices
	 * specified, just return, the error
	 * has already been noted.
	 */
	swap = fl->f_next;
	if (swap == 0 || swap->f_type != SWAPSPEC) {
		(void) unlink(path(newswapname));
		fclose(fp);
		return (swap);
	}
	fprintf(fp, "dev_t\trootdev = makedev(%d, 0x%08x);\t\t/* %s */\n",
		major(fl->f_rootdev), minor(fl->f_rootdev),
		devtoname(fl->f_rootdev));
	if (fl->f_dumpdev != NODEV) {
		fprintf(fp, "dev_t\tdumpdev = makedev(%d, 0x%08x);\t\t/* %s */\n",
			major(fl->f_dumpdev), minor(fl->f_dumpdev),
			devtoname(fl->f_dumpdev));
	} else {
		fprintf(fp, "dev_t\tdumpdev = NODEV;\t\t\t/* unconfigured */\n");
	}
	fprintf(fp, "\n");
	fprintf(fp, "void\nsetconf()\n{\n}\n");
	fclose(fp);
	moveifchanged(path(newswapname), path(swapname));
	return (swap);
}

static	int devtablenotread = 1;
static	struct devdescription {
	char	*dev_name;
	int	dev_major;
	struct	devdescription *dev_next;
} *devtable;

/*
 * Given a device name specification figure out:
 *	major device number
 *	partition
 *	device name
 *	unit number
 * This is a hack, but the system still thinks in
 * terms of major/minor instead of string names.
 */
dev_t
nametodev(name, defunit, defslice, defpartition)
	char *name;
	int defunit;
	int defslice;
	char defpartition;
{
	char *cp, partition;
	int unit, slice;
	register struct devdescription *dp;

	cp = name;
	if (cp == 0) {
		fprintf(stderr, "config: internal error, nametodev\n");
		exit(1);
	}
	while (*cp && !isdigit(*cp))
		cp++;
	unit = *cp ? atoi(cp) : defunit;
	if (unit < 0 || unit > 31) {
		fprintf(stderr,
"config: %s: invalid device specification, unit out of range\n", name);
		unit = defunit;			/* carry on more checking */
	}
	if (*cp) {
		*cp++ = '\0';
		while (*cp && isdigit(*cp))
			cp++;
	}
	slice = defslice;
	if (*cp == 's') {
		++cp;
		if (*cp) {
			slice = atoi(cp);
			if (slice < 0 || slice >= MAX_SLICES - 1) {
				fprintf(stderr,
"config: %s: invalid device specification, slice out of range\n", cp);
				slice = defslice;
			}
			if (slice != COMPATIBILITY_SLICE)
				slice++;
			*cp++ = '\0';
			while (*cp && isdigit(*cp))
				cp++;
		}
	}
	partition = *cp ? *cp : defpartition;
	if (partition < 'a' || partition > 'h') {
		fprintf(stderr,
"config: %c: invalid device specification, bad partition\n", *cp);
		partition = defpartition;	/* carry on */
	}
	if (devtablenotread)
		initdevtable();
	for (dp = devtable; dp; dp = dp->dev_next)
		if (eq(name, dp->dev_name))
			break;
	if (dp == 0) {
		fprintf(stderr, "config: %s: unknown device\n", name);
		return (NODEV);
	}
	return (makedev(dp->dev_major,
			dkmakeminor(unit, slice, partition - 'a')));
}

char *
devtoname(dev)
	dev_t dev;
{
	char buf[80];
	register struct devdescription *dp;
	int part;
	char partname[2];
	int slice;
	char slicename[32];

	if (devtablenotread)
		initdevtable();
	for (dp = devtable; dp; dp = dp->dev_next)
		if (major(dev) == dp->dev_major)
			break;
	if (dp == 0)
		dp = devtable;
	part = dkpart(dev);
	slice = dkslice(dev);
	slicename[0] = partname[0] = '\0';
	if (slice != WHOLE_DISK_SLICE || part != RAW_PART) {
		partname[0] = 'a' + part;
		partname[1] = '\0';
		if (slice != COMPATIBILITY_SLICE)
			sprintf(slicename, "s%d", slice - 1);
	}
	(void) sprintf(buf, "%s%d%s%s", dp->dev_name,
		dkunit(dev), slicename, partname);
	return (ns(buf));
}

initdevtable()
{
	char linebuf[256];
	char buf[BUFSIZ];
	int maj;
	register struct devdescription **dp = &devtable;
	FILE *fp;

	(void) sprintf(buf, "../conf/devices.%s", machinename);
	fp = fopen(buf, "r");
	if (fp == NULL) {
		fprintf(stderr, "config: can't open %s\n", buf);
		exit(1);
	}
	while(fgets(linebuf,256,fp)) {
		/*******************************\
		* Allow a comment		*
		\*******************************/
		if(linebuf[0] == '#') continue;

		if (sscanf(linebuf, "%s\t%d\n", buf, &maj) == 2) {
			*dp = (struct devdescription *)malloc(sizeof (**dp));
			memset(*dp, 0, sizeof(**dp));
			(*dp)->dev_name = ns(buf);
			(*dp)->dev_major = maj;
			dp = &(*dp)->dev_next;
		} else {
			fprintf(stderr,"illegal line in devices file\n");
			break;
		}
	}
	*dp = 0;
	fclose(fp);
	devtablenotread = 0;
}
