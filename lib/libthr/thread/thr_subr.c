/*-
 * Copyright (c) 2003 Michael Telahun Makonnen
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	Problems/Questions to: Mike Makonnen <mtm@FreeBSD.Org>
 *
 * $FreeBSD$
 */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <pthread.h>

#include "thr_private.h"

/*
 * Lock for the process global signal actions list.
 * This lock does NOT insure up-to-date-ness, only integrity.
 */
struct umtx sigactList_lock = UMTX_INITIALIZER;

/*
 * proc_sigact_copyin(sig, actp)
 *	Copy the contents of actp into the process global
 *	action for signal sig.
 */
void
proc_sigact_copyin(int sig, const struct sigaction *actp)
{
	UMTX_LOCK(&sigactList_lock);
	bcopy((const void *)actp, (void *)&_thread_sigact[sig - 1],
	    sizeof(struct sigaction));
	UMTX_UNLOCK(&sigactList_lock);
}

/*
 * proc_sigact_copyout(sig, sigact)
 *	Copy the contents of the process global action for
 *	signal sig into sigact.
 */
void
proc_sigact_copyout(int sig, struct sigaction *actp)
{
	UMTX_LOCK(&sigactList_lock);
	bcopy((const void *)&_thread_sigact[sig - 1], (void *)actp,
	    sizeof(struct sigaction));
	UMTX_UNLOCK(&sigactList_lock);
}

/*
 * proc_sigact_sigaction(sig)
 *	Obtains the struct sigaction associated with signal sig.
 *	The address of the structure is the return value. It is
 *	upto the caller to check the value of the structure at
 *	that address against SIG_IGN and SIG_DFL before trying
 *	to dereference it.
 */
struct sigaction *
proc_sigact_sigaction(int sig)
{
	struct sigaction *actp;

	UMTX_LOCK(&sigactList_lock);
	actp = &_thread_sigact[sig - 1];
	UMTX_UNLOCK(&sigactList_lock);
	return (actp);
}
