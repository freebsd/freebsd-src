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
static char sccsid[] = "@(#)misc.c	5.5 (Berkeley) 2/28/91";
#endif /* not lint */

# include	"monop.ext"
# include	<ctype.h>
# include	<signal.h>

# define	execsh(sh)	execl(sh, shell_name[roll(1, num_names)-1], 0)

static char	*shell_def	= "/bin/csh",
		*shell_name[]	= {
			".Hi Mom!",
			".Kick Me",
			".I'm really the next process down",
			".Hi Kids!",
			".This space for rent",
			".Singin' in the rain....",
			".I am but a Cog in the Wheel of Life",
			".Look out!!! Behind you!!!!!",
			".Looking for a good time, sailor?",
			".I don't get NO respect...",
			".Augghh!  You peeked!"
		};

static int	num_names	= sizeof shell_name / sizeof (char *);;

char	*shell_in();

/*
 *	This routine executes a truncated set of commands until a
 * "yes or "no" answer is gotten.
 */
getyn(prompt)
reg char	*prompt; {

	reg int	com;

	for (;;)
		if ((com=getinp(prompt, yn)) < 2)
			return com;
		else
			(*func[com-2])();
}
/*
 *	This routine tells the player if he's out of money.
 */
notify() {

	if (cur_p->money < 0)
		printf("That leaves you $%d in debt\n", -cur_p->money);
	else if (cur_p->money == 0)
		printf("that leaves you broke\n");
	else if (fixing && !told_em && cur_p->money > 0) {
		printf("-- You are now Solvent ---\n");
		told_em = TRUE;
	}
}
/*
 *	This routine switches to the next player
 */
next_play() {

	player = ++player % num_play;
	cur_p = &play[player];
	num_doub = 0;
}
/*
 *	This routine gets an integer from the keyboard after the
 * given prompt.
 */
get_int(prompt)
reg char	*prompt; {

	reg int		num;
	reg char	*sp;
	char		buf[257];

	for (;;) {
inter:
		printf(prompt);
		num = 0;
		for (sp = buf; (*sp=getchar()) != '\n'; sp++)
			if (*sp == -1)	/* check for interrupted system call */
				goto inter;
		if (sp == buf)
			continue;
		for (sp = buf; isspace(*sp); sp++)
			continue;
		for (; isdigit(*sp); sp++)
			num = num * 10 + *sp - '0';
		if (*sp == '\n')
			return num;
		else
			printf("I can't understand that\n");
	}
}
/*
 *	This routine sets the monopoly flag from the list given.
 */
set_ownlist(pl)
int	pl; {

	reg int	num;		/* general counter		*/
	reg MON	*orig;		/* remember starting monop ptr	*/
	reg OWN	*op;		/* current owned prop		*/
	OWN	*orig_op;		/* origianl prop before loop	*/

	op = play[pl].own_list;
#ifdef DEBUG
	printf("op [%d] = play[pl [%d] ].own_list;\n", op, pl);
#endif
	while (op) {
#ifdef DEBUG
		printf("op->sqr->type = %d\n", op->sqr->type);
#endif
		switch (op->sqr->type) {
		  case UTIL:
#ifdef DEBUG
			printf("  case UTIL:\n");
#endif
			for (num = 0; op && op->sqr->type == UTIL; op = op->next)
				num++;
			play[pl].num_util = num;
#ifdef DEBUG
			printf("play[pl].num_util = num [%d];\n", num);
#endif
			break;
		  case RR:
#ifdef DEBUG
			printf("  case RR:\n");
#endif
			for (num = 0; op && op->sqr->type == RR; op = op->next) {
#ifdef DEBUG
				printf("iter: %d\n", num);
				printf("op = %d, op->sqr = %d, op->sqr->type = %d\n", op, op->sqr, op->sqr->type);
#endif
				num++;
			}
			play[pl].num_rr = num;
#ifdef DEBUG
			printf("play[pl].num_rr = num [%d];\n", num);
#endif
			break;
		  case PRPTY:
#ifdef DEBUG
			printf("  case PRPTY:\n");
#endif
			orig = op->sqr->desc->mon_desc;
			orig_op = op;
			num = 0;
			while (op && op->sqr->desc->mon_desc == orig) {
#ifdef DEBUG
				printf("iter: %d\n", num);
#endif
				num++;
#ifdef DEBUG
				printf("op = op->next ");
#endif
				op = op->next;
#ifdef DEBUG
				printf("[%d];\n", op);
#endif
			}
#ifdef DEBUG
			printf("num = %d\n");
#endif
			if (orig == 0) {
				printf("panic:  bad monopoly descriptor: orig = %d\n", orig);
				printf("player # %d\n", pl+1);
				printhold(pl);
				printf("orig_op = %d\n", orig_op);
				printf("orig_op->sqr->type = %d (PRPTY)\n", op->sqr->type);
				printf("orig_op->next = %d\n", op->next);
				printf("orig_op->sqr->desc = %d\n", op->sqr->desc);
				printf("op = %d\n", op);
				printf("op->sqr->type = %d (PRPTY)\n", op->sqr->type);
				printf("op->next = %d\n", op->next);
				printf("op->sqr->desc = %d\n", op->sqr->desc);
				printf("num = %d\n", num);
			}
#ifdef DEBUG
			printf("orig->num_in = %d\n", orig->num_in);
#endif
			if (num == orig->num_in)
				is_monop(orig, pl);
			else
				isnot_monop(orig);
			break;
		}
	}
}
/*
 *	This routine sets things up as if it is a new monopoly
 */
is_monop(mp, pl)
reg MON	*mp;
int	pl; {

	reg char	*sp;
	reg int		i;

	mp->owner = pl;
	mp->num_own = mp->num_in;
	for (i = 0; i < mp->num_in; i++)
		mp->sq[i]->desc->monop = TRUE;
	mp->name = mp->mon_n;
}
/*
 *	This routine sets things up as if it is no longer a monopoly
 */
isnot_monop(mp)
reg MON	*mp; {

	reg char	*sp;
	reg int		i;

	mp->owner = -1;
	for (i = 0; i < mp->num_in; i++)
		mp->sq[i]->desc->monop = FALSE;
	mp->name = mp->not_m;
}
/*
 *	This routine gives a list of the current player's routine
 */
list() {

	printhold(player);
}
/*
 *	This routine gives a list of a given players holdings
 */
list_all() {

	reg int	pl;

	while ((pl=getinp("Whose holdings do you want to see? ", name_list)) < num_play)
		printhold(pl);
}
/*
 *	This routine gives the players a chance before it exits.
 */
void
quit() {

	putchar('\n');
	if (getyn("Do you all really want to quit? ", yn) == 0)
		exit(0);
	signal(SIGINT, quit);
}
/*
 *	This routine copies one structure to another
 */
cpy_st(s1, s2, size)
reg int	*s1, *s2, size; {

	size /= 2;
	while (size--)
		*s1++ = *s2++;
}
/*
 *	This routine forks off a shell.  It uses the users login shell
 */
shell_out() {

	static char	*shell = NULL;

	printline();
	if (shell == NULL)
		shell = shell_in();
	fflush(stdout);
	if (!fork()) {
		signal(SIGINT, SIG_DFL);
		execsh(shell);
	}
	ignoresigs();
	wait();
	resetsigs();
	putchar('\n');
	printline();
}
/*
 *	This routine looks up the users login shell
 */
# include	<sys/types.h>
# include	<pwd.h>

char		*getenv();

char *
shell_in() {

	reg struct passwd	*pp;
	reg char		*sp;

	if ((sp = getenv("SHELL")) == NULL) {
		pp = getpwuid(getuid());
		if (pp->pw_shell[0] != '\0')
			return pp->pw_shell;
		else
			return shell_def;
		/*return (*(pp->pw_shell) != '\0' ? pp->pw_shell : shell_def);*/
	}
	return sp;
}
/*
 *	This routine sets things up to ignore all the signals.
 */
ignoresigs() {

	reg int	i;

	for (i = 0; i < NSIG; i++)
		signal(i, SIG_IGN);
}
/*
 *	This routine sets up things as they were before.
 */
resetsigs() {

	reg int	i;

	for (i = 0; i < NSIG; i++)
		signal(i, SIG_DFL);
	signal(SIGINT, quit);
}
