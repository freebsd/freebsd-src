#!/usr/sbin/dtrace -s
/*
 * filebyproc.d - snoop files opened by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * $Id: filebyproc.d 3 2007-08-01 10:50:08Z brendan $
 */

syscall::open*:entry { printf("%s %s", execname, copyinstr(arg0)); }
