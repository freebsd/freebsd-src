#!/usr/sbin/dtrace -s
/*
 * swapinfo.d - print virtual memory info (swap).
 *              Written using DTrace (Solaris 10 3/05)
 *
 * Prints swap usage details for RAM and disk based swap.
 * This script is UNDER CONSTRUCTION, check for newer versions.
 *
 * $Id: swapinfo.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:       swapinfo.d	(check for newer versions)
 *
 * FIELDS:
 *              RAM Total       Total RAM installed
 *              RAM Unusable    RAM consumed by the OBP and TSBs
 *              RAM Kernel      Kernel resident in RAM (and usually locked)
 *              RAM Locked      Locked memory pages from swap (Anon)
 *              RAM Used        anon + exec + file pages used
 *              RAM Free        free memory + page cache free
 *              Disk Total      Total disk swap configured
 *              Disk Resv       Disk swap allocated + reserved
 *              Disk Avail      Disk swap available for reservation
 *              Swap Total      Total Virtual Memory usable
 *              Swap Resv       VM allocated + reserved
 *              Swap Avail      VM available for reservation
 *              Swap MinFree    VM kept free from reservations
 *
 * SEE ALSO: swapinfo - K9Toolkit, http://www.brendangregg.com/k9toolkit.html
 *           vmstat 1 2; swap -s; echo ::memstat | mdb -k
 *           RMCmem - The MemTool Package
 *           RICHPse - The SE Toolkit
 *           "Clearing up swap space confusion" Unix Insider, Adrian Cockcroft
 *           "Solaris Internals", Jim Mauro, Richard McDougall
 *           /usr/include/vm/anon.h, /usr/include/sys/systm.h
 *
 * COPYRIGHT: Copyright (c) 2005, 2006 Brendan Gregg.
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
 * Author: Brendan Gregg  [Sydney, Australia]
 *
 * 11-Jun-2005  Brendan Gregg   Created this.
 * 24-Apr-2006	   "	  "	Improved disk measurements; changed terms.
 * 24-Apr-2006	   "	  "	Last update.
 */

#pragma D option quiet
#pragma D option bufsize=16k

inline int DEBUG = 0;

dtrace:::BEGIN
{
	/* Debug stats */
	this->ani_max = `k_anoninfo.ani_max;
	this->ani_phys_resv = `k_anoninfo.ani_phys_resv;
	this->ani_mem_resv = `k_anoninfo.ani_mem_resv;
	this->ani_locked = `k_anoninfo.ani_locked_swap;
	this->availrmem = `availrmem;

	/* RAM stats */
	this->ram_total = `physinstalled;
	this->unusable  = `physinstalled - `physmem;
	this->locked    = `pages_locked;
	this->ram_used  = `availrmem - `freemem;
	this->freemem   = `freemem;
	this->kernel    = `physmem - `pages_locked - `availrmem;

	/* Disk stats */
	this->disk_total = `k_anoninfo.ani_max;
	this->disk_resv = `k_anoninfo.ani_phys_resv;
	this->disk_avail = this->disk_total - this->disk_resv;

	/* Total Swap stats */
	this->minfree = `swapfs_minfree;
	this->reserve = `swapfs_reserve;
	/* this is TOTAL_AVAILABLE_SWAP from /usr/include/vm/anon.h, */
	this->swap_total = `k_anoninfo.ani_max +
	    (`availrmem - `swapfs_minfree > 0 ?
	    `availrmem - `swapfs_minfree : 0);
	/* this is CURRENT_TOTAL_AVAILABLE_SWAP from /usr/include/vm/anon.h, */
	this->swap_avail = `k_anoninfo.ani_max - `k_anoninfo.ani_phys_resv +
	    (`availrmem - `swapfs_minfree > 0 ?
	    `availrmem - `swapfs_minfree : 0);
	this->swap_resv = this->swap_total - this->swap_avail;

	/* Convert to Mbytes */
	this->ani_phys_resv *= `_pagesize;  this->ani_phys_resv /= 1048576;
	this->ani_mem_resv *= `_pagesize;  this->ani_mem_resv /= 1048576;
	this->ani_locked *= `_pagesize;  this->ani_locked /= 1048576;
	this->ani_max	*= `_pagesize;  this->ani_max	/= 1048576;
	this->availrmem	*= `_pagesize;  this->availrmem	/= 1048576;
	this->ram_total	*= `_pagesize;  this->ram_total	/= 1048576;
	this->unusable	*= `_pagesize;  this->unusable	/= 1048576;
	this->kernel	*= `_pagesize;  this->kernel	/= 1048576;
	this->locked	*= `_pagesize;  this->locked	/= 1048576;
	this->ram_used	*= `_pagesize;  this->ram_used	/= 1048576;
	this->freemem	*= `_pagesize;  this->freemem	/= 1048576;
	this->disk_total *= `_pagesize; this->disk_total /= 1048576;
	this->disk_resv	*= `_pagesize;  this->disk_resv	/= 1048576;
	this->disk_avail *= `_pagesize;  this->disk_avail /= 1048576;
	this->swap_total *= `_pagesize; this->swap_total /= 1048576;
	this->swap_avail *= `_pagesize;  this->swap_avail /= 1048576;
	this->swap_resv	*= `_pagesize;  this->swap_resv	/= 1048576;
	this->minfree	*= `_pagesize;  this->minfree	/= 1048576;
	this->reserve	*= `_pagesize;  this->reserve	/= 1048576;

	/* Print debug */
	DEBUG ? printf("DEBUG   availrmem %5d MB\n", this->availrmem) : 1;
	DEBUG ? printf("DEBUG     freemem %5d MB\n", this->freemem) : 1;
	DEBUG ? printf("DEBUG     ani_max %5d MB\n", this->ani_max) : 1;
	DEBUG ? printf("DEBUG ani_phys_re %5d MB\n", this->ani_phys_resv) : 1;
	DEBUG ? printf("DEBUG  ani_mem_re %5d MB\n", this->ani_mem_resv) : 1;
	DEBUG ? printf("DEBUG  ani_locked %5d MB\n", this->ani_locked) : 1;
	DEBUG ? printf("DEBUG     reserve %5d MB\n", this->reserve) : 1;
	DEBUG ? printf("\n") : 1;

	/* Print report */
	printf("RAM  _______Total %5d MB\n", this->ram_total);
	printf("RAM      Unusable %5d MB\n", this->unusable);
	printf("RAM        Kernel %5d MB\n", this->kernel);
	printf("RAM        Locked %5d MB\n", this->locked);
	printf("RAM          Used %5d MB\n", this->ram_used);
	printf("RAM          Free %5d MB\n", this->freemem);
	printf("\n");
	printf("Disk _______Total %5d MB\n", this->disk_total);
	printf("Disk         Resv %5d MB\n", this->disk_resv);
	printf("Disk        Avail %5d MB\n", this->disk_avail);
	printf("\n");
	printf("Swap _______Total %5d MB\n", this->swap_total);
	printf("Swap         Resv %5d MB\n", this->swap_resv);
	printf("Swap        Avail %5d MB\n", this->swap_avail);
	printf("Swap    (Minfree) %5d MB\n", this->minfree);

	DEBUG ? printf("\nNow run other commands for confirmation.\n") : 1;
	! DEBUG ? exit(0) : 1;
}
