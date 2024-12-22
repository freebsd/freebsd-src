/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2017-2018 Mellanox Technologies, Ltd.
 * All rights reserved.
 * Copyright (c) 2018 Johannes Lundberg <johalun0@gmail.com>
 * Copyright (c) 2020-2022 The FreeBSD Foundation
 * Copyright (c) 2021 Vladimir Kondratyev <wulf@FreeBSD.org>
 * Copyright (c) 2023 Serenity Cyber Security, LLC
 *
 * Portions of this software were developed by Bjoern A. Zeeb and
 * Emmanuel Vadot under sponsorship from the FreeBSD Foundation.
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

#ifndef	_LINUXKPI_LINUX_KSTRTOX_H_
#define	_LINUXKPI_LINUX_KSTRTOX_H_

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/libkern.h>

#include <linux/compiler.h>
#include <linux/types.h>

#include <asm/uaccess.h>

static inline unsigned long long
simple_strtoull(const char *cp, char **endp, unsigned int base)
{
	return (strtouq(cp, endp, base));
}

static inline long long
simple_strtoll(const char *cp, char **endp, unsigned int base)
{
	return (strtoq(cp, endp, base));
}

static inline unsigned long
simple_strtoul(const char *cp, char **endp, unsigned int base)
{
	return (strtoul(cp, endp, base));
}

static inline long
simple_strtol(const char *cp, char **endp, unsigned int base)
{
	return (strtol(cp, endp, base));
}

static inline int
kstrtoul(const char *cp, unsigned int base, unsigned long *res)
{
	char *end;

	*res = strtoul(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	return (0);
}

static inline int
kstrtol(const char *cp, unsigned int base, long *res)
{
	char *end;

	*res = strtol(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	return (0);
}

static inline int
kstrtoint(const char *cp, unsigned int base, int *res)
{
	char *end;
	long temp;

	*res = temp = strtol(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	if (temp != (int)temp)
		return (-ERANGE);
	return (0);
}

static inline int
kstrtouint(const char *cp, unsigned int base, unsigned int *res)
{
	char *end;
	unsigned long temp;

	*res = temp = strtoul(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	if (temp != (unsigned int)temp)
		return (-ERANGE);
	return (0);
}

static inline int
kstrtou8(const char *cp, unsigned int base, uint8_t *res)
{
	char *end;
	unsigned long temp;

	*res = temp = strtoul(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	if (temp != (uint8_t)temp)
		return (-ERANGE);
	return (0);
}

static inline int
kstrtou16(const char *cp, unsigned int base, uint16_t *res)
{
	char *end;
	unsigned long temp;

	*res = temp = strtoul(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	if (temp != (uint16_t)temp)
		return (-ERANGE);
	return (0);
}

static inline int
kstrtou32(const char *cp, unsigned int base, uint32_t *res)
{

	return (kstrtouint(cp, base, res));
}

static inline int
kstrtos32(const char *cp, unsigned int base, int32_t *res)
{

	return (kstrtoint(cp, base, res));
}

static inline int
kstrtos64(const char *cp, unsigned int base, int64_t *res)
{
	char *end;

	*res = strtoq(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	return (0);
}

static inline int
kstrtoll(const char *cp, unsigned int base, long long *res)
{
	return (kstrtos64(cp, base, (int64_t *)res));
}

static inline int
kstrtou64(const char *cp, unsigned int base, u64 *res)
{
	char *end;

	*res = strtouq(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	return (0);
}

static inline int
kstrtoull(const char *cp, unsigned int base, unsigned long long *res)
{
	return (kstrtou64(cp, base, (uint64_t *)res));
}

static inline int
kstrtobool(const char *s, bool *res)
{
	int len;

	if (s == NULL || (len = strlen(s)) == 0 || res == NULL)
		return (-EINVAL);

	/* skip newline character, if any */
	if (s[len - 1] == '\n')
		len--;

	if (len == 1 && strchr("yY1", s[0]) != NULL)
		*res = true;
	else if (len == 1 && strchr("nN0", s[0]) != NULL)
		*res = false;
	else if (strncasecmp("on", s, len) == 0)
		*res = true;
	else if (strncasecmp("off", s, len) == 0)
		*res = false;
	else
		return (-EINVAL);

	return (0);
}

static inline int
kstrtobool_from_user(const char __user *s, size_t count, bool *res)
{
	char buf[8] = {};

	if (count > (sizeof(buf) - 1))
		count = (sizeof(buf) - 1);

	if (copy_from_user(buf, s, count))
		return (-EFAULT);

	return (kstrtobool(buf, res));
}

static inline int
kstrtoint_from_user(const char __user *s, size_t count, unsigned int base,
    int *p)
{
	char buf[36] = {};

	if (count > (sizeof(buf) - 1))
		count = (sizeof(buf) - 1);

	if (copy_from_user(buf, s, count))
		return (-EFAULT);

	return (kstrtoint(buf, base, p));
}

static inline int
kstrtouint_from_user(const char __user *s, size_t count, unsigned int base,
    unsigned int *p)
{
	char buf[36] = {};

	if (count > (sizeof(buf) - 1))
		count = (sizeof(buf) - 1);

	if (copy_from_user(buf, s, count))
		return (-EFAULT);

	return (kstrtouint(buf, base, p));
}

static inline int
kstrtou32_from_user(const char __user *s, size_t count, unsigned int base,
    unsigned int *p)
{

	return (kstrtouint_from_user(s, count, base, p));
}

static inline int
kstrtou8_from_user(const char __user *s, size_t count, unsigned int base,
    uint8_t *p)
{
	char buf[8] = {};

	if (count > (sizeof(buf) - 1))
		count = (sizeof(buf) - 1);

	if (copy_from_user(buf, s, count))
		return (-EFAULT);

	return (kstrtou8(buf, base, p));
}

#endif	/* _LINUXKPI_LINUX_KSTRTOX_H_ */
