/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Apple BCE mailbox -- 64-bit request/reply over BAR4 registers.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/sema.h>
#include <machine/bus.h>
#include <machine/atomic.h>

#include "apple_bce.h"
#include "apple_bce_mailbox.h"

int
bce_mailbox_init(struct bce_mailbox *mb, struct resource *reg)
{
	mb->reg = reg;
	mb->status = 0;
	sema_init(&mb->mb_cmpl, 0, "bce_mb");
	mb->mb_result = 0;
	mb->initialized = 1;
	return (0);
}

void
bce_mailbox_destroy(struct bce_mailbox *mb)
{
	if (mb->initialized) {
		sema_destroy(&mb->mb_cmpl);
		mb->initialized = 0;
	}
}

/*
 * Send a mailbox message and wait for reply.
 * Only one message in flight at a time (enforced by cmpxchg).
 */
int
bce_mailbox_send(struct bce_mailbox *mb, uint64_t msg, uint64_t *recv,
    unsigned int timeout_ms)
{
	int old;

	/* Acquire: idle (0) -> pending (1) */
	old = atomic_cmpset_int(&mb->status, 0, 1);
	if (old == 0)
		return (EBUSY);

	/* Write 64-bit message as 4x u32 to MBOX_OUT */
	bus_write_4(mb->reg, BCE_REG_MBOX_OUT, (uint32_t)msg);
	bus_write_4(mb->reg, BCE_REG_MBOX_OUT + 4, (uint32_t)(msg >> 32));
	bus_write_4(mb->reg, BCE_REG_MBOX_OUT + 8, 0);
	bus_write_4(mb->reg, BCE_REG_MBOX_OUT + 12, 0);

	/* Wait for interrupt-driven reply */
	if (sema_timedwait(&mb->mb_cmpl, hz * timeout_ms / 1000) != 0) {
		/* Timeout -- reset to idle */
		atomic_store_int(&mb->status, 0);
		return (ETIMEDOUT);
	}

	if (atomic_load_int(&mb->status) != 2) {
		atomic_store_int(&mb->status, 0);
		return (ETIMEDOUT);
	}

	if (recv != NULL)
		*recv = mb->mb_result;

	atomic_store_int(&mb->status, 0);
	return (0);
}

/*
 * Called from ISR to retrieve mailbox reply.
 */
int
bce_mailbox_handle_interrupt(struct bce_mailbox *mb)
{
	uint32_t res, lo, hi;
	int count;

	res = bus_read_4(mb->reg, BCE_REG_MBOX_REPLY_CTR);
	count = (res >> 20) & 0xf;

	if (count == 0)
		return (ENOMSG);

	while (count-- > 0) {
		lo = bus_read_4(mb->reg, BCE_REG_MBOX_REPLY);
		hi = bus_read_4(mb->reg, BCE_REG_MBOX_REPLY + 4);
		bus_read_4(mb->reg, BCE_REG_MBOX_REPLY + 8);
		bus_read_4(mb->reg, BCE_REG_MBOX_REPLY + 12);
		mb->mb_result = ((uint64_t)hi << 32) | lo;
	}

	if (atomic_load_int(&mb->status) == 1) {
		atomic_store_int(&mb->status, 2);
		sema_post(&mb->mb_cmpl);
	}

	return (0);
}
