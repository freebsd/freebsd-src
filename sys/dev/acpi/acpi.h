/*-
 * Copyright (c) 1999 Takanori Watanabe <takawata@shidahara1.planet.sci.kobe-u.ac.jp>
 * Copyright (c) 1999, 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *	$FreeBSD$
 */

#ifndef _DEV_ACPI_ACPI_H_
#define _DEV_ACPI_ACPI_H_

/* PowerResource control */
struct acpi_powerres_device {
	LIST_ENTRY(acpi_powerres_device) links;
	struct	aml_name *name;
	u_int8_t	state;		/* D0 to D3 */
	u_int8_t	next_state;	/* initialized with D0 */
};

struct acpi_powerres_device_ref {
	LIST_ENTRY(acpi_powerres_device_ref) links;
	struct	acpi_powerres_device *device;
};

struct acpi_powerres_info {
	LIST_ENTRY(acpi_powerres_info) links;
	struct	aml_name *name;
	u_int8_t	state;		/* OFF or ON */
	LIST_HEAD(, acpi_powerres_device_ref) reflist[3]; /* for _PR[0-2] */
};

/* softc */
typedef struct acpi_softc {
	struct	ACPIsdt *rsdt;
	struct	ACPIsdt *facp;
	struct	FACPbody *facp_body;
	struct	ACPIsdt *dsdt;
	struct	FACS *facs;
	int	system_state_initialized;
	int	broken_wakeuplogic;
	struct	acpi_system_state_package system_state_package;
	LIST_HEAD(, acpi_powerres_info) acpi_powerres_inflist;
	LIST_HEAD(, acpi_powerres_device) acpi_powerres_devlist;
} acpi_softc_t;

void acpi_powerres_set_sleeping_state(acpi_softc_t *sc, u_int8_t state);

#endif	/* _DEV_ACPI_ACPI_H_ */
