/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017, Bryan Venteicher <bryanv@FreeBSD.org>
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

#ifndef _VIRTIO_ENDIAN_H_
#define _VIRTIO_ENDIAN_H_

#include <sys/endian.h>
#ifndef _KERNEL
#include <stdbool.h>
#endif /* _KERNEL */

/*
 * VirtIO V1 (modern) uses little endian, while legacy VirtIO uses the guest's
 * native endian. These functions convert to and from the Guest's (driver's)
 * and the Host's (device's) endianness when needed.
 */

static inline uint16_t
virtio_htog16(bool modern, uint16_t val)
{
	if (modern)
		return (le16toh(val));
	else
		return (val);
}

static inline uint16_t
virtio_gtoh16(bool modern, uint16_t val)
{
	if (modern)
		return (htole16(val));
	else
		return (val);
}

static inline uint32_t
virtio_htog32(bool modern, uint32_t val)
{
	if (modern)
		return (le32toh(val));
	else
		return (val);
}

static inline uint32_t
virtio_gtoh32(bool modern, uint32_t val)
{
	if (modern)
		return (htole32(val));
	else
		return (val);
}

static inline uint64_t
virtio_htog64(bool modern, uint64_t val)
{
	if (modern)
		return (le64toh(val));
	else
		return (val);
}

static inline uint64_t
virtio_gtoh64(bool modern, uint64_t val)
{
	if (modern)
		return (htole64(val));
	else
		return (val);
}

#endif /* _VIRTIO_ENDIAN_H_ */
