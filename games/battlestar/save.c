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
#if 0
static char sccsid[] = "@(#)save.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD: src/games/battlestar/save.c,v 1.8 1999/11/30 03:48:39 billf Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>			/* MAXPATHLEN */
#include <fcntl.h>
#include <stdlib.h>
#include <err.h>
#include "externs.h"

void
restore()
{
	char *home;
	char home1[MAXPATHLEN];
	int n;
	int tmp;
	FILE *fp;

	if ( (home = getenv("HOME")) != NULL) 
  	  sprintf(home1, "%.*s/Bstar", MAXPATHLEN - 7, home);
	else return;

	if ((fp = fopen(home1, "r")) == 0) {
		perror(home1);
		return;
	}
	fread(&WEIGHT, sizeof WEIGHT, 1, fp);
	fread(&CUMBER, sizeof CUMBER, 1, fp);
	fread(&gclock, sizeof gclock, 1, fp);
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
	fread(&gtime, sizeof gtime, 1, fp);
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
	/* We must check the last read, to catch truncated save files.  */
	if (fread(&ego, sizeof ego, 1, fp) < 1)
		errx(1, "save file %s too short", home1);
	fclose(fp);
}

void
save()
{
	struct stat sbuf;
	char *home;
	char home1[MAXPATHLEN];
	int n;
	int tmp, fd;
	FILE *fp;

	home = getenv("HOME");
	if (home == 0)
		return;
	sprintf(home1, "%.*s/Bstar", MAXPATHLEN - 7, home);

	/* Try to open the file safely. */
	if (stat(home1, &sbuf) < 0) {	  	
		fd = open(home1, O_WRONLY|O_CREAT|O_EXCL, 0600);
	        if (fd < 0) {
          		fprintf(stderr, "Can't create %s\n", home1);
           		return;
	        }
	} else {
		if ((sbuf.st_mode & S_IFLNK) == S_IFLNK) {
			fprintf(stderr, "No symlinks!\n");
			return;
		}

		fd = open(home1, O_WRONLY|O_EXCL);
		if (fd < 0) {
			fprintf(stderr, "Can't open %s for writing\n", home1);
			return;
		}
	}

	if ((fp = fdopen(fd, "w")) == 0) {
		perror(home1);
		return;
	}

	printf("Saved in %s.\n", home1);
	fwrite(&WEIGHT, sizeof WEIGHT, 1, fp);
	fwrite(&CUMBER, sizeof CUMBER, 1, fp);
	fwrite(&gclock, sizeof gclock, 1, fp);
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
	fwrite(&gtime, sizeof gtime, 1, fp);
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
