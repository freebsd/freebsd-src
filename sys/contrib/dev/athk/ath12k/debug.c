// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/vmalloc.h>
#include "core.h"
#include "debug.h"

void ath12k_info(struct ath12k_base *ab, const char *fmt, ...)
{
	struct va_format vaf = {
		.fmt = fmt,
	};
	va_list args;

	va_start(args, fmt);
	vaf.va = &args;
#if defined(__linux__)
	dev_info(ab->dev, "%pV", &vaf);
#elif defined(__FreeBSD__)
	{
		char *str;
		vasprintf(&str, M_KMALLOC, fmt, args);
		dev_printk(KERN_INFO, ab->dev, "%s", str);
		free(str, M_KMALLOC);
	}
#endif
	/* TODO: Trace the log */
	va_end(args);
}

void ath12k_err(struct ath12k_base *ab, const char *fmt, ...)
{
	struct va_format vaf = {
		.fmt = fmt,
	};
	va_list args;

	va_start(args, fmt);
	vaf.va = &args;
#if defined(__linux__)
	dev_err(ab->dev, "%pV", &vaf);
#elif defined(__FreeBSD__)
	{
		char *str;
		vasprintf(&str, M_KMALLOC, fmt, args);
		dev_printk(KERN_ERR, ab->dev, "%s", str);
		free(str, M_KMALLOC);
	}
#endif
	/* TODO: Trace the log */
	va_end(args);
}

void ath12k_warn(struct ath12k_base *ab, const char *fmt, ...)
{
	struct va_format vaf = {
		.fmt = fmt,
	};
	va_list args;

	va_start(args, fmt);
	vaf.va = &args;
#if defined(__linux__)
	dev_warn_ratelimited(ab->dev, "%pV", &vaf);
#elif defined(__FreeBSD__)
	{
		static linux_ratelimit_t __ratelimited;

		if (linux_ratelimited(&__ratelimited)) {
			char *str;
			vasprintf(&str, M_KMALLOC, fmt, args);
			dev_printk(KERN_WARN, ab->dev, "%s", str);
			free(str, M_KMALLOC);
		}
	}
#endif
	/* TODO: Trace the log */
	va_end(args);
}

#ifdef CONFIG_ATH12K_DEBUG

void __ath12k_dbg(struct ath12k_base *ab, enum ath12k_debug_mask mask,
		  const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	if (ath12k_debug_mask & mask)
#if defined(__linux__)
		dev_dbg(ab->dev, "%pV", &vaf);
#elif defined(__FreeBSD__)
	{
		char *str;
		vasprintf(&str, M_KMALLOC, fmt, args);
		dev_printk(KERN_DEBUG, ab->dev, "%s", str);
		free(str, M_KMALLOC);
	}
#endif

	/* TODO: trace log */

	va_end(args);
}

void ath12k_dbg_dump(struct ath12k_base *ab,
		     enum ath12k_debug_mask mask,
		     const char *msg, const char *prefix,
		     const void *buf, size_t len)
{
#if defined(__linux__)
	char linebuf[256];
	size_t linebuflen;
	const void *ptr;
#elif defined(__FreeBSD__)
	struct sbuf *sb;
	int rc;
#endif

	if (ath12k_debug_mask & mask) {
		if (msg)
			__ath12k_dbg(ab, mask, "%s\n", msg);

#if defined(__linux__)
		for (ptr = buf; (ptr - buf) < len; ptr += 16) {
			linebuflen = 0;
			linebuflen += scnprintf(linebuf + linebuflen,
						sizeof(linebuf) - linebuflen,
						"%s%08x: ",
						(prefix ? prefix : ""),
						(unsigned int)(ptr - buf));
			hex_dump_to_buffer(ptr, len - (ptr - buf), 16, 1,
					   linebuf + linebuflen,
					   sizeof(linebuf) - linebuflen, true);
			dev_dbg(ab->dev, "%s\n", linebuf);
		}
#elif defined(__FreeBSD__)
		sb = sbuf_new_auto();
		if (sb == NULL)
			goto trace;

		sbuf_hexdump(sb, buf, len, prefix, 0);
		sbuf_trim(sb);
		rc = sbuf_finish(sb);
		if (rc == 0)
			dev_printk(KERN_DEBUG, ab->dev, "%s\n", sbuf_data(sb));
		sbuf_delete(sb);
trace: ;
#endif
	}
}

#endif /* CONFIG_ATH12K_DEBUG */
