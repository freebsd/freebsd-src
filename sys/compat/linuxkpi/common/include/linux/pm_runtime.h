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
 * We could #error in case CONFIG_PM or CONFIG_PM_SLEEP are set but that
 * would likely be too intrusive for now.
 *
 * We could likely use dev_dbg() in here instead of pr_debug() if the former
 * would be mapped to the latter with additional dev information.
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
	return (1);
}

static inline int
pm_runtime_get_if_in_use(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
	return (-EINVAL);
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
	return (-EINVAL);
}

static inline int
pm_runtime_resume_and_get(struct device *dev __unused)
{
	pr_debug("%s TODO\n", __func__);
	return (1);
}

#endif	/* _LINUXKPI_LINUX_PM_RUNTIME_H_ */
