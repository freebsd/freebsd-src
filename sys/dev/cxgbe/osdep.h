/*-
 * Copyright (c) 2010 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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
 *
 * $FreeBSD$
 *
 */

#ifndef __CXGBE_OSDEP_H_
#define __CXGBE_OSDEP_H_

#include <sys/cdefs.h>
#include <sys/ctype.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <dev/pci/pcireg.h>

#define CH_ERR(adap, fmt, ...) log(LOG_ERR, fmt, ##__VA_ARGS__)
#define CH_WARN(adap, fmt, ...) log(LOG_WARNING, fmt, ##__VA_ARGS__)
#define CH_ALERT(adap, fmt, ...) log(LOG_ALERT, fmt, ##__VA_ARGS__)
#define CH_WARN_RATELIMIT(adap, fmt, ...) log(LOG_WARNING, fmt, ##__VA_ARGS__)

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint8_t  __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef uint8_t  __be8;
typedef uint16_t __be16;
typedef uint32_t __be32;
typedef uint64_t __be64;

#if BYTE_ORDER == BIG_ENDIAN
#define __BIG_ENDIAN_BITFIELD
#elif BYTE_ORDER == LITTLE_ENDIAN
#define __LITTLE_ENDIAN_BITFIELD
#else
#error "Must set BYTE_ORDER"
#endif

typedef boolean_t bool;
#define false FALSE
#define true TRUE

#define mdelay(x) DELAY((x) * 1000)
#define udelay(x) DELAY(x)

#define __devinit
#define simple_strtoul strtoul
#define DIV_ROUND_UP(x, y) howmany(x, y)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define swab16(x) bswap16(x) 
#define swab32(x) bswap32(x) 
#define swab64(x) bswap64(x) 
#define le16_to_cpu(x) le16toh(x)
#define le32_to_cpu(x) le32toh(x)
#define le64_to_cpu(x) le64toh(x)
#define cpu_to_le16(x) htole16(x)
#define cpu_to_le32(x) htole32(x)
#define cpu_to_le64(x) htole64(x)
#define be16_to_cpu(x) be16toh(x)
#define be32_to_cpu(x) be32toh(x)
#define be64_to_cpu(x) be64toh(x)
#define cpu_to_be16(x) htobe16(x)
#define cpu_to_be32(x) htobe32(x)
#define cpu_to_be64(x) htobe64(x)

#define SPEED_10	10
#define SPEED_100	100
#define SPEED_1000	1000
#define SPEED_10000	10000
#define DUPLEX_HALF	0
#define DUPLEX_FULL	1
#define AUTONEG_DISABLE	0
#define AUTONEG_ENABLE	1

#define PCI_CAP_ID_VPD  PCIY_VPD
#define PCI_VPD_ADDR    PCIR_VPD_ADDR
#define PCI_VPD_ADDR_F  0x8000
#define PCI_VPD_DATA    PCIR_VPD_DATA

#define PCI_CAP_ID_EXP		PCIY_EXPRESS
#define PCI_EXP_DEVCTL		PCIR_EXPRESS_DEVICE_CTL
#define PCI_EXP_DEVCTL_PAYLOAD	PCIM_EXP_CTL_MAX_PAYLOAD
#define PCI_EXP_DEVCTL_READRQ	PCIM_EXP_CTL_MAX_READ_REQUEST
#define PCI_EXP_LNKCTL		PCIR_EXPRESS_LINK_CTL
#define PCI_EXP_LNKSTA		PCIR_EXPRESS_LINK_STA
#define PCI_EXP_LNKSTA_CLS	PCIM_LINK_STA_SPEED
#define PCI_EXP_LNKSTA_NLW	PCIM_LINK_STA_WIDTH

static inline int
ilog2(long x)
{
	KASSERT(x > 0 && powerof2(x), ("%s: invalid arg %ld", __func__, x));

	return (flsl(x) - 1);
}

static inline char *
strstrip(char *s)
{
	char c, *r, *trim_at;

	while (isspace(*s))
		s++;
	r = trim_at = s;

	while ((c = *s++) != 0) {
		if (!isspace(c))
			trim_at = s;
	}
	*trim_at = 0;

	return (r);
}

#endif
