/*
 * Copyright (c) 2014 The FreeBSD Foundation.
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include "libc_private.h"

#define	SLOT(a, b) \
	[INTERPOS_##a] = (interpos_func_t)b,
#ifndef NO_SYSCALLS
#define SLOT_SYS(s) \
	[INTERPOS_##s] = (interpos_func_t)__sys_##s,
#else
#define SLOT_SYS(s)
#endif
#define SLOT_LIBC(s) \
	[INTERPOS_##s] = (interpos_func_t)__libc_##s,
interpos_func_t __libc_interposing[INTERPOS_MAX] = {
	SLOT_SYS(accept)
	SLOT_SYS(accept4)
	SLOT_SYS(aio_suspend)
	SLOT_SYS(close)
	SLOT_SYS(connect)
	SLOT_SYS(fcntl)
	SLOT_SYS(fsync)
	SLOT_SYS(fork)
	SLOT_SYS(msync)
	SLOT_SYS(nanosleep)
	SLOT_SYS(openat)
	SLOT_SYS(poll)
	SLOT_SYS(pselect)
	SLOT_SYS(read)
	SLOT_SYS(readv)
	SLOT_SYS(recvfrom)
	SLOT_SYS(recvmsg)
	SLOT_SYS(select)
	SLOT_SYS(sendmsg)
	SLOT_SYS(sendto)
	SLOT_SYS(setcontext)
	SLOT_SYS(sigaction)
	SLOT_SYS(sigprocmask)
	SLOT_SYS(sigsuspend)
	SLOT_LIBC(sigwait)
	SLOT_SYS(sigtimedwait)
	SLOT_SYS(sigwaitinfo)
	SLOT_SYS(swapcontext)
	SLOT_LIBC(system)
	SLOT_LIBC(tcdrain)
	SLOT_SYS(wait4)
	SLOT_SYS(write)
	SLOT_SYS(writev)
	SLOT(_pthread_mutex_init_calloc_cb, _pthread_mutex_init_calloc_cb_stub)
	SLOT(spinlock, __libc_spinlock_stub)
	SLOT(spinunlock, __libc_spinunlock_stub)
	SLOT_SYS(kevent)
	SLOT_SYS(wait6)
	SLOT_SYS(ppoll)
	SLOT_LIBC(map_stacks_exec)
	SLOT_SYS(fdatasync)
};
#undef SLOT
#undef SLOT_SYS
#undef SLOT_LIBC

interpos_func_t *
__libc_interposing_slot(int interposno)
{

	return (&__libc_interposing[interposno]);
}
