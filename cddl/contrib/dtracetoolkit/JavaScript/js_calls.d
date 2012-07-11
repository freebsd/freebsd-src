#!/usr/sbin/dtrace -Zs
/*
 * js_calls.d - count JavaScript calls using DTrace.
 *              Written for the JavaScript DTrace provider.
 *
 * $Id: js_calls.d 63 2007-10-04 04:34:38Z brendan $
 *
 * This traces activity from all browsers on the system that are
 * running with JavaScript provider support.
 *
 * USAGE: js_calls.d 		# hit Ctrl-C to end
 *
 * FIELDS:
 *		FILE		Filename of the JavaScript program
 *		TYPE		Type of call (func/obj-new/...)
 *		NAME		Descriptive name of call
 *		COUNT		Number of calls during sample
 *
 * Filename and function names are printed if available.
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

javascript*:::function-entry
{
	this->name = copyinstr(arg2);
        @calls[basename(copyinstr(arg0)), "func", this->name] = count();
}

javascript*:::execute-start
{
	this->filename = basename(copyinstr(arg0));
        @calls[this->filename, "exec", "."] = count();
}

javascript*:::object-create-start
{
	this->name = copyinstr(arg1);
	this->filename = basename(copyinstr(arg0));
        @calls[this->filename, "obj-new", this->name] = count();
}

javascript*:::object-finalize
{
	this->name = copyinstr(arg1);
        @calls["<null>", "obj-free", this->name] = count();
}

dtrace:::END
{
        printf(" %-24s %-10s %-30s %8s\n", "FILE", "TYPE", "NAME", "CALLS");
        printa(" %-24s %-10s %-30s %@8d\n", @calls);
}
