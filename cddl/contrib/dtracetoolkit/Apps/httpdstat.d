#!/usr/sbin/dtrace -s
/*
 * httpdstat.d - realtime httpd statistics. Uses DTrace.
 *
 * $Id: httpdstat.d 2 2007-08-01 10:01:43Z brendan $
 *
 * USAGE:	httpdstat.d [interval [count]]
 *
 *		interval	seconds
 *		count		number of samples
 *
 * FIELDS:
 *		TIME		Time, string
 *		NUM		Number of connections
 *		GET		Number of "GET"s
 *		POST		Number of "POST"s
 *		HEAD		Number of "HEAD"s
 *		TRACE		Number of "TRACE"s
 *
 * All of the statistics are printed as a value per interval (not per second).
 *
 * NOTE: This version does not process subsequent operations on keepalives.
 *
 * IDEA: Ryan Matteson (who first wrote a solution to this).
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
 * 20-Nov-2005	Brendan Gregg	Created this.
 */

#pragma D option quiet
#pragma D option defaultargs

inline int SCREEN = 21;

/*
 * Program Start
 */
dtrace:::BEGIN
{
	num = 0; get = 0; head = 0; post = 0; trac = 0;
	lines = SCREEN + 1;
	secs = $1 ? $1 : 1;
	counts = $2 ? $2 : -1;
	first = 1;
}

profile:::tick-1sec
{
	secs--;
}

/*
 * Print Header
 */
dtrace:::BEGIN,
profile:::tick-1sec
/first || (secs == 0 && lines > SCREEN)/
{
	printf("%-20s %6s %6s %5s %5s %5s\n", "TIME",
	    "NUM", "GET", "POST", "HEAD", "TRACE");
	lines = 0;
	first = 0;
}

/*
 * Track Accept Events
 */
syscall::accept:return
/execname == "httpd"/
{
	self->buf = 1;
}

syscall::read:entry
/self->buf/
{
	self->buf = arg1;
}

/*
 * Tally Data
 */
syscall::read:return
/self->buf && arg0/
{
	this->str = (char *)copyin(self->buf, arg0);
	this->str[4] = '\0';
	get  += stringof(this->str) == "GET " ? 1 : 0;
	post += stringof(this->str) == "POST" ? 1 : 0;
	head += stringof(this->str) == "HEAD" ? 1 : 0;
	trac += stringof(this->str) == "TRAC" ? 1 : 0;
	num++;
	self->buf = 0;
}

/*
 * Print Output
 */
profile:::tick-1sec
/secs == 0/
{
	printf("%-20Y %6d %6d %5d %5d %5d\n", walltimestamp,
	    num, get, post, head, trac);
	num = 0; get = 0; head = 0; post = 0; trac = 0;
	secs = $1 ? $1 : 1;
	lines++;
	counts--;
}

/*
 * End
 */
profile:::tick-1sec
/counts == 0/
{
	exit(0);
}
