/*
 *	monster.c		Larn is copyrighted 1986 by Noah Morgan.
 * $FreeBSD: src/games/larn/monster.c,v 1.6 1999/11/16 11:47:40 marcel Exp $
 *
 *	This file contains the following functions:
 *	----------------------------------------------------------------------------
 *
 *	createmonster(monstno) 		Function to create a monster next to the player
 *		int monstno;
 *
 *	int cgood(x,y,itm,monst)	Function to check location for emptiness
 *		int x,y,itm,monst;
 *
 *	createitem(it,arg) 			Routine to place an item next to the player
 *		int it,arg;
 *
 *	cast() 				Subroutine called by parse to cast a spell for the user
 *
 *	speldamage(x) 		Function to perform spell functions cast by the player
 *		int x;
 *
 *	loseint()			Routine to decrement your int (intelligence) if > 3
 *
 *	isconfuse() 		Routine to check to see if player is confused
 *
 *	nospell(x,monst)	Routine to return 1 if a spell doesn't affect a monster
 *		int x,monst;
 *
 *	fullhit(xx)			Function to return full damage against a monst (aka web)
 *		int xx;
 *
 *	direct(spnum,dam,str,arg)	Routine to direct spell damage 1 square in 1 dir
 *		int spnum,dam,arg;
 *		char *str;
 *
 *	godirect(spnum,dam,str,delay,cshow)		Function to perform missile attacks
 *		int spnum,dam,delay;
 *		char *str,cshow;
 *
 *	ifblind(x,y)	Routine to put "monster" or the monster name into lastmosnt
 *		int x,y;
 *
 *	tdirect(spnum)			Routine to teleport away a monster
 *		int spnum;
 *
 *	omnidirect(sp,dam,str)  Routine to damage all monsters 1 square from player
 *		int sp,dam;
 *		char *str;
 *
 *	dirsub(x,y)			Routine to ask for direction, then modify x,y for it
 *		int *x,*y;
 *
 *	vxy(x,y)		  	Routine to verify/fix (*x,*y) for being within bounds
 *		int *x,*y;
 *
 *	dirpoly(spnum)		Routine to ask for a direction and polymorph a monst
 *		int spnum;
 *
 *	hitmonster(x,y) 	Function to hit a monster at the designated coordinates
 *		int x,y;
 *
 *	hitm(x,y,amt)		Function to just hit a monster at a given coordinates
 *		int x,y,amt;
 *
 *	hitplayer(x,y) 		Function for the monster to hit the player from (x,y)
 *		int x,y;
 *
 *	dropsomething(monst) 	Function to create an object when a monster dies
 *		int monst;
 *
 *	dropgold(amount) 		Function to drop some gold around player
 *		int amount;
 *
 *	something(level) 		Function to create a random item around player
 *		int level;
 *
 *	newobject(lev,i) 		Routine to return a randomly selected new object
 *		int lev,*i;
 *
 *  spattack(atckno,xx,yy) 	Function to process special attacks from monsters
 *  	int atckno,xx,yy;
 *
 *	checkloss(x) 	Routine to subtract hp from user and flag bottomline display
 *		int x;
 *
 *	annihilate()   Routine to annihilate monsters around player, playerx,playery
 *
 *	newsphere(x,y,dir,lifetime)  Function to create a new sphere of annihilation
 *		int x,y,dir,lifetime;
 *
 *	rmsphere(x,y)		Function to delete a sphere of annihilation from list
 *		int x,y;
 *
 *	sphboom(x,y)		Function to perform the effects of a sphere detonation
 *		int x,y;
 *
 *	genmonst()			Function to ask for monster and genocide from game
 *
 */
#include "header.h"

struct isave	/* used for altar reality */
	{
	char type;	/* 0=item,  1=monster */
	char id;	/* item number or monster number */
	short arg;	/* the type of item or hitpoints of monster */
	};

/*
 *	createmonster(monstno) 		Function to create a monster next to the player
 *		int monstno;
 *
 *	Enter with the monster number (1 to MAXMONST+8)
 *	Returns no value.
 */
createmonster(mon)
	int mon;
	{
	int x,y,k,i;
	if (mon<1 || mon>MAXMONST+8)	/* check for monster number out of bounds */
		{
		beep(); lprintf("\ncan't createmonst(%d)\n",(long)mon); nap(3000); return;
		}
	while (monster[mon].genocided && mon<MAXMONST) mon++; /* genocided? */
	for (k=rnd(8), i= -8; i<0; i++,k++)	/* choose direction, then try all */
		{
		if (k>8) k=1;	/* wraparound the diroff arrays */
		x = playerx + diroffx[k];		y = playery + diroffy[k];
		if (cgood(x,y,0,1))	/* if we can create here */
			{
			mitem[x][y] = mon;
			hitp[x][y] = monster[mon].hitpoints;
			stealth[x][y]=know[x][y]=0;
			switch(mon)
				{
				case ROTHE: case POLTERGEIST: case VAMPIRE: stealth[x][y]=1;
				};
			return;
			}
		}
	}

/*
 *	int cgood(x,y,itm,monst)	  Function to check location for emptiness
 *		int x,y,itm,monst;
 *
 *	Routine to return TRUE if a location does not have itm or monst there
 *	returns FALSE (0) otherwise
 *	Enter with itm or monst TRUE or FALSE if checking it
 *	Example:  if itm==TRUE check for no item at this location
 *			  if monst==TRUE check for no monster at this location
 *	This routine will return FALSE if at a wall or the dungeon exit on level 1
 */
int cgood(x,y,itm,monst)
	int x,y;
	int itm,monst;
	{
	if ((y>=0) && (y<=MAXY-1) && (x>=0) && (x<=MAXX-1)) /* within bounds? */
	  if (item[x][y]!=OWALL)	/* can't make anything on walls */
		if (itm==0 || (item[x][y]==0))	/* is it free of items? */
		  if (monst==0 || (mitem[x][y]==0))	/* is it free of monsters? */
		    if ((level!=1) || (x!=33) || (y!=MAXY-1)) /* not exit to level 1 */
			  return(1);
	return(0);
	}

/*
 *	createitem(it,arg) 		Routine to place an item next to the player
 *		int it,arg;
 *
 *	Enter with the item number and its argument (iven[], ivenarg[])
 *	Returns no value, thus we don't know about createitem() failures.
 */
createitem(it,arg)
	int it,arg;
	{
	int x,y,k,i;
	if (it >= MAXOBJ) return;	/* no such object */
	for (k=rnd(8), i= -8; i<0; i++,k++)	/* choose direction, then try all */
		{
		if (k>8) k=1;	/* wraparound the diroff arrays */
		x = playerx + diroffx[k];		y = playery + diroffy[k];
		if (cgood(x,y,1,0))	/* if we can create here */
			{
			item[x][y] = it;  know[x][y]=0;  iarg[x][y]=arg;  return;
			}
		}
	}

/*
 *	cast() 		Subroutine called by parse to cast a spell for the user
 *
 *	No arguments and no return value.
 */
static char eys[] = "\nEnter your spell: ";
cast()
	{
	int i,j,a,b,d;
	cursors();
	if (c[SPELLS]<=0) {	lprcat("\nYou don't have any spells!");	return;	}
	lprcat(eys);		--c[SPELLS];
	while ((a=getchar())=='D')
		{ seemagic(-1); cursors();  lprcat(eys); }
	if (a=='\33') goto over; /*	to escape casting a spell	*/
	if ((b=getchar())=='\33') goto over; /*	to escape casting a spell	*/
	if ((d=getchar())=='\33')
		{ over: lprcat(aborted); c[SPELLS]++; return; } /*	to escape casting a spell	*/
#ifdef EXTRA
	c[SPELLSCAST]++;
#endif
	for (lprc('\n'),j= -1,i=0; i<SPNUM; i++) /*seq search for his spell, hash?*/
		if ((spelcode[i][0]==a) && (spelcode[i][1]==b) && (spelcode[i][2]==d))
			if (spelknow[i])
				{  speldamage(i);  j = 1;  i=SPNUM; }

	if (j == -1) lprcat("  Nothing Happened ");
	bottomline();
	}

static int dirsub();

/*
 *	speldamage(x) 		Function to perform spell functions cast by the player
 *		int x;
 *
 *	Enter with the spell number, returns no value.
 *	Please insure that there are 2 spaces before all messages here
 */
speldamage(x)
	int x;
	{
	int i,j,clev;
	int xl,xh,yl,yh;
	char *p,*kn,*pm;
	if (x>=SPNUM) return;	/* no such spell */
	if (c[TIMESTOP])  { lprcat("  It didn't seem to work"); return; }  /* not if time stopped */
	clev = c[LEVEL];
	if ((rnd(23)==7) || (rnd(18) > c[INTELLIGENCE]))
		{ lprcat("  It didn't work!");  return; }
	if (clev*3+2 < x) { lprcat("  Nothing happens.  You seem inexperienced at this"); return; }

	switch(x)
		{
/* ----- LEVEL 1 SPELLS ----- */

		case 0:	if (c[PROTECTIONTIME]==0)	c[MOREDEFENSES]+=2; /* protection field +2 */
				c[PROTECTIONTIME] += 250;   return;

		case 1: i = rnd(((clev+1)<<1)) + clev + 3;
				godirect(x,i,(clev>=2)?"  Your missiles hit the %s":"  Your missile hit the %s",100,'+'); /* magic missile */

				return;

		case 2:	if (c[DEXCOUNT]==0)	c[DEXTERITY]+=3; /*	dexterity	*/
				c[DEXCOUNT] += 400;  	return;

		case 3: i=rnd(3)+1;
				p="  While the %s slept, you smashed it %d times";
			ws:	direct(x,fullhit(i),p,i); /*	sleep	*/	return;

		case 4:	/*	charm monster	*/	c[CHARMCOUNT] += c[CHARISMA]<<1;	return;

		case 5:	godirect(x,rnd(10)+15+clev,"  The sound damages the %s",70,'@'); /*	sonic spear */
				return;

/* ----- LEVEL 2 SPELLS ----- */

		case 6: i=rnd(3)+2;	p="  While the %s is entangled, you hit %d times";
				goto ws; /* web */

		case 7:	if (c[STRCOUNT]==0) c[STREXTRA]+=3;	/*	strength	*/
				c[STRCOUNT] += 150+rnd(100);    return;

		case 8:	yl = playery-5;     /* enlightenment */
				yh = playery+6;   xl = playerx-15;   xh = playerx+16;
				vxy(&xl,&yl);   vxy(&xh,&yh); /* check bounds */
				for (i=yl; i<=yh; i++) /* enlightenment	*/
					for (j=xl; j<=xh; j++)	know[j][i]=1;
				draws(xl,xh+1,yl,yh+1);	return;

		case 9:	raisehp(20+(clev<<1));  return;  /* healing */

		case 10:	c[BLINDCOUNT]=0;	return;	/* cure blindness	*/

		case 11:	createmonster(makemonst(level+1)+8);  return;

		case 12:	if (rnd(11)+7 <= c[WISDOM]) direct(x,rnd(20)+20+clev,"  The %s believed!",0);
					else lprcat("  It didn't believe the illusions!");
					return;

		case 13:	/* if he has the amulet of invisibility then add more time */
					for (j=i=0; i<26; i++)
						if (iven[i]==OAMULET) j+= 1+ivenarg[i];
					c[INVISIBILITY] += (j<<7)+12;   return;

/* ----- LEVEL 3 SPELLS ----- */

		case 14:	godirect(x,rnd(25+clev)+25+clev,"  The fireball hits the %s",40,'*'); return; /*	fireball */

		case 15:	godirect(x,rnd(25)+20+clev,"  Your cone of cold strikes the %s",60,'O');	/*	cold */
					return;

		case 16:	dirpoly(x);  return;	/*	polymorph */

		case 17:	c[CANCELLATION]+= 5+clev;	return;	/*	cancellation	*/

		case 18:	c[HASTESELF]+= 7+clev;  return;  /*	haste self	*/

		case 19:	omnidirect(x,30+rnd(10),"  The %s gasps for air");	/* cloud kill */
					return;

		case 20:	xh = min(playerx+1,MAXX-2);		yh = min(playery+1,MAXY-2);
					for (i=max(playerx-1,1); i<=xh; i++) /* vaporize rock */
					  for (j=max(playery-1,1); j<=yh; j++)
						{
						kn = &know[i][j];    pm = &mitem[i][j];
						switch(*(p= &item[i][j]))
						  {
						  case OWALL: if (level < MAXLEVEL+MAXVLEVEL-1)
											*p = *kn = 0;
										break;

						  case OSTATUE: if (c[HARDGAME]<3)
											 {
											 *p=OBOOK; iarg[i][j]=level;  *kn=0;
											 }
										break;

						  case OTHRONE: *pm=GNOMEKING;  *kn=0;  *p= OTHRONE2;
										hitp[i][j]=monster[GNOMEKING].hitpoints; break;

						  case OALTAR:	*pm=DEMONPRINCE;  *kn=0;
										hitp[i][j]=monster[DEMONPRINCE].hitpoints; break;
						  };
						switch(*pm)
							{
							case XORN:	ifblind(i,j);  hitm(i,j,200); break; /* Xorn takes damage from vpr */
							}
						}
					return;

/* ----- LEVEL 4 SPELLS ----- */

		case 21:	direct(x,100+clev,"  The %s shrivels up",0); /* dehydration */
					return;

		case 22:	godirect(x,rnd(25)+20+(clev<<1),"  A lightning bolt hits the %s",1,'~');	/*	lightning */
					return;

		case 23:	i=min(c[HP]-1,c[HPMAX]/2);	/* drain life */
					direct(x,i+i,"",0);	c[HP] -= i;  	return;

		case 24:	if (c[GLOBE]==0) c[MOREDEFENSES] += 10;
					c[GLOBE] += 200;  loseint();  /* globe of invulnerability */
					return;

		case 25:	omnidirect(x,32+clev,"  The %s struggles for air in your flood!"); /* flood */
					return;

		case 26:	if (rnd(151)==63) { beep(); lprcat("\nYour heart stopped!\n"); nap(4000);  died(270); return; }
					if (c[WISDOM]>rnd(10)+10) direct(x,2000,"  The %s's heart stopped",0); /* finger of death */
					else lprcat("  It didn't work"); return;

/* ----- LEVEL 5 SPELLS ----- */

		case 27:	c[SCAREMONST] += rnd(10)+clev;  return;  /* scare monster */

		case 28:	c[HOLDMONST] += rnd(10)+clev;  return;  /* hold monster */

		case 29:	c[TIMESTOP] += rnd(20)+(clev<<1);  return;  /* time stop */

		case 30:	tdirect(x);  return;  /* teleport away */

		case 31:	omnidirect(x,35+rnd(10)+clev,"  The %s cringes from the flame"); /* magic fire */
					return;

/* ----- LEVEL 6 SPELLS ----- */

		case 32:	if ((rnd(23)==5) && (wizard==0)) /* sphere of annihilation */
						{
						beep(); lprcat("\nYou have been enveloped by the zone of nothingness!\n");
						nap(4000);  died(258); return;
						}
					xl=playerx; yl=playery;
					loseint();
					i=dirsub(&xl,&yl); /* get direction of sphere */
					newsphere(xl,yl,i,rnd(20)+11);	/* make a sphere */
					return;

		case 33:	genmonst();  spelknow[33]=0;  /* genocide */
					loseint();
					return;

		case 34:	/* summon demon */
					if (rnd(100) > 30) { direct(x,150,"  The demon strikes at the %s",0);  return; }
					if (rnd(100) > 15) { lprcat("  Nothing seems to have happened");  return; }
					lprcat("  The demon turned on you and vanished!"); beep();
					i=rnd(40)+30;  lastnum=277;
					losehp(i); /* must say killed by a demon */ return;

		case 35:	/* walk through walls */
					c[WTW] += rnd(10)+5;	return;

		case 36:	/* alter reality */
					{
					struct isave *save;	/* pointer to item save structure */
					int sc;	sc=0;	/* # items saved */
					save = (struct isave *)malloc(sizeof(struct isave)*MAXX*MAXY*2);
					for (j=0; j<MAXY; j++)
						for (i=0; i<MAXX; i++) /* save all items and monsters */
							{
							xl = item[i][j];
							if (xl && xl!=OWALL && xl!=OANNIHILATION)
								{
								save[sc].type=0;  save[sc].id=item[i][j];
								save[sc++].arg=iarg[i][j];
								}
							if (mitem[i][j])
								{
								save[sc].type=1;  save[sc].id=mitem[i][j];
								save[sc++].arg=hitp[i][j];
								}
							item[i][j]=OWALL;   mitem[i][j]=0;
							if (wizard) know[i][j]=1; else know[i][j]=0;
							}
					eat(1,1);	if (level==1) item[33][MAXY-1]=0;
					for (j=rnd(MAXY-2), i=1; i<MAXX-1; i++) item[i][j]=0;
					while (sc>0) /* put objects back in level */
						{
						--sc;
						if (save[sc].type == 0)
							{
							int trys;
							for (trys=100, i=j=1; --trys>0 && item[i][j]; i=rnd(MAXX-1), j=rnd(MAXY-1));
							if (trys) { item[i][j]=save[sc].id; iarg[i][j]=save[sc].arg; }
							}
						else
							{ /* put monsters back in */
							int trys;
							for (trys=100, i=j=1; --trys>0 && (item[i][j]==OWALL || mitem[i][j]); i=rnd(MAXX-1), j=rnd(MAXY-1));
							if (trys) { mitem[i][j]=save[sc].id; hitp[i][j]=save[sc].arg; }
							}
						}
					loseint();
					draws(0,MAXX,0,MAXY);  if (wizard==0) spelknow[36]=0;
					free((char*)save);	 positionplayer();  return;
					}

		case 37:	/* permanence */ adjtime(-99999L);  spelknow[37]=0; /* forget */
					loseint();
					return;

		default:	lprintf("  spell %d not available!",(long)x); beep();  return;
		};
	}

/*
 *	loseint()		Routine to subtract 1 from your int (intelligence) if > 3
 *
 *	No arguments and no return value
 */
loseint()
	{
	if (--c[INTELLIGENCE]<3)  c[INTELLIGENCE]=3;
	}

/*
 *	isconfuse() 		Routine to check to see if player is confused
 *
 *	This routine prints out a message saying "You can't aim your magic!"
 *	returns 0 if not confused, non-zero (time remaining confused) if confused
 */
isconfuse()
	{
	if (c[CONFUSE]) { lprcat(" You can't aim your magic!"); beep(); }
	return(c[CONFUSE]);
	}

/*
 *	nospell(x,monst)	Routine to return 1 if a spell doesn't affect a monster
 *		int x,monst;
 *
 *	Subroutine to return 1 if the spell can't affect the monster
 *	  otherwise returns 0
 *	Enter with the spell number in x, and the monster number in monst.
 */
nospell(x,monst)
	int x,monst;
	{
	int tmp;
	if (x>=SPNUM || monst>=MAXMONST+8 || monst<0 || x<0) return(0);	/* bad spell or monst */
	if ((tmp=spelweird[monst-1][x])==0) return(0);
	cursors();  lprc('\n');  lprintf(spelmes[tmp],monster[monst].name);  return(1);
	}

/*
 *	fullhit(xx)		Function to return full damage against a monster (aka web)
 *		int xx;
 *
 *	Function to return hp damage to monster due to a number of full hits
 *	Enter with the number of full hits being done
 */
fullhit(xx)
	int xx;
	{
	int i;
	if (xx<0 || xx>20) return(0);	/* fullhits are out of range */
	if (c[LANCEDEATH]) return(10000);	/* lance of death */
	i = xx * ((c[WCLASS]>>1)+c[STRENGTH]+c[STREXTRA]-c[HARDGAME]-12+c[MOREDAM]);
	return( (i>=1) ? i : xx );
	}

/*
 *	direct(spnum,dam,str,arg)	Routine to direct spell damage 1 square in 1 dir
 *		int spnum,dam,arg;
 *		char *str;
 *
 *	Routine to ask for a direction to a spell and then hit the monster
 *	Enter with the spell number in spnum, the damage to be done in dam,
 *	  lprintf format string in str, and lprintf's argument in arg.
 *	Returns no value.
 */
direct(spnum,dam,str,arg)
	int spnum,dam,arg;
	char *str;
	{
	int x,y;
	int m;
	if (spnum<0 || spnum>=SPNUM || str==0) return; /* bad arguments */
	if (isconfuse()) return;
	dirsub(&x,&y);
	m = mitem[x][y];
	if (item[x][y]==OMIRROR)
		{
		if (spnum==3) /* sleep */
			{
			lprcat("You fall asleep! "); beep();
		fool:
			arg += 2;
			while (arg-- > 0) { parse2(); nap(1000); }
			return;
			}
		else if (spnum==6) /* web */
			{
			lprcat("You get stuck in your own web! "); beep();
			goto fool;
			}
		else
			{
			lastnum=278;
			lprintf(str,"spell caster (thats you)",(long)arg);
			beep(); losehp(dam); return;
			}
		}
	if (m==0)
		{	lprcat("  There wasn't anything there!");	return;  }
	ifblind(x,y);
	if (nospell(spnum,m)) { lasthx=x;  lasthy=y; return; }
	lprintf(str,lastmonst,(long)arg);       hitm(x,y,dam);
	}

/*
 *	godirect(spnum,dam,str,delay,cshow)		Function to perform missile attacks
 *		int spnum,dam,delay;
 *		char *str,cshow;
 *
 *	Function to hit in a direction from a missile weapon and have it keep
 *	on going in that direction until its power is exhausted
 *	Enter with the spell number in spnum, the power of the weapon in hp,
 *	  lprintf format string in str, the # of milliseconds to delay between
 *	  locations in delay, and the character to represent the weapon in cshow.
 *	Returns no value.
 */
godirect(spnum,dam,str,delay,cshow)
	int spnum,dam,delay;
	char *str,cshow;
	{
	char *p;
	int x,y,m;
	int dx,dy;
	if (spnum<0 || spnum>=SPNUM || str==0 || delay<0) return; /* bad args */
	if (isconfuse()) return;
	dirsub(&dx,&dy);	x=dx;	y=dy;
	dx = x-playerx;		dy = y-playery;		x = playerx;	y = playery;
	while (dam>0)
		{
		x += dx;    y += dy;
	    if ((x > MAXX-1) || (y > MAXY-1) || (x < 0) || (y < 0))
			{
			dam=0;	break;  /* out of bounds */
			}
		if ((x==playerx) && (y==playery)) /* if energy hits player */
			{
			cursors(); lprcat("\nYou are hit my your own magic!"); beep();
			lastnum=278;  losehp(dam);  return;
			}
		if (c[BLINDCOUNT]==0) /* if not blind show effect */
			{
			cursor(x+1,y+1); lprc(cshow); nap(delay); show1cell(x,y);
			}
		if ((m=mitem[x][y]))	/* is there a monster there? */
			{
			ifblind(x,y);
			if (nospell(spnum,m)) { lasthx=x;  lasthy=y; return; }
			cursors(); lprc('\n');
			lprintf(str,lastmonst);		dam -= hitm(x,y,dam);
			show1cell(x,y);  nap(1000);		x -= dx;	y -= dy;
			}
		else switch (*(p= &item[x][y]))
			{
			case OWALL:	cursors(); lprc('\n'); lprintf(str,"wall");
						if (dam>=50+c[HARDGAME]) /* enough damage? */
						 if (level<MAXLEVEL+MAXVLEVEL-1) /* not on V3 */
						  if ((x<MAXX-1) && (y<MAXY-1) && (x) && (y))
							{
							lprcat("  The wall crumbles");
					god3:	*p=0;
					god:	know[x][y]=0;
							show1cell(x,y);
							}
				god2:	dam = 0;	break;

			case OCLOSEDDOOR:	cursors(); lprc('\n'); lprintf(str,"door");
						if (dam>=40)
							{
							lprcat("  The door is blasted apart");
							goto god3;
							}
						goto god2;

			case OSTATUE:	cursors(); lprc('\n'); lprintf(str,"statue");
						if (c[HARDGAME]<3)
						  if (dam>44)
							{
							lprcat("  The statue crumbles");
							*p=OBOOK; iarg[x][y]=level;
							goto god;
							}
						goto god2;

			case OTHRONE:	cursors(); lprc('\n'); lprintf(str,"throne");
					if (dam>39)
						{
						mitem[x][y]=GNOMEKING; hitp[x][y]=monster[GNOMEKING].hitpoints;
						*p = OTHRONE2;
						goto god;
						}
					goto god2;

			case OMIRROR:	dx *= -1;	dy *= -1;	break;
			};
		dam -= 3 + (c[HARDGAME]>>1);
		}
	}

/*
 *	ifblind(x,y)	Routine to put "monster" or the monster name into lastmosnt
 *		int x,y;
 *
 *	Subroutine to copy the word "monster" into lastmonst if the player is blind
 *	Enter with the coordinates (x,y) of the monster
 *	Returns no value.
 */
ifblind(x,y)
	int x,y;
	{
	char *p;
	vxy(&x,&y);	/* verify correct x,y coordinates */
	if (c[BLINDCOUNT]) { lastnum=279;  p="monster"; }
		else { lastnum=mitem[x][y];  p=monster[lastnum].name; }
	strcpy(lastmonst,p);
	}

/*
 *	tdirect(spnum)		Routine to teleport away a monster
 *		int spnum;
 *
 *	Routine to ask for a direction to a spell and then teleport away monster
 *	Enter with the spell number that wants to teleport away
 *	Returns no value.
 */
tdirect(spnum)
	int spnum;
	{
	int x,y;
	int m;
	if (spnum<0 || spnum>=SPNUM) return; /* bad args */
	if (isconfuse()) return;
	dirsub(&x,&y);
	if ((m=mitem[x][y])==0)
		{	lprcat("  There wasn't anything there!");	return;  }
	ifblind(x,y);
	if (nospell(spnum,m)) { lasthx=x;  lasthy=y; return; }
	fillmonst(m);  mitem[x][y]=know[x][y]=0;
	}

/*
 *	omnidirect(sp,dam,str)   Routine to damage all monsters 1 square from player
 *		int sp,dam;
 *		char *str;
 *
 *	Routine to cast a spell and then hit the monster in all directions
 *	Enter with the spell number in sp, the damage done to wach square in dam,
 *	  and the lprintf string to identify the spell in str.
 *	Returns no value.
 */
omnidirect(spnum,dam,str)
	int spnum,dam;
	char *str;
	{
	int x,y,m;
	if (spnum<0 || spnum>=SPNUM || str==0) return; /* bad args */
	for (x=playerx-1; x<playerx+2; x++)
		for (y=playery-1; y<playery+2; y++)
			{
			if (m=mitem[x][y])
				{
				if (nospell(spnum,m) == 0)
					{
					ifblind(x,y);
					cursors(); lprc('\n'); lprintf(str,lastmonst);
					hitm(x,y,dam);  nap(800);
					}
				else  { lasthx=x;  lasthy=y; }
				}
			}
	}

/*
 *	static dirsub(x,y)		Routine to ask for direction, then modify x,y for it
 *		int *x,*y;
 *
 *	Function to ask for a direction and modify an x,y for that direction
 *	Enter with the origination coordinates in (x,y).
 *	Returns index into diroffx[] (0-8).
 */
static int
dirsub(x,y)
	int *x,*y;
	{
	int i;
	lprcat("\nIn What Direction? ");
	for (i=0; ; )
		switch(getchar())
			{
			case 'b':	i++;
			case 'n':	i++;
			case 'y':	i++;
			case 'u':	i++;
			case 'h':	i++;
			case 'k':	i++;
			case 'l':	i++;
			case 'j':	i++;		goto out;
			};
out:
	*x = playerx+diroffx[i];		*y = playery+diroffy[i];
	vxy(x,y);  return(i);
	}

/*
 *	vxy(x,y)	   Routine to verify/fix coordinates for being within bounds
 *		int *x,*y;
 *
 *	Function to verify x & y are within the bounds for a level
 *	If *x or *y is not within the absolute bounds for a level, fix them so that
 *	  they are on the level.
 *	Returns TRUE if it was out of bounds, and the *x & *y in the calling
 *	routine are affected.
 */
vxy(x,y)
	int *x,*y;
	{
	int flag=0;
	if (*x<0) { *x=0; flag++; }
	if (*y<0) { *y=0; flag++; }
	if (*x>=MAXX) { *x=MAXX-1; flag++; }
	if (*y>=MAXY) { *y=MAXY-1; flag++; }
	return(flag);
	}

/*
 *	dirpoly(spnum)		Routine to ask for a direction and polymorph a monst
 *		int spnum;
 *
 *	Subroutine to polymorph a monster and ask for the direction its in
 *	Enter with the spell number in spmun.
 *	Returns no value.
 */
dirpoly(spnum)
	int spnum;
	{
	int x,y,m;
	if (spnum<0 || spnum>=SPNUM) return; /* bad args */
	if (isconfuse()) return;	/* if he is confused, he can't aim his magic */
	dirsub(&x,&y);
	if (mitem[x][y]==0)
		{	lprcat("  There wasn't anything there!");	return;  }
	ifblind(x,y);
	if (nospell(spnum,mitem[x][y])) { lasthx=x;  lasthy=y; return; }
	while ( monster[m = mitem[x][y] = rnd(MAXMONST+7)].genocided );
	hitp[x][y] = monster[m].hitpoints;
	show1cell(x,y);  /* show the new monster */
	}

/*
 *	hitmonster(x,y) 	Function to hit a monster at the designated coordinates
 *		int x,y;
 *
 *	This routine is used for a bash & slash type attack on a monster
 *	Enter with the coordinates of the monster in (x,y).
 *	Returns no value.
 */
hitmonster(x,y)
	int x,y;
	{
	int tmp,monst,damag,flag;
	if (c[TIMESTOP])  return;  /* not if time stopped */
	vxy(&x,&y);	/* verify coordinates are within range */
	if ((monst = mitem[x][y]) == 0) return;
	hit3flag=1;  ifblind(x,y);
	tmp = monster[monst].armorclass + c[LEVEL] + c[DEXTERITY] + c[WCLASS]/4 - 12;
	cursors();
	if ((rnd(20) < tmp-c[HARDGAME]) || (rnd(71) < 5)) /* need at least random chance to hit */
		{
		lprcat("\nYou hit");  flag=1;
		damag = fullhit(1);
		if (damag<9999) damag=rnd(damag)+1;
		}
	else
		{
		lprcat("\nYou missed");  flag=0;
		}
	lprcat(" the "); lprcat(lastmonst);
	if (flag)	/* if the monster was hit */
	  if ((monst==RUSTMONSTER) || (monst==DISENCHANTRESS) || (monst==CUBE))
		if (c[WIELD]>0)
		  if (ivenarg[c[WIELD]] > -10)
			{
			lprintf("\nYour weapon is dulled by the %s",lastmonst); beep();
			--ivenarg[c[WIELD]];
			}
	if (flag)  hitm(x,y,damag);
	if (monst == VAMPIRE) if (hitp[x][y]<25)  { mitem[x][y]=BAT; know[x][y]=0; }
	}

/*
 *	hitm(x,y,amt)		Function to just hit a monster at a given coordinates
 *		int x,y,amt;
 *
 *	Returns the number of hitpoints the monster absorbed
 *	This routine is used to specifically damage a monster at a location (x,y)
 *	Called by hitmonster(x,y)
 */
hitm(x,y,amt)
	int x,y;
	int amt;
	{
	int monst;
	int hpoints,amt2;
	vxy(&x,&y);	/* verify coordinates are within range */
	amt2 = amt;		/* save initial damage so we can return it */
	monst = mitem[x][y];
	if (c[HALFDAM]) amt >>= 1;	/* if half damage curse adjust damage points */
	if (amt<=0) amt2 = amt = 1;
	lasthx=x;  lasthy=y;
	stealth[x][y]=1;	/* make sure hitting monst breaks stealth condition */
	c[HOLDMONST]=0;	/* hit a monster breaks hold monster spell	*/
	switch(monst) /* if a dragon and orb(s) of dragon slaying	*/
		{
		case WHITEDRAGON:		case REDDRAGON:			case GREENDRAGON:
		case BRONZEDRAGON:		case PLATINUMDRAGON:	case SILVERDRAGON:
			amt *= 1+(c[SLAYING]<<1);	break;
		}
/* invincible monster fix is here */
	if (hitp[x][y] > monster[monst].hitpoints)
		hitp[x][y] = monster[monst].hitpoints;
	if ((hpoints = hitp[x][y]) <= amt)
		{
#ifdef EXTRA
		c[MONSTKILLED]++;
#endif
		lprintf("\nThe %s died!",lastmonst);
		raiseexperience((long)monster[monst].experience);
		amt = monster[monst].gold;  if (amt>0) dropgold(rnd(amt)+amt);
		dropsomething(monst);	disappear(x,y);	bottomline();
		return(hpoints);
		}
	hitp[x][y] = hpoints-amt;	return(amt2);
	}

/*
 *	hitplayer(x,y) 		Function for the monster to hit the player from (x,y)
 *		int x,y;
 *
 *	Function for the monster to hit the player with monster at location x,y
 *	Returns nothing of value.
 */
hitplayer(x,y)
	int x,y;
	{
	int dam,tmp,mster,bias;
	vxy(&x,&y);	/* verify coordinates are within range */
	lastnum = mster = mitem[x][y];
/*	spirit naga's and poltergeist's do nothing if scarab of negate spirit	*/
	if (c[NEGATESPIRIT] || c[SPIRITPRO])  if ((mster ==POLTERGEIST) || (mster ==SPIRITNAGA))  return;
/*	if undead and cube of undead control	*/
	if (c[CUBEofUNDEAD] || c[UNDEADPRO]) if ((mster ==VAMPIRE) || (mster ==WRAITH) || (mster ==ZOMBIE)) return;
	if ((know[x][y]&1) == 0)
		{
		know[x][y]=1; show1cell(x,y);
		}
	bias = (c[HARDGAME]) + 1;
	hitflag = hit2flag = hit3flag = 1;
	yrepcount=0;
	cursors();	ifblind(x,y);
	if (c[INVISIBILITY]) if (rnd(33)<20)
		{
		lprintf("\nThe %s misses wildly",lastmonst);	return;
		}
	if (c[CHARMCOUNT]) if (rnd(30)+5*monster[mster].level-c[CHARISMA]<30)
		{
		lprintf("\nThe %s is awestruck at your magnificence!",lastmonst);
		return;
		}
	if (mster==BAT) dam=1;
	else
		{
		dam = monster[mster].damage;
		dam += rnd((int)((dam<1)?1:dam)) + monster[mster].level;
		}
	tmp = 0;
	if (monster[mster].attack>0)
	  if (((dam + bias + 8) > c[AC]) || (rnd((int)((c[AC]>0)?c[AC]:1))==1))
		{ if (spattack(monster[mster].attack,x,y)) { flushall(); return; }
		  tmp = 1;  bias -= 2; cursors(); }
	if (((dam + bias) > c[AC]) || (rnd((int)((c[AC]>0)?c[AC]:1))==1))
		{
		lprintf("\n  The %s hit you ",lastmonst);	tmp = 1;
		if ((dam -= c[AC]) < 0) dam=0;
		if (dam > 0) { losehp(dam); bottomhp(); flushall(); }
		}
	if (tmp == 0)  lprintf("\n  The %s missed ",lastmonst);
	}

/*
 *	dropsomething(monst) 	Function to create an object when a monster dies
 *		int monst;
 *
 *	Function to create an object near the player when certain monsters are killed
 *	Enter with the monster number
 *	Returns nothing of value.
 */
dropsomething(monst)
	int monst;
	{
	switch(monst)
		{
		case ORC:			  case NYMPH:	   case ELF:	  case TROGLODYTE:
		case TROLL:			  case ROTHE:	   case VIOLETFUNGI:
		case PLATINUMDRAGON:  case GNOMEKING:  case REDDRAGON:
			something(level); return;

		case LEPRECHAUN: if (rnd(101)>=75) creategem();
						 if (rnd(5)==1) dropsomething(LEPRECHAUN);   return;
		}
	}

/*
 *	dropgold(amount) 	Function to drop some gold around player
 *		int amount;
 *
 *	Enter with the number of gold pieces to drop
 *	Returns nothing of value.
 */
dropgold(amount)
	int amount;
	{
	if (amount > 250) createitem(OMAXGOLD,amount/100);  else  createitem(OGOLDPILE,amount);
	}

/*
 *	something(level) 	Function to create a random item around player
 *		int level;
 *
 *	Function to create an item from a designed probability around player
 *	Enter with the cave level on which something is to be dropped
 *	Returns nothing of value.
 */
something(level)
	int level;
	{
	int j;
	int i;
	if (level<0 || level>MAXLEVEL+MAXVLEVEL) return;	/* correct level? */
	if (rnd(101)<8) something(level); /* possibly more than one item */
	j = newobject(level,&i);		createitem(j,i);
	}

/*
 *	newobject(lev,i) 	Routine to return a randomly selected new object
 *		int lev,*i;
 *
 *	Routine to return a randomly selected object to be created
 *	Returns the object number created, and sets *i for its argument
 *	Enter with the cave level and a pointer to the items arg
 */
static char nobjtab[] = { 0, OSCROLL,  OSCROLL,  OSCROLL,  OSCROLL, OPOTION,
	OPOTION, OPOTION, OPOTION, OGOLDPILE, OGOLDPILE, OGOLDPILE, OGOLDPILE,
	OBOOK, OBOOK, OBOOK, OBOOK, ODAGGER, ODAGGER, ODAGGER, OLEATHER, OLEATHER,
	OLEATHER, OREGENRING, OPROTRING, OENERGYRING, ODEXRING, OSTRRING, OSPEAR,
	OBELT, ORING, OSTUDLEATHER, OSHIELD, OFLAIL, OCHAIN, O2SWORD, OPLATE,
	OLONGSWORD };

newobject(lev,i)
	int lev,*i;
	{
	int tmp=32,j;
	if (level<0 || level>MAXLEVEL+MAXVLEVEL) return(0);	/* correct level? */
	if (lev>6) tmp=37; else if (lev>4) tmp=35;
	j = nobjtab[tmp=rnd(tmp)];	/* the object type */
	switch(tmp)
		{
		case 1: case 2: case 3: case 4:	*i=newscroll();	break;
		case 5: case 6: case 7: case 8:	*i=newpotion();	break;
		case 9: case 10: case 11: case 12: *i=rnd((lev+1)*10)+lev*10+10; break;
		case 13: case 14: case 15: case 16:	*i=lev;	break;
		case 17: case 18: case 19: if (!(*i=newdagger()))  return(0);  break;
		case 20: case 21: case 22: if (!(*i=newleather()))  return(0);  break;
		case 23: case 32: case 35: *i=rund(lev/3+1); break;
		case 24: case 26: *i=rnd(lev/4+1);   break;
		case 25: *i=rund(lev/4+1); break;
		case 27: *i=rnd(lev/2+1);   break;
		case 30: case 33: *i=rund(lev/2+1);   break;
		case 28: *i=rund(lev/3+1); if (*i==0) return(0); break;
		case 29: case 31: *i=rund(lev/2+1); if (*i==0) return(0); break;
		case 34: *i=newchain();   	break;
		case 36: *i=newplate();   	break;
		case 37: *i=newsword();		break;
		}
	return(j);
	}

/*
 *  spattack(atckno,xx,yy) 	Function to process special attacks from monsters
 *  	int atckno,xx,yy;
 *
 *	Enter with the special attack number, and the coordinates (xx,yy)
 *		of the monster that is special attacking
 *	Returns 1 if must do a show1cell(xx,yy) upon return, 0 otherwise
 *
 * atckno   monster     effect
 * ---------------------------------------------------
 *	0	none
 *	1	rust monster	eat armor
 *	2	hell hound		breathe light fire
 *	3	dragon			breathe fire
 *	4	giant centipede	weakening sing
 *	5	white dragon	cold breath
 *	6	wraith			drain level
 *	7	waterlord		water gusher
 *	8	leprechaun		steal gold
 *	9	disenchantress	disenchant weapon or armor
 *	10	ice lizard		hits with barbed tail
 *	11	umber hulk		confusion
 *	12	spirit naga		cast spells	taken from special attacks
 *	13	platinum dragon	psionics
 *	14	nymph			steal objects
 *	15	bugbear			bite
 *	16	osequip			bite
 *
 *	char rustarm[ARMORTYPES][2];
 *	special array for maximum rust damage to armor from rustmonster
 *	format is: { armor type , minimum attribute
 */
#define ARMORTYPES 6
static char rustarm[ARMORTYPES][2] = { OSTUDLEATHER,-2,	ORING,-4, OCHAIN,-5,
	OSPLINT,-6,		OPLATE,-8,		OPLATEARMOR,-9  };
static char spsel[] = { 1, 2, 3, 5, 6, 8, 9, 11, 13, 14 };
spattack(x,xx,yy)
	int x,xx,yy;
	{
	int i,j=0,k,m;
	char *p=0;
	if (c[CANCELLATION]) return(0);
	vxy(&xx,&yy);	/* verify x & y coordinates */
	switch(x)
		{
		case 1:	/* rust your armor, j=1 when rusting has occurred */
				m = k = c[WEAR];
				if ((i=c[SHIELD]) != -1)
				  {
					if (--ivenarg[i] < -1) ivenarg[i]= -1; else j=1;
				  }
				if ((j==0) && (k != -1))
				  {
				  m = iven[k];
				  for (i=0; i<ARMORTYPES; i++)
					if (m == rustarm[i][0]) /* find his armor in table */
						{
						if (--ivenarg[k]< rustarm[i][1])
							ivenarg[k]= rustarm[i][1]; else j=1;
						break;
						}
				  }
				if (j==0)	/* if rusting did not occur */
				  switch(m)
					{
					case OLEATHER:	p = "\nThe %s hit you -- Your lucky you have leather on";
									break;
				    case OSSPLATE:	p = "\nThe %s hit you -- Your fortunate to have stainless steel armor!";
									break;
					}
				else  { beep(); p = "\nThe %s hit you -- your armor feels weaker"; }
				break;

		case 2:		i = rnd(15)+8-c[AC];
			spout:	p="\nThe %s breathes fire at you!";
					if (c[FIRERESISTANCE])
					  p="\nThe %s's flame doesn't phase you!";
					else
			spout2: if (p) { lprintf(p,lastmonst); beep(); }
					checkloss(i);
					return(0);

		case 3:		i = rnd(20)+25-c[AC];  goto spout;

		case 4:	if (c[STRENGTH]>3)
					{
					p="\nThe %s stung you!  You feel weaker"; beep();
					--c[STRENGTH];
					}
				else p="\nThe %s stung you!";
				break;

		case 5:		p="\nThe %s blasts you with his cold breath";
					i = rnd(15)+18-c[AC];  goto spout2;

		case 6:		lprintf("\nThe %s drains you of your life energy!",lastmonst);
					loselevel();  beep();  return(0);

		case 7:		p="\nThe %s got you with a gusher!";
					i = rnd(15)+25-c[AC];  goto spout2;

		case 8:		if (c[NOTHEFT]) return(0); /* he has a device of no theft */
					if (c[GOLD])
						{
						p="\nThe %s hit you -- Your purse feels lighter";
						if (c[GOLD]>32767)  c[GOLD]>>=1;
							else c[GOLD] -= rnd((int)(1+(c[GOLD]>>1)));
						if (c[GOLD] < 0) c[GOLD]=0;
						}
					else  p="\nThe %s couldn't find any gold to steal";
					lprintf(p,lastmonst); disappear(xx,yy); beep();
					bottomgold();  return(1);

		case 9:	for(j=50; ; )	/* disenchant */
					{
					i=rund(26);  m=iven[i]; /* randomly select item */
					if (m>0 && ivenarg[i]>0 && m!=OSCROLL && m!=OPOTION)
						{
						if ((ivenarg[i] -= 3)<0) ivenarg[i]=0;
						lprintf("\nThe %s hits you -- you feel a sense of loss",lastmonst);
						srcount=0; beep(); show3(i);  bottomline();  return(0);
						}
					if (--j<=0)
						{
						p="\nThe %s nearly misses"; break;
						}
					break;
					}
				break;

		case 10:   p="\nThe %s hit you with his barbed tail";
				   i = rnd(25)-c[AC];  goto spout2;

		case 11:	p="\nThe %s has confused you"; beep();
					c[CONFUSE]+= 10+rnd(10);		break;

		case 12:	/*	performs any number of other special attacks	*/
					return(spattack(spsel[rund(10)],xx,yy));

		case 13:	p="\nThe %s flattens you with his psionics!";
					i = rnd(15)+30-c[AC];  goto spout2;

		case 14:	if (c[NOTHEFT]) return(0); /* he has device of no theft */
					if (emptyhanded()==1)
					  {
					  p="\nThe %s couldn't find anything to steal";
					  break;
					  }
					lprintf("\nThe %s picks your pocket and takes:",lastmonst);
					beep();
					if (stealsomething()==0) lprcat("  nothing"); disappear(xx,yy);
					bottomline();  return(1);

		case 15:	i= rnd(10)+ 5-c[AC];
			spout3:	p="\nThe %s bit you!";
					goto spout2;

		case 16:	i= rnd(15)+10-c[AC];  goto spout3;
		};
	if (p) { lprintf(p,lastmonst); bottomline(); }
	return(0);
	}

/*
 *	checkloss(x) 	Routine to subtract hp from user and flag bottomline display
 *		int x;
 *
 *	Routine to subtract hitpoints from the user and flag the bottomline display
 *	Enter with the number of hit points to lose
 *	Note: if x > c[HP] this routine could kill the player!
 */
checkloss(x)
	int x;
	{
	if (x>0) { losehp(x);  bottomhp(); }
	}

/*
 *	annihilate() 	Routine to annihilate all monsters around player (playerx,playery)
 *
 *	Gives player experience, but no dropped objects
 *	Returns the experience gained from all monsters killed
 */
annihilate()
	{
	int i,j;
	long k;
	char *p;
	for (k=0, i=playerx-1; i<=playerx+1; i++)
	  for (j=playery-1; j<=playery+1; j++)
		if (!vxy(&i,&j)) /* if not out of bounds */
			{
			if (*(p= &mitem[i][j]))	/* if a monster there */
				if (*p<DEMONLORD+2)
					{
					k += monster[*p].experience;	*p=know[i][j]=0;
					}
				else
					{
					lprintf("\nThe %s barely escapes being annihilated!",monster[*p].name);
					hitp[i][j] = (hitp[i][j]>>1) + 1; /* lose half hit points*/
					}
			}
	if (k>0)
		{
		lprcat("\nYou hear loud screams of agony!");	raiseexperience((long)k);
		}
	return(k);
	}

/*
 *	newsphere(x,y,dir,lifetime)  Function to create a new sphere of annihilation
 *		int x,y,dir,lifetime;
 *
 *	Enter with the coordinates of the sphere in x,y
 *	  the direction (0-8 diroffx format) in dir, and the lifespan of the
 *	  sphere in lifetime (in turns)
 *	Returns the number of spheres currently in existence
 */
newsphere(x,y,dir,life)
	int x,y,dir,life;
	{
	int m;
	struct sphere *sp;
	if (((sp=(struct sphere *)malloc(sizeof(struct sphere)))) == 0)
		return(c[SPHCAST]);	/* can't malloc, therefore failure */
	if (dir>=9) dir=0;	/* no movement if direction not found */
	if (level==0) vxy(&x,&y);	/* don't go out of bounds */
	else
		{
		if (x<1) x=1;  if (x>=MAXX-1) x=MAXX-2;
		if (y<1) y=1;  if (y>=MAXY-1) y=MAXY-2;
		}
	if ((m=mitem[x][y]) >= DEMONLORD+4)	/* demons dispel spheres */
		{
		know[x][y]=1; show1cell(x,y);	/* show the demon (ha ha) */
		cursors(); lprintf("\nThe %s dispels the sphere!",monster[m].name);
		beep(); rmsphere(x,y);	/* remove any spheres that are here */
		return(c[SPHCAST]);
		}
	if (m==DISENCHANTRESS) /* disenchantress cancels spheres */
		{
		cursors(); lprintf("\nThe %s causes cancellation of the sphere!",monster[m].name); beep();
boom:	sphboom(x,y);	/* blow up stuff around sphere */
		rmsphere(x,y);	/* remove any spheres that are here */
		return(c[SPHCAST]);
		}
	if (c[CANCELLATION]) /* cancellation cancels spheres */
		{
		cursors(); lprcat("\nAs the cancellation takes effect, you hear a great earth shaking blast!"); beep();
		goto boom;
		}
	if (item[x][y]==OANNIHILATION) /* collision of spheres detonates spheres */
		{
		cursors(); lprcat("\nTwo spheres of annihilation collide! You hear a great earth shaking blast!"); beep();
		rmsphere(x,y);
		goto boom;
		}
	if (playerx==x && playery==y) /* collision of sphere and player! */
		{
		cursors();
		lprcat("\nYou have been enveloped by the zone of nothingness!\n");
		beep(); rmsphere(x,y);	/* remove any spheres that are here */
		nap(4000);  died(258);
		}
	item[x][y]=OANNIHILATION;  mitem[x][y]=0;  know[x][y]=1;
	show1cell(x,y);	/* show the new sphere */
	sp->x=x;  sp->y=y;  sp->lev=level;  sp->dir=dir;  sp->lifetime=life;  sp->p=0;
	if (spheres==0) spheres=sp;	/* if first node in the sphere list */
	else	/* add sphere to beginning of linked list */
		{
		sp->p = spheres;	spheres = sp;
		}
	return(++c[SPHCAST]);	/* one more sphere in the world */
	}

/*
 *	rmsphere(x,y)		Function to delete a sphere of annihilation from list
 *		int x,y;
 *
 *	Enter with the coordinates of the sphere (on current level)
 *	Returns the number of spheres currently in existence
 */
rmsphere(x,y)
	int x,y;
	{
	struct sphere *sp,*sp2=0;
	for (sp=spheres; sp; sp2=sp,sp=sp->p)
	  if (level==sp->lev)	/* is sphere on this level? */
	    if ((x==sp->x) && (y==sp->y))	/* locate sphere at this location */
			{
			item[x][y]=mitem[x][y]=0;  know[x][y]=1;
			show1cell(x,y);	/* show the now missing sphere */
			--c[SPHCAST];
			if (sp==spheres) { sp2=sp; spheres=sp->p; free((char*)sp2); }
			else
				{ sp2->p = sp->p;  free((char*)sp); }
			break;
			}
	return(c[SPHCAST]);	/* return number of spheres in the world */
	}

/*
 *	sphboom(x,y)	Function to perform the effects of a sphere detonation
 *		int x,y;
 *
 *	Enter with the coordinates of the blast, Returns no value
 */
sphboom(x,y)
	int x,y;
	{
	int i,j;
	if (c[HOLDMONST]) c[HOLDMONST]=1;
	if (c[CANCELLATION]) c[CANCELLATION]=1;
	for (j=max(1,x-2); j<min(x+3,MAXX-1); j++)
	  for (i=max(1,y-2); i<min(y+3,MAXY-1); i++)
		{
		item[j][i]=mitem[j][i]=0;
		show1cell(j,i);
		if (playerx==j && playery==i)
			{
			cursors(); beep();
			lprcat("\nYou were too close to the sphere!");
			nap(3000);
			died(283); /* player killed in explosion */
			}
		}
	}

/*
 *	genmonst()		Function to ask for monster and genocide from game
 *
 *	This is done by setting a flag in the monster[] structure
 */
genmonst()
	{
	int i,j;
	cursors();  lprcat("\nGenocide what monster? ");
	for (i=0; (!isalpha(i)) && (i!=' '); i=getchar());
	lprc(i);
	for (j=0; j<MAXMONST; j++)	/* search for the monster type */
		if (monstnamelist[j]==i)	/* have we found it? */
			{
			monster[j].genocided=1;	/* genocided from game */
			lprintf("  There will be no more %s's",monster[j].name);
			/* now wipe out monsters on this level */
			newcavelevel(level); draws(0,MAXX,0,MAXY); bot_linex();
			return;
			}
	lprcat("  You sense failure!");
	}

