/*-
 * Copyright (c) 2002 New Gold Technology.  All rights reserved.
 * Copyright (c) 2002 Juli Mallett.  All rights reserved.
 *
 * This software was written by Juli Mallett <jmallett@FreeBSD.org> for the
 * FreeBSD project.  Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistribution of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistribution in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/signal.h>
#include <sys/ksiginfo.h>

MALLOC_DEFINE(M_KSIGINFO, "ksiginfos", "Kernel signal info structures");

int
ksiginfo_alloc(struct ksiginfo **ksip, int signo)
{
	int error;
	struct ksiginfo *ksi;

	error = 0;

	ksi = malloc(sizeof *ksi, M_KSIGINFO, M_ZERO | M_WAITOK);
	KASSERT(ksi != NULL, ("ksiginfo_alloc(%d): allocation failed.", signo));
	if (ksi == NULL) {
		error = ENOMEM;
		goto out;
	}
	ksi->ksi_signo = signo;
	if (curproc != NULL) {
		ksi->ksi_pid = curproc->p_pid;
		ksi->ksi_ruid = curproc->p_ucred->cr_uid;
	}
out:
	*ksip = ksi;
	return (error);
}

int
ksiginfo_dequeue(struct ksiginfo **ksip, struct proc *p, int signo)
{
	int error;
	struct ksiginfo *ksi;

	error = 0;
	ksi = NULL;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (TAILQ_EMPTY(&p->p_sigq)) {
		error = EDOOFUS;
		goto out;
	}
	if (!signo) {
		ksi = TAILQ_FIRST(&p->p_sigq);
		goto out;
	}
	TAILQ_FOREACH(ksi, &p->p_sigq, ksi_queue) {
		if (ksi->ksi_signo == signo)
			goto out;
	}
	error = ESRCH;
	ksi = NULL;
out:
	if (ksi != NULL)
		TAILQ_REMOVE(&p->p_sigq, ksi, ksi_queue);
	*ksip = ksi;
	return (error);
}

int
ksiginfo_destroy(struct ksiginfo **ksip)
{
	int error;
	struct ksiginfo *ksi;

	error = 0;

	ksi = *ksip;
	if (ksi == NULL) {
		error = EDOOFUS;
		goto out;
	}
	free(ksi, M_KSIGINFO);
	ksi = NULL;
out:
	*ksip = ksi;
	return (error);
}

int
ksiginfo_to_sigset_t(struct proc *p, sigset_t *setp)
{
	int error;
	sigset_t set;
	struct ksiginfo *ksi;

	error = 0;
	SIGEMPTYSET(set);

	PROC_LOCK_ASSERT(p, MA_OWNED);
	/*
	 * We could set EDOOFUS here, however if there are no queued
	 * signals, then an empty signal set _is_ valid.
	 */
	if (TAILQ_EMPTY(&p->p_sigq))
		goto out;
	TAILQ_FOREACH(ksi, &p->p_sigq, ksi_queue)
		SIGADDSET(set, ksi->ksi_signo);
out:
	*setp = set;
	return (error);
}

int
signal_add(struct proc *p, struct ksiginfo *ksi, int signo)
{
	int error;
	
	error = 0;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (ksi == NULL) {
		PROC_UNLOCK(p);
		error = ksiginfo_alloc(&ksi, signo);
		PROC_LOCK(p);
		if (error)
			goto out;
	}
	TAILQ_INSERT_HEAD(&p->p_sigq, ksi, ksi_queue);
out:
	return (error);
}

int
signal_delete(struct proc *p, struct ksiginfo *ksi, int signo)
{
	int error;

	error = 0;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (ksi == NULL) {
		while (signal_queued(p, signo)) {
			error = ksiginfo_dequeue(&ksi, p, signo);
			if (error)
				goto out;
			error = ksiginfo_destroy(&ksi);
			if (error)
				goto out;
		}
	}
out:
	if (ksi != NULL) {
		TAILQ_REMOVE(&p->p_sigq, ksi, ksi_queue);
		ksiginfo_destroy(&ksi);
	}
	return (error);
}

int
signal_delete_mask(struct proc *p, int mask)
{
	int error;
	struct ksiginfo *ksi, *prev;

	error = 0;
	ksi = prev = NULL;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (TAILQ_EMPTY(&p->p_sigq))
		goto out;
	TAILQ_FOREACH(ksi, &p->p_sigq, ksi_queue) {
		if (prev != NULL) {
			TAILQ_REMOVE(&p->p_sigq, prev, ksi_queue);
			error = ksiginfo_destroy(&prev);
			if (error)
				goto out;
		}
		if (sigmask(ksi->ksi_signo) & mask)
			prev = ksi;
	}
	if (prev != NULL) {
		TAILQ_REMOVE(&p->p_sigq, prev, ksi_queue);
		error = ksiginfo_destroy(&prev);
		/*
		 * XXX - Could just fall off the bottom...
		 */
		if (error)
			goto out;
	}
out:
	return (error);
}

int
signal_pending(struct proc *p)
{
	int error, pending;
	sigset_t set;

	error = 0;
	pending = 0;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (TAILQ_EMPTY(&p->p_sigq))
		goto out;
	if (p->p_flag & P_TRACED) {
		pending = 1;
		goto out;
	}
	error = ksiginfo_to_sigset_t(p, &set);
	if (error)
		goto out;
	pending = !sigsetmasked(&set, &p->p_sigmask);
	if (pending)
		goto out;
out:
	return (pending);
}

int
signal_queued(struct proc *p, int signo)
{
	int error, pending;
	struct ksiginfo *ksi;

	error = 0;
	pending = 0;
	ksi = NULL;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (TAILQ_EMPTY(&p->p_sigq))
		goto out;
	/*
	 * Since we know the queue is not empty, we can just do
	 * pending = TAILQ_FIRST(&p->p_sigq)->ksi_signo, if the
	 * signo is 0, however since we have to use at least one
	 * more TailQ manipulation macro either way, might as well
	 * just do it like this, as I think it optimises better,
	 * even if the failure case is more expensive.
	 */
	TAILQ_FOREACH(ksi, &p->p_sigq, ksi_queue) {
		pending = !signo || ksi->ksi_signo == signo;
		if (pending)
			goto out;
	}
out:
	return (pending);
}

int
signal_queued_mask(struct proc *p, sigset_t mask)
{
	int pending;
	struct ksiginfo *ksi;

	pending = 0;
	ksi = NULL;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (TAILQ_EMPTY(&p->p_sigq))
		goto out;
	TAILQ_FOREACH(ksi, &p->p_sigq, ksi_queue) {
		if (SIGISMEMBER(mask, ksi->ksi_signo)) {
			pending = ksi->ksi_signo;
			goto out;
		}
	}
out:
	return (pending);
}
