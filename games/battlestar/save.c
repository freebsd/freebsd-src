/*
 * Copyright (c) 1983, 1993
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
static char sccsid[] = "@(#)save.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

#include "externs.h"

restore()
{
	char *getenv();
	char *home;
	char home1[100];
	register int n;
	int tmp;
	register FILE *fp;

	home = getenv("HOME");
	strcpy(home1, home);
	strcat(home1, "/Bstar");
	if ((fp = fopen(home1, "r")) == 0) {
		perror(home1);
		return;
	}
	fread(&WEIGHT, sizeof WEIGHT, 1, fp);
	fread(&CUMBER, sizeof CUMBER, 1, fp);
	fread(&clock, sizeof clock, 1, fp);
	fread(&tmp, sizeof tmp, 1, fp);
	location = tmp ? dayfile : nightfile;
	for (n = 1; n <= NUMOFROOMS; n++) {
		fread(location[n].link, sizeof location[n].link, 1, fp);
		fread(location[n].objects, sizeof location[n].objects, 1, fp);
	}
	fread(inven, sizeof inven, 1, fp);
	fread(wear, sizeof wear, 1, fp);
	fread(injuries, sizeof injuries, 1, fp);
	fread(notes, sizeof notes, 1, fp);
	fread(&direction, sizeof direction, 1, fp);
	fread(&position, sizeof position, 1, fp);
	fread(&time, sizeof time, 1, fp);
	fread(&fuel, sizeof fuel, 1, fp);
	fread(&torps, sizeof torps, 1, fp);
	fread(&carrying, sizeof carrying, 1, fp);
	fread(&encumber, sizeof encumber, 1, fp);
	fread(&rythmn, sizeof rythmn, 1, fp);
	fread(&followfight, sizeof followfight, 1, fp);
	fread(&ate, sizeof ate, 1, fp);
	fread(&snooze, sizeof snooze, 1, fp);
	fread(&meetgirl, sizeof meetgirl, 1, fp);
	fread(&followgod, sizeof followgod, 1, fp);
	fread(&godready, sizeof godready, 1, fp);
	fread(&win, sizeof win, 1, fp);
	fread(&wintime, sizeof wintime, 1, fp);
	fread(&matchlight, sizeof matchlight, 1, fp);
	fread(&matchcount, sizeof matchcount, 1, fp);
	fread(&loved, sizeof loved, 1, fp);
	fread(&pleasure, sizeof pleasure, 1, fp);
	fread(&power, sizeof power, 1, fp);
	fread(&ego, sizeof ego, 1, fp);
}

save()
{
	char *getenv();
	char *home;
	char home1[100];
	register int n;
	int tmp;
	FILE *fp;

	home = getenv("HOME");
	strcpy(home1, home);
	strcat(home1, "/Bstar");
	if ((fp = fopen(home1, "w")) == 0) {
		perror(home1);
		return;
	}
	printf("Saved in %s.\n", home1);
	fwrite(&WEIGHT, sizeof WEIGHT, 1, fp);
	fwrite(&CUMBER, sizeof CUMBER, 1, fp);
	fwrite(&clock, sizeof clock, 1, fp);
	tmp = location == dayfile;
	fwrite(&tmp, sizeof tmp, 1, fp);
	for (n = 1; n <= NUMOFROOMS; n++) {
		fwrite(location[n].link, sizeof location[n].link, 1, fp);
		fwrite(location[n].objects, sizeof location[n].objects, 1, fp);
	}
	fwrite(inven, sizeof inven, 1, fp);
	fwrite(wear, sizeof wear, 1, fp);
	fwrite(injuries, sizeof injuries, 1, fp);
	fwrite(notes, sizeof notes, 1, fp);
	fwrite(&direction, sizeof direction, 1, fp);
	fwrite(&position, sizeof position, 1, fp);
	fwrite(&time, sizeof time, 1, fp);
	fwrite(&fuel, sizeof fuel, 1, fp);
	fwrite(&torps, sizeof torps, 1, fp);
	fwrite(&carrying, sizeof carrying, 1, fp);
	fwrite(&encumber, sizeof encumber, 1, fp);
	fwrite(&rythmn, sizeof rythmn, 1, fp);
	fwrite(&followfight, sizeof followfight, 1, fp);
	fwrite(&ate, sizeof ate, 1, fp);
	fwrite(&snooze, sizeof snooze, 1, fp);
	fwrite(&meetgirl, sizeof meetgirl, 1, fp);
	fwrite(&followgod, sizeof followgod, 1, fp);
	fwrite(&godready, sizeof godready, 1, fp);
	fwrite(&win, sizeof win, 1, fp);
	fwrite(&wintime, sizeof wintime, 1, fp);
	fwrite(&matchlight, sizeof matchlight, 1, fp);
	fwrite(&matchcount, sizeof matchcount, 1, fp);
	fwrite(&loved, sizeof loved, 1, fp);
	fwrite(&pleasure, sizeof pleasure, 1, fp);
	fwrite(&power, sizeof power, 1, fp);
	fwrite(&ego, sizeof ego, 1, fp);
}
