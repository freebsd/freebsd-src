/* regen.c 			Larn is copyrighted 1986 by Noah Morgan. */
/* $FreeBSD: src/games/larn/regen.c,v 1.4 1999/11/16 02:57:24 billf Exp $ */
#include "header.h"
/*
	*******
	REGEN()
	*******
	regen()

	subroutine to regenerate player hp and spells
 */
regen()
	{
	int i,flag;
	long *d;
	d = c;
#ifdef EXTRA
	d[MOVESMADE]++;
#endif
	if (d[TIMESTOP])  { if(--d[TIMESTOP]<=0) bottomline();  return; }	/* for stop time spell */
	flag=0;

	if (d[STRENGTH]<3)	{ d[STRENGTH]=3; flag=1; }
	if ((d[HASTESELF]==0) || ((d[HASTESELF] & 1) == 0))
		gtime++;

	if (d[HP] != d[HPMAX])
		if (d[REGENCOUNTER]-- <= 0)		/*	regenerate hit points	*/
			{
			d[REGENCOUNTER] = 22 + (d[HARDGAME]<<1) - d[LEVEL];
			if ((d[HP] += d[REGEN]) > d[HPMAX])  d[HP] = d[HPMAX];
			bottomhp();
			}

	if (d[SPELLS] < d[SPELLMAX])		/*	regenerate spells	*/
		if (d[ECOUNTER]-- <= 0)
			{
			d[ECOUNTER] = 100+4*(d[HARDGAME]-d[LEVEL]-d[ENERGY]);
			d[SPELLS]++;	bottomspell();
			}

	if (d[HERO])			if (--d[HERO]<=0) { for (i=0; i<6; i++) d[i] -= 10; flag=1; }
	if (d[ALTPRO])			if (--d[ALTPRO]<=0)			{ d[MOREDEFENSES]-=3; flag=1; }
	if (d[PROTECTIONTIME])	if (--d[PROTECTIONTIME]<=0)	{ d[MOREDEFENSES]-=2; flag=1; }
	if (d[DEXCOUNT])		if (--d[DEXCOUNT]<=0)		{ d[DEXTERITY]-=3; flag=1; }
	if (d[STRCOUNT])		if (--d[STRCOUNT]<=0)		{ d[STREXTRA]-=3; flag=1; }
	if (d[BLINDCOUNT])		if (--d[BLINDCOUNT]<=0)		{ cursors();  lprcat("\nThe blindness lifts  "); beep(); }
	if (d[CONFUSE])			if (--d[CONFUSE]<=0) { cursors();  lprcat("\nYou regain your senses"); beep(); }
	if (d[GIANTSTR])		if (--d[GIANTSTR]<=0) { d[STREXTRA] -= 20; flag=1; }
	if (d[CHARMCOUNT])		if ((--d[CHARMCOUNT]) <= 0) flag=1;
	if (d[INVISIBILITY])	if ((--d[INVISIBILITY]) <= 0) flag=1;
	if (d[CANCELLATION])	if ((--d[CANCELLATION]) <= 0) flag=1;
	if (d[WTW])				if ((--d[WTW]) <= 0) flag=1;
	if (d[HASTESELF])		if ((--d[HASTESELF]) <= 0) flag=1;
	if (d[AGGRAVATE])		--d[AGGRAVATE];
	if (d[SCAREMONST])		if ((--d[SCAREMONST]) <= 0) flag=1;
	if (d[STEALTH])			if ((--d[STEALTH]) <= 0) flag=1;
	if (d[AWARENESS])		--d[AWARENESS];
	if (d[HOLDMONST])		if ((--d[HOLDMONST]) <= 0) flag=1;
	if (d[HASTEMONST])		--d[HASTEMONST];
	if (d[FIRERESISTANCE])	if ((--d[FIRERESISTANCE]) <= 0) flag=1;
	if (d[GLOBE])			if (--d[GLOBE]<=0) { d[MOREDEFENSES]-=10; flag=1; }
	if (d[SPIRITPRO])		if (--d[SPIRITPRO] <= 0) flag=1;
	if (d[UNDEADPRO])		if (--d[UNDEADPRO] <= 0) flag=1;
	if (d[HALFDAM])			if (--d[HALFDAM]<=0)  { cursors();  lprcat("\nYou now feel better "); beep(); }
	if (d[SEEINVISIBLE])
	  if (--d[SEEINVISIBLE]<=0)
		{ monstnamelist[INVISIBLESTALKER] = ' ';
		  cursors();  lprcat("\nYou feel your vision return to normal"); beep(); }
	if (d[ITCHING])
		{
		if (d[ITCHING]>1)
			if ((d[WEAR]!= -1) || (d[SHIELD]!= -1))
				if (rnd(100)<50)
					{
					d[WEAR]=d[SHIELD]= -1; cursors();
					lprcat("\nThe hysteria of itching forces you to remove your armor!");
					beep(); recalc();  bottomline();
					}
		if (--d[ITCHING]<=0) { cursors();  lprcat("\nYou now feel the irritation subside!"); beep(); }
		}
	if (d[CLUMSINESS])
		{
		if (d[WIELD] != -1)
			if (d[CLUMSINESS]>1)
			  if (item[playerx][playery]==0)	/* only if nothing there */
				if (rnd(100)<33) /* drop your weapon due to clumsiness */
					drop_object((int)d[WIELD]);
		if (--d[CLUMSINESS]<=0) { cursors();  lprcat("\nYou now feel less awkward!"); beep(); }
		}
	if (flag) bottomline();
	}

