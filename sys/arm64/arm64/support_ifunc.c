/*-
 * Copyright (c) 2022 The FreeBSD Foundation
 *
 * This software was developed by Andrew Turner under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/param.h>

#include <machine/atomic.h>
#include <machine/ifunc.h>
#define _MD_WANT_SWAPWORD
#include <machine/md_var.h>

int casueword32_llsc(volatile uint32_t *, uint32_t, uint32_t *, uint32_t);
int casueword32_lse(volatile uint32_t *, uint32_t, uint32_t *, uint32_t);

int casueword_llsc(volatile u_long *, u_long, u_long *, u_long);
int casueword_lse(volatile u_long *, u_long, u_long *, u_long);

int swapueword8_llsc(volatile uint8_t *, uint8_t *);
int swapueword8_lse(volatile uint8_t *, uint8_t *);

int swapueword32_llsc(volatile uint32_t *, uint32_t *);
int swapueword32_lse(volatile uint32_t *, uint32_t *);

DEFINE_IFUNC(, int, casueword32, (volatile uint32_t *base, uint32_t oldval,
    uint32_t *oldvalp, uint32_t newval))
{
	if (lse_supported)
		return (casueword32_lse);

	return (casueword32_llsc);
}

DEFINE_IFUNC(, int, casueword, (volatile u_long *base, u_long oldval,
    u_long *oldvalp, u_long newval))
{
	if (lse_supported)
		return (casueword_lse);

	return (casueword_llsc);
}

DEFINE_IFUNC(, int, swapueword8, (volatile uint8_t *base, uint8_t *val))
{
	if (lse_supported)
		return (swapueword8_lse);

	return (swapueword8_llsc);
}

DEFINE_IFUNC(, int, swapueword32, (volatile uint32_t *base, uint32_t *val))
{
	if (lse_supported)
		return (swapueword32_lse);

	return (swapueword32_llsc);
}
