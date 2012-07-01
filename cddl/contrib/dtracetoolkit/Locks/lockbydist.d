#!/usr/sbin/dtrace -s
/*
 * lockbydist.d - lock distrib. by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * $Id: lockbydist.d 3 2007-08-01 10:50:08Z brendan $
 */

lockstat:::adaptive-block { @time[execname] = quantize(arg1); }
