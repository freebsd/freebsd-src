/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* def.monst.h - version 1.0.2 */

struct monst {
	struct monst *nmon;
	struct permonst *data;
	unsigned m_id;
	xchar mx,my;
	xchar mdx,mdy;		/* if mdispl then pos where last displayed */
#define	MTSZ	4
	coord mtrack[MTSZ];	/* monster track */
	schar mhp,mhpmax;
	char mappearance;	/* nonzero for undetected 'M's and for '1's */
	Bitfield(mimic,1);	/* undetected mimic */
	Bitfield(mdispl,1);	/* mdx,mdy valid */
	Bitfield(minvis,1);	/* invisible */
	Bitfield(cham,1);	/* shape-changer */
	Bitfield(mhide,1);	/* hides beneath objects */
	Bitfield(mundetected,1);	/* not seen in present hiding place */
	Bitfield(mspeed,2);
	Bitfield(msleep,1);
	Bitfield(mfroz,1);
	Bitfield(mconf,1);
	Bitfield(mflee,1);	/* fleeing */
	Bitfield(mfleetim,7);	/* timeout for mflee */
	Bitfield(mcan,1);	/* has been cancelled */
	Bitfield(mtame,1);		/* implies peaceful */
	Bitfield(mpeaceful,1);	/* does not attack unprovoked */
	Bitfield(isshk,1);	/* is shopkeeper */
	Bitfield(isgd,1);	/* is guard */
	Bitfield(mcansee,1);	/* cansee 1, temp.blinded 0, blind 0 */
	Bitfield(mblinded,7);	/* cansee 0, temp.blinded n, blind 0 */
	Bitfield(mtrapped,1);	/* trapped in a pit or bear trap */
	Bitfield(mnamelth,6);	/* length of name (following mxlth) */
#ifndef NOWORM
	Bitfield(wormno,5);	/* at most 31 worms on any level */
#endif NOWORM
	unsigned mtrapseen;	/* bitmap of traps we've been trapped in */
	long mlstmv;	/* prevent two moves at once */
	struct obj *minvent;
	long mgold;
	unsigned mxlth;		/* length of following data */
	/* in order to prevent alignment problems mextra should
	   be (or follow) a long int */
	long mextra[1];		/* monster dependent info */
};

#define newmonst(xl)	(struct monst *) alloc((unsigned)(xl) + sizeof(struct monst))

extern struct monst *fmon;
extern struct monst *fallen_down;
struct monst *m_at();

/* these are in mspeed */
#define MSLOW 1 /* slow monster */
#define MFAST 2 /* speeded monster */

#define	NAME(mtmp)	(((char *) mtmp->mextra) + mtmp->mxlth)
#define	MREGEN		"TVi1"
#define	UNDEAD		"ZVW "
