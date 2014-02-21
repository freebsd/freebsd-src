#!/usr/sbin/dtrace -s
/*
 * nfswizard.d - nfs client activity wizard.
 *               Written using DTrace (Solaris 10 3/05).
 *
 * This examines activity caused by NFS client processes on the same server
 * that you are running this script on. A detailed report is generated
 * to explain various details of NFS client activity, including response
 * times and file access.
 *
 * $Id: nfswizard.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:     nfswizard.d    # hit Ctrl-C to end sample
 *
 * COPYRIGHT: Copyright (c) 2005, 2006 Brendan Gregg.
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
 * 02-Dec-2005  Brendan Gregg   Created this.
 * 20-Apr-2006	   "	  "	Last update.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
	scriptstart = walltimestamp;
	timestart = timestamp;
}

io:nfs::start
{
	/* tally file sizes */
	@file[args[2]->fi_pathname] = sum(args[0]->b_bcount);

	/* time response */
	start[args[0]->b_addr] = timestamp;

	/* overall stats */
	@rbytes = sum(args[0]->b_flags & B_READ ? args[0]->b_bcount : 0);
	@wbytes = sum(args[0]->b_flags & B_READ ? 0 : args[0]->b_bcount);
	@events = count();
}

io:nfs::done
/start[args[0]->b_addr]/
{
	/* calculate and save response time stats */
	this->elapsed = timestamp - start[args[0]->b_addr];
	@maxtime = max(this->elapsed);
	@avgtime = avg(this->elapsed);
	@qnztime = quantize(this->elapsed / 1000);
}

dtrace:::END
{
	/* print header */
	printf("NFS Client Wizard. %Y -> %Y\n\n", scriptstart, walltimestamp);

	/* print read/write stats */
	printa("Read:  %@d bytes ", @rbytes);
	normalize(@rbytes, 1000000);
	printa("(%@d Mb)\n", @rbytes);
	printa("Write: %@d bytes ", @wbytes);
	normalize(@wbytes, 1000000);
	printa("(%@d Mb)\n\n", @wbytes);

	/* print throughput stats */
	denormalize(@rbytes);
	normalize(@rbytes, (timestamp - timestart) / 1000000);
	printa("Read:  %@d Kb/sec\n", @rbytes);
	denormalize(@wbytes);
	normalize(@wbytes, (timestamp - timestart) / 1000000);
	printa("Write: %@d Kb/sec\n\n", @wbytes);

	/* print time stats */
	printa("NFS I/O events:    %@d\n", @events);
	normalize(@avgtime, 1000000);
	printa("Avg response time: %@d ms\n", @avgtime);
	normalize(@maxtime, 1000000);
	printa("Max response time: %@d ms\n\n", @maxtime);
	printa("Response times (us):%@d\n", @qnztime);

	/* print file stats */
	printf("Top 25 files accessed (bytes):\n");
	printf("   %-64s %s\n", "PATHNAME", "BYTES");
	trunc(@file, 25);
	printa("   %-64s %@d\n", @file);
}
