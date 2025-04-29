/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Colin Percival
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

#ifndef	__ACPI_GPIOBUS_H__
#define	__ACPI_GPIOBUS_H__

#include <sys/bus.h>

#include <contrib/dev/acpica/include/acpi.h>

enum acpi_gpiobus_ivars {
	ACPI_GPIOBUS_IVAR_HANDLE	= 10600,
	ACPI_GPIOBUS_IVAR_FLAGS,
};

#define ACPI_GPIOBUS_ACCESSOR(var, ivar, type)			\
	__BUS_ACCESSOR(acpi_gpiobus, var, ACPI_GPIOBUS, ivar, type)

ACPI_GPIOBUS_ACCESSOR(handle,	HANDLE,		ACPI_HANDLE)
ACPI_GPIOBUS_ACCESSOR(flags,	FLAGS,		uint32_t)

#undef ACPI_GPIOBUS_ACCESSOR

#endif	/* __ACPI_GPIOBUS_H__ */
