/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1998 Kenneth D. Merry.
 *               2015 Yoshihiro Ota
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
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
 */

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#ifdef lint
static const char sccsid[] = "@(#)disks.c	8.1 (Berkeley) 6/6/93";
#endif

#include <sys/types.h>
#include <sys/devicestat.h>
#include <sys/resource.h>

#include <ctype.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "systat.h"
#include "extern.h"
#include "devs.h"

typedef enum {
	DS_MATCHTYPE_NONE,
	DS_MATCHTYPE_SPEC,
	DS_MATCHTYPE_PATTERN
} last_match_type;

struct statinfo cur_dev, last_dev, run_dev;

static last_match_type last_type;
struct device_selection *dev_select;
long generation;
int num_devices, num_selected;
int num_selections;
long select_generation;
static struct devstat_match *matches = NULL;
static int num_matches = 0;
static char **specified_devices;
static int num_devices_specified = 0;

static int dsmatchselect(const char *args, devstat_select_mode select_mode,
			 int maxshowdevs, struct statinfo *s1);
static int dsselect(const char *args, devstat_select_mode select_mode,
		    int maxshowdevs, struct statinfo *s1);

int
dsinit(int maxshowdevs)
{
	/*
	 * Make sure that the userland devstat version matches the kernel
	 * devstat version.  If not, exit and print a message informing
	 * the user of his mistake.
	 */
	if (devstat_checkversion(NULL) < 0)
		errx(1, "%s", devstat_errbuf);

	if( cur_dev.dinfo ) // init was alreay ran
		return(1);

	if ((num_devices = devstat_getnumdevs(NULL)) < 0) {
		warnx("%s", devstat_errbuf);
		return(0);
	}

	cur_dev.dinfo = calloc(1, sizeof(struct devinfo));
	last_dev.dinfo = calloc(1, sizeof(struct devinfo));
	run_dev.dinfo = calloc(1, sizeof(struct devinfo));

	generation = 0;
	num_devices = 0;
	num_selected = 0;
	num_selections = 0;
	select_generation = 0;
	last_type = DS_MATCHTYPE_NONE;

	if (devstat_getdevs(NULL, &cur_dev) == -1)
		errx(1, "%s", devstat_errbuf);

	num_devices = cur_dev.dinfo->numdevs;
	generation = cur_dev.dinfo->generation;

	dev_select = NULL;

	/*
	 * At this point, selectdevs will almost surely indicate that the
	 * device list has changed, so we don't look for return values of 0
	 * or 1.  If we get back -1, though, there is an error.
	 */
	if (devstat_selectdevs(&dev_select, &num_selected, &num_selections,
	    &select_generation, generation, cur_dev.dinfo->devices, num_devices,
	    NULL, 0, NULL, 0, DS_SELECT_ADD, maxshowdevs, 0) == -1)
		errx(1, "%d %s", __LINE__, devstat_errbuf);

	return(1);
}


void
dsgetinfo(struct statinfo* dev)
{
	switch (devstat_getdevs(NULL, dev)) {
	case -1:
		errx(1, "%s", devstat_errbuf);
		break;
	case 1:
		num_devices = dev->dinfo->numdevs;
		generation = dev->dinfo->generation;
		cmdkre("refresh", NULL);
		break;
	default:
		break;
	}
}

int
dscmd(const char *cmd, const char *args, int maxshowdevs, struct statinfo *s1)
{
	int retval;

	if (prefix(cmd, "display") || prefix(cmd, "add"))
		return(dsselect(args, DS_SELECT_ADDONLY, maxshowdevs, s1));
	if (prefix(cmd, "ignore") || prefix(cmd, "delete"))
		return(dsselect(args, DS_SELECT_REMOVE, maxshowdevs, s1));
	if (prefix(cmd, "show") || prefix(cmd, "only"))
		return(dsselect(args, DS_SELECT_ONLY, maxshowdevs, s1));
	if (prefix(cmd, "type") || prefix(cmd, "match"))
		return(dsmatchselect(args, DS_SELECT_ONLY, maxshowdevs, s1));
	if (prefix(cmd, "refresh")) {
		retval = devstat_selectdevs(&dev_select, &num_selected,
		    &num_selections, &select_generation, generation,
		    s1->dinfo->devices, num_devices,
		    (last_type ==DS_MATCHTYPE_PATTERN) ?  matches : NULL,
		    (last_type ==DS_MATCHTYPE_PATTERN) ?  num_matches : 0,
		    (last_type == DS_MATCHTYPE_SPEC) ?specified_devices : NULL,
		    (last_type == DS_MATCHTYPE_SPEC) ?num_devices_specified : 0,
		    (last_type == DS_MATCHTYPE_NONE) ?  DS_SELECT_ADD :
		    DS_SELECT_ADDONLY, maxshowdevs, 0);
		if (retval == -1) {
			warnx("%s", devstat_errbuf);
			return(0);
		} else if (retval == 1)
			return(2);
	}
	if (prefix(cmd, "drives")) {
		int i;
		move(CMDLINE, 0);
		clrtoeol();
		for (i = 0; i < num_devices; i++) {
			printw("%s%d ", s1->dinfo->devices[i].device_name,
			       s1->dinfo->devices[i].unit_number);
		}
		return(1);
	}
	return(0);
}

static int
dsmatchselect(const char *args, devstat_select_mode select_mode, int maxshowdevs,
	      struct statinfo *s1)
{
	char **tempstr, *tmpstr, *tmpstr1;
	char *tstr[100];
	int num_args = 0;
	int i;
	int retval = 0;

	if (!args) {
		warnx("dsmatchselect: no arguments");
		return(1);
	}

	/*
	 * Break the (pipe delimited) input string out into separate
	 * strings.
	 */
	tmpstr = tmpstr1 = strdup(args);
	for (tempstr = tstr, num_args  = 0;
	     (*tempstr = strsep(&tmpstr1, "|")) != NULL && (num_args < 100);
	     num_args++)
		if (**tempstr != '\0')
			if (++tempstr >= &tstr[100])
				break;
	free(tmpstr);

	if (num_args > 99) {
		warnx("dsmatchselect: too many match arguments");
		return(0);
	}

	/*
	 * If we've gone through the matching code before, clean out
	 * previously used memory.
	 */
	if (num_matches > 0) {
		free(matches);
		matches = NULL;
		num_matches = 0;
	}

	for (i = 0; i < num_args; i++) {
		if (devstat_buildmatch(tstr[i], &matches, &num_matches) != 0) {
			warnx("%s", devstat_errbuf);
			return(0);
		}
	}
	if (num_args > 0) {

		last_type = DS_MATCHTYPE_PATTERN;

		retval = devstat_selectdevs(&dev_select, &num_selected,
		    &num_selections, &select_generation, generation,
		    s1->dinfo->devices, num_devices, matches, num_matches,
		    NULL, 0, select_mode, maxshowdevs, 0);
		if (retval == -1)
			err(1, "device selection error");
		else if (retval == 1)
			return(2);
	}
	return(1);
}

static int
dsselect(const char *args, devstat_select_mode select_mode, int maxshowdevs,
	 struct statinfo *s1)
{
	char *cp, *tmpstr, *tmpstr1, *buffer;
	int i;
	int retval = 0;

	if (!args) {
		warnx("dsselect: no argument");
		return(1);
	}

	/*
	 * If we've gone through this code before, free previously
	 * allocated resources.
	 */
	if (num_devices_specified > 0) {
		for (i = 0; i < num_devices_specified; i++)
			free(specified_devices[i]);
		free(specified_devices);
		specified_devices = NULL;
		num_devices_specified = 0;
	}

	/* do an initial malloc */
	specified_devices = (char **)malloc(sizeof(char *));

	tmpstr = tmpstr1 = strdup(args);
	cp = strchr(tmpstr1, '\n');
	if (cp)
		*cp = '\0';
	for (;;) {
		for (cp = tmpstr1; *cp && isspace(*cp); cp++)
			;
		tmpstr1 = cp;
		for (; *cp && !isspace(*cp); cp++)
			;
		if (*cp)
			*cp++ = '\0';
		if (cp - tmpstr1 == 0)
			break;
		for (i = 0; i < num_devices; i++) {
			asprintf(&buffer, "%s%d", dev_select[i].device_name,
				dev_select[i].unit_number);
			if (strcmp(buffer, tmpstr1) == 0) {

				num_devices_specified++;

				specified_devices =(char **)realloc(
						specified_devices,
						sizeof(char *) *
						num_devices_specified);
				specified_devices[num_devices_specified -1]=
					strdup(tmpstr1);
				free(buffer);

				break;
			}
			else
				free(buffer);
		}
		if (i >= num_devices)
			error("%s: unknown drive", args);
		tmpstr1 = cp;
	}
	free(tmpstr);

	if (num_devices_specified > 0) {
		last_type = DS_MATCHTYPE_SPEC;

		retval = devstat_selectdevs(&dev_select, &num_selected,
		    &num_selections, &select_generation, generation,
		    s1->dinfo->devices, num_devices, NULL, 0,
		    specified_devices, num_devices_specified,
		    select_mode, maxshowdevs, 0);
		if (retval == -1)
			err(1, "%s", devstat_errbuf);
		else if (retval == 1)
			return(2);
	}
	return(1);
}


void
dslabel(int maxdrives, int diskcol, int diskrow)
{
	int i, j;

	mvprintw(diskrow, diskcol, "Disks");
	mvprintw(diskrow + 1, diskcol, "KB/t");
	mvprintw(diskrow + 2, diskcol, "tps");
	mvprintw(diskrow + 3, diskcol, "MB/s");
	mvprintw(diskrow + 4, diskcol, "%%busy");
	/*
	 * For now, we don't support a fourth disk statistic.  So there's
	 * no point in providing a label for it.  If someone can think of a
	 * fourth useful disk statistic, there is room to add it.
	 */
	/* mvprintw(diskrow + 4, diskcol, " msps"); */
	j = 0;
	for (i = 0; i < num_devices && j < maxdrives; i++)
		if (dev_select[i].selected) {
			char tmpstr[80];
			sprintf(tmpstr, "%s%d", dev_select[i].device_name,
				dev_select[i].unit_number);
			mvprintw(diskrow, diskcol + 5 + 6 * j,
				" %5.5s", tmpstr);
			j++;
		}
}

static void
dsshow2(int diskcol, int diskrow, int dn, int lc, struct statinfo *now, struct statinfo *then)
{
	long double transfers_per_second;
	long double kb_per_transfer, mb_per_second;
	long double elapsed_time, device_busy;
	int di;

	di = dev_select[dn].position;

	if (then != NULL) {
		/* Calculate relative to previous sample */
		elapsed_time = now->snap_time - then->snap_time;
	} else {
		/* Calculate relative to device creation */
		elapsed_time = now->snap_time - devstat_compute_etime(
		    &now->dinfo->devices[di].creation_time, NULL);
	}

	if (devstat_compute_statistics(&now->dinfo->devices[di], then ?
	    &then->dinfo->devices[di] : NULL, elapsed_time,
	    DSM_KB_PER_TRANSFER, &kb_per_transfer,
	    DSM_TRANSFERS_PER_SECOND, &transfers_per_second,
	    DSM_MB_PER_SECOND, &mb_per_second,
	    DSM_BUSY_PCT, &device_busy,
	    DSM_NONE) != 0)
		errx(1, "%s", devstat_errbuf);

	lc = diskcol + lc * 6;
	putlongdouble(kb_per_transfer, diskrow + 1, lc, 5, 2, 0);
	putlongdouble(transfers_per_second, diskrow + 2, lc, 5, 0, 0);
	putlongdouble(mb_per_second, diskrow + 3, lc, 5, 2, 0);
	putlongdouble(device_busy, diskrow + 4, lc, 5, 0, 0);
}

void
dsshow(int maxdrives, int diskcol, int diskrow, struct statinfo *now, struct statinfo *then)
{
	int i, lc;

	for (i = 0, lc = 0; i < num_devices && lc < maxdrives; i++)
		if (dev_select[i].selected)
			dsshow2(diskcol, diskrow, i, ++lc, now, then);
}
