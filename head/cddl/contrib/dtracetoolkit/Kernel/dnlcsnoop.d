#!/usr/sbin/dtrace -s
/*
 * dnlcsnoop.d - snoop DNLC activity.
 *               Written in DTrace (Solaris 10 3/05).
 *
 * The DNLC is the Directory Name Lookup Cache. Filename lookups often
 * return a hit from here, before needing to traverse the regular file
 * system cache or go to disk.
 *
 * $Id: dnlcsnoop.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	dnlcsnoop.d     # wait several seconds, then hit Ctrl-C
 *
 * FIELDS:
 *		PID             Process ID
 *		CMD          	Command name
 *		TIME         	Elapsed time for lookup, us
 *		HIT          	DNLC hit Y/N
 *		PATH         	Path for DNLC lookup
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
 * 27-Mar-2004	Brendan Gregg	Created this.
 * 14-Jun-2005	   "      "	Rewrote this a lot.
 * 20-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet

/*
 * Print header
 */
dtrace:::BEGIN
{
	printf("%6s %-12s %5s %3s %s\n",
	    "PID", "CMD", "TIME", "HIT", "PATH");
}

/*
 * DNLC lookup
 */
fbt:genunix:dnlc_lookup:entry
{
	/* store path */
	self->path = stringof(args[0]->v_path);

	/* check for trailing "/" */
	this->len = strlen(self->path);
	self->join = *(char *)(args[0]->v_path + this->len - 1) == '/' ?
	    "" : "/";

	/* store lookup name */
	self->name = stringof(arg1);

	/* store start time */
	self->start = timestamp;
}

/*
 * DNLC return
 */
fbt:genunix:dnlc_lookup:return
/self->start/
{
	/* calculate elapsed time */
	this->elapsed = (timestamp - self->start) / 1000;

	/* print output */
	printf("%6d %-12.12s %5d %3s %s%s%s\n",
	    pid, execname, this->elapsed, arg1 == 0 ? "N" : "Y",
	    self->path, self->join, self->name);

	/* clear variables */
	self->path = 0;
	self->join = 0;
	self->name = 0;
	self->start = 0;
}
