/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Project.
 * All rights reserved.
 *
 * See suspend.h for full license text.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/uio.h>

#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>

#include "suspend.h"
#include "../power/cpufreq.h"
#include "../power/display_power.h"

#define PM_LOG(level, fmt, ...)		do {		\
	printf("pm: " fmt "\n", ##__VA_ARGS__);\
} while (0)

FEATURE(module_mobile_pm, "Mobile power management / suspend subsystem");

static struct pm_context pm_ctx;

static void
pm_handle_suspend(void)
{
	struct pm_device_ops *dev;

	pm_ctx.current = pm_ctx.target;
	PM_LOG(LOG_INFO, "entering suspend state %d", pm_ctx.current);

	for (dev = pm_ctx.dev_ops; dev != NULL; dev = dev->next) {
		if (dev->notifier)
			dev->notifier(PM_STATE_POWER_ON, pm_ctx.current,
			    dev->arg);
		if (dev->save_state)
			dev->save_state(dev->arg);
	}
}

static void
pm_handle_resume(void)
{
	struct pm_device_ops *dev;

	PM_LOG(LOG_INFO, "resuming from suspend state %d", pm_ctx.current);

	for (dev = pm_ctx.dev_ops; dev != NULL; dev = dev->next) {
		if (dev->restore_state)
			dev->restore_state(dev->arg);
		if (dev->notifier)
			dev->notifier(pm_ctx.current, PM_STATE_POWER_ON,
			    dev->arg);
	}

	pm_ctx.current = PM_STATE_POWER_ON;
}

static int
pm_notify_ops(enum pm_state old, enum pm_state new, void *arg)
{
	struct pm_ops_entry *ops;

	if (old == new)
		return (0);

	switch (new) {
	case PM_SUSPEND_TO_IDLE:
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		for (ops = pm_ctx.ops_list; ops; ops = ops->next) {
			if (ops->pre_suspend)
				ops->pre_suspend(ops->arg);
		}
		pm_handle_suspend();
		for (ops = pm_ctx.ops_list; ops; ops = ops->next) {
			if (ops->post_suspend)
				ops->post_suspend(ops->arg);
		}
		break;
	case PM_STATE_POWER_ON:
	default:
		for (ops = pm_ctx.ops_list; ops; ops = ops->next) {
			if (ops->pre_resume)
				ops->pre_resume(ops->arg);
		}
		pm_handle_resume();
		for (ops = pm_ctx.ops_list; ops; ops = ops->next) {
			if (ops->post_resume)
				ops->post_resume(ops->arg);
		}
		break;
	}

	return (0);
}

int
pm_init(void)
{
	memset(&pm_ctx, 0, sizeof(pm_ctx));
	pm_ctx.current = PM_STATE_POWER_ON;
	pm_ctx.suspend_enabled = 1;

	cf_init();

	PM_LOG(LOG_INFO, "Power management subsystem initialized");
	return (0);
}

static int
pm_trigger_suspend(enum pm_state state)
{
	struct pm_device_ops *dev;
	int fd, ret;
	uint64_t wc;
	const char *st_str;

	if (!pm_ctx.suspend_enabled)
		return (EPERM);

	if (!pm_can_suspend()) {
		PM_LOG(LOG_WARNING, "Suspend blocked by %u blockers",
		    pm_ctx.blocker_count);
		return (EBUSY);
	}

	switch (state) {
	case PM_SUSPEND_TO_IDLE:
		st_str = "freeze";
		break;
	case PM_SUSPEND_STANDBY:
		st_str = "standby";
		break;
	case PM_SUSPEND_MEM:
		st_str = "mem";
		break;
	default:
		return (EINVAL);
	}

	PM_LOG(LOG_INFO, "Requesting suspend: %s", st_str);

	fd = open("/sys/power/state", O_WRONLY);
	if (fd < 0)
		return (errno);

	ret = write(fd, st_str, strlen(st_str));
	if (ret < 0) {
		close(fd);
		return (ret < 0 ? errno : ret);
	}
	close(fd);

	fd = open("/sys/power/wakeup_count", O_RDONLY);
	if (fd >= 0) {
		memset(&wc, 0, sizeof(wc));
		read(fd, &wc, sizeof(wc));
		close(fd);
		pm_ctx.wake_count = wc;
	}

	for (dev = pm_ctx.dev_ops; dev; dev = dev->next) {
		if (dev->notifier)
			dev->notifier(PM_STATE_POWER_ON, state, dev->arg);
		if (dev->save_state)
			dev->save_state(dev->arg);
	}

	pm_ctx.current = state;

	for (dev = pm_ctx.dev_ops; dev; dev = dev->next) {
		if (dev->restore_state)
			dev->restore_state(dev->arg);
		if (dev->notifier)
			dev->notifier(state, PM_STATE_POWER_ON, dev->arg);
	}

	PM_LOG(LOG_INFO, "Resumed from state %d", state);
	pm_ctx.current = PM_STATE_POWER_ON;

	return (0);
}

int
pm_suspend(enum pm_state state)
{
	int error;

	error = pm_notify_ops(PM_STATE_POWER_ON, state, NULL);
	if (error)
		return (error);

	return (pm_trigger_suspend(state));
}

int
pm_resume(void)
{
	return (pm_trigger_suspend(PM_STATE_POWER_ON));
}

int
pm_register_device_ops(pm_save_fn save, pm_restore_fn restore,
    pm_notify_fn notif, void *arg)
{
	struct pm_device_ops *dev;

	dev = malloc(sizeof(*dev), M_PM, M_WAITOK | M_ZERO);
	if (dev == NULL)
		return (ENOMEM);

	dev->save_state = save;
	dev->restore_state = restore;
	dev->notifier = notif;
	dev->arg = arg;
	dev->next = pm_ctx.dev_ops;
	pm_ctx.dev_ops = dev;

	PM_LOG(LOG_DEBUG, "Device ops registered at %p", dev);
	return (0);
}

int
pm_register_ops(pm_ops_fn pre_s, pm_ops_fn post_s, pm_ops_fn pre_r,
    pm_ops_fn post_r, void *arg)
{
	struct pm_ops_entry *ops;

	ops = malloc(sizeof(*ops), M_PM, M_WAITOK | M_ZERO);
	if (ops == NULL)
		return (ENOMEM);

	ops->pre_suspend = pre_s;
	ops->post_suspend = post_s;
	ops->pre_resume = pre_r;
	ops->post_resume = post_r;
	ops->arg = arg;
	ops->next = pm_ctx.ops_list;
	pm_ctx.ops_list = ops;

	return (0);
}

int
pm_set_wake_source(enum pm_wake_source source, bool enable)
{
	uint32_t i;

	for (i = 0; i < pm_ctx.wake_count && i < PM_MAX_WAKE_SOURCES; i++) {
		if (pm_ctx.wake_events[i].source == source) {
			pm_ctx.wake_events[i].active = enable ? 1 : 0;
			return (0);
		}
	}

	if (!enable || pm_ctx.wake_count >= PM_MAX_WAKE_SOURCES)
		return (ENOSPC);

	pm_ctx.wake_events[pm_ctx.wake_count].source = source;
	pm_ctx.wake_events[pm_ctx.wake_count].active = 1;
	pm_ctx.wake_events[pm_ctx.wake_count].timestamp = 0;
	pm_ctx.wake_count++;

	return (0);
}

int
pm_ack_wake_event(enum pm_wake_source source)
{
	uint32_t i;

	for (i = 0; i < pm_ctx.wake_count; i++) {
		if (pm_ctx.wake_events[i].source == source) {
			pm_ctx.wake_events[i].timestamp = 0;
			return (0);
		}
	}

	return (ENOENT);
}

int
pm_add_suspend_blocker(const char *name, uint32_t owner)
{
	uint32_t b;

	if (!name || pm_ctx.blocker_count >= PM_MAX_SUSPEND_BLOCKERS)
		return (EINVAL);

	for (b = 0; b < pm_ctx.blocker_count; b++) {
		if (strcmp(pm_ctx.blockers[b].name, name) == 0) {
			if (pm_ctx.blockers[b].owner_tid == owner)
				pm_ctx.blockers[b].refcount++;
			return (0);
		}
	}

	b = pm_ctx.blocker_count++;
	strlcpy(pm_ctx.blockers[b].name, name,
	    sizeof(pm_ctx.blockers[b].name));
	pm_ctx.blockers[b].owner_tid = owner;
	pm_ctx.blockers[b].refcount = 1;
	pm_ctx.blockers[b].active = 1;

	PM_LOG(LOG_DEBUG, "Suspend blocker added: %s (owner=%u)", name, owner);
	return (0);
}

int
pm_remove_suspend_blocker(const char *name)
{
	uint32_t b;

	if (!name)
		return (EINVAL);

	for (b = 0; b < pm_ctx.blocker_count; b++) {
		if (strcmp(pm_ctx.blockers[b].name, name) == 0) {
			if (pm_ctx.blockers[b].refcount > 1)
				pm_ctx.blockers[b].refcount--;
			else {
				pm_ctx.blockers[b].active = 0;
				pm_ctx.blockers[b] =
				    pm_ctx.blockers[--pm_ctx.blocker_count];
			}
			PM_LOG(LOG_DEBUG, "Suspend blocker removed: %s",
			    name);
			return (0);
		}
	}

	return (ENOENT);
}

bool
pm_can_suspend(void)
{
	uint32_t b;

	if (!pm_ctx.suspend_enabled)
		return (false);

	for (b = 0; b < pm_ctx.blocker_count; b++) {
		if (pm_ctx.blockers[b].active)
			return (false);
	}

	return (true);
}

enum pm_state
pm_get_current_state(void)
{
	return (pm_ctx.current);
}

int
pm_get_wake_events(struct pm_wake_event events[PM_MAX_WAKE_SOURCES],
    uint32_t *count)
{
	uint32_t i;

	if (events == NULL || count == NULL)
		return (EINVAL);

	i = 0;
	if (i < pm_ctx.wake_count && i < PM_MAX_WAKE_SOURCES)
		events[i++] = pm_ctx.wake_events[i];

	*count = MIN(i, PM_MAX_WAKE_SOURCES);
	return (0);
}
