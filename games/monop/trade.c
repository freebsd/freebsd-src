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
static char sccsid[] = "@(#)trade.c	5.5 (Berkeley) 6/1/90";
#endif /* not lint */

# include	"monop.ext"

struct trd_st {			/* how much to give to other player	*/
	int	trader;			/* trader number		*/
	int	cash;			/* amount of cash 		*/
	int	gojf;			/* # get-out-of-jail-free cards	*/
	OWN	*prop_list;		/* property list		*/
};

typedef	struct trd_st	TRADE;

static char	*list[MAX_PRP+2];

static int	used[MAX_PRP];

static TRADE	trades[2];

trade() {

	reg int	tradee, i;

	trading = TRUE;
	for (i = 0; i < 2; i++) {
		trades[i].cash = 0;
		trades[i].gojf = FALSE;
		trades[i].prop_list = NULL;
	}
over:
	if (num_play == 1) {
		printf("There ain't no-one around to trade WITH!!\n");
		return;
	}
	if (num_play > 2) {
		tradee = getinp("Which player do you wish to trade with? ",
		    name_list);
		if (tradee == num_play)
			return;
		if (tradee == player) {
			printf("You can't trade with yourself!\n");
			goto over;
		}
	}
	else
		tradee = 1 - player;
	get_list(0, player);
	get_list(1, tradee);
	if (getyn("Do you wish a summary? ") == 0)
		summate();
	if (getyn("Is the trade ok? ") == 0)
		do_trade();
}
/*
 *	This routine gets the list of things to be trader for the
 * player, and puts in the structure given.
 */
get_list(struct_no, play_no)
int	struct_no, play_no; {

	reg int		sn, pn;
	reg PLAY	*pp;
	int		numin, prop, num_prp;
	OWN		*op;
	TRADE		*tp;

	for (numin = 0; numin < MAX_PRP; numin++)
		used[numin] = FALSE;
	sn = struct_no, pn = play_no;
	pp = &play[pn];
	tp = &trades[sn];
	tp->trader = pn;
	printf("player %s (%d):\n", pp->name, pn+1);
	if (pp->own_list) {
		numin = set_list(pp->own_list);
		for (num_prp = numin; num_prp; ) {
			prop = getinp("Which property do you wish to trade? ",
			    list);
			if (prop == numin)
				break;
			else if (used[prop])
				printf("You've already allocated that.\n");
			else {
				num_prp--;
				used[prop] = TRUE;
				for (op = pp->own_list; prop--; op = op->next)
					continue;
				add_list(pn, &(tp->prop_list), sqnum(op->sqr));
			}
		}
	}
	if (pp->money > 0) {
		printf("You have $%d.  ", pp->money);
		tp->cash = get_int("How much are you trading? ");
	}
	if (pp->num_gojf > 0) {
once_more:
		printf("You have %d get-out-of-jail-free cards. ",pp->num_gojf);
		tp->gojf = get_int("How many are you trading? ");
		if (tp->gojf > pp->num_gojf) {
			printf("You don't have that many.  Try again.\n");
			goto once_more;
		}
	}
}
/*
 *	This routine sets up the list of tradable property.
 */
set_list(the_list)
reg OWN	*the_list; {

	reg int	i;
	reg OWN	*op;

	i = 0;
	for (op = the_list; op; op = op->next)
		if (!used[i])
			list[i++] = op->sqr->name;
	list[i++] = "done";
	list[i--] = 0;
	return i;
}
/*
 *	This routine summates the trade.
 */
summate() {

	reg bool	some;
	reg int		i;
	reg TRADE	*tp;
	OWN	*op;

	for (i = 0; i < 2; i++) {
		tp = &trades[i];
		some = FALSE;
		printf("Player %s (%d) gives:\n", play[tp->trader].name,
			tp->trader+1);
		if (tp->cash > 0)
			printf("\t$%d\n", tp->cash), some++;
		if (tp->gojf > 0)
			printf("\t%d get-out-of-jail-free card(s)\n", tp->gojf),
			some++;
		if (tp->prop_list) {
			for (op = tp->prop_list; op; op = op->next)
				putchar('\t'), printsq(sqnum(op->sqr), TRUE);
			some++;
		}
		if (!some)
			printf("\t-- Nothing --\n");
	}
}
/*
 *	This routine actually executes the trade.
 */
do_trade() {

	move_em(&trades[0], &trades[1]);
	move_em(&trades[1], &trades[0]);
}
/*
 *	This routine does a switch from one player to another
 */
move_em(from, to)
TRADE	*from, *to; {

	reg PLAY	*pl_fr, *pl_to;
	reg OWN		*op;

	pl_fr = &play[from->trader];
	pl_to = &play[to->trader];

	pl_fr->money -= from->cash;
	pl_to->money += from->cash;
	pl_fr->num_gojf -= from->gojf;
	pl_to->num_gojf += from->gojf;
	for (op = from->prop_list; op; op = op->next) {
		add_list(to->trader, &(pl_to->own_list), sqnum(op->sqr));
		op->sqr->owner = to->trader;
		del_list(from->trader, &(pl_fr->own_list), sqnum(op->sqr));
	}
	set_ownlist(to->trader);
}
/*
 *	This routine lets a player resign
 */
resign() {

	reg int	i, new_own;
	reg OWN	*op;
	SQUARE	*sqp;

	if (cur_p->money <= 0) {
		switch (board[cur_p->loc].type) {
		  case UTIL:
		  case RR:
		  case PRPTY:
			new_own = board[cur_p->loc].owner;
			break;
		  default:		/* Chance, taxes, etc */
			new_own = num_play;
			break;
		}
		if (new_own == num_play)
			printf("You would resign to the bank\n");
		else
			printf("You would resign to %s\n", name_list[new_own]);
	}
	else if (num_play == 1) {
		new_own = num_play;
		printf("You would resign to the bank\n");
	}
	else {
		name_list[num_play] = "bank";
		do {
			new_own = getinp("Who do you wish to resign to? ",
			    name_list);
			if (new_own == player)
				printf("You can't resign to yourself!!\n");
		} while (new_own == player);
		name_list[num_play] = "done";
	}
	if (getyn("Do you really want to resign? ", yn) != 0)
		return;
	if (num_play == 1) {
		printf("Then NOBODY wins (not even YOU!)\n");
		exit(0);
	}
	if (new_own < num_play) {	/* resign to player		*/
		printf("resigning to player\n");
		trades[0].trader = new_own;
		trades[0].cash = trades[0].gojf = 0;
		trades[0].prop_list = NULL;
		trades[1].trader = player;
		trades[1].cash = cur_p->money > 0 ? cur_p->money : 0;
		trades[1].gojf = cur_p->num_gojf;
		trades[1].prop_list = cur_p->own_list;
		do_trade();
	}
	else {				/* resign to bank		*/
		printf("resigning to bank\n");
		for (op = cur_p->own_list; op; op = op->next) {
			sqp = op->sqr;
			sqp->owner = -1;
			sqp->desc->morg = FALSE;
			if (sqp->type == PRPTY) {
				isnot_monop(sqp->desc->mon_desc);
				sqp->desc->houses = 0;
			}
		}
		if (cur_p->num_gojf)
			ret_card(cur_p);
	}
	for (i = player; i < num_play; i++) {
		name_list[i] = name_list[i+1];
		if (i + 1 < num_play)
			cpy_st(&play[i], &play[i+1], sizeof (PLAY));
	}
	name_list[num_play--] = 0;
	for (i = 0; i < N_SQRS; i++)
		if (board[i].owner > player)
			--board[i].owner;
	player = --player < 0 ? num_play - 1 : player;
	next_play();
	if (num_play < 2) {
		printf("\nThen %s WINS!!!!!\n", play[0].name);
		printhold(0);
		printf("That's a grand worth of $%d.\n",
			play[0].money+prop_worth(&play[0]));
		exit(0);
	}
}
