#if __GNUC__ >= 3 && __GNUC_MINOR__ >= 4 || __GNUC__ >= 4
#define	USED	__attribute__ ((used))
#elif defined __GNUC__
#define	USED	__attribute__ ((unused))
#else
#define	USED
#endif
#ifdef	NEQN
static const char sccsid[] USED = "@(#)/usr/ucb/neqn.sl	5.12 (gritter) 1/13/08";
#else
static const char sccsid[] USED = "@(#)/usr/ucb/eqn.sl	5.12 (gritter) 1/13/08";
#endif
/* SLIST */
/*
diacrit.c: * Sccsid @(#)diacrit.c	1.7 (gritter) 1/13/08
e.h: * Sccsid @(#)e.h	1.13 (gritter) 1/13/08
e.y: * Sccsid @(#)e.y	1.7 (gritter) 10/2/07
eqnbox.c: * Sccsid @(#)eqnbox.c	1.7 (gritter) 1/13/08
font.c: * Sccsid @(#)font.c	1.5 (gritter) 1/13/08
fromto.c: * Sccsid @(#)fromto.c	1.5 (gritter) 10/19/06
funny.c: * Sccsid @(#)funny.c	1.6 (gritter) 10/19/06
glob.c: * Sccsid @(#)glob.c	1.8 (gritter) 10/19/06
integral.c: * Sccsid @(#)integral.c	1.5 (gritter) 10/19/06
io.c: * Sccsid @(#)io.c	1.13 (gritter) 1/13/08
lex.c: * Sccsid @(#)lex.c	1.7 (gritter) 11/21/07
lookup.c: * Sccsid @(#)lookup.c	1.5 (gritter) 9/18/05
mark.c: * Sccsid @(#)mark.c	1.3 (gritter) 8/12/05
matrix.c: * Sccsid @(#)matrix.c	1.4 (gritter) 10/29/05
move.c: * Sccsid @(#)move.c	1.4 (gritter) 10/29/05
over.c: * Sccsid @(#)over.c	1.5 (gritter) 10/19/06
paren.c: * Sccsid @(#)paren.c	1.4 (gritter) 10/29/05
pile.c: * Sccsid @(#)pile.c	1.4 (gritter) 10/29/05
shift.c: * Sccsid @(#)shift.c	1.6 (gritter) 1/13/08
size.c: * Sccsid @(#)size.c	1.5 (gritter) 10/19/06
sqrt.c: * Sccsid @(#)sqrt.c	1.6 (gritter) 1/13/08
text.c: * Sccsid @(#)text.c	1.8 (gritter) 1/13/08
*/
