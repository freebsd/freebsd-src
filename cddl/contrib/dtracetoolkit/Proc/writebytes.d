#!/usr/sbin/dtrace -s
/*
 * writebytes.d - write bytes by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * $Id: writebytes.d 3 2007-08-01 10:50:08Z brendan $
 */

sysinfo:::writech { @bytes[execname] = sum(arg0); }
