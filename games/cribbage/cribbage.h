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

void	 addmsg __P((const char *, ...));
int	 adjust __P((CARD [], CARD));
int	 anymove __P((CARD [], int, int));
int	 anysumto __P((CARD [], int, int, int));
void	 bye __P((void));
int	 cchose __P((CARD [], int, int));
void	 cdiscard __P((BOOLEAN));
int	 chkscr __P((int *, int));
int	 comphand __P((CARD [], char *));
void	 cremove __P((CARD, CARD [], int));
int	 cut __P((BOOLEAN, int));
int	 deal __P((int));
void	 discard __P((BOOLEAN));
void	 do_wait __P((void));
void	 endmsg __P((void));
int	 eq __P((CARD, CARD));
int	 fifteens __P((CARD [], int));
void	 game __P((void));
void	 gamescore __P((void));
char	*getline __P((void));
int	 getuchar __P((void));
int	 incard __P((CARD *));
int	 infrom __P((CARD [], int, char *));
void	 instructions __P((void));
int	 isone __P((CARD, CARD [], int));
void	 makeboard __P((void));
void	 makedeck __P((CARD []));
void	 makeknown __P((CARD [], int));
void	 msg __P((const char *, ...));
int	 msgcard __P((CARD, BOOLEAN));
int	 msgcrd __P((CARD, BOOLEAN, char *, BOOLEAN));
int	 number __P((int, int, char *));
int	 numofval __P((CARD [], int, int));
int	 pairuns __P((CARD [], int));
int	 peg __P((BOOLEAN));
int	 pegscore __P((CARD, CARD [], int, int));
int	 playhand __P((BOOLEAN));
int	 plyrhand __P((CARD [], char *));
void	 prcard __P((WINDOW *, int, int, CARD, BOOLEAN));
void	 prcrib __P((BOOLEAN, BOOLEAN));
void	 prhand __P((CARD [], int, WINDOW *, BOOLEAN));
void	 printcard __P((WINDOW *, int, CARD, BOOLEAN));
void	 prpeg __P((int, int, BOOLEAN));
void	 prtable __P((int));
int	 readchar __P((void));
void	 rint __P((int));
int	 score __P((BOOLEAN));
int	 scorehand __P((CARD [], CARD, int, BOOLEAN, BOOLEAN));
void	 shuffle __P((CARD []));
void	 sorthand __P((CARD [], int));
void	 wait_for __P((int));
