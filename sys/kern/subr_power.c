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

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/power.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

enum power_stype	 power_standby_stype	= POWER_STYPE_UNKNOWN;
enum power_stype	 power_suspend_stype	= POWER_STYPE_UNKNOWN;
enum power_stype	 power_hibernate_stype	= POWER_STYPE_UNKNOWN;

static u_int		 power_pm_type	= POWER_PM_TYPE_NONE;
static power_pm_fn_t	 power_pm_fn	= NULL;
static void		*power_pm_arg	= NULL;
static bool		 power_pm_supported[POWER_STYPE_COUNT] = {0};
static struct task	 power_pm_task;

enum power_stype
power_name_to_stype(const char *name)
{
	enum power_stype	stype;

	for (stype = 0; stype < POWER_STYPE_COUNT; stype++) {
		if (strcasecmp(name, power_stype_names[stype]) == 0)
			return (stype);
	}
	return (POWER_STYPE_UNKNOWN);
}

const char *
power_stype_to_name(enum power_stype stype)
{
	if (stype == POWER_STYPE_UNKNOWN)
		return ("NONE");
	if (stype < POWER_STYPE_AWAKE || stype >= POWER_STYPE_COUNT)
		return (NULL);
	return (power_stype_names[stype]);
}

static int
sysctl_supported_stypes(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct sbuf sb;
	enum power_stype stype;

	sbuf_new(&sb, NULL, 32, SBUF_AUTOEXTEND);
	for (stype = 0; stype < POWER_STYPE_COUNT; stype++) {
		if (power_pm_supported[stype])
			sbuf_printf(&sb, "%s ", power_stype_to_name(stype));
	}
	sbuf_trim(&sb);
	sbuf_finish(&sb);
	error = sysctl_handle_string(oidp, sbuf_data(&sb), sbuf_len(&sb), req);
	sbuf_delete(&sb);

	return (error);
}

static int
power_sysctl_stype(SYSCTL_HANDLER_ARGS)
{
	char			name[10];
	int			err;
	enum power_stype	new_stype, old_stype;

	old_stype = *(enum power_stype *)oidp->oid_arg1;
	strlcpy(name, power_stype_to_name(old_stype), sizeof(name));
	err = sysctl_handle_string(oidp, name, sizeof(name), req);
	if (err != 0 || req->newptr == NULL)
		return (err);

	new_stype = power_name_to_stype(name);
	if (new_stype == POWER_STYPE_UNKNOWN)
		return (EINVAL);
	if (!power_pm_supported[new_stype])
		return (EOPNOTSUPP);
	if (new_stype != old_stype)
		*(enum power_stype *)oidp->oid_arg1 = new_stype;
	return (0);
}

static SYSCTL_NODE(_kern, OID_AUTO, power, CTLFLAG_RW, 0,
    "Generic power management related sysctls");

SYSCTL_PROC(_kern_power, OID_AUTO, supported_stype,
    CTLTYPE_STRING | CTLFLAG_RD, 0, 0, sysctl_supported_stypes, "A",
    "List supported sleep types");
SYSCTL_PROC(_kern_power, OID_AUTO, standby, CTLTYPE_STRING | CTLFLAG_RW,
    &power_standby_stype, 0, power_sysctl_stype, "A",
    "Sleep type to enter on standby");
SYSCTL_PROC(_kern_power, OID_AUTO, suspend, CTLTYPE_STRING | CTLFLAG_RW,
    &power_suspend_stype, 0, power_sysctl_stype, "A",
    "Sleep type to enter on suspend");
SYSCTL_PROC(_kern_power, OID_AUTO, hibernate, CTLTYPE_STRING | CTLFLAG_RW,
    &power_hibernate_stype, 0, power_sysctl_stype, "A",
    "Sleep type to enter on hibernate");

static void
power_pm_deferred_fn(void *arg, int pending)
{
	enum power_stype stype = (intptr_t)arg;

	power_pm_fn(POWER_CMD_SUSPEND, power_pm_arg, stype);
}

int
power_pm_register(u_int pm_type, power_pm_fn_t pm_fn, void *pm_arg,
    bool pm_supported[static POWER_STYPE_COUNT])
{
	int	error;

	if (power_pm_type == POWER_PM_TYPE_NONE ||
	    power_pm_type == pm_type) {
		power_pm_type	= pm_type;
		power_pm_fn	= pm_fn;
		power_pm_arg	= pm_arg;
		memcpy(power_pm_supported, pm_supported,
		    sizeof(power_pm_supported));
		if (power_pm_supported[POWER_STYPE_STANDBY])
			power_standby_stype = POWER_STYPE_STANDBY;
		if (power_pm_supported[POWER_STYPE_SUSPEND_TO_MEM])
			power_suspend_stype = POWER_STYPE_SUSPEND_TO_MEM;
		else if (power_pm_supported[POWER_STYPE_SUSPEND_TO_IDLE])
			power_suspend_stype = POWER_STYPE_SUSPEND_TO_IDLE;
		if (power_pm_supported[POWER_STYPE_HIBERNATE])
			power_hibernate_stype = POWER_STYPE_HIBERNATE;
		error = 0;
		TASK_INIT(&power_pm_task, 0, power_pm_deferred_fn, NULL);
	} else {
		error = ENXIO;
	}

	return (error);
}

u_int
power_pm_get_type(void)
{

	return (power_pm_type);
}

void
power_pm_suspend(int state)
{
	enum power_stype	stype;

	if (power_pm_fn == NULL)
		return;

	switch (state) {
	case POWER_SLEEP_STATE_STANDBY:
		stype = power_standby_stype;
		break;
	case POWER_SLEEP_STATE_SUSPEND:
		stype = power_suspend_stype;
		break;
	case POWER_SLEEP_STATE_HIBERNATE:
		stype = power_hibernate_stype;
		break;
	default:
		printf("%s: unknown sleep state %d\n", __func__, state);
		return;
	}

	power_pm_task.ta_context = (void *)(intptr_t)stype;
	taskqueue_enqueue(taskqueue_thread, &power_pm_task);
}

/*
 * Power profile.
 */

static int	power_profile_state = POWER_PROFILE_PERFORMANCE;

int
power_profile_get_state(void)
{
	return (power_profile_state);
}

void
power_profile_set_state(int state) 
{
	int		changed;
    
	if (state != power_profile_state) {
		power_profile_state = state;
		changed = 1;
		if (bootverbose) {
			printf("system power profile changed to '%s'\n",
				(state == POWER_PROFILE_PERFORMANCE) ?
				"performance" : "economy");
		}
	} else {
		changed = 0;
	}

	if (changed)
		EVENTHANDLER_INVOKE(power_profile_change, 0);
}
