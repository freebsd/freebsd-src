/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.main.c - version 1.0.3 */
/* $FreeBSD$ */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "hack.h"

#ifdef QUEST
#define	gamename	"quest"
#else
#define	gamename	"hack"
#endif

extern char plname[PL_NSIZ], pl_character[PL_CSIZ];
extern struct permonst mons[CMNUM+2];
extern char genocided[60], fut_geno[];

int (*afternmv)();
int (*occupation)();
char *occtxt;			/* defined when occupation != NULL */

void done1();
void hangup();

int hackpid;				/* current pid */
int locknum;				/* max num of players */
#ifdef DEF_PAGER
char *catmore;				/* default pager */
#endif
char SAVEF[PL_NSIZ + 11] = "save/";	/* save/99999player */
char *hname;		/* name of the game (argv[0] of call) */
char obuf[BUFSIZ];	/* BUFSIZ is defined in stdio.h */

extern char *nomovemsg;
extern long wailmsg;

#ifdef CHDIR
static void chdirx();
#endif

main(argc,argv)
int argc;
char *argv[];
{
	int fd;
#ifdef CHDIR
	char *dir;
#endif

	hname = argv[0];
	hackpid = getpid();

#ifdef CHDIR			/* otherwise no chdir() */
	/*
	 * See if we must change directory to the playground.
	 * (Perhaps hack runs suid and playground is inaccessible
	 *  for the player.)
	 * The environment variable HACKDIR is overridden by a
	 *  -d command line option (must be the first option given)
	 */

	dir = getenv("HACKDIR");
	if(argc > 1 && !strncmp(argv[1], "-d", 2)) {
		argc--;
		argv++;
		dir = argv[0]+2;
		if(*dir == '=' || *dir == ':') dir++;
		if(!*dir && argc > 1) {
			argc--;
			argv++;
			dir = argv[0];
		}
		if(!*dir)
		    error("Flag -d must be followed by a directory name.");
	}
#endif

	/*
	 * Who am i? Algorithm: 1. Use name as specified in HACKOPTIONS
	 *			2. Use $USER or $LOGNAME	(if 1. fails)
	 *			3. Use getlogin()		(if 2. fails)
	 * The resulting name is overridden by command line options.
	 * If everything fails, or if the resulting name is some generic
	 * account like "games", "play", "player", "hack" then eventually
	 * we'll ask him.
	 * Note that we trust him here; it is possible to play under
	 * somebody else's name.
	 */
	{ char *s;

	  initoptions();
	  if(!*plname && (s = getenv("USER")))
		(void) strncpy(plname, s, sizeof(plname)-1);
	  if(!*plname && (s = getenv("LOGNAME")))
		(void) strncpy(plname, s, sizeof(plname)-1);
	  if(!*plname && (s = getlogin()))
		(void) strncpy(plname, s, sizeof(plname)-1);
	}

	/*
	 * Now we know the directory containing 'record' and
	 * may do a prscore().
	 */
	if(argc > 1 && !strncmp(argv[1], "-s", 2)) {
#ifdef CHDIR
		chdirx(dir,0);
#endif
		prscore(argc, argv);
		exit(0);
	}

	/*
	 * It seems he really wants to play.
	 * Remember tty modes, to be restored on exit.
	 */
	gettty();
	setbuf(stdout,obuf);
	umask(007);
	setrandom();
	startup();
	cls();
	u.uhp = 1;	/* prevent RIP on early quits */
	u.ux = FAR;	/* prevent nscr() */
	(void) signal(SIGHUP, hangup);

	/*
	 * Find the creation date of this game,
	 * so as to avoid restoring outdated savefiles.
	 */
	gethdate(hname);

	/*
	 * We cannot do chdir earlier, otherwise gethdate will fail.
	 */
#ifdef CHDIR
	chdirx(dir,1);
#endif

	/*
	 * Process options.
	 */
	while(argc > 1 && argv[1][0] == '-'){
		argv++;
		argc--;
		switch(argv[0][1]){
#ifdef WIZARD
		case 'D':
/*			if(!strcmp(getlogin(), WIZARD)) */
				wizard = TRUE;
/*			else
				printf("Sorry.\n"); */
			break;
#endif
#ifdef NEWS
		case 'n':
			flags.nonews = TRUE;
			break;
#endif
		case 'u':
			if(argv[0][2])
			  (void) strncpy(plname, argv[0]+2, sizeof(plname)-1);
			else if(argc > 1) {
			  argc--;
			  argv++;
			  (void) strncpy(plname, argv[0], sizeof(plname)-1);
			} else
				printf("Player name expected after -u\n");
			break;
		default:
			/* allow -T for Tourist, etc. */
			(void) strncpy(pl_character, argv[0]+1,
				sizeof(pl_character)-1);

			/* printf("Unknown option: %s\n", *argv); */
		}
	}

	if(argc > 1)
		locknum = atoi(argv[1]);
#ifdef MAX_NR_OF_PLAYERS
	if(!locknum || locknum > MAX_NR_OF_PLAYERS)
		locknum = MAX_NR_OF_PLAYERS;
#endif
#ifdef DEF_PAGER
	if(!(catmore = getenv("HACKPAGER")) && !(catmore = getenv("PAGER")))
		catmore = DEF_PAGER;
#endif
#ifdef MAIL
	getmailstatus();
#endif
#ifdef WIZARD
	if(wizard) (void) strcpy(plname, "wizard"); else
#endif
	if(!*plname || !strncmp(plname, "player", 4)
		    || !strncmp(plname, "games", 4))
		askname();
	plnamesuffix();		/* strip suffix from name; calls askname() */
				/* again if suffix was whole name */
				/* accepts any suffix */
#ifdef WIZARD
	if(!wizard) {
#endif
		/*
		 * check for multiple games under the same name
		 * (if !locknum) or check max nr of players (otherwise)
		 */
		(void) signal(SIGQUIT,SIG_IGN);
		(void) signal(SIGINT,SIG_IGN);
		if(!locknum)
			(void) strcpy(lock,plname);
		getlock();	/* sets lock if locknum != 0 */
#ifdef WIZARD
	} else {
		char *sfoo;
		(void) strcpy(lock,plname);
		if(sfoo = getenv("MAGIC"))
			while(*sfoo) {
				switch(*sfoo++) {
				case 'n': (void) srandom(*sfoo++);
					break;
				}
			}
		if(sfoo = getenv("GENOCIDED")){
			if(*sfoo == '!'){
				struct permonst *pm = mons;
				char *gp = genocided;

				while(pm < mons+CMNUM+2){
					if(!index(sfoo, pm->mlet))
						*gp++ = pm->mlet;
					pm++;
				}
				*gp = 0;
			} else
				(void) strncpy(genocided, sfoo, sizeof(genocided)-1);
			(void) strcpy(fut_geno, genocided);
		}
	}
#endif
	setftty();
	(void) sprintf(SAVEF, "save/%d%s", getuid(), plname);
	regularize(SAVEF+5);		/* avoid . or / in name */
	if((fd = open(SAVEF,0)) >= 0 &&
	   (uptodate(fd) || unlink(SAVEF) == 666)) {
		(void) signal(SIGINT,done1);
		pline("Restoring old save file...");
		(void) fflush(stdout);
		if(!dorecover(fd))
			goto not_recovered;
		pline("Hello %s, welcome to %s!", plname, gamename);
		flags.move = 0;
	} else {
not_recovered:
		fobj = fcobj = invent = 0;
		fmon = fallen_down = 0;
		ftrap = 0;
		fgold = 0;
		flags.ident = 1;
		init_objects();
		u_init();

		(void) signal(SIGINT,done1);
		mklev();
		u.ux = xupstair;
		u.uy = yupstair;
		(void) inshop();
		setsee();
		flags.botlx = 1;
		makedog();
		{ struct monst *mtmp;
		  if(mtmp = m_at(u.ux, u.uy)) mnexto(mtmp);	/* riv05!a3 */
		}
		seemons();
#ifdef NEWS
		if(flags.nonews || !readnews())
			/* after reading news we did docrt() already */
#endif
			docrt();

		/* give welcome message before pickup messages */
		pline("Hello %s, welcome to %s!", plname, gamename);

		pickup(1);
		read_engr_at(u.ux,u.uy);
		flags.move = 1;
	}

	flags.moonphase = phase_of_the_moon();
	if(flags.moonphase == FULL_MOON) {
		pline("You are lucky! Full moon tonight.");
		u.uluck++;
	} else if(flags.moonphase == NEW_MOON) {
		pline("Be careful! New moon tonight.");
	}

	initrack();

	for(;;) {
		if(flags.move) {	/* actual time passed */

			settrack();

			if(moves%2 == 0 ||
			  (!(Fast & ~INTRINSIC) && (!Fast || rn2(3)))) {
				extern struct monst *makemon();
				movemon();
				if(!rn2(70))
				    (void) makemon((struct permonst *)0, 0, 0);
			}
			if(Glib) glibr();
			timeout();
			++moves;
			if(flags.time) flags.botl = 1;
			if(u.uhp < 1) {
				pline("You die...");
				done("died");
			}
			if(u.uhp*10 < u.uhpmax && moves-wailmsg > 50){
			    wailmsg = moves;
			    if(u.uhp == 1)
			    pline("You hear the wailing of the Banshee...");
			    else
			    pline("You hear the howling of the CwnAnnwn...");
			}
			if(u.uhp < u.uhpmax) {
				if(u.ulevel > 9) {
					if(Regeneration || !(moves%3)) {
					    flags.botl = 1;
					    u.uhp += rnd((int) u.ulevel-9);
					    if(u.uhp > u.uhpmax)
						u.uhp = u.uhpmax;
					}
				} else if(Regeneration ||
					(!(moves%(22-u.ulevel*2)))) {
					flags.botl = 1;
					u.uhp++;
				}
			}
			if(Teleportation && !rn2(85)) tele();
			if(Searching && multi >= 0) (void) dosearch();
			gethungry();
			invault();
			amulet();
		}
		if(multi < 0) {
			if(!++multi){
				pline(nomovemsg ? nomovemsg :
					"You can move again.");
				nomovemsg = 0;
				if(afternmv) (*afternmv)();
				afternmv = 0;
			}
		}

		find_ac();
#ifndef QUEST
		if(!flags.mv || Blind)
#endif
		{
			seeobjs();
			seemons();
			nscr();
		}
		if(flags.botl || flags.botlx) bot();

		flags.move = 1;

		if(multi >= 0 && occupation) {
			if(monster_nearby())
				stop_occupation();
			else if ((*occupation)() == 0)
				occupation = 0;
			continue;
		}

		if(multi > 0) {
#ifdef QUEST
			if(flags.run >= 4) finddir();
#endif
			lookaround();
			if(!multi) {	/* lookaround may clear multi */
				flags.move = 0;
				continue;
			}
			if(flags.mv) {
				if(multi < COLNO && !--multi)
					flags.mv = flags.run = 0;
				domove();
			} else {
				--multi;
				rhack(save_cm);
			}
		} else if(multi == 0) {
#ifdef MAIL
			ckmailstatus();
#endif
			rhack((char *) 0);
		}
		if(multi && multi%7 == 0)
			(void) fflush(stdout);
	}
}

glo(foo)
int foo;
{
	/* construct the string  xlock.n  */
	char *tf;

	tf = lock;
	while(*tf && *tf != '.') tf++;
	(void) sprintf(tf, ".%d", foo);
}

/*
 * plname is filled either by an option (-u Player  or  -uPlayer) or
 * explicitly (-w implies wizard) or by askname.
 * It may still contain a suffix denoting pl_character.
 */
askname(){
int c,ct;
	printf("\nWho are you? ");
	(void) fflush(stdout);
	ct = 0;
	while((c = getchar()) != '\n'){
		if(c == EOF) error("End of input\n");
		/* some people get confused when their erase char is not ^H */
		if(c == '\010') {
			if(ct) ct--;
			continue;
		}
		if(c != '-')
		if(c < 'A' || (c > 'Z' && c < 'a') || c > 'z') c = '_';
		if(ct < sizeof(plname)-1) plname[ct++] = c;
	}
	plname[ct] = 0;
	if(ct == 0) askname();
}

/*VARARGS1*/
impossible(s,x1,x2)
char *s;
{
	pline(s,x1,x2);
	pline("Program in disorder - perhaps you'd better Quit.");
}

#ifdef CHDIR
static void
chdirx(dir, wr)
char *dir;
boolean wr;
{

#ifdef SECURE
	if(dir					/* User specified directory? */
#ifdef HACKDIR
	       && strcmp(dir, HACKDIR)		/* and not the default? */
#endif
		) {
		/* revoke */
		setgid(getgid());
	}
#endif

#ifdef HACKDIR
	if(dir == NULL)
		dir = HACKDIR;
#endif

	if(dir && chdir(dir) < 0) {
		perror(dir);
		error("Cannot chdir to %s.", dir);
	}

	/* warn the player if he cannot write the record file */
	/* perhaps we should also test whether . is writable */
	/* unfortunately the access systemcall is worthless */
	if(wr) {
	    int fd;

	    if(dir == NULL)
		dir = ".";
	    if((fd = open(RECORD, 2)) < 0) {
		printf("Warning: cannot write %s/%s", dir, RECORD);
		getret();
	    } else
		(void) close(fd);
	}
}
#endif

stop_occupation()
{
	if(occupation) {
		pline("You stop %s.", occtxt);
		occupation = 0;
	}
}
