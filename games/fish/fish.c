/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Muffy Barkocy.
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
static char copyright[] =
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)fish.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

#include <sys/types.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pathnames.h"

#define	RANKS		13
#define	HANDSIZE	7
#define	CARDS		4

#define	USER		1
#define	COMPUTER	0
#define	OTHER(a)	(1 - (a))

char *cards[] = {
	"A", "2", "3", "4", "5", "6", "7",
	"8", "9", "10", "J", "Q", "K", NULL,
};
#define	PRC(card)	(void)printf(" %s", cards[card])

int promode;
int asked[RANKS], comphand[RANKS], deck[RANKS];
int userasked[RANKS], userhand[RANKS];

main(argc, argv)
	int argc;
	char **argv;
{
	int ch, move;

	while ((ch = getopt(argc, argv, "p")) != -1)
		switch(ch) {
		case 'p':
			promode = 1;
			break;
		case '?':
		default:
			(void)fprintf(stderr, "usage: fish [-p]\n");
			exit(1);
		}

	srandom(time((time_t *)NULL));
	instructions();
	init();

	if (nrandom(2) == 1) {
		printplayer(COMPUTER);
		(void)printf("get to start.\n");
		goto istart;
	}
	printplayer(USER);
	(void)printf("get to start.\n");

	for (;;) {
		move = usermove();
		if (!comphand[move]) {
			if (gofish(move, USER, userhand))
				continue;
		} else {
			goodmove(USER, move, userhand, comphand);
			continue;
		}

istart:		for (;;) {
			move = compmove();
			if (!userhand[move]) {
				if (!gofish(move, COMPUTER, comphand))
					break;
			} else
				goodmove(COMPUTER, move, comphand, userhand);
		}
	}
	/* NOTREACHED */
}

usermove()
{
	register int n;
	register char **p;
	char buf[256];

	(void)printf("\nYour hand is:");
	printhand(userhand);

	for (;;) {
		(void)printf("You ask me for: ");
		(void)fflush(stdout);
		if (fgets(buf, BUFSIZ, stdin) == NULL)
			exit(0);
		if (buf[0] == '\0')
			continue;
		if (buf[0] == '\n') {
			(void)printf("%d cards in my hand, %d in the pool.\n",
			    countcards(comphand), countcards(deck));
			(void)printf("My books:");
			(void)countbooks(comphand);
			continue;
		}
		buf[strlen(buf) - 1] = '\0';
		if (!strcasecmp(buf, "p") && !promode) {
			promode = 1;
			(void)printf("Entering pro mode.\n");
			continue;
		}
		if (!strcasecmp(buf, "quit"))
			exit(0);
		for (p = cards; *p; ++p)
			if (!strcasecmp(*p, buf))
				break;
		if (!*p) {
			(void)printf("I don't understand!\n");
			continue;
		}
		n = p - cards;
		if (userhand[n]) {
			userasked[n] = 1;
			return(n);
		}
		if (nrandom(3) == 1)
			(void)printf("You don't have any of those!\n");
		else
			(void)printf("You don't have any %s's!\n", cards[n]);
		if (nrandom(4) == 1)
			(void)printf("No cheating!\n");
		(void)printf("Guess again.\n");
	}
	/* NOTREACHED */
}

compmove()
{
	static int lmove;

	if (promode)
		lmove = promove();
	else {
		do {
			lmove = (lmove + 1) % RANKS;
		} while (!comphand[lmove] || comphand[lmove] == CARDS);
	}
	asked[lmove] = 1;

	(void)printf("I ask you for: %s.\n", cards[lmove]);
	return(lmove);
}

promove()
{
	register int i, max;

	for (i = 0; i < RANKS; ++i)
		if (userasked[i] &&
		    comphand[i] > 0 && comphand[i] < CARDS) {
			userasked[i] = 0;
			return(i);
		}
	if (nrandom(3) == 1) {
		for (i = 0;; ++i)
			if (comphand[i] && comphand[i] != CARDS) {
				max = i;
				break;
			}
		while (++i < RANKS)
			if (comphand[i] != CARDS &&
			    comphand[i] > comphand[max])
				max = i;
		return(max);
	}
	if (nrandom(1024) == 0723) {
		for (i = 0; i < RANKS; ++i)
			if (userhand[i] && comphand[i])
				return(i);
	}
	for (;;) {
		for (i = 0; i < RANKS; ++i)
			if (comphand[i] && comphand[i] != CARDS &&
			    !asked[i])
				return(i);
		for (i = 0; i < RANKS; ++i)
			asked[i] = 0;
	}
	/* NOTREACHED */
}

drawcard(player, hand)
	int player;
	int *hand;
{
	int card;

	while (deck[card = nrandom(RANKS)] == 0);
	++hand[card];
	--deck[card];
	if (player == USER || hand[card] == CARDS) {
		printplayer(player);
		(void)printf("drew %s", cards[card]);
		if (hand[card] == CARDS) {
			(void)printf(" and made a book of %s's!\n",
			     cards[card]);
			chkwinner(player, hand);
		} else
			(void)printf(".\n");
	}
	return(card);
}

gofish(askedfor, player, hand)
	int askedfor, player;
	int *hand;
{
	printplayer(OTHER(player));
	(void)printf("say \"GO FISH!\"\n");
	if (askedfor == drawcard(player, hand)) {
		printplayer(player);
		(void)printf("drew the guess!\n");
		printplayer(player);
		(void)printf("get to ask again!\n");
		return(1);
	}
	return(0);
}

goodmove(player, move, hand, opphand)
	int player, move;
	int *hand, *opphand;
{
	printplayer(OTHER(player));
	(void)printf("have %d %s%s.\n",
	    opphand[move], cards[move], opphand[move] == 1 ? "": "'s");

	hand[move] += opphand[move];
	opphand[move] = 0;

	if (hand[move] == CARDS) {
		printplayer(player);
		(void)printf("made a book of %s's!\n", cards[move]);
		chkwinner(player, hand);
	}

	chkwinner(OTHER(player), opphand);

	printplayer(player);
	(void)printf("get another guess!\n");
}

chkwinner(player, hand)
	int player;
	register int *hand;
{
	register int cb, i, ub;

	for (i = 0; i < RANKS; ++i)
		if (hand[i] > 0 && hand[i] < CARDS)
			return;
	printplayer(player);
	(void)printf("don't have any more cards!\n");
	(void)printf("My books:");
	cb = countbooks(comphand);
	(void)printf("Your books:");
	ub = countbooks(userhand);
	(void)printf("\nI have %d, you have %d.\n", cb, ub);
	if (ub > cb) {
		(void)printf("\nYou win!!!\n");
		if (nrandom(1024) == 0723)
			(void)printf("Cheater, cheater, pumpkin eater!\n");
	} else if (cb > ub) {
		(void)printf("\nI win!!!\n");
		if (nrandom(1024) == 0723)
			(void)printf("Hah!  Stupid peasant!\n");
	} else
		(void)printf("\nTie!\n");
	exit(0);
}

printplayer(player)
	int player;
{
	switch (player) {
	case COMPUTER:
		(void)printf("I ");
		break;
	case USER:
		(void)printf("You ");
		break;
	}
}

printhand(hand)
	int *hand;
{
	register int book, i, j;

	for (book = i = 0; i < RANKS; i++)
		if (hand[i] < CARDS)
			for (j = hand[i]; --j >= 0;)
				PRC(i);
		else
			++book;
	if (book) {
		(void)printf(" + Book%s of", book > 1 ? "s" : "");
		for (i = 0; i < RANKS; i++)
			if (hand[i] == CARDS)
				PRC(i);
	}
	(void)putchar('\n');
}

countcards(hand)
	register int *hand;
{
	register int i, count;

	for (count = i = 0; i < RANKS; i++)
		count += *hand++;
	return(count);
}

countbooks(hand)
	int *hand;
{
	int i, count;

	for (count = i = 0; i < RANKS; i++)
		if (hand[i] == CARDS) {
			++count;
			PRC(i);
		}
	if (!count)
		(void)printf(" none");
	(void)putchar('\n');
	return(count);
}

init()
{
	register int i, rank;

	for (i = 0; i < RANKS; ++i)
		deck[i] = CARDS;
	for (i = 0; i < HANDSIZE; ++i) {
		while (!deck[rank = nrandom(RANKS)]);
		++userhand[rank];
		--deck[rank];
	}
	for (i = 0; i < HANDSIZE; ++i) {
		while (!deck[rank = nrandom(RANKS)]);
		++comphand[rank];
		--deck[rank];
	}
}

nrandom(n)
	int n;
{
	long random();

	return((int)random() % n);
}

instructions()
{
	int input;
	char buf[1024];

	(void)printf("Would you like instructions (y or n)? ");
	input = getchar();
	while (getchar() != '\n');
	if (input != 'y')
		return;

	(void)sprintf(buf, "%s %s", _PATH_MORE, _PATH_INSTR);
	(void)system(buf);
	(void)printf("Hit return to continue...\n");
	while ((input = getchar()) != EOF && input != '\n');
}

usage()
{
	(void)fprintf(stderr, "usage: fish [-p]\n");
	exit(1);
}
