#!/usr/sbin/dtrace -s
/*
 * seeksize.d - analyse disk head seek distance by process.
 *              Written using DTrace (Solaris 10 3/05).
 *
 * Disk I/O events caused by processes will in turn cause the disk heads
 * to seek. This program analyses those seeks, so that we can determine
 * if processes are causing the disks to seek in a "random" or "sequential"
 * manner.
 *
 * $Id: seeksize.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	seeksize.d		# wait several seconds, then hit Ctrl-C
 *
 * FIELDS:
 *		PID	process ID
 *		CMD	command and argument list
 *		value	distance in disk blocks (sectors)
 *		count	number of I/O operations
 *
 * SEE ALSO: bitesize.d, iosnoop
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
 * 11-Sep-2004	Brendan Gregg	Created this.
 * 10-Oct-2004	   "      "	Rewrote to use the io provider.
 * 20-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet

/*
 * Print header
 */
dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

self int last[dev_t];

/*
 * Process io start
 */
io:genunix::start
/self->last[args[0]->b_edev] != 0/
{
	/* calculate seek distance */
	this->last = self->last[args[0]->b_edev];
	this->dist = (int)(args[0]->b_blkno - this->last) > 0 ?
	    args[0]->b_blkno - this->last : this->last - args[0]->b_blkno;

	/* store details */
	@Size[pid, curpsinfo->pr_psargs] = quantize(this->dist);
}

io:genunix::start
{
	/* save last position of disk head */
	self->last[args[0]->b_edev] = args[0]->b_blkno +
	    args[0]->b_bcount / 512;
}

/*
 * Print final report
 */
dtrace:::END
{
	printf("\n%8s  %s\n", "PID", "CMD");
	printa("%8d  %S\n%@d\n", @Size);
}
