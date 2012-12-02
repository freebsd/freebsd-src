#!/usr/sbin/dtrace -s
/*
 * modcalls.d - kernel function calls by module. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * $Id: modcalls.d 3 2007-08-01 10:50:08Z brendan $
 */

fbt:::entry { @calls[probemod] = count(); }
