#!/usr/sbin/dtrace -s
/*
 * lockbyproc.d - lock time by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * $Id: lockbyproc.d 3 2007-08-01 10:50:08Z brendan $
 */

lockstat:::adaptive-block { @time[execname] = sum(arg1); }
