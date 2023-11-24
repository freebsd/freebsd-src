/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
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
 */

#ifndef _DEV_INTEL_SPI_H_
#define _DEV_INTEL_SPI_H_

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>

enum intelspi_vers {
	SPI_BAYTRAIL,
	SPI_BRASWELL,
	SPI_LYNXPOINT,
	SPI_SUNRISEPOINT,
};

struct intelspi_softc {
	ACPI_HANDLE		sc_handle;
	device_t		sc_dev;
	enum intelspi_vers	sc_vers;
	struct mtx		sc_mtx;
	int			sc_mem_rid;
	struct resource		*sc_mem_res;
	int			sc_irq_rid;
	struct resource		*sc_irq_res;
	void			*sc_irq_ih;
	struct spi_command	*sc_cmd;
	uint32_t		sc_len;
	uint32_t		sc_read;
	volatile uint32_t	sc_flags;
	uint32_t		sc_written;
	uint32_t		sc_clock;
	uint32_t		sc_mode;
	/* LPSS private register storage for suspend-resume */
	uint32_t		sc_regs[9];
};

int	intelspi_attach(device_t dev);
int	intelspi_detach(device_t dev);
int	intelspi_transfer(device_t dev, device_t child, struct spi_command *cmd);
int	intelspi_suspend(device_t dev);
int	intelspi_resume(device_t dev);

#endif
