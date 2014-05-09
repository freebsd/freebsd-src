#!/usr/sbin/dtrace -Zs
/*
 * js_objgc.d - trace JavaScript Object GC using DTrace.
 *              Written for the JavaScript DTrace provider.
 *
 * $Id: js_objgc.d 63 2007-10-04 04:34:38Z brendan $
 *
 * This traces JavaScript activity from all running browers on the system
 * which support the JavaScript DTrace provider.
 *
 * USAGE: js_objgc.d		# hit Ctrl-C to end
 *
 * FIELDS:
 *		FILE		Filename that contained the function
 *		CLASS		Class to which this new object belongs
 *		TOTAL		Object entropy (positive == uncollected)
 *
 * This script provides information on which objects are not being garbage
 * collected, an issue which causes the browser to steadily leak memory.
 * We trace object creation (+1) and destruction (-1), and provide a
 * summary each second of the running tally of the object class and
 * originating filename. If over the period of several minutes an object
 * type is still steadily increasing, then that would be of interest.
 * Be patient, depending on the rate of object creation it can take over
 * ten minutes for garbage collect to kick in.
 *
 * NOTES:
 * - it is possible that you will see negative entropy. That happens
 * when you begin tracing after some objects have already been created,
 * and then trace their destruction.
 * - there are other Things that GC handles, other than Objects; extra
 * probes can be added to trace them, should the need arise.
 *
 * COPYRIGHT: Copyright (c) 2007 Brendan Gregg.
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
 * 09-Sep-2007	Brendan Gregg	Created this.
 */

/* if you get dynvardrops, increase this, */
#pragma D option dynvarsize=32m
#pragma D option quiet

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

javascript*:::object-create
/arg2/
{
	this->file = basename(copyinstr(arg0));
	@objs[this->file, copyinstr(arg1)] = sum(1);
	filename[arg2] = this->file;
}

javascript*:::object-finalize
/filename[arg2] == NULL/
{
	@objs["<missed>", copyinstr(arg1)] = sum(-1);
}

javascript*:::object-finalize
/filename[arg2] != NULL/
{
	@objs[filename[arg2], copyinstr(arg1)] = sum(-1);
	filename[arg2] = 0;
}

profile:::tick-1sec,
dtrace:::END
{
	printf("\n %-24s %8s %-20s %23Y\n", "FILE", "TOTAL", "CLASS",
	    walltimestamp);
	printa(" %-24.24s %@8d %s\n", @objs);
}
