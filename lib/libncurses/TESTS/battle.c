/* 
 * battle.c - original author: Bruce Holloway
 *		mods by: Chuck A DeGaul
 */

#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <ncurses.h>

#define	OTHER	1-turn

char numbers[] = "   0  1  2  3  4  5  6  7  8  9";

char carrier[] = "Aircraft Carrier";
char battle[] = "Battleship";
char sub[] = "Submarine";
char destroy[] = "Destroyer";
char ptboat[] = "PT Boat";

char name[40];
char dftname[] = "Stranger";

struct _ships {
    char *name;
    char symbol;
    char length;
    char start;		/* Coordinates - 0,0=0; 10,10=100. */
    char dir;		/* Direction - 0 = right; 1 = down. */
    char hits;		/* How many times has this ship been hit? (-1==sunk) */
};

struct _ships plyship[] = {
    { carrier,'A',5,0,0,0 },
    { battle,'B',4,0,0,0 },
    { destroy,'D',3,0,0,0 },
    { sub,'S',3,0,0,0 },
    { ptboat,'P',2,0,0,0 },
};

struct _ships cpuship[] = {
    { carrier,'A',5,0,0,0 },
    { battle,'B',4,0,0,0 },
    { destroy,'D',3,0,0,0 },
    { sub,'S',3,0,0,0 },
    { ptboat,'P',2,0,0,0 },
};

char hits[2][100], board[2][100];	/* "Hits" board, and main board. */

int srchstep;
int cpuhits;
int cstart, cdir;
int plywon=0, cpuwon=0;			/* How many games has each won? */
int turn;				/* 0=player, 1=computer */
int huntoffs;				/* Offset on search strategy */

int salvo, blitz, ask, seemiss;		/* options */

void intro(void);
void initgame(void);
int rnd(int);
void plyplace(struct _ships *);
int getdir(void);
void placeship(struct _ships *, int, int);
int checkplace(struct _ships *, int, int);
void error(char *);
void prompt(void);
char getcoord(void);
void cpuplace(struct _ships *);
int awinna(void);
int plyturn(void);
int hitship(int);
int cputurn(void);
int playagain(void);
void uninitgame();
int sgetc(char *);
int do_options(int, char *[]);
int scount(int);

int
main(int argc, char **argv)
{
    do_options(argc, argv);

    intro();
    do {
		initgame();
		while(awinna() == -1) {
		    if (!blitz) {
				if (!salvo) {
		    	    if (turn) 
		    	    	cputurn(); 
		    	    else plyturn();
				} else {
				     register int i;

				     i = scount(turn);
				     while (i--) {
					 	if (turn) 
					 	    if (cputurn()) 
							 	if (awinna() != -1) 
							     	i = 0;
					 	else 
					 	    if(plyturn()) 
							 	if (awinna() != -1) 
							     	i = 0;
				 	}
			    }
		    } else {
		    	while((turn) ? cputurn() : plyturn());
		    }
		    turn = OTHER;
		}
    } while(playagain());
    uninitgame();
    exit(0);
}

#define	PR	addstr

void
intro()
{
char *tmpname;

    srand(time(0L));			/* Kick the random number generator */

    signal(SIGINT,uninitgame);
    if(signal(SIGQUIT,SIG_IGN) != SIG_IGN) signal(SIGQUIT,uninitgame);
#if 1
	/* for some bizzare reason, getlogin or cuserid cause havoc with the terminal */
	if ((tmpname = getlogin()) != NULL) {
		strcpy(name, tmpname);
	} else
#endif
	strcpy(name,dftname);
    name[0] = toupper(name[0]);

    initscr();
    savetty();
    nonl(); 
    cbreak(); 
    noecho();
    clear();
    mvaddstr(4,29,"Welcome to Battleship!");
    move(8,0);
PR("                                                  \\\n");
PR("                           \\                     \\ \\\n");
PR("                          \\ \\                   \\ \\ \\_____________\n");
PR("                         \\ \\ \\_____________      \\ \\/            |\n");
PR("                          \\ \\/             \\      \\/             |\n");
PR("                           \\/               \\_____/              |__\n");
PR("           ________________/                                       |\n");
PR("           \\  S.S. Penguin                                         |\n");
PR("            \\                                                     /\n");
PR("             \\___________________________________________________/\n");
    mvaddstr(20,27,"Hit any key to continue..."); refresh();
    getch();
}

void
initgame()
{
int i;

    clear();
    mvaddstr(0,35,"BATTLESHIP");
    mvaddstr(4,12,"Main Board");
    mvaddstr(6,0,numbers);
    move(7,0);
    for(i=0; i<10; ++i){
		printw("%c  .  .  .  .  .  .  .  .  .  .  %c\n",i+'A',i+'A');
	}
    mvaddstr(17,0,numbers);
    mvaddstr(4,55,"Hit/Miss Board");
    mvaddstr(6,45,numbers);
    for(i=0; i<10; ++i){
		mvprintw(7+i,45,"%c  .  .  .  .  .  .  .  .  .  .  %c",i+'A',i+'A');
	}
    mvaddstr(17,45,numbers);
    for(turn=0; turn<2; ++turn)
	for(i=0; i<100; ++i){
	    hits[turn][i] = board[turn][i] = 0;
    }
    for(turn=0; turn<2; ++turn){
	for(i=0; i<5; ++i)
	    if (!turn) 
	    	plyplace(&plyship[i]);
	    else 
	    	cpuplace(&cpuship[i]);
	}
    turn = rnd(2);
    cstart = cdir = -1;
    cpuhits = 0;
    srchstep = 3;
    huntoffs = rnd(srchstep);
}

int
rnd(int n)
{
    return(((rand() & 0x7FFF) % n));
}

void
plyplace(ss)
struct _ships *ss;
{
int c, d;

    do {
		prompt();
		printw("Place your %s (ex.%c%d) ? ",ss->name,rnd(10)+'A',rnd(10));
		c = getcoord();
		d = getdir();
	} while(!checkplace(ss,c,d));
    placeship(ss,c,d);
}

int 
getdir()
{

    prompt(); 
    addstr("What direction (0=right, 1=down) ? ");
    return(sgetc("01")-'0');
}

void
placeship(ss,c,d)
struct _ships *ss;
int c, d;
{
int x, y, l, i;

    for(l=0; l<ss->length; ++l){
		i = c + l * ((d) ? 10 : 1);
		board[turn][i] = ss->symbol;
		x = (i % 10) * 3 + 3;
		y = (i / 10) + 7;
		if(!turn) mvaddch(y,x,ss->symbol);
	}
    ss->start = c;
    ss->dir = d;
    ss->hits = 0;
}

int
checkplace(ss,c,d)
struct _ships *ss;
int c, d;
{
int x, y, l;

    x = c%10; y = c/10;
    if(((x+ss->length) > 10 && !d) || ((y+ss->length) > 10 && d==1)){
	if(!turn)
	    switch(rnd(3)){
		case 0:
		    error("Ship is hanging from the edge of the world");
		    break;
		case 1:
		    error("Try fitting it on the board");
		    break;
		case 2:
		    error("Figure I won't find it if you put it there?");
		    break;
		}
	return(0);
	}
    for(l=0; l<ss->length; ++l){
	x = c + l * ((d) ? 10 : 1);
	if(board[turn][x]){
	    if(!turn)
		switch(rnd(3)){
		    case 0:
			error("There's already a ship there");
			break;
		    case 1:
			error("Collision alert! Aaaaaagh!");
			break;
		    case 2:
			error("Er, Admiral, what about the other ship?");
			break;
		    }
	    return(0);
	    }
	}
    return(1);
}

void
error(s)
char *s;
{
    prompt(); 
    beep();
    printw("%s -- hit any key to continue --",s);
    refresh();
    getch();
}

void
prompt(){
    move(22,0); 
    clrtoeol();
}

char
getcoord()
{
int ch, x, y;

redo:
    y = sgetc("ABCDEFGHIJ");
    do {
		ch = getch();
		if (ch == 0x7F || ch == 8) {
	    	addstr("\b \b"); 
	    	refresh();
	    	goto redo;
	    }
	} while(ch < '0' || ch > '9');
    addch(x = ch); 
    refresh();
    return((y-'A')*10+x-'0');
}

void
cpuplace(ss)
struct _ships *ss;
{
int c, d;

    do{
		c = rnd(100); 
		d = rnd(2);
	} while(!checkplace(ss,c,d));
    placeship(ss,c,d);
}

int
awinna()
{
int i, j;
struct _ships *ss;

    for (i = 0; i < 2; ++i) {
		ss = (i) ? cpuship : plyship;
		for(j=0; j<5; ++j, ++ss)
		    if(ss->length != ss->hits)
				break;
		if(j == 5) return(OTHER);
	}
    return(-1);
}

int
plyturn()
{
int c, res;
char *m;

    prompt();
    addstr("Where do you want to shoot? ");
    c = getcoord();
    if(!(res = hits[turn][c])){
	hits[turn][c] = res = (board[OTHER][c]) ? 'H' : 'M';
	mvaddch(7+c/10,48+3*(c%10),(res=='H') ? 'H' : 'o');
	if(c = hitship(c)){
	    prompt();
	    switch(rnd(3)){
		case 0:
		    m = "You sank my %s!";
		    break;
		case 1:
		    m = "I have this sinking feeling about my %s....";
		    break;
		case 2:
		    m = "Have some mercy for my %s!";
		    break;
		}
	    move(23,0); 
	    clrtoeol(); 
	    beep();
	    printw(m,cpuship[c-1].name); refresh();
	    return(awinna() == -1);
	    }
	}
    prompt();
    move(23,0); clrtoeol();
    printw("You %s.",(res=='M')?"missed":"scored a hit"); refresh();
    return(res == 'H');
}

int
hitship(c)
int c;
{
struct _ships *ss;
int sym, i, j;

    ss = (turn) ? plyship : cpuship;
    if (!(sym = board[OTHER][c])) return(0);
    for (i = 0; i < 5; ++i, ++ss)
	if (ss->symbol == sym) {
	    j = ss->hits; 
	    ++j; 
	    ss->hits = j;
	    if (j == ss->length) 
	    	return(i+1);
	    return(0);
	 }
}

int 
cputurn()
{
int c, res, x, y, i, d;

redo:
    if (cstart == -1){
		if (cpuhits){
		    for(i=0, c=rnd(100); i<100; ++i, c = (c+1) % 100)
			if(hits[turn][c] == 'H')
			    break;
		    if(i != 100){
				cstart = c;
				cdir = -1;
				goto fndir;
			}
	    }
		do {
		    i = 0;
		    do{
				while(hits[turn][c=rnd(100)]);
				x = c % 10; y = c / 10;
				if(++i == 1000) break;
			} while(((x+huntoffs) % srchstep) != (y % srchstep));
		    if(i == 1000) --srchstep;
	    } while(i == 1000);
	}
    else if(cdir == -1){
fndir:	for(i=0, d=rnd(4); i++ < 4; d = (d+1) % 4){
		    x = cstart%10; y = cstart/10;
		    switch(d){
				case 0: ++x; break;
				case 1: ++y; break;
				case 2: --x; break;
				case 3: --y; break;
			}
		    if(x<0 || x>9 || y<0 || y>9) continue;
		    if(hits[turn][c=y*10+x]) continue;
		    cdir = -2;
		    break;
	    }
		if(i == 4){
		    cstart = -1;
		    goto redo;
	    }
	}
    else{
		x = cstart%10; y = cstart/10;
		switch(cdir){
		    case 0: ++x; break;
		    case 1: ++y; break;
		    case 2: --x; break;
		    case 3: --y; break;
	    }
		if(x<0 || x>9 || y<0 || y>9 || hits[turn][y*10+x]){
		    cdir = (cdir+2) % 4;
		    for(;;){
				switch(cdir){
				    case 0: ++x; break;
				    case 1: ++y; break;
				    case 2: --x; break;
				    case 3: --y; break;
			    }
				if(x<0 || x>9 || y<0 || y>9){ cstart = -1; 
				goto redo; 
			}
			if(!hits[turn][y*10+x]) break;
		}
    }
	c = y*10 + x;
	}

    if (!ask) {
        res = (board[OTHER][c]) ? 'H' : 'M';
        move(21,0); clrtoeol();
        printw("I shoot at %c%d. I %s!",c/10+'A',c%10,(res=='H')?"hit":"miss");
    } else {
        for(;;){
	    prompt();
	    printw("I shoot at %c%d. Do I (H)it or (M)iss? ",c/10+'A',c%10);
	    res = sgetc("HM");
	    if((res=='H' && !board[OTHER][c]) || (res=='M' && board[OTHER][c])){
	        error("You lie!");
	        continue;
	        }
	    break;
	    }
        addch(res);
    }
    hits[turn][c] = res;
    if(res == 'H') {
	++cpuhits;
	if(cstart == -1) cdir = -1;
	cstart = c;
	if(cdir == -2) cdir = d;
	mvaddch(7+(c/10),3+3*(c%10),'*');
	if (blitz && !ask) {
	    refresh();
	    sleep(1);
	}
    }
    else { 
	if (seemiss) {
	    mvaddch(7+(c/10),3+3*(c%10),' ');
	} else {
	    if(cdir == -2) cdir = -1;
	}
    }
    if(c=hitship(c)){
	cstart = -1;
	cpuhits -= plyship[c-1].length;
	x = plyship[c-1].start;
	d = plyship[c-1].dir;
	y = plyship[c-1].length;
	for(i=0; i<y; ++i){
	    hits[turn][x] = '*';
	    x += (d) ? 10 : 1;
	    }
	}
    if (salvo && !ask) {
	refresh();
	sleep(1);
    }
    if(awinna() != -1) return(0);
    return(res == 'H');
}

int
playagain()
{
int i, x, y, dx, dy, j;

    for(i=0; i<5; ++i){
	x = cpuship[i].start; y = x/10+7; x = (x % 10) * 3 + 48;
	dx = (cpuship[i].dir) ? 0 : 3;
	dy = (cpuship[i].dir) ? 1 : 0;
	for(j=0; j < cpuship[i].length; ++j){
	    mvaddch(y,x,cpuship[i].symbol);
	    x += dx; y += dy;
	    }
	}

    if(awinna()) ++cpuwon; else ++plywon;
    i = 18 + strlen(name);
    if(plywon >= 10) ++i;
    if(cpuwon >= 10) ++i;
    mvprintw(2,(80-i)/2,"%s: %d     Computer: %d",name,plywon,cpuwon);

    prompt();
    printw((awinna()) ? "Want to be humiliated again, %s? "
		  : "Going to give me a chance for revenge, %s? ",name);
    return(sgetc("YN") == 'Y');
}

void
uninitgame(int sig)
{
    refresh();
    endwin();
    exit(0);
}

int
sgetc(s)
char *s;
{
char *s1;
int ch;

    refresh();
    for (;;) {
		ch = toupper(getch());
		for (s1 = s; *s1 && ch != *s1; ++s1);
		if (*s1) {
	    	addch(ch); 
	    	refresh();
	    	return(ch);
	    }
	}
}

/* 
 * I should use getopts() from libc.a, but I'm leary that other UNIX
 * systems might not have it, although I'd love to use it.
 */
 
int
do_options(c,op)
int c;
char *op[];
{
register int i;

    if (c > 1) {
		for (i=1; i<c; i++) {
		    switch(op[i][0]) {
		    default:
		    case '?':
			fprintf(stderr, "Usage: battle [ -s | -b ] [ -a ] [ -m ]\n");
			fprintf(stderr, "\tWhere the options are:\n");
			fprintf(stderr, "\t-s : play a salvo game (mutex with -b)\n");
			fprintf(stderr, "\t-b : play a blitz game (mutex with -s)\n");
			fprintf(stderr, "\t-a : computer asks you for hit/miss\n");
			fprintf(stderr, "\t-m : computer misses are displayed\n");
			exit(1);
			break;
		    case '-':
			switch(op[i][1]) {
			case 'b':
			    blitz = 1;
			    if (salvo == 1) {
				fprintf(stderr,
				    "Bad Arg: -b and -s are mutually exclusive\n");
				exit(1);
			    }
			    break;
			case 's':
			    salvo = 1;
			    if (blitz == 1) {
				fprintf(stderr,
				    "Bad Arg: -s and -b are mutually exclusive\n");
				exit(1);
			    }
			    break;
			case 'a':
			    ask = 1;
			    break;
			case 'm':
			    seemiss = 1;
			    break;
			default:
			    fprintf(stderr,
				"Bad Arg: type \"%s ?\" for usage message\n", op[0]);
			    exit(1);
			}
   		    }
		}
		fprintf(stdout, "Playing optional game (");
		if (salvo) 
		    fprintf(stdout, "salvo, noblitz, ");
		else if (blitz)
			    fprintf(stdout, "blitz, nosalvo, ");
			else 
			    fprintf(stdout, "noblitz, nosalvo, ");

		if (ask) 
		    fprintf(stdout, "ask, ");
		else 
		    fprintf(stdout, "noask, ");

		if (seemiss) 
		    fprintf(stdout, "seemiss)\n");
		else 
		    fprintf(stdout, "noseemiss)\n");
	}	
    else
	fprintf(stdout,
	    "Playing standard game (no blitz, no slavo, no ask, no seemiss)\n");
    sleep(2);
    return(0);
}

int
scount(who)
int who;
{
int i, shots;
struct _ships *sp;

    if (who) {
	/* count cpu shots */
	sp = cpuship;
    } else {
	/* count player shots */
	sp = plyship;
    }
    for (i=0, shots = 0; i<5; i++, sp++) {
	/* extra test for machines with unsigned chars! */
	if (sp->hits == (char) -1 || sp->hits >= sp->length) {
	    continue;	/* dead ship */
	} else {
	    shots++;
	}
    }
    return(shots);
}

