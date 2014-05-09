#!/usr/sbin/dtrace -s
/*
 * syscallbysysc.d - report on syscalls by syscall. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * $Id: syscallbysysc.d 3 2007-08-01 10:50:08Z brendan $
 */

syscall:::entry { @num[probefunc] = count(); }
