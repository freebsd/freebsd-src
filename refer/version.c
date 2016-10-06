#if __GNUC__ >= 3 && __GNUC_MINOR__ >= 4 || __GNUC__ >= 4
#define	USED	__attribute__ ((used))
#elif defined __GNUC__
#define	USED	__attribute__ ((unused))
#else
#define	USED
#endif
static const char sccsid[] USED = "@(#)/usr/ucb/refer.sl	5.5 (gritter) 9/7/08";
/* SLIST */
/*
addbib.c: * Sccsid @(#)addbib.c	1.3 (gritter) 10/22/05
deliv2.c: * Sccsid @(#)deliv2.c	1.3 (gritter) 10/22/05
glue1.c: * Sccsid @(#)glue1.c	1.4 (gritter) 9/7/08
glue2.c: * Sccsid @(#)glue2.c	1.3 (gritter) 10/22/05
glue3.c: * Sccsid @(#)glue3.c	1.5 (gritter) 9/7/08
glue4.c: * Sccsid @(#)glue4.c	1.4 (gritter) 9/7/08
glue5.c: * Sccsid @(#)glue5.c	1.4 (gritter) 10/2/07
hunt1.c: * Sccsid @(#)hunt1.c	1.4 (gritter) 9/7/08
hunt2.c: * Sccsid @(#)hunt2.c	1.3 (gritter) 10/22/05
hunt3.c: * Sccsid @(#)hunt3.c	1.3 (gritter) 10/22/05
hunt5.c: * Sccsid @(#)hunt5.c	1.3 (gritter) 10/22/05
hunt6.c: * Sccsid @(#)hunt6.c	1.4 (gritter) 10/2/07
hunt7.c: * Sccsid @(#)hunt7.c	1.3 (gritter) 10/22/05
hunt8.c: * Sccsid @(#)hunt8.c	1.4 (gritter) 01/12/07
hunt9.c: * Sccsid @(#)hunt9.c	1.3 (gritter) 10/22/05
inv1.c: * Sccsid @(#)inv1.c	1.3 (gritter) 10/22/05
inv2.c: * Sccsid @(#)inv2.c	1.3 (gritter) 10/22/05
inv3.c: * Sccsid @(#)inv3.c	1.3 (gritter) 10/22/05
inv5.c: * Sccsid @(#)inv5.c	1.5 (gritter) 12/25/06
inv6.c: * Sccsid @(#)inv6.c	1.3 (gritter) 10/22/05
lookbib.c: * Sccsid @(#)lookbib.c	1.3 (gritter) 10/22/05
mkey1.c: * Sccsid @(#)mkey1.c	1.3 (gritter) 10/22/05
mkey2.c: * Sccsid @(#)mkey2.c	1.3 (gritter) 10/22/05
mkey3.c: * Sccsid @(#)mkey3.c	1.3 (gritter) 10/22/05
refer..c: * Sccsid @(#)refer..c	1.5 (gritter) 12/25/06
refer0.c: * Sccsid @(#)refer0.c	1.3 (gritter) 10/22/05
refer1.c: * Sccsid @(#)refer1.c	1.3 (gritter) 10/22/05
refer2.c: * Sccsid @(#)refer2.c	1.4 (gritter) 9/7/08
refer3.c: * Sccsid @(#)refer3.c	1.3 (gritter) 10/22/05
refer4.c: * Sccsid @(#)refer4.c	1.3 (gritter) 10/22/05
refer5.c: * Sccsid @(#)refer5.c	1.3 (gritter) 10/22/05
refer6.c: * Sccsid @(#)refer6.c	1.3 (gritter) 10/22/05
refer7.c: * Sccsid @(#)refer7.c	1.3 (gritter) 10/22/05
refer8.c: * Sccsid @(#)refer8.c	1.3 (gritter) 10/22/05
shell.c: * Sccsid @(#)shell.c	1.4 (gritter) 12/25/06
sortbib.c: * Sccsid @(#)sortbib.c	1.3 (gritter) 10/22/05
tick.c: * Sccsid @(#)tick.c	1.3 (gritter) 10/22/05
*/
