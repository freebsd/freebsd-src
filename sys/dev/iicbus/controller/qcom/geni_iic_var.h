/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Poul-Henning Kamp <phk@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _GENIIIC_GENI_VAR_H_
#define _GENIIIC_GENI_VAR_H_

#include "bus_if.h"
#include "device_if.h"
#include "iicbus_if.h"

struct geniiic_softc {
	device_t	dev;
	device_t	iicbus;
	struct resource	*regs_res;
	int		regs_rid;
	struct resource	*intr_res;
	int		intr_rid;
	void		*intr_handle;
	int		intr_type;
	uint32_t	intr_mask;

	bool		bus_locked;

	bool		platform_attached;

	int		nfail;
	unsigned	worst;

	unsigned	rx_fifo_size;
	bool		rx_complete;
	bool		rx_fifo;
	uint8_t		*rx_buf;
	unsigned	rx_len;
	uint32_t	cmd_status;

	// Protect access to the bus
	struct sx	bus_lock;
	struct sx	real_bus_lock;

	// Coordinate with interrupt routine
	struct mtx	intr_lock;
};

typedef struct geniiic_softc geniiic_softc_t;

int geniiic_attach(geniiic_softc_t *sc);
int geniiic_detach(geniiic_softc_t *sc);
int geniiic_suspend(geniiic_softc_t *sc);
int geniiic_resume(geniiic_softc_t *sc);

extern iicbus_transfer_t geniiic_transfer;
extern iicbus_reset_t   geniiic_reset;
extern iicbus_callback_t geniiic_callback;

#endif /* _GENIIIC_GENI_VAR_H_ */
