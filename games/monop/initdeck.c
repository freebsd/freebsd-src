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
static char sccsid[] = "@(#)initdeck.c	5.5 (Berkeley) 6/1/90";
#endif /* not lint */

# include	<stdio.h>
# include	"deck.h"

/*
 *	This program initializes the card files for monopoly.
 * It reads in a data file with Com. Chest cards, followed by
 * the Chance card.  The two are seperated by a line of "%-".
 * All other cards are seperated by lines of "%%".  In the front
 * of the file is the data for the decks in the same order.
 * This includes the seek pointer for the start of each card.
 * All cards start with their execution code, followed by the
 * string to print, terminated with a null byte.
 */

# define	TRUE	1
# define	FALSE	0

# define	bool	char
# define	reg	register

char	*infile		= "cards.inp",		/* input file		*/
	*outfile	= "cards.pck";		/* "packed" file	*/

extern long	ftell();
extern char *calloc();

DECK	deck[2];

FILE	*inf, *outf;

main(ac, av)
int	ac;
char	*av[]; {

	getargs(ac, av);
	if ((inf = fopen(infile, "r")) == NULL) {
		perror(infile);
		exit(1);
	}
	count();
	/*
	 * allocate space for pointers.
	 */
	CC_D.offsets = (long *)calloc(CC_D.num_cards + 1, sizeof (long));
	CH_D.offsets = (long *)calloc(CH_D.num_cards + 1, sizeof (long));
	fseek(inf, 0L, 0);
	if ((outf = fopen(outfile, "w")) == NULL) {
		perror(outfile);
		exit(0);
	}

	fwrite(deck, sizeof (DECK), 2, outf);
	fwrite(CC_D.offsets, sizeof (long), CC_D.num_cards, outf);
	fwrite(CH_D.offsets, sizeof (long), CH_D.num_cards, outf);
	putem();

	fclose(inf);
	fseek(outf, 0, 0L);
	fwrite(deck, sizeof (DECK), 2, outf);
	fwrite(CC_D.offsets, sizeof (long), CC_D.num_cards, outf);
	fwrite(CH_D.offsets, sizeof (long), CH_D.num_cards, outf);
	fclose(outf);
	printf("There were %d com. chest and %d chance cards\n", CC_D.num_cards, CH_D.num_cards);
	exit(0);
}

getargs(ac, av)
int	ac;
char	*av[]; {

	if (ac > 1)
		infile = av[1];
	if (ac > 2)
		outfile = av[2];
}

/*
 * count the cards
 */
count() {

	reg bool	newline;
	reg DECK	*in_deck;
	reg char	c;

	newline = TRUE;
	in_deck = &CC_D;
	while ((c=getc(inf)) != EOF)
		if (newline && c == '%') {
			newline = FALSE;
			in_deck->num_cards++;
			if (getc(inf) == '-')
				in_deck = &CH_D;
		}
		else
			newline = (c == '\n');
	in_deck->num_cards++;
}
/*
 *	put strings in the file
 */
putem() {

	reg bool	newline;
	reg DECK	*in_deck;
	reg char	c;
	reg int		num;

	in_deck = &CC_D;
	CC_D.num_cards = 1;
	CH_D.num_cards = 0;
	CC_D.offsets[0] = ftell(outf);
	putc(getc(inf), outf);
	putc(getc(inf), outf);
	for (num = 0; (c=getc(inf)) != '\n'; )
		num = num * 10 + (c - '0');
	putw(num, outf);
	newline = FALSE;
	while ((c=getc(inf)) != EOF)
		if (newline && c == '%') {
			putc('\0', outf);
			newline = FALSE;
			if (getc(inf) == '-')
				in_deck = &CH_D;
			while (getc(inf) != '\n')
				continue;
			in_deck->offsets[in_deck->num_cards++] = ftell(outf);
			if ((c=getc(inf)) == EOF)
				break;
			putc(c, outf);
			putc(c = getc(inf), outf);
			for (num = 0; (c=getc(inf)) != EOF && c != '\n'; )
				num = num * 10 + (c - '0');
			putw(num, outf);
		}
		else {
			putc(c, outf);
			newline = (c == '\n');
		}
	putc('\0', outf);
}
