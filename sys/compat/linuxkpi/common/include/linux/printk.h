/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2017 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _LINUXKPI_LINUX_PRINTK_H_
#define	_LINUXKPI_LINUX_PRINTK_H_

#include <linux/kernel.h>

/* GID printing macros */
#define	GID_PRINT_FMT			"%.4x:%.4x:%.4x:%.4x:%.4x:%.4x:%.4x:%.4x"
#define	GID_PRINT_ARGS(gid_raw)		htons(((u16 *)gid_raw)[0]), htons(((u16 *)gid_raw)[1]),\
					htons(((u16 *)gid_raw)[2]), htons(((u16 *)gid_raw)[3]),\
					htons(((u16 *)gid_raw)[4]), htons(((u16 *)gid_raw)[5]),\
					htons(((u16 *)gid_raw)[6]), htons(((u16 *)gid_raw)[7])

enum {
	DUMP_PREFIX_NONE,
	DUMP_PREFIX_ADDRESS,
	DUMP_PREFIX_OFFSET
};

int __lkpi_hexdump_printf(void *, const char *, ...) __printflike(2, 3);

void lkpi_hex_dump(int(*)(void *, const char *, ...), void *arg1,
    const char *, const char *, const int, const int, const int,
    const void *, size_t, const bool);

static inline void
print_hex_dump(const char *level, const char *prefix_str,
    const int prefix_type, const int rowsize, const int groupsize,
    const void *buf, size_t len, const bool ascii)
{
	lkpi_hex_dump(__lkpi_hexdump_printf, NULL, level, prefix_str, prefix_type,
	    rowsize, groupsize, buf, len, ascii);
}

static inline void
print_hex_dump_bytes(const char *prefix_str, const int prefix_type,
    const void *buf, size_t len)
{
	print_hex_dump(NULL, prefix_str, prefix_type, 16, 1, buf, len, 0);
}

#define	printk_ratelimit() ({			\
	static linux_ratelimit_t __ratelimited;	\
	linux_ratelimited(&__ratelimited);	\
})

#define	printk_ratelimited(...) ({		\
	bool __retval = printk_ratelimit();	\
	if (__retval)				\
		printk(__VA_ARGS__);		\
	__retval;				\
})

#define	pr_err_ratelimited(fmt, ...) \
	printk_ratelimited(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)

#define	print_hex_dump_debug(...) \
	print_hex_dump(KERN_DEBUG, ##__VA_ARGS__)

#define	pr_info_ratelimited(fmt, ...) \
	printk_ratelimited(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)

#define	no_printk(fmt, ...)					\
({								\
	if (0)							\
		printk(pr_fmt(fmt), ##__VA_ARGS__);		\
	0;							\
})

#endif					/* _LINUXKPI_LINUX_PRINTK_H_ */
