#if __GNUC__ >= 3 && __GNUC_MINOR__ >= 4 || __GNUC__ >= 4
#define	USED	__attribute__ ((used))
#elif defined __GNUC__
#define	USED	__attribute__ ((unused))
#else
#define	USED
#endif
static const char sccsid[] USED = "@(#)/usr/ucb/daps.sl	1.5 (gritter) 7/9/06";
/* SLIST */
/*
aps.h: * Sccsid @(#)aps.h	1.3 (gritter) 8/9/05
build.c: * Sccsid @(#)build.c	1.4 (gritter) 8/13/05
daps.c: * Sccsid @(#)daps.c	1.8 (gritter) 7/9/06
daps.h: * Sccsid @(#)daps.h	1.3 (gritter) 8/9/05
dev.h: * Sccsid @(#)dev.h	1.3 (gritter) 8/9/05
draw.c:	Sccsid @(#)draw.c	1.1 (gritter) 7/3/06	
getopt.c: * Sccsid @(#)getopt.c	1.8 (gritter) 8/2/05
makedev.c: * Sccsid @(#)makedev.c	1.3 (gritter) 8/9/05
*/
