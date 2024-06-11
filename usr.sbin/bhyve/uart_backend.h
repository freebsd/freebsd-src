/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 NetApp, Inc.
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

#ifndef _UART_BACKEND_H_
#define	_UART_BACKEND_H_

#include <stdbool.h>

#include "mevent.h"

struct uart_softc;
struct vm_snapshot_meta;

void	uart_rxfifo_drain(struct uart_softc *sc, bool loopback);
int	uart_rxfifo_getchar(struct uart_softc *sc);
int	uart_rxfifo_numchars(struct uart_softc *sc);
int	uart_rxfifo_putchar(struct uart_softc *sc, uint8_t ch, bool loopback);
void	uart_rxfifo_reset(struct uart_softc *sc, int size);
int	uart_rxfifo_size(struct uart_softc *sc);
#ifdef BHYVE_SNAPSHOT
int	uart_rxfifo_snapshot(struct uart_softc *sc,
	    struct vm_snapshot_meta *meta);
#endif

struct uart_softc *uart_init(void);
int	uart_tty_open(struct uart_softc *sc, const char *path,
	    void (*drain)(int, enum ev_type, void *), void *arg);
void	uart_softc_lock(struct uart_softc *sc);
void	uart_softc_unlock(struct uart_softc *sc);
#endif /* _UART_BACKEND_H_ */
