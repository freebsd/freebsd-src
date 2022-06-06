/*-
 * Copyright (c) 2020 The FreeBSD Foundation
 *
 * This software was developed by Emmanuel Vadot under sponsorship
 * from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <linux/dmi.h>

static char *dmi_data[DMI_STRING_MAX];

static void
linux_dmi_preload(void *arg)
{

	dmi_data[DMI_BIOS_VENDOR] = kern_getenv("smbios.bios.vendor");
	dmi_data[DMI_BIOS_VERSION] = kern_getenv("smbios.bios.version");
	dmi_data[DMI_BIOS_DATE] = kern_getenv("smbios.bios.reldate");
	dmi_data[DMI_SYS_VENDOR] = kern_getenv("smbios.system.maker");
	dmi_data[DMI_PRODUCT_NAME] = kern_getenv("smbios.system.product");
	dmi_data[DMI_PRODUCT_VERSION] = kern_getenv("smbios.system.version");
	dmi_data[DMI_PRODUCT_SERIAL] = kern_getenv("smbios.system.serial");
	dmi_data[DMI_PRODUCT_UUID] = kern_getenv("smbios.system.uuid");
	dmi_data[DMI_BOARD_VENDOR] = kern_getenv("smbios.planar.maker");
	dmi_data[DMI_BOARD_NAME] = kern_getenv("smbios.planar.product");
	dmi_data[DMI_BOARD_VERSION] = kern_getenv("smbios.planar.version");
	dmi_data[DMI_BOARD_SERIAL] = kern_getenv("smbios.planar.serial");
	dmi_data[DMI_BOARD_ASSET_TAG] = kern_getenv("smbios.planar.tag");
	dmi_data[DMI_CHASSIS_VENDOR] = kern_getenv("smbios.chassis.maker");
	dmi_data[DMI_CHASSIS_TYPE] = kern_getenv("smbios.chassis.type");
	dmi_data[DMI_CHASSIS_VERSION] = kern_getenv("smbios.chassis.version");
	dmi_data[DMI_CHASSIS_SERIAL] = kern_getenv("smbios.chassis.serial");
	dmi_data[DMI_CHASSIS_ASSET_TAG] = kern_getenv("smbios.chassis.tag");
}
SYSINIT(linux_dmi_preload, SI_SUB_DRIVERS, SI_ORDER_ANY, linux_dmi_preload, NULL);

/* Match a system against a field */
bool
linux_dmi_match(enum dmi_field f, const char *str)
{

	if (f < DMI_STRING_MAX &&
	    dmi_data[f] != NULL &&
	    strcmp(dmi_data[f], str) == 0)
		return(true);
	return (false);
}

/* Match a system against the struct, all matches must be ok */
static bool
linux_dmi_matches(const struct dmi_system_id *dsi)
{
	enum dmi_field slot;
	int i;

	for (i = 0; i < nitems(dsi->matches); i++) {
		slot = dsi->matches[i].slot;
		if (slot == DMI_NONE)
			break;
		if (slot >= DMI_STRING_MAX ||
		    dmi_data[slot] == NULL)
			return (false);
		if (dsi->matches[i].exact_match) {
			if (dmi_match(slot, dsi->matches[i].substr))
				continue;
		} else if (strstr(dmi_data[slot],
			dsi->matches[i].substr) != NULL) {
			continue;
		}
		return (false);
	}
	return (true);
}

/* Return the string matching the field */
const char *
linux_dmi_get_system_info(int field)
{

	if (field < DMI_STRING_MAX)
		return (dmi_data[field]);
	return (NULL);
}

/* 
 * Match a system against the structs list
 * If a match is found return the corresponding structure.
 */
const struct dmi_system_id *
linux_dmi_first_match(const struct dmi_system_id *list)
{
	const struct dmi_system_id *dsi;

	for (dsi = list; dsi->matches[0].slot != 0; dsi++) {
		if (linux_dmi_matches(dsi))
			return (dsi);
	}

	return (NULL);
}

/*
 * Match a system against the structs list
 * For each match call the callback with the corresponding data
 * Return the number of matches.
 */
int
linux_dmi_check_system(const struct dmi_system_id *sysid)
{
	const struct dmi_system_id *dsi;
	int matches = 0;

	for (dsi = sysid; dsi->matches[0].slot != 0; dsi++) {
		if (linux_dmi_matches(dsi)) {
			matches++;
			if (dsi->callback && dsi->callback(dsi))
				break;
		}
	}

	return (matches);
}
