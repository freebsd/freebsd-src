/*	main.c		*/
/* $FreeBSD: src/games/larn/main.c,v 1.9 1999/11/30 03:48:59 billf Exp $ */
#include <sys/types.h>
#include <sys/stat.h>
#include "header.h"
#include <pwd.h>
static const char copyright[]="\nLarn is copyrighted 1986 by Noah Morgan.\n";
int srcount=0;	/* line counter for showstr()	*/
int dropflag=0; /* if 1 then don't lookforobject() next round */
int rmst=80;	/*	random monster creation counter		*/
int userid;		/* the players login user id number */
char nowelcome=0,nomove=0; /* if (nomove) then don't count next iteration as a move */
static char viewflag=0;
	/*	if viewflag then we have done a 99 stay here and don't showcell in the main loop */
char restorflag=0;	/* 1 means restore has been done	*/
static char cmdhelp[] = "\
Cmd line format: larn [-slicnh] [-o<optsifle>] [-##] [++]\n\
  -s   show the scoreboard\n\
  -l   show the logfile (wizard id only)\n\
  -i   show scoreboard with inventories of dead characters\n\
  -c   create new scoreboard (wizard id only)\n\
  -n   suppress welcome message on starting game\n\
  -##  specify level of difficulty (example: -5)\n\
  -h   print this help text\n\
  ++   restore game from checkpoint file\n\
  -o<optsfile>   specify .larnopts filename to be used instead of \"~/.larnopts\"\n\
";
#ifdef VT100
static char *termtypes[] = { "vt100", "vt101", "vt102", "vt103", "vt125",
	"vt131", "vt140", "vt180", "vt220", "vt240", "vt241", "vt320", "vt340",
	"vt341"  };
#endif VT100
/*
	************
	MAIN PROGRAM
	************
 */
main(argc,argv)
	int argc;
	char **argv;
	{
	int i,j;
	int hard;
	char *ptr=0,*ttype;
	struct passwd *pwe;
	struct stat sb;

/*
 *	first task is to identify the player
 */
#ifndef VT100
	init_term();	/* setup the terminal (find out what type) for termcap */
#endif VT100
	if (((ptr = getlogin()) == 0) || (*ptr==0)) {	/* try to get login name */
	  if (pwe=getpwuid(getuid())) /* can we get it from /etc/passwd? */
		ptr = pwe->pw_name;
	  else
	  if ((ptr = getenv("USER")) == 0)
		if ((ptr = getenv("LOGNAME")) == 0)
		  {
		  noone: write(2, "Can't find your logname.  Who Are You?\n",39);
				 exit(1);
		  }
	}
	if (ptr==0) goto noone;
	if (strlen(ptr)==0) goto noone;
/*
 *	second task is to prepare the pathnames the player will need
 */
	strcpy(loginname,ptr); /* save loginname of the user for logging purposes */
	strcpy(logname,ptr);	/* this will be overwritten with the players name */
	if ((ptr = getenv("HOME")) == 0) ptr = ".";
	strcpy(savefilename, ptr);
	strcat(savefilename, "/Larn.sav");	/* save file name in home directory */
	sprintf(optsfile, "%s/.larnopts",ptr);	/* the .larnopts filename */

/*
 *	now malloc the memory for the dungeon
 */
	cell = (struct cel *)malloc(sizeof(struct cel)*(MAXLEVEL+MAXVLEVEL)*MAXX*MAXY);
	if (cell == 0) died(-285);	/* malloc failure */
	lpbuf    = malloc((5* BUFBIG)>>2);	/* output buffer */
	inbuffer = malloc((5*MAXIBUF)>>2);	/* output buffer */
	if ((lpbuf==0) || (inbuffer==0)) died(-285); /* malloc() failure */

	lcreat((char*)0);	newgame();		/*	set the initial clock  */ hard= -1;

#ifdef VT100
/*
 *	check terminal type to avoid users who have not vt100 type terminals
 */
	ttype = getenv("TERM");
	for (j=1, i=0; i<sizeof(termtypes)/sizeof(char *); i++)
		if (strcmp(ttype,termtypes[i]) == 0) { j=0;  break; }
	if (j)
		{
		lprcat("Sorry, Larn needs a VT100 family terminal for all it's features.\n"); lflush();
		exit(1);
		}
#endif VT100

/*
 *	now make scoreboard if it is not there (don't clear)
 */
	if (stat(scorefile,&sb) < 0 || sb.st_size == 0) /* not there */
		makeboard();

/*
 *	now process the command line arguments
 */
	for (i=1; i<argc; i++)
		{
		if (argv[i][0] == '-')
		  switch(argv[i][1])
			{
			case 's': showscores();  exit(0);  /* show scoreboard   */

			case 'l': /* show log file     */
						diedlog();              exit(0);

			case 'i': showallscores();  exit(0);  /* show all scoreboard */

			case 'c': 		 /* anyone with password can create scoreboard */
					  lprcat("Preparing to initialize the scoreboard.\n");
					  if (getpassword() != 0)  /*make new scoreboard*/
							{
							makeboard(); lprc('\n'); showscores();
							}
					  exit(0);

			case 'n':	/* no welcome msg	*/ nowelcome=1; argv[i][0]=0; break;

			case '0': case '1': case '2': case '3': case '4': case '5':
			case '6': case '7': case '8': case '9':	/* for hardness */
						sscanf(&argv[i][1],"%d",&hard);
						break;

			case 'h':	/* print out command line arguments */
						write(1,cmdhelp,sizeof(cmdhelp));  exit(0);

			case 'o':	/* specify a .larnopts filename */
						strncpy(optsfile,argv[i]+2,127);  break;

			default:        printf("Unknown option <%s>\n",argv[i]);  exit(1);
			};

		if (argv[i][0] == '+')
			{
			clear();	restorflag = 1;
			if (argv[i][1] == '+')
				{
				hitflag=1; restoregame(ckpfile); /* restore checkpointed game */
				}
			i = argc;
			}
		}

	readopts();		/* read the options file if there is one */


#ifdef UIDSCORE
	userid = geteuid();	/* obtain the user's effective id number */
#else UIDSCORE
	userid = getplid(logname);	/* obtain the players id number */
#endif UIDSCORE
	if (userid < 0) { write(2,"Can't obtain playerid\n",22); exit(1); }

#ifdef HIDEBYLINK
/*
 *	this section of code causes the program to look like something else to ps
 */
	if (strcmp(psname,argv[0])) /* if a different process name only */
		{
		if ((i=access(psname,1)) < 0)
			{		/* link not there */
			if (link(argv[0],psname)>=0)
				{
				argv[0] = psname;   execv(psname,argv);
				}
			}
		else
			unlink(psname);
		}

	for (i=1; i<argc; i++)
		{
		szero(argv[i]);	/* zero the argument to avoid ps snooping */
		}
#endif HIDEBYLINK

	if (access(savefilename,0)==0)	/* restore game if need to */
		{
		clear();	restorflag = 1;
		hitflag=1;	restoregame(savefilename);  /* restore last game	*/
		}
	sigsetup();		/* trap all needed signals	*/
	sethard(hard);	/* set up the desired difficulty				*/
	setupvt100();	/*	setup the terminal special mode				*/
	if (c[HP]==0)	/* create new game */
		{
		makeplayer();	/*	make the character that will play			*/
		newcavelevel(0);/*	make the dungeon						 	*/
		predostuff = 1;	/* tell signals that we are in the welcome screen */
		if (nowelcome==0) welcome();	 /* welcome the player to the game */
		}
	drawscreen();	/*	show the initial dungeon					*/
	predostuff = 2;	/* tell the trap functions that they must do a showplayer()
						from here on */
	/* nice(1); */	/* games should be run niced */
	yrepcount = hit2flag = 0;
	while (1)
		{
		if (dropflag==0) lookforobject(); /* see if there is an object here	*/
			else dropflag=0; /* don't show it just dropped an item */
		if (hitflag==0) { if (c[HASTEMONST]) movemonst(); movemonst(); }	/*	move the monsters		*/
		if (viewflag==0) showcell(playerx,playery); else viewflag=0;	/*	show stuff around player	*/
		if (hit3flag) flushall();
		hitflag=hit3flag=0;	nomove=1;
		bot_linex();	/* update bottom line */
		while (nomove)
			{
			if (hit3flag) flushall();
			nomove=0; parse();
			}	/*	get commands and make moves	*/
		regen();			/*	regenerate hp and spells			*/
		if (c[TIMESTOP]==0)
			if (--rmst <= 0)
				{ rmst = 120-(level<<2); fillmonst(makemonst(level)); }
		}
	}

/*
	showstr()

	show character's inventory
 */
showstr()
	{
	int i,number;
	for (number=3, i=0; i<26; i++)
		if (iven[i]) number++;	/* count items in inventory */
	t_setup(number);	qshowstr();	  t_endup(number);
	}

qshowstr()
	{
	int i,j,k,sigsav;
	srcount=0;  sigsav=nosignal;  nosignal=1; /* don't allow ^c etc */
	if (c[GOLD]) { lprintf(".)   %d gold pieces",(long)c[GOLD]); srcount++; }
	for (k=26; k>=0; k--)
	  if (iven[k])
		{  for (i=22; i<84; i++)
			 for (j=0; j<=k; j++)  if (i==iven[j])  show3(j); k=0; }

	lprintf("\nElapsed time is %d.  You have %d mobuls left",(long)((gtime+99)/100+1),(long)((TIMELIMIT-gtime)/100));
	more();		nosignal=sigsav;
	}

/*
 *	subroutine to clear screen depending on # lines to display
 */
t_setup(count)
	int count;
	{
	if (count<20)  /* how do we clear the screen? */
		{
		cl_up(79,count);  cursor(1,1);
		}
	else
		{
		resetscroll(); clear();
		}
	}

/*
 *	subroutine to restore normal display screen depending on t_setup()
 */
t_endup(count)
	int count;
	{
	if (count<18)  /* how did we clear the screen? */
		draws(0,MAXX,0,(count>MAXY) ? MAXY : count);
	else
		{
		drawscreen(); setscroll();
		}
	}

/*
	function to show the things player is wearing only
 */
showwear()
	{
	int i,j,sigsav,count;
	sigsav=nosignal;  nosignal=1; /* don't allow ^c etc */
	srcount=0;

	 for (count=2,j=0; j<=26; j++)	 /* count number of items we will display */
	   if (i=iven[j])
		switch(i)
			{
			case OLEATHER:	case OPLATE:	case OCHAIN:
			case ORING:		case OSTUDLEATHER:	case OSPLINT:
			case OPLATEARMOR:	case OSSPLATE:	case OSHIELD:
			count++;
			};

	t_setup(count);

	for (i=22; i<84; i++)
		 for (j=0; j<=26; j++)
		   if (i==iven[j])
			switch(i)
				{
				case OLEATHER:	case OPLATE:	case OCHAIN:
				case ORING:		case OSTUDLEATHER:	case OSPLINT:
				case OPLATEARMOR:	case OSSPLATE:	case OSHIELD:
				show3(j);
				};
	more();		nosignal=sigsav;	t_endup(count);
	}

/*
	function to show the things player can wield only
 */
showwield()
	{
	int i,j,sigsav,count;
	sigsav=nosignal;  nosignal=1; /* don't allow ^c etc */
	srcount=0;

	 for (count=2,j=0; j<=26; j++)	/* count how many items */
	   if (i=iven[j])
		switch(i)
			{
			case ODIAMOND:  case ORUBY:  case OEMERALD:  case OSAPPHIRE:
			case OBOOK:     case OCHEST:  case OLARNEYE: case ONOTHEFT:
			case OSPIRITSCARAB:  case OCUBEofUNDEAD:
			case OPOTION:   case OSCROLL:  break;
			default:  count++;
			};

	t_setup(count);

	for (i=22; i<84; i++)
		 for (j=0; j<=26; j++)
		   if (i==iven[j])
			switch(i)
				{
				case ODIAMOND:  case ORUBY:  case OEMERALD:  case OSAPPHIRE:
				case OBOOK:     case OCHEST:  case OLARNEYE: case ONOTHEFT:
				case OSPIRITSCARAB:  case OCUBEofUNDEAD:
				case OPOTION:   case OSCROLL:  break;
				default:  show3(j);
				};
	more();		nosignal=sigsav;	t_endup(count);
	}

/*
 *	function to show the things player can read only
 */
showread()
	{
	int i,j,sigsav,count;
	sigsav=nosignal;  nosignal=1; /* don't allow ^c etc */
	srcount=0;

	for (count=2,j=0; j<=26; j++)
		switch(iven[j])
			{
			case OBOOK:	case OSCROLL:	count++;
			};
	t_setup(count);

	for (i=22; i<84; i++)
		 for (j=0; j<=26; j++)
		   if (i==iven[j])
			switch(i)
				{
				case OBOOK:	case OSCROLL:	show3(j);
				};
	more();		nosignal=sigsav;	t_endup(count);
	}

/*
 *	function to show the things player can eat only
 */
showeat()
	{
	int i,j,sigsav,count;
	sigsav=nosignal;  nosignal=1; /* don't allow ^c etc */
	srcount=0;

	for (count=2,j=0; j<=26; j++)
		switch(iven[j])
			{
			case OCOOKIE:	count++;
			};
	t_setup(count);

	for (i=22; i<84; i++)
		 for (j=0; j<=26; j++)
		   if (i==iven[j])
			switch(i)
				{
				case OCOOKIE:	show3(j);
				};
	more();		nosignal=sigsav;	t_endup(count);
	}

/*
	function to show the things player can quaff only
 */
showquaff()
	{
	int i,j,sigsav,count;
	sigsav=nosignal;  nosignal=1; /* don't allow ^c etc */
	srcount=0;

	for (count=2,j=0; j<=26; j++)
		switch(iven[j])
			{
			case OPOTION:	count++;
			};
	t_setup(count);

	for (i=22; i<84; i++)
		 for (j=0; j<=26; j++)
		   if (i==iven[j])
			switch(i)
				{
				case OPOTION:	show3(j);
				};
	more();		nosignal=sigsav;		t_endup(count);
	}

show1(idx,str2)
	int idx;
	char *str2[];
	{
	if (str2==0)  lprintf("\n%c)   %s",idx+'a',objectname[iven[idx]]);
	else if (*str2[ivenarg[idx]]==0)  lprintf("\n%c)   %s",idx+'a',objectname[iven[idx]]);
	else lprintf("\n%c)   %s of%s",idx+'a',objectname[iven[idx]],str2[ivenarg[idx]]);
	}

show3(index)
	int index;
	{
	switch(iven[index])
		{
		case OPOTION:	show1(index,potionname);  break;
		case OSCROLL:	show1(index,scrollname);  break;

		case OLARNEYE:		case OBOOK:			case OSPIRITSCARAB:
		case ODIAMOND:		case ORUBY:			case OCUBEofUNDEAD:
		case OEMERALD:		case OCHEST:		case OCOOKIE:
		case OSAPPHIRE:		case ONOTHEFT:		show1(index,(char **)0);  break;

		default:		lprintf("\n%c)   %s",index+'a',objectname[iven[index]]);
						if (ivenarg[index]>0) lprintf(" + %d",(long)ivenarg[index]);
						else if (ivenarg[index]<0) lprintf(" %d",(long)ivenarg[index]);
						break;
		}
	if (c[WIELD]==index) lprcat(" (weapon in hand)");
	if ((c[WEAR]==index) || (c[SHIELD]==index))  lprcat(" (being worn)");
	if (++srcount>=22) { srcount=0; more(); clear(); }
	}

/*
	subroutine to randomly create monsters if needed
 */
randmonst()
	{
	if (c[TIMESTOP]) return;	/*	don't make monsters if time is stopped	*/
	if (--rmst <= 0)
		{
		rmst = 120 - (level<<2);  fillmonst(makemonst(level));
		}
	}


/*
	parse()

	get and execute a command
 */
parse()
	{
	int i,j,k,flag;
	while	(1)
		{
		k = yylex();
		switch(k)	/*	get the token from the input and switch on it	*/
			{
			case 'h':	moveplayer(4);	return;		/*	west		*/
			case 'H':	run(4);			return;		/*	west		*/
			case 'l':	moveplayer(2);	return;		/*	east		*/
			case 'L':	run(2);			return;		/*	east		*/
			case 'j':	moveplayer(1);	return;		/*	south		*/
			case 'J':	run(1);			return;		/*	south		*/
			case 'k':	moveplayer(3);	return;		/*	north		*/
			case 'K':	run(3);			return;		/*	north		*/
			case 'u':	moveplayer(5);	return;		/*	northeast	*/
			case 'U':	run(5);			return;		/*	northeast	*/
			case 'y':	moveplayer(6);  return;		/*	northwest	*/
			case 'Y':	run(6);			return;		/*	northwest	*/
			case 'n':	moveplayer(7);	return;		/*	southeast	*/
			case 'N':	run(7);			return;		/*	southeast	*/
			case 'b':	moveplayer(8);	return;		/*	southwest	*/
			case 'B':	run(8);			return;		/*	southwest	*/

			case '.':	if (yrepcount) viewflag=1; return;		/*	stay here		*/

			case 'w':	yrepcount=0;	wield();	return;		/*	wield a weapon */

			case 'W':	yrepcount=0;	wear();		return;	/*	wear armor	*/

			case 'r':	yrepcount=0;
						if (c[BLINDCOUNT]) { cursors(); lprcat("\nYou can't read anything when you're blind!"); } else
						if (c[TIMESTOP]==0) readscr(); return;		/*	to read a scroll	*/

			case 'q':	yrepcount=0;	if (c[TIMESTOP]==0) quaff();	return;	/*	quaff a potion		*/

			case 'd':	yrepcount=0;	if (c[TIMESTOP]==0) dropobj(); return;	/*	to drop an object	*/

			case 'c':	yrepcount=0;	cast();		return;		/*	cast a spell	*/

			case 'i':	yrepcount=0;	nomove=1;  showstr();	return;		/*	status		*/

			case 'e':	yrepcount=0;
						if (c[TIMESTOP]==0) eatcookie(); return;	/*	to eat a fortune cookie */

			case 'D':	yrepcount=0;	seemagic(0);	nomove=1; return;	/*	list spells and scrolls */

			case '?':	yrepcount=0;	help(); nomove=1; return;	/*	give the help screen*/

			case 'S':	clear();  lprcat("Saving . . ."); lflush();
						savegame(savefilename); wizard=1; died(-257);	/*	save the game - doesn't return	*/

			case 'Z':	yrepcount=0;	if (c[LEVEL]>9) { oteleport(1); return; }
						cursors(); lprcat("\nAs yet, you don't have enough experience to use teleportation");
						return;	/*	teleport yourself	*/

			case '^':	/* identify traps */  flag=yrepcount=0;  cursors();
						lprc('\n');  for (j=playery-1; j<playery+2; j++)
							{
							if (j < 0) j=0;		if (j >= MAXY) break;
							for (i=playerx-1; i<playerx+2; i++)
								{
								if (i < 0) i=0;	if (i >= MAXX) break;
								switch(item[i][j])
									{
									case OTRAPDOOR:		case ODARTRAP:
									case OTRAPARROW:	case OTELEPORTER:
										lprcat("\nIts "); lprcat(objectname[item[i][j]]);  flag++;
									};
								}
							}
						if (flag==0) lprcat("\nNo traps are visible");
						return;

#if WIZID
			case '_':	/*	this is the fudge player password for wizard mode*/
						yrepcount=0;	cursors(); nomove=1;
						if (userid!=wisid)
							{
							lprcat("Sorry, you are not empowered to be a wizard.\n");
							scbr(); /* system("stty -echo cbreak"); */
							lflush();  return;
							}
						if (getpassword()==0)
							{
							scbr(); /* system("stty -echo cbreak"); */ return;
							}
						wizard=1;  scbr(); /* system("stty -echo cbreak"); */
						for (i=0; i<6; i++)  c[i]=70;  iven[0]=iven[1]=0;
						take(OPROTRING,50);   take(OLANCE,25);  c[WIELD]=1;
						c[LANCEDEATH]=1;   c[WEAR] = c[SHIELD] = -1;
						raiseexperience(6000000L);  c[AWARENESS] += 25000;
						{
						int i,j;
						for (i=0; i<MAXY; i++)
							for (j=0; j<MAXX; j++)  know[j][i]=1;
						for (i=0; i<SPNUM; i++)	spelknow[i]=1;
						for (i=0; i<MAXSCROLL; i++)  scrollname[i][0]=' ';
						for (i=0; i<MAXPOTION; i++)  potionname[i][0]=' ';
						}
						for (i=0; i<MAXSCROLL; i++)
						  if (strlen(scrollname[i])>2) /* no null items */
							{ item[i][0]=OSCROLL; iarg[i][0]=i; }
						for (i=MAXX-1; i>MAXX-1-MAXPOTION; i--)
						  if (strlen(potionname[i-MAXX+MAXPOTION])>2) /* no null items */
							{ item[i][0]=OPOTION; iarg[i][0]=i-MAXX+MAXPOTION; }
						for (i=1; i<MAXY; i++)
							{ item[0][i]=i; iarg[0][i]=0; }
						for (i=MAXY; i<MAXY+MAXX; i++)
							{ item[i-MAXY][MAXY-1]=i; iarg[i-MAXY][MAXY-1]=0; }
						for (i=MAXX+MAXY; i<MAXX+MAXY+MAXY; i++)
							{ item[MAXX-1][i-MAXX-MAXY]=i; iarg[MAXX-1][i-MAXX-MAXY]=0; }
						c[GOLD]+=25000;	drawscreen();	return;
#endif

			case 'T':	yrepcount=0;	cursors();  if (c[SHIELD] != -1) { c[SHIELD] = -1; lprcat("\nYour shield is off"); bottomline(); } else
										if (c[WEAR] != -1) { c[WEAR] = -1; lprcat("\nYour armor is off"); bottomline(); }
						else lprcat("\nYou aren't wearing anything");
						return;

			case 'g':	cursors();
						lprintf("\nThe stuff you are carrying presently weighs %d pounds",(long)packweight());
			case ' ':	yrepcount=0;	nomove=1;  return;

			case 'v':	yrepcount=0;	cursors();
						lprintf("\nCaverns of Larn, Version %d.%d, Diff=%d",(long)VERSION,(long)SUBVERSION,(long)c[HARDGAME]);
						if (wizard) lprcat(" Wizard"); nomove=1;
						if (cheat) lprcat(" Cheater");
						lprcat(copyright);
						return;

			case 'Q':	yrepcount=0;	quit(); nomove=1;	return;	/*	quit		*/

			case 'L'-64:  yrepcount=0;	drawscreen();  nomove=1; return;	/*	look		*/

#if WIZID
#ifdef EXTRA
			case 'A':	yrepcount=0;	nomove=1; if (wizard) { diag(); return; }  /*	create diagnostic file */
						return;
#endif
#endif
			case 'P':	cursors();
						if (outstanding_taxes>0)
							lprintf("\nYou presently owe %d gp in taxes.",(long)outstanding_taxes);
						else
							lprcat("\nYou do not owe any taxes.");
						return;
			};
		}
	}

parse2()
	{
	if (c[HASTEMONST]) movemonst(); movemonst(); /*	move the monsters		*/
	randmonst();	regen();
	}

run(dir)
	int dir;
	{
	int i;
	i=1; while (i)
		{
		i=moveplayer(dir);
		if (i>0) {  if (c[HASTEMONST]) movemonst();  movemonst(); randmonst(); regen(); }
		if (hitflag) i=0;
		if (i!=0)  showcell(playerx,playery);
		}
	}

/*
	function to wield a weapon
 */
wield()
	{
	int i;
	while (1)
		{
		if ((i = whatitem("wield"))=='\33')  return;
		if (i != '.')
			{
			if (i=='*') showwield();
			else  if (iven[i-'a']==0) { ydhi(i); return; }
			else if (iven[i-'a']==OPOTION) { ycwi(i); return; }
			else if (iven[i-'a']==OSCROLL) { ycwi(i); return; }
			else  if ((c[SHIELD]!= -1) && (iven[i-'a']==O2SWORD)) { lprcat("\nBut one arm is busy with your shield!"); return; }
			else  { c[WIELD]=i-'a'; if (iven[i-'a'] == OLANCE) c[LANCEDEATH]=1; else c[LANCEDEATH]=0;  bottomline(); return; }
			}
		}
	}

/*
	common routine to say you don't have an item
 */
ydhi(x)
	int x;
	{ cursors();  lprintf("\nYou don't have item %c!",x); }
ycwi(x)
	int x;
	{ cursors();  lprintf("\nYou can't wield item %c!",x); }

/*
	function to wear armor
 */
wear()
	{
	int i;
	while (1)
		{
		if ((i = whatitem("wear"))=='\33')  return;
		if (i != '.')
			{
			if (i=='*') showwear(); else
			switch(iven[i-'a'])
				{
				case 0:  ydhi(i); return;
				case OLEATHER:  case OCHAIN:  case OPLATE:	case OSTUDLEATHER:
				case ORING:		case OSPLINT:	case OPLATEARMOR:	case OSSPLATE:
						if (c[WEAR] != -1) { lprcat("\nYou're already wearing some armor"); return; }
							c[WEAR]=i-'a';  bottomline(); return;
				case OSHIELD:	if (c[SHIELD] != -1) { lprcat("\nYou are already wearing a shield"); return; }
								if (iven[c[WIELD]]==O2SWORD) { lprcat("\nYour hands are busy with the two handed sword!"); return; }
								c[SHIELD] = i-'a';  bottomline(); return;
				default:	lprcat("\nYou can't wear that!");
				};
			}
		}
	}

/*
	function to drop an object
 */
dropobj()
	{
	int i;
	char *p;
	long amt;
	p = &item[playerx][playery];
	while (1)
		{
		if ((i = whatitem("drop"))=='\33')  return;
		if (i=='*') showstr(); else
			{
			if (i=='.')	/* drop some gold */
				{
				if (*p) { lprcat("\nThere's something here already!"); return; }
				lprcat("\n\n");
				cl_dn(1,23);
				lprcat("How much gold do you drop? ");
				if ((amt=readnum((long)c[GOLD])) == 0) return;
				if (amt>c[GOLD])
					{ lprcat("\nYou don't have that much!"); return; }
				if (amt<=32767)
					{ *p=OGOLDPILE; i=amt; }
				else if (amt<=327670L)
					{ *p=ODGOLD; i=amt/10; amt = 10*i; }
				else if (amt<=3276700L)
					{ *p=OMAXGOLD; i=amt/100; amt = 100*i; }
				else if (amt<=32767000L)
					{ *p=OKGOLD; i=amt/1000; amt = 1000*i; }
				else
					{ *p=OKGOLD; i=32767; amt = 32767000L; }
				c[GOLD] -= amt;
				lprintf("You drop %d gold pieces",(long)amt);
				iarg[playerx][playery]=i; bottomgold();
				know[playerx][playery]=0; dropflag=1;  return;
				}
			drop_object(i-'a');
			return;
			}
		}
	}

/*
 *	readscr()		Subroutine to read a scroll one is carrying
 */
readscr()
	{
	int i;
	while (1)
		{
		if ((i = whatitem("read"))=='\33')  return;
		if (i != '.')
			{
			if (i=='*') showread(); else
				{
				if (iven[i-'a']==OSCROLL) { read_scroll(ivenarg[i-'a']); iven[i-'a']=0; return; }
				if (iven[i-'a']==OBOOK)   { readbook(ivenarg[i-'a']);  iven[i-'a']=0; return; }
				if (iven[i-'a']==0) { ydhi(i); return; }
				lprcat("\nThere's nothing on it to read");  return;
				}
			}
		}
	}

/*
 *	subroutine to eat a cookie one is carrying
 */
eatcookie()
{
int i;
char *p;
while (1)
	{
	if ((i = whatitem("eat"))=='\33')  return;
	if (i != '.') {
		if (i=='*') showeat(); else
			{
			if (iven[i-'a']==OCOOKIE)
				{
				lprcat("\nThe cookie was delicious.");
				iven[i-'a']=0;
				if (!c[BLINDCOUNT])
					{
					if (p=fortune(fortfile))
						{
						lprcat("  Inside you find a scrap of paper that says:\n");
						lprcat(p);
						}
					}
				return;
				}
			if (iven[i-'a']==0) { ydhi(i); return; }
			lprcat("\nYou can't eat that!");  return;
			}
		 }
	}
}

/*
 *	subroutine to quaff a potion one is carrying
 */
quaff()
	{
	int i;
	while (1)
		{
		if ((i = whatitem("quaff"))=='\33')  return;
		if (i != '.')
			{
			if (i=='*') showquaff(); else
				{
				if (iven[i-'a']==OPOTION) { quaffpotion(ivenarg[i-'a']); iven[i-'a']=0; return; }
				if (iven[i-'a']==0) { ydhi(i); return; }
				lprcat("\nYou wouldn't want to quaff that, would you? ");  return;
				}
			}
		}
	}

/*
	function to ask what player wants to do
 */
whatitem(str)
	char *str;
	{
	int i;
	cursors();  lprintf("\nWhat do you want to %s [* for all] ? ",str);
	i=0; while (i>'z' || (i<'a' && i!='*' && i!='\33' && i!='.')) i=getchar();
	if (i=='\33')  lprcat(" aborted");
	return(i);
	}

/*
	subroutine to get a number from the player
	and allow * to mean return amt, else return the number entered
 */
unsigned long readnum(mx)
	long mx;
	{
	int i;
	unsigned long amt=0;
	sncbr();
	if ((i=getchar()) == '*')  amt = mx;   /* allow him to say * for all gold */
	else
		while (i != '\n')
			{
			if (i=='\033') { scbr(); lprcat(" aborted"); return(0); }
			if ((i <= '9') && (i >= '0') && (amt<99999999))
				amt = amt*10+i-'0';
			i = getchar();
			}
	scbr();  return(amt);
	}

#ifdef HIDEBYLINK
/*
 *	routine to zero every byte in a string
 */
szero(str)
	char *str;
	{
	while (*str)
		*str++ = 0;
	}
#endif HIDEBYLINK
