#!/usr/sbin/dtrace -Zs
/*
 * rb_calls.d - count Ruby calls using DTrace.
 *              Written for the Ruby DTrace provider.
 *
 * $Id: rb_calls.d 28 2007-09-13 10:49:37Z brendan $
 *
 * This traces activity from all Ruby programs on the system that are
 * running with Ruby provider support.
 *
 * USAGE: rb_calls.d 		# hit Ctrl-C to end
 *
 * FIELDS:
 *		FILE		Filename of the Ruby program
 *		TYPE		Type of call (method/obj-new/...)
 *		NAME		Descriptive name of call
 *		COUNT		Number of calls during sample
 *
 * Filename and method names are printed if available.
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

#pragma D option quiet

dtrace:::BEGIN
{
        printf("Tracing... Hit Ctrl-C to end.\n");
}

ruby*:::function-entry
{
	this->name = strjoin(strjoin(copyinstr(arg0), "::"), copyinstr(arg1));
        @calls[basename(copyinstr(arg2)), "method", this->name] = count();
}

ruby*:::object-create-start
{
	this->name = copyinstr(arg0);
	this->filename = basename(copyinstr(arg1));
	this->filename = this->filename != NULL ? this->filename : ".";
        @calls[this->filename, "obj-new", this->name] = count();
}

ruby*:::object-free
{
	this->name = copyinstr(arg0);
        @calls[".", "obj-free", this->name] = count();
}

ruby*:::gc-begin
{
        @calls[".", "gc", "begin"] = count();
}

ruby*:::raise
{
	this->name = copyinstr(arg0);
        @calls[basename(copyinstr(arg1)), "raise", this->name] = count();
}

ruby*:::rescue
{
        @calls[basename(copyinstr(arg0)), "rescue", "-"] = count();
}

dtrace:::END
{
        printf(" %-24s %-10s %-30s %8s\n", "FILE", "TYPE", "NAME", "CALLS");
        printa(" %-24s %-10s %-30s %@8d\n", @calls);
}
