#!/usr/sbin/dtrace -s
/*
 * writedist.d - write distribution by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * $Id: writedist.d 3 2007-08-01 10:50:08Z brendan $
 */

sysinfo:::writech { @dist[execname] = quantize(arg0); }
