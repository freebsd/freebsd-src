/*-
 * Copyright (c) 1986, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)Version.c	4.10 (Berkeley) 4/24/91
 */

#ifndef lint
char sccsid[] = "@(#)named 4.8.3 %WHEN% %WHOANDWHERE%\n";
#endif /* not lint */

char Version[] = "named 4.8.3 %WHEN%\n\t%WHOANDWHERE%\n";

#ifdef COMMENT

SCCS/s.Version.c:

D 4.8.3	90/08/15 09:21:21	bloom	37	35	00031/00028/00079
Version distributed with 4.3 Reno tape (June 1990)
(with additional changes for backward compat. after Reno)

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
	Makefile.dist	5.4 (Berkeley) 8/15/90
Makefile.reno
	Makefile	5.8 (Berkeley) 7/28/90
Version.c
	Version.c	4.9 (Berkeley) 7/21/90
db.h
	db.h	4.16 (Berkeley) 6/1/90
db_dump.c
	db_dump.c	4.30 (Berkeley) 6/1/90
db_glue.c
	db_glue.c	4.4 (Berkeley) 6/1/90
db_load.c
	db_load.c	4.37 (Berkeley) 6/1/90
db_lookup.c
	db_lookup.c	4.17 (Berkeley) 6/1/90
db_reload.c
	db_reload.c	4.21 (Berkeley) 6/1/90
db_save.c
	db_save.c	4.15 (Berkeley) 6/1/90
db_update.c
	db_update.c	4.26 (Berkeley) 6/1/90
named-xfer.c
	named-xfer.c	4.16 (Berkeley) 8/15/90
named.reload
	named.reload	5.1 (Berkeley) 2/8/89
named.reload.reno
	named.reload	5.2 (Berkeley) 6/27/89
named.restart
	named.restart	5.2 (Berkeley) 2/5/89
named.restart.reno
	named.restart	5.4 (Berkeley) 6/27/89
ns.h
	ns.h	4.32 (Berkeley) 8/15/90
ns_forw.c
	ns_forw.c	4.30 (Berkeley) 6/27/90
ns_init.c
	ns_init.c	4.35 (Berkeley) 6/27/90
ns_main.c
	ns_main.c	4.51 (Berkeley) 8/15/90
ns_maint.c
	ns_maint.c	4.38 (Berkeley) 8/15/90
ns_req.c
	ns_req.c	4.44 (Berkeley) 6/27/90
ns_resp.c
	ns_resp.c	4.63 (Berkeley) 6/1/90
ns_sort.c
	ns_sort.c	4.8 (Berkeley) 6/1/90
ns_stats.c
	ns_stats.c	4.10 (Berkeley) 6/27/90
pathnames.h
	pathnames.h	5.4 (Berkeley) 6/1/90

#endif COMMENT
