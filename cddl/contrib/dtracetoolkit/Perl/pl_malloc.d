#!/usr/sbin/dtrace -Zs
/*
 * pl_malloc.d - Perl libc malloc analysis.
 *               Written for the Perl DTrace provider.
 *
 * $Id: pl_malloc.d 19 2007-09-12 07:47:59Z brendan $
 *
 * This is an expiremental script to identify who is calling malloc() for
 * memory allocation, and to print distribution plots of the requested bytes.
 * If a malloc() occured while in a Perl subroutine, then that subroutine is
 * identified as responsible; else the caller of malloc() is identified as
 * responsible - which will be a function from the Perl engine.
 *
 * USAGE: pl_malloc.d { -p PID | -c cmd }	# hit Ctrl-C to end
 *
 * Filename and subroutine names are printed if available.
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

dtrace:::BEGIN
{
	printf("Tracing... Hit Ctrl-C to end.\n");
}

perl$target:::sub-entry
{
	self->file = basename(copyinstr(arg1));
	self->name = copyinstr(arg0);
}

perl$target:::sub-return
{
	self->file = 0;
	self->name = 0;
}

pid$target:libc:malloc:entry
/self->file != NULL/
{
	@malloc_sub_size[self->file, self->name] = sum(arg0);
	@malloc_sub_dist[self->file, self->name] = quantize(arg0);
}

pid$target:libc:malloc:entry
/self->name == NULL/
{
	@malloc_lib_size[usym(ucaller)] = sum(arg0);
	@malloc_lib_dist[usym(ucaller)] = quantize(arg0);
}


dtrace:::END
{
	printf("\nPerl malloc byte distributions by engine caller,\n\n");
	printa("   %A, total bytes = %@d %@d\n", @malloc_lib_size,
	    @malloc_lib_dist);

	printf("\nPerl malloc byte distributions by Perl file and ");
	printf("subroutine,\n\n");
	printa("   %s, %s, bytes total = %@d %@d\n", @malloc_sub_size,
	    @malloc_sub_dist);
}
