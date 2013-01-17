#!/usr/sbin/dtrace -s
/*
 * rwbytype.d - read/write bytes by vnode type.
 *              Written using DTrace (Solaris 10 3/05).
 *
 * This program identifies the vnode type of read/write activity - whether
 * that is for regular files, sockets, character special devices, etc.
 *
 * $Id: rwbytype.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:       rwbytype.d    # hit Ctrl-C to end sample
 *
 * FIELDS:
 *		PID		number of rwbytype
 *		CMD		process name
 *		VTYPE		vnode type (describes I/O type)
 *		DIR		direction (Read/Write)
 *		BYTES		bytes transferred
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
 * 18-Oct-2005  Brendan Gregg   Created this.
 * 20-Apr-2006	   "      "	Last update.
 */

#pragma D option quiet

typedef struct vtype2str {
	string code;
};

translator struct vtype2str < int T > {
	/* the order has been picked for performance reasons */
	code =
	    T == 1 ? "reg"   :
	    T == 9 ? "sock"  :
	    T == 4 ? "chr"   :
	    T == 6 ? "fifo"  :
	    T == 8 ? "proc"  :
	    T == 2 ? "dir"   :
	    T == 3 ? "blk"   :
	    T == 5 ? "lnk"   :
	    T == 7 ? "door"  :
	    T == 10 ? "port" :
	    T == 11 ? "bad"  : "non";
};

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

fbt::fop_read:entry,
fbt::fop_write:entry
{
	self->type = xlate <struct vtype2str *>(args[0]->v_type)->code;
	self->size = args[1]->uio_resid;
	self->uiop = args[1];
}

fbt::fop_read:return
/self->uiop/
{
	this->resid = self->uiop->uio_resid;
	@bytes[pid, execname, self->type, "R"] = sum(self->size - this->resid);
	self->type = 0;
	self->size = 0;
	self->uiop = 0;
}

/* this is delibrately redundant code for performance reasons */
fbt::fop_write:return
/self->uiop/
{
	this->resid = self->uiop->uio_resid;
	@bytes[pid, execname, self->type, "W"] = sum(self->size - this->resid);
	self->type = 0;
	self->size = 0;
	self->uiop = 0;
}

dtrace:::END
{
	printf("%-6s %-16s %6s %4s %9s\n",
	    "PID", "CMD", "VTYPE", "DIR", "BYTES");
	printa("%-6d %-16s %6s %4s %@9d\n", @bytes);
}
