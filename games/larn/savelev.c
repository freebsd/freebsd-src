/* savelev.c		 Larn is copyrighted 1986 by Noah Morgan. */
/* $FreeBSD: src/games/larn/savelev.c,v 1.3 1999/11/16 02:57:24 billf Exp $ */
#include "header.h"
extern struct cel *cell;

/*
 *	routine to save the present level into storage
 */
savelevel()
	{
	struct cel *pcel;
	char *pitem,*pknow,*pmitem;
	short *phitp,*piarg;
	struct cel *pecel;
	pcel = &cell[level*MAXX*MAXY];	/* pointer to this level's cells */
	pecel = pcel + MAXX*MAXY;	/* pointer to past end of this level's cells */
	pitem=item[0]; piarg=iarg[0]; pknow=know[0]; pmitem=mitem[0]; phitp=hitp[0];
	while (pcel < pecel)
		{
		pcel->mitem  = *pmitem++;
		pcel->hitp   = *phitp++;
		pcel->item   = *pitem++;
		pcel->know   = *pknow++;
		pcel++->iarg = *piarg++;
		}
	}

/*
 *	routine to restore a level from storage
 */
getlevel()
	{
	struct cel *pcel;
	char *pitem,*pknow,*pmitem;
	short *phitp,*piarg;
	struct cel *pecel;
	pcel = &cell[level*MAXX*MAXY];	/* pointer to this level's cells */
	pecel = pcel + MAXX*MAXY;	/* pointer to past end of this level's cells */
	pitem=item[0]; piarg=iarg[0]; pknow=know[0]; pmitem=mitem[0]; phitp=hitp[0];
	while (pcel < pecel)
		{
		*pmitem++ = pcel->mitem;
		*phitp++ = pcel->hitp;
		*pitem++ = pcel->item;
		*pknow++ = pcel->know;
		*piarg++ = pcel++->iarg;
		}
	}
