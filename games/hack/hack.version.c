/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.version.c - version 1.0.3 */
/* $FreeBSD$ */

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
