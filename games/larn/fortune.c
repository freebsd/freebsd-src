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
static char sccsid[] = "@(#)fortune.c	5.5 (Berkeley) 6/10/91";
#endif /* not lint */

/* fortune.c		 Larn is copyrighted 1986 by Noah Morgan. */

/*
 * function to return a random fortune from the fortune file
 */

char *flines[] = {
	"gem value = gem * 2 ^ perfection",
	"sitting down can have unexpected results",
	"don't pry into the affairs of others",
	"drinking can be hazardous to your health",
	"beware of the gusher!",
	"some monsters are greedy",
	"nymphs have light fingers",
	"try kissing a disenchantress!",
	"hammers and brains don't mix",
	"what does a potion of cure dianthroritis taste like?",
	"hit point gain/loss when raising a level depends on constitution",
	"healing a mighty wizard can be exhilarating",
	"be sure to pay your taxes",
	"are Vampires afraid of something?",
	"some dragons can fly",
	"dos thou strive for perfection?",
	"patience is a virtue, unless your daughter dies",
	"what does the Eye of Larn see in its guardian?",
	"a level 25 player casts like crazy!",
	"energy rings affect spell regeneration",
	"difficulty affects regeneration",
	"control of the pesty spirits is most helpful",
	"don't fall into a bottomless pit",
	"dexterity allows you to carry more",
	"you can get 2 points of WC for the price of one",
	"never enter the dungeon naked!  the monsters will laugh at you!",
	"did someone put itching powder in your armor?",
	"you klutz!",
	"avoid opening doors.  you never know whats on the other side.",
	"infinite regeneration ---> temptation",
	"the greatest weapon in the game has not the highest Weapon Class",
	"you can't buy the most powerful scroll",
	"identify things before you use them",
	"there's more than one way through a wall"
};

#define NFORTUNES	34

char *
fortune()
{
	return (flines[random() % NFORTUNES]);
}
