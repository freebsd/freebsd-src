/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
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
#ifndef	_ASM_BYTEORDER_H_
#define	_ASM_BYTEORDER_H_

#include <sys/types.h>
#include <sys/endian.h>

#define	cpu_to_le64	htole64
#define	le64_to_cpu	le64toh
#define	cpu_to_le32	htole32
#define	le32_to_cpu	le32toh
#define	cpu_to_le16	htole16
#define	le16_to_cpu	le16toh
#define	cpu_to_be64	htobe64
#define	be64_to_cpu	be64toh
#define	cpu_to_be32	htobe32
#define	be32_to_cpu	be32toh
#define	cpu_to_be16	htobe16
#define	be16_to_cpu	be16toh
#define	__be16_to_cpu	be16toh

#define	cpu_to_le64p(x)	htole64(*(x))
#define	le64_to_cpup(x)	le64toh(*(x))
#define	cpu_to_le32p(x)	htole32(*(x))
#define	le32_to_cpup(x)	le32toh(*(x))
#define	cpu_to_le16p(x)	htole16(*(x))
#define	le16_to_cpup(x)	le16toh(*(x))
#define	cpu_to_be64p(x)	htobe64(*(x))
#define	be64_to_cpup(x)	be64toh(*(x))
#define	cpu_to_be32p(x)	htobe32(*(x))
#define	be32_to_cpup(x)	be32toh(*(x))
#define	cpu_to_be16p(x)	htobe16(*(x))
#define	be16_to_cpup(x)	be16toh(*(x))

#define	cpu_to_le64s(x)	do { *(x) = cpu_to_le64p((x)) } while (0)
#define	le64_to_cpus(x)	do { *(x) = le64_to_cpup((x)) } while (0)
#define	cpu_to_le32s(x)	do { *(x) = cpu_to_le32p((x)) } while (0)
#define	le32_to_cpus(x)	do { *(x) = le32_to_cpup((x)) } while (0)
#define	cpu_to_le16s(x)	do { *(x) = cpu_to_le16p((x)) } while (0)
#define	le16_to_cpus(x)	do { *(x) = le16_to_cpup((x)) } while (0)
#define	cpu_to_be64s(x)	do { *(x) = cpu_to_be64p((x)) } while (0)
#define	be64_to_cpus(x)	do { *(x) = be64_to_cpup((x)) } while (0)
#define	cpu_to_be32s(x)	do { *(x) = cpu_to_be32p((x)) } while (0)
#define	be32_to_cpus(x)	do { *(x) = be32_to_cpup((x)) } while (0)
#define	cpu_to_be16s(x)	do { *(x) = cpu_to_be16p((x)) } while (0)
#define	be16_to_cpus(x)	do { *(x) = be16_to_cpup((x)) } while (0)

#endif	/* _ASM_BYTEORDER_H_ */
