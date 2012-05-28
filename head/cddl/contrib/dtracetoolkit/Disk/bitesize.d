#!/usr/sbin/dtrace -s
/*
 * bitesize.d - analyse disk I/O size by process.
 *              Written using DTrace (Solaris 10 3/05).
 *
 * This produces a report for the size of disk events caused by
 * processes. These are the disk events sent by the block I/O driver.
 *
 * If applications must use the disks, we generally prefer they do so
 * with large I/O sizes.
 *
 * $Id: bitesize.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	bitesize.d	# wait several seconds, then hit Ctrl-C
 *
 * FIELDS:
 *		PID		process ID
 *		CMD		command and argument list
 *		value		size in bytes
 *		count		number of I/O operations
 *
 * NOTES:
 *
 * The application may be requesting smaller sized operations, which
 * are being rounded up to the nearest sector size or UFS block size.
 * To analyse what the application is requesting, DTraceToolkit programs
 * such as Proc/fddist may help.
 *
 * SEE ALSO: seeksize.d, iosnoop
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
 * 31-Mar-2004	Brendan Gregg	Created this, build 51.
 * 10-Oct-2004	   "      "	Rewrote to use the io provider, build 63.
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
 * Process io start
 */
io:::start
{
	/* fetch details */
	this->size = args[0]->b_bcount;

	/* store details */
	@Size[pid, curpsinfo->pr_psargs] = quantize(this->size);
}

/*
 * Print final report
 */
dtrace:::END
{
	printf("\n%8s  %s\n", "PID", "CMD");
	printa("%8d  %S\n%@d\n", @Size);
}
