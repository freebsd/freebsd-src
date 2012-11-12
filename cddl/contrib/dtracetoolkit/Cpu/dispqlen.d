#!/usr/sbin/dtrace -s
/*
 * dispqlen.d - dispatcher queue length by CPU.
 *              Written using DTrace (Solaris 10 3/05).
 *
 * $Id: dispqlen.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	dispqlen.d		# hit Ctrl-C to end sample
 *
 * NOTES: The dispatcher queue length is an indication of CPU saturation.
 * It is not an indicatior of utilisation - the CPUs may or may not be
 * utilised when the dispatcher queue reports a length of zero.
 *
 * SEE ALSO:    uptime(1M)
 *
 * COPYRIGHT: Copyright (c) 2005 Brendan Gregg.
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
 * 27-Jun-2005  Brendan Gregg   Created this.
 * 14-Feb-2006	   "      "	Last update.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("Sampling... Hit Ctrl-C to end.\n");
}

profile:::profile-1000hz
{
	@queue[cpu] =
	    lquantize(curthread->t_cpu->cpu_disp->disp_nrunnable, 0, 64, 1);
}

dtrace:::END
{
	printa(" CPU %d%@d\n", @queue);
}
