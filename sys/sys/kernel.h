/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	from: @(#)kernel.h	7.4 (Berkeley) 2/15/91
 *	$Id: kernel.h,v 1.3 1993/10/24 06:23:11 paul Exp $
 */

/* Global variables for the kernel. */
long rmalloc();

/* 1.1 */
long hostid;
char hostname[MAXHOSTNAMELEN];
int hostnamelen;
char domainname[MAXHOSTNAMELEN];
int domainnamelen;

/* 1.2 */
struct timeval boottime;
struct timeval time;
struct timezone tz;			/* XXX */

int hz;					/* clock frequency */
int phz;				/* alternate clock's frequency */
int tick;
int lbolt;				/* once a second sleep address */
int realitexpire();

fixpt_t	averunnable[3];
#if defined(COMPAT_43) && (defined(vax) || defined(tahoe))
double	avenrun[3];
#endif /* COMPAT_43 */

#ifdef GPROF
u_long s_textsize;
int profiling;
u_short *kcount;
char *s_lowpc;
#endif
