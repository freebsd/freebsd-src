/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Project.
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

#ifndef _MOBILE_POWER_SUSPEND_H_
#define _MOBILE_POWER_SUSPEND_H_

#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdbool.h>

#define PM_MAX_WAKE_SOURCES	16
#define PM_MAX_SUSPEND_BLOCKERS	64
#define PM_MAX_OPS_HANDLERS	32

enum pm_state {
	PM_STATE_POWER_ON,
	PM_SUSPEND_TO_IDLE,
	PM_SUSPEND_STANDBY,
	PM_SUSPEND_MEM,
};

enum pm_wake_source {
	PM_WAKE_RTC,
	PM_WAKE_GPIO,
	PM_WAKE_USB,
	/* USB host resume */
	PM_WAKE_PWRBTN,
	PM_WAKE_KEYPAD,
	PM_WAKE_LID,
	PM_WAKE_NETWORK,
	PM_WAKE_ALARM,
};

struct pm_wake_event {
	enum pm_wake_source source;
	uint64_t timestamp;
	uint8_t	active;
};

struct pm_suspend_blocker {
	char name[64];
	uint32_t owner_tid;
	uint32_t refcount;
	uint8_t	active;
};

typedef	void (*pm_ops_fn)(void *arg);
typedef int (*pm_notify_fn)(enum pm_state old, enum pm_state new, void *arg);
typedef int (*pm_save_fn)(void *arg);
typedef int (*pm_restore_fn)(void *arg);

struct pm_device_ops {
	pm_save_fn	save_state;
	pm_restore_fn	restore_state;
	pm_notify_fn	notifier;
	void		*arg;
	struct pm_device_ops *next;
};

struct pm_ops_entry {
	pm_ops_fn	pre_suspend;
	pm_ops_fn	post_suspend;
	pm_ops_fn	pre_resume;
	pm_ops_fn	post_resume;
	void		*arg;
	struct pm_ops_entry *next;
};

struct pm_context {
	enum pm_state	current;
	enum pm_state	target;
	uint32_t	wake_count;
	struct pm_wake_event	wake_events[PM_MAX_WAKE_SOURCES];
	struct pm_suspend_blocker blockers[PM_MAX_SUSPEND_BLOCKERS];
	uint32_t	blocker_count;
	struct pm_device_ops	*dev_ops;
	struct pm_ops_entry	*ops_list;
	int		fd;
	uint8_t		suspend_enabled;
};

#define PM_IOCTL_SUSPEND		_IOW('P', 0x01, enum pm_state)
#define PM_IOCTL_RESUME		_IO('P',  0x02)
#define PM_IOCTL_GETSTATE		_IOWR('P', 0x03, enum pm_state)
#define PM_IOCTL_WAKEUP		_IOW('P', 0x04, enum pm_wake_source)
#define PM_IOCTL_GETWAKE		_IOWR('P', 0x05, struct pm_wake_event)
#define PM_IOCTL_ADDBLOCKER	_IOW('P', 0x06, struct pm_suspend_blocker)
#define PM_IOCTL_RMBLOCKER	_IOW('P', 0x07, char[64])
#define PM_IOCTL_ENABLE		_IO('P',  0x08)
#define PM_IOCTL_DISABLE		_IO('P',  0x09)
#define PM_IOCTL_ACKWAKE		_IOW('P', 0x0A, enum pm_wake_source)

#define PM_WAKE_SOURCE_RTC		"/sys/power/wake_lock"
#define PM_WAKE_UNLOCK		"/sys/power/wake_unlock"
#define PM_SUSPEND_STATE		"/sys/power/state"
#define PM_WAKE_COUNT		"/sys/power/wakeup_count"

int pm_init(void);
int pm_suspend(enum pm_state state);
int pm_resume(void);
int pm_register_device_ops(pm_save_fn save, pm_restore_fn restore,
    pm_notify_fn notif, void *arg);
int pm_register_ops(pm_ops_fn pre_s, pm_ops_fn post_s, pm_ops_fn pre_r,
    pm_ops_fn post_r, void *arg);
int pm_set_wake_source(enum pm_wake_source source, bool enable);
int pm_ack_wake_event(enum pm_wake_source source);
int pm_add_suspend_blocker(const char *name, uint32_t owner);
int pm_remove_suspend_blocker(const char *name);
bool pm_can_suspend(void);
enum pm_state pm_get_current_state(void);
int pm_get_wake_events(struct pm_wake_event events[PM_MAX_WAKE_SOURCES],
    uint32_t *count);

#endif /* _MOBILE_POWER_SUSPEND_H_ */
