/*
 * Copyright (c) 1980 Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1980 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)worms.c	5.9 (Berkeley) 2/28/91";
#endif /* not lint */

/*
 *
 *	 @@@        @@@    @@@@@@@@@@     @@@@@@@@@@@    @@@@@@@@@@@@
 *	 @@@        @@@   @@@@@@@@@@@@    @@@@@@@@@@@@   @@@@@@@@@@@@@
 *	 @@@        @@@  @@@@      @@@@   @@@@           @@@@ @@@  @@@@
 *	 @@@   @@   @@@  @@@        @@@   @@@            @@@  @@@   @@@
 *	 @@@  @@@@  @@@  @@@        @@@   @@@            @@@  @@@   @@@
 *	 @@@@ @@@@ @@@@  @@@        @@@   @@@            @@@  @@@   @@@
 *	  @@@@@@@@@@@@   @@@@      @@@@   @@@            @@@  @@@   @@@
 *	   @@@@  @@@@     @@@@@@@@@@@@    @@@            @@@  @@@   @@@
 *	    @@    @@       @@@@@@@@@@     @@@            @@@  @@@   @@@
 *
 *				 Eric P. Scott
 *			  Caltech High Energy Physics
 *				 October, 1980
 *
 */
#include <sys/types.h>
#include <stdio.h>
#ifdef USG
#include <termio.h>
#else
#include <sgtty.h>
#endif
#include <signal.h>

extern char *malloc(), *getenv(), *tgetstr(), *tgoto();

static struct options {
	int nopts;
	int opts[3];
}
	normal[8] = {
	{ 3, { 7, 0, 1 } },
	{ 3, { 0, 1, 2 } },
	{ 3, { 1, 2, 3 } },
	{ 3, { 2, 3, 4 } },
	{ 3, { 3, 4, 5 } },
	{ 3, { 4, 5, 6 } },
	{ 3, { 5, 6, 7 } },
	{ 3, { 6, 7, 0 } }
},	upper[8] = {
	{ 1, { 1, 0, 0 } },
	{ 2, { 1, 2, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 2, { 4, 5, 0 } },
	{ 1, { 5, 0, 0 } },
	{ 2, { 1, 5, 0 } }
},
	left[8] = {
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 2, { 2, 3, 0 } },
	{ 1, { 3, 0, 0 } },
	{ 2, { 3, 7, 0 } },
	{ 1, { 7, 0, 0 } },
	{ 2, { 7, 0, 0 } }
},
	right[8] = {
	{ 1, { 7, 0, 0 } },
	{ 2, { 3, 7, 0 } },
	{ 1, { 3, 0, 0 } },
	{ 2, { 3, 4, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 2, { 6, 7, 0 } }
},
	lower[8] = {
	{ 0, { 0, 0, 0 } },
	{ 2, { 0, 1, 0 } },
	{ 1, { 1, 0, 0 } },
	{ 2, { 1, 5, 0 } },
	{ 1, { 5, 0, 0 } },
	{ 2, { 5, 6, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } }
},
	upleft[8] = {
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 1, { 3, 0, 0 } },
	{ 2, { 1, 3, 0 } },
	{ 1, { 1, 0, 0 } }
},
	upright[8] = {
	{ 2, { 3, 5, 0 } },
	{ 1, { 3, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 1, { 5, 0, 0 } }
},
	lowleft[8] = {
	{ 3, { 7, 0, 1 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 1, { 1, 0, 0 } },
	{ 2, { 1, 7, 0 } },
	{ 1, { 7, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } }
},
	lowright[8] = {
	{ 0, { 0, 0, 0 } },
	{ 1, { 7, 0, 0 } },
	{ 2, { 5, 7, 0 } },
	{ 1, { 5, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } },
	{ 0, { 0, 0, 0 } }
};

#define	cursor(c, r)	tputs(tgoto(CM, c, r), 1, fputchar)

char *tcp;
int fputchar();

static char	flavor[] = {
	'O', '*', '#', '$', '%', '0', '@', '~'
};
static short	xinc[] = {
	1,  1,  1,  0, -1, -1, -1,  0
}, yinc[] = {
	-1,  0,  1,  1,  1,  0, -1, -1
};
static struct	worm {
	int orientation, head;
	short *xpos, *ypos;
} *worm;

main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	extern short ospeed;
	extern char *optarg, *UP;
	register int x, y, h, n;
	register struct worm *w;
	register struct options *op;
	register short *ip;
	register char *term;
	int CO, IN, LI, last, bottom, ch, length, number, trail, Wrap;
	void onsig();
	short **ref;
	char *AL, *BC, *CM, *EI, *HO, *IC, *IM, *IP, *SR;
	char *field, tcb[100], *mp;
	long random();
#ifdef USG
	struct termio sg;
#else
	struct sgttyb sg;
#endif

	length = 16;
	number = 3;
	trail = ' ';
	field = NULL;
	while ((ch = getopt(argc, argv, "fl:n:t")) != EOF)
		switch(ch) {
		case 'f':
			field = "WORM";
			break;
		case 'l':
			if ((length = atoi(optarg)) < 2 || length > 1024) {
				(void)fprintf(stderr,
				    "worms: invalid length (%d - %d).\n",
				     2, 1024);
				exit(1);
			}
			break;
		case 'n':
			if ((number = atoi(optarg)) < 1) {
				(void)fprintf(stderr,
				    "worms: invalid number of worms.\n");
				exit(1);
			}
			break;
		case 't':
			trail = '.';
			break;
		case '?':
		default:
			(void)fprintf(stderr,
			    "usage: worms [-ft] [-length #] [-number #]\n");
			exit(1);
		}

	if (!(term = getenv("TERM"))) {
		(void)fprintf(stderr, "worms: no TERM environment variable.\n");
		exit(1);
	}
	if (!(worm = (struct worm *)malloc((u_int)number *
	    sizeof(struct worm))) || !(mp = malloc((u_int)1024)))
		nomem();
	if (tgetent(mp, term) <= 0) {
		(void)fprintf(stderr, "worms: %s: unknown terminal type.\n",
		    term);
		exit(1);
	}
	tcp = tcb;
	if (!(CM = tgetstr("cm", &tcp))) {
		(void)fprintf(stderr,
		    "worms: terminal incapable of cursor motion.\n");
		exit(1);
	}
	AL = tgetstr("al", &tcp);
	BC = tgetflag("bs") ? "\b" : tgetstr("bc", &tcp);
	if ((CO = tgetnum("co")) <= 0)
		CO = 80;
	last = CO - 1;
	EI = tgetstr("ei", &tcp);
	HO = tgetstr("ho", &tcp);
	IC = tgetstr("ic", &tcp);
	IM = tgetstr("im", &tcp);
	IN = tgetflag("in");
	IP = tgetstr("ip", &tcp);
	if ((LI = tgetnum("li")) <= 0)
		LI = 24;
	bottom = LI - 1;
	SR = tgetstr("sr", &tcp);
	UP = tgetstr("up", &tcp);
#ifdef USG
	ioctl(1, TCGETA, &sg);
	ospeed = sg.c_cflag&CBAUD;
#else
	gtty(1, &sg);
	ospeed = sg.sg_ospeed;
#endif
	Wrap = tgetflag("am");
	if (!(ip = (short *)malloc((u_int)(LI * CO * sizeof(short)))))
		nomem();
	if (!(ref = (short **)malloc((u_int)(LI * sizeof(short *)))))
		nomem();
	for (n = 0; n < LI; ++n) {
		ref[n] = ip;
		ip += CO;
	}
	for (ip = ref[0], n = LI * CO; --n >= 0;)
		*ip++ = 0;
	if (Wrap)
		ref[bottom][last] = 1;
	for (n = number, w = &worm[0]; --n >= 0; w++) {
		w->orientation = w->head = 0;
		if (!(ip = (short *)malloc((u_int)(length * sizeof(short)))))
			nomem();
		w->xpos = ip;
		for (x = length; --x >= 0;)
			*ip++ = -1;
		if (!(ip = (short *)malloc((u_int)(length * sizeof(short)))))
			nomem();
		w->ypos = ip;
		for (y = length; --y >= 0;)
			*ip++ = -1;
	}

	(void)signal(SIGHUP, onsig);
	(void)signal(SIGINT, onsig);
	(void)signal(SIGQUIT, onsig);
	(void)signal(SIGSTOP, onsig);
	(void)signal(SIGTSTP, onsig);
	(void)signal(SIGTERM, onsig);

	tputs(tgetstr("ti", &tcp), 1, fputchar);
	tputs(tgetstr("cl", &tcp), 1, fputchar);
	if (field) {
		register char *p = field;

		for (y = bottom; --y >= 0;) {
			for (x = CO; --x >= 0;) {
				fputchar(*p++);
				if (!*p)
					p = field;
			}
			if (!Wrap)
				fputchar('\n');
			(void)fflush(stdout);
		}
		if (Wrap) {
			if (IM && !IN) {
				for (x = last; --x > 0;) {
					fputchar(*p++);
					if (!*p)
						p = field;
				}
				y = *p++;
				if (!*p)
					p = field;
				fputchar(*p);
				if (BC)
					tputs(BC, 1, fputchar);
				else
					cursor(last - 1, bottom);
				tputs(IM, 1, fputchar);
				if (IC)
					tputs(IC, 1, fputchar);
				fputchar(y);
				if (IP)
					tputs(IP, 1, fputchar);
				tputs(EI, 1, fputchar);
			}
			else if (SR || AL) {
				if (HO)
					tputs(HO, 1, fputchar);
				else
					cursor(0, 0);
				if (SR)
					tputs(SR, 1, fputchar);
				else
					tputs(AL, LI, fputchar);
				for (x = CO; --x >= 0;) {
					fputchar(*p++);
					if (!*p)
						p = field;
				}
			}
			else for (x = last; --x >= 0;) {
				fputchar(*p++);
				if (!*p)
					p = field;
			}
		}
		else for (x = CO; --x >= 0;) {
			fputchar(*p++);
			if (!*p)
				p = field;
		}
	}
	for (;;) {
		(void)fflush(stdout);
		for (n = 0, w = &worm[0]; n < number; n++, w++) {
			if ((x = w->xpos[h = w->head]) < 0) {
				cursor(x = w->xpos[h] = 0,
				     y = w->ypos[h] = bottom);
				fputchar(flavor[n % sizeof(flavor)]);
				ref[y][x]++;
			}
			else
				y = w->ypos[h];
			if (++h == length)
				h = 0;
			if (w->xpos[w->head = h] >= 0) {
				register int x1, y1;

				x1 = w->xpos[h];
				y1 = w->ypos[h];
				if (--ref[y1][x1] == 0) {
					cursor(x1, y1);
					if (trail)
						fputchar(trail);
				}
			}
			op = &(!x ? (!y ? upleft : (y == bottom ? lowleft : left)) : (x == last ? (!y ? upright : (y == bottom ? lowright : right)) : (!y ? upper : (y == bottom ? lower : normal))))[w->orientation];
			switch (op->nopts) {
			case 0:
				(void)fflush(stdout);
				abort();
				return;
			case 1:
				w->orientation = op->opts[0];
				break;
			default:
				w->orientation =
				    op->opts[(int)random() % op->nopts];
			}
			cursor(x += xinc[w->orientation],
			    y += yinc[w->orientation]);
			if (!Wrap || x != last || y != bottom)
				fputchar(flavor[n % sizeof(flavor)]);
			ref[w->ypos[h] = y][w->xpos[h] = x]++;
		}
	}
}

void
onsig()
{
	tputs(tgetstr("cl", &tcp), 1, fputchar);
	tputs(tgetstr("te", &tcp), 1, fputchar);
	exit(0);
}

fputchar(c)
	char c;
{
	putchar(c);
}

nomem()
{
	(void)fprintf(stderr, "worms: not enough memory.\n");
	exit(1);
}
