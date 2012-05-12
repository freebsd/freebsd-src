#!/usr/sbin/dtrace -s
/*
 * newproc.d - snoop new processes as they are executed. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * $Id: newproc.d 3 2007-08-01 10:50:08Z brendan $
 */

proc:::exec-success { trace(curpsinfo->pr_psargs); }
