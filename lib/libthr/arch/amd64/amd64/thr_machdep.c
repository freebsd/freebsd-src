/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
 *
 */

#define _WANT_P_OSREL
#include <sys/param.h>
#include <errno.h>
#include <machine/sysarch.h>

#include "libc_private.h"
#include "thr_private.h"

void
__thr_setup_tsd(struct pthread *thread)
{
	void *base;
	int error;

	if (__getosreldate() < P_OSREL_TLSBASE) {
		amd64_set_tlsbase(thread->tcb);
		return;
	}

	/*
	 * Make tlsbase handling more compatible with code, like Go
	 * runtime, which wants to manage fsbase itself, and which do
	 * not need assistance in setting fsbase for signal handlers.
	 *
	 * If the main thread did not used amd64_set_tlsbase(), which
	 * means that rtld/libc was not utilized, do not use
	 * amd64_set_tlsbase() either.  Also do not mark new threads
	 * as using C runtime with the THR_C_RUNTIME flag.
	 */
	error = sysarch(AMD64_GET_TLSBASE, &base);
	if (error != 0 && errno == ESRCH) {
		__thr_new_flags &= ~THR_C_RUNTIME;
		amd64_set_fsbase(thread->tcb);
	} else {
		amd64_set_tlsbase(thread->tcb);
	}
}
