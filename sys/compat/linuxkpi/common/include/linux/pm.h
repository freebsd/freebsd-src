/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2024 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
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

#ifndef	_LINUXKPI_LINUX_PM_H
#define	_LINUXKPI_LINUX_PM_H

#include <linux/kernel.h>	/* pr_debug */
#include <asm/atomic.h>

/* Needed but breaks linux_usb.c */
/* #include <linux/completion.h> */
/* #include <linux/wait.h> */

struct device;

typedef struct pm_message {
	int event;
} pm_message_t;

struct dev_pm_domain {
};

struct dev_pm_info {
	atomic_t usage_count;
};

#define	PM_EVENT_FREEZE		0x0001
#define	PM_EVENT_SUSPEND	0x0002

#define	pm_sleep_ptr(_p)					\
    IS_ENABLED(CONFIG_PM_SLEEP) ? (_p) : NULL

#ifdef CONFIG_PM_SLEEP
#define	__SET_PM_OPS(_suspendfunc, _resumefunc)			\
	.suspend	= _suspendfunc,				\
	.resume		= _resumefunc,				\
	.freeze		= _suspendfunc,				\
	.thaw		= _resumefunc,				\
	.poweroff	= _suspendfunc,				\
	.restore	= _resumefunc,				\

#define	SIMPLE_DEV_PM_OPS(_name, _suspendfunc, _resumefunc)	\
const struct dev_pm_ops _name = {				\
	__SET_PM_OPS(_suspendfunc, _resumefunc)			\
}

#define	DEFINE_SIMPLE_DEV_PM_OPS(_name, _suspendfunc, _resumefunc) \
const struct dev_pm_ops _name = {				\
	__SET_PM_OPS(_suspendfunc, _resumefunc)			\
}

#define	SET_SYSTEM_SLEEP_PM_OPS(_suspendfunc, _resumefunc)	\
	__SET_PM_OPS(_suspendfunc, _resumefunc)
#else
#define	SIMPLE_DEV_PM_OPS(_name, _suspendfunc, _resumefunc)	\
const struct dev_pm_ops _name = {				\
}
#define	DEFINE_SIMPLE_DEV_PM_OPS(_name, _suspendfunc, _resumefunc) \
const struct dev_pm_ops _name = {				\
}
#endif

bool linuxkpi_device_can_wakeup(struct device *);
#define	device_can_wakeup(_dev)		linuxkpi_device_can_wakeup(_dev)

static inline void
pm_wakeup_event(struct device *dev __unused, unsigned int x __unused)
{

	pr_debug("%s: TODO\n", __func__);
}

#endif	/* _LINUXKPI_LINUX_PM_H */
