#!/usr/sbin/dtrace -Zs
/*
 * rb_malloc.d - Ruby operations and libc malloc statistics.
 *               Written for the Ruby DTrace provider.
 *
 * $Id: rb_malloc.d 20 2007-09-12 09:28:22Z brendan $
 *
 * WARNING: This script is not 100% accurate; This prints libc malloc() byte
 * distributions by "recent" Ruby operation, which we hope will be usually
 * relevant. This is an experimental script that may be improved over time.
 *
 * USAGE: rb_malloc.d { -p PID | -c cmd }	# hit Ctrl-C to end
 *
 * FIELDS:
 *		1		Filename of the Ruby program
 *		2		Type of operation (method/objnew/startup)
 *		3		Name of operation
 *
 * Filename and method names are printed if available.
 *
 * COPYRIGHT: Copyright (c) 2007 Brendan Gregg.
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
 * 09-Sep-2007	Brendan Gregg	Created this.
 */

#pragma D option quiet

self string filename;

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

ruby$target:::function-entry
{
	self->file = basename(copyinstr(arg2));
	self->type = "method";
	self->name = strjoin(strjoin(copyinstr(arg0), "::"), copyinstr(arg1));
}

ruby$target:::object-create-start
{
	self->file = basename(copyinstr(arg1));
	self->type = "objnew";
	self->name = copyinstr(arg0);
}

pid$target:libc:malloc:entry
/self->file != NULL/
{
	@mallocs[self->file, self->type, self->name] = quantize(arg0);
}

pid$target:libc:malloc:entry
/self->file == NULL/
{
	@mallocs["ruby", "startup", "-"] = quantize(arg0);
}


dtrace:::END
{
	printf("Ruby malloc byte distributions by recent Ruby operation,\n");
	printa("   %s, %s, %s %@d\n", @mallocs);
}
