/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)externs.h	8.1 (Berkeley) 5/31/93
 */

#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdlib.h>
#include "machdep.h"

	/* program mode */
int mode;
jmp_buf restart;
#define MODE_PLAYER	1
#define MODE_DRIVER	2
#define MODE_LOGGER	3

	/* command line flags */
char debug;				/* -D */
char randomize;				/* -x, give first available ship */
char longfmt;				/* -l, print score in long format */
char nobells;				/* -b, don't ring bell before Signal */

	/* other initial modes */
char issetuid;				/* running setuid */

#define die()           (random() % 6 + 1)
#define sqr(a)		((a) * (a))
#define abs(a)		((a) > 0 ? (a) : -(a))
#define min(a,b)	((a) < (b) ? (a) : (b))

#define grappled(a)	((a)->file->ngrap)
#define fouled(a)	((a)->file->nfoul)
#define snagged(a)	(grappled(a) + fouled(a))

#define grappled2(a, b)	((a)->file->grap[(b)->file->index].sn_count)
#define fouled2(a, b)	((a)->file->foul[(b)->file->index].sn_count)
#define snagged2(a, b)	(grappled2(a, b) + fouled2(a, b))

#define Xgrappled2(a, b) ((a)->file->grap[(b)->file->index].sn_turn < turn-1 ? grappled2(a, b) : 0)
#define Xfouled2(a, b)	((a)->file->foul[(b)->file->index].sn_turn < turn-1 ? fouled2(a, b) : 0)
#define Xsnagged2(a, b)	(Xgrappled2(a, b) + Xfouled2(a, b))

#define cleangrapple(a, b, c)	Cleansnag(a, b, c, 1)
#define cleanfoul(a, b, c)	Cleansnag(a, b, c, 2)
#define cleansnag(a, b, c)	Cleansnag(a, b, c, 3)

#define sterncolour(sp)	((sp)->file->stern+'0'-((sp)->file->captured?10:0))
#define sternrow(sp)	((sp)->file->row + dr[(sp)->file->dir])
#define sterncol(sp)	((sp)->file->col + dc[(sp)->file->dir])

#define capship(sp)	((sp)->file->captured?(sp)->file->captured:(sp))

#define readyname(r)	((r) & R_LOADING ? '*' : ((r) & R_INITIAL ? '!' : ' '))

/* loadL and loadR, should match loadname[] */
#define L_EMPTY		0		/* should be 0, don't change */
#define L_GRAPE		1
#define L_CHAIN		2
#define L_ROUND		3
#define L_DOUBLE	4
#define L_EXPLODE	5

/*
 * readyL and readyR, these are bits, except R_EMPTY
 */
#define R_EMPTY		0		/* not loaded and not loading */
#define R_LOADING	1		/* loading */
#define R_DOUBLE	2		/* loading double */
#define R_LOADED	4		/* loaded */
#define R_INITIAL	8		/* loaded initial */

#define HULL		0
#define RIGGING		1

#define W_CAPTAIN	1
#define W_CAPTURED	2
#define W_CLASS		3
#define W_CREW		4
#define W_DBP		5
#define W_DRIFT		6
#define W_EXPLODE	7
#define W_FILE		8
#define W_FOUL		9
#define W_GUNL		10
#define W_GUNR		11
#define W_HULL		12
#define W_MOVE		13
#define W_OBP		14
#define W_PCREW		15
#define W_UNFOUL	16
#define W_POINTS	17
#define W_QUAL		18
#define W_UNGRAP	19
#define W_RIGG		20
#define W_COL		21
#define W_DIR		22
#define W_ROW		23
#define W_SIGNAL	24
#define W_SINK		25
#define W_STRUCK	26
#define W_TA		27
#define W_ALIVE		28
#define W_TURN		29
#define W_WIND		30
#define W_FS		31
#define W_GRAP		32
#define W_RIG1		33
#define W_RIG2		34
#define W_RIG3		35
#define W_RIG4		36
#define W_BEGIN		37
#define W_END		38
#define W_DDEAD		39

#define NLOG 10
struct logs {
	char l_name[20];
	int l_uid;
	int l_shipnum;
	int l_gamenum;
	int l_netpoints;
};

struct BP {
	short turnsent;
	struct ship *toship;
	short mensent;
};

struct snag {
	short sn_count;
	short sn_turn;
};

#define NSCENE	nscene
#define NSHIP	10
#define NBP	3

#define NNATION	8
#define N_A	0
#define N_B	1
#define N_S	2
#define N_F	3
#define N_J	4
#define N_D	5
#define N_K	6
#define N_O	7

struct File {
	int index;
	char captain[20];		/* 0 */
	short points;			/* 20 */
	char loadL;			/* 22 */
	char loadR;			/* 24 */
	char readyL;			/* 26 */
	char readyR;			/* 28 */
	struct BP OBP[NBP];		/* 30 */
	struct BP DBP[NBP];		/* 48 */
	char struck;			/* 66 */
	struct ship *captured;		/* 68 */
	short pcrew;			/* 70 */
	char movebuf[60];		/* 72 */
	char drift;			/* 132 */
	short nfoul;
	short ngrap;
	struct snag foul[NSHIP];	/* 134 */
	struct snag grap[NSHIP];	/* 144 */
	char RH;			/* 274 */
	char RG;			/* 276 */
	char RR;			/* 278 */
	char FS;			/* 280 */
	char explode;			/* 282 */
	char sink;			/* 284 */
	char dir;
	short col;
	short row;
	char loadwith;
	char stern;
};

struct ship {
	char *shipname;			/* 0 */
	struct shipspecs *specs;	/* 2 */
	char nationality;		/* 4 */
	short shiprow;			/* 6 */
	short shipcol;			/* 8 */
	char shipdir;			/* 10 */
	struct File *file;		/* 12 */
};

struct scenario {
	char winddir;			/* 0 */
	char windspeed;			/* 2 */
	char windchange;		/* 4 */
	char vessels;			/* 12 */
	char *name;			/* 14 */
	struct ship ship[NSHIP];	/* 16 */
};
extern struct scenario scene[];
int nscene;

struct shipspecs {
	char bs;
	char fs;
	char ta;
	short guns;
	char class;
	char hull;
	char qual;
	char crew1;
	char crew2;
	char crew3;
	char gunL;
	char gunR;
	char carL;
	char carR;
	char rig1;
	char rig2;
	char rig3;
	char rig4;
	short pts;
};
extern struct shipspecs specs[];

struct scenario *cc;		/* the current scenario */
struct ship *ls;		/* &cc->ship[cc->vessels] */

#define SHIP(s)		(&cc->ship[s])
#define foreachship(sp)	for ((sp) = cc->ship; (sp) < ls; (sp)++)

struct windeffects {
	char A, B, C, D;
};
struct windeffects WET[7][6];

struct Tables {
	char H, G, C, R;
};
struct Tables RigTable[11][6];
struct Tables HullTable[11][6];

char AMMO[9][4];
char HDT[9][10];
char HDTrake[9][10];
char QUAL[9][5];
char MT[9][3];

extern char *countryname[];
extern char *classname[];
extern char *directionname[];
extern char *qualname[];
extern char loadname[];

extern char rangeofshot[];

extern char dr[], dc[];

int winddir;
int windspeed;
int turn;
int game;
int alive;
int people;
char hasdriver;

char *info();
char *quality();
double arctan();
char *saywhat();
struct ship *closestenemy();

char *rindex();
char *strcpy();
char *strcat();
char *strncpy();
char *getenv();
char *gets();
