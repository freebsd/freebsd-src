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
static char sccsid[] = "@(#)houses.c	5.5 (Berkeley) 6/1/90";
#endif /* not lint */

# include	"monop.ext"

static char	*names[N_MON+2],
		cur_prop[80];

static MON	*monops[N_MON];

/*
 *	These routines deal with buying and selling houses
 */
buy_houses() {

	reg int num_mon;
	reg MON	*mp;
	reg OWN	*op;
	bool	good,got_morg;
	int	i,p;

over:
	num_mon = 0;
	good = TRUE;
	got_morg = FALSE;
	for (op = cur_p->own_list; op && op->sqr->type != PRPTY; op = op->next)
		continue;
	while (op)
		if (op->sqr->desc->monop) {
			mp = op->sqr->desc->mon_desc;
			names[num_mon] = (monops[num_mon]=mp)->name;
			num_mon++;
			got_morg = good = FALSE;
			for (i = 0; i < mp->num_in; i++) {
				if (op->sqr->desc->morg)
					got_morg++;
				if (op->sqr->desc->houses != 5)
					good++;
				op = op->next;
			}
			if (!good || got_morg)
				--num_mon;
		}
		else
			op = op->next;
	if (num_mon == 0) {
		if (got_morg)
			printf("You can't build on mortgaged monopolies.\n");
		else if (!good)
			printf("You can't build any more.\n");
		else
			printf("But you don't have any monopolies!!\n");
		return;
	}
	if (num_mon == 1)
		buy_h(monops[0]);
	else {
		names[num_mon++] = "done";
		names[num_mon--] = 0;
		if ((p=getinp("Which property do you wish to buy houses for? ", names)) == num_mon)
			return;
		buy_h(monops[p]);
		goto over;
	}
}

buy_h(mnp)
MON	*mnp; {

	reg int	i;
	reg MON	*mp;
	reg int	price;
	shrt	input[3],temp[3];
	int	tot;
	PROP	*pp;

	mp = mnp;
	price = mp->h_cost * 50;
blew_it:
	list_cur(mp);
	printf("Houses will cost $%d\n", price);
	printf("How many houses do you wish to buy for\n");
	for (i = 0; i < mp->num_in; i++) {
		pp = mp->sq[i]->desc;
over:
		if (pp->houses == 5) {
			printf("%s (H):\n", mp->sq[i]->name);
			input[i] = 0;
			temp[i] = 5;
			continue;
		}
		(void)sprintf(cur_prop, "%s (%d): ",
			mp->sq[i]->name, pp->houses);
		input[i] = get_int(cur_prop);
		temp[i] = input[i] + pp->houses;
		if (temp[i] > 5) {
			printf("That's too many.  The most you can buy is %d\n",
				5 - pp->houses);
				goto over;
			}
	}
	if (mp->num_in == 3 && (abs(temp[0] - temp[1]) > 1 ||
	    abs(temp[0] - temp[2]) > 1 || abs(temp[1] - temp[2]) > 1)) {
err:		printf("That makes the spread too wide.  Try again\n");
		goto blew_it;
	}
	else if (mp->num_in == 2 && abs(temp[0] - temp[1]) > 1)
		goto err;
	for (tot = i = 0; i < mp->num_in; i++)
		tot += input[i];
	if (tot) {
		printf("You asked for %d houses for $%d\n", tot, tot * price);
		if (getyn("Is that ok? ", yn) == 0) {
			cur_p->money -= tot * price;
			for (tot = i = 0; i < mp->num_in; i++)
				mp->sq[i]->desc->houses = temp[i];
		}
	}
}

/*
 *	This routine sells houses.
 */
sell_houses() {

	reg int	num_mon;
	reg MON	*mp;
	reg OWN	*op;
	bool	good;
	int	p;

over:
	num_mon = 0;
	good = TRUE;
	for (op = cur_p->own_list; op; op = op->next)
		if (op->sqr->type == PRPTY && op->sqr->desc->monop) {
			mp = op->sqr->desc->mon_desc;
			names[num_mon] = (monops[num_mon]=mp)->name;
			num_mon++;
			good = 0;
			do
				if (!good && op->sqr->desc->houses != 0)
					good++;
			while (op->next && op->sqr->desc->mon_desc == mp
			    && (op=op->next));
			if (!good)
				--num_mon;
		}
	if (num_mon == 0) {
		printf("You don't have any houses to sell!!\n");
		return;
	}
	if (num_mon == 1)
		sell_h(monops[0]);
	else {
		names[num_mon++] = "done";
		names[num_mon--] = 0;
		if ((p=getinp("Which property do you wish to sell houses from? ", names)) == num_mon)
			return;
		sell_h(monops[p]);
		notify();
		goto over;
	}
}

sell_h(mnp)
MON	*mnp; {

	reg int	i;
	reg MON	*mp;
	reg int	price;
	shrt	input[3],temp[3];
	int	tot;
	PROP	*pp;

	mp = mnp;
	price = mp->h_cost * 25;
blew_it:
	printf("Houses will get you $%d apiece\n", price);
	list_cur(mp);
	printf("How many houses do you wish to sell from\n");
	for (i = 0; i < mp->num_in; i++) {
		pp = mp->sq[i]->desc;
over:
		if (pp->houses == 0) {
			printf("%s (0):\n", mp->sq[i]->name);
			input[i] = temp[i] = 0;
			continue;
		}
		if (pp->houses < 5)
			(void)sprintf(cur_prop,"%s (%d): ",
				mp->sq[i]->name,pp->houses);
		else
			(void)sprintf(cur_prop,"%s (H): ",mp->sq[i]->name);
		input[i] = get_int(cur_prop);
		temp[i] = pp->houses - input[i];
		if (temp[i] < 0) {
			printf("That's too many.  The most you can sell is %d\n", pp->houses);
				goto over;
			}
	}
	if (mp->num_in == 3 && (abs(temp[0] - temp[1]) > 1 ||
	    abs(temp[0] - temp[2]) > 1 || abs(temp[1] - temp[2]) > 1)) {
err:		printf("That makes the spread too wide.  Try again\n");
		goto blew_it;
	}
	else if (mp->num_in == 2 && abs(temp[0] - temp[1]) > 1)
		goto err;
	for (tot = i = 0; i < mp->num_in; i++)
		tot += input[i];
	if (tot) {
		printf("You asked to sell %d houses for $%d\n",tot,tot * price);
		if (getyn("Is that ok? ", yn) == 0) {
			cur_p->money += tot * price;
			for (tot = i = 0; i < mp->num_in; i++)
				mp->sq[i]->desc->houses = temp[i];
		}
	}
}

list_cur(mp)
reg MON	*mp; {

	reg int		i;
	reg SQUARE	*sqp;

	for (i = 0; i < mp->num_in; i++) {
		sqp = mp->sq[i];
		if (sqp->desc->houses == 5)
			printf("%s (H) ", sqp->name);
		else
			printf("%s (%d) ", sqp->name, sqp->desc->houses);
	}
	putchar('\n');
}
