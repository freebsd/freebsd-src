#!/usr/sbin/dtrace -s
/*
 * vmstat.d - vmstat demo in DTrace.
 *            Written using DTrace (Solaris 10 3/05).
 *
 * This has been written to demonstrate fetching the same data as vmstat
 * from DTrace. This program is intended as a starting point for other
 * DTrace scripts, by beginning with familiar statistics.
 *
 * $Id: vmstat.d 8 2007-08-06 05:55:26Z brendan $
 *
 * USAGE:	vmstat.d
 *
 * FIELDS:
 *		w	swapped out LWPs	number
 *		swap	virtual memory free	Kbytes
 *		free	free RAM		Kbytes
 *		re	page reclaims		Kbytes
 *		mf	minor faults		Kbytes
 *		pi	page ins		Kbytes
 *		po	page outs		Kbytes
 *		fr	pages freed		Kbytes
 *		sr	scan rate		pages
 *		in	interrupts		number
 *		sy	system calls		number
 *		cs	context switches	number
 *
 * NOTES:
 *  Most of the statistics are in units of kilobytes, unlike the
 *  original vmstat command which sometimes uses page counts.
 *  As this program does not use Kstat, there is no summary since boot line.
 *  Free RAM is both free free + cache free.
 *
 * SEE ALSO:	vmstat(1M)
 *
 * COPYRIGHT: Copyright (c) 2005 Brendan Gregg.
 *
 * CDDL HEADER START
 *
 *  The contents of this file are subject to the terms of the
 *  Common Development and Distribution License, Version 1.0 only
 *  (the "License").  You may not use this file except in compliance
 *  with the License.
 *
 *  You can obtain a copy of the license at Docs/cddl1.txt
 *  or http://www.opensolaris.org/os/licensing.
 *  See the License for the specific language governing permissions
 *  and limitations under the License.
 *
 * CDDL HEADER END
 *
 * 11-Jun-2005  Brendan Gregg   Created this.
 * 08-Jan-2006	   "      "	Last update.
 */

#pragma D option quiet

inline int SCREEN = 21;

/*
 * Initialise variables
 */
dtrace:::BEGIN
{
	pi = 0; po = 0; re = 0; sr = 0; mf = 0; fr = 0;
	sy = 0; in = 0; cs = 0;
	lines = SCREEN + 1;
}

/*
 * Print header
 */
dtrace:::BEGIN,
profile:::tick-1sec
/lines++ > SCREEN/
{
	printf(" %1s %10s %8s %5s %5s %4s %4s %4s %4s %5s %6s %4s\n",
	    "w", "swap", "free", "re", "mf", "pi", "po", "fr", "sr",
	    "in", "sy", "cs");
	lines = 0;
}

/*
 * Probe events
 */
vminfo:::pgpgin   { pi += arg0; }
vminfo:::pgpgout  { po += arg0; }
vminfo:::pgrec    { re += arg0; }
vminfo:::scan	  { sr += arg0; }
vminfo:::as_fault { mf += arg0; }
vminfo:::dfree    { fr += arg0; }

syscall:::entry		{ sy++; }
sdt:::interrupt-start	{ in++; }
sched::resume:on-cpu	{ cs++; }

/*
 * Print output line
 */
profile:::tick-1sec
{
	/* fetch free mem */
	this->free = `freemem;

	/*
	 * fetch free swap
	 *
	 * free swap is described in /usr/include/vm/anon.h as,
	 * MAX(ani_max - ani_resv, 0) + (availrmem - swapfs_minfree)
	 */
	this->ani_max = `k_anoninfo.ani_max;
	this->ani_resv = `k_anoninfo.ani_phys_resv + `k_anoninfo.ani_mem_resv;
	this->swap = (this->ani_max - this->ani_resv > 0 ?
	    this->ani_max - this->ani_resv : 0) + `availrmem - `swapfs_minfree;

	/* fetch w */
	this->w = `nswapped;

	/* convert to Kbytes */
	pi *= `_pagesize / 1024;
	po *= `_pagesize / 1024;
	re *= `_pagesize / 1024;
	sr *= `_pagesize / 1024;
	mf *= `_pagesize / 1024;
	fr *= `_pagesize / 1024;
	this->swap *= `_pagesize / 1024;
	this->free *= `_pagesize / 1024;

	/* print line */
	printf(" %1d %10d %8d %5d %5d %4d %4d %4d %4d %5d %6d %4d\n",
	    this->w, this->swap, this->free, re, mf, pi, po, fr, sr,
	    in, sy, cs);

	/* clear counters */
	pi = 0; po = 0; re = 0; sr = 0; mf = 0; fr = 0;
	sy = 0; in = 0; cs = 0;
}
