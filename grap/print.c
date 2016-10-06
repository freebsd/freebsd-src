/*
 * Changes by Gunnar Ritter, Freiburg i. Br., Germany, October 2005.
 *
 * Derived from Plan 9 v4 /sys/src/cmd/grap/
 *
 * Copyright (C) 2003, Lucent Technologies Inc. and others.
 * All Rights Reserved.
 *
 * Distributed under the terms of the Lucent Public License Version 1.02.
 */

/*	Sccsid @(#)print.c	1.3 (gritter) 10/18/05	*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "grap.h"
#include "y.tab.h"

double	margin	= MARGIN;	/* extra space around edges */
extern	double	frame_ht, frame_wid, ticklen;
extern int just, sizeop, tick_dir;
extern double sizexpr, lab_up, lab_rt;

char	graphname[50] = "Graph";
char	graphpos[200] = "";

void print(void)	/* arrange final output */
{
	FILE *fd;
	Obj *p, *dfp;
	int c;
	double dx, dy, xfac, yfac;

	if (tfd != NULL) {
		fclose(tfd);	/* end the temp file */
		tfd = stdout;
	}

	if ((p=lookup("margin",0)) != NULL)
		margin = p->fval;
	if (frame_ht < 0)	/* wasn't set explicitly, so use default */
		frame_ht = getvar(lookup("frameht", 0));
	if (frame_wid < 0)
		frame_wid = getvar(lookup("framewid", 0));
	dfp = NULL;
	for (p = objlist; p; p = p->next) {
		dprintf("print: name = <%s>, type = %d\n", p->name, p->type);
		if (p->type == NAME) {
			Point pt, pt1;	
			pt = p->pt;
			pt1 = p->pt1;
			fprintf(tfd, "\t# %s %g .. %g, %g .. %g\n",
				p->name, pt.x, pt1.x, pt.y, pt1.y);
			if (p->log & XFLAG) {
				if (pt.x <= 0.0)
					FATAL("can't take log of x coord %g", pt.x);
				logit(pt.x);
				logit(pt1.x);
			}
			if (p->log & YFLAG) {
				if (pt.y <= 0.0)
					FATAL("can't take log of y coord %g", pt.y);
				logit(pt.y);
				logit(pt1.y);
			}
			if (!(p->coord & XFLAG)) {
				dx = pt1.x - pt.x;
				pt.x -= margin * dx;
				pt1.x += margin * dx;
			}
			if (!(p->coord & YFLAG)) {
				dy = pt1.y - pt.y;
				pt.y -= margin * dy;
				pt1.y += margin * dy;
			}
			if (autoticks && strcmp(p->name, dflt_coord) == 0) {
				p->pt = pt;
				p->pt1 = pt1;
				if (p->log & XFLAG) {
					p->pt.x = pow(10.0, pt.x);
					p->pt1.x = pow(10.0, pt1.x);
				}
				if (p->log & YFLAG) {
					p->pt.y = pow(10.0, pt.y);
					p->pt1.y = pow(10.0, pt1.y);
				}
				dfp = setauto();
			}		
			dx = pt1.x - pt.x;
			dy = pt1.y - pt.y;
			xfac = dx > 0 ? frame_wid/dx : frame_wid/2;
			yfac = dy > 0 ? frame_ht/dy : frame_ht/2;

			fprintf(tfd, "define xy_%s @ ", p->name);
			if (dx > 0)
				fprintf(tfd, "\t(($1)-(%g))*%g", pt.x, xfac);
			else
				fprintf(tfd, "\t%g", xfac);
			if (dy > 0)
				fprintf(tfd, ", (($2)-(%g))*%g @\n", pt.y, yfac);
			else
				fprintf(tfd, ", %g @\n", yfac);
			fprintf(tfd, "define x_%s @ ", p->name);
			if (dx > 0)
				fprintf(tfd, "\t(($1)-(%g))*%g @\n", pt.x, xfac);
			else
				fprintf(tfd, "\t%g @\n", xfac);
			fprintf(tfd, "define y_%s @ ", p->name);
			if (dy > 0)
				fprintf(tfd, "\t(($1)-(%g))*%g @\n", pt.y, yfac);
			else
				fprintf(tfd, "\t%g @\n", yfac);
		}
	}
	if (codegen)
		frame();
	if (codegen && autoticks && dfp)
		do_autoticks(dfp);

	if ((fd = fopen(tempfile, "r")) != NULL) {
		while ((c = getc(fd)) != EOF)
			putc(c, tfd);
		fclose(fd);
	}
	tfd = NULL;
}

void endstat(void)	/* clean up after each statement */
{

	just = sizeop = 0;
	lab_up = lab_rt = 0.0;
	sizexpr = 0.0;
	nnum = 0;
	ntick = 0;
	tside = 0;
	tick_dir = OUT;
	ticklen = TICKLEN;
}

void graph(char *s)	/* graph statement */
{
	char *p, *os;
	int c;

	if (codegen) {
		fprintf(stdout, "%s: [\n", graphname);
		print();	/* pump out previous graph */
		fprintf(stdout, "\n] %s\n", graphpos);
		reset();
	}
	if (s) {
		dprintf("into graph with <%s>\n", s);
		opentemp();
		os = s;
		while ((c = *s) == ' ' || c == '\t')
			s++;
		if (c == '\0')
			WARNING("no name on graph statement");
		if (!isupper((int)s[0]))
			WARNING("graph name %s must be capitalized", s);
		for (p=graphname; (c = *s) != ' ' && c != '\t' && c != '\0'; )
			*p++ = *s++;
		*p = '\0';
		n_strcpy(graphpos, s, sizeof(graphpos));
		dprintf("graphname = <%s>, graphpos = <%s>\n", graphname, graphpos);
		free(os);
	}
}

void setup(void)		/* done at each .G1 */
{
	static int firstG1 = 0;

	reset();
	opentemp();
	frame_ht = frame_wid = -1;	/* reset in frame() */
	ticklen = getvar(lookup("ticklen", 0));
	if (firstG1++ == 0)
		do_first();
	codegen = synerr = 0;
	n_strcpy(graphname, "Graph", sizeof(graphname));
	n_strcpy(graphpos, "", sizeof(graphpos));
}

void do_first(void)	/* done at first .G1:  definitions, etc. */
{
	extern int lib;
	extern char *lib_defines;
	static char buf[50], buf1[50];	/* static because pbstr uses them */
	FILE *fp;
	extern int getpid(void);

	snprintf(buf, sizeof(buf), "define pid /%d/\n", getpid());
	pbstr(buf);	
	if (lib != 0) {
		if ((fp = fopen(lib_defines, "r")) != NULL) {
			snprintf(buf1, sizeof(buf1), "copy \"%s\"\n",
			    lib_defines);
			pbstr(buf1);
			fclose(fp);
		} else {
			fprintf(stderr, "grap warning: can't open %s\n", lib_defines);
		}
	}
}

void reset(void)		/* done at each "graph ..." statement */
{
	Obj *p, *np, *deflist;
	extern int tlist, toffside, autodir;

	curr_coord = dflt_coord;
	ncoord = auto_x = 0;
	autoticks = LEFT|BOT;
	autodir = 0;
	tside = tlist = toffside = 0;
	tick_dir = OUT;
	margin = MARGIN;
	deflist = NULL;
	for (p = objlist; p; p = np) {
		np = p->next;
		if (p->type == DEFNAME || p->type == VARNAME) {
			p->next = deflist;
			deflist = p;
		} else {
			free(p->name);
			freeattr(p->attr);
			free((char *) p);
		}
	}
	objlist = deflist;
}

void opentemp(void)
{
	if (tfd != stdout) {
		if (tfd != NULL)
			fclose(tfd);
		if ((tfd = fopen(tempfile, "w")) == NULL) {
			fprintf(stderr, "grap: can't open %s\n", tempfile);
			exit(1);
		}
  	}
}
