/*
 * Copyright (C) 2001 Julian Elischer <julian@freebsd.org>
 * for the FreeBSD Foundation.
 *
 *  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible 
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef SYS_KSE_H
#define SYS_KSE_H
#include <machine/kse.h>
/* 
 * This file defines the structures needed for communication between
 * the userland and the kernel when running a KSE-based threading system.
 * The only programs that should see this file are the UTS and the kernel.
 */

/* 
 * Each userland thread has one of these buried in it's 
 * Thread control structure somewhere.
 */
struct thread_mailbox
{
	struct thread_mailbox *next_completed;
	unsigned int	flags;
	void		*UTS_handle;	/* The UTS can use this for anything */
	union kse_td_ctx ctx;		/* thread's saved context goes here. */
};


/* 
 * You need to supply one of these as the argument to the 
 * kse_new() system call.
 */
struct kse_mailbox 
{
	struct thread_mailbox *current_thread;
	struct thread_mailbox *completed_threads;
	unsigned int	flags;
	void		*UTS_handle;	/* The UTS can use this for anything */
};
#define KEMBXF_CRITICAL 0x00000001

struct kse_global_mailbox
{
	unsigned int	flags;
};
#define GMBXF_CRITICAL 0x00000001

/* some provisional sycalls: */

#endif
