/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.makemon.c - version 1.0.2 */

#include	"hack.h"
extern char fut_geno[];
extern char *index();
extern struct obj *mkobj_at();
struct monst zeromonst;

/*
 * called with [x,y] = coordinates;
 *	[0,0] means anyplace
 *	[u.ux,u.uy] means: call mnexto (if !in_mklev)
 *
 *	In case we make an Orc or killer bee, we make an entire horde (swarm);
 *	note that in this case we return only one of them (the one at [x,y]).
 */
struct monst *
makemon(ptr,x,y)
register struct permonst *ptr;
{
	register struct monst *mtmp;
	register tmp, ct;
	boolean anything = (!ptr);
	extern boolean in_mklev;

	if(x != 0 || y != 0) if(m_at(x,y)) return((struct monst *) 0);
	if(ptr){
		if(index(fut_geno, ptr->mlet)) return((struct monst *) 0);
	} else {
		ct = CMNUM - strlen(fut_geno);
		if(index(fut_geno, 'm')) ct++;  /* make only 1 minotaur */
		if(index(fut_geno, '@')) ct++;
		if(ct <= 0) return(0); 		  /* no more monsters! */
		tmp = rn2(ct*dlevel/24 + 7);
		if(tmp < dlevel - 4) tmp = rn2(ct*dlevel/24 + 12);
		if(tmp >= ct) tmp = rn1(ct - ct/2, ct/2);
		for(ct = 0; ct < CMNUM; ct++){
			ptr = &mons[ct];
			if(index(fut_geno, ptr->mlet))
				continue;
			if(!tmp--) goto gotmon;
		}
		panic("makemon?");
	}
gotmon:
	mtmp = newmonst(ptr->pxlth);
	*mtmp = zeromonst;	/* clear all entries in structure */
	for(ct = 0; ct < ptr->pxlth; ct++)
		((char *) &(mtmp->mextra[0]))[ct] = 0;
	mtmp->nmon = fmon;
	fmon = mtmp;
	mtmp->m_id = flags.ident++;
	mtmp->data = ptr;
	mtmp->mxlth = ptr->pxlth;
	if(ptr->mlet == 'D') mtmp->mhpmax = mtmp->mhp = 80;
	else if(!ptr->mlevel) mtmp->mhpmax = mtmp->mhp = rnd(4);
	else mtmp->mhpmax = mtmp->mhp = d(ptr->mlevel, 8);
	mtmp->mx = x;
	mtmp->my = y;
	mtmp->mcansee = 1;
	if(ptr->mlet == 'M'){
		mtmp->mimic = 1;
		mtmp->mappearance = ']';
	}
	if(!in_mklev) {
		if(x == u.ux && y == u.uy && ptr->mlet != ' ')
			mnexto(mtmp);
		if(x == 0 && y == 0)
			rloc(mtmp);
	}
	if(ptr->mlet == 's' || ptr->mlet == 'S') {
		mtmp->mhide = mtmp->mundetected = 1;
		if(in_mklev)
		if(mtmp->mx && mtmp->my)
			(void) mkobj_at(0, mtmp->mx, mtmp->my);
	}
	if(ptr->mlet == ':') {
		mtmp->cham = 1;
		(void) newcham(mtmp, &mons[dlevel+14+rn2(CMNUM-14-dlevel)]);
	}
	if(ptr->mlet == 'I' || ptr->mlet == ';')
		mtmp->minvis = 1;
	if(ptr->mlet == 'L' || ptr->mlet == 'N'
	    || (in_mklev && index("&w;", ptr->mlet) && rn2(5))
	) mtmp->msleep = 1;

#ifndef NOWORM
	if(ptr->mlet == 'w' && getwn(mtmp))
		initworm(mtmp);
#endif NOWORM

	if(anything) if(ptr->mlet == 'O' || ptr->mlet == 'k') {
		coord enexto();
		coord mm;
		register int cnt = rnd(10);
		mm.x = x;
		mm.y = y;
		while(cnt--) {
			mm = enexto(mm.x, mm.y);
			(void) makemon(ptr, mm.x, mm.y);
		}
	}

	return(mtmp);
}

coord
enexto(xx,yy)
register xchar xx,yy;
{
	register xchar x,y;
	coord foo[15], *tfoo;
	int range;

	tfoo = foo;
	range = 1;
	do {	/* full kludge action. */
		for(x = xx-range; x <= xx+range; x++)
			if(goodpos(x, yy-range)) {
				tfoo->x = x;
				tfoo++->y = yy-range;
				if(tfoo == &foo[15]) goto foofull;
			}
		for(x = xx-range; x <= xx+range; x++)
			if(goodpos(x,yy+range)) {
				tfoo->x = x;
				tfoo++->y = yy+range;
				if(tfoo == &foo[15]) goto foofull;
			}
		for(y = yy+1-range; y < yy+range; y++)
			if(goodpos(xx-range,y)) {
				tfoo->x = xx-range;
				tfoo++->y = y;
				if(tfoo == &foo[15]) goto foofull;
			}
		for(y = yy+1-range; y < yy+range; y++)
			if(goodpos(xx+range,y)) {
				tfoo->x = xx+range;
				tfoo++->y = y;
				if(tfoo == &foo[15]) goto foofull;
			}
		range++;
	} while(tfoo == foo);
foofull:
	return( foo[rn2(tfoo-foo)] );
}

goodpos(x,y)	/* used only in mnexto and rloc */
{
	return(
	! (x < 1 || x > COLNO-2 || y < 1 || y > ROWNO-2 ||
	   m_at(x,y) || !ACCESSIBLE(levl[x][y].typ)
	   || (x == u.ux && y == u.uy)
	   || sobj_at(ENORMOUS_ROCK, x, y)
	));
}

rloc(mtmp)
struct monst *mtmp;
{
	register tx,ty;
	register char ch = mtmp->data->mlet;

#ifndef NOWORM
	if(ch == 'w' && mtmp->mx) return;	/* do not relocate worms */
#endif NOWORM
	do {
		tx = rn1(COLNO-3,2);
		ty = rn2(ROWNO);
	} while(!goodpos(tx,ty));
	mtmp->mx = tx;
	mtmp->my = ty;
	if(u.ustuck == mtmp){
		if(u.uswallow) {
			u.ux = tx;
			u.uy = ty;
			docrt();
		} else	u.ustuck = 0;
	}
	pmon(mtmp);
}

struct monst *
mkmon_at(let,x,y)
char let;
register int x,y;
{
	register int ct;
	register struct permonst *ptr;

	for(ct = 0; ct < CMNUM; ct++) {
		ptr = &mons[ct];
		if(ptr->mlet == let)
			return(makemon(ptr,x,y));
	}
	return(0);
}
