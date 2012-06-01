#!/usr/sbin/dtrace -s
/*
 * dnlcps.d - DNLC stats by process.
 *            Written in DTrace (Solaris 10 3/05).
 *
 * The DNLC is the Directory Name Lookup Cache. Filename lookups often
 * return a hit from here, before needing to traverse the regular file
 * system cache or go to disk.
 *
 * dnlcps.d prints DNLC statistics by process.
 *
 * $Id: dnlcps.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	dnlcps.d        # wait several seconds, then hit Ctrl-C
 *
 * FIELDS:
 *		PID             Process ID
 *		CMD          	Command name
 *		value        	0 == miss, 1 == hit
 *		count        	number of occurrences
 *
 * COPYRIGHT: Copyright (c) 2006 Brendan Gregg.
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
 * 27-Mar-2004	Brendan Gregg	Created this.
 * 14-Jun-2005	   "      "	Rewrote this a lot.
 * 18-Feb-2006	   "      "	Last update.
 */

#pragma D option quiet

/*
 * Print header
 */
dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

/*
 * DNLC return
 */
fbt:genunix:dnlc_lookup:return
{
	this->code = arg1 == 0 ? 0 : 1;
	@Result[execname, pid] = lquantize(this->code, 0, 1, 1);
}

/*
 * Print report
 */
dtrace:::END
{
	printa(" CMD: %-16s PID: %d\n%@d\n", @Result);
}
