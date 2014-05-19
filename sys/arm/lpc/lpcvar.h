/*-
 * Copyright (c) 2011 Jakub Wojciech Klama <jceel@FreeBSD.org>
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

#ifndef	_ARM_LPC_LPCVAR_H
#define	_ARM_LPC_LPCVAR_H

#include <sys/types.h>
#include <sys/bus.h>
#include <machine/bus.h>

/* Clocking and power control */
uint32_t lpc_pwr_read(device_t, int);
void lpc_pwr_write(device_t, int, uint32_t);

/* GPIO */
void lpc_gpio_init(void);
int lpc_gpio_set_flags(device_t, int, int);
int lpc_gpio_set_state(device_t, int, int);
int lpc_gpio_get_state(device_t, int, int *);

/* DMA */
struct lpc_dmac_channel_config
{
	int		ldc_fcntl;
	int		ldc_src_periph;
	int		ldc_src_width;
	int		ldc_src_incr;
	int		ldc_src_burst;
	int		ldc_dst_periph;
	int		ldc_dst_width;
	int		ldc_dst_incr;
	int		ldc_dst_burst;
	void		(*ldc_success_handler)(void *);
	void		(*ldc_error_handler)(void *);
	void *		ldc_handler_arg;
};

int lpc_dmac_config_channel(device_t, int, struct lpc_dmac_channel_config *);
int lpc_dmac_setup_transfer(device_t, int, bus_addr_t, bus_addr_t, bus_size_t, int);
int lpc_dmac_enable_channel(device_t, int);
int lpc_dmac_disable_channel(device_t, int);
int lpc_dmac_start_burst(device_t, int);

#endif	/* _ARM_LPC_LPCVAR_H */
