/*	display.c		Larn is copyrighted 1986 by Noah Morgan. */
/* $FreeBSD: src/games/larn/display.c,v 1.4 1999/11/16 02:57:21 billf Exp $ */
#include "header.h"
#define makecode(_a,_b,_c) (((_a)<<16) + ((_b)<<8) + (_c))

static int minx,maxx,miny,maxy,k,m;
static char bot1f=0,bot2f=0,bot3f=0;
char always=0;
/*
	bottomline()

	now for the bottom line of the display
 */
bottomline()
	{	recalc();	bot1f=1;	}
bottomhp()
	{	bot2f=1;	}
bottomspell()
	{	bot3f=1;	}
bottomdo()
	{
	if (bot1f) { bot3f=bot1f=bot2f=0; bot_linex(); return; }
	if (bot2f) { bot2f=0; bot_hpx(); }
	if (bot3f) { bot3f=0; bot_spellx(); }
	}

static void botsub();

bot_linex()
	{
	int i;
	if (cbak[SPELLS] <= -50 || (always))
		{
		cursor( 1,18);
		if (c[SPELLMAX]>99)  lprintf("Spells:%3d(%3d)",(long)c[SPELLS],(long)c[SPELLMAX]);
						else lprintf("Spells:%3d(%2d) ",(long)c[SPELLS],(long)c[SPELLMAX]);
		lprintf(" AC: %-3d  WC: %-3d  Level",(long)c[AC],(long)c[WCLASS]);
		if (c[LEVEL]>99) lprintf("%3d",(long)c[LEVEL]);
					else lprintf(" %-2d",(long)c[LEVEL]);
		lprintf(" Exp: %-9d %s\n",(long)c[EXPERIENCE],class[c[LEVEL]-1]);
		lprintf("HP: %3d(%3d) STR=%-2d INT=%-2d ",
			(long)c[HP],(long)c[HPMAX],(long)(c[STRENGTH]+c[STREXTRA]),(long)c[INTELLIGENCE]);
		lprintf("WIS=%-2d CON=%-2d DEX=%-2d CHA=%-2d LV:",
			(long)c[WISDOM],(long)c[CONSTITUTION],(long)c[DEXTERITY],(long)c[CHARISMA]);

		if ((level==0) || (wizard))  c[TELEFLAG]=0;
		if (c[TELEFLAG])  lprcat(" ?");  else  lprcat(levelname[level]);
		lprintf("  Gold: %-6d",(long)c[GOLD]);
		always=1;  botside();
		c[TMP] = c[STRENGTH]+c[STREXTRA];
		for (i=0; i<100; i++) cbak[i]=c[i];
		return;
		}

	botsub(makecode(SPELLS,8,18),"%3d");
	if (c[SPELLMAX]>99)  botsub(makecode(SPELLMAX,12,18),"%3d)");
					else botsub(makecode(SPELLMAX,12,18),"%2d) ");
	botsub(makecode(HP,5,19),"%3d");
	botsub(makecode(HPMAX,9,19),"%3d");
	botsub(makecode(AC,21,18),"%-3d");
	botsub(makecode(WCLASS,30,18),"%-3d");
	botsub(makecode(EXPERIENCE,49,18),"%-9d");
	if (c[LEVEL] != cbak[LEVEL])
		{ cursor(59,18);	lprcat(class[c[LEVEL]-1]);  }
	if (c[LEVEL]>99) botsub(makecode(LEVEL,40,18),"%3d");
				else botsub(makecode(LEVEL,40,18)," %-2d");
	c[TMP] = c[STRENGTH]+c[STREXTRA];	botsub(makecode(TMP,18,19),"%-2d");
	botsub(makecode(INTELLIGENCE,25,19),"%-2d");
	botsub(makecode(WISDOM,32,19),"%-2d");
	botsub(makecode(CONSTITUTION,39,19),"%-2d");
	botsub(makecode(DEXTERITY,46,19),"%-2d");
	botsub(makecode(CHARISMA,53,19),"%-2d");
	if ((level != cbak[CAVELEVEL]) || (c[TELEFLAG] != cbak[TELEFLAG]))
		{
		if ((level==0) || (wizard))  c[TELEFLAG]=0;
		cbak[TELEFLAG] = c[TELEFLAG];
		cbak[CAVELEVEL] = level;	cursor(59,19);
		if (c[TELEFLAG])  lprcat(" ?");  else  lprcat(levelname[level]);
		}
	botsub(makecode(GOLD,69,19),"%-6d");
	botside();
	}

/*
	special subroutine to update only the gold number on the bottomlines
	called from ogold()
 */
bottomgold()
	{
	botsub(makecode(GOLD,69,19),"%-6d");
/*	botsub(GOLD,"%-6d",69,19); */
	}

/*
	special routine to update hp and level fields on bottom lines
	called in monster.c hitplayer() and spattack()
 */
bot_hpx()
	{
	if (c[EXPERIENCE] != cbak[EXPERIENCE])
		{
		recalc();	 bot_linex();
		}
	else botsub(makecode(HP,5,19),"%3d");
	}

/*
	special routine to update number of spells called from regen()
 */
bot_spellx()
	{
	botsub(makecode(SPELLS,9,18),"%2d");
	}

/*
	common subroutine for a more economical bottomline()
 */
static struct bot_side_def
	{
	int typ;
	char *string;
	}
	bot_data[] =
	{
	STEALTH,"stealth",		UNDEADPRO,"undead pro",		SPIRITPRO,"spirit pro",
	CHARMCOUNT,"Charm",		TIMESTOP,"Time Stop",		HOLDMONST,"Hold Monst",
	GIANTSTR,"Giant Str",	FIRERESISTANCE,"Fire Resit", DEXCOUNT,"Dexterity",
	STRCOUNT,"Strength",	SCAREMONST,"Scare",			HASTESELF,"Haste Self",
	CANCELLATION,"Cancel",	INVISIBILITY,"Invisible",	ALTPRO,"Protect 3",
	PROTECTIONTIME,"Protect 2", WTW,"Wall-Walk"
	};

botside()
	{
	int i,idx;
	for (i=0; i<17; i++)
		{
		idx = bot_data[i].typ;
		if ((always) || (c[idx] != cbak[idx]))
		   {
		   if ((always) || (cbak[idx] == 0))
				{ if (c[idx]) { cursor(70,i+1); lprcat(bot_data[i].string); } }  else
		   if (c[idx]==0)     { cursor(70,i+1); lprcat("          "); }
		   cbak[idx]=c[idx];
		   }
		}
	always=0;
	}

static void
botsub(idx,str)
	int idx;
	char *str;
	{
	int x,y;
	y = idx & 0xff;		x = (idx>>8) & 0xff;	  idx >>= 16;
	if (c[idx] != cbak[idx])
		{ cbak[idx]=c[idx];  cursor(x,y);  lprintf(str,(long)c[idx]); }
	}

/*
 *	subroutine to draw only a section of the screen
 *	only the top section of the screen is updated.  If entire lines are being
 *	drawn, then they will be cleared first.
 */
int d_xmin=0,d_xmax=MAXX,d_ymin=0,d_ymax=MAXY;	/* for limited screen drawing */
draws(xmin,xmax,ymin,ymax)
	int xmin,xmax,ymin,ymax;
	{
	int i,idx;
	if (xmin==0 && xmax==MAXX) /* clear section of screen as needed */
		{
		if (ymin==0) cl_up(79,ymax);
		else for (i=ymin; i<ymin; i++)  cl_line(1,i+1);
		xmin = -1;
		}
	d_xmin=xmin;	d_xmax=xmax;	d_ymin=ymin;	d_ymax=ymax;	/* for limited screen drawing */
	drawscreen();
	if (xmin<=0 && xmax==MAXX) /* draw stuff on right side of screen as needed*/
		{
		for (i=ymin; i<ymax; i++)
			{
			idx = bot_data[i].typ;
			if (c[idx])
				{
				cursor(70,i+1); lprcat(bot_data[i].string);
				}
			cbak[idx]=c[idx];
			}
		}
	}

/*
	drawscreen()

	subroutine to redraw the whole screen as the player knows it
 */
char screen[MAXX][MAXY],d_flag;	/* template for the screen */
drawscreen()
	{
	int i,j,k;
	int lastx,lasty;  /* variables used to optimize the object printing */
	if (d_xmin==0 && d_xmax==MAXX && d_ymin==0 && d_ymax==MAXY)
		{
		d_flag=1;  clear(); /* clear the screen */
		}
	else
		{
		d_flag=0;  cursor(1,1);
		}
	if (d_xmin<0)
		d_xmin=0; /* d_xmin=-1 means display all without bottomline */

	for (i=d_ymin; i<d_ymax; i++)
	  for (j=d_xmin; j<d_xmax; j++)
		if (know[j][i]==0)  screen[j][i] = ' ';  else
		if (k=mitem[j][i])  screen[j][i] = monstnamelist[k];  else
		if ((k=item[j][i])==OWALL) screen[j][i] = '#';
		else screen[j][i] = ' ';

	for (i=d_ymin; i<d_ymax; i++)
		{
		j=d_xmin;  while ((screen[j][i]==' ') && (j<d_xmax)) j++;
		/* was m=0 */
		if (j >= d_xmax)  m=d_xmin; /* don't search backwards if blank line */
		else
			{	/* search backwards for end of line */
			m=d_xmax-1;  while ((screen[m][i]==' ') && (m>d_xmin)) --m;
			if (j<=m)  cursor(j+1,i+1);  else continue;
			}
		while (j <= m)
			{
			if (j <= m-3)
				{
				for (k=j; k<=j+3; k++) if (screen[k][i] != ' ') k=1000;
				if (k < 1000)
					{ while(screen[j][i]==' ' && j<=m) j++;  cursor(j+1,i+1); }
				}
			lprc(screen[j++][i]);
			}
		}
	setbold();		/* print out only bold objects now */

	for (lastx=lasty=127, i=d_ymin; i<d_ymax; i++)
		for (j=d_xmin; j<d_xmax; j++)
			{
			if (k=item[j][i])
				if (k != OWALL)
					if ((know[j][i]) && (mitem[j][i]==0))
						if (objnamelist[k]!=' ')
							{
							if (lasty!=i+1 || lastx!=j)
								cursor(lastx=j+1,lasty=i+1); else lastx++;
							lprc(objnamelist[k]);
							}
			}

	resetbold();  if (d_flag)  { always=1; botside(); always=1; bot_linex(); }
	oldx=99;
	d_xmin = 0 , d_xmax = MAXX , d_ymin = 0 , d_ymax = MAXY; /* for limited screen drawing */
	}

/*
	showcell(x,y)

	subroutine to display a cell location on the screen
 */
showcell(x,y)
	int x,y;
	{
	int i,j,k,m;
	if (c[BLINDCOUNT])  return;	/* see nothing if blind		*/
	if (c[AWARENESS]) { minx = x-3;	maxx = x+3;	miny = y-3;	maxy = y+3; }
			else	  { minx = x-1;	maxx = x+1;	miny = y-1;	maxy = y+1; }

	if (minx < 0) minx=0;		if (maxx > MAXX-1) maxx = MAXX-1;
	if (miny < 0) miny=0;		if (maxy > MAXY-1) maxy = MAXY-1;

	for (j=miny; j<=maxy; j++)
	  for (m=minx; m<=maxx; m++)
		if (know[m][j]==0)
			{
			cursor(m+1,j+1);
			x=maxx;  while (know[x][j]) --x;
			for (i=m; i<=x; i++)
				{
				if ((k=mitem[i][j]) != 0)  lprc(monstnamelist[k]);
				else switch(k=item[i][j])
					{
					case OWALL:  case 0: case OIVTELETRAP:  case OTRAPARROWIV:
					case OIVDARTRAP: case OIVTRAPDOOR:
						lprc(objnamelist[k]);	break;

					default: setbold(); lprc(objnamelist[k]); resetbold();
					};
				know[i][j]=1;
				}
			m = maxx;
			}
	}

/*
	this routine shows only the spot that is given it.  the spaces around
	these coordinated are not shown
	used in godirect() in monster.c for missile weapons display
 */
show1cell(x,y)
	int x,y;
	{
	if (c[BLINDCOUNT])  return;	/* see nothing if blind		*/
	cursor(x+1,y+1);
	if ((k=mitem[x][y]) != 0)  lprc(monstnamelist[k]);
		else switch(k=item[x][y])
			{
			case OWALL:  case 0:  case OIVTELETRAP:  case OTRAPARROWIV:
			case OIVDARTRAP: case OIVTRAPDOOR:
				lprc(objnamelist[k]);	break;

			default: setbold(); lprc(objnamelist[k]); resetbold();
			};
	know[x][y]|=1;	/* we end up knowing about it */
	}

/*
	showplayer()

	subroutine to show where the player is on the screen
	cursor values start from 1 up
 */
showplayer()
	{
	cursor(playerx+1,playery+1);
	oldx=playerx;  oldy=playery;
	}

/*
	moveplayer(dir)

	subroutine to move the player from one room to another
	returns 0 if can't move in that direction or hit a monster or on an object
	else returns 1
	nomove is set to 1 to stop the next move (inadvertent monsters hitting
	players when walking into walls) if player walks off screen or into wall
 */
short diroffx[] = { 0,  0, 1,  0, -1,  1, -1, 1, -1 };
short diroffy[] = { 0,  1, 0, -1,  0, -1, -1, 1,  1 };
moveplayer(dir)
	int dir;			/*	from = present room #  direction = [1-north]
							[2-east] [3-south] [4-west] [5-northeast]
							[6-northwest] [7-southeast] [8-southwest]
						if direction=0, don't move--just show where he is */
	{
	int k,m,i,j;
	if (c[CONFUSE]) if (c[LEVEL]<rnd(30)) dir=rund(9); /*if confused any dir*/
	k = playerx + diroffx[dir];		m = playery + diroffy[dir];
	if (k<0 || k>=MAXX || m<0 || m>=MAXY) { nomove=1; return(yrepcount = 0); }
	i = item[k][m];			j = mitem[k][m];
	if (i==OWALL && c[WTW]==0) { nomove=1;  return(yrepcount = 0); }		/*	hit a wall	*/
	if (k==33 && m==MAXY-1 && level==1)
		{
		newcavelevel(0); for (k=0; k<MAXX; k++) for (m=0; m<MAXY; m++)
		if (item[k][m]==OENTRANCE)
		  { playerx=k; playery=m; positionplayer();  drawscreen(); return(0); }
		}
	if (j>0)     { hitmonster(k,m);	return(yrepcount = 0); } /* hit a monster*/
	lastpx = playerx;			lastpy = playery;
	playerx = k;		playery = m;
	if (i && i!=OTRAPARROWIV && i!=OIVTELETRAP && i!=OIVDARTRAP && i!=OIVTRAPDOOR) return(yrepcount = 0);  else return(1);
	}

/*
 *	function to show what magic items have been discovered thus far
 *	enter with -1 for just spells, anything else will give scrolls & potions
 */
static int lincount,count;
seemagic(arg)
	int arg;
	{
	int i,number;
	count = lincount = 0;  nosignal=1;

	if (arg== -1) /* if display spells while casting one */
		{
		for (number=i=0; i<SPNUM; i++) if (spelknow[i]) number++;
		number = (number+2)/3 + 4;	/* # lines needed to display */
		cl_up(79,number);  cursor(1,1);
		}
	else
		{
		resetscroll();  clear();
		}

	lprcat("The magic spells you have discovered thus far:\n\n");
	for (i=0; i<SPNUM; i++)
		if (spelknow[i])
			{ lprintf("%s %-20s ",spelcode[i],spelname[i]);  seepage(); }

	if (arg== -1)
		{
		seepage();  more();	 nosignal=0;
		draws(0,MAXX,0,number);  return;
		}

	lincount += 3;  if (count!=0) { count=2;  seepage(); }

	lprcat("\nThe magic scrolls you have found to date are:\n\n");
	count=0;
	for (i=0; i<MAXSCROLL; i++)
		if (scrollname[i][0])
		  if (scrollname[i][1]!=' ')
			{ lprintf("%-26s",&scrollname[i][1]);  seepage(); }

	lincount += 3;  if (count!=0) { count=2;  seepage(); }

	lprcat("\nThe magic potions you have found to date are:\n\n");
	count=0;
	for (i=0; i<MAXPOTION; i++)
		if (potionname[i][0])
		  if (potionname[i][1]!=' ')
			{ lprintf("%-26s",&potionname[i][1]);  seepage(); }

	if (lincount!=0) more();	nosignal=0;  setscroll();	drawscreen();
	}

/*
 *	subroutine to paginate the seemagic function
 */
seepage()
	{
	if (++count==3)
		{
		lincount++;	count=0;	lprc('\n');
		if (lincount>17) {	lincount=0;  more();  clear();	}
		}
	}
