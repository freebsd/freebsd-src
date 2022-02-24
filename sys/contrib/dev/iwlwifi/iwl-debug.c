// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Copyright (C) 2005-2011, 2021 Intel Corporation
 */
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/export.h>
#if defined(CONFIG_IWLWIFI_DEBUG)
#include <linux/net.h>
#endif
#include "iwl-drv.h"
#include "iwl-debug.h"
#if defined(__FreeBSD__)
#include "iwl-modparams.h"
#endif
#include "iwl-devtrace.h"

#if defined(__FreeBSD__)
#if defined(CONFIG_IWLWIFI_DEBUG)
#include <sys/systm.h>		/* hexdump(9) */
#include <linux/preempt.h>
#endif
#endif

#if defined(__linux__)
#define __iwl_fn(fn)						\
void __iwl_ ##fn(struct device *dev, const char *fmt, ...)	\
{								\
	struct va_format vaf = {				\
		.fmt = fmt,					\
	};							\
	va_list args;						\
								\
	va_start(args, fmt);					\
	vaf.va = &args;						\
	dev_ ##fn(dev, "%pV", &vaf);				\
	trace_iwlwifi_ ##fn(&vaf);				\
	va_end(args);						\
}
#elif defined(__FreeBSD__)
#define __iwl_fn(fn)						\
void __iwl_ ##fn(struct device *dev, const char *fmt, ...)	\
{								\
	struct va_format vaf = {				\
		.fmt = fmt,					\
	};							\
	va_list args;						\
	char *str;						\
								\
	va_start(args, fmt);					\
	vaf.va = &args;						\
	vasprintf(&str, M_KMALLOC, fmt, args);			\
	dev_ ##fn(dev, "%s", str);				\
	trace_iwlwifi_ ##fn(&vaf);				\
	free(str, M_KMALLOC);					\
	va_end(args);						\
}
#endif

__iwl_fn(warn)
IWL_EXPORT_SYMBOL(__iwl_warn);
__iwl_fn(info)
IWL_EXPORT_SYMBOL(__iwl_info);
__iwl_fn(crit)
IWL_EXPORT_SYMBOL(__iwl_crit);

void __iwl_err(struct device *dev, enum iwl_err_mode mode, const char *fmt, ...)
{
	struct va_format vaf = {
		.fmt = fmt,
	};
	va_list args, args2;

	va_start(args, fmt);
	switch (mode) {
	case IWL_ERR_MODE_RATELIMIT:
		if (net_ratelimit())
			break;
		fallthrough;
	case IWL_ERR_MODE_REGULAR:
	case IWL_ERR_MODE_RFKILL:
		va_copy(args2, args);
		vaf.va = &args2;
#if defined(__linux_)
		if (mode == IWL_ERR_MODE_RFKILL)
			dev_err(dev, "(RFKILL) %pV", &vaf);
		else
			dev_err(dev, "%pV", &vaf);
#elif defined(__FreeBSD__)
		char *str;
		vasprintf(&str, M_KMALLOC, fmt, args2);
		dev_err(dev, "%s%s", (mode == IWL_ERR_MODE_RFKILL) ? "(RFKILL)" : "", str);
		free(str, M_KMALLOC);
#endif
		va_end(args2);
		break;
	default:
		break;
	}
	trace_iwlwifi_err(&vaf);
	va_end(args);
}
IWL_EXPORT_SYMBOL(__iwl_err);

#if defined(CONFIG_IWLWIFI_DEBUG) || defined(CONFIG_IWLWIFI_DEVICE_TRACING)

#ifdef CONFIG_IWLWIFI_DEBUG
bool
iwl_have_debug_level(enum iwl_dl level)
{

	return (iwlwifi_mod_params.debug_level & level || level == IWL_DL_ANY);
}

/* Passing the iwl_drv * in seems pointless. */
void
iwl_print_hex_dump(void *drv __unused, enum iwl_dl level,
    const char *prefix, uint8_t *data, size_t len)
{

	/* Given we have a level, check for it. */
	if (!iwl_have_debug_level(level))
		return;

#if defined(__linux_)
	/* XXX I am cluseless in my editor. pcie/trans.c to the rescue. */
	print_hex_dump(KERN_ERR, prefix, DUMP_PREFIX_OFFSET,
	    32, 4, data, len, 0);
#elif defined(__FreeBSD__)
	hexdump(data, len, prefix, 0);
#endif
}
#endif

void __iwl_dbg(struct device *dev,
	       u32 level, bool limit, const char *function,
	       const char *fmt, ...)
{
	struct va_format vaf = {
		.fmt = fmt,
	};
	va_list args;

	va_start(args, fmt);
	vaf.va = &args;
#ifdef CONFIG_IWLWIFI_DEBUG
	if (iwl_have_debug_level(level) &&
	    (!limit || net_ratelimit())) {
#if defined(__linux_)
		dev_printk(KERN_DEBUG, dev, "%s %pV", function, &vaf);
#elif defined(__FreeBSD__)
		char *str;
		vasprintf(&str, M_KMALLOC, fmt, args);
		dev_printk(KERN_DEBUG, dev, "%d %u %s %s",
		    curthread->td_tid, (unsigned int)ticks, function, str);
		free(str, M_KMALLOC);
#endif
	}

#endif
	trace_iwlwifi_dbg(level, function, &vaf);
	va_end(args);
}
IWL_EXPORT_SYMBOL(__iwl_dbg);
#endif
