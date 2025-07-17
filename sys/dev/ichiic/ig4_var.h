/*
 * Copyright (c) 2014 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com> and was subsequently ported
 * to FreeBSD by Michael Gmelin <freebsd@grem.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _ICHIIC_IG4_VAR_H_
#define _ICHIIC_IG4_VAR_H_

#include "bus_if.h"
#include "device_if.h"
#include "pci_if.h"
#include "iicbus_if.h"

enum ig4_vers {
	IG4_EMAG,
	IG4_HASWELL,
	IG4_ATOM,
	IG4_SKYLAKE,
	IG4_APL,
	IG4_CANNONLAKE,
	IG4_TIGERLAKE,
	IG4_GEMINILAKE
};

/* Controller has additional registers */
#define	IG4_HAS_ADDREGS(vers)	((vers) >= IG4_SKYLAKE)

struct ig4_hw {
	uint32_t	ic_clock_rate;	/* MHz */
	uint32_t	sda_fall_time;	/* nsec */
	uint32_t	scl_fall_time;	/* nsec */
	uint32_t	sda_hold_time;	/* nsec */
	int		txfifo_depth;
	int		rxfifo_depth;
};

struct ig4_cfg {
	uint32_t	version;
	uint32_t	bus_speed;
	uint16_t	ss_scl_hcnt;
	uint16_t	ss_scl_lcnt;
	uint16_t	ss_sda_hold;
	uint16_t	fs_scl_hcnt;
	uint16_t	fs_scl_lcnt;
	uint16_t	fs_sda_hold;
	int		txfifo_depth;
	int		rxfifo_depth;
};

struct ig4iic_softc {
	device_t	dev;
	device_t	iicbus;
	struct resource	*regs_res;
	int		regs_rid;
	struct resource	*intr_res;
	int		intr_rid;
	void		*intr_handle;
	int		intr_type;
	enum ig4_vers	version;
	struct ig4_cfg	cfg;
	uint32_t	intr_mask;
	uint8_t		last_slave;
	bool		platform_attached : 1;
	bool		use_10bit : 1;
	bool		slave_valid : 1;

	/*
	 * Locking semantics:
	 *
	 * Functions implementing the icbus interface that interact
	 * with the controller acquire an exclusive lock on call_lock
	 * to prevent interleaving of calls to the interface.
	 *
	 * io_lock is used as condition variable to synchronize active process
	 * with the interrupt handler. It should not be used for tasks other
	 * than waiting for interrupt and passing parameters to and from
	 * it's handler.
	 */
	struct sx	call_lock;
	struct mtx	io_lock;
};

typedef struct ig4iic_softc ig4iic_softc_t;

/* Attach/Detach called from ig4iic_pci_*() */
int ig4iic_attach(ig4iic_softc_t *sc);
int ig4iic_detach(ig4iic_softc_t *sc);
int ig4iic_suspend(ig4iic_softc_t *sc);
int ig4iic_resume(ig4iic_softc_t *sc);

/* iicbus methods */
extern iicbus_transfer_t ig4iic_transfer;
extern iicbus_reset_t   ig4iic_reset;
extern iicbus_callback_t ig4iic_callback;

#endif /* _ICHIIC_IG4_VAR_H_ */
