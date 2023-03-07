/*-
 * Copyright (c) 2017 Mark Johnston <markj@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
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
 * $FreeBSD$
 */

#ifndef _LINUXKPI_LINUX_SMP_H_
#define	_LINUXKPI_LINUX_SMP_H_

#include <asm/smp.h>

/*
 * Important note about the use of the function provided below:
 *
 * The callback function passed to on_each_cpu() is called from a
 * so-called critical section, and if you need a mutex you will have
 * to rewrite the code to use native FreeBSD mtx spinlocks instead of
 * the spinlocks provided by the LinuxKPI! Be very careful to not call
 * any LinuxKPI functions inside the on_each_cpu()'s callback
 * function, because they may sleep, unlike in native Linux.
 *
 * Enabling witness(4) when testing, can catch such issues.
 */
#define	on_each_cpu(cb, data, wait) ({				\
	CTASSERT(wait);						\
	linux_on_each_cpu(cb, data);				\
})

extern int	linux_on_each_cpu(void (*)(void *), void *);

#endif /* _LINUXKPI_LINUX_SMP_H_ */
