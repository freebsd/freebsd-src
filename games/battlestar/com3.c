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
static char sccsid[] = "@(#)com3.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

#include "externs.h"

dig()
{
	if (testbit(inven,SHOVEL)){
		puts("OK");
		time++;
		switch(position){
			case 144:		/* copse near beach */
				if (!notes[DUG]){
					setbit(location[position].objects,DEADWOOD);
					setbit(location[position].objects,COMPASS);
					setbit(location[position].objects,KNIFE);
					setbit(location[position].objects,MACE);
					notes[DUG] = 1;
				}
				break;

			default:
				puts("Nothing happens.");
		}
	}
	else	
		puts("You don't have a shovel.");
}

jump()
{
	register int n;

	switch(position){
		default:
			puts("Nothing happens.");
			return(-1);

		case 242:
			position = 133;
			break;
		case 214:
		case 215:
		case 162:
		case 159:
			position = 145;
			break;
		case 232:
			position = 275;
			break;
		case 3:
			position = 1;
			break;
		case 172:
			position = 201;
	}
	puts("Ahhhhhhh...");
	injuries[12] = injuries[8] = injuries[7] = injuries[6] = 1;
	for (n=0; n < NUMOFOBJECTS; n++)
		if (testbit(inven,n)){
			clearbit(inven,n);
			setbit(location[position].objects,n);
		}
	carrying = 0;
	encumber = 0;
	return(0);
}

bury()
{
	int value;

	if (testbit(inven,SHOVEL)){
		while(wordtype[++wordnumber] != OBJECT && wordtype[wordnumber] != NOUNS && wordnumber < wordcount);
		value = wordvalue[wordnumber];
		if (wordtype[wordnumber] == NOUNS && (testbit(location[position].objects,value) || value == BODY))
			switch(value){
				case BODY:
					wordtype[wordnumber] = OBJECT;
					if (testbit(inven,MAID) || testbit(location[position].objects,MAID))
						value = MAID;
					if (testbit(inven,DEADWOOD) || testbit(location[position].objects,DEADWOOD))
						value = DEADWOOD;
					if (testbit(inven,DEADGOD) || testbit(location[position].objects,DEADGOD))
						value = DEADGOD;
					if (testbit(inven,DEADTIME) || testbit(location[position].objects,DEADTIME))
						value = DEADTIME;
					if (testbit(inven,DEADNATIVE) || testbit(location[position].objects,DEADNATIVE))
						value = DEADNATIVE;
					break;

				case NATIVE:
				case NORMGOD:
					puts("She screams as you wrestle her into the hole.");
				case TIMER:
					power += 7;
					ego -= 10;
				case AMULET:
				case MEDALION:
				case TALISMAN:
					wordtype[wordnumber] = OBJECT;
					break;

				default:
					puts("Wha..?");
			}
		if (wordtype[wordnumber] == OBJECT && position > 88 && (testbit(inven,value) || testbit(location[position].objects,value))){
			puts("Buried.");
			if (testbit(inven,value)){
				clearbit(inven,value);
				carrying -= objwt[value];
				encumber -= objcumber[value];
			}
			clearbit(location[position].objects,value);
			switch(value){
				case MAID:
				case DEADWOOD:
				case DEADNATIVE:
				case DEADTIME:
				case DEADGOD:
					ego += 2;
					printf("The %s should rest easier now.\n",objsht[value]);
			}
		}
		else
			puts("It doesn't seem to work.");
	}
	else
		puts("You aren't holding a shovel.");
}

drink()
{
	register int n;

	if (testbit(inven,POTION)){
		puts("The cool liquid runs down your throat but turns to fire and you choke.");
		puts("The heat reaches your limbs and tingles your spirit.  You feel like falling");
		puts("asleep.");
		clearbit(inven, POTION);
		WEIGHT = MAXWEIGHT;
		CUMBER = MAXCUMBER;
		for (n=0; n < NUMOFINJURIES; n++)
			injuries[n] = 0;
		time++;
		zzz();
	}
	else
		puts("I'm not thirsty.");
}

shoot()
{
	int firstnumber, value;
	register int n;

	if (!testbit(inven,LASER))
		puts("You aren't holding a blaster.");
	else {
		firstnumber = wordnumber;
		while(wordtype[++wordnumber] == ADJS);
		while(wordnumber<=wordcount && wordtype[wordnumber] == OBJECT){
			value = wordvalue[wordnumber];
			printf("%s:\n", objsht[value]);
			for (n=0; objsht[value][n]; n++);
			if (testbit(location[position].objects,value)){
				clearbit(location[position].objects,value);
				time++;
				printf("The %s explode%s\n",objsht[value],(objsht[value][n-1]=='s' ? (objsht[value][n-2]=='s' ? "s." : ".") : "s."));
				if (value == BOMB)
					die();
			}
			else
				printf("I dont see any %s around here.\n", objsht[value]);
			if (wordnumber < wordcount - 1 && wordvalue[++wordnumber] == AND)
				wordnumber++;
			else
				return(firstnumber);
		}
			    /* special cases with their own return()'s */

		if (wordnumber <= wordcount && wordtype[wordnumber] == NOUNS){
			time++;
			switch(wordvalue[wordnumber]){
			
				case DOOR:
					switch(position){
						case 189:
						case 231:
							puts("The door is unhinged.");
							location[189].north = 231;
							location[231].south = 189;
							whichway(location[position]);
							break;
						case 30:
							puts("The wooden door splinters.");
							location[30].west = 25;
							whichway(location[position]);
							break;
						case 31:
							puts("The laser blast has no effect on the door.");
							break;
						case 20:
							puts("The blast hits the door and it explodes into flame.  The magnesium burns");
							puts("so rapidly that we have no chance to escape.");
							die();
						default:
							puts("Nothing happens.");
					}
					break;

				case NORMGOD:
					if (testbit(location[position].objects,BATHGOD)){
						puts("The goddess is hit in the chest and splashes back against the rocks.");
						puts("Dark blood oozes from the charred blast hole.  Her naked body floats in the");
						puts("pools and then off downstream.");
						clearbit(location[position].objects,BATHGOD);
						setbit(location[180].objects,DEADGOD);
						power += 5;
						ego -= 10;
						notes[JINXED]++;
					} else if (testbit(location[position].objects,NORMGOD)){
						puts("The blast catches the goddess in the stomach, knocking her to the ground.");
						puts("She writhes in the dirt as the agony of death taunts her.");
						puts("She has stopped moving.");
						clearbit(location[position].objects,NORMGOD);
						setbit(location[position].objects,DEADGOD);
						power += 5;
						ego -= 10;
						notes[JINXED]++;
						if (wintime)
							live();
						break;
					} else
						puts("I don't see any goddess around here.");
					break;

				case TIMER:
					if (testbit(location[position].objects,TIMER)){
						puts("The old man slumps over the bar.");
						power++;
						ego -= 2;
						notes[JINXED]++;
						clearbit(location[position].objects,TIMER);
						setbit(location[position].objects,DEADTIME);
					}
					else puts("What old timer?");
					break;
				case MAN:
					if (testbit(location[position].objects,MAN)){
						puts("The man falls to the ground with blood pouring all over his white suit.");
						puts("Your fantasy is over.");
						die();
					}
					else puts("What man?");
					break;
				case NATIVE:
					if (testbit(location[position].objects,NATIVE)){
						puts("The girl is blown backwards several feet and lies in a pool of blood.");
						clearbit(location[position].objects,NATIVE);
						setbit(location[position].objects,DEADNATIVE);
						power += 5;
						ego -= 2;
						notes[JINXED]++;
					} else puts("There is no girl here.");
					break;
				case -1:
					puts("Shoot what?");
					break;

				default:
					printf("You can't shoot the %s.\n",objsht[wordvalue[wordnumber]]);
			}
		}
		else puts("You must be a looney.");
	}
	return(firstnumber);
}
