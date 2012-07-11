#!/usr/sbin/dtrace -s
/*
 * hotspot.d - plot disk event by location, look for hotspots.
 *             Written in DTrace (Solaris 10 3/05).
 *
 * This simple DTrace script determines if disk activity is occuring in
 * the one place - a "hotspot". This helps us understand the system's usage
 * of a disk, it does not imply that the existance or not of a hotspot is
 * good or bad (often may be good, less seeking).
 *
 * $Id: hotspot.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:       hotspot.d       # hit Ctrl-C to end
 *
 * FIELDS:
 *              Disk            disk instance name
 *              Major           driver major number
 *              Minor           driver minor number
 *              value           location, by megabyte
 *              count           number of I/O operations
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
 * 07-May-2005	Brendan Gregg	Created this.
 * 20-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet

inline int DISK_MB_MAX = 1000000;	/* max size of a single disk */
inline int REPORT_SCALE_MB = 1000;	/* output step size for report */

/*
 * Print header
 */
dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

/*
 * Process disk event
 */
io:::start
{
	this->mb = args[0]->b_blkno / 2048;
	@Block[args[1]->dev_statname, args[1]->dev_major, args[1]->dev_minor] =
	    lquantize(this->mb, 0, DISK_MB_MAX, REPORT_SCALE_MB);
}

/*
 * Print final report
 */
dtrace:::END
{
	printa("Disk: %s   Major,Minor: %d,%d\n%@d\n", @Block);
}
