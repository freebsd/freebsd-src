#!/usr/sbin/dtrace -s
/*
 * mmapfiles.d - mmap'd files by process.
 *               Written using DTrace (Solaris 10 3/05).
 *
 * $Id: mmapfiles.d 14 2007-09-11 08:03:35Z brendan $
 *
 * USAGE:       mmapfiles.d    # hit Ctrl-C to end sample
 *
 * FIELDS:
 *		MMAPS		number of mmaps
 *		CMD		process name
 *		PATHNAME	pathname of mmap'd file
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
 * 18-Oct-2005	Brendan Gregg	Created this.
 * 20-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

syscall::mmap:entry
/(int)arg4 > 0/
{
	/*
	 * Fetch filename
	 */
	this->filep = curthread->t_procp->p_user.u_finfo.fi_list[arg4].uf_file;
	this->vnodep = this->filep != 0 ? this->filep->f_vnode : 0;
	self->vpath = this->vnodep ? (this->vnodep->v_path != 0 ?
	    cleanpath(this->vnodep->v_path) : "<unknown>") : "<unknown>";

	/* Store Details */
	@hits[execname, self->vpath] = count();
}

dtrace:::END
{
	/* Print Details */
	printf("%5s %-16s %s\n", "MMAPS", "CMD", "PATHNAME");
	printa("%@5d %-16s %s\n", @hits);
}
