#!/usr/sbin/dtrace -Cs
/*
 * kstat_types.d - Trace kstat reads with type info.
 *                 Written using DTrace (Solaris 10 3/05)
 *
 * kstat is the Kernel Statistics framework, which is used by tools
 * such as vmstat, iostat, mpstat and sar. Try running vmstat while
 * kstat_types.d is tracing - you should see details of the kstat
 * reads performed.
 *
 * $Id: kstat_types.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:	kstat_types.d	(early release, check for updates)
 *
 * FIELDS:
 *		CMD		command name
 *		CLASS		kstat class (ks_class)
 *		TYPE		kstat type as a string (ks_type)
 *		MOD:INS:NAME	kstat module:instance:name
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
 * 11-Feb-2006	Brendan Gregg	Created this.
 * 11-Feb-2006	   "      "	Last update.
 */

#include <sys/isa_defs.h>

#pragma D option quiet

dtrace:::BEGIN
{
	printf("%-16s %-16s %-6s %s\n",
	    "CMD", "CLASS", "TYPE", "MOD:INS:NAME");
}

fbt::read_kstat_data:entry
{
#ifdef _MULTI_DATAMODEL
	self->uk = (kstat32_t *)copyin((uintptr_t)arg1, sizeof (kstat32_t));
#else
	self->uk = (kstat_t *)copyin((uintptr_t)arg1, sizeof (kstat_t));
#endif
	printf("%-16s %-16s %-6s %s:%d:%s\n", execname,
	    self->uk->ks_class == "" ? "." : self->uk->ks_class,
	    self->uk->ks_type == 0 ? "raw"
	    :  self->uk->ks_type == 1 ? "named"
	    :  self->uk->ks_type == 2 ? "intr"
	    :  self->uk->ks_type == 3 ? "io"
	    :  self->uk->ks_type == 4 ? "timer" : "?",
	    self->uk->ks_module, self->uk->ks_instance, self->uk->ks_name);
}
