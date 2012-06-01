#!/usr/sbin/dtrace -s
/*
 * readdist.d - read distribution by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * $Id: readdist.d 3 2007-08-01 10:50:08Z brendan $
 */

sysinfo:::readch { @dist[execname] = quantize(arg0); }
