/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 *	from: @(#)vtimes.h	7.2 (Berkeley) 5/5/91
 *	$Id: vtimes.h,v 1.2 1993/10/16 17:18:31 rgrimes Exp $
 */

/*
 * Structure returned by vtimes() and in vwait().
 * In vtimes() two of these are returned, one for the process itself
 * and one for all its children.  In vwait() these are combined
 * by adding componentwise (except for maxrss, which is max'ed).
 */
struct vtimes {
	int	vm_utime;		/* user time (60'ths) */
	int	vm_stime;		/* system time (60'ths) */
	/* divide next two by utime+stime to get averages */
	unsigned vm_idsrss;		/* integral of d+s rss */
	unsigned vm_ixrss;		/* integral of text rss */
	int	vm_maxrss;		/* maximum rss */
	int	vm_majflt;		/* major page faults */
	int	vm_minflt;		/* minor page faults */
	int	vm_nswap;		/* number of swaps */
	int	vm_inblk;		/* block reads */
	int	vm_oublk;		/* block writes */
};
