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
 *
 *	@(#)cribbage.h	8.1 (Berkeley) 5/31/93
 */

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

extern  CARD		deck[ CARDS ];		/* a deck */
extern  CARD		phand[ FULLHAND ];	/* player's hand */
extern  CARD		chand[ FULLHAND ];	/* computer's hand */
extern  CARD		crib[ CINHAND ];	/* the crib */
extern  CARD		turnover;		/* the starter */

extern  CARD		known[ CARDS ];		/* cards we have seen */
extern  int		knownum;		/* # of cards we know */

extern  int		pscore;			/* player's score */
extern  int		cscore;			/* comp's score */
extern  int		glimit;			/* points to win game */

extern  int		pgames;			/* player's games won */
extern  int		cgames;			/* comp's games won */
extern  int		gamecount;		/* # games played */
extern	int		Lastscore[2];		/* previous score for each */

extern  BOOLEAN		iwon;			/* if comp won last */
extern  BOOLEAN		explain;		/* player mistakes explained */
extern  BOOLEAN		rflag;			/* if all cuts random */
extern  BOOLEAN		quiet;			/* if suppress random mess */
extern	BOOLEAN		playing;		/* currently playing game */

extern  char		expl[];			/* string for explanation */

void	 addmsg(const char *, ...);
int	 adjust(CARD [], CARD);
int	 anymove(CARD [], int, int);
int	 anysumto(CARD [], int, int, int);
void	 bye(void);
int	 cchose(CARD [], int, int);
void	 cdiscard(BOOLEAN);
int	 chkscr(int *, int);
int	 comphand(CARD [], char *);
void	 cremove(CARD, CARD [], int);
int	 cut(BOOLEAN, int);
int	 deal(int);
void	 discard(BOOLEAN);
void	 do_wait(void);
void	 endmsg(void);
int	 eq(CARD, CARD);
int	 fifteens(CARD [], int);
void	 game(void);
void	 gamescore(void);
char	*getline(void);
int	 getuchar(void);
int	 incard(CARD *);
int	 infrom(CARD [], int, char *);
void	 instructions(void);
int	 isone(CARD, CARD [], int);
void	 makeboard(void);
void	 makedeck(CARD []);
void	 makeknown(CARD [], int);
void	 msg(const char *, ...);
int	 msgcard(CARD, BOOLEAN);
int	 msgcrd(CARD, BOOLEAN, char *, BOOLEAN);
int	 number(int, int, char *);
int	 numofval(CARD [], int, int);
int	 pairuns(CARD [], int);
int	 peg(BOOLEAN);
int	 pegscore(CARD, CARD [], int, int);
int	 playhand(BOOLEAN);
int	 plyrhand(CARD [], char *);
void	 prcard(WINDOW *, int, int, CARD, BOOLEAN);
void	 prcrib(BOOLEAN, BOOLEAN);
void	 prhand(CARD [], int, WINDOW *, BOOLEAN);
void	 printcard(WINDOW *, int, CARD, BOOLEAN);
void	 prpeg(int, int, BOOLEAN);
void	 prtable(int);
int	 readchar(void);
void	 rint(int);
int	 score(BOOLEAN);
int	 scorehand(CARD [], CARD, int, BOOLEAN, BOOLEAN);
void	 shuffle(CARD []);
void	 sorthand(CARD [], int);
void	 wait_for(int);
