/*
 *	@(#)Version.c	4.9 (Berkeley) 7/21/90
 *	$Id: version.c,v 8.2 1997/04/24 23:59:02 vixie Exp $
 */

#ifndef lint
char sccsid[] = "@(#)named %VERSION% %WHEN% %WHOANDWHERE%";
char rcsid[] = "$Id: version.c,v 8.2 1997/04/24 23:59:02 vixie Exp $";
#endif /* not lint */

char Version[] = "named %VERSION% %WHEN%\n\t%WHOANDWHERE%";
char ShortVersion[] = "%VERSION%";

#ifdef COMMENT

SCCS/s.Version.c:

D 4.8.3	90/06/27 17:05:21	bloom	37	35	00031/00028/00079
Version distributed with 4.3 Reno tape (June 1990)

D 4.8.2	89/09/18 13:57:11	bloom	35	34	00020/00014/00087
Interim fixes release

D 4.8.1	89/02/08 17:12:15	karels	34	33	00026/00017/00075
branch for 4.8.1

D 4.8	88/07/09 14:27:00       karels  33      28      00043/00031/00049
4.8 is here!

D 4.7	87/11/20 13:15:52	karels	25	24	00000/00000/00062
4.7.3 beta

D 4.6	87/07/21 12:15:52	karels	25	24	00000/00000/00062
4.6 declared stillborn

D 4.5	87/02/10 12:33:25	kjd	24	18	00000/00000/00062
February 1987, Network Release. Child (bind) grows up, parent (kevin) leaves home.

D 4.4	86/10/01 10:06:26	kjd	18	12	00020/00017/00042
October 1, 1986 Network Distribution

D 4.3	86/06/04 12:12:18	kjd	12	7	00015/00028/00044
Version distributed with 4.3BSD

D 4.2	86/04/30 20:57:16	kjd	7	1	00056/00000/00016
Network distribution Freeze and one more version until 4.3BSD

D 1.1	86/04/30 19:30:00	kjd	1	0	00016/00000/00000
date and time created 86/04/30 19:30:00 by kjd

code versions:

Makefile
	Makefile	4.14 (Berkeley) 2/28/88
db.h
	db.h	4.13 (Berkeley) 2/17/88
db_dump.c
	db_dump.c	4.20 (Berkeley) 2/17/88
db_load.c
	db_load.c	4.26 (Berkeley) 2/28/88
db_lookup.c
	db_lookup.c	4.14 (Berkeley) 2/17/88
db_reload.c
	db_reload.c	4.15 (Berkeley) 2/28/88
db_save.c
	db_save.c	4.13 (Berkeley) 2/17/88
db_update.c
	db_update.c	4.16 (Berkeley) 2/28/88
ns_forw.c
	ns_forw.c	4.26 (Berkeley) 3/28/88
ns_init.c
	ns_init.c	4.23 (Berkeley) 2/28/88
ns_main.c
	 Copyright (c) 1986 Regents of the University of California.\n\
	ns_main.c	4.30 (Berkeley) 3/7/88
ns_maint.c
	ns_maint.c	4.23 (Berkeley) 2/28/88
ns_req.c
	ns_req.c	4.32 (Berkeley) 3/31/88
ns_resp.c
	ns_resp.c	4.50 (Berkeley) 4/7/88
ns_sort.c
	ns_sort.c	4.3 (Berkeley) 2/17/88
ns_stats.c
	ns_stats.c	4.3 (Berkeley) 2/17/88
newvers.sh
	newvers.sh	4.4 (Berkeley) 3/28/88

#endif /* COMMENT */
