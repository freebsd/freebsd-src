/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.bones.c - version 1.0.3 */
/* $FreeBSD: src/games/hack/hack.bones.c,v 1.4 1999/11/16 10:26:35 marcel Exp $ */

#include "hack.h"
extern char plname[PL_NSIZ];
extern long somegold();
extern struct monst *makemon();
extern struct permonst pm_ghost;

char bones[] = "bones_xx";

/* save bones and possessions of a deceased adventurer */
savebones(){
int fd;
struct obj *otmp;
struct trap *ttmp;
struct monst *mtmp;
	if(dlevel <= 0 || dlevel > MAXLEVEL) return;
	if(!rn2(1 + dlevel/2)) return;	/* not so many ghosts on low levels */
	bones[6] = '0' + (dlevel/10);
	bones[7] = '0' + (dlevel%10);
	if((fd = open(bones,0)) >= 0){
		(void) close(fd);
		return;
	}
	/* drop everything; the corpse's possessions are usually cursed */
	otmp = invent;
	while(otmp){
		otmp->ox = u.ux;
		otmp->oy = u.uy;
		otmp->age = 0;		/* very long ago */
		otmp->owornmask = 0;
		if(rn2(5)) otmp->cursed = 1;
		if(!otmp->nobj){
			otmp->nobj = fobj;
			fobj = invent;
			invent = 0;	/* superfluous */
			break;
		}
		otmp = otmp->nobj;
	}
	if(!(mtmp = makemon(PM_GHOST, u.ux, u.uy))) return;
	mtmp->mx = u.ux;
	mtmp->my = u.uy;
	mtmp->msleep = 1;
	(void) strcpy((char *) mtmp->mextra, plname);
	mkgold(somegold() + d(dlevel,30), u.ux, u.uy);
	for(mtmp = fmon; mtmp; mtmp = mtmp->nmon){
		mtmp->m_id = 0;
		if(mtmp->mtame) {
			mtmp->mtame = 0;
			mtmp->mpeaceful = 0;
		}
		mtmp->mlstmv = 0;
		if(mtmp->mdispl) unpmon(mtmp);
	}
	for(ttmp = ftrap; ttmp; ttmp = ttmp->ntrap)
		ttmp->tseen = 0;
	for(otmp = fobj; otmp; otmp = otmp->nobj) {
		otmp->o_id = 0;
	     /* otmp->o_cnt_id = 0; - superfluous */
		otmp->onamelth = 0;
		otmp->known = 0;
		otmp->invlet = 0;
		if(otmp->olet == AMULET_SYM && !otmp->spe) {
			otmp->spe = -1;      /* no longer the actual amulet */
			otmp->cursed = 1;    /* flag as gotten from a ghost */
		}
	}
	if((fd = creat(bones, FMASK)) < 0) return;
	savelev(fd,dlevel);
	(void) close(fd);
}

getbones(){
int fd,x,y,ok;
	if(rn2(3)) return(0);	/* only once in three times do we find bones */
	bones[6] = '0' + dlevel/10;
	bones[7] = '0' + dlevel%10;
	if((fd = open(bones, 0)) < 0) return(0);
	if((ok = uptodate(fd)) != 0){
		getlev(fd, 0, dlevel);
		for(x = 0; x < COLNO; x++) for(y = 0; y < ROWNO; y++)
			levl[x][y].seen = levl[x][y].new = 0;
	}
	(void) close(fd);
#ifdef WIZARD
	if(!wizard)	/* duvel!frans: don't remove bones while debugging */
#endif WiZARD
	    if(unlink(bones) < 0){
		pline("Cannot unlink %s .", bones);
		return(0);
	}
	return(ok);
}
