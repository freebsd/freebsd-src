#!/usr/sbin/dtrace -s
/*
 * rfsio.d - read FS I/O stats, with cache miss rate.
 *           Written using DTrace (Solaris 10 3/05)
 *
 * This script provides statistics on the number of reads and the bytes
 * read from filesystems (logical), and the number of bytes read from
 * disk (physical). A summary is printed every five seconds by filesystem.
 *
 * A total miss-rate is also provided for the file system cache.
 *
 * $Id: rfsio.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	rfsio.d
 *
 * IDEA: Richard McDougall, Solaris Internals 2nd Ed, FS Chapter.
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
 * 19-Mar-2006  Brendan Gregg   Created this.
 * 23-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet

self int trace;
uint64_t lbytes;
uint64_t pbytes;

dtrace:::BEGIN
{
	trace("Tracing...\n");
}

fbt::fop_read:entry
/self->trace == 0/
{
	self->fs_mount = args[0]->v_vfsp == `rootvfs ? "/" :
	    args[0]->v_vfsp->vfs_vnodecovered ?
	    stringof(args[0]->v_vfsp->vfs_vnodecovered->v_path) : NULL;
}

fbt::fop_read:entry
/self->fs_mount != NULL/
{
	@rio[self->fs_mount, "logical"] = count();
	lbytes += args[1]->uio_resid;
	self->size = args[1]->uio_resid;
	self->uiop = args[1];
}

fbt::fop_read:return
/self->size/
{
	@rbytes[self->fs_mount, "logical"] =
	    sum(self->size - self->uiop->uio_resid);
	self->size = 0;
	self->uiop = 0;
	self->fs_mount = 0;
}

io::bdev_strategy:start
/self->size && args[0]->b_flags & B_READ/
{
	@rio[self->fs_mount, "physical"] = count();
	@rbytes[self->fs_mount, "physical"] = sum(args[0]->b_bcount);
	pbytes += args[0]->b_bcount;
}

profile:::tick-5s
{
	trunc(@rio, 20);
	trunc(@rbytes, 20);
	printf("\033[H\033[2J");
	printf("\nRead IOPS (count)\n");
	printa("%-32s %10s %10@d\n", @rio);
	printf("\nRead Bandwidth (bytes)\n");
	printa("%-32s %10s %10@d\n", @rbytes);
	printf("\nTotal File System miss-rate: %d%%\n",
	    lbytes ? 100 * pbytes / lbytes : 0);
	trunc(@rbytes);
	trunc(@rio);
	lbytes = pbytes = 0;
}
