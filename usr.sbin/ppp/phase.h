/*
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: phase.h,v 1.8 1997/10/26 01:03:32 brian Exp $
 *
 *	TODO:
 */

#define	PHASE_DEAD		0	/* Link is dead */
#define	PHASE_ESTABLISH		1	/* Establishing link */
#define	PHASE_AUTHENTICATE	2	/* Being authenticated */
#define	PHASE_NETWORK		3
#define	PHASE_TERMINATE		4	/* Terminating link */

extern int phase;			/* Curent phase */

extern void NewPhase(int);
