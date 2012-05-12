#!/usr/sbin/dtrace -s
/*
 * minfbyproc.d - minor faults by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * $Id: minfbyproc.d 3 2007-08-01 10:50:08Z brendan $
 */

vminfo:::as_fault { @mem[execname] = sum(arg0); }
