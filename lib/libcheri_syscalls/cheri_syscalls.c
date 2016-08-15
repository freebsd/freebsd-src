/*-
 * Copyright (c) 2016 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#include <sys/cdefs.h>
#include <sys/types.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>
#include <cheri/cheri_system.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>

#define SYS_STUB(_num, _ret, _sys,					\
    _protoargs, _protoargs_chk, _protoargs_err,				\
    _callargs, _callargs_chk, _callargs_err, _localcheck)		\
_ret _sys _protoargs;							\
__weak_reference(_sys, _##_sys);					\
_ret									\
_sys _protoargs								\
{									\
									\
	_localcheck							\
	return (__cheri_system_sys_##_sys _callargs_err);		\
}

#define SYS_STUB_ARGHASPTRS(...)	SYS_STUB(__VA_ARGS__)

/*
 * We need to perform what ever custom handling is required for the va_list
 * on the calling side since it might be the wrong type outside the sandbox.
 */
#define SYS_STUB_VA(_num, _ret, _sys, _lastarg,				\
    _protoargs, _vprotoargs, _protoargs_chk, _protoargs_err,		\
    _callargs, _callargs_chk, _callargs_err, _localcheck)		\
_ret _sys _vprotoargs;							\
__weak_reference(_sys, _##_sys);

#include <compat/cheriabi/cheriabi_sysstubs.h>

#undef SYS_STUB
#undef SYS_STUB_VA

int
open(const char * path, int flags, ...)
{
	va_list ap;
	int mode;

	va_start(ap, flags);
	if ((flags & O_CREAT) != 0)
		mode = va_arg(ap, int);
	else
		mode = 0;
	va_end(ap);

	return (__cheri_system_sys_open(&errno, path, flags, mode));
}

int
openat(int fd, const char * path, int flags, ...)
{
	va_list ap;
	int mode;

	va_start(ap, flags);
	if ((flags & O_CREAT) != 0)
		mode = va_arg(ap, int);
	else
		mode = 0;
	va_end(ap);

	return (__cheri_system_sys_openat(&errno, fd, path, flags, mode));
}

int
fcntl(int fd, int cmd, ...)
{
	va_list ap;
	intptr_t arg;

	va_start(ap, cmd);
	switch (cmd) {
	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
		arg = va_arg(ap, intptr_t);
		break;

	case F_GETFD:
	case F_GETFL:
	case F_GETOWN:
		arg = 0;
		break;

	default:
		arg = va_arg(ap, long);
		break;
	}
	va_end(ap);

	return (__cheri_system_sys_fcntl(&errno, fd, cmd, arg));
}

int	ioctl(int fd __unused, unsigned long request __unused, ...);
int
ioctl(int fd __unused, unsigned long request __unused, ...)
{

	return (ENOSYS);
}

__weak_reference(ioctl, _ioctl);
__weak_reference(sigaction, __libc_sigaction);
__weak_reference(sigprocmask, __libc_sigprocmask);
__weak_reference(sigprocmask, __sys_sigprocmask);
__weak_reference(sigwait, __libc_sigwait);
__weak_reference(thr_kill, __sys_thr_kill);
__weak_reference(thr_self, __sys_thr_self);
