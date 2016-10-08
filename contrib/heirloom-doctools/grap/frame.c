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

/*	Sccsid @(#)frame.c	1.3 (gritter) 10/18/05	*/
#include <stdio.h>
#include <stdlib.h>
#include "grap.h"
#include "y.tab.h"

double	frame_ht;	/* default frame height */
double	frame_wid;	/* and width */

int	nsides	= 0;		/* how many sides given on this frame */
char	*sides[] = {
		"\tline from Frame.nw to Frame.ne",
		"\tline from Frame.sw to Frame.se",
		"\tline from Frame.sw to Frame.nw",
		"\tline from Frame.se to Frame.ne"
};
char	*newsides[4] = { 0, 0, 0, 0 };	/* filled in later */

void frame(void)		/* pump out frame definition, reset for next */
{
	int i;

	fprintf(tfd, "\tframeht = %g\n", frame_ht);
	fprintf(tfd, "\tframewid = %g\n", frame_wid);
	fprintf(tfd, "Frame:\tbox ht frameht wid framewid with .sw at 0,0 ");
	if (nsides == 0)
		fprintf(tfd, "\n");
	else {
		fprintf(tfd, "invis\n");
		for (i = 0; i < 4; i++) {
			if (newsides[i]) {
				fprintf(tfd, "%s\n", newsides[i]);
				free(newsides[i]);
				newsides[i] = 0;
			} else
				fprintf(tfd, "%s\n", sides[i]);
		}
		nsides = 0;
	}
}

void frameht(double f)	/* set height of frame */
{
	frame_ht = f;
}

void framewid(double f)	/* set width of frame */
{
	frame_wid = f;
}

void frameside(int type, Attr *desc)	/* create and remember sides */
{
	int n = 0;
	char buf[100];

	nsides++;
	switch (type) {
	case 0:		/* no side specified; kludge up all */
		frameside(TOP, desc);
		frameside(BOT, desc);
		frameside(LEFT, desc);
		frameside(RIGHT, desc);
		return;
	case TOP:	n = 0; break;
	case BOT:	n = 1; break;
	case LEFT:	n = 2; break;
	case RIGHT:	n = 3; break;
	}
	snprintf(buf, sizeof(buf), "%s %s", sides[n], desc_str(desc));
	newsides[n] = tostring(buf);
}
