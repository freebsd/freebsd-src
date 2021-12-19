/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2016 Mellanox Technologies, Ltd.
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
 *
 * $FreeBSD$
 */
#ifndef	_LINUXKPI_ASM_BYTEORDER_H_
#define	_LINUXKPI_ASM_BYTEORDER_H_

#include <sys/types.h>
#include <sys/endian.h>
#include <asm/types.h>

#if BYTE_ORDER == LITTLE_ENDIAN
#define	__LITTLE_ENDIAN
#else
#define	__BIG_ENDIAN
#endif

#define	__cpu_to_le64(x)	htole64(x)
#define	cpu_to_le64(x)		__cpu_to_le64(x)
#define	__le64_to_cpu(x)	le64toh(x)
#define	le64_to_cpu(x)		__le64_to_cpu(x)
#define	__cpu_to_le32(x)	htole32(x)
#define	cpu_to_le32(x)		__cpu_to_le32(x)
#define	__le32_to_cpu(x)	le32toh(x)
#define	le32_to_cpu(x)		__le32_to_cpu(x)
#define	__cpu_to_le16(x)	htole16(x)
#define	cpu_to_le16(x)		__cpu_to_le16(x)
#define	__le16_to_cpu(x)	le16toh(x)
#define	le16_to_cpu(x)		__le16_to_cpu(x)
#define	__cpu_to_be64(x)	htobe64(x)
#define	cpu_to_be64(x)		__cpu_to_be64(x)
#define	__be64_to_cpu(x)	be64toh(x)
#define	be64_to_cpu(x)		__be64_to_cpu(x)
#define	__cpu_to_be32(x)	htobe32(x)
#define	cpu_to_be32(x)		__cpu_to_be32(x)
#define	__be32_to_cpu(x)	be32toh(x)
#define	be32_to_cpu(x)		__be32_to_cpu(x)
#define	__cpu_to_be16(x)	htobe16(x)
#define	cpu_to_be16(x)		__cpu_to_be16(x)
#define	__be16_to_cpu(x)	be16toh(x)
#define	be16_to_cpu(x)		__be16_to_cpu(x)

#define	__cpu_to_le64p(x)	htole64(*((const uint64_t *)(x)))
#define	cpu_to_le64p(x)		__cpu_to_le64p(x)
#define	__le64_to_cpup(x)	le64toh(*((const uint64_t *)(x)))
#define	le64_to_cpup(x)		__le64_to_cpup(x)
#define	__cpu_to_le32p(x)	htole32(*((const uint32_t *)(x)))
#define	cpu_to_le32p(x)		__cpu_to_le32p(x)
#define	__le32_to_cpup(x)	le32toh(*((const uint32_t *)(x)))
#define	le32_to_cpup(x)		__le32_to_cpup(x)
#define	__cpu_to_le16p(x)	htole16(*((const uint16_t *)(x)))
#define	cpu_to_le16p(x)		__cpu_to_le16p(x)
#define	__le16_to_cpup(x)	le16toh(*((const uint16_t *)(x)))
#define	le16_to_cpup(x)		__le16_to_cpup(x)
#define	__cpu_to_be64p(x)	htobe64(*((const uint64_t *)(x)))
#define	cpu_to_be64p(x)		__cpu_to_be64p(x)
#define	__be64_to_cpup(x)	be64toh(*((const uint64_t *)(x)))
#define	be64_to_cpup(x)		__be64_to_cpup(x)
#define	__cpu_to_be32p(x)	htobe32(*((const uint32_t *)(x)))
#define	cpu_to_be32p(x)		__cpu_to_be32p(x)
#define	__be32_to_cpup(x)	be32toh(*((const uint32_t *)(x)))
#define	be32_to_cpup(x)		__be32_to_cpup(x)
#define	__cpu_to_be16p(x)	htobe16(*((const uint16_t *)(x)))
#define	cpu_to_be16p(x)		__cpu_to_be16p(x)
#define	__be16_to_cpup(x)	be16toh(*((const uint16_t *)(x)))
#define	be16_to_cpup(x)		__be16_to_cpup(x)


#define	__cpu_to_le64s(x)	do { *((uint64_t *)(x)) = cpu_to_le64p((x)); } while (0)
#define	cpu_to_le64s(x)		__cpu_to_le64s(x)
#define	__le64_to_cpus(x)	do { *((uint64_t *)(x)) = le64_to_cpup((x)); } while (0)
#define	le64_to_cpus(x)		__le64_to_cpus(x)
#define	__cpu_to_le32s(x)	do { *((uint32_t *)(x)) = cpu_to_le32p((x)); } while (0)
#define	cpu_to_le32s(x)		__cpu_to_le32s(x)
#define	__le32_to_cpus(x)	do { *((uint32_t *)(x)) = le32_to_cpup((x)); } while (0)
#define	le32_to_cpus(x)		__le32_to_cpus(x)
#define	__cpu_to_le16s(x)	do { *((uint16_t *)(x)) = cpu_to_le16p((x)); } while (0)
#define	cpu_to_le16s(x)		__cpu_to_le16s(x)
#define	__le16_to_cpus(x)	do { *((uint16_t *)(x)) = le16_to_cpup((x)); } while (0)
#define	le16_to_cpus(x)		__le16_to_cpus(x)
#define	__cpu_to_be64s(x)	do { *((uint64_t *)(x)) = cpu_to_be64p((x)); } while (0)
#define	cpu_to_be64s(x)		__cpu_to_be64s(x)
#define	__be64_to_cpus(x)	do { *((uint64_t *)(x)) = be64_to_cpup((x)); } while (0)
#define	be64_to_cpus(x)		__be64_to_cpus(x)
#define	__cpu_to_be32s(x)	do { *((uint32_t *)(x)) = cpu_to_be32p((x)); } while (0)
#define	cpu_to_be32s(x)		__cpu_to_be32s(x)
#define	__be32_to_cpus(x)	do { *((uint32_t *)(x)) = be32_to_cpup((x)); } while (0)
#define	be32_to_cpus(x)		__be32_to_cpus(x)
#define	__cpu_to_be16s(x)	do { *((uint16_t *)(x)) = cpu_to_be16p((x)); } while (0)
#define	cpu_to_be16s(x)		__cpu_to_be16s(x)
#define	__be16_to_cpus(x)	do { *((uint16_t *)(x)) = be16_to_cpup((x)); } while (0)
#define	be16_to_cpus(x)		__be16_to_cpus(x)

#define	swab16(x)	bswap16(x)
#define	swab32(x)	bswap32(x)
#define	swab64(x)	bswap64(x)

static inline void
be64_add_cpu(uint64_t *var, uint64_t val)
{
	*var = cpu_to_be64(be64_to_cpu(*var) + val);
}

static inline void
be32_add_cpu(uint32_t *var, uint32_t val)
{
	*var = cpu_to_be32(be32_to_cpu(*var) + val);
}

static inline void
be16_add_cpu(uint16_t *var, uint16_t val)
{
	*var = cpu_to_be16(be16_to_cpu(*var) + val);
}

static __inline void
le64_add_cpu(uint64_t *var, uint64_t val)
{
	*var = cpu_to_le64(le64_to_cpu(*var) + val);
}

static __inline void
le32_add_cpu(uint32_t *var, uint32_t val)
{
	*var = cpu_to_le32(le32_to_cpu(*var) + val);
}

static inline void
le16_add_cpu(uint16_t *var, uint16_t val)
{
	*var = cpu_to_le16(le16_to_cpu(*var) + val);
}

#endif	/* _LINUXKPI_ASM_BYTEORDER_H_ */
