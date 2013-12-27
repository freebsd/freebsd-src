#!/usr/sbin/dtrace -Zs
/*
 * php_funccalls.d - measure PHP function calls using DTrace.
 *                   Written for the PHP DTrace provider.
 *
 * $Id: php_funccalls.d 53 2007-09-24 04:58:38Z brendan $
 *
 * This traces PHP activity from all running programs on the system
 * which support the PHP DTrace provider.
 *
 * USAGE: php_funccalls.d 		# hit Ctrl-C to end
 *
 * FIELDS:
 *              FILE		Filename that contained the function
 *		FUNC		PHP function name
 *		CALLS		Function calls during this sample
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

php*:::function-entry
/arg0/
{
	@funcs[basename(copyinstr(arg1)), copyinstr(arg0)] = count();
}

dtrace:::END
{
	printf(" %-32s %-32s %8s\n", "FILE", "FUNC", "CALLS");
	printa(" %-32s %-32s %@8d\n", @funcs);
}
