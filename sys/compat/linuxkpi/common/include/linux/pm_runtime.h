/*
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * Portions of this software were developed by Björn Zeeb
 * under sponsorship from the FreeBSD Foundation.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _LINUXKPI_LINUX_PM_RUNTIME_H_
#define _LINUXKPI_LINUX_PM_RUNTIME_H_

#include <linux/kernel.h>	/* pr_debug */
#include <linux/device.h>
#include <linux/pm.h>

/*
 * Return values, where needed, are based on the !CONFIG_PM/!CONFIG_PM_SLEEP
 * assumption given we currently do not support anything in here.
 * Several values used to be hard coded in the previous implementation
 * with the assumption that CONFIG_PM was set for the driver to compile
 * and returned appropriate values as needed for drm-kmod.  They were restored
 * inline leaving the TODO logging available as the implementation itself is
 * still missing as are any non-trivial dev->power bits in LinuxKPI.
 */

static inline void
pm_runtime_enable(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
}

static inline void
pm_runtime_disable(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
}

static inline void
pm_runtime_allow(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
}

static inline void
pm_runtime_forbid(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
}


static inline void
pm_runtime_mark_last_busy(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
}

static inline void
pm_runtime_get_noresume(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
}

static inline void
pm_runtime_put_noidle(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
}

static inline bool
pm_runtime_suspended(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
	return (false);
}

static inline int
pm_runtime_autosuspend(struct device *dev)
{
	pm_runtime_mark_last_busy(dev);
	pr_debug("%s TODO\n", __func__);
	return (-ENOSYS);
}

static inline void
pm_runtime_use_autosuspend(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
}

static inline void
pm_runtime_dont_use_autosuspend(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
}

static inline void
pm_runtime_set_autosuspend_delay(struct device *dev __unused, int delay __unused)
{
	pr_debug("%s TODO\n", __func__);
}

static inline int
pm_runtime_put_autosuspend(struct device *dev)
{
	pm_runtime_mark_last_busy(dev);
	pr_debug("%s TODO\n", __func__);
	return (-ENOSYS);
}

static inline int
pm_runtime_set_active(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
	return (0);
}

static inline void
pm_runtime_put(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
}

static inline int
pm_runtime_resume(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
	return (1);
}

static inline int
pm_runtime_get_sync(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
#ifdef CONFIG_PM
	return (0);
#else
	return (1);
#endif
}

static inline int
pm_runtime_get_if_in_use(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
#ifdef CONFIG_PM
	return (1);
#else
	return (-EINVAL);
#endif
}

#if defined(LINUXKPI_VERSION) && LINUXKPI_VERSION < 60900
static inline int
pm_runtime_get_if_active(struct device *dev __unused, bool x __unused)
#else
static inline int
pm_runtime_get_if_active(struct device *dev __unused)
#endif
{
	pr_debug("%s TODO\n", __func__);
#ifdef CONFIG_PM
	return (1);
#else
	return (-EINVAL);
#endif

}

static inline int
pm_runtime_resume_and_get(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
#ifdef CONFIG_PM
	return (0);
#else
	return (1);
#endif
}

#endif	/* _LINUXKPI_LINUX_PM_RUNTIME_H_ */
