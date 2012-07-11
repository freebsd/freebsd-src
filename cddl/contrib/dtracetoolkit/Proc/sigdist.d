#!/usr/sbin/dtrace -s
/*
 * sigdist.d - signal distribution by process name.
 *             Written using DTrace (Solaris 10 3/05)
 *
 * This is a simple DTrace script that prints the number of signals
 * recieved by process and signal number. This script is also available
 * as /usr/demo/dtrace/sig.d, where it originates.
 *
 * $Id: sigdist.d 4 2007-08-01 11:01:38Z brendan $
 *
 * USAGE: 	sigdist.d	# hit Ctrl-C to end
 *
 * FIELDS:
 *		SENDER		process name of sender
 *		RECIPIENT	process name of target
 *		SIG		signal number, see signal(3head)
 *		COUNT		number of signals sent
 *
 * BASED ON: /usr/demo/dtrace/sig.d
 *
 * SEE ALSO: DTrace Guide "proc Provider" chapter (docs.sun.com)
 *           kill.d(1M)
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
 * 09-Jun-2005	Brendan Gregg	Created this.
 * 20-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

proc:::signal-send
{
	@Count[execname, stringof(args[1]->pr_fname), args[2]] = count();
}

dtrace:::END
{
	printf("%16s %16s %6s %6s\n", "SENDER", "RECIPIENT", "SIG", "COUNT");
	printa("%16s %16s %6d %6@d\n", @Count);
}
