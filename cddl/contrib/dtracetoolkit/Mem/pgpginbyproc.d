#!/usr/sbin/dtrace -s
/*
 * pgpginbyproc.d - pages paged in by process name. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * $Id: pgpginbyproc.d 3 2007-08-01 10:50:08Z brendan $
 */

vminfo:::pgpgin { @pg[execname] = sum(arg0); }
