/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.version.c - version 1.0.3 */
/* $Header: /a/cvs/386BSD/src/games/hack/hack.version.c,v 1.1.1.1 1993/06/12 14:40:26 rgrimes Exp $ */

#include	"date.h"

doversion(){
	pline("%s 1.0.3 - last edit %s.", (
#ifdef QUEST
		"Quest"
#else
		"Hack"
#endif QUEST
		), datestring);
	return(0);
}
