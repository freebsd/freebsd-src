/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.mfndpos.h - version 1.0.2 */

#define	ALLOW_TRAPS	0777
#define	ALLOW_U		01000
#define	ALLOW_M		02000
#define	ALLOW_TM	04000
#define	ALLOW_ALL	(ALLOW_U | ALLOW_M | ALLOW_TM | ALLOW_TRAPS)
#define	ALLOW_SSM	010000
#define	ALLOW_ROCK	020000
#define	NOTONL		040000
#define	NOGARLIC	0100000
