/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */
     
/*
 * Copyright (c) 1983, 1984 1985, 1986, 1987, 1988, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

/*	from OpenSolaris "diacrit.c	1.6	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)diacrit.c	1.7 (gritter) 1/13/08
 */

#include "e.h"
#include "y.tab.h"

void
diacrit(int p1, int type) {
	int c, t;
#ifndef NEQN
	float effps;
#endif /* NEQN */

	c = oalloc();
	t = oalloc();
#ifdef NEQN
	nrwid(p1, ps, p1);
	printf(".nr 10 %gu\n", max(eht[p1]-ebase[p1]-VERT(2),0));
#else /* NEQN */
	effps = EFFPS(ps);
	nrwid(p1, effps, p1);
	printf(".nr 10 %gp\n", VERT(max(eht[p1]-ebase[p1]-EM(1,ps),0)));	/* vertical shift if high */
	printf(".if \\n(ct>1 .nr 10 \\n(10+\\s%s.25m\\s0\n", tsize(effps));
	if (type != HAT && type != TILDE) {
		printf(".nr %d \\s%s.1m\\s0\n", t, tsize(effps));	/* horiz shift if high */
		printf(".if \\n(ct>1 .nr %d \\s%s.15m\\s0\n", t, tsize(effps));
	} else
		printf(".nr %d 0\n", t);
#endif /* NEQN */
	switch(type) {
		case VEC:	/* vec */
#ifndef NEQN
			printf(".ds %d \\v'-.4m'\\s%s\\(->\\s0\\v'.4m'\n",
			    c, tsize(max(effps-3, 6)));
			break;
#endif /* NEQN */
		case DYAD:	/* dyad */
#ifdef NEQN
			printf(".ds %d \\v'-12p'_\\v'12p'\n", c);
#else /* !NEQN */
			printf(".ds %d \\v'-.4m'\\s%s\\z\\(<-\\(->\\s0\\v'.4m'\n",
			    c, tsize(max(effps-3, 6)));
#endif /* !NEQN */
			break;
		case HAT:
			printf(".ds %d ^\n", c);
			break;
		case TILDE:
			printf(".ds %d ~\n", c);
			break;
		case DOT:
#ifndef NEQN
			printf(".ds %d \\s%s\\v'-.67m'.\\v'.67m'\\s0\n", c, tsize(effps));
#else /* NEQN */
			printf(".ds %d \\v'-12p'.\\v'12p'\n", c);
#endif /* NEQN */
			break;
		case DOTDOT:
#ifndef NEQN
			printf(".ds %d \\s%s\\v'-.67m'..\\v'.67m\\s0'\n", c, tsize(effps));
#else /* NEQN */
			printf(".ds %d \\v'-12p'..\\v'12p'\n", c);
#endif /* NEQN */
			break;
		case BAR:
#ifndef NEQN
			printf(".ds %d \\s%s\\v'.28m'\\h'.05m'\\l'\\n(%du-.1m\\(rn'\\h'.05m'\\v'-.28m'\\s0\n",
				c, tsize(effps), p1);
#else /* NEQN */
			printf(".ds %d \\v'-12p'\\l'\\n(%du'\\v'12p'\n", 
				c, p1);
#endif /* NEQN */
			break;
		case UNDER:
#ifndef NEQN
			printf(".ds %d \\l'\\n(%du\\(ul'\n", c, p1);
			printf(".nr %d 0\n", t);
			printf(".nr 10 0-%gp\n", ebase[p1]);
#else /* NEQN */
			printf(".ds %d \\l'\\n(%du'\n", c, p1);
#endif /* NEQN */
			break;
		}
	nrwid(c, ps, c);
#ifndef NEQN
	if (!ital(lfont[p1]))
		printf(".nr %d 0\n", t);
	printf(".as %d \\h'-\\n(%du-\\n(%du/2u+\\n(%du'\\v'0-\\n(10u'\\*(%d", 
		p1, p1, c, t, c);
	printf("\\v'\\n(10u'\\h'-\\n(%du+\\n(%du/2u-\\n(%du'\n", c, p1, t);
	/* BUG - should go to right end of widest */
#else /* NEQN */
	printf(".as %d \\h'-\\n(%du-\\n(%du/2u'\\v'0-\\n(10u'\\*(%d", 
		p1, p1, c, c);
	printf("\\v'\\n(10u'\\h'-\\n(%du+\\n(%du/2u'\n", c, p1);
#endif /* NEQN */
#ifndef NEQN
	if (type != UNDER)
		eht[p1] += VERT(EM(0.15, ps));	/* 0.15m */
	if(dbg)printf(".\tdiacrit: %c over S%d, lf=%c, rf=%c, h=%g,b=%g\n",
		type, p1, lfont[p1], rfont[p1], eht[p1], ebase[p1]);
#else /* NEQN */
	if (type != UNDER)
		eht[p1] += VERT(1);
	if (dbg) printf(".\tdiacrit: %c over S%d, h=%d, b=%d\n", type, p1, eht[p1], ebase[p1]);
#endif /* NEQN */
	ofree(c); ofree(t);
}
