/* scores.c			 Larn is copyrighted 1986 by Noah Morgan.
 * $FreeBSD$
 *
 *	Functions in this file are:
 *
 *	readboard() 	Function to read in the scoreboard into a static buffer
 *	writeboard()	Function to write the scoreboard from readboard()'s buffer
 *	makeboard() 	Function to create a new scoreboard (wipe out old one)
 *	hashewon()	 Function to return 1 if player has won a game before, else 0
 *	long paytaxes(x)	 Function to pay taxes if any are due
 *	winshou()		Subroutine to print out the winning scoreboard
 *	shou(x)			Subroutine to print out the non-winners scoreboard
 *	showscores()		Function to show the scoreboard on the terminal
 *	showallscores()	Function to show scores and the iven lists that go with them
 *	sortboard()		Function to sort the scoreboard
 *	newscore(score, whoo, whyded, winner) 	Function to add entry to scoreboard
 *	new1sub(score,i,whoo,taxes) 		  Subroutine to put player into a
 *	new2sub(score,i,whoo,whyded)	 	  Subroutine to put player into a
 *	died(x) 	Subroutine to record who played larn, and what the score was
 *	diedsub(x) Subroutine to print out a line showing player when he is killed
 *	diedlog() 	Subroutine to read a log file and print it out in ascii format
 *	getplid(name)		Function to get players id # from id file
 *
 */
#include <sys/types.h>
#include <sys/times.h>
#include <sys/stat.h>
#include "header.h"

struct scofmt			/*	This is the structure for the scoreboard 		*/
	{
	long score;			/* the score of the player 							*/
	long suid;			/* the user id number of the player 				*/
	short what;			/* the number of the monster that killed player 	*/
	short level;		/* the level player was on when he died 			*/
	short hardlev;		/* the level of difficulty player played at 		*/
	short order;		/* the relative ordering place of this entry 		*/
	char who[40];		/* the name of the character 						*/
	char sciv[26][2];	/* this is the inventory list of the character 		*/
	};
struct wscofmt			/* This is the structure for the winning scoreboard */
	{
	long score;			/* the score of the player 							*/
	long timeused;		/* the time used in mobuls to win the game 			*/
	long taxes;			/* taxes he owes to LRS 							*/
	long suid;			/* the user id number of the player 				*/
	short hardlev;		/* the level of difficulty player played at 		*/
	short order;		/* the relative ordering place of this entry 		*/
	char who[40];		/* the name of the character 						*/
	};

struct log_fmt			/* 102 bytes struct for the log file 				*/
	{
	long score;			/* the players score 								*/
	time_t diedtime;		/* time when game was over 							*/
	short cavelev;		/* level in caves 									*/
	short diff;			/* difficulty player played at 						*/
#ifdef EXTRA
	long elapsedtime;	/* real time of game in seconds 					*/
	long bytout;		/* bytes input and output 							*/
	long bytin;
	long moves;			/* number of moves made by player 					*/
	short ac;			/* armor class of player 							*/
	short hp,hpmax;		/* players hitpoints 								*/
	short cputime;		/* cpu time needed in seconds 						*/
	short killed,spused;/* monsters killed and spells cast 					*/
	short usage;		/* usage of the cpu in % 							*/
	short lev;			/* player level 									*/
#endif
	char who[12];		/* player name 										*/
	char what[46];		/* what happened to player 							*/
	};

static struct scofmt sco[SCORESIZE];	/* the structure for the scoreboard  */
static struct wscofmt winr[SCORESIZE];	/* struct for the winning scoreboard */
static struct log_fmt logg;				/* structure for the log file 		 */
static char *whydead[] = {
	"quit", "suspended", "self - annihilated", "shot by an arrow",
	"hit by a dart", "fell into a pit", "fell into a bottomless pit",
	"a winner", "trapped in solid rock", "killed by a missing save file",
	"killed by an old save file", "caught by the greedy cheater checker trap",
	"killed by a protected save file","killed his family and committed suicide",
	"erased by a wayward finger", "fell through a bottomless trap door",
	"fell through a trap door", "drank some poisonous water",
	"fried by an electric shock", "slipped on a volcano shaft",
	"killed by a stupid act of frustration", "attacked by a revolting demon",
	"hit by his own magic", "demolished by an unseen attacker",
	"fell into the dreadful sleep", "killed by an exploding chest",
/*26*/	"killed by a missing maze data file", "annihilated in a sphere",
	"died a post mortem death","wasted by a malloc() failure"
	};

/*
 *	readboard() 	Function to read in the scoreboard into a static buffer
 *
 *	returns -1 if unable to read in the scoreboard, returns 0 if all is OK
 */
readboard()
	{
	if (lopen(scorefile)<0)
	  { lprcat("Can't read scoreboard\n"); lflush(); return(-1); }
	lrfill((char*)sco,sizeof(sco));		lrfill((char*)winr,sizeof(winr));
	lrclose();  lcreat((char*)0);  return(0);
	}

/*
 *	writeboard()	Function to write the scoreboard from readboard()'s buffer
 *
 *	returns -1 if unable to write the scoreboard, returns 0 if all is OK
 */
writeboard()
	{
	set_score_output();
	if (lcreat(scorefile)<0)
	  { lprcat("Can't write scoreboard\n"); lflush(); return(-1); }
	lwrite((char*)sco,sizeof(sco));		lwrite((char*)winr,sizeof(winr));
	lwclose();  lcreat((char*)0);  return(0);
	}

/*
 *	makeboard() 		Function to create a new scoreboard (wipe out old one)
 *
 *	returns -1 if unable to write the scoreboard, returns 0 if all is OK
 */
makeboard()
	{
	int i;
	for (i=0; i<SCORESIZE; i++)
		{
		winr[i].taxes = winr[i].score = sco[i].score = 0;
		winr[i].order = sco[i].order = i;
		}
	if (writeboard()) return(-1);
	chmod(scorefile,0660);
	return(0);
	}

/*
 *	hashewon()	 Function to return 1 if player has won a game before, else 0
 *
 *	This function also sets c[HARDGAME] to appropriate value -- 0 if not a
 *	winner, otherwise the next level of difficulty listed in the winners
 *	scoreboard.  This function also sets outstanding_taxes to the value in
 *	the winners scoreboard.
 */
hashewon()
	{
	int i;
	c[HARDGAME] = 0;
	if (readboard() < 0) return(0);	/* can't find scoreboard */
	for (i=0; i<SCORESIZE; i++)	/* search through winners scoreboard */
	   if (winr[i].suid == userid)
		  if (winr[i].score > 0)
			{
			c[HARDGAME]=winr[i].hardlev+1;  outstanding_taxes=winr[i].taxes;
			return(1);
			}
	return(0);
	}

/*
 *	long paytaxes(x)		 Function to pay taxes if any are due
 *
 *	Enter with the amount (in gp) to pay on the taxes.
 *	Returns amount actually paid.
 */
long paytaxes(x)
	long x;
	{
	int i;
	long amt;
	if (x<0) return(0L);
	if (readboard()<0) return(0L);
	for (i=0; i<SCORESIZE; i++)
		if (winr[i].suid == userid)	/* look for players winning entry */
			if (winr[i].score>0) /* search for a winning entry for the player */
				{
				amt = winr[i].taxes;
				if (x < amt) amt=x;		/* don't overpay taxes (Ughhhhh) */
				winr[i].taxes -= amt;
				outstanding_taxes -= amt;
				if (writeboard()<0) return(0);
				return(amt);
				}
	return(0L);	/* couldn't find user on winning scoreboard */
	}

/*
 *	winshou()		Subroutine to print out the winning scoreboard
 *
 *	Returns the number of players on scoreboard that were shown
 */
winshou()
	{
	struct wscofmt *p;
	int i,j,count;
	for (count=j=i=0; i<SCORESIZE; i++) /* is there anyone on the scoreboard? */
		if (winr[i].score != 0)
			{ j++; break; }
	if (j)
		{
		lprcat("\n  Score    Difficulty   Time Needed   Larn Winners List\n");

		for (i=0; i<SCORESIZE; i++)	/* this loop is needed to print out the */
		  for (j=0; j<SCORESIZE; j++) /* winners in order */
			{
			p = &winr[j];	/* pointer to the scoreboard entry */
			if (p->order == i)
				{
				if (p->score)
					{
					count++;
					lprintf("%10d     %2d      %5d Mobuls   %s \n",
					(long)p->score,(long)p->hardlev,(long)p->timeused,p->who);
					}
				break;
				}
			}
		}
	return(count);	/* return number of people on scoreboard */
	}

/*
 *	shou(x)			Subroutine to print out the non-winners scoreboard
 *		int x;
 *
 *	Enter with 0 to list the scores, enter with 1 to list inventories too
 *	Returns the number of players on scoreboard that were shown
 */
shou(x)
	int x;
	{
	int i,j,n,k;
	int count;
	for (count=j=i=0; i<SCORESIZE; i++)	/* is the scoreboard empty? */
		if (sco[i].score!= 0)
			{ j++; break; }
	if (j)
		{
		lprcat("\n   Score   Difficulty   Larn Visitor Log\n");
		for (i=0; i<SCORESIZE; i++) /* be sure to print them out in order */
		  for (j=0; j<SCORESIZE; j++)
			if (sco[j].order == i)
				{
				if (sco[j].score)
					{
					count++;
					lprintf("%10d     %2d       %s ",
						(long)sco[j].score,(long)sco[j].hardlev,sco[j].who);
					if (sco[j].what < 256) lprintf("killed by a %s",monster[sco[j].what].name);
						else lprintf("%s",whydead[sco[j].what - 256]);
					if (x != 263) lprintf(" on %s",levelname[sco[j].level]);
					if (x)
						{
						for (n=0; n<26; n++) { iven[n]=sco[j].sciv[n][0]; ivenarg[n]=sco[j].sciv[n][1]; }
						for (k=1; k<99; k++)
						  for (n=0; n<26; n++)
							if (k==iven[n])  { srcount=0; show3(n); }
						lprcat("\n\n");
						}
					else lprc('\n');
					}
				j=SCORESIZE;
				}
		}
	return(count);	/* return the number of players just shown */
	}

/*
 *	showscores()		Function to show the scoreboard on the terminal
 *
 *	Returns nothing of value
 */
static char esb[] = "The scoreboard is empty.\n";
showscores()
	{
	int i,j;
	lflush();  lcreat((char*)0);  if (readboard()<0) return;
	i=winshou();	j=shou(0);
	if (i+j == 0) lprcat(esb); else lprc('\n');
	lflush();
	}

/*
 *	showallscores()	Function to show scores and the iven lists that go with them
 *
 *	Returns nothing of value
 */
showallscores()
	{
	int i,j;
	lflush();  lcreat((char*)0);  if (readboard()<0) return;
	c[WEAR] = c[WIELD] = c[SHIELD] = -1;  /* not wielding or wearing anything */
	for (i=0; i<MAXPOTION; i++) potionname[i][0]=' ';
	for (i=0; i<MAXSCROLL; i++) scrollname[i][0]=' ';
	i=winshou();  j=shou(1);
	if (i+j==0) lprcat(esb); else lprc('\n');
	lflush();
	}

/*
 *	sortboard()		Function to sort the scoreboard
 *
 *	Returns 0 if no sorting done, else returns 1
 */
sortboard()
	{
	int i,j,pos;
	long jdat;
	for (i=0; i<SCORESIZE; i++) sco[i].order = winr[i].order = -1;
	pos=0;  while (pos < SCORESIZE)
		{
		jdat=0;
		for (i=0; i<SCORESIZE; i++)
			if ((sco[i].order < 0) && (sco[i].score >= jdat))
				{ j=i;  jdat=sco[i].score; }
		sco[j].order = pos++;
		}
	pos=0;  while (pos < SCORESIZE)
		{
		jdat=0;
		for (i=0; i<SCORESIZE; i++)
			if ((winr[i].order < 0) && (winr[i].score >= jdat))
				{ j=i;  jdat=winr[i].score; }
		winr[j].order = pos++;
		}
	return(1);
	}

/*
 *	newscore(score, whoo, whyded, winner) 	Function to add entry to scoreboard
 *		int score, winner, whyded;
 *		char *whoo;
 *
 *	Enter with the total score in gp in score,  players name in whoo,
 *		died() reason # in whyded, and TRUE/FALSE in winner if a winner
 *	ex.		newscore(1000, "player 1", 32, 0);
 */
newscore(score, whoo, whyded, winner)
	long score;
	int winner, whyded;
	char *whoo;
	{
	int i;
	long taxes;
	if (readboard() < 0) return; 	/*	do the scoreboard	*/
	/* if a winner then delete all non-winning scores */
	if (cheat) winner=0;	/* if he cheated, don't let him win */
	if (winner)
		{
		for (i=0; i<SCORESIZE; i++) if (sco[i].suid == userid) sco[i].score=0;
		taxes = score*TAXRATE;
		score += 100000*c[HARDGAME];	/* bonus for winning */
	/* if he has a slot on the winning scoreboard update it if greater score */
		for (i=0; i<SCORESIZE; i++) if (winr[i].suid == userid)
				{ new1sub(score,i,whoo,taxes); return; }
	/* he had no entry. look for last entry and see if he has a greater score */
		for (i=0; i<SCORESIZE; i++) if (winr[i].order == SCORESIZE-1)
				{ new1sub(score,i,whoo,taxes); return; }
		}
	else if (!cheat) /* for not winning scoreboard */
		{
	/* if he has a slot on the scoreboard update it if greater score */
		for (i=0; i<SCORESIZE; i++) if (sco[i].suid == userid)
				{ new2sub(score,i,whoo,whyded); return; }
	/* he had no entry. look for last entry and see if he has a greater score */
		for (i=0; i<SCORESIZE; i++) if (sco[i].order == SCORESIZE-1)
				{ new2sub(score,i,whoo,whyded); return; }
		}
	}

/*
 *	new1sub(score,i,whoo,taxes) 	  Subroutine to put player into a
 *		int score,i,whyded,taxes;		  winning scoreboard entry if his score
 *		char *whoo; 					  is high enough
 *
 *	Enter with the total score in gp in score,  players name in whoo,
 *		died() reason # in whyded, and TRUE/FALSE in winner if a winner
 *		slot in scoreboard in i, and the tax bill in taxes.
 *	Returns nothing of value
 */
new1sub(score,i,whoo,taxes)
	long score,taxes;
	int i;
	char *whoo;
	{
	struct wscofmt *p;
	p = &winr[i];
	p->taxes += taxes;
	if ((score >= p->score) || (c[HARDGAME] > p->hardlev))
		{
		strcpy(p->who,whoo);  		p->score=score;
		p->hardlev=c[HARDGAME];		p->suid=userid;
		p->timeused=gtime/100;
		}
	}

/*
 *	new2sub(score,i,whoo,whyded)	 	  Subroutine to put player into a
 *		int score,i,whyded,taxes;		  non-winning scoreboard entry if his
 *		char *whoo; 					  score is high enough
 *
 *	Enter with the total score in gp in score,  players name in whoo,
 *		died() reason # in whyded, and slot in scoreboard in i.
 *	Returns nothing of value
 */
new2sub(score,i,whoo,whyded)
	long score;
	int i,whyded;
	char *whoo;
	{
	int j;
	struct scofmt *p;
	p = &sco[i];
	if ((score >= p->score) || (c[HARDGAME] > p->hardlev))
		{
		strcpy(p->who,whoo);  p->score=score;
		p->what=whyded;       p->hardlev=c[HARDGAME];
		p->suid=userid;		  p->level=level;
		for (j=0; j<26; j++)
			{ p->sciv[j][0]=iven[j]; p->sciv[j][1]=ivenarg[j]; }
		}
	}

/*
 *	died(x) 	Subroutine to record who played larn, and what the score was
 *		int x;
 *
 *	if x < 0 then don't show scores
 *	died() never returns! (unless c[LIFEPROT] and a reincarnatable death!)
 *
 *		< 256	killed by the monster number
 *		256		quit
 *		257		suspended
 *		258		self - annihilated
 *		259		shot by an arrow
 *		260		hit by a dart
 *		261		fell into a pit
 *		262		fell into a bottomless pit
 *		263		a winner
 *		264		trapped in solid rock
 *		265		killed by a missing save file
 *		266		killed by an old save file
 *		267		caught by the greedy cheater checker trap
 *		268		killed by a protected save file
 *		269		killed his family and killed himself
 *		270		erased by a wayward finger
 *		271		fell through a bottomless trap door
 *		272		fell through a trap door
 *		273		drank some poisonous water
 *		274		fried by an electric shock
 *		275		slipped on a volcano shaft
 *		276		killed by a stupid act of frustration
 *		277		attacked by a revolting demon
 *		278		hit by his own magic
 *		279		demolished by an unseen attacker
 *		280		fell into the dreadful sleep
 *		281		killed by an exploding chest
 *		282		killed by a missing maze data file
 *		283		killed by a sphere of annihilation
 *		284		died a post mortem death
 *		285		malloc() failure
 *		300		quick quit -- don't put on scoreboard
 */

static int scorerror;
died(x)
	int x;
	{
	int f,win;
	char ch,*mod;
	time_t zzz;
	long i;
	struct tms cputime;
	if (c[LIFEPROT]>0) /* if life protection */
		{
		switch((x>0) ? x : -x)
			{
			case 256: case 257: case 262: case 263: case 265: case 266:
			case 267: case 268: case 269: case 271: case 282: case 284:
			case 285: case 300:  goto invalid; /* can't be saved */
			};
		--c[LIFEPROT]; c[HP]=1; --c[CONSTITUTION];
		cursors(); lprcat("\nYou feel wiiieeeeerrrrrd all over! "); beep();
		lflush();  sleep(4);
		return; /* only case where died() returns */
		}
invalid:
	clearvt100();  lflush();  f=0;
	if (ckpflag) unlink(ckpfile);	/* remove checkpoint file if used */
	if (x<0) { f++; x = -x; }	/* if we are not to display the scores */
	if ((x == 300) || (x == 257))  exit(0);  /* for quick exit or saved game */
	if (x == 263)  win = 1;  else  win = 0;
	c[GOLD] += c[BANKACCOUNT];   c[BANKACCOUNT] = 0;
		/*	now enter the player at the end of the scoreboard */
	newscore(c[GOLD], logname, x, win);
	diedsub(x);	/* print out the score line */  lflush();

	set_score_output();
	if ((wizard == 0) && (c[GOLD] > 0)) 	/*	wizards can't score		*/
		{
#ifndef NOLOG
		if (lappend(logfile)<0)  /* append to file */
			{
			if (lcreat(logfile)<0) /* and can't create new log file */
		    	{
				lcreat((char*)0);
				lprcat("\nCan't open record file:  I can't post your score.\n");
				sncbr();  resetscroll();  lflush();  exit(1);
				}
			chmod(logfile,0660);
			}
		strcpy(logg.who,loginname);
		logg.score = c[GOLD];		logg.diff = c[HARDGAME];
		if (x < 256)
			{
			ch = *monster[x].name;
			if (ch=='a' || ch=='e' || ch=='i' || ch=='o' || ch=='u')
				mod="an";  else mod="a";
			sprintf(logg.what,"killed by %s %s",mod,monster[x].name);
			}
		else sprintf(logg.what,"%s",whydead[x - 256]);
		logg.cavelev=level;
		time(&zzz);	  /* get cpu time -- write out score info */
		logg.diedtime=zzz;
#ifdef EXTRA
		times(&cputime);  /* get cpu time -- write out score info */
		logg.cputime = i = (cputime.tms_utime + cputime.tms_stime)/60 + c[CPUTIME];
		logg.lev=c[LEVEL];			logg.ac=c[AC];
		logg.hpmax=c[HPMAX];		logg.hp=c[HP];
		logg.elapsedtime=(zzz-initialtime+59)/60;
		logg.usage=(10000*i)/(zzz-initialtime);
		logg.bytin=c[BYTESIN];		logg.bytout=c[BYTESOUT];
		logg.moves=c[MOVESMADE];	logg.spused=c[SPELLSCAST];
		logg.killed=c[MONSTKILLED];
#endif
		lwrite((char*)&logg,sizeof(struct log_fmt));	 lwclose();
#endif NOLOG

/*	now for the scoreboard maintenance -- not for a suspended game 	*/
		if (x != 257)
			{
			if (sortboard())  scorerror = writeboard();
			}
		}
	if ((x==256) || (x==257) || (f != 0)) exit(0);
	if (scorerror == 0) showscores();	/* if we updated the scoreboard */
	if (x == 263) mailbill();               exit(0);
	}

/*
 *	diedsub(x) Subroutine to print out the line showing the player when he is killed
 *		int x;
 */
diedsub(x)
int x;
	{
	char ch,*mod;
	lprintf("Score: %d, Diff: %d,  %s ",(long)c[GOLD],(long)c[HARDGAME],logname);
	if (x < 256)
		{
		ch = *monster[x].name;
		if (ch=='a' || ch=='e' || ch=='i' || ch=='o' || ch=='u')
			mod="an";  else mod="a";
		lprintf("killed by %s %s",mod,monster[x].name);
		}
	else lprintf("%s",whydead[x - 256]);
	if (x != 263) lprintf(" on %s\n",levelname[level]);  else lprc('\n');
	}

/*
 *	diedlog() 	Subroutine to read a log file and print it out in ascii format
 */
diedlog()
	{
	int n;
	char *p;
	struct stat stbuf;
	lcreat((char*)0);
	if (lopen(logfile)<0)
		{
		lprintf("Can't locate log file <%s>\n",logfile);
		return;
		}
	if (fstat(fd,&stbuf) < 0)
		{
		lprintf("Can't  stat log file <%s>\n",logfile);
		return;
		}
	for (n=stbuf.st_size/sizeof(struct log_fmt); n>0; --n)
		{
		lrfill((char*)&logg,sizeof(struct log_fmt));
		p = ctime(&logg.diedtime); p[16]='\n'; p[17]=0;
		lprintf("Score: %d, Diff: %d,  %s %s on %d at %s",(long)(logg.score),(long)(logg.diff),logg.who,logg.what,(long)(logg.cavelev),p+4);
#ifdef EXTRA
		if (logg.moves<=0) logg.moves=1;
		lprintf("  Experience Level: %d,  AC: %d,  HP: %d/%d,  Elapsed Time: %d minutes\n",(long)(logg.lev),(long)(logg.ac),(long)(logg.hp),(long)(logg.hpmax),(long)(logg.elapsedtime));
		lprintf("  CPU time used: %d seconds,  Machine usage: %d.%02d%%\n",(long)(logg.cputime),(long)(logg.usage/100),(long)(logg.usage%100));
		lprintf("  BYTES in: %d, out: %d, moves: %d, deaths: %d, spells cast: %d\n",(long)(logg.bytin),(long)(logg.bytout),(long)(logg.moves),(long)(logg.killed),(long)(logg.spused));
		lprintf("  out bytes per move: %d,  time per move: %d ms\n",(long)(logg.bytout/logg.moves),(long)((logg.cputime*1000)/logg.moves));
#endif
		}
		lflush();  lrclose();  return;
	}

#ifndef UIDSCORE
/*
 *	getplid(name)		Function to get players id # from id file
 *
 *	Enter with the name of the players character in name.
 *	Returns the id # of the players character, or -1 if failure.
 *	This routine will try to find the name in the id file, if its not there,
 *	it will try to make a new entry in the file.  Only returns -1 if can't
 *	find him in the file, and can't make a new entry in the file.
 *	Format of playerids file:
 *			Id # in ascii     \n     character name     \n
 */
static int havepid= -1;	/* playerid # if previously done */
getplid(nam)
	char *nam;
	{
	int fd7,high=999,no;
	char *p,*p2;
	char name[80];
	if (havepid != -1) return(havepid);	/* already did it */
	lflush();	/* flush any pending I/O */
	sprintf(name,"%s\n",nam);	/* append a \n to name */
	if (lopen(playerids) < 0)	/* no file, make it */
		{
		if ((fd7=creat(playerids,0666)) < 0)  return(-1); /* can't make it */
		close(fd7);  goto addone;	/* now append new playerid record to file */
		}
	for (;;)	/* now search for the name in the player id file */
		{
		p = lgetl();  if (p==NULL) break;	/* EOF? */
		no = atoi(p);	/* the id # */
		p2= lgetl();  if (p2==NULL) break;	/* EOF? */
		if (no>high) high=no;	/* accumulate highest id # */
		if (strcmp(p2,name)==0)	/* we found him */
			{
			return(no);	/* his id number */
			}
		}
	lrclose();
	/* if we get here, we didn't find him in the file -- put him there */
addone:
	if (lappend(playerids) < 0) return(-1);	/* can't open file for append */
	lprintf("%d\n%s",(long)++high,name);  /* new id # and name */
	lwclose();
	lcreat((char*)0);	/* re-open terminal channel */
	return(high);
	}
#endif UIDSCORE

