#!/usr/sbin/dtrace -s
/*
 * iofile.d - I/O wait time by filename and process.
 *            Written using DTrace (Solaris 10 3/05).
 *
 * This prints the total I/O wait times for each filename by process.
 * This can help determine why an application is performing poorly by
 * identifying which file they are waiting on, and the total times.
 * Both disk and NFS I/O are measured.
 *
 * $Id: iofile.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	iofile.d	# wait, then hit Ctrl-C to end
 *
 * FIELDS:
 *		PID		Process ID
 *		CMD		Process name
 *		TIME		Total wait time for disk events, us
 *		FILE		File pathname
 *
 * BASED ON: /usr/demo/dtrace/iocpu.d
 *
 * SEE ALSO: iosnoop, iotop
 *
 * PORTIONS: Copyright (c) 2005, 2006 Brendan Gregg.
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
 * 24-Jul-2005	Brendan Gregg	Created this.
 * 20-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet

/* print header */
dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

/* save time at start */
io:::wait-start
{
	self->start = timestamp;
}

/* process event */
io:::wait-done
/self->start/
{
	/*
	 * wait-done is used as we are measing wait times. It also
	 * is triggered when the correct thread is on the CPU, obviating
	 * the need to link process details to the start event.
	 */
	this->elapsed = timestamp - self->start;
	@files[pid, execname, args[2]->fi_pathname] = sum(this->elapsed);
	self->start = 0;
}

/* print report */
dtrace:::END
{
	normalize(@files, 1000);
	printf("%6s %-12s %8s %s\n", "PID", "CMD", "TIME", "FILE");
	printa("%6d %-12.12s %@8d %s\n", @files);
}
