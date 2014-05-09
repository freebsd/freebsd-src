#!/usr/sbin/dtrace -s
/*
 * syscallbyproc.d - report on syscalls by process name . DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * $Id: syscallbyproc.d 3 2007-08-01 10:50:08Z brendan $
 */

syscall:::entry { @num[execname] = count(); }
