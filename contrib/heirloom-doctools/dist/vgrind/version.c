#if __GNUC__ >= 3 && __GNUC_MINOR__ >= 4 || __GNUC__ >= 4
#define	USED	__attribute__ ((used))
#elif defined __GNUC__
#define	USED	__attribute__ ((unused))
#else
#define	USED
#endif
static const char sccsid[] USED = "@(#)/usr/ucblib/vfontedpr.sl	5.1 (gritter) 10/25/05";
/* SLIST */
/*
regexp.c: * Sccsid @(#)regexp.c	1.3 (gritter) 10/22/05
retest.c: * Sccsid @(#)retest.c	1.3 (gritter) 10/22/05
vfontedpr.c: * Sccsid @(#)vfontedpr.c	1.4 (gritter) 10/22/05
vgrindefs.c: * Sccsid @(#)vgrindefs.c	1.3 (gritter) 10/22/05
*/
