#!/usr/sbin/dtrace -s
/*
 * stacksize.d - measure stack size for running threads.
 *               Written using DTrace (Solaris 10 3/05).
 *
 * $Id: stacksize.d 3 2007-08-01 10:50:08Z brendan $
 *
 * USAGE:       stacksize.d              # hit Ctrl-C to end sample
 *
 * FIELDS:
 *		value		size of the user stack
 *		count		number of samples at this size
 *
 * SEE ALSO:    pmap(1)
 *
 * COPYRIGHT: Copyright (c) 2006 Jonathan Adams
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
 * 16-Feb-2006	Jonathan Adams	Created this.
 * 16-Feb-2006     "      "	Last update.
 */

#pragma D option quiet

this uintptr_t stkinfoptr;
this uintptr_t stkptr;

dtrace:::BEGIN
{
	trace("Sampling... Hit Ctrl-C to end\n");
}

sched:::on-cpu, profile:::profile-997
{
	this->stkinfoptr = 0;
	this->stkptr = 0;
}

sched:::on-cpu, profile:::profile-997
/execname != "sched"/
{
	this->stkinfoptr = curthread->t_lwp->lwp_ustack;
	this->stkptr = (uintptr_t)0;
}

sched:::on-cpu, profile:::profile-997
/this->stkinfoptr != 0 && curpsinfo->pr_dmodel == PR_MODEL_ILP32/
{
	this->stkinfo32 = (stack32_t *)copyin(this->stkinfoptr,
	    sizeof (stack32_t));
	this->stktop = (uintptr_t)this->stkinfo32->ss_sp +
	    this->stkinfo32->ss_size;
	this->stkptr = (uintptr_t)uregs[R_SP];
}

sched:::on-cpu, profile:::profile-997
/this->stkinfoptr != 0 && curpsinfo->pr_dmodel == PR_MODEL_LP64/
{
	this->stkinfo = (stack_t *)copyin(this->stkinfoptr,
	    sizeof (stack_t));
	this->stktop = (uintptr_t)this->stkinfo->ss_sp +
	    this->stkinfo->ss_size;
	this->stkptr = (uintptr_t)uregs[R_SP];
}

sched:::on-cpu, profile:::profile-997
/this->stkptr != 0/
{
	@sizes[execname] = quantize(this->stktop - this->stkptr);
}

dtrace:::ERROR
{
	@errors[execname] = count();
}

dtrace:::END
{
	printa(@sizes);
	printf("\nErrors:\n");
	printa("    %@d %s\n", @errors);
}
