/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.mkmaze.c - version 1.0.2 */
/* $FreeBSD: src/games/hack/hack.mkmaze.c,v 1.4 1999/11/16 10:26:37 marcel Exp $ */

#include "hack.h"
#include "def.mkroom.h"		/* not really used */
extern struct monst *makemon();
extern struct permonst pm_wizard;
extern struct obj *mkobj_at();
extern coord mazexy();
struct permonst hell_hound =
	{ "hell hound", 'd', 12, 14, 2, 3, 6, 0 };

makemaz()
{
	int x,y;
	int zx,zy;
	coord mm;
	boolean al = (dlevel >= 30 && !flags.made_amulet);

	for(x = 2; x < COLNO-1; x++)
		for(y = 2; y < ROWNO-1; y++)
			levl[x][y].typ = (x%2 && y%2) ? 0 : HWALL;
	if(al) {
	    struct monst *mtmp;

	    zx = 2*(COLNO/4) - 1;
	    zy = 2*(ROWNO/4) - 1;
	    for(x = zx-2; x < zx+4; x++) for(y = zy-2; y <= zy+2; y++) {
		levl[x][y].typ =
		    (y == zy-2 || y == zy+2 || x == zx-2 || x == zx+3) ? POOL :
		    (y == zy-1 || y == zy+1 || x == zx-1 || x == zx+2) ? HWALL:
		    ROOM;
	    }
	    (void) mkobj_at(AMULET_SYM, zx, zy);
	    flags.made_amulet = 1;
	    walkfrom(zx+4, zy);
	    if(mtmp = makemon(&hell_hound, zx, zy))
		mtmp->msleep = 1;
	    if(mtmp = makemon(PM_WIZARD, zx+1, zy)) {
		mtmp->msleep = 1;
		flags.no_of_wizards = 1;
	    }
	} else {
	    mm = mazexy();
	    zx = mm.x;
	    zy = mm.y;
	    walkfrom(zx,zy);
	    (void) mksobj_at(WAN_WISHING, zx, zy);
	    (void) mkobj_at(ROCK_SYM, zx, zy);	/* put a rock on top of it */
	}

	for(x = 2; x < COLNO-1; x++)
		for(y = 2; y < ROWNO-1; y++) {
			switch(levl[x][y].typ) {
			case HWALL:
				levl[x][y].scrsym = '-';
				break;
			case ROOM:
				levl[x][y].scrsym = '.';
				break;
			}
		}
	for(x = rn1(8,11); x; x--) {
		mm = mazexy();
		(void) mkobj_at(rn2(2) ? GEM_SYM : 0, mm.x, mm.y);
	}
	for(x = rn1(10,2); x; x--) {
		mm = mazexy();
		(void) mkobj_at(ROCK_SYM, mm.x, mm.y);
	}
	mm = mazexy();
	(void) makemon(PM_MINOTAUR, mm.x, mm.y);
	for(x = rn1(5,7); x; x--) {
		mm = mazexy();
		(void) makemon((struct permonst *) 0, mm.x, mm.y);
	}
	for(x = rn1(6,7); x; x--) {
		mm = mazexy();
		mkgold(0L,mm.x,mm.y);
	}
	for(x = rn1(6,7); x; x--)
		mktrap(0,1,(struct mkroom *) 0);
	mm = mazexy();
	levl[(xupstair = mm.x)][(yupstair = mm.y)].scrsym = '<';
	levl[xupstair][yupstair].typ = STAIRS;
	xdnstair = ydnstair = 0;
}

walkfrom(x,y) int x,y; {
int q,a,dir;
int dirs[4];
	levl[x][y].typ = ROOM;
	while(1) {
		q = 0;
		for(a = 0; a < 4; a++)
			if(okay(x,y,a)) dirs[q++]= a;
		if(!q) return;
		dir = dirs[rn2(q)];
		move(&x,&y,dir);
		levl[x][y].typ = ROOM;
		move(&x,&y,dir);
		walkfrom(x,y);
	}
}

move(x,y,dir)
int *x, *y;
int dir;
{
	switch(dir){
		case 0: --(*y); break;
		case 1: (*x)++; break;
		case 2: (*y)++; break;
		case 3: --(*x); break;
	}
}

okay(x,y,dir)
int x,y;
int dir;
{
	move(&x,&y,dir);
	move(&x,&y,dir);
	if(x<3 || y<3 || x>COLNO-3 || y>ROWNO-3 || levl[x][y].typ != 0)
		return(0);
	else
		return(1);
}

coord
mazexy(){
	coord mm;
	mm.x = 3 + 2*rn2(COLNO/2 - 2);
	mm.y = 3 + 2*rn2(ROWNO/2 - 2);
	return mm;
}
