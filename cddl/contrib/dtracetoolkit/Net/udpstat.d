#!/usr/sbin/dtrace -s
/*
 * udpstat.d - print UDP statistics. Uses DTrace.
 *
 * This prints UDP statistics every second, retrieved from the MIB provider.
 *
 * $Id: udpstat.d 59 2007-10-03 08:21:58Z brendan $
 *
 * USAGE:	udpstat.d
 *
 * FIELDS:
 *		UDP_out		UDP datagrams sent
 *		UDP_outErr	UDP datagrams errored on send
 *		UDP_in		UDP datagrams received
 *		UDP_inErr	UDP datagrams undeliverable
 *		UDP_noPort	UDP datagrams received to closed ports
 *
 * The above UDP statistics are documented in the mib2_udp struct
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
 * 25-Jul-2005  Brendan Gregg   Created this.
 * 25-Jul-2005	   "      "	Last update.
 */

#pragma D option quiet

/*
 * Declare Globals
 */
dtrace:::BEGIN
{
	UDP_in = 0; UDP_out = 0;
	UDP_inErr = 0; UDP_outErr = 0; UDP_noPort = 0;
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
	    "UDP_out", "UDP_outErr", "UDP_in", "UDP_inErr", "UDP_noPort");

	line = LINES;
}

/*
 * Save Data
 */
mib:::udp*InDatagrams	{ UDP_in += arg0;	}
mib:::udp*OutDatagrams	{ UDP_out += arg0;	}
mib:::udpInErrors	{ UDP_inErr += arg0;	}
mib:::udpInCksumErrs	{ UDP_inErr += arg0;	}
mib:::udpOutErrors	{ UDP_outErr += arg0;	}
mib:::udpNoPorts	{ UDP_noPort += arg0;	}

/*
 * Print Output
 */
profile:::tick-1sec
{
	printf("%11d %11d %11d %11d %11d\n",
	    UDP_out, UDP_outErr, UDP_in, UDP_inErr, UDP_noPort);

	/* clear values */
	UDP_out		= 0;
	UDP_outErr	= 0;
	UDP_in		= 0;
	UDP_inErr	= 0;
	UDP_noPort	= 0;
}
