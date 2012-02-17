/*
 * Copyright (C) 2000 Jason Evans <jasone@freebsd.org>.
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#include "namespace.h"
#include <unistd.h>
#include <pthread.h>
#include "un-namespace.h"
#include "thr_private.h"

extern unsigned int	__sleep(unsigned int);
extern int		__usleep(useconds_t);

unsigned int	_sleep(unsigned int seconds);

LT10_COMPAT_PRIVATE(_sleep);
LT10_COMPAT_DEFAULT(sleep);
LT10_COMPAT_PRIVATE(_usleep);
LT10_COMPAT_DEFAULT(usleep);

__weak_reference(_sleep, sleep);
__weak_reference(_usleep, usleep);

unsigned int
_sleep(unsigned int seconds)
{
	struct pthread *curthread = _get_curthread();
	unsigned int	ret;

	_thr_cancel_enter(curthread);
	ret = __sleep(seconds);
	_thr_cancel_leave(curthread, 1);
	
	return (ret);
}

int
_usleep(useconds_t useconds)
{
	struct pthread *curthread = _get_curthread();
	unsigned int	ret;

	_thr_cancel_enter(curthread);
	ret = __usleep(useconds);
	_thr_cancel_leave(curthread, 1);
	
	return (ret);
}
