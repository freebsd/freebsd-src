#!/usr/sbin/dtrace -s
/*
 * pathopens.d - full pathnames opened successfully count.
 *               Written using DTrace (Solaris 10 3/05)
 *
 * This program prints a count of the number of times files have been
 * successfully opened. This is somewhat special in that the full pathname
 * is calculated, even if the file open referred to a relative pathname.
 *
 * $Id: pathopens.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	fileopens.d
 *
 * FIELDS:
 *		PATHNAME	full pathname
 *		COUNT		number of successful opens
 *
 * Similar to a script from DExplorer.
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
 * 28-Jun-2005	Brendan Gregg	Created this.
 * 12-Jan-2006	   "	  "	Fixed known error.
 * 20-Apr-2006	   "	  "	Last update.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

syscall::open*:entry
{
	self->pathp = arg0;
	self->ok = 1;
}

syscall::open*:return
/self->ok && arg0 != -1/
{
	self->file = copyinstr(self->pathp);
	self->char0 = copyin(self->pathp, 1);

	/* fetch current working directory */
	this->path = curthread->t_procp->p_user.u_cdir->v_path;

	/*
	 * Make the full pathname
	 *
	 * This routine takes the cwd and the filename, and generates a
	 * full pathname. Sometimes the filename is absolute, so we must
	 * ignore the cwd. This also checks if the cwd ends in an
	 * unnecessary '/'.
	 */
	this->len = strlen(this->path);
	self->join = *(char *)(this->path + this->len - 1) == '/' ?  "" : "/";
	self->dir = strjoin(cwd, self->join);
	self->dir = *(char *)self->char0 == '/' ? "" : self->dir;
	self->full = strjoin(self->dir, self->file);

	/* save to aggregation */
	@num[self->full] = count();

	/* cleanup */
	self->join  = 0;
	self->full  = 0;
	self->dir   = 0;
	self->file  = 0;
	self->char0 = 0;
}

syscall::open*:return
/self->ok/
{
	/* cleanup */
	self->ok    = 0;
	self->pathp = 0;
}

dtrace:::END
{
	printf("%6s %s\n", "COUNT", "PATHNAME");
	printa("%@6d %s\n", @num);
}
