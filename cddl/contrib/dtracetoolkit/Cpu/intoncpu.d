#!/usr/sbin/dtrace -s
/*
 * intoncpu.d - print interrupt on-cpu usage.
 *              Written using DTrace (Solaris 10 3/05)
 *
 * $Id: intoncpu.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:       intoncpu.d      # wait several seconds, then hit Ctrl-C
 *
 * FIELDS:
 *		value	Time interrupt thread was on-cpu, ns
 *		count	Number of occurrences of at least this time
 *
 * BASED ON: /usr/demo/dtrace/intr.d
 *
 * SEE ALSO: DTrace Guide "sdt Provider" chapter (docs.sun.com)
 *           intrstat(1M)
 *
 * PORTIONS: Copyright (c) 2005, 2006 Brendan Gregg.
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
 * 09-May-2005  Brendan Gregg   Created this.
 * 20-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

sdt:::interrupt-start
{
	self->ts = vtimestamp;
}

sdt:::interrupt-complete
/self->ts && arg0 != 0/
{
	this->devi = (struct dev_info *)arg0;
	/* this checks the pointer is valid, */
	self->name = this->devi != 0 ?
	    stringof(`devnamesp[this->devi->devi_major].dn_name) : "?";
	this->inst = this->devi != 0 ? this->devi->devi_instance : 0;
	@Time[self->name, this->inst] = quantize(vtimestamp - self->ts);
	self->name = 0;
}

dtrace:::END
{
	printa("%s%d\n%@d", @Time);
}
