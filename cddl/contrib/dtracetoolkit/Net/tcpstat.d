#!/usr/sbin/dtrace -s
/*
 * tcpstat.d - print TCP statistics. Uses DTrace.
 *
 * This prints TCP statistics every second, retrieved from the MIB provider.
 *
 * $Id: tcpstat.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	tcpstat.d
 *
 * FIELDS:
 *		TCP_out		TCP bytes sent
 *		TCP_outRe	TCP bytes retransmitted
 *		TCP_in		TCP bytes received
 *		TCP_inDup	TCP bytes received duplicated
 *		TCP_inUn	TCP bytes received out of order
 *
 * The above TCP statistics are documented in the mib2_tcp struct
 * in the /usr/include/inet/mib2.h file; and also in the mib provider
 * chapter of the DTrace Guide, http://docs.sun.com/db/doc/817-6223.
 *
 * COPYRIGHT: Copyright (c) 2005 Brendan Gregg.
 *
 * CDDL HEADER START
 *
 *  The contents of this file are subject to the terms of the
 *  Common Development and Distribution License, Version 1.0 only
 *  (the "License").  You may not use this file except in compliance
 *  with the License.
 *
 *  You can obtain a copy of the license at Docs/cddl1.txt
 *  or http://www.opensolaris.org/os/licensing.
 *  See the License for the specific language governing permissions
 *  and limitations under the License.
 *
 * CDDL HEADER END
 *
 * 15-May-2005  Brendan Gregg   Created this.
 * 15-May-2005	   "      "	Last update.
 */

#pragma D option quiet

/*
 * Declare Globals
 */
dtrace:::BEGIN
{
	TCP_out = 0; TCP_outRe = 0;
	TCP_in = 0; TCP_inDup = 0; TCP_inUn = 0;
	LINES = 20; line = 0;
}

/*
 * Print Header
 */
profile:::tick-1sec { line--; }

profile:::tick-1sec
/line <= 0 /
{
	printf("%11s %11s %11s %11s %11s\n",
	    "TCP_out", "TCP_outRe", "TCP_in", "TCP_inDup", "TCP_inUn");

	line = LINES;
}

/*
 * Save Data
 */
mib:::tcpOutDataBytes		{ TCP_out += arg0;   }
mib:::tcpRetransBytes		{ TCP_outRe += arg0; }
mib:::tcpInDataInorderBytes	{ TCP_in += arg0;    }
mib:::tcpInDataDupBytes		{ TCP_inDup += arg0; }
mib:::tcpInDataUnorderBytes	{ TCP_inUn += arg0;  }

/*
 * Print Output
 */
profile:::tick-1sec
{
	printf("%11d %11d %11d %11d %11d\n",
	    TCP_out, TCP_outRe, TCP_in, TCP_inDup, TCP_inUn);

	/* clear values */
	TCP_out   = 0;
	TCP_outRe = 0;
	TCP_in    = 0;
	TCP_inDup = 0;
	TCP_inUn  = 0;
}
