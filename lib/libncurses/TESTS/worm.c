/*

	 @@@        @@@    @@@@@@@@@@     @@@@@@@@@@@    @@@@@@@@@@@@
	 @@@        @@@   @@@@@@@@@@@@    @@@@@@@@@@@@   @@@@@@@@@@@@@
	 @@@        @@@  @@@@      @@@@   @@@@           @@@@ @@@  @@@@
	 @@@   @@   @@@  @@@        @@@   @@@            @@@  @@@   @@@
	 @@@  @@@@  @@@  @@@        @@@   @@@            @@@  @@@   @@@
	 @@@@ @@@@ @@@@  @@@        @@@   @@@            @@@  @@@   @@@
	  @@@@@@@@@@@@   @@@@      @@@@   @@@            @@@  @@@   @@@
	   @@@@  @@@@     @@@@@@@@@@@@    @@@            @@@  @@@   @@@
	    @@    @@       @@@@@@@@@@     @@@            @@@  @@@   @@@

				 Eric P. Scott
			  Caltech High Energy Physics
				 October, 1980

*/

#include <ncurses.h>
#include <signal.h>

#define cursor(col,row) move(row,col)

int Wrap;
short *ref[128];
static char flavor[]={
    'O', '*', '#', '$', '%', '0'
};
static short xinc[]={
     1,  1,  1,  0, -1, -1, -1,  0
}, yinc[]={
    -1,  0,  1,  1,  1,  0, -1, -1
};
static struct worm {
    int orientation, head;
    short *xpos, *ypos;
} worm[40];
static char *field;
static int length=16, number=3, trail=' ';
static struct options {
    int nopts;
    int opts[3];
} normal[8]={
    { 3, { 7, 0, 1 } },
    { 3, { 0, 1, 2 } },
    { 3, { 1, 2, 3 } },
    { 3, { 2, 3, 4 } },
    { 3, { 3, 4, 5 } },
    { 3, { 4, 5, 6 } },
    { 3, { 5, 6, 7 } },
    { 3, { 6, 7, 0 } }
}, upper[8]={
    { 1, { 1, 0, 0 } },
    { 2, { 1, 2, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 2, { 4, 5, 0 } },
    { 1, { 5, 0, 0 } },
    { 2, { 1, 5, 0 } }
}, left[8]={
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 2, { 2, 3, 0 } },
    { 1, { 3, 0, 0 } },
    { 2, { 3, 7, 0 } },
    { 1, { 7, 0, 0 } },
    { 2, { 7, 0, 0 } }
}, right[8]={
    { 1, { 7, 0, 0 } },
    { 2, { 3, 7, 0 } },
    { 1, { 3, 0, 0 } },
    { 2, { 3, 4, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 2, { 6, 7, 0 } }
}, lower[8]={
    { 0, { 0, 0, 0 } },
    { 2, { 0, 1, 0 } },
    { 1, { 1, 0, 0 } },
    { 2, { 1, 5, 0 } },
    { 1, { 5, 0, 0 } },
    { 2, { 5, 6, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } }
}, upleft[8]={
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 1, { 3, 0, 0 } },
    { 2, { 1, 3, 0 } },
    { 1, { 1, 0, 0 } }
}, upright[8]={
    { 2, { 3, 5, 0 } },
    { 1, { 3, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 1, { 5, 0, 0 } }
}, lowleft[8]={
    { 3, { 7, 0, 1 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 1, { 1, 0, 0 } },
    { 2, { 1, 7, 0 } },
    { 1, { 7, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } }
}, lowright[8]={
    { 0, { 0, 0, 0 } },
    { 1, { 7, 0, 0 } },
    { 2, { 5, 7, 0 } },
    { 1, { 5, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } }
};

void onsig();
float ranf();

main(argc,argv)
int argc;
char *argv[];
{
int x, y;
int n;
struct worm *w;
struct options *op;
int h;
short *ip;
int last, bottom;

    for (x=1;x<argc;x++) {
		register char *p;
		p=argv[x];
		if (*p=='-') p++;
		switch (*p) {
		case 'f':
		    field="WORM";
		    break;
		case 'l':
		    if (++x==argc) goto usage;
		    if ((length=atoi(argv[x]))<2||length>1024) {
				fprintf(stderr,"%s: Invalid length\n",*argv);
				exit(1);
		    }
		    break;
		case 'n':
		    if (++x==argc) goto usage;
		    if ((number=atoi(argv[x]))<1||number>40) {
				fprintf(stderr,"%s: Invalid number of worms\n",*argv);
				exit(1);
		    }
		    break;
		case 't':
		    trail='.';
		    break;
		default:
		usage:
		    fprintf(stderr, "usage: %s [-field] [-length #] [-number #] [-trail]\n",*argv);
		    exit(1);
		    break;
		}
    }

    signal(SIGINT, onsig);
    initscr();
    bottom = LINES-1;
    last = COLS-1;

    ip=(short *)malloc(LINES*COLS*sizeof (short));

    for (n=0;n<LINES;) {
		ref[n++]=ip; ip+=COLS;
    }
    for (ip=ref[0],n=LINES*COLS;--n>=0;) *ip++=0;
    ref[bottom][last]=1;
    for (n=number, w= &worm[0];--n>=0;w++) {
		w->orientation=w->head=0;
		if (!(ip=(short *)malloc(length*sizeof (short)))) {
		    fprintf(stderr,"%s: out of memory\n",*argv);
		    exit(1);
		}
		w->xpos=ip;
		for (x=length;--x>=0;) *ip++ = -1;
		if (!(ip=(short *)malloc(length*sizeof (short)))) {
		    fprintf(stderr,"%s: out of memory\n",*argv);
		    exit(1);
		}
		w->ypos=ip;
		for (y=length;--y>=0;) *ip++ = -1;
    }
    if (field) {
		register char *p;
		p=field;
		for (y=bottom;--y>=0;) {
		    for (x=COLS;--x>=0;) {
				addch(*p++);
				if (!*p) p=field;
		    }
            addch('\n');
        }
    }
    refresh();

    for (;;) {
		for (n=0,w= &worm[0];n<number;n++,w++) {
		    if ((x=w->xpos[h=w->head])<0) {
				cursor(x=w->xpos[h]=0,y=w->ypos[h]=bottom);
				addch(flavor[n%6]);
				ref[y][x]++;
		    }
		    else y=w->ypos[h];
		    if (++h==length) h=0;
		    if (w->xpos[w->head=h]>=0) {
				register int x1, y1;
				x1=w->xpos[h]; y1=w->ypos[h];
				if (--ref[y1][x1]==0) {
				    cursor(x1,y1); addch(trail);
				}
		    }
            op= &(x==0 ? (y==0 ? upleft : (y==bottom ? lowleft : left)) :
                (x==last ? (y==0 ? upright : (y==bottom ? lowright : right)) :
			(y==0 ? upper : (y==bottom ? lower : normal))))[w->orientation];
		    switch (op->nopts) {
		    case 0:
				refresh();
				endwin();
				exit(0);
		    case 1:
				w->orientation=op->opts[0];
				break;
		    default:
				w->orientation=op->opts[(int)(ranf()*(float)op->nopts)];
		    }
		    cursor(x+=xinc[w->orientation], y+=yinc[w->orientation]);
		    if (!Wrap||x!=last||y!=bottom) addch(flavor[n%6]);
		    ref[w->ypos[h]=y][w->xpos[h]=x]++;
		}
		refresh();
    }
}

void
onsig()
{
	endwin();
	exit(0);
}

float
ranf()
{
float rv;
long r = rand();

    r &= 077777;
    rv =((float)r/32767.);
    return rv;
}
