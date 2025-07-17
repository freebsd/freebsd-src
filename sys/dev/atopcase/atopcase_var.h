/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2023 Val Packett <val@packett.cool>
 * Copyright (c) 2023 Vladimir Kondratyev <wulf@FreeBSD.org>
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

#ifndef _ATOPCASE_VAR_H_
#define	_ATOPCASE_VAR_H_

#include "opt_hid.h"

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/taskqueue.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include <dev/backlight/backlight.h>

#include <dev/hid/hid.h>

struct atopcase_child {
	device_t		hidbus;

	struct hid_device_info	hw;

	uint8_t			device;
	uint8_t			name[80];

	uint8_t			rdesc[ATOPCASE_MSG_SIZE];
	size_t			rdesc_len;

	hid_intr_t		*intr_handler;
	void			*intr_ctx;

	bool			open;
};

struct atopcase_softc {
	device_t		sc_dev;

	ACPI_HANDLE		sc_handle;
	int			sc_gpe_bit;

	int			sc_irq_rid;
	struct resource		*sc_irq_res;
	void			*sc_irq_ih;
	volatile unsigned int	sc_intr_cnt;

	struct timeout_task	sc_task;
	struct taskqueue	*sc_tq;

	bool			sc_booted;
	bool			sc_wait_for_status;

	uint8_t			sc_hid[HID_PNP_ID_SIZE];
	uint8_t			sc_vendor[80];
	uint8_t			sc_product[80];
	uint8_t			sc_serial[80];
	uint16_t		sc_vid;
	uint16_t		sc_pid;
	uint16_t		sc_ver;

	/*
	 * Writes are complex and async (i.e. 2 responses arrive via interrupt)
	 * and cannot be interleaved (no new writes until responses arrive).
	 * they are serialized with sc_write_sx lock.
	 */
	struct sx		sc_write_sx;
	/*
	 * SPI transfers must be separated by a small pause. As they can be
	 * initiated by both interrupts and users, do ATOPCASE_SPI_PAUSE()
	 * after each transfer and serialize them with sc_sx or sc_mtx locks
	 * depending on interupt source (GPE or PIC). Still use sc_write_sx
	 * lock while polling.
	 */
	struct sx		sc_sx;
	struct mtx		sc_mtx;

	struct atopcase_child	sc_kb;
	struct atopcase_child	sc_tp;

	struct cdev		*sc_backlight;
	uint32_t		sc_backlight_level;

	uint16_t		sc_msg_len;
	uint8_t			sc_msg[ATOPCASE_MSG_SIZE];
	struct atopcase_packet	sc_buf;
	struct atopcase_packet	sc_junk;
};

#ifdef HID_DEBUG
enum atopcase_log_level {
	ATOPCASE_LLEVEL_DISABLED = 0,
	ATOPCASE_LLEVEL_INFO,
	ATOPCASE_LLEVEL_DEBUG, /* for troubleshooting */
	ATOPCASE_LLEVEL_TRACE, /* log every packet */
};
extern enum atopcase_log_level atopcase_debug;
#endif

int atopcase_receive_packet(struct atopcase_softc *);
int atopcase_init(struct atopcase_softc *);
int atopcase_destroy(struct atopcase_softc *sc);
int atopcase_intr(struct atopcase_softc *);
void atopcase_intr_setup(device_t, device_t, hid_intr_t, void *,
    struct hid_rdesc_info *);
void atopcase_intr_unsetup(device_t, device_t);
int atopcase_intr_start(device_t, device_t);
int atopcase_intr_stop(device_t, device_t);
void atopcase_intr_poll(device_t, device_t);
int atopcase_get_rdesc(device_t, device_t, void *, hid_size_t);
int atopcase_set_report(device_t, device_t, const void *, hid_size_t, uint8_t,
    uint8_t);
int atopcase_backlight_update_status(device_t, struct backlight_props *);
int atopcase_backlight_get_status(device_t, struct backlight_props *);
int atopcase_backlight_get_info(device_t, struct backlight_info *);

#endif /* _ATOPCASE_VAR_H_ */
