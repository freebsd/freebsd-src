/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Apple BCE mailbox interface.
 */

#ifndef _APPLE_BCE_MAILBOX_H_
#define _APPLE_BCE_MAILBOX_H_

#include "apple_bce.h"

int	bce_mailbox_init(struct bce_mailbox *mb, struct resource *reg);
void	bce_mailbox_destroy(struct bce_mailbox *mb);
int	bce_mailbox_send(struct bce_mailbox *mb, uint64_t msg, uint64_t *recv,
	    unsigned int timeout_ms);
int	bce_mailbox_handle_interrupt(struct bce_mailbox *mb);

#endif /* _APPLE_BCE_MAILBOX_H_ */
