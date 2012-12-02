#!/usr/sbin/dtrace -s
/*
 * whatexec.d - Examine the type of files exec'd.
 *              Written using DTrace (Solaris 10 3/05)
 *
 * This prints the first four chacacters of files that are executed.
 * This traces the kernel function findexec_by_hdr(), which checks for
 * a known magic number in the file's header.
 *
 * The idea came from a demo I heard about from the UK, where a
 * "blue screen of death" was displayed for "MZ" files (although I
 * haven't seen the script or the demo).
 *
 * $Id: whatexec.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	whatexec.d	(early release, check for updates)
 *
 * FIELDS:
 *		PEXEC		parent command name
 *		EXEC		pathname to file exec'd
 *		OK		is type runnable, Y/N
 *		TYPE		first four characters from file
 *
 * COPYRIGHT: Copyright (c) 2006 Brendan Gregg.
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
 * 11-Feb-2006  Brendan Gregg   Created this.
 * 25-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet

this char *buf;

dtrace:::BEGIN
{
	printf("%-16s %-38s %2s %s\n", "PEXEC", "EXEC", "OK", "TYPE");
}

fbt::gexec:entry
{
	self->file = cleanpath((*(struct vnode **)arg0)->v_path);
	self->ok = 1;
}

fbt::findexec_by_hdr:entry
/self->ok/
{
	bcopy(args[0], this->buf = alloca(5), 4);
	this->buf[4] = '\0';
	self->hdr = stringof(this->buf);
}

fbt::findexec_by_hdr:return
/self->ok/
{
	printf("%-16s %-38s %2s %S\n", execname, self->file,
	    arg1 == NULL ? "N" : "Y", self->hdr);
	self->hdr = 0;
}

fbt::gexec:return
{
	self->file = 0;
	self->ok = 0;
}
