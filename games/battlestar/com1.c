/*
 * Copyright (c) 1983, 1993
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
 */

#ifndef lint
static char sccsid[] = "@(#)com1.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

#include "externs.h"

move(thataway, token)
int thataway, token;
{
	wordnumber++;
	if ((!notes[CANTMOVE] && !notes[LAUNCHED]) || testbit(location[position].objects, LAND) || fuel > 0 && notes[LAUNCHED])
		if (thataway) {
			position = thataway;
			newway(token);
			gtime++;
		}
		else {
			puts("You can't go this way.");
			newway(token);
			whichway(location[position]);
			return(0);
		}
	else if (notes[CANTMOVE] && !notes[LAUNCHED])
		puts("You aren't able to move; you better drop something.");
	else
		puts("You are out of fuel; now you will rot in space forever!");
	return(1);
}

convert(tothis)		/* Converts day to night and vice versa. 	    */
int tothis;		/* Day objects are permanent.  Night objects are added*/
{			/* at dusk, and subtracted at dawn.		*/
	register struct objs *p;
	register i, j;

	if (tothis == TONIGHT) {
		for (i = 1; i <= NUMOFROOMS; i++)
			for (j = 0; j < NUMOFWORDS; j++)
				nightfile[i].objects[j] = dayfile[i].objects[j];
		for (p = nightobjs; p->room != 0; p++)
			setbit(nightfile[p->room].objects, p->obj);
		location = nightfile;
	} else {
		for (i = 1; i <= NUMOFROOMS; i++)
			for (j = 0; j < NUMOFWORDS; j++)
				dayfile[i].objects[j] = nightfile[i].objects[j];
		for (p = nightobjs; p->room != 0; p++)
			clearbit(dayfile[p->room].objects, p->obj);
		location = dayfile;
	}
}

news()
{
	register int n;
	int hurt;

	if (gtime > 30 && position < 32){
		puts("An explosion of shuddering magnitude splinters bulkheads and");
		puts("ruptures the battlestar's hull.  You are sucked out into the");
		puts("frozen void of space and killed.");
		die();
	}
	if (gtime > 20 && position < 32)
		puts("Explosions rock the battlestar.");
	if (gtime > snooze){
		puts("You drop from exhaustion...");
		zzz();
	}
	if (gtime > snooze - 5)
		puts("You're getting tired.");
	if (gtime > (rythmn + CYCLE)) {
		if (location == nightfile) {
			convert(TODAY);
			if (OUTSIDE && gtime - rythmn - CYCLE < 10) {
				puts("Dew lit sunbeams stretch out from a watery sunrise and herald the dawn.");
				puts("You awake from a misty dream-world into stark reality.");
				puts("It is day.");
			}
		} else {
			convert(TONIGHT);
			clearbit(location[POOLS].objects, BATHGOD);
			if (OUTSIDE && gtime - rythmn - CYCLE < 10) {
				puts("The dying sun sinks into the ocean, leaving a blood stained sunset.");
				puts("The sky slowly fades from orange to violet to black.  A few stars");
				puts("flicker on, and it is night.");
				puts("The world seems completly different at night.");
			}
		}
		rythmn = gtime - gtime % CYCLE;
	}
	if (!wiz && !tempwiz)
		if ((testbit(inven,TALISMAN) || testbit(wear,TALISMAN)) && (testbit(inven,MEDALION) || testbit(wear,MEDALION)) && (testbit(inven,AMULET) || testbit(wear,AMULET))){
			tempwiz = 1;
			puts("The three amulets glow and reenforce each other in power.\nYou are now a wizard.");
	}
	if (testbit(location[position].objects,ELF)){
		printf("%s\n",objdes[ELF]);
		fight(ELF,rnd(30));
	}
	if (testbit(location[position].objects,DARK)){
		printf("%s\n",objdes[DARK]);
		fight(DARK,100);
	}
	if (testbit(location[position].objects,WOODSMAN)){
		printf("%s\n",objdes[WOODSMAN]);
		fight(WOODSMAN,50);
	}
	switch(position){

		case 267:
		case 257:	/* entering a cave */
		case 274:
		case 246:
			notes[CANTSEE] = 1;
			break;
		case 160:
		case 216:	/* leaving a cave */
		case 230:
		case 231:
		case 232:
			notes[CANTSEE] = 0;
			break;
	}
	if (testbit(location[position].objects, GIRL))
		meetgirl = 1;
	if (meetgirl && CYCLE * 1.5 - gtime < 10){
		setbit(location[GARDEN].objects,GIRLTALK);
		setbit(location[GARDEN].objects,LAMPON);
		setbit(location[GARDEN].objects,ROPE);
	}
	if (position == DOCK && (beenthere[position] || gtime > CYCLE)){
		clearbit(location[DOCK].objects, GIRL);
		clearbit(location[DOCK].objects,MAN);
	}
	if (meetgirl && gtime - CYCLE * 1.5 > 10){
		clearbit(location[GARDEN].objects,GIRLTALK);
		clearbit(location[GARDEN].objects,LAMPON);
		clearbit(location[GARDEN].objects,ROPE);
		meetgirl = 0;
	}
	if (testbit(location[position].objects,CYLON)){
		puts("Oh my God, you're being shot at by an alien spacecraft!");
		printf("The targeting computer says we have %d seconds to attack!\n",gclock);
		fflush(stdout);
		sleep(1);
		if (!visual()){
			hurt = rnd(NUMOFINJURIES);
			injuries[hurt] = 1;
			puts("Laser blasts sear the cockpit, and the alien veers off in a victory roll.");
			puts("The viper shudders under a terrible explosion.");
			printf("I'm afraid you have suffered %s.\n", ouch[hurt]);
		}
		else
			clearbit(location[position].objects,CYLON);
	}
	if (injuries[SKULL] && injuries[INCISE] && injuries[NECK]){
		puts("I'm afraid you have suffered fatal injuries.");
		die();
	}
	for (n=0; n < NUMOFINJURIES; n++)
		if (injuries[n] == 1){
			injuries[n] = 2;
			if (WEIGHT > 5)
				WEIGHT -= 5;
			else
				WEIGHT = 0;
		}
	if (injuries[ARM] == 2){
		CUMBER -= 5;
		injuries[ARM]++;
	}
	if (injuries[RIBS] == 2){
		CUMBER -= 2;
		injuries[RIBS]++;
	}
	if (injuries[SPINE] == 2){
		WEIGHT = 0;
		injuries[SPINE]++;
	}
	if (carrying > WEIGHT || encumber > CUMBER)
		notes[CANTMOVE] = 1;
	else
		notes[CANTMOVE] = 0;
}

crash()
{
	int hurt1,hurt2;

	fuel--;
	if (!location[position].flyhere || (testbit(location[position].objects,LAND) && fuel <= 0)){
		if (!location[position].flyhere)
			puts("You're flying too low.  We're going to crash!");
		else{
			puts("You're out of fuel.  We'll have to crash land!");
			if (!location[position].down){
				puts("Your viper strikes the ground and explodes into firey fragments.");
				puts("Thick black smoke billows up from the wreckage.");
				die();
			}
			position = location[position].down;
		}
		notes[LAUNCHED] = 0;
		setbit(location[position].objects,CRASH);
		gtime += rnd(CYCLE/4);
		puts("The viper explodes into the ground and you lose consciousness...");
		zzz();
		hurt1 = rnd(NUMOFINJURIES - 2) + 2;
		hurt2 = rnd(NUMOFINJURIES - 2) + 2;
		injuries[hurt1] = 1;
		injuries[hurt2] = 1;
		injuries[0] = 1;	/* abrasions */
		injuries[1] = 1;	/* lacerations */
		printf("I'm afraid you have suffered %s and %s.\n",ouch[hurt1],ouch[hurt2]);
	}
}
