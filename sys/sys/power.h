/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 Mitsuru IWASAKI
 * All rights reserved.
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * Portions of this software were developed by Aymeric Wibo
 * <obiwac@freebsd.org> under sponsorship from the FreeBSD Foundation.
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

#ifndef _SYS_POWER_H_
#define _SYS_POWER_H_
#ifdef _KERNEL

#include <sys/_eventhandler.h>
#include <sys/types.h>

/* Power management system type */
#define POWER_PM_TYPE_ACPI		0x01
#define POWER_PM_TYPE_NONE		0xff

/* Commands for Power management function */
#define POWER_CMD_SUSPEND		0x00

/*
 * Sleep state.
 *
 * These are high-level sleep states that the system can enter.  They map to
 * a specific generic sleep type (enum power_stype).
 */
#define POWER_SLEEP_STATE_STANDBY	0x00
#define POWER_SLEEP_STATE_SUSPEND	0x01
#define POWER_SLEEP_STATE_HIBERNATE	0x02

/*
 * Sleep type.
 *
 * These are the specific generic methods of entering a sleep state.  E.g.
 * POWER_SLEEP_STATE_SUSPEND could be set to enter either suspend-to-RAM (which
 * is S3 on ACPI systems), or suspend-to-idle (S0ix on ACPI systems).  This
 * would be done through the kern.power.suspend sysctl.
 */
enum power_stype {
	POWER_STYPE_AWAKE,
	POWER_STYPE_STANDBY,
	POWER_STYPE_SUSPEND_TO_MEM,
	POWER_STYPE_SUSPEND_TO_IDLE,
	POWER_STYPE_HIBERNATE,
	POWER_STYPE_POWEROFF,
	POWER_STYPE_COUNT,
	POWER_STYPE_UNKNOWN,
};

static const char * const power_stype_names[POWER_STYPE_COUNT] = {
	[POWER_STYPE_AWAKE]		= "awake",
	[POWER_STYPE_STANDBY]		= "standby",
	[POWER_STYPE_SUSPEND_TO_MEM]	= "s2mem",
	[POWER_STYPE_SUSPEND_TO_IDLE]	= "s2idle",
	[POWER_STYPE_HIBERNATE]		= "hibernate",
	[POWER_STYPE_POWEROFF]		= "poweroff",
};

extern enum power_stype	 power_standby_stype;
extern enum power_stype	 power_suspend_stype;
extern enum power_stype	 power_hibernate_stype;

extern enum power_stype	 power_name_to_stype(const char *_name);
extern const char	*power_stype_to_name(enum power_stype _stype);

typedef int (*power_pm_fn_t)(u_long _cmd, void* _arg, enum power_stype _stype);
extern int	 power_pm_register(u_int _pm_type, power_pm_fn_t _pm_fn,
			void *_pm_arg,
			bool _pm_supported[static POWER_STYPE_COUNT]);
extern u_int	 power_pm_get_type(void);
extern void	 power_pm_suspend(int);

/*
 * System power API.
 */
#define POWER_PROFILE_PERFORMANCE        0
#define POWER_PROFILE_ECONOMY            1

extern int	power_profile_get_state(void);
extern void	power_profile_set_state(int);

typedef void (*power_profile_change_hook)(void *, int);
EVENTHANDLER_DECLARE(power_profile_change, power_profile_change_hook);

#endif	/* _KERNEL */
#endif	/* !_SYS_POWER_H_ */
