/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
static char sccsid[] = "@(#)backgammon.c	5.1 (Berkeley) 4/8/91";
#endif /* not lint */

/*
**	The game of Backgammon
*/

#include	<stdio.h>

#define	WHITE		0
#define	BROWN		1
#define	NIL		(-1)
#define	MAXGMOV		10
#define	MAXIMOVES	1000
#define	RULES		"/usr/games/lib/backrules"

char	level;		/*'b'=beginner, 'i'=intermediate, 'e'=expert*/

int	die1;
int	die2;
int	i;
int	j;
int	l;
int	m;
int	pflg = 1;
int	nobroll = 0;
int	count;
int	imoves;
int	goodmoves[MAXGMOV];
int	probmoves[MAXGMOV];

int	brown[] = {		/* brown position table */
	0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 
	0, 0, 0, 0, 3, 0, 5, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

int	white[] = {		/* white position table */
	0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 
	0, 0, 0, 0, 3, 0, 5, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

int	probability[] = {
	0, 11, 12, 13, 14, 15, 16, 
	06, 05, 04, 03, 02, 01
};

struct	{
	int	pos[4];
	int	mov[4];
} moves[MAXIMOVES];

main()
{
	int	go[5], tvec[2];
	int	k, n, pid, ret, rpid, t;
	char	s[100];

	srand(time(0));
	go[5] = NIL;
	fprintf(stdout, "Instructions? ");
	gets(s);
	if(*s == 'y')
		instructions();
	putchar('\n');
	fprintf(stdout, "Opponent's level: b - beginner,\n");
	fprintf(stdout, "i - intermediate, e - expert? ");
	level='e';
	gets(s);
	if(*s == 'b')
		level = 'b';
	else if(*s == 'i')
		level = 'i';
	putchar('\n');
	fprintf(stdout, "You will play brown.\n\n");
	fprintf(stdout, "Would you like to roll your own dice? ");
	gets(s);
	putchar('\n');
	if(*s == 'y')
		nobroll = 1;
	fprintf(stdout, "Would you like to go first? ");
	gets(s);
	putchar('\n');
	if(*s == 'y')
		goto nowhmove;
whitesmv:
	roll(WHITE);
	fprintf(stdout, "white rolls %d, %d\n", die1, die2);
	fprintf(stdout, "white's move is:");
	if(nextmove(white, brown) == NIL)
		goto nowhmove;
	if(piececount(white, 0, 24) == 0){
		fprintf(stdout, "White wins");
		if(piececount(brown, 0, 6) != 0)
			fprintf(stdout, " with a Backgammon!\n");
		else if (piececount(brown, 0, 24) == 24)
			fprintf(stdout, " with a Gammon.\n");
		else
			fprintf(stdout, ".\n");
		exit(0);
	}
nowhmove:
	if(pflg)
		prtbrd();
	roll(BROWN);
retry:
	fprintf(stdout, "\nYour roll is %d  %d\n", die1, die2);
	fprintf(stdout, "Move? ");
	gets(s);
	switch(*s) {
		case '\0':			/* empty line */
			fprintf(stdout, "Brown's move skipped.\n");
			goto whitesmv;

		case 'b':			/* how many beared off? */
			fprintf(stdout, "Brown:   %d\n", piececount(brown, 0, 24) - 15);
			fprintf(stdout, "White:   %d\n", piececount(white, 0, 24) - 15);
			goto retry;

		case 'p':			/* print board */
			prtbrd();
			goto retry;

		case 's':			/* stop auto printing of board */
			pflg = 0;
			goto retry;

		case 'r':			/* resume auto printing */
			pflg = 1;
			goto retry;

		case 'm':			/* print possible moves */
			pmoves();
			goto retry;

		case 'q':			/* i give up */
			exit(0);

		case '!':			/* escape to Shell */
			if(s[1] != '\0')
				system(s+1);
			else if((pid = fork()) == 0) {
				execl("/bin/sh", "sh", "-", 0);
				fprintf(stderr, "back: cannot exec /bin/sh!\n");
				exit(2);
			}
			while((rpid = wait(&ret)) != pid && rpid != -1)
				;
			goto retry;

		case '?':			/* well, what can i do? */
			fprintf(stdout, "<newline>	skip this move\n");
			fprintf(stdout, "b		number beared off\n");
			fprintf(stdout, "p		print board\n");
			fprintf(stdout, "q		quit\n");
			fprintf(stdout, "r		resume auto print of board\n");
			fprintf(stdout, "s		stop auto print of board\n");
			fprintf(stdout, "!		escape to Shell\n");
			goto retry;
	}
	n = sscanf(s,"%d%d%d%d%d",&go[0],&go[1],&go[2],&go[3],&go[4]);
	if((die1 != die2 && n > 2) || n > 4){
		fprintf(stdout, "Too many moves.\n");
		goto retry;
	}
	go[n] = NIL;
	if(*s=='-'){
		go[0]= -go[0];
		t=die1;
		die1=die2;
		die2=t;
	}
	for(k = 0; k < n; k++){
		if(0 <= go[k] && go[k] <= 24)
			continue;
		else{
			fprintf(stdout, "Move %d illegal.\n", go[k]);
			goto retry;
		}
	}
	if(play(brown, white, go))
		goto retry;
	if(piececount(brown, 0, 24) == 0){
		fprintf(stdout, "Brown wins");
		if(piececount(white, 0, 6) != 0)
			fprintf(stdout, " with a Backgammon.\n");
		else if(piececount(white, 0, 24) == 24)
			fprintf(stdout, " with a gammon.\n");
		else
			fprintf(stdout, ".\n");
		exit(0);
	}
	goto whitesmv;
}

play(player,playee,pos)
int *player,*playee,pos[];
{
	int	k, n, die, ipos;

	for(k=0; k < player[0]; k++){  /*blots on player[0] must be moved first*/
		if(pos[k] == NIL)
			break;
		if(pos[k] != 0){
			fprintf(stdout, "Stone on bar must be moved first.\n");
			return(NIL);
		}
	}
	for(k = 0; (ipos=pos[k]) != NIL; k++){
		die = k?die2:die1;
		n = 25-ipos-die;
		if(player[ipos] == 0)
			goto badmove;
		if(n > 0 && playee[n] >= 2)
			goto badmove;
		if(n <= 0){
			if(piececount(player,0,18) != 0)
				goto badmove;
			if((ipos+die) != 25 && piececount(player,19,24-die)!=0)
				goto badmove;
		}
		player[ipos]--;
		player[ipos+die]++;
	}
	for(k = 0; pos[k] != NIL; k++){
		die = k?die2:die1;
		n = 25-pos[k]-die;
		if(n>0 && playee[n]==1){
			playee[n]=0;
			playee[0]++;
		}
	}
	return(0);

badmove:
	fprintf(stdout, "Move %d illegal.\n", ipos);
	while(k--){
		die=k?die2:die1;
		player[pos[k]]++;
		player[pos[k]+die]--;
	}
	return(NIL);
}
nextmove(player,playee)
int *player,*playee;
{
	int	k;

	imoves=0;
	movegen(player,playee);
	if(die1!=die2){
		k=die1;
		die1=die2;
		die2=k;
		movegen(player,playee);
	}
	if(imoves==0){
		fprintf(stdout, "no move possible.\n");
		return(NIL);
	}
	k=strategy(player,playee);		/*select kth possible move*/
	prtmov(k);
	update(player,playee,k);
	return(0);
}
prtmov(k)
int k;
{
	int	n;

	if(k == NIL)
		fprintf(stdout, "No move possible\n");
	else for(n = 0; n < 4; n++){
		if(moves[k].pos[n] == NIL)
			break;
		fprintf(stdout, "    %d, %d",25-moves[k].pos[n],moves[k].mov[n]);
	}
	fprintf(stdout, "\n");
}
update(player,playee,k)
int *player,*playee,k;
{
	int	n,t;

	for(n = 0; n < 4; n++){
		if(moves[k].pos[n] == NIL)
			break;
		player[moves[k].pos[n]]--;
		player[moves[k].pos[n]+moves[k].mov[n]]++;
		t=25-moves[k].pos[n]-moves[k].mov[n];
		if(t>0 && playee[t]==1){
			playee[0]++;
			playee[t]--;
		}
	}
}
piececount(player,startrow,endrow)
int *player,startrow,endrow;
{
	int	sum;

	sum=0;
	while(startrow <= endrow)
		sum += player[startrow++];
	return(sum);
}
pmoves()
{
	int	i1, i2;

	fprintf(stdout, "Possible moves are:\n");
	for(i1 = 0; i1 < imoves; i1++){
		fprintf(stdout, "\n%d",i1);
		 for (i2 = 0; i2<4; i2++){
			if(moves[i1].pos[i2] == NIL)
				break;
			fprintf(stdout, "%d, %d",moves[i1].pos[i2],moves[i1].mov[i2]);
		}
	}
	fprintf(stdout, "\n");
}

roll(who)
{
	register n;
	char	 s[10];

	if(who == BROWN && nobroll) {
		fprintf(stdout, "Roll? ");
		gets(s);
		n = sscanf(s, "%d%d", &die1, &die2);
		if(n != 2 || die1 < 1 || die1 > 6 || die2 < 1 || die2 > 6)
			fprintf(stdout, "Illegal - I'll do it!\n");
		else
			return;
	}
	die1 = ((rand()>>8) % 6) + 1;
	die2 = ((rand()>>8) % 6) + 1;
}

movegen(mover,movee)
int *mover,*movee;
{
	int	k;

	for(i = 0; i <= 24; i++){
		count = 0;
		if(mover[i] == 0)
			continue;
		if((k=25-i-die1) > 0 && movee[k] >= 2)
			if(mover[0] > 0)
				break;
		else
			continue;
		if(k <= 0){
			if(piececount(mover, 0, 18) != 0)
				break;
			if((i+die1) != 25 && piececount(mover,19,i-1) != 0)
				break;
		}
		mover[i]--;
		mover[i+die1]++;
		count = 1;
		for(j = 0; j <= 24; j++){
			if(mover[j]==0)
				continue;
			if((k=25-j-die2) > 0 && movee[k] >= 2)
				if(mover[0] > 0)
					break;
			else
				continue;
			if(k <= 0){
				if(piececount(mover,0,18) != 0)
					break;
				if((j+die2) != 25 && piececount(mover,19,j-1) != 0)
					break;
			}
			mover[j]--;
			mover[j+die2]++;
			count = 2;
			if(die1 != die2){
				moverecord(mover);
				if(mover[0] > 0)
					break;
				else
					continue;
			}
			for(l = 0; l <= 24; l++){
				if(mover[l] == 0)
					continue;
				if((k=25-l-die1) > 0 && movee[k] >= 2)
					if(mover[0] > 0)
						break;
				else
					continue;
				if(k <= 0){
					if(piececount(mover, 0, 18) != 0)
						break;
					if((l+die2) != 25 && piececount(mover,19,l-1) != 0)
						break;
				}
				mover[l]--;
				mover[l+die1]++;
				count=3;
				for(m=0;m<=24;m++){
					if(mover[m]==0)
						continue;
					if((k=25-m-die1) >= 0 && movee[k] >= 2)
						if(mover[0] > 0)
							break;
					else
						continue;
					if(k <= 0){
						if(piececount(mover,0,18) != 0)
							break;
						if((m+die2) != 25 && piececount(mover,19,m-1) != 0)
							break;
					}
					count=4;
					moverecord(mover);
					if(mover[0] > 0)
						break;
				}
				if(count == 3)
					moverecord(mover);
				else{
					mover[l]++;
					mover[l+die1]--;
				}
				if(mover[0] > 0)
					break;
			}
			if(count == 2)
				moverecord(mover);
			else{
				mover[j]++;
				mover[j+die1]--;
			}
			if(mover[0] > 0)
				break;
		}
		if(count == 1)
			moverecord(mover);
		else{
			mover[i]++;
			mover[i+die1]--;
		}
		if(mover[0] > 0)
			break;
	}
}
moverecord(mover)
int *mover;
{
	int	t;

	if(imoves < MAXIMOVES) {
		for(t = 0; t <= 3; t++)
			moves[imoves].pos[t] = NIL;
		switch(count) {
		case 4:
			moves[imoves].pos[3]=m;
			moves[imoves].mov[3]=die1;

		case 3:
			moves[imoves].pos[2]=l;
			moves[imoves].mov[2]=die1;

		case 2:
			moves[imoves].pos[1]=j;
			moves[imoves].mov[1]=die2;

		case 1:
			moves[imoves].pos[0]=i;
			moves[imoves].mov[0]=die1;
			imoves++;
		}
	}
	switch(count) {
	case 4:
		break;

	case 3:
		mover[l]++;
		mover[l+die1]--;
		break;

	case 2:
		mover[j]++;
		mover[j+die2]--;
		break;

	case 1:
		mover[i]++;
		mover[i+die1]--;
	}
}

strategy(player,playee)
int *player,*playee;
{
	int	k, n, nn, bestval, moveval, prob;

	n = 0;
	if(imoves == 0)
		return(NIL);
	goodmoves[0] = NIL;
	bestval = -32000;
	for(k = 0; k < imoves; k++){
		if((moveval=eval(player,playee,k,&prob)) < bestval)
			continue;
		if(moveval > bestval){
			bestval = moveval;
			n = 0;
		}
		if(n<MAXGMOV){
			goodmoves[n]=k;
			probmoves[n++]=prob;
		}
	}
	if(level=='e' && n>1){
		nn=n;
		n=0;
		prob=32000;
		for(k = 0; k < nn; k++){
			if((moveval=probmoves[k]) > prob)
				continue;
			if(moveval<prob){
				prob=moveval;
				n=0;
			}
			goodmoves[n]=goodmoves[k];
			probmoves[n++]=probmoves[k];
		}
	}
	return(goodmoves[(rand()>>4)%n]);
}

eval(player,playee,k,prob)
int *player,*playee,k,*prob;
{
	int	newtry[31], newother[31], *r, *q, *p, n, sum, first;
	int	ii, lastwhite, lastbrown;

	*prob = sum = 0;
	r = player+25;
	p = newtry;
	q = newother;
	while(player<r){
		*p++= *player++;
		*q++= *playee++;
	}
	q=newtry+31;
	for(p = newtry+25; p < q; p++)		/* zero out spaces for hit pieces */
		*p = 0;
	for(n = 0; n < 4; n++){
		if(moves[k].pos[n] == NIL)
			break;
		newtry[moves[k].pos[n]]--;
		newtry[ii=moves[k].pos[n]+moves[k].mov[n]]++;
		if(ii<25 && newother[25-ii]==1){
			newother[25-ii]=0;
			newother[0]++;
			if(ii<=15 && level=='e')		/* hit if near other's home */
				sum++;
		}
	}
	for(lastbrown = 0; newother[lastbrown] == 0; lastbrown++);
		;
	for(lastwhite = 0; newtry[lastwhite] == 0; lastwhite++)
		;
	lastwhite = 25-lastwhite;
	if(lastwhite<=6 && lastwhite<lastbrown)
		sum=1000;
									/* experts running game. */
									/* first priority is to */
									/* get all pieces into */
									/* white's home */
	if(lastwhite<lastbrown && level=='e' && lastwhite>6) {
		for(sum = 1000; lastwhite > 6; lastwhite--)
			sum = sum-lastwhite*newtry[25-lastwhite];
	}
	for(first = 0; first < 25; first++)
		if(newother[first] != 0)		/*find other's first piece*/
			break;
	q = newtry+25;
	for(p = newtry+1; p < q;)			/* blocked points are good */
		if(*p++ > 1)
			sum++;
	if(first > 5) {					/* only stress removing pieces if */
							/* homeboard cannot be hit */
		q = newtry+31;
		p=newtry+25;
		for(n = 6; p < q; n--)
			sum += *p++ * n;			/*remove pieces, but just barely*/
	}
	if(level != 'b'){
		r = newtry+25-first;	/*singles past this point can't be hit*/
		for(p = newtry+7; p < r; )
			if(*p++ == 1)		/*singles are bad after 1st 6 points if they can be hit*/
				sum--;
		q = newtry+3;
		for(p = newtry; p < q; )	   /*bad to be on 1st three points*/
			sum -= *p++;
	}

	for(n = 1; n <= 4; n++)
		*prob += n*getprob(newtry,newother,6*n-5,6*n);
	return(sum);
}
instructions()
{
	register fd, r;
	char	 buf[BUFSIZ];

	if((fd = open(RULES, 0)) < 0) {
		fprintf(stderr, "back: cannot open %s\n", RULES);
		return;
	}
	while(r = read(fd, buf, BUFSIZ))
		write(1, buf, r);
}

getprob(player,playee,start,finish)
int *player,*playee,start,finish;
{			/*returns the probability (times 102) that any
			  pieces belonging to 'player' and lying between
			  his points 'start' and 'finish' will be hit
			  by a piece belonging to playee
			*/
	int	k, n, sum;

	sum = 0;
	for(; start <= finish; start++){
		if(player[start] == 1){
			for(k = 1; k <= 12; k++){
				if((n=25-start-k) < 0)
					break;
				if(playee[n] != 0)
					sum += probability[k];
			}
		}
	}
	return(sum);
}
prtbrd()
{
	int	k;
	static char undersc[]="______________________________________________________";

	fprintf(stdout, "White's Home\n%s\r",undersc);
	for(k = 1; k <= 6; k++)
		fprintf(stdout, "%4d",k);
	fprintf(stdout, "    ");
	for(k = 7; k <= 12; k++)
		fprintf(stdout, "%4d",k); 
	putchar('\n');
	numline(brown, white, 1, 6);
	fprintf(stdout, "    ");
	numline(brown, white, 7, 12);
	putchar('\n');
	colorline(brown, 'B', white, 'W', 1, 6);
	fprintf(stdout, "    ");
	colorline(brown, 'B', white, 'W', 7, 12);
	putchar('\n');
	if(white[0] != 0)
		fprintf(stdout, "%28dW\n",white[0]);
	else
		putchar('\n');
	if(brown[0] != 0)
		fprintf(stdout, "%28dB\n", brown[0]);
	else
		putchar('\n');
	colorline(white, 'W', brown, 'B', 1, 6);
	fprintf(stdout, "    ");
	colorline(white, 'W', brown, 'B', 7, 12);
	fprintf(stdout, "\n%s\r",undersc);
	numline(white, brown, 1, 6);
	fprintf(stdout, "    ");
	numline(white, brown, 7, 12);
	putchar('\n');
	for(k = 24; k >= 19; k--)
		fprintf(stdout, "%4d",k);
	fprintf(stdout, "    ");
	for(k = 18; k >= 13; k--)
		fprintf(stdout, "%4d",k);
	fprintf(stdout, "\nBrown's Home\n\n\n\n\n");
}
numline(upcol,downcol,start,fin)
int *upcol,*downcol,start,fin;
{
	int	k, n;

	for(k = start; k <= fin; k++){
		if((n = upcol[k]) != 0 || (n = downcol[25-k]) != 0)
			fprintf(stdout, "%4d", n);
		else
			fprintf(stdout, "    ");
	}
}
colorline(upcol,c1,downcol,c2,start,fin)
int *upcol,*downcol,start,fin;
char c1,c2;
{
	int	k;
	char 	c;

	for(k = start; k <= fin; k++){
		c = ' ';
		if(upcol[k] != 0)
			c = c1;
		if(downcol[25-k] != 0)
			c = c2;
		fprintf(stdout, "   %c",c);
	}
}
