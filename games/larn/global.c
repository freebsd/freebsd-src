/*	global.c 		Larn is copyrighted 1986 by Noah Morgan.
 * $FreeBSD: src/games/larn/global.c,v 1.5 1999/11/16 02:57:21 billf Exp $
 *
 *	raiselevel()		subroutine to raise the player one level
 *	loselevel()		subroutine to lower the player by one level
 *	raiseexperience(x)	subroutine to increase experience points
 *	loseexperience(x)	subroutine to lose experience points
 *	losehp(x)			subroutine to remove hit points from the player
 *	losemhp(x)			subroutine to remove max # hit points from the player
 *	raisehp(x)			subroutine to gain hit points
 *	raisemhp(x)			subroutine to gain maximum hit points
 *	losespells(x)		subroutine to lose spells
 *	losemspells(x)		subroutine to lose maximum spells
 *	raisespells(x)		subroutine to gain spells
 *	raisemspells(x)		subroutine to gain maximum spells
 *	recalc()			function to recalculate the armor class of the player
 *	makemonst(lev)		function to return monster number for a randomly selected monster
 *	positionplayer()	function to be sure player is not in a wall
 *	quit()				subroutine to ask if the player really wants to quit
 *
 */

#include "header.h"
extern int score[],srcount,dropflag;
extern short playerx,playery,lastnum;
extern char cheat,level,monstnamelist[];
extern char lastmonst[],*what[],*who[];
extern char winner[];
extern char logname[],monstlevel[];
extern char sciv[SCORESIZE+1][26][2],*potionname[],*scrollname[];
/*
	***********
	RAISE LEVEL
	***********
	raiselevel()

	subroutine to raise the player one level
	uses the skill[] array to find level boundarys
	uses c[EXPERIENCE]  c[LEVEL]
 */
raiselevel()
	{
	if (c[LEVEL] < MAXPLEVEL) raiseexperience((long)(skill[c[LEVEL]]-c[EXPERIENCE]));
	}

/*
	***********
	LOOSE LEVEL
	***********
    loselevel()

	subroutine to lower the players character level by one
 */
loselevel()
	{
	if (c[LEVEL] > 1) loseexperience((long)(c[EXPERIENCE] - skill[c[LEVEL]-1] + 1));
	}

/*
	****************
	RAISE EXPERIENCE
	****************
	raiseexperience(x)

	subroutine to increase experience points
 */
raiseexperience(x)
	long x;
	{
	int i,tmp;
	i=c[LEVEL];	c[EXPERIENCE]+=x;
	while (c[EXPERIENCE] >= skill[c[LEVEL]] && (c[LEVEL] < MAXPLEVEL))
		{
		tmp = (c[CONSTITUTION]-c[HARDGAME])>>1;
		c[LEVEL]++;	raisemhp((int)(rnd(3)+rnd((tmp>0)?tmp:1)));
		raisemspells((int)rund(3));
		if (c[LEVEL] < 7-c[HARDGAME]) raisemhp((int)(c[CONSTITUTION]>>2));
		}
	if (c[LEVEL] != i)
		{
		cursors();
		beep(); lprintf("\nWelcome to level %d",(long)c[LEVEL]);	/* if we changed levels	*/
		}
	bottomline();
	}

/*
	****************
	LOOSE EXPERIENCE
	****************
	loseexperience(x)

	subroutine to lose experience points
 */
loseexperience(x)
	long x;
	{
	int i,tmp;
	i=c[LEVEL];		c[EXPERIENCE]-=x;
	if (c[EXPERIENCE] < 0) c[EXPERIENCE]=0;
	while (c[EXPERIENCE] < skill[c[LEVEL]-1])
		{
		if (--c[LEVEL] <= 1) c[LEVEL]=1;	/*	down one level		*/
		tmp = (c[CONSTITUTION]-c[HARDGAME])>>1;	/* lose hpoints */
		losemhp((int)rnd((tmp>0)?tmp:1));	/* lose hpoints */
		if (c[LEVEL] < 7-c[HARDGAME]) losemhp((int)(c[CONSTITUTION]>>2));
		losemspells((int)rund(3));				/*	lose spells		*/
		}
	if (i!=c[LEVEL])
		{
		cursors();
		beep(); lprintf("\nYou went down to level %d!",(long)c[LEVEL]);
		}
	bottomline();
	}

/*
	********
	LOOSE HP
	********
	losehp(x)
	losemhp(x)

	subroutine to remove hit points from the player
	warning -- will kill player if hp goes to zero
 */
losehp(x)
	int x;
	{
	if ((c[HP] -= x) <= 0)
		{
		beep(); lprcat("\n");  nap(3000);  died(lastnum);
		}
	}

losemhp(x)
	int x;
	{
	c[HP] -= x;		if (c[HP] < 1)		c[HP]=1;
	c[HPMAX] -= x;	if (c[HPMAX] < 1)	c[HPMAX]=1;
	}

/*
	********
	RAISE HP
	********
	raisehp(x)
	raisemhp(x)

	subroutine to gain maximum hit points
 */
raisehp(x)
	int x;
	{
	if ((c[HP] += x) > c[HPMAX]) c[HP] = c[HPMAX];
	}

raisemhp(x)
	int x;
	{
	c[HPMAX] += x;	c[HP] += x;
	}

/*
	************
	RAISE SPELLS
	************
	raisespells(x)
	raisemspells(x)

	subroutine to gain maximum spells
 */
raisespells(x)
	int x;
	{
	if ((c[SPELLS] += x) > c[SPELLMAX])	c[SPELLS] = c[SPELLMAX];
	}

raisemspells(x)
	int x;
	{
	c[SPELLMAX]+=x; c[SPELLS]+=x;
	}

/*
	************
	LOOSE SPELLS
	************
	losespells(x)
	losemspells(x)

	subroutine to lose maximum spells
 */
losespells(x)
	int x;
	{
	if ((c[SPELLS] -= x) < 0) c[SPELLS]=0;
	}

losemspells(x)
	int x;
	{
	if ((c[SPELLMAX] -= x) < 0) c[SPELLMAX]=0;
	if ((c[SPELLS] -= x) < 0) c[SPELLS]=0;
	}

/*
	makemonst(lev)
		int lev;

	function to return monster number for a randomly selected monster
		for the given cave level
 */
makemonst(lev)
	int lev;
	{
	int tmp,x;
	if (lev < 1)	lev = 1;			if (lev > 12)	lev = 12;
	tmp=WATERLORD;
	if (lev < 5)
		while (tmp==WATERLORD) tmp=rnd((x=monstlevel[lev-1])?x:1);
	else while (tmp==WATERLORD)
		tmp=rnd((x=monstlevel[lev-1]-monstlevel[lev-4])?x:1)+monstlevel[lev-4];

	while (monster[tmp].genocided && tmp<MAXMONST) tmp++; /* genocided? */
	return(tmp);
	}

/*
	positionplayer()

	function to be sure player is not in a wall
 */
positionplayer()
	{
	int try;
	try = 2;
	while ((item[playerx][playery] || mitem[playerx][playery]) && (try))
		if (++playerx >= MAXX-1)
			{
			playerx = 1;
			if (++playery >= MAXY-1)
				{	playery = 1;	--try;	}
			}
	if (try==0)	 lprcat("Failure in positionplayer\n");
	}

/*
	recalc()	function to recalculate the armor class of the player
 */
recalc()
	{
	int i,j,k;
	c[AC] = c[MOREDEFENSES];
	if (c[WEAR] >= 0)
		switch(iven[c[WEAR]])
			{
			case OSHIELD:		c[AC] += 2 + ivenarg[c[WEAR]]; break;
			case OLEATHER:		c[AC] += 2 + ivenarg[c[WEAR]]; break;
			case OSTUDLEATHER:	c[AC] += 3 + ivenarg[c[WEAR]]; break;
			case ORING:			c[AC] += 5 + ivenarg[c[WEAR]]; break;
			case OCHAIN:		c[AC] += 6 + ivenarg[c[WEAR]]; break;
			case OSPLINT:		c[AC] += 7 + ivenarg[c[WEAR]]; break;
			case OPLATE:		c[AC] += 9 + ivenarg[c[WEAR]]; break;
			case OPLATEARMOR:	c[AC] += 10 + ivenarg[c[WEAR]]; break;
			case OSSPLATE:		c[AC] += 12 + ivenarg[c[WEAR]]; break;
			}

	if (c[SHIELD] >= 0) if (iven[c[SHIELD]] == OSHIELD) c[AC] += 2 + ivenarg[c[SHIELD]];
	if (c[WIELD] < 0)  c[WCLASS] = 0;  else
		{
		i = ivenarg[c[WIELD]];
		switch(iven[c[WIELD]])
			{
			case ODAGGER:    c[WCLASS] =  3 + i;  break;
			case OBELT:	     c[WCLASS] =  7 + i;  break;
			case OSHIELD:	 c[WCLASS] =  8 + i;  break;
			case OSPEAR:     c[WCLASS] = 10 + i;  break;
			case OFLAIL:     c[WCLASS] = 14 + i;  break;
			case OBATTLEAXE: c[WCLASS] = 17 + i;  break;
			case OLANCE:	 c[WCLASS] = 19 + i;  break;
			case OLONGSWORD: c[WCLASS] = 22 + i;  break;
			case O2SWORD:    c[WCLASS] = 26 + i;  break;
			case OSWORD:     c[WCLASS] = 32 + i;  break;
			case OSWORDofSLASHING: c[WCLASS] = 30 + i; break;
			case OHAMMER:    c[WCLASS] = 35 + i;  break;
			default:	     c[WCLASS] = 0;
			}
		}
	c[WCLASS] += c[MOREDAM];

/*	now for regeneration abilities based on rings	*/
	c[REGEN]=1;		c[ENERGY]=0;
	j=0;  for (k=25; k>0; k--)  if (iven[k]) {j=k; k=0; }
	for (i=0; i<=j; i++)
		{
		switch(iven[i])
			{
			case OPROTRING: c[AC]     += ivenarg[i] + 1;	break;
			case ODAMRING:  c[WCLASS] += ivenarg[i] + 1;	break;
			case OBELT:     c[WCLASS] += ((ivenarg[i]<<1)) + 2;	break;

			case OREGENRING:	c[REGEN]  += ivenarg[i] + 1;	break;
			case ORINGOFEXTRA:	c[REGEN]  += 5 * (ivenarg[i]+1); break;
			case OENERGYRING:	c[ENERGY] += ivenarg[i] + 1;	break;
			}
		}
	}


/*
	quit()

	subroutine to ask if the player really wants to quit
 */
quit()
	{
	int i;
	cursors();	strcpy(lastmonst,"");
	lprcat("\n\nDo you really want to quit?");
	while (1)
		{
		i=getchar();
		if (i == 'y')	{ died(300); return; }
		if ((i == 'n') || (i == '\33'))	{ lprcat(" no"); lflush(); return; }
		lprcat("\n");  setbold();  lprcat("Yes");  resetbold();  lprcat(" or ");
		setbold();  lprcat("No");  resetbold();  lprcat(" please?   Do you want to quit? ");
		}
	}

/*
	function to ask --more-- then the user must enter a space
 */
more()
	{
	lprcat("\n  --- press ");  standout("space");  lprcat(" to continue --- ");
	while (getchar() != ' ');
	}

/*
	function to put something in the players inventory
	returns 0 if success, 1 if a failure
 */
take(itm,arg)
	int itm,arg;
	{
	int i,limit;
/*	cursors(); */
	if ((limit = 15+(c[LEVEL]>>1)) > 26)  limit=26;
	for (i=0; i<limit; i++)
		if (iven[i]==0)
			{
			iven[i] = itm;  ivenarg[i] = arg;  limit=0;
			switch(itm)
				{
				case OPROTRING:	case ODAMRING: case OBELT: limit=1;  break;
				case ODEXRING:		c[DEXTERITY] += ivenarg[i]+1; limit=1;	break;
				case OSTRRING:		c[STREXTRA]  += ivenarg[i]+1;	limit=1; break;
				case OCLEVERRING:	c[INTELLIGENCE] += ivenarg[i]+1;  limit=1; break;
				case OHAMMER:		c[DEXTERITY] += 10;	c[STREXTRA]+=10;
									c[INTELLIGENCE]-=10;	limit=1;	 break;

				case OORBOFDRAGON:	c[SLAYING]++;		break;
				case OSPIRITSCARAB: c[NEGATESPIRIT]++;	break;
				case OCUBEofUNDEAD: c[CUBEofUNDEAD]++;	break;
				case ONOTHEFT:		c[NOTHEFT]++;		break;
				case OSWORDofSLASHING:	c[DEXTERITY] +=5;	limit=1; break;
				};
			lprcat("\nYou pick up:"); srcount=0;  show3(i);
			if (limit) bottomline();  return(0);
			}
	lprcat("\nYou can't carry anything else");  return(1);
	}

/*
	subroutine to drop an object  returns 1 if something there already else 0
 */
drop_object(k)
	int k;
	{
	int itm;
	if ((k<0) || (k>25)) return(0);
	itm = iven[k];	cursors();
	if (itm==0) { lprintf("\nYou don't have item %c! ",k+'a'); return(1); }
	if (item[playerx][playery])
		{ beep(); lprcat("\nThere's something here already"); return(1); }
	if (playery==MAXY-1 && playerx==33) return(1); /* not in entrance */
	item[playerx][playery] = itm;
	iarg[playerx][playery] = ivenarg[k];
	srcount=0; lprcat("\n  You drop:"); show3(k); /* show what item you dropped*/
	know[playerx][playery] = 0;  iven[k]=0;
	if (c[WIELD]==k) c[WIELD]= -1;		if (c[WEAR]==k)  c[WEAR] = -1;
	if (c[SHIELD]==k) c[SHIELD]= -1;
	adjustcvalues(itm,ivenarg[k]);
	dropflag=1; /* say dropped an item so wont ask to pick it up right away */
	return(0);
	}

/*
	function to enchant armor player is currently wearing
 */
enchantarmor()
	{
	int tmp;
	if (c[WEAR]<0) { if (c[SHIELD] < 0)
		{ cursors(); beep(); lprcat("\nYou feel a sense of loss"); return; }
					else { tmp=iven[c[SHIELD]]; if (tmp != OSCROLL) if (tmp != OPOTION) { ivenarg[c[SHIELD]]++; bottomline(); } } }
	tmp = iven[c[WEAR]];
	if (tmp!=OSCROLL) if (tmp!=OPOTION)  { ivenarg[c[WEAR]]++;  bottomline(); }
	}

/*
	function to enchant a weapon presently being wielded
 */
enchweapon()
	{
	int tmp;
	if (c[WIELD]<0)
		{ cursors(); beep(); lprcat("\nYou feel a sense of loss"); return; }
	tmp = iven[c[WIELD]];
	if (tmp!=OSCROLL) if (tmp!=OPOTION)
		{ ivenarg[c[WIELD]]++;
		  if (tmp==OCLEVERRING) c[INTELLIGENCE]++;  else
		  if (tmp==OSTRRING)	c[STREXTRA]++;  else
		  if (tmp==ODEXRING)    c[DEXTERITY]++;		  bottomline(); }
	}

/*
	routine to tell if player can carry one more thing
	returns 1 if pockets are full, else 0
 */
pocketfull()
	{
	int i,limit;
	if ((limit = 15+(c[LEVEL]>>1)) > 26)  limit=26;
	for (i=0; i<limit; i++) if (iven[i]==0) return(0);
	return(1);
	}

/*
	function to return 1 if a monster is next to the player else returns 0
 */
nearbymonst()
	{
	int tmp,tmp2;
	for (tmp=playerx-1; tmp<playerx+2; tmp++)
		for (tmp2=playery-1; tmp2<playery+2; tmp2++)
			if (mitem[tmp][tmp2]) return(1); /* if monster nearby */
	return(0);
	}

/*
	function to steal an item from the players pockets
	returns 1 if steals something else returns 0
 */
stealsomething()
	{
	int i,j;
	j=100;
	while (1)
		{
		i=rund(26);
		if (iven[i]) if (c[WEAR]!=i) if (c[WIELD]!=i) if (c[SHIELD]!=i)
			{
			srcount=0; show3(i);
			adjustcvalues(iven[i],ivenarg[i]);  iven[i]=0; return(1);
			}
		if (--j <= 0) return(0);
		}
	}

/*
	function to return 1 is player carrys nothing else return 0
 */
emptyhanded()
	{
	int i;
	for (i=0; i<26; i++)
		if (iven[i]) if (i!=c[WIELD]) if (i!=c[WEAR]) if (i!=c[SHIELD]) return(0);
	return(1);
	}

/*
	function to create a gem on a square near the player
 */
creategem()
	{
	int i,j;
	switch(rnd(4))
		{
		case 1:	 i=ODIAMOND;	j=50;	break;
		case 2:	 i=ORUBY;		j=40;	break;
		case 3:	 i=OEMERALD;	j=30;	break;
		default: i=OSAPPHIRE;	j=20;	break;
		};
	createitem(i,rnd(j)+j/10);
	}

/*
	function to change character levels as needed when dropping an object
	that affects these characteristics
 */
adjustcvalues(itm,arg)
	int itm,arg;
	{
	int flag;
	flag=0;
	switch(itm)
		{
		case ODEXRING:	c[DEXTERITY] -= arg+1;  flag=1; break;
		case OSTRRING:	c[STREXTRA]  -= arg+1;  flag=1; break;
		case OCLEVERRING: c[INTELLIGENCE] -= arg+1;  flag=1; break;
		case OHAMMER:	c[DEXTERITY] -= 10;	c[STREXTRA] -= 10;
						c[INTELLIGENCE] += 10; flag=1; break;
		case OSWORDofSLASHING:	c[DEXTERITY] -= 5;	flag=1; break;
		case OORBOFDRAGON:		--c[SLAYING];		return;
		case OSPIRITSCARAB:		--c[NEGATESPIRIT];	return;
		case OCUBEofUNDEAD:		--c[CUBEofUNDEAD];	return;
		case ONOTHEFT:			--c[NOTHEFT]; 		return;
		case OLANCE:		c[LANCEDEATH]=0;	return;
		case OPOTION:	case OSCROLL:	return;

		default:	flag=1;
		};
	if (flag) bottomline();
	}

/*
	function to read a string from token input "string"
	returns a pointer to the string
 */
gettokstr(str)
	char *str;
	{
	int i,j;
	i=50;
	while ((getchar() != '"') && (--i > 0));
	i=36;
	while (--i > 0)
		{
		if ((j=getchar()) != '"') *str++ = j;  else i=0;
		}
	*str = 0;
	i=50;
	if (j != '"') while ((getchar() != '"') && (--i > 0)); /* if end due to too long, then find closing quote */
	}

/*
	function to ask user for a password (no echo)
	returns 1 if entered correctly, 0 if not
 */
static char gpwbuf[33];
getpassword()
	{
	int i,j;
	char *gpwp;
	extern char *password;
	scbr();	/*	system("stty -echo cbreak"); */
	gpwp = gpwbuf;	lprcat("\nEnter Password: "); lflush();
	i = strlen(password);
	for (j=0; j<i; j++) read(0,gpwp++,1);	  gpwbuf[i]=0;
	sncbr(); /* system("stty echo -cbreak"); */
	if (strcmp(gpwbuf,password) != 0)
		{	lprcat("\nSorry\n");  lflush(); return(0);	}
	else  return(1);
	}

/*
	subroutine to get a yes or no response from the user
	returns y or n
 */
getyn()
	{
	int i;
	i=0; while (i!='y' && i!='n' && i!='\33') i=getchar();
	return(i);
	}

/*
	function to calculate the pack weight of the player
	returns the number of pounds the player is carrying
 */
packweight()
	{
	int i,j,k;
	k=c[GOLD]/1000; j=25;  while ((iven[j]==0) && (j>0)) --j;
	for (i=0; i<=j; i++)
		switch(iven[i])
			{
			case 0:												break;
			case OSSPLATE:   case OPLATEARMOR:		k += 40;	break;
			case OPLATE:							k += 35;	break;
			case OHAMMER:							k += 30;	break;
			case OSPLINT:							k += 26;	break;
			case OSWORDofSLASHING:	case OCHAIN:
			case OBATTLEAXE:   		case O2SWORD:	k += 23;	break;
			case OLONGSWORD:   		case OSWORD:
			case ORING:				case OFLAIL:	k += 20;	break;
			case OLANCE: 		case OSTUDLEATHER:	k += 15;	break;
			case OLEATHER:   		case OSPEAR:	k += 8;		break;
			case OORBOFDRAGON:   	case OBELT:		k += 4;		break;
			case OSHIELD:							k += 7;		break;
			case OCHEST:		k += 30 + ivenarg[i];			break;
			default:								k++;
			};
	return(k);
	}

#ifndef MACRORND
	/* macros to generate random numbers   1<=rnd(N)<=N   0<=rund(N)<=N-1 */
rnd(x)
	int x;
	{
	return((random()%x)+1);
	}

rund(x)
	int x;
	{
	return(random()%x);
	}
#endif MACRORND
