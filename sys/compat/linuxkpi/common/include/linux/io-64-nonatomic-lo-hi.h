/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Felix Palmen
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
#ifndef	_LINUXKPI_LINUX_IO_64_NONATOMIC_LO_HI_H_
#define	_LINUXKPI_LINUX_IO_64_NONATOMIC_LO_HI_H_

#include <linux/io.h>

static inline uint64_t
lo_hi_readq(const volatile void *addr)
{
	const volatile uint32_t *p = addr;
	uint32_t l, h;

	__io_br();
	l = le32toh(__raw_readl(p));
	h = le32toh(__raw_readl(p + 1));
	__io_ar();

	return (l + ((uint64_t)h << 32));
}

static inline void
lo_hi_writeq(uint64_t v, volatile void *addr)
{
	volatile uint32_t *p = addr;

	__io_bw();
	__raw_writel(htole32(v), p);
	__raw_writel(htole32(v >> 32), p + 1);
	__io_aw();
}

#ifndef readq
#define readq(addr)	lo_hi_readq(addr)
#endif

#ifndef writeq
#define writeq(v, addr)	lo_hi_writeq(v, addr)
#endif

#endif	/* _LINUXKPI_LINUX_IO_64_NONATOMIC_LO_HI_H_ */
