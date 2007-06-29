/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
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

/*
 *  FileName :    xgehal-event.h
 *
 *  Description:  event types
 *
 *  Created:      7 June 2004
 */

#ifndef XGE_HAL_EVENT_H
#define XGE_HAL_EVENT_H

#include <dev/nxge/include/xge-os-pal.h>

__EXTERN_BEGIN_DECLS

#define XGE_HAL_EVENT_BASE		0
#define XGE_LL_EVENT_BASE		100

/**
 * enum xge_hal_event_e - Enumerates slow-path HAL events.
 * @XGE_HAL_EVENT_UNKNOWN: Unknown (and invalid) event.
 * @XGE_HAL_EVENT_SERR: Serious hardware error event.
 * @XGE_HAL_EVENT_LINK_IS_UP: The link state has changed from 'down' to
 * 'up'; upper-layer driver (typically, link layer) is
 * supposed to wake the queue, etc.
 * @XGE_HAL_EVENT_LINK_IS_DOWN: Link-down event.
 *                    The link state has changed from 'down' to 'up';
 *                    upper-layer driver is supposed to stop traffic, etc.
 * @XGE_HAL_EVENT_ECCERR: ECC error event.
 * @XGE_HAL_EVENT_PARITYERR: Parity error event.
 * @XGE_HAL_EVENT_TARGETABORT: Target abort event. Used when device
 * aborts transmit operation with the corresponding transfer code
 * (for T_CODE enum see xgehal-fifo.h and xgehal-ring.h)
 * @XGE_HAL_EVENT_SLOT_FREEZE: Slot-freeze event. Driver tries to distinguish
 * slot-freeze from the rest critical events (e.g. ECC) when it is
 * impossible to PIO read "through" the bus, i.e. when getting all-foxes.
 *
 * xge_hal_event_e enumerates slow-path HAL eventis.
 *
 * See also: xge_hal_uld_cbs_t{}, xge_uld_link_up_f{},
 * xge_uld_link_down_f{}.
 */
typedef enum xge_hal_event_e {
	XGE_HAL_EVENT_UNKNOWN		= 0,
	/* HAL events */
	XGE_HAL_EVENT_SERR		= XGE_HAL_EVENT_BASE + 1,
	XGE_HAL_EVENT_LINK_IS_UP	= XGE_HAL_EVENT_BASE + 2,
	XGE_HAL_EVENT_LINK_IS_DOWN	= XGE_HAL_EVENT_BASE + 3,
	XGE_HAL_EVENT_ECCERR		= XGE_HAL_EVENT_BASE + 4,
	XGE_HAL_EVENT_PARITYERR		= XGE_HAL_EVENT_BASE + 5,
	XGE_HAL_EVENT_TARGETABORT       = XGE_HAL_EVENT_BASE + 6,
	XGE_HAL_EVENT_SLOT_FREEZE       = XGE_HAL_EVENT_BASE + 7,
} xge_hal_event_e;

__EXTERN_END_DECLS

#endif /* XGE_HAL_EVENT_H */
