/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.version.c - version 1.0.3 */
/* $Header: /home/ncvs/src/games/hack/hack.version.c,v 1.1.1.1.6.1 1996/06/05 02:37:55 jkh Exp $ */

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
