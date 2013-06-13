#!/usr/sbin/dtrace -CZs
/*
 * j_calldist.d - measure Java elapsed times for different types of operation.
 *                Written for the Java hotspot DTrace provider.
 *
 * $Id: j_calldist.d 59 2007-10-03 08:21:58Z brendan $
 *
 * This traces activity from all Java processes on the system with hotspot
 * provider support (1.6.0). Method calls are only visible when using the
 * flag "+ExtendedDTraceProbes". eg, java -XX:+ExtendedDTraceProbes classfile
 *
 * USAGE: j_calldist.d [top]	# hit Ctrl-C to end
 *
 * The "top" optional argument will truncate the output for each report
 * section to that many lines, with a default of 10.
 *
 * FIELDS:
 *		1		Process ID
 *		2		Type of call (method/gc)
 *		3		Name of call
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

#define TOP	10		/* default output truncation */
#define B_FALSE	0

#pragma D option quiet
#pragma D option defaultargs

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
	top = $1 != 0 ? $1 : TOP;
}

hotspot*:::method-entry
{
	self->depth[arg0]++;
	self->exclude[arg0, self->depth[arg0]] = 0;
	self->method[arg0, self->depth[arg0]] = timestamp;
}

hotspot*:::method-return
/self->method[arg0, self->depth[arg0]]/
{
	this->elapsed_incl = timestamp - self->method[arg0, self->depth[arg0]];
	this->elapsed_excl = this->elapsed_incl -
	    self->exclude[arg0, self->depth[arg0]];
	self->method[arg0, self->depth[arg0]] = 0;
	self->exclude[arg0, self->depth[arg0]] = 0;

	this->class = (char *)copyin(arg1, arg2 + 1);
	this->class[arg2] = '\0';
	this->method = (char *)copyin(arg3, arg4 + 1);
	this->method[arg4] = '\0';
	this->name = strjoin(strjoin(stringof(this->class), "."),
	    stringof(this->method));

	@types_incl[pid, "method", this->name] =
	    quantize(this->elapsed_incl / 1000);
	@types_excl[pid, "method", this->name] =
	    quantize(this->elapsed_excl / 1000);

	self->depth[arg0]--;
	self->exclude[arg0, self->depth[arg0]] += this->elapsed_incl;
}

hotspot*:::gc-begin
{
	self->gc = timestamp;
	self->full = (boolean_t)arg0;
}

hotspot*:::gc-end
/self->gc/
{
	this->elapsed = timestamp - self->gc;

	@types[pid, "gc", self->full == B_FALSE ? "GC" : "Full GC"] =
	    quantize(this->elapsed / 1000);

	self->gc = 0;
	self->full = 0;
}

dtrace:::END
{
	trunc(@types, top);
	printf("\nTop %d elapsed times (us),\n", top);
	printa("   PID=%d, %s, %s %@d\n", @types);

	trunc(@types_excl, top);
	printf("\nTop %d exclusive method elapsed times (us),\n", top);
	printa("   PID=%d, %s, %s %@d\n", @types_excl);

	trunc(@types_incl, top);
	printf("\nTop %d inclusive method elapsed times (us),\n", top);
	printa("   PID=%d, %s, %s %@d\n", @types_incl);
}
