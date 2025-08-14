/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
 */

#ifndef _LINUXKPI_ACPI_ACPI_BUS_H_
#define _LINUXKPI_ACPI_ACPI_BUS_H_

/* Aliase struct acpi_device to device_t */
#define	acpi_device	_device

typedef char acpi_device_class[20];

struct acpi_bus_event {
	acpi_device_class device_class;
	uint32_t type;
	uint32_t data;
};

#define	acpi_dev_present(...)	lkpi_acpi_dev_present(__VA_ARGS__)
#define	acpi_dev_get_first_match_dev(...)	\
	lkpi_acpi_dev_get_first_match_dev(__VA_ARGS__)

ACPI_HANDLE	bsd_acpi_get_handle(device_t bsddev);
bool		acpi_check_dsm(ACPI_HANDLE handle, const guid_t *uuid, int rev,
		    uint64_t funcs);
ACPI_OBJECT *	acpi_evaluate_dsm_typed(ACPI_HANDLE handle, const guid_t *uuid,
		    int rev, int func, ACPI_OBJECT *argv4,
		    ACPI_OBJECT_TYPE type);
int		register_acpi_notifier(struct notifier_block *nb);
int		unregister_acpi_notifier(struct notifier_block *nb);
uint32_t	acpi_target_system_state(void);
bool		lkpi_acpi_dev_present(const char *hid, const char *uid,
		    int64_t hrv);
struct acpi_device *lkpi_acpi_dev_get_first_match_dev(const char *hid,
		    const char *uid, int64_t hrv);

union linuxkpi_acpi_object;

union linuxkpi_acpi_object *
acpi_evaluate_dsm(ACPI_HANDLE ObjHandle, const guid_t *guid,
    UINT64 rev, UINT64 func, union linuxkpi_acpi_object *arg);

#endif /* _LINUXKPI_ACPI_ACPI_BUS_H_ */
