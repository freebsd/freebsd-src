/*	diag.c		Larn is copyrighted 1986 by Noah Morgan. */
#include <sys/types.h>
#include <sys/times.h>
#include <sys/stat.h>
#include "header.h"
extern int rmst,maxitm,lasttime;
extern char nosignal;
static struct tms cputime;
/*
	***************************
	DIAG -- dungeon diagnostics
	***************************

	subroutine to print out data for debugging
 */
#ifdef EXTRA
static int rndcount[16];
diag()
	{
	register int i,j;
	int hit,dam;
	cursors();  lwclose();
	if (lcreat(diagfile) < 0)	/*	open the diagnostic file	*/
		{
		lcreat((char*)0); lprcat("\ndiagnostic failure\n"); return(-1);
		}

	write(1,"\nDiagnosing . . .\n",18);
	lprcat("\n\nBeginning of DIAG diagnostics ----------\n");

/*	for the character attributes	*/

	lprintf("\n\nPlayer attributes:\n\nHit points: %2d(%2d)",(long)c[HP],(long)c[HPMAX]);
	lprintf("\ngold: %d  Experience: %d  Character level: %d  Level in caverns: %d",
		(long)c[GOLD],(long)c[EXPERIENCE],(long)c[LEVEL],(long)level);
	lprintf("\nTotal types of monsters: %d",(long)MAXMONST+8);

	lprcat("\f\nHere's the dungeon:\n\n");

	i=level;
	for (j=0; j<MAXLEVEL+MAXVLEVEL; j++)
		{
		newcavelevel(j);
		lprintf("\nMaze for level %s:\n",levelname[level]);
		diagdrawscreen();
		}
	newcavelevel(i);

	lprcat("\f\nNow for the monster data:\n\n");
	lprcat("   Monster Name      LEV  AC   DAM  ATT  DEF    GOLD   HP     EXP   \n");
	lprcat("--------------------------------------------------------------------------\n");
	for (i=0; i<=MAXMONST+8; i++)
		{
		lprintf("%19s  %2d  %3d ",monster[i].name,(long)monster[i].level,(long)monster[i].armorclass);
		lprintf(" %3d  %3d  %3d  ",(long)monster[i].damage,(long)monster[i].attack,(long)monster[i].defense);
		lprintf("%6d  %3d   %6d\n",(long)monster[i].gold,(long)monster[i].hitpoints,(long)monster[i].experience);
		}

	lprcat("\n\nHere's a Table for the to hit percentages\n");
	lprcat("\n     We will be assuming that players level = 2 * monster level");
	lprcat("\n     and that the players dexterity and strength are 16.");
	lprcat("\n    to hit: if (rnd(22) < (2[monst AC] + your level + dex + WC/8 -1)/2) then hit");
	lprcat("\n    damage = rund(8) + WC/2 + STR - c[HARDGAME] - 4");
	lprcat("\n    to hit:  if rnd(22) < to hit  then player hits\n");
	lprcat("\n    Each entry is as follows:  to hit / damage / number hits to kill\n");
	lprcat("\n          monster     WC = 4         WC = 20        WC = 40");
	lprcat("\n---------------------------------------------------------------");
	for (i=0; i<=MAXMONST+8; i++)
		{
		hit = 2*monster[i].armorclass+2*monster[i].level+16;
		dam = 16 - c[HARDGAME];
		lprintf("\n%20s   %2d/%2d/%2d       %2d/%2d/%2d       %2d/%2d/%2d",
					monster[i].name,
					(long)(hit/2),(long)max(0,dam+2),(long)(monster[i].hitpoints/(dam+2)+1),
					(long)((hit+2)/2),(long)max(0,dam+10),(long)(monster[i].hitpoints/(dam+10)+1),
					(long)((hit+5)/2),(long)max(0,dam+20),(long)(monster[i].hitpoints/(dam+20)+1));
		}

	lprcat("\n\nHere's the list of available potions:\n\n");
	for (i=0; i<MAXPOTION; i++)	lprintf("%20s\n",&potionname[i][1]);
	lprcat("\n\nHere's the list of available scrolls:\n\n");
	for (i=0; i<MAXSCROLL; i++)	lprintf("%20s\n",&scrollname[i][1]);
	lprcat("\n\nHere's the spell list:\n\n");
	lprcat("spell          name           description\n");
	lprcat("-------------------------------------------------------------------------------------------\n\n");
	for (j=0; j<SPNUM; j++)
		{
		lprc(' ');	lprcat(spelcode[j]);
		lprintf(" %21s  %s\n",spelname[j],speldescript[j]);
		}

	lprcat("\n\nFor the c[] array:\n");
	for (j=0; j<100; j+=10)
		{
		lprintf("\nc[%2d] = ",(long)j); for (i=0; i<9; i++) lprintf("%5d ",(long)c[i+j]);
		}

	lprcat("\n\nTest of random number generator ----------------");
	lprcat("\n    for 25,000 calls divided into 16 slots\n\n");

	for (i=0; i<16; i++)  rndcount[i]=0;
	for (i=0; i<25000; i++)	rndcount[rund(16)]++;
	for (i=0; i<16; i++)  { lprintf("  %5d",(long)rndcount[i]); if (i==7) lprc('\n'); }

	lprcat("\n\n");			lwclose();
	lcreat((char*)0);		lprcat("Done Diagnosing . . .");
	return(0);
	}
/*
	subroutine to count the number of occurrences of an object
 */
dcount(l)
	int l;
	{
	register int i,j,p;
	int k;
	k=0;
	for (i=0; i<MAXX; i++)
		for (j=0; j<MAXY; j++)
			for (p=0; p<MAXLEVEL; p++)
				if (cell[p*MAXX*MAXY+i*MAXY+j].item == l) k++;
	return(k);
	}

/*
	subroutine to draw the whole screen as the player knows it
 */
diagdrawscreen()
	{
	register int i,j,k;

	for (i=0; i<MAXY; i++)

/*	for the east west walls of this line	*/
		{
		for (j=0; j<MAXX; j++)	if (k=mitem[j][i]) lprc(monstnamelist[k]); else
								lprc(objnamelist[item[j][i]]);
		lprc('\n');
		}
	}
#endif

/*
	to save the game in a file
 */
static time_t zzz=0;
savegame(fname)
	char *fname;
	{
	register int i,k;
	register struct sphere *sp;
	struct stat statbuf;
	nosignal=1;  lflush();	savelevel();
	ointerest();
	if (lcreat(fname) < 0)
		{
		lcreat((char*)0); lprintf("\nCan't open file <%s> to save game\n",fname);
		nosignal=0;  return(-1);
		}

	set_score_output();
	lwrite((char*)beenhere,MAXLEVEL+MAXVLEVEL);
	for (k=0; k<MAXLEVEL+MAXVLEVEL; k++)
		if (beenhere[k])
			lwrite((char*)&cell[k*MAXX*MAXY],sizeof(struct cel)*MAXY*MAXX);
	times(&cputime);	/* get cpu time */
	c[CPUTIME] += (cputime.tms_utime+cputime.tms_stime)/60;
	lwrite((char*)&c[0],100*sizeof(long));
	lprint((long)gtime);		lprc(level);
	lprc(playerx);		lprc(playery);
	lwrite((char*)iven,26);	lwrite((char*)ivenarg,26*sizeof(short));
	for (k=0; k<MAXSCROLL; k++)  lprc(scrollname[k][0]);
	for (k=0; k<MAXPOTION; k++)  lprc(potionname[k][0]);
	lwrite((char*)spelknow,SPNUM);		 lprc(wizard);
	lprc(rmst);		/*	random monster generation counter */
	for (i=0; i<90; i++)	lprc(itm[i].qty);
	lwrite((char*)course,25);			lprc(cheat);		lprc(VERSION);
	for (i=0; i<MAXMONST; i++) lprc(monster[i].genocided); /* genocide info */
	for (sp=spheres; sp; sp=sp->p)
		lwrite((char*)sp,sizeof(struct sphere));	/* save spheres of annihilation */
	time(&zzz);			lprint((long)(zzz-initialtime));
	lwrite((char*)&zzz,sizeof(long));
	if (fstat(lfd,&statbuf)< 0) lprint(0L);
	else lprint((long)statbuf.st_ino); /* inode # */
	lwclose();	lastmonst[0] = 0;
#ifndef VT100
	setscroll();
#endif VT100
	lcreat((char*)0);  nosignal=0;
	return(0);
	}

restoregame(fname)
	char *fname;
	{
	register int i,k;
	register struct sphere *sp,*sp2;
	struct stat filetimes;
	cursors(); lprcat("\nRestoring . . .");  lflush();
	if (lopen(fname) <= 0)
		{
		lcreat((char*)0); lprintf("\nCan't open file <%s>to restore game\n",fname);
		nap(2000); c[GOLD]=c[BANKACCOUNT]=0;  died(-265); return;
		}

	lrfill((char*)beenhere,MAXLEVEL+MAXVLEVEL);
	for (k=0; k<MAXLEVEL+MAXVLEVEL; k++)
		if (beenhere[k])
			lrfill((char*)&cell[k*MAXX*MAXY],sizeof(struct cel)*MAXY*MAXX);

	lrfill((char*)&c[0],100*sizeof(long));	gtime = lrint();
	level = c[CAVELEVEL] = lgetc();
	playerx = lgetc();		playery = lgetc();
	lrfill((char*)iven,26);		lrfill((char*)ivenarg,26*sizeof(short));
	for (k=0; k<MAXSCROLL; k++)  scrollname[k][0] = lgetc();
	for (k=0; k<MAXPOTION; k++)  potionname[k][0] = lgetc();
	lrfill((char*)spelknow,SPNUM);		wizard = lgetc();
	rmst = lgetc();			/*	random monster creation flag */

	for (i=0; i<90; i++)	itm[i].qty = lgetc();
	lrfill((char*)course,25);			cheat = lgetc();
	if (VERSION != lgetc())		/*  version number  */
		{
		cheat=1;
		lprcat("Sorry, But your save file is for an older version of larn\n");
		nap(2000); c[GOLD]=c[BANKACCOUNT]=0;  died(-266); return;
		}

	for (i=0; i<MAXMONST; i++) monster[i].genocided=lgetc(); /* genocide info */
	for (sp=0,i=0; i<c[SPHCAST]; i++)
		{
		sp2 = sp;
		sp = (struct sphere *)malloc(sizeof(struct sphere));
		if (sp==0) { write(2,"Can't malloc() for sphere space\n",32); break; }
		lrfill((char*)sp,sizeof(struct sphere));	/* get spheres of annihilation */
		sp->p=0;	/* null out pointer */
		if (i==0) spheres=sp;	/* beginning of list */
			else sp2->p = sp;
		}

	time(&zzz);
	initialtime = zzz-lrint();
	fstat(fd,&filetimes);	/*	get the creation and modification time of file	*/
	lrfill((char*)&zzz,sizeof(long));	zzz += 6;
	if (filetimes.st_ctime > zzz) fsorry();	/*	file create time	*/
	else if (filetimes.st_mtime > zzz) fsorry(); /*	file modify time	*/
	if (c[HP]<0) { died(284); return; }	/* died a post mortem death */

	oldx = oldy = 0;
	i = lrint();  /* inode # */
	if (i && (filetimes.st_ino!=i)) fsorry();
	lrclose();
	if (strcmp(fname,ckpfile) == 0)
		{
		if (lappend(fname) < 0) fcheat();  else { lprc(' '); lwclose(); }
		lcreat((char*)0);
		}
	else if (unlink(fname) < 0) fcheat(); /* can't unlink save file */
/*	for the greedy cheater checker	*/
	for (k=0; k<6; k++) if (c[k]>99) greedy();
	if (c[HPMAX]>999 || c[SPELLMAX]>125) greedy();
	if (c[LEVEL]==25 && c[EXPERIENCE]>skill[24]) /* if patch up lev 25 player */
		{
		long tmp;
		tmp = c[EXPERIENCE]-skill[24]; /* amount to go up */
		c[EXPERIENCE] = skill[24];
		raiseexperience((long)tmp);
		}
	getlevel();  lasttime=gtime;
	}

/*
	subroutine to not allow greedy cheaters
 */
greedy()
	{
#if WIZID
	if (wizard) return;
#endif

	lprcat("\n\nI am so sorry, but your character is a little TOO good!  Since this\n");
	lprcat("cannot normally happen from an honest game, I must assume that you cheated.\n");
	lprcat("In that you are GREEDY as well as a CHEATER, I cannot allow this game\n");
	lprcat("to continue.\n"); nap(5000);  c[GOLD]=c[BANKACCOUNT]=0;  died(-267); return;
	}

/*
	subroutine to not allow altered save files and terminate the attempted
	restart
 */
fsorry()
	{
	lprcat("\nSorry, but your savefile has been altered.\n");
	lprcat("However, seeing as I am a good sport, I will let you play.\n");
	lprcat("Be advised though, you won't be placed on the normal scoreboard.");
	cheat = 1;	nap(4000);
	}

/*
	subroutine to not allow game if save file can't be deleted
 */
fcheat()
	{
#if WIZID
	if (wizard) return;
#endif

	lprcat("\nSorry, but your savefile can't be deleted.  This can only mean\n");
	lprcat("that you tried to CHEAT by protecting the directory the savefile\n");
	lprcat("is in.  Since this is unfair to the rest of the larn community, I\n");
	lprcat("cannot let you play this game.\n");
	nap(5000);  c[GOLD]=c[BANKACCOUNT]=0;  died(-268); return;
	}
