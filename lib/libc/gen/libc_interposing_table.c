/*
 * Copyright (c) 2014 The FreeBSD Foundation.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <stddef.h>
#include "libc_private.h"

#define	SLOT(a, b) \
	[INTERPOS_##a] = (interpos_func_t)b
interpos_func_t __libc_interposing[INTERPOS_MAX] = {
	SLOT(system, __libc_system),
	SLOT(tcdrain, __libc_tcdrain),
	SLOT(_pthread_mutex_init_calloc_cb, _pthread_mutex_init_calloc_cb_stub),
	SLOT(spinlock, __libc_spinlock_stub),
	SLOT(spinunlock, __libc_spinunlock_stub),
	SLOT(map_stacks_exec, __libc_map_stacks_exec),
	SLOT(distribute_static_tls, __libc_distribute_static_tls),
	SLOT(uexterr_gettext, __libc_uexterr_gettext),
};
#undef SLOT

interpos_func_t *
__libc_interposing_slot(int interposno)
{
	if (__libc_interposing[interposno] == NULL)
		return (__libsys_interposing_slot(interposno));
	return (&__libc_interposing[interposno]);
}
