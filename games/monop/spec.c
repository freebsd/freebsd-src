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
static char sccsid[] = "@(#)spec.c	5.3 (Berkeley) 6/1/90";
#endif /* not lint */

# include	"monop.ext"

static char	*perc[]	= {
	"10%", "ten percent", "%", "$200", "200", 0
	};

inc_tax() {			/* collect income tax			*/

	reg int	worth, com_num;

	com_num = getinp("Do you wish to lose 10%% of your total worth or $200? ", perc);
	worth = cur_p->money + prop_worth(cur_p);
	printf("You were worth $%d", worth);
	worth /= 10;
	if (com_num > 2) {
		if (worth < 200)
			printf(".  Good try, but not quite.\n");
		else if (worth > 200)
			lucky(".\nGood guess.  ");
		cur_p->money -= 200;
	}
	else {
		printf(", so you pay $%d", worth);
		if (worth > 200)
			printf("  OUCH!!!!.\n");
		else if (worth < 200)
			lucky("\nGood guess.  ");
		cur_p->money -= worth;
	}
	if (worth == 200)
		lucky("\nIt makes no difference!  ");
}
goto_jail() {			/* move player to jail			*/

	cur_p->loc = JAIL;
}
lux_tax() {			/* landing on luxury tax		*/

	printf("You lose $75\n");
	cur_p->money -= 75;
}
cc() {				/* draw community chest card		*/

	get_card(&CC_D);
}
chance() {			/* draw chance card			*/

	get_card(&CH_D);
}
