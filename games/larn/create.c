/*	create.c		Larn is copyrighted 1986 by Noah Morgan. */
/* $FreeBSD: src/games/larn/create.c,v 1.4 1999/11/16 02:57:20 billf Exp $ */
#include "header.h"
extern char spelknow[],larnlevels[];
extern char beenhere[],wizard,level;
extern short oldx,oldy;
/*
	makeplayer()

	subroutine to create the player and the players attributes
	this is called at the beginning of a game and at no other time
 */
makeplayer()
	{
	int i;
	scbr();  clear();
	c[HPMAX]=c[HP]=10;		/*	start player off with 15 hit points	*/
	c[LEVEL]=1;				/*	player starts at level one			*/
	c[SPELLMAX]=c[SPELLS]=1;	/*	total # spells starts off as 3	*/
	c[REGENCOUNTER]=16;		c[ECOUNTER]=96;	/*start regeneration correctly*/
	c[SHIELD] = c[WEAR] = c[WIELD] = -1;
	for (i=0; i<26; i++)  iven[i]=0;
	spelknow[0]=spelknow[1]=1; /*he knows protection, magic missile*/
	if (c[HARDGAME]<=0)
		{
		iven[0]=OLEATHER; iven[1]=ODAGGER;
		ivenarg[1]=ivenarg[0]=c[WEAR]=0;  c[WIELD]=1;
		}
	playerx=rnd(MAXX-2);	playery=rnd(MAXY-2);
	oldx=0;			oldy=25;
	gtime=0;			/*	time clock starts at zero	*/
	cbak[SPELLS] = -50;
	for (i=0; i<6; i++)  c[i]=12; /* make the attributes, ie str, int, etc.	*/
	recalc();
	}

/*
	newcavelevel(level)
	int level;

	function to enter a new level.  This routine must be called anytime the
	player changes levels.  If that level is unknown it will be created.
	A new set of monsters will be created for a new level, and existing
	levels will get a few more monsters.
	Note that it is here we remove genocided monsters from the present level.
 */
newcavelevel(x)
	int x;
	{
	int i,j;
	if (beenhere[level]) savelevel();	/* put the level back into storage	*/
	level = x;				/* get the new level and put in working storage */
	if (beenhere[x]==0) for (i=0; i<MAXY; i++) for (j=0; j<MAXX; j++) know[j][i]=mitem[j][i]=0;
		else { getlevel(); sethp(0);  goto chgn; }
	makemaze(x);	makeobject(x);	beenhere[x]=1;  sethp(1);

#if WIZID
	if (wizard || x==0)
#else
	if (x==0)
#endif

		for (j=0; j<MAXY; j++)
			for (i=0; i<MAXX; i++)
				know[i][j]=1;
chgn: checkgen();	/* wipe out any genocided monsters */
	}

/*
	makemaze(level)
	int level;

	subroutine to make the caverns for a given level.  only walls are made.
 */
static int mx,mxl,mxh,my,myl,myh,tmp2;
 makemaze(k)
	int k;
	{
	int i,j,tmp;
	int z;
	if (k > 1 && (rnd(17)<=4 || k==MAXLEVEL-1 || k==MAXLEVEL+MAXVLEVEL-1))
		{
		if (cannedlevel(k));	return;		/* read maze from data file */
		}
	if (k==0)  tmp=0;  else tmp=OWALL;
	for (i=0; i<MAXY; i++)	for (j=0; j<MAXX; j++)	item[j][i]=tmp;
	if (k==0) return;		eat(1,1);
	if (k==1) item[33][MAXY-1]=0;	/* exit from dungeon */

/*	now for open spaces -- not on level 10	*/
	if (k != MAXLEVEL-1)
		{
		tmp2 = rnd(3)+3;
		for (tmp=0; tmp<tmp2; tmp++)
			{
			my = rnd(11)+2;   myl = my - rnd(2);  myh = my + rnd(2);
			if (k < MAXLEVEL)
				{
				mx = rnd(44)+5;  mxl = mx - rnd(4);  mxh = mx + rnd(12)+3;
				z=0;
				}
		  	else
				{
				mx = rnd(60)+3;  mxl = mx - rnd(2);  mxh = mx + rnd(2);
				z = makemonst(k);
				}
			for (i=mxl; i<mxh; i++)		for (j=myl; j<myh; j++)
				{  item[i][j]=0;
				   if ((mitem[i][j]=z)) hitp[i][j]=monster[z].hitpoints;
				}
			}
		}
	if (k!=MAXLEVEL-1) { my=rnd(MAXY-2);  for (i=1; i<MAXX-1; i++)	item[i][my] = 0; }
	if (k>1)  treasureroom(k);
	}

/*
	function to eat away a filled in maze
 */
eat(xx,yy)
	int xx,yy;
	{
	int dir,try;
	dir = rnd(4);	try=2;
	while (try)
		{
		switch(dir)
			{
			case 1:	if (xx <= 2) break;		/*	west	*/
					if ((item[xx-1][yy]!=OWALL) || (item[xx-2][yy]!=OWALL))	break;
					item[xx-1][yy] = item[xx-2][yy] = 0;
					eat(xx-2,yy);	break;

			case 2:	if (xx >= MAXX-3) break;	/*	east	*/
					if ((item[xx+1][yy]!=OWALL) || (item[xx+2][yy]!=OWALL))	break;
					item[xx+1][yy] = item[xx+2][yy] = 0;
					eat(xx+2,yy);	break;

			case 3:	if (yy <= 2) break;		/*	south	*/
					if ((item[xx][yy-1]!=OWALL) || (item[xx][yy-2]!=OWALL))	break;
					item[xx][yy-1] = item[xx][yy-2] = 0;
					eat(xx,yy-2);	break;

			case 4:	if (yy >= MAXY-3 ) break;	/*	north	*/
					if ((item[xx][yy+1]!=OWALL) || (item[xx][yy+2]!=OWALL))	break;
					item[xx][yy+1] = item[xx][yy+2] = 0;
					eat(xx,yy+2);	break;
			};
		if (++dir > 4)	{ dir=1;  --try; }
		}
	}

/*
 *	function to read in a maze from a data file
 *
 *	Format of maze data file:  1st character = # of mazes in file (ascii digit)
 *				For each maze: 18 lines (1st 17 used) 67 characters per line
 *
 *	Special characters in maze data file:
 *
 *		#	wall			D	door			.	random monster
 *		~	eye of larn		!	cure dianthroritis
 *		-	random object
 */
cannedlevel(k)
	int k;
	{
	char *row,*lgetl();
	int i,j;
	int it,arg,mit,marg;
	if (lopen(larnlevels)<0)
		{
		write(1,"Can't open the maze data file\n",30);	 died(-282); return(0);
		}
	i=lgetc();  if (i<='0') { died(-282); return(0); }
	for (i=18*rund(i-'0'); i>0; i--)	lgetl();   /* advance to desired maze */
	for (i=0; i<MAXY; i++)
		{
		row = lgetl();
		for (j=0; j<MAXX; j++)
			{
			it = mit = arg = marg = 0;
			switch(*row++)
				{
				case '#': it = OWALL;								break;
				case 'D': it = OCLOSEDDOOR;  	arg = rnd(30);		break;
				case '~': if (k!=MAXLEVEL-1) break;
						  it = OLARNEYE;
						  mit = rund(8)+DEMONLORD;
						  marg = monster[mit].hitpoints;			break;
				case '!': if (k!=MAXLEVEL+MAXVLEVEL-1)  break;
						  it = OPOTION;			arg = 21;
						  mit = DEMONLORD+7;
						  marg = monster[mit].hitpoints;			break;
				case '.': if (k<MAXLEVEL)  break;
						  mit = makemonst(k+1);
						  marg = monster[mit].hitpoints;			break;
				case '-': it = newobject(k+1,&arg);					break;
				};
			item[j][i] = it;		iarg[j][i] = arg;
			mitem[j][i] = mit;		hitp[j][i] = marg;

#if WIZID
			know[j][i] = (wizard) ? 1 : 0;
#else
			know[j][i] = 0;
#endif
			}
		}
	lrclose();
	return(1);
	}

/*
	function to make a treasure room on a level
	level 10's treasure room has the eye in it and demon lords
	level V3 has potion of cure dianthroritis and demon prince
 */
treasureroom(lv)
	int lv;
	{
	int tx,ty,xsize,ysize;

	for (tx=1+rnd(10);  tx<MAXX-10;  tx+=10)
	  if ( (lv==MAXLEVEL-1) || (lv==MAXLEVEL+MAXVLEVEL-1) || rnd(13)==2)
		{
		xsize = rnd(6)+3;  	    ysize = rnd(3)+3;
		ty = rnd(MAXY-9)+1;  /* upper left corner of room */
		if (lv==MAXLEVEL-1 || lv==MAXLEVEL+MAXVLEVEL-1)
			troom(lv,xsize,ysize,tx=tx+rnd(MAXX-24),ty,rnd(3)+6);
			else troom(lv,xsize,ysize,tx,ty,rnd(9));
		}
	}

/*
 *	subroutine to create a treasure room of any size at a given location
 *	room is filled with objects and monsters
 *	the coordinate given is that of the upper left corner of the room
 */
troom(lv,xsize,ysize,tx,ty,glyph)
	int lv,xsize,ysize,tx,ty,glyph;
	{
	int i,j;
	int tp1,tp2;
	for (j=ty-1; j<=ty+ysize; j++)
		for (i=tx-1; i<=tx+xsize; i++)			/* clear out space for room */
			item[i][j]=0;
	for (j=ty; j<ty+ysize; j++)
		for (i=tx; i<tx+xsize; i++)				/* now put in the walls */
			{
			item[i][j]=OWALL; mitem[i][j]=0;
			}
	for (j=ty+1; j<ty+ysize-1; j++)
		for (i=tx+1; i<tx+xsize-1; i++)			/* now clear out interior */
			item[i][j]=0;

	switch(rnd(2))		/* locate the door on the treasure room */
		{
		case 1:	item[i=tx+rund(xsize)][j=ty+(ysize-1)*rund(2)]=OCLOSEDDOOR;
				iarg[i][j] = glyph;		/* on horizontal walls */
				break;
		case 2: item[i=tx+(xsize-1)*rund(2)][j=ty+rund(ysize)]=OCLOSEDDOOR;
				iarg[i][j] = glyph;		/* on vertical walls */
				break;
		};

	tp1=playerx;  tp2=playery;  playery=ty+(ysize>>1);
	if (c[HARDGAME]<2)
		for (playerx=tx+1; playerx<=tx+xsize-2; playerx+=2)
			for (i=0, j=rnd(6); i<=j; i++)
				{ something(lv+2); createmonster(makemonst(lv+1)); }
	else
		for (playerx=tx+1; playerx<=tx+xsize-2; playerx+=2)
			for (i=0, j=rnd(4); i<=j; i++)
				{ something(lv+2); createmonster(makemonst(lv+3)); }

	playerx=tp1;  playery=tp2;
	}

static void fillroom();

/*
	***********
	MAKE_OBJECT
	***********
	subroutine to create the objects in the maze for the given level
 */
makeobject(j)
	int j;
	{
	int i;
	if (j==0)
		{
		fillroom(OENTRANCE,0);		/*	entrance to dungeon			*/
		fillroom(ODNDSTORE,0);		/*	the DND STORE				*/
		fillroom(OSCHOOL,0);		/*	college of Larn				*/
		fillroom(OBANK,0);			/*	1st national bank of larn 	*/
		fillroom(OVOLDOWN,0);		/*	volcano shaft to temple 	*/
		fillroom(OHOME,0);			/*	the players home & family 	*/
		fillroom(OTRADEPOST,0);		/*  the trading post			*/
		fillroom(OLRS,0);			/*  the larn revenue service 	*/
		return;
		}

	if (j==MAXLEVEL) fillroom(OVOLUP,0); /* volcano shaft up from the temple */

/*	make the fixed objects in the maze STAIRS	*/
	if ((j>0) && (j != MAXLEVEL-1) && (j != MAXLEVEL+MAXVLEVEL-1))
		fillroom(OSTAIRSDOWN,0);
	if ((j > 1) && (j != MAXLEVEL))			fillroom(OSTAIRSUP,0);

/*	make the random objects in the maze		*/

	fillmroom(rund(3),OBOOK,j);				fillmroom(rund(3),OALTAR,0);
	fillmroom(rund(3),OSTATUE,0);			fillmroom(rund(3),OPIT,0);
	fillmroom(rund(3),OFOUNTAIN,0);			fillmroom( rnd(3)-2,OIVTELETRAP,0);
	fillmroom(rund(2),OTHRONE,0);			fillmroom(rund(2),OMIRROR,0);
	fillmroom(rund(2),OTRAPARROWIV,0);		fillmroom( rnd(3)-2,OIVDARTRAP,0);
	fillmroom(rund(3),OCOOKIE,0);
	if (j==1) fillmroom(1,OCHEST,j);
		else fillmroom(rund(2),OCHEST,j);
	if ((j != MAXLEVEL-1) && (j != MAXLEVEL+MAXVLEVEL-1))
		fillmroom(rund(2),OIVTRAPDOOR,0);
	if (j<=10)
		{
		fillmroom((rund(2)),ODIAMOND,rnd(10*j+1)+10);
		fillmroom(rund(2),ORUBY,rnd(6*j+1)+6);
		fillmroom(rund(2),OEMERALD,rnd(4*j+1)+4);
		fillmroom(rund(2),OSAPPHIRE,rnd(3*j+1)+2);
		}
	for (i=0; i<rnd(4)+3; i++)
		fillroom(OPOTION,newpotion());	/*	make a POTION	*/
	for (i=0; i<rnd(5)+3; i++)
		fillroom(OSCROLL,newscroll());	/*	make a SCROLL	*/
	for (i=0; i<rnd(12)+11; i++)
		fillroom(OGOLDPILE,12*rnd(j+1)+(j<<3)+10); /* make GOLD	*/
	if (j==5)	fillroom(OBANK2,0);				/*	branch office of the bank */
	froom(2,ORING,0);				/* a ring mail 			*/
	froom(1,OSTUDLEATHER,0);		/* a studded leather	*/
	froom(3,OSPLINT,0);				/* a splint mail		*/
	froom(5,OSHIELD,rund(3));		/* a shield				*/
	froom(2,OBATTLEAXE,rund(3));	/* a battle axe			*/
	froom(5,OLONGSWORD,rund(3));	/* a long sword			*/
	froom(5,OFLAIL,rund(3));		/* a flail				*/
	froom(4,OREGENRING,rund(3));	/* ring of regeneration */
	froom(1,OPROTRING,rund(3));	/* ring of protection	*/
	froom(2,OSTRRING,4);   		/* ring of strength + 4 */
	froom(7,OSPEAR,rnd(5));		/* a spear				*/
	froom(3,OORBOFDRAGON,0);	/* orb of dragon slaying*/
	froom(4,OSPIRITSCARAB,0);		/*scarab of negate spirit*/
	froom(4,OCUBEofUNDEAD,0);		/* cube of undead control	*/
	froom(2,ORINGOFEXTRA,0);	/* ring of extra regen		*/
	froom(3,ONOTHEFT,0);			/* device of antitheft 		*/
	froom(2,OSWORDofSLASHING,0); /* sword of slashing */
	if (c[BESSMANN]==0)
		{
		froom(4,OHAMMER,0);/*Bessman's flailing hammer*/ c[BESSMANN]=1;
		}
	if (c[HARDGAME]<3 || (rnd(4)==3))
		{
		if (j>3)
			{
			froom(3,OSWORD,3); 		/* sunsword + 3  		*/
			froom(5,O2SWORD,rnd(4));  /* a two handed sword	*/
			froom(3,OBELT,4);			/* belt of striking		*/
			froom(3,OENERGYRING,3);	/* energy ring			*/
			froom(4,OPLATE,5);		/* platemail + 5 		*/
			}
		}
	}

/*
	subroutine to fill in a number of objects of the same kind
 */

fillmroom(n,what,arg)
	int n,arg;
	char what;
	{
	int i;
	for (i=0; i<n; i++)		fillroom(what,arg);
	}
froom(n,itm,arg)
	int n,arg;
	char itm;
	{	if (rnd(151) < n) fillroom(itm,arg);	}

/*
	subroutine to put an object into an empty room
 *	uses a random walk
 */
static void
fillroom(what,arg)
	int arg;
	char what;
	{
	int x,y;

#ifdef EXTRA
	c[FILLROOM]++;
#endif

	x=rnd(MAXX-2);  y=rnd(MAXY-2);
	while (item[x][y])
		{

#ifdef EXTRA
		c[RANDOMWALK]++;	/* count up these random walks */
#endif

		x += rnd(3)-2;		y += rnd(3)-2;
		if (x > MAXX-2)  x=1;		if (x < 1)  x=MAXX-2;
		if (y > MAXY-2)  y=1;		if (y < 1)  y=MAXY-2;
		}
	item[x][y]=what;		iarg[x][y]=arg;
	}

/*
	subroutine to put monsters into an empty room without walls or other
	monsters
 */
fillmonst(what)
	char what;
	{
	int x,y,trys;
	for (trys=5; trys>0; --trys) /* max # of creation attempts */
	  {
	  x=rnd(MAXX-2);  y=rnd(MAXY-2);
	  if ((item[x][y]==0) && (mitem[x][y]==0) && ((playerx!=x) || (playery!=y)))
	  	{
		mitem[x][y] = what;  know[x][y]=0;
		hitp[x][y] = monster[what].hitpoints;  return(0);
		}
	  }
	return(-1); /* creation failure */
	}

/*
	creates an entire set of monsters for a level
	must be done when entering a new level
	if sethp(1) then wipe out old monsters else leave them there
 */
sethp(flg)
	int flg;
	{
	int i,j;
	if (flg) for (i=0; i<MAXY; i++) for (j=0; j<MAXX; j++) stealth[j][i]=0;
	if (level==0) { c[TELEFLAG]=0; return; } /*	if teleported and found level 1 then know level we are on */
	if (flg)   j = rnd(12) + 2 + (level>>1);   else   j = (level>>1) + 1;
	for (i=0; i<j; i++)  fillmonst(makemonst(level));
	positionplayer();
	}

/*
 *	Function to destroy all genocided monsters on the present level
 */
checkgen()
	{
	int x,y;
	for (y=0; y<MAXY; y++)
		for (x=0; x<MAXX; x++)
			if (monster[mitem[x][y]].genocided)
				mitem[x][y]=0; /* no more monster */
	}
