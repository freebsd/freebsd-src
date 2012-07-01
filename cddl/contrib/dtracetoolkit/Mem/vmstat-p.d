#!/usr/sbin/dtrace -s
/*
 * vmstat-p.d - vmstat -p demo in DTrace.
 *              Written using DTrace (Solaris 10 3/05).
 *
 * This has been written to demonstrate fetching similar data as vmstat
 * from DTrace. This program is intended as a starting point for other
 * DTrace scripts, by beginning with familiar statistics.
 *
 * $Id: vmstat-p.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	vmstat-p.d
 *
 * FIELDS:
 *		swap	virtual memory free	Kbytes
 *		free	free RAM		Kbytes
 *		re	page reclaims		Kbytes
 *		mf	minor faults		Kbytes
 *		sr	scan rate		pages
 *		epi	executable page ins	Kbytes
 *		epo	executable page outs	Kbytes
 *		epf	executable frees	Kbytes
 *		api	anonymous page ins	Kbytes
 *		apo	anonymous page outs	Kbytes
 *		apf	anonymous frees		Kbytes
 *		fpi	filesystem page ins	Kbytes
 *		fpo	filesystem page outs	Kbytes
 *		fpf	filesystem frees	Kbytes
 *
 * NOTES:
 *	Most of the statistics are in units of kilobytes, unlike the
 *	original vmstat command which sometimes uses page counts.
 *	As this program does not use Kstat, there is no summary since
 *	boot line. Free RAM is both free free + cache free.
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
	sy = 0; in = 0; cs = 0; maj = 0; cow = 0; pro = 0;
	epi = 0; epo = 0; epf = 0; api = 0; apo = 0; apf = 0;
	fpi = 0; fpo = 0; fpf = 0;
	lines = SCREEN + 1;
}

/*
 * Print header
 */
dtrace:::BEGIN,
tick-1sec
/lines++ > SCREEN/
{
	printf("%14s %13s %16s %14s %13s\n",
	    "memory", "page", "executable", "anonymous", "filesystem");
	printf("%9s %7s %5s %4s %3s ",
	    "swap", "free", "re", "mf", "sr");
	printf("%4s %4s %4s %4s %4s %4s %4s %4s %4s\n",
	    "epi", "epo", "epf", "api", "apo", "apf", "fpi", "fpo", "fpf");
	lines = 0;
}

/*
 * Probe events
 */
vminfo:::pgrec	   { re += arg0; }
vminfo:::scan	   { sr += arg0; }
vminfo:::as_fault  { mf += arg0; }
vminfo:::execpgin  { epi += arg0; }
vminfo:::execpgout { epo += arg0; }
vminfo:::execfree  { epf += arg0; }
vminfo:::anonpgin  { api += arg0; }
vminfo:::anonpgout { apo += arg0; }
vminfo:::anonfree  { apf += arg0; }
vminfo:::fspgin    { fpi += arg0; }
vminfo:::fspgout   { fpo += arg0; }
vminfo:::fsfree    { fpf += arg0; }

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
	epi *= `_pagesize / 1024;
	epo *= `_pagesize / 1024;
	epf *= `_pagesize / 1024;
	api *= `_pagesize / 1024;
	apo *= `_pagesize / 1024;
	apf *= `_pagesize / 1024;
	fpi *= `_pagesize / 1024;
	fpo *= `_pagesize / 1024;
	fpf *= `_pagesize / 1024;
	re  *= `_pagesize / 1024;
	sr  *= `_pagesize / 1024;
	mf  *= `_pagesize / 1024;
	this->swap *= `_pagesize / 1024;
	this->free *= `_pagesize / 1024;

	/* print line */
	printf("%9d %7d %5d %4d %3d ",
	    this->swap, this->free, re, mf, sr);
	printf("%4d %4d %4d %4d %4d %4d %4d %4d %4d\n",
	    epi, epo, epf, api, apo, apf, fpi, fpo, fpf);

	/* clear counters */
	pi = 0; po = 0; re = 0; sr = 0; mf = 0; fr = 0;
	sy = 0; in = 0; cs = 0; maj = 0; cow = 0; pro = 0;
	epi = 0; epo = 0; epf = 0; api = 0; apo = 0; apf = 0;
	fpi = 0; fpo = 0; fpf = 0;
}
