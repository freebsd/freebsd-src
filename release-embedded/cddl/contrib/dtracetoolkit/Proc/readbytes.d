#!/usr/sbin/dtrace -s
/*
 * readbytes.d - read bytes by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * $Id: readbytes.d 3 2007-08-01 10:50:08Z brendan $
 */

sysinfo:::readch { @bytes[execname] = sum(arg0); }
