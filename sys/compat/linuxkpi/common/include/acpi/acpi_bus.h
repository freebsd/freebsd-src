/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Vladimir Kondratyev <wulf@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
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

#ifndef _LINUXKPI_ACPI_ACPI_BUS_H_
#define _LINUXKPI_ACPI_ACPI_BUS_H_

typedef char acpi_device_class[20];

struct acpi_bus_event {
	acpi_device_class device_class;
	uint32_t type;
	uint32_t data;
};

ACPI_HANDLE	bsd_acpi_get_handle(device_t bsddev);
bool		acpi_check_dsm(ACPI_HANDLE handle, const char *uuid, int rev,
		    uint64_t funcs);
ACPI_OBJECT *	acpi_evaluate_dsm_typed(ACPI_HANDLE handle, const char *uuid,
		    int rev, int func, ACPI_OBJECT *argv4,
		    ACPI_OBJECT_TYPE type);
int		register_acpi_notifier(struct notifier_block *nb);
int		unregister_acpi_notifier(struct notifier_block *nb);
uint32_t	acpi_target_system_state(void);

#endif /* _LINUXKPI_ACPI_ACPI_BUS_H_ */
