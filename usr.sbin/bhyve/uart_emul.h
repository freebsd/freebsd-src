/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _UART_EMUL_H_
#define	_UART_EMUL_H_

#define	UART_NS16550_IO_BAR_SIZE	8

struct uart_ns16550_softc;
struct uart_pl011_softc;
struct vm_snapshot_meta;

typedef void (*uart_intr_func_t)(void *arg);

int	uart_legacy_alloc(int unit, int *ioaddr, int *irq);

struct uart_ns16550_softc *uart_ns16550_init(uart_intr_func_t intr_assert,
	    uart_intr_func_t intr_deassert, void *arg);
uint8_t	uart_ns16550_read(struct uart_ns16550_softc *sc, int offset);
void	uart_ns16550_write(struct uart_ns16550_softc *sc, int offset,
	    uint8_t value);
int	uart_ns16550_tty_open(struct uart_ns16550_softc *sc,
	    const char *device);
#ifdef BHYVE_SNAPSHOT
int	uart_ns16550_snapshot(struct uart_ns16550_softc *sc,
	    struct vm_snapshot_meta *meta);
#endif

uint32_t uart_pl011_read(struct uart_pl011_softc *sc, int offset);
void	uart_pl011_write(struct uart_pl011_softc *sc, int offset,
	    uint32_t value);
struct uart_pl011_softc *uart_pl011_init(uart_intr_func_t intr_assert,
	    uart_intr_func_t intr_deassert, void *arg);
int	uart_pl011_tty_open(struct uart_pl011_softc *sc, const char *device);

#endif /* _UART_EMUL_H_ */
