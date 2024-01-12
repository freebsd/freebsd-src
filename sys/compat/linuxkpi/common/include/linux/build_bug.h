/*-
 * Copyright (c) 2017 Mark Johnston <markj@FreeBSD.org>
 * Copyright (c) 2018 Johannes Lundberg <johalun0@gmail.com>
 * Copyright (c) 2021 The FreeBSD Foundation
 * Copyright (c) 2021 Vladimir Kondratyev <wulf@FreeBSD.org>
 * Copyright (c) 2023 Serenity Cyber Security, LLC
 *
 * Portions of this software were developed by Bjoern A. Zeeb
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_LINUXKPI_LINUX_BUILD_BUG_H_
#define	_LINUXKPI_LINUX_BUILD_BUG_H_

#include <sys/param.h>

#include <linux/compiler.h>

/*
 * BUILD_BUG_ON() can happen inside functions where _Static_assert() does not
 * seem to work.  Use old-schoold-ish CTASSERT from before commit
 * a3085588a88fa58eb5b1eaae471999e1995a29cf but also make sure we do not
 * end up with an unused typedef or variable. The compiler should optimise
 * it away entirely.
 */
#define	_O_CTASSERT(x)		_O__CTASSERT(x, __LINE__)
#define	_O__CTASSERT(x, y)	_O___CTASSERT(x, y)
#define	_O___CTASSERT(x, y)	while (0) { \
    typedef char __assert_line_ ## y[(x) ? 1 : -1]; \
    __assert_line_ ## y _x __unused; \
    _x[0] = '\0'; \
}

#define	BUILD_BUG()			do { CTASSERT(0); } while (0)
#define	BUILD_BUG_ON(x)			do { _O_CTASSERT(!(x)) } while (0)
#define	BUILD_BUG_ON_MSG(x, msg)	BUILD_BUG_ON(x)
#define	BUILD_BUG_ON_NOT_POWER_OF_2(x)	BUILD_BUG_ON(!powerof2(x))
#define	BUILD_BUG_ON_INVALID(expr)	while (0) { (void)(expr); }
#define	BUILD_BUG_ON_ZERO(x)	((int)sizeof(struct { int:-((x) != 0); }))

#define static_assert(x, ...)		__static_assert(x, ##__VA_ARGS__, #x)
#define __static_assert(x, msg, ...)	_Static_assert(x, msg)

#endif
