/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.mkshop.c - version 1.0.3 */

#ifndef QUEST
#include "hack.h"
#include "def.mkroom.h"
#include "def.eshk.h"
#include <stdlib.h>
#define	ESHK	((struct eshk *)(&(shk->mextra[0])))
extern struct monst *makemon();
extern struct obj *mkobj_at();
extern int nroom;
extern char shtypes[];	/* = "=/)%?!["; 8 types: 7 specialized, 1 mixed */
schar shprobs[] = { 3,3,5,5,10,10,14,50 };	/* their probabilities */

mkshop(){
register struct mkroom *sroom;
register int sh,sx,sy,i = -1;
register char let;
int roomno;
register struct monst *shk;
#ifdef WIZARD
	/* first determine shoptype */
	if(wizard){
		register char *ep = getenv("SHOPTYPE");
		if(ep){
			if(*ep == 'z' || *ep == 'Z'){
				mkzoo(ZOO);
				return;
			}
			if(*ep == 'm' || *ep == 'M'){
				mkzoo(MORGUE);
				return;
			}
			if(*ep == 'b' || *ep == 'B'){
				mkzoo(BEEHIVE);
				return;
			}
			if(*ep == 's' || *ep == 'S'){
				mkswamp();
				return;
			}
			for(i=0; shtypes[i]; i++)
				if(*ep == shtypes[i]) break;
			goto gottype;
		}
	}
gottype:
#endif WIZARD
	for(sroom = &rooms[0], roomno = 0; ; sroom++, roomno++){
		if(sroom->hx < 0) return;
		if(sroom - rooms >= nroom) {
			pline("rooms not closed by -1?");
			return;
		}
		if(sroom->rtype) continue;
		if(!sroom->rlit || has_dnstairs(sroom) || has_upstairs(sroom))
			continue;
		if(
#ifdef WIZARD
		   (wizard && getenv("SHOPTYPE") && sroom->doorct != 0) ||
#endif WIZARD
			sroom->doorct <= 2 && sroom->doorct > 0) break;
	}

	if(i < 0) {			/* shoptype not yet determined */
	    register int j;

	    for(j = rn2(100), i = 0; (j -= shprobs[i])>= 0; i++)
		if(!shtypes[i]) break;			/* superfluous */
	    if(isbig(sroom) && i + SHOPBASE == WANDSHOP)
		i = GENERAL-SHOPBASE;
	}
	sroom->rtype = i + SHOPBASE;
	let = shtypes[i];
	sh = sroom->fdoor;
	sx = doors[sh].x;
	sy = doors[sh].y;
	if(sx == sroom->lx-1) sx++; else
	if(sx == sroom->hx+1) sx--; else
	if(sy == sroom->ly-1) sy++; else
	if(sy == sroom->hy+1) sy--; else {
#ifdef WIZARD
	    /* This is said to happen sometimes, but I've never seen it. */
	    if(wizard) {
		register int j = sroom->doorct;
		extern int doorindex;

		pline("Where is shopdoor?");
		pline("Room at (%d,%d),(%d,%d).", sroom->lx, sroom->ly,
			sroom->hx, sroom->hy);
		pline("doormax=%d doorct=%d fdoor=%d",
			doorindex, sroom->doorct, sh);
		while(j--) {
			pline("door [%d,%d]", doors[sh].x, doors[sh].y);
			sh++;
		}
		more();
	    }
#endif WIZARD
	    return;
	}
	if(!(shk = makemon(PM_SHK,sx,sy))) return;
	shk->isshk = shk->mpeaceful = 1;
	shk->msleep = 0;
	shk->mtrapseen = ~0;	/* we know all the traps already */
	ESHK->shoproom = roomno;
	ESHK->shoplevel = dlevel;
	ESHK->shd = doors[sh];
	ESHK->shk.x = sx;
	ESHK->shk.y = sy;
	ESHK->robbed = 0;
	ESHK->visitct = 0;
	ESHK->following = 0;
	shk->mgold = 1000 + 30*rnd(100);	/* initial capital */
	ESHK->billct = 0;
	findname(ESHK->shknam, let);
	for(sx = sroom->lx; sx <= sroom->hx; sx++)
	for(sy = sroom->ly; sy <= sroom->hy; sy++){
		register struct monst *mtmp;
		if((sx == sroom->lx && doors[sh].x == sx-1) ||
		   (sx == sroom->hx && doors[sh].x == sx+1) ||
		   (sy == sroom->ly && doors[sh].y == sy-1) ||
		   (sy == sroom->hy && doors[sh].y == sy+1)) continue;
		if(rn2(100) < dlevel && !m_at(sx,sy) &&
		   (mtmp = makemon(PM_MIMIC, sx, sy))){
			mtmp->mimic = 1;
			mtmp->mappearance =
			    (let && rn2(10) < dlevel) ? let : ']';
			continue;
		}
		(void) mkobj_at(let, sx, sy);
	}
}

mkzoo(type)
int type;
{
	register struct mkroom *sroom;
	register struct monst *mon;
	register int sh,sx,sy,i;
	int goldlim = 500 * dlevel;
	int moct = 0;
	struct permonst *morguemon();

	i = nroom;
	for(sroom = &rooms[rn2(nroom)]; ; sroom++) {
		if(sroom == &rooms[nroom])
			sroom = &rooms[0];
		if(!i-- || sroom->hx < 0)
			return;
		if(sroom->rtype)
			continue;
		if(type == MORGUE && sroom->rlit)
			continue;
		if(has_upstairs(sroom) || (has_dnstairs(sroom) && rn2(3)))
			continue;
		if(sroom->doorct == 1 || !rn2(5))
			break;
	}
	sroom->rtype = type;
	sh = sroom->fdoor;
	for(sx = sroom->lx; sx <= sroom->hx; sx++)
	for(sy = sroom->ly; sy <= sroom->hy; sy++){
		if((sx == sroom->lx && doors[sh].x == sx-1) ||
		   (sx == sroom->hx && doors[sh].x == sx+1) ||
		   (sy == sroom->ly && doors[sh].y == sy-1) ||
		   (sy == sroom->hy && doors[sh].y == sy+1)) continue;
		mon = makemon(
		   (type == MORGUE) ? morguemon() :
		   (type == BEEHIVE) ? PM_KILLER_BEE : (struct permonst *) 0,
		   sx, sy);
		if(mon) mon->msleep = 1;
		switch(type) {
		case ZOO:
		   i = sq(dist2(sx,sy,doors[sh].x,doors[sh].y));
		   if(i >= goldlim) i = 5*dlevel;
		   goldlim -= i;
		   mkgold((long)(10 + rn2(i)), sx, sy);
		   break;
		case MORGUE:
		   /* Usually there is one dead body in the morgue */
		   if(!moct && rn2(3)) {
			mksobj_at(CORPSE, sx, sy);
			moct++;
		   }
		   break;
		case BEEHIVE:
		   if(!rn2(3)) mksobj_at(LUMP_OF_ROYAL_JELLY, sx, sy);
		   break;
		}
	}
}

struct permonst *
morguemon()
{
	extern struct permonst pm_ghost;
	register int i = rn2(100), hd = rn2(dlevel);

	if(hd > 10 && i < 10) return(PM_DEMON);
	if(hd > 8 && i > 85) return(PM_VAMPIRE);
	return((i < 40) ? PM_GHOST : (i < 60) ? PM_WRAITH : PM_ZOMBIE);
}

mkswamp()	/* Michiel Huisjes & Fred de Wilde */
{
	register struct mkroom *sroom;
	register int sx,sy,i,eelct = 0;
	extern struct permonst pm_eel;

	for(i=0; i<5; i++) {		/* 5 tries */
		sroom = &rooms[rn2(nroom)];
		if(sroom->hx < 0 || sroom->rtype ||
		   has_upstairs(sroom) || has_dnstairs(sroom))
			continue;

		/* satisfied; make a swamp */
		sroom->rtype = SWAMP;
		for(sx = sroom->lx; sx <= sroom->hx; sx++)
		for(sy = sroom->ly; sy <= sroom->hy; sy++)
		if((sx+sy)%2 && !o_at(sx,sy) && !t_at(sx,sy)
			     && !m_at(sx,sy) && !nexttodoor(sx,sy)){
			levl[sx][sy].typ = POOL;
			levl[sx][sy].scrsym = POOL_SYM;
			if(!eelct || !rn2(4)) {
				(void) makemon(PM_EEL, sx, sy);
				eelct++;
			}
		}
	}
}

nexttodoor(sx,sy)
register sx,sy;
{
	register dx,dy;
	register struct rm *lev;
	for(dx = -1; dx <= 1; dx++) for(dy = -1; dy <= 1; dy++)
		if((lev = &levl[sx+dx][sy+dy])->typ == DOOR ||
		    lev->typ == SDOOR || lev->typ == LDOOR)
			return(1);
	return(0);
}

has_dnstairs(sroom)
register struct mkroom *sroom;
{
	return(sroom->lx <= xdnstair && xdnstair <= sroom->hx &&
		   sroom->ly <= ydnstair && ydnstair <= sroom->hy);
}

has_upstairs(sroom)
register struct mkroom *sroom;
{
	return(sroom->lx <= xupstair && xupstair <= sroom->hx &&
		   sroom->ly <= yupstair && yupstair <= sroom->hy);
}

isbig(sroom)
register struct mkroom *sroom;
{
	register int area = (sroom->hx - sroom->lx) * (sroom->hy - sroom->ly);
	return( area > 20 );
}

dist2(x0,y0,x1,y1){
	return((x0-x1)*(x0-x1) + (y0-y1)*(y0-y1));
}

sq(a) int a; {
	return(a*a);
}
#endif QUEST
